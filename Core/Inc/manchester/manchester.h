#ifndef MANCHESTER_H
#define MANCHESTER_H

#include "stm32f1xx_hal.h"

//#define MANCHESTER_DEBUG

#ifdef MANCHESTER_DEBUG
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

#define WAIT_FOR_START_MAX_MS 100u
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
 * The stage of comm. process
 */
typedef enum MANCHESTER_RxStage {
	MANCHESTER_Rx_Idle,
	MANCHESTER_Rx_WaitForStart,
	MANCHESTER_Rx_StartEdgeDetected,
	MANCHESTER_Rx_DataReceiving,
	MANCHESTER_Rx_OutOfSync
} MANCHESTER_RxStage_t;

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

extern uint32_t captured1;
extern uint8_t timerEvent;

void MANCHESTER_DebugLEDToggle(void);

ErrorStatus MANCHESTER_InitContext(MANCHESTER_Context_t *context,
				   TIM_HandleTypeDef *htim, size_t numStartBits,
				   size_t numStopBits, size_t bitRate,
				   MANCHESTER_BitOrder_t bitOrder,
				   uint8_t startStopBit);

ErrorStatus MANCHESTER_Receive(MANCHESTER_Data_t *data,
			       MANCHESTER_Context_t *context);
// ErrorStatus MANCHESTER_Receive(TIM_HandleTypeDef *htim, uint8_t *dataPtr,
//			       size_t *pnumBits, size_t numStartStopBits,
//			       size_t bitRate, MANCHESTER_BitOrder_t bitOrder,
//			       uint8_t startStopBit);

ErrorStatus MANCHESTER_Transmit(MANCHESTER_Data_t *data,
				MANCHESTER_Context_t *context);

//ErrorStatus MANCHESTER_Transmit(TIM_HandleTypeDef *htim, uint8_t *dataPtr,
//				const size_t *pnumBits, size_t numStartStopBits,
//				size_t bitRate, MANCHESTER_BitOrder_t bitOrder,
//				uint8_t startStopBit);

#endif // MANCHESTER_H
