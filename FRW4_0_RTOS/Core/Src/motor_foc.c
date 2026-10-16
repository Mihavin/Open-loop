/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    motor_foc.c
  * @brief   Basic FOC implementation for this project (STM32H743 + TIM1 + ADC1/2/3 + TIM2 encoder).
  *
  * This module is designed to live entirely in USER code (no CubeMX regeneration conflicts).
  * It uses:
  *   - ADC injected conversion complete interrupt (ADC1) as the fast current control loop trigger.
  *   - TIM7 period elapsed interrupt as the slow speed loop trigger (optional).
  ******************************************************************************
  */
/* USER CODE END Header */

#include "motor_foc.h"

#include "main.h"
#include "adc.h"
#include "tim.h"
#include "leds.h"
#include "fault_manager.h"
#include "drv_spi.h"

extern drv8323_spi_t g_drv8323;
static volatile bool g_calibrated;
static bool g_started;
static volatile uint32_t g_adc_mask;
static volatile uint32_t g_sample_tick;
static uint32_t g_slow_tick;
static uint32_t g_driver_poll_tick;
static int32_t g_position_origin;
static uint32_t g_angle_raw;
static volatile int32_t g_angle_mod;

#include <math.h>

/* ----------- Small helpers ----------- */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float clamp_f(float x, float lo, float hi)
{
  if (!isfinite(x)) return 0.0f;
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

static inline float wrap_0_2pi(float a)
{
  /* Wrap angle to [0, 2pi) */
  while (a >= (2.0f * (float)M_PI)) a -= (2.0f * (float)M_PI);
  while (a < 0.0f)                 a += (2.0f * (float)M_PI);
  return a;
}

typedef struct
{
  float kp;
  float ki;
  float integrator;
  float out_min;
  float out_max;
} PI_t;

static inline void PI_Init(PI_t *pi, float kp, float ki, float out_min, float out_max)
{
  pi->kp = kp;
  pi->ki = ki;
  pi->integrator = 0.0f;
  pi->out_min = out_min;
  pi->out_max = out_max;
}

static inline float PI_Run(PI_t *pi, float err, float dt)
{
  /* Basic PI with integrator clamping anti-windup */
  float p = pi->kp * err;

  pi->integrator += (pi->ki * err) * dt;
  pi->integrator = clamp_f(pi->integrator, pi->out_min, pi->out_max);

  float out = p + pi->integrator;
  out = clamp_f(out, pi->out_min, pi->out_max);
  return out;
}

/* ----------- FOC state ----------- */
typedef struct
{
  volatile MotorFOC_State_t state;
  volatile MotorFOC_Mode_t mode;

  /* Offsets (ADC counts) */
  float off_ia;
  float off_ib;
  float off_ic;

  /* Currents (A) */
  volatile float ia;
  volatile float ib;
  volatile float ic;

  volatile float id;
  volatile float iq;

  /* References */
  volatile float id_ref;
  volatile float iq_ref;
  volatile float speed_ref_rpm;

  /* Position reference / feedback (encoder counts, multi-turn) */
  volatile int32_t pos_ref_counts;
  volatile int32_t pos_counts;

  /* Speed estimate */
  volatile float speed_rpm;
  int32_t enc_prev;

  /* Encoder -> electrical angle offset */
  float elec_offset;

  /* PI controllers */
  PI_t pi_id;
  PI_t pi_iq;
  PI_t pi_speed;
  PI_t pi_pos;
//  PI_t pi_pos;

  /* Calibration */
  uint32_t calib_samples;
  uint32_t calib_target;
  uint64_t sum_ia;
  uint64_t sum_ib;
  uint64_t sum_ic;

  /* Alignment timing */
  uint32_t align_ticks;       /* speed timer ticks */
  uint32_t align_ticks_total; /* speed timer ticks */

  /* Configuration derived */
  float dt_i;   /* current loop dt */
  float dt_w;   /* speed loop dt */
  float vbus;
  float v_limit; /* max alpha-beta voltage magnitude */

} MotorFOC_t;

static MotorFOC_t g_foc;

typedef enum
{
  MOTOR_FOC_REQ_NONE           = 0u,
  MOTOR_FOC_REQ_ARM            = 1u << 0,
  MOTOR_FOC_REQ_DISARM         = 1u << 1,
  MOTOR_FOC_REQ_EMERGENCY_STOP = 1u << 2,
  MOTOR_FOC_REQ_ZERO_ENCODER   = 1u << 3,
  MOTOR_FOC_REQ_STOP           = 1u << 4,
} MotorFOC_RequestBits_t;

typedef struct
{
  MotorFOC_Mode_t mode;
  float speed_ref_rpm;
  float torque_iq_ref_a;
  int32_t position_ref_counts;
  uint32_t requests;
  uint32_t seq;
} MotorFOC_Command_t;

static MotorFOC_Command_t g_cmd;

/* Short critical section usable before/after scheduler start.
 * Do not use mutexes here: ADC ISR participates in the control path.
 */
static inline uint32_t foc_irq_lock(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  __DMB();
  return primask;
}

static inline void foc_irq_unlock(uint32_t primask)
{
  __DMB();
  __set_PRIMASK(primask);
}

static MotorFOC_Command_t foc_cmd_snapshot_and_clear_requests(void)
{
  MotorFOC_Command_t cmd;

  uint32_t primask = foc_irq_lock();
  cmd = g_cmd;
  g_cmd.requests = MOTOR_FOC_REQ_NONE;
  foc_irq_unlock(primask);

  return cmd;
}

static void foc_request(uint32_t req)
{
  uint32_t primask = foc_irq_lock();

  g_cmd.requests |= req;
  g_cmd.seq++;

  foc_irq_unlock(primask);
}

static void foc_cmd_init(int32_t initial_pos_counts)
{
  uint32_t primask = foc_irq_lock();

  g_cmd.mode = MOTOR_FOC_MODE_SPEED;
  g_cmd.speed_ref_rpm = 0.0f;
  g_cmd.torque_iq_ref_a = 0.0f;
  g_cmd.position_ref_counts = initial_pos_counts;
  g_cmd.requests = MOTOR_FOC_REQ_NONE;
  g_cmd.seq = 0u;

  foc_irq_unlock(primask);
}

static void foc_set_current_refs(float id_ref, float iq_ref)
{
  uint32_t primask = foc_irq_lock();

  g_foc.id_ref = id_ref;
  g_foc.iq_ref = iq_ref;

  foc_irq_unlock(primask);
}

/* Current scale: ADC counts -> amps */
static float adc_counts_to_amps(float adc_counts)
{
  /* After offset removal: V = counts * Vref / fullscale */
  const float adc_lsb = (MOTOR_FOC_ADC_VREF / MOTOR_FOC_ADC_FULL_SCALE);
  const float volts = adc_counts * adc_lsb;
  const float amps = volts / (MOTOR_FOC_SHUNT_OHMS * MOTOR_FOC_AMP_GAIN);
  return amps;
}

static void pwm_set_duty_abc(float duty_a, float duty_b, float duty_c)
{
  duty_a = clamp_f(duty_a, 0.0f, 1.0f);
  duty_b = clamp_f(duty_b, 0.0f, 1.0f);
  duty_c = clamp_f(duty_c, 0.0f, 1.0f);

  const uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(duty_a * (float)arr));
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(duty_b * (float)arr));
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint32_t)(duty_c * (float)arr));
}

