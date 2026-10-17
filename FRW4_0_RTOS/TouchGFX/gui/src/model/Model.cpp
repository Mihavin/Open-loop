#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <gui/common/FrontendApplication.hpp>
#include <math.h>
#include <string.h>

static const uint32_t BOOT_MIN_MS = 2200;
static const uint32_t ARMING_TIMEOUT_MS = 3000;
static const uint32_t NOTICE_MS = 2500;
static const uint32_t CLEAR_REPLY_MS = 300;
static const float STANDSTILL_RPM = 10.0f;   /* motor rpm, same threshold as the firmware */
static const int PEDAL_DEADBAND = 30;        /* permille */
static const int PEDAL_NOISE = 4;            /* permille ignored by the interpolation */
static const float BRAKE_BLEND_RPM = 60.0f;  /* motor rpm over which brake torque is full */

Model::Model()
    : modelListener(0), op(OP_BOOT), currentScreen(SCR_BOOT), modeSel(OM_RPM), dir(1), travDir(1), slewRpm(0.0f),
      lastSent(0.0f), brakeOff(false), holdSent(false), originRev(0.0f), originCounts(0), lastSentCounts(0), sentPosition(false), interpRev(0.0f), turnsRev(0.0f), lastCounts(0), lastPedal(0), dryActive(false),
      dryEndMs(0), frwDown(false), revDown(false), chordUsed(false), traverseOn(false), traverseEnd(false), bootStartMs(0),
      armingStartMs(0), noticeUntilMs(0), clearRequestedMs(0), lastTickMs(0), clearRefused(false), uiInitialised(false)
{
    memset(&tele, 0, sizeof(tele));
    setpoints[OM_RPM] = 300.0f;    /* shaft rpm */
    setpoints[OM_TORQUE] = 0.5f;   /* brake amps at full pedal */
    setpoints[OM_INTERP] = 0.5f;   /* shaft revolutions per pedal stroke */
    setpoints[OM_DRY] = 30.0f;     /* shaft rpm */
    cfg.leadMm = 2.0f;
    cfg.travelMm = 70.0f;
    cfg.rotPerMm = 2.50f;
    cfg.pedalFullCounts = 200;
    cfg.maxShaftRpm = 1000;
    cfg.rpmPerDetent = 5;
    cfg.dryMinutes = 30;
    cfg.snapDeg = 45;
    cfg.holdTorqueA = 0.4f;
    cfg.rpmModeTorqueA = 1.0f;
    cfg.torqueModeMaxRpm = 300;
    cfg.buzzer = true;
    cfg.vibration = true;
    cfg.backlightPercent = 75;
    noticeText[0] = 0;
}

/* ---------------- derived values ---------------- */

ui::Mode Model::firmwareMode(int m) const
{
    switch (m)
    {
    case OM_TORQUE: return ui::MODE_TORQUE;
    case OM_INTERP: return ui::MODE_POSITION;
    default: return ui::MODE_SPEED;
    }
}

float Model::setpointMax() const
{
    const ui::Limits& lim = ui::limits();
    float fwRpm = lim.maxMotorRpm / lim.gearRatio;
    float rpmMax = (float)cfg.maxShaftRpm < fwRpm ? (float)cfg.maxShaftRpm : fwRpm;
    switch (modeSel)
    {
    case OM_TORQUE: return lim.maxTorqueA;
    case OM_INTERP: return 2.0f;
    case OM_DRY: return rpmMax < 300.0f ? rpmMax : 300.0f;
    default: return rpmMax;
    }
}

const char* Model::modeName() const
{
    switch (modeSel)
    {
    case OM_TORQUE: return "TORQUE";
    case OM_INTERP: return "INTERP";
    case OM_DRY: return "DRY";
    default: return "RPM";
    }
}

float Model::shaftRpm() const { return tele.speedRpm / ui::limits().gearRatio; }

float Model::currentA() const
{
    /* Amplitude of a balanced three-phase set. */
    float s = tele.ia * tele.ia + tele.ib * tele.ib + tele.ic * tele.ic;
    return sqrtf(s * (2.0f / 3.0f));
}

