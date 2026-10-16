#include "tmc2130_motion.h"
//test
#include "motor_foc.h"
#include "fault_manager.h"
#include <string.h>
#include <math.h>

#define TMC2130_MOTION_DEFAULT_MAX_SPEED_STEPS_S       2000.0f
#define TMC2130_MOTION_DEFAULT_ACCEL_STEPS_S2          4000.0f
#define TMC2130_MOTION_DEFAULT_MIN_RUN_SPEED_STEPS_S     10.0f
#define TMC2130_MOTION_DEFAULT_POSITION_TOL_STEPS         0.5f
#define TMC2130_MOTION_MAX_DT_S                           0.1f

static float motion_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float motion_signf(float value)
{
    return (value < 0.0f) ? -1.0f : 1.0f;
}

static float motion_clampf(float value, float min_value, float max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static uint32_t motion_frequency_from_speed(float speed_steps_s)
{
    float freq = motion_absf(speed_steps_s);
    if (freq < 0.5f)
        return 0u;
    /* The 16-bit compare engine supports >=8 Hz at its 1 MHz timebase.
       Position still comes from emitted edges, including this slow tail. */
    if (freq < 8.0f)
        return 8u;

    if (freq > 4294967295.0f)
        return 0xFFFFFFFFu;

    return (uint32_t)(freq + 0.5f);
}

static void motion_stop_output(TMC2130Motion_Handle_t *motion)
{
    if ((motion != NULL) && (motion->driver != NULL))
        (void)TMC2130_StopStepClock(motion->driver);
}

static HAL_StatusTypeDef motion_apply_output(TMC2130Motion_Handle_t *motion)
{
    uint32_t frequency_hz = motion_frequency_from_speed(motion->velocity_steps_s);

    if (frequency_hz == 0u)
    {
        motion_stop_output(motion);
        return HAL_OK;
    }

    int32_t direction = motion->velocity_steps_s < 0 ? -1 : 1;
    if (motion->driver->step_direction != direction)
    {
        if (TMC2130_IsStepClockRunning(motion->driver))
            return TMC2130_StopStepClock(motion->driver);
        TMC2130_SetDirection(direction < 0 ? TMC2130_DIR_REVERSE : TMC2130_DIR_FORWARD);
    }
    int32_t target = (int32_t)roundf(motion->target_steps);
    if (motion->state == TMC2130_MOTION_STOPPING)
        target = TMC2130_GetEmittedSteps(motion->driver) + direction * 1000;
    TMC2130_SetStepTarget(motion->driver, target);
    if (TMC2130_IsStepClockRunning(motion->driver))
        return TMC2130_SetStepFrequency(motion->driver, frequency_hz);
    return TMC2130_StartStepClock(motion->driver, frequency_hz);
}

void TMC2130Motion_DefaultLimits(TMC2130Motion_Limits_t *limits)
{
    if (limits == NULL)
        return;

    limits->max_speed_steps_s = TMC2130_MOTION_DEFAULT_MAX_SPEED_STEPS_S;
    limits->accel_steps_s2 = TMC2130_MOTION_DEFAULT_ACCEL_STEPS_S2;
    limits->min_run_speed_steps_s = TMC2130_MOTION_DEFAULT_MIN_RUN_SPEED_STEPS_S;
    limits->position_tolerance_steps = TMC2130_MOTION_DEFAULT_POSITION_TOL_STEPS;
}

HAL_StatusTypeDef TMC2130Motion_Init(TMC2130Motion_Handle_t *motion,
                                     TMC2130_Handle_t *driver,
                                     const TMC2130Motion_Limits_t *limits)
{
    if ((motion == NULL) || (driver == NULL))
        return HAL_ERROR;

    memset(motion, 0, sizeof(*motion));
    motion->driver = driver;

    if (limits != NULL)
        motion->limits = *limits;
    else
        TMC2130Motion_DefaultLimits(&motion->limits);

    if ((motion->limits.max_speed_steps_s <= 0.0f) ||
        (motion->limits.accel_steps_s2 <= 0.0f) ||
        (motion->limits.position_tolerance_steps < 0.0f))
    {
        motion->state = TMC2130_MOTION_FAULT;
        return HAL_ERROR;
    }

    motion->limits.min_run_speed_steps_s = motion_clampf(motion->limits.min_run_speed_steps_s,
                                                         0.0f,
                                                         motion->limits.max_speed_steps_s);
    motion->state = TMC2130_MOTION_IDLE;
    motion_stop_output(motion);
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130Motion_Enable(TMC2130Motion_Handle_t *motion, bool enable)
{
    if ((motion == NULL) || (motion->driver == NULL))
        return HAL_ERROR;

    if (enable && Fault_IsCriticalActive())
        return HAL_ERROR;

    motion->enabled = enable;

    if (enable && (motion->state == TMC2130_MOTION_FAULT))
    {
        motion->velocity_steps_s = 0.0f;
        motion->target_steps = motion->position_steps;
        motion->sync_enabled = false;
        motion->state = TMC2130_MOTION_IDLE;
    }

    if (!enable)
    {
        motion->velocity_steps_s = 0.0f;
        motion_stop_output(motion);
        motion->state = TMC2130_MOTION_IDLE;
    }

    return TMC2130_Enable(motion->driver, enable);
}

HAL_StatusTypeDef TMC2130Motion_MoveTo(TMC2130Motion_Handle_t *motion, float target_steps)
{
    if ((motion == NULL) || (motion->driver == NULL) || !motion->enabled ||
        Fault_IsCriticalActive())
    {
        return HAL_ERROR;
    }

    if (!isfinite(target_steps) || fabsf(target_steps) > 1000000.0f) return HAL_ERROR;
    motion->sync_enabled = false;
    motion->target_steps = roundf(target_steps);
    motion->state = TMC2130_MOTION_MOVING;
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130Motion_MoveBy(TMC2130Motion_Handle_t *motion, float delta_steps)
{
    if (motion == NULL)
        return HAL_ERROR;

    return TMC2130Motion_MoveTo(motion, motion->target_steps + delta_steps);
}

HAL_StatusTypeDef TMC2130Motion_Stop(TMC2130Motion_Handle_t *motion)
{
    if ((motion == NULL) || (motion->driver == NULL))
        return HAL_ERROR;

    motion->sync_enabled = false;
    motion->target_steps = motion->position_steps;
    motion->state = TMC2130_MOTION_STOPPING;
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130Motion_EmergencyStop(TMC2130Motion_Handle_t *motion)
{
    if ((motion == NULL) || (motion->driver == NULL))
        return HAL_ERROR;

    Fault_Report(FAULT_USER_STOP); /* Never wait for a normal pulse-end stop. */
    motion->enabled = false;
    motion->sync_enabled = false;
    motion->velocity_steps_s = 0.0f;
    motion->target_steps = motion->position_steps;
    motion_stop_output(motion);
    motion->state = TMC2130_MOTION_FAULT;
    return HAL_OK;
}

void TMC2130Motion_Task(TMC2130Motion_Handle_t *motion, float dt_s)
{
    if ((motion == NULL) || (motion->driver == NULL)) return;
    motion->position_steps = (float)TMC2130_GetEmittedSteps(motion->driver);
    if (!motion->enabled) return;

    if (Fault_IsCriticalActive())
    {
        motion->sync_enabled = false;
        motion->velocity_steps_s = 0.0f;
        motion->target_steps = motion->position_steps;
        motion_stop_output(motion);
        motion->state = TMC2130_MOTION_FAULT;
        return;
    }

    if ((motion->state == TMC2130_MOTION_FAULT) || (dt_s <= 0.0f))
        return;

    if (!isfinite(dt_s) || dt_s > TMC2130_MOTION_MAX_DT_S)
    { Fault_Report(FAULT_MOTION_TIMEOUT); return; }

    if (motion->sync_enabled && (motion->state != TMC2130_MOTION_STOPPING))
    {
        motion->target_steps = motion->sync_offset_steps +
                               ((float)MotorFOC_GetPositionCounts() *
                                motion->sync_scale_steps_per_bldc_count);
        if (!isfinite(motion->target_steps) || fabsf(motion->target_steps) > 1000000.0f)
        { Fault_Report(FAULT_STEPPER_DRIVER); return; }
        motion->target_steps = roundf(motion->target_steps);
        motion->state = TMC2130_MOTION_SYNC;
    }

    float desired_velocity = 0.0f;
    float error = motion->target_steps - motion->position_steps;
    float abs_error = motion_absf(error);
    float abs_velocity = motion_absf(motion->velocity_steps_s);

    if (motion->state == TMC2130_MOTION_STOPPING)
    {
        desired_velocity = 0.0f;
    }
    else if ((abs_error <= motion->limits.position_tolerance_steps) &&
             (abs_velocity <= motion->limits.min_run_speed_steps_s))
    {
        motion->velocity_steps_s = 0.0f;
        motion_stop_output(motion);
        if (!motion->sync_enabled)
            motion->state = TMC2130_MOTION_IDLE;
        return;
    }
    else
    {
        float target_dir = motion_signf(error);
        float stopping_distance = (abs_velocity * abs_velocity) /
                                  (2.0f * motion->limits.accel_steps_s2);

        if (stopping_distance >= abs_error)
            desired_velocity = 0.0f;
        else
            desired_velocity = target_dir * motion->limits.max_speed_steps_s;
    }

    float velocity_delta = motion->limits.accel_steps_s2 * dt_s;
    if (motion->velocity_steps_s < desired_velocity)
    {
        motion->velocity_steps_s += velocity_delta;
        if (motion->velocity_steps_s > desired_velocity)
            motion->velocity_steps_s = desired_velocity;
    }
    else if (motion->velocity_steps_s > desired_velocity)
    {
        motion->velocity_steps_s -= velocity_delta;
        if (motion->velocity_steps_s < desired_velocity)
            motion->velocity_steps_s = desired_velocity;
    }

    if ((desired_velocity != 0.0f) &&
        (motion_absf(motion->velocity_steps_s) < motion->limits.min_run_speed_steps_s))
    {
        motion->velocity_steps_s = motion_signf(desired_velocity) *
                                   motion->limits.min_run_speed_steps_s;
    }

    if ((motion->state == TMC2130_MOTION_STOPPING) &&
        (motion_absf(motion->velocity_steps_s) < 0.5f))
    {
        motion->velocity_steps_s = 0.0f;
        motion->target_steps = motion->position_steps;
        motion_stop_output(motion);
        motion->state = TMC2130_MOTION_IDLE;
        return;
    }

    if (motion_apply_output(motion) != HAL_OK)
    {
        motion->velocity_steps_s = 0.0f;
        motion_stop_output(motion);
        motion->state = TMC2130_MOTION_FAULT;
        Fault_Report(FAULT_STEPPER_DRIVER);
    }
}

HAL_StatusTypeDef TMC2130Motion_EnableBldcSync(TMC2130Motion_Handle_t *motion,
                                               float scale_steps_per_bldc_count,
                                               float offset_steps)
{
    if ((motion == NULL) || (motion->driver == NULL) || !motion->enabled ||
        Fault_IsCriticalActive())
    {
        return HAL_ERROR;
    }

    motion->sync_scale_steps_per_bldc_count = scale_steps_per_bldc_count;
    motion->sync_offset_steps = offset_steps;
    motion->sync_enabled = true;
    motion->state = TMC2130_MOTION_SYNC;
    return HAL_OK;
}

void TMC2130Motion_DisableBldcSync(TMC2130Motion_Handle_t *motion)
{
    if (motion == NULL)
        return;

    motion->sync_enabled = false;
    if (motion->state == TMC2130_MOTION_SYNC)
        motion->state = TMC2130_MOTION_IDLE;
}

TMC2130Motion_State_t TMC2130Motion_GetState(const TMC2130Motion_Handle_t *motion)
{
    return (motion != NULL) ? motion->state : TMC2130_MOTION_FAULT;
}

float TMC2130Motion_GetPositionSteps(const TMC2130Motion_Handle_t *motion)
{
    return (motion != NULL) ? motion->position_steps : 0.0f;
}

float TMC2130Motion_GetTargetSteps(const TMC2130Motion_Handle_t *motion)
{
    return (motion != NULL) ? motion->target_steps : 0.0f;
}

float TMC2130Motion_GetVelocityStepsS(const TMC2130Motion_Handle_t *motion)
{
    return (motion != NULL) ? motion->velocity_steps_s : 0.0f;
}
