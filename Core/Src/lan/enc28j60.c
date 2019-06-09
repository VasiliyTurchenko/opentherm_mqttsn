/* Header:  ENC28J60 ethernet stack
* File Name: enc28j60.c
* Author:  Livelover from www.easyelectronics.ru
* Modified for STM32 by turchenkov@gmail.com
* Date: 03-Jan-2017
*/
#if !defined  (USE_HAL_DRIVER)
	#error "!!USE_HAL_DRIVER!!"
#endif


#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#if defined (STM32F103xB)
#include "stm32f1xx.h"
#elif (STM32F303xC)
#include "stm32f3xx.h"
#else
	#error "MCU NOT DEFINED!"
#endif

#include "enc28j60.h"

#include "spi.h"

/*
 * SPI
 */
#ifndef USE_HAL_DMA_SPI
	#ifndef USE_HAL_IO_SPI
		#error	"Error! SPI I/O method isn't selected! (0 out of 2)\n"
	#endif
#endif
#ifdef USE_HAL_DMA_SPI
	#ifdef USE_HAL_IO_SPI
		#error	"Error! SPI I/O method isn't selected! (2 out of 2)\n"
	#endif
#endif




#define		ENC28J60_CS_L	{ ENC28J60_CS_GPIO_Port->BSRR = (uint32_t)ENC28J60_CS_Pin << 16U; }
#define		ENC28J60_CS_H	{ ENC28J60_CS_GPIO_Port->BSRR = ENC28J60_CS_Pin; }

#define		ENC28J60_RST_L	{ ENC28J60_RESET_GPIO_Port->BSRR = (uint32_t)ENC28J60_RESET_Pin << 16U;}
#define		ENC28J60_RST_H	{ ENC28J60_RESET_GPIO_Port->BSRR = ENC28J60_RESET_Pin; }

/* set by SPI routines */
/* shared by all functions using spi1*/
extern	uint8_t		RX_ready_flag;
extern	uint8_t		TX_done_flag;

extern osMutexId ETH_Mutex01Handle;

static uint8_t enc28j60_current_bank = 0;
static uint16_t enc28j60_rxrdpt = 0;		// stored value of the read ptr

//
extern uint8_t * mac_to_enc(void);
//

volatile	uint32_t	bad_eth_frames_cnt = 0;
volatile	uint32_t	enc_hw_err_cnt = 0;

/**
  * @brief  enc28j60_rxtx is a basic function for reading and writing ENC28J60 registers
  * @note
  * @param  a data byte to be sent
  * @retval a read data byte
  */
uint8_t enc28j60_rxtx(uint8_t data)
{
static	HAL_StatusTypeDef	IOresult;
	uint8_t		retdata;
	retdata = 0u;
//	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
//		taskENTER_CRITICAL();
//	}
//	ENC28J60_CS_L;
//	ENC28J60_CS_L;
//	ENC28J60_CS_L;
	IOresult = HAL_SPI_TransmitReceive(&hspi2, &data, &retdata, (uint16_t)0x01, (uint32_t)0x05);
//	ENC28J60_CS_H;
//	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
//		taskEXIT_CRITICAL();
//	}
	return retdata;
}

#define enc28j60_rx() enc28j60_rxtx((uint8_t)0xff)
#define enc28j60_tx(data) enc28j60_rxtx(data)

// Generic SPI read command
uint8_t enc28j60_read_op(uint8_t cmd, uint8_t adr)
{
	uint8_t data;

	ENC28J60_CS_L;
	enc28j60_tx(cmd | (adr & ENC28J60_ADDR_MASK));
	if(adr & 0x80) // throw out dummy byte
		enc28j60_rx(); // when reading MII/MAC register
	data = enc28j60_rx();
	ENC28J60_CS_H;
	return data;
}

// Generic SPI write command
void enc28j60_write_op(uint8_t cmd, uint8_t adr, uint8_t data)
{
	ENC28J60_CS_L;
	enc28j60_tx(cmd | (adr & ENC28J60_ADDR_MASK));
	enc28j60_tx(data);
	ENC28J60_CS_H;
}


/**
  * @brief  enc28j60_soft_reset performs a software reset of the chip
  * @note  must not run after FreeRTOS sheduler started
  * @param  none
  * @retval none
  */
void enc28j60_soft_reset(void)
{
	ENC28J60_CS_L;
	enc28j60_tx(ENC28J60_SPI_SC);
	ENC28J60_CS_H;

	enc28j60_current_bank = 0;

//	_delay_ms(1); // Wait until device initializes
	HAL_Delay(10);
}


/*
 * Memory access
 */

// Set register bank
void enc28j60_set_bank(uint8_t adr)
{
	uint8_t bank;

	if( (adr & ENC28J60_ADDR_MASK) < ENC28J60_COMMON_CR ) {
		bank = (adr >> 5) & 0x03; //BSEL1|BSEL0=0x03
		if(bank != enc28j60_current_bank) {
			enc28j60_write_op(ENC28J60_SPI_BFC, ECON1, 0x03);
			enc28j60_write_op(ENC28J60_SPI_BFS, ECON1, bank);
			enc28j60_current_bank = bank;
		}
	}
}

