/*
** This file is part of the stm32f3disco project.
** Copyright 2018 <turchenkov@gmail.com>.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include <limits.h>

#ifdef STM32F303xC

#include "stm32f303xc.h"
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"

#elif STM32F103xB

#include "stm32f103xb.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_rcc.h"

#else

#error "MCU is not defined!"

#endif

#include "main.h"

#include "tim.h"
#include "manchester.h"
#include "bit_queue.h"

/* local enums */
typedef enum Edge { /* edge type */
		    rising,
		    falling,
		    neutral

} Edge_t;

/* structure holds timer configuration for the stage */
typedef struct TimerCfg {
	uint32_t CCMR1;
	uint32_t CR1;
	uint32_t CR2;
	uint32_t SMCR;
	uint32_t PSC;
	uint32_t CCER;
	uint32_t ARR;
	HAL_TIM_StateTypeDef state;
	uint8_t NeedMSPInit;
} TimerCfg_t;

/* static functions */

static ErrorStatus transmitBit(uint32_t timeoutMS, uint8_t bit);
static ErrorStatus transmitStartStopBits(uint32_t timeoutMS, uint8_t val, size_t num);

static void configTimer(TIM_HandleTypeDef *htim, size_t stage);

ErrorStatus processPulse(PulseData_t *const p,
			 MANCHESTER_Context_t *const context,
			 BitQueue_t *const qu);

ErrorStatus processStartStopBits(size_t nBits,
				 MANCHESTER_Context_t *const context,
				 BitQueue_t *const qu);
ErrorStatus processPayLoad(MANCHESTER_Context_t *const context,
			   MANCHESTER_Data_t *data, BitQueue_t *const qu);

ErrorStatus prepareRx(MANCHESTER_Context_t *const context);

ErrorStatus receiveBits(MANCHESTER_Context_t *context, BitQueue_t *qu);

/* access to the timer mutex */
extern osMutexId ManchesterTimer01MutexHandle;
//extern uint32_t measured1;
extern TaskHandle_t ManchTaskHandle;

static PulseData_t pulses;

/* pointer to the high 16 bit counter value - incremented every UIE interrupt */
uint16_t highCntValue;

/* flaf for UIE interrupt handler */
uint8_t RecvState;

/* array of timer configurations used when receiving packet */
static TimerCfg_t timerCfgData[] = {

/* 0 */	{ .CCMR1 = 0u,
	  .CR1 = 0u,
	  .CR2 = 0u,
	  .SMCR = 0u,
	  .PSC = 71u,
	  .CCER = 0u,
	  .ARR = UINT32_MAX,
	  .state = HAL_TIM_STATE_RESET,
	  .NeedMSPInit = 0 }, // default

/* 1 */ { 0,0,0,0,0,0,0,0,0},

/* 2 */	{ 0,0,0,0,0,0,0,0,0},

/* 3 */	{ 0,0,0,0,0,0,0,0,0},

/* 4 */	{ 0,0,0,0,0,0,0,0,0},

	/* for transmit */
/* 5 */	{ .CCMR1 = 0u,
	  .CR1 = TIM_CLOCKDIVISION_DIV1 | TIM_COUNTERMODE_DOWN |
		 TIM_AUTORELOAD_PRELOAD_ENABLE,
	  .CR2 = TIM_TRGO_RESET,
	  .SMCR = 0u,
	  .PSC = 71u,
	  .CCER = 0u,
	  .ARR = 65535u,
	  .state = HAL_TIM_STATE_READY,
	  .NeedMSPInit = 1 },

/* 6 */	{ 0,0,0,0,0,0,0,0,0},

	/* for receiving using EXTI */
/* 7 */	{ .CCMR1 = 0u,
	  .CR1 = TIM_CLOCKDIVISION_DIV1 | TIM_COUNTERMODE_UP |
		 TIM_AUTORELOAD_PRELOAD_ENABLE,
	  .CR2 = TIM_TRGO_RESET,
	  .SMCR = 0u,
	  .PSC = 71u,
	  .CCER = 0u,
	  .ARR = 65535u,
	  .state = HAL_TIM_STATE_READY,
	  .NeedMSPInit = 0 }, // GPIO input pin samlping

};


