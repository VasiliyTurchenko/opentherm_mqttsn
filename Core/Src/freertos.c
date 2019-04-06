/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2019 STMicroelectronics International N.V.
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include "lan_poll_task.h"
#include "manchester_task.h"
#include "logger_task.h"
#include "service_task.h"
#include "publish_task.h"
#include "subscribe_task.h"

#include "string.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define	STACK_FILLER	0xC0DAFADEU

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */


/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

osMutexId FS_Mutex01Handle;
osStaticMutexDef_t FS_Mutex01_ControlBlock;

osMutexId CRC_MutexHandle;
osStaticMutexDef_t CRC_Mutex_ControlBlock;

osMutexId ManchesterTimer01MutexHandle;
osStaticMutexDef_t ManchesterTimer01Mutex_ControlBlock;

#ifdef DEBUG
static volatile size_t LANPollTaskBuffer_depth;
static volatile size_t PublishTaskBuffer_depth;
static volatile size_t DiagPrintTaskBuffer_depth;
static volatile size_t ProcSPSTaskBuffer_depth;
static volatile size_t SubscribeTaskBuffer_depth;
static volatile size_t ServiceTaskBuffer_depth;
static volatile size_t ManchTaskBuffer_depth;
#endif


/* USER CODE END Variables */
osThreadId LANPollTaskHandle;
uint32_t LANPollTaskBuffer[128];
osStaticThreadDef_t LANPollTaskControlBlock;
osThreadId PublishTaskHandle;
uint32_t PublishTaskBuffer[256];
osStaticThreadDef_t PublishTaskControlBlock;
osThreadId DiagPrTaskHandle;
uint32_t DiagPrintTaskBuffer[200];
osStaticThreadDef_t DiagPrintTaskControlBlock;
osThreadId ProcSPSTaskHandle;
uint32_t ProcSPSTaskBuffer[128];
osStaticThreadDef_t ProcSPSTaskControlBlock;
osThreadId SubscrbTaskHandle;
uint32_t SubscribeTaskBuffer[256];
osStaticThreadDef_t SubscribeTaskControlBlock;
osThreadId ServiceTaskHandle;
uint32_t ServiceTaskBuffer[168];
osStaticThreadDef_t ServiceTaskControlBlock;
osThreadId ManchTaskHandle;
uint32_t ManchTaskBuffer[128];
osStaticThreadDef_t ManchTaskControlBlock;
osMutexId ETH_Mutex01Handle;
osStaticMutexDef_t ETH_Mutex01_ControlBlock;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void Start_LANPollTask(void const *argument);
void Start_PublishTask(void const *argument);
void Start_DiagPrintTask(void const *argument);
void Start_ProcSPSTask(void const *argument);
void Start_SubscribeTask(void const *argument);
void Start_ServiceTask(void const *argument);
void Start_ManchTask(void const *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
				   StackType_t **ppxIdleTaskStackBuffer,
				   uint32_t *pulIdleTaskStackSize);

/* Hook prototypes */
void vApplicationIdleHook(void);
void vApplicationTickHook(void);

/* USER CODE BEGIN 2 */
__weak void vApplicationIdleHook(void)
{
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
   task. It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()). If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */

#ifdef DEBUG
	uint32_t * p;
	size_t	i;

	p = LANPollTaskBuffer;
	i = 0U;
	while ((i < 128U) && (*p == STACK_FILLER)) {
		p++;
		i++;
	}
	LANPollTaskBuffer_depth = i;

	p = PublishTaskBuffer;
	i = 0U;
	while ((i < 256U) && (*p == STACK_FILLER)) {
		p++;
		i++;
	}
	PublishTaskBuffer_depth = i;

	p = DiagPrintTaskBuffer;
	i = 0U;
	while ((i < 200U) && (*p == STACK_FILLER)) {
		p++;
		i++;
	}
	DiagPrintTaskBuffer_depth = i;

	p = ProcSPSTaskBuffer;
	i = 0U;
	while ((i < 128U) && (*p == STACK_FILLER)) {
		p++;
		i++;
	}
	ProcSPSTaskBuffer_depth = i;

	p = SubscribeTaskBuffer;
	i = 0U;
	while ((i < 256U) && (*p == STACK_FILLER)) {
		p++;
		i++;
	}
	SubscribeTaskBuffer_depth = i;


	p = ServiceTaskBuffer;
	i = 0U;
	while ((i < 168U) && (*p == STACK_FILLER)) {
		p++;
		i++;
	}
	ServiceTaskBuffer_depth = i;

	p = ManchTaskBuffer;
	i = 0U;
	while ((i < 128U) && (*p == STACK_FILLER)) {
		p++;
		i++;
	}
	ManchTaskBuffer_depth = i;

