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
#include "cmsis_os.h"
#include "task.h"

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

#endif

#include "main.h"

#include "manchester.h"
#include "tim.h"

/* local enums */
typedef enum Edge { /* edge type */
                    rising,
                    falling
} Edge_t;

#define PULSE_CORRECTION 20u;

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

ErrorStatus transmitBit(uint32_t timeoutMS, TIM_HandleTypeDef *htim,
                        uint8_t bit);
ErrorStatus transmitStartStopBits(uint32_t timeoutMS, TIM_HandleTypeDef *htim,
                                  uint8_t val, size_t num);

ErrorStatus receiveBit(uint32_t timeoutMS, uint8_t *val, uint8_t mask);
ErrorStatus receiveStartStopBits(uint32_t timeoutMS, uint8_t val, size_t num);
void configTimer(TIM_HandleTypeDef *htim, size_t stage);
uint32_t capturePulse(TIM_HandleTypeDef *htim, uint32_t timeout, Edge_t edge);

uint8_t decidePulseWidth(uint32_t measTime, MANCHESTER_Context_t *context);
uint8_t decodeBit(uint8_t one, uint8_t two);
uint8_t sampleAt(TIM_HandleTypeDef *htim, uint32_t timeoutMS, uint32_t tts);
uint8_t sampleAndDecodeBit(TIM_HandleTypeDef *htim, uint32_t measTime);

uint8_t swapBits(uint8_t val);
void reverseBitString(uint8_t *data, size_t len);
size_t sbfr(uint8_t *field, uint32_t flen, size_t ns);

/* access to the timer mutex */
extern osMutexId ManchesterTimer01MutexHandle;
// extern uint32_t measured1;
extern TaskHandle_t myTask02_MANCHHandle;

/* local variables */
uint32_t captured1; // last err line

#define LAST_ERR captured1 = __LINE__

/* array of timer configurations used when receiving packet */
static TimerCfg_t timerCfgData[] = {
    {.CCMR1 = 0u,
     .CR1 = 0u,
     .CR2 = 0u,
     .SMCR = 0u,
     .PSC = 8, // 71u,
     .CCER = 0u,
     .ARR = UINT32_MAX,
     .state = HAL_TIM_STATE_RESET,
     .NeedMSPInit = 0}, // default

    /* wait for 1st rising edge */
    {.CCMR1 = TIM_ICSELECTION_DIRECTTI | TIM_ICPSC_DIV1 | (12U << 4U),
     .CR1 = TIM_CLOCKDIVISION_DIV4 | TIM_COUNTERMODE_UP |
            TIM_AUTORELOAD_PRELOAD_DISABLE,
     .CR2 = TIM_TRGO_RESET,
     .SMCR = TIM_SLAVEMODE_EXTERNAL1 | TIM_TS_TI1FP1,
     .PSC = 8, // 71u,
     .CCER = TIM_INPUTCHANNELPOLARITY_RISING,
     .ARR = 1u,
     .state = HAL_TIM_STATE_READY,
     .NeedMSPInit = 1},

    {.CCMR1 = TIM_ICSELECTION_DIRECTTI | TIM_ICPSC_DIV1 | (12U << 4U),
     .CR1 = TIM_CLOCKDIVISION_DIV4 | TIM_COUNTERMODE_UP |
            TIM_AUTORELOAD_PRELOAD_DISABLE,
     .CR2 = TIM_TRGO_RESET,
     .SMCR = 0u,
     .PSC = 8, // 71u,
     .CCER = TIM_INPUTCHANNELPOLARITY_FALLING | TIM_CCER_CC1E,
     .ARR = 65535u,
     .state = HAL_TIM_STATE_READY,
     .NeedMSPInit = 0}, // measure first pulse

    {.CCMR1 = 0u,
     .CR1 = TIM_CLOCKDIVISION_DIV1 | TIM_COUNTERMODE_DOWN |
            TIM_AUTORELOAD_PRELOAD_ENABLE,
     .CR2 = TIM_TRGO_RESET,
     .SMCR = 0u,
     .PSC = 8, // 71u,
     .CCER = 0u,
     .ARR = 65535u,
     .state = HAL_TIM_STATE_READY,
     .NeedMSPInit = 0}, // GPIO input pin samlping

    /* gated mode */
    {.CCMR1 = TIM_ICSELECTION_DIRECTTI | TIM_ICPSC_DIV1 | (12U << 4U),
     .CR1 = TIM_CLOCKDIVISION_DIV4 | TIM_COUNTERMODE_UP |
            TIM_AUTORELOAD_PRELOAD_DISABLE,
     .CR2 = TIM_TRGO_RESET,
     .SMCR = TIM_SLAVEMODE_GATED | TIM_TS_TI1FP1,
     .PSC = 8, // 71u,
     .CCER = TIM_INPUTCHANNELPOLARITY_RISING | TIM_CCER_CC1E,
     .ARR = 65535u,
     .state = HAL_TIM_STATE_READY,
     .NeedMSPInit = 1}, // measure first pulse

    /* for transmit */
    {.CCMR1 = 0u,
     .CR1 = TIM_CLOCKDIVISION_DIV1 | TIM_COUNTERMODE_DOWN |
            TIM_AUTORELOAD_PRELOAD_ENABLE,
     .CR2 = TIM_TRGO_RESET,
     .SMCR = 0u,
     .PSC = 8, // 71u,
     .CCER = 0u,
     .ARR = 65535u,
     .state = HAL_TIM_STATE_READY,
     .NeedMSPInit = 1}, // GPIO input pin samlping

};