// Read register
uint8_t enc28j60_rcr(uint8_t adr)
{
	enc28j60_set_bank(adr);
	return enc28j60_read_op(ENC28J60_SPI_RCR, adr);
}

// Read register pair
uint16_t enc28j60_rcr16(uint8_t adr)
{
	enc28j60_set_bank(adr);
	return enc28j60_read_op(ENC28J60_SPI_RCR, adr) |
		(enc28j60_read_op(ENC28J60_SPI_RCR, adr+1) << 8);
}

// Write register
void enc28j60_wcr(uint8_t adr, uint8_t arg)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_WCR, adr, arg);
}

// Write register pair
void enc28j60_wcr16(uint8_t adr, uint16_t arg)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_WCR, adr, arg);
	enc28j60_write_op(ENC28J60_SPI_WCR, adr+1, arg>>8);
}

// Clear bits in register (reg &= ~mask)
void enc28j60_bfc(uint8_t adr, uint8_t mask)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_BFC, adr, mask);
}

// Set bits in register (reg |= mask)
void enc28j60_bfs(uint8_t adr, uint8_t mask)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_BFS, adr, mask);
}

// Read Rx/Tx buffer (at ERDPT)
void enc28j60_read_buffer(uint8_t *buf, uint16_t len)
{
	ENC28J60_CS_L;
	enc28j60_tx(ENC28J60_SPI_RBM);

#ifdef	USE_HAL_IO_SPI
	while(len--) {
		*(buf++) = enc28j60_rx();
	}

#endif
#ifdef	USE_HAL_DMA_SPI
	HAL_StatusTypeDef	IOresult;
	RX_ready_flag = 0;

	if (len > sizeof(uint16_t)) {
		IOresult = HAL_SPI_Receive_DMA(&hspi2, buf, len);
		while (RX_ready_flag == 0) { taskYIELD();}

	} else {
		IOresult = HAL_SPI_Receive(&hspi2, buf, len, 05);
	}
#endif

	ENC28J60_CS_H;
}

// Write Rx/Tx buffer (at EWRPT)
void enc28j60_write_buffer(uint8_t *buf, uint16_t len)
{
	ENC28J60_CS_L;
	enc28j60_tx(ENC28J60_SPI_WBM);

#ifdef	USE_HAL_IO_SPI
	while(len--)  {
		enc28j60_tx(*(buf++));
	}
#endif
#ifdef	USE_HAL_DMA_SPI
	HAL_StatusTypeDef	IOresult;
	TX_done_flag = 0;

	if (len > sizeof(uint16_t)) {
		IOresult = HAL_SPI_Transmit_DMA(&hspi2, buf, len);
		while (TX_done_flag == 0) { taskYIELD();}
	} else {
		IOresult = HAL_SPI_Transmit(&hspi2, buf, len, 05);
	}
#endif
	ENC28J60_CS_H;
}

// Read PHY register
uint16_t enc28j60_read_phy(uint8_t adr)
{
	enc28j60_wcr(MIREGADR, adr);
	enc28j60_bfs(MICMD, MICMD_MIIRD);
	while(enc28j60_rcr(MISTAT) & MISTAT_BUSY)
		;
	enc28j60_bfc(MICMD, MICMD_MIIRD);
	return enc28j60_rcr16(MIRD);
}

// Write PHY register
void enc28j60_write_phy(uint8_t adr, uint16_t data)
{
	enc28j60_wcr(MIREGADR, adr);
	enc28j60_wcr16(MIWR, data);
	while(enc28j60_rcr(MISTAT) & MISTAT_BUSY)
		;
}


/*
 * Init & packet Rx/Tx
 */

void enc28j60_init(const uint8_t *macadr)
{
	// Initialize SPI
	// Already done by HAL MSP init
	// Reset ENC28J60

	ENC28J60_RST_L;
	HAL_Delay(10);
	ENC28J60_RST_H;

	enc28j60_soft_reset();

	uint8_t		s = 0U;
	do {
		s = enc28j60_rcr(ESTAT);
	} while ((s & (uint8_t)ESTAT_CLKRDY) == 0u);

	// Setup Rx/Tx buffer
	enc28j60_wcr16(ERXST, ENC28J60_RXSTART);
	enc28j60_wcr16(ERXRDPT, ENC28J60_RXSTART);
	enc28j60_wcr16(ERXND, ENC28J60_RXEND);
	enc28j60_rxrdpt = ENC28J60_RXSTART;

	// Setup MAC
	enc28j60_wcr(MACON1, MACON1_TXPAUS| // Enable flow control
		MACON1_RXPAUS|MACON1_MARXEN); // Enable MAC Rx
	enc28j60_wcr(MACON2, 0); // Clear reset
	enc28j60_wcr(MACON3, MACON3_PADCFG0| // Enable padding,
		MACON3_TXCRCEN|MACON3_FRMLNEN|MACON3_FULDPX); // Enable crc & frame len chk
	enc28j60_wcr16(MAMXFL, ENC28J60_MAXFRAME);
	enc28j60_wcr(MABBIPG, 0x15); // Set inter-frame gap
	enc28j60_wcr(MAIPGL, 0x12);
	enc28j60_wcr(MAIPGH, 0x0c);
	enc28j60_wcr(MAADR5, macadr[0]); // Set MAC address
	enc28j60_wcr(MAADR4, macadr[1]);
	enc28j60_wcr(MAADR3, macadr[2]);
	enc28j60_wcr(MAADR2, macadr[3]);
	enc28j60_wcr(MAADR1, macadr[4]);
	enc28j60_wcr(MAADR0, macadr[5]);

	// Setup PHY
	enc28j60_write_phy(PHCON1, PHCON1_PDPXMD); // Force full-duplex mode
	enc28j60_write_phy(PHCON2, PHCON2_HDLDIS); // Disable loopback
	enc28j60_write_phy(PHLCON, PHLCON_LACFG2| // Configure LED ctrl
		PHLCON_LBCFG2|PHLCON_LBCFG1|PHLCON_LBCFG0|
		PHLCON_LFRQ0|PHLCON_STRCH);

	// Enable Rx packets
	enc28j60_bfs(ECON1, ECON1_RXEN);
}

