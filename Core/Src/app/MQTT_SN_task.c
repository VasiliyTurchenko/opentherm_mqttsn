/*******************************************************************************
 * Copyright (c) Vasiliy Turchenko
 *
 * date: 10-Jan-2018
 *  MQTT-SN simple procedures - publishing
 *
 *******************************************************************************/


#include "MQTT_SN_task.h"
/* #include "stm32f1xx.h" */
#include "xprintf.h"
#include "data_acquisition.h"
#include "hex_gen.h"

#include "debug_settings.h"


MQTT_SN_Context_t	mqttsncontext01;	/* the static instance of the context for publishing */
MQTT_SN_Context_t	mqttsncontext02;	/* the static instance of the context for subscribing */

/* const char *roottopic = "tvv/5413/in-home/1st_floor/DIO_board/"; */

#define		TOPIC_TEXT	"LD_ID:000"
#define		TOPIC_CMD	"CMD:000"

const MQTTSNPacket_connectData opt =  MQTTSNPacket_connectData_initializer;

const	TickType_t	maxidle = 15*1000;

/**
  * Resets last processed packet id field in the context
  * @param pcontext the pointer to the context
  * @return ErrorStatus SUCCESS or ERROR
  */
ErrorStatus mqtt_sn_reset_last_packid(MQTT_SN_Context_p pcontext)
{
	pcontext->last_procd_packet_id = 0;
	return SUCCESS;
}


/**
  * Initialises MQTT-SN context
  * @param pcontext the pointer to the context to be initialized
  * @return ErrorStatus SUCCESS or ERROR
  */

ErrorStatus mqtt_sn_init_context(MQTT_SN_Context_p pcontext, \
				 const char* clientIDstring,\
				 const uint16_t hostgwport, \
				 const uint32_t hostgwip)
{
	ErrorStatus result;
	result = ERROR;
	pcontext->Host_GW_IP = hostgwip;
	pcontext->Host_GW_Port = hostgwport;
	pcontext->Root_Topic = Get_Root_Topic();
	pcontext->options = opt;

	pcontext->options.clientID.cstring = (char*)clientIDstring;  /* "DIO-MPVV"; */
	pcontext->state = IDLE;
	pcontext->packetid = 0;
	pcontext->time_OK = 0;

	socket_p	insoc = NULL;
	socket_p	outsoc = NULL;
	insoc = bind_socket(pcontext->Host_GW_IP, pcontext->Host_GW_Port, 0, SOC_MODE_READ);
	if (insoc == NULL) {
		goto fExit;		/* error with socket */
	}
	outsoc = bind_socket(pcontext->Host_GW_IP, pcontext->Host_GW_Port, 0, SOC_MODE_WRITE);
	if (outsoc == NULL) {
		/* release insoc */
		insoc = close_socket(insoc);	/* release good socket */
		goto fExit;
	}
	result = SUCCESS;		/* all is OK! */
fExit:
	/* correct insocket's local port */

	insoc->loc_port = outsoc->loc_port;

	pcontext->insoc = insoc;
	pcontext->outsoc = outsoc;
	return result;
}
/* end of function mqtt_init_context */

/**
  * Attempts to connect to MQTT-SN gateway
  * @param pcontext the pointer to the connection context
  * @return ErrorStatus SUCCESS or ERROR - connected or not
  */
ErrorStatus mqtt_sn_connect(MQTT_SN_Context_p pcontext)
{
	ErrorStatus	result;
	result = ERROR;

	if (pcontext->state != IDLE) {goto fExit;}	/* connect initiation is possible only
							   from IDLE state */
	uint8_t		buf[100];
	int 		buflen;
	buflen = sizeof(buf);
	int 		len;
	len = MQTTSNSerialize_connect(buf, buflen, &pcontext->options);

	pcontext->state = CONNECTING;

	result = write_socket(pcontext->outsoc, buf, len);
	if (result == ERROR)
	{
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT) ||  defined (MQTT_SN_SUB_DEBUG_PRINT)
	xputs("write_socket() during connection returned error\n");
#endif
		pcontext->state = IDLE;
		goto fExit;
	}