float Model::spindleRev() const
{
    const ui::Limits& lim = ui::limits();
    return ((float)tele.positionCounts / lim.encoderCpr - originRev) / lim.gearRatio;
}

float Model::spindleAngleDeg() const
{
    float rev = spindleRev();
    float frac = rev - floorf(rev);
    return frac * 360.0f;
}

bool Model::reversing() const
{
    return op == OP_ARMED && (modeSel == OM_RPM || modeSel == OM_DRY) && slewRpm * (float)dir < -1.0f;
}

bool Model::standstill() const { return fabsf(tele.speedRpm) <= STANDSTILL_RPM; }
bool Model::pedalReleased() const { return tele.pedalPermille <= PEDAL_DEADBAND; }
uint32_t Model::bootElapsedMs() const { return tele.millis - bootStartMs; }

uint32_t Model::dryRemainingMs() const
{
    if (!dryActive) return (uint32_t)cfg.dryMinutes * 60000u;
    return (int32_t)(dryEndMs - tele.millis) > 0 ? dryEndMs - tele.millis : 0u;
}

float Model::stepsPerMm() const
{
    return cfg.leadMm > 0.01f ? ui::limits().stepsPerStepperRev / cfg.leadMm : 0.0f;
}

float Model::carriageMm() const
{
    float spm = stepsPerMm();
    return spm > 0.0f ? tele.stepperPositionSteps / spm : 0.0f;
}

bool Model::openMenuAllowed() const
{
    if (op == OP_DISARMED) return true;
    return op == OP_ARMED && standstill() && pedalReleased() && !dryActive;
}

void Model::setNotice(const char* text)
{
    strncpy(noticeText, text, sizeof(noticeText) - 1);
    noticeText[sizeof(noticeText) - 1] = 0;
    noticeUntilMs = tele.millis + NOTICE_MS;
}

void Model::resetOrigin()
{
    originCounts = tele.positionCounts;
    originRev = (float)originCounts / ui::limits().encoderCpr;
    interpRev = 0.0f;
    sentPosition = false;
    applySnap();
    lastPedal = tele.pedalPermille;
    lastSent = 0.0f;
    slewRpm = 0.0f;
    holdSent = false;
}

void Model::applySnap()
{
    /* The detent grid lives in motor counts: shaft pitch times the gear ratio. */
    float pitch = modeSel == OM_INTERP ? (float)cfg.snapDeg * ui::limits().gearRatio : 0.0f;
    ui::setPositionSnap(pitch, originCounts, cfg.holdTorqueA);
}

void Model::applyLimits()
{
    const ui::Limits& lim = ui::limits();
    if (modeSel == OM_TORQUE)
        ui::setOperatingLimits((float)cfg.torqueModeMaxRpm * lim.gearRatio, lim.maxTorqueA);
    else
        ui::setOperatingLimits((float)cfg.maxShaftRpm * lim.gearRatio, cfg.rpmModeTorqueA);
}

/* ---------------- navigation ---------------- */

void Model::goTo(ScreenId id)
{
    if (id == currentScreen && uiInitialised) return;
    currentScreen = id;
    FrontendApplication& app = *static_cast<FrontendApplication*>(touchgfx::Application::getInstance());
    switch (id)
    {
    case SCR_BOOT: app.gotoBootScreenScreenNoTransition(); break;
    case SCR_RUN: app.gotoRunScreenScreenNoTransition(); break;
    case SCR_MENU: app.gotoMenuScreenScreenNoTransition(); break;
    case SCR_TRAVERSE: app.gotoTraverseScreenScreenNoTransition(); break;
    case SCR_INTERP: app.gotoInterpScreenScreenNoTransition(); break;
    case SCR_DIAGNOSTICS: app.gotoDiagnosticsScreenScreenNoTransition(); break;
    case SCR_SETTINGS: app.gotoSettingsScreenScreenNoTransition(); break;
    case SCR_FAULT: app.gotoFaultScreenScreenNoTransition(); break;
    }
}

/* ---------------- per frame ---------------- */

