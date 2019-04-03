/** @file manchester_task.h
 *  @brief manchester RX-TX routines
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-03-2019
 */

#ifndef MANCHESTER_TASK_H
#define MANCHESTER_TASK_H


#define MANCHESTER_RECEIVE_NOTIFY	((uint32_t)1)
#define MANCHESTER_TRANSMIT_NOTIFY	((uint32_t)2)


extern uint8_t Rx_buf[4];
extern uint8_t Tx_buf[4];


void manchester_task_init(void);
void manchester_task_run(void);


#endif // MANCHESTER_TASK_H
