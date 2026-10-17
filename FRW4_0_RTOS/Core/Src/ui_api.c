#include "ui_api.h"

#include <string.h>

#include "fault_manager.h"
#include "cmsis_os2.h"
#include "main.h"
#include <math.h>

typedef enum { STEP_ENABLE, STEP_TO, STEP_BY, STEP_STOP, STEP_ZERO, STEP_SYNC, STEP_UNSYNC } StepCommandKind;
typedef struct { StepCommandKind kind; float a, b; } StepCommand;
static osMessageQueueId_t g_step_queue;
static UI_SystemSnapshot_t g_step_snapshot;
static volatile uint32_t g_source = UI_CONTROL_KNOB;

void UI_InitCommandQueues(void)
{
    g_step_queue = osMessageQueueNew(16, sizeof(StepCommand), NULL);
    if (!g_step_queue) Fault_Report(FAULT_CPU);
}

UI_ControlSource_t UI_GetControlSource(void)
{
    return (UI_ControlSource_t)__atomic_load_n(&g_source, __ATOMIC_ACQUIRE);
}

bool UI_SelectControlSource(UI_ControlSource_t source)
{
    uint32_t key = __get_PRIMASK(); __disable_irq();
    if ((unsigned)source > UI_CONTROL_API || MotorFOC_GetState() != MOTOR_FOC_STATE_IDLE ||
        fabsf(MotorFOC_GetSpeedRPM()) > 10.0f) { __set_PRIMASK(key); return false; }
    __atomic_store_n(&g_source, source, __ATOMIC_RELEASE);
    __set_PRIMASK(key);
    return true;
}

static void claim_api(void)
{
    __atomic_store_n(&g_source, UI_CONTROL_API, __ATOMIC_RELEASE);
}

static void queue_step(StepCommandKind kind, float a, float b)
{
    StepCommand command = {kind, a, b};
    if (!isfinite(a) || !isfinite(b) || !g_step_queue ||
        osMessageQueuePut(g_step_queue, &command, 0, 0) != osOK)
        Fault_Report(FAULT_STEPPER_DRIVER);
}

extern TMC2130Motion_Handle_t g_stepper_motion;

void UI_GetSnapshot(UI_SystemSnapshot_t *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    out->bldc_state = MotorFOC_GetState();
    out->bldc_mode = MotorFOC_GetMode();
    out->bldc_speed_rpm = MotorFOC_GetSpeedRPM();
    out->bldc_position_counts = MotorFOC_GetPositionCounts();
    out->bldc_position_ref_counts = MotorFOC_GetPositionRefCounts();
    out->ia = MotorFOC_GetIa();
    out->ib = MotorFOC_GetIb();
    out->ic = MotorFOC_GetIc();

    uint32_t key = __get_PRIMASK(); __disable_irq();
    out->stepper_state = g_step_snapshot.stepper_state;
    out->stepper_position_steps = g_step_snapshot.stepper_position_steps;
    out->stepper_target_steps = g_step_snapshot.stepper_target_steps;
    out->stepper_velocity_steps_s = g_step_snapshot.stepper_velocity_steps_s;
    __set_PRIMASK(key);

    out->faults = Fault_GetActive();
}

void UI_SetBldcMode(MotorFOC_Mode_t mode)
{
    claim_api();
    MotorFOC_SetMode(mode);
}

void UI_SetBldcSpeed(float rpm)
{
    claim_api();
    MotorFOC_SetCommand(MOTOR_FOC_MODE_SPEED, rpm, 0.0f, MotorFOC_GetPositionCounts());
}

void UI_SetBldcTorque(float iq_a)
{
    claim_api();
    MotorFOC_SetCommand(MOTOR_FOC_MODE_TORQUE, 0.0f, iq_a, MotorFOC_GetPositionCounts());
}

void UI_SetBldcPositionRev(float rev)
{
    claim_api();
    MotorFOC_SetPositionRevolutions(rev);
}

