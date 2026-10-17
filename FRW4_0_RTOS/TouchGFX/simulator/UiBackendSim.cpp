/* GUI backend for the PC simulator: a small fake plant and keyboard controls.
 *
 * Keys (the simulator window must have focus):
 *   + / -   turn the knob          Enter / space  press the knob      ` (backtick) hold the knob
 *   f / r   FRW / REV              g              FRW+REV together (traverse lock)
 *   s / S   SIDE press / hold      b / B          BACK press / hold
 *   p / o   pedal down / up        h              pedal home switch
 *   a / z   turn the spindle by hand (+/- 15 motor degrees; in torque mode: spin it up)
 *   x / c   assert / release a motor-driver fault line
 *   u       user emergency stop    q              quit
 *
 * FRW4_SIM_SCRIPT="frame:key,frame:key,..." injects keys at frame numbers and
 * FRW4_SIM_SHOTS="frame:name,..." saves screenshots/<name>.bmp, for automated runs.
 * FRW4_SIM_EXIT=frame quits at that frame.
 */
#include <gui/common/UiBackend.hpp>
#include <platform/hal/simulator/sdl2/HALSDL2.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace ui
{
/* Hooks called from FrontendApplication in simulator builds. */
void simulatorKey(uint8_t c);
void simulatorFrameDone();

static const Limits g_limits = { 2.0f, 4000.0f, 2000.0f, 1.6f, 10.0f, 3200.0f };

namespace
{
struct Plant
{
    uint8_t state;
    uint8_t mode;
    float cmdRpm, cmdIq, cmdPosRev;
    float snapCounts, snapOrigin, snapHoldIq;
    float extRpm; /* torque mode: spindle turned from outside, braked by cmdIq */
    uint32_t idleFrames;
    float rpm;
    float posCounts;
    float stepperSteps, stepperScale, stepperOffset;
    uint8_t stepperState;
    bool stepperEnabled;
    uint32_t faults;
    bool nfaultAsserted;
    bool clearRequested;
    int pedalPermille;
    bool pedalHomed;
    uint32_t alignUntil;
    uint32_t frame;
    uint32_t sideDownSince, backDownSince;
};
Plant p;

struct Queue
{
    InputEvent items[32];
    unsigned head, tail;
    void push(uint8_t type, uint8_t key, int16_t value)
    {
        unsigned next = (head + 1) % 32;
        if (next == tail) return;
        items[head].type = type;
        items[head].key = key;
        items[head].value = value;
        head = next;
    }
};
Queue q;

uint32_t millisNow() { return p.frame * 1000u / 60u; }

void pressRelease(Key key, bool hold)
{
    q.push(IN_PRESS, key, 0);
    if (hold) q.push(IN_HOLD, key, 0);
    q.push(IN_RELEASE, key, hold ? 1 : 0);
}

const char* g_script;
const char* g_shots;
uint32_t g_exitFrame;

void injectKey(char c)
{
    switch (c)
    {
    case '+': case '=': case 'k': q.push(IN_TURN, KEY_KNOB, 1); break;
    case '-': case '_': case 'j': q.push(IN_TURN, KEY_KNOB, -1); break;
    case '\r': case '\n': case ' ': pressRelease(KEY_KNOB, false); break;
    case '`': pressRelease(KEY_KNOB, true); break;
    case 'f': pressRelease(KEY_FRW, false); break;
    case 'r': pressRelease(KEY_REV, false); break;
    case 'g': q.push(IN_PRESS, KEY_FRW, 0); q.push(IN_PRESS, KEY_REV, 0); q.push(IN_RELEASE, KEY_REV, 0); q.push(IN_RELEASE, KEY_FRW, 0); break;
    case 's': pressRelease(KEY_SIDE, false); break;
    case 'S': pressRelease(KEY_SIDE, true); break;
    case 'b': pressRelease(KEY_BACK, false); break;
    case 'B': pressRelease(KEY_BACK, true); break;
    case 'p': p.pedalPermille += 100; if (p.pedalPermille > 1000) p.pedalPermille = 1000; break;
    case 'o': p.pedalPermille -= 100; if (p.pedalPermille < 0) p.pedalPermille = 0; break;
    case 'h': p.pedalHomed = true; p.pedalPermille = 0; break;
    case 'a': if (p.mode == MODE_TORQUE) p.extRpm += 150.0f; else p.posCounts += g_limits.encoderCpr / 24.0f; break;
    case 'z': if (p.mode == MODE_TORQUE) p.extRpm -= 150.0f; else p.posCounts -= g_limits.encoderCpr / 24.0f; break;
    case 'x': p.nfaultAsserted = true; p.faults |= FAULT_BLDC_DRIVER; break;
    case 'c': p.nfaultAsserted = false; break;
    case 'u': p.faults |= FAULT_USER_STOP; break;
    case 'q': touchgfx::HALSDL2::stopApplication(); break;
    default: break;
    }
}

/* Walk "frame:token,frame:token" and call fn(token) for entries at the current frame. */
template <typename F>
void runSchedule(const char* schedule, F fn)
{
    if (!schedule) return;
    const char* s = schedule;
    while (*s)
    {
        char* end;
        unsigned long frame = strtoul(s, &end, 10);
        if (end == s || *end != ':') break;
        const char* token = end + 1;
        const char* comma = strchr(token, ',');
        size_t len = comma ? (size_t)(comma - token) : strlen(token);
        if (frame == p.frame && len) fn(token, len);
        s = comma ? comma + 1 : token + len;
    }
}

void step()
{
    const float dt = 1.0f / 60.0f;
    p.frame++;
    runSchedule(g_script, [](const char* t, size_t) { injectKey(t[0]); });
    if (g_exitFrame && p.frame >= g_exitFrame) touchgfx::HALSDL2::stopApplication();

    if (p.faults & FAULT_CRITICAL)
    {
        p.state = BLDC_FAULT;
        p.rpm = 0;
        p.stepperState = p.stepperEnabled ? STEP_FAULT : STEP_IDLE;
        return;
    }
    if (p.state == BLDC_FAULT) p.state = BLDC_IDLE;
    if (p.state == BLDC_ALIGN && millisNow() >= p.alignUntil) p.state = BLDC_RUN;

    float target = 0;
    if (p.state == BLDC_RUN)
    {
        if (p.mode == MODE_SPEED) target = p.cmdRpm;
        else if (p.mode == MODE_TORQUE)
        {
            /* An external hand spins the spindle (extRpm); the brake current opposes it. */
            float brake = p.cmdIq / g_limits.maxTorqueA * 1500.0f; /* rpm/s at full current */
            p.extRpm += (brake - p.extRpm * 0.15f) * dt;
            if (p.extRpm * (p.extRpm - brake * dt) < 0.0f) p.extRpm = 0.0f;
            target = p.extRpm;
        }
        else
        {
            /* Detent: after 300 ms without a new reference, a shaft pushed past half a
               pitch pulls the reference to the nearest grid point. */
            if (p.snapCounts > 0 && ++p.idleFrames >= 18)
            {
                float ref = p.cmdPosRev * g_limits.encoderCpr;
                float dev = p.posCounts - ref;
                if (fabsf(dev) > p.snapCounts / 2)
                {
                    float k = floorf((p.posCounts - p.snapOrigin) / p.snapCounts + 0.5f);
                    p.cmdPosRev = (p.snapOrigin + k * p.snapCounts) / g_limits.encoderCpr;
                }
            }
            float err = p.cmdPosRev * g_limits.encoderCpr - p.posCounts; /* counts */
            target = err * 0.02f * 60.0f / g_limits.encoderCpr * 60.0f;
            if (target > 600) target = 600;
            if (target < -600) target = -600;
        }
    }
    p.rpm += (target - p.rpm) * (dt / 0.35f);
    if (fabsf(p.rpm) < 0.5f && target == 0) p.rpm = 0;
    p.posCounts += p.rpm / 60.0f * g_limits.encoderCpr * dt;
    if (p.stepperState == STEP_SYNC)
        p.stepperSteps = (p.posCounts - p.stepperOffset) * p.stepperScale;
}
} // namespace

void init()
{
    memset(&p, 0, sizeof(p));
    memset(&q, 0, sizeof(q));
    p.mode = MODE_SPEED;
    p.state = BLDC_IDLE;
    g_script = getenv("FRW4_SIM_SCRIPT");
    g_shots = getenv("FRW4_SIM_SHOTS");
    const char* e = getenv("FRW4_SIM_EXIT");
    g_exitFrame = e ? (uint32_t)strtoul(e, 0, 10) : 0;
}

const Limits& limits() { return g_limits; }

void simulatorKey(uint8_t c) { injectKey((char)c); }

/* Called once per frame by the simulator main loop hook (see FrontendApplication). */
void simulatorFrameDone()
{
    runSchedule(g_shots, [](const char* t, size_t len) {
        char name[64];
        if (len >= sizeof(name) - 5) len = sizeof(name) - 5;
        memcpy(name, t, len);
        strcpy(name + len, ".bmp");
        static_cast<touchgfx::HALSDL2*>(touchgfx::HAL::getInstance())->saveScreenshot(0, name);
    });
}

bool popInput(InputEvent& event)
{
    if (q.tail == q.head) return false;
    event = q.items[q.tail];
    q.tail = (q.tail + 1) % 32;
    return true;
}

void read(Telemetry& out)
{
    step();
    out.bldcState = p.state;
    out.bldcMode = p.mode;
    out.speedRpm = p.rpm;
    float iq = p.state == BLDC_RUN ? 0.15f + fabsf(p.rpm) / g_limits.maxMotorRpm * 1.1f : 0.0f;
    float ph = p.posCounts / g_limits.encoderCpr * 6.2831853f * 7.0f;
    out.ia = iq * cosf(ph);
    out.ib = iq * cosf(ph - 2.0944f);
    out.ic = iq * cosf(ph + 2.0944f);
    out.positionCounts = (int32_t)p.posCounts;
    out.positionRefCounts = (int32_t)(p.cmdPosRev * g_limits.encoderCpr);
    out.stepperState = p.stepperState;
    out.stepperPositionSteps = p.stepperSteps;
    out.faults = p.faults;
    out.faultCausePresent = p.nfaultAsserted;
    out.pedalPermille = (uint16_t)p.pedalPermille;
    out.pedalHomed = p.pedalHomed;
    out.sideHeldMs = 0;
    out.backHeldMs = 0;
    out.millis = millisNow();
}

void setMode(Mode mode)
{
    if (fabsf(p.rpm) < 10.0f) p.mode = (uint8_t)mode;
}
void setSpeed(float motorRpm) { p.cmdRpm = motorRpm; p.cmdIq = 0; }
void setTorque(float iqA) { p.cmdIq = iqA; p.cmdRpm = 0; }
void setPositionRev(float motorRev) { p.cmdPosRev = motorRev; p.idleFrames = 0; }
void setPositionSnap(float pitchDeg, int32_t originCounts, float holdIqA)
{
    p.snapCounts = pitchDeg > 0.0f ? pitchDeg / 360.0f * g_limits.encoderCpr : 0.0f;
    p.snapOrigin = (float)originCounts;
    p.snapHoldIq = holdIqA;
    p.idleFrames = 0;
}

void arm()
{
    if (p.state != BLDC_IDLE || (p.faults & FAULT_CRITICAL)) return;
    p.state = BLDC_ALIGN;
    p.alignUntil = millisNow() + 700;
}
void disarm() { if (p.state != BLDC_FAULT) p.state = BLDC_IDLE; p.cmdRpm = p.cmdIq = 0; }
void stop() { disarm(); }

void requestFaultClear()
{
    if (!p.nfaultAsserted) p.faults = 0;
}

void stepperEnable(bool enable)
{
    p.stepperEnabled = enable;
    if (!enable || p.stepperState == STEP_FAULT) p.stepperState = STEP_IDLE;
}
void stepperSync(float scale, float offset)
{
    if (!p.stepperEnabled || (p.faults & FAULT_CRITICAL)) return;
    p.stepperScale = scale;
    p.stepperOffset = offset;
    p.stepperState = STEP_SYNC;
}
void stepperUnsync() { if (p.stepperState == STEP_SYNC) p.stepperState = STEP_IDLE; }
void stepperStop() { p.stepperState = STEP_IDLE; }
void stepperZero() { p.stepperSteps = 0; }

void feedback(Feedback pattern)
{
    static const char* names[] = { "click", "confirm", "arm", "disarm", "warn", "timer", "fault" };
    if (getenv("FRW4_SIM_VERBOSE")) printf("[frame %u] feedback %s\n", (unsigned)p.frame, names[pattern]);
}
void setFeedbackEnabled(bool, bool) {}
void setPedalFullCounts(uint16_t) {}
void setBacklight(uint8_t) {}

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