void enc28j60_send_packet(uint8_t *data, uint16_t len)
{
/* Take MUTEX */
	if (xSemaphoreTake(ETH_Mutex01Handle, portMAX_DELAY) == pdTRUE) {
//		while(enc28j60_rcr(ECON1) & ECON1_TXRTS)      // wait while tx logic is busy
//		{
			// TXRTS may not clear - ENC28J60 bug. We must reset
			// transmit logic in cause of Tx error
//			if(enc28j60_rcr(EIR) & EIR_TXERIF)
//			{
//				enc28j60_bfs(ECON1, ECON1_TXRST);
//				enc28j60_bfc(ECON1, ECON1_TXRST);
//			}
//		}
		uint8_t		s;
		s = enc28j60_rcr(ESTAT);
		if ((s & (uint8_t)ESTAT_CLKRDY) == 0u) {
/*hardware error !*/
		enc28j60_init(mac_to_enc());
		enc_hw_err_cnt++;
			enc28j60_bfs((uint8_t)ECON1, (uint8_t)ECON1_TXRST);
			enc28j60_bfc((uint8_t)ECON1, (uint8_t)ECON1_TXRST);
			enc28j60_bfc((uint8_t)EIR, (uint8_t)EIR_TXERIF);
		}
		uint8_t		a;
		a = enc28j60_rcr(EIR);
		if ( (a & (uint8_t)EIR_TXERIF) == (uint8_t)EIR_TXERIF ) {
			enc28j60_bfs((uint8_t)ECON1, (uint8_t)ECON1_TXRST);
			enc28j60_bfc((uint8_t)ECON1, (uint8_t)ECON1_TXRST);
			enc28j60_bfc((uint8_t)EIR, (uint8_t)EIR_TXERIF);
		}

		enc28j60_wcr16(EWRPT, ENC28J60_TXSTART);
		enc28j60_write_buffer((uint8_t*)"\x00", 1);
		enc28j60_write_buffer(data, len);

		enc28j60_wcr16(ETXST, ENC28J60_TXSTART);
		enc28j60_wcr16(ETXND, ENC28J60_TXSTART + len);

		enc28j60_bfs(ECON1, ECON1_TXRTS); // Request packet send
/* Give MUTEX */
		xSemaphoreGive(ETH_Mutex01Handle);
	}
}

uint16_t enc28j60_recv_packet(uint8_t *buf, uint16_t buflen)
{
	uint16_t len = 0, rxlen, status, temp;
/* Take MUTEX */
	if (xSemaphoreTake(ETH_Mutex01Handle, portMAX_DELAY) == pdTRUE) {

		if(enc28j60_rcr(EPKTCNT) != 0)
		{
			enc28j60_wcr16(ERDPT, enc28j60_rxrdpt);      // ERDPT - read ptr, 16 bit

			enc28j60_read_buffer((void*)&enc28j60_rxrdpt, sizeof(enc28j60_rxrdpt));
						// 1st read - next packet ptr  16bit
			enc28j60_read_buffer((void*)&rxlen, sizeof(rxlen));
						// 2nd read - length of eth packet
			enc28j60_read_buffer((void*)&status, sizeof(status));
						// 3rd read - status flags. if 16th bit is set - RX OK!
			if(status & 0x80) //success
			{
				if ((rxlen >= MIN_ETH_FRAME_SIZE) && (rxlen <= (buflen-4))) {
					len = rxlen - 4; //throw out crc
					enc28j60_read_buffer(buf, len);
				} else {
					bad_eth_frames_cnt++;
				}
			}
			// Set Rx read pointer to next packet
			temp = (enc28j60_rxrdpt - 1) & ENC28J60_BUFEND;
			enc28j60_wcr16(ERXRDPT, temp);

			// Decrement packet counter
			enc28j60_bfs(ECON2, ECON2_PKTDEC);
		}
/* Give MUTEX */
		xSemaphoreGive(ETH_Mutex01Handle);
	}
	return len;
}

/*########################### EOF ################################################################*/

