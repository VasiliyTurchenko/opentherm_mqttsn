/**
 ******************************************************************************
 * @file    tftp_server.c
 * @author  turchenkov@gmail.com
 * @date    04-March-2018
 * @brief   Simple single-connection TFTP server
 ******************************************************************************
 */
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "stm32f1xx.h"
#include "xprintf.h"

#include "lan.h"
#include "tftp_server.h"
#include "conf_fn.h"
#include "crc.h"
#include "mystrcpy.h"
#include "TFTP_ser_deser.h"

#include "nvmem.h"


#include "debug_settings.h"

#define TFTP_BUFFER_SIZE	(TFTP_DATA_LEN_MAX + TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN)

/* static variables */
static uint8_t			tftp_buffer[TFTP_BUFFER_SIZE];
static tftp_context_t		tftp_context;
static tftp_context_p	context1;

#ifdef	TFTP_STRESS_TEST
static	size_t	fsize = TFTP_STRESS_FILESIZE;
#endif

#ifdef	TFTP_ERR_STATS
static	uint16_t	send_err_errors;
#endif

#ifdef	TFTP_DEBUG_PRINT
static	uint32_t	t0;
static	uint32_t	t1;
#endif

/* private functions */

static void TFTP_Block_Read(const tftp_context_p context);
static void TFTP_Check_for_Abort(const tftp_context_p context);
static void TFTP_Close_File(tftp_context_p context);
static void TFTP_Init_Context(const tftp_context_p context);
static void TFTP_Int_Err_Handler(tftp_context_p context, uint16_t err_rank);
static void TFTP_Open_File(tftp_context_p context);
static void TFTP_Process_Error(const tftp_context_p context);
static void TFTP_Reply_ACK(const tftp_context_p context);
static void TFTP_Rx_and_Deser(const tftp_context_p context);
static void TFTP_Serialize_and_Send(const tftp_context_p context);
static void TFTP_Wait_for_Ack(const tftp_context_p context);
static void TFTP_Write_File(const tftp_context_p context);

/**
  * @brief tftpd_run is a simple tftp server
  * @note  THREAD SAFE
  * @param  none
  * @retval none
  */
