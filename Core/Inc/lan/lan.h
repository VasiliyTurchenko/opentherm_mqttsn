/* Header:  ENC28J60 ethernet stack
* File Name:  lan.h
* Author:  Livelover from www.easyelectronics.ru
* Modified for STM32 by turchenkov@gmail.com
* Date: 03-Jan-2017
*/

#ifndef		__LAN_H
#define		__LAN_H

#include <string.h>
#include "enc28j60.h"

#ifdef STM32F103xB
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_def.h"
#elif STM32F303xC
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_def.h"

#else

#error "MCU TARGET NOT DEFINED!"

#endif


#include "main.h"

/*
 * Options
 */

#define WITH_ICMP
#define WITH_DHCP
			#undef	WITH_DHCP/* !!!!!!!!!!!!!!!!!!!!!!!!!!!! */
#define WITH_UDP
#define WITH_TCP
			#undef WITH_TCP	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
#define WITH_TCP_REXMIT
			#undef WITH_TCP_REXMIT /* !!!!!!!!!!!!!!!!!!!!!! */
//#define	NUM_ETH_BUFFERS		10U
#define	NUM_ETH_BUFFERS		6U
//#define	NUM_SOCKETS		NUM_ETH_BUFFERS
#define	NUM_SOCKETS		10U

#define ARP_TIMEOUT		20U	/*milliseconds */
#define NUM_ARP_RETRIES		50U

#define	ARP_TIMEOUT_MS		((uint32_t)(2*60*60*1000))	/* 2 hours */


#define	START_EUPH_PORT		50000U


#define		FRAME_BUSY	((uint8_t)0)
#define		FRAME_NOT_BUSY	((uint8_t)(~FRAME_BUSY))

#define		UDP_PAYLOAD_START	((uint16_t)42)
#define		UDP_PAYLOAD_SIZE	((uint16_t)(256))



/* typedef	uint8_t 		udp_data_t[UDP_PAYLOAD_SIZE]; */

enum	EthBufState			/*!< ethernet buffer state */
	{
		ETH_BUF_BUSY = 1,
		ETH_BUF_FREE = 2,
	};
enum	SocketState			/*!< socket state */
	{
		SOCK_BUSY = 1,
		SOCK_FREE = 2,
	};


enum	YesNo				/*!< type for events */
	{
		pad1,			/*!< skip zero*/
		YES,
		NO
	};


/*
 * Config
 */

#define MAC_ADDR			{0x00,0x13,0x37,0x01,0x23,0x45}

#ifndef WITH_DHCP
#	define IP_ADDR			inet_addr(192,168,0,222)
#	define IP_SUBNET_MASK		inet_addr(255,255,255,0)
#	define IP_DEFAULT_GATEWAY	inet_addr(192,168,0,1)
#endif

#define ARP_CACHE_SIZE			3
#define IP_PACKET_TTL			64
#define TCP_MAX_CONNECTIONS		5
#define TCP_WINDOW_SIZE			65535
#define TCP_SYN_MSS			512
#ifdef WITH_TCP_REXMIT
#	define TCP_REXMIT_TIMEOUT	1000
#	define TCP_REXMIT_LIMIT		5
#else
#	define TCP_CONN_TIMEOUT		2500
#endif

#define		UDP_PAYLOAD_START	((uint16_t)42)

/*
 * BE conversion
 */

//#define htons(a)			( (uint16_t)((((a)>>8u)&0x00ffu) | (((a)<<8u)&0xff00u)) )

#define htons(a) ( (uint16_t)(((uint16_t)((uint16_t)a >> 8) & 0x00FFU) | ((uint16_t)((uint16_t)a << 8) & 0xFF00U)) )

#define ntohs(a)			htons(a)

/*
#define htonl(a)			( (((a)>>24)&0xff) | (((a)>>8)&0xff00) | \
					  (((a)<<8)&0xff0000) | (((a)<<24)&0xff000000) )
#define ntohl(a)			htonl(a)
*/

