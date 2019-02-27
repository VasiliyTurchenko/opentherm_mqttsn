/** @file debug.h
 *  @brief
 *
 *  @author
 *  @bug
 *  @date 27-Jan-2019
 */

#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#define DEBUG

#ifdef DEBUG
	#define NDEBUG_STATIC
#else
	#define NDEBUG_STATIC static inline
#endif

#endif
