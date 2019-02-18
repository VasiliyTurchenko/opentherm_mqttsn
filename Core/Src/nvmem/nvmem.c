/**
 * @file nvmem.c
 * @author Vasiliy Turchenko
 * @date 18-Feb-2017
 * @version 0.0.1
 *
 */

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include <string.h>

#include "nvmem.h"
#include "spi.h"
#include "xprintf.h"
#include "debug_settings.h"


#define	FRAM_SPI	hspi2

#define	FRAM_CS_L	{ FRAM_CS_GPIO_Port->BSRR = (uint32_t)FRAM_CS_Pin << 16U; }
#define	FRAM_CS_H	{ FRAM_CS_GPIO_Port->BSRR = FRAM_CS_Pin; }

extern	uint8_t		RX_ready_flag;
extern	uint8_t		TX_done_flag;

extern osMutexId ETH_Mutex01Handle;


/**
  * @brief  Read_FRAM reads bytes from non-volatile FRAM memory
  * @note
  * @param  addr_to is a pointer to the RAM buffer
  * @param  fram_addr is address of data in the FRAM chip
  * @param  frlen is length of the data to be read
  * @note   if (fram_addr+frlen) >= chip memmory size, the function returns ERROR
  * @retval ERROR or SUCCESS
  */
ErrorStatus Read_FRAM(uint8_t * addr_to, uint32_t fram_addr, size_t frlen)
{
	ErrorStatus	result;
	result = ERROR;
	uint8_t		t_tmp[3];
	BaseType_t	mut;
	
	if (frlen == 0U) { goto fExit; }
	if ((fram_addr + frlen) > NVMEM_SIZE) { goto fExit; }
/* Decide if rtos has already started */	
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
/* Take MUTEX */
		mut = xSemaphoreTake(ETH_Mutex01Handle, portMAX_DELAY);
	}
	t_tmp[0] = FRAM_READ;
	t_tmp[1] = (uint8_t)(fram_addr >> 8U);
	t_tmp[2] = (uint8_t)(fram_addr);
	FRAM_CS_L;
	/* 1. spi_out FRAM_READ command*/
	HAL_StatusTypeDef	IOresult;
	RX_ready_flag = 0U;
	if (HAL_SPI_Transmit(&FRAM_SPI,\
				    (uint8_t*)t_tmp, \
				    0x03U, \
				    0x03U) == HAL_OK) {
		/* read data */
		if (frlen > sizeof(uint16_t)) {
			IOresult = HAL_SPI_Receive_DMA(&FRAM_SPI, addr_to, (uint16_t)frlen);
			while (RX_ready_flag == 0U) {
				if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
					taskYIELD();
				} else {
					UNUSED(0);
				}
			}
		} else {
			IOresult = HAL_SPI_Receive(&FRAM_SPI, addr_to, (uint16_t)frlen, 0x03U);
		}
		if (IOresult == HAL_OK) {result = SUCCESS; }
	}
	FRAM_CS_H;
/* Give MUTEX */
/* Decide if rtos has already started */	
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		mut = xSemaphoreGive(ETH_Mutex01Handle);
	}
	(void)mut;
fExit:	
	return result;
}

/**
  * @brief  Write_FRAM writes bytes to non-volatile FRAM memory
  * @note
  * @param  addr_from is a pointer to the RAM buffer
  * @param  fram_addr is a destination address in the FRAM chip
  * @param  frlen is length of the data to be written
  * @note   if (fram_addr+frlen) >= chip memmory size, the function returns ERROR
  * @retval ERROR or SUCCESS
  */