static void svm_from_vab(float v_alpha, float v_beta, float vbus,
                         float *d_a, float *d_b, float *d_c)
{
  /* SVPWM using zero-sequence injection:
   *  1) Convert alpha/beta voltages to phase voltages
   *  2) Add common-mode (-(vmax+vmin)/2) to maximize modulation range
   *  3) Convert to duty [0..1] using duty = 0.5 + v_phase / Vbus
   */
  const float one_over_vbus = 1.0f / vbus;
  const float k = 0.8660254037844386f; /* sqrt(3)/2 */

  float v_a = v_alpha;
  float v_b = (-0.5f * v_alpha) + (k * v_beta);
  float v_c = (-0.5f * v_alpha) - (k * v_beta);

  float v_max = v_a;
  if (v_b > v_max) v_max = v_b;
  if (v_c > v_max) v_max = v_c;

  float v_min = v_a;
  if (v_b < v_min) v_min = v_b;
  if (v_c < v_min) v_min = v_c;

  const float v_off = 0.5f * (v_max + v_min);

  v_a -= v_off;
  v_b -= v_off;
  v_c -= v_off;

  *d_a = 0.5f + (v_a * one_over_vbus);
  *d_b = 0.5f + (v_b * one_over_vbus);
  *d_c = 0.5f + (v_c * one_over_vbus);
}

/* Compute electrical angle from encoder */
static inline float get_mech_angle_rad(int32_t enc_cnt)
{
  /* Use modulo CPR to wrap mechanical angle */
  int32_t mod = (int32_t)MOTOR_FOC_ENCODER_CPR;
  int32_t cnt = enc_cnt % mod;
  if (cnt < 0) cnt += mod;
  return ((float)cnt) * (2.0f * (float)M_PI / (float)MOTOR_FOC_ENCODER_CPR);
}

static inline float get_elec_angle_rad(float mech_angle, float elec_offset)
{
  float theta = ((float)MOTOR_FOC_POLE_PAIRS) * mech_angle + elec_offset;
  return wrap_0_2pi(theta);
}

