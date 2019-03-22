/** @file startup.h
 *  @brief stratup system functions
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 28-Feb-2019
 *  @note runs berfore RTOS started!
 */

#ifndef STARTUP_H
#define STARTUP_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>

#ifdef STM32F303xC
	#include "stm32f3xx_hal.h"
#elif STM32F103xB
	#include "stm32f1xx_hal.h"
#endif

#include "tiny-fs.h"

/** @brief BOOT type */
typedef enum {
	COLD_BOOT = 1,
	WARM_BOOT,
} Boot_t;

/** @brief reboot cause type */
enum	reboot_cause	{
	RB_NONE = 0,
	RB_CMD,		/* Reboot due to remote command */
//	RB_WDT,		/* Reboot due to watchdog timer */
	RB_CFG_CHNG,	/* Software reboot due to congiguration change */
	RB_CFG_ERR,	/* Software reboot due to congiguration restore error */
	RB_PWR_LOSS     /* Power loss event */
	};
typedef	enum reboot_cause reb_cause_t;

/** @brief boot action type */
enum	boot_action	{
	B_ACT_COLD = 1,	/* Cold boot required */
	B_ACT_CFG,	/* Only configuration must be restored */
	B_ACT_CFG_DAQ	/* Configuration and process data must be restored */
	};
typedef	enum boot_action boot_action_t;

/** @brief FRAM state type */
enum	FRAM_state	{
	FRAM_OK = 1,	/* no FRAM error detected */
	FRAM_BAD	/* FRAM error was detected */
	};
typedef	enum FRAM_state	 FRAM_state_t;


extern volatile	Boot_t		Boot_Mode;		/* boot mode - cold or warm */
extern volatile	uint32_t	RCC_CSR_copy;		/* copy of the RCC_CSR */
extern volatile boot_action_t	Boot_Action;
extern volatile uint32_t	Saved_Magic_Bits;	/* watchdog */
extern const Media_Desc_t Media0;			/* FRAM media description */

extern volatile bool Transmit_non_RTOS;
extern volatile bool Transmit_nonRTOS;

ErrorStatus AppStartUp(void);
void Set_Reboot_Cause(reb_cause_t c_arg);



#ifdef __cplusplus
}
#endif


#endif // STARTUP_H