void UI_SetBldcLimits(float speed_limit_rpm, float iq_limit_a)
{
    claim_api();
    MotorFOC_SetOperatingLimits(speed_limit_rpm, iq_limit_a);
}

void UI_SetBldcPositionSnap(float pitch_deg, int32_t origin_counts, float hold_iq_a)
{
    claim_api();
    int32_t counts = 0;
    if (pitch_deg > 0.0f && pitch_deg <= 360.0f)
        counts = (int32_t)(pitch_deg / 360.0f * (float)MOTOR_FOC_ENCODER_CPR + 0.5f);
    MotorFOC_SetPositionSnap(counts, origin_counts, hold_iq_a);
}

void UI_BldcArm(void)
{
    MotorFOC_RequestArm();
}

void UI_BldcDisarm(void)
{
    MotorFOC_RequestDisarm();
}

void UI_BldcStop(void)
{
    MotorFOC_RequestStop();
}

void UI_StepperEnable(bool enable) { queue_step(STEP_ENABLE, enable ? 1 : 0, 0); }
void UI_StepperMoveTo(float steps) { queue_step(STEP_TO, steps, 0); }
void UI_StepperMoveBy(float steps) { queue_step(STEP_BY, steps, 0); }
void UI_StepperStop(void) { queue_step(STEP_STOP, 0, 0); }
void UI_StepperEmergencyStop(void) { Fault_Report(FAULT_USER_STOP); }
void UI_StepperZero(void) { queue_step(STEP_ZERO, 0, 0); }
void UI_StepperSyncEnable(float scale, float offset) { queue_step(STEP_SYNC, scale, offset); }
void UI_StepperSyncDisable(void) { queue_step(STEP_UNSYNC, 0, 0); }

void UI_ProcessStepperCommands(void)
{
    StepCommand command;
    while (g_step_queue && osMessageQueueGet(g_step_queue, &command, NULL, 0) == osOK)
    {
        if (Fault_IsCriticalActive()) continue;
        HAL_StatusTypeDef status = HAL_OK;
        switch (command.kind)
        {
        case STEP_ENABLE: status = TMC2130Motion_Enable(&g_stepper_motion, command.a != 0); break;
        case STEP_TO: status = TMC2130Motion_MoveTo(&g_stepper_motion, command.a); break;
        case STEP_BY: status = TMC2130Motion_MoveBy(&g_stepper_motion, command.a); break;
        case STEP_STOP: status = TMC2130Motion_Stop(&g_stepper_motion); break;
        case STEP_ZERO:
            if (g_stepper_motion.state != TMC2130_MOTION_IDLE ||
                TMC2130_ZeroEmittedSteps(g_stepper_motion.driver) != HAL_OK) status = HAL_ERROR;
            else g_stepper_motion.position_steps = g_stepper_motion.target_steps = 0;
            break;
        case STEP_SYNC: status = TMC2130Motion_EnableBldcSync(&g_stepper_motion, command.a, command.b); break;
        case STEP_UNSYNC: TMC2130Motion_DisableBldcSync(&g_stepper_motion); break;
        default: status = HAL_ERROR; break;
        }
        if (status != HAL_OK) Fault_Report(FAULT_STEPPER_DRIVER);
    }
}

void UI_PublishStepperSnapshot(void)
{
    uint32_t key = __get_PRIMASK(); __disable_irq();
    g_step_snapshot.stepper_state = TMC2130Motion_GetState(&g_stepper_motion);
    g_step_snapshot.stepper_position_steps = TMC2130Motion_GetPositionSteps(&g_stepper_motion);
    g_step_snapshot.stepper_target_steps = TMC2130Motion_GetTargetSteps(&g_stepper_motion);
    g_step_snapshot.stepper_velocity_steps_s = TMC2130Motion_GetVelocityStepsS(&g_stepper_motion);
    __set_PRIMASK(key);
}
