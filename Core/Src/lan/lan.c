/* Header:  ENC28J60 ethernet stack
* File Name: lan.c
* Author:  Livelover from www.easyelectronics.ru
* Modified for STM32 by turchenkov@gmail.com
* Date: 03-Jan-2017
*/

#include <stdbool.h>
#include <limits.h>

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "mutex_helpers.h"

#include "lan.h"
#include "logging.h"
#include "hex_gen.h"

#define NET_BUF_STAT

#define ETH_MAXFRAME ENC28J60_MAXFRAME
//#define		UDP_PAYLOAD_START	((uint16_t)42)

// MAC address
uint8_t mac_addr[6];

/* ARP MUTEX */
static osMutexId ARP_MutexHandle __attribute__((section(".ccmram")));
static osStaticMutexDef_t ARP_Mutex_ControlBlock __attribute__((section(".ccmram")));

// for statistic purpose

static volatile uint8_t max_pack_cnt = 0;
static volatile uint32_t eth_filter_misses = 0;
static volatile uint32_t ip_filter_misses = 0;

static volatile uint8_t minfreenb = NUM_ETH_BUFFERS;
static volatile uint8_t freenb = NUM_ETH_BUFFERS;

static volatile uint32_t udp_fits_callbacks = 0u;
static volatile uint32_t udp_callbacks = 0u;
static volatile uint32_t readsocfits = 0u;
static volatile uint32_t socdatalosts = 0u;
static volatile uint32_t socfastreads = 0u;
static volatile uint32_t lan_getmem_errors = 0u;
static volatile uint32_t lan_freemem_errors = 0u;
static volatile uint32_t lan_poll_mallocs = 0u;
static volatile uint32_t lan_poll_frees = 0u;
static volatile uint32_t readsoc_mallocs = 0u;
static volatile uint32_t readsoc_frees = 0u;
static volatile uint32_t wr_mallocs_frees = 0u;
static volatile uint32_t arp_mallocs_frees = 0u;
static volatile uint32_t wr_soc_err = 0u;

// IP address/mask/gateway
#ifndef WITH_DHCP
uint32_t ip_addr;
uint32_t ip_mask;
uint32_t ip_gateway;
#endif

#define ip_broadcast (ip_addr | ~ip_mask)

// Packet buffers
static uint8_t eth_buf[NUM_ETH_BUFFERS][ENC28J60_MAXFRAME]; /* ethernet buffers*/

static socket_t sockets[NUM_SOCKETS]; /*  sockets pool */
/* each net_buf belongs to the one of the sockets */
/* each eth_buf_state belongs to the one of the sockets*/
static enum EthBufState eth_buf_state[NUM_ETH_BUFFERS]; /* states of the ethernet buffers */

/**
 * @brief arp_cache ARP cache array
 */
static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];

/* */
static void icmp_filter(eth_frame_t *frame, uint16_t len);

/* ARP functions */
static void arp_filter(eth_frame_t *frame, uint16_t len);
static uint8_t *arp_resolve(uint32_t node_ip_addr);
static uint8_t *arp_search_cache(uint32_t node_ip_addr);
static void request_arp(uint32_t node_ip_addr);
static int arp_get_cache_index(uint32_t node_ip_addr);
static inline void arp_clear_cache(void);

/* Ethernet level */
static void eth_send(eth_frame_t *frame, uint16_t len);
static void eth_reply(eth_frame_t *frame, uint16_t len);
static void eth_resend(eth_frame_t *frame, uint16_t len);
static eth_frame_t *eth_filter(eth_frame_t *frame, uint16_t len);

/* IP level */
static uint8_t ip_send(eth_frame_t *frame, uint16_t len);
static void ip_reply(eth_frame_t *frame, uint16_t len);
static void ip_resend(eth_frame_t *frame, uint16_t len);
static uint16_t ip_cksum(uint32_t sum, uint8_t *buf, uint16_t len);
static eth_frame_t *ip_filter(eth_frame_t *frame, uint16_t len);

#if (0)
void dhcp_filter(eth_frame_t *frame, uint16_t len);
#endif

/* TCP stubs */
static uint8_t tcp_listen(uint8_t id, eth_frame_t *frame);
static void tcp_read(uint8_t id, eth_frame_t *frame, uint8_t re);
static void tcp_write(uint8_t id, eth_frame_t *frame, uint16_t len);
static void tcp_closed(uint8_t id, uint8_t hard);

/* memory dispatcher */
static uint8_t *lan_getmem(void);
static uint8_t *lan_freemem(uint8_t *buf);

uint8_t *getMAC(void);

/* not used right now */
static uint32_t rtime(void);

/* UDP level */
static eth_frame_t *udp_packet_callback(eth_frame_t *frame, uint16_t len);
static eth_frame_t *udp_filter(eth_frame_t *frame, uint16_t len);

/* raw socket read */
static uint16_t read_sock(socket_p soc, uint8_t *buf, int32_t buflen, const uint8_t attempts);

/**
 * @brief set_notif_params
 * @param soc
 * @param TaskToNotify
 * @param readTimeOutMS
 * @return
 */
socket_p set_notif_params(socket_p soc, void *TaskToNotify, uint32_t readTimeOutMS)
{
	socket_p retVal = soc;
	if (soc != NULL) {
		soc->TaskToNotify = TaskToNotify;
		soc->readTimeOutMS = readTimeOutMS;
	}
	return retVal;
}

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
socket_p bind_socket(const uint32_t remIP, const uint16_t remPort, const uint16_t locPort,
		     const uint8_t mode)
{
	taskENTER_CRITICAL(); // call may be nested
	socket_p result;
	result = NULL;
	uint8_t i;
	for (i = 0U; i < NUM_SOCKETS; i++) {
		/*		if (eth_buf_state[i] == FREE) {		*/ /* is this buffer free ?*/
		if (sockets[i].soc_state == SOCK_FREE) {
			/* try to allocate buffer */
			/*			sockets[i].buf = lan_getmem();
				if (sockets[i].buf != NULL) {	*/	/* memory allocated */
			/* +15-Mar-2018 */ sockets[i].buf = NULL;
			sockets[i].datalost = SOC_DATA_NOT_LOST;
			sockets[i].soc_state = SOCK_BUSY; /* mark the socket as busy */
			sockets[i].rem_ip_addr = remIP;
			sockets[i].rem_port = remPort;
			sockets[i].last_error = 0U;	 /* no error */
			sockets[i].proto = IP_PROTOCOL_UDP; /* currently UDP only */
			sockets[i].mode = mode;
			sockets[i].len = 0U;
			sockets[i].loc_ip_addr = ip_addr;
			if (locPort != 0U) {
				sockets[i].loc_port = locPort;
			} else {
				sockets[i].loc_port = (START_EUPH_PORT + i);
			}
			result = &sockets[i]; /* return ptr to it's socket */
			/*	15-Mar-2018			} */
			break;
		}
	}
	taskEXIT_CRITICAL(); // call may be nested
	return result;
} /* end of the function bind_socket */

/**
  * @brief  changes mode of previously opened socket
  * @note
  * @param  soc - socket to be changed
  * @param  mode - the mode to change to
  * @retval soc pointer in OK, NULL if ERROR
  */
socket_p change_soc_mode(socket_p soc, const uint8_t mode)
{
	socket_p result;
	result = NULL;
	/* function is safe against null pointers (11-Mar-2018) */
	if ((soc == NULL) || (soc->soc_state == SOCK_FREE)) {
		return NULL;
	}
	if ((mode == SOC_MODE_READ) || (mode == SOC_MODE_WRITE)) {
		taskENTER_CRITICAL();
		uint8_t i;
		for (i = 0; i < NUM_SOCKETS; i++) {
			if (soc == &sockets[i]) { /* soc points to the socket */
				soc->mode = mode;
				result = soc;
				break;
			}
		}
		taskEXIT_CRITICAL();
	}
	return result;
}
/* end of the function change_soc_mode */

/**
  * releases a socket to the pool
  * @param soc pointer to the socket
  * @return NULL if OK, pointer to the socket if not OK
  *
  */
