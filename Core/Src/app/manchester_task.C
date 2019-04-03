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

#include "messages.h"

#include "manchester.h"
#include "manchester_task.h"

/* manchester configuration */
static const size_t startBits = 1U;
static const size_t stopBits = 1U;
static const size_t bitRate = 1000U;
static const uint8_t startStopBit = 1U;
static const MANCHESTER_BitOrder_t bitOrder = MANCHESTER_BitOrderMSBFirst;

static MANCHESTER_Context_t manchester_context;
static MANCHESTER_Data_t manchester_Rx_data;
static MANCHESTER_Data_t manchester_Tx_data;

uint8_t Rx_buf[4];
uint8_t Tx_buf[4];

/*extern */TaskHandle_t TaskToNotify_afterRx;
/*extern */TaskHandle_t TaskToNotify_afterTx;

static char *task_name = "manchester_task";

/**
 * @brief manchester_task_init
 */
void manchester_task_init(void)
{
	register_magic(MANCHESTER_TASK_MAGIC);


	messages_TaskInit_started(task_name);

	ErrorStatus res;
	res = MANCHESTER_InitContext(&manchester_context, &htim4, startBits,
				     stopBits, bitRate, bitOrder, startStopBit);
	if (res == ERROR) {
		messages_TaskInit_fail(task_name);
		while (1) {
			;
		}
	} else {
		messages_TaskInit_OK(task_name);
	}
	return;
}

/**
 * @brief manchester_task_run
 */
void manchester_task_run(void)
{
	i_am_alive(MANCHESTER_TASK_MAGIC);
	static uint32_t notif_val = 0U;

	ErrorStatus result = ERROR;


	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val, pdMS_TO_TICKS(1000U)) ==
	    pdTRUE) {
		if (notif_val == MANCHESTER_TRANSMIT_NOTIFY) {
			manchester_Tx_data.dataPtr = Tx_buf;
			manchester_Tx_data.numBits = 4 * CHAR_BIT;
			manchester_Tx_data.numBitsActual = 0U;

			result = MANCHESTER_Transmit(&manchester_Tx_data, &manchester_context);

			xputs(task_name);
			xputs(" transmits\n");

			xTaskNotify(TaskToNotify_afterTx, (uint32_t)result ,eSetValueWithOverwrite);

		} else if (notif_val == MANCHESTER_RECEIVE_NOTIFY) {

			manchester_Rx_data.dataPtr = Rx_buf;
			manchester_Rx_data.numBits = 4 * CHAR_BIT;
			manchester_Rx_data.numBitsActual = 0U;

			result = MANCHESTER_Receive(&manchester_Rx_data, &manchester_context);

			xputs(task_name);
			xputs(" receives\n");

			xTaskNotify(TaskToNotify_afterRx, (uint32_t)result, eSetValueWithOverwrite);

		} else {
			xputs(task_name);
			xputs(" got bad notification value\n");
		}
	} else {
		xputs(task_name);
		xputs(" is idle\n");
	}
}
