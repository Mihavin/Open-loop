#include "buzzer.h"

#ifndef BUZZER_MAX_ARR
#define BUZZER_MAX_ARR  (0xFFFFu)   // TIM8 is 16-bit on STM32H7
#endif

static uint32_t buzzer_get_tim8_clk_hz(void)
{
  // Timer clock rule (STM32): if APB prescaler != 1, timer clock = 2 * PCLK
  RCC_ClkInitTypeDef clk = {0};
  uint32_t flash_latency = 0;
  HAL_RCC_GetClockConfig(&clk, &flash_latency);

  uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
  if (clk.APB2CLKDivider != RCC_HCLK_DIV1)
  {
    return pclk2 * 2u;
  }
  return pclk2;
}

static HAL_StatusTypeDef buzzer_apply(Buzzer_Handle_t *bz, uint32_t freq_hz, uint8_t duty_percent)
{
  if (bz == NULL || bz->htim == NULL)
    return HAL_ERROR;

  if (freq_hz == 0u || duty_percent == 0u)
  {
    return Buzzer_Stop(bz);
  }

  if (duty_percent > 100u)
    duty_percent = 100u;

  uint64_t tim_clk = (bz->tim_clk_hz != 0u) ? bz->tim_clk_hz : buzzer_get_tim8_clk_hz();

  // Guard against impossible frequencies (avoid divide-by-zero / tiny ARR)
  if (freq_hz >= (uint32_t)(tim_clk / 2u))
    freq_hz = (uint32_t)(tim_clk / 2u);

  // We want: freq = tim_clk / ((PSC+1)*(ARR+1))
  // Choose a prescaler factor so ARR fits in 16-bit.
  uint64_t ticks = tim_clk / (uint64_t)freq_hz;   // desired total ticks per period (approx)

  if (ticks < 2u)
    ticks = 2u;

  // presc_factor = ceil(ticks / (BUZZER_MAX_ARR+1))
  uint64_t presc_factor = (ticks + (uint64_t)BUZZER_MAX_ARR) / ((uint64_t)BUZZER_MAX_ARR + 1u);
  if (presc_factor < 1u)
    presc_factor = 1u;
  if (presc_factor > 65536u) // PSC is 16-bit => PSC+1 max 65536
    presc_factor = 65536u;

  // arr_plus1 = tim_clk / (freq * presc_factor)
  uint64_t arr_plus1 = tim_clk / ((uint64_t)freq_hz * presc_factor);
  if (arr_plus1 < 1u)
    arr_plus1 = 1u;
  if (arr_plus1 > ((uint64_t)BUZZER_MAX_ARR + 1u))
    arr_plus1 = (uint64_t)BUZZER_MAX_ARR + 1u;

  uint32_t psc = (uint32_t)(presc_factor - 1u);
  uint32_t arr = (uint32_t)(arr_plus1 - 1u);

  // Duty -> CCR
  uint32_t ccr = (uint32_t)(((uint64_t)(arr + 1u) * duty_percent) / 100u);
  if (ccr > arr) ccr = arr; // clamp

  // Safest simple approach: stop PWM, update registers, start again.
  (void)HAL_TIM_PWM_Stop(bz->htim, bz->channel);

  __HAL_TIM_DISABLE(bz->htim);

  __HAL_TIM_SET_PRESCALER(bz->htim, psc);
  __HAL_TIM_SET_AUTORELOAD(bz->htim, arr);
  __HAL_TIM_SET_COMPARE(bz->htim, bz->channel, ccr);

  // Force update event so PSC/ARR take effect immediately
  __HAL_TIM_SET_COUNTER(bz->htim, 0u);
  HAL_TIM_GenerateEvent(bz->htim, TIM_EVENTSOURCE_UPDATE);

  __HAL_TIM_ENABLE(bz->htim);

  if (HAL_TIM_PWM_Start(bz->htim, bz->channel) != HAL_OK)
    return HAL_ERROR;

#if defined(TIM_BDTR_MOE)
  // TIM8 is an advanced timer; make sure MOE is enabled
  __HAL_TIM_MOE_ENABLE(bz->htim);
#endif

  bz->duty_percent = duty_percent;
  bz->is_running = 1u;
  return HAL_OK;
}

void Buzzer_Init(Buzzer_Handle_t *bz, TIM_HandleTypeDef *htim8)
{
  if (bz == NULL) return;

  bz->htim         = htim8;
  bz->channel      = TIM_CHANNEL_1;
  bz->tim_clk_hz   = buzzer_get_tim8_clk_hz();
  bz->duty_percent = 50u;
  bz->is_running   = 0u;

  // Ensure it's stopped initially
  if (bz->htim != NULL)
  {
    (void)HAL_TIM_PWM_Stop(bz->htim, bz->channel);
#if defined(TIM_BDTR_MOE)
    __HAL_TIM_MOE_DISABLE(bz->htim);
#endif
  }
}

HAL_StatusTypeDef Buzzer_ToneHz(Buzzer_Handle_t *bz, uint32_t freq_hz, uint8_t duty_percent)
{
  return buzzer_apply(bz, freq_hz, duty_percent);
}

HAL_StatusTypeDef Buzzer_Stop(Buzzer_Handle_t *bz)
{
  if (bz == NULL || bz->htim == NULL)
    return HAL_ERROR;

  (void)HAL_TIM_PWM_Stop(bz->htim, bz->channel);

#if defined(TIM_BDTR_MOE)
  __HAL_TIM_MOE_DISABLE(bz->htim);
#endif

  bz->is_running = 0u;
  return HAL_OK;
}

HAL_StatusTypeDef Buzzer_Beep(Buzzer_Handle_t *bz, uint32_t freq_hz, uint32_t duration_ms, uint8_t duty_percent)
{
  HAL_StatusTypeDef st = Buzzer_ToneHz(bz, freq_hz, duty_percent);
  if (st != HAL_OK) return st;

  HAL_Delay(duration_ms);

  return Buzzer_Stop(bz);
}
