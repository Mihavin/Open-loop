#include "foc.h"

#include "main.h"
#include "adc.h"
#include "tim.h"
#include "gpio.h"

#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692f
#endif

/*
 * FOC with:
 *  - 3-shunt current sensing (ADC injected conversions)
 *  - Encoder feedback on TIM2 (A/B)
 *  - Control loop called at fixed rate (TIM6 -> FOC_ControlStep)
 *
 * Fixes for "sometimes reverse / slow":
 *  1) ENCODER_CPR must match the REAL counts/rev seen in TIM2->CNT.
 *     If you see 4096 counts per mechanical rev (x4), set ENCODER_CPR = 4096.
 *  2) ALIGN must use a FIXED stator angle (do not use encoder angle during align),
 *     otherwise theta_e_offset depends on the random start position and can flip
 *     torque direction between runs.
 *  3) Optional: prevent speed PI from commanding reverse torque (PREVENT_REVERSE).
 */

// ==================== User tunables ====================

// Must match TIM6 update frequency
#define CONTROL_HZ                20000.0f
#define DT_S                      (1.0f / CONTROL_HZ)

// Motor / encoder
#define POLE_PAIRS                7.0f

/* IMPORTANT: counts per MECHANICAL revolution as seen by TIM2->CNT (after x4 in encoder mode).
 * You previously measured 4096 counts/rev, so default is 4096.
 */
#define ENCODER_CPR               800U
#define ENCODER_DIR               (+1)    /* +1 or -1 to invert encoder direction */
#define THETA_E_SIGN              (+1)    /* +1 or -1 to invert electrical angle sign */

// DC bus (replace with measured if available)
#define VBUS_V                    24.0f

// Current sense scaling (UPDATE to your hardware!)
#define ADC_COUNTS_MAX            4095.0f   // 12-bit
#define ADC_VREF_V                3.0f
#define SHUNT_RES_OHMS            0.004f
#define CSA_GAIN                  20.0f

// Current protection
#define I_TRIP_A                  20.0f

// Alignment (fixed stator vector)
#define ALIGN_TIME_S              0.50f
#define ALIGN_ID_A                1.5f      // d-axis current during alignment
#define ALIGN_THETA_E_RAD         0.0f      // fixed stator electrical angle used during ALIGN

// Speed control
#define SPEED_TARGET_RPM          2000.0f
#define SPEED_KP                  0.015f
#define SPEED_KI                  0.25f     /* your 1.8 was very aggressive; start lower */
#define IQ_LIMIT_A                2.8f

/* If 1: clamp iq_ref so it never commands reverse torque for a positive target.
 * This prevents "sometimes reverse" due to speed PI overshoot/noise.
 */
#define PREVENT_REVERSE           1

// Current (Id/Iq) control
#define ID_REF_A                  0.0f
#define ID_KP                     0.08f
#define ID_KI                     8.0f
#define IQ_KP                     0.08f
#define IQ_KI                     8.0f

// Voltage limit (leave some margin for deadtime)
#define V_LIMIT_V                 (0.55f * VBUS_V)

// Duty clamp
#define DUTY_MIN                  0.02f
#define DUTY_MAX                  0.98f

// ==================== Internal state ====================

typedef enum {
    FOC_STATE_ALIGN = 0,
    FOC_STATE_RUN
} foc_state_t;

static volatile foc_state_t g_state = FOC_STATE_ALIGN;
static volatile uint32_t g_state_ticks = 0;
static volatile bool g_tripped = false;

// ADC offsets (counts)
static uint16_t off_soa = 2048, off_sob = 2048, off_soc = 2048;

// Filtered speed
static float mech_rpm = 0.0f;
static float speed_filt_rpm = 0.0f;

// Angle offsets
static float theta_e_offset = 0.0f; // electrical offset [rad]

// PI integrators
static float id_int = 0.0f;
static float iq_int = 0.0f;
static float speed_int = 0.0f;

// Debug values
static float iabc_rms = 0.0f;
static float iq_ref_A = 0.0f;

// ==================== Helpers ====================

static inline float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline float wrap_0_2pi(float x)
{
    x = fmodf(x, TWO_PI);
    if (x < 0.0f) x += TWO_PI;
    return x;
}

static inline uint16_t adc_jdr1(const ADC_HandleTypeDef *hadc)
{
    return (uint16_t)(hadc->Instance->JDR1 & 0xFFFFu);
}

// Current mapping (your note):
//  - ADC3 injected channel -> SOA (PF10)
//  - ADC2 injected channel -> SOB (PC5)
//  - ADC1 injected channel -> SOC (PA7)
static inline void read_phase_currents(float *ia, float *ib, float *ic)
{
    uint16_t raw_soa = adc_jdr1(&hadc3);
    uint16_t raw_sob = adc_jdr1(&hadc2);
    uint16_t raw_soc = adc_jdr1(&hadc1);

    int32_t d_soa = (int32_t)raw_soa - (int32_t)off_soa;
    int32_t d_sob = (int32_t)raw_sob - (int32_t)off_sob;
    int32_t d_soc = (int32_t)raw_soc - (int32_t)off_soc;

    float v_lsb = ADC_VREF_V / ADC_COUNTS_MAX;
    float v_soa = (float)d_soa * v_lsb;
    float v_sob = (float)d_sob * v_lsb;
    float v_soc = (float)d_soc * v_lsb;

    float denom = SHUNT_RES_OHMS * CSA_GAIN;
    if (denom <= 0.0f) denom = 1.0f;

    /* NOTE: sign depends on shunt orientation / CSA polarity.
     * If torque direction seems "wrong" even after ALIGN, flip signs here.
     */
    *ia = v_soa / denom;
    *ib = v_sob / denom;
    *ic = v_soc / denom;
}

