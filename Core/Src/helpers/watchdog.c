/**
 * @file watchdog.c
 * @author Vasiliy Turchenko
 * @date 06-Feb-2018
 * @version 0.0.1
 *
 */

#include "watchdog.h"
#include "iwdg.h"
#include "rtc.h"



static uint32_t		magic_bits = 0x00U;
static uint32_t		magic_mask = 0x00U;


void start_iwdt(const TickType_t timeout);

/**
 * @brief i_am_alive is used to notify the watchdog that a caller task is still alive
 * @param magic bit from caller task
 * @return none
 */
void i_am_alive(const uint32_t magic)
{

	if ((magic == 0U) || (magic > 31U)) {return;}		/* bad magic */
	if ((magic_bits & (0x01U << magic)) == 0U) {return;}	/* magic isn't registered */
	taskENTER_CRITICAL();
	magic_bits &= ~(0x01U << magic);		/* reset the bit corresponding the caller task */
// 04-04-2018
	/* save magic_bits to backup domain */
	HAL_RTCEx_BKUPWrite(&hrtc, WDT_REG, magic_bits);

	if (magic_bits == 0U) {

//		xputs("\n\n\nWDG reload\n\n\n");

		HAL_IWDG_Refresh(&hiwdg);		/* reload iwdt */
		magic_bits = magic_mask;
	}
	taskEXIT_CRITICAL();


}

/**
 * @brief register_magic is used to register the task in the magic_mask field
 * @param magic bit position from caller task
 * @return none
 */
void register_magic(const uint32_t magic_pos)
{
	if ((magic_pos == 0U) || (magic_pos > 31U)) {return;}	/* bad magic position */
	taskENTER_CRITICAL();
	magic_mask |= (0x01U << magic_pos);			/* set the bit corresponding the caller task */
	magic_bits = magic_mask;
	taskEXIT_CRITICAL();
}

/**
 * @brief start_iwdt initializes and starts the independent watchdog timer
 * @param TickType_t timeout	- timeout in ms
 * @return none
 */
void start_iwdt(const TickType_t timeout)
{
	/* ... */

	magic_bits = magic_mask;
}