void tftpd_run(void)
{
/* initialize context */
#ifdef	TFTP_ERR_STATS
//	xprintf("send errors: %d\n", send_err_errors);
#endif
#ifdef	TFTP_DEBUG_PRINT
//	xputs("enter tftpd_run..\n");
	t0 = 0U;
	t1 = 0U;
#endif

	context1 = &tftp_context;
	TFTP_Init_Context(context1);
	context1->in_sock = bind_socket(MAKE_IP(192, 168, 0, 1),	/* remote IP */
					   0,			/* remote port */
					   TFTP_PORT,		/* local port */
					   SOC_MODE_READ);	/* mode */
	if ( context1->in_sock == NULL ) { goto fExit; }	// EXIT
/* read socket	*/
	uint16_t	payload;
	payload = read_socket_nowait(context1->in_sock, \
				     tftp_buffer, \
				     (int32_t)TFTP_BUFFER_SIZE);
	if ( payload == 0 ) {
		TFTP_Int_Err_Handler(context1, ERR_RANK_CIS);
		goto fExit; }
	if (TFTP_Deserialize_Packet(tftp_buffer, \
				    (size_t)payload, \
				    context1) == ERROR) {
		TFTP_Int_Err_Handler(context1, ERR_RANK_CIS);
		goto fExit;	/* the packet is not an TFTP packet */
	}
/* a valid tftp packet has arrived */
/* now it's time to catch an socket for writing */
	context1->out_sock = bind_socket(context1->in_sock->rem_ip_addr,	/* remote IP */
					context1->in_sock->rem_port,	/* remote port */
					0U,				/* local port */
					SOC_MODE_WRITE);		/* mode */
	if (context1->out_sock == NULL) {
		TFTP_Int_Err_Handler(context1, ERR_RANK_CIS | ERR_RANK_CF);
		TFTP_Check_for_Abort(context1); // hang and reboot the system in case of severe error!
		goto fExit;
	}
/* rebind the in_soc local port from tftp port to out_soc local port */
	taskENTER_CRITICAL();
	context1->in_sock->loc_port = context1->out_sock->loc_port;
	taskEXIT_CRITICAL();

	if ( (context1->OpCode != TFTP_RRQ) && (context1->OpCode != TFTP_WRQ) ){
		/* illegal opcode */
		context1->ErrCode = TFTP_ERROR_ILLEGAL_OPERATION;
		context1->State = TFTP_STATE_IDLE;
		TFTP_Process_Error(context1);
		TFTP_Int_Err_Handler(context1, ERR_RANK_CIS | ERR_RANK_COS);
		TFTP_Check_for_Abort(context1); // hang and reboot the system in case of severe error!
		goto fExit;
	}
	TFTP_Open_File(context1);
	if (context1->ErrCode != TFTP_ERROR_NOERROR) {
		TFTP_Process_Error(context1);
		TFTP_Int_Err_Handler(context1, ERR_RANK_CIS | ERR_RANK_COS);
		TFTP_Check_for_Abort(context1); // hang and reboot the system in case of severe error!
		goto fExit;
	}
	if (context1->OpCode == TFTP_RRQ) {
/* reading here */
		context1->OpCode = TFTP_DATA;
		context1->State = TFTP_STATE_SENDING;
		context1->BlockNum = 0U;
		do {
			TFTP_Block_Read(context1);
#ifdef	TFTP_DEBUG_PRINT
			xprintf("bl rd:%d\n", context1->BlockNum);

#endif
			if (context1->ErrCode != TFTP_ERROR_NOERROR) {
				TFTP_Process_Error(context1);
				TFTP_Int_Err_Handler(context1, (ERR_RANK_CF | ERR_RANK_CIS | ERR_RANK_COS));
				TFTP_Check_for_Abort(context1); // hang and reboot the system in case of severe error!
				goto fExit;
			}
#ifdef	TFTP_DEBUG_PRINT
			xputs("->S&S\n");
#endif
			TFTP_Serialize_and_Send(context1);	/* send the packet */
			if (context1->State ==  TFTP_STATE_ERR_ABORT) {
				TFTP_Int_Err_Handler(context1, (ERR_RANK_CF | ERR_RANK_CIS | ERR_RANK_COS));
				TFTP_Check_for_Abort(context1); // hang and reboot the system in case of severe error!
				goto fExit;
			}
#ifdef	TFTP_DEBUG_PRINT
			xputs("S&S->\n");
#endif
/*waiting for ACK */
			TFTP_Wait_for_Ack(context1);
			if (context1->State !=  TFTP_STATE_SENDING) {
				TFTP_Int_Err_Handler(context1, (ERR_RANK_CF | ERR_RANK_CIS | ERR_RANK_COS));
				TFTP_Check_for_Abort(context1); // hang and reboot the system in case of severe error!
				goto fExit;
			}
		} while (context1->DataLen == TFTP_DATA_LEN_MAX);
	} else {
/* writing here */
		context1->BlockNum = 0U;
		TFTP_Reply_ACK(context1);
		if (context1->State == TFTP_STATE_ERR_ABORT) {
			TFTP_Int_Err_Handler(context1, (ERR_RANK_CF | ERR_RANK_CIS | ERR_RANK_COS));
			TFTP_Check_for_Abort(context1); // hang and reboot the system in case of severe error!
			goto fExit;
		}
/* receving frames */
#ifdef	TFTP_DEBUG_PRINT
		xputs("data receiving started..\n");
#endif
		do {
			TFTP_Rx_and_Deser(context1);
			if (context1->State !=  TFTP_STATE_RECEIVING) {
				TFTP_Int_Err_Handler(context1, (ERR_RANK_CF | ERR_RANK_CIS | ERR_RANK_COS));
				TFTP_Check_for_Abort(context1); // hang and reboot the system in case of severe error!
				goto fExit;
			}
		} while (context1->DataLen == TFTP_DATA_LEN_MAX);
		/* all OK here */
		/* file is successfully received */
	}
/* normal exit using error handler */
	TFTP_Int_Err_Handler(context1, (ERR_RANK_CIS | ERR_RANK_COS | ERR_RANK_CF));
	TFTP_Check_for_Abort(context1); // hang the system if severe error!

#ifdef	TFTP_ERR_STATS
	xprintf("send errors: %d\n", send_err_errors);
#endif
fExit:
	return;
} /* end of the function  tftpd_run */

