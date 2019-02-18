/**
 * @file nvmem.h
 * @author Vasiliy Turchenko
 * @date 18-Feb-2017
 * @version 0.0.1
 *
 */
 
#ifndef _NVMEM_H
#define _NVMEM_H

#include "stm32f1xx.h"


#define	_25LC64
#define	NVMEM_SIZE	8192U


#define	FRAM_WREN	0x06U		/*!< WREN opcode */
#define	FRAM_WRDI	0x04U		/*!< WRDI opcode */
#define	FRAM_RDSR	0x05U		/*!< RDSR opcode */
#define	FRAM_WRSR	0x01U		/*!< WRSR opcode */
#define	FRAM_READ	0x03U		/*!< READ opcode */
#define	FRAM_WRITE	0x02U		/*!< WRITE opcode */

/* Status register bits */

#define	SR_WPEN		0x80U
#define SR_BP1		0x08U
#define SR_BP0		0x04U


ErrorStatus Read_FRAM(uint8_t * addr_to, uint32_t fram_addr, size_t frlen);

ErrorStatus Write_FRAM(uint8_t * addr_from, uint32_t fram_addr, size_t frlen);

void Test_FRAM(void);


#endif
