/** @file subscribe_task.c
*  @brief MQTT_SN subscribe task
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 10-03-2019
 */

#include <limits.h>

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

#include "mqtt_config_helper.h"

#include "manchester_task.h"


#include "opentherm_wrappers.h"

//#include "MQTT_SN_task.h"

extern const Media_Desc_t Media0;

extern osThreadId SubscrbTaskHandle;

TaskHandle_t TaskToNotify_afterTx;
TaskHandle_t TaskToNotify_afterRx;

extern osThreadId ManchTaskHandle;

/* all in the one */
static const struct MQTT_parameters MQTT_sub_parameters = {
	{
		.ip_n = { "MQTT" },
		.ip_v = { "192168000001" },
		.ip_p = { "03333" },
		.ip_nl = { "\n" },
	},
	.MQTT_IP_filename = { "MQS_IP\0" },
	{
		.rootTopic = { "tvv/5413/in-home/1st_floor/boiler/\0" },
		.topicText = { "CMD:000\0" },
		.pub_client_id_string = { "BOILER-SUB\0" },
	},
	.MQTT_topic_filename = { "MQS_TO\0" },
	.long_taskName = "subscribe_task",
	.short_taskName = "MQTT-SN SUB ",
};

/* runtime config data */
static struct MQTT_topic_para MQTT_sub_working_set = {
	.rootTopic = { '\0' },
	.topicText = { '\0' },
	.pub_client_id_string = { '\0' },
};

/* configuration pools */
static cfg_pool_t MQS_IP_cfg = {
	{ .ip = 0U, .port = 0U },
	&MQTT_sub_parameters.MQTT_IP_parameters,
	(const char *)&MQTT_sub_parameters.MQTT_IP_filename,
};

/**
 * @brief subscribe_task_init
 */
void subscribe_task_init(void)
{
	register_magic(SUB_TASK_MAGIC);

	mqtt_initialize(SUB_TASK_MAGIC, &MQTT_sub_parameters,
			&MQTT_sub_working_set, &MQS_IP_cfg);

	/* got here - initialize context */
	TaskToNotify_afterTx = SubscrbTaskHandle;
	TaskToNotify_afterRx = SubscrbTaskHandle;

/* code for slave */
#ifdef SLAVEBOARD

	xputs("OpenTherm initialization ");
	if (OPENTHERM_InitSlave() != OPENTHERM_ResOK) {
		xputs("error!\n");
	} else {
		xputs("OK.\n");
	}

/* code for master */
#elif defined(MASTERBOARD)

#else
#error NEITHER SLAVEBOARD NOR MASTERBOARD IS DEFINED!
#endif
}

#if defined(MASTERBOARD)
/**
 * @brief subscribe_task_run
 */
void subscribe_task_run(void)
{
	static uint32_t data = 0U;
	static uint32_t notif_val = 0U;

	i_am_alive(SUB_TASK_MAGIC);

	*(uint32_t *)(&Tx_buf[0]) = data;

	xTaskNotify(ManchTaskHandle, MANCHESTER_TRANSMIT_NOTIFY,
		    eSetValueWithOverwrite);

	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		xputs("sub task: got notification tx ");
		if (notif_val == 0U) {
			xputs("ERR\n");
		} else {
			xputs("OK\n");
			data++;
		}
	}

	vTaskDelay(pdMS_TO_TICKS(20U));

	*(uint32_t *)(&Rx_buf[0]) = 0U;

	xTaskNotify(ManchTaskHandle, MANCHESTER_RECEIVE_NOTIFY,
		    eSetValueWithOverwrite);

	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		xputs("sub task:notif. rx");
		if (notif_val == 0U) {
			xputs(" ERR\n");
			vTaskDelay(pdMS_TO_TICKS(800U));

		} else {
			xputs(" OK. ");
			xprintf("Received: %d\n", *(uint32_t *)(&Rx_buf[0]));
		}
	}
	vTaskDelay(pdMS_TO_TICKS(200U));
}

#endif

#if defined(SLAVEBOARD)

void subscribe_task_run(void)
{
	static uint32_t data = 0U;

	uint32_t notif_val = 0U;

	i_am_alive(SUB_TASK_MAGIC);

	*(uint32_t *)(&Rx_buf[0]) = 0U;

	xTaskNotifyStateClear(ManchTaskHandle);

	xTaskNotify(ManchTaskHandle, MANCHESTER_RECEIVE_NOTIFY,
		    eSetValueWithOverwrite);

	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		if (notif_val == 0U) {
			//			xputs("manch. rx ERR\n");
			goto fExit;

		} else {
			xputs("\nsub task: notif. rx OK. ");
			xprintf("Received: %d\n", *(uint32_t *)(&Rx_buf[0]));
		}
	} else {
		goto fExit;
	}

	*(uint32_t *)(&Tx_buf[0]) = *(uint32_t *)(&Rx_buf[0]);

	vTaskDelay(pdMS_TO_TICKS(500U));

	xTaskNotifyStateClear(ManchTaskHandle);

	taskENTER_CRITICAL();
	xTaskNotify(ManchTaskHandle, MANCHESTER_TRANSMIT_NOTIFY,
		    eSetValueWithOverwrite);
	taskEXIT_CRITICAL();
	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		xputs("sub task: notif. tx ");
		if (notif_val == 0U) {
			xputs("ERR\n");
		} else {
			xputs("OK\n");
			data++;
		}
	}

fExit:
	return;
}

#endif
