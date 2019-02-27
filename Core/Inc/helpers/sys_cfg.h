/**
 ******************************************************************************
 * @file    sys_cfg.h
 * @author  turchenkov@gmail.com
 * @date    22-Jan-2019
 * @brief   system configuration defs
 ******************************************************************************
 * @attention use at your own risk
 ******************************************************************************
 */
#ifndef SYS_CFG_H
#define SYS_CFG_H

#include <limits.h>
#include <stdint.h>

/**
 * @brief	USART config
 */
#define USART_SPEED 115200U /* baudrate */

/*
                        Packet size
                        
USART Speed
                64	128	256	512	1024	2048
115200	11520	5,6	11,1	22,2	44,4	88,9	177,8
57600	5760	11,1	22,2	44,4	88,9	177,8	355,6
38400	3840	16,7	33,3	66,7	133,3	266,7	533,3
19200	1920	33,3	66,7	133,3	266,7	533,3	1066,7
14400	1440	44,4	88,9	177,8	355,6	711,1	1422,2
9600	960	66,7	133,3	266,7	533,3	1066,7	2133,3
4800	480	133,3	266,7	533,3	1066,7	2133,3	4266,7
2400	240	266,7	533,3	1066,7	2133,3	4266,7	8533,3
1200	120	533,3	1066,7	2133,3	4266,7	8533,3	17066,7
*/

/* buffer size in bytes (there's two of them)*/
#define USART_TX_BUFSIZE 512U

/* time to transmit the full buffer */
#define BUF_TX_TIME                                                            \
  ((uint32_t)(1U / (1000U * ((USART_SPEED / 10U) / USART_TX_BUFSIZE))))
#define COMM_TASK_PERIOD (BUF_TX_TIME + 1U)

#endif
/* ################### E.O.F. ############################################### */
