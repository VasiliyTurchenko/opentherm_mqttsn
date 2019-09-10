/**
  * @file json_err.h
  * @author Vasiliy Turchenko
  * @date 27-Jan-2017
  **/

#ifndef JSON_ERR_H
#define JSON_ERR_H


#define JSON_ERR_OBSTART	(uint32_t)1	/* non-WS when expecting object start */
#define JSON_ERR_ATTRSTART	(uint32_t)2	/* non-WS when expecting attrib start */
#define JSON_ERR_BADATTR	(uint32_t)3	/* unknown attribute name */
#define JSON_ERR_ATTRLEN	(uint32_t)4	/* attribute name too long */
#define JSON_ERR_NOARRAY	(uint32_t)5	/* saw [ when not expecting array */
#define JSON_ERR_NOBRAK 	(uint32_t)6	/* array element specified, but no [ */
#define JSON_ERR_STRLONG	(uint32_t)7	/* string value too long */
#define JSON_ERR_TOKLONG	(uint32_t)8	/* token value too long */
#define JSON_ERR_BADTRAIL	(uint32_t)9	/* garbage while expecting comma or } or ] */
#define JSON_ERR_ARRAYSTART	(uint32_t)10	/* didn't find expected array start */
#define JSON_ERR_OBJARR 	(uint32_t)11	/* error while parsing object array */
#define JSON_ERR_SUBTOOLONG	(uint32_t)12	/* too many array elements */
#define JSON_ERR_BADSUBTRAIL	(uint32_t)13	/* garbage while expecting array comma */
#define JSON_ERR_SUBTYPE	(uint32_t)14	/* unsupported array element type */
#define JSON_ERR_BADSTRING	(uint32_t)15	/* error while string parsing */
#define JSON_ERR_CHECKFAIL	(uint32_t)16	/* check attribute not matched */
#define JSON_ERR_NOPARSTR	(uint32_t)17	/* can't support strings in parallel arrays */
#define JSON_ERR_BADENUM	(uint32_t)18	/* invalid enumerated value */
#define JSON_ERR_QNONSTRING	(uint32_t)19	/* saw quoted value when expecting nonstring */
#define JSON_ERR_NONQSTRING	(uint32_t)20	/* didn't see quoted value when expecting string */
#define JSON_ERR_MISC		(uint32_t)21	/* other data conversion error */
#define JSON_ERR_BADNUM		(uint32_t)22	/* error while parsing a numerical argument */
#define JSON_ERR_NULLPTR	(uint32_t)23	/* unexpected null value or attribute pointer */
#define JSON_ERR_NOROOM		(uint32_t)24	/* no room to hold result in the output buffer */


#endif // JSON_ERR_H