#if(0)
/**
 * @brief MANCHESTER_DebugLEDToggle
 */
void MANCHESTER_DebugLED8Toggle(void)
{
#ifdef MANCHESTER_DEBUG
	static uint8_t ledState;
	ledState = (ledState == 0u) ? 1u : 0u;
	HAL_GPIO_TogglePin(LD8_GPIO_Port, LD8_Pin);
#endif
}

void MANCHESTER_DebugLED6Toggle(void)
{
#ifdef MANCHESTER_DEBUG
	static uint8_t ledState;
	ledState = (ledState == 0u) ? 1u : 0u;
	HAL_GPIO_TogglePin(LD6_GPIO_Port, LD6_Pin);
#endif
}
#endif


/**
 ************** CONTEXT INITIALIZATION **************************
 */

/**
 * @brief MANCHESTER_InitContext initializes context
 * @param context pointer to context
 * @param htim
 * @param numStartBits at least 1 start bit required, numStartBits <= MAX_NUM_START_BITS
 * @param numStopBits <= MAX_NUM_STOP_BITS
 * @param bitRate
 * @param bitOrder
 * @param startStopBit
 * @return
 */
ErrorStatus MANCHESTER_InitContext(MANCHESTER_Context_t *context,
				   TIM_HandleTypeDef *htim, size_t numStartBits,
				   size_t numStopBits, size_t bitRate,
				   MANCHESTER_BitOrder_t bitOrder,
				   uint8_t startStopBit)
{
	ErrorStatus retVal = ERROR;
	if ((context == NULL) || (htim == NULL) ||
	    (IS_TIM_INSTANCE(htim->Instance) == 0)) {
		goto fExit;
	}
	context->htim = htim;
	context->bitOrder = bitOrder;
	context->bitRate = bitRate;
	if ((numStartBits > MAX_NUM_START_BITS) || (numStartBits == 0u)) {
		goto fExit;
	}
	context->numStartBits = numStartBits;
	if (numStopBits > MAX_NUM_STOP_BITS) {
		goto fExit;
	}
	context->numStopBits = numStopBits;
	context->startStopBit = startStopBit;

	/* parameters for time measurement */
	uint32_t timerClkFreq = HAL_RCC_GetPCLK1Freq() * 2u;
	uint32_t prescVal = htim->Instance->PSC & 0xFFFF;
	timerClkFreq = timerClkFreq / (prescVal + 1u);
	/* half-byte time value in timerClkFreq ticks */
	context->halfBitTime = timerClkFreq / (bitRate * 2u);
	uint32_t tol = (context->halfBitTime / HALF_BYTE_TOLERANCE);
	context->halfBitMinTime = context->halfBitTime - tol;
	context->halfBitMaxTime = context->halfBitTime + tol;
	/* pulse detection timeout in ms */
	context->pulseTimeout = (1000u / (bitRate * 2u)) + 2u;
	context->filterTime = 7u; /* 7 us*/
	retVal = SUCCESS;
fExit:
	return retVal;
}

/**
 ************** RECEIVING ROUTINES **************************
 */

// using EXTI and timer
/**
 * @brief MANCHESTER_Receive
 * @param data pointer to data context
 * @param context pointer to conmmunication context
 * @return SUCCESS or ERROR
 */
#ifdef MANCHESTER_DEBUG
volatile static size_t bits_received;
volatile static size_t mr_err;
#define MR_ERR_LINE mr_err = __LINE__
#else
#define MR_ERR_LINE
#endif