/**
 * @brief MANCHESTER_DebugLEDToggle
 */
void MANCHESTER_DebugLEDToggle(void) {
#ifdef MANCHESTER_DEBUG
  static uint8_t captureLedState;
  captureLedState = (captureLedState == 0u) ? 1u : 0u;
  HAL_GPIO_TogglePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin);
#endif
}

/**
 ************** CONTEXT INITIALIZATION **************************
 */

/**
 * @brief MANCHESTER_InitContext initializes context
 * @param context pointer to context
 * @param htim
 * @param numStartBits at least 1 start bit required, numStartBits <=
 * MAX_NUM_START_BITS
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
                                   uint8_t startStopBit) {
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
  // TODO
  /** external clock mode1, input TI1FP1, downcounting from preloaded 1 downto 0
   *  If Fck_int = 72 MHz and internal clock div CKD = 4
   *  F DTS = 18MHz
   *  ICxF[3:0] = 15 ---> Fsampling = F DTS : 32 = 562500Hz; N = 8; Tfilter
   * = 14.3uS ICxF[3:0] = 12 ---> Fsampling = F DTS : 16 = 1125000Hz; N = 8;
   * Tfilter = 7.15uS
   */
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
/**
 * @brief MANCHESTER_Receive
 * @param data pointer to data context
 * @param context pointer to conmmunication context
 * @return SUCCESS or ERROR
 */
ErrorStatus MANCHESTER_Receive(MANCHESTER_Data_t *data,
                               MANCHESTER_Context_t *context) {
  ErrorStatus retVal = ERROR;
  STROBE_1;

  /* storage of received half-bits */
  //	uint8_t p0 = UINT8_MAX; // 1st half-bit of the first bit in the packet
  uint8_t p1 = UINT8_MAX; // 2nd half-bit of the first bit in the packet
  uint8_t p2 = UINT8_MAX; // 1st half-bit of the second bit in the packet
  uint8_t p3 = UINT8_MAX; // 2nd half-bit of the second bit in the packet

  /* FREERTOS mutex*/
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
    mut = xSemaphoreTake(ManchesterTimer01MutexHandle, portMAX_DELAY);
  }
#define GATED_MODE