/**
  * @brief  TFTP_Rx_and_Deser receives and deserializes an tftp packet
  * @note   also it sends an ACK or ERROR back
  * @param  tftp_context_p context
  * @retval none
  */
static void TFTP_Rx_and_Deser(const tftp_context_p context)
{
	tftp_context_t	temp_context;
//	uint8_t		spare_buffer[TFTP_OPCODE_LEN+ \
//				     TFTP_BLKNUM_LEN+ \
//				     TFTP_MAX_ERRMSG_LEN+1U]; /* spare buffer only for ack */
	TickType_t	entree_time;
	entree_time = xTaskGetTickCount();
	const TickType_t xTimeOut = pdMS_TO_TICKS(TFTP_TIMEOUT_MSECS);
	size_t		payload;
	ErrorStatus	Deser_result;
	uint16_t	blkn;
	blkn = context->BlockNum;		/* last acknoweleged block number */
	uint8_t		NumRetries = TFTP_MAX_RETRIES;

	do {
		NumRetries--;
		do {
#ifdef	TFTP_DEBUG_PRINT
			t1 = HAL_GetTick() - t0;
#endif
			context->State = TFTP_STATE_RECEIVING;
			payload = (size_t)read_socket_nowait(context->in_sock, \
								tftp_buffer, \
								(int32_t)TFTP_BUFFER_SIZE);
			if ( payload != 0U ) {
				break;
			}
			context->State = TFTP_STATE_RECEIVING_ERROR;
			taskYIELD();
		} while ( xTaskGetTickCount() < (entree_time + xTimeOut) );
		Deser_result = TFTP_Deserialize_Packet(tftp_buffer, \
							(size_t)payload, \
							context);
		if (Deser_result == SUCCESS) {
			if ( (context->OpCode == TFTP_DATA) && (context->BlockNum == (blkn + 1U)) ) {
/* data packet has arrived */
				temp_context = *context;
				TFTP_Write_File(context);
#ifdef	TFTP_DEBUG_PRINT
				t0 = HAL_GetTick();
#endif
				TFTP_Reply_ACK(&temp_context);
				context->fstate = temp_context.fstate;
				context->ErrCode = temp_context.ErrCode;
				context->State = temp_context.State;
				break;
			}
		}
		if ( ((Deser_result == SUCCESS) &&
		      (context->OpCode == TFTP_DATA) &&
		      (context->BlockNum == blkn )) || (payload == 0U) ) {
/* resend ack for previous packet */
			temp_context = *context;
			TFTP_Reply_ACK(&temp_context);
#ifdef	TFTP_DEBUG_PRINT
			xprintf("reack block #%d\n",temp_context.BlockNum );
#endif
			context->fstate = temp_context.fstate;
			context->ErrCode = temp_context.ErrCode;
			context->State = temp_context.State;
			continue;	/* an extrnal loop do - while */
		}
		/* not data packet with num or num-1 */
		context->State = TFTP_STATE_RECEIVING_ERROR;
		break;
	}  while ( NumRetries > 0U );
}
/* end of the function  TFTP_Rx_and_Deser */

/**
  * @brief
  * @note
  * @param
  * @param
  * @param
  * @retval none
  */
static void TFTP_Write_File(const tftp_context_p context)
{
#ifdef	TFTP_DEBUG_PRINT
	xprintf("\n\tRx bl: %d\n", context->BlockNum);
#endif
	return;
}
/* end of the function  */



/**
  * @brief  TFTP_Reply_ACK sends back an ack packet
  * @note
  * @param  tftp_context_p context
  * @retval tftp_context_p context
  */
static void TFTP_Reply_ACK(const tftp_context_p context)
{
	context->State = TFTP_STATE_SENDING_ACK;
	context->OpCode = TFTP_ACK;
	TFTP_Serialize_and_Send(context);
	context->State = (context->State == TFTP_STATE_ERR_ABORT ) ? TFTP_STATE_ERR_ABORT : TFTP_STATE_RECEIVING;
	return;
}
/* end of the function TFTP_Reply_ACK */

/**
  * @brief  TFTP_Wait_for_Ack waits for a valid ack packet
  * @note
  * @param  tftp_context_p context
  * @retval none
  */
