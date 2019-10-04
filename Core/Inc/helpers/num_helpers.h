/** @file num_helpers.h
 *  @brief ASCII to numeric typee converter
 *
 *  @author Vasily Turchenko
 *  @bug
 *  @date 04-Oct-2019
 */

#ifndef NUM_HELPERS_H
#define NUM_HELPERS_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>

typedef enum num_types_ {
	NOT_A_NUM,
	U32_VAL,
	S32_VAL,
	U16_VAL,
	S16_VAL,
	U8_VAL,
	S8_VAL,
	FLOAT_VAL,
//	DOUBLE_VAL,
} num_types;

union n_t_val {
	uint32_t	u32_val;
	int32_t		i32_val;
	uint16_t	u16_val;
	int16_t		i16_val;
	uint8_t		u8_val;
	int8_t		i8_val;
	float		f_val;
//	double		d_val;
};

typedef struct numeric_t_ {
	num_types	type;
	union n_t_val	val;
} numeric_t;

numeric_t str_to_num(const char *s);
const char * type_to_str(num_types type);

#ifdef __cplusplus
 }
#endif


#endif // NUM_HELPERS_H
