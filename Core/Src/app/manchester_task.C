/** @file manchester_task.c
 *  @brief manchester RX-TX routines
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-03-2019
 */

#include "tim.h"
#include "logging.h"
#include "my_comm.h"
#include "watchdog.h"
#include "task_tokens.h"

#include "messages.h"

#include "manchester.h"
#include "manchester_task.h"
#include "debug_settings.h"

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

extern TaskHandle_t TaskToNotify_afterRx;
extern TaskHandle_t TaskToNotify_afterTx;

#if (0)
/* What to suspend */
extern osThreadId LANPollTaskHandle;

extern osThreadId PublishTaskHandle;

extern osThreadId DiagPrTaskHandle;

extern osThreadId ProcSPSTaskHandle;

extern osThreadId SubscrbTaskHandle;

extern osThreadId ServiceTaskHandle;

static osThreadId * taskHandles [] = {&LANPollTaskHandle, &PublishTaskHandle, &DiagPrTaskHandle,
					&ProcSPSTaskHandle, &SubscrbTaskHandle, &ServiceTaskHandle };
static const size_t taskHandlesQty = sizeof (taskHandles) / sizeof (taskHandles[0]);




/**
 * @brief suspendAll suspends all the tasks
 */
static void suspendAll(void)
{
	for (size_t i = 0U; i < taskHandlesQty; i++) {
		vTaskSuspend(*taskHandles[i]);
	}
}

static void resumeAll(void)
{
	for (size_t i = 0U; i < taskHandlesQty; i++) {
		vTaskResume(*taskHandles[i]);
	}
}
#endif

/**
 * @brief manchester_task_init
 */
void manchester_task_init(void)
{
	register_magic(MANCHESTER_TASK_MAGIC);


	messages_TaskInit_started();

	ErrorStatus res;
	res = MANCHESTER_InitContext(&manchester_context, &htim2, startBits,
				     stopBits, bitRate, bitOrder, startStopBit);
	if (res == ERROR) {
		messages_TaskInit_fail();
		vTaskDelay(pdMS_TO_TICKS(portMAX_DELAY));
	} else {
		messages_TaskInit_OK();
	}
	return;
}

/**
 * @brief manchester_task_run
 */
void manchester_task_run(void)
{
	i_am_alive(MANCHESTER_TASK_MAGIC);
	uint32_t notif_val = 0U;

	ErrorStatus result = ERROR;


	if (xTaskNotifyWait(ULONG_MAX, ULONG_MAX, &notif_val, pdMS_TO_TICKS(1000U)) ==
	    pdTRUE) {

		if (notif_val == MANCHESTER_TRANSMIT_NOTIFY) {
			manchester_Tx_data.dataPtr = Tx_buf;
			manchester_Tx_data.numBits = 4 * CHAR_BIT;
			manchester_Tx_data.numBitsActual = 0U;


			result = MANCHESTER_Transmit(&manchester_Tx_data, &manchester_context);

#if (MANCH_TASK_DEBUG_PRINT == 1)
			log_xputs(MSG_LEVEL_INFO, " TX>>\n");
#endif
			xTaskNotify(TaskToNotify_afterTx, (uint32_t)result ,eSetValueWithOverwrite);

		} else if (notif_val == MANCHESTER_RECEIVE_NOTIFY) {

			manchester_Rx_data.dataPtr = Rx_buf;
			manchester_Rx_data.numBits = 4 * CHAR_BIT;
			manchester_Rx_data.numBitsActual = 0U;

			result = MANCHESTER_Receive(&manchester_Rx_data, &manchester_context);

			xTaskNotify(TaskToNotify_afterRx, (uint32_t)result, eSetValueWithOverwrite);

		} else {
#if (MANCH_TASK_DEBUG_PRINT == 1)
			log_xprintf(MSG_LEVEL_PROC_ERR, " bad notif. value %d\n", notif_val);
#endif
		}
	} else {
#if (MANCH_TASK_DEBUG_PRINT == 1)
		log_xputs(MSG_LEVEL_INFO, " is idle\n");
#endif
	}
}

