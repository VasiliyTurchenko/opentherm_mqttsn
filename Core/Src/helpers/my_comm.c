/**
  ******************************************************************************
  * @file    my_comm.c
  * @author  Vasiliy Turchenko
  * @brief   my_comm.c
  * @date    30-12-2017
  * @modified 20-Jan-2019
  * @verbatim
  ==============================================================================
		  ##### How to use this driver #####

  ==============================================================================
  * @endverbatim
  */

#include "FreeRTOS.h"
#include "task.h"

#include "my_comm.h"
#include "usart.h"
#include "lan.h"

uint8_t TxBuf1[BUFSIZE];
uint8_t TxBuf2[BUFSIZE];
void *pActTxBuf;
void *pXmitTxBuf;

#ifdef USE_RX
uint8_t RxBuf[BUFSIZE];
void *pRxBuf;
#endif

size_t TxTail;

#ifdef USE_RX
size_t RxTail;
#endif

uint8_t ActiveTxBuf;

uint8_t XmitState;
uint8_t ActBufState;
bool TransmitFuncRunning;

ErrorStatus XmitError = SUCCESS;

size_t MaxTail = 0U;

/**
  * @brief InitComm initializes buffers and pointers
  * @note
  * @param  none
  * @retval none
  */
ErrorStatus InitComm(void)
{
	ErrorStatus result;

	for (size_t i = 0U; i < BUFSIZE; i++) {
		TxBuf1[i] = 0U;
		TxBuf2[i] = 0U;
#ifdef USE_RX
		RxBuf[i] = 0U;
#endif
	}

	TxTail = 0U;
	pActTxBuf = &TxBuf1;
	pXmitTxBuf = &TxBuf2;
#ifdef USE_RX
	pRxBuf = &RxBuf;
#endif
	ActiveTxBuf = BUF1_ACTIVE;
	ActBufState = STATE_UNLOCKED;
	XmitState = STATE_UNLOCKED;
#ifdef USE_RX
	RxTail = 0U;
#endif

	result = SUCCESS;
	return result;
}
/* end of the function  InitComm */

/**
  * @brief  Transmit invokes transmit procedure
  * @param  ptr is a pointer to udp socket
  * @note   if ther is no sockets, ptr must be NULL
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
		result = write_socket((socket_p)ptr, pXmitTxBuf,
				      (int32_t)tmptail);

		XmitState = STATE_UNLOCKED;
	}
fExit:
	TransmitFuncRunning = false;
	return result;
}
/* end of the function Transmit() */

/**
  * @brief  myxfunc_out puts the char to the xmit buffer
  * @note   helper for xprintf
  * @param  char
  * @retval none
  */
void myxfunc_out(unsigned char c)
{
	while (ActBufState == STATE_LOCKED) {
		/* alas, the active buffer is locked */
		taskYIELD();
	}
	if (ActBufState == STATE_UNLOCKED) {
		ActBufState = STATE_LOCKED; /* temporary lock the buffer */

		MaxTail = (TxTail > MaxTail) ? TxTail : MaxTail;

		while (TxTail == BUFSIZE) {
			ActBufState = STATE_UNLOCKED;
			taskYIELD();
			ActBufState = STATE_LOCKED;
		}
		*((uint8_t *)pActTxBuf + TxTail) = c;
		TxTail++;
		ActBufState = STATE_UNLOCKED;
	}
}
/* end of the function  */

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
 * @brief HAL_UART_TxCpltCallback
 * @param huart
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == &huart3) {
		/* transmit completed! */
		XmitState = STATE_UNLOCKED;
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
