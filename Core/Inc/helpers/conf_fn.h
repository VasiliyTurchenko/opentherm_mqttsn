/**
 * @file conf_fn.h
 * @author Vasiliy Turchenko
 * @date 13-Feb-2017
 * @version 0.0.1
 *
 */

#ifndef _CONF_FN_H
#define _CONF_FN_H

#include <stdint.h>
#include <stdbool.h>

#include "stm32f1xx.h"
#include "opentherm_daq_def.h"

/* macro defines */

#ifndef		ROOT_TOPIC_LEN
	#define		ROOT_TOPIC_LEN (40)
#endif

#ifndef		CLIENT_ID_STRING
	#define		CLIENT_ID_STRING (10)
#endif

#define MAKE_IP(a,b,c,d)	( ((uint32_t)a) | ((uint32_t)b << 8) | \
				((uint32_t)c << 16) | ((uint32_t)d << 24) )

enum	ip_params	{
	/*LAN module parameters */
	MY_IP = 0,			/* !< board's IP */
	MY_NETMASK = 1,                 /* !< board's network netmask */
	DEFAULT_GW = 2,                 /* !< board's network default gateway */
	/* Diag socket params */
	LISTENER_IP = 3,                /* !< listener IP  and port*/
	/* MQTT-SN PUB params */
	MQTT_SN_PUB_IP = 4,             /* !< mqtt-sn gateway IP and port for publish */
	/* MQTT-SN SUB params */
	MQTT_SN_SUB_IP = 5,             /* !< mqtt-sn gateway IP and port for subscribe */
	/* NTP params */
	NTP_SERV1 = 6,                  /* !< ntp server (main) ip and port */
	NTP_SERV2 = 7,                  /* !< ntp server (reserve) ip and port */
/* the enum member below must always be the last in the enum definition!*/
	NUM_IP_PARAMS = 8
	};

typedef 	enum ip_params ip_params_t;

typedef 	uint8_t mac_addr_t[6];

typedef	uint32_t	ip_addr_t;

typedef	struct	ip_pair {		/* !< general storage object */
		ip_addr_t	ip;
		uint16_t	port;
	}	ip_pair_t;

enum	reboot_cause	{
	RB_NONE = 0U,
	RB_CMD,		/* Reboot due to remote command */
//	RB_WDT,		/* Reboot due to watchdog timer */
	RB_CFG_CHNG,	/* Software reboot due to congiguration change */
	RB_CFG_ERR,	/* Software reboot due to congiguration restore error */
	RB_PWR_LOSS     /* Power loss event */
	};
typedef	enum reboot_cause reb_cause_t;

enum	boot_action	{
	B_ACT_COLD = 0,	/* Cold boot required */
	B_ACT_CFG,	/* Only configuration must be restored */
	B_ACT_CFG_DAQ	/* Configuration and process data must be restored */
	};
typedef	enum boot_action boot_action_t;

enum	FRAM_state	{
	FRAM_pad = 0,	/* padding value */
	FRAM_OK,	/* no FRAM error detected */
	FRAM_BAD	/* FRAM error was detected */
	};
typedef	enum FRAM_state	 FRAM_state_t;


typedef struct settings {
		reb_cause_t	saved_reboot_cause;
		mac_addr_t	my_mac;		/* !< board's mac address*/
		ip_pair_t	ip_arr[NUM_IP_PARAMS];
	/* MQTT-SN global params */
		char		roottopic[ROOT_TOPIC_LEN+1];	/* !< root topic string */
	/* MQTT-SN PUB params */
		uint32_t	GI_period;			/* !< GI period in milliseconds */
		char		pub_client_id_string[CLIENT_ID_STRING+1]; /* !< mqtt-sn client id string */
	/* MQTT-SN SUB params */
		uint32_t	mqtt_sn_sub_timeout;		/* !< mqtt-sn task's max_idle */
		char		sub_client_id_string[CLIENT_ID_STRING+1]; /* !< mqtt-sn client id string */
	} settings_t;

typedef	settings_t	*settings_p;			/* !< pointer to the settings struct */


/* BLOB DEF */
typedef	struct	blob {
		settings_t	savedsettings;		/* all the settings */
		uint32_t	savedsettingsCRCR32;
		tSPS		savedsps[NUM_SPS];	/* all the sps info */
		uint32_t	savedspsCRC32;
		tMV		savedmv[NUM_MV];	/* all the mv info */
		uint32_t	savedmvCRC32;
		tCMD		savedcmd[NUM_CMD];      /* all the cmd info */
		uint32_t	savedcmdCRC32;
	}	blob_t;

typedef	blob_t		*blob_p;

/* CRC32 actions */
enum	crc32_action	{
	CRC_ACT_NONE = 0U,
	CRC_ACT_CALC,
	CRC_ACT_CALC_AND_STORE,
	CRC_ACT_CHECK
	};
typedef	enum crc32_action crc32_action_t;

/* exported variables */

extern volatile	uint8_t		Boot_Mode;	/* boot mode - cold or warm */
extern	volatile	uint32_t	RCC_CSR_copy;		/* copy of the RCC_CSR */

/* for mqtt-sn tasks */
extern volatile	char	work_roottopic[ROOT_TOPIC_LEN+1];
extern volatile	char	pub_client_id_string[CLIENT_ID_STRING+1];
extern volatile	char	sub_client_id_string[CLIENT_ID_STRING+1];


/* exported functions */


void Get_MAC(mac_addr_t mymac);
void Save_MAC_to_NVMEM(const mac_addr_t mymac);

void Get_IP_Params(ip_pair_t * target, const ip_params_t kind_of_ip);
void Save_IP_Pair_to_NVMEM(const ip_pair_t * tmpp, const ip_params_t i);

uint32_t Get_GI_Period(void);
void Save_GI_Period_to_NVMEM(const uint32_t ms);

char * Get_Pub_ID_String(void);

char * Get_Sub_ID_String(void);

char * Get_Root_Topic(void);

void Power_Loss(void);

ErrorStatus Warm_Boot(void);

void FRAM_Read_Error_Handler(void);

void FRAM_Write_Error_Handler(void);

boot_action_t Get_Reboot_Action(void);

void Set_Reboot_Cause(reb_cause_t c_arg);

ErrorStatus Proc_NVMEM_CRC32(crc32_action_t action, \
				uint32_t * crcval, \
				uint32_t fram_addr, \
				size_t frlen, \
				uint32_t crc32addr);

void Init_FRAM_settings(reb_cause_t c_arg);

FRAM_state_t Get_FRAM_State(void);

#endif
/* EOF */