ErrorStatus MANCHESTER_Receive(MANCHESTER_Data_t *data,
			       MANCHESTER_Context_t *context)
{
	STROBE_1;
	pulses.low = 0x00u;
	pulses.high = 0x00u;
	pulses.hasLow1T = 0x00u;
	ErrorStatus retVal = ERROR;
	BitQueue_t qu = { 0x00U, 0x00U };
	RecvState = 0x01U;

	xTaskNotifyStateClear(ManchTaskHandle);

#ifdef MANCHESTER_DEBUG
	bits_received = 0x00u;
	mr_err = 0x00u;
#endif
	/* FREERTOS mutex*/
	BaseType_t mut = 0;

	if ((data == NULL) || (context == NULL)) {
		return ERROR;
	}
	if ((data->dataPtr == NULL) || (context->htim == NULL)) {
		return ERROR;
	}
	/* Decide if rtos has already started */
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		/* Take MUTEX */
		mut = xSemaphoreTake(ManchesterTimer01MutexHandle,
				     portMAX_DELAY);
	}

	retVal = prepareRx(context);
	if (retVal != SUCCESS) {
		MR_ERR_LINE;
		goto fExit;
	}

	retVal = processStartStopBits(context->numStartBits, context, &qu);
	if (retVal != SUCCESS) {
		MR_ERR_LINE;
		goto fExit;
	}

	retVal = processPayLoad(context, data, &qu);
	if (retVal != SUCCESS) {
		MR_ERR_LINE;
		goto fExit;
	}

	retVal = processStartStopBits(context->numStopBits, context, &qu);
	if (retVal != SUCCESS) {
		MR_ERR_LINE;
		goto fExit;
	}
	retVal = SUCCESS;

fExit:
	HAL_NVIC_DisableIRQ(EXTI0_IRQn);
	configTimer(context->htim, 0x00u);
	/* Give MUTEX */
	if (mut != 0) {
		/* Decide if rtos has already started */
		if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
			mut = xSemaphoreGive(ManchesterTimer01MutexHandle);
		}
	}
	RecvState = 0x00U;
	return retVal;
}

/*
	set up timer and nvic
	enable irq
	test Rx line for space condition

	set timeout to WAIT_FOR_START_MAX_MS
	wait for 1st rising edge

	1st rising edge:
		check Rx line for "1"
		save time t0
		set timeout to pulseTimeout
		return

	start bits receiving....

		wait for Interrupt
		if timeout -> error
		check Rx line for "0"
		if not "0" -> error
		save time t1
		pulses.high = t1 - t0
		wait for interrupt
		if timepout ->
			pulses.low = halfbit time
			exit
		else
		   check Rx line for "1"
			if not "1" - -> error exit
		   save time t2
		   pulses.low = t2 - t1
		   process pulses
		   if not error
			t0 = t2
		return

*/

/**
 * @brief prepareRx
 * @param context
 * @return
 */
#ifdef MANCHESTER_DEBUG
volatile static size_t pr_err;
#define PR_ERR_LINE do {pr_err = __LINE__; STROBE_0;} while(0)
#else
#define PR_ERR_LINE
#endif

ErrorStatus prepareRx(MANCHESTER_Context_t *const context)
{
#ifdef MANCHESTER_DEBUG
	pr_err = 0x00U;
#endif

	ErrorStatus retval = ERROR;
	context->t0 = 0x00U;
	highCntValue = 0x00u;
	configTimer(context->htim, 0x07u);

	BaseType_t xResult;
	uint32_t ulNotifiedValue;
	if (HAL_GPIO_ReadPin(MANCHESTER_RX_GPIO_Port, MANCHESTER_RX_Pin) !=
	    GPIO_PIN_RESET) {
		PR_ERR_LINE;
		goto fExit;
	}

	__HAL_GPIO_EXTI_CLEAR_IT(
		MANCHESTER_RX_Pin);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);

	/* wait for 1st rising edge */

	xResult = xTaskNotifyWait(pdFALSE, ULONG_MAX, &ulNotifiedValue,
				  pdMS_TO_TICKS(WAIT_FOR_START_MAX_MS));
	STROBE_0;
	if (xResult != pdPASS) {
		PR_ERR_LINE;
		goto fExit;
	}
	if (HAL_GPIO_ReadPin(MANCHESTER_RX_GPIO_Port, MANCHESTER_RX_Pin) !=
	    GPIO_PIN_SET) {
		PR_ERR_LINE;
		goto fExit;
	}
	{
		context->htim->Instance->CNT = 0x00u;
		context->htim->Instance->EGR = TIM_EGR_UG;
		context->htim->Instance->SR = 0x00u;
		context->htim->Instance->DIER = context->htim->Instance->DIER | TIM_DIER_UIE;
		context->htim->Instance->CR1 |= 0x01u;

	}
	context->t0 =
		context->htim->Instance->CNT + (uint32_t)(highCntValue << 16U);