#ifndef GATED_MODE
  /* initialize the timer hardware for receiving 1st rising edge */
  /* initialize timer as interrupt controller */
  TimerConfig(context->htim, 1u);
  retVal = SUCCESS;
  /* Wait to be notified of an interrupt. */
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(WAIT_FOR_START_MAX_MS);
  BaseType_t xResult;
  uint32_t ulNotifiedValue;
  /* Go! */
  htim->Instance->SR = 0u;
  // we do not need to set CEN bit to have TRIG Interrupt
  htim->Instance->DIER = TIM_DIER_TIE;

  xResult = xTaskNotifyWait(pdFALSE,          /* Don't clear bits on entry. */
                            ULONG_MAX,        /* Clear all bits on exit. */
                            &ulNotifiedValue, /* Stores the notified value. */
                            xMaxBlockTime);
  htim->Instance->DIER &= ~TIM_DIER_TIE;
  if (xResult != pdPASS) {
    retVal = ERROR;
    measured = 0xFFFF;
    LAST_ERR;
    goto fExit;
  }

  /* set up the timer for capturing falling/rising edge */
  TimerConfig(context->htim, 2u);

  measured = capturePulse(htim, pulseTimeout, falling);
#else

  configTimer(context->htim, 4u);
  STROBE_0;
  retVal = SUCCESS;
  BaseType_t xResult;
  uint32_t ulNotifiedValue;
  htim->Instance->CNT = 0u;
  htim->Instance->EGR = TIM_EGR_UG;
  htim->Instance->SR = 0u;
  htim->Instance->DIER = TIM_DIER_TIE;
  htim->Instance->CR1 |= 01u;
  xResult = xTaskNotifyWait(pdFALSE,          /* Don't clear bits on entry. */
                            ULONG_MAX,        /* Clear all bits on exit. */
                            &ulNotifiedValue, /* Stores the notified value. */
                            pdMS_TO_TICKS(10000));

  if (xResult != pdPASS) {
    retVal = ERROR;
    //		measured1 = 0xFFFF;
    LAST_ERR;
    goto fExit;
  }
  /* we need second event */
  xResult = xTaskNotifyWait(pdFALSE,          /* Don't clear bits on entry. */
                            ULONG_MAX,        /* Clear all bits on exit. */
                            &ulNotifiedValue, /* Stores the notified value. */
                            pdMS_TO_TICKS(context->pulseTimeout));

  if (xResult != pdPASS) {
    retVal = ERROR;
    //		measured1 = 0xFFFF;
    LAST_ERR;
    goto fExit;
  }
  htim->Instance->DIER &= ~TIM_DIER_TIE;
  htim->Instance->CR1 &= ~01u;
  uint32_t measuredPulse = htim->Instance->CNT;