socket_p close_socket(socket_p soc)
{
	if (soc == NULL) { /* function is safe for null pointers (11-Mar-2018) */
		return NULL;
	}
	taskENTER_CRITICAL();
	socket_p result;
	result = soc;
	uint8_t i;
	for (i = 0; i < NUM_SOCKETS; i++) {
		if ((soc == &sockets[i]) && (soc->soc_state == SOCK_BUSY)) {
			/* try to free buffer memory */
			if (lan_freemem(soc->buf) != NULL) {
				/* memory manager error !*/
				UNUSED(0);
			};
			memset(soc, 0, sizeof(socket_t));
			soc->soc_state = SOCK_FREE;
			result = NULL; /* return NULL */
			break;
		}
	}
	taskEXIT_CRITICAL();
	return result;
} /* end of the function close_socket */

/**
  * searches free ethternet buffer from the pool and returns pointer to it
  * @param none
  * @return pointer to the available buffer or NULL in there is no free buffers left
  */					/*		THREAD-SAFE */
/* modified 13-Mar-2018 to implement zero-copy */
static uint8_t *lan_getmem(void)
{
	taskENTER_CRITICAL();
	uint8_t *result;
	result = NULL;
	uint8_t i;
	for (i = 0u; i < NUM_ETH_BUFFERS; i++) {
		if (eth_buf_state[i] == ETH_BUF_FREE) {  /* is this buffer free ?*/
			result = (uint8_t *)eth_buf[i];  /* return ptr to it */
			eth_buf_state[i] = ETH_BUF_BUSY; /* mark the buffer */
			break;
		}
	}
#ifdef NET_BUF_STAT
	uint8_t tmp = 0u;
	for (i = 0u; i < NUM_ETH_BUFFERS; i++) {
		tmp = (eth_buf_state[i] == ETH_BUF_FREE) ? (tmp + 1u) : tmp;
	}
	freenb = tmp;
	minfreenb = (tmp < minfreenb) ? tmp : minfreenb;
#endif
	taskEXIT_CRITICAL();
	return result;
}

/**
  * releases ethternet buffer to the pool
  * @param buf pointer to the buffer
  * @return NULL if successfully released, pointer to the buffer in not
  */                                   /*		THREAD-SAFE */
/* modified 13-Mar-2018 to implement zero-copy */
static uint8_t *lan_freemem(uint8_t *buf)
{
	taskENTER_CRITICAL();
	uint8_t *result;
	result = buf;
	if (buf != NULL) {
		uint8_t i;
		for (i = 0u; i < NUM_ETH_BUFFERS; i++) {
			if (((uint8_t *)eth_buf[i] == buf) &&
			    (eth_buf_state[i] == ETH_BUF_BUSY)) { /* is it the buffer ? */
				eth_buf_state[i] = ETH_BUF_FREE;  /* mark the buffer as free*/
				result = NULL;
				break;
			}
		}
	}
#ifdef NET_BUF_STAT
	uint8_t tmp = 0u;
	uint8_t i;
	for (i = 0u; i < NUM_ETH_BUFFERS; i++) {
		tmp = (eth_buf_state[i] == ETH_BUF_FREE) ? (tmp + 1u) : tmp;
	}
	freenb = tmp;
	minfreenb = (tmp < minfreenb) ? tmp : minfreenb;
#endif
	taskEXIT_CRITICAL();
	return result;
}

// TCP connection pool
#ifdef WITH_TCP
tcp_state_t tcp_pool[TCP_MAX_CONNECTIONS];
#endif

// DHCP
#ifdef WITH_DHCP
dhcp_status_code_t dhcp_status;
static uint32_t dhcp_server;
static uint32_t dhcp_renew_time;
static uint32_t dhcp_retry_time;
static uint32_t dhcp_transaction_id;
static uint32_t ip_addr;
static uint32_t ip_mask;
static uint32_t ip_gateway;
#endif

/*
 * DHCP
 */

#ifdef WITH_DHCP

#define dhcp_add_option(ptr, optcode, type, value)                                                 \
	((dhcp_option_t *)ptr)->code = optcode;                                                    \
	((dhcp_option_t *)ptr)->len = sizeof(type);                                                \
	*(type *)(((dhcp_option_t *)ptr)->data) = value;                                           \
	ptr += sizeof(dhcp_option_t) + sizeof(type);                                               \
	if (sizeof(type) & 1)                                                                      \
		*(ptr++) = 0;

void dhcp_filter(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void *)(frame->data);
	udp_packet_t *udp = (void *)(ip->data);
	dhcp_message_t *dhcp = (void *)(udp->data);
	dhcp_option_t *option;
	uint8_t *op, optlen;
	uint32_t offered_net_mask = 0, offered_gateway = 0;
	uint32_t lease_time = 0, renew_time = 0, renew_server = 0;
	uint8_t type = 0;
	uint32_t temp;

	// Check if DHCP messages directed to us
	if ((len >= sizeof(dhcp_message_t)) && (dhcp->operation == (uint8_t)DHCP_OP_REPLY) &&
	    (dhcp->transaction_id == dhcp_transaction_id) &&
	    (dhcp->magic_cookie == DHCP_MAGIC_COOKIE)) {
		len -= (uint16_t)(sizeof(dhcp_message_t));

		// parse DHCP message
		op = dhcp->options;
		while (len >= sizeof(dhcp_option_t)) {
			option = (void *)op;
			if (option->code == DHCP_CODE_PAD) {
				op++;
				len--;
			} else if (option->code == DHCP_CODE_END) {
				break;
			} else {
				switch (option->code) {
				case DHCP_CODE_MESSAGETYPE:
					type = *(option->data);
					break;
				case DHCP_CODE_SUBNETMASK:
					offered_net_mask = *(uint32_t *)(option->data);
					break;
				case DHCP_CODE_GATEWAY:
					offered_gateway = *(uint32_t *)(option->data);
					break;
				case DHCP_CODE_DHCPSERVER:
					renew_server = *(uint32_t *)(option->data);
					break;
				case DHCP_CODE_LEASETIME:
					temp = *(uint32_t *)(option->data);
					lease_time = ntohl(temp);
					if (lease_time > (uint32_t)21600)
						lease_time = (uint32_t)21600;
					break;
				case DHCP_CODE_RENEWTIME:
					temp = *(uint32_t *)(option->data);
					renew_time = ntohl(temp);
					if (renew_time > (uint32_t)21600)
						renew_time = (uint32_t)21600;
					break;
				default:
					/* MISRA requires comment here!*/
					break;
				}

				optlen = sizeof(dhcp_option_t) + option->len;
				op += optlen;
				len -= optlen;
			}
		}

		if (!renew_server)
			renew_server = ip->from_addr;

		switch (type) {
		// DHCP offer?
		case DHCP_MESSAGE_OFFER:
			if ((dhcp_status == DHCP_WAITING_OFFER) && (dhcp->offered_addr != 0)) {
				dhcp_status = DHCP_WAITING_ACK;

				// send DHCP request
				ip->to_addr = inet_addr(255, 255, 255, 255);

				udp->to_port = DHCP_SERVER_PORT;
				udp->from_port = DHCP_CLIENT_PORT;

				op = dhcp->options;
				dhcp_add_option(op, DHCP_CODE_MESSAGETYPE, uint8_t,
						DHCP_MESSAGE_REQUEST);
				dhcp_add_option(op, DHCP_CODE_REQUESTEDADDR, uint32_t,
						dhcp->offered_addr);
				dhcp_add_option(op, DHCP_CODE_DHCPSERVER, uint32_t, renew_server);
				*(op++) = DHCP_CODE_END;

				dhcp->operation = DHCP_OP_REQUEST;
				dhcp->offered_addr = 0;
				dhcp->server_addr = 0;
				dhcp->flags = DHCP_FLAG_BROADCAST;

				udp_send(frame, (uint8_t *)op - (uint8_t *)dhcp);
			}
			break;

		// DHCP ack?
		case DHCP_MESSAGE_ACK:
			if (dhcp_status == DHCP_WAITING_ACK) {
				if (!renew_time)
					renew_time = lease_time / 2;

				dhcp_status = DHCP_ASSIGNED;
				dhcp_server = renew_server;
				dhcp_renew_time = rtime() + renew_time;
				dhcp_retry_time = rtime() + lease_time;

				// network up
				ip_addr = dhcp->offered_addr;
				ip_mask = offered_net_mask;
				ip_gateway = offered_gateway;
			}
			break;
		default:
			break;
		}
	}
}

