/** @file opentherm_wrappers.h
 *  @brief opentherm wrappers
 *
 *  @author Vasiliy Turchenko
 *  @bug
 *  @date 29-Apr-2019
 */

#ifndef OPENTHERM_WRAPPERS_H
#define OPENTHERM_WRAPPERS_H

#ifdef __cplusplus
extern "C" {
#endif

/* code for slave */
#ifdef SLAVEBOARD
#include "opentherm_slave.h"

/* code for master */
#elif defined(MASTERBOARD)

#else
#error NEITHER SLAVEBOARD NOR MASTERBOARD IS DEFINED!
#endif

#ifdef __cplusplus
}
#endif


#endif // OPENTHERM_WRAPPERS_H