retval = SUCCESS;
fExit:
	return retval;
}

/**
 * @brief receiveBits
 * @param context
 * @param qu
 * @return
 */
#ifdef MANCHESTER_DEBUG
volatile static uint32_t rb_err;
#define RB_ERR_LINE do {rb_err = __LINE__; STROBE_0;} while(0)

#else
#define RB_ERR_LINE
#endif

ErrorStatus receiveBits(MANCHESTER_Context_t *context, BitQueue_t *qu)
{
#ifdef MANCHESTER_DEBUG
	rb_err = 0x00u;
#endif

	ErrorStatus retVal = ERROR;
	BaseType_t xResult;
	uint32_t ulNotifiedValue;
	TIM_HandleTypeDef *htim = context->htim;

	TickType_t timeOut = pdMS_TO_TICKS(context->pulseTimeout);
	if (HAL_GPIO_ReadPin(MANCHESTER_RX_GPIO_Port, MANCHESTER_RX_Pin) !=
	    GPIO_PIN_SET) {
		RB_ERR_LINE;
		goto fExit;
	}

	xResult =
		xTaskNotifyWait(pdFALSE, ULONG_MAX, &ulNotifiedValue, timeOut);
	if (xResult != pdPASS) {
		RB_ERR_LINE;
		goto fExit;
	}
	if (HAL_GPIO_ReadPin(MANCHESTER_RX_GPIO_Port, MANCHESTER_RX_Pin) !=
	    GPIO_PIN_RESET) {
		RB_ERR_LINE;
		goto fExit;
	}

	uint32_t t1 = htim->Instance->CNT + (uint32_t)(highCntValue << 16U);
	pulses.high = t1 - context->t0;
	xResult =
		xTaskNotifyWait(pdFALSE, ULONG_MAX, &ulNotifiedValue, timeOut);
	if (xResult != pdPASS) {
		if (HAL_GPIO_ReadPin(MANCHESTER_RX_GPIO_Port,
				     MANCHESTER_RX_Pin) == GPIO_PIN_RESET) {
			pulses.low = context->halfBitTime;
			retVal = processPulse(&pulses, context, qu);
			if (retVal != SUCCESS) {
				RB_ERR_LINE;
			}
			goto fExit;
		} else {
			RB_ERR_LINE;
			goto fExit;
		}
	} else {
		if (HAL_GPIO_ReadPin(MANCHESTER_RX_GPIO_Port,
				     MANCHESTER_RX_Pin) != GPIO_PIN_SET) {
			RB_ERR_LINE;
			goto fExit;
		}
		uint32_t t2 =
			htim->Instance->CNT + (uint32_t)(highCntValue << 16U);
		pulses.low = t2 - t1;
		context->t0 = t2;
		retVal = processPulse(&pulses, context, qu);
		if (retVal != SUCCESS) {
			RB_ERR_LINE;
		}
	}
fExit:

	return retVal;
}

/**
 * @brief processPulse
 * @param p
 * @param context
 * @param qu
 * @return SUCCESS or ERROR
 */
#ifdef MANCHESTER_DEBUG
volatile static uint32_t pp_err;
#define PP_ERR_LINE do {pp_err = __LINE__; STROBE_0;} while(0)
#else
#define PP_ERR_LINE
#endif