void dhcp_poll()
{
	eth_frame_t *frame = (void *)net_buf;
	ip_packet_t *ip = (void *)(frame->data);
	udp_packet_t *udp = (void *)(ip->data);
	dhcp_message_t *dhcp = (void *)(udp->data);
	uint8_t *op;

	// Too slow (
	/* Link is down
	if(!(enc28j60_read_phy(PHSTAT1) & PHSTAT1_LLSTAT))
	{
		dhcp_status = DHCP_INIT;
		dhcp_retry_time = rtime() + 2;

		*/
	/* network down */ /*
		ip_addr = 0;
		ip_mask = 0;
		ip_gateway = 0;

		return;
	}*/

	// time to initiate DHCP
	//  (startup/lease end)
	if (rtime() >= dhcp_retry_time) {
		dhcp_status = DHCP_WAITING_OFFER;
		dhcp_retry_time = rtime() + 15;
		dhcp_transaction_id = HAL_GetTick() + (HAL_GetTick() << 16);

		// network down
		ip_addr = 0;
		ip_mask = 0;
		ip_gateway = 0;

		// send DHCP discover
		ip->to_addr = inet_addr(255, 255, 255, 255);

		udp->to_port = DHCP_SERVER_PORT;
		udp->from_port = DHCP_CLIENT_PORT;

		memset(dhcp, 0, sizeof(dhcp_message_t));
		dhcp->operation = DHCP_OP_REQUEST;
		dhcp->hw_addr_type = DHCP_HW_ADDR_TYPE_ETH;
		dhcp->hw_addr_len = 6;
		dhcp->transaction_id = dhcp_transaction_id;
		dhcp->flags = DHCP_FLAG_BROADCAST;
		memcpy(dhcp->hw_addr, mac_addr, 6);
		dhcp->magic_cookie = DHCP_MAGIC_COOKIE;

		op = dhcp->options;
		dhcp_add_option(op, DHCP_CODE_MESSAGETYPE, uint8_t, DHCP_MESSAGE_DISCOVER);
		*(op++) = DHCP_CODE_END;

		udp_send(frame, (uint8_t *)op - (uint8_t *)dhcp);
	}

	// time to renew lease
	if ((rtime() >= dhcp_renew_time) && (dhcp_status == DHCP_ASSIGNED)) {
		dhcp_transaction_id = HAL_GetTick() + (HAL_GetTick() << 16);

		// send DHCP request
		ip->to_addr = dhcp_server;

		udp->to_port = DHCP_SERVER_PORT;
		udp->from_port = DHCP_CLIENT_PORT;

		memset(dhcp, 0, sizeof(dhcp_message_t));
		dhcp->operation = DHCP_OP_REQUEST;
		dhcp->hw_addr_type = DHCP_HW_ADDR_TYPE_ETH;
		dhcp->hw_addr_len = 6;
		dhcp->transaction_id = dhcp_transaction_id;
		dhcp->client_addr = ip_addr;
		memcpy(dhcp->hw_addr, mac_addr, 6);
		dhcp->magic_cookie = DHCP_MAGIC_COOKIE;

		op = dhcp->options;
		dhcp_add_option(op, DHCP_CODE_MESSAGETYPE, uint8_t, DHCP_MESSAGE_REQUEST);
		dhcp_add_option(op, DHCP_CODE_REQUESTEDADDR, uint32_t, ip_addr);
		dhcp_add_option(op, DHCP_CODE_DHCPSERVER, uint32_t, dhcp_server);
		*(op++) = DHCP_CODE_END;

		if (!udp_send(frame, (uint8_t *)op - (uint8_t *)dhcp)) {
			dhcp_renew_time = rtime() + (uint32_t)5;
			return;
		}

		dhcp_status = DHCP_WAITING_ACK;
	}
}

#endif

/*
 * TCP (ver. 3.0)
 * lots of indian bydlocode here
 *
 * History:
 *	1.0 first attempt
 *	2.0 second attempt, first suitable working variant
 *	2.1 added normal seq/ack management
 *	3.0 added rexmit feature
 */

#ifdef WITH_TCP

// packet sending mode
tcp_sending_mode_t tcp_send_mode;

// "ack sent" flag
uint8_t tcp_ack_sent;

// send TCP packet
// must be set manually:
//	- tcp.flags
uint8_t tcp_xmit(tcp_state_t *st, eth_frame_t *frame, uint16_t len)
{
	uint8_t status = 1;
	uint16_t temp, plen = len;

	ip_packet_t *ip = (void *)(frame->data);
	tcp_packet_t *tcp = (void *)(ip->data);

	if (tcp_send_mode == TCP_SENDING_SEND) {
		// set packet fields
		ip->to_addr = st->remote_addr;
		ip->from_addr = ip_addr;
		ip->protocol = IP_PROTOCOL_TCP;
		tcp->to_port = st->remote_port;
		tcp->from_port = st->local_port;
	}

	if (tcp_send_mode == TCP_SENDING_REPLY) {
		// exchange src/dst ports
		temp = tcp->from_port;
		tcp->from_port = tcp->to_port;
		tcp->to_port = temp;
	}

	if (tcp_send_mode != TCP_SENDING_RESEND) {
		// fill packet header ("static" fields)
		tcp->window = htons(TCP_WINDOW_SIZE);
		tcp->urgent_ptr = 0;
	}

	if (tcp->flags & TCP_FLAG_SYN) {
		// add MSS option (max. segment size)
		tcp->data_offset = (sizeof(tcp_packet_t) + 4) << 2;
		tcp->data[0] = 2; //option: MSS
		tcp->data[1] = 4; //option len
		tcp->data[2] = TCP_SYN_MSS >> 8;
		tcp->data[3] = TCP_SYN_MSS & 0xff;
		plen = 4;
	} else {
		tcp->data_offset = sizeof(tcp_packet_t) << 2;
	}

	// set stream pointers
	tcp->seq_num = htonl(st->seq_num);
	tcp->ack_num = htonl(st->ack_num);

	// set checksum
	plen += sizeof(tcp_packet_t);
	tcp->cksum = 0;
	tcp->cksum = ip_cksum(plen + IP_PROTOCOL_TCP, (uint8_t *)tcp - 8, plen + 8);

	// send packet
	switch (tcp_send_mode) {
	case TCP_SENDING_SEND:
		status = ip_send(frame, plen);
		tcp_send_mode = TCP_SENDING_RESEND;
		break;
	case TCP_SENDING_REPLY:
		ip_reply(frame, plen);
		tcp_send_mode = TCP_SENDING_RESEND;
		break;
	case TCP_SENDING_RESEND:
		ip_resend(frame, plen);
		break;
	}

	// advance sequence number
	st->seq_num += len;
	if ((tcp->flags & TCP_FLAG_SYN) || (tcp->flags & TCP_FLAG_FIN))
		st->seq_num++;

	// set "ACK sent" flag
	if ((tcp->flags & TCP_FLAG_ACK) && (status))
		tcp_ack_sent = 1;

	return status;
}

// sending SYN to peer
// return: 0xff - error, other value - connection id (not established)
uint8_t tcp_open(uint32_t addr, uint16_t port, uint16_t local_port)
{
	eth_frame_t *frame = (void *)net_buf;
	ip_packet_t *ip = (void *)(frame->data);
	tcp_packet_t *tcp = (void *)(ip->data);
	tcp_state_t *st = 0, *pst;
	uint8_t id;
	uint32_t seq_num;

	// search for free conection slot
	for (id = 0; id < TCP_MAX_CONNECTIONS; ++id) {
		pst = tcp_pool + id;

		if (pst->status == TCP_CLOSED) {
			st = pst;
			break;
		}
	}

	// free connection slot found
	if (st) {
		// add new connection
		seq_num = HAL_GetTick() + (HAL_GetTick() << 16);

		st->status = TCP_SYN_SENT;
		st->event_time = HAL_GetTick();
		st->seq_num = seq_num;
		st->ack_num = 0;
		st->remote_addr = addr;
		st->remote_port = port;
		st->local_port = local_port;

#ifdef WITH_TCP_REXMIT
		st->is_closing = 0;
		st->rexmit_count = 0;
		st->seq_num_saved = seq_num;
#endif

		// send packet
		tcp_send_mode = TCP_SENDING_SEND;
		tcp->flags = TCP_FLAG_SYN;
		if (tcp_xmit(st, frame, 0))
			return id;

		st->status = TCP_CLOSED;
	}

	return 0xff;
}

