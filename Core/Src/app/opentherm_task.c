/** @file opentherm_task.c
*  @brief opentherm task
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 29-06-2019
 */

#include "watchdog.h"
#include "lan.h"
#include "logging.h"
#include "task_tokens.h"

#include "config_files.h"
#include "tiny-fs.h"
#include "file_io.h"
#include "ascii_helpers.h"
#include "ip_helpers.h"
#include "messages.h"

#include "opentherm_task.h"
#include "opentherm.h"
#include "hex_gen.h"

#include "manchester_task.h"

#ifdef MASTERBOARD
/* to do not include opentherm_master.h */
extern tMV * OPENTHERM_getMV_for_Pub(size_t i);
extern tMV * OPENTHERM_getControllableMV(size_t i);
extern openThermResult_t OPENTHERM_InitMaster(void);
extern openThermResult_t OPENTHERM_ReadSlave(tMV *const pMV,
				      uint32_t (*commFun)(uint32_t));
extern openThermResult_t OPENTHERM_WriteSlave(tMV *const pMV,
				       uint32_t (*commFun)(uint32_t));
extern tMV * OPENTHERM_findMVbyLDID(ldid_t ldid);

extern const Media_Desc_t Media0;
void static create_pub_MV_file(void);
static ErrorStatus configMVs(void);

#elif SLAVEBOARD
extern tMV * OPENTHERM_getMV(size_t i);
extern openThermResult_t OPENTHERM_InitSlave(void);
extern openThermResult_t OPENTHERM_SlaveRespond(uint32_t msg,
					 uint32_t (*commFun)(uint32_t),
					 tMV *(*GetMV)(uint8_t));

#else
#error MASTERBOARD or SLAVEBOARD not defined
#endif

/* messages */
#ifdef MASTERBOARD
static char *task_name = "opentherm_task (master)";
#elif SLAVEBOARD
static char *task_name = "opentherm_task (slave)";
#else

#endif

extern osThreadId OpenThermTaskHandle;
TaskHandle_t TaskToNotify_afterTx = NULL;
TaskHandle_t TaskToNotify_afterRx = NULL;
extern osThreadId ManchTaskHandle;

bool	opentherm_configured = false;

/**
 * @brief comm_func function used by opentherm to communicate with the slave
 * @param val the value to be sent to the slave
 * @return value received from the slave
 */
static uint32_t comm_func(uint32_t val)
{
	static uint32_t notif_val = 0U;
	uint32_t retVal = val;
	static const char * got_notif_tx_err = "got notification tx error.";
	static const char * got_notif_tx_ok = "got notification tx OK.";
	static const char * got_notif_rx_err = "got notif. rx error.";
	static const char * got_notif_rx_ok = "got notif. rx OK.";


	*(uint32_t *)(&Tx_buf[0]) = val;

	xTaskNotify(ManchTaskHandle, MANCHESTER_TRANSMIT_NOTIFY,
		    eSetValueWithOverwrite);

	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		if (notif_val == (ErrorStatus)ERROR) {
			log_xputs(MSG_LEVEL_PROC_ERR, got_notif_tx_err);
		} else {
			log_xputs(MSG_LEVEL_INFO, got_notif_tx_ok);

		}
	}

	vTaskDelay(pdMS_TO_TICKS(20U));

	*(uint32_t *)(&Rx_buf[0]) = 0U;

	xTaskNotify(ManchTaskHandle, MANCHESTER_RECEIVE_NOTIFY,
		    eSetValueWithOverwrite);

	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		if (notif_val == (ErrorStatus)ERROR) {
			log_xputs(MSG_LEVEL_PROC_ERR, got_notif_rx_err);
			retVal = 0U;

		} else {
			log_xputs(MSG_LEVEL_INFO, got_notif_rx_ok);
			retVal = *(uint32_t *)(&Rx_buf[0]);
			log_xprintf(MSG_LEVEL_EXT_INF, "Received: %d", retVal);
		}
	}
	return retVal;
}

/**
 * @brief opentherm_task_init initializes opentherm driver
 */