//	measured1 = measuredPulse;
#endif // GATED_MODE
  uint8_t pulseVal = decidePulseWidth(measuredPulse, context);
  if ((pulseVal != 1u) && (pulseVal != 2u)) {
    retVal = ERROR; // bad pulse
    LAST_ERR;
    goto fExit;
  }

  /* set up the timer for sampling */
  configTimer(context->htim, 3u);
  uint8_t bit2; // 2nd bit of the packet
  if (context->startStopBit == 1u) {
    if (pulseVal == 1u) {
      p1 = sampleAt(htim, context->pulseTimeout,
                    (measuredPulse / 2u) - 104u - 56u);
      if (p1 != 0) {    // wrong start bit = 11 is illegal
        retVal = ERROR; // bad pulse
        LAST_ERR;
        goto fExit;
      }
      p2 = sampleAt(htim, context->pulseTimeout, measuredPulse - 64u - 24u);
      p3 = sampleAt(htim, context->pulseTimeout, measuredPulse - 64u - 24u);
    } else {
      retVal = ERROR; // bad pulse
      LAST_ERR;
      goto fExit;
    }
  }
  if (context->startStopBit == 0u) {
    p1 = 1;
    if (pulseVal == 1u) {
      p2 = sampleAt(htim, context->pulseTimeout, (measuredPulse / 2u));
      p3 = sampleAt(htim, context->pulseTimeout, measuredPulse);
    }
    if (pulseVal == 2u) {
      p2 = 1u;
      p3 = sampleAt(htim, context->pulseTimeout, measuredPulse / -6u);
    }
  }
  /* set timer sampling interval = measured (i.e. actual half-bit time) */
  htim->Instance->ARR = measuredPulse;
  /*enable timer and wait for flag */
  htim->Instance->SR = 0u;
  htim->Instance->CR1 = htim->Instance->CR1 | TIM_CR1_CEN;
  htim->Instance->DIER = htim->Instance->DIER | TIM_DIER_UIE;

  /* here we have two first bits received*/
  bit2 = decodeBit(p2, p3);
  uint8_t needInject = 1u; // bit2 - first bit of the payload!
  if (context->numStartBits > 1u) {
    needInject = 0u;
    if (context->startStopBit != bit2) {
      retVal = ERROR;
      LAST_ERR;
      goto fExit;
    }
    uint32_t startBitsLeft = context->numStartBits - 2u;
    retVal = receiveStartStopBits(context->pulseTimeout, context->startStopBit,
                                  startBitsLeft);
    if (retVal != SUCCESS) {
      LAST_ERR;
      goto fExit;
    }
  }
  /* data bits go here */

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
    if (needInject == 1u) {
      if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
        mask = 0x02u;
        needInject = 0u;
        data->dataPtr[0u] = bit2;
      } else {
        mask = 0x40u;
        needInject = 0u;
        data->dataPtr[0u] = (uint8_t)(bit2 << 7u);
      }
    } else {
      if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
        mask = 0x01u;
      } else {
        mask = 0x80u;
      }
    }
    do {
      retVal = receiveBit(context->pulseTimeout, &(data->dataPtr[i]), mask);
      if (retVal != SUCCESS) {
        LAST_ERR;
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
    data->dataPtr[fullBytes] = 0x00u;
    if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
      mask = 0x01u;
    } else {
      mask = (0x80u >> skipBits);
    }
    while (bitsInTheLastByte > 0u) {
      retVal =
          receiveBit(context->pulseTimeout, &(data->dataPtr[fullBytes]), mask);
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
  /* stop bits receiving */
  retVal = receiveStartStopBits(context->pulseTimeout, context->startStopBit,
                                context->numStopBits);
  if (retVal != SUCCESS) {
    goto fExit;
  }
fExit:
  htim->Instance->DIER = htim->Instance->DIER & ~(TIM_DIER_UIE);
  configTimer(context->htim, 0u);
  /* Give MUTEX */
  if (mut != 0) {
    /* Decide if rtos has already started */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
      mut = xSemaphoreGive(ManchesterTimer01MutexHandle);
    }
    (void)mut;
  }
  if (retVal == SUCCESS) {
    STROBE_1;
    STROBE_1;
    STROBE_0;
    STROBE_0;
    STROBE_1;
    STROBE_1;
    STROBE_0;
    STROBE_0;
  }
  return retVal;
}

/**
 * @brief sampleAt
 * @param htim
 * @param measured
 * @return
 */
uint8_t sampleAt(TIM_HandleTypeDef *htim, uint32_t timeoutMS, uint32_t tts) {
  BaseType_t xResult;
  uint32_t ulNotifiedValue;
  uint8_t sample;

  htim->Instance->ARR = tts;
  htim->Instance->EGR = TIM_EGR_UG;
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(timeoutMS);
  /*enable timer and wait for flag */
  htim->Instance->SR = 0u;
  htim->Instance->CR1 = htim->Instance->CR1 | TIM_CR1_CEN;
  htim->Instance->DIER = htim->Instance->DIER | TIM_DIER_UIE;
  /* Wait to be notified of an interrupt. */
  xResult = xTaskNotifyWait(pdFALSE,          /* Don't clear bits on entry. */
                            ULONG_MAX,        /* Clear all bits on exit. */
                            &ulNotifiedValue, /* Stores the notified value. */
                            xMaxBlockTime);
  //	STROBE_1;
  if (xResult != pdPASS) {
    sample = UINT8_MAX;
  } else {
    //		STROBE_1;
    sample = (uint8_t)(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0));
    //		STROBE_0;
  }
  htim->Instance->DIER = htim->Instance->DIER & ~(TIM_DIER_UIE);
  htim->Instance->CR1 = htim->Instance->CR1 & ~(TIM_CR1_CEN);
  return sample;
}

