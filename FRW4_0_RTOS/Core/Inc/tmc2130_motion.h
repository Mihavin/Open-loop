#ifndef TMC2130_MOTION_H
#define TMC2130_MOTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "tmc2130.h"

/*
 * Non-blocking acceleration-limited TMC2130 motion helper.
 *
 * Example integration (after MX_SPI3_Init() and MX_TIM12_Init()):
 *
 *   static TMC2130_Handle_t g_tmc2130;
 *   static TMC2130Motion_Handle_t g_stepper_motion;
 *
 *   TMC2130_Config_t cfg;
 *   TMC2130_ConfigDefaults(&cfg);
 *   TMC2130_Init(&g_tmc2130, &hspi3, &htim12, TIM_CHANNEL_2, &cfg);
 *
 *   TMC2130Motion_Limits_t limits;
 *   TMC2130Motion_DefaultLimits(&limits);
 *   TMC2130Motion_Init(&g_stepper_motion, &g_tmc2130, &limits);
 *   TMC2130Motion_Enable(&g_stepper_motion, true);
 *
 * Periodic task, typically every 1 ms:
 *
 *   TMC2130Motion_Task(&g_stepper_motion, 0.001f);
 */

typedef enum
{
    TMC2130_MOTION_IDLE = 0,
    TMC2130_MOTION_MOVING,
    TMC2130_MOTION_SYNC,
    TMC2130_MOTION_STOPPING,
    TMC2130_MOTION_FAULT
} TMC2130Motion_State_t;

typedef struct
{
    float max_speed_steps_s;
    float accel_steps_s2;
    float min_run_speed_steps_s;
    float position_tolerance_steps;
} TMC2130Motion_Limits_t;

typedef struct
{
    TMC2130_Handle_t *driver;
    TMC2130Motion_Limits_t limits;

    TMC2130Motion_State_t state;

    float position_steps;
    float target_steps;
    float velocity_steps_s;

    bool enabled;
    bool sync_enabled;

    float sync_scale_steps_per_bldc_count;
    float sync_offset_steps;
} TMC2130Motion_Handle_t;

void TMC2130Motion_DefaultLimits(TMC2130Motion_Limits_t *limits);

HAL_StatusTypeDef TMC2130Motion_Init(TMC2130Motion_Handle_t *motion,
                                     TMC2130_Handle_t *driver,
                                     const TMC2130Motion_Limits_t *limits);

HAL_StatusTypeDef TMC2130Motion_Enable(TMC2130Motion_Handle_t *motion, bool enable);

HAL_StatusTypeDef TMC2130Motion_MoveTo(TMC2130Motion_Handle_t *motion, float target_steps);
HAL_StatusTypeDef TMC2130Motion_MoveBy(TMC2130Motion_Handle_t *motion, float delta_steps);

HAL_StatusTypeDef TMC2130Motion_Stop(TMC2130Motion_Handle_t *motion);
HAL_StatusTypeDef TMC2130Motion_EmergencyStop(TMC2130Motion_Handle_t *motion);

void TMC2130Motion_Task(TMC2130Motion_Handle_t *motion, float dt_s);

HAL_StatusTypeDef TMC2130Motion_EnableBldcSync(TMC2130Motion_Handle_t *motion,
                                               float scale_steps_per_bldc_count,
                                               float offset_steps);

void TMC2130Motion_DisableBldcSync(TMC2130Motion_Handle_t *motion);

TMC2130Motion_State_t TMC2130Motion_GetState(const TMC2130Motion_Handle_t *motion);
float TMC2130Motion_GetPositionSteps(const TMC2130Motion_Handle_t *motion);
float TMC2130Motion_GetTargetSteps(const TMC2130Motion_Handle_t *motion);
float TMC2130Motion_GetVelocityStepsS(const TMC2130Motion_Handle_t *motion);

#ifdef __cplusplus
}
#endif

#endif /* TMC2130_MOTION_H */
