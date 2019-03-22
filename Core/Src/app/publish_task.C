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
		HAL_GPIO_TogglePin(ESP_PWR_GPIO_Port, ESP_PWR_Pin);
		osDelay(100U);
		HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
		HAL_GPIO_TogglePin(ESP_PWR_GPIO_Port, ESP_PWR_Pin);
		osDelay(100U);

}
