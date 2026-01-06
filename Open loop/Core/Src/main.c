#include "main.h"

#include "gpio.h"
#include "dma.h"
#include "bdma.h"
#include "adc.h"
#include "tim.h"

#include "foc.h"

#include <math.h>
#include <stdbool.h>

// ==============================
// User settings
// ==============================

#define CONTROL_LOOP_HZ          20000U    // must match foc.c CONTROL_HZ

// Current offset calibration (motor stopped, PWM outputs disabled)
#define OFFSET_CAL_SAMPLES       300U
#define OFFSET_CAL_DELAY_MS      1U

static void SystemClock_Config(void);

static void TIM6_SetFrequency(uint32_t control_hz)
{
    // Configure TIM6 to generate update IRQ at control_hz.
    // Use a 1 MHz timer tick for easy division.
    uint32_t tim_clk = HAL_RCC_GetPCLK1Freq();
    // On STM32H7, if APB prescaler != 1 then TIMxCLK = 2*PCLK
    // Cube typically sets APB1 prescaler to /2, so multiplier applies.
    tim_clk *= 2U;

    const uint32_t tick_hz = 1000000U;
    uint32_t psc = (tim_clk / tick_hz) - 1U;
    uint32_t arr = (tick_hz / control_hz) - 1U;

    __HAL_TIM_DISABLE(&htim6);
    __HAL_TIM_SET_PRESCALER(&htim6, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim6, arr);
    __HAL_TIM_SET_COUNTER(&htim6, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE(&htim6);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_BDMA_Init();

    MX_ADC1_Init();
    MX_ADC2_Init();
    MX_ADC3_Init();

    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_TIM6_Init();

    // ---------------- Driver enable / CAL sequence (kept from your previous code) ----------------
    HAL_Delay(10);
    HAL_GPIO_WritePin(EN_DRV_GPIO_Port, EN_DRV_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(EN_DRV_GPIO_Port, EN_DRV_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(EN_DRV_GPIO_Port, EN_DRV_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    // ---------------- ADC calibration ----------------
    // NOTE: If this hangs, it means ADC kernel clock isn't configured.
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);

    // Start injected conversions (triggered by TIM1 TRGO2 UPDATE)
    HAL_ADCEx_InjectedStart(&hadc1);
    HAL_ADCEx_InjectedStart(&hadc2);
    HAL_ADCEx_InjectedStart(&hadc3);

    // ---------------- Encoder start (TIM2) ----------------
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);

    // ---------------- PWM start (TIM1) ----------------
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

    // Ensure TIM1 counter is running (needed for TRGO2 -> ADC injected trigger)
    HAL_TIM_Base_Start(&htim1);

    // Keep outputs disabled during offset calibration
    __HAL_TIM_MOE_DISABLE(&htim1);

    FOC_Init();

    // TIM1 must be running so TRGO2 keeps updating injected conversions.
    // With MOE disabled, the gate outputs remain off.
    FOC_CalibrateCurrentOffsets(OFFSET_CAL_SAMPLES, OFFSET_CAL_DELAY_MS);

    // Enable outputs and start control loop
    __HAL_TIM_MOE_ENABLE(&htim1);

    // TIM6 IRQ enable (CubeMX project didn't enable it)
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);

    TIM6_SetFrequency(CONTROL_LOOP_HZ);
    HAL_TIM_Base_Start_IT(&htim6);

    uint32_t last_hb = HAL_GetTick();

    while (1)
    {
        if (FOC_IsTripped())
        {
            // Hard trip latched: blink LED0 fast
            __HAL_TIM_MOE_DISABLE(&htim1);
            HAL_GPIO_WritePin(EN_DRV_GPIO_Port, EN_DRV_Pin, GPIO_PIN_RESET);

            while (1)
            {
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_15);
                HAL_Delay(100);
            }
        }

        // Heartbeat
        uint32_t now = HAL_GetTick();
        if ((now - last_hb) >= 250U)
        {
            last_hb = now;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_15 | GPIO_PIN_8);
            HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_14);
        }

        __WFI();
    }
}

// TIM6 period callback used by HAL_TIM_IRQHandler
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        FOC_ControlStep();
    }
}

// =========================
// Clock config
// =========================

#include "stm32h7xx_hal_rcc_ex.h"

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // Supply configuration update enable
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  // Configure the main internal regulator output voltage
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  // PLL1 (CPU)
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = 64;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 30;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  // Bus clocks
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  // IMPORTANT: ADC kernel clock. Without this, ADC calibration may hang.
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;

  PeriphClkInitStruct.PLL2.PLL2M = 4;
  PeriphClkInitStruct.PLL2.PLL2N = 60;
  PeriphClkInitStruct.PLL2.PLL2P = 2;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;

  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

