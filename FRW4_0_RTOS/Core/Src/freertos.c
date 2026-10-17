/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "motor_foc.h"
#include "telemetry.h"
#include "leds.h"
#include "buzzer.h"
#include "drv_spi.h"
#include "app_touchgfx.h"
#include "main.h"
#include "tmc2130.h"
#include "tmc2130_motion.h"
#include "fault_manager.h"
#include "spi.h"
#include "tim.h"
#include "ui_api.h"
#include "operator_input.h"
#include "ui_input.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim5;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// bitmask events
#define EVT_PEDAL_HOME (1u << 0)
#define EVT_FAN	    (1u << 3)
#define EVT_ENC_I	(1u << 5)

#ifndef ENABLE_DEBUG_TELEMETRY
#define ENABLE_DEBUG_TELEMETRY 0
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
float ped_enc = 0;
TMC2130_Handle_t g_tmc2130;
TMC2130Motion_Handle_t g_stepper_motion;
/* USER CODE END Variables */
/* Definitions for MotorTask */
osThreadId_t MotorTaskHandle;
const osThreadAttr_t MotorTask_attributes = {
  .name = "MotorTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityRealtime7,
};
/* Definitions for GUITask */
osThreadId_t GUITaskHandle;
const osThreadAttr_t GUITask_attributes = {
  .name = "GUITask",
  .stack_size = 6144 * 4,
  .priority = (osPriority_t) osPriorityNormal7,
};
/* Definitions for HIDTask */
osThreadId_t HIDTaskHandle;
const osThreadAttr_t HIDTask_attributes = {
  .name = "HIDTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal7,
};
/* Definitions for CommTask */
osThreadId_t CommTaskHandle;
const osThreadAttr_t CommTask_attributes = {
  .name = "CommTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal7,
};
/* Definitions for StorageTask */
osThreadId_t StorageTaskHandle;
const osThreadAttr_t StorageTask_attributes = {
  .name = "StorageTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal7,
};
/* Definitions for EXTITask */
osThreadId_t EXTITaskHandle;
const osThreadAttr_t EXTITask_attributes = {
  .name = "EXTITask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal4,
};
/* Definitions for StepperTask */
osThreadId_t StepperTaskHandle;
const osThreadAttr_t StepperTask_attributes = {
  .name = "StepperTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal6,
};
/* Definitions for HIDQ */
osMessageQueueId_t HIDQHandle;
const osMessageQueueAttr_t HIDQ_attributes = {
  .name = "HIDQ"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* USER CODE END FunctionPrototypes */

void StartMotorTask(void *argument);
void StartGUITask(void *argument);
void StartHIDTask(void *argument);
void StartCommTask(void *argument);
void StartStorageTask(void *argument);
void StartEXTITask(void *argument);
void StartStepperTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    Fault_Report(FAULT_CPU);
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
    Fault_Report(FAULT_CPU);
    taskDISABLE_INTERRUPTS();
    for (;;) {}
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of HIDQ */
  HIDQHandle = osMessageQueueNew (8, sizeof(uint16_t), &HIDQ_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  UI_InitCommandQueues();
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of MotorTask */
  MotorTaskHandle = osThreadNew(StartMotorTask, NULL, &MotorTask_attributes);

  /* creation of GUITask */
  GUITaskHandle = osThreadNew(StartGUITask, NULL, &GUITask_attributes);

  /* creation of HIDTask */
  HIDTaskHandle = osThreadNew(StartHIDTask, NULL, &HIDTask_attributes);

  /* creation of CommTask */
  CommTaskHandle = osThreadNew(StartCommTask, NULL, &CommTask_attributes);

  /* creation of StorageTask */
  StorageTaskHandle = osThreadNew(StartStorageTask, NULL, &StorageTask_attributes);

  /* creation of EXTITask */
  EXTITaskHandle = osThreadNew(StartEXTITask, NULL, &EXTITask_attributes);

  /* creation of StepperTask */
  StepperTaskHandle = osThreadNew(StartStepperTask, NULL, &StepperTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  configASSERT(MotorTaskHandle != NULL);
  configASSERT(GUITaskHandle != NULL);
  configASSERT(HIDTaskHandle != NULL);
  configASSERT(CommTaskHandle != NULL);
  configASSERT(StorageTaskHandle != NULL);
  configASSERT(EXTITaskHandle != NULL);
  configASSERT(StepperTaskHandle != NULL);
  configASSERT(HIDQHandle != NULL);
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartMotorTask */
/**
  * @brief  Function implementing the MotorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartMotorTask */
void StartMotorTask(void *argument)
{
  /* USER CODE BEGIN StartMotorTask */
  /* Infinite loop */

    MotorFOC_Init();
    MotorFOC_Start();
    Fault_WatchdogStart();
    TickType_t last_wake = xTaskGetTickCount();
   // bool armed = false;

    for (;;)
    {
        Fault_Heartbeat(FAULT_HEALTH_MOTOR);
        Fault_WatchdogPoll();
        (void)Fault_ServiceClearRequest();
        MotorFOC();
        if (MotorFOC_GetState() == MOTOR_FOC_STATE_FAULT)
        {
            Fault_Report(FAULT_BLDC_DRIVER);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1));
    }
  /* USER CODE END StartMotorTask */
}

/* USER CODE BEGIN Header_StartGUITask */
/**
* @brief Function implementing the GUITask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartGUITask */
void StartGUITask(void *argument)
{
  /* USER CODE BEGIN StartGUITask */
  /* Infinite loop */
	__HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, 25);
	 MX_TouchGFX_Process();
  for(;;)
  {
#if ENABLE_DEBUG_TELEMETRY
	  Telemetry_Update();
#endif
    osDelay(20);
  }
  /* USER CODE END StartGUITask */
}

static void hid_tone(uint32_t hz)
{
  if (hz) (void)Buzzer_ToneHz(&g_buzzer, hz, 50u);
  else (void)Buzzer_Stop(&g_buzzer);
}

static void hid_vibration(bool on)
{
  HAL_GPIO_WritePin(VIB_GPIO_Port, VIB_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* USER CODE BEGIN Header_StartHIDTask */
/**
* @brief Function implementing the HIDTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartHIDTask */
void StartHIDTask(void *argument)
{
  /* USER CODE BEGIN StartHIDTask */
  (void)argument;
  /* Operator input is decoded here and consumed by the TouchGFX model
     (ui_input.h). Setpoints, arming and mode changes are owned by the UI. */
  uint32_t previous = __HAL_TIM_GET_COUNTER(&htim3);
  UI_InputInit();
  UI_FeedbackBind(hid_tone, hid_vibration);
  for (;;)
  {
    Fault_Heartbeat(FAULT_HEALTH_HID);
    uint32_t now = HAL_GetTick();
    uint32_t count = __HAL_TIM_GET_COUNTER(&htim3);
    int32_t delta = Operator_EncoderDelta(count, previous, __HAL_TIM_GET_AUTORELOAD(&htim3) + 1u);
    previous = count;
    uint32_t pressed = 0;
    /* Knob, SIDE and BACK are active low; FRW and REV are active high. */
    if (HAL_GPIO_ReadPin(UI_ENC_BUTTON_GPIO_Port, UI_ENC_BUTTON_Pin) == GPIO_PIN_RESET) pressed |= 1u << UI_KEY_KNOB;
    if (HAL_GPIO_ReadPin(SIDE_BUTTON_GPIO_Port, SIDE_BUTTON_Pin) == GPIO_PIN_RESET) pressed |= 1u << UI_KEY_SIDE;
    if (HAL_GPIO_ReadPin(BACK_BUTTON_GPIO_Port, BACK_BUTTON_Pin) == GPIO_PIN_RESET) pressed |= 1u << UI_KEY_BACK;
    if (HAL_GPIO_ReadPin(FRW_BUT_GPIO_Port, FRW_BUT_Pin) == GPIO_PIN_SET) pressed |= 1u << UI_KEY_FRW;
    if (HAL_GPIO_ReadPin(REV_BUT_GPIO_Port, REV_BUT_Pin) == GPIO_PIN_SET) pressed |= 1u << UI_KEY_REV;
    UI_InputPoll(now, delta, pressed);
    UI_PedalSetCounts((int16_t)__HAL_TIM_GET_COUNTER(&htim4));
    UI_FeedbackTick(now);
    osDelay(10);
  }
  /* USER CODE END StartHIDTask */
}

/* USER CODE BEGIN Header_StartCommTask */
/**
* @brief Function implementing the CommTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCommTask */
void StartCommTask(void *argument)
{
  /* USER CODE BEGIN StartCommTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCommTask */
}

/* USER CODE BEGIN Header_StartStorageTask */
/**
* @brief Function implementing the StorageTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartStorageTask */
void StartStorageTask(void *argument)
{
  /* USER CODE BEGIN StartStorageTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartStorageTask */
}

/* USER CODE BEGIN Header_StartEXTITask */
/**
* @brief Function implementing the EXTITask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartEXTITask */
void StartEXTITask(void *argument)
{
  /* USER CODE BEGIN StartEXTITask */

    uint32_t events;

    // per-input debounce timestamps
    TickType_t t_limit = 0, t_fan = 0, t_enc_I = 0;
    const TickType_t db = pdMS_TO_TICKS(20); // 20ms debounce (buttons/switches)

  /* Infinite loop */
  for(;;)
  {
      // wait until ISR notifies us
      xTaskNotifyWait(0, 0xFFFFFFFFu, &events, portMAX_DELAY);

      TickType_t now = xTaskGetTickCount();

      if ((events & EVT_PEDAL_HOME) && (now - t_limit) > db)
      {
          t_limit = now;
          /* Pedal home switch: re-zero the pedal encoder. Not a fault. */
          __HAL_TIM_SET_COUNTER(&htim4, 0);
          UI_PedalMarkHomed();

      }

      /* SIDE, BACK, FRW, REV and the knob button are polled by the HID task
         (debounce + hold timing live in ui_input.c). */

      if ((events & EVT_FAN) && (now - t_fan) > db)
      {
          t_fan = now;
          /* This net is the PWM command; no rotation feedback exists. */
      }

      if ((events & EVT_ENC_I) && (now - t_enc_I) > db)
      {
          t_enc_I = now;
          // Handle encoder index pulse
      }

    osDelay(50);
  }
  /* USER CODE END StartEXTITask */
}

/* USER CODE BEGIN Header_StartStepperTask */
/**
* @brief Function implementing the StepperTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartStepperTask */
void StartStepperTask(void *argument)
{
  /* USER CODE BEGIN StartStepperTask */
    (void)argument;

    TMC2130_Config_t cfg;
    TMC2130_ConfigDefaults(&cfg);
    cfg.microsteps = 16;
    cfg.interpolate_256 = true;
    cfg.ihold = 8;
    cfg.irun = 16;
    cfg.ihold_delay = 6;

    if (TMC2130_Init(&g_tmc2130, &hspi3, &htim12, TIM_CHANNEL_2, &cfg) != HAL_OK)
    {
        Fault_Report(FAULT_STEPPER_DRIVER | FAULT_SPI);
    }

    TMC2130Motion_Limits_t limits;
    TMC2130Motion_DefaultLimits(&limits);

    if (TMC2130Motion_Init(&g_stepper_motion, &g_tmc2130, &limits) != HAL_OK)
    {
        Fault_Report(FAULT_STEPPER_DRIVER);
    }

    (void)TMC2130Motion_Enable(&g_stepper_motion, false);

	TickType_t last = xTaskGetTickCount();
  /* Infinite loop */
  for(;;)
  {
      static TickType_t previous = 0;
      TickType_t now = xTaskGetTickCount();
      float dt = previous ? (float)(now - previous) / configTICK_RATE_HZ : 0.001f;
      previous = now;
      Fault_Heartbeat(FAULT_HEALTH_STEPPER);
      UI_ProcessStepperCommands();
      static TickType_t status_tick = 0;
      if ((TickType_t)(now - status_tick) >= pdMS_TO_TICKS(50))
      {
          uint32_t status, gstat;
          status_tick = now;
          if (TMC2130_ReadDRVStatus(&g_tmc2130, &status) != HAL_OK ||
              TMC2130_ReadGSTAT(&g_tmc2130, &gstat) != HAL_OK)
              Fault_Report(FAULT_STEPPER_DRIVER | FAULT_SPI);
          else if ((status & ((1u << 25) | (1u << 27) | (1u << 28))) || (gstat & 7u))
              Fault_Report(FAULT_STEPPER_DRIVER); /* overtemperature, short or reset/UV */
      }
      TMC2130Motion_Task(&g_stepper_motion, dt);
      UI_PublishStepperSnapshot();
      vTaskDelayUntil(&last, pdMS_TO_TICKS(1));
  }
  /* USER CODE END StartStepperTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    BaseType_t hpw = pdFALSE;
    uint32_t bits = 0;

    if (GPIO_Pin == LIMIT_BUTTON_Pin) bits |= EVT_PEDAL_HOME; /* pedal home switch: zero TIM4 */

    /* Buttons are polled by the HID task; their EXTI edges only wake the MCU. */

    /* FAN_FAULT is actually the MAX31740 PWM output; never report it as a fault. */

    if (GPIO_Pin == I_ENC_Pin) bits |= EVT_ENC_I;

    if (bits && EXTITaskHandle)
    {
        xTaskNotifyFromISR(EXTITaskHandle, bits, eSetBits, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
}
/* USER CODE END Application */

