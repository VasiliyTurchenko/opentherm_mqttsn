/** @file publish_task.c
*  @brief MQTT_SN publish task
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 10-03-2019
 */

#ifdef MASTERBOARD

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
#include "mqtt_sn.h"

#include "opentherm.h"
#include "opentherm_json.h"

extern const Media_Desc_t Media0;
extern osMutexId ETH_Mutex01Handle;

extern tMV *OPENTHERM_getMV_for_Pub(size_t i);
extern ldid_t OPENTHERM_GetNextMVLD(uint16_t *start_index);

/* all in the one */
static const struct MQTT_parameters MQTT_pub_parameters = {
	{
		.ip_n = { "MQTT" },
		.ip_v = { "192168000001" },
		.ip_p = { "03333" },
		.ip_nl = { "\n" },
	},
	.MQTT_IP_filename = { "MQP_IP\0" },
	{
		.rootTopic = { "tvv/5413/in-home/1st_floor/boiler/\0" },
		.topicText = { "LD_ID:000\0" },
		.pub_client_id_string = { "BOILER-PUB\0" },
	},
	.MQTT_topic_filename = { "MQP_TO\0" },
	.long_taskName = "publish_task",
	.short_taskName = "MQTT-SN PUB ",
};

/* runtime config data */
static struct MQTT_topic_para MQTT_pub_working_set = {
	.rootTopic = { '\0' },
	.topicText = { '\0' },
	.pub_client_id_string = { '\0' },
};

/* configuration pools */
static cfg_pool_t MQP_IP_cfg = {
	{ .ip = 0U, .port = 0U },
	&MQTT_pub_parameters.MQTT_IP_parameters,
	(const char *)&MQTT_pub_parameters.MQTT_IP_filename,
};

/**
 * @brief publish_task_init
 */
void publish_task_init(void)
{
	register_magic(PUB_TASK_MAGIC);

	mqtt_initialize(PUB_TASK_MAGIC, &MQTT_pub_parameters,
			&MQTT_pub_working_set, &MQP_IP_cfg);

	/* got here - initialize context */
}

/**
 * @brief publish_task_run
 */
void publish_task_run(void)
{
	i_am_alive(PUB_TASK_MAGIC);

	while (lan_up() != 1U) {
		osDelay(pdMS_TO_TICKS(200U));
	}

/* 1.	initialize pub context */
	static uint32_t conn_attempts = 0U;
	while (mqtt_sn_init_context(&mqttsncontext01, &MQTT_pub_working_set,
				    &MQP_IP_cfg) != SUCCESS) {
#ifdef MQTT_SN_PUB_DEBUG_PRINT
		xputs("initializing publish context...\n");
#endif
		mqtt_sn_deinit_context(&mqttsncontext01);
		osDelay(200U);
		/* watchdog reboots in case of many unsuccessful inits*/
	}

/* 2.	context initialized, connect  */
	while (mqttsncontext01.state != CONNECTED) {
		conn_attempts++; /* increment attempts counter */
#ifdef MQTT_SN_PUB_DEBUG_PRINT
		xprintf("publish connection attempt %d\n", conn_attempts);
#endif
		if (mqtt_sn_connect(&mqttsncontext01) == SUCCESS) {
			break;
		}
		i_am_alive(PUB_TASK_MAGIC);
		osDelay(200U); /* wait 200ms and try again */
		HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
	}

/* 3.	Now we have to register topics */
	ErrorStatus regresult = ERROR;
	mqttsncontext01.currPubSubMV = 0U; /* RESET list */
	do {
		/* request the LD to be registered */
		ldid_t ld_id;
		ld_id = OPENTHERM_GetNextMVLD(&mqttsncontext01.currPubSubMV);
		if (mqttsncontext01.currPubSubMV == 0xFFFF) {
			/* end of array reached */
			break;
		}
#ifdef MQTT_SN_PUB_DEBUG_PRINT
		xprintf("registering LDID:%d\n", nextLD);
#endif
		regresult = mqtt_sn_register_topic(&mqttsncontext01,
						   ld_id);
		if (regresult == ERROR) {
			break;
		} /* exit the loop with regresult = error */
		mqttsncontext01.currPubSubMV++;
		i_am_alive(PUB_TASK_MAGIC);
		HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
		osDelay(50U);
	} while (1);

	/* here all the topics are registered */
/* 4.	start the periodic part of the task */
	TickType_t xLastWakeTime;
	TickType_t xPeriod = pdMS_TO_TICKS(200U);
	xLastWakeTime = xTaskGetTickCount(); // get value only once!
	/* Infinite loop - internal */

	if (regresult == SUCCESS) {
		ErrorStatus publishresult = ERROR;
		for (;;) {
			/* iterate over MV list */
			for (size_t i = 0U; i < MV_ARRAY_LENGTH; i++) {
				/* get MV */

				tMV *pMV = OPENTHERM_getMV_for_Pub(
					i); /* mutex lock !*/

				if (pMV == NULL) {
					/* this MV is not suitable for publishing */
					continue;
				}

				/* convert MV to JSON */
				char *pjson;
				pjson = (char *)ConvertMVToJSON(pMV);

				/* publish JSON */
				publishresult = mqtt_sn_publish_topic(
					&mqttsncontext01, pMV->TopicId, pjson);
				if (publishresult == ERROR) {
					break;
				} /* break the internal infinite loop*/
				i_am_alive(PUB_TASK_MAGIC);
				HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port,
						   GREEN_LED_Pin);
				vTaskDelayUntil(&xLastWakeTime, xPeriod);
			}

			/* what to do if publish result != SUCCESS */
			if (publishresult == ERROR) {
				break;
			}
		}
	}
/* 5.	release sockets */
	ErrorStatus deinitresult;
	deinitresult = mqtt_sn_deinit_context(&mqttsncontext01);
	if (deinitresult == ERROR) {
#ifdef MQTT_SN_PUB_DEBUG_PRINT
		xputs("mqtt_sn_deinit_context(pub_context) error!\n");
#endif
		/* need reboot */
		for (;;) {
			/* LOCK */
		}
	}
}

#elif SLAVEBOARD

#include "main.h"
#include "task_tokens.h"
#include "watchdog.h"

/**
 * @brief publish_task_init
 */
void publish_task_init(void)
{
	register_magic(PUB_TASK_MAGIC);
}

/**
 * @brief publish_task_run
 */
void publish_task_run(void)
{
	i_am_alive(PUB_TASK_MAGIC);
	HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);

	osDelay(100U);
	HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);

	osDelay(100U);
}

#else
#error Neither MASTERBOARD nor SLAVEBOARD defined!
#endif
