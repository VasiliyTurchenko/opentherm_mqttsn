/**
 * @file json.c
 * @author Vasiliy Turchenko
 * @date 27-Jan-2017
 * @version 0.0.1
 *
 */

#include "json.h"
#include "ascii_helpers.h"

#if JSON_DEBUG == 1

#define xprintf printf
#define JSON_DEBUG_PRINT

#elif JSON_DEBUG == 0

#include "logging.h"

#else
#error "ERROR. DEBUG NOT DEFINED!"
#endif

static const uint8_t pattern_false[] = { "false" };
static const uint8_t pattern_true[] = { "true" };
static const uint8_t pattern_null[] = { "null" };

/* private funcrions */
static inline void skip_spaces(const uint8_t **ptr);
NDEBUG_STATIC uint32_t pair_out(uint8_t **adst, size_t *aremain,
				const pjson_obj asrc);
NDEBUG_STATIC enum jenum_t decode_enum(const uint8_t **ptr);
NDEBUG_STATIC size_t check_esc(size_t sym_index, const uint8_t *ptr,
			       size_t len);
NDEBUG_STATIC size_t get_jstrlen(const uint8_t *ptr);

/**
 * @brief skip_spaces
 * @param ptr address of the pointer to the current buffer position
 * @return *ptr points to the next non-space value
 */
/* ECMA-404 defines these symbols as whitespaces */
#define IS_ECMA_WHITESPACE(A)                                                  \
	(((A) == 0x20U) || ((A) == 0x09U) || ((A) == 0x0AU) || ((A) == 0x0DU))

static inline void skip_spaces(const uint8_t **ptr)
{
	while (IS_ECMA_WHITESPACE(**ptr)) {
		(*ptr)++;
	}
}

/**
 * @brief decode_enum
 * @param ptr address of the pointer to the current buffer position
 * @return decoded value or jpad if bad enum
 */
NDEBUG_STATIC enum jenum_t decode_enum(const uint8_t **ptr)
{
	enum jenum_t decoded = jpad;
	const uint8_t *pattern = NULL;
	switch (**ptr) {
	case 0x66U: /* f */ {
		pattern = pattern_false;
		decoded = jFALSE;
		break;
	}
	case 0x74U: /* t */ {
		pattern = pattern_true;
		decoded = jTRUE;
		break;
	}
	case 0x6eU: /* n */ {
		pattern = pattern_null;
		decoded = jNULL;
		break;
	}
	default: {
		break;
	}
	}
	if (decoded != jpad) {
		while (*pattern != 0x00U) {
			if ((**ptr == 0x00U) || (**ptr != *pattern)) {
				decoded = jpad;
				goto fExit; /* error, token != false */
			}
			(*ptr)++;
			pattern++;
		}
	}
fExit:
	return decoded;
}
/*---------------------------------------------------------------------*/

/** pair_out creates a pair "name":"value"
  * @param adst pointer to the place in the outbuffer
  * @param aremain remaining free bytes in the outbuffer
  * @param asrc source json object
  * @return 0 if no error or error code
  *
  **/