ErrorStatus processPulse(PulseData_t *const p,
			 MANCHESTER_Context_t *const context,
			 BitQueue_t *const qu)
{
#ifdef MANCHESTER_DEBUG
	pp_err = 0x00u;
#endif
	uint8_t retVal = ERROR;
	uint8_t activeBits;
	activeBits = (p->hasLow1T == 0x01U) ? 0x01U : 0x00U;
	uint8_t tmp = 0x00U;
	if ((p->high > context->halfBitMinTime) &&
	    (p->high < context->halfBitMaxTime)) {
		tmp = (uint8_t)(0x01U << activeBits);
		activeBits++;
	} else if ((p->high > 2u * context->halfBitMinTime) &&
		   (p->high < 2u * context->halfBitMaxTime)) {
		tmp = (uint8_t)(0x03U << activeBits);
		activeBits++;
		activeBits++;
	} else {
		PP_ERR_LINE;
		goto fExit;
	}
	if ((p->low > context->halfBitMinTime) &&
	    (p->low < context->halfBitMaxTime)) {
		activeBits++;
	} else if ((p->low > 2u * context->halfBitMinTime) &&
		   (p->low < 2u * context->halfBitMaxTime)) {
		activeBits++;
		activeBits++;
	} else {
		PP_ERR_LINE;
		goto fExit;
	}

	while (activeBits > 0x01U) {
		if ((tmp & 0x03U) == 0x01U) {
			// bit = 1
			if (putBitInQueue(0x01U, qu) != bqOk) {
				for (;;) {
				};
			}
#ifdef MANCHESTER_DEBUG
			bits_received++;
#endif
		} else if ((tmp & 0x03U) == 0x02U) {
			// bit = 0
			if (putBitInQueue(0x00U, qu) != bqOk) {
				for (;;) {
				};
			}
#ifdef MANCHESTER_DEBUG
			bits_received++;
#endif
		} else {
			PP_ERR_LINE;
			goto fExit;
		}
		activeBits--;
		activeBits--;
		tmp = (tmp >> 0x02U);
	}
	p->hasLow1T = (activeBits == 0x01U) ? 0x01U : 0x00U;
	retVal = SUCCESS;
fExit:
	return retVal;
}

/**
 * @brief processStartStopBits
 * @param nBits
 * @param context
 * @param qu
 * @return
 */
#ifdef MANCHESTER_DEBUG
volatile static uint32_t psb_err;
#define PSB_ERR_LINE do {psb_err = __LINE__; STROBE_0;} while(0)
#else
#define PSB_ERR_LINE
#endif

ErrorStatus processStartStopBits(size_t nBits,
				 MANCHESTER_Context_t *const context,
				 BitQueue_t *const qu)
{
#ifdef MANCHESTER_DEBUG
	psb_err = 0x00U;
#endif
	ErrorStatus retVal = ERROR;
	uint8_t tmpBit;
	while (nBits > 0x00U) {
		if (dequeueBit(&tmpBit, qu) == bqEmpty) {
			retVal = receiveBits(context, qu);
			if (retVal != SUCCESS) {
				PSB_ERR_LINE;
				break;
			} else {
				dequeueBit(&tmpBit, qu);
			}
		}
		if (tmpBit != context->startStopBit) {
			PSB_ERR_LINE;
			retVal = ERROR;
			break;
		} else {
			retVal = SUCCESS;
		}
		nBits--;
	}
	return retVal;
}

/**
 * @brief processPayLoad
 * @param context
 * @param data
 * @param qu
 * @return
 */

#ifdef MANCHESTER_DEBUG
volatile static uint32_t ppl_err;
#define PPL_ERR_LINE do {ppl_err = __LINE__; STROBE_0;} while(0)
#else
#define PPL_ERR_LINE
#endif