/**
 * @brief receiveBit receives a single bit
 * @param htim
 * @param val
 * @param mask
 * @return SUCCESS or ERROR
 */
ErrorStatus receiveBit(uint32_t timeoutMS, uint8_t *val, uint8_t mask) {
  ErrorStatus retVal = SUCCESS;

  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(timeoutMS);
  BaseType_t xResult;
  uint32_t ulNotifiedValue;
  uint8_t p0;
  uint8_t p1;

  /* Wait to be notified of an interrupt. */
  xResult = xTaskNotifyWait(pdFALSE,          /* Don't clear bits on entry. */
                            ULONG_MAX,        /* Clear all bits on exit. */
                            &ulNotifiedValue, /* Stores the notified value. */
                            xMaxBlockTime);
  if (xResult != pdPASS) {
    retVal = ERROR;
    goto fExit;
  }
  /* read GPIO PIN */
  p0 = (uint8_t)(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0));
  /* Wait to be notified of an interrupt. */
  xResult = xTaskNotifyWait(pdFALSE,          /* Don't clear bits on entry. */
                            ULONG_MAX,        /* Clear all bits on exit. */
                            &ulNotifiedValue, /* Stores the notified value. */
                            xMaxBlockTime);
  if (xResult != pdPASS) {
    retVal = ERROR;
    goto fExit;
  }
  /* read GPIO PIN */
  p1 = (uint8_t)(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0));
  uint8_t bit = decodeBit(p0, p1);
  if (bit == UINT8_MAX) {
    retVal = ERROR;
    goto fExit;
  }
  if (bit == 1u) {
    *val |= mask;
  }
fExit:
  return retVal;
}

/**
 * @brief receiveStartStopBits
 * @param htim
 * @param val
 * @param num
 * @return
 */
ErrorStatus receiveStartStopBits(uint32_t timeoutMS, uint8_t val, size_t num) {
  ErrorStatus retVal;
  retVal = SUCCESS;

  while (num > 0u) {
    uint8_t tmp = 0u;
    retVal = receiveBit(timeoutMS, &tmp, 01u);
    if (retVal != SUCCESS) {
      LAST_ERR;
      goto fExit;
    }
    if (val != tmp) {
      retVal = ERROR;
      LAST_ERR;
      goto fExit;
    }
    num--;
  }
fExit:
  return retVal;
}

/**
 * @brief decodeBit decodes a bit
 * @param one
 * @param two
 * @return 0 or 1 or UINT8_MAX in case of error
 */
uint8_t decodeBit(uint8_t one, uint8_t two) {
  if (one == two) {
    return UINT8_MAX;
  }
  return one;
}

/**
 * @brief capturePulse captures pulse width
 * @param htim timer handle
 * @param timeout value in ms
 * @param edge rising or falling
 * @return UINT32_MAX in case of error, value in us otherwize
 * @note timer is already configured
 */
uint32_t capturePulse(TIM_HandleTypeDef *htim, uint32_t timeout, Edge_t edge) {
  uint32_t retVal = UINT32_MAX;
  if (edge == rising) {
    htim->Instance->CCER |= TIM_INPUTCHANNELPOLARITY_RISING;
  } else {
    htim->Instance->CCER |= TIM_INPUTCHANNELPOLARITY_FALLING;
  }

  htim->Instance->CCER |= TIM_CCER_CC1E;
  htim->Instance->SR = 0u;
  htim->Instance->DIER = TIM_DIER_CC1IE;
  htim->Instance->CR1 = TIM_CR1_CEN;
  TickType_t xMaxBlockTime = pdMS_TO_TICKS(timeout);
  BaseType_t xResult = xTaskNotifyWait(pdFALSE, /* Don't clear bits on entry. */
                                       ULONG_MAX, /* Clear all bits on exit. */
                                       &retVal, /* Stores the notified value. */
                                       xMaxBlockTime);
  if (xResult != pdPASS) {
    retVal = 0xFFFFFFFEU;
  }
  htim->Instance->DIER &= ~TIM_DIER_CC1IE;
  htim->Instance->CCER &= ~TIM_CCER_CC1E;
  htim->Instance->CR1 &= ~TIM_CR1_CEN;
  return retVal + PULSE_CORRECTION;
}

