/** @file service_task.c
 *  @brief various servce tasks
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-Mar-2019
 */

#include "watchdog.h"
#include "lan.h"
#include "logging.h"
#include "task_tokens.h"

#include "config_files.h"
#include "tiny-fs.h"
#include "ascii_helpers.h"
#include "ip_helpers.h"
#include "messages.h"

#include "file_io.h"

#include "ntp.h"
#include "tftp_server.h"

#define SEC_TO_01012000 946684800U
#define LEAPS_TO_0101200 (27U - 5U)

#define RESYNC_MS 30000U

extern const Media_Desc_t Media0;

extern osMutexId ETH_Mutex01Handle;

static const struct IP TFTP_IP_Cfg_File_Default = {
	.ip_n = { "TFTP" },
	.ip_v = { "192168000002" },
	.ip_p = { "00069" },
	.ip_nl = { "\n" },
};

static const struct IP NTP1_IP_Cfg_File_Default = {
	.ip_n = { "NTP1" },
	.ip_v = { "192168000001" },
	.ip_p = { "00123" },
	.ip_nl = { "\n" },
};

static const struct IP NTP2_IP_Cfg_File_Default = {
	.ip_n = { "NTP2" },
	.ip_v = { "216239035012" },
	.ip_p = { "00123" },
	.ip_nl = { "\n" },
};

static char *task_name = "service_task";

/* configuration pools */
static cfg_pool_t TFTP_cfg = {
	{ .ip = 0U, .port = 0U },
	&TFTP_IP_Cfg_File_Default,
	"TFTPCFG",
};

static cfg_pool_t NTP1_cfg = {
	{ .ip = 0U, .port = 0U },
	&NTP1_IP_Cfg_File_Default,
	"NTP1CFG",
};

static cfg_pool_t NTP2_cfg = {
	{ .ip = 0U, .port = 0U },
	&NTP2_IP_Cfg_File_Default,
	"NTP2CFG",
};

/**
 * @brief service_task_init
 */
void service_task_init(void)
{

	register_magic(SERVICE_TASK_MAGIC);
	i_am_alive(SERVICE_TASK_MAGIC);
	messages_TaskInit_started();

	FRESULT res;
	bool need_reboot = false;

	res = ReadIPConfigFileNew(&Media0, &TFTP_cfg);
	log_xputs(MSG_LEVEL_TASK_INIT, "TFTP:");
	if (res == FR_OK) {
		log_xputs(MSG_LEVEL_TASK_INIT, params_loaded);
		tftpd_init(&TFTP_cfg.pair);
	} else {
		log_xputs(MSG_LEVEL_TASK_INIT, params_not_loaded);

		res = DeleteFile(&Media0, TFTP_cfg.file_name);
		res = SaveIPConfigFileNew(&Media0, &TFTP_cfg);
		if (res != FR_OK) {
			log_xputs(MSG_LEVEL_TASK_INIT, "TFTP:");
			log_xputs(MSG_LEVEL_TASK_INIT, error_saving);
		}
		need_reboot = true;
	}

	res = ReadIPConfigFileNew(&Media0, &NTP1_cfg);

	log_xputs(MSG_LEVEL_TASK_INIT, "NTP1:");
	if (res == FR_OK) {
		log_xputs(MSG_LEVEL_TASK_INIT, params_loaded);
	} else {
		log_xputs(MSG_LEVEL_TASK_INIT, params_not_loaded);

		res = DeleteFile(&Media0, NTP1_cfg.file_name);
		res = SaveIPConfigFileNew(&Media0, &NTP1_cfg);
		if (res != FR_OK) {
			log_xputs(MSG_LEVEL_TASK_INIT, "NTP1:");
			log_xputs(MSG_LEVEL_TASK_INIT,error_saving);
		}
		need_reboot = true;
	}
	res = ReadIPConfigFileNew(&Media0, &NTP2_cfg);

	log_xputs(MSG_LEVEL_TASK_INIT, "NTP2:");
	if (res == FR_OK) {
		log_xputs(MSG_LEVEL_TASK_INIT, params_loaded);
	} else {
		log_xputs(MSG_LEVEL_TASK_INIT, params_not_loaded);

		res = DeleteFile(&Media0, NTP2_cfg.file_name);
		res = SaveIPConfigFileNew(&Media0, &NTP2_cfg);
		if (res != FR_OK) {
			log_xputs(MSG_LEVEL_TASK_INIT, "NTP2");
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

/**
 * @brief service_task_run
 */
void service_task_run(void)
{
	static TickType_t xLastSyncTime = 0U;
	static TickType_t xPeriod = pdMS_TO_TICKS(RESYNC_MS);

	i_am_alive(SERVICE_TASK_MAGIC);

	HAL_GPIO_TogglePin(YELLOW_LED_GPIO_Port, YELLOW_LED_Pin);
//	HAL_GPIO_TogglePin(ESP_RESET_GPIO_Port, ESP_RESET_Pin);
	osDelay(500U);
	log_xputs(MSG_LEVEL_INFO, "ping-");
	HAL_GPIO_TogglePin(YELLOW_LED_GPIO_Port, YELLOW_LED_Pin);
//	HAL_GPIO_TogglePin(ESP_RESET_GPIO_Port, ESP_RESET_Pin);
	osDelay(500U);
	log_xputs(MSG_LEVEL_INFO, "-pong\n");

	if ((xTaskGetTickCount() - xLastSyncTime) > xPeriod ) {
		if ( (NTP_sync(NTP1_cfg.pair) == SUCCESS) ||
		     (NTP_sync(NTP2_cfg.pair) == SUCCESS) ) {
			xLastSyncTime = xTaskGetTickCount();
		} else {
			xLastSyncTime += pdMS_TO_TICKS(1000U);
		}
		arp_age_entries();

		size_t n_entries = arp_get_capacity();
		for(size_t i  = 0U; i < n_entries; i++) {
			log_xputs(MSG_LEVEL_INFO, arp_get_entry_string(i));
		}
	}

	tftpd_run();

	/* remote reboot */
	if (DeleteFile(&Media0, "REBOOT") == FR_OK) {
		log_xputs(MSG_LEVEL_FATAL, "Reboot requested..\n");
		do {/*loop*/} while (true);
	}
}