// send TCP data
// dont use somewhere except tcp_write callback!
void tcp_send(uint8_t id, eth_frame_t *frame, uint16_t len, uint8_t options)
{
	ip_packet_t *ip = (void *)(frame->data);
	tcp_packet_t *tcp = (void *)(ip->data);
	tcp_state_t *st = tcp_pool + id;
	uint8_t flags = TCP_FLAG_ACK;

	// check if connection established
	if (st->status != TCP_ESTABLISHED)
		return;

	// send PSH/ACK
	if (options & TCP_OPTION_PUSH)
		flags |= TCP_FLAG_PSH;

	// send FIN/ACK
	if (options & TCP_OPTION_CLOSE) {
		flags |= TCP_FLAG_FIN;
		st->status = TCP_FIN_WAIT;
	}

	// send packet
	tcp->flags = flags;
	tcp_xmit(st, frame, len);
}

// processing tcp packets
void tcp_filter(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void *)(frame->data);
	tcp_packet_t *tcp = (void *)(ip->data);
	tcp_state_t *st = 0, *pst;
	uint8_t id, tcpflags;

	if (ip->to_addr != ip_addr)
		return;

	// tcp data length
	len -= tcp_head_size(tcp);

	// me needs only SYN/FIN/ACK/RST
	tcpflags = tcp->flags & (TCP_FLAG_SYN | TCP_FLAG_ACK | TCP_FLAG_RST | TCP_FLAG_FIN);

	// sending packets back
	tcp_send_mode = TCP_SENDING_REPLY;
	tcp_ack_sent = 0;

	// search connection pool for connection
	//	to specific port from specific host/port
	for (id = 0; id < TCP_MAX_CONNECTIONS; ++id) {
		pst = tcp_pool + id;

		if ((pst->status != TCP_CLOSED) && (ip->from_addr == pst->remote_addr) &&
		    (tcp->from_port == pst->remote_port) && (tcp->to_port == pst->local_port)) {
			st = pst;
			break;
		}
	}

	// connection not found/new connection
	if (!st) {
		// received SYN - initiating new connection
		if (tcpflags == TCP_FLAG_SYN) {
			// search for free slot for connection
			for (id = 0; id < TCP_MAX_CONNECTIONS; ++id) {
				pst = tcp_pool + id;

				if (pst->status == TCP_CLOSED) {
					st = pst;
					break;
				}
			}

			// slot found and app accepts connection?
			if (st && tcp_listen(id, frame)) {
				// add embrionic connection to pool
				st->status = TCP_SYN_RECEIVED;
				st->event_time = HAL_GetTick();
				st->seq_num = HAL_GetTick() + (HAL_GetTick() << 16);
				st->ack_num = ntohl(tcp->seq_num) + 1;
				st->remote_addr = ip->from_addr;
				st->remote_port = tcp->from_port;
				st->local_port = tcp->to_port;

#ifdef WITH_TCP_REXMIT
				st->is_closing = 0;
				st->rexmit_count = 0;
				st->seq_num_saved = st->seq_num;
#endif

				// send SYN/ACK
				tcp->flags = TCP_FLAG_SYN | TCP_FLAG_ACK;
				tcp_xmit(st, frame, 0);
			}
		}
	}

	else {
		// connection reset by peer?
		if (tcpflags & TCP_FLAG_RST) {
			if ((st->status == TCP_ESTABLISHED) || (st->status == TCP_FIN_WAIT)) {
				tcp_closed(id, 1);
			}
			st->status = TCP_CLOSED;
			return;
		}

		// me needs only ack packet
		if ((ntohl(tcp->seq_num) != st->ack_num) || (ntohl(tcp->ack_num) != st->seq_num) ||
		    (!(tcpflags & TCP_FLAG_ACK))) {
			return;
		}

#ifdef WITH_TCP_REXMIT
		// save sequence number
		st->seq_num_saved = st->seq_num;

		// reset rexmit counter
		st->rexmit_count = 0;
#endif

		// update ack pointer
		st->ack_num += len;
		if ((tcpflags & TCP_FLAG_FIN) || (tcpflags & TCP_FLAG_SYN))
			st->ack_num++;

		// reset timeout counter
		st->event_time = HAL_GetTick();

		switch (st->status) {
		// SYN sent by me (active open, step 1)
		// awaiting SYN/ACK (active open, step 2)
		case TCP_SYN_SENT:

			// received packet must be SYN/ACK
			if (tcpflags != (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
				st->status = TCP_CLOSED;
				break;
			}

			// send ACK (active open, step 3)
			tcp->flags = TCP_FLAG_ACK;
			tcp_xmit(st, frame, 0);

			// connection is now established
			st->status = TCP_ESTABLISHED;

			// app can send some data
			tcp_read(id, frame, 0);

			break;

		// SYN received my me (passive open, step 1)
		// SYN/ACK sent by me (passive open, step 2)
		// awaiting ACK (passive open, step 3)
		case TCP_SYN_RECEIVED:

			// received packet must be ACK
			if (tcpflags != TCP_FLAG_ACK) {
				st->status = TCP_CLOSED;
				break;
			}

			// connection is now established
			st->status = TCP_ESTABLISHED;

			// app can send some data
			tcp_read(id, frame, 0);

			break;

		// connection established
		// awaiting ACK or FIN/ACK
		case TCP_ESTABLISHED:

			// received FIN/ACK?
			// (passive close, step 1)
			if (tcpflags == (TCP_FLAG_FIN | TCP_FLAG_ACK)) {
				// feed data to app
				if (len)
					tcp_write(id, frame, len);

				// send FIN/ACK (passive close, step 2)
				tcp->flags = TCP_FLAG_FIN | TCP_FLAG_ACK;
				tcp_xmit(st, frame, 0);

				// connection is now closed
				st->status = TCP_CLOSED;
				tcp_closed(id, 0);
			}

			// received ACK
			else if (tcpflags == TCP_FLAG_ACK) {
				// feed data to app
				if (len)
					tcp_write(id, frame, len);

				// app can send some data
				tcp_read(id, frame, 0);

				// send ACK
				if ((len) && (!tcp_ack_sent)) {
					tcp->flags = TCP_FLAG_ACK;
					tcp_xmit(st, frame, 0);
				}
			}

			break;

		// FIN/ACK sent by me (active close, step 1)
		// awaiting ACK or FIN/ACK
		case TCP_FIN_WAIT:

			// received FIN/ACK?
			// (active close, step 2)
			if (tcpflags == (TCP_FLAG_FIN | TCP_FLAG_ACK)) {
				// feed data to app
				if (len)
					tcp_write(id, frame, len);

				// send ACK (active close, step 3)
				tcp->flags = TCP_FLAG_ACK;
				tcp_xmit(st, frame, 0);

				// connection is now closed
				st->status = TCP_CLOSED;
				tcp_closed(id, 0);
			}

			// received ACK+data?
			// (buffer flushing by peer)
			else if ((tcpflags == TCP_FLAG_ACK) && (len)) {
				// feed data to app
				tcp_write(id, frame, len);

				// send ACK
				tcp->flags = TCP_FLAG_ACK;
				tcp_xmit(st, frame, 0);

#ifdef WITH_TCP_REXMIT
				// our data+FIN/ACK acked
				st->is_closing = 1;
#endif
			}

			break;

		default:
			break;
		}
	}
}