#endif






}
/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
__weak void vApplicationTickHook(void)
{
	/* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the tick hook is called from an interrupt context, so
   code must not attempt to block, and only the interrupt safe FreeRTOS API
   functions can be used (those that end in FromISR()). */
}
/* USER CODE END 3 */

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
				   StackType_t **ppxIdleTaskStackBuffer,
				   uint32_t *pulIdleTaskStackSize)
{
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
	*ppxIdleTaskStackBuffer = &xIdleStack[0];
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
	/* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void)
{
	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Create the mutex(es) */
	/* definition and creation of ETH_Mutex01 */
	osMutexStaticDef(ETH_Mutex01, &ETH_Mutex01_ControlBlock);
	ETH_Mutex01Handle = osMutexCreate(osMutex(ETH_Mutex01));

	/* USER CODE BEGIN RTOS_MUTEX */

	/* access mutex to the single simultaneous file  tiny_fs */
	osMutexStaticDef(FS_Mutex01, &FS_Mutex01_ControlBlock);
	FS_Mutex01Handle = osMutexCreate(osMutex(FS_Mutex01));

	osMutexStaticDef(CRC_Mutex, &CRC_Mutex_ControlBlock);
	CRC_MutexHandle = osMutexCreate(osMutex(CRC_Mutex));

	osMutexStaticDef(ManchesterTimer01Mutex, &ManchesterTimer01Mutex_ControlBlock);
	CRC_MutexHandle = osMutexCreate(osMutex(ManchesterTimer01Mutex));

	/* USER CODE END RTOS_MUTEX */

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* Create the thread(s) */
	/* definition and creation of LANPollTask */
	osThreadStaticDef(LANPollTask, Start_LANPollTask, osPriorityHigh, 0,
			  128, LANPollTaskBuffer, &LANPollTaskControlBlock);
	LANPollTaskHandle = osThreadCreate(osThread(LANPollTask), NULL);

	/* definition and creation of PublishTask */
	osThreadStaticDef(PublishTask, Start_PublishTask, osPriorityNormal, 0,
			  256, PublishTaskBuffer, &PublishTaskControlBlock);
	PublishTaskHandle = osThreadCreate(osThread(PublishTask), NULL);

	/* definition and creation of DiagPrTask */
	osThreadStaticDef(DiagPrTask, Start_DiagPrintTask,
			  osPriorityAboveNormal, 0, 200, DiagPrintTaskBuffer,
			  &DiagPrintTaskControlBlock);
	DiagPrTaskHandle = osThreadCreate(osThread(DiagPrTask), NULL);

	/* definition and creation of ProcSPSTask */
	osThreadStaticDef(ProcSPSTask, Start_ProcSPSTask, osPriorityHigh, 0,
			  128, ProcSPSTaskBuffer, &ProcSPSTaskControlBlock);
	ProcSPSTaskHandle = osThreadCreate(osThread(ProcSPSTask), NULL);

	/* definition and creation of SubscrbTask */
	osThreadStaticDef(SubscrbTask, Start_SubscribeTask, osPriorityNormal, 0,
			  256, SubscribeTaskBuffer, &SubscribeTaskControlBlock);
	SubscrbTaskHandle = osThreadCreate(osThread(SubscrbTask), NULL);

	/* definition and creation of ServiceTask */
	osThreadStaticDef(ServiceTask, Start_ServiceTask, osPriorityNormal, 0,
			  168, ServiceTaskBuffer, &ServiceTaskControlBlock);
	ServiceTaskHandle = osThreadCreate(osThread(ServiceTask), NULL);

	/* definition and creation of ManchTask */
	osThreadStaticDef(ManchTask, Start_ManchTask, osPriorityAboveNormal, 0,
			  128, ManchTaskBuffer, &ManchTaskControlBlock);
	ManchTaskHandle = osThreadCreate(osThread(ManchTask), NULL);

	/* USER CODE BEGIN RTOS_THREADS */
/**
 * @todo ADD STACK DEPTH MONITOR
*/
#ifdef DEBUG
	uint32_t * p;
	size_t	i;

	p = LANPollTaskBuffer;
	i = 120U;
	while (i > 0U) {
		*p = STACK_FILLER;
		p++;
		i--;
	}

	p = PublishTaskBuffer;
	i = 248U;
	while (i > 0U) {
		*p = STACK_FILLER;
		p++;
		i--;
	}

	p = DiagPrintTaskBuffer;
	i = 192U;
	while (i > 0U) {
		*p = STACK_FILLER;
		p++;
		i--;
	}

	p = ProcSPSTaskBuffer;
	i = 120U;
	while (i > 0U) {
		*p = STACK_FILLER;
		p++;
		i--;
	}

	p = SubscribeTaskBuffer;
	i = 248U;
	while (i > 0U) {
		*p = STACK_FILLER;
		p++;
		i--;
	}

	p = ServiceTaskBuffer;
	i = 160U;
	while (i > 0U) {
		*p = STACK_FILLER;
		p++;
		i--;
	}

	p = ManchTaskBuffer;
	i = 120U;
	while (i > 0U) {
		*p = STACK_FILLER;
		p++;
		i--;
	}

#endif

	/* USER CODE END RTOS_THREADS */

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	/* USER CODE END RTOS_QUEUES */
}