/**
 * @brief decideBit decides is measured duration a half-bit, or bit, or bad
 * value
 * @param measured
 * @param context
 * @return UINT8_MAX in case of error, 1 if half-bit, 2 if bit
 */
uint8_t decidePulseWidth(uint32_t measured, MANCHESTER_Context_t *context) {
  uint8_t retVal = UINT8_MAX;
  if ((measured >= context->halfBitMinTime) &&
      (measured <= context->halfBitMaxTime)) {
    retVal = 1u;
  } else if ((measured >= (2u * context->halfBitMinTime)) &&
             (measured <= (2u * context->halfBitMaxTime))) {
    retVal = 2u;
  }
  return retVal;
}

/**
 * @brief configTimer configures the selected timer for
 * @param htim
 * @param stage index of the config. params.table
 */
void configTimer(TIM_HandleTypeDef *htim, size_t stage) {
  if (timerCfgData[stage].state == HAL_TIM_STATE_RESET) {
    // HAL_TIM_Base_MspDeInit(htim);
  } else {
    if (timerCfgData[stage].NeedMSPInit == 1u) {
      HAL_TIM_Base_MspInit(htim);
    }
  }
  /* copied from STM32 Cube code */
  uint32_t tmpsmcr;
  tmpsmcr = htim->Instance->SMCR;
  tmpsmcr &= ~(TIM_SMCR_SMS | TIM_SMCR_TS);
  tmpsmcr &= ~(TIM_SMCR_ETF | TIM_SMCR_ETPS | TIM_SMCR_ECE | TIM_SMCR_ETP);
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
 * @brief sbfr shifts bit field "* field" of "flen" bytes right by "ns" times
 * @param field
 * @param flen length in bytes
 * @param ns
 * @return the actual nr of shifts
 */
size_t sbfr(uint8_t *field, uint32_t flen, size_t ns) {
  ns = (ns > flen * CHAR_BIT) ? flen * CHAR_BIT : ns; // limit number of shifts
  size_t sns;
  sns = ns;
  size_t i;
  uint8_t carry_flag;
  uint8_t next;
  while (ns > 0) {
    carry_flag = 0;
    next = 0;
    for (i = flen; i > 0; i--) {
      next = (field[i - 1] & 0x01) ? 0x80 : 0x00;
      field[i - 1] = (uint8_t)(field[i - 1] >> 1) | carry_flag;
      carry_flag = next;
    }
    ns--;
  };
  return sns;
}

/**
 * @brief swapBits swaps order of bits in byte
 * @param val input byte
 * @return swapped byte
 */
uint8_t swapBits(uint8_t val) {
  uint8_t retVal = 0U;
  uint8_t mask = 0x80U;
  uint8_t tmp = 0x01U;
  for (size_t i = 0; i < CHAR_BIT; i++) {
    if ((val & mask) == mask) {
      retVal = retVal | tmp;
    }
    mask = mask >> 1u;
    tmp = (uint8_t)(tmp << 1u);
  }
  return retVal;
}

/**
 * @brief reverseBitString reverses a bit string
 * @param data pointer to first byte
 * @param len length of the string in bits
 */
void reverseBitString(uint8_t *data, size_t len) {
  size_t fullBytes = len / CHAR_BIT;
  size_t bitsInTheLastByte = len % CHAR_BIT;
  size_t totalBytes = (bitsInTheLastByte > 0u) ? fullBytes + 1u : fullBytes;
  size_t skipBits; /* number of highest bits of the last byte to be skipped */
  skipBits = (bitsInTheLastByte > 0u) ? (CHAR_BIT - bitsInTheLastByte) : 0u;

  size_t steps = totalBytes / 2u;
  size_t i = 0u;
  size_t j = totalBytes;
  while (i < steps) {
    uint8_t tmp = data[i];
    data[i] = swapBits(data[j - 1u]);
    data[j - 1u] = swapBits(tmp);
    i++;
    j--;
  }
  if ((totalBytes % 2u) == 1u) {
    data[i] = swapBits(data[i]);
  }
  // shift bit string rigth if the last source byte has < CHAR_BIT bits
  if (skipBits > 0u) {
    sbfr(data, totalBytes, skipBits);
  }
}

/**
 ************** TRANSMISSION ROUTINES **************************
 */

/**
 * @brief MANCHESTER_Receive
 * @param data pointer to data context
 * @param context pointer to conmmunication context
 * @return SUCCESS or ERROR
 */
ErrorStatus MANCHESTER_Transmit(MANCHESTER_Data_t *data,
                                MANCHESTER_Context_t *context) {
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
    mut = xSemaphoreTake(ManchesterTimer01MutexHandle, portMAX_DELAY);
  }

  /* initialize timer hardware for transmission */
  configTimer(htim, 5u);
  htim->Instance->ARR = context->halfBitTime;
  htim->Instance->EGR = TIM_EGR_UG;

  htim->Instance->SR = 0u;
  htim->Instance->CR1 = htim->Instance->CR1 | TIM_CR1_CEN;
  htim->Instance->DIER = htim->Instance->DIER | TIM_DIER_UIE;

  /* start bits transmission */
  retVal = transmitStartStopBits(context->pulseTimeout, htim,
                                 context->startStopBit, context->numStartBits);
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
      retVal =
          transmitBit(context->pulseTimeout, htim, data->dataPtr[i] & mask);
      if (retVal != SUCCESS) {
        goto fExit;
      }
      if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
        mask = (uint8_t)(mask << 1);
      } else {
        mask = (uint8_t)(mask >> 1);
      }

    } while (mask != 0x00u);
    i++;
  }
  while (bitsInTheLastByte > 0u) {
    if (context->bitOrder == MANCHESTER_BitOrderLSBFirst) {
      mask = 0x01u;
    } else {
      mask = (0x80u >> skipBits);
    }
    retVal = transmitBit(context->pulseTimeout, htim,
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
  /* stop bits transmission */
  retVal = transmitStartStopBits(context->pulseTimeout, htim,
                                 context->startStopBit, context->numStopBits);
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
ErrorStatus transmitStartStopBits(uint32_t timeoutMS, TIM_HandleTypeDef *htim,
                                  uint8_t val, size_t num) {
  ErrorStatus retVal = SUCCESS;
  while ((num > 0u) && (retVal == SUCCESS)) {
    retVal = transmitBit(timeoutMS, htim, val);
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
ErrorStatus transmitBit(uint32_t timeoutMS, TIM_HandleTypeDef *htim,
                        uint8_t val) {
  ErrorStatus retVal = SUCCESS;

  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(timeoutMS);
  BaseType_t xResult;
  uint32_t ulNotifiedValue;

  /* Wait to be notified of an interrupt. */
  xResult = xTaskNotifyWait(pdFALSE,          /* Don't clear bits on entry. */
                            ULONG_MAX,        /* Clear all bits on exit. */
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
    HAL_GPIO_WritePin(MANCHESTER_TX_GPIO_Port, MANCHESTER_TX_Pin, GPIO_PIN_SET);
  }
  /* Wait to be notified of an interrupt. */
  xResult = xTaskNotifyWait(pdFALSE,          /* Don't clear bits on entry. */
                            ULONG_MAX,        /* Clear all bits on exit. */
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
