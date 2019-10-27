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
extern tMV *OPENTHERM_getMV_for_Pub(size_t i);
extern tMV *OPENTHERM_getControllableMV(size_t i);
extern openThermResult_t OPENTHERM_InitMaster(void);
extern openThermResult_t OPENTHERM_ReadSlave(tMV *const pMV,
					     uint32_t (*commFun)(uint32_t));
extern openThermResult_t OPENTHERM_WriteSlave(tMV *const pMV,
					      uint32_t (*commFun)(uint32_t));
extern tMV *OPENTHERM_findMVbyLDID(ldid_t ldid);

extern const Media_Desc_t Media0;

#if(0)
void static create_pub_MV_file(void);
static ErrorStatus configMVs(void);

static ErrorStatus customize_ctrl_MVs(void);
void static create_ctrl_MV_file(void);
#endif

static ErrorStatus configOpenTherm(void);
static void create_CFG_OT_file(void);


#elif SLAVEBOARD
#include "num_helpers.h"

extern tMV *OPENTHERM_getMV(size_t i);
extern openThermResult_t OPENTHERM_InitSlave(void);
extern openThermResult_t OPENTHERM_SlaveRespond(uint32_t msg,
						uint32_t (*commFun)(uint32_t),
						tMV *(*GetMV)(uint8_t));
extern tMV *OPENTHERM_GetSlaveMV(uint8_t DataId);
extern ErrorStatus DAQ_Update_MV(tMV *pMV, numeric_t rcvd_num);
/* imitates boiler */
static void update_imitator(void);

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

bool opentherm_configured = false;

#ifdef MASTERBOARD
/**
 * @brief comm_func function used by opentherm to communicate with the slave
 * @param val the value to be sent to the slave
 * @return value received from the slave
 */
static uint32_t master_comm_func(uint32_t val)
{
	static uint32_t notif_val = 0U;
	uint32_t retVal = val;
	static const char *got_notif_tx_err = "got notification tx error.";
	static const char *got_notif_tx_ok = "got notification tx OK.";
	static const char *got_notif_rx_err = "got notif. rx error.";
	static const char *got_notif_rx_ok = "got notif. rx OK.";

	static uint32_t precalculated_delay_ms = 0U;
	uint32_t t0;
	uint32_t t1;

	vTaskDelay(pdMS_TO_TICKS(precalculated_delay_ms));

	t0 = HAL_GetTick();

	*(uint32_t *)(&Tx_buf[0]) = val;

	xTaskNotify(ManchTaskHandle, MANCHESTER_TRANSMIT_NOTIFY,
		    eSetValueWithOverwrite);

	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		if (notif_val == (ErrorStatus)ERROR) {
			log_xputs(MSG_LEVEL_PROC_ERR, got_notif_tx_err);
		} else {
			log_xputs(MSG_LEVEL_PROC_ERR /*MSG_LEVEL_INFO*/,
				  got_notif_tx_ok);
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
			log_xprintf(MSG_LEVEL_PROC_ERR /*MSG_LEVEL_EXT_INF*/,
				    "Received: %d", retVal);
		}
	}
	t1 = HAL_GetTick();
	uint32_t dt = t1 - t0;
	precalculated_delay_ms = (dt > 1100U) ? 0U : (1100U - dt);

	return retVal;
}
#endif

#ifdef SLAVEBOARD

//#include "main.h"

//#define STROBE_1                                                               \
//	do {                                                                   \
//		HAL_GPIO_WritePin(ESP_PWR_GPIO_Port, ESP_PWR_Pin,    \
//				  GPIO_PIN_SET);                               \
//	} while (0)

//#define STROBE_0                                                               \
//	do {                                                                   \
//		HAL_GPIO_WritePin(ESP_PWR_GPIO_Port, ESP_PWR_Pin,    \
//				  GPIO_PIN_RESET);                             \
//	} while (0)