#define inet_addr(a,b,c,d)	( ((uint32_t)a) | ((uint32_t)b << 8) | \
				((uint32_t)c << 16) | ((uint32_t)d << 24) )

/*
 * Ethernet
 */

#define ETH_TYPE_ARP		htons(0x0806)
#define ETH_TYPE_IP		htons(0x0800)

typedef struct __attribute__((packed)) eth_frame {
			uint8_t		to_addr[6];
			uint8_t		from_addr[6];
			uint16_t	type;
			uint8_t		data[];
	} eth_frame_t;

/*
 * ARP
 */

#define ARP_HW_TYPE_ETH		htons(0x0001)
#define ARP_PROTO_TYPE_IP	htons(0x0800)

#define ARP_TYPE_REQUEST	htons(1)
#define ARP_TYPE_RESPONSE	htons(2)

typedef  struct __attribute__((packed)) arp_message {
			uint16_t	hw_type;
			uint16_t 	proto_type;
			uint8_t 	hw_addr_len;
			uint8_t 	proto_addr_len;
			uint16_t 	type;
			uint8_t 	mac_addr_from[6];
			uint32_t 	ip_addr_from;
			uint8_t 	mac_addr_to[6];
			uint32_t 	ip_addr_to;
	} arp_message_t;

typedef struct arp_cache_entry {
			uint32_t	ip_addr;
			int32_t		age;
			uint8_t		mac_addr[6];

	} arp_cache_entry_t;

/*
 * IP
 */
/* added 14-Lan-2018 */

#define			SOC_MODE_READ		(uint8_t)0x01 /*!< read data w/o waiting */
#define			SOC_MODE_WRITE		(uint8_t)0x02 /*!< for transmission */
#define			SOC_NEW_DATA		(uint8_t)0x80 /*!< new data has arrived */


#define			SOC_ERR_WRONG_SOC_MODE	(uint16_t)0x01	/*!< socket mode id is inadequate to operation */
#define			SOC_ERR_NOT_ENOUGH_MEM_BUF  (uint16_t)0x02 /*!< not enough buffer memory to hold received data */

enum	DataLost_	{		/* Is the data lost or not */
	SOC_DATA_NOT_LOST = 0,
	SOC_DATA_LOST
	};

typedef	enum	DataLost_	DataLost_t;




typedef  struct /*__attribute__((packed))*/ socket {
			uint8_t			*buf;			/*!< ptr to the buffer for eth.frame*/
			uint32_t		loc_ip_addr;		/*!< local IP*/
			uint32_t		rem_ip_addr;		/*!< remote IP */
			uint16_t		rem_port;		/*!< remote port */
			uint16_t		loc_port;		/*!< local port */
			uint16_t		last_error;		/*!< last error */
			uint16_t		len;			/*!< length of of the received eth.frame */
			enum SocketState        soc_state;
			uint8_t			proto;			/*!< protocol */
			uint8_t			mode;			/*!< read or write ?*/
			DataLost_t		datalost;		/*!< the previous new data is overwritten */
	} socket_t;

typedef	socket_t	*socket_p;				/*!< pointer to the socket */


#define IP_PROTOCOL_ICMP	1
#define IP_PROTOCOL_TCP		6
#define IP_PROTOCOL_UDP		17

typedef struct __attribute__((packed)) ip_packet {
			uint8_t 	ver_head_len;
			uint8_t 	tos;
			uint16_t 	total_len;
			uint16_t 	fragment_id;
			uint16_t	flags_framgent_offset;
			uint8_t		ttl;
			uint8_t		protocol;
			uint16_t	cksum;
			uint32_t	from_addr;
			uint32_t	to_addr;
			uint8_t		data[];
	} ip_packet_t;


/*
 * ICMP
 */

#define ICMP_TYPE_ECHO_RQ	8
#define ICMP_TYPE_ECHO_RPLY	0

typedef struct __attribute__((packed)) icmp_echo_packet {
			uint8_t		type;
			uint8_t		code;
			uint16_t	cksum;
			uint16_t	id;
			uint16_t	seq;
			uint8_t		data[];
	} icmp_echo_packet_t;


/*
 * UDP
 */