void Model::tick()
{
    if (!uiInitialised)
    {
        ui::init();
        ui::read(tele);
        bootStartMs = tele.millis;
        lastTickMs = tele.millis;
        lastCounts = tele.positionCounts;
        applySettings();
        uiInitialised = true;
    }
    ui::read(tele);
    /* Turns counter: every shaft revolution except in dry mode, signed by the rotation. */
    if (modeSel != OM_DRY)
    {
        const ui::Limits& lim = ui::limits();
        turnsRev += (float)(tele.positionCounts - lastCounts) / (lim.encoderCpr * lim.gearRatio);
    }
    lastCounts = tele.positionCounts;
    pollInputs();
    updateState();
    runControl();
    lastTickMs = tele.millis;
    if (noticeText[0] && (int32_t)(tele.millis - noticeUntilMs) > 0) noticeText[0] = 0;
    if (modelListener) modelListener->onUpdate();
}

void Model::pollInputs()
{
    ui::InputEvent e;
    for (int n = 0; n < 8 && ui::popInput(e); ++n)
    {
        if (op == OP_BOOT) continue;
        if (!handleGlobal(e) && modelListener) modelListener->onInput(e);
    }
}

bool Model::handleGlobal(const ui::InputEvent& e)
{
    if (e.type == ui::IN_HOLD && e.key == ui::KEY_BACK)
    {
        /* Hold BACK 2 s: armed -> stop and disarm; disarmed -> reset the turns counter. */
        if (op == OP_ARMED || op == OP_ARMING) disarmNow(false);
        else if (op == OP_DISARMED && currentScreen == SCR_RUN) resetTurns();
        else ui::feedback(ui::FB_CLICK);
        if (op != OP_FAULT) goTo(SCR_RUN);
        return true;
    }
    if (e.type == ui::IN_HOLD && e.key == ui::KEY_SIDE)
    {
        if (op == OP_DISARMED) startArming();
        else if (op == OP_ARMED || op == OP_ARMING) disarmNow(false);
        else ui::feedback(ui::FB_WARN);
        return true;
    }
    if (e.type == ui::IN_HOLD && e.key == ui::KEY_KNOB)
    {
        if (op == OP_FAULT) return true;
        if (currentScreen == SCR_RUN)
        {
            if (openMenuAllowed()) { ui::feedback(ui::FB_CLICK); goTo(SCR_MENU); }
            else { ui::feedback(ui::FB_WARN); setNotice("Stop before opening the menu"); }
        }
        return true;
    }
    if (op == OP_FAULT)
    {
        if (e.type == ui::IN_RELEASE && e.key == ui::KEY_KNOB && e.value == 0) confirmFaultClear();
        return true; /* nothing else works on the fault screen */
    }

    /* FRW / REV: traverse direction; both together: lock or release the traverse. */
    if (e.key == ui::KEY_FRW || e.key == ui::KEY_REV)
    {
        bool& mine = e.key == ui::KEY_FRW ? frwDown : revDown;
        bool& other = e.key == ui::KEY_FRW ? revDown : frwDown;
        if (e.type == ui::IN_PRESS)
        {
            mine = true;
            if (other)
            {
                chordUsed = true;
                bool on = !traverseOn;
                if (engageTraverse(on)) { ui::feedback(on ? ui::FB_CONFIRM : ui::FB_CLICK); setNotice(on ? "Traverse locked to the spindle" : "Traverse released"); }
                else { ui::feedback(ui::FB_WARN); setNotice("Traverse not available"); }
            }
        }
        else if (e.type == ui::IN_RELEASE)
        {
            mine = false;
            if (chordUsed) { if (!other) chordUsed = false; }
            else if (e.value == 0) setTraverseDirection(e.key == ui::KEY_FRW ? 1 : -1);
        }
        return true;
    }

    /* SIDE short press: reverse the spindle. */
    if (e.type == ui::IN_RELEASE && e.key == ui::KEY_SIDE && e.value == 0)
    {
        reverseSpindle();
        return true;
    }

    /* BACK short press while armed on the run screen: brake release. */
    if (e.type == ui::IN_RELEASE && e.key == ui::KEY_BACK && e.value == 0 && op == OP_ARMED && currentScreen == SCR_RUN)
    {
        toggleBrake();
        return true;
    }
    return false;
}

