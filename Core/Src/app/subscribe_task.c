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


/**
 * @brief subscribe_task_init
 */
void subscribe_task_init(void)
{
	register_magic(SUB_TASK_MAGIC);
}

/**
 * @brief subscribe_task_run
 */
void subscribe_task_run(void)
{
	i_am_alive(SUB_TASK_MAGIC);
}