NDEBUG_STATIC uint32_t pair_out(uint8_t **adst, size_t *aremain,
				const pjson_obj asrc)
{
	uint32_t result;
	result = 0U;
	size_t a;
	static const uint8_t d1[] = JSONDELIM_P;
	static const uint8_t d2[] = JSONDELIM_O;
	static const uint8_t tail[] = JSONTAIL;

	const uint8_t *src;

	/* name out */
	if (asrc->pjname == NULL) {
		result = JSON_ERR_NULLPTR;
		goto fExit;
	}
	if ((asrc->jvaltype != jVALSZ) && (asrc->jvaltype != jVALENUM)) {
		/* only jVALSZ and jVALENUM are supported */
		result = JSON_ERR_MISC;
		goto fExit;
	}

	a = strlen((char *)asrc->pjname);
#ifdef JSON_DEBUG_PRINT
	xprintf("%d ", __LINE__);
	xprintf("strlen((char*)asrc->pjname) = %d\n", a);
#endif
	if (*aremain < a) {
		result = JSON_ERR_NOROOM; /* error with out buffer */
		goto fExit;
	}
	src = asrc->pjname;
	while (*src != 0x00U) {
		**adst = *src;
		(*adst)++;
		src++;
		(*aremain)--;
	}
#ifdef JSON_DEBUG_PRINT
	xprintf("%d ", __LINE__);
	xprintf("serialize.json.pair_out.aremain = %d\n", *aremain);
#endif
	/* name : value delimiter out */
	if (*aremain < sizeof(d1)) {
		result = JSON_ERR_NOROOM; /* error with out buffer */
		goto fExit;
	}
	src = d1;
	while (*src != 0x00U) {
		**adst = *src;
		(*adst)++;
		src++;
		(*aremain)--;
	}
#ifdef JSON_DEBUG_PRINT
	xprintf("%d ", __LINE__);
	xprintf("serialize.json.pair_out.aremain = %d\n", *aremain);
#endif
	/*value out and finalize if asrc is the last in the chain*/
	switch (asrc->jvaltype) {
	case jVALSZ:
		if (asrc->pjsonval->jvalsz == NULL) {
			result = JSON_ERR_NULLPTR;
			goto fExit;
		}
		**adst = 0x22U; /* leading " after ": */
		(*adst)++;
		(*aremain)--;
		a = strlen((char *)asrc->pjsonval->jvalsz);
#ifdef JSON_DEBUG_PRINT
		xprintf("%d ", __LINE__);
		xprintf("strlen(asrc->pjsonval->jvalsz) = %d\n", a);
#endif
		if (*aremain < a) {
			result = JSON_ERR_NOROOM; /* error with out buffer */
			goto fExit;
		}
		src = (asrc->pjsonval->jvalsz);
		while (*src != 0x00U) {
			**adst = *src;
			(*adst)++;
			src++;
			(*aremain)--;
		}
		**adst = 0x22U; /* trailing " after string value */
		(*adst)++;
		(*aremain)--;
		break; /* jVALSZ */

	case jVALENUM:
		a = sizeof(pattern_false); /* "false" is the longest */
#ifdef JSON_DEBUG_PRINT
		xprintf("%d ", __LINE__);
		xprintf("sizeof(jfalse)= %d\n", a);
#endif
		if (*aremain < a) {
			result = JSON_ERR_NOROOM;
			goto fExit;
		}
		switch (asrc->pjsonval->jvalenum) {
		case jTRUE:
			src = pattern_true;
			break;
		case jFALSE:
			src = pattern_false;
			break;
		case jNULL:
			src = pattern_null;
			break;
		case jpad:
		default:
			result = JSON_ERR_BADENUM;
			goto fExit;
			break;
		}
		while (*src != 0x00U) {
			**adst = *src;
			(*adst)++;
			src++;
			(*aremain)--;
		}
		break; /* jVALFIXED */

	case jvalpad:
	case jVALNUM:
	case jVALOBJ:
	case jVALARR:
	default:
		result = JSON_ERR_MISC;
		goto fExit;
		break; /* default */
	}
#ifdef JSON_DEBUG_PRINT
	xprintf("%d ", __LINE__);
	xprintf("serialize.json.pair_out.aremain = %d\n", *aremain);
#endif

	src = (asrc->next_obj == NULL) ? tail : d2; /* last entry or not ?*/
	size_t b;
	b = (src == tail) ? (sizeof(tail) + 1U) : (sizeof(d2) + 1U);

	if (*aremain < b) {
		result = JSON_ERR_NOROOM; /* error with out buffer */
		goto fExit;
	}
	while (*src != 0x00U) {
		**adst = *src;
		(*adst)++;
		src++;
		(*aremain)--;
	}
fExit:
	return result;
}

/**
  * serialize_json creates string containing json object
  * @param pbuf pointer to the output buffer
  * @param bufsize size of the output buffer
  * @param jsrc pointer to the first json pair in the chain
  * @return 0 if no error or error code
  *
  */