/**
 * @brief slave_comm_func
 * @param val
 * @return
 */
static ErrorStatus slave_comm_func_rx(uint32_t *rxd)
{
	static const char *got_notif_rx_err = "got notif. rx error.";
	static const char *got_notif_rx_ok = "got notif. rx OK.";
	ErrorStatus notif_val;
	ErrorStatus retVal = ERROR;

	*(uint32_t *)(&Rx_buf[0]) = 0x0BADF00DU;
	//STROBE_1;
	xTaskNotify(ManchTaskHandle, MANCHESTER_RECEIVE_NOTIFY,
		    eSetValueWithOverwrite);

	xTaskNotifyStateClear(NULL);
	static char hex_val[] = {"0x00000000"};

	if (xTaskNotifyWait(0x00U, ULONG_MAX, (uint32_t*)&notif_val,
			    pdMS_TO_TICKS(1000U)) == pdTRUE) {
		if (notif_val == (ErrorStatus)ERROR) {
			if (*(uint32_t *)(&Rx_buf[0]) == 0x0BADF00DU) {
				/* timeout */
				retVal = ERROR;
				*rxd = 0U;
			} else {
				log_xputs(MSG_LEVEL_PROC_ERR, got_notif_rx_err);
				retVal = ERROR;
				*rxd = *(uint32_t *)(&Rx_buf[0]);
			}

		} else {
			log_xputs(MSG_LEVEL_INFO, got_notif_rx_ok);
			retVal = SUCCESS;
			*rxd = *(uint32_t *)(&Rx_buf[0]);
			uint32_to_asciiz(*rxd, hex_val);
			log_xprintf(MSG_LEVEL_EXT_INF, "Received: %s", hex_val);
		}
	} else {
		/* we can't get here */
		log_xputs(MSG_LEVEL_FATAL, "slave_comm_func_rx() bad place!");
	}

	//STROBE_0;
	return retVal;
}

/**
 * @brief slave_comm_func_tx
 * @param val
 */
static uint32_t slave_comm_func_tx(uint32_t val)
{
	static uint32_t notif_val = 0U;
	static const char *got_notif_tx_err = "got notification tx error.";
	static const char *got_notif_tx_ok = "got notification tx OK.";

	*(uint32_t *)(&Tx_buf[0]) = val;

	static char hex_val[] = {"0x00000000"};

	//STROBE_1;

	xTaskNotify(ManchTaskHandle, MANCHESTER_TRANSMIT_NOTIFY,
		    eSetValueWithOverwrite);

	xTaskNotifyStateClear(NULL);
	if (xTaskNotifyWait(0x00U, ULONG_MAX, &notif_val, pdMS_TO_TICKS(64U)) ==
	    pdTRUE) {
		if (notif_val == (ErrorStatus)ERROR) {
			log_xputs(MSG_LEVEL_PROC_ERR, got_notif_tx_err);
		} else {
			log_xputs(MSG_LEVEL_INFO, got_notif_tx_ok);
			uint32_to_asciiz(val, hex_val);
			log_xprintf(MSG_LEVEL_INFO, "Sent %s", hex_val);
		}
	}
	//STROBE_0;
	return 0U;
}

