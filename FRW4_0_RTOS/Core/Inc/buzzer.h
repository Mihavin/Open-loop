#ifndef BUZZER_H
#define BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"   // If you use another STM32 family, replace accordingly.

typedef struct
{
  TIM_HandleTypeDef *htim;     // should point to htim8
  uint32_t channel;            // TIM_CHANNEL_1
  uint32_t tim_clk_hz;         // resolved from RCC at init (TIM8 clock)
  uint8_t  duty_percent;       // 0..100
  uint8_t  is_running;         // internal
} Buzzer_Handle_t;

/**
 * @brief Initialize a buzzer instance for TIM8 CH1.
 *        TIM8 must already be initialized by CubeMX (MX_TIM8_Init()).
 */
void Buzzer_Init(Buzzer_Handle_t *bz, TIM_HandleTypeDef *htim8);

/**
 * @brief Start (or retune) the buzzer to a frequency (Hz) and duty (%).
 * @param freq_hz 1..(timer_clk/2). If 0, this stops the buzzer.
 * @param duty_percent 0..100 (50 is typical).
 */
HAL_StatusTypeDef Buzzer_ToneHz(Buzzer_Handle_t *bz, uint32_t freq_hz, uint8_t duty_percent);

/**
 * @brief Stop PWM output (silence).
 */
HAL_StatusTypeDef Buzzer_Stop(Buzzer_Handle_t *bz);

/**
 * @brief Simple blocking beep: start tone, delay, stop.
 *        Uses HAL_Delay(), so it's blocking.
 */
HAL_StatusTypeDef Buzzer_Beep(Buzzer_Handle_t *bz, uint32_t freq_hz, uint32_t duration_ms, uint8_t duty_percent);

#ifdef __cplusplus
}
#endif

#endif // BUZZER_H