/* USER CODE BEGIN Header_Start_LANPollTask */
/**
  * @brief  Function implementing the LANPollTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_Start_LANPollTask */
void Start_LANPollTask(void const *argument)
{
	/* USER CODE BEGIN StartLAN_Poll_Task */
	(void)argument;
	const TickType_t xPeriod = pdMS_TO_TICKS(2U);
	lan_poll_task_init();
	/* infinite loop */
	for (;;) {
		lan_poll_task_run();
		vTaskDelay(xPeriod);
	}
	/* USER CODE END StartLAN_Poll_Task */
}

/* USER CODE BEGIN Header_Start_PublishTask */
/**
* @brief Function implementing the PublishTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Start_PublishTask */
void Start_PublishTask(void const *argument)
{
	/* USER CODE BEGIN Start_PublishTask */
	(void)argument;
	publish_task_init();
	/* Infinite loop */
	for (;;) {
		publish_task_run();
		osDelay(1);
	}
	/* USER CODE END Start_PublishTask */
}

/* USER CODE BEGIN Header_Start_DiagPrintTask */
/**
* @brief Function implementing the DiagPrTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Start_DiagPrintTask */
void Start_DiagPrintTask(void const *argument)
{
	/* USER CODE BEGIN Start_DiagPrintTask */
	(void)argument;
	logger_task_init();
	TickType_t xLastWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS(3);
	xLastWakeTime = xTaskGetTickCount();			/* get value only once! */


	/* Infinite loop */
	for (;;) {
		logger_task_run();
		vTaskDelayUntil(&xLastWakeTime, xPeriod);	/* wake up every 3ms */
	}
	/* USER CODE END Start_DiagPrintTask */
}

/* USER CODE BEGIN Header_Start_ProcSPSTask */
/**
* @brief Function implementing the ProcSPSTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Start_ProcSPSTask */
void Start_ProcSPSTask(void const *argument)
{
	/* USER CODE BEGIN Start_ProcSPSTask */
	(void)argument;

	/* Infinite loop */
	for (;;) {

		osDelay(1);
	}
	/* USER CODE END Start_ProcSPSTask */
}

/* USER CODE BEGIN Header_Start_SubscribeTask */
/**
* @brief Function implementing the SubscrbTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Start_SubscribeTask */
void Start_SubscribeTask(void const *argument)
{
	/* USER CODE BEGIN Start_SubscribeTask */
	(void)argument;
	subscribe_task_init();
	/* Infinite loop */
	for (;;) {
		subscribe_task_run();
		osDelay(1);
	}
	/* USER CODE END Start_SubscribeTask */
}

/* USER CODE BEGIN Header_Start_ServiceTask */
/**
* @brief Function implementing the ServiceTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Start_ServiceTask */
void Start_ServiceTask(void const *argument)
{
	/* USER CODE BEGIN Start_ServiceTask */
	(void)argument;
	service_task_init();
	/* Infinite loop */
	for (;;) {
		service_task_run();
		osDelay(1);
	}
	/* USER CODE END Start_ServiceTask */
}

/* USER CODE BEGIN Header_Start_ManchTask */
/**
* @brief Function implementing the ManchTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Start_ManchTask */
void Start_ManchTask(void const *argument)
{
	/* USER CODE BEGIN Start_ManchTask */

	(void)argument;

	manchester_task_init();

	/* Infinite loop */
	for (;;) {
		manchester_task_run();
		osDelay(1);
	}
	/* USER CODE END Start_ManchTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
