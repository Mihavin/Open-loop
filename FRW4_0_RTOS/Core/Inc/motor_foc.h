/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    motor_foc.h
  * @brief   Simple Field Oriented Control (FOC) module for 3-phase BLDC/PMSM.
  *
  * Hardware mapping is taken from the CubeMX .ioc in this project:
  *   - PWM: TIM1 CH1/CH1N = Phase A (INHA/INLA)
  *          TIM1 CH2/CH2N = Phase B (INHB/INLB)
  *          TIM1 CH3/CH3N = Phase C (INHC/INLC)
  *   - Current sense (injected ADC, triggered by TIM1 TRGO2):
  *          SOA -> PC4  -> ADC1  (Phase A current)
  *          SOB -> PA6  -> ADC2  (Phase B current)
  *          SOC -> PF7  -> ADC3  (Phase C current)
  *   - Encoder: TIM2 CH1/CH2 on PA0/PA1
  *   - Driver enable: EN_DRV pin (GPIOC, PC14)
  *
  * Notes:
  *  - You MUST set the motor/board-specific constants below (shunt, gain, pole pairs, encoder CPR, bus voltage).
  *  - Current loop runs from ADC injected conversion complete callback (typically at PWM frequency).
  *  - Speed loop runs from TIM7 interrupt (default 1 kHz as configured in .ioc).
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef MOTOR_FOC_H
#define MOTOR_FOC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
  MOTOR_FOC_STATE_IDLE = 0,
  MOTOR_FOC_STATE_CALIB,
  MOTOR_FOC_STATE_ALIGN,
  MOTOR_FOC_STATE_RUN,
  MOTOR_FOC_STATE_FAULT,
} MotorFOC_State_t;

typedef enum
{
  MOTOR_FOC_MODE_TORQUE = 0,
  MOTOR_FOC_MODE_SPEED,
  MOTOR_FOC_MODE_POSITION,
} MotorFOC_Mode_t;

/* --------------------------- User tuning section --------------------------- */
/* Motor parameters */
#ifndef MOTOR_FOC_POLE_PAIRS
#define MOTOR_FOC_POLE_PAIRS          (7)          /* electrical pole pairs */
#endif

#ifndef MOTOR_FOC_ENCODER_CPR
#define MOTOR_FOC_ENCODER_CPR         (4000)   /* encoder counts per mechanical revolution (x4 for quadrature) */
#endif

/* Current sensing parameters (set to your hardware) */
#ifndef MOTOR_FOC_SHUNT_OHMS
#define MOTOR_FOC_SHUNT_OHMS          (0.004f)     /* shunt resistor in ohms */
#endif

#ifndef MOTOR_FOC_AMP_GAIN
#define MOTOR_FOC_AMP_GAIN            (40.0f)      /* current sense amplifier gain */
#endif

#ifndef MOTOR_FOC_ADC_VREF
#define MOTOR_FOC_ADC_VREF            (3.35f)       /* ADC reference voltage */
#endif

#ifndef MOTOR_FOC_ADC_FULL_SCALE
#define MOTOR_FOC_ADC_FULL_SCALE      (65536.0f)    /* 12-bit ADC -> 4096 */
#endif

/* DC bus voltage (if you don't measure it, set nominal here) */
#ifndef MOTOR_FOC_VBUS_VOLTS
#define MOTOR_FOC_VBUS_VOLTS          (24.0f)
#endif

/* Control rates */
#ifndef MOTOR_FOC_PWM_HZ
#define MOTOR_FOC_PWM_HZ              (40000.0f)   /* should match TIM1 PWM frequency */
#endif

#ifndef MOTOR_FOC_SPEED_HZ
#define MOTOR_FOC_SPEED_HZ            (1000.0f)    /* should match TIM7 update rate */
#endif

/* Limits */
#ifndef MOTOR_FOC_ID_MAX_A
#define MOTOR_FOC_ID_MAX_A            (1.6f)
#endif

#ifndef MOTOR_FOC_IQ_MAX_A
#define MOTOR_FOC_IQ_MAX_A            (1.6f)
#endif

#ifndef MOTOR_FOC_SPEED_MAX_RPM
#define MOTOR_FOC_SPEED_MAX_RPM       (3000.0f)
#endif

