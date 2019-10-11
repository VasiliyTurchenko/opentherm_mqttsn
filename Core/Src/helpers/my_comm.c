/**
  ******************************************************************************
  * @file    my_comm.c
  * @author  Vasiliy Turchenko
  * @brief   my_comm.c
  * @date    30-12-2017
  * @modified 20-Jan-2019
  * @modified 11-Jul-2019
  * @verbatim
  ==============================================================================
		  ##### How to use this driver #####

  ==============================================================================
  * @endverbatim
  */

#include "cmsis_os.h"

#include "my_comm.h"
#include "usart.h"
#include "lan.h"

uint8_t TxBuf1[BUFSIZE];
uint8_t TxBuf2[BUFSIZE];
volatile void *pActTxBuf;
volatile void *pXmitTxBuf;
volatile size_t TxTail;

//volatile uint8_t ActiveTxBuf;

volatile uint8_t XmitState;
volatile uint8_t ActBufState;
volatile bool TransmitFuncRunning;

volatile ErrorStatus XmitError = SUCCESS;

volatile size_t MaxTail = 0U;

/* Tx buffers access mutex */
/* defined in freertos.c */
extern osMutexId xfunc_outMutexHandle;

extern osThreadId DiagPrTaskHandle;

/******************************** initialisation functions ********************/

/**
  * @brief InitComm initializes buffers and pointers
  * @note Non-RTOS function
  * @param  none
  * @retval none
  */
ErrorStatus InitComm(void)
{
	ErrorStatus result;

	for (size_t i = 0U; i < BUFSIZE; i++) {
		TxBuf1[i] = 0U;
		TxBuf2[i] = 0U;
	}
	TxTail = 0U;
	pActTxBuf = &TxBuf1;
	pXmitTxBuf = &TxBuf2;
	ActBufState = STATE_UNLOCKED;
	XmitState = STATE_UNLOCKED;
	result = SUCCESS;
	return result;
}
/* end of the function  InitComm */

/******************************** xfunc_out functions ********************/

/**
 * @brief myxfunc_out_dummy does nothing
  * @note   helper for xprintf
  * @param  char
  * @retval none
  */
void myxfunc_out_dummy(unsigned char c)
{
	(void)c;
}

/**
 * @brief myxfunc_out_no_RTOS puts the char to the xmit buffer
  * @note   helper for xprintf
  * @param  char
  * @retval none
  */
void myxfunc_out_no_RTOS(unsigned char c)
{
	if (ActBufState == STATE_UNLOCKED) {
		ActBufState = STATE_LOCKED;
		MaxTail = (TxTail > MaxTail) ? TxTail : MaxTail;
		if (TxTail < BUFSIZE) {
			*((uint8_t *)pActTxBuf + TxTail) = c;
			TxTail++;
		}
		ActBufState = STATE_UNLOCKED;
	}
}

/**
  * @brief  myxfunc_out_RTOS puts the char to the xmit buffer
  * @note   helper for xprintf
  * @param  char
  * @retval none
  */
void myxfunc_out_RTOS(unsigned char c)
{
	while (osMutexWait(xfunc_outMutexHandle, pdMS_TO_TICKS(1U)) != osOK) {
		//		taskYIELD();
//		vTaskDelay(pdMS_TO_TICKS(1U));
	}
	if (TxTail < BUFSIZE) {
		MaxTail = (TxTail > MaxTail) ? TxTail : MaxTail;
		*((uint8_t *)pActTxBuf + TxTail) = c;
		TxTail++;
	}
	osMutexRelease(xfunc_outMutexHandle);
}
/* end of the function myxfunc_out_RTOS */

/******************************** transmit functions ********************/

/**
  * @brief  Transmit invokes transmit procedure
  * @param  ptr is a pointer to udp socket
  * @note   if ther is no sockets, ptr must be NULL
  * @note   non-RTOS function
  * @retval ERROR or SUCCESS
  */
