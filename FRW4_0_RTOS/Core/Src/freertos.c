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
#define EVT_LIMIT   (1u << 0)
#define EVT_SIDE    (1u << 1)
#define EVT_BACK    (1u << 2)
#define EVT_FAN	    (1u << 3)
#define EVT_UI		(1u << 4)
#define EVT_ENC_I	(1u << 5)
#define EVT_FRW     (1u << 6)
#define EVT_REV     (1u << 7)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
float ped_enc = 0;
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

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
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

  /* USER CODE BEGIN RTOS_THREADS */
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
    while (MotorFOC_GetState() == MOTOR_FOC_STATE_CALIB)
    {
        osDelay(1);
    }
   // MotorFOC_Arm();
    MotorFOC_SetMode(MOTOR_FOC_MODE_SPEED);
  //  MotorFOC_SetSpeedRPM(1500);
    TickType_t last_wake = xTaskGetTickCount();
   // bool armed = false;

    for (;;)
    {
        MotorFOC();      // runs alignment timing + speed loop
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
	  Telemetry_Update();
    osDelay(20);
  }
  /* USER CODE END StartGUITask */
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
  /* Infinite loop */
  for(;;)
  {


	  //	float ped_enc = (float)__HAL_TIM_GET_COUNTER(&htim4);
	  	ped_enc  = (float) __HAL_TIM_GET_COUNTER(&htim3);
	  /*
	  	if (UI_enc != bight)
	  	{
	  		__HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, UI_enc);
	  		bight = UI_enc;
	  	}
	  */
	  	if (MotorFOC_GetMode() == MOTOR_FOC_MODE_POSITION)
	  	{
	  		MotorFOC_SetPositionRevolutions(ped_enc * 0.1f);
	  	}

	  	if (MotorFOC_GetMode()  == MOTOR_FOC_MODE_SPEED)
	  	{
	  		MotorFOC_SetSpeedRPM(ped_enc);
	  	}

	  	if (MotorFOC_GetMode()  == MOTOR_FOC_MODE_TORQUE)
	  	{
	  		MotorFOC_SetTorqueCurrentA(ped_enc * 0.01f);
	  	}


    osDelay(100);
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
	uint32_t ui_button_click_count = 0;
    uint32_t events;

    // per-input debounce timestamps
    TickType_t t_limit = 0, t_side = 0, t_back = 0, t_fan = 0, t_ui = 0, t_enc_I = 0, t_frw = 0, t_rev = 0;
    const TickType_t db = pdMS_TO_TICKS(20); // 20ms debounce (buttons/switches)

  /* Infinite loop */
  for(;;)
  {
      // wait until ISR notifies us
      xTaskNotifyWait(0, 0xFFFFFFFFu, &events, portMAX_DELAY);

      TickType_t now = xTaskGetTickCount();

      if ((events & EVT_LIMIT) && (now - t_limit) > db)
      {
          t_limit = now;
          __HAL_TIM_SET_COUNTER(&htim4, 0); //reset pedal counter

      }

      if ((events & EVT_SIDE) && (now - t_side) > db)
      {
          t_side = now;
          // Handle side switch
      }

      if ((events & EVT_BACK) && (now - t_back) > db)
      {
          t_back = now;
          // Handle back switch
      }


      if ((events & EVT_FAN) && (now - t_fan) > db)
      {
          t_fan = now;
          // Handle fan failure
      }

      if ((events & EVT_UI) && (now - t_ui) > db)
      {
          t_ui = now;
          ped_enc = 0 ;
          while(MotorFOC_GetSpeedRPM() > 10){}
          ui_button_click_count++;
          if (ui_button_click_count > 2) {ui_button_click_count = 0;}
          MotorFOC_SetMode(ui_button_click_count); //toggle modes
          __HAL_TIM_SET_COUNTER(&htim3, 0);

          MotorFOC_RequestDisarm();
          MotorFOC_RequestZeroEncoder();
          MotorFOC_RequestArm();
      }

      if ((events & EVT_ENC_I) && (now - t_enc_I) > db)
      {
          t_enc_I = now;
          // Handle encoder index pulse
      }

      if ((events & EVT_FRW) && (now - t_frw) > db)
      {
          t_frw = now;
          // Handle forward button
      }

      if ((events & EVT_REV) && (now - t_rev) > db)
      {
          t_rev = now;
          // Handle reverse button
      }

    osDelay(50);
  }
  /* USER CODE END StartEXTITask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    BaseType_t hpw = pdFALSE;
    uint32_t bits = 0;

    if (GPIO_Pin == LIMIT_BUTTON_Pin) bits |= EVT_LIMIT;

    if (GPIO_Pin == SIDE_BUTTON_Pin)  bits |= EVT_SIDE;

    if (GPIO_Pin == BACK_BUTTON_Pin)  bits |= EVT_BACK;

    if (GPIO_Pin == FAN_FAULT_Pin)  bits |= EVT_FAN;

    if (GPIO_Pin == UI_ENC_BUTTON_Pin) bits |= EVT_UI;

    if (GPIO_Pin == I_ENC_Pin) bits |= EVT_ENC_I;

    if (GPIO_Pin == FRW_BUT_Pin) bits |= EVT_FRW;

    if (GPIO_Pin == REV_BUT_Pin) bits |= EVT_REV;

    if (bits && EXTITaskHandle)
    {
        xTaskNotifyFromISR(EXTITaskHandle, bits, eSetBits, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
}
/* USER CODE END Application */

