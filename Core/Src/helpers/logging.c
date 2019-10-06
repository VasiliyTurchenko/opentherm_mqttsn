/** @file logging.c
 *  @brief central logging system
 *
 *  @author Vasiliy Turchenko
 *  @bug
 *  @date 05-Oct-2019
 */

#include "logging.h"
#include "myCRT.h"

static uint32_t current_mask = 0U;
const char *delim = " : ";

/**
 * @brief filterIsPassed
 * @param lvl
 * @return
 */
bool filterIsPassed(MSG_LEVEL lvl)
{
	uint32_t reqLevel = (uint32_t)lvl & (0x7FFFFFFFU);
	return ((current_mask & reqLevel) != 0U);
}

/**
 * @brief log_set_mask_on
 * @param lvl
 */
void log_set_mask_on(MSG_LEVEL lvl)
{
	uint32_t reqLevel = (uint32_t)lvl & (0x7FFFFFFFU);
	current_mask |= reqLevel;
}

/**
 * @brief log_set_mask_off
 * @param lvl
 */
void log_set_mask_off(MSG_LEVEL lvl)
{
	uint32_t reqLevel = (uint32_t)lvl & (0x7FFFFFFFU);
	current_mask &= ~reqLevel;
}

///**
// * @brief log_xputc
// * @param lvl
// * @param c
// */
//void log_xputc(MSG_LEVEL lvl, char c)
//{
//if (filterIsPassed(lvl)) {
//		xputc(c);
//	}
//}

/**
 * @brief log_current_task_name
 */
void log_current_task_name(void)
{
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		xputs(pcTaskGetName(xTaskGetCurrentTaskHandle()));
		xputs(delim);
	}
}

static void sel_color(MSG_LEVEL lvl)
{
	switch(lvl) {
	case (MSG_LEVEL_FATAL) : {
		CRT_textColor(cBOLDRED);
		break;
	}
	case (MSG_LEVEL_SERIOUS) : {
		CRT_textColor(cBOLDYELLOW);
		break;
	}
	case (MSG_LEVEL_PROC_ERR) : {
		CRT_textColor(cBOLDMAGENTA);
		break;
	}
	case (MSG_LEVEL_INFO) : {
		CRT_textColor(cBOLDGREEN);
		break;
	}
	default:
		break;
	}
}

/**
 * @brief log_xputs
 * @param lvl
 * @param str
 */
void log_xputs(MSG_LEVEL lvl, const char *str)
{
	if (filterIsPassed(lvl)) {
	/* change color */
		sel_color(lvl);
		log_current_task_name();
		xputs(str);
		xputc('\n');
		CRT_resetToDefaults();
	}
}