void Model::reverseSpindle()
{
    if (modeSel == OM_TORQUE)
    {
        ui::feedback(ui::FB_WARN);
        setNotice("Torque mode has no direction");
        return;
    }
    dir = -dir;
    ui::feedback(ui::FB_CONFIRM);
    if (op == OP_ARMED)
    {
        if (modeSel == OM_INTERP) resetOrigin();
        setNotice(dir > 0 ? "Reversing: forward" : "Reversing: reverse");
    }
    else setNotice(dir > 0 ? "Direction: forward" : "Direction: reverse");
}

void Model::toggleBrake()
{
    if (modeSel != OM_RPM && modeSel != OM_DRY)
    {
        ui::feedback(ui::FB_WARN);
        return;
    }
    brakeOff = !brakeOff;
    lastSent = 0.0f;
    ui::feedback(brakeOff ? ui::FB_CLICK : ui::FB_CONFIRM);
    setNotice(brakeOff ? "Brake released, spindle free" : "Brake on");
}

void Model::startArming()
{
    if (tele.faults & ui::FAULT_CRITICAL) { enterFault(); return; }
    if (!pedalReleased())
    {
        ui::feedback(ui::FB_WARN);
        setNotice("Release the pedal to arm");
        return;
    }
    if (currentScreen != SCR_RUN) goTo(SCR_RUN);
    ui::setMode(firmwareMode(modeSel));
    applyLimits();
    resetOrigin();
    brakeOff = false;
    dryActive = false;
    ui::arm();
    armingStartMs = tele.millis;
    op = OP_ARMING;
    ui::feedback(ui::FB_CLICK);
}

void Model::disarmNow(bool quiet)
{
    ui::stop();
    ui::disarm();
    lastSent = 0.0f;
    slewRpm = 0.0f;
    brakeOff = false;
    dryActive = false;
    op = OP_DISARMED;
    if (!quiet) ui::feedback(ui::FB_DISARM);
}

void Model::enterFault()
{
    op = OP_FAULT;
    clearRefused = false;
    clearRequestedMs = 0;
    lastSent = 0.0f;
    slewRpm = 0.0f;
    dryActive = false;
    brakeOff = false;
    traverseOn = false; /* the stepper is already cut by the fault manager */
    ui::stepperEnable(false);
    ui::feedback(ui::FB_FAULT);
    goTo(SCR_FAULT);
}

void Model::updateState()
{
    bool critical = (tele.faults & ui::FAULT_CRITICAL) != 0;
    if (critical && op != OP_FAULT && op != OP_BOOT) enterFault();

    switch (op)
    {
    case OP_BOOT:
        if (bootElapsedMs() >= BOOT_MIN_MS)
        {
            if (critical) enterFault();
            else { op = OP_DISARMED; goTo(SCR_RUN); }
        }
        break;
    case OP_ARMING:
        if (tele.bldcState == ui::BLDC_RUN)
        {
            op = OP_ARMED;
            resetOrigin();
            ui::feedback(ui::FB_ARM);
            setNotice(modeSel == OM_DRY ? "Armed: press the knob to start" : "Armed: pedal runs, release stops");
        }
        else if (tele.millis - armingStartMs > ARMING_TIMEOUT_MS || tele.bldcState == ui::BLDC_IDLE)
        {
            if (tele.millis - armingStartMs > 150)
            {
                disarmNow(true);
                ui::feedback(ui::FB_WARN);
                setNotice("Arming failed, try again");
            }
        }
        break;
    case OP_ARMED:
        if (tele.bldcState != ui::BLDC_RUN)
        {
            /* Stopped from outside the UI (API stop, driver dropout). */
            disarmNow(false);
            setNotice("Motor stopped");
        }
        else if (dryActive && (int32_t)(tele.millis - dryEndMs) >= 0)
        {
            dryActive = false;
            ui::feedback(ui::FB_TIMER);
            setNotice("Dry run finished");
        }
        break;
    case OP_FAULT:
        if (!critical)
        {
            disarmNow(true);
            clearRefused = false;
            ui::feedback(ui::FB_CONFIRM);
            setNotice("Fault cleared, disarmed");
            goTo(SCR_RUN);
        }
        else if (clearRequestedMs && tele.millis - clearRequestedMs > CLEAR_REPLY_MS)
        {
            clearRequestedMs = 0;
            clearRefused = true;
            ui::feedback(ui::FB_WARN);
        }
        break;
    default:
        break;
    }

    if (traverseOn && fabsf(carriageMm()) > cfg.travelMm)
    {
        engageTraverse(false);
        traverseEnd = true;
        ui::feedback(ui::FB_WARN);
        setNotice("Traverse end reached");
    }
}