#endif

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
		while (configOpenTherm() != SUCCESS) {
			/* error configuration read */
			log_xputs(
				MSG_LEVEL_TASK_INIT,
				"Existing CFG_OT file configuration failed! Creating a new CFG_OT.");
			create_CFG_OT_file();
		}
		log_xputs(MSG_LEVEL_TASK_INIT,
			  "Opentherm is set up!");
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
	openThermResult_t res;
	for (size_t i = 0U; i < MV_ARRAY_LENGTH; i++) {
		i_am_alive(OPENTHERM_TASK_MAGIC);

		/* 1. get controllable MV */
		tMV *pMV = OPENTHERM_getControllableMV(i);
		if ( (pMV != NULL) && \
			/* this is a second MV in the pair */
			/* this MV is already processed */
		     ((pMV->LD_ID & 0x100U) == 0U) ) {

			/* 2. write to the slave */
			res = OPENTHERM_WriteSlave(pMV, master_comm_func);
			log_xputs(MSG_LEVEL_INFO, openThermErrorStr(res));
		}
		/* 3. get publicable MV */
		pMV = OPENTHERM_getMV_for_Pub(i);
		if ( (pMV != NULL) && \
		     ((pMV->LD_ID & 0x100U) == 0U) ) {
			/* 4. read slave  */
			res = OPENTHERM_ReadSlave(pMV, master_comm_func);
			log_xputs(MSG_LEVEL_INFO, openThermErrorStr(res));
		}
	}
}

#elif SLAVEBOARD
void opentherm_task_run(void)
{
	i_am_alive(OPENTHERM_TASK_MAGIC);

	uint32_t rcvd;
	/* wait for incoming message */
	//STROBE_1;
	ErrorStatus rx_result;
	do {
		//		taskYIELD();
		rx_result = slave_comm_func_rx(&rcvd);
		i_am_alive(OPENTHERM_TASK_MAGIC);
	} while (rx_result != SUCCESS);

	/* something useful was received */
	/* standard requires 80 ... 800 ms delay before answer */
	vTaskDelay(pdMS_TO_TICKS(100U));

	openThermResult_t res;
	res = OPENTHERM_SlaveRespond(rcvd, slave_comm_func_tx,
				     OPENTHERM_GetSlaveMV);
	if (res != OPENTHERM_ResOK) {
		log_xputs(MSG_LEVEL_PROC_ERR, openThermErrorStr(res));
	} else {
		log_xputs(MSG_LEVEL_INFO, "OPENTHERM_SlaveRespond() OK.");
	}
	//STROBE_0;
	/* standard requires at least 100 ms delay after answer */
	vTaskDelay(pdMS_TO_TICKS(100U));

	/* */
	update_imitator();
}

/**
 * @brief sweep
 * @param ldid
 * @param step
 * @param inc
 */
static void sweep(ldid_t ldid, float step, bool * inc)
{
	tMV *targetMV = NULL;
	float fake_val;
	targetMV = OPENTHERM_GetSlaveMV((uint8_t)ldid);
	if (targetMV != NULL) {
		fake_val = targetMV->Val.fVal;
		if (*inc) {
			fake_val += step;
			if (fake_val >= targetMV->Highest.fVal) {
				*inc = false;
			}
		} else {
			fake_val -= step;
			if (fake_val <= targetMV->Lowest.fVal) {
				*inc = true;
			}
		}
		numeric_t setpoint = {FLOAT_VAL, {.f_val = fake_val}};
		if (DAQ_Update_MV(targetMV, setpoint) == SUCCESS) {
			log_xputs(MSG_LEVEL_INFO, "fake_temp was set");
		} else {
			log_xputs(MSG_LEVEL_PROC_ERR, "fake_temp set error");
		}
	}
}

/**
 * @brief update_imitator
 */
static void update_imitator(void)
{
	static const ldid_t boiler_temp_id = MSG_ID_TBOILER;
	static bool MSG_ID_TBOILER_inc = true;
	sweep(boiler_temp_id, 0.05f, &MSG_ID_TBOILER_inc);

	static const ldid_t outside_temp_id = MSG_ID_TOUTSIDE;
	static bool MSG_ID_TOUTSIDE_inc = true;
	sweep(outside_temp_id, 0.025f, &MSG_ID_TOUTSIDE_inc);

	static const ldid_t mod_level = MSG_ID_REL_MOD_LEVEL;
	static bool MSG_ID_REL_MOD_LEVEL_inc = true;
	sweep(mod_level, 0.001f, &MSG_ID_REL_MOD_LEVEL_inc);

	static const ldid_t tret = MSG_ID_TRET;
	static bool MSG_ID_TRET_inc = true;
	sweep(tret, 0.3f, &MSG_ID_TRET_inc);

}


