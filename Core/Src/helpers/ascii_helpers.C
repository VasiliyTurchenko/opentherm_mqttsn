/** @file ascii_helpers.c
 *  @brief some useful helpers
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-Mar-2019
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/**
 * @brief isHex checks is all symbols in the array are suitable to represent hex number
 * @param buf pointer to the buffer
 * @param len length of the buffer
 * @return true or false
 */
bool isHex(const void *const buf, size_t len)
{
	bool retVal = false;
	if ((buf == NULL) || (len == 0U)) {
		goto fExit;
	}
	retVal = true;
	const char *p = buf;
	for (size_t i = 0U; i < len; ++i) {
		if ((p[i] >= '0') && (p[i] <= '9')) {
			continue;
		} else if ((p[i] >= 'a') && (p[i] <= 'f')) {
			continue;
		} else if ((p[i] >= 'A') && (p[i] <= 'F')) {
			continue;
		} else {
			retVal = false;
			break;
		}
	}
fExit:
	return retVal;
}

/**
 * @brief isDec checks is all symbols in the array are suitable to represent decimal number
 * @param buf pointer to the buffer
 * @param len length of the buffer
 * @return true or false
 */
bool isDec(const void *const buf, size_t len)
{
	bool retVal = false;
	if ((buf == NULL) || (len == 0U)) {
		goto fExit;
	}
	retVal = true;
	const char *p = buf;
	for (size_t i = 0U; i < len; ++i) {
		if ((p[i] >= '0') && (p[i] <= '9')) {
			continue;
		} else {
			retVal = false;
			break;
		}
	}
fExit:
	return retVal;
}

/**
 * @brief cton converts ascii char to hex nibble
 * @param b source char
 * @return nibble
 * @note if b can't repersent a hex value function returns zero
 */
uint8_t cton(char b)
{
	if ((b <= 0x39) && (b >= 0x30)) {
		b = b - 0x30;
	} else if ((b <= 0x46) && (b >= 0x41)) {
		b = b - 0x37;
	} else if ((b <= 0x66) && (b >= 0x61)) {
		b = b - 0x57;
	} else
		b = 0;
	return (uint8_t)b;
}

/**
 * @brief dcton converts ascii char to dec number
 * @param b source char
 * @return nibble
 * @note if b can't repersent a hex value function returns zero
 */
static inline uint8_t dcton(char b)
{
	if ((b <= 0x39) && (b >= 0x30)) {
		b = b - 0x30;
	} else
		b = 0;
	return (uint8_t)b;
}


/**
 * @brief ahex2byte converts array of ascii symbols to byte
 * @param buf pointer to the first (most significant nibble) symbol
 * @param len length of the source field
 * @return byte
 * @note the function does not checks can the source symbols repersent a hex value!
 */
uint8_t ahex2byte(const void * const buf, size_t len)
{
	uint8_t retVal = 0U;
	if ( (buf == NULL) || (len == 0U) ) {
		goto fExit;
	}
	const char * p = buf;
	char tmpL = (char)p[len-1U];
	char tmpH = 0;
	if (len > 1U) {
		tmpH = (char)p[len-2U];
	}
	retVal = cton(tmpL) | (uint8_t)(cton(tmpH) << 4U);
fExit:
	return retVal;
}

/**
 * @brief adec2byte converts array of ascii symbols to byte
 * @param buf pointer to the first (hundreds) symbol
 * @param len length of the source field
 * @return byte
 * @note the function does not checks can the source symbols repersent a decimal value!
 */
uint8_t adec2byte(const void * const buf, size_t len)
{
	uint8_t retVal = 0U;
	if ( (buf == NULL) || (len == 0U) ) {
		goto fExit;
	}
	const char * p = buf;
	char tmpU = p[len-1U];
	char tmpD = 0;
	if (len > 1U) {
		tmpD = p[len-2U];
	}
	char tmpH = 0;
	if (len > 2U) {
		tmpH = p[len-3U];
	}

	retVal = dcton(tmpU) + 10U * dcton(tmpD) + 100U * dcton(tmpH);
fExit:
	return retVal;
}


/**
 * @brief ahex2uint16 converts array of ascii symbols to uint16
 * @param buf pointer to the first (most significant nibble) symbol
 * @param len length of the source field
 * @return uint16
 * @note the function does not checks can the source symbols repersent a hex value!
 */
uint16_t ahex2uint16(const void * const buf, size_t len)
{
	uint16_t retVal = 0U;
	if ( (buf == NULL) || (len == 0U) ) {
		goto fExit;
	}
	const char * p = buf;
	size_t cnt = (len > 4U) ? 4U : len;
	for (size_t i = 0U; i < cnt; ++i) {
		retVal |= (uint16_t)(cton(p[len - 1U - i]) << (i * 4U));
	}
fExit:
	return retVal;
}

/**
 * @brief adec2uint16 converts array of ascii symbols to uint16
 * @param buf pointer to the first (most significant nibble) symbol
 * @param len length of the source field
 * @return uint16
 * @note the function does not checks can the source symbols repersent a decimal value!
 */
uint16_t adec2uint16(const void * const buf, size_t len)
{
	uint16_t retVal = 0U;
	if ( (buf == NULL) || (len == 0U) ) {
		goto fExit;
	}
	const char * p = buf;
	size_t cnt = (len > 5U) ? 5U : len;

	uint16_t pow10[] = {1U, 10U, 100U, 1000U, 10000U};

	for (size_t i = 0U; i < cnt; ++i) {
		retVal += (uint16_t)(dcton(p[len - 1U - i]) * pow10[i]) ;
	}
fExit:
	return retVal;
}