typedef struct __attribute__((packed)) udp_packet {
			uint16_t	from_port;
			uint16_t	to_port;
			uint16_t	len;
			uint16_t	cksum;
			uint8_t		data[];
	} udp_packet_t;


/*
 * TCP
 */

#define TCP_FLAG_URG		0x20
#define TCP_FLAG_ACK		0x10
#define TCP_FLAG_PSH		0x08
#define TCP_FLAG_RST		0x04
#define TCP_FLAG_SYN		0x02
#define TCP_FLAG_FIN		0x01

typedef struct __attribute__((packed)) tcp_packet {
			uint16_t	from_port;
			uint16_t	to_port;
			uint32_t	seq_num;
			uint32_t	ack_num;
			uint8_t		data_offset;
			uint8_t		flags;
			uint16_t	window;
			uint16_t	cksum;
			uint16_t	urgent_ptr;
			uint8_t		data[];
	} tcp_packet_t;

#define tcp_head_size(tcp)	(((tcp)->data_offset & 0xf0) >> 2)
#define tcp_get_data(tcp)	((uint8_t*)(tcp) + tcp_head_size(tcp))

typedef enum 	tcp_status_code {
			TCP_CLOSED,
			TCP_SYN_SENT,
			TCP_SYN_RECEIVED,
			TCP_ESTABLISHED,
			TCP_FIN_WAIT
	} tcp_status_code_t;

typedef struct __attribute__((packed)) tcp_state {
		tcp_status_code_t	status;
		uint32_t		event_time;
		uint32_t		seq_num;
		uint32_t		ack_num;
		uint32_t		remote_addr;
		uint16_t		remote_port;
		uint16_t		local_port;
#ifdef WITH_TCP_REXMIT
		uint8_t			is_closing;
		uint8_t			rexmit_count;
		uint32_t		seq_num_saved;
#endif
	} tcp_state_t;

typedef enum tcp_sending_mode {
		TCP_SENDING_SEND,
		TCP_SENDING_REPLY,
		TCP_SENDING_RESEND
	} tcp_sending_mode_t;

#define TCP_OPTION_PUSH			0x01
#define TCP_OPTION_CLOSE		0x02

/*
 * DHCP
 */

#define DHCP_SERVER_PORT		htons(67)
#define DHCP_CLIENT_PORT		htons(68)

#define DHCP_OP_REQUEST			1
#define DHCP_OP_REPLY			2

#define DHCP_HW_ADDR_TYPE_ETH		1

#define DHCP_FLAG_BROADCAST		htons(0x8000)

#define DHCP_MAGIC_COOKIE		htonl(0x63825363)

typedef struct __attribute__((packed)) dhcp_message {
		uint8_t			operation;
		uint8_t			hw_addr_type;
		uint8_t			hw_addr_len;
		uint8_t			unused1;
		uint32_t		transaction_id;
		uint16_t		second_count;
		uint16_t		flags;
		uint32_t		client_addr;
		uint32_t		offered_addr;
		uint32_t		server_addr;
		uint32_t		unused2;
		uint8_t			hw_addr[16];
		uint8_t			unused3[192];
		uint32_t 		magic_cookie;
		uint8_t options[];
	} dhcp_message_t;

#define DHCP_CODE_PAD			(uint8_t)0
#define DHCP_CODE_END			(uint8_t)255
#define DHCP_CODE_SUBNETMASK		(uint8_t)1
#define DHCP_CODE_GATEWAY		(uint8_t)3
#define DHCP_CODE_REQUESTEDADDR		(uint8_t)50
#define DHCP_CODE_LEASETIME		(uint8_t)51
#define DHCP_CODE_MESSAGETYPE		(uint8_t)53
#define DHCP_CODE_DHCPSERVER		(uint8_t)54
#define DHCP_CODE_RENEWTIME		(uint8_t)58
#define DHCP_CODE_REBINDTIME		(uint8_t)59

typedef struct __attribute__((packed)) dhcp_option {
		uint8_t			code;
		uint8_t			len;
		uint8_t			data[];
	} dhcp_option_t;

