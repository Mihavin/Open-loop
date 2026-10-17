#include "ui_input.h"

#define QUEUE_SIZE 32u /* power of two */
#define DEBOUNCE_SAMPLES 2u

typedef struct
{
    uint8_t raw_count;   /* consecutive samples at the new level */
    bool down;           /* debounced level */
    bool hold_sent;
    uint32_t down_since;
} KeyState;

static UI_InputEvent_t g_queue[QUEUE_SIZE];
static volatile uint8_t g_head, g_tail;
static KeyState g_keys[UI_KEY_COUNT];
static int32_t g_knob_accum;

static volatile int32_t g_pedal_counts;
static volatile uint16_t g_pedal_full = 200u;
static volatile bool g_pedal_homed;

static void push(uint8_t type, uint8_t key, int16_t value)
{
    uint8_t head = g_head;
    uint8_t next = (uint8_t)((head + 1u) & (QUEUE_SIZE - 1u));
    if (next == g_tail) return; /* GUI stalled; drop rather than block the HID task */
    g_queue[head].type = type;
    g_queue[head].key = key;
    g_queue[head].value = value;
    __atomic_store_n(&g_head, next, __ATOMIC_RELEASE);
}

static uint32_t hold_ms(UI_Key_t key)
{
    switch (key)
    {
    case UI_KEY_SIDE: return UI_HOLD_MS_SIDE;
    case UI_KEY_BACK: return UI_HOLD_MS_BACK;
    default: return UI_HOLD_MS_OTHER;
    }
}

void UI_InputInit(void)
{
    g_head = g_tail = 0;
    g_knob_accum = 0;
    for (unsigned i = 0; i < UI_KEY_COUNT; ++i)
    {
        g_keys[i].raw_count = 0;
        g_keys[i].down = false;
        g_keys[i].hold_sent = false;
        g_keys[i].down_since = 0;
    }
}

void UI_InputPoll(uint32_t now_ms, int32_t knob_delta, uint32_t pressed_mask)
{
    /* Knob: the TIM3 count per detent varies with the encoder; report every count. */
    g_knob_accum += knob_delta;
    if (g_knob_accum != 0)
    {
        int32_t v = g_knob_accum > 127 ? 127 : g_knob_accum < -127 ? -127 : g_knob_accum;
        push(UI_INPUT_TURN, UI_KEY_KNOB, (int16_t)v);
        g_knob_accum -= v;
    }

    for (unsigned k = 0; k < UI_KEY_COUNT; ++k)
    {
        KeyState *s = &g_keys[k];
        bool raw = (pressed_mask >> k) & 1u;
        if (raw == s->down)
        {
            s->raw_count = 0;
        }
        else if (++s->raw_count >= DEBOUNCE_SAMPLES)
        {
            s->raw_count = 0;
            s->down = raw;
            if (raw)
            {
                s->down_since = now_ms;
                s->hold_sent = false;
                push(UI_INPUT_PRESS, (uint8_t)k, 0);
            }
            else
            {
                push(UI_INPUT_RELEASE, (uint8_t)k, s->hold_sent ? 1 : 0);
            }
        }
        if (s->down && !s->hold_sent && (uint32_t)(now_ms - s->down_since) >= hold_ms((UI_Key_t)k))
        {
            s->hold_sent = true;
            push(UI_INPUT_HOLD, (uint8_t)k, 0);
        }
    }
}

bool UI_InputPop(UI_InputEvent_t *event)
{
    uint8_t tail = g_tail;
    if (tail == __atomic_load_n(&g_head, __ATOMIC_ACQUIRE)) return false;
    *event = g_queue[tail];
    __atomic_store_n(&g_tail, (uint8_t)((tail + 1u) & (QUEUE_SIZE - 1u)), __ATOMIC_RELEASE);
    return true;
}

bool UI_KeyIsDown(UI_Key_t key)
{
    return (unsigned)key < UI_KEY_COUNT && g_keys[key].down;
}

uint32_t UI_KeyHeldMs(UI_Key_t key, uint32_t now_ms)
{
    if ((unsigned)key >= UI_KEY_COUNT || !g_keys[key].down) return 0;
    return now_ms - g_keys[key].down_since;
}

/* ---- pedal ---- */

