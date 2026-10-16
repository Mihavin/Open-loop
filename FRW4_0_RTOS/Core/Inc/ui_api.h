#ifndef UI_API_H
#define UI_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "motor_foc.h"
#include "tmc2130_motion.h"

typedef struct
{
    MotorFOC_State_t bldc_state;
    MotorFOC_Mode_t bldc_mode;
    float bldc_speed_rpm;
    int32_t bldc_position_counts;
    float ia;
    float ib;
    float ic;

    TMC2130Motion_State_t stepper_state;
    float stepper_position_steps;
    float stepper_target_steps;
    float stepper_velocity_steps_s;

    uint32_t faults;
} UI_SystemSnapshot_t;

void UI_GetSnapshot(UI_SystemSnapshot_t *out);
typedef enum { UI_CONTROL_KNOB, UI_CONTROL_API } UI_ControlSource_t;
bool UI_SelectControlSource(UI_ControlSource_t source); /* only while idle */
UI_ControlSource_t UI_GetControlSource(void);
void UI_InitCommandQueues(void); /* called before task creation */
void UI_ProcessStepperCommands(void); /* StepperTask only */
void UI_PublishStepperSnapshot(void); /* StepperTask only */

void UI_SetBldcMode(MotorFOC_Mode_t mode);
void UI_SetBldcSpeed(float rpm);
void UI_SetBldcTorque(float iq_a);
void UI_SetBldcPositionRev(float rev);

void UI_BldcArm(void);
void UI_BldcDisarm(void);
void UI_BldcStop(void);

void UI_StepperEnable(bool enable);
void UI_StepperMoveTo(float steps);
void UI_StepperMoveBy(float steps);
void UI_StepperStop(void);
void UI_StepperEmergencyStop(void);
void UI_StepperZero(void);
void UI_StepperSyncEnable(float scale, float offset);
void UI_StepperSyncDisable(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_API_H */
