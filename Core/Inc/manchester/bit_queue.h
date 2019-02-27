/**
 ** This file is part of the stm32f3disco project.
 ** Copyright 2018 Vasiliy Turchenko <turchenkov@gmail.com>.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef BIT_QUEUE_H
#define BIT_QUEUE_H

#include <stdint.h>
#include <stddef.h>

typedef struct BitQueue {
	uint32_t queue; /* holds up to 32 bits */
	size_t pFirst;  /* points at the first bit , 0 - the queue is empty */
} BitQueue_t;

typedef enum BitQueueStatus { bqOk, bqEmpty, bqFull, bqErr } BitQueueStatus_t;

BitQueueStatus_t putBitInQueue(uint8_t bit, BitQueue_t *const qu);
BitQueueStatus_t dequeueBit(uint8_t *bit, BitQueue_t *const qu);

#endif // BIT_QUEUE_H
