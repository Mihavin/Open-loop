/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED0_Pin GPIO_PIN_3
#define LED0_GPIO_Port GPIOE
#define SIDE_BUTTON_Pin GPIO_PIN_8
#define SIDE_BUTTON_GPIO_Port GPIOI
#define BACK_BUTTON_Pin GPIO_PIN_13
#define BACK_BUTTON_GPIO_Port GPIOC
#define LIMIT_BUTTON_Pin GPIO_PIN_14
#define LIMIT_BUTTON_GPIO_Port GPIOC
#define UI_ENC_BUTTON_Pin GPIO_PIN_15
#define UI_ENC_BUTTON_GPIO_Port GPIOC
#define DRV_EN_Pin GPIO_PIN_6
#define DRV_EN_GPIO_Port GPIOF
#define SOC_Pin GPIO_PIN_7
#define SOC_GPIO_Port GPIOF
#define ENC_MOSI_Pin GPIO_PIN_1
#define ENC_MOSI_GPIO_Port GPIOC
#define ENC_MISO_Pin GPIO_PIN_2
#define ENC_MISO_GPIO_Port GPIOC
#define A_ENC_Pin GPIO_PIN_0
#define A_ENC_GPIO_Port GPIOA
#define B_ENC_Pin GPIO_PIN_1
#define B_ENC_GPIO_Port GPIOA
#define VREF_EN_Pin GPIO_PIN_5
#define VREF_EN_GPIO_Port GPIOH
#define EXT_NSS_Pin GPIO_PIN_4
#define EXT_NSS_GPIO_Port GPIOA
#define DRV_SCK_Pin GPIO_PIN_5
#define DRV_SCK_GPIO_Port GPIOA
#define SOB_Pin GPIO_PIN_6
#define SOB_GPIO_Port GPIOA
#define A_UI_ENC_Pin GPIO_PIN_7
#define A_UI_ENC_GPIO_Port GPIOA
#define SOA_Pin GPIO_PIN_4
#define SOA_GPIO_Port GPIOC
#define DRV_CAL_Pin GPIO_PIN_5
#define DRV_CAL_GPIO_Port GPIOC
#define I_ENC_Pin GPIO_PIN_1
#define I_ENC_GPIO_Port GPIOB
#define FAN_FAULT_Pin GPIO_PIN_6
#define FAN_FAULT_GPIO_Port GPIOH
#define KLV_LED_Pin GPIO_PIN_10
#define KLV_LED_GPIO_Port GPIOH
#define DRV_NFAULT_Pin GPIO_PIN_12
#define DRV_NFAULT_GPIO_Port GPIOB
#define INLA_Pin GPIO_PIN_13
#define INLA_GPIO_Port GPIOB
#define INLB_Pin GPIO_PIN_14
#define INLB_GPIO_Port GPIOB
#define INLC_Pin GPIO_PIN_15
#define INLC_GPIO_Port GPIOB
#define B_PED_ENC_Pin GPIO_PIN_12
#define B_PED_ENC_GPIO_Port GPIOD
#define B_UI_ENC_Pin GPIO_PIN_6
#define B_UI_ENC_GPIO_Port GPIOC
#define INHA_Pin GPIO_PIN_8
#define INHA_GPIO_Port GPIOA
#define INHB_Pin GPIO_PIN_9
#define INHB_GPIO_Port GPIOA
#define INHC_Pin GPIO_PIN_10
#define INHC_GPIO_Port GPIOA
#define ENC_SCK_Pin GPIO_PIN_12
#define ENC_SCK_GPIO_Port GPIOA
#define ENC_NSS_Pin GPIO_PIN_0
#define ENC_NSS_GPIO_Port GPIOI
#define EXT_SCK_Pin GPIO_PIN_10
#define EXT_SCK_GPIO_Port GPIOC
#define EXT_MISO_Pin GPIO_PIN_11
#define EXT_MISO_GPIO_Port GPIOC
#define EXT_MOSI_Pin GPIO_PIN_12
#define EXT_MOSI_GPIO_Port GPIOC
#define LED2_Pin GPIO_PIN_4
#define LED2_GPIO_Port GPIOD
#define LED1_Pin GPIO_PIN_5
#define LED1_GPIO_Port GPIOD
#define DRV_MOSI_Pin GPIO_PIN_7
#define DRV_MOSI_GPIO_Port GPIOD
#define DRV_MISO_Pin GPIO_PIN_9
#define DRV_MISO_GPIO_Port GPIOG
#define DRV_NSS_Pin GPIO_PIN_10
#define DRV_NSS_GPIO_Port GPIOG
#define LTDC_STNDB_Pin GPIO_PIN_5
#define LTDC_STNDB_GPIO_Port GPIOB
#define A_PED_ENC_Pin GPIO_PIN_7
#define A_PED_ENC_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_5
#define BUZZER_GPIO_Port GPIOI
#define VIB_Pin GPIO_PIN_6
#define VIB_GPIO_Port GPIOI
#define TEST_Pin GPIO_PIN_7
#define TEST_GPIO_Port GPIOI

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
