/***************************************************************************
 * bbuf.h: Interface to bounded buffer ADT
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

#ifndef BBUF_H
#define BBUF_H

/**************************************************************************
 * Includes
 **************************************************************************/

#include <stdint.h>

/**************************************************************************
 * Macros
 **************************************************************************/

/**************************************************************************
 * Types
 **************************************************************************/

typedef struct bbuf *bbuf_t;

/**************************************************************************
 * Prototypes
 **************************************************************************/

bbuf_t BBufNew(uint16_t capacity);
void   BBufPut(bbuf_t bb, void *data);
void  *BBufGet(bbuf_t bb);
void   BBufWaitUntilEmpty(struct bbuf *bb);

#endif

/* END OF FILE */


