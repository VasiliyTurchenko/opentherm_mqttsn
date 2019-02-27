/**
 * @file conf_fn.c
 * @author Vasiliy Turchenko
 * @date 13-Feb-2017
 * @version 0.0.1
 * @date 25-02-2018
 * @note CRC check added
 */

#include <stddef.h> 
#include <string.h>

#include "stm32f1xx.h"
#include "stm32f1xx_hal_rtc.h"
#include "rtc.h"
#include "nvmem.h"
#include "conf_fn.h"
#include "crc.h"



				
#define BAD_FRAM_MAGIC		0xDEADU		/* magic to indicate a bad FRAM */
#define MAGIC_REG		10U		/* BKP_REG number to store */

/* variables */

volatile	uint8_t		Boot_Mode = COLD_BOOT;		/* boot mode - cold or warm */
volatile	uint32_t	RCC_CSR_copy;			/* copy of the RCC_CSR */
volatile	char		work_roottopic[ROOT_TOPIC_LEN+1]; /* for mqtt-sn tasks */
volatile	char		work_pub_client_id_string[CLIENT_ID_STRING+1];
volatile	char		work_sub_client_id_string[CLIENT_ID_STRING+1];

const
settings_t
default_set = {				/* RO memory */
		.saved_reboot_cause = RB_NONE,
		.my_mac = {0x00,0x13,0x37,0x01,0x23,0x45},
		.ip_arr = {
				{ MAKE_IP(192, 168, 0, 222), 0x00U },	/* MY_IP */
				{ MAKE_IP(255, 255, 255, 0), 0x00U },   /* MY_NETMASK */
				{ MAKE_IP(192, 168, 0, 1), 0x00U },   	/* DEFAULT_GW */
				{ MAKE_IP(192, 168, 0, 1), 5000U },	/* LISTENER_IP */
				{ MAKE_IP(192, 168, 0, 1), 3333U },	/* MQTT_SN_PUB_IP */
				{ MAKE_IP(192, 168, 0, 1), 3333U },	/* MQTT_SN_SUB_IP */
				{ MAKE_IP(192, 168, 0, 1), 123U },				/* NTP_SERV1 */
				{ MAKE_IP(216, 239, 35, 12), 123U }				/* NTP_SERV2 */
			},
/* NOT USED */	.mqtt_sn_sub_timeout = 15000U, /* NOT USED YET */
		.sub_client_id_string = "DIO-CMD",
		.roottopic = "tvv/5413/in-home/1st_floor/DIO_board/",
		.GI_period = 4000U,
		.pub_client_id_string = "DIO-MPVV"
      };

// const	uint32_t	set_siz = sizeof(default_set);

/* functions declarations  */
void Get_IP_Pair_fom_NVMEM(ip_pair_t * tmpp, const ip_params_t i);



/**
 * @brief Get_MAC
 * @param mymac array to be filled with mac
 * @return none
 */
void Get_MAC(mac_addr_t mymac)
{
	ErrorStatus	IOresult;
	if (Boot_Mode == COLD_BOOT) { 		/* use ROM value */
		memcpy(mymac, default_set.my_mac, sizeof(mac_addr_t));
	} else {
		IOresult = Read_FRAM(mymac, \
			           offsetof(blob_t, savedsettings.my_mac), \
				   sizeof(mac_addr_t));
		if (IOresult != SUCCESS) { FRAM_Read_Error_Handler(); } // NO RETURN
	}
	return;
}

/**
 * @brief Save_MAC_to_NVMEM
 * @param mymac array to be copied into settings structure
 * @return none
 */
