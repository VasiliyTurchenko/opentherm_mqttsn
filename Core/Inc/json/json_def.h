/*******************************************************************************
 * Copyright (c) turchenkov@gmail.com
 *
 * @filename json_def.h
 *
 * Contributors:
 *    Vasiliy Turchenko
 * @date 26-Jan-2018
 *******************************************************************************/

#ifndef JSON_DEF_H_
#define JSON_DEF_H_

#include <stdint.h>


#define JSONHEAD	"{\""
#define	JSONDELIM_O	", \""
#define JSONDELIM_P	"\":"
#define JSONTAIL	"}"
#define MIN_ROOM	(uint32_t)10		/* {" ":" "}\0 - total 10 chars */

#define	MAXJSONSTRING	(uint32_t)64

enum	jenum_t				/*!< fixed JSON values */
{
	jpad = 0,
	jTRUE,
	jFALSE,
	jNULL
};

enum	jValType				/*!< type of the value field */
{
	jvalpad = 0,
	jVALSZ,				/*!< the value is a string */
	jVALNUM,			/*!< the value is a number */
	jVALOBJ,			/*!< the value is an object */
	jVALARR,			/*!< the value is an array */
	jVALENUM			/*!< the value is an fixed type */
};

union	jsonVal
{
	uint8_t	*	jvalsz;		/*!< pointer to the json name string */
	double		jvalnum;	/*!< number */
	void	*	jvalobj;	/*!< pointer to object as value */
	void	*	jvalarr;	/*!< pointer to array as value */
	enum jenum_t	jvalenum;	/*!< jTRUE, jFALSE or jNULL */
};

typedef union jsonval *	 pjsonval_t;	/*!< pointer to json value */

typedef struct
{
	enum jValType	jvaltype;	/*!< type of the value */
	uint8_t	*	pjname;		/*!< pointer to object.string */
	union jsonVal *	pjsonval;	/*!< pointer to the value part of json entry */
	void *		next_obj;	/*!< pointer to the next object */
	
}	json_obj_t;

typedef json_obj_t *	pjson_obj; 	/*!< pointer to the json entry */
                                                                 


#endif /* JSON_DEF_H */


