/** @file mqtt_config_helper.c
 *  @brief A helper function for configuration MQTT-SN pub and sub tasks
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 04-Apr-2019
 */


#include "watchdog.h"
#include "lan.h"
#include "logging.h"
#include "task_tokens.h"

#include "config_files.h"
#include "tiny-fs.h"
#include "file_io.h"
#include "ascii_helpers.h"
#include "ip_helpers.h"
#include "messages.h"

static const char *ip_string = "IP";
static const char *topic_string = "topic";

extern const Media_Desc_t Media0;

/**
 * @brief mqtt_initialize
 * @param task_magic
 * @param params_pool
 * @param working_set
 * @param ip_pool
 */
void mqtt_initialize(uint32_t task_magic,
			const struct MQTT_parameters * params_pool,
			struct MQTT_topic_para * working_set,
			cfg_pool_t * ip_pool)
{
	i_am_alive(task_magic);

	messages_TaskInit_started();

	FRESULT res;
	bool need_reboot = false;

	res = ReadIPConfigFileNew(&Media0, ip_pool);
	log_xputs(MSG_LEVEL_TASK_INIT, params_pool->short_taskName);

	log_xputs(MSG_LEVEL_TASK_INIT, ip_string);
	if (res == FR_OK) {
		log_xputs(MSG_LEVEL_TASK_INIT, params_loaded);
	} else {
		log_xputs(MSG_LEVEL_TASK_INIT, params_not_loaded);

		res = DeleteFile(&Media0, ip_pool->file_name);
		res = SaveIPConfigFileNew(&Media0, ip_pool);
		if (res != FR_OK) {
			log_xputs(MSG_LEVEL_TASK_INIT, error_saving);
		}
		need_reboot = true;
	}

	i_am_alive(task_magic);

	const size_t btr = sizeof(struct MQTT_topic_para);
	size_t br = 0U;

	res = ReadBytes(&Media0,
			(const char *)params_pool->MQTT_topic_filename,
			0U, btr, &br, (uint8_t *)working_set);
	log_xputs(MSG_LEVEL_TASK_INIT, topic_string);
	if ((res == FR_OK) && (btr == br)) {
		log_xputs(MSG_LEVEL_TASK_INIT, params_loaded);
	} else {
		log_xputs(MSG_LEVEL_TASK_INIT, params_not_loaded);
		br = 0U;
		res = DeleteFile(
			&Media0,
			(const char *)params_pool->MQTT_topic_filename);
		res = WriteBytes(
			&Media0,
			(const char *)params_pool->MQTT_topic_filename,
			0U, btr, &br,
			(const uint8_t *)&params_pool->MQTT_topic_parameters);

		if ((res != FR_OK) || (br != btr)) {
			log_xputs(MSG_LEVEL_TASK_INIT, error_saving);
		}
		need_reboot = true;
	}

	if (need_reboot) {
		messages_TaskInit_fail();
		vTaskDelay(pdMS_TO_TICKS(portMAX_DELAY));

	} else {
		messages_TaskInit_OK();
	}
}