#else
#endif

#ifdef MASTERBOARD


static const char template[] = "000:N/READ:GI/WRITE:N\n";
static const size_t t_len = sizeof(template);
static const char * filename = "CFG_OT";
static const size_t yn_pos = 4U;

/**
 * @brief configOpenTherm reads configuration file and setups parameters
 * of the OpenTherm app
 * @return ERROR or SUCCESS
 */
static ErrorStatus configOpenTherm(void)
{
/*
	format:
	DATA_ID:Y|N/READ:SP|GI|SG|NO/WRITE:Y|N
*/



	static const size_t read_pos = 5U;
	static const size_t spgi_pos = 11U;
	static const size_t write_pos = 13U;
	static const size_t wr_pos = 20U;

	static const char sp_[] = "SP";
	static const char gi_[] = "GI";
	static const char spgi_[] = "SG";
	static const char no_[] = "NO";

	static const char y_[] = "Y\n";
	static const char n_[] = "N\n";

//	static const char data_id[] = "000";
	static const char read[] = "/READ:";
	static const char write[] = "/WRITE:";

	ErrorStatus retVal = ERROR;
	size_t bwr; /* bytes was read */
	char read_rec[t_len];
	size_t pos = 0U;
	size_t configured = 0U;

	do {
		if ((ReadBytes(&Media0, filename, pos, t_len, &bwr,
			       (uint8_t *)&read_rec) == FR_OK) &&
		    (bwr == t_len)) {
			/* one record was read */
			/* parse record */
			uint8_t dataid = adec2byte(&read_rec[0], 3U);

			bool off = (read_rec[yn_pos] == 'N') ? true : false;

			if ( strncmp(read, &read_rec[read_pos], (sizeof(read) - 1U)) != 0 ) {
				break;
			}
			enum tReport rep_type;
			size_t to_cmp = sizeof (sp_) - 1U;
			if (strncmp(sp_, &read_rec[spgi_pos], to_cmp) == 0) {
				rep_type = SP;
			} else if ((strncmp(gi_, &read_rec[spgi_pos], to_cmp) == 0)) {
				rep_type = GI;
			} else if ((strncmp(spgi_, &read_rec[spgi_pos], to_cmp) == 0)) {
				rep_type = SP_GI;
			} else if ((strncmp(no_, &read_rec[spgi_pos], to_cmp) == 0)) {
				rep_type = Inact;
			} else {
				/* error */
				break;
			}

			if (strncmp(write, &read_rec[write_pos], (sizeof(write) - 1U)) != 0) {
				break;
			}
			enum tControllable ctrl_type;
			if (strcmp(y_, &read_rec[wr_pos]) == 0) {
				ctrl_type = Yes;
			} else if ((strcmp(n_, &read_rec[wr_pos]) == 0)) {
				ctrl_type = No;
			} else {
				/* error */
				break;
			}

			/* search for OT message */
			const opentThermMsg_t * msg = GetMessageTblEntry((ldid_t)dataid);
			if (msg == NULL) {
				break;
			}

			if ( (ctrl_type == Yes) && ((msg->msgMode != wr) && (msg->msgMode != rw)) ) {
				break; // can not write to the read-only entry
			}

			/* find one or two MVs*/
			/* find relevant MV */
			tMV *targetMV = OPENTHERM_findMVbyLDID((ldid_t)dataid);
			if (targetMV != NULL) {
				targetMV->ReportType = rep_type;
				targetMV->Ctrl = ctrl_type;
				targetMV->Off = off;
			} else {
				/* error */
				break;
			}
			if (msg->msgDataType2 != none ) {
				ldid_t second_id;
				second_id = (ldid_t)dataid | (ldid_t)0x100U;
				targetMV = OPENTHERM_findMVbyLDID(second_id);
				if (targetMV != NULL) {
					targetMV->ReportType = rep_type;
					targetMV->Ctrl = ctrl_type;
					targetMV->Off = off;
				} else {
					/* error */
					break;
				}
			}
			pos = pos + t_len;
			configured++;

		}  else {
			break;
		}
	} while (bwr == t_len);
	if (configured == MSG_TBL_LENGTH) {
		retVal = SUCCESS;
	}
	return retVal;
}