// periodic event
void tcp_poll()
{
#ifdef WITH_TCP_REXMIT
	eth_frame_t *frame = (void *)net_buf;
	ip_packet_t *ip = (void *)(frame->data);
	tcp_packet_t *tcp = (void *)(ip->data);
#endif

	uint8_t id;
	tcp_state_t *st;

	for (id = 0; id < TCP_MAX_CONNECTIONS; ++id) {
		st = tcp_pool + id;

#ifdef WITH_TCP_REXMIT
		// connection timed out?
		if ((st->status != TCP_CLOSED) &&
		    (HAL_GetTick() - st->event_time > TCP_REXMIT_TIMEOUT)) {
			// rexmit limit reached?
			if (st->rexmit_count > TCP_REXMIT_LIMIT) {
				// close connection
				st->status = TCP_CLOSED;
				tcp_closed(id, 1); // callback
			}

			// should rexmit?
			else {
				// reset timeout counter
				st->event_time = HAL_GetTick();

				// increment rexmit counter
				st->rexmit_count++;

				// load previous state
				st->seq_num = st->seq_num_saved;

				// will send packets
				tcp_send_mode = TCP_SENDING_SEND;
				tcp_ack_sent = 0;

				// rexmit
				switch (st->status) {
				// rexmit SYN
				case TCP_SYN_SENT:
					tcp->flags = TCP_FLAG_SYN;
					tcp_xmit(st, frame, 0);
					break;

				// rexmit SYN/ACK
				case TCP_SYN_RECEIVED:
					tcp->flags = TCP_FLAG_SYN | TCP_FLAG_ACK;
					tcp_xmit(st, frame, 0);
					break;

				// rexmit data+FIN/ACK or ACK (in FIN_WAIT state)
				case TCP_FIN_WAIT:

					// data+FIN/ACK acked?
					if (st->is_closing) {
						tcp->flags = TCP_FLAG_ACK;
						tcp_xmit(st, frame, 0);
						break;
					}

					// rexmit data+FIN/ACK
					st->status = TCP_ESTABLISHED;

				// rexmit data
				case TCP_ESTABLISHED:
					tcp_read(id, frame, 1);
					if (!tcp_ack_sent) {
						tcp->flags = TCP_FLAG_ACK;
						tcp_xmit(st, frame, 0);
					}
					break;

				default:
					break;
				}
			}
		}
#else
		// check if connection timed out
		if ((st->status != TCP_CLOSED) &&
		    (HAL_GetTick() - st->event_time > TCP_CONN_TIMEOUT)) {
			// kill connection
			st->status = TCP_CLOSED;
			tcp_closed(id, 1); // callback
		}
#endif
	}
}

#endif

/*
 * UDP
 */

#ifdef WITH_UDP

/**
  * sends an UDP packet
  * @param frame pointer to the full ethernet frame
  * @param uint16_t len is UDP data payload length
  * fields must be set:
  * - ip.dst
  * - udp.src_port
  * - udp.dst_port                                   THREAD - SAFE
  */
uint8_t udp_send(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void *)(frame->data);
	udp_packet_t *udp = (void *)(ip->data);

	len += (uint16_t)sizeof(udp_packet_t);

	ip->protocol = IP_PROTOCOL_UDP;
	ip->from_addr = ip_addr;

	udp->len = htons(len);
	udp->cksum = 0;
	udp->cksum =
		ip_cksum((uint32_t)(len + (uint32_t)IP_PROTOCOL_UDP), (uint8_t *)udp - 8, len + 8);

	return ip_send(frame, len);
}

// reply to UDP packet
// len is UDP data payload length
void udp_reply(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void *)(frame->data);
	udp_packet_t *udp = (void *)(ip->data);
	uint16_t temp;

	len += sizeof(udp_packet_t);

	ip->to_addr = ip_addr;

	temp = udp->from_port;
	udp->from_port = udp->to_port;
	udp->to_port = temp;

	udp->len = htons(len);

	udp->cksum = 0;
	udp->cksum =
		ip_cksum((uint32_t)(len + (uint32_t)IP_PROTOCOL_UDP), (uint8_t *)udp - 8, len + 8);

	ip_reply(frame, len);
}

/**
  * processes an UDP packet
  * @param frame pointer to the full ethernet frame
  * @param len length of thr udp packet
  * @return eth_frame_t* changed pointer to the frame or NULL in pointer isn't changed
  * @note  THREAD - SAFE
  */
static eth_frame_t *udp_filter(eth_frame_t *frame, uint16_t len)
{
	eth_frame_t *retval;
	retval = frame;
	ip_packet_t *ip = (void *)(frame->data);
	udp_packet_t *udp = (void *)(ip->data);

	if (len >= sizeof(udp_packet_t)) {
		len = ntohs(udp->len) - sizeof(udp_packet_t);

		switch (udp->to_port) {
#ifdef WITH_DHCP
		case DHCP_CLIENT_PORT:
			dhcp_filter(frame, len);
			break;
#endif
		default:
			retval = udp_packet_callback(frame, len); /* callback */
			break;
		}
	}
	return retval;
}

#endif /* of #ifdef WITH_UDP */

/*
 * ICMP
 */

#ifdef WITH_ICMP

/**
  * processes the ICMP packet
  * @param frame pointer to the full eth. frame
  * @param len lenght oc ICMP packet
  * @return none
  */
static void icmp_filter(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *packet = (void *)frame->data;
	icmp_echo_packet_t *icmp = (void *)packet->data;

	if (len >= sizeof(icmp_echo_packet_t)) {
		if (icmp->type == (uint8_t)ICMP_TYPE_ECHO_RQ) {
			icmp->type = (uint8_t)ICMP_TYPE_ECHO_RPLY;
			icmp->cksum += 8U; // update cksum
			ip_reply(frame, len);
		}
	}
}
/*   end of icmp_filter() 									*/
#endif

/*
 * IP
 */

/**
 * @brief ip_cksum calculates IP checksum
 * @param sum
 * @param buf
 * @param len
 * @return
 */
static uint16_t ip_cksum(uint32_t sum, uint8_t *buf, uint16_t len)
{
	while (len >= 2U) {
		sum += ((uint16_t)*buf << 8) | *(buf + 1);
		buf += 2;
		len -= 2U;
	}

	if (len != 0) {
		sum += (uint16_t)*buf << 8;
	}
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return (~htons((uint16_t)sum));
}

// send IP packet
// fields must be set:
//	- ip.dst
//	- ip.proto
// len is IP packet payload length
static uint8_t ip_send(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void *)(frame->data);
	uint32_t route_ip;
	uint8_t *mac_addr_to;

	// set frame.dst
	if (ip->to_addr == ip_broadcast) {
		// use broadcast MAC
		memset(frame->to_addr, 0xff, 6);
	} else {
		// apply route
		if (((ip->to_addr ^ ip_addr) & ip_mask) == 0) {
			route_ip = ip->to_addr;
		} else {
			route_ip = ip_gateway;
		}
		/* resolve mac address */
		mac_addr_to = arp_resolve(route_ip); /* it may take time to resolve !*/
		if (mac_addr_to == NULL) {
			return 0; // err!
		}
		memcpy(frame->to_addr, mac_addr_to, 6);
	}

	// set frame.type
	frame->type = (uint16_t)ETH_TYPE_IP;

	// fill IP header
	len += sizeof(ip_packet_t);

	ip->ver_head_len = 0x45;
	ip->tos = 0;
	ip->total_len = htons(len);
	ip->fragment_id = 0;
	ip->flags_framgent_offset = 0;
	ip->ttl = IP_PACKET_TTL;
	ip->cksum = 0;
	ip->from_addr = ip_addr;
	ip->cksum = ip_cksum(0, (void *)ip, sizeof(ip_packet_t));

	// send frame
	eth_send(frame, len);
	return 1; // ok
}

// send IP packet back
// len is IP packet payload length
static void ip_reply(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *packet = (void *)(frame->data);

	len += sizeof(ip_packet_t);

	packet->total_len = htons(len);
	packet->fragment_id = 0;
	packet->flags_framgent_offset = 0;
	packet->ttl = IP_PACKET_TTL;
	packet->cksum = 0;
	packet->to_addr = packet->from_addr;
	packet->from_addr = ip_addr;
	packet->cksum = ip_cksum(0, (void *)packet, sizeof(ip_packet_t));

	eth_reply((void *)frame, len);
}

// can be called directly after
//	ip_send/ip_reply with new data
static void ip_resend(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void *)(frame->data);

	len += sizeof(ip_packet_t);
	ip->total_len = htons(len);
	ip->cksum = 0;
	ip->cksum = ip_cksum(0, (void *)ip, sizeof(ip_packet_t));

	eth_resend(frame, len);
}

/**
  * processes received ip packet
  * @param *frame pointer to the full eth. packet
  * @param len length of the ip part of the packet
  * @return eth_frame_t* changed pointer to the frame or NULL in pointer isn't changed
  */