void Save_MAC_to_NVMEM(const mac_addr_t mymac)
{
	uint32_t	tmpCRC32 = 0U;
	ErrorStatus	IOresult;
	ErrorStatus	ActResult;
	
	IOresult = Write_FRAM((uint8_t*)mymac, \
				offsetof(blob_t, savedsettings.my_mac), \
				sizeof(mac_addr_t));
	if (IOresult != SUCCESS ) { FRAM_Write_Error_Handler(); } // NO RETURN
	ActResult = Proc_NVMEM_CRC32( CRC_ACT_CALC_AND_STORE, \
				   &tmpCRC32, \
				   offsetof(blob_t, savedsettings),
				   sizeof(settings_t),
				   offsetof(blob_t, savedsettingsCRCR32) );
	(void)ActResult;
}

/**
 * @brief Get_IP_Pair_fom_NVMEM
 * @param &tmpp pointer to the target pair location
 * @param i index of pair into the settings.ip_arr[] struct 
 * @return none
 */
void Get_IP_Pair_fom_NVMEM(ip_pair_t * tmpp, const ip_params_t i)
{
	ErrorStatus	IOresult;
	IOresult = Read_FRAM((uint8_t*)tmpp, \
			   offsetof(blob_t, savedsettings.ip_arr[i]), 
			   sizeof(ip_pair_t));
	if (IOresult != SUCCESS) { FRAM_Read_Error_Handler(); } // NO RETURN
	return;
}

/**
 * @brief Save_IP_Pair_to_NVMEM
 * @param &tmpp pointer to the src pair location
 * @param i index of pair into the settings.ip_arr[] struct 
 * @return none
 */
void Save_IP_Pair_to_NVMEM(const ip_pair_t * tmpp, const ip_params_t i)
{
	uint32_t	tmpCRC32 = 0U;
	ErrorStatus	IOresult;
	ErrorStatus	ActResult;
	IOresult = Write_FRAM((uint8_t*)tmpp, \
				offsetof(blob_t, savedsettings.ip_arr[i]), \
				sizeof(ip_pair_t));
	if (IOresult != SUCCESS ) { FRAM_Write_Error_Handler(); } // NO RETURN
	ActResult = Proc_NVMEM_CRC32( CRC_ACT_CALC_AND_STORE, \
				   &tmpCRC32, \
				   offsetof(blob_t, savedsettings),
				   sizeof(settings_t),
				   offsetof(blob_t, savedsettingsCRCR32) );
	(void)ActResult;
	return;
}

/**
 * @brief Get_IP_Params
 * @param *target pointer to the ip_pair_t structure
 * @param kind_of_ip specifies what kind of IP pair to return
 * @return none
 */
void Get_IP_Params(ip_pair_t * target, const ip_params_t kind_of_ip)
{
	ip_pair_t	tmp;
	tmp.ip = 0x00U;
	tmp.port = 0x00U;
	if (Boot_Mode == COLD_BOOT) { 		/* use ROM value */
		* target = default_set.ip_arr[kind_of_ip];
	} else {
		Get_IP_Pair_fom_NVMEM(&tmp, kind_of_ip);
		* target = tmp;
	}
	return;
}

/**
 * @brief Get_GI_Period returns General Interrogation period (ms)
 * @param none
 * @return time in milliseconds
 */
uint32_t Get_GI_Period(void)
{
	uint32_t	result;
	if (Boot_Mode == COLD_BOOT) { 		/* use ROM value */
		result = default_set.GI_period;
	} else {
		ErrorStatus IOresult;
		IOresult = Read_FRAM((uint8_t*)&result, \
			        offsetof(blob_t, savedsettings.GI_period), \
				sizeof(result));
		if (IOresult != SUCCESS) { FRAM_Read_Error_Handler(); } // NO RETURN				
	}
	return result;
}

/**
 * @brief Save_GI_Period_to_NVMEM sets General Interrogation period (ms)
 * @param  time in milliseconds
 * @return none
 */
