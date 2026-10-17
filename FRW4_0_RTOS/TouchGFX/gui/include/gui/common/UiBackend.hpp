#ifndef UI_BACKEND_HPP
#define UI_BACKEND_HPP

#include <stdint.h>

/* Everything the GUI needs from the machine, behind one small interface so the
   same screens run on the target (target/UiBackendTarget.cpp, on top of
   ui_api.h / ui_input.h / fault_manager.h) and in the PC simulator
   (simulator/UiBackendSim.cpp, a fake plant driven from the keyboard). */
namespace ui
{
/* Mirrors MotorFOC_State_t / MotorFOC_Mode_t / TMC2130Motion_State_t. */
enum BldcState { BLDC_IDLE = 0, BLDC_CALIB, BLDC_ALIGN, BLDC_RUN, BLDC_FAULT };
enum Mode { MODE_TORQUE = 0, MODE_SPEED, MODE_POSITION, MODE_COUNT };
enum StepperState { STEP_IDLE = 0, STEP_MOVING, STEP_SYNC, STEP_STOPPING, STEP_FAULT };

/* Mirrors FaultBits_t in fault_manager.h (checked by a static_assert on the target). */
enum FaultBit
{
    FAULT_BLDC_DRIVER = 1u << 0,
    FAULT_STEPPER_DRIVER = 1u << 1,
    FAULT_FAN = 1u << 2,
    FAULT_SPI = 1u << 4,
    FAULT_MOTION_TIMEOUT = 1u << 5,
    FAULT_CPU = 1u << 6,
    FAULT_CURRENT = 1u << 7,
    FAULT_ADC = 1u << 8,
    FAULT_USER_STOP = 1u << 9
};
static const uint32_t FAULT_CRITICAL = FAULT_BLDC_DRIVER | FAULT_STEPPER_DRIVER | FAULT_SPI | FAULT_MOTION_TIMEOUT |
                                       FAULT_CPU | FAULT_CURRENT | FAULT_ADC | FAULT_USER_STOP;

/* Mirrors UI_Key_t / UI_InputType_t / UI_Feedback_t in ui_input.h. */
enum Key { KEY_KNOB = 0, KEY_FRW, KEY_REV, KEY_SIDE, KEY_BACK, KEY_COUNT };
enum InputType { IN_NONE = 0, IN_TURN, IN_PRESS, IN_HOLD, IN_RELEASE };
enum Feedback { FB_CLICK = 0, FB_CONFIRM, FB_ARM, FB_DISARM, FB_WARN, FB_TIMER, FB_FAULT };

struct InputEvent
{
    uint8_t type;
    uint8_t key;
    int16_t value;
};

struct Telemetry
{
    uint8_t bldcState;
    uint8_t bldcMode;
    float speedRpm;     /* motor shaft, signed */
    float ia, ib, ic;   /* phase currents, A */
    int32_t positionCounts;
    int32_t positionRefCounts; /* position-mode reference, moves when the detent snaps */
    uint8_t stepperState;
    float stepperPositionSteps;
    uint32_t faults;
    bool faultCausePresent; /* a fault line is still asserted */
    uint16_t pedalPermille;
    bool pedalHomed;
    uint32_t sideHeldMs;
    uint32_t backHeldMs;
    uint32_t millis;
};

struct Limits
{
    float gearRatio;        /* motor revolutions per output-shaft revolution */
    float encoderCpr;       /* motor encoder counts per motor revolution */
    float maxMotorRpm;
    float maxTorqueA;
    float maxPositionRev;   /* motor revolutions either side of the origin */
    float stepsPerStepperRev;
};

void init();
const Limits& limits();
bool popInput(InputEvent& event);
void read(Telemetry& out);

void setMode(Mode mode);
void setSpeed(float motorRpm);
void setTorque(float iqA);
void setPositionRev(float motorRev);
/* Position-mode detent: pitch in motor degrees (0 = off) around originCounts. */
void setPositionSnap(float pitchDeg, int32_t originCounts, float holdIqA);
/* Operator limits: speed ceiling in motor rpm, torque ceiling in A (both capped by Limits). */
void setOperatingLimits(float speedLimitMotorRpm, float iqLimitA);
void arm();
void disarm();
void stop();
void requestFaultClear();

void stepperEnable(bool enable);
void stepperSync(float stepsPerEncoderCount, float offsetSteps);
void stepperUnsync();
void stepperStop();
void stepperZero();

void feedback(Feedback pattern);
void setFeedbackEnabled(bool buzzer, bool vibration);
void setPedalFullCounts(uint16_t counts);
void setBacklight(uint8_t percent);

const char* faultName(uint32_t bit);
} // namespace ui

#endif // UI_BACKEND_HPP