void Model::sendSpeed(float target, float dt)
{
    /* Slew the command so a reverse is a controlled, actively braked ramp through zero. */
    const ui::Limits& lim = ui::limits();
    float rate = 2.0f * (float)cfg.maxShaftRpm;
    if (rate < 200.0f) rate = 200.0f;
    float step = rate * dt;
    float diff = target - slewRpm;
    if (diff > step) diff = step;
    if (diff < -step) diff = -step;
    slewRpm += diff;
    if (fabsf(target - slewRpm) < 0.01f) slewRpm = target;
    float cmd = slewRpm * lim.gearRatio;
    if (fabsf(cmd - lastSent) > 0.25f || (cmd == 0.0f && lastSent != 0.0f))
    {
        ui::setSpeed(cmd);
        lastSent = cmd;
    }
}

void Model::runControl()
{
    if (op != OP_ARMED) return;
    const ui::Limits& lim = ui::limits();
    float dt = (float)(tele.millis - lastTickMs) / 1000.0f;
    if (dt <= 0.0f || dt > 0.2f) dt = 1.0f / 60.0f;
    bool locked = controlLocked();
    float pedal = locked ? 0.0f : (float)tele.pedalPermille / 1000.0f;
    if (tele.pedalPermille <= PEDAL_DEADBAND) pedal = 0.0f;

    if (brakeOff && (modeSel == OM_RPM || modeSel == OM_DRY))
    {
        /* Brake released: zero torque, the spindle turns freely until the pedal or timer asks for speed. */
        bool wantsSpeed = modeSel == OM_RPM ? pedal > 0.0f : dryActive;
        if (wantsSpeed) brakeOff = false;
        else
        {
            if (lastSent != -1.0f) { ui::setTorque(0.0f); lastSent = -1.0f; slewRpm = 0.0f; }
            return;
        }
    }

    switch (modeSel)
    {
    case OM_RPM:
        sendSpeed(setpoints[OM_RPM] * (float)dir * pedal, dt);
        break;
    case OM_DRY:
        sendSpeed(dryActive ? setpoints[OM_DRY] * (float)dir : 0.0f, dt);
        break;
    case OM_TORQUE:
    {
        /* Braking force against the spindle's own motion, full above BRAKE_BLEND_RPM. */
        float blend = tele.speedRpm / BRAKE_BLEND_RPM;
        if (blend > 1.0f) blend = 1.0f;
        if (blend < -1.0f) blend = -1.0f;
        float cmd = -blend * setpoints[OM_TORQUE] * pedal;
        if (fabsf(cmd - lastSent) > 0.005f || (cmd == 0.0f && lastSent != 0.0f))
        {
            ui::setTorque(cmd);
            lastSent = cmd;
        }
        break;
    }
    default: /* OM_INTERP */
    {
        /* The firmware detent may have moved the reference (shaft turned by hand):
           continue the stroke from there instead of pulling the spindle back. */
        if (sentPosition && (tele.positionRefCounts - lastSentCounts > 2 || lastSentCounts - tele.positionRefCounts > 2))
        {
            originRev = (float)tele.positionRefCounts / lim.encoderCpr - interpRev * lim.gearRatio * (float)dir;
            lastSentCounts = tele.positionRefCounts;
        }
        int p = locked ? lastPedal : (int)tele.pedalPermille;
        int delta = p - lastPedal;
        if (delta < 0) delta = -delta;
        if (delta >= PEDAL_NOISE)
        {
            interpRev += (float)delta / 1000.0f * setpoints[OM_INTERP];
            lastPedal = p;
        }
        float target = originRev + interpRev * lim.gearRatio * (float)dir;
        if (target != lastSent || !sentPosition)
        {
            ui::setPositionRev(target);
            lastSent = target;
            lastSentCounts = (int32_t)(target * lim.encoderCpr + (target >= 0.0f ? 0.5f : -0.5f));
            sentPosition = true;
        }
        break;
    }
    }
}