void Save_GI_Period_to_NVMEM(const uint32_t ms)
{
	uint32_t	tmpCRC32 = 0U;
	ErrorStatus	IOresult;
	ErrorStatus	ActResult;
	IOresult = Write_FRAM((uint8_t*)&ms, \
			      offsetof(blob_t, savedsettings.GI_period), \
			      sizeof(ms));
	if (IOresult != SUCCESS) { FRAM_Write_Error_Handler(); } // NO RETURN
	ActResult = Proc_NVMEM_CRC32( CRC_ACT_CALC_AND_STORE, \
				   &tmpCRC32, \
				   offsetof(blob_t, savedsettings),
				   sizeof(settings_t),
				   offsetof(blob_t, savedsettingsCRCR32) );
	(void)ActResult;
	return;
}	

/**
 * @brief Get_Root_Topic returns pointer to the root topic stringz
 * @param none
 * @return char * or NULL in case of error 
 */
char * Get_Root_Topic(void)
{
	char *	result;
	if (Boot_Mode == COLD_BOOT) { 		/* use ROM value */
		result = (char*)default_set.roottopic;
	} else {
		ErrorStatus IOresult;
		IOresult = Read_FRAM((uint8_t*)work_roottopic, \
			        offsetof(blob_t, savedsettings.roottopic), \
				(size_t)(ROOT_TOPIC_LEN+1));
                if (IOresult != SUCCESS) { FRAM_Read_Error_Handler(); } // NO RETURN
		result = (char*)work_roottopic;
	}
	return result;
}

/**
 * @brief Get_Pub_ID_String returns pointer to the publish id stringz
 * @param none
 * @return char * 
 */
char * Get_Pub_ID_String(void)
{
	char *	result;
	if (Boot_Mode == COLD_BOOT) { 		/* use ROM value */
		result = (char*)default_set.pub_client_id_string;
	} else {
		ErrorStatus IOresult;
		IOresult = Read_FRAM((uint8_t*)work_pub_client_id_string, \
			        offsetof(blob_t, savedsettings.pub_client_id_string), \
				(size_t)(CLIENT_ID_STRING+1));
                if (IOresult != SUCCESS) { FRAM_Read_Error_Handler(); } // NO RETURN
		result = (char*)work_pub_client_id_string;
	}
	return result;
}

/**
 * @brief Get_Sub_ID_String returns pointer to the subscribe id stringz
 * @param none
 * @return char * 
 */
char * Get_Sub_ID_String(void)
{
	char *	result;
	if (Boot_Mode == COLD_BOOT) { 		/* use ROM value */
		result = (char*)default_set.sub_client_id_string;
	} else {
		ErrorStatus IOresult;
		IOresult = Read_FRAM((uint8_t*)work_sub_client_id_string, \
			        offsetof(blob_t, savedsettings.sub_client_id_string), \
				(size_t)(CLIENT_ID_STRING+1));
                if (IOresult != SUCCESS) { FRAM_Read_Error_Handler(); } // NO RETURN				
		result = (char*)work_sub_client_id_string;
	}
	return result;
}

/**
  * @brief  Being invoked by power loss event, the function stores all the SPS', \
  * @brief  MVs and CMDs into non-volatile memmory
  *
  * @param  none
  * @retval none
  */
void Power_Loss(void)
{
// stop daq and gi task !!


	uint32_t	mycrc32 = 0U;
	ErrorStatus	ActResult;
	
	if (Write_FRAM( (uint8_t*)SPS_array, \
		    offsetof(blob_t, savedsps), \
		    sizeof(SPS_array) ) == ERROR) {
		FRAM_Write_Error_Handler();
		};
//	
	ActResult = Proc_NVMEM_CRC32( CRC_ACT_CALC_AND_STORE, \
				      &mycrc32, \
				      offsetof(blob_t, savedsps),
				      sizeof(SPS_array),
				      offsetof(blob_t, savedspsCRC32) );
		
	if (Write_FRAM( (uint8_t*)MV_array, \
		    offsetof(blob_t, savedmv), \
		    sizeof(MV_array) ) == ERROR) {
		FRAM_Write_Error_Handler();
		};
//
	ActResult = Proc_NVMEM_CRC32( CRC_ACT_CALC_AND_STORE, \
					&mycrc32, \
					offsetof(blob_t, savedmv),
					sizeof(MV_array),
					offsetof(blob_t, savedmvCRC32) );
	
	if (Write_FRAM( (uint8_t*)CMD_array, \
		    offsetof(blob_t, savedcmd), \
		    sizeof(CMD_array) ) == ERROR) {
		FRAM_Write_Error_Handler();
		};
//
	ActResult = Proc_NVMEM_CRC32( CRC_ACT_CALC_AND_STORE, \
					&mycrc32, \
					offsetof(blob_t, savedcmd),
					sizeof(CMD_array),
					offsetof(blob_t, savedcmdCRC32) );

// unfreeze daq and gi tasks	
	(void)ActResult;
}