static void TFTP_Wait_for_Ack(const tftp_context_p context)
{
#ifdef	TFTP_DEBUG_PRINT
	uint8_t		resends = 0u;
#endif
#ifdef	TFTP_DEBUG_PRINT
			xputs("->WFA\n");
#endif
	tftp_context_t	temp_context;
	temp_context = *context;	/* save current context with the last sent data packet */
	/* tftp_buffer also has last serialized data packet */
	uint8_t		spare_buffer[TFTP_OPCODE_LEN+ \
				     TFTP_BLKNUM_LEN+ \
				     TFTP_MAX_ERRMSG_LEN+1U]; /* spare buffer only for ack */
	TickType_t	entree_time;
	TickType_t xTimeOut = pdMS_TO_TICKS(TFTP_TIMEOUT_MSECS);
	size_t	payload;
	ErrorStatus	Deser_result;
	uint8_t		NumRetries = TFTP_MAX_RETRIES;

do {
	entree_time = xTaskGetTickCount();
	NumRetries--;
	do {
#ifdef	TFTP_DEBUG_PRINT
			t0 = HAL_GetTick();
#endif
		payload = (size_t)read_socket_nowait(temp_context.in_sock, \
						     spare_buffer, \
						     sizeof(spare_buffer));
		if (payload != 0U) {
			break;
		}
			context->State = TFTP_STATE_RECEIVING_ERROR;
			taskYIELD();
		}
	 while ( xTaskGetTickCount() < (entree_time + xTimeOut) );

/* here the payload != 0 */
		Deser_result = TFTP_Deserialize_Packet(spare_buffer, payload, &temp_context);

		if ( (Deser_result == SUCCESS) && \
			(temp_context.OpCode == TFTP_ACK) && \
			(context->BlockNum == temp_context.BlockNum) ) {
/* we've got the ACK */
			 context->State = TFTP_STATE_SENDING;
#ifdef	TFTP_DEBUG_PRINT
				t1 = HAL_GetTick();
				xprintf("got the ack =%d ", temp_context.BlockNum);
				xprintf("dt= %d\n", (t1 - t0));
#endif
			 goto fExit;
		}

		if ( (Deser_result == SUCCESS) && \
			(temp_context.OpCode == TFTP_ERR) ) {
/* reply with the ERR */
#ifdef	TFTP_DEBUG_PRINT
				xputs("reply with err\n");
#endif
			TFTP_Serialize_and_Send(&temp_context);
			*context = temp_context;
			context->State = TFTP_STATE_RECEIVING_ERROR;
			goto fExit;
		}
/* resend last data packet*/
#ifdef	TFTP_DEBUG_PRINT
			resends++;
			xputs("resend\n");
#endif
		TFTP_Serialize_and_Send(context);
		if (context->State == TFTP_STATE_ERR_ABORT) {
			goto fExit;
		}
	} while (NumRetries > 0U);

fExit:
#ifdef	TFTP_DEBUG_PRINT
			xputs("WFA->\n");
#endif
	return;
}

/**
  * @brief  TFTP_Serialize_and_Send serializes an TFTP packet and sends it
  * @note
  * @param  tftp_context_p context
  * @retval none
  */
static void TFTP_Serialize_and_Send(const tftp_context_p context)
{
	uint32_t	outpayload;
	outpayload = TFTP_Serialize_Packet(tftp_buffer, TFTP_BUFFER_SIZE, context);
	if (outpayload == UINT32_MAX) { /*error during serialization */
#ifdef	TFTP_ERR_STATS
		send_err_errors++;
#endif
		context->State = TFTP_STATE_ERR_ABORT;		/* reset the state machine */
		goto fExit;
	}
	if (write_socket(context->out_sock, tftp_buffer, (int32_t)outpayload) == ERROR) {
#ifdef	TFTP_ERR_STATS
		send_err_errors++;
#endif
		context->State = TFTP_STATE_ERR_ABORT;		/* reset the state machine */
	}
fExit:
	return;
}
/* end of the function TFTP_Serialize_and_Send */

/**
  * @brief   TFTP_Process_Error sends back an error packet
  * @note
  * @param  context related to the error
  * @retval ERROR or SUCCESS
  */
