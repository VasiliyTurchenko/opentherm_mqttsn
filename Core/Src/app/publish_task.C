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

#include "mqtt_config_helper.h"

//#include "MQTT_SN_task.h"

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
	HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
	HAL_GPIO_TogglePin(ESP_PWR_GPIO_Port, ESP_PWR_Pin);
	osDelay(100U);
	HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
	HAL_GPIO_TogglePin(ESP_PWR_GPIO_Port, ESP_PWR_Pin);
	osDelay(100U);
}
