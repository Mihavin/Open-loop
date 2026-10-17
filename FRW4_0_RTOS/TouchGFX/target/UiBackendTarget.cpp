/* GUI backend on the STM32H743 target: forwards to the firmware's C APIs.
   Runs in the GUI task; every call below is safe from there. */
#include <gui/common/UiBackend.hpp>

extern "C" {
#include "fault_manager.h"
#include "motor_foc.h"
#include "tmc2130_motion.h"
#include "ui_api.h"
#include "ui_input.h"
#include "tim.h"
}

#define UI_SAME(a, b) ((uint32_t)(a) == (uint32_t)(b))
static_assert(UI_SAME(ui::FAULT_BLDC_DRIVER, FAULT_BLDC_DRIVER) && UI_SAME(ui::FAULT_STEPPER_DRIVER, FAULT_STEPPER_DRIVER) &&
              UI_SAME(ui::FAULT_FAN, FAULT_FAN) && UI_SAME(ui::FAULT_SPI, FAULT_SPI) &&
              UI_SAME(ui::FAULT_MOTION_TIMEOUT, FAULT_MOTION_TIMEOUT) && UI_SAME(ui::FAULT_CPU, FAULT_CPU) &&
              UI_SAME(ui::FAULT_CURRENT, FAULT_CURRENT) && UI_SAME(ui::FAULT_ADC, FAULT_ADC) &&
              UI_SAME(ui::FAULT_USER_STOP, FAULT_USER_STOP) && UI_SAME(ui::FAULT_CRITICAL, FAULT_CRITICAL_MASK),
              "ui::FaultBit must mirror FaultBits_t");
static_assert(UI_SAME(ui::KEY_KNOB, UI_KEY_KNOB) && UI_SAME(ui::KEY_BACK, UI_KEY_BACK) && UI_SAME(ui::KEY_COUNT, UI_KEY_COUNT),
              "ui::Key must mirror UI_Key_t");
static_assert(UI_SAME(ui::IN_TURN, UI_INPUT_TURN) && UI_SAME(ui::IN_RELEASE, UI_INPUT_RELEASE), "ui::InputType must mirror UI_InputType_t");
static_assert(UI_SAME(ui::FB_CLICK, UI_FB_CLICK) && UI_SAME(ui::FB_TIMER, UI_FB_TIMER) && UI_SAME(ui::FB_FAULT, UI_FB_FAULT), "ui::Feedback must mirror UI_Feedback_t");
static_assert(UI_SAME(ui::BLDC_FAULT, MOTOR_FOC_STATE_FAULT) && UI_SAME(ui::MODE_POSITION, MOTOR_FOC_MODE_POSITION),
              "ui enums must mirror motor_foc.h");
static_assert(UI_SAME(ui::STEP_FAULT, TMC2130_MOTION_FAULT), "ui::StepperState must mirror TMC2130Motion_State_t");

namespace ui
{
static const Limits g_limits = {
    MOTOR_FOC_GEAR_RATIO, (float)MOTOR_FOC_ENCODER_CPR, MOTOR_FOC_SPEED_MAX_RPM,
    MOTOR_FOC_IQ_MAX_A, MOTOR_FOC_POSITION_MAX_REVS, 200.0f * 16.0f
};

void init() {}
const Limits& limits() { return g_limits; }

bool popInput(InputEvent& event)
{
    UI_InputEvent_t raw;
    if (!UI_InputPop(&raw)) return false;
    event.type = raw.type;
    event.key = raw.key;
    event.value = raw.value;
    return true;
}

void read(Telemetry& out)
{
    UI_SystemSnapshot_t snap;
    UI_GetSnapshot(&snap);
    out.bldcState = (uint8_t)snap.bldc_state;
    out.bldcMode = (uint8_t)snap.bldc_mode;
    out.speedRpm = snap.bldc_speed_rpm;
    out.ia = snap.ia;
    out.ib = snap.ib;
    out.ic = snap.ic;
    out.positionCounts = snap.bldc_position_counts;
    out.positionRefCounts = snap.bldc_position_ref_counts;
    out.stepperState = (uint8_t)snap.stepper_state;
    out.stepperPositionSteps = snap.stepper_position_steps;
    out.faults = snap.faults;
    out.faultCausePresent = Fault_CausePresent();
    out.pedalPermille = UI_PedalPermille();
    out.pedalHomed = UI_PedalIsHomed();
    uint32_t now = HAL_GetTick();
    out.sideHeldMs = UI_KeyHeldMs(UI_KEY_SIDE, now);
    out.backHeldMs = UI_KeyHeldMs(UI_KEY_BACK, now);
    out.millis = now;
}

void setMode(Mode mode) { UI_SetBldcMode((MotorFOC_Mode_t)mode); }
void setSpeed(float motorRpm) { UI_SetBldcSpeed(motorRpm); }
void setTorque(float iqA) { UI_SetBldcTorque(iqA); }
void setPositionRev(float motorRev) { UI_SetBldcPositionRev(motorRev); }
void setPositionSnap(float pitchDeg, int32_t originCounts, float holdIqA) { UI_SetBldcPositionSnap(pitchDeg, originCounts, holdIqA); }
void arm() { UI_BldcArm(); }
void disarm() { UI_BldcDisarm(); }
void stop() { UI_BldcStop(); }
void requestFaultClear() { Fault_RequestClear(); }

void stepperEnable(bool enable) { UI_StepperEnable(enable); }
void stepperSync(float stepsPerEncoderCount, float offsetSteps) { UI_StepperSyncEnable(stepsPerEncoderCount, offsetSteps); }
void stepperUnsync() { UI_StepperSyncDisable(); }
void stepperStop() { UI_StepperStop(); }
void stepperZero() { UI_StepperZero(); }

void feedback(Feedback pattern) { UI_FeedbackPlay((UI_Feedback_t)pattern); }
void setFeedbackEnabled(bool buzzer, bool vibration) { UI_FeedbackSetEnabled(buzzer, vibration); }
void setPedalFullCounts(uint16_t counts) { UI_PedalSetFullCounts(counts); }

void setBacklight(uint8_t percent)
{
    /* TIM5 CH1 drives the backlight, period 99: compare == duty percent. */
    if (percent > 100) percent = 100;
    if (percent < 5) percent = 5;
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, percent);
}

const char* faultName(uint32_t bit)
{
    switch (bit)
    {
    case FAULT_BLDC_DRIVER: return "Motor driver";
    case FAULT_STEPPER_DRIVER: return "Stepper driver";
    case FAULT_FAN: return "Fan";
    case FAULT_SPI: return "Driver SPI";
    case FAULT_MOTION_TIMEOUT: return "Motion timeout";
    case FAULT_CPU: return "Task watchdog";
    case FAULT_CURRENT: return "Overcurrent";
    case FAULT_ADC: return "Current sensing";
    case FAULT_USER_STOP: return "Emergency stop";
    default: return "Unknown";
    }
}
} // namespace ui