/* ----------- Public API ----------- */
void MotorFOC_Init(void)
{
  g_foc.state = MOTOR_FOC_STATE_IDLE;
  g_foc.mode  = MOTOR_FOC_MODE_SPEED;

  g_foc.off_ia = 0.0f;
  g_foc.off_ib = 0.0f;
  g_foc.off_ic = 0.0f;

  g_foc.ia = 0.0f;
  g_foc.ib = 0.0f;
  g_foc.ic = 0.0f;
  g_foc.id = 0.0f;
  g_foc.iq = 0.0f;

  g_foc.id_ref = 0.0f;
  g_foc.iq_ref = 0.0f;
  g_foc.speed_ref_rpm = 0.0f;
  g_foc.speed_rpm = 0.0f;

  /* Position reference defaults to the current encoder count */
  g_foc.pos_counts = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
  g_foc.pos_ref_counts = g_foc.pos_counts;

  foc_cmd_init(g_foc.pos_counts);
  g_position_origin = g_foc.pos_counts;
  g_angle_raw = (uint32_t)g_foc.pos_counts;
  g_angle_mod = g_foc.pos_counts % MOTOR_FOC_ENCODER_CPR;

  g_foc.elec_offset = 0.0f;

  RCC_ClkInitTypeDef clock = {0};
  uint32_t latency;
  HAL_RCC_GetClockConfig(&clock, &latency);
  uint32_t timer_hz = HAL_RCC_GetPCLK2Freq();
  if (clock.APB2CLKDivider != RCC_HCLK_DIV1) timer_hz *= 2u;
  g_foc.dt_i = (float)((htim1.Init.Prescaler + 1u) * 2u * htim1.Init.Period) / (float)timer_hz;
  g_calibrated = false;
  g_started = false;
  g_adc_mask = 0u;
  g_foc.dt_w = 1.0f / MOTOR_FOC_SPEED_HZ;

  g_foc.vbus = MOTOR_FOC_VBUS_VOLTS;
  g_foc.v_limit = g_foc.vbus * 0.57735026919f; /* ~Vbus/sqrt(3) */

  /* PI output limits (voltages) */
  PI_Init(&g_foc.pi_id, MOTOR_FOC_KP_ID, MOTOR_FOC_KI_ID, -g_foc.v_limit, g_foc.v_limit);
  PI_Init(&g_foc.pi_iq, MOTOR_FOC_KP_IQ, MOTOR_FOC_KI_IQ, -g_foc.v_limit, g_foc.v_limit);

  /* Speed PI output = iq_ref (A) */
  PI_Init(&g_foc.pi_speed, MOTOR_FOC_KP_SPEED, MOTOR_FOC_KI_SPEED, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);

  /* Position PI output = speed_ref (RPM) */
  PI_Init(&g_foc.pi_pos, MOTOR_FOC_KP_POS, MOTOR_FOC_KI_POS,
          -MOTOR_FOC_SPEED_MAX_RPM, MOTOR_FOC_SPEED_MAX_RPM);

  /* Calibration settings */
  g_foc.calib_samples = 0;
  g_foc.calib_target  = 512; /* ~40ms @ 25kHz */
  g_foc.sum_ia = 0;
  g_foc.sum_ib = 0;
  g_foc.sum_ic = 0;

  /* Alignment timing: based on speed loop ticks */
  g_foc.align_ticks = 0;
  g_foc.align_ticks_total = (uint32_t)((MOTOR_FOC_ALIGN_TIME_MS * MOTOR_FOC_SPEED_HZ) / 1000.0f);
  if (g_foc.align_ticks_total == 0) g_foc.align_ticks_total = 1;

  g_foc.enc_prev = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
}

static bool foc_configure_driver(void)
{
  /* ENABLE low resets registers. Every arm performs a checked configuration
     while OSSI/OIS force all six timer outputs low. */
  CLEAR_BIT(htim1.Instance->BDTR, TIM_BDTR_MOE);
  if (Fault_IsCriticalActive()) return false;
  drv8323_cfg_t cfg;
  drv8323_cfg_set_defaults(&cfg);
  if (drv8323_init(&g_drv8323, &cfg) != HAL_OK ||
      HAL_GPIO_ReadPin(DRV_NFAULT_GPIO_Port, DRV_NFAULT_Pin) == GPIO_PIN_RESET)
  {
    Fault_Report(FAULT_BLDC_DRIVER | FAULT_SPI);
    return false;
  }
  if (Fault_IsCriticalActive()) { Fault_OutputsOff(); return false; }
  __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_BREAK);
  return true;
}

void MotorFOC_Start(void)
{
  if (g_started || Fault_IsCriticalActive()) return;
  HAL_GPIO_WritePin(DRV_EN_GPIO_Port, DRV_EN_Pin, GPIO_PIN_RESET);
  pwm_set_duty_abc(0.5f, 0.5f, 0.5f);
  /* Start all timer channels behind the disabled external driver. */
  if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK ||
      HAL_TIM_Base_Start(&htim1) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2) != HAL_OK ||
      HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3) != HAL_OK ||
      HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK)
  { Fault_Report(FAULT_CPU); return; }
  CLEAR_BIT(htim1.Instance->BDTR, TIM_BDTR_MOE);
  if (!foc_configure_driver()) return;
  HAL_GPIO_WritePin(DRV_CAL_GPIO_Port, DRV_CAL_Pin, GPIO_PIN_SET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(DRV_CAL_GPIO_Port, DRV_CAL_Pin, GPIO_PIN_RESET);
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK ||
      HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK ||
      HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK)
  { Fault_Report(FAULT_ADC); return; }
  g_foc.calib_samples = 0;
  g_foc.sum_ia = g_foc.sum_ib = g_foc.sum_ic = 0;
  g_adc_mask = 0;
  g_sample_tick = g_slow_tick = HAL_GetTick();
  g_driver_poll_tick = g_slow_tick;
  g_foc.state = MOTOR_FOC_STATE_CALIB;
  /* All three completion callbacks rendezvous before consuming a sample. */
  HAL_NVIC_SetPriority(ADC3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(ADC3_IRQn);
  if (HAL_ADCEx_InjectedStart_IT(&hadc2) != HAL_OK ||
      HAL_ADCEx_InjectedStart_IT(&hadc3) != HAL_OK ||
      HAL_ADCEx_InjectedStart_IT(&hadc1) != HAL_OK)
  { Fault_Report(FAULT_ADC); return; }
  g_started = true;
}

