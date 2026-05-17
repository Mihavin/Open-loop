#include "ui_api.h"

#include <string.h>

#include "fault_manager.h"

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
    out->ia = MotorFOC_GetIa();
    out->ib = MotorFOC_GetIb();
    out->ic = MotorFOC_GetIc();

    out->stepper_state = TMC2130Motion_GetState(&g_stepper_motion);
    out->stepper_position_steps = TMC2130Motion_GetPositionSteps(&g_stepper_motion);
    out->stepper_target_steps = TMC2130Motion_GetTargetSteps(&g_stepper_motion);
    out->stepper_velocity_steps_s = TMC2130Motion_GetVelocityStepsS(&g_stepper_motion);

    out->faults = Fault_GetActive();
}

void UI_SetBldcMode(MotorFOC_Mode_t mode)
{
    MotorFOC_SetMode(mode);
}

void UI_SetBldcSpeed(float rpm)
{
    MotorFOC_SetCommand(MOTOR_FOC_MODE_SPEED, rpm, 0.0f, MotorFOC_GetPositionCounts());
}

void UI_SetBldcTorque(float iq_a)
{
    MotorFOC_SetCommand(MOTOR_FOC_MODE_TORQUE, 0.0f, iq_a, MotorFOC_GetPositionCounts());
}

void UI_SetBldcPositionRev(float rev)
{
    MotorFOC_SetPositionRevolutions(rev);
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

void UI_StepperEnable(bool enable)
{
    (void)TMC2130Motion_Enable(&g_stepper_motion, enable);
}

void UI_StepperMoveTo(float steps)
{
    (void)TMC2130Motion_MoveTo(&g_stepper_motion, steps);
}

void UI_StepperMoveBy(float steps)
{
    (void)TMC2130Motion_MoveBy(&g_stepper_motion, steps);
}

void UI_StepperStop(void)
{
    (void)TMC2130Motion_Stop(&g_stepper_motion);
}

void UI_StepperEmergencyStop(void)
{
    (void)TMC2130Motion_EmergencyStop(&g_stepper_motion);
}

void UI_StepperZero(void)
{
    (void)TMC2130Motion_EmergencyStop(&g_stepper_motion);
    g_stepper_motion.position_steps = 0.0f;
    g_stepper_motion.target_steps = 0.0f;
}

void UI_StepperSyncEnable(float scale, float offset)
{
    (void)TMC2130Motion_EnableBldcSync(&g_stepper_motion, scale, offset);
}

void UI_StepperSyncDisable(void)
{
    TMC2130Motion_DisableBldcSync(&g_stepper_motion);
}