ErrorStatus processPayLoad(MANCHESTER_Context_t *const context,
			   MANCHESTER_Data_t *data, BitQueue_t *const qu)
{
#ifdef MANCHESTER_DEBUG
	ppl_err = 0x00U;
#endif
	ErrorStatus retVal;
	retVal = ERROR;

	size_t fullBytes;
	fullBytes = (data->numBits / CHAR_BIT);
	size_t bitsInTheLastByte;
	bitsInTheLastByte = (data->numBits % CHAR_BIT);
	size_t skipBits; /* number of highest bits of the last byte to be skipped */
	skipBits = CHAR_BIT - bitsInTheLastByte;

	size_t i = 0u;
	uint8_t mask;
	while (i < fullBytes) {
		data->dataPtr[i] = 0u;
		mask = (context->bitOrder == MANCHESTER_BitOrderLSBFirst) ?
			       0x01U :
			       0x80U;

		do {
			uint8_t tmpBit;
			while (dequeueBit(&tmpBit, qu) == bqEmpty) {
				retVal = receiveBits(context, qu);
				if (retVal != SUCCESS) {
					PPL_ERR_LINE;
					goto fExit;
				}
			}

			data->dataPtr[i] = (tmpBit == 0x00U) ?
						   data->dataPtr[i] :
						   (data->dataPtr[i] | mask);

			if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
				mask = (uint8_t)(mask << 1u);
			} else {
				mask = (uint8_t)(mask >> 1u);
			}
		} while (mask != 0x00u);
		i++;
	}
	if (bitsInTheLastByte != 0u) {
		data->dataPtr[fullBytes] = 0x00u;
		mask = (context->bitOrder == MANCHESTER_BitOrderLSBFirst) ?
			       0x01U :
			       (0x80u >> skipBits);
		while (bitsInTheLastByte > 0u) {
			uint8_t tmpBit;
			while (dequeueBit(&tmpBit, qu) == bqEmpty) {
				retVal = receiveBits(context, qu);
				if (retVal != SUCCESS) {
					PPL_ERR_LINE;
					goto fExit;
				}
			}

			data->dataPtr[i] = (tmpBit == 0x00U) ?
						   data->dataPtr[i] :
						   (data->dataPtr[i] | mask);

			if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
				mask = (uint8_t)(mask << 1);
			} else {
				mask = (uint8_t)(mask >> 1);
			}
			bitsInTheLastByte--;
		}
	}
fExit:
	return retVal;
}

/**
 * @brief configTimer configures the selected timer for
 * @param htim
 * @param stage index of the config. params.table
 */
static void configTimer(TIM_HandleTypeDef *htim, size_t stage)
{
	if (timerCfgData[stage].state == HAL_TIM_STATE_RESET) {
		//		HAL_TIM_Base_MspDeInit(htim);
	} else {
		if (timerCfgData[stage].NeedMSPInit == 1u) {
			HAL_TIM_Base_MspInit(htim);
		}
	}
	/* copied from STM32 Cube code */
	uint32_t tmpsmcr;
	tmpsmcr = htim->Instance->SMCR;
	tmpsmcr &= ~(TIM_SMCR_SMS | TIM_SMCR_TS);
	tmpsmcr &=
		~(TIM_SMCR_ETF | TIM_SMCR_ETPS | TIM_SMCR_ECE | TIM_SMCR_ETP);
	htim->Instance->SMCR = tmpsmcr;
	/* end of copied... from STM32 Cube code */

	htim->Instance->CR1 = timerCfgData[stage].CR1;
	htim->Instance->ARR = timerCfgData[stage].ARR;
	htim->Instance->PSC = timerCfgData[stage].PSC;
	htim->Instance->EGR = TIM_EGR_UG;

	htim->Instance->CR2 = timerCfgData[stage].CR2;
	htim->Instance->CCMR1 = timerCfgData[stage].CCMR1;
	htim->Instance->SMCR = timerCfgData[stage].SMCR;
	htim->Instance->CCER = timerCfgData[stage].CCER;
	htim->Instance->DIER = 0u;
	htim->State = timerCfgData[stage].state;
}

/**
 * @brief HAL_GPIO_EXTI_Callback
 * @param GPIO_Pin
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == MANCHESTER_RX_Pin) {
		STROBE_0;
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		uint32_t notification = 0x00u;
//		MANCHESTER_DebugLED6Toggle();
		STROBE_1;
		xTaskNotifyFromISR(ManchTaskHandle, notification,
				   eSetValueWithOverwrite,
				   &xHigherPriorityTaskWoken);

		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

/**
 ************** TRANSMISSION ROUTINES **************************
 */

/**
 * @brief MANCHESTER_Transmit
 * @param data pointer to data context
 * @param context pointer to conmmunication context
 * @return SUCCESS or ERROR
 */
