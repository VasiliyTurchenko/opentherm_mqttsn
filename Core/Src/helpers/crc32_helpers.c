/** @file crc32_helpers.c
 *  @brief helpers for hardware CRC32
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 16-Mar-2019
 */

#include "crc.h"
#include "crc32_helpers.h"
#include "mutex_helpers.h"

/* do use TAKE_MUTEX(CRC_MutexHandle) and GIVE_MUTEX(CRC_MutexHandle) ! */

uint32_t CRC32_helper(uint8_t *buf, size_t bufsize, uint32_t initial)
{
	uint32_t retVal = 0U;
	if (buf == NULL) {
		goto fExit;
	}
	/* initialize crc32 hardware module */
	if (HAL_CRC_GetState(&hcrc) != HAL_CRC_STATE_READY) {
		MX_CRC_Init();
	}

	size_t words = bufsize / sizeof(uint32_t);
	size_t bytes = bufsize % sizeof(uint32_t);
	uint32_t *p;
	p = (uint32_t *)(void *)buf;

	if (initial != 0U) {
		retVal = HAL_CRC_Calculate(&hcrc, &initial,
					   1U); /* process initial */
	}

	/* whole words */
	if (words != 0U) {
		if (initial == 0U) {
			retVal = HAL_CRC_Calculate(&hcrc, p,
						   words); /* then buffer */
		} else {
			retVal = HAL_CRC_Accumulate(&hcrc, p,
						    words); /* then buffer */
		}
	}

	/* bytes left */
	if (bytes != 0U) {
#if defined(_LITTLE_ENDIAN)
		union {
			uint32_t u32;
			uint8_t u8[4];
		} tmp;

		tmp.u32 = 0U;

		if (bytes > 2U) { /* single byte in the tail */
			tmp.u8[0] = buf[bufsize - 3U];
		}
		if (bytes > 3U) {
			tmp.u8[1] = buf[bufsize - 2U];
		}
		if (bytes > 4U) {
			tmp.u8[2] = buf[bufsize - 1U];
		}

#elif defined(_BIG_ENDIAN)
		union {
			uint32_t u32;
			uint8_t u8[4];
		} tmp;

		tmp.u32 = 0U;

		if (bytes > 2U) { /* single byte in the tail */
			tmp.u8[3] = buf[bufsize - 3U];
		}
		if (bytes > 3U) {
			tmp.u8[2] = buf[bufsize - 2U];
		}
		if (bytes > 4U) {
			tmp.u8[1] = buf[bufsize - 1U];
		}
#else
#error Endianness error!
#endif

		if ((initial != 0U) || (words != 0U)) {
			retVal = HAL_CRC_Accumulate(&hcrc, &tmp.u32, 1U);
		} else {
			retVal = HAL_CRC_Calculate(&hcrc, &tmp.u32, 1U);
		}
	}

fExit:
	HAL_CRC_DeInit(&hcrc);
	return retVal;
}
