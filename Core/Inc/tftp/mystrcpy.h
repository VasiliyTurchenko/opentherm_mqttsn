/**
 * @file mystrcpy.h
 * @author Vasiliy Turchenko
 * @date 08-Mar-2018
 * @version 0.0.1
 */

#ifndef _MYSTRCPY
#define _MYSTRCPY

#include	<stdint.h>
#include	<stddef.h>

size_t mystrcpy(uint8_t **dst, uint8_t **src, size_t nbytes, const uint8_t * maxsrcaddr);
size_t mystrcpynf(uint8_t **dst, uint8_t **src, size_t nbytes, const uint8_t * maxsrcaddr);
void write_u16(uint8_t **addr, const uint16_t val);
void write_u16_htons(uint8_t **addr, const uint16_t val);
uint16_t read_u16(uint8_t **addr);
uint16_t read_u16_ntohs(uint8_t **addr);

#endif
/* E.O.F. */