/**
  * @brief  Warm_Boot tries to restore sps and mv states from non-volatile memory
  * @note   Must be called before RTOS started
  * @note   The function is not thread-safe
  * @param  none
  * @retval ERROR in case of unsuccessful restoration; SUCCESS otherwise
  */
ErrorStatus Warm_Boot(void)
{
	ErrorStatus	IOresult;
	IOresult = ERROR;
	ErrorStatus	ActResult;
	uint32_t	mycrc32 = 0U;	

/* SPS */	
	ActResult = Proc_NVMEM_CRC32(CRC_ACT_CHECK, \
				     &mycrc32, \
				     offsetof(blob_t, savedsps),
				     sizeof(SPS_array),
				     offsetof(blob_t, savedspsCRC32));
	if (ActResult != SUCCESS) { goto fExit; }
	
	IOresult = Read_FRAM( (uint8_t*)SPS_array, \
				offsetof(blob_t, savedsps), \
				sizeof(SPS_array) );
	if (IOresult == ERROR) { FRAM_Read_Error_Handler(); } // NO RETURN
/* MV */
	ActResult = Proc_NVMEM_CRC32( CRC_ACT_CHECK, \
				&mycrc32, \
				offsetof(blob_t, savedmv),
				sizeof(MV_array),
				offsetof(blob_t, savedmvCRC32) );
	if (ActResult != SUCCESS) { goto fExit; }
	IOresult = Read_FRAM( (uint8_t*)SPS_array, \
				offsetof(blob_t, savedsps), \
				sizeof(SPS_array) );
	if (IOresult == ERROR) { FRAM_Read_Error_Handler(); } // NO RETURN
fExit:
	return ActResult;
}

/**
  * @brief  Handle_FRAM_Write_error must handle all the errors related to unsuccessful access to NVMEM info
  * @note  
  * @param  none
  * @retval none
  */
void FRAM_Write_Error_Handler()
{
	/* save tag BAD_NVMEM to backup domain */
	HAL_RTCEx_BKUPWrite(&hrtc, MAGIC_REG, BAD_FRAM_MAGIC);
	/* reboot via watchdog */
	while (1) { ; }
}

/**
  * @brief  Handle_FRAM_Read_error must handle all the errors related to unsuccessful access to NVMEM info
  * @note  
  * @param  none
  * @retval none
  */
void FRAM_Read_Error_Handler()
{
	FRAM_Write_Error_Handler();	// temporary solution
}

/**
  * @brief  Get_FRAM_State checks if the FRAM error was registerd before last reboot
  * @note  
  * @param  none
  * @retval FRAM_OK or FRAM_BAD
  */

FRAM_state_t Get_FRAM_State(void)
{
	uint32_t	st;
	FRAM_state_t	result;
	st = HAL_RTCEx_BKUPRead(&hrtc, MAGIC_REG);
	result = (st == BAD_FRAM_MAGIC) ? FRAM_BAD : FRAM_OK;
	return result;
}


/**
  * @brief  Get_Reboot_Action defines the action to be performed during boot-up
  * @note   if reboot cause is successfully fetched from NVMEM, savedsettings.saved_reboot_cause will be cleared
  * @param  none
  * @retval boot action
  */
