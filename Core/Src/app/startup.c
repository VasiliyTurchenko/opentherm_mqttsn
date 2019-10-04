/** @file startup.c
 *  @brief stratup system functions
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 28-Feb-2019
 *  @note runs berfore ROTS started!
 */

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include <string.h>

#include "rtc.h"

#include "rtc_magics.h"

#include "config_files.h"

#include "xprintf.h"
#include "my_comm.h"
#include "nvmem.h"
#include "tiny-fs.h"
#include "file_io.h"

#include "ascii_helpers.h"

#include "lan.h"

#include "startup.h"
#include "buildinfo.h"

#ifndef MAKE_IP
#define MAKE_IP(a, b, c, d)                                                    \
	(((uint32_t)a) | ((uint32_t)b << 8) | ((uint32_t)c << 16) |            \
	 ((uint32_t)d << 24))
#endif

static inline void Cold_Boot(void);
static Boot_t HW_Boot_Select(void);
static FRESULT SaveIPCfg(const Media_Desc_t *media);
static ErrorStatus Init_NIC(const Media_Desc_t *media);

volatile Boot_t Boot_Mode = COLD_BOOT; /* boot mode - cold or warm */
volatile uint32_t RCC_CSR_copy;	/* copy of the RCC_CSR */
volatile boot_action_t Boot_Action;
volatile uint32_t Saved_Magic_Bits; /* watchdog */

/* where to put diagnostic data */
volatile bool Transmit_non_RTOS;

/* FRAM media description */
const Media_Desc_t Media0 = { .readFunc = Read_FRAM,
			      .writeFunc = Write_FRAM,
			      .MediaSize = NVMEM_SIZE,
			      .mode = MEDIA_RW };

/* configuration file */
static const char *IP_Cfg_File = "IP_CFG";

/* cold bood flag */
static Boot_t boot_flag = COLD_BOOT;

static const struct ip_cfg IP_Cfg_File_Default = {
	.MAC_addr.mac_h = { "MAC_" },
	.MAC_addr.mac_v = { "EE65EE33FF81" },
	.MAC_addr.mac_pad = { "     \n" },
	.Own_IP.ip_n = { "IP__" },
	.Own_IP.ip_v = { "192"
			 "168"
			 "000"
			 "223" },
	.Own_IP.ip_p = { "00000" },
	.Own_IP.ip_nl = { "\n" },
	.Net_Mask.ip_n = { "NM__" },
	.Net_Mask.ip_v = { "255"
			   "255"
			   "255"
			   "000" },
	.Net_Mask.ip_p = { "00000" },
	.Net_Mask.ip_nl = { "\n" },
	.GW.ip_n = { "GW__" },
	.GW.ip_v = { "192"
		     "168"
		     "000"
		     "001" },
	.GW.ip_p = { "00000" },
	.GW.ip_nl = { "\n" },
	.zero = '\0',
};

/*
 * 1. Define reboot cause:
 *	watchdog
 *	configuration changed
 *	cold boot
 *	spontaneous
 *
 * 2. Check hardware
 * 3. Check filesystem
 * 4. Read working configuration
 *	OR
 *    Start tasks with default config
 */

/**
 * @brief reste_after_delay
 */
static void reset_after_delay(void)
{
	HAL_Delay(500U);
	NVIC_SystemReset();
}

/**
 * @brief AppStartUp
 * @return ERROR or SUCCESS
 */
ErrorStatus AppStartUp(void)
{
	ErrorStatus retVal;
	retVal = ERROR;

#if defined(MASTERBOARD)
	static const char *id = "MASTER ";
#endif

#if defined(SLAVEBOARD)
	static const char *id = "SLAVE ";
#endif

	InitComm();
	xfunc_out = myxfunc_out_no_RTOS; /* diagnostic print */
	/* set up periodic UART transmissions */
	Transmit_non_RTOS = true;

	xputs("\n\nStarting up...\n");
	xputs(id);
	xputs(buildNum_s);
	xputs(buildDateTime);

	Cold_Boot(); /* Cold_Boot always goes first */

	boot_flag = HW_Boot_Select();

	if (boot_flag == COLD_BOOT) {
		xputs("Cold boot selected.\n");
	}

	/* check FRAM hardware */
	InitFS();
	FRESULT res = f_checkFS(&Media0);
	if ((res == FR_NO_FILESYSTEM) || (res == FR_NO_FILE) ||
	    (boot_flag == COLD_BOOT)) {
		/* we need to format media */
		xputs(FRESULT_String(res));
		xputs("\nNo filesystem! Formatting...\n");

		/* check media */
		if (Quick_FRAM_Test() != SUCCESS) {
			xputs("FRAM test failed! Rebooting.\n");
			reset_after_delay();
		}
		if (Format(&Media0) != SUCCESS) {
			xputs("Media formatting failed! Rebooting.\n");
			reset_after_delay();
		}
		/* create ip config file */
		res = SaveIPCfg(&Media0);
		if (res != FR_OK) {
			xputs(FRESULT_String(res));
			xputs(" SaveIPCfg() failed! Rebooting.\n");
			reset_after_delay();
		} else {
			/* good reboot */
			xputs("SaveIPCfg() successful! Rebooting.\n");
			reset_after_delay();
		}

	} else if (res != FR_OK) {
		xputs(FRESULT_String(res));
		xputs("\nDisk error! Rebooting.\n");
		reset_after_delay();
	} else {
		xputs("Disk is OK.\n");
	}

	/* assume we have IP_CFG file */
	if (Init_NIC(&Media0) != SUCCESS) {
		/* reboot with defaults */
		xputs("IP configuration error!\n");
		res = SaveIPCfg(&Media0);

		xputs("Default IP configuration ");
		if (res == FR_OK) {
			xputs("saved.\n");
		} else {
			xputs("error!\n");
		}
		xputs("Rebooting\n");
		reset_after_delay();
	}
	/* ip configuration retrieved  */
	xputs("IP configuration OK!\n");
	lan_init();
	retVal = SUCCESS;

	return retVal;
}