static void foc_apply_disarm(void)
{
  CLEAR_BIT(htim1.Instance->BDTR, TIM_BDTR_MOE);
  /* Keep driver/ADC/encoder awake for zero-current and standstill monitoring.
     Emergency shutdown additionally removes driver enable. */
  pwm_set_duty_abc(0.5f, 0.5f, 0.5f);
  g_foc.id_ref = g_foc.iq_ref = g_foc.speed_ref_rpm = 0.0f;
  g_foc.pi_id.integrator = g_foc.pi_iq.integrator = 0.0f;
  g_foc.pi_speed.integrator = g_foc.pi_pos.integrator = 0.0f;
  g_foc.state = Fault_IsCriticalActive() ? MOTOR_FOC_STATE_FAULT : MOTOR_FOC_STATE_IDLE;
}

static void foc_apply_emergency_stop(void)
{
  Fault_OutputsOff();
  g_foc.id_ref = g_foc.iq_ref = g_foc.speed_ref_rpm = 0.0f;
  g_foc.state = MOTOR_FOC_STATE_FAULT;
}

static void foc_apply_zero_encoder(void)
{
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  g_foc.enc_prev = 0;
  g_foc.pos_counts = 0;
  g_foc.pos_ref_counts = 0;
  g_position_origin = 0;
  g_angle_raw = 0;
  g_angle_mod = 0;
}

static void foc_apply_stop(void)
{
  foc_apply_disarm();
  /* STOP is a restartable output inhibit; acquisition and encoder keep running. */
  uint32_t key = foc_irq_lock();
  g_cmd.speed_ref_rpm = g_cmd.torque_iq_ref_a = 0.0f;
  g_cmd.position_ref_counts = g_foc.pos_counts;
  foc_irq_unlock(key);
}

static void foc_apply_arm(void)
{
  if (!g_started || !g_calibrated || g_foc.state != MOTOR_FOC_STATE_IDLE ||
      fabsf(g_foc.speed_rpm) > 10.0f || Fault_IsCriticalActive()) return;
  if (!foc_configure_driver()) return;
  pwm_set_duty_abc(0.5f, 0.5f, 0.5f);
  g_foc.align_ticks = 0;
  g_foc.id_ref = MOTOR_FOC_ALIGN_ID_A;
  g_foc.iq_ref = 0.0f;
  g_foc.pi_id.integrator = g_foc.pi_iq.integrator = 0.0f;
  uint32_t key = foc_irq_lock();
  if (!Fault_IsCriticalActive() &&
      HAL_GPIO_ReadPin(DRV_NFAULT_GPIO_Port, DRV_NFAULT_Pin) == GPIO_PIN_SET)
  {
    g_foc.state = MOTOR_FOC_STATE_ALIGN;
    __HAL_TIM_MOE_ENABLE(&htim1); /* Last operation, after configuration/readback. */
  }
  foc_irq_unlock(key);
}

void MotorFOC_Stop(void)
{
  MotorFOC_RequestStop();
}

void MotorFOC_Arm(void)
{
  MotorFOC_RequestArm();
}

void MotorFOC_Disarm(void)
{
  MotorFOC_RequestDisarm();
}

void MotorFOC_RequestArm(void)
{
  foc_request(MOTOR_FOC_REQ_ARM);
}

void MotorFOC_RequestDisarm(void)
{
  foc_request(MOTOR_FOC_REQ_DISARM);
}

void MotorFOC_RequestEmergencyStop(void)
{
  Fault_Report(FAULT_USER_STOP);
  foc_request(MOTOR_FOC_REQ_EMERGENCY_STOP);
}

void MotorFOC_RequestZeroEncoder(void)
{
  uint32_t primask = foc_irq_lock();
  if (g_foc.state != MOTOR_FOC_STATE_IDLE || fabsf(g_foc.speed_rpm) > 10.0f)
  { foc_irq_unlock(primask); return; }

  g_cmd.position_ref_counts = 0;
  g_cmd.requests |= MOTOR_FOC_REQ_ZERO_ENCODER;
  g_cmd.seq++;

  foc_irq_unlock(primask);
}

void MotorFOC_RequestStop(void)
{
  foc_request(MOTOR_FOC_REQ_STOP);
}

void MotorFOC_SetMode(MotorFOC_Mode_t mode)
{
  uint32_t primask = foc_irq_lock();
  if ((unsigned)mode > MOTOR_FOC_MODE_POSITION ||
      (mode != g_cmd.mode && (g_foc.state != MOTOR_FOC_STATE_IDLE || fabsf(g_foc.speed_rpm) > 10.0f)))
  { foc_irq_unlock(primask); return; }

  if (g_cmd.mode != mode)
  {
    g_position_origin = g_foc.pos_counts;
    g_cmd.mode = mode;
    g_cmd.seq++;
  }

  foc_irq_unlock(primask);
}