uint32_t serialize_json(uint8_t *pbuf, const size_t bufsize,
			json_obj_t *jsrc)
{
	static const uint8_t head[] = JSONHEAD;

	uint32_t result;
	result = 0;
	if (jsrc == NULL) { /* does the source object exist? */
		result = JSON_ERR_NULLPTR;
		goto fExit;
	}
	if ((pbuf == NULL) || (bufsize < MIN_ROOM)) {
		result = JSON_ERR_NOROOM; /* error with the out buffer */
		goto fExit;
	}
	size_t remain; /*!< remaining room in the outbuf */
	remain = bufsize - (size_t)1;

#ifdef JSON_DEBUG_PRINT
	xprintf("%d ", __LINE__);
	xprintf("initial value of serialize.json.remain = %d\n", remain);
#endif

	/* head out */
	const uint8_t *src;
	uint8_t *dst;

	src = head;
	dst = pbuf;
	while (*src != 0x00U) {
		*dst = *src;
		dst++;
		src++;
	}
	remain -= sizeof(head);

#ifdef JSON_DEBUG_PRINT
	xprintf("%d ", __LINE__);
	xprintf("serialize.json.remain = %d\n", remain);
#endif
	/* pair generate and output */
	pjson_obj jptr;
	jptr = jsrc;
	do {
		result = pair_out(&dst, &remain, jptr);
		if (result != 0U) {
			goto fExit;
		}
		jptr = (pjson_obj)(jptr->next_obj);
	} while (jptr != NULL);

	*dst = 0x00U;
	remain--;

#ifdef JSON_DEBUG_PRINT
	xprintf("%d ", __LINE__);
	xprintf("serialize.json.remain = %d\n", remain);
#endif
	result = 0U;

fExit:
	return result;
}

/**
  * check_esc checks is the string provided a valid esc sequence
  * @param ii - current symbol index starting from 0
  * @param ptr - pointer which points to the start of the string
  * @param len - length of the string defined by the caller
  * @return length of the valid esc sequence or 0 in sequence is invalid
  */
NDEBUG_STATIC size_t check_esc(size_t sym_index, const uint8_t *ptr, size_t len)
{
	size_t result = 0U;

	/* ii + esc_len must be < len */
	/* result length must be less than 6 bytes! */
	sym_index++; /* skip over leading reverse solidus (U+005C) */

	/* check for single byte escapes */
	uint8_t sym = ptr[sym_index];
	if ((sym == 0x22U) || /* quotation mark */
	    (sym == 0x5CU) || /* reverse solidus */
	    (sym == 0x2fU) || /* solidus */
	    (sym == 0x62U) || /* b */
	    (sym == 0x66U) || /* f */
	    (sym == 0x6eU) || /* n */
	    (sym == 0x72U) || /* r */
	    (sym == 0x74U) /* t */) {
		result = 1U;
		goto fExit;
	}

	/* check for u hex hex hex hex */
	if ((sym == 0x75U) && ((sym_index + 4U) < len)) { /* u */
		if (isHex(&ptr[sym_index], 4U) == true) {
			result = 4U;
			goto fExit;
		}
	}
fExit:
	return result;
}

/** get_jstrlen returns the length of the valid json string which is between the quotation mark characters (U+0022).
  * @param ptr pointer to the start of the string, it points to the first symbol after leading quotation mark character (U+0022).
  * @return 0 if string is not valid; length of the string othervise
  *
  */
NDEBUG_STATIC size_t get_jstrlen(const uint8_t *ptr)
{
	size_t result = 0U;
	size_t len1 = strlen((const char *)ptr);

	if ((len1 == 0) || (len1 == MAXJSONSTRING)) {
		goto fExit; /* bad string - too short or too long */
	}

	/* scan for " */
	size_t esc_len; /* length of esc sequence  in the stream */

	size_t volatile i = 0U;
	while (i < len1) { /* find json string */
		if (ptr[i] < 0x20U) {
			/* chars from 0x00 to 0x01f are not allowed in the string */
			break;
		}

		switch (ptr[i]) { /* here go the symbols which are greater than 0x01f */
		case 0x5c: { /* reverse solidus (U+005C) found */
			if (i == (len1 - 1U)) {
				/* 0x5c can not be the last symbol */
				goto fExit;
			}

			/*check for valid esc sequence */
			esc_len = check_esc(i, ptr, len1);

			if (esc_len == 0U) {
				goto fExit; /* esc seq is invalid */
			} else {
				i += esc_len;
			}
			break;
		}
		case 0x22: { /* quotation mark character (U+0022) found */
			result = i;
			goto fExit; /* the length of the string found */
			break;
		}
		default: {
			break;
		}
		}
		i++;
	}
fExit:
	return result;
}