/**
  * @brief Cold_Boot
  */
static inline void Cold_Boot(void)
{
	(void)(0);
}

/**
 * @brief  HW_Boot_Select() detects boot mode
 * @note
 * @param  none
 * @retval COLD_BOOT or WARM_BOOT
 */
static Boot_t HW_Boot_Select(void)
{
	Boot_t result;
	result = WARM_BOOT;
	if (HAL_GPIO_ReadPin(COLD_BOOT_GPIO_Port, COLD_BOOT_Pin) ==
	    GPIO_PIN_RESET) {
		/* COLD_BOOT_pin is tied to GND */
		uint8_t tout;
		for (tout = 100U; tout > 0u; tout--) {
			HAL_Delay(10U); /* wait 10 ms */
			if (HAL_GPIO_ReadPin(COLD_BOOT_GPIO_Port,
					     COLD_BOOT_Pin) == GPIO_PIN_SET) {
				/* tie to GND is lost */
				break;
			}
		}
		result = COLD_BOOT;
	}
	return result;
}
/* end of HW_Boot_Select() */

/**
 * @brief SaveIPCfg saves default values to the file
 * @param media
 * @return FRESULT
 */
static FRESULT SaveIPCfg(const Media_Desc_t *media)
{
	FRESULT retVal;
	const size_t datalen = sizeof(IP_Cfg_File_Default);
	size_t bw;

	retVal = DeleteFile(media, IP_Cfg_File);
	if ((retVal == FR_OK) || (retVal == FR_NO_FILE)) {
		retVal = WriteBytes(media, IP_Cfg_File, 0U,
				    datalen, &bw,
				    (const uint8_t *)&IP_Cfg_File_Default);

		if (bw != datalen) {
			retVal = FR_INVALID_PARAMETER;
		}
	}

	return retVal;
}

/**
 * @brief Init_NIC
 * @param media
 * @return ERROR or SUCCESS
 */
static ErrorStatus Init_NIC(const Media_Desc_t *media)
{
	ErrorStatus retVal = ERROR;
	FRESULT res;
	struct ip_cfg buf;
	size_t br = 0U;

	res = ReadBytes(media, IP_Cfg_File, 0U,
			sizeof(struct ip_cfg), &br, (uint8_t *)&buf);
	if ((res != FR_OK) || (br != sizeof(struct ip_cfg))) {
		goto fExit;
	}

	if (memcmp(buf.MAC_addr.mac_h, IP_Cfg_File_Default.MAC_addr.mac_h,
		   4U) != 0) {
		goto fExit;
	}
	if (isHex(&buf.MAC_addr.mac_v, MAC_LEN) == false) {
		goto fExit;
	}
	{
		size_t mac_cnt = 0U;
		for (size_t i = 0U; i < MAC_LEN; i = i + 2U) {
			mac_addr[mac_cnt] =
				ahex2byte(&buf.MAC_addr.mac_v[i], 2U);
			mac_cnt++;
		}
	}
	/* IP */
	if (memcmp(buf.Own_IP.ip_n, IP_Cfg_File_Default.Own_IP.ip_n, 4U) != 0) {
		goto fExit;
	}
	if (isDec(&buf.Own_IP.ip_v, IP_LEN) == false) {
		goto fExit;
	}
	{
		uint8_t ip0;
		uint8_t ip1;
		uint8_t ip2;
		uint8_t ip3;

		ip0 = adec2byte(&buf.Own_IP.ip_v, 3U);
		ip1 = adec2byte(&buf.Own_IP.ip_v[3], 3U);
		ip2 = adec2byte(&buf.Own_IP.ip_v[6], 3U);
		ip3 = adec2byte(&buf.Own_IP.ip_v[9], 3U);
		ip_addr = MAKE_IP(ip0, ip1, ip2, ip3);
	}
	/* GW */
	if (memcmp(buf.GW.ip_n, IP_Cfg_File_Default.GW.ip_n, 4U) != 0) {
		goto fExit;
	}
	if (isDec(&buf.GW.ip_v, IP_LEN) == false) {
		goto fExit;
	}
	{
		uint8_t ip0;
		uint8_t ip1;
		uint8_t ip2;
		uint8_t ip3;

		ip0 = adec2byte(&buf.GW.ip_v, 3U);
		ip1 = adec2byte(&buf.GW.ip_v[3], 3U);
		ip2 = adec2byte(&buf.GW.ip_v[6], 3U);
		ip3 = adec2byte(&buf.GW.ip_v[9], 3U);
		ip_gateway = MAKE_IP(ip0, ip1, ip2, ip3);
	}
	/* NM */
	if (memcmp(buf.Net_Mask.ip_n, IP_Cfg_File_Default.Net_Mask.ip_n, 4U) !=
	    0) {
		goto fExit;
	}
	if (isDec(&buf.Net_Mask.ip_v, IP_LEN) == false) {
		goto fExit;
	}
	{
		uint8_t ip0;
		uint8_t ip1;
		uint8_t ip2;
		uint8_t ip3;

		ip0 = adec2byte(&buf.Net_Mask.ip_v, 3U);
		ip1 = adec2byte(&buf.Net_Mask.ip_v[3], 3U);
		ip2 = adec2byte(&buf.Net_Mask.ip_v[6], 3U);
		ip3 = adec2byte(&buf.Net_Mask.ip_v[9], 3U);
		ip_mask = MAKE_IP(ip0, ip1, ip2, ip3);
	}
	retVal = SUCCESS;
fExit:
	return retVal;
}
