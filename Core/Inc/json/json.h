/**
 * @file json.h
 * @author Vasiliy Turchenko
 * @date 27-Jan-2017
 * @version 0.0.1
 *
 */
#ifndef	_JSON_H
#define _JSON_H

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "json_def.h"
#include "json_err.h"

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
uint32_t serialize_json(const uint8_t * pbuf, const size_t bufsize, json_obj_t *jsrc);



/**
  * deserialize_json creates json object(s) from the input stringz
  * @param pbuf pointer to the inpit buffer
  * @param payload size of the input buffer
  * @param jdst pointer to the first json pair in the chain
  * @return 0 if no error or error code
  *
  */
uint32_t deserialize_json(const uint8_t * pbuf, const size_t payload, json_obj_t *jdst);




#endif
