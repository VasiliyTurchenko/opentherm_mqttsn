/** @file num_helpers.c
 *  @brief ASCII to numeric typee converter
 *
 *  @author Vasily Turchenko
 *  @bug
 *  @date 04-Oct-2019
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "num_helpers.h"

#define ABS_UINT32_MAX_64B ((uint64_t)UINT32_MAX)
#define ABS_INT32_MAX_64B ((uint64_t)INT32_MAX)
#define ABS_INT32_MIN_64B (1U + (uint64_t)INT32_MAX)

#define ABS_UINT16_MAX_64B ((uint64_t)UINT16_MAX)
#define ABS_INT16_MAX_64B ((uint64_t)INT16_MAX)
#define ABS_INT16_MIN_64B (1U + (uint64_t)INT16_MAX)

#define ABS_UINT8_MAX_64B ((uint64_t)UINT8_MAX)
#define ABS_INT8_MAX_64B ((uint64_t)INT8_MAX)
#define ABS_INT8_MIN_64B (1U + (uint64_t)INT8_MAX)


static numeric_t stof(const char *s)
{
	numeric_t retVal = {.type = FLOAT_VAL};

	float rez = 0.0f;
	float fact = 1.0f;

	if (*s == '-') {
		s++;
		fact = -1.0f;
	}

	bool point_seen = false;
	while (*s != 0x00) {
		if (*s == '.') {
			if (point_seen) {
				/* more than one point - error */
				retVal.type = NOT_A_NUM;
				break;
			}
			point_seen = true;
			s++;
			continue;
		}

		int d = *s - '0';
		if (d >= 0 && d <= 9) {
			if (point_seen) {
				fact /= 10.0f;
			}
			rez = rez * 10.0f + (float)d;
			s++;
		} else {
			/* illegal symbol */
			retVal.type = NOT_A_NUM;
			break;
		}
	}
	retVal.val.f_val = (float)(rez * fact);
	return retVal;
}

/**
 * @brief str_to_num
 * @param s pointer to the source string
 * @return
 */
numeric_t str_to_num(const char *s)
{
	numeric_t retVal = { .val.u32_val = 0U, .type = NOT_A_NUM };
	const char *s_sav = s;
	while (isspace((int)*s)) {
		s++;
	}
	bool val_signed = false;
	if (*s == '-') {
		val_signed = true;
		s++;
	}

	/* try to deduce int type */
	uint64_t decoded_it = 0U;
	while (isdigit((int)*s)) {
		decoded_it = decoded_it * 10U + (uint8_t)((*s) - 0x30);
		s++;
	}
	bool need_float_result = false;
	if (*s == 0x00U) {
		/* the string does not contain anyting except digits */
		if (!val_signed) {
			/* unsigned result */
			if (decoded_it <= ABS_UINT8_MAX_64B) {
				retVal.type = U8_VAL;
				retVal.val.u8_val = (uint8_t)decoded_it;
				goto fExit;
			}
			if (decoded_it <= ABS_UINT16_MAX_64B) {
				retVal.type = U16_VAL;
				retVal.val.u16_val = (uint16_t)decoded_it;
				goto fExit;
			}
			if (decoded_it <= ABS_UINT32_MAX_64B) {
				retVal.type = U32_VAL;
				retVal.val.u32_val = (uint32_t)decoded_it;
				goto fExit;
			}
			need_float_result =
				true; /*the result doesn't fit into unsigned inegers */
		} else {
			/* signed result */
			if (decoded_it <= ABS_INT8_MIN_64B) {
				retVal.type = S8_VAL;
				retVal.val.i8_val =
					-(int8_t)((uint8_t)decoded_it);
				goto fExit;
			}
			if (decoded_it <= ABS_INT16_MIN_64B) {
				retVal.type = S16_VAL;
				retVal.val.i16_val =
					-(int16_t)((uint16_t)decoded_it);
				goto fExit;
			}
			if (decoded_it <= ABS_INT32_MIN_64B) {
				retVal.type = S32_VAL;
				retVal.val.i32_val =
					-(int32_t)((uint32_t)decoded_it);
				goto fExit;
			}
			need_float_result =
				true; /*the result doesn't fit into unsigned inegers */
		}
	}
	/* is the source str suitable for float */
	retVal = stof(s_sav);
fExit:
	return retVal;
}

/**
 * @brief type_to_str
 * @param type
 * @return
 */
const char * type_to_str(num_types type)
{
	const char *retVal;
	switch (type) {
		case U8_VAL: {
			retVal = "U8";
			break;
		}
		case S8_VAL: {
			retVal = "S8";
			break;
		}
		case U16_VAL: {
			retVal = "U16";
			break;
		}
		case S16_VAL: {
			retVal = "S16";
			break;
		}
		case U32_VAL: {
			retVal = "U32";
			break;
		}
		case S32_VAL: {
			retVal = "S32";
			break;
		}
		case FLOAT_VAL: {
			retVal = "FLOAT";
			break;
		}
		case NOT_A_NUM: {
			retVal = "NOT_A_NUM";
			break;
		}
		default: {
			retVal = "BAD ENUM!";
			break;
		}
	}
	return retVal;
}