#define DHCP_MESSAGE_DISCOVER		1
#define DHCP_MESSAGE_OFFER		2
#define DHCP_MESSAGE_REQUEST		3
#define DHCP_MESSAGE_DECLINE		4
#define DHCP_MESSAGE_ACK		5
#define DHCP_MESSAGE_NAK		6
#define DHCP_MESSAGE_RELEASE		7
#define DHCP_MESSAGE_INFORM		8

typedef enum dhcp_status_code {
		DHCP_INIT,
		DHCP_ASSIGNED,
		DHCP_WAITING_OFFER,
		DHCP_WAITING_ACK
	} dhcp_status_code_t;

/*
 * LAN
 */

/*extern uint8_t 	net_buf[NUM_ETH_BUFFERS][ENC28J60_MAXFRAME]; */

#ifndef WITH_DHCP
extern uint32_t ip_addr;
extern uint32_t ip_mask;
extern uint32_t ip_gateway;
#endif

extern uint8_t mac_addr[6];

// LAN calls
void lan_init(void);
void lan_poll(void);
uint8_t lan_up(void);


// UDP calls
uint8_t udp_send(eth_frame_t *frame, uint16_t len);
void udp_reply(eth_frame_t *frame, uint16_t len);

// TCP callbacks
// uint8_t tcp_listen(uint8_t id, eth_frame_t *frame);
// void tcp_read(uint8_t id, eth_frame_t *frame, uint8_t re);
// void tcp_write(uint8_t id, eth_frame_t *frame, uint16_t len);
// void tcp_closed(uint8_t id, uint8_t hard);

// TCP calls
uint8_t tcp_open(uint32_t addr, uint16_t port, uint16_t local_port);
void tcp_send(uint8_t id, eth_frame_t *frame, uint16_t len, uint8_t options);

// htonl
uint32_t htonl(uint32_t a);

#define ntohl(num)	(htonl(num))


/**
  * assigns a socket from free pool
  * @param remIP remote IP
  * @param remPort remote port
  * @param locPort local port
  * @param mode opening mode - read or write
  * if locPort = 0, then the local port will be automatically selected
  * @return socket_p pointer to the socket if OK; NULL otherwise
  *
  */
socket_p bind_socket(const uint32_t remIP, const uint16_t remPort, \
						   const uint16_t locPort, \
						   const uint8_t mode);

/**
  * releases a socket to the pool
  * @param soc pointer to the socket
  * @return NULL if OK, pointer to the socket if not OK
  *
  */
socket_p close_socket(socket_p soc);

/**
  * sends data from buf via previously opened socket
  * @param soc the pointer to the socket
  * @param *buf is the pointer to the data
  * @param buflen length of the data
  * @return ErrorStatus SUCCESS or ERROR
  */
ErrorStatus write_socket(socket_p soc, uint8_t* buf, int32_t buflen);

/** reads bytes from network
  * @param  soc pointer to the previously open socket
  * @return number of received bytes if OK; NULL if not ok, last_error field contains additional info
  */
uint16_t read_socket(socket_p soc, uint8_t* buf, int32_t buflen);

/** runs out of freertos!  every 1ms by TIM0 stimulus
  * returns num of free netbuffers
  * @param none
  * @return nb number of free buffers
  */
void	nb_stat(void);

/** decrements time to live of the arp cache entry every 1ms
  * invalidates the outdated entry
  * @param none
  * @return none
  */

void arp_age_entries(void);

/** reads bytes from network  without waiting for data     THREAD - SAFE
  * @param  soc pointer to the previously open socket
  * @return number of received bytes if OK; NULL if not ok, last_error field contains additional info
  */
uint16_t read_socket_nowait(socket_p soc, uint8_t* buf, int32_t buflen);

/**
  * changes mode of previously opened socket
  * @param  soc - socket to be changed
  * @param  newmode - the mode to change to
  * @retval soc pointer in OK, NULL if ERROR
  */
socket_p change_soc_mode(socket_p soc, const uint8_t mode);

#endif
/* ############################################################################################# */