/* JSON FORMAT STRING */
/*
   TOKENS ARE:

*/
#if(0)
typedef enum JSON_FORMAT_TOK_ {
	JFT_SPACE		= 0x20,
	JFT_CURLY_BR_OPEN	= 0x7B,
	JFT_CURLY_BR_CL		= 0x7D,
	JFT_COLON		= 0X3A,
	JFT_QUOT		= 0x22,
	JFT_TXT_NAME		= 0xEE,
	JFT_TXT_VAL		= 0xEF,
	JFT_ENDL		= 0x00,
} JSON_FORMAT_TOKEN;
						/*____{___"____"this_is_a_text"*/
static const JSON_FORMAT_TOKEN fmt_string[] = {JFT_SPACE, JFT_CURLY_BR_OPEN, JFT_SPACE, JFT_QUOT, JFT_TXT_NAME, JFT_QUOT, \
						/*____:____"this_is_a_text_"_____*/
						JFT_SPACE, JFT_COLON, JFT_SPACE, JFT_QUOT, JFT_TXT_VAL, JFT_QUOT, JFT_SPACE, \
						/*_____}______\0*/
						JFT_CURLY_BR_CL, JFT_SPACE, JFT_ENDL};

static const size_t fmt_string_l = sizeof (fmt_string) / sizeof (fmt_string[0]);
uint32_t deserialize_json_f(const uint8_t *pbuf, const size_t payload_size,
			  json_obj_t *jdst)
{
	uint32_t result = JSON_ERR_MISC;
	if( payload_size != strlen((const char *)pbuf) ) {
		goto fExit;
	}
	const uint8_t *walk_ptr = pbuf; /*!< ptr to current char */
	for (size_t i = 0U; i < fmt_string_l; i++) {

		if ( fmt_string[i] == JFT_SPACE) {
			skip_spaces(&walk_ptr);
			continue;
		}
		if ( (fmt_string[i] == JFT_CURLY_BR_OPEN) && (*walk_ptr != 0x7bU) ) {
			result = JSON_ERR_OBSTART;
			break;
		} else {
			walk_ptr++;
		}
		if ( (fmt_string[i] == JFT_QUOT) && (*walk_ptr != 0x22U) ) {
			result = JSON_ERR_OBSTART;
			break;
		} else {
			walk_ptr++;
		}
		/* text name starts here */
		if ( fmt_string[i] == JFT_TXT_NAME ) {
			size_t jstrlen = get_jstrlen(walk_ptr);
			if (jstrlen == 0U) {
				result = JSON_ERR_BADSTRING;
				break;
			}
			 /* fill jname part with string */
			memcpy(jdst->pjname, walk_ptr, jstrlen);
			*(jdst->pjname + jstrlen) = 0x00;
			walk_ptr += jstrlen;
		}

		if ( (fmt_string[i] == JFT_COLON) && (*walk_ptr != 0x3A)) {
			result = JSON_ERR_BADSTRING;
			break;
		} else {
			walk_ptr++;
		}

		if ( fmt_string[i] == JFT_TXT_VAL ) {
			/* two options here : enum or string */

			size_t jstrlen = get_jstrlen(walk_ptr);
			if (jstrlen == 0U) {
				result = JSON_ERR_BADSTRING;
				break;
			}
			/* fill jval part with string */



	}

fExit:
	return result;
}
#endif

/**
  * deserialize_json creates json object(s) from the input stringz
  * @param pbuf pointer to the inpit buffer
  * @param payload_size is the size of the payload in the input buffer
  * @param jdst pointer to the first json pair in the chain
  * @return 0 if no error or error code
  *
  */
uint32_t deserialize_json(const uint8_t *pbuf, const size_t payload_size,
			  json_obj_t *jdst)
{
	uint32_t result = 0U;
	size_t slen = strlen((const char *)pbuf); /* trailng 0 is not counted */
	if (slen != payload_size) {
		result = JSON_ERR_MISC;
		goto fExit;
	}

	const uint8_t *walk_ptr; /*!< ptr to current char */
	walk_ptr = pbuf;
	skip_spaces(&walk_ptr);

	if (*walk_ptr != 0x7bU) {
		result = JSON_ERR_OBSTART;
		goto fExit;
	}
	walk_ptr++;

	skip_spaces(&walk_ptr);
	if (*walk_ptr != 0x22U) {
		result = JSON_ERR_OBSTART;
		goto fExit;
	}
	walk_ptr++;

	/* find "name string" length */
	size_t jstrlen;
	jstrlen = get_jstrlen(walk_ptr);

	if (jstrlen > 0U) {
		memcpy(jdst->pjname, walk_ptr,
		       jstrlen); /* fill jname part with string */
		*(jdst->pjname + jstrlen) = 0x00;
	} else {
		result = JSON_ERR_BADSTRING;
		goto fExit;
	}

	/* detect string : value delimiter */
	walk_ptr += jstrlen; /* points at trailing \" */
	if (*walk_ptr != 0x22U) {
		result = JSON_ERR_MISC;

#ifdef JSON_DEBUG_PRINT
		xprintf("%d ", __LINE__);
		xprintf(" ALARM! \n");
#endif
		goto fExit; /* something goes wrong!!!*/
	}
	walk_ptr++; /* now it points to the first symbol after \" */

	/* look for start of value. ..., 0x22, n*space, 0x3a, n*space, S */
	/*                                        ^                    ^ */
	/*                                    walk_ptr           start of value */
	skip_spaces(&walk_ptr); /* walk thru spaces */

	if (*walk_ptr != 0x3aU) { /* didn't find colon */
		result = JSON_ERR_BADSTRING;
		goto fExit;
	}
	walk_ptr++;
	skip_spaces(&walk_ptr); /* walk thru spaces */

	/* here starts value */
	enum jenum_t decoded = jpad;

	if (*walk_ptr != 0x22U) {
		/* not found string value leading double-quotes */
		decoded = decode_enum(&walk_ptr);
		/* if walk_ptr points at start of valid enum,   */

		/* decode enum and move walk_ptr to the first symbol after enum */
		if (decoded != jpad) { /* valid enum found */
			jdst->jvaltype = jVALENUM;
			jdst->pjsonval->jvalenum = decoded;
		} else {
			result = JSON_ERR_BADENUM;
			goto fExit;
		}
	} else {
		walk_ptr++;
		jstrlen = get_jstrlen(walk_ptr);

		if (jstrlen > 0U) {
			jdst->jvaltype = jVALSZ;

			/* fill string value part */
			memcpy(jdst->pjsonval->jvalsz, walk_ptr, jstrlen);
			*(jdst->pjsonval->jvalsz + jstrlen) = 0x00;

			walk_ptr += jstrlen;

			if (*walk_ptr != 0x22U) {
				result = JSON_ERR_MISC;
#ifdef JSON_DEBUG_PRINT
				xprintf("%d ", __LINE__);
				xprintf(" ALARM! \n");
#endif
				goto fExit; /* something goes wrong!!!*/
			}

			walk_ptr++; /* now it points to the first symbol after \" */

		} else {
			result = JSON_ERR_BADSTRING;
			goto fExit;
		}
	}
	/* here walk_ptr points at first symbol after enum or after trailing string value double-quotes */
	skip_spaces(&walk_ptr);
	/* here we expect to see } */
	if ((*walk_ptr != 0x7dU) && (*walk_ptr != 0x2cU)) {
		/* only simple JSON */
		result = JSON_ERR_BADTRAIL;
		goto fExit;
	}
	result = 0U;
fExit:
	return result;
}
/* ----------------------------------------------------------------------------------------------------------- */
