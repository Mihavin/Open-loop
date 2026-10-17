#ifndef MODEL_HPP
#define MODEL_HPP

#include <gui/common/UiBackend.hpp>

class ModelListener;

/* Operator state machine (UX concept "FRW4 operator UX concept", revised 2026-10-17):
 *   boot -> disarmed -> hold SIDE 1 s (pedal released) -> arming -> armed
 *   hold SIDE again or hold BACK 2 s to disarm; any critical fault -> fault screen,
 *   cleared by a knob press once the cause is gone.
 * Operator modes:
 *   RPM     pedal scales the speed setpoint; releasing brakes to standstill and holds
 *   TORQUE  pedal sets the braking force against an externally turned spindle
 *   INTERP  spindle follows the pedal stroke: one stroke = N revolutions, both ways forward
 *   DRY     timed rotation at a fixed speed, pedal not used
 * SIDE (short) reverses the spindle on the fly with a slewed, actively braked ramp.
 * BACK (short, armed) releases/re-applies the standstill brake. FRW/REV set the
 * traverse direction, FRW+REV together lock/unlock the traverse to the spindle.
 * A turns counter accumulates shaft revolutions in every mode but DRY; hold BACK 2 s
 * while disarmed (or the menu item) resets it. */
class Model
{
public:
    enum OpState { OP_BOOT = 0, OP_DISARMED, OP_ARMING, OP_ARMED, OP_FAULT };
    enum OpMode { OM_RPM = 0, OM_TORQUE, OM_INTERP, OM_DRY, OM_COUNT };
    enum ScreenId { SCR_BOOT = 0, SCR_RUN, SCR_MENU, SCR_TRAVERSE, SCR_INTERP, SCR_DIAGNOSTICS, SCR_SETTINGS, SCR_FAULT };

    struct Settings
    {
        float leadMm;           /* lead screw lead */
        float travelMm;         /* usable carriage travel */
        float rotPerMm;         /* traverse sync: shaft rotations per mm of carriage travel */
        uint16_t pedalFullCounts;
        int maxShaftRpm;
        int rpmPerDetent;
        int dryMinutes;
        int snapDeg;            /* interpolation detent pitch, shaft degrees, 0 = off */
        float holdTorqueA;      /* holding current while the detent is active */
        float rpmModeTorqueA;   /* torque ceiling while the speed loop drives (RPM, Dry, Interp) */
        int torqueModeMaxRpm;   /* shaft rpm ceiling in Torque mode: the drive brakes above it */
        bool buzzer;
        bool vibration;
        int backlightPercent;
    };

    Model();
    void bind(ModelListener* listener) { modelListener = listener; }
    void tick();

    /* --- state for the views --- */
    const ui::Telemetry& telemetry() const { return tele; }
    OpState opState() const { return op; }
    ScreenId screen() const { return currentScreen; }
    OpMode opMode() const { return (OpMode)modeSel; }
    int direction() const { return dir; }         /* spindle: +1 forward, -1 reverse */
    bool reversing() const;                        /* slewing through zero after SIDE */
    bool brakeReleased() const { return brakeOff; }
    int traverseDirection() const { return travDir; }
    float setpoint() const { return setpoints[modeSel]; }
    float setpointForMode(OpMode m) const { return setpoints[m]; }
    float setpointMax() const;
    const char* modeName() const;
    float shaftRpm() const;
    float currentA() const;
    float spindleRev() const;                      /* shaft revolutions since the mode origin */
    float spindleAngleDeg() const;                 /* 0..360, shaft, relative to the mode origin */
    float turnsCount() const { return turnsRev; }  /* shaft revolutions since the last reset */
    bool turnsCounted() const { return modeSel != OM_DRY; }
    bool standstill() const;
    bool pedalReleased() const;
    bool controlLocked() const { return currentScreen != SCR_RUN; }
    const char* notice() const { return noticeText; }
    uint32_t bootElapsedMs() const;
    bool traverseEngaged() const { return traverseOn; }
    bool traverseAtEnd() const { return traverseEnd; }
    float carriageMm() const;
    float stepsPerMm() const;
    bool dryRunning() const { return dryActive; }
    uint32_t dryRemainingMs() const;
    Settings& settings() { return cfg; }
    const Settings& settings() const { return cfg; }
    bool faultClearRefused() const { return clearRefused; }
    uint32_t millis() const { return tele.millis; }

    /* --- actions for the presenters --- */
    void feedback(ui::Feedback f) { ui::feedback(f); }
    bool adjustSetpoint(int detents);
    bool selectMode(OpMode m);
    bool toggleRpmTorque();
    bool toggleDryTimer();
    bool openMenuAllowed() const;
    void goTo(ScreenId id);
    void setRotPerMm(float value);
    void adjustSnap(int detents);
    void adjustHoldTorque(int detents);
    void adjustStroke(int detents);
    bool engageTraverse(bool on);
    void setTraverseDirection(int d);
    void applySettings();
    void confirmFaultClear();
    void resetTurns();

private:
    void pollInputs();
    bool handleGlobal(const ui::InputEvent& e);
    void updateState();
    void runControl();
    void sendSpeed(float shaftRpmTarget, float dt);
    void setNotice(const char* text);
    void startArming();
    void disarmNow(bool quiet);
    void enterFault();
    void resetOrigin();
    void applySnap();
    void applyLimits();
    void reverseSpindle();
    void toggleBrake();
    ui::Mode firmwareMode(int m) const;

    ModelListener* modelListener;
    ui::Telemetry tele;
    OpState op;
    ScreenId currentScreen;
    int modeSel;
    int dir, travDir;
    float setpoints[OM_COUNT];
    float slewRpm;         /* current slewed speed command, shaft rpm */
    float lastSent;
    bool brakeOff;
    bool holdSent;
    float originRev;       /* motor revolutions at mode entry */
    int32_t originCounts;  /* the same in encoder counts (detent grid anchor) */
    int32_t lastSentCounts; /* last position reference sent, for snap detection */
    bool sentPosition;
    float interpRev;       /* accumulated interpolation travel, shaft rev */
    float turnsRev;        /* turns counter, shaft rev */
    int32_t lastCounts;    /* encoder counts at the previous tick */
    int lastPedal;
    bool dryActive;
    uint32_t dryEndMs;
    bool frwDown, revDown, chordUsed;
    Settings cfg;
    bool traverseOn, traverseEnd;
    uint32_t bootStartMs, armingStartMs, noticeUntilMs, clearRequestedMs, lastTickMs;
    bool clearRefused;
    char noticeText[48];
    bool uiInitialised;
};

#endif // MODEL_HPP
