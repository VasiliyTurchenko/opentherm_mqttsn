/** @file subscribe_task.c
*  @brief MQTT_SN subscribe task
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

}

/**
 * @brief subscribe_task_run
 */
void subscribe_task_run(void)
{
	i_am_alive(SUB_TASK_MAGIC);
}