void opentherm_task_init(void)
{
	register_magic(OPENTHERM_TASK_MAGIC);
	i_am_alive(OPENTHERM_TASK_MAGIC);
	messages_TaskInit_started();
#ifdef MASTERBOARD
	if (OPENTHERM_InitMaster() != OPENTHERM_ResOK) {
#elif SLAVEBOARD
	if (OPENTHERM_InitSlave() != OPENTHERM_ResOK) {
#else
#endif
		messages_TaskInit_fail();
		vTaskDelay(pdMS_TO_TICKS(portMAX_DELAY));
	} else {
		messages_TaskInit_OK();
		TaskToNotify_afterTx = OpenThermTaskHandle;
		TaskToNotify_afterRx = OpenThermTaskHandle;
	/* config file workaround */
#ifdef MASTERBOARD
		while (configMVs() != SUCCESS) {
			/* error configuration read */
			log_xputs(MSG_LEVEL_TASK_INIT, "Existing PUB_MV file configuration failed! Creating a new PUB_MV.");
			create_pub_MV_file();
		}
		log_xputs(MSG_LEVEL_TASK_INIT, "Report types for MVs are set up!");
#endif
	}
	opentherm_configured = true;
}

#ifdef MASTERBOARD
/**
 * @brief opentherm_task_run task function
 */
void opentherm_task_run(void)
{
	/* let's start */
	openThermResult_t	res;
	for (size_t i = 0U; i < MV_ARRAY_LENGTH; i++) {
		i_am_alive(OPENTHERM_TASK_MAGIC);

	/* 1. get controllable MV */
		tMV *pMV = OPENTHERM_getControllableMV(i);
		if (pMV == NULL) {
			log_xputs(MSG_LEVEL_PROC_ERR, "OPENTHERM_getControllableMV ERROR: pMV == NULL");
			continue;
		}
	/* 2. write to the slave */
		res = OPENTHERM_WriteSlave(pMV, comm_func);
		log_xputs(MSG_LEVEL_INFO, openThermErrorStr(res));

	/* 3. get publicable MV */
		pMV = OPENTHERM_getMV_for_Pub(i);
		if (pMV == NULL) {
			log_xputs(MSG_LEVEL_PROC_ERR, "OPENTHERM_getMV_for_Pub ERROR: pMV == NULL");
			continue;
		}
	/* 4. read slave  */
		res = OPENTHERM_ReadSlave(pMV, comm_func);
		log_xputs(MSG_LEVEL_INFO, openThermErrorStr(res));
	}
}

#elif SLAVEBOARD
void opentherm_task_run(void)
{
	i_am_alive(OPENTHERM_TASK_MAGIC);
}
#else
#endif

#ifdef MASTERBOARD

/* template MV config string */
static const char template_str[] = "00000:SPGI\n";
static const size_t rec_len = sizeof(template_str);
static const char sp_[] = "SP__\n";
static const char gi_[] = "__GI\n";
static const char spgi_[] = "SPGI\n";
static const char no_[] = "NO__\n";

/**
 * @brief configMVs configures ReportType field of every MV
 * @return
 */
static ErrorStatus configMVs(void)
{
	ErrorStatus retVal = ERROR;
	size_t	bwr;	/* bytes was read */
	char read_rec[rec_len];
	size_t	pos = 0U;
	size_t configured = 0U;

	do {
		if ( (ReadBytes(&Media0,
				"PUB_MV",
				pos,
				rec_len,
				&bwr,
				(uint8_t*)&read_rec) == FR_OK) && (bwr == rec_len) ){
			/* one record was read */
			/* parse record */
			enum tReport rep_type;

			if (strcmp(sp_, &read_rec[6]) == 0) {
				rep_type = SP;
			} else if ((strcmp(gi_, &read_rec[6]) == 0)) {
				rep_type = GI;
			} else if ((strcmp(spgi_, &read_rec[6]) == 0)) {
				rep_type = SP_GI;
			} else if ((strcmp(no_, &read_rec[6]) == 0)) {
				rep_type = Inact;
			} else {
				/* error */
				break;
			}

			ldid_t ldid;
			ldid = (ldid_t)adec2uint16(read_rec, 5U);

			/* find relevant MV */
			tMV * targetMV = OPENTHERM_findMVbyLDID(ldid);
			if (targetMV != NULL) {
				targetMV->ReportType = rep_type;
			} else {
				/* error */
				break;
			}
			pos = pos + rec_len;
			configured++;

		} else {
			break;
		}
	} while (bwr == rec_len);
	if (configured == MV_ARRAY_LENGTH) {
		retVal = SUCCESS;
	}
	return retVal;
}

/**
 * @brief create_pub_MV_file creates new PUB_MV file
 */
static void create_pub_MV_file(void)
{
	char	outstr[rec_len];

	strncpy((char*)outstr, template_str, sizeof(template_str));

	size_t estimated_fsize = MV_ARRAY_LENGTH * (sizeof(template_str));
	const char * pub_mv = "PUB_MV";

	if (DeleteFile(&Media0, pub_mv) != FR_OK) {

		log_xputs(MSG_LEVEL_SERIOUS, "Can not delete file PUB_MV");
	}

	if (AllocSpaceForFile(&Media0, pub_mv,estimated_fsize) != FR_OK) {
		log_xputs(MSG_LEVEL_SERIOUS, " file PUB_MV creation error!");
	} else {
	/* write entry by entry */
		size_t fpos = 0U;
		size_t bw = 0U;
		size_t btw = sizeof(template_str);

		for (size_t i = 0; i < MV_ARRAY_LENGTH; i++) {

			tMV * pMV = OPENTHERM_getMV_for_Pub(i);

			uint16_to_asciiz((uint16_t)pMV->LD_ID, (char*)outstr);

			if(pMV->ReportType == GI) {
				strncpy(&outstr[6], gi_, sizeof(gi_));
			} else if (pMV->ReportType == SP) {
				strncpy(&outstr[6], sp_, sizeof(sp_));
			} else if (pMV->ReportType == SP_GI) {
				strncpy(&outstr[6], spgi_, sizeof(spgi_));
			} else {
				strncpy(&outstr[6], no_, sizeof(no_));
			}

			if ((WriteBytes(&Media0, pub_mv, fpos, btw, &bw, (const uint8_t*)outstr) != FR_OK) ||
				(bw != btw) ) {
				log_xputs(MSG_LEVEL_SERIOUS, "PUB_MV file write error!");
				break;
			} else {
				fpos = fpos + btw;
			}
		}

	}
}
#endif