void MotorFOC_SetCommand(MotorFOC_Mode_t mode,
                         float speed_rpm,
                         float torque_iq_A,
                         int32_t position_counts)
{
  if ((unsigned)mode > MOTOR_FOC_MODE_POSITION ||
      (mode != g_cmd.mode && (g_foc.state != MOTOR_FOC_STATE_IDLE || fabsf(g_foc.speed_rpm) > 10.0f)))
    return;

  speed_rpm = clamp_f(speed_rpm, -MOTOR_FOC_SPEED_MAX_RPM, MOTOR_FOC_SPEED_MAX_RPM);
  torque_iq_A = clamp_f(torque_iq_A, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);

  uint32_t primask = foc_irq_lock();

  if ((mode != g_cmd.mode) && (g_foc.state != MOTOR_FOC_STATE_IDLE || fabsf(g_foc.speed_rpm) > 10.0f))
  { foc_irq_unlock(primask); return; }
  if (mode != g_cmd.mode) g_position_origin = g_foc.pos_counts;
  g_cmd.mode = mode;
  g_cmd.speed_ref_rpm = speed_rpm;
  g_cmd.torque_iq_ref_a = torque_iq_A;
  int64_t difference = (int64_t)position_counts - g_position_origin;
  if (difference > MOTOR_FOC_POSITION_MAX_COUNTS) position_counts = g_position_origin + MOTOR_FOC_POSITION_MAX_COUNTS;
  if (difference < -MOTOR_FOC_POSITION_MAX_COUNTS) position_counts = g_position_origin - MOTOR_FOC_POSITION_MAX_COUNTS;
  g_cmd.position_ref_counts = position_counts;
  g_cmd.seq++;

  foc_irq_unlock(primask);
}

void MotorFOC_SetSpeedRPM(float rpm)
{
  rpm = clamp_f(rpm, -MOTOR_FOC_SPEED_MAX_RPM, MOTOR_FOC_SPEED_MAX_RPM);

  uint32_t primask = foc_irq_lock();

  g_cmd.speed_ref_rpm = rpm;
  g_cmd.seq++;

  foc_irq_unlock(primask);
}

void MotorFOC_SetTorqueCurrentA(float iq_A)
{
  iq_A = clamp_f(iq_A, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);

  uint32_t primask = foc_irq_lock();

  g_cmd.torque_iq_ref_a = iq_A;
  g_cmd.seq++;

  foc_irq_unlock(primask);
}

void MotorFOC_SetPositionCounts(int32_t pos_counts)
{
  uint32_t primask = foc_irq_lock();

  int64_t difference = (int64_t)pos_counts - g_position_origin;
  if (difference > MOTOR_FOC_POSITION_MAX_COUNTS) pos_counts = g_position_origin + MOTOR_FOC_POSITION_MAX_COUNTS;
  if (difference < -MOTOR_FOC_POSITION_MAX_COUNTS) pos_counts = g_position_origin - MOTOR_FOC_POSITION_MAX_COUNTS;
  g_cmd.position_ref_counts = pos_counts;
  g_cmd.seq++;

  foc_irq_unlock(primask);
}

void MotorFOC_SetPositionRevolutions(float revolutions)
{
  if (!isfinite(revolutions)) return;
  double counts = (double)revolutions * MOTOR_FOC_ENCODER_CPR;
  /* Bound before conversion, including at the int32 counter endpoints. */
  if (counts > 2147483647.0) counts = 2147483647.0;
  if (counts < -2147483648.0) counts = -2147483648.0;
  MotorFOC_SetPositionCounts((int32_t)counts);
}

MotorFOC_State_t MotorFOC_GetState(void)
{
  uint32_t primask = foc_irq_lock();
  MotorFOC_State_t s = g_foc.state;
  foc_irq_unlock(primask);
  return s;
}

MotorFOC_Mode_t MotorFOC_GetMode(void)
{
  uint32_t primask = foc_irq_lock();
  MotorFOC_Mode_t m = g_foc.mode;
  foc_irq_unlock(primask);
  return m;
}

MotorFOC_Mode_t MotorFOC_GetRequestedMode(void)
{
  uint32_t primask = foc_irq_lock();
  MotorFOC_Mode_t m = g_cmd.mode;
  foc_irq_unlock(primask);
  return m;
}

float MotorFOC_GetIa(void)
{
  uint32_t primask = foc_irq_lock();
  float v = g_foc.ia;
  foc_irq_unlock(primask);
  return v;
}

float MotorFOC_GetIb(void)
{
  uint32_t primask = foc_irq_lock();
  float v = g_foc.ib;
  foc_irq_unlock(primask);
  return v;
}

float MotorFOC_GetIc(void)
{
  uint32_t primask = foc_irq_lock();
  float v = g_foc.ic;
  foc_irq_unlock(primask);
  return v;
}

float MotorFOC_GetId(void)
{
  uint32_t primask = foc_irq_lock();
  float v = g_foc.id;
  foc_irq_unlock(primask);
  return v;
}

float MotorFOC_GetIq(void)
{
  uint32_t primask = foc_irq_lock();
  float v = g_foc.iq;
  foc_irq_unlock(primask);
  return v;
}

