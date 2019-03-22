/** @file service_task.c
 *  @brief various servce tasks
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-Mar-2019
 */

#include "watchdog.h"
#include "lan.h"
#include "xprintf.h"
#include "task_tokens.h"


#define	SEC_TO_01012000		946684800U
#define LEAPS_TO_0101200	(27U - 5U)


/**
 * @brief service_task_init
 */
void service_task_init(void)
{
	register_magic(SERVICE_TASK_MAGIC);
}


/**
 * @brief service_task_run
 */
void service_task_run(void)
{
	i_am_alive(SERVICE_TASK_MAGIC);

		HAL_GPIO_TogglePin(YELLOW_LED_GPIO_Port, YELLOW_LED_Pin);
		HAL_GPIO_TogglePin(ESP_RESET_GPIO_Port, ESP_RESET_Pin);
		osDelay(500U);
		xputs("ping-");
		HAL_GPIO_TogglePin(YELLOW_LED_GPIO_Port, YELLOW_LED_Pin);
		HAL_GPIO_TogglePin(ESP_RESET_GPIO_Port, ESP_RESET_Pin);
		osDelay(500U);
		xputs("pong\n");


}