static void create_CFG_OT_file(void)
{
	char outstr[t_len];

	strncpy((char *)outstr, template, t_len);

	size_t estimated_fsize = MSG_TBL_LENGTH * t_len;

	if (DeleteFile(&Media0, filename) != FR_OK) {
		log_xputs(MSG_LEVEL_SERIOUS, "Can not delete file CFG_OT");
	}

	if (AllocSpaceForFile(&Media0, filename, estimated_fsize) != FR_OK) {
		log_xputs(MSG_LEVEL_SERIOUS, " file CFG_OT creation error!");
	} else {
		/* write entry by entry */
		size_t fpos = 0U;
		size_t bw = 0U;
		size_t btw = t_len;

		for (size_t i = 0; i < MSG_TBL_LENGTH; i++) {
			const opentThermMsg_t * msg = &messagesTbl[i];
			uint8_to_asciiz((uint8_t)msg->msgId, (char *)outstr);

			/* by default only dataID = 0, 1, 17, and 25 are ON */
			switch (i) {
			case 0:
			case 1:
			case 17:
			case 25:
				{
					outstr[yn_pos] = 'Y';
					break;
				}
			default:{
					outstr[yn_pos] = 'N';
					break;
				}
			}
			if ((WriteBytes(&Media0, filename, fpos, btw, &bw,
					(const uint8_t *)outstr) != FR_OK) ||
			    (bw != btw)) {
				log_xputs(MSG_LEVEL_SERIOUS,
					  "CFG_OT file write error!");
				break;
			} else {
				fpos = fpos + btw;
			}
		}
	}
}