float MotorFOC_GetSpeedRPM(void)
{
  uint32_t primask = foc_irq_lock();
  float v = g_foc.speed_rpm;
  foc_irq_unlock(primask);
  return v;
}

int32_t MotorFOC_GetPositionCounts(void)
{
  uint32_t primask = foc_irq_lock();
  int32_t v = g_foc.pos_counts;
  foc_irq_unlock(primask);
  return v;
}
//float MotorFOC_GetAngle(void ) { return g_foc.enc_prev; }

/* ----------- ISR callbacks ----------- */
/* This callback is called from HAL_ADC_IRQHandler() when injected conversion finishes */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (Fault_IsCriticalActive()) { foc_apply_emergency_stop(); return; }
  uint32_t bit = hadc == &hadc1 ? 1u : hadc == &hadc2 ? 2u : hadc == &hadc3 ? 4u : 0u;
  if (!bit) return;
  if (g_adc_mask & bit) { Fault_Report(FAULT_ADC); return; }
  g_adc_mask |= bit;
  if (g_adc_mask != 7u) return;
  g_adc_mask = 0u;
  g_sample_tick = HAL_GetTick();
  /* Integrate unsigned counter differences before reducing modulo CPR. Taking
     signed CNT % CPR directly jumps electrical angle at either counter wrap. */
  uint32_t angle_now = __HAL_TIM_GET_COUNTER(&htim2);
  int32_t angle_delta = (int32_t)(angle_now - g_angle_raw);
  g_angle_raw = angle_now;
  g_angle_mod = (int32_t)(((int64_t)g_angle_mod + angle_delta) % MOTOR_FOC_ENCODER_CPR);


  /* Read injected ADC values (rank 1) */
  const uint32_t raw_ia = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1); /* SOA -> phase A */
  const uint32_t raw_ib = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_1); /* SOB -> phase B */
  const uint32_t raw_ic = HAL_ADCEx_InjectedGetValue(&hadc3, ADC_INJECTED_RANK_1); /* SOC -> phase C */

  if (g_foc.state == MOTOR_FOC_STATE_CALIB)
  {
    g_foc.sum_ia += raw_ia;
    g_foc.sum_ib += raw_ib;
    g_foc.sum_ic += raw_ic;
    g_foc.calib_samples++;

    if (g_foc.calib_samples >= g_foc.calib_target)
    {
      g_foc.off_ia = (float)((double)g_foc.sum_ia / (double)g_foc.calib_samples);
      g_foc.off_ib = (float)((double)g_foc.sum_ib / (double)g_foc.calib_samples);
      g_foc.off_ic = (float)((double)g_foc.sum_ic / (double)g_foc.calib_samples);

      g_foc.id_ref = 0.0f;
      g_foc.iq_ref = 0.0f;

      if (g_foc.off_ia < 0.4f * MOTOR_FOC_ADC_FULL_SCALE || g_foc.off_ia > 0.6f * MOTOR_FOC_ADC_FULL_SCALE ||
          g_foc.off_ib < 0.4f * MOTOR_FOC_ADC_FULL_SCALE || g_foc.off_ib > 0.6f * MOTOR_FOC_ADC_FULL_SCALE ||
          g_foc.off_ic < 0.4f * MOTOR_FOC_ADC_FULL_SCALE || g_foc.off_ic > 0.6f * MOTOR_FOC_ADC_FULL_SCALE)
      { Fault_Report(FAULT_ADC); return; }
      g_calibrated = true;
      g_foc.state = MOTOR_FOC_STATE_IDLE;

#if MOTOR_FOC_AUTOSTART
      foc_request(MOTOR_FOC_REQ_ARM);
#endif
    }
    return;
  }

  /* Convert to amps using offsets */
  const float ia = adc_counts_to_amps((float)raw_ia - g_foc.off_ia);
  const float ib = adc_counts_to_amps((float)raw_ib - g_foc.off_ib);
  const float ic = adc_counts_to_amps((float)raw_ic - g_foc.off_ic);

  g_foc.ia = ia;
  g_foc.ib = ib;
  g_foc.ic = ic;

  if (g_foc.state != MOTOR_FOC_STATE_ALIGN &&
      g_foc.state != MOTOR_FOC_STATE_RUN)
  {
    /* Not controlling -> keep zero voltage */
    pwm_set_duty_abc(0.5f, 0.5f, 0.5f);
    return;
  }

  if (fabsf(ia) > MOTOR_FOC_CURRENT_TRIP_A || fabsf(ib) > MOTOR_FOC_CURRENT_TRIP_A ||
      fabsf(ic) > MOTOR_FOC_CURRENT_TRIP_A || raw_ia < 1024u || raw_ib < 1024u || raw_ic < 1024u ||
      raw_ia > 64511u || raw_ib > 64511u || raw_ic > 64511u)
  { Fault_Report(FAULT_CURRENT); return; }

  /* Clarke transform (assuming ia+ib+ic ~= 0) */
  const float i_alpha = ia;
  const float i_beta  = (ia + 2.0f * ib) * 0.57735026919f; /* 1/sqrt(3) */

  float sin_t = 0.0f;
  float cos_t = 1.0f;
  float theta = 0.0f;

  if (g_foc.state == MOTOR_FOC_STATE_RUN)
  {
    const float mech = get_mech_angle_rad(g_angle_mod);
    theta = get_elec_angle_rad(mech, g_foc.elec_offset);
    sin_t = sinf(theta);
    cos_t = cosf(theta);
  }
  else /* ALIGN */
  {
    /* Align in stator frame: theta = 0 -> sin=0, cos=1 */
    theta = 0.0f;
    sin_t = 0.0f;
    cos_t = 1.0f;
  }

  /* Park transform */
  const float i_d =  (i_alpha * cos_t) + (i_beta * sin_t);
  const float i_q = (-i_alpha * sin_t) + (i_beta * cos_t);

  g_foc.id = i_d;
  g_foc.iq = i_q;

  /* Current references */
  float id_ref = g_foc.id_ref;
  float iq_ref = g_foc.iq_ref;

  if (g_foc.state == MOTOR_FOC_STATE_RUN)
  {
    /* clamp refs */
    id_ref = clamp_f(id_ref, -MOTOR_FOC_ID_MAX_A, MOTOR_FOC_ID_MAX_A);
    iq_ref = clamp_f(iq_ref, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);
  }
  else
  {
    /* ALIGN: keep iq=0 */
    iq_ref = 0.0f;
  }

  /* PI current control -> vd, vq */
  const float err_d = id_ref - i_d;
  const float err_q = iq_ref - i_q;

  float v_d = PI_Run(&g_foc.pi_id, err_d, g_foc.dt_i);
  float v_q = PI_Run(&g_foc.pi_iq, err_q, g_foc.dt_i);

  /* Voltage magnitude limit (vector clamp) */
  float v_mag = sqrtf((v_d * v_d) + (v_q * v_q));
  if (v_mag > g_foc.v_limit && v_mag > 0.0f)
  {
    const float scale = g_foc.v_limit / v_mag;
    v_d *= scale;
    v_q *= scale;
    /* Optional: could implement back-calculation anti-windup here */
  }

  /* Inverse Park -> alpha/beta */
  const float v_alpha = (v_d * cos_t) - (v_q * sin_t);
  const float v_beta  = (v_d * sin_t) + (v_q * cos_t);

  /* SVPWM -> duty */
  float d_a, d_b, d_c;
  svm_from_vab(v_alpha, v_beta, g_foc.vbus, &d_a, &d_b, &d_c);

  pwm_set_duty_abc(d_a, d_b, d_c);
}

