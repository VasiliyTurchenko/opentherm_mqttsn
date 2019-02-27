/**
 * @file mystrcpy.c
 * @author Vasiliy Turchenko
 * @date 08-Mar-2018
 * @version 0.0.1
 */

#include <stddef.h>
#include "mystrcpy.h"

/*
 * copies not more than n characters (characters that follow a null
* character are not copied) from the array pointed to by s2 into the array
* pointed to by s1. If copying takes place between objects that overlap,
* the behaviour is undefined.
*/
/**
  * @brief mystrcpy copies not more than characters from src to dst.
  * @note  copy process stops after copying '\0' from the source array
  * @note  remaining memory is filled with zeroes
  * @param  dst points to the pointer to the destination array
  * @param  src points to the pointer to the source array
  * @param  nbytes is max characters to be copied
  * @param  maxsrcaddr is a pointer to the last byte of the source array!
  * @retval size_t mystrcpy = number of the actual copied characters excluding trailing zero
  * @retval dst points at the first uncopied character
  * @retval src points at the first uncopied character
  */
size_t mystrcpy(uint8_t **dst, uint8_t **src, size_t nbytes, const uint8_t * maxsrcaddr)
{
	size_t		res;
	res = 0U;
	if ( ((*dst) == NULL) || \
	     ((*src) == NULL) || \
	     (maxsrcaddr == NULL) || \
	     (maxsrcaddr < *src) ) {
		return UINT32_MAX;
	}
	if (nbytes == 0x00U) {
		return res;
	}

	size_t		i;
	uintptr_t	k;
	k = (uintptr_t)*src + (uintptr_t)nbytes - sizeof(uint8_t);
	uintptr_t	l;
	l = (uintptr_t)maxsrcaddr - ((uintptr_t)*src + sizeof(uint8_t)) ;
	i = ( (uintptr_t)maxsrcaddr < k ) ? l : nbytes;
	size_t		nb;
	nb = i;
	while (i > 0x00U) {
		if (**src == 0U) {
			(*src)++;
			break;
		}
		**dst = **src;
		(*dst)++;
		(*src)++;
		i--;
	}
	res = (nb - i);
	while (i > 0x00U) {
		**dst = 0x00U;
		(*dst)++;
		i--;
	}
	return res;
}
/* end of the function  */

/**
  * @brief mystrcpynf copies not more than characters from src to dst.
  * @brief a trailing zero is also copied
  * @note  copy process stops after copying '\0' from the source array
  * @param  dst points to the pointer to the destination array
  * @param  src points to the pointer to the source array
  * @param  nbytes is max characters to be copied
  * @param  maxsrcaddr is a pointer to the last byte of the source array!
  * @retval size_t mystrcpy = number of the actual copied characters excluding trailing zero
  * @retval dst points at the first uncopied character
  * @retval src points at the first uncopied character
  */
size_t mystrcpynf(uint8_t **dst, uint8_t **src, size_t nbytes, const uint8_t * maxsrcaddr)
{
	size_t		res;
	res = 0U;
	if ( ((*dst) == NULL) || \
	     ((*src) == NULL) || \
	     (maxsrcaddr == NULL) || \
	     (maxsrcaddr < *src) ) {
		return UINT32_MAX;
	}
	if (nbytes == 0x00U) {
		return res;
	}
	size_t		i;
	uintptr_t	k;
	k = (uintptr_t)*src + (uintptr_t)nbytes - sizeof(uint8_t);
	uintptr_t	l;
	l = (uintptr_t)maxsrcaddr - ((uintptr_t)*src + sizeof(uint8_t)) ;
	i = ( (uintptr_t)maxsrcaddr < k ) ? l : nbytes;
	size_t		nb;
	nb = i;
	while (i > 0x00U) {
		**dst = **src;
		(*dst)++;
		i--;
		if (**src == 0U) {
			(*src)++;
			break;
		}
		(*src)++;
	}
	res = (nb - i);
	return res;
}  

/**
  * @brief  write_u16 stores 16-bit value to thr memory pointed by uint8_t* addr
  * @note   addr then inncrements
  * @param  addr pointer to the target location
  * @param  val 16-bit value
  * @retval none
  */
void write_u16(uint8_t **addr, const uint16_t val)
{
	**addr = (uint8_t)val;
	(*addr)++;
	**addr = (uint8_t)(val >> 8U);
	(*addr)++;	
}

/**
  * @brief  write_u16_htons converts from host to net byte order and stores 16-bit value
  *	    to the memory pointed by uint8_t* addr
  * @note   addr then inncrements
  * @param  addr pointer to the target location
  * @param  val 16-bit value
  * @retval none
  */
void write_u16_htons(uint8_t **addr, const uint16_t val)
{
	**addr = (uint8_t)(val >> 8U);
	(*addr)++;
	**addr = (uint8_t)val;
	(*addr)++;	
}



/**
  * @brief  read_u16 reads uint16 from memory location pointed by addr
  * @note  addr then inncrements
  * @param  addr pointer to the source location  
  * @retval uint16_t value from memory
  */
uint16_t read_u16(uint8_t **addr)
{
	uint16_t	result;
	result = (uint16_t)**addr;
	(*addr)++;
	result = result | (((uint16_t)(**addr)) << 8U);
	(*addr)++;
	return result;
}

/**
  * @brief  read_u16_ntohs reads network ordered uint16 from memory location pointed by addr
  * @note  addr then inncrements
  * @param  addr pointer to the source location  
  * @retval uint16_t value from memory
  */
uint16_t read_u16_ntohs(uint8_t **addr)
{
	uint16_t	result;
	result = (((uint16_t)(**addr)) << 8U);
	(*addr)++;
	result = result | (uint16_t)**addr;
	(*addr)++;
	return result;
}



/* ############################ EOF ############################################################# */