static void TFTP_Process_Error(const tftp_context_p context)
{
	/* send error */
	ErrorStatus	result;
	result = SUCCESS;
	context->OpCode = TFTP_ERR;
	switch (context->ErrCode) {
		case TFTP_ERROR_FILE_NOT_FOUND:
			strcpy((char*)context->ErrMsg, "No file");
			break;
		case TFTP_ERROR_ACCESS_VIOLATION:
			strcpy((char*)context->ErrMsg, "Acc.err.");
			break;
		case TFTP_ERROR_DISK_FULL:
			strcpy((char*)context->ErrMsg, "Diskfull");
			break;
		case TFTP_ERROR_ILLEGAL_OPERATION:
			strcpy((char*)context->ErrMsg, "Ill.op.");
			break;
		case TFTP_ERROR_UNKNOWN_TRFR_ID:
			strcpy((char*)context->ErrMsg, "XfrIDErr");
			break;
		case TFTP_ERROR_FILE_EXISTS:
			strcpy((char*)context->ErrMsg, "F.exists");
			break;
		case TFTP_ERROR_NO_SUCH_USER:
			strcpy((char*)context->ErrMsg, "Bad user");
			break;
		case TFTP_ERROR_NOERROR:
		default:
			result = ERROR;
			break;
	}
	if (result != SUCCESS) {
#ifdef	TFTP_ERR_STATS
		send_err_errors++;
#endif
		goto fExit;
	}
	TFTP_Serialize_and_Send(context);
fExit:
	return;
}

/**
  * @brief TFTP_Open_File opens a file
  * @note
  * @param  tftp_context_p context
  * @retval tftp_context_p context
  */
static void TFTP_Open_File(tftp_context_p context)
{
#define	DIR_ENTRIES	2
	const	char	*directory[DIR_ENTRIES] = { "settings.bin" , "image.bin" };
	const	char	*mode = "OCTET";

	if ( (context->fstate == FS_OPENED_READ) || (context->fstate == FS_OPENED_WRITE) ) {
		context->ErrCode = TFTP_ERROR_ACCESS_VIOLATION;	/* file is already opened */
		goto fExit;
	}
	uint8_t	*dst;
	dst = (uint8_t*)context->TFTP_Mode;
	{
	uint8_t	fl;
		fl = (uint8_t)TFTP_MAX_MODE_LEN;
		while ( (*dst != 0x00U) && (fl > 0U) ) {
			*dst = *dst  & (uint8_t)0xDFU;
			dst++; fl--;
		}
		if (strcmp(mode, context->TFTP_Mode) != 0) {
			context->ErrCode = TFTP_ERROR_ILLEGAL_OPERATION;	/* only octet is supported  */
			goto fExit;
		}
	}

	uint8_t		d;
	context->ErrCode = TFTP_ERROR_FILE_NOT_FOUND;	/* no such file */
	for (d = 0; d < (uint8_t)DIR_ENTRIES; d++) {
		if (strcmp(directory[d], context->FileName) == 0) {
		/* the file is the file */
			context->ErrCode = TFTP_ERROR_NOERROR;
			context->FilePos = 0U;
#ifdef	TFTP_STRESS_TEST
			context->FileSize = fsize;
#else
			context->FileSize = (d == 0U) ? sizeof(settings_t) : (size_t)( FLASH_BANK1_END - FLASH_BASE + 1 );
#endif
			if (context->OpCode == TFTP_RRQ) {
				context->fstate = FS_OPENED_READ;
			} else if ( context->OpCode == TFTP_WRQ) {
				context->fstate = FS_OPENED_WRITE;
			} else {
				/* file mode unknown */
				context->ErrCode = TFTP_ERROR_ILLEGAL_OPERATION;
			}
			break;
		}
	}
fExit:
	return;
#undef	DIR_ENTRIES
}

/**
  * @brief TFTP_Close_File closes the previously opened file
  * @note
  * @param  handle is a file handle
  * @retval NULL in the case of SUCCESS or file handle if file is not closed
  */
static void TFTP_Close_File(tftp_context_p context)
{
	if ( (context->fstate == FS_OPENED_READ) || (context->fstate == FS_OPENED_WRITE) ) {
		context->fstate = FS_CLOSED;
		context->FilePos = 0x00U;
		context->ErrCode = TFTP_ERROR_NOERROR;
	} else {
		context->ErrCode = TFTP_ERROR_ILLEGAL_OPERATION;
	}
	return;
}

