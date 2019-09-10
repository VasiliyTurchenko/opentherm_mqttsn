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

#include "mqtt_sn.h"
#include "mqtt_config_helper.h"

#include "manchester_task.h"

//#include "MQTT_SN_task.h"

extern const Media_Desc_t Media0;

extern ldid_t OPENTHERM_GetNextMV_CMD(uint16_t *start_index);

//extern osThreadId SubscrbTaskHandle;

//TaskHandle_t TaskToNotify_afterTx;
//TaskHandle_t TaskToNotify_afterRx;

//extern osThreadId ManchTaskHandle;

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
//	TaskToNotify_afterTx = SubscrbTaskHandle;
//	TaskToNotify_afterRx = SubscrbTaskHandle;

/* code for slave */
#ifdef SLAVEBOARD

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
	i_am_alive(SUB_TASK_MAGIC);

	while (lan_up() != 1U) {
		osDelay(pdMS_TO_TICKS(200U));
	}

/* 1.	initialize sub context */
	static uint32_t conn_attempts = 0U;
	while (mqtt_sn_init_context(&mqttsncontext02, &MQTT_sub_working_set,
				    &MQS_IP_cfg) != SUCCESS) {
#ifdef MQTT_SN_SUB_DEBUG_PRINT
		xputs("initializing subscribe context...\n");
#endif
		mqtt_sn_deinit_context(&mqttsncontext02);
		osDelay(200U);
		/* watchdog reboots in case of many unsuccessful inits*/
	}
/* 2.	context initialized, connect  */
	while (mqttsncontext02.state != CONNECTED) {
		conn_attempts++; /* increment attempts counter */
#ifdef MQTT_SN_SUB_DEBUG_PRINT
		xprintf("subscribe connection attempt %d\n", conn_attempts);
#endif
		if (mqtt_sn_connect(&mqttsncontext02) == SUCCESS) {
			break;
		}
		i_am_alive(SUB_TASK_MAGIC);
		osDelay(200U); /* wait 200ms and try again */
		HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
	}
/* 3.	Now we have to subscribe to the topics */
	ErrorStatus subresult = ERROR;
	mqttsncontext02.currPubSubMV = 0U; /* RESET list */
	do {
		/* request the LD to be subscribed */
		ldid_t ld_id;
		ld_id = OPENTHERM_GetNextMV_CMD(&mqttsncontext02.currPubSubMV);

		if (mqttsncontext02.currPubSubMV == 0xFFFF) {
			/* end of array reached */
			break;
		}
#ifdef MQTT_SN_SUB_DEBUG_PRINT
		xprintf("registering LDID:%d\n", nextLD);
#endif
		subresult = mqtt_sn_subscribe_topic(&mqttsncontext02,
						   ld_id);
		if (subresult == ERROR) {
			break;
		} /* exit the loop with regresult = error */

		mqttsncontext02.currPubSubMV++;
		i_am_alive(SUB_TASK_MAGIC);
		HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
		osDelay(50U);
	} while (1);

	/* here we've subscribed to all the topics */
/* 4.	start the periodic part of the task */
	TickType_t xLastWakeTime;
	TickType_t xPeriod = pdMS_TO_TICKS(200U);
	xLastWakeTime = xTaskGetTickCount(); // get value only once!
	/* Infinite loop - internal */

	if (subresult == SUCCESS) {
		ErrorStatus publishresult = ERROR;
		mqtt_sn_reset_last_packid(&mqttsncontext02);
		for (;;) {
			publishresult = mqtt_sn_poll_subscribed(&mqttsncontext02);
			if (publishresult == ERROR) { break; }		/* break the internal infinite loop*/
			i_am_alive(SUB_TASK_MAGIC);
			vTaskDelayUntil(&xLastWakeTime, xPeriod);	/* wake up every 50ms! */
		}
	}
/* 5.	release sockets */
	ErrorStatus deinitresult;
	deinitresult = mqtt_sn_deinit_context(&mqttsncontext02);
	if (deinitresult == ERROR) {
#ifdef MQTT_SN_SUB_DEBUG_PRINT
		xputs("mqtt_sn_deinit_context(sub_context) error!\n");
#endif
		/* need reboot */
		for (;;) {
			/* LOCK */
		}
	}
} /* of subscribe_task_run(void) */

#endif

#if defined(SLAVEBOARD)

void subscribe_task_run(void)
{
//	static uint32_t data = 0U;

//	uint32_t notif_val = 0U;

	i_am_alive(SUB_TASK_MAGIC);

//	*(uint32_t *)(&Rx_buf[0]) = 0U;

//	xTaskNotifyStateClear(ManchTaskHandle);

//	xTaskNotify(ManchTaskHandle, MANCHESTER_RECEIVE_NOTIFY,
//		    eSetValueWithOverwrite);

//	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
//			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
//		if (notif_val == (ErrorStatus)ERROR) {
//			//			xputs("manch. rx ERR\n");
//			goto fExit;

//		} else {
//			xputs("\nsub task: notif. rx OK. ");
//			xprintf("Received: %d\n", *(uint32_t *)(&Rx_buf[0]));
//		}
//	} else {
//		goto fExit;
//	}

//	*(uint32_t *)(&Tx_buf[0]) = *(uint32_t *)(&Rx_buf[0]);

//	vTaskDelay(pdMS_TO_TICKS(500U));

//	xTaskNotifyStateClear(ManchTaskHandle);

//	taskENTER_CRITICAL();
//	xTaskNotify(ManchTaskHandle, MANCHESTER_TRANSMIT_NOTIFY,
//		    eSetValueWithOverwrite);
//	taskEXIT_CRITICAL();
//	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
//			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
//		xputs("sub task: notif. tx ");
//		if (notif_val == (ErrorStatus)ERROR) {
//			xputs("ERR\n");
//		} else {
//			xputs("OK\n");
//			data++;
//		}
//	}

//fExit:
//	return;
}

#endif
