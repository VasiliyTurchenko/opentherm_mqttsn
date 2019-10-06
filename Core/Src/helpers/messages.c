/** @file messages.c
 *  @brief text messages helpers
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 23-Mar-2019
 */

#include "logging.h"

//static char * init_msg = " initialization";
//static char * rebooting = " rebooting.\n";
//static char * started = " started.\n";

//char *params_load = " parameters load";
char *params_loaded = " parameters loaded";
char *params_not_loaded = " parameters loading error";

char *error_saving = "Error saving config file";
//char *cfg_file = " cfg file.\n";


void messages_Task_started(void)
{
	log_xputs(MSG_LEVEL_TASK_INIT, " started.");
}

void messages_TaskInit_started(void)
{
	log_xputs(MSG_LEVEL_TASK_INIT, " initialization started.");
}

void messages_TaskInit_OK(void)
{
	log_xputs(MSG_LEVEL_TASK_INIT, " initialization OK.");
}

void messages_TaskInit_fail(void)
{
	log_xputs(MSG_LEVEL_TASK_INIT, " initialization failed. Rebooting.");
}