ErrorStatus MANCHESTER_Transmit(MANCHESTER_Data_t *data,
				MANCHESTER_Context_t *context)
{
	ErrorStatus retVal = SUCCESS;
	BaseType_t mut = 0;
	if ((data == NULL) || (context == NULL)) {
		return ERROR;
	}
	if ((data->dataPtr == NULL) || (context->htim == NULL)) {
		return ERROR;
	}
	TIM_HandleTypeDef *htim = context->htim;
	/* Decide if rtos has already started */
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		/* Take MUTEX */
		mut = xSemaphoreTake(ManchesterTimer01MutexHandle,
				     portMAX_DELAY);
	}

	xTaskNotifyStateClear(ManchTaskHandle);

	/* initialize timer hardware for transmission */
	configTimer(htim, 5u);
	htim->Instance->ARR = context->halfBitTime;
	htim->Instance->EGR = TIM_EGR_UG;

	htim->Instance->SR = 0u;
	htim->Instance->CR1 = htim->Instance->CR1 | TIM_CR1_CEN;
	htim->Instance->DIER = htim->Instance->DIER | TIM_DIER_UIE;

	/* start bits transmission */
	retVal = transmitStartStopBits(context->pulseTimeout,
				       context->startStopBit,
				       context->numStartBits);
	if (retVal != SUCCESS) {
		goto fExit;
	}
	/* data transmission */
	size_t fullBytes;
	fullBytes = (data->numBits / CHAR_BIT);
	size_t bitsInTheLastByte;
	bitsInTheLastByte = (data->numBits % CHAR_BIT);
	size_t skipBits; /* number of highest bits of the last byte to be skipped */
	skipBits = CHAR_BIT - bitsInTheLastByte;
	size_t i = 0u;
	uint8_t mask;
	while (i < fullBytes) {
		if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
			mask = 0x01u;
		} else {
			mask = 0x80u;
		}
		do {
			retVal = transmitBit(context->pulseTimeout,
					     data->dataPtr[i] & mask);
			if (retVal != SUCCESS) {
				goto fExit;
			}
			if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
				mask = (uint8_t)(mask << 1u);
			} else {
				mask = (uint8_t)(mask >> 1u);
			}

		} while (mask != 0x00u);
		i++;
	}
	if (bitsInTheLastByte != 0u) {
		if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
			mask = 0x01u;
		} else {
			mask = (0x80u >> skipBits);
		}
		while (bitsInTheLastByte > 0u) {
			retVal = transmitBit(context->pulseTimeout,
					     data->dataPtr[fullBytes] & mask);
			if (retVal != SUCCESS) {
				goto fExit;
			}
			if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
				mask = (uint8_t)(mask << 1);
			} else {
				mask = (uint8_t)(mask >> 1);
			}
			bitsInTheLastByte--;
		}
	}
	/* stop bits transmission */
	retVal = transmitStartStopBits(context->pulseTimeout,
				       context->startStopBit,
				       context->numStopBits);
	if (retVal != SUCCESS) {
		goto fExit;
	}
fExit:

	/* deinitialize timer hardware after receiving */
	htim->Instance->DIER = htim->Instance->DIER & ~(TIM_DIER_UIE);
	htim->Instance->CR1 = htim->Instance->CR1 & ~(TIM_CR1_CEN);
	configTimer(htim, 0u);

	/* Give MUTEX */
	if (mut != 0) {
		/* Decide if rtos has already started */
		if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
			mut = xSemaphoreGive(ManchesterTimer01MutexHandle);
		}
		(void)mut;
	}
	return retVal;
}

/**
 * @brief transmitStartStopBits
 * @param htim timer to use to transmit
 * @param val 0 as logic 0, logic one otherwize
 * @param num number of repetitions
 * @return
 */
static ErrorStatus transmitStartStopBits(uint32_t timeoutMS, uint8_t val, size_t num)
{
	ErrorStatus retVal = SUCCESS;
	while ((num > 0u) && (retVal == SUCCESS)) {
		retVal = transmitBit(timeoutMS, val);
		num--;
	}
	return retVal;
}