ErrorStatus Write_FRAM(uint8_t * addr_from, uint32_t fram_addr, size_t frlen)
{
	ErrorStatus	result;
	result = ERROR;
	uint8_t		t_tmp[3];
	BaseType_t	mut;	
	
	if (frlen == 0U) { goto fExit; }
	if ((fram_addr + frlen) > NVMEM_SIZE) { goto fExit; }
/* Decide if rtos has already started */	
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
/* Take MUTEX */
		mut = xSemaphoreTake(ETH_Mutex01Handle, portMAX_DELAY);
	}
	HAL_StatusTypeDef	IOresult;		
	/* 1. WREN out */
	TX_done_flag = 0U;
	t_tmp[0] = FRAM_WREN;		
	FRAM_CS_L;
	if (HAL_SPI_Transmit(&FRAM_SPI,\
			    (uint8_t*)t_tmp, \
			    0x01U, \
			    0x03U) != HAL_OK) { goto fGiveAndExit; }
	FRAM_CS_H;
	FRAM_CS_H;
	FRAM_CS_H;
	/* 2. Clear any write protection */
	t_tmp[0] = FRAM_WRSR;
	t_tmp[1] = 0x00U;	/* WPEN = 0, WP1 = 0, WP2 = 0 */
	FRAM_CS_L;
	if (HAL_SPI_Transmit(&FRAM_SPI,\
			    (uint8_t*)t_tmp, \
			    0x02U, \
			0x03U) != HAL_OK) { goto fGiveAndExit; }
	FRAM_CS_H;
	FRAM_CS_H;
	FRAM_CS_H;
	/* 3. WREN out again */
	t_tmp[0] = FRAM_WREN;
	FRAM_CS_L;
	if (HAL_SPI_Transmit(&FRAM_SPI,\
			    (uint8_t*)t_tmp, \
			    0x01U, \
			    0x03U) != HAL_OK) { goto fGiveAndExit; }
	FRAM_CS_H;
	FRAM_CS_H;
	FRAM_CS_H;
	/* 3. Write data */

	t_tmp[0] = FRAM_WRITE;
	t_tmp[1] = (uint8_t)(fram_addr >> 8U);
	t_tmp[2] = (uint8_t)(fram_addr);
	FRAM_CS_L;
	if (HAL_SPI_Transmit(&FRAM_SPI,\
			    (uint8_t*)t_tmp, \
			    0x03U, \
			    0x03U) != HAL_OK) { goto fGiveAndExit; }
		
	if (frlen > sizeof(uint16_t)) {
		IOresult = HAL_SPI_Transmit_DMA(&FRAM_SPI, addr_from, (uint16_t)frlen);
		while (TX_done_flag == 0U) {
			if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
				taskYIELD();
			} else {
				UNUSED(0);
			}
		}
	} else {
		IOresult = HAL_SPI_Transmit(&FRAM_SPI, addr_from, (uint16_t)frlen, 0x03U);
	}
	FRAM_CS_H;
	FRAM_CS_H;
	FRAM_CS_H;
	if (IOresult == HAL_OK) {result = SUCCESS; }
	/* 4. Set write protection */
	t_tmp[0] = FRAM_WRDI;
	FRAM_CS_L;
	if (HAL_SPI_Transmit(&FRAM_SPI,\
			    (uint8_t*)t_tmp, \
			    0x01U, \
			    0x03U) != HAL_OK) { result = ERROR; }
fGiveAndExit:
	FRAM_CS_H;
/* Decide if rtos has already started */	
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		xSemaphoreGive(ETH_Mutex01Handle);
	}
	(void)mut;
fExit:
	return result;
}

/**
  * @brief  Test_FRAM
  * @note  
  * @param  none
  * @retval none
  */
 
#if (0)  
void Test_FRAM(void)
{
	const	uint16_t	block_sizes[] = {0U, 1U, 2U, 4U, 8U, 16U, 256U, 512U};
	uint16_t	block_size = 0;
	uint32_t	start_addr = 0;

// #define		_BYTE_TEST

#ifdef		_BYTE_TEST	
	static volatile uint8_t		buffr;
	static volatile uint8_t		buffw;
#else	/* dword test */
	static volatile uint32_t		buffr[16];
	static volatile uint32_t		buffw[16];
#endif	
	uint32_t	curr_addr;

	uint32_t		i;
	ErrorStatus	res;

	xprintf("\n test loop started\n");
	
	i = 0U;
	while (i < NVMEM_SIZE) {
#ifdef		_BYTE_TEST	        
		buffw = (uint8_t)i;
#else
		buffw[0] = i;
#endif
		res = Write_FRAM((uint8_t*)&buffw, i, sizeof(buffw));
		if (res != SUCCESS) {
			xprintf("buff write ERR at addr %d\n", i);
		}
		res = Read_FRAM((uint8_t*)&buffr, i, sizeof(buffr));
		if (res != SUCCESS) {
			xprintf("buff read ERR ar addr %d\n", i); 
		}
		if (memcmp( (void*)buffw, (void*)buffr, sizeof(buffr)) != 0) {
#ifdef		_BYTE_TEST	        		
		xprintf("addr: %d, wr: %d, rd: %d\n", i, buffw, buffr);
#else
		xprintf("uint32_t[16] cmp err at addr: %d\n", i);
#endif
		}
		i += sizeof(buffw);
	}
}

#endif
/* ################################## EOF ################################ */