/* ---------------- presenter actions ---------------- */

bool Model::adjustSetpoint(int detents)
{
    float step;
    switch (modeSel)
    {
    case OM_TORQUE: step = 0.02f; break;
    case OM_INTERP: step = 0.1f; break;
    case OM_DRY: step = 1.0f; break;
    default: step = (float)cfg.rpmPerDetent; break;
    }
    if (modeSel == OM_DRY && dryActive) return false;
    float before = setpoints[modeSel];
    float v = before + step * (float)detents;
    float max = setpointMax();
    float min = modeSel == OM_INTERP ? 0.1f : 0.0f;
    if (v < min) v = min;
    if (v > max) v = max;
    v = floorf(v / step + 0.5f) * step; /* stay on the step grid */
    setpoints[modeSel] = v;
    return v != before;
}

bool Model::selectMode(OpMode m)
{
    if ((int)m == modeSel) return true;
    bool allowed = op == OP_DISARMED || (op == OP_ARMED && standstill() && pedalReleased() && !dryActive);
    if (!allowed)
    {
        ui::feedback(ui::FB_WARN);
        setNotice("Stop before changing mode");
        return false;
    }
    modeSel = (int)m;
    brakeOff = false;
    dryActive = false;
    resetOrigin();
    ui::setMode(firmwareMode(modeSel));
    applyLimits();
    ui::feedback(ui::FB_CONFIRM);
    return true;
}

bool Model::toggleRpmTorque()
{
    return selectMode(modeSel == OM_TORQUE ? OM_RPM : OM_TORQUE);
}

bool Model::toggleDryTimer()
{
    if (modeSel != OM_DRY) return false;
    if (op != OP_ARMED)
    {
        ui::feedback(ui::FB_WARN);
        setNotice("Arm first: hold SIDE 1 s");
        return false;
    }
    dryActive = !dryActive;
    if (dryActive) dryEndMs = tele.millis + (uint32_t)cfg.dryMinutes * 60000u;
    brakeOff = false;
    ui::feedback(dryActive ? ui::FB_CONFIRM : ui::FB_CLICK);
    setNotice(dryActive ? "Dry run started" : "Dry run stopped");
    return true;
}

void Model::setTraverseDirection(int d)
{
    d = d < 0 ? -1 : 1;
    if (d == travDir) { ui::feedback(ui::FB_CLICK); return; }
    travDir = d;
    ui::feedback(ui::FB_CONFIRM);
    if (traverseOn)
    {
        /* Re-lock with the new sign from the current spindle position: no carriage jump. */
        float spm = stepsPerMm();
        const ui::Limits& lim = ui::limits();
        float scale = spm / (cfg.rotPerMm * lim.encoderCpr * lim.gearRatio) * (float)travDir;
        ui::stepperSync(scale, (float)tele.positionCounts);
    }
    setNotice(travDir > 0 ? "Traverse: forward" : "Traverse: reverse");
}

static const int SNAP_STEPS[] = { 0, 15, 30, 45, 60, 90, 120, 180 };

void Model::adjustSnap(int detents)
{
    const int n = (int)(sizeof(SNAP_STEPS) / sizeof(SNAP_STEPS[0]));
    int idx = 0;
    for (int i = 0; i < n; ++i) { if (SNAP_STEPS[i] == cfg.snapDeg) idx = i; }
    idx += detents;
    if (idx < 0) idx = 0;
    if (idx > n - 1) idx = n - 1;
    cfg.snapDeg = SNAP_STEPS[idx];
    applySnap();
}