static inline void clarke(float ia, float ib, float ic, float *ialpha, float *ibeta)
{
    (void)ic;
    *ialpha = ia;
    *ibeta  = (ia + 2.0f * ib) * 0.57735026919f; // 1/sqrt(3)
}

static inline void park(float ialpha, float ibeta, float sin_t, float cos_t, float *id, float *iq)
{
    *id =  ialpha * cos_t + ibeta * sin_t;
    *iq = -ialpha * sin_t + ibeta * cos_t;
}

static inline void inv_park(float vd, float vq, float sin_t, float cos_t, float *valpha, float *vbeta)
{
    *valpha = vd * cos_t - vq * sin_t;
    *vbeta  = vd * sin_t + vq * cos_t;
}

static inline void svpwm_from_vab(float valpha, float vbeta, float vbus, float *da, float *db, float *dc)
{
    const float k = 0.86602540378f; // sqrt(3)/2
    float va = valpha;
    float vb = -0.5f * valpha + k * vbeta;
    float vc = -0.5f * valpha - k * vbeta;

    // Zero-sequence injection
    float vmax = fmaxf(va, fmaxf(vb, vc));
    float vmin = fminf(va, fminf(vb, vc));
    float voff = -0.5f * (vmax + vmin);

    va += voff;
    vb += voff;
    vc += voff;

    // duty = 0.5 + v_phase / vbus
    *da = clampf(0.5f + (va / vbus), DUTY_MIN, DUTY_MAX);
    *db = clampf(0.5f + (vb / vbus), DUTY_MIN, DUTY_MAX);
    *dc = clampf(0.5f + (vc / vbus), DUTY_MIN, DUTY_MAX);
}

static inline float get_mech_angle_rad(uint32_t cnt)
{
    uint32_t mod = cnt % ENCODER_CPR;
    float theta = TWO_PI * ((float)mod / (float)ENCODER_CPR);
    theta *= (float)ENCODER_DIR;
    return wrap_0_2pi(theta);
}

static inline void update_speed_estimate(void)
{
    static uint32_t last_cnt = 0;

    uint32_t cnt = __HAL_TIM_GET_COUNTER(&htim2);
    int32_t dc = (int32_t)(cnt - last_cnt);
    last_cnt = cnt;

    float rev_per_tick = ((float)ENCODER_DIR * (float)dc) / (float)ENCODER_CPR;
    float rev_per_s = rev_per_tick * CONTROL_HZ;
    mech_rpm = rev_per_s * 60.0f;

    // LPF (make slower than 1ms to reduce quantization noise)
    const float alpha = 0.01f; // ~5ms time constant at 20kHz
    speed_filt_rpm += alpha * (mech_rpm - speed_filt_rpm);
}

static inline void set_pwm_duty(float da, float db, float dc)
{
    uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1U;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(da * (float)period));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(db * (float)period));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint32_t)(dc * (float)period));
}

static inline void foc_disable_outputs(void)
{
    __HAL_TIM_MOE_DISABLE(&htim1);

#ifdef EN_DRV_GPIO_Port
#ifdef EN_DRV_Pin
    HAL_GPIO_WritePin(EN_DRV_GPIO_Port, EN_DRV_Pin, GPIO_PIN_RESET);
#endif
#endif

    set_pwm_duty(0.5f, 0.5f, 0.5f);
}

// ==================== Public API ====================

void FOC_Init(void)
{
    g_state = FOC_STATE_ALIGN;
    g_state_ticks = 0;
    g_tripped = false;

    id_int = iq_int = speed_int = 0.0f;
    theta_e_offset = 0.0f;

    mech_rpm = 0.0f;
    speed_filt_rpm = 0.0f;
    iabc_rms = 0.0f;
    iq_ref_A = 0.0f;
}

