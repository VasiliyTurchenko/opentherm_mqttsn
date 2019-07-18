/**
  ******************************************************************************
  * @file    TFTP_data.h
  * @author  turchenkov@gmail.com
  * @date    11-Mar-2018
  * @brief   Data structures definitions for the tftp protocol functions
  ******************************************************************************
  * @attention use at your own risk
  ******************************************************************************
  */

#include <stdint.h>
#ifdef STM32F103xB
#include "stm32f1xx.h"
#elif STM32F303xC
#include "stm32f3xx.h"
#else

#error "MCU TARGET NOT DEFINED!"

#endif

#include "lan.h"

#include "tiny-fs.h"

/* TFTP config defines */

#define TFTP_MAX_MODE_LEN	7U
#define TFTP_MAX_RETRIES	5U
#define TFTP_TIMEOUT_MSECS	5000U
#define TFTP_TIMER_MSECS	10U
#define TFTP_MAX_FILENAME_LEN   ((MAX_FILENAME_LEN) + 1)
#define	TFTP_MAX_ERRMSG_LEN	8U

#define	TFTP_PORT		69U		/* local tftp server port */

#define TFTP_OPCODE_LEN         2U
#define TFTP_BLKNUM_LEN         2U
#define TFTP_ERRCODE_LEN        2U

#ifdef MASTERBOARD
#define TFTP_DATA_LEN_MAX       512U
#elif SLAVEBOARD
//#define TFTP_DATA_LEN_MAX       128U
#define TFTP_DATA_LEN_MAX       ((FS_CLUSTER_SIZE) * 8)
#else
	#error "Define MASTERBOARD or SLAVEBOARD"
#endif

#define TFTP_DATA_PKT_HDR_LEN   ( TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN )
#define TFTP_ERR_PKT_HDR_LEN    ( TFTP_OPCODE_LEN + TFTP_ERRCODE_LEN )
#define TFTP_ACK_PKT_LEN        ( TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN )
#define TFTP_DATA_PKT_LEN_MAX   ( TFTP_DATA_PKT_HDR_LEN + TFTP_DATA_LEN_MAX )

/* TFTP operation codes */
#define	TFTP_RRQ 		0x0001U
#define	TFTP_WRQ		0x0002U
#define	TFTP_DATA		0x0003U
#define	TFTP_ACK		0x0004U
#define	TFTP_ERR		0x0005U

/* the ranks of the internal errors */
#define	ERR_RANK_CIS	(uint16_t)0x01U		/*!< need to close in_sock */
#define	ERR_RANK_COS	(uint16_t)0x02U		/*!< need to close out_sock */
#define	ERR_RANK_CF	(uint16_t)0x04U		/*!< need to close file */


/* TFTP error codes as specified in RFC1350  */
enum	tftp_errorcode	{
	TFTP_ERROR_NOERROR	     = 0,
	TFTP_ERROR_FILE_NOT_FOUND    = 1,
	TFTP_ERROR_ACCESS_VIOLATION  = 2,
	TFTP_ERROR_DISK_FULL         = 3,
	TFTP_ERROR_ILLEGAL_OPERATION = 4,
	TFTP_ERROR_UNKNOWN_TRFR_ID   = 5,
	TFTP_ERROR_FILE_EXISTS       = 6,
	TFTP_ERROR_NO_SUCH_USER      = 7
	};
typedef	enum tftp_errorcode	tftp_errorcode_t;

/* current states of a tftp machine */
enum    tftp_state      {
	TFTP_STATE_IDLE = 0,
	TFTP_STATE_RRQ,
	TFTP_STATE_WRQ,
	TFTP_STATE_SENDING,
	TFTP_STATE_RECEIVING,
	TFTP_STATE_SENDING_ERROR,
	TFTP_STATE_RECEIVING_ERROR,
	TFTP_STATE_SENDING_ACK,
	TFTP_STATE_RECEIVING_ACK,
	TFTP_STATE_ERR_ABORT
	};
typedef enum    tftp_state      tftp_state_t;


/* a context of the tftp operation */
typedef struct tftp_context_ {
	uint16_t		OpCode;		/*!< operation code as described in RFC1350 */
	char			FileName[TFTP_MAX_FILENAME_LEN+1];
	char			TFTP_Mode[TFTP_MAX_MODE_LEN+1];
	tftp_errorcode_t	ErrCode;
	char			ErrMsg[TFTP_MAX_ERRMSG_LEN+1];
	size_t			PacketLen;
	uint16_t		BlockNum;
	uint8_t	*		DataPtr;
	size_t			DataLen;
	tftp_state_t		State;
/* file handle */
	fHandle_t		file;
/* UDP sockets */
	socket_p		in_sock;	/*!< pointer to the input socket */
	socket_p		out_sock;	/*!< pointer to the output socket */
	} tftp_context_t;

typedef	tftp_context_t * tftp_context_p;



/* ################################### E.O.F. ################################################### */
