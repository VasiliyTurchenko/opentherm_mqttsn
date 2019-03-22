/** @file manchester_task.c
 *  @brief manchester RX-TX routines
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-03-2019
 */

#include "tim.h"
#include "xprintf.h"
#include "my_comm.h"
#include "watchdog.h"
#include "task_tokens.h"

#include "manchester.h"
#include "manchester_task.h"


/* manchester configuration */
static const size_t startBits = 1U;
static const size_t stopBits = 1U;
static const size_t bitRate = 1000U;
static const uint8_t startStopBit  = 1U;
static const MANCHESTER_BitOrder_t bitOrder = MANCHESTER_BitOrderMSBFirst;

static MANCHESTER_Context_t manchester_context;
static MANCHESTER_Data_t manchester_Rx_data;
static MANCHESTER_Data_t manchester_Tx_data;

/**
 * @brief manchester_task_init
 */
void manchester_task_init(void)
{
	register_magic(MANCHESTER_TASK_MAGIC);
	xputs("Initializing manchester context...\n");
//	ErrorStatus MANCHESTER_InitContext(MANCHESTER_Context_t *context,
//				   TIM_HandleTypeDef *htim, size_t numStartBits,
//				   size_t numStopBits, size_t bitRate,
//				   MANCHESTER_BitOrder_t bitOrder,
//				   uint8_t startStopBit);
	ErrorStatus res;
	res = MANCHESTER_InitContext(&manchester_context,
					&htim4,
					startBits,
					stopBits,
					bitRate,
					bitOrder,
					startStopBit);
	if (res == ERROR) {
		xputs("Manchester context init error!\n");
		while (1) {
			;
		}
	} else {
		xputs("Manchester context init OK\n");
	}
	return;
}


/**
 * @brief manchester_task_run
 */
void manchester_task_run(void)
{
	i_am_alive(MANCHESTER_TASK_MAGIC);
}
