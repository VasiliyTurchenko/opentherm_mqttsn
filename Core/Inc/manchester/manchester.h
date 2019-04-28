#ifndef MANCHESTER_H
#define MANCHESTER_H

#ifdef STM32F303xC
	#include "stm32f3xx_hal.h"
#elif STM32F103xB
	#include "stm32f1xx_hal.h"
#endif

#define MANCHESTER_DEBUG

#ifdef MANCHESTER_DEBUG

#define TX_RX_STROBE_Pin GPIO_PIN_4
#define TX_RX_STROBE_GPIO_Port GPIOA

#define STROBE_1                                                               \
	do {                                                                   \
		HAL_GPIO_WritePin(TX_RX_STROBE_GPIO_Port, TX_RX_STROBE_Pin,    \
				  GPIO_PIN_SET);                               \
	} while (0)

#define STROBE_0                                                               \
	do {                                                                   \
		HAL_GPIO_WritePin(TX_RX_STROBE_GPIO_Port, TX_RX_STROBE_Pin,    \
				  GPIO_PIN_RESET);                             \
	} while (0)
#else
#define STROBE_1
#define STROBE_0
#endif


/* reseved area for bits-to-code and back conversion */
/* size in bytes */
//#define MANCHESTER_DTA_SIZE

#define WAIT_FOR_START_MAX_MS (800u - 20u)
#define HALF_BYTE_TOLERANCE 12u /* 1/12 = 8.333%*/

#define MAX_NUM_START_BITS 16
#define MAX_NUM_STOP_BITS 16

/**
 * Bit order LSB or MSB
 */
typedef enum MANCHESTER_BitOrder {
	MANCHESTER_BitOrderLSBFirst,
	MANCHESTER_BitOrderMSBFirst,
} MANCHESTER_BitOrder_t;

/**
 * Context structure for MANCHESTER functions
 */
typedef struct MANCHESTER_Context {
	TIM_HandleTypeDef *htim; /* link to the timer used for communication */
	size_t numStartBits;     /* number of start bits */
	size_t numStopBits;      /* number of stop bits */
	size_t bitRate;		 /* bits per second */
	MANCHESTER_BitOrder_t bitOrder; /* msb or lsb */
	uint8_t startStopBit;		/* 1 or 0 */
	/* calculated data after initialization */
	uint32_t halfBitTime; /* nominal half-bit duration, ms */
	uint32_t halfBitMinTime;
	uint32_t halfBitMaxTime;
	uint32_t filterTime; /* input pin filtering time, us */
	uint32_t pulseTimeout;	/* pulse capture timeout, ms */
	/* run-time data */
	uint32_t t0; /* the moment of rising edge has come */
} MANCHESTER_Context_t;

/**
 * Context structure for MANCHESTER code communication
 */
typedef struct MANCHESTER_Data {
	uint8_t *dataPtr;     /* pointer to data buffer */
	size_t numBits;       /* requested number of bits , number >= 8 */
	size_t numBitsActual; /* actual number of received/transmitted bits */
//	ErrorStatus lastError;
} MANCHESTER_Data_t;

/**
 * Pulse measurement
 */
typedef struct PulseData {
	uint32_t	low;	/* channel1 - capture falling */
	uint32_t	high;	/* channel1 - capture rising */
	uint8_t		hasLow1T; /* if set, we have 1T low half-bit from prev. pulse */
} PulseData_t;

/* pointer to the high 16 bit counter value - incremented every UIE interrupt */
extern uint16_t highCntValue;

/* flaf for UIE interrupt handler */
extern uint8_t RecvState;

void MANCHESTER_DebugLED8Toggle(void);

ErrorStatus MANCHESTER_InitContext(MANCHESTER_Context_t *context,
				   TIM_HandleTypeDef *htim, size_t numStartBits,
				   size_t numStopBits, size_t bitRate,
				   MANCHESTER_BitOrder_t bitOrder,
				   uint8_t startStopBit);

ErrorStatus MANCHESTER_Receive(MANCHESTER_Data_t *data,
			       MANCHESTER_Context_t *context);

ErrorStatus MANCHESTER_Transmit(MANCHESTER_Data_t *data,
				MANCHESTER_Context_t *context);

void MANCHESTER_TimerISR(void);

#endif // MANCHESTER_H