static eth_frame_t *ip_filter(eth_frame_t *frame, uint16_t len)
{
	uint16_t hcs;
	ip_packet_t *packet = (void *)(frame->data);
	eth_frame_t *retval;
	retval = frame;
	//if(len >= sizeof(ip_packet_t))
	//{
	hcs = packet->cksum;
	packet->cksum = 0;

	if ((packet->ver_head_len == 0x45) &&
	    (ip_cksum(0, (void *)packet, sizeof(ip_packet_t)) == hcs) &&
	    ((packet->to_addr == ip_addr) || (packet->to_addr == ip_broadcast))) {
		len = ntohs(packet->total_len) - sizeof(ip_packet_t);

		switch (packet->protocol) {
#ifdef WITH_ICMP
		case IP_PROTOCOL_ICMP:
			icmp_filter(frame, len);
			break;
#endif

#ifdef WITH_UDP
		case IP_PROTOCOL_UDP:
			retval = udp_filter(frame, len);
			break;
#endif

#ifdef WITH_TCP
		case IP_PROTOCOL_TCP:
			tcp_filter(frame, len);
			break;
#endif
		default:
			ip_filter_misses++;
			break;
		}

	} else { /* bad IP packet */
		;
	}
	return retval;
}

/*
 * ARP
 */
/**
 * @brief arp_get_entry_string returns pointer to the string
 * @param number index of the entry
 * @return pointer to the result string or NULL
 */
char * arp_get_entry_string(size_t number)
{
	static const char retVal_template[] = {"AA:BB:CC:DD:EE:FF AAA.BBB.CCC.DDD 99999\n\0"};
	(void)retVal_template;
	static const size_t t_len = sizeof(retVal_template);
	static char retVal[41];
	char * ptarget = NULL;
	if (number < ARP_CACHE_SIZE) {
		/* convert entry to the string */
		ptarget = &retVal[0];
		for (size_t i = 0U; i < 6U; i++) { /* MAC */
			*ptarget = mybtol((arp_cache[number].mac_addr[i]  >> 4U) & 0x0F);
			ptarget++;
			*ptarget = mybtol(arp_cache[number].mac_addr[i] & 0x0F);
			ptarget++;
			*ptarget = ':';
			ptarget++;
		}
		/* IP addr */
		*(ptarget - 1U) = ' ';
		size_t shift = 0U;
		for (size_t i = 0U; i < 4U; i++) {
			uint8_t b = (uint8_t)(arp_cache[number].ip_addr >> shift);
			uint8_to_asciiz(b, ptarget);
			shift +=8U;
			ptarget += 3U;
			*ptarget = '.';
			ptarget++;
		}
		*(ptarget - 1U) = ' ';
		uint16_to_asciiz((uint16_t)arp_cache[number].age, ptarget);
		*(ptarget + 5) = '\n';
		ptarget = retVal;
	}
	return ptarget;
}

/**
 * @brief arp_clear_cache
 */
static inline void arp_clear_cache(void)
{
	TAKE_MUTEX(ARP_MutexHandle);
	memset(arp_cache, 0 ,sizeof (arp_cache));
	GIVE_MUTEX(ARP_MutexHandle);
}

/** decrements time to live of the arp cache entry every 1s
  * invalidates the outdated entry
  * @param none
  * @return none
  */
void arp_age_entries(void)
{
	uint8_t i;
	TAKE_MUTEX(ARP_MutexHandle);
	for (i = 0; i < ARP_CACHE_SIZE; i++) {
		if (arp_cache[i].age > 0) {
			arp_cache[i].age--;
		} else {
			memset(&arp_cache[i], 0, sizeof(arp_cache_entry_t));
		}
		GIVE_MUTEX(ARP_MutexHandle);
	}
}

/** looks for the cache for MAC by given IP
  * @param uint32_t node_ip_addr the address to be found
  * @return NULL if not found; pointer to the valid MAC if found
  * 									THREAD-SAFE
  */
static uint8_t *arp_search_cache(uint32_t node_ip_addr)
{
	int i;
	uint8_t *result = NULL;
	i = arp_get_cache_index(node_ip_addr);
	if (i != -1) {
		result = arp_cache[i].mac_addr;
	}
	return result;
}

/**
 * @brief arp_get_cache_index returns index of te entry in the cache or -1
 * @param node_ip_addr
 * @return
 */
static int arp_get_cache_index(uint32_t node_ip_addr)
{
	int rv = -1;
	TAKE_MUTEX(ARP_MutexHandle);
	for (int i = 0; i < ARP_CACHE_SIZE; ++i) {
		if (arp_cache[i].ip_addr == node_ip_addr) {
			rv = i;
			break;
		}
	}
	GIVE_MUTEX(ARP_MutexHandle);
	return rv;
}

/**
  * sends ARP request
  * @papram node_ip_addr ip address to be reached
  * @return none
  * This is asynchronous function            THREAD - SAFE
  */
static void request_arp(uint32_t node_ip_addr)
{
	eth_frame_t *frame;
	/*	uint8_t *mac; */
	frame = (eth_frame_t *)lan_getmem(); /* get buffer from the pool */
	if (frame != NULL) {		     /* not enough memory */
		arp_mallocs_frees++;
		arp_message_t *msg = (void *)(frame->data);
		memset(frame->to_addr, 0xff, 6);
		frame->type = ETH_TYPE_ARP;
		msg->hw_type = ARP_HW_TYPE_ETH;
		msg->proto_type = ARP_PROTO_TYPE_IP;
		msg->hw_addr_len = 6;
		msg->proto_addr_len = 4;
		msg->type = ARP_TYPE_REQUEST;
		memcpy(msg->mac_addr_from, mac_addr, 6);
		msg->ip_addr_from = ip_addr;
		memset(msg->mac_addr_to, 0x00, 6);
		msg->ip_addr_to = node_ip_addr;
		eth_send(frame, sizeof(arp_message_t));
		if (lan_freemem((uint8_t *)frame) != NULL) {
			UNUSED(0);
		} else {
			arp_mallocs_frees--;
		}
	}
	return;
}

/**
  * resolves MAC address
  * @param uint32_t node_ip_addr  the IP address requested
  * @return pointer to the MAC address or NULL if not resolver
  * function does not breals any other buffer				THREAD-SAFE
  */
static uint8_t *arp_resolve(uint32_t node_ip_addr)
{
	uint8_t *mac;
	mac = NULL;
	uint8_t i;
	/* search arp cache */
	for (i = 0; i < NUM_ARP_RETRIES; i++) {
		mac = arp_search_cache(node_ip_addr);
		if (mac != NULL) {
			goto fExit;
		} else {
			request_arp(node_ip_addr);
			osDelay(ARP_TIMEOUT); /* let's wait for cache update */
		}
	}
fExit:
	return mac;
}

/**
 * @brief arp_get_oldest_index returns the oldest arp entry index
 * @return
 */
static int arp_get_oldest_index(void)
{
	int retVal = 0;
	int32_t oldest = INT32_MAX;
	uint8_t i;
	for (i = 0; i < ARP_CACHE_SIZE; i++) {
		if (arp_cache[i].age < oldest) {
			oldest = arp_cache[i].age;
			retVal = i;
		}
	}
	return retVal;
}

/**
  * processes received ARP packet
  * @param *frame pointer to the full eth. packet
  * @param len length of the packet
  * @return none
  */							/*  THREAD-SAFE */
static void arp_filter(eth_frame_t *frame, uint16_t len)
{
	arp_message_t *msg = (void *)(frame->data);

	if (len >= sizeof(arp_message_t)) {
		if ((msg->hw_type == ARP_HW_TYPE_ETH) && (msg->proto_type == ARP_PROTO_TYPE_IP) &&
		    (msg->ip_addr_to == ip_addr)) {
			switch (msg->type) {
			case ARP_TYPE_REQUEST: {
				msg->type = ARP_TYPE_RESPONSE;
				memcpy(msg->mac_addr_to, msg->mac_addr_from, 6);
				memcpy(msg->mac_addr_from, mac_addr, 6);
				msg->ip_addr_to = msg->ip_addr_from;
				msg->ip_addr_from = ip_addr;
				eth_reply(frame, sizeof(arp_message_t));
				break;
			}
			case ARP_TYPE_RESPONSE: {
				int idx = 0;
				/* was there this IP already ?*/
				idx = arp_get_cache_index(msg->ip_addr_from);
				if (idx == -1) {
					/* is there free entry ? */
					idx = (uint8_t)arp_get_cache_index(0);
					if (idx == -1) {
						/* no free entry */
						idx = (uint8_t)arp_get_oldest_index();
					}
				}
				TAKE_MUTEX(ARP_MutexHandle);
				arp_cache[idx].ip_addr = msg->ip_addr_from;
				memcpy(arp_cache[idx].mac_addr, msg->mac_addr_from, 6);
				arp_cache[idx].age = (int32_t)ARP_TIMEOUT_S;
				GIVE_MUTEX(ARP_MutexHandle);
				break;
			}
			}
		}
	}
}
/* end of arp_filter */

