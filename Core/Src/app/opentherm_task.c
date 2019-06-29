/** @file opentherm_task.c
*  @brief opentherm task
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 29-06-2019
 */

#include "watchdog.h"
#include "lan.h"
#include "xprintf.h"
#include "task_tokens.h"

#include "config_files.h"
#include "tiny-fs.h"
#include "file_io.h"
#include "ascii_helpers.h"
#include "ip_helpers.h"
#include "messages.h"

#include "opentherm_task.h"
#include "opentherm.h"

#include "manchester_task.h"

#ifdef MASTERBOARD
/* to do not include opentherm_master.h */
extern tMV * OPENTHERM_getMV(size_t i);
extern openThermResult_t OPENTHERM_InitMaster(void);
extern openThermResult_t OPENTHERM_ReadSlave(tMV *const pMV,
				      uint32_t (*commFun)(uint32_t));
#elif SLAVEBOARD
extern tMV * OPENTHERM_getMV(size_t i);
extern openThermResult_t OPENTHERM_InitSlave(void);
extern openThermResult_t OPENTHERM_SlaveRespond(uint32_t msg,
					 uint32_t (*commFun)(uint32_t),
					 tMV *(*GetMV)(uint8_t));

#else
#error MASTERBOARD or SLAVEBOARD not defined
#endif

/* messages */
#ifdef MASTERBOARD
static char *task_name = "opentherm_task (master)";
#elif SLAVEBOARD
static char *task_name = "opentherm_task (slave)";
#else

#endif

extern osThreadId OpenThermTaskHandle;
TaskHandle_t TaskToNotify_afterTx = NULL;
TaskHandle_t TaskToNotify_afterRx = NULL;
extern osThreadId ManchTaskHandle;

/**
 * @brief comm_func function used by opentherm to communicate with the slave
 * @param val the value to be sent to the slave
 * @return value received from the slave
 */
static uint32_t comm_func(uint32_t val)
{
	static uint32_t notif_val = 0U;
	uint32_t retVal = val;

	*(uint32_t *)(&Tx_buf[0]) = val;

	xTaskNotify(ManchTaskHandle, MANCHESTER_TRANSMIT_NOTIFY,
		    eSetValueWithOverwrite);

	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		xputs("opentherm_task comm: got notification tx ");
		if (notif_val == (ErrorStatus)ERROR) {
			xputs("ERR\n");
		} else {
			xputs("OK\n");
		}
	}

	vTaskDelay(pdMS_TO_TICKS(20U));

	*(uint32_t *)(&Rx_buf[0]) = 0U;

	xTaskNotify(ManchTaskHandle, MANCHESTER_RECEIVE_NOTIFY,
		    eSetValueWithOverwrite);

	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		xputs("opentherm_task comm: notif. rx");
		if (notif_val == (ErrorStatus)ERROR) {
			xputs(" ERR\n");
			retVal = 0U;

		} else {
			xputs(" OK. ");
			retVal = *(uint32_t *)(&Rx_buf[0]);
			xprintf("Received: %d\n", retVal);
		}
	}
	return retVal;
}

/**
 * @brief opentherm_task_init initializes opentherm driver
 */
void opentherm_task_init(void)
{
	register_magic(OPENTHERM_TASK_MAGIC);
	i_am_alive(OPENTHERM_TASK_MAGIC);
	messages_TaskInit_started(task_name);
#ifdef MASTERBOARD
	if (OPENTHERM_InitMaster() != OPENTHERM_ResOK) {
#elif SLAVEBOARD
	if (OPENTHERM_InitSlave() != OPENTHERM_ResOK) {
#else
#endif
		messages_TaskInit_fail(task_name);
		vTaskDelay(pdMS_TO_TICKS(portMAX_DELAY));
	} else {
		messages_TaskInit_OK(task_name);
		TaskToNotify_afterTx = OpenThermTaskHandle;
		TaskToNotify_afterRx = OpenThermTaskHandle;
	}
}

#ifdef MASTERBOARD
/**
 * @brief opentherm_task_run task function
 */
void opentherm_task_run(void)
{
	/* let's start */
	openThermResult_t	res;
	for (size_t i = 0U; i < MV_ARRAY_LENGTH; i++) {
	i_am_alive(OPENTHERM_TASK_MAGIC);
		tMV *pMV = OPENTHERM_getMV(i);
		if (pMV == NULL) {
			xputs("ERROR: pMV == NULL\n");
			continue;
		}
		res = OPENTHERM_ReadSlave(pMV, comm_func);
		xputs(openThermErrorStr(res));
		xputs("\n");
	}
}

#elif SLAVEBOARD
void opentherm_task_run(void)
{
	i_am_alive(OPENTHERM_TASK_MAGIC);
}
#else
#endif
