/***************************************************************************
 * encodetask.h: Encapsulation of a FLAC encoding task for ripright.
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

#ifndef ENCODETASK_H
#define ENCODETASK_H

/**************************************************************************
 * Includes
 **************************************************************************/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "art.h"

/**************************************************************************
 * Macros
 **************************************************************************/

#define MAX_ENCODE_TASK_TAGS  32

/**************************************************************************
 * Types
 **************************************************************************/

/** Structure for an audio encoding task.
 */
typedef struct encodetask
{
    uint32_t  trackNum;

    /** Output filename. */
    char     *outFilename;

    /** Temporary output filename to use during encoding. */
    char     *outTempFilename;

    /** Count of tags. */
    uint32_t  metaTagCount;

    /** List of meta-data tags for the audio. */
    char     *metaTags[MAX_ENCODE_TASK_TAGS];

    /** Number of audio channels. */
    uint8_t   nChannels;

    /** Bis per sample. */
    uint8_t   bitsPerSample;

    /** The audio sample rate in Hz. */
    uint32_t  sampleRateHz;

    /** Count of samples per channel in the stream. */
    uint64_t  totalSamples;

    /** The cover art if known, else NULL. */
    art_t     coverArt;

    /** Open handle to the actual audio data. */
    FILE     *rawData;
}
encodetask_t;

/**************************************************************************
 * Prototypes
 **************************************************************************/

encodetask_t *EncTaskNew(const char *filename, uint8_t nChannels, uint64_t totalSamples);

void          EncTaskSetOutputFilename(encodetask_t *et, const char *filename);

void          EncTaskAddTag(encodetask_t *et, const char *fmt, ...);

void          EncTaskPrint(const encodetask_t *et, FILE *out);

FILE         *EncTaskGetRawFile(const encodetask_t *et);

void          EncTaskSetArt(encodetask_t *et, art_t art);

void          EncTaskFree(encodetask_t *et);

#endif /* ENCODETASK */

/* END OF FILE */