void FOC_CalibrateCurrentOffsets(uint32_t samples, uint32_t delay_ms)
{
    if (samples == 0U) samples = 1U;

    HAL_GPIO_WritePin(CAL_GPIO_Port, CAL_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    uint32_t sum_soa = 0, sum_sob = 0, sum_soc = 0;
    for (uint32_t i = 0; i < samples; i++)
    {
        HAL_Delay(delay_ms);
        sum_soa += adc_jdr1(&hadc3);
        sum_sob += adc_jdr1(&hadc2);
        sum_soc += adc_jdr1(&hadc1);
    }

    off_soa = (uint16_t)(sum_soa / samples);
    off_sob = (uint16_t)(sum_sob / samples);
    off_soc = (uint16_t)(sum_soc / samples);

    HAL_GPIO_WritePin(CAL_GPIO_Port, CAL_Pin, GPIO_PIN_RESET);
}

void FOC_ControlStep(void)
{
    if (g_tripped)
        return;

    // Read currents
    float ia, ib, ic;
    read_phase_currents(&ia, &ib, &ic);

    // Diagnostics RMS
    iabc_rms = sqrtf((ia*ia + ib*ib + ic*ic) / 3.0f);

    // Hard current trip
    float i_peak = fmaxf(fabsf(ia), fmaxf(fabsf(ib), fabsf(ic)));
    if (i_peak > I_TRIP_A)
    {
        g_tripped = true;
        foc_disable_outputs();
        return;
    }

    // Encoder angle and speed
    update_speed_estimate();
    float theta_m = get_mech_angle_rad(__HAL_TIM_GET_COUNTER(&htim2));

    // Electrical angle from encoder + offset (RUN mode)
    float theta_e_meas = (float)THETA_E_SIGN * (POLE_PAIRS * theta_m + theta_e_offset);
    theta_e_meas = wrap_0_2pi(theta_e_meas);

    // ALIGN uses FIXED stator angle; RUN uses measured electrical angle
    float theta_ctrl = (g_state == FOC_STATE_ALIGN) ? ALIGN_THETA_E_RAD : theta_e_meas;

    float sin_t = sinf(theta_ctrl);
    float cos_t = cosf(theta_ctrl);

    // Clarke/Park
    float ialpha, ibeta;
    clarke(ia, ib, ic, &ialpha, &ibeta);

    float id, iq;
    park(ialpha, ibeta, sin_t, cos_t, &id, &iq);

    // State machine refs
    float id_ref = ID_REF_A;
    float iq_ref = 0.0f;

    if (g_state == FOC_STATE_ALIGN)
    {
        // Force fixed stator d-axis current to physically align rotor
        id_ref = ALIGN_ID_A;
        iq_ref = 0.0f;

        g_state_ticks++;
        if (g_state_ticks >= (uint32_t)(ALIGN_TIME_S * CONTROL_HZ))
        {
            // After alignment, compute encoder->electrical offset so that
            // theta_e_meas == ALIGN_THETA_E_RAD at this aligned position.
            theta_e_offset = (ALIGN_THETA_E_RAD / (float)THETA_E_SIGN) - (POLE_PAIRS * theta_m);
            theta_e_offset = wrap_0_2pi(theta_e_offset);

            // Reset integrators to avoid bump
            id_int = iq_int = speed_int = 0.0f;

            g_state = FOC_STATE_RUN;
            g_state_ticks = 0;
        }
    }
    else
    {
        // Outer speed PI -> iq_ref
        float e = SPEED_TARGET_RPM - speed_filt_rpm;

        speed_int += SPEED_KI * e * DT_S;
        speed_int = clampf(speed_int, -IQ_LIMIT_A, IQ_LIMIT_A);

        iq_ref = SPEED_KP * e + speed_int;
        iq_ref = clampf(iq_ref, -IQ_LIMIT_A, IQ_LIMIT_A);

#if PREVENT_REVERSE
        if (SPEED_TARGET_RPM >= 0.0f)
        {
            if (iq_ref < 0.0f) iq_ref = 0.0f;
            if (speed_int < 0.0f) speed_int = 0.0f;
        }
        else
        {
            if (iq_ref > 0.0f) iq_ref = 0.0f;
            if (speed_int > 0.0f) speed_int = 0.0f;
        }
#endif
    }

    iq_ref_A = iq_ref;

    // Current PI controllers -> voltage commands (vd/vq)
    float ed = id_ref - id;
    float eq = iq_ref - iq;

    id_int += ID_KI * ed * DT_S;
    iq_int += IQ_KI * eq * DT_S;

    // Basic integrator clamps
    id_int = clampf(id_int, -V_LIMIT_V, V_LIMIT_V);
    iq_int = clampf(iq_int, -V_LIMIT_V, V_LIMIT_V);

    float vd = ID_KP * ed + id_int;
    float vq = IQ_KP * eq + iq_int;

    // Voltage magnitude limit
    float vmag = sqrtf(vd*vd + vq*vq);
    if (vmag > V_LIMIT_V && vmag > 0.001f)
    {
        float s = V_LIMIT_V / vmag;
        vd *= s;
        vq *= s;
    }

    // Inverse Park -> alpha/beta
    float valpha, vbeta;
    inv_park(vd, vq, sin_t, cos_t, &valpha, &vbeta);

    // SVPWM duties
    float da, db, dc;
    svpwm_from_vab(valpha, vbeta, VBUS_V, &da, &db, &dc);

    set_pwm_duty(da, db, dc);
}

bool FOC_IsTripped(void)
{
    return g_tripped;
}

float FOC_GetMechRpm(void)
{
    return speed_filt_rpm;
}

float FOC_GetIabcRms(void)
{
    return iabc_rms;
}

float FOC_GetIqRef(void)
{
    return iq_ref_A;
}
