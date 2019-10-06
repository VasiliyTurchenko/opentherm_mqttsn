/**
 * @file json.h
 * @author Vasiliy Turchenko
 * @date 27-Jan-2017
 * @version 0.0.1
 *
 */
#ifndef	JSON_H
#define JSON_H

#include <stdio.h>
#include <string.h>

#include "json_def.h"
#include "json_err.h"


#define JSON_DEBUG	(0)	/* no debug */
//#define JSON_DEBUG	(1)	/* debug and tests are enabled */

#if JSON_DEBUG == 1

#define NDEBUG_STATIC

#elif JSON_DEBUG == 0

#define NDEBUG_STATIC static inline

#else
#error "ERROR. DEBUG NOT DEFINED!"
#endif


/**
  * serialize_json creates string containing json object
  * @param pbuf pointer to the output buffer
  * @param bufsize size of the output buffer
  * @param jtype type of the resultin json object
  * @param pname pointer to the name string
  * @param pval pointer to the value object
  * @return 0 if no error or error code
  *
  */
uint32_t serialize_json(uint8_t *pbuf, const size_t bufsize, json_obj_t *jsrc);




/**
  * deserialize_json creates json object(s) from the input stringz
  * @param pbuf pointer to the inpit buffer
  * @param payload size of the input buffer
  * @param jdst pointer to the first json pair in the chain
  * @return 0 if no error or error code
  *
  */
uint32_t deserialize_json(const uint8_t * pbuf, const size_t payload, json_obj_t *jdst);

#if JSON_DEBUG == 1

/** get_jstrlen returns the length of the valid json string which is between the quotation mark characters (U+0022).
  * @param ptr pointer to the start of the string, it points to the first symbol after leading quotation mark character (U+0022).
  * @return 0 if string is not valid; length of the string othervise
  *
  */
size_t get_jstrlen(const uint8_t *ptr);

/**
  * check_esc checks is the string provided a valid esc sequence
  * @param ii - current symbol number
  * @param ptr - pointer which points to the reverse solidus (U+005C) found by caller
  * @param len - length of the string defined by the caller
  * @return length of the valid esc sequence or 0 in sequence is invalid
  */
size_t check_esc(size_t ii, const uint8_t *ptr, size_t len);

/**
 * @brief decode_enum
 * @param ptr address of the pointer to the current buffer position
 * @return decoded value or jpad if bad enum
 */
enum jenum_t decode_enum(const uint8_t **ptr);

#endif

#endif
