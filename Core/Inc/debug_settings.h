/**
  ******************************************************************************
  * @file    debug_settings.h
  * @author  turchenkov@gmail.com
  * @date    12-Mar-2018
  * @brief   debug defines for the project
  ******************************************************************************
  * @attention use at your own risk
  ******************************************************************************
  */

#define DEBUG_PRINT_ERR_LEVEL_ALL	1	/* print all messages */
#define DEBUG_PRINT_ERR_LEVEL_ERR	2	/* print only err messages */
#define DEBUG_PRINT_ERR_LEVEL_NOTHING	10	/* print nothing */

/* TFTP section */

#define	TFTP_DEBUG_PRINT

/* MQTT-SN PUB section */

#define	MQTT_SN_PUB_DEBUG_PRINT DEBUG_PRINT_ERR_LEVEL_ERR

/* MQTT-SN SUB section */

#define	MQTT_SN_SUB_DEBUG_PRINT DEBUG_PRINT_ERR_LEVEL_ERR

/* CONF_FN sections */

#define CONF_FN_DEBUG_PRINT

/* NTP section */

#define NTP_DEBUG_PRINT

/* JSON section */

//#define JSON_DEBUG_PRINT

/* DAQ section */

#define DAQ_DEBUG_PRINT DEBUG_PRINT_ERR_LEVEL_ERR

/* Manchester task section */
//#define MANCH_TASK_DEBUG_PRINT 1
#define MANCH_TASK_DEBUG_PRINT 0



/* ################################### E.O.F. ################################################### */