/*
 * Ethernet
 */

// send new Ethernet frame to same host
//	(can be called directly after eth_send)
static void eth_resend(eth_frame_t *frame, uint16_t len)
{
	enc28j60_send_packet((void *)frame, len + sizeof(eth_frame_t));
}

// send Ethernet frame
// fields must be set:
//	- frame.dst
//	- frame.type
static void eth_send(eth_frame_t *frame, uint16_t len)
{
	memcpy(frame->from_addr, mac_addr, 6);
	enc28j60_send_packet((void *)frame, (uint16_t)(len + (uint16_t)sizeof(eth_frame_t)));
}

// send Ethernet frame back
static void eth_reply(eth_frame_t *frame, uint16_t len)
{
	memcpy(frame->to_addr, frame->from_addr, 6);
	memcpy(frame->from_addr, mac_addr, 6);
	enc28j60_send_packet((void *)frame, (uint16_t)(len + (uint16_t)sizeof(eth_frame_t)));
}

/**
  * @brief processes received ethernet packet
  * @param *frame pointer to the full eth. packet
  * @param len length of the packet
  * @return eth_frame_t* changed pointer to the frame or NULL in pointer isn't changed
  */
static eth_frame_t *eth_filter(eth_frame_t *frame, uint16_t len)
{
	eth_frame_t *retval;
	retval = frame;
	if (len >= sizeof(eth_frame_t)) {
		switch (frame->type) {
		case ETH_TYPE_ARP:
			arp_filter(frame, (uint16_t)(len - (uint16_t)sizeof(eth_frame_t)));

			break;
		case ETH_TYPE_IP:
			retval = ip_filter(frame, len - sizeof(eth_frame_t));
			break;
		default:
			eth_filter_misses++;
			put_dump(frame, (unsigned long)0, (int)len, sizeof(char));

			break;
		}
	} else { /* something wrong with the frame arrived */
		put_dump(frame, (unsigned long)0, (int)len, sizeof(char));
	}
	return retval;
}
/*                  end of eth_filter()								*/

/*
 * LAN
 */
uint8_t *getMAC(void)
{
	return mac_addr;
}

void lan_init() /* NOT THREAD-SAFE */
{
	//	Get_MAC(mac_addr);
	//	ip_pair_t tmp;
	//	Get_IP_Params(&tmp, MY_IP);
	//	ip_addr = tmp.ip;
	//	Get_IP_Params(&tmp, DEFAULT_GW);
	//	Get_IP_Params(&tmp, MY_NETMASK);
	//	ip_mask = tmp.ip;

	uint8_t i;
	for (i = 0u; i < NUM_ETH_BUFFERS; i++) {
		eth_buf_state[i] = ETH_BUF_FREE; /* initialize states array */
	}
	for (i = 0u; i < NUM_SOCKETS; i++) {
		sockets[i].soc_state = SOCK_FREE;
		/*		sockets[i].buf = (uint8_t*)&(eth_buf[i]);	*/ /* link the buffer with the socket */
		sockets[i].buf = NULL;
	}

	osMutexStaticDef(CRC_Mutex, &ARP_Mutex_ControlBlock);
	ARP_MutexHandle = osMutexCreate(osMutex(CRC_Mutex));

	enc28j60_init(mac_addr);
	wr_soc_err = 0U;

#ifdef WITH_DHCP
	dhcp_retry_time = rtime() + (uint32_t)2;
#endif
}

/**
  * receives the ethernet packet
  * lan_poll must be started as separate thread!
  * @param none
  * @return none
  */                                             /* THREAD - SAFE */
void lan_poll()
{
	uint16_t len;
	uint8_t *net_buf;
	eth_frame_t *retval;
	net_buf = lan_getmem();
	retval = (eth_frame_t *)net_buf;
	if (net_buf != NULL) {
		lan_poll_mallocs++;
		len = enc28j60_recv_packet(net_buf, ENC28J60_MAXFRAME);
		if (len == 0u) {
			/* nothing is arrived, free mem*/
			goto fExit;
		}
		retval = eth_filter((eth_frame_t *)net_buf, len);
	} else {
		lan_getmem_errors++;
	}
#ifdef WITH_DHCP
	dhcp_poll();
#endif

#ifdef WITH_TCP
	tcp_poll();
#endif
fExit:
	if (retval != NULL) {
		if ((lan_freemem((uint8_t *)retval)) != NULL) {
			UNUSED(0); /* Memory manager error */
			lan_freemem_errors++;
		} else {
			lan_poll_frees++;
		}
	}
	return;
}
/*   end of lan_poll() */

/**
  * receives the lan state
  * @param none
  * @return 1 if lan is up; 0 otherwise
  */
uint8_t lan_up()
{
	return ((ip_addr != (uint32_t)0) ? (uint8_t)1 : (uint8_t)0);
}
/*   end of lan_up() */

/* function simply returns time in seconds */
uint32_t rtime()
{
	return (HAL_GetTick() / (uint32_t)1000); // Assume that 1 tick = 1ms!!!!!
}
/* end of rtime() */

/* function htonl - host to network byte order */
uint32_t htonl(uint32_t a)
{
	return ((uint32_t)((a >> 24) | (a << 24) | ((a & (uint32_t)0x00ff0000) >> 8) |
			   ((a & (uint32_t)0x0000ff00) << 8)));
}
/* end of function htonl - host to network byte order */

/** reads bytes from network                                        THREAD - SAFE
  * @param  soc pointer to the previously opened socket
  * @param  buf pointer to buffer
  * @param  buflen length of the buffer
  * @param  attempts number of attempts by 5ms
  * @return number of received bytes if OK; NULL if not ok, last_error field contains additional info
  */
static uint16_t read_sock(socket_p soc, uint8_t *buf, int32_t buflen, const uint8_t attempts)
{
	uint16_t result = 0x00U;
	uint8_t maxattempts = attempts;

	if (soc == NULL) {
		goto fExit;
	}						    /* bad sockets */
	if ((soc->mode & SOC_MODE_READ) != SOC_MODE_READ) { /* socket open not for reading */
		soc->last_error = SOC_ERR_WRONG_SOC_MODE;
		goto fExit;
	} else {
		/* wait for the data */
#if (LAN_NOTIFICATION != 1)
		while ((soc->mode & SOC_NEW_DATA) != SOC_NEW_DATA) {
			/** @TODO add wait for semaphore here 		*/
			if ((maxattempts--) == 0U) {
				goto fExit;
			}
			osDelay(5U); /* 5 ms */
		}
#else
		uint32_t timeout = pdMS_TO_TICKS(soc->readTimeOutMS);
		uint32_t notif_val;
		if (soc->TaskToNotify != NULL) {
			if (xTaskNotifyWait(ULONG_MAX, ULONG_MAX, &notif_val, timeout) != pdTRUE) {
				goto fExit;
			}
		} else {
			while ((soc->mode & SOC_NEW_DATA) != SOC_NEW_DATA) {
				/** @TODO add wait for semaphore here 		*/
				if ((maxattempts--) == 0U) {
					goto fExit;
				}
				osDelay(5U); /* 5 ms */
			}
		}
#endif
		/* data arrived here */
		uintptr_t payload;
		uintptr_t start_pos;
		payload = (uintptr_t)soc->len; /* soc->len is a UDP payload size */
		/* if soc->proto != UDP, soc->len means total eth frame size */

		if (soc->proto == (uint8_t)IP_PROTOCOL_UDP) { /* include empty UDP */
			start_pos = (uintptr_t)UDP_PAYLOAD_START;
		} else {
			start_pos = 0U;
		}
		if (payload > (uintptr_t)buflen) {
			soc->last_error =
				SOC_ERR_NOT_ENOUGH_MEM_BUF; /* data received is longer than buffer supplied */
			payload = (uintptr_t)buflen;
		}
		const uint8_t *sr;
		uint8_t *dst;
		sr = (uint8_t *)(soc->buf + start_pos);
		dst = buf;
		result = (uint16_t)payload;
		if (((payload % sizeof(uint32_t)) == 0U) &&
		    (((uintptr_t)sr % sizeof(uint32_t)) == 0U) &&
		    (((uintptr_t)dst % sizeof(uint32_t)) == 0U)) {
			payload /= sizeof(uint32_t);
			uint32_t *d = (uint32_t *)dst;
			uint32_t *ss = (uint32_t *)sr;
			while (payload > 0U) {
				*d = *ss;
				d++;
				ss++;
				payload -= sizeof(uint32_t);
			}
			socfastreads++;
		} else {
			while (payload > 0U) { /* copy data */
				*dst = *sr;
				dst++;
				sr++;
				payload--;
			}
		}
		soc->mode = soc->mode & (~SOC_NEW_DATA); /*clear new data flag */
		/* free soc->buf memory +15-Mar-2018*/
		soc->buf = lan_freemem(soc->buf);
		if (soc->buf != NULL) {
			lan_freemem_errors++;
		} else {
			readsoc_frees++;
		}
	}
fExit:
	if (result != 0u) {
		readsocfits++;
	}
	return result;
}
/* end of the function read_sock */

