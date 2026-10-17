#ifndef UI_INPUT_H
#define UI_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Operator controls, decoded in the HID task and consumed by the TouchGFX
   model. Pure logic, no HAL: the HID task feeds levels and counters in,
   the GUI task pops events out. Single producer, single consumer. */

typedef enum
{
    UI_KEY_KNOB = 0, /* push button in the UI encoder knob */
    UI_KEY_FRW,
    UI_KEY_REV,
    UI_KEY_SIDE,
    UI_KEY_BACK,
    UI_KEY_COUNT
} UI_Key_t;

typedef enum
{
    UI_INPUT_NONE = 0,
    UI_INPUT_TURN,    /* knob turned; value = detents, sign = direction */
    UI_INPUT_PRESS,   /* key went down (debounced) */
    UI_INPUT_HOLD,    /* key held past its hold time; sent once per press */
    UI_INPUT_RELEASE, /* key went up; value = 1 when HOLD was sent for this press */
} UI_InputType_t;

typedef struct
{
    uint8_t type;  /* UI_InputType_t */
    uint8_t key;   /* UI_Key_t */
    int16_t value;
} UI_InputEvent_t;

#define UI_HOLD_MS_SIDE 1000u /* arm / disarm */
#define UI_HOLD_MS_BACK 2000u /* stop and disarm from anywhere */
#define UI_HOLD_MS_OTHER 700u

void UI_InputInit(void);
/* Call every ~10 ms from the HID task. pressed_mask bit n = (1u << UI_Key_t). */
void UI_InputPoll(uint32_t now_ms, int32_t knob_delta, uint32_t pressed_mask);
bool UI_InputPop(UI_InputEvent_t *event);
bool UI_KeyIsDown(UI_Key_t key);
/* Milliseconds the key has been held so far, 0 when up. Used for hold progress rings. */
uint32_t UI_KeyHeldMs(UI_Key_t key, uint32_t now_ms);

/* Pedal encoder (TIM4). Counts are zeroed by the pedal home switch. */
void UI_PedalSetCounts(int32_t counts);
int32_t UI_PedalGetCounts(void);
void UI_PedalSetFullCounts(uint16_t counts); /* counts at full travel, a user setting */
uint16_t UI_PedalGetFullCounts(void);
uint16_t UI_PedalPermille(void);             /* 0..1000 of full travel */
void UI_PedalMarkHomed(void);
bool UI_PedalIsHomed(void);

/* Non-blocking buzzer and vibration patterns, stepped from the HID task. */
typedef enum
{
    UI_FB_CLICK = 0, /* knob detent / key press */
    UI_FB_CONFIRM,   /* value accepted */
    UI_FB_ARM,
    UI_FB_DISARM,
    UI_FB_WARN,      /* rejected action */
    UI_FB_TIMER,     /* dry-run timer finished */
    UI_FB_FAULT,
    UI_FB_COUNT
} UI_Feedback_t;

typedef void (*UI_ToneFn)(uint32_t hz); /* 0 = silent */
typedef void (*UI_VibFn)(bool on);

void UI_FeedbackBind(UI_ToneFn tone, UI_VibFn vib);
void UI_FeedbackSetEnabled(bool buzzer, bool vibration);
void UI_FeedbackPlay(UI_Feedback_t pattern);
void UI_FeedbackTick(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* UI_INPUT_H */
