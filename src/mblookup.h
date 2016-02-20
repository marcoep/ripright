/***************************************************************************
 * mblookup.h: MusicBrainz lookup routines.
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

#ifndef MBLOOKUP_H
#define MBLOOKUP_H

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

typedef struct
{
    char                *artistName;
    char                *artistNameSort;
    uint8_t              artistIdCount;
    char               **artistId;
}
mbartistcredit_t;


typedef struct
{
    char                *trackName;
    char                *trackId;
    mbartistcredit_t     trackArtist;
}
mbtrack_t;


/** Representation of a MusicBrainz medium.
 * This gives information about some specific CD.
 */
typedef struct
{
    /** Sequence number of the disc in the release.
     * For single CD releases, this will always be 1, otherwise it will be
     * the number of the CD in the released set.
     */
    uint16_t             discNum;

    /** Title of the disc if there is one. */
    char                *title;

    /** Count of tracks in the medium. */
    uint16_t             trackCount;

    /** Array of the track information. */
    mbtrack_t           *track;

}
mbmedium_t;

typedef struct
{
    char                *releaseGroupId;
    char                *releaseId;
    char                *releaseDate;
    char                *asin;
    char                *albumTitle;
    char                *releaseType;
    mbartistcredit_t     albumArtist;

    /** Total mediums in the release i.e. number of CDs for multidisc releases. */
    uint16_t             discTotal;

    mbmedium_t           medium;
}
mbrelease_t;


typedef struct
{
    uint16_t             releaseCount;
    mbrelease_t         *release;
}
mbresult_t;

/**************************************************************************
 * Prototypes
 **************************************************************************/

bool MbLookup(const char *discId, mbresult_t *res);
void MbPrint(const mbresult_t *res);

void MbFree(mbresult_t *res);

#endif

/* END OF FILE */