/** reads bytes from network  without waiting for data     THREAD - SAFE
  * @param  soc pointer to the previously open socket
  * @return number of received bytes if OK; NULL if not ok, last_error field contains additional info
  */
uint16_t read_socket_nowait(socket_p soc, uint8_t *buf, int32_t buflen)
{
	return (read_sock(soc, buf, buflen, 1u));
}

/** reads bytes from network  with waiting for data     THREAD - SAFE
  * @param  soc pointer to the previously open socket
  * @return number of received bytes if OK; NULL if not ok, last_error field contains additional info
  */
uint16_t read_socket(socket_p soc, uint8_t *buf, int32_t buflen)
{
	return (read_sock(soc, buf, buflen, 100u));
}

/**
  * sends data from buf via previously opened socket !!!! UDP ONLY !!!!
  * @param soc the pointer to the socket
  * @param *buf is the pointer to the data
  * @param buflen length of the data
  * @return ErrorStatus SUCCESS or ERROR             THREAD - SAFE
  */
ErrorStatus write_socket(socket_p soc, uint8_t *buf, int32_t buflen)
{
	ErrorStatus result;
	result = ERROR;
	eth_frame_t *frame;

	/*  ETH_MAXFRAME (600 bytes) - UDP_PAYLOAD_START (42)  =  558 bytes for data */
	const int32_t max_payload_len = ((int32_t)ETH_MAXFRAME - (int32_t)UDP_PAYLOAD_START);

	if ((soc != NULL) && (soc->mode == SOC_MODE_WRITE) &&
	    (buflen <= max_payload_len)) { /* socket is OK for writing */
					   /* len is also OK */
		/*+15-Mar-2018 : try to allocate soc->buf memory */
		taskENTER_CRITICAL();
		soc->buf = lan_getmem();
		taskEXIT_CRITICAL();
		if (soc->buf == NULL) {
			lan_getmem_errors++;
			goto fExit; /* malloc error */
/**/		}
wr_mallocs_frees++;
frame = (eth_frame_t *)(soc->buf);
ip_packet_t *ip = (ip_packet_t *)(frame->data);
udp_packet_t *udp = (udp_packet_t *)(ip->data);
uint8_t *udp_data_p = (udp->data);

memcpy(udp_data_p, buf, (size_t)buflen); /* copy the payload */

ip->to_addr = soc->rem_ip_addr;
udp->from_port = htons(soc->loc_port);
udp->to_port = htons(soc->rem_port);
soc->len = (uint16_t)buflen;

result = (udp_send(frame, (uint16_t)buflen) == 1u) ? SUCCESS : ERROR;
/*+15-Mar-2018 : free soc->buf memory */
taskENTER_CRITICAL();
soc->buf = lan_freemem(soc->buf);
taskEXIT_CRITICAL();
if (soc->buf != NULL) {
	lan_freemem_errors++;
} else {
	wr_mallocs_frees--;
}
	}
fExit:
	if (result == ERROR) {
		wr_soc_err++;
		if (wr_soc_err > 3) {
			while (1) {
				/**/
			}
		} else {
		/* try to reset ARP cache */
			arp_clear_cache(); // + 05-Oct-2019
		}
	}
	return result;
}
/* end of the function write_socket */

/**
  * callback function processes received UDP frame
  * @param frame - pointer to the whole ethernet frame
  * @param len - length of the udp payload of the frame
  * @return eth_frame_t* changed pointer to the frame or NULL in pointer isn't changed
  */
static eth_frame_t *udp_packet_callback(eth_frame_t *frame, uint16_t len)
{
	eth_frame_t *retval;
	retval = frame;

	/* add 06-Sep-2019 */
	bool needNotify = false;

	if (frame == NULL) {
		return frame; /* function is safe against null pointers (11-Mar-2018) */
	}
	uint8_t i;
	ip_packet_t *ip = (void *)(frame->data);
	udp_packet_t *udp = (udp_packet_t *)(ip->data);
	taskENTER_CRITICAL();
	for (i = 0u; i < NUM_SOCKETS; i++) {
		if ((sockets[i].rem_ip_addr == ip->from_addr) &&
		    (sockets[i].loc_ip_addr == ip->to_addr) &&
		    /*		 (sockets[i].mode == SOC_MODE_READ) && */
		    ((sockets[i].mode & SOC_MODE_READ) != 0U) && /* + 15-Mar-2018  */
		    (sockets[i].proto == (uint8_t)IP_PROTOCOL_UDP) &&
		    (sockets[i].loc_port == ntohs(udp->to_port))) {
			/* receive from ANY port added 11-03-2018 */
			sockets[i].rem_port = (sockets[i].rem_port == 0U) ? ntohs(udp->from_port) :
									    sockets[i].rem_port;

			if (sockets[i].rem_port == ntohs(udp->from_port)) {
				/* proceed with payload  */
				sockets[i].len = len;
				if ((sockets[i].mode & SOC_NEW_DATA) != 0U) {
					/* previous new data is not read, will be overwritten !*/
					sockets[i].datalost = SOC_DATA_LOST; /* + 15-Mar-2018  */
					socdatalosts++;			     /* + 15-Mar-2018  */
				}
				sockets[i].mode |= SOC_NEW_DATA; /* set flag */
				/* zero-copy implementation 13-Mar-2018 */
				/* swap sockets[i].buf and frame */
				retval = (eth_frame_t *)sockets[i].buf;
				sockets[i].buf = (uint8_t *)frame;
				readsoc_mallocs++;
				udp_fits_callbacks++;
				needNotify = true;
				break; /* only first fit socket has new data */
			}
		} /* end of the "5 conditions" if */
	}	 /* end of for i loop */
	taskEXIT_CRITICAL();
	/* notify task here */
	/* notification added 06-Sep-2019 */
	if ((needNotify) && (sockets[i].TaskToNotify != NULL)) {
		xTaskNotify(sockets[i].TaskToNotify, /* the task which is waiting for the data */
			    (uint32_t)(&sockets[i]), /* pointer for fast socket access */
			    eSetValueWithOverwrite);
	}
	udp_callbacks++;
	return retval;
}

/** runs out of freertos!  every 1ms by TIM0 stimulus
  * returns num of free netbuffers
  * @param none
  * @return nb number of free buffers
  */
void nb_stat(void)
{
#ifndef NET_BUF_STAT
	uint8_t i;
	uint32_t tmp = 0u;
	for (i = 0u; i < NUM_ETH_BUFFERS; i++) {
		tmp = (eth_buf_state[i] == ETH_BUF_FREE) ? (tmp + 1u) : tmp;
	}
	freenb = tmp;
	minfreenb = (tmp < minfreenb) ? tmp : minfreenb;
#endif
}
/**/

// TCP callbacks
static uint8_t tcp_listen(uint8_t id, eth_frame_t *frame)
{
	UNUSED(frame);
	return id;
}

static void tcp_read(uint8_t id, eth_frame_t *frame, uint8_t re)
{
	UNUSED(frame);
	UNUSED(re);
	UNUSED(id);
}

static void tcp_write(uint8_t id, eth_frame_t *frame, uint16_t len)
{
	UNUSED(frame);
	UNUSED(len);
	UNUSED(id);
}

static void tcp_closed(uint8_t id, uint8_t hard)
{
	UNUSED(id);
	UNUSED(hard);
}

/*###################################### EOF #####################################################*/