void UI_PedalSetCounts(int32_t counts) { __atomic_store_n(&g_pedal_counts, counts, __ATOMIC_RELEASE); }
int32_t UI_PedalGetCounts(void) { return __atomic_load_n(&g_pedal_counts, __ATOMIC_ACQUIRE); }
void UI_PedalSetFullCounts(uint16_t counts) { g_pedal_full = counts ? counts : 1u; }
uint16_t UI_PedalGetFullCounts(void) { return g_pedal_full; }
void UI_PedalMarkHomed(void) { g_pedal_homed = true; }
bool UI_PedalIsHomed(void) { return g_pedal_homed; }

uint16_t UI_PedalPermille(void)
{
    int32_t counts = UI_PedalGetCounts();
    if (counts < 0) counts = -counts; /* travel direction depends on the encoder wiring */
    uint32_t full = g_pedal_full;
    uint32_t permille = ((uint32_t)counts * 1000u + full / 2u) / full;
    return permille > 1000u ? 1000u : (uint16_t)permille;
}

/* ---- feedback ---- */

typedef struct { uint16_t tone_hz; uint8_t vib; uint8_t ms; } FbStep; /* ms in units of 10 ms */

#define STEPS 5
static const FbStep g_patterns[UI_FB_COUNT][STEPS] = {
    [UI_FB_CLICK]   = { {3000, 0, 1} },
    [UI_FB_CONFIRM] = { {2200, 0, 4}, {0, 0, 3}, {2800, 0, 6} },
    [UI_FB_ARM]     = { {1800, 1, 8}, {2400, 1, 8}, {3000, 0, 12} },
    [UI_FB_DISARM]  = { {2400, 0, 8}, {1600, 0, 12} },
    [UI_FB_WARN]    = { {900, 1, 8}, {0, 0, 6}, {900, 0, 8} },
    [UI_FB_TIMER]   = { {2600, 1, 30}, {0, 0, 15}, {2600, 1, 30}, {0, 0, 15}, {2600, 1, 30} },
    [UI_FB_FAULT]   = { {1200, 1, 25}, {0, 0, 10}, {1200, 1, 25}, {0, 0, 10}, {1200, 0, 25} },
};

static UI_ToneFn g_tone;
static UI_VibFn g_vib;
static bool g_buzzer_on = true, g_vib_on = true;
static volatile int8_t g_requested = -1;
static int8_t g_playing = -1;
static uint8_t g_step;
static uint32_t g_step_start;

void UI_FeedbackBind(UI_ToneFn tone, UI_VibFn vib) { g_tone = tone; g_vib = vib; }
void UI_FeedbackSetEnabled(bool buzzer, bool vibration) { g_buzzer_on = buzzer; g_vib_on = vibration; }

void UI_FeedbackPlay(UI_Feedback_t pattern)
{
    if ((unsigned)pattern >= UI_FB_COUNT) return;
    /* A fault or arm sound replaces a click; a click never interrupts them. */
    int8_t current = __atomic_load_n(&g_requested, __ATOMIC_ACQUIRE);
    if (current >= 0 && (int)pattern < current) return;
    __atomic_store_n(&g_requested, (int8_t)pattern, __ATOMIC_RELEASE);
}

static void apply(const FbStep *step)
{
    if (g_tone) g_tone((step && g_buzzer_on) ? step->tone_hz : 0u);
    if (g_vib) g_vib(step && g_vib_on && step->vib);
}

void UI_FeedbackTick(uint32_t now_ms)
{
    int8_t requested = __atomic_exchange_n(&g_requested, (int8_t)-1, __ATOMIC_ACQ_REL);
    if (requested >= 0 && (g_playing < 0 || requested >= g_playing))
    {
        g_playing = requested;
        g_step = 0;
        g_step_start = now_ms;
        apply(&g_patterns[g_playing][0]);
        return;
    }
    if (g_playing < 0) return;
    const FbStep *step = &g_patterns[g_playing][g_step];
    if ((uint32_t)(now_ms - g_step_start) < (uint32_t)step->ms * 10u) return;
    g_step++;
    if (g_step >= STEPS || g_patterns[g_playing][g_step].ms == 0)
    {
        g_playing = -1;
        apply(0);
        return;
    }
    g_step_start = now_ms;
    apply(&g_patterns[g_playing][g_step]);
}