void Model::adjustHoldTorque(int detents)
{
    float v = cfg.holdTorqueA + 0.05f * (float)detents;
    float max = ui::limits().maxTorqueA;
    if (v < 0.05f) v = 0.05f;
    if (v > max) v = max;
    cfg.holdTorqueA = floorf(v / 0.05f + 0.5f) * 0.05f;
    applySnap();
}

void Model::adjustStroke(int detents)
{
    float v = setpoints[OM_INTERP] + 0.1f * (float)detents;
    if (v < 0.1f) v = 0.1f;
    if (v > 2.0f) v = 2.0f;
    setpoints[OM_INTERP] = floorf(v / 0.1f + 0.5f) * 0.1f;
}

void Model::setRotPerMm(float value)
{
    cfg.rotPerMm = value < 0.05f ? 0.05f : value > 20.0f ? 20.0f : value;
}

bool Model::engageTraverse(bool on)
{
    if (on)
    {
        if (tele.faults & ui::FAULT_CRITICAL) return false;
        float spm = stepsPerMm();
        if (spm <= 0.0f || cfg.rotPerMm <= 0.0f) return false;
        const ui::Limits& lim = ui::limits();
        /* stepper steps per motor-encoder count, signed by the traverse direction */
        float scale = spm / (cfg.rotPerMm * lim.encoderCpr * lim.gearRatio) * (float)travDir;
        ui::stepperEnable(true);
        ui::stepperZero();
        ui::stepperSync(scale, (float)tele.positionCounts);
        traverseOn = true;
        traverseEnd = false;
    }
    else if (traverseOn)
    {
        ui::stepperUnsync();
        ui::stepperEnable(false);
        traverseOn = false;
    }
    return true;
}

void Model::applySettings()
{
    if (cfg.leadMm < 0.5f) cfg.leadMm = 0.5f;
    if (cfg.travelMm < 5.0f) cfg.travelMm = 5.0f;
    if (cfg.pedalFullCounts < 20) cfg.pedalFullCounts = 20;
    if (cfg.maxShaftRpm < 50) cfg.maxShaftRpm = 50;
    if (cfg.rpmPerDetent < 1) cfg.rpmPerDetent = 1;
    if (cfg.dryMinutes < 1) cfg.dryMinutes = 1;
    if (cfg.holdTorqueA < 0.05f) cfg.holdTorqueA = 0.05f;
    if (cfg.holdTorqueA > ui::limits().maxTorqueA) cfg.holdTorqueA = ui::limits().maxTorqueA;
    ui::setPedalFullCounts(cfg.pedalFullCounts);
    ui::setFeedbackEnabled(cfg.buzzer, cfg.vibration);
    if (cfg.rpmModeTorqueA < 0.1f) cfg.rpmModeTorqueA = 0.1f;
    if (cfg.rpmModeTorqueA > ui::limits().maxTorqueA) cfg.rpmModeTorqueA = ui::limits().maxTorqueA;
    if (cfg.torqueModeMaxRpm < 10) cfg.torqueModeMaxRpm = 10;
    ui::setBacklight((uint8_t)cfg.backlightPercent);
    applyLimits();
    int keep = modeSel;
    for (int m = 0; m < OM_COUNT; ++m)
    {
        modeSel = m;
        float max = setpointMax();
        if (setpoints[m] > max) setpoints[m] = max;
    }
    modeSel = keep;
}

void Model::resetTurns()
{
    turnsRev = 0.0f;
    ui::feedback(ui::FB_CONFIRM);
    setNotice("Turns counter reset");
}

void Model::confirmFaultClear()
{
    if (op != OP_FAULT) return;
    if (tele.faultCausePresent)
    {
        clearRefused = true;
        ui::feedback(ui::FB_WARN);
        return;
    }
    clearRefused = false;
    clearRequestedMs = tele.millis ? tele.millis : 1;
    ui::requestFaultClear();
    ui::feedback(ui::FB_CLICK);
}
