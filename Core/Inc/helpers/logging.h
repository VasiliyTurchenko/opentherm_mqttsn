/** @file logging.h
 *  @brief central logging system
 *
 *  @author Vasiliy Turchenko
 *  @bug
 *  @date 05-Oct-2019
 */

#ifndef LOGGING_H
#define LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "cmsis_os.h"
#include "xprintf.h"

/* log messages level */

/* fatal failures, only reboot can help */
#define MSG_LEVEL_FATAL_ 1

/* serious failures, no reboot needed */
#define MSG_LEVEL_SERIOUS_ 2

/* normal process errors, no reboot needed */
#define MSG_LEVEL_PROC_ERR_ 4

/* info messages */
#define MSG_LEVEL_INFO_ 8

/* extended info messages */
#define MSG_LEVEL_EXT_INF_ 16

/* task initialisation messages */
#define MSG_LEVEL_TASK_INIT_ 32

typedef enum {
	MSG_LEVEL_FATAL = MSG_LEVEL_FATAL_,
	MSG_LEVEL_SERIOUS = MSG_LEVEL_SERIOUS_,
	MSG_LEVEL_PROC_ERR = MSG_LEVEL_PROC_ERR_,
	MSG_LEVEL_INFO = MSG_LEVEL_INFO_,
	MSG_LEVEL_EXT_INF = MSG_LEVEL_EXT_INF_,
	MSG_LEVEL_TASK_INIT = MSG_LEVEL_TASK_INIT_,
	MSG_LEVEL_ALL = 0x7FFFFFFF,
} MSG_LEVEL;

extern const char *delim;

void log_set_mask_on(MSG_LEVEL lvl);
void log_set_mask_off(MSG_LEVEL lvl);
//void log_xputc(MSG_LEVEL lvl, char c);
void log_xputs(MSG_LEVEL lvl, const char *str);
void log_current_task_name(void);

bool filterIsPassed(MSG_LEVEL lvl);

#define log_xprintf(MSG_LVL, ...)                                              \
	do {                                                                   \
		if (filterIsPassed((MSG_LVL))) {                               \
			log_current_task_name();                               \
			xprintf(__VA_ARGS__);                                  \
			xputc('\n');                                           \
		}                                                              \
	} while (false)

#ifdef __cplusplus
}
#endif

#endif // LOGGING_H
