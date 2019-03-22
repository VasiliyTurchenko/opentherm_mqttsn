/** @file crc32_helpers.c
 *  @brief helpers for hardware CRC32
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 16-Mar-2019
 */

#ifndef CRC32_HELPERS_H
#define CRC32_HELPERS_H

#ifdef STM32F303xC
#include "stm32f3xx.h"
#include "stm32f3xx_hal_crc.h"
#elif STM32F103xB
#include "stm32f1xx.h"
#include "stm32f1xx_hal_crc.h"
#else
#error MCU NOT DEFINED
#endif

uint32_t CRC32_helper(uint8_t *buf, size_t bufsize, uint32_t initial);

#endif // CRC32_HELPERS_H
