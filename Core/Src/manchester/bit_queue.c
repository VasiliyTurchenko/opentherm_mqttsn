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

#include <limits.h>
#include "bit_queue.h"

/**
 * @brief putBitInQueue
 * @param bit
 * @param qu pointer to the queue
 * @return
 */
BitQueueStatus_t putBitInQueue(uint8_t bit, BitQueue_t *const qu)
{
	if (qu == NULL) {
		return bqErr;
	}
	if (qu->pFirst > sizeof(qu->queue) * CHAR_BIT) {
		return bqFull;
	} else {
		qu->queue = (uint32_t)(qu->queue << 0x01U) | (bit & 0x01U);
		qu->pFirst++;
	}
	return bqOk;
}

BitQueueStatus_t dequeueBit(uint8_t *bit, BitQueue_t *const qu)
{
	if (qu == NULL) {
		return bqErr;
	}
	if (qu->pFirst == 0x00U) {
		return bqEmpty;
	}
	uint32_t mask = (uint32_t)(0x01U << (qu->pFirst - 0x01U));
	*bit = ((qu->queue & mask) == 0x00U) ? 0x00U : 0x01U;
	qu->pFirst--;
	return bqOk;
}