/* Alignment (encoder offset calibration) */
#ifndef MOTOR_FOC_ALIGN_TIME_MS
#define MOTOR_FOC_ALIGN_TIME_MS       (700U)
#endif

#ifndef MOTOR_FOC_ALIGN_ID_A
#define MOTOR_FOC_ALIGN_ID_A          (1.5f)       /* d-axis current during alignment (A) */
#endif

/* PI gains (start small!) */
#ifndef MOTOR_FOC_KP_ID
#define MOTOR_FOC_KP_ID               (0.05f)
#endif
#ifndef MOTOR_FOC_KI_ID
#define MOTOR_FOC_KI_ID               (30.0f)
#endif

#ifndef MOTOR_FOC_KP_IQ
#define MOTOR_FOC_KP_IQ               (0.05f)
#endif
#ifndef MOTOR_FOC_KI_IQ
#define MOTOR_FOC_KI_IQ               (30.0f)
#endif

#ifndef MOTOR_FOC_KP_SPEED
#define MOTOR_FOC_KP_SPEED            (0.025f)
#endif
#ifndef MOTOR_FOC_KI_SPEED
#define MOTOR_FOC_KI_SPEED            (1.60f)
#endif

/* Position loop gains (outer-most loop).
 *
 * Position mode uses a cascaded control structure:
 *   position PI  -> speed_ref_rpm
 *   speed PI      -> iq_ref (torque current)
 *   current PI    -> voltage (SVPWM)
 *
 * The position controller error is expressed in mechanical revolutions.
 * Therefore:
 *   - MOTOR_FOC_KP_POS units: RPM / rev
 *   - MOTOR_FOC_KI_POS units: RPM / (rev*s)
 */
#ifndef MOTOR_FOC_KP_POS
#define MOTOR_FOC_KP_POS              (300.0f)
#endif
#ifndef MOTOR_FOC_KI_POS
#define MOTOR_FOC_KI_POS              (0.0015f)
#endif

/* Autostart behaviour:
 *  - If 0: only calibrates current offsets and stays IDLE. Call MotorFOC_Arm() to enable.
 *  - If 1: after offset calibration it performs alignment and enters RUN automatically.
 */
#ifndef MOTOR_FOC_AUTOSTART
#define MOTOR_FOC_AUTOSTART           (1)
#endif
/* -------------------------------------------------------------------------- */

void MotorFOC_Init(void);
void MotorFOC_Start(void);
void MotorFOC_Stop(void);

/* Queue driver enable and alignment; applied by the MotorFOC() slow loop. */
void MotorFOC_Arm(void);

/* Queue driver disable; applied by the MotorFOC() slow loop. */
void MotorFOC_Disarm(void);

void MotorFOC_RequestArm(void);
void MotorFOC_RequestDisarm(void);
void MotorFOC_RequestEmergencyStop(void);
void MotorFOC_RequestZeroEncoder(void);

void MotorFOC_SetMode(MotorFOC_Mode_t mode);
void MotorFOC_SetCommand(MotorFOC_Mode_t mode,
                         float speed_rpm,
                         float torque_iq_A,
                         int32_t position_counts);

void MotorFOC(void);

void MotorFOC_SetSpeedRPM(float rpm);
void MotorFOC_SetTorqueCurrentA(float iq_A);

/* Position mode setpoint.
 *
 * NOTE: Position is expressed in encoder counts of TIM2 (quadrature counts).
 * This is an absolute count (multi-turn) as provided by TIM2. If you want a
 * 0..CPR-1 angle, wrap externally before calling.
 */
void MotorFOC_SetPositionCounts(int32_t pos_counts);

/* Convenience helper: set an absolute mechanical position in revolutions.
 * revolutions * MOTOR_FOC_ENCODER_CPR -> encoder counts.
 */
void MotorFOC_SetPositionRevolutions(float revolutions);

MotorFOC_State_t MotorFOC_GetState(void);
MotorFOC_Mode_t  MotorFOC_GetMode(void);

/* Optional debug getters (updated in ISR) */
float MotorFOC_GetIa(void);
float MotorFOC_GetIb(void);
float MotorFOC_GetIc(void);
float MotorFOC_GetId(void);
float MotorFOC_GetIq(void);
float MotorFOC_GetSpeedRPM(void);
int32_t MotorFOC_GetPositionCounts(void);
void Motor_OpenLoopStep_20kHz(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_FOC_H */
