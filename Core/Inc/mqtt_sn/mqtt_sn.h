/**
  ******************************************************************************
  * @file    MQTT_SN_task.h
  * @author  Vasiliy Turchenko
  * @version V0.0.1
  * @date    12-Jan-2018
  * @brief   MQTT-SN simple client imlementation for publishing 21-Jan-2018
  * @brief   MQTT-SN simple client imlementation for subscribing 21-Jan-2018
  *
  ******************************************************************************
  */

 #ifndef		__MQTT_SN_task_H
 #define		__MQTT_SN_task_H

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tiny-fs.h"
#include "config_files.h"
#include "ip_helpers.h"

#include "MQTTSNPacket\MQTTSNPacket.h"
#include "MQTTSNPacket\MQTTSNConnect.h"

#include "opentherm_daq_def.h"
#include "lan.h"


enum	tConnState					/*!< MQTT-SN connection state */
	{
		IDLE,					/*!< idle state */
		CONNECTING,				/*!< trying to connect to the gateway */
		CONNECTED,				/*!< connected */
		DISCONNECTING				/*!< disconnecting...*/
	};

typedef	struct MQTT_SN_Context
	{
		uint16_t	Host_GW_Port;		/*!< MQTT-SN gateway port */
		uint32_t	Host_GW_IP;		/*!< MQTT-SN gateway IP address */
/*		uint32_t	Subnet_Mask;	*/	/*!< The subnet mask */
/*		uint32_t	Gateway_IP;	*/	/*!< The Gateway IP address */
		char		*Root_Topic;		/*!< Root topic asciiz string */
		MQTTSNPacket_connectData	options;/*!< connection options  */
		enum tConnState			state;	/*!< current context state */
		uint16_t	packetid;		/*!< for puback */
		socket_p	insoc;			/*!< socket_p for read */
		socket_p	outsoc;			/*!< socket_p for write */
		TickType_t	time_OK;		/*!< timestamp of the last successful event*/
		uint16_t	last_procd_packet_id;	/*!< last processed packetid */

		uint16_t	currPubSubMV;		/*!< current pub/sub MV index */

	}	MQTT_SN_Context_t;			/*!< Context for MQTT-SN connection */

typedef	MQTT_SN_Context_t	*MQTT_SN_Context_p;

extern MQTT_SN_Context_t	mqttsncontext01;	/* the static instance of the context - publish */
extern MQTT_SN_Context_t	mqttsncontext02;	/* the static instance of the context - subscribe */

ErrorStatus mqtt_sn_connect(MQTT_SN_Context_p pcontext);

ErrorStatus mqtt_sn_init_context(MQTT_SN_Context_p pcontext, \
				 struct MQTT_topic_para * topic_params,\
				 cfg_pool_t *ip_para);

/**
  * De-Initialises MQTT-SN context
  * @param pcontext the pointer to the context to be de-initialized
  * @return ErrorStatus SUCCESS or ERROR
  */
ErrorStatus mqtt_sn_deinit_context(MQTT_SN_Context_p pcontext);

ErrorStatus mqtt_sn_register_topic(MQTT_SN_Context_p pcontext, ldid_t ldid);

ErrorStatus mqtt_sn_publish_topic(MQTT_SN_Context_p pcontext, uint16_t topicid, const char *topicstring);


/**
  * Subscribes to the topic
  * @param pcontext the pointer to the context used for subscription
  * @param ldid to be subscribed
  * @return ErrorStatus SUCCESS or ERROR
  */
ErrorStatus mqtt_sn_subscribe_topic(MQTT_SN_Context_p pcontext, ldid_t ldid);

/** polls network for the topics subscribed
  * @param pcontext the pointer to the context used for subscription
  * @return ErrorStatus SUCCESS or ERROR
  *
  */
ErrorStatus mqtt_sn_poll_subscribed(MQTT_SN_Context_p pcontext);

/**
  * Resets last processed packet id field in the context
  * @param pcontext the pointer to the context
  * @return ErrorStatus SUCCESS or ERROR
  */
ErrorStatus mqtt_sn_reset_last_packid(MQTT_SN_Context_p pcontext);


 #endif  /* __MQTT_SN_task_H */
 /* ###################################  EOF ####################################################*/