/**
 * @brief transmitBit
 * @param timeoutMS
 * @param htim
 * @param val
 * @return
 */
static ErrorStatus transmitBit(uint32_t timeoutMS, uint8_t val)
{
	ErrorStatus retVal = SUCCESS;

	const TickType_t xMaxBlockTime = pdMS_TO_TICKS(timeoutMS);
	BaseType_t xResult;
	uint32_t ulNotifiedValue;

	/* Wait to be notified of an interrupt. */
	xResult = xTaskNotifyWait(
		pdFALSE,	  /* Don't clear bits on entry. */
		ULONG_MAX,	/* Clear all bits on exit. */
		&ulNotifiedValue, /* Stores the notified value. */
		xMaxBlockTime);

	if ((xResult != pdPASS) || (ulNotifiedValue != 1u)) {
		retVal = ERROR;
		goto fExit;
	}

	if (val == 0u) {
		HAL_GPIO_WritePin(MANCHESTER_TX_GPIO_Port, MANCHESTER_TX_Pin,
				  GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(MANCHESTER_TX_GPIO_Port, MANCHESTER_TX_Pin,
				  GPIO_PIN_SET);
	}
	/* Wait to be notified of an interrupt. */
	xResult = xTaskNotifyWait(
		pdFALSE,	  /* Don't clear bits on entry. */
		ULONG_MAX,	/* Clear all bits on exit. */
		&ulNotifiedValue, /* Stores the notified value. */
		xMaxBlockTime);
	if ((xResult != pdPASS) || (ulNotifiedValue != 1u)) {
		retVal = ERROR;
		goto fExit;
	}

	HAL_GPIO_TogglePin(MANCHESTER_TX_GPIO_Port, MANCHESTER_TX_Pin);
fExit:
	return retVal;
}


/**
 * @brief MANCHESTER_TimerISR interrupt handler
 */
void MANCHESTER_TimerISR(void)
{
//	STROBE_1;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	uint32_t notification = UINT32_MAX;
	/* TIM Trigger detection event */
	if (__HAL_TIM_GET_FLAG(&MANCHESTER_Timer, TIM_FLAG_TRIGGER) != RESET) {
		if (__HAL_TIM_GET_IT_SOURCE(&MANCHESTER_Timer, TIM_IT_TRIGGER) != RESET) {
			__HAL_TIM_CLEAR_FLAG(&MANCHESTER_Timer, TIM_FLAG_TRIGGER);
//			MANCHESTER_DebugLEDToggle();
			notification = xTaskGetTickCountFromISR();
			goto fExit;
		}
	}

	/* input capture event */
	if (__HAL_TIM_GET_FLAG(&MANCHESTER_Timer, TIM_FLAG_CC1) != RESET) {
		if (__HAL_TIM_GET_IT_SOURCE(&MANCHESTER_Timer, TIM_IT_CC1) != RESET) {
			__HAL_TIM_CLEAR_FLAG(&MANCHESTER_Timer, TIM_FLAG_CC1);
			if ((MANCHESTER_Timer.Instance->CCMR1 & TIM_CCMR1_CC1S) != 0x00U) {
//				MANCHESTER_DebugLEDToggle();
				notification = HAL_TIM_ReadCapturedValue(&MANCHESTER_Timer, TIM_CHANNEL_1);
				goto fExit;
			}
		}
	}

	/* update event */
	if (__HAL_TIM_GET_FLAG(&MANCHESTER_Timer, TIM_FLAG_UPDATE) != RESET) {
		if (__HAL_TIM_GET_IT_SOURCE(&MANCHESTER_Timer, TIM_IT_UPDATE) != RESET) {
			__HAL_TIM_CLEAR_FLAG(&MANCHESTER_Timer, TIM_FLAG_UPDATE);
//			MANCHESTER_DebugLEDToggle();
			notification = 1u;
			goto fExit;
		}
	}

  HAL_TIM_IRQHandler(&MANCHESTER_Timer);

	return;
fExit:
	xTaskNotifyFromISR(ManchTaskHandle, notification,
			eSetValueWithOverwrite,
			  &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	STROBE_0;
	return;
}