boot_action_t Get_Reboot_Action(void)
{
	reb_cause_t	tmp_rc = RB_NONE;
	
	ErrorStatus	ActResult;
	ActResult = ERROR;
	
	ErrorStatus	IOresult;
	IOresult = ERROR;
	
	uint32_t	tmpCRC32;
	tmpCRC32 = 0U;
	
	boot_action_t	bac;
	
	/* check CRC32 of savedsettings */
        ActResult = Proc_NVMEM_CRC32( CRC_ACT_CHECK, \
				   &tmpCRC32, \
				   offsetof(blob_t, savedsettings),
				   sizeof(settings_t),
                                   offsetof(blob_t, savedsettingsCRCR32) );
	if (ActResult != SUCCESS) {
		bac = B_ACT_COLD;
		goto fExit;
	}	/* crc32_action_t error error */
	
	IOresult = Read_FRAM( (uint8_t*)&tmp_rc, \
				offsetof(blob_t, savedsettings.saved_reboot_cause), \
				sizeof(tmp_rc) );
	if (IOresult != SUCCESS) { FRAM_Read_Error_Handler(); }	// NO RETURN
	
	if (tmp_rc == RB_CFG_ERR) {
		bac = B_ACT_COLD;	/* the cold boot process is required */
		goto fExit;
	}
	
	if ( (RCC_CSR_copy & (RCC_CSR_LPWRRSTF | RCC_CSR_SFTRSTF)) != 0U ) { /* reset via soft or low power */
		bac = B_ACT_CFG;	/* only config to be restored */
		goto fExit;
	}
	
	if (tmp_rc == RB_CMD) {
		bac = B_ACT_CFG;	/* only config to be restored */
		goto fExit;
	}
	
	if (tmp_rc == RB_CFG_CHNG) {
		bac = B_ACT_CFG;	/* only config to be restored */
		goto fExit;
	}

	if (tmp_rc == RB_PWR_LOSS) {
		bac = B_ACT_CFG_DAQ;	/* all info to be restored */
		goto fExit;
	}

        if ( (RCC_CSR_copy & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) != 0U ) { /* IWDG or WWDG reset */
		if (tmp_rc == RB_NONE) {
			bac = B_ACT_CFG;	/* only config to be restored */
			goto fExit;
		}
	}

	if ( (RCC_CSR_copy & (RCC_CSR_PINRSTF | RCC_CSR_PORRSTF)) != 0U ) { /* reset via RSTPIN or power on */
		if (tmp_rc == RB_NONE) {
			bac = B_ACT_CFG_DAQ;	/* all info to be restored */
			goto fExit;
		}
	}
	bac = B_ACT_COLD;
fExit:	
	/* clear savedsettings.saved_reboot_cause */
	Set_Reboot_Cause(RB_NONE);
	return bac;
}

/**
  * @brief  Set_Reboot_Cause saves the reboot cause into NVMEM
  * @note
  * @param  reboot cause
  * @retval none
  */
void Set_Reboot_Cause(reb_cause_t c_arg)
{
	ErrorStatus	IOresult;
	IOresult = ERROR;
	ErrorStatus	ActResult;
	ActResult = ERROR;
	uint32_t	tmpCRC32;
	tmpCRC32 = 0U;

	IOresult = Write_FRAM( (uint8_t*)&c_arg, \
			     offsetof(blob_t, savedsettings.saved_reboot_cause), \
			     sizeof(c_arg) );
	if (IOresult == ERROR) { FRAM_Write_Error_Handler(); }	// NO RETURN
	ActResult = Proc_NVMEM_CRC32( CRC_ACT_CALC_AND_STORE, \
						&tmpCRC32, \
						offsetof(blob_t, savedsettings),
						sizeof(settings_t),
						offsetof(blob_t, savedsettingsCRCR32) );
	(void)ActResult;
	return;
}