/*	if (MQTTSNPacket_read(pcontext->insoc, buf, buflen, &read_socket) == MQTTSN_CONNACK) */
	TickType_t	tout;
	tout = pdMS_TO_TICKS(500U);
	TickType_t	entree;
	entree = xTaskGetTickCount();
	int	MQTTSNPacket_read_result;
		MQTTSNPacket_read_result = MQTTSNPACKET_READ_ERROR;
	do {
		MQTTSNPacket_read_result = MQTTSNPacket_read(pcontext->insoc, buf, buflen, &read_socket_nowait);
		if (MQTTSNPacket_read_result !=  MQTTSNPACKET_READ_ERROR) {
			break;
		}
		taskYIELD();
	} while (xTaskGetTickCount() < (entree+tout));

	if (MQTTSNPacket_read_result == MQTTSN_CONNACK)
	{
		int connack_rc = -1;
		if ((MQTTSNDeserialize_connack(&connack_rc, buf, buflen) != 1) || connack_rc != 0)
		{
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT) ||  defined (MQTT_SN_SUB_DEBUG_PRINT)
			xprintf("unable to connect, retcode %d\n", connack_rc);
#endif
			result = ERROR;
			pcontext->state = IDLE;
			goto fExit;
		} else {
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT) ||  defined (MQTT_SN_SUB_DEBUG_PRINT)
			xprintf("connected with retcode %d\n", connack_rc);
#endif
			result = SUCCESS;
			pcontext->state = CONNECTED;
		}
	} else {	/* MQTTSN_CONNACK isn't received */
		result = ERROR;
		pcontext->state = IDLE;
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT) ||  defined (MQTT_SN_SUB_DEBUG_PRINT)
		xprintf("MQTTSN_CONNACK isn't received for ");
		xprintf(pcontext->Root_Topic); xputc('\n');
#endif

	}
fExit:
	return result;
}
/* end of function mqtt_sn_connect */

/**
  * Registers the topic (= LD_ID) at the MQTT-SN gateway
  * @param pcontext the pointer to the connection context
  * @param ldid the locical data id
  * @return ErrorStatus SUCCESS or ERROR
  */
ErrorStatus mqtt_sn_register_topic(MQTT_SN_Context_p pcontext, uint8_t ldid)
{
	ErrorStatus	result;
	result = ERROR;

	if (pcontext->state != CONNECTED) {
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT)
	xprintf("not connected, can't register \n");
#endif
	goto fExit;
	}

	uint8_t		buf[100];
	int32_t		buflen;
	buflen = sizeof(buf);
	uint32_t 	len;
	len = 0U;
	uint32_t 	rc;
	rc = 0U;

	MQTTSNString 	topicstr;
	uint16_t 	topicid;

#if	defined  (MQTT_SN_PUB_DEBUG_PRINT)
	/* xprintf("Registering\n"); */
#endif
	/* compose topicname */
static	char	tmptopicstr[MAX_TOPICSTR_LEN];
	char	*sr;
	char	*d;
	sr = pcontext->Root_Topic;
	uint32_t	srclen;
	srclen = strlen(pcontext->Root_Topic);
	d = tmptopicstr;
	while (*sr != '\0') {
		*d = *sr; sr++; d++;
	}
	char	entity[] = {TOPIC_TEXT};		/* template */
	uint8_to_asciiz(ldid, &entity[6]);		/* convert ldid to text*/
	sr = entity;
	while (*sr != '\0') {
		*d = *sr; sr++; d++;
	}
	*d = '\0';					/* finalize */
	topicstr.cstring = tmptopicstr;
	topicstr.lenstring.len = (int16_t)strlen(tmptopicstr);

	uint16_t	packetid;
	packetid = 0U;

	len = (uint32_t)MQTTSNSerialize_register(buf, buflen, 0, packetid, &topicstr);

	result = write_socket(pcontext->outsoc, buf, len);
	if (result == ERROR)
	{
		goto fExit;
	}
