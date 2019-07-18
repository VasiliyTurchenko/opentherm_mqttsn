/** @file opentherm_task.h
*  @brief opentherm task
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 29-06-2019
 */

#ifndef OPENTHERM_TASK_H
#define OPENTHERM_TASK_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stdbool.h"

void opentherm_task_init(void);
void opentherm_task_run(void);

#ifdef __cplusplus
 }
#endif

extern bool	opentherm_configured;

#endif // OPENTHERM_TASK_H
