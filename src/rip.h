/***************************************************************************
 * rip.h: Interface to the CD ripping module.
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

#ifndef RIP_H
#define RIP_H

/**************************************************************************
 * Includes
 **************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/**************************************************************************
 * Macros
 **************************************************************************/

/**************************************************************************
 * Types
 **************************************************************************/

typedef struct rip rip_t;

/**************************************************************************
 * Prototypes
 **************************************************************************/

rip_t   *RipNew(const char *dev);
uint16_t RipGetTrackCount(rip_t *r);
bool     RipTrack(rip_t *r, const int32_t track, FILE *outfile, uint8_t *nChannels, uint64_t *samplesPerChannel);
void     RipFree(rip_t *r);

#endif

/* END OF FILE */