#if(0)

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
	size_t bwr; /* bytes was read */
	char read_rec[rec_len];
	size_t pos = 0U;
	size_t configured = 0U;

	do {
		if ((ReadBytes(&Media0, "PUB_MV", pos, rec_len, &bwr,
			       (uint8_t *)&read_rec) == FR_OK) &&
		    (bwr == rec_len)) {
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
			tMV *targetMV = OPENTHERM_findMVbyLDID(ldid);
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
	char outstr[rec_len];

	strncpy((char *)outstr, template_str, sizeof(template_str));

	size_t estimated_fsize = MV_ARRAY_LENGTH * (sizeof(template_str));
	const char *pub_mv = "PUB_MV";

	if (DeleteFile(&Media0, pub_mv) != FR_OK) {
		log_xputs(MSG_LEVEL_SERIOUS, "Can not delete file PUB_MV");
	}

	if (AllocSpaceForFile(&Media0, pub_mv, estimated_fsize) != FR_OK) {
		log_xputs(MSG_LEVEL_SERIOUS, " file PUB_MV creation error!");
	} else {
		/* write entry by entry */
		size_t fpos = 0U;
		size_t bw = 0U;
		size_t btw = sizeof(template_str);

		for (size_t i = 0; i < MV_ARRAY_LENGTH; i++) {
			tMV *pMV = OPENTHERM_getMV_for_Pub(i);

			uint16_to_asciiz((uint16_t)pMV->LD_ID, (char *)outstr);

			if (pMV->ReportType == GI) {
				strncpy(&outstr[6], gi_, sizeof(gi_));
			} else if (pMV->ReportType == SP) {
				strncpy(&outstr[6], sp_, sizeof(sp_));
			} else if (pMV->ReportType == SP_GI) {
				strncpy(&outstr[6], spgi_, sizeof(spgi_));
			} else {
				strncpy(&outstr[6], no_, sizeof(no_));
			}

			if ((WriteBytes(&Media0, pub_mv, fpos, btw, &bw,
					(const uint8_t *)outstr) != FR_OK) ||
			    (bw != btw)) {
				log_xputs(MSG_LEVEL_SERIOUS,
					  "PUB_MV file write error!");
				break;
			} else {
				fpos = fpos + btw;
			}
		}
	}
}

/**  CONTROLLABLES CUSTOMIZATION **/

static const char cust_ctrl_str[] = "00000:WREN\n"; // write enabled
static const size_t cust_rec_len = sizeof(cust_ctrl_str);
static const char wren_[] = "WREN\n";
static const char skip_[] = "SKIP\n";

/**
 * @brief customize_ctrl_MVs customizes Opentherm's MVs to the boiler capabilities
 *	  nol all the statndard defined writeable entries exist in the particular boiler
 * @return
 * @note  must be called after OPENTHERM_InitMaster()
 */
static ErrorStatus customize_ctrl_MVs(void)
{

	ErrorStatus retVal = ERROR;
	size_t bwr; /* bytes was read */
	char read_rec[cust_rec_len];
	size_t pos = 0U;
	size_t configured = 0U;

	do {
		if ((ReadBytes(&Media0, "CTRL_MV", pos, rec_len, &bwr,
			       (uint8_t *)&read_rec) == FR_OK) &&
		    (bwr == rec_len)) {
			/* one record was read */
			/* parse record */
			enum tControllable ctrl_type;

			if (strcmp(wren_, &read_rec[6]) == 0) {
				ctrl_type = Yes;
			} else if ((strcmp(skip_, &read_rec[6]) == 0)) {
				ctrl_type = No;
			} else {
				/* error */
				break;
			}

			ldid_t ldid;
			ldid = (ldid_t)adec2uint16(read_rec, 5U);

			/* find relevant MV */
			tMV *targetMV = OPENTHERM_findMVbyLDID(ldid);
			if (targetMV != NULL) {
				if (targetMV->Ctrl == Yes) {
					targetMV->Ctrl = ctrl_type;
				}
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
 * @brief create_ctrl_MV_file
 */
static void create_ctrl_MV_file(void)
{
	char outstr[cust_rec_len];

	strncpy((char *)outstr, cust_ctrl_str, sizeof(cust_ctrl_str));

	size_t estimated_fsize = MV_ARRAY_LENGTH * (sizeof(template_str));
	const char *ctrl_mv = "CTRL_MV";

	if (DeleteFile(&Media0, ctrl_mv) != FR_OK) {
		log_xputs(MSG_LEVEL_SERIOUS, "Can not delete file CTRL_MV");
	}

	if (AllocSpaceForFile(&Media0, ctrl_mv, estimated_fsize) != FR_OK) {
		log_xputs(MSG_LEVEL_SERIOUS, " file CTRL_MV creation error!");
	} else {
		/* write entry by entry */
		size_t fpos = 0U;
		size_t bw = 0U;
		size_t btw = sizeof(cust_ctrl_str);

		for (size_t i = 0; i < MV_ARRAY_LENGTH; i++) {
			tMV *pMV = OPENTHERM_getMV_for_Pub(i);

			uint16_to_asciiz((uint16_t)pMV->LD_ID, (char *)outstr);

			/* all controllables are disabled by default */
			strncpy(&outstr[6], skip_, sizeof(skip_));

			if ((WriteBytes(&Media0, ctrl_mv, fpos, btw, &bw,
					(const uint8_t *)outstr) != FR_OK) ||
			    (bw != btw)) {
				log_xputs(MSG_LEVEL_SERIOUS,
					  "CTRL_MV file write error!");
				break;
			} else {
				fpos = fpos + btw;
			}
		}
	}
}
#endif

#endif