/*	if (MQTTSNPacket_read(pcontext->insoc, buf, buflen, &read_socket) == MQTTSN_REGACK) 	*/ /* wait for regack */
	TickType_t	tout;
	tout = pdMS_TO_TICKS(500U);
	TickType_t	entree;
	entree = xTaskGetTickCount();
	int	MQTTSNPacket_read_result;
		MQTTSNPacket_read_result = MQTTSNPACKET_READ_ERROR;
	do {
		MQTTSNPacket_read_result = MQTTSNPacket_read(pcontext->insoc, buf, buflen, &read_socket_nowait);
		if (MQTTSNPacket_read_result !=  MQTTSNPACKET_READ_ERROR) {
			break;
		}
		taskYIELD();
	} while (xTaskGetTickCount() < (entree+tout));
	if (MQTTSNPacket_read_result ==  MQTTSN_REGACK)
	{
		uint16_t	submsgid;
		uint8_t		returncode;
		rc = MQTTSNDeserialize_regack(&topicid, &submsgid, &returncode, buf, buflen);
		if (returncode != 0)
		{
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT)
			xprintf("retcode %d\n", returncode);
#endif
			result = ERROR;
			goto fExit;
		} else {
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT)
		/*	xprintf("regack topic id %d\n", topicid); */
#endif
			DAQ_UpdateLD_callback(topicid, ldid, Pub);
			result = SUCCESS;
		}
	} else {
		result = ERROR;
	}
fExit:
	(void)rc;
	(void)srclen;
	return result;
} /* end of function mqtt_sn_register_topic */

/**
  * Publishes the topic at the MQTT-SN gateway
  * @param pcontext the pointer to the connection context
  * @param topicid the pre-registered topic id
  * @param *topicstr is the payload fot all the activity here )
  * @return ErrorStatus SUCCESS or ERROR
  */
ErrorStatus mqtt_sn_publish_topic(MQTT_SN_Context_p pcontext, uint16_t topicid, const char *topicstring)
{
	ErrorStatus	result;
	result = ERROR;

	if (pcontext->state != CONNECTED) {
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT)
		xprintf("not connected, unable to publish\n");
#endif
		goto fExit;
		}

	MQTTSN_topicid topic;
/*	MQTTSNString topicstr; */

	uint8_t		buf[100];
	int32_t		buflen;
	buflen = sizeof(buf);

	int32_t		len;
	len = 0;
	int32_t		rc;
//	rc = 0;

	uint8_t		dup;
	dup = 0;

	int qos;
	qos = 1;				/* At least once, ack */

	uint8_t retained;
	retained = 0;

	++pcontext->packetid;			/* increment the packet ID */

	/* publish with short name */

	topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
	topic.data.id = topicid;

	int32_t payloadlen;
	payloadlen = (int32_t)strlen(topicstring);

/* payload byte buffer - the MQTT publish payload */
/* payloadlen integer - the length of the MQTT payload	*/

	len = MQTTSNSerialize_publish(buf, buflen, dup, qos, retained, pcontext->packetid,
			topic, (uint8_t*)topicstring, payloadlen);

	result = write_socket(pcontext->outsoc, buf, len);

	/* wait for puback */
/*	rc = MQTTSNPacket_read(pcontext->insoc, buf, buflen, &read_socket); */
	TickType_t	tout;
	tout = pdMS_TO_TICKS(500U);
	TickType_t	entree;
	entree = xTaskGetTickCount();
		rc = MQTTSNPACKET_READ_ERROR;
	do {
		rc = MQTTSNPacket_read(pcontext->insoc, buf, buflen, &read_socket_nowait);
		if (rc !=  MQTTSNPACKET_READ_ERROR) {
			break;
		}
		taskYIELD();
	} while (xTaskGetTickCount() < (entree+tout));

	if (rc == MQTTSN_PUBACK) {
		result = SUCCESS;
	} else {
		result = ERROR;			/* need DISCONNECTING !!!*/
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT)
	xprintf("no PUBACK received, reconnecting\n");
#endif

	}
	{
#if	defined  (MQTT_SN_PUB_DEBUG_PRINT)
		uint16_t packet_id, topic_id;
		uint8_t returncode;

		if (MQTTSNDeserialize_puback(&topic_id, &packet_id, &returncode, buf, buflen) != 1 || returncode != MQTTSN_RC_ACCEPTED) {
			xprintf("unable to publish, retcode %d\n", returncode);
		} else {
/*			xprintf("puback received, id %d\n", packet_id)*/;
		}
#endif
		;
	}