/**
  * @brief  Proc_NVMEM_CRC32 works with the crc32 of NVMEM region
  * @note
  * @param  action - CRC_ACT_CALC, CRC_ACT_CALC_AND_STORE, CRC_ACT_CHECK
  * @param  *crcval - pointer to the reslut
  * @param  framaddr - start address of the FRAM region
  * @param  frlen - size of the region in bytes; must be divded by sizeof(uint32_t) w/o remainder
  * @param  crc32addr - address of the NVMEM to save crc32 value
  * @retval SUCCESS if action is successful, ERROR otherwise
  */
ErrorStatus Proc_NVMEM_CRC32(crc32_action_t action, \
				uint32_t * crcval, \
				uint32_t fram_addr, \
				size_t frlen, \
				uint32_t crc32addr)
{
	ErrorStatus	IOresult;
	ErrorStatus	ActResult;
	IOresult = ERROR;
	ActResult = ERROR;
	
	if ((action != CRC_ACT_CALC) && \
		(action != CRC_ACT_CALC_AND_STORE) && \
		(action != CRC_ACT_CHECK) ) { goto fExit; }
	if ( (frlen % sizeof(uint32_t)) != 0U) { goto fExit; }

	/* initialize crc32 hardware module */
	if (HAL_CRC_GetState(&hcrc) != HAL_CRC_STATE_READY) {
		MX_CRC_Init();
	}
	uint32_t	temp_crc;
	temp_crc = 0U;
	uint32_t	temp_read;
	temp_read = 0U;
	uintptr_t	i;
	i = 0U;

	while ( i < frlen ) {
		IOresult = Read_FRAM( (uint8_t*)&temp_read, (fram_addr + i), sizeof(uint32_t) );
		if (IOresult == ERROR) {
			FRAM_Read_Error_Handler();	// NO RETURN
		}
		if (i == 0U) {
			temp_crc = HAL_CRC_Calculate( &hcrc, &temp_read, 1U );
		} else {
			temp_crc = HAL_CRC_Accumulate( &hcrc, &temp_read, 1U );
		}
		i += sizeof(uint32_t);
	}
	switch (action) {
		case CRC_ACT_CALC:
			*crcval = temp_crc;
			ActResult = SUCCESS;
			break;
		case CRC_ACT_CALC_AND_STORE:
			*crcval = temp_crc;
			/* now store calculated crc32 into NVMEM */
			IOresult = Write_FRAM( (uint8_t*)&temp_crc, crc32addr, sizeof(uint32_t) );
			if (IOresult == ERROR) {
				FRAM_Write_Error_Handler();	// NO RETURN
			}
			ActResult = SUCCESS;
			break;
		case CRC_ACT_CHECK:
			/* crc32(data+crc32(data)) == 0 */
			*crcval = temp_crc;
			IOresult = Read_FRAM( (uint8_t*)&temp_read, crc32addr, sizeof(uint32_t) );
			if (IOresult == ERROR) {
				FRAM_Read_Error_Handler();	// NO RETURN
			}

			ActResult = (temp_crc == temp_read) ? SUCCESS : ERROR;
			break;
		case CRC_ACT_NONE:
		default:
			ActResult = ERROR; /*  */
			break;
	}
fExit:
	HAL_CRC_DeInit(&hcrc);	
	return ActResult;
}
/**
  * @brief  Init_FRAM_settings initializes the savedsettings array in FRAM
  * @note  
  * @param  reb_cause_t c_arg the reboot cause to be stored into NVMEM settings
  * @retval none
  */
void Init_FRAM_settings(reb_cause_t c_arg)
{
	ErrorStatus	IOresult;
	IOresult = ERROR;
	
	IOresult = Write_FRAM( (uint8_t*)&default_set, \
				0U, \
				sizeof(settings_t) );
	if (IOresult != SUCCESS) {
		FRAM_Write_Error_Handler();	// NO RETURN
	}
	Set_Reboot_Cause(c_arg);
	return;
}

/* ################################# EOF #########################################*/
