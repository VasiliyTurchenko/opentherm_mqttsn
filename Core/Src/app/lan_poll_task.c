/** @file lan_poll_task.c
 *  @brief lan polling
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-03-2019
 */

#include "FreeRTOS.h"
#include "watchdog.h"
#include "xprintf.h"
#include "lan.h"
#include "task_tokens.h"

extern osMutexId ETH_Mutex01Handle;

/**
 * @brief lan_poll_task_init
 */
void lan_poll_task_init(void)
{
	/* watchdog magic */
	register_magic(LAN_POLL_TASK_MAGIC);
	xputs("lan_poll task init OK.\n");
}

/**
 * @brief lan_poll_task_run
 */
void lan_poll_task_run(void)
{
	if (ETH_Mutex01Handle != NULL) {
		if (lan_up() == 1U) {
			lan_poll();
		}
	}
	i_am_alive(LAN_POLL_TASK_MAGIC);
}
