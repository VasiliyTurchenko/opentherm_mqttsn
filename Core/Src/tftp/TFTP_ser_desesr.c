/**
  ******************************************************************************
  * @file    TFTP_ser_deser.c
  * @author  turchenkov@gmail.com
  * @date    11-Mar-2018
  * @brief   Serialisation and deserialization functions for the tftp protocol
  ******************************************************************************
  * @attention use at your own risk
  ******************************************************************************
  */ 

#include "TFTP_ser_deser.h"
#include "mystrcpy.h"


/**
  * @brief TFTP_Deserialize_Packet parses the received packet 
  * @note  THREAD SAFE
  * @param  buf points to the buffer to be parsed
  * @param  buflen length of the data
  * @param  context - pointer to the tftp context structure
  * @retval ERROR or SUCCESS
  */
ErrorStatus TFTP_Deserialize_Packet(uint8_t * const buf, const size_t buflen, const tftp_context_p context)
{
	ErrorStatus	result;
	result = ERROR;
	uint8_t	* sptr;		/*!< source ptr */
	sptr = (uint8_t*)buf;
	uint8_t	* dptr;		/*!< destination ptr */
	uint8_t * buflim;
	buflim = (uint8_t*)((uintptr_t)buf + (uintptr_t)buflen - (uintptr_t)sizeof(uint8_t));
		
	/* try to define what is the packet */
	if (buflen < TFTP_ACK_PKT_LEN) { goto fExit; }	/* too small */
	uint16_t	opc;
	opc = read_u16_ntohs(&sptr);
	/* sptr now points to the first byte after opcode */
	if (opc == TFTP_RRQ) {
		context->OpCode = TFTP_RRQ;
	}
	if (opc == TFTP_WRQ) {
		context->OpCode = TFTP_WRQ;
	}
	switch (opc) {
		case TFTP_RRQ:
		case TFTP_WRQ:	
			dptr = (uint8_t*)context->FileName;
			if (mystrcpy(&dptr, &sptr, TFTP_MAX_FILENAME_LEN, buflim) == UINT32_MAX) {
				result = ERROR;
			} else {
				*dptr = 0x00U;
				/*now sptr points to the mode string */
				dptr = (uint8_t*)context->TFTP_Mode;
				if (mystrcpy(&dptr, &sptr, TFTP_MAX_MODE_LEN, buflim) == UINT32_MAX) {
					result = ERROR;
				} else {
					*dptr = 0x00U;
					result = SUCCESS;
				}
			}
			break;
		case TFTP_DATA:
			context->OpCode = TFTP_DATA;
			context->BlockNum = read_u16_ntohs(&sptr);
			/* now we have to find data size */
			context->DataPtr = sptr;
			context->DataLen = (size_t)(buflen - (TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN));
			result = SUCCESS;			
			break;
		case TFTP_ACK:
			context->OpCode = TFTP_ACK;	/**/
			context->BlockNum = read_u16_ntohs(&sptr);
			result = SUCCESS;
			break;
		case TFTP_ERR:
			context->OpCode = TFTP_ERR;	/**/
			context->ErrCode = read_u16_ntohs(&sptr);
			dptr = (uint8_t*)context->ErrMsg;
			if (mystrcpy(&dptr, &sptr, TFTP_MAX_ERRMSG_LEN, buflim) == UINT32_MAX) {
				result = ERROR;
			} else {
				*dptr = 0x00U;
				result = SUCCESS;
			}
			break;
		default:
			result = ERROR;
			break;
	}
	/* */
fExit:	
	return result;
}

/**
  * @brief  TFTP_Serialize_Packet serializes the packet using context info
  * @note  
  * @param  buf points to the buffer to be filled with the data
  * @param  buflen pointer to the length of the data
  * @param  context - pointer to the tftp context structure
 * @retval  payload data size or UINT32_MAX in case of error
  */
size_t TFTP_Serialize_Packet(uint8_t * const buf, const size_t buflen, const tftp_context_p context)
{
	size_t	result;
	result = 0x00U;
	uint8_t	*	sptr;		/*!< source ptr */
	uint8_t *	dptr;		/*!< destination ptr */
	dptr = (uint8_t*)buf;
//	const uint8_t *	buflim;
//	buflim = (uint8_t*)(buf + buflen - sizeof(uint8_t));
	if (context->OpCode == TFTP_RRQ) {
		write_u16_htons( &dptr, (uint16_t)TFTP_RRQ ) ;
	}
	if (context->OpCode == TFTP_WRQ) {
		write_u16_htons( &dptr, (uint16_t)TFTP_WRQ );
	}
	switch (context->OpCode) {
		case TFTP_RRQ:
		case TFTP_WRQ:			
			sptr = (uint8_t*)(context->FileName);
			if (mystrcpynf(&dptr, \
				     &sptr, \
				     TFTP_MAX_FILENAME_LEN, \
				     (uint8_t*)&context->FileName[TFTP_MAX_FILENAME_LEN]) == UINT32_MAX) {
				result = UINT32_MAX;
			} else {
				sptr = (uint8_t*)context->TFTP_Mode;
				if (mystrcpynf(&dptr, \
					      &sptr, \
					      TFTP_MAX_MODE_LEN, \
					       (uint8_t*)&context->TFTP_Mode[TFTP_MAX_MODE_LEN]) == UINT32_MAX) {
					result = UINT32_MAX;
				} else {
					result = (size_t)((size_t)dptr - (size_t)buf);
				}
			}
			break;
		case TFTP_DATA:
			write_u16_htons( &dptr, (uint16_t)TFTP_DATA );
			write_u16_htons( &dptr, (uint16_t)context->BlockNum );
			size_t	numbytes;
			numbytes = ( context->DataLen > (buflen - (TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN)) ) ? \
				   ( buflen - (TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN) ) : context->DataLen;
			memcpy(dptr, context->DataPtr, numbytes);
			result = (size_t)(numbytes + TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN);
			break;
		case TFTP_ACK:
			write_u16_htons( &dptr, (uint16_t)TFTP_ACK );
			write_u16_htons( &dptr, (uint16_t)context->BlockNum );
			result = (size_t)(TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN);
			break;
		case TFTP_ERR:
			write_u16_htons( &dptr, (uint16_t)TFTP_ERR );
			write_u16_htons( &dptr, (uint16_t)context->ErrCode );
			sptr = (uint8_t*)context->ErrMsg;
			if (mystrcpynf(&dptr, \
				     &sptr, \
				     TFTP_MAX_ERRMSG_LEN, \
				     (uint8_t*)&context->ErrMsg[TFTP_MAX_ERRMSG_LEN]) == UINT32_MAX) {
				result = UINT32_MAX;
			} else {
				result = (size_t)(dptr - buf);
			}
			break;
		default:
			result = UINT32_MAX;
			break;
	}
	/**/
	return result;
}
/* end of the function  */

/* ################################### E.O.F. ################################################### */
