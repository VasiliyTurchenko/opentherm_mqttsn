/** @file publish_task.c
*  @brief MQTT_SN publish task
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 10-03-2019
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

#include "MQTT_SN_task.h"

extern const Media_Desc_t Media0;

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

static const char *ip_string = "IP";
static const char *topic_string = "topic";

/**
 * @brief publish_task_init
 */
void publish_task_init(void)
{
	register_magic(PUB_TASK_MAGIC);
	i_am_alive(PUB_TASK_MAGIC);
	messages_TaskInit_started(MQTT_pub_parameters.long_taskName);

	FRESULT res;
	bool need_reboot = false;

	res = ReadIPConfigFileNew(&Media0, &MQP_IP_cfg);
	xputs(MQTT_pub_parameters.short_taskName);
	xputs(ip_string);
	xputs(params_load);
	if (res == FR_OK) {
		xputs(params_loaded);
	} else {
		xputs(params_not_loaded);

		res = DeleteFile(&Media0, MQP_IP_cfg.file_name);
		res = SaveIPConfigFileNew(&Media0, &MQP_IP_cfg);
		if (res != FR_OK) {
			xputs(error_saving);
			xputs(MQTT_pub_parameters.short_taskName);
			xputs(cfg_file);
		}
		need_reboot = true;
	}

	i_am_alive(PUB_TASK_MAGIC);

	const size_t btr = sizeof(struct MQTT_topic_para);
	size_t br = 0U;

	res = ReadBytes(&Media0,
			(const char *)&MQTT_pub_parameters.MQTT_topic_filename,
			0U, btr, &br, (uint8_t *)&MQTT_pub_working_set);
	xputs(MQTT_pub_parameters.short_taskName);
	xputs(topic_string);
	xputs(params_load);
	if ((res == FR_OK) && (btr == br)) {
		xputs(params_loaded);
	} else {
		xputs(params_not_loaded);
		res = DeleteFile(
			&Media0,
			(const char *)&MQTT_pub_parameters.MQTT_topic_filename);
		res = WriteBytes(
			&Media0,
			(const char *)&MQTT_pub_parameters.MQTT_topic_filename,
			0U, btr, &br,
			(const uint8_t *)&MQTT_pub_parameters
				.MQTT_topic_parameters);

		if ((res != FR_OK) || (br != btr)) {
			xputs(error_saving);
			xputs(MQTT_pub_parameters.short_taskName);
			xputs(cfg_file);
		}
		need_reboot = true;
	}

	if (need_reboot) {
		messages_TaskInit_fail(MQTT_pub_parameters.long_taskName);
		vTaskDelay(pdMS_TO_TICKS(500U));
	} else {
		messages_TaskInit_OK(MQTT_pub_parameters.long_taskName);
		/* here call MQTT_SN init context */
	}
}

/**
 * @brief publish_task_run
 */
void publish_task_run(void)
{
	i_am_alive(PUB_TASK_MAGIC);
	HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
	HAL_GPIO_TogglePin(ESP_PWR_GPIO_Port, ESP_PWR_Pin);
	osDelay(100U);
	HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
	HAL_GPIO_TogglePin(ESP_PWR_GPIO_Port, ESP_PWR_Pin);
	osDelay(100U);
}