fExit:
	return result;
} /* end of function mqtt_sn_publish_topic */

/**
  * De-Initialises MQTT-SN context
  * @param pcontext the pointer to the context to be de-initialized
  * @return ErrorStatus SUCCESS or ERROR
  */

ErrorStatus mqtt_sn_deinit_context(MQTT_SN_Context_p pcontext)
{
	ErrorStatus	result1;
	ErrorStatus	result2;

	result1 = ((close_socket(pcontext->insoc)) == NULL) ? SUCCESS : ERROR;
	result2 = ((close_socket(pcontext->outsoc)) == NULL) ? SUCCESS : ERROR;

	pcontext->state = IDLE;

	if ((result1 == SUCCESS) && (result2 == SUCCESS)) {
		return SUCCESS;
	} else {
		return ERROR;
	}
}  /* end of function mqtt_sn_deinit_context */

/**
  * Subscribes to the topic
  * @param pcontext the pointer to the context used for subscription
  * @param ldid to be subscribed, ?if ldid = 0xff then # used for subscription?
  * @return ErrorStatus SUCCESS or ERROR
  */
ErrorStatus mqtt_sn_subscribe_topic(MQTT_SN_Context_p pcontext, uint8_t ldid)
{
	ErrorStatus	result;
	result = ERROR;

	if (pcontext->state != CONNECTED) {
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
	xprintf("Not connected, can't subscibe\n");
#endif
	goto fExit;
	}
	uint8_t		buf[100];
	int32_t		buflen;
	buflen = sizeof(buf);
	uint32_t 	len;
	len = 0U;
	uint32_t 	rc;
	rc = 0U;

#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
	xprintf("subscribing...\n");
#endif
	MQTTSN_topicid topic;
	topic.type = MQTTSN_TOPIC_TYPE_NORMAL;

	uint16_t topicid;

/*	MQTTSNString 	topicstr; */

	/* compose topicname */
static	char	tmptopicstr[MAX_TOPICSTR_LEN];
	char	*sr;
	char	*d;
	sr = pcontext->Root_Topic;
	uint32_t	srclen;
	srclen = strlen(pcontext->Root_Topic);
	d = tmptopicstr;
	while (*sr != '\0') {
		*d = *sr; sr++; d++;
	}
	char	entity[] = {TOPIC_CMD};			/* template */
	uint8_to_asciiz(ldid, &entity[4]);		/* convert ldid to text*/
	sr = entity;
	while (*sr != '\0') {
		*d = *sr; sr++; d++;
	}
	*d = '\0';					/* finalize */

	uint16_t	packetid;
	packetid = 1U;
	topic.data.long_.name = tmptopicstr;
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
	xprintf(tmptopicstr); xputc('\n');
#endif
	topic.data.long_.len = strlen(topic.data.long_.name);
	len = MQTTSNSerialize_subscribe(buf, buflen, 0, 2, packetid, &topic);
	result = write_socket(pcontext->outsoc, buf, len);
	if (result == ERROR)
	{
		goto fExit;
	}
/*	osDelay(500U); */
	TickType_t	tout;
	tout = pdMS_TO_TICKS(500U);
	TickType_t	entree;
	entree = xTaskGetTickCount();
	int	MQTTSNPacket_read_result;
		MQTTSNPacket_read_result = MQTTSNPACKET_READ_ERROR;
	do {
		MQTTSNPacket_read_result = MQTTSNPacket_read(pcontext->insoc, buf, buflen, &read_socket_nowait);
		if (MQTTSNPacket_read_result !=  MQTTSNPACKET_READ_ERROR) {
			break;
		}
		taskYIELD();
	} while (xTaskGetTickCount() < (entree+tout));

/*	if (MQTTSNPacket_read(pcontext->insoc, buf, buflen, &read_socket) == MQTTSN_SUBACK) */	/* wait for suback */
	if ( MQTTSNPacket_read_result ==  MQTTSN_SUBACK)
	{
		unsigned short submsgid;
		int granted_qos;
		unsigned char returncode;

		rc = MQTTSNDeserialize_suback(&granted_qos, &topicid, &submsgid, &returncode, buf, buflen);

		DAQ_UpdateLD_callback(topicid, ldid, Sub);	/* update properties */
		pcontext->time_OK = xTaskGetTickCount();
		result = SUCCESS;

		if (granted_qos != 2 || returncode != 0)
		{
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
			xprintf("granted qos != 2, %d retcode %d\n", granted_qos, returncode);
#endif
			;  /*  comment  */
		} else {
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
			xprintf("SUBACK topic id %d\n", topicid);
#endif
			; /*  comment  */
		}
	} else {
		result = ERROR;
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
		xprintf("No SUBACK received for: ");
		xprintf(tmptopicstr); xprintf("\n");
#endif
	}
fExit:
	(void)rc;
	(void)srclen;
	return result;
}

