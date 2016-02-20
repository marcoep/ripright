/***************************************************************************
 * format.h: Filename formatting routines for RipRight.
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

#ifndef FORMAT_H
#define FORMAT_H

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

/**************************************************************************
 * Global Variables
 **************************************************************************/

extern bool gWin32Escapes;

/**************************************************************************
 * Prototypes
 **************************************************************************/

char *Format(const char *prefix,
             const char *format,
             uint16_t    trackNum,
             const char *artist,
             const char *artistSort,
             const char *albumArtist,
             const char *albumArtistSort,
             const char *albumName,
             const char *trackName,
             const char *releaseType);

bool FormatIsValid(const char *format);

#endif

/* END OF FILE */