/* MotorTask owner: measured elapsed time, nominally 1 ms. */
void MotorFOC(void)
{
  MotorFOC_Command_t cmd = foc_cmd_snapshot_and_clear_requests();
  if (Fault_IsCriticalActive()) { foc_apply_emergency_stop(); return; }
  uint32_t now = HAL_GetTick();
  uint32_t elapsed_ms = now - g_slow_tick;
  if (g_started)
  {
    /* Catch a limit already held at boot as well as EXTI edges. */
    if (HAL_GPIO_ReadPin(LIMIT_BUTTON_GPIO_Port, LIMIT_BUTTON_Pin) == GPIO_PIN_RESET)
    { Fault_Report(FAULT_LIMIT); return; }
    if (HAL_GPIO_ReadPin(DRV_NFAULT_GPIO_Port, DRV_NFAULT_Pin) == GPIO_PIN_RESET ||
        __HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_BREAK))
    { Fault_Report(FAULT_BLDC_DRIVER); return; }
    if ((uint32_t)(now - g_sample_tick) > 10u)
    { Fault_Report(FAULT_ADC); return; }
    if ((uint32_t)(now - g_driver_poll_tick) >= 50u)
    {
      uint16_t status;
      g_driver_poll_tick = now;
      if (drv8323_read_reg(&g_drv8323, DRV_REG_FAULT_STATUS_1, &status) != HAL_OK)
      { Fault_Report(FAULT_SPI); return; }
      if (drv8323_fault_active(status)) { Fault_Report(FAULT_BLDC_DRIVER); return; }
    }
  }
  if (elapsed_ms > 10u) { Fault_Report(FAULT_MOTION_TIMEOUT); return; }
  if (elapsed_ms != 0u)
  {
    g_slow_tick = now;
    g_foc.dt_w = (float)elapsed_ms * 0.001f;
    /* Keep speed feedback live even while disarmed: mode changes use standstill. */
    int32_t enc = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
    int32_t delta = (int32_t)((uint32_t)enc - (uint32_t)g_foc.enc_prev);
    g_foc.enc_prev = enc;
    g_foc.pos_counts = enc;
    g_foc.speed_rpm = (float)delta * 60.0f / ((float)MOTOR_FOC_ENCODER_CPR * g_foc.dt_w);
  }

  if (cmd.requests & MOTOR_FOC_REQ_EMERGENCY_STOP)
  {
    foc_apply_emergency_stop();
    return;
  }

  if (cmd.requests & MOTOR_FOC_REQ_STOP)
  {
    foc_apply_stop();
    return;
  }

  if (cmd.requests & MOTOR_FOC_REQ_DISARM)
  {
    foc_apply_stop();
    return; /* A coalesced ARM can never override DISARM. */
  }

  if (cmd.requests & MOTOR_FOC_REQ_ZERO_ENCODER)
  {
    if (g_foc.state == MOTOR_FOC_STATE_IDLE && fabsf(g_foc.speed_rpm) <= 10.0f) foc_apply_zero_encoder();
  }

  if (cmd.requests & MOTOR_FOC_REQ_ARM)
  {
    foc_apply_arm();
  }

  if (elapsed_ms == 0u) return;

  if (cmd.mode != g_foc.mode)
  {
    g_foc.pi_speed.integrator = 0.0f;
    g_foc.pi_pos.integrator = 0.0f;
    g_foc.mode = cmd.mode;
  }

  if (g_foc.state == MOTOR_FOC_STATE_ALIGN)
  {
    g_foc.align_ticks += elapsed_ms;

    if (g_foc.align_ticks >= g_foc.align_ticks_total)
    {
      /* Capture encoder position while rotor is aligned to theta=0 */
      const int32_t enc = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
      const float mech = get_mech_angle_rad(g_angle_mod);

      /* theta_e = pole_pairs*mech + offset; want theta_e==0 at this mech */
      g_foc.elec_offset = -((float)MOTOR_FOC_POLE_PAIRS) * mech;
      g_foc.elec_offset = wrap_0_2pi(g_foc.elec_offset);

      /* Clear alignment current */
      foc_set_current_refs(0.0f, 0.0f);

      /* Enter RUN */
      g_foc.enc_prev = enc;
      g_foc.pi_speed.integrator = 0.0f;
      g_foc.pi_pos.integrator   = 0.0f;
      g_foc.pos_counts          = enc;
      g_foc.pos_ref_counts      = enc;
      g_foc.state = MOTOR_FOC_STATE_RUN;
    }
    return;
  }

  if (g_foc.state != MOTOR_FOC_STATE_RUN)
    return;

  const int32_t enc_now = g_foc.pos_counts;
  const float rpm = g_foc.speed_rpm;


  if (g_foc.mode == MOTOR_FOC_MODE_SPEED)
  {
    float max_delta = 500.0f * g_foc.dt_w; /* conservative motor-rpm/s bring-up ramp */
    float requested = g_foc.speed_ref_rpm + clamp_f(cmd.speed_ref_rpm - g_foc.speed_ref_rpm, -max_delta, max_delta);
    const float err = requested - rpm;

    float iq_cmd = PI_Run(&g_foc.pi_speed, err, g_foc.dt_w);
    iq_cmd = clamp_f(iq_cmd, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);

    g_foc.speed_ref_rpm = requested;   /* debug/telemetry copy */
    foc_set_current_refs(0.0f, iq_cmd);
  }
  else if (g_foc.mode == MOTOR_FOC_MODE_POSITION)
  {
    const int64_t err_counts = (int64_t)cmd.position_ref_counts - (int64_t)enc_now;
    const float err_rev = (float)err_counts / (float)MOTOR_FOC_ENCODER_CPR;

    float speed_ref = PI_Run(&g_foc.pi_pos, err_rev, g_foc.dt_w);
    speed_ref = clamp_f(speed_ref, -MOTOR_FOC_SPEED_MAX_RPM, MOTOR_FOC_SPEED_MAX_RPM);

    const float err_speed = speed_ref - rpm;

    float iq_cmd = PI_Run(&g_foc.pi_speed, err_speed, g_foc.dt_w);
    iq_cmd = clamp_f(iq_cmd, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);

    g_foc.speed_ref_rpm = speed_ref;           /* debug/telemetry copy */
    g_foc.pos_ref_counts = cmd.position_ref_counts;
    foc_set_current_refs(0.0f, iq_cmd);
  }
  else
  {
    float iq_cmd = clamp_f(cmd.torque_iq_ref_a, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);
    foc_set_current_refs(0.0f, iq_cmd);
  }
}

static float theta = 0.0f;

void Motor_OpenLoopStep_20kHz(void)
{
    /* Debug entry point cannot bypass arming or a latched fault. */
    if (Fault_IsCriticalActive() || g_foc.state != MOTOR_FOC_STATE_RUN) return;
    const float electrical_hz = 5.0f;     // start low
    const float dt = 1.0f / 20000.0f;
 //   const float vbus = 24.0f;
    const float mod  = 0.08f;             // small voltage

    theta += 2.0f * 3.1415926f * electrical_hz * dt;
    if (theta > 2.0f * 3.1415926f) theta -= 2.0f * 3.1415926f;

    float va = mod * sinf(theta);
    float vb = mod * sinf(theta - 2.0943951f);
    float vc = mod * sinf(theta + 2.0943951f);

    // convert -1..+1 to duty 0..1 with zero-sequence injection
    float vmin = fminf(va, fminf(vb, vc));
    float vmax = fmaxf(va, fmaxf(vb, vc));
    float voff = -0.5f * (vmax + vmin);

    float da = 0.5f + 0.5f * (va + voff);
    float db = 0.5f + 0.5f * (vb + voff);
    float dc = 0.5f + 0.5f * (vc + voff);

    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(da * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(db * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint32_t)(dc * arr));
}
