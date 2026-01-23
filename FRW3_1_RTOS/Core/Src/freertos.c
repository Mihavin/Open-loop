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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "motor_foc.h"
#include "telemetry.h"
#include "leds.h"
#include "buzzer.h"
#include "drv_spi.h"
#include "app_touchgfx.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern TIM_HandleTypeDef htim3;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

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

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
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

   // bool armed = false;

    for (;;)
    {
        MotorFOC();      // runs alignment timing + speed loop
        osDelay(1);
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
  	  float sp = (float)__HAL_TIM_GET_COUNTER(&htim3);
  	  MotorFOC_SetSpeedRPM(sp);
  	MotorFOC_SetMode(MOTOR_FOC_MODE_SPEED);
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

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

