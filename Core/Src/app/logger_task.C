/** @file logger_task.c
 *  @brief logger task - diagnostic prints
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-03-2019
 */

#include "tiny-fs.h"
#include "watchdog.h"
#include "lan.h"
#include "my_comm.h"
#include "xprintf.h"
#include "task_tokens.h"
#include "config_files.h"
#include "ascii_helpers.h"
#include "ip_helpers.h"
#include "messages.h"

#include "file_io.h"

#ifndef MAKE_IP
#define MAKE_IP(a, b, c, d)                                                    \
	(((uint32_t)a) | ((uint32_t)b << 8) | ((uint32_t)c << 16) |            \
	 ((uint32_t)d << 24))
#endif

static void __attribute__((noreturn)) loop(char *msg);
static void __attribute__((noreturn)) init_log_ip_cfg(void);

static socket_p pdiagsoc;
extern osMutexId ETH_Mutex01Handle;
extern volatile bool Transmit_non_RTOS;

extern const Media_Desc_t Media0;

static const struct IP Log_IP_Cfg_File_Default = {
	.ip_n = { "LIP_" },
	.ip_v = { "192168000001" },
	.ip_p = { "05008" },
	.ip_nl = { "\n" },
};

/* configuration file */
static const char *IP_Cfg_File = "LIP_CFG";

static char * task_name = "logger_task";

/**
 * @brief logger_task_init
 */
void logger_task_init(void)
{

	pdiagsoc = NULL;
	register_magic(LOGGER_TASK_MAGIC);
	i_am_alive(LOGGER_TASK_MAGIC);

	messages_TaskInit_started(task_name);

	ip_pair_t IP_params;
	FRESULT res;

	res = ReadIPConfigFile(&Media0,
				IP_Cfg_File,
				&Log_IP_Cfg_File_Default,
				&IP_params);

	if (res == FR_OK) {
		if ((ETH_Mutex01Handle != NULL) && (lan_up() == 1U)) {
			pdiagsoc = bind_socket(IP_params.ip, IP_params.port, 0,
					       SOC_MODE_WRITE);
		} else {
			loop("lan_up() = 0\n");
		}
		if (pdiagsoc == NULL) {
			loop("No avail. socket\n");
		}
		messages_TaskInit_OK(task_name);
		xputs("Switching logging to UDP.\n");
//		taskENTER_CRITICAL();

//		taskEXIT_CRITICAL();
		xputs(task_name);
		xputs(" switched to UDP.\n");
		i_am_alive(LOGGER_TASK_MAGIC);
	} else {
		init_log_ip_cfg();
	}
}

/**
 * @brief logger_task_run
 */
void logger_task_run(void)
{
	Transmit_RTOS(pdiagsoc);
	i_am_alive(LOGGER_TASK_MAGIC);
}

/**
 * @brief Loop
 * @param msg
 */
static void __attribute__((noreturn)) loop(char *msg)
{
	messages_TaskInit_fail(task_name);

	xputs(msg);

	for (;;) {
		Transmit(NULL);
		vTaskDelay(pdMS_TO_TICKS(50U));
	} /* LOCK */ /* REBOOT */
}

static void __attribute__((noreturn)) init_log_ip_cfg(void)
{
	FRESULT res;

	res = DeleteFile(&Media0, IP_Cfg_File);
	res = SaveIPConfigFile(&Media0,
				IP_Cfg_File,
				&Log_IP_Cfg_File_Default);
	if (res != FR_OK) {
		loop("Error saving log IP cfg file.\n");
	}
	xputs("Log IP cfg file saved.\n");
	for (;;) {
		Transmit(NULL);
		vTaskDelay(pdMS_TO_TICKS(50U));
	} /* LOCK */ /* REBOOT */
}
