/***************************************************************************
 * bbuf.c: Bounded buffer
 * Copyright (C) 2011-2015 Michael C McTernan, mike@mcternan.uk
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 ***************************************************************************/

/**************************************************************************
 * Includes
 **************************************************************************/

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "x_mem.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

/**************************************************************************
 * Types
 **************************************************************************/

struct bbuf
{
    pthread_mutex_t lock;
    pthread_cond_t  notFull, notEmpty;

    uint16_t head, tail, size;

    void *store[];
};

/**************************************************************************
 * Local Variables
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

static bool isFull(struct bbuf *bb)
{
    return ((bb->head + 1) % bb->size) == bb->tail;
}

static bool isEmpty(struct bbuf *bb)
{
    return bb->head == bb->tail;
}

/**************************************************************************
 * Global Functions
 **************************************************************************/

struct bbuf *BBufNew(uint16_t capacity)
{
    struct bbuf *bb;

    bb = x_calloc(sizeof(struct bbuf) + (sizeof(void *) * capacity), 1);
    pthread_mutex_init(&bb->lock, NULL);
    pthread_cond_init(&bb->notFull, NULL);
    pthread_cond_init(&bb->notEmpty, NULL);

    bb->head = bb->tail = 0;
    bb->size = capacity;

    return bb;
}


void BBufPut(struct bbuf *bb, void *data)
{
    pthread_mutex_lock(&bb->lock);

    while(isFull(bb))
    {
        pthread_cond_wait(&bb->notFull, &bb->lock);
    }

    bb->store[bb->head] = data;
    bb->head = (bb->head + 1) % bb->size;

    pthread_cond_signal(&bb->notEmpty);
    pthread_mutex_unlock(&bb->lock);
}


void *BBufGet(struct bbuf *bb)
{
    void *res;

    pthread_mutex_lock(&bb->lock);

    while(isEmpty(bb))
    {
        pthread_cond_wait(&bb->notEmpty, &bb->lock);
    }

    res = bb->store[bb->tail];
    bb->tail = (bb->tail + 1) % bb->size;

    pthread_cond_signal(&bb->notFull);
    pthread_mutex_unlock(&bb->lock);

    return res;
}


void BBufWaitUntilEmpty(struct bbuf *bb)
{
    pthread_mutex_lock(&bb->lock);

    while(!isEmpty(bb))
    {
        pthread_cond_wait(&bb->notFull, &bb->lock);
    }

    pthread_mutex_unlock(&bb->lock);
}

/* END OF FILE */

