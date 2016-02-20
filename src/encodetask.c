/***************************************************************************
 * encodetask.c: Encapsulation of a FLAC encoding task for ripright.
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

#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include "encodetask.h"
#include "x_mem.h"
#include "art.h"
#include "log.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

/**************************************************************************
 * Types
 **************************************************************************/

/**************************************************************************
 * Local Variables
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**************************************************************************
 * Global Functions
 **************************************************************************/

encodetask_t *EncTaskNew(const char *filename, uint8_t nChannels, uint64_t totalSamples)
{
    encodetask_t *r = x_calloc(sizeof(encodetask_t), 1);

    r->rawData = fopen(filename, "rb");
    r->nChannels = nChannels;
    r->totalSamples = totalSamples;

    if(r->rawData == NULL)
    {
        LogErr("Error: Failed to open encoding intermediate file: %m\n");
        exit(EXIT_SUCCESS);
    }

    return r;
}


void EncTaskPrint(const encodetask_t *et, FILE *out)
{
    printf("%s\n", et->outFilename);

    for(uint32_t tag = 0; tag < et->metaTagCount; tag++)
    {
        fprintf(out, "  Tag%" PRIu32 ": %s\n", tag, et->metaTags[tag]);
    }

    if(et->coverArt)
    {
        fprintf(out, "  Artwork: %ux%u pixels\n",
                ArtGetWidth(et->coverArt), ArtGetHeight(et->coverArt));
    }

    fprintf(out, "\n");
}



void EncTaskSetOutputFilename(encodetask_t *et, const char *filename)
{
    char *buf, *c;

    /* Add to the task */
    if(et->outFilename)
    {
        free(et->outFilename);
        free(et->outTempFilename);
    }

    et->outFilename = x_strdup(filename);

    /* Copy the name into a temporary buffer */
    buf = x_malloc(strlen(filename) + 7);
    strcpy(buf, filename);

    /* Find the last slash */
    c = buf + strlen(buf);
    while(c > buf && *c != '/')
    {
        c--;
    }

    /* Skip the slash if not at the start of the buffer */
    if(c != buf)
    {
        c++;
    }

    /* Add a '.' infront of the filename */
    memmove(c + 1, c, strlen(c) + 1);
    *c = '.';
    strcat(c, ".part");

    /* Store the temp filename */
    et->outTempFilename = buf;
}


void EncTaskAddTag(encodetask_t *et, const char *fmt, ...)
{
    char    buf[4096];
    va_list ap;

    /* Print the string */
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* Add to the task */
    assert(et->metaTagCount < MAX_ENCODE_TASK_TAGS);
    et->metaTags[et->metaTagCount++] = x_strdup(buf);
}


FILE *EncTaskGetRawFile(const encodetask_t *et)
{
    return et->rawData;
}


void EncTaskSetArt(encodetask_t *et, art_t art)
{
    if(et->coverArt)
    {
        ArtFree(et->coverArt);
    }
    et->coverArt = ArtDup(art);
}


void EncTaskFree(encodetask_t *et)
{
    fclose(et->rawData);

    while(et->metaTagCount > 0)
    {
        free(et->metaTags[--et->metaTagCount]);
    }

    if(et->coverArt)
    {
        ArtFree(et->coverArt);
    }

    free(et->outFilename);
    free(et->outTempFilename);
    free(et);
}

/* END OF FILE */