/**
  * @brief  initializes all the internal structures
  * @note
  * @param  none
  * @retval none
  */
static void TFTP_Init_Context(const tftp_context_p context)
{
#ifdef	TFTP_ERR_STATS
	send_err_errors = 0U;
#endif
	/* initialize tftp context */
	context->BlockNum = 0U;
	context->DataLen = 0U;
	context->DataPtr = NULL;
	context->ErrCode = TFTP_ERROR_NOERROR;
	context->ErrMsg[0] = '\0';
	context->FileName[0] = '\0';
	context->FilePos = 0U;
	context->FileSize = 0U;
	context->OpCode = 0U;
	context->PacketLen = 0U;
	context->TFTP_Mode[0] = '\0';
	context->fstate = FS_CLOSED;
	context->State = TFTP_STATE_IDLE;
	context->in_sock = NULL;
	context->out_sock = NULL;
}

/**
     * @brief  TFTP_Int_Err_Handler handles the internal TFTP and socket errors
     * @note
     * @param  tftp_context_p context of the tftp instance
     * @param  err_rank_t err_rank the rank of the error
     * @retval none
     */
static void TFTP_Int_Err_Handler(tftp_context_p context, uint16_t err_rank)
	{
	if ( (err_rank & ERR_RANK_CF) != 0U ) {
		TFTP_Close_File(context);
	}
	if ( (err_rank & ERR_RANK_CIS) != 0U ) {
		if ( close_socket(context->in_sock) != NULL) {
			context->State = TFTP_STATE_ERR_ABORT;
		};
	}
	if ( (err_rank & ERR_RANK_COS) != 0U ) {
		if ( close_socket(context->out_sock) != NULL) {
			context->State = TFTP_STATE_ERR_ABORT;
		};
	}
	return;
}

/**
  * @brief TFTP_Check_for_Abort must kill the entrie tftp task
  * @note
  * @param  tftp_context_p context
  * @retval none
  */
static void TFTP_Check_for_Abort(const tftp_context_p context)
{
	if (context->State == TFTP_STATE_ERR_ABORT) {
		UNUSED(0);
	}
	return;
}
/* end of the function TFTP_Check_for_Abort  */

/**
  * @brief  TFTP_Block_Read reads a portion of data fron the file
  * @note
  * @param  tftp_context_p context
  * @retval tftp_context_p context
  */
static void TFTP_Block_Read(const tftp_context_p context)
{
	if (context->fstate != FS_OPENED_READ) {
		context->ErrCode = TFTP_ERROR_ILLEGAL_OPERATION;
		goto fExit;
	}
	size_t	stopval;
	stopval = ((context->FilePos + TFTP_DATA_LEN_MAX)  < context->FileSize) ? \
			TFTP_DATA_LEN_MAX : (context->FileSize - context->FilePos);
	context->DataLen = stopval;
#ifdef	TFTP_STRESS_TEST
	context->FilePos += stopval;
#endif
	context->DataPtr = (uint8_t*)(&tftp_buffer[0] + (ptrdiff_t)(TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN));
	uint8_t	* dptr;
	dptr = context->DataPtr;
#ifdef	TFTP_STRESS_TEST
	while (stopval > 0U) {
		*dptr = (uint8_t)stopval;	// dummy data
		dptr++;
		stopval--;
	}
	context->BlockNum++;
#else
	if ( context->FileSize == sizeof(settings_t) ) {
	/* settings */
		if (stopval != 0U) {
			if ( Read_FRAM(dptr, context->FilePos, stopval) == SUCCESS ) {
				context->FilePos += stopval;
				context->BlockNum++;
			} else {
				context->ErrCode = TFTP_ERROR_ACCESS_VIOLATION;
			}

		} else {
			context->BlockNum++;
		}
	} else {
		/* ROM image */
		uintptr_t sptr;
		sptr = (uintptr_t)(FLASH_BASE + context->FilePos);
		if (stopval != 0U) {
			memcpy(dptr, (uint8_t*)sptr , stopval);
			context->FilePos += stopval;
		}
		context->BlockNum++;
	}
#endif
fExit:
	return;
}
/* end of the function  TFTP_Block_Read */


/* ########################### EOF ############################################################## */
