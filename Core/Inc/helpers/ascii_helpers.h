/** @file ascii_helpers.c
 *  @brief some useful helpers
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-Mar-2019
 */
#ifndef ASCII_HELPERS_H
#define ASCII_HELPERS_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool isHex(const void * const buf, size_t len);
bool isDec(const void * const buf, size_t len);

uint8_t cton(char b);
uint8_t ahex2byte(const void * const buf, size_t len);
uint8_t adec2byte(const void * const buf, size_t len);

uint16_t ahex2uint16(const void * const buf, size_t len);
uint16_t adec2uint16(const void * const buf, size_t len);

#ifdef __cplusplus
}
#endif


#endif // ASCII_HELPERS_H
