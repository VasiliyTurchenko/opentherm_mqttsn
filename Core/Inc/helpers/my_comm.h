/**
 ******************************************************************************
 * @file    my_comm.h
 * @author  Vasiliy Turchenko
 * @brief   my_comm.c
 * @date    30-12-2017
 * @modified 20-Jan-2019
 */

#ifndef MY_COMM_H
#define MY_COMM_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef STM32F303xC
#include "stm32f3xx.h"
#elif STM32F103xB
#include "stm32f1xx.h"
#else
#error MCU NOT DEFINED
#endif

#include "sys_cfg.h"

/*-------------------------------------------------------------------------------------------------

			Packet size



USART Speed
		64	128	256	512	1024	2048
115200	11520	5,6	11,1	22,2	44,4	88,9	177,8
57600	5760	11,1	22,2	44,4	88,9	177,8	355,6
38400	3840	16,7	33,3	66,7	133,3	266,7	533,3
19200	1920	33,3	66,7	133,3	266,7	533,3	1066,7
14400	1440	44,4	88,9	177,8	355,6	711,1	1422,2
9600	960	66,7	133,3	266,7	533,3	1066,7	2133,3
4800	480	133,3	266,7	533,3	1066,7	2133,3	4266,7
2400	240	266,7	533,3	1066,7	2133,3	4266,7	8533,3
1200	120	533,3	1066,7	2133,3	4266,7	8533,3	17066,7

*/

/* USE_RX enables Rx buffer and routines */
//#define		USE_RX

#define BUFSIZE USART_TX_BUFSIZE

#define BUF1_ACTIVE ((uint8_t)0)
#define BUF2_ACTIVE ((uint8_t)(~BUF1_ACTIVE))

#define STATE_UNLOCKED ((uint8_t)0)
#define STATE_LOCKED ((uint8_t)(~STATE_UNLOCKED))

extern uint8_t TxBuf1[BUFSIZE];
extern uint8_t TxBuf2[BUFSIZE];
extern void *pActTxBuf;
extern void *pXmitTxBuf;
extern uint8_t RxBuf[BUFSIZE];
extern void *pRxBuf;

extern size_t TxTail;
extern size_t RxTail;
extern uint8_t ActiveTxBuf;

extern uint8_t XmitState;
extern uint8_t ActBufState;
extern bool TransmitFuncRunning;
// extern	HAL_StatusTypeDef	XmitStatus;

extern ErrorStatus XmitError;

extern size_t MaxTail;

ErrorStatus InitComm(void);
ErrorStatus Transmit(const void *ptr);
void myxfunc_out(unsigned char c);
void myxfunc_out_no_RTOS(unsigned char c);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

#endif
/* ############################### end of the file ########################## */
