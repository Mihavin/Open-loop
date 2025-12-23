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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TEST_Pin GPIO_PIN_3
#define TEST_GPIO_Port GPIOE
#define TEST_EXTI_IRQn EXTI3_IRQn
#define CAL_Pin GPIO_PIN_13
#define CAL_GPIO_Port GPIOC
#define EN_DRV_Pin GPIO_PIN_14
#define EN_DRV_GPIO_Port GPIOC
#define LED0_Pin GPIO_PIN_15
#define LED0_GPIO_Port GPIOC
#define SOA_Pin GPIO_PIN_10
#define SOA_GPIO_Port GPIOF
#define BUZZER_Pin GPIO_PIN_0
#define BUZZER_GPIO_Port GPIOH
#define I_Pin GPIO_PIN_1
#define I_GPIO_Port GPIOH
#define I_EXTI_IRQn EXTI1_IRQn
#define A_Pin GPIO_PIN_0
#define A_GPIO_Port GPIOA
#define B_Pin GPIO_PIN_1
#define B_GPIO_Port GPIOA
#define SOC_Pin GPIO_PIN_7
#define SOC_GPIO_Port GPIOA
#define SOB_Pin GPIO_PIN_5
#define SOB_GPIO_Port GPIOC
#define INLB_Pin GPIO_PIN_0
#define INLB_GPIO_Port GPIOB
#define INHA_Pin GPIO_PIN_9
#define INHA_GPIO_Port GPIOE
#define INLC_Pin GPIO_PIN_12
#define INLC_GPIO_Port GPIOE
#define NFAULT_Pin GPIO_PIN_12
#define NFAULT_GPIO_Port GPIOB
#define INLA_Pin GPIO_PIN_13
#define INLA_GPIO_Port GPIOB
#define SW_Pin GPIO_PIN_11
#define SW_GPIO_Port GPIOD
#define SW_EXTI_IRQn EXTI15_10_IRQn
#define E2_Pin GPIO_PIN_12
#define E2_GPIO_Port GPIOD
#define E1_Pin GPIO_PIN_13
#define E1_GPIO_Port GPIOD
#define LED_KLV_Pin GPIO_PIN_2
#define LED_KLV_GPIO_Port GPIOG
#define LED2_Pin GPIO_PIN_8
#define LED2_GPIO_Port GPIOC
#define INHB_Pin GPIO_PIN_9
#define INHB_GPIO_Port GPIOA
#define INHC_Pin GPIO_PIN_10
#define INHC_GPIO_Port GPIOA
#define NFAULT_FAN_Pin GPIO_PIN_3
#define NFAULT_FAN_GPIO_Port GPIOD
#define NSS2_Pin GPIO_PIN_4
#define NSS2_GPIO_Port GPIOD
#define NSS1_Pin GPIO_PIN_5
#define NSS1_GPIO_Port GPIOD
#define NSS_Pin GPIO_PIN_10
#define NSS_GPIO_Port GPIOG
#define LED1_Pin GPIO_PIN_14
#define LED1_GPIO_Port GPIOG
#define E1B4_Pin GPIO_PIN_4
#define E1B4_GPIO_Port GPIOB
#define E2B5_Pin GPIO_PIN_5
#define E2B5_GPIO_Port GPIOB
#define SWB6_Pin GPIO_PIN_6
#define SWB6_GPIO_Port GPIOB
#define SWB6_EXTI_IRQn EXTI9_5_IRQn
#define SW1_Pin GPIO_PIN_7
#define SW1_GPIO_Port GPIOB
#define SW1_EXTI_IRQn EXTI9_5_IRQn
#define SW2_Pin GPIO_PIN_9
#define SW2_GPIO_Port GPIOB
#define SW2_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
