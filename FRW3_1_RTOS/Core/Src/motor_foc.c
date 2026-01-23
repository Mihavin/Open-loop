/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    motor_foc.c
  * @brief   Basic FOC implementation for this project (STM32H723 + TIM1 + ADC1/2/3 + TIM2 encoder).
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

#include <math.h>

/* ----------- Small helpers ----------- */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float clamp_f(float x, float lo, float hi)
{
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

  /* Speed estimate */
  volatile float speed_rpm;
  int32_t enc_prev;

  /* Encoder -> electrical angle offset */
  float elec_offset;

  /* PI controllers */
  PI_t pi_id;
  PI_t pi_iq;
  PI_t pi_speed;

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

  g_foc.elec_offset = 0.0f;

  g_foc.dt_i = 1.0f / MOTOR_FOC_PWM_HZ;
  g_foc.dt_w = 1.0f / MOTOR_FOC_SPEED_HZ;

  g_foc.vbus = MOTOR_FOC_VBUS_VOLTS;
  g_foc.v_limit = g_foc.vbus * 0.57735026919f; /* ~Vbus/sqrt(3) */

  /* PI output limits (voltages) */
  PI_Init(&g_foc.pi_id, MOTOR_FOC_KP_ID, MOTOR_FOC_KI_ID, -g_foc.v_limit, g_foc.v_limit);
  PI_Init(&g_foc.pi_iq, MOTOR_FOC_KP_IQ, MOTOR_FOC_KI_IQ, -g_foc.v_limit, g_foc.v_limit);

  /* Speed PI output = iq_ref (A) */
  PI_Init(&g_foc.pi_speed, MOTOR_FOC_KP_SPEED, MOTOR_FOC_KI_SPEED, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);

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

void MotorFOC_Start(void)
{
  /* Keep driver disabled initially */
  HAL_GPIO_WritePin(DRV_EN_GPIO_Port, DRV_EN_Pin, GPIO_PIN_SET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(DRV_CAL_GPIO_Port, DRV_CAL_Pin, GPIO_PIN_SET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(DRV_CAL_GPIO_Port,DRV_CAL_Pin, GPIO_PIN_RESET);
  /* Start encoder counter */
  (void)HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  HAL_Delay(1);

  /* Put PWM in a safe 50% duty state before enabling outputs */
  pwm_set_duty_abc(0.5f, 0.5f, 0.5f);

  /* Ensure TIM1 counter is running (needed for PWM outputs + TRGO2 -> ADC injected trigger) */
  (void)HAL_TIM_Base_Start(&htim1);

  /* Start PWM timer and complementary outputs (signals will toggle, but driver is disabled) */
  (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  (void)HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);

  (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  (void)HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

  (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  (void)HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

  /* TIM1 advanced timer outputs require MOE=1 */


  /* Start OC4 (used for TRGO2 -> ADC injected trigger) */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

  /* Start injected ADC conversions:
   *  - Use ADC1 interrupt as the single "current-loop" trigger
   *  - Start ADC2/ADC3 without interrupts (we read their injected values in ADC1 callback)
   */
  (void)HAL_ADCEx_InjectedStart_IT(&hadc1);
  (void)HAL_ADCEx_InjectedStart(&hadc2);
  (void)HAL_ADCEx_InjectedStart(&hadc3);
 __HAL_TIM_MOE_ENABLE(&htim1);
  /* Start speed loop timer - no need for rtos */
 // (void)HAL_TIM_Base_Start_IT(&htim7);
 // HAL_TIM_Base_Start_IT(&htim6);

  /* Reset calibration accumulators */
  g_foc.calib_samples = 0;
  g_foc.sum_ia = 0;
  g_foc.sum_ib = 0;
  g_foc.sum_ic = 0;
  g_foc.state = MOTOR_FOC_STATE_CALIB;

#if MOTOR_FOC_AUTOSTART
  /* Autostart will arm once calibration is done */
#endif
}

void MotorFOC_Stop(void)
{
  MotorFOC_Disarm();

//  (void)HAL_TIM_Base_Stop_IT(&htim7);

  (void)HAL_ADCEx_InjectedStop_IT(&hadc1);
  (void)HAL_ADCEx_InjectedStop(&hadc2);
  (void)HAL_ADCEx_InjectedStop(&hadc3);

  (void)HAL_TIM_OC_Stop(&htim1, TIM_CHANNEL_4);

  (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
  (void)HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);

  (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
  (void)HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);

  (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
  (void)HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_3);

  (void)HAL_TIM_Encoder_Stop(&htim2, TIM_CHANNEL_ALL);

  g_foc.state = MOTOR_FOC_STATE_IDLE;
}

void MotorFOC_Arm(void)
{
	LED1_On();
  if (g_foc.state == MOTOR_FOC_STATE_FAULT)
    return;

  /* Only allow arming after calibration finished */
  if (g_foc.state == MOTOR_FOC_STATE_CALIB)
    return;

  /* Enable gate driver */
  HAL_GPIO_WritePin(DRV_EN_GPIO_Port, DRV_EN_Pin, GPIO_PIN_SET);

  /* Enter alignment stage to determine encoder electrical offset */
  g_foc.align_ticks = 0;
  g_foc.id_ref = MOTOR_FOC_ALIGN_ID_A;
  g_foc.iq_ref = 0.0f;

  /* Reset current PI integrators */
  g_foc.pi_id.integrator = 0.0f;
  g_foc.pi_iq.integrator = 0.0f;

  g_foc.state = MOTOR_FOC_STATE_ALIGN;
}

void MotorFOC_Disarm(void)
{
  /* Disable driver outputs */
  HAL_GPIO_WritePin(DRV_EN_GPIO_Port, DRV_EN_Pin, GPIO_PIN_RESET);

  /* Force zero voltage vector (50% duty) */
  pwm_set_duty_abc(0.5f, 0.5f, 0.5f);

  g_foc.id_ref = 0.0f;
  g_foc.iq_ref = 0.0f;
  g_foc.state  = MOTOR_FOC_STATE_IDLE;
}

void MotorFOC_SetMode(MotorFOC_Mode_t mode)
{
  g_foc.mode = mode;
}

void MotorFOC_SetSpeedRPM(float rpm)
{
  rpm = clamp_f(rpm, -MOTOR_FOC_SPEED_MAX_RPM, MOTOR_FOC_SPEED_MAX_RPM);
  g_foc.speed_ref_rpm = rpm;
}

void MotorFOC_SetTorqueCurrentA(float iq_A)
{
  iq_A = clamp_f(iq_A, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);
  g_foc.iq_ref = iq_A;
}

MotorFOC_State_t MotorFOC_GetState(void)
{
  return g_foc.state;
}

MotorFOC_Mode_t MotorFOC_GetMode(void)
{
  return g_foc.mode;
}

float MotorFOC_GetIa(void) { return g_foc.ia; }
float MotorFOC_GetIb(void) { return g_foc.ib; }
float MotorFOC_GetIc(void) { return g_foc.ic; }
float MotorFOC_GetId(void) { return g_foc.id; }
float MotorFOC_GetIq(void) { return g_foc.iq; }
float MotorFOC_GetSpeedRPM(void) { return g_foc.speed_rpm; }

/* ----------- ISR callbacks ----------- */
/* This callback is called from HAL_ADC_IRQHandler() when injected conversion finishes */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc != &hadc1)
  {
    /* Use ADC1 as the single control-loop trigger */
    return;
  }

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

      g_foc.state = MOTOR_FOC_STATE_IDLE;

#if MOTOR_FOC_AUTOSTART
      MotorFOC_Arm();
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

  /* Clarke transform (assuming ia+ib+ic ~= 0) */
  const float i_alpha = ia;
  const float i_beta  = (ia + 2.0f * ib) * 0.57735026919f; /* 1/sqrt(3) */

  float sin_t = 0.0f;
  float cos_t = 1.0f;
  float theta = 0.0f;

  if (g_foc.state == MOTOR_FOC_STATE_RUN)
  {
    const int32_t enc = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
    const float mech = get_mech_angle_rad(enc);
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

/* TIM7 period callback for speed loop + alignment timing */
void MotorFOC(void)
{
  if (g_foc.state == MOTOR_FOC_STATE_ALIGN)
  {
    g_foc.align_ticks++;

    if (g_foc.align_ticks >= g_foc.align_ticks_total)
    {
      /* Capture encoder position while rotor is aligned to theta=0 */
      const int32_t enc = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
      const float mech = get_mech_angle_rad(enc);

      /* theta_e = pole_pairs*mech + offset; want theta_e==0 at this mech */
      g_foc.elec_offset = -((float)MOTOR_FOC_POLE_PAIRS) * mech;
      g_foc.elec_offset = wrap_0_2pi(g_foc.elec_offset);

      /* Clear alignment current */
      g_foc.id_ref = 0.0f;
      g_foc.iq_ref = 0.0f;

      /* Enter RUN */
      g_foc.enc_prev = enc;
      g_foc.pi_speed.integrator = 0.0f;
      g_foc.state = MOTOR_FOC_STATE_RUN;
    }
    return;
  }

  if (g_foc.state != MOTOR_FOC_STATE_RUN)
    return;

  /* Speed estimate from encoder delta */
  const int32_t enc_now = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
  int32_t delta = enc_now - g_foc.enc_prev;
  g_foc.enc_prev = enc_now;

  /* Convert to mechanical rad/s */
  const float mech_rad = ((float)delta) * (2.0f * (float)M_PI / (float)MOTOR_FOC_ENCODER_CPR);
  const float mech_w   = mech_rad / g_foc.dt_w;
  const float rpm      = mech_w * (60.0f / (2.0f * (float)M_PI));
  g_foc.speed_rpm = rpm;

  if (g_foc.mode == MOTOR_FOC_MODE_SPEED)
  {
    /* Speed PI -> iq_ref */
    const float err = g_foc.speed_ref_rpm - rpm;
    float iq_cmd = PI_Run(&g_foc.pi_speed, err, g_foc.dt_w);
    iq_cmd = clamp_f(iq_cmd, -MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_IQ_MAX_A);
    g_foc.iq_ref = iq_cmd;
    g_foc.id_ref = 0.0f;
  }
  else
  {
    /* Torque mode: iq_ref already set by user; keep id_ref=0 */
    g_foc.id_ref = 0.0f;
  }
}


static float theta = 0.0f;

void Motor_OpenLoopStep_20kHz(void)
{
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