/** polls network for the topics subscribed
  * @param pcontext the pointer to the context used for subscription
  * @return ErrorStatus SUCCESS or ERROR
  *
  */
ErrorStatus mqtt_sn_poll_subscribed(MQTT_SN_Context_p pcontext)
{
	ErrorStatus	result;
	result = ERROR;

	uint8_t		buf[100];
	int32_t		buflen;
	buflen = sizeof(buf);
	int32_t		len;


	int32_t	rcvd;
	/* no wait! */
	rcvd = MQTTSNPacket_read(pcontext->insoc, buf, buflen, &read_socket_nowait);
	if (rcvd == MQTTSN_PUBLISH) {

		pcontext->time_OK = xTaskGetTickCount();		/* save the time when MQTTSN_PUBLISH was received */

		uint16_t packet_id;
		int32_t qos;
		int32_t payloadlen;
		uint8_t* payload;
		uint8_t dup;
		uint8_t retained;
		MQTTSN_topicid pubtopic;
		if (MQTTSNDeserialize_publish(&dup, &qos, &retained, &packet_id, \
					   &pubtopic, &payload, &payloadlen, \
					   buf, buflen) != 1) {
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
			xputs("Error deserializing published data\n");
#endif
			goto fExit;
		} else {		/* all ok, received correct data */
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
			xprintf("publish received, id %d qos %d ->>", packet_id, qos);
			xprintf("%d\n", pcontext->time_OK);

#endif
			/* proceed topic */
			uint8_t * pl;
			pl = payload;
			while (payloadlen > 0) {
				xputc(*pl);
				pl++;
				payloadlen--;
			}
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
			xputc('\t');
			xprintf("topicid from payload:%d\t", pubtopic.data.id);
#endif
			/* dispatch topic */
			/* check if the packet_id is from the past */
			if ( (packet_id > pcontext->last_procd_packet_id) || \
			     ((packet_id < 0x8000U) && (pcontext->last_procd_packet_id > 0x8000U)) ) \
			     { /* normally sequenced packet id */
				DAQ_Dispatch(payload, pubtopic, packet_id);
				pcontext->last_procd_packet_id = packet_id;
				/* end of proceed topic */
			} else {
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
					xprintf("\npacketid %d is from the past!\n", packet_id);
#endif
			}
			result = SUCCESS;
			if (qos == 1U) {
				ErrorStatus pubackresult;
				len = MQTTSNSerialize_puback(buf, buflen, pubtopic.data.id, packet_id, MQTTSN_RC_ACCEPTED);
				pubackresult = write_socket(pcontext->outsoc, buf, len);
				if (pubackresult == SUCCESS) {
#if	defined  (MQTT_SN_SUB_DEBUG_PRINT)
					xputs("PUBACK sent\n");
#endif
					;
				}
			}
		}			/* of all ok, received correct data */
	} /* of MQTTSN_PUBLISH processing*/
	else {
		if ((xTaskGetTickCount() - pcontext->time_OK) > maxidle) {
			result = ERROR;
		} else {
			result = SUCCESS;
		}	/* stub */
	}
fExit:
	return result;
}


/* ######################### EOF ################################################################ */
