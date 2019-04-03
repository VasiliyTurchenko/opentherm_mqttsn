/** @file config_files.h
 *  @brief configuration files definitions
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 10-Mar-2019
 */


#ifndef CONFIG_FILES_H
#define CONFIG_FILES_H

#ifdef __cplusplus
 extern "C" {
#endif

#define MAC_LEN 12U
#define IP_LEN 12U
#define PORT_LEN 5U

#ifndef MAX_FILENAME_LEN
#define MAX_FILENAME_LEN 7U	/* look at tiny-fs.h */
#endif

struct __attribute__((packed)) MAC {
	char mac_h[4];
	char mac_v[MAC_LEN];
	char mac_pad[6];
};

struct __attribute__((packed)) IP {
	char ip_n[4];
	char ip_v[IP_LEN];
	char ip_p[PORT_LEN];
	char ip_nl[1];
};

struct __attribute__((packed)) ip_cfg {
	struct MAC MAC_addr;
	struct IP Own_IP;
	struct IP Net_Mask;
	struct IP GW;
	char zero;
};


#define		ROOT_TOPIC_LEN (40)
#define		MAX_TOPICSTR_LEN	(ROOT_TOPIC_LEN + 10)


struct __attribute__((packed)) MQTT_topic_para {
	char rootTopic[ROOT_TOPIC_LEN];
	char topicText[10];
	char pub_client_id_string[11];
};


struct __attribute__((packed)) MQTT_parameters {
	struct IP MQTT_IP_parameters;			 /* struct maps to their file */
	char MQTT_IP_filename[MAX_FILENAME_LEN + 1];
	struct MQTT_topic_para MQTT_topic_parameters; /* struct maps to their file */
	char MQTT_topic_filename[MAX_FILENAME_LEN + 1];
	char * long_taskName;
	char * short_taskName;
};


/* IP_Cfg_File format */
//static const char *IP_Cfg_File_Default =
//	"MAC_AABBCCDDEEFF     \n" // ENC28J60 MAC
//	"IP__19216800000100000\n" // ip address
//	"NM__25525525500000000\n" // netmask
//	"GW__19216800000100000\n" // default gateway


/* Log_IP_Cfg_File format */
//	"LIP_19216800000105000\n" // log listener ip: port

/* NTP_IP_Cfg_File format */
//	"NTP119216800000100123\n" // NTP server 1 ip: port
//	"NTP219216800000100123\n";// NTP server 2 ip: port


#ifdef __cplusplus
 }
#endif


#endif // CONFIG_FILES_H