ErrorStatus Transmit(const void *ptr)
{
	ErrorStatus result = SUCCESS;

	TransmitFuncRunning = true;

	static HAL_StatusTypeDef XmitStatus;

	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		taskENTER_CRITICAL();
	}

	if ((XmitState != STATE_UNLOCKED) || (ActBufState == STATE_LOCKED) ||
	    (TxTail == (size_t)0U)) {
		/* usart didn't transmit yet OR outfunc is runnung OR  active buffer is empty */
		if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
			taskEXIT_CRITICAL();
		}
		goto fExit;
	}
	// there is no ongoing transmission
	XmitState = STATE_LOCKED;   // set lock
	ActBufState = STATE_LOCKED; // switch active buffer

	// there is something to transmit
	size_t tmptail;
	tmptail = TxTail;
	if (pActTxBuf == TxBuf1) {
		pActTxBuf = TxBuf2;
		TxTail = 0U; // RESET index
		pXmitTxBuf = TxBuf1;
	} else {
		if (pActTxBuf == TxBuf2) {
			pActTxBuf = TxBuf1;
			TxTail = 0U; // RESET index
			pXmitTxBuf = TxBuf2;
		} else {
			ActBufState = STATE_UNLOCKED; // wrong act buf pointer!!
			XmitState = STATE_UNLOCKED;
			if (xTaskGetSchedulerState() !=
			    taskSCHEDULER_NOT_STARTED) {
				taskEXIT_CRITICAL();
			}
			goto fExit;
		}
	}

	// here all the conditions are OK. let's send!
	ActBufState = STATE_UNLOCKED;
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		taskEXIT_CRITICAL();
	}

	if (ptr == NULL) {
		XmitStatus = HAL_UART_Transmit_DMA(
			&huart3, (uint8_t *)pXmitTxBuf, (uint16_t)tmptail);
		result = (XmitStatus == HAL_ERROR) ? ERROR : SUCCESS;
	} else {
		result = write_socket((socket_p)ptr, (uint8_t*)pXmitTxBuf,
				      (int32_t)tmptail);

		XmitState = STATE_UNLOCKED;
	}
fExit:
	TransmitFuncRunning = false;
	return result;
}
/* end of the function Transmit() */

/**
  * @brief  Transmit_RTOS invokes transmit procedure
  * @param  ptr is a pointer to udp socket
  * @note   if ther is no sockets, ptr must be NULL
  * @retval ERROR or SUCCESS
  */
ErrorStatus Transmit_RTOS(const void *ptr)
{
	ErrorStatus result = SUCCESS;
	static HAL_StatusTypeDef XmitStatus;

	if (TxTail == (size_t)0U) {
		goto fExit; /* nothing to do */
	}

	/* temporary raise task priority */
	UBaseType_t task_prio;
	task_prio = uxTaskPriorityGet(DiagPrTaskHandle);
	/* set the new maximal priority */
	vTaskPrioritySet(DiagPrTaskHandle,
			 ((UBaseType_t)configMAX_PRIORITIES - (UBaseType_t)1U));

	/* take the first mutex */
	if (osMutexWait(xfunc_outMutexHandle, 0) != osOK) {
		vTaskPrioritySet(DiagPrTaskHandle, task_prio);
		goto fExit; /* try next time*/
	}

	size_t tmptail;
	tmptail = TxTail;

	if (pActTxBuf == TxBuf1) {
		pActTxBuf = TxBuf2;
		TxTail = 0U;
		pXmitTxBuf = TxBuf1;

		osMutexRelease(xfunc_outMutexHandle);

	} else if (pActTxBuf == TxBuf2) {
		pActTxBuf = TxBuf1;
		TxTail = 0U;
		pXmitTxBuf = TxBuf2;

		osMutexRelease(xfunc_outMutexHandle);

	} else {
		/* error */
		osMutexRelease(xfunc_outMutexHandle);
		vTaskPrioritySet(DiagPrTaskHandle, task_prio);
		goto fExit;
	}

	/* restore usual priority */
	vTaskPrioritySet(DiagPrTaskHandle, task_prio);

	/* here all the conditions are OK. let's send! */
	if (ptr != NULL) {
		result = write_socket((socket_p)ptr,
					pXmitTxBuf,
					(int32_t)tmptail);

		XmitState = STATE_UNLOCKED;
	} else {
		/* transmit over USART */
		XmitStatus = HAL_UART_Transmit_DMA(
			&huart3, (uint8_t *)pXmitTxBuf, (uint16_t)tmptail);
		result = (XmitStatus == HAL_ERROR) ? ERROR : SUCCESS;

		/* we have to wait a notification INSTEAD ! */
		static uint32_t notified_val = 0U;
		if (xTaskNotifyWait(ULONG_MAX, ULONG_MAX, &notified_val,
				    portMAX_DELAY) == pdTRUE) {
			if (notified_val != 1U) {
				// error!!!
			}
		}
	}
fExit:
	return result;
}
/* end of the function Transmit_RTOS() */

/**
 * @brief HAL_UART_TxCpltCallback
 * @param huart
 */
void my_comm_HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == &huart3) {
		/* transmit completed! */
		XmitState = STATE_UNLOCKED;

		/* usart1 IRQ priority must be lower than MAX_SYSCALL_...._PRIORITY */
		BaseType_t xHigherPriorityTaskWoken;

		if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
			if (xTaskNotifyFromISR(DiagPrTaskHandle, 1U,
					       eSetValueWithOverwrite,
					       &xHigherPriorityTaskWoken) !=
			    pdPASS) {
				// error
			}
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
}
/* end of HAL_UART_TxCpltCallback() */

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	static uint32_t ec = 0U;
	ec = huart->ErrorCode;

	UNUSED(ec);
}

/* ############################### end of file ############################### */
