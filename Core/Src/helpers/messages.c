/** @file messages.c
 *  @brief text messages helpers
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 23-Mar-2019
 */

#include "xprintf.h"

static char * init_msg = " initialization";
static char * rebooting = " rebooting.\n";
static char * started = " started.\n";

char *params_load = " parameters load";
char *params_loaded = "ed\n";
char *params_not_loaded = "ing error\n";

char *error_saving = "Error saving ";
char *cfg_file = " cfg file.\n";



void messages_Task_started(const char * msg)
{
	xputs(msg);
	xputs(started);
}

void messages_TaskInit_started(const char * msg)
{
	xputs(msg);
	xputs(init_msg);
	xputs(started);
}

void messages_TaskInit_OK(const char * msg)
{
	xputs(msg);
	xputs(init_msg);
	xputs(" OK.\n");

}

void messages_TaskInit_fail(const char * msg)
{
	xputs(msg);
	xputs(init_msg);
	xputs(" failed,");
	xputs(rebooting);
}

