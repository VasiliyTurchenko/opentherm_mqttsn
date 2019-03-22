/** @file mutex_helpers.h
 *  @brief mutex macros
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 13-Mar-2019
 */

#ifndef MUTEX_HELPERS_H
#define MUTEX_HELPERS_H

#define TAKE_MUTEX(X) 	do { \
				if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) { \
					if (xSemaphoreTake((X), pdMS_TO_TICKS(10U)) != pdTRUE) { taskYIELD(); } \
				} \
			} while (0)

#define GIVE_MUTEX(X) 	do { \
				if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) { \
					xSemaphoreGive(X); } \
			} while (0)

#endif // MUTEX_HELPERS_H
