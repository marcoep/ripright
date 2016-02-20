/***************************************************************************
 * rip.c: CD ripping module that uses the cdparanoia library.
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

#include "config.h"
#include <pthread.h>
#ifdef HAVE_CDDA_INTERFACE_H
#include <cdda_interface.h>
#include <cdda_paranoia.h>
#endif
#ifdef HAVE_CDDA_CDDA_INTERFACE_H
#include <cdda/cdda_interface.h>
#include <cdda/cdda_paranoia.h>
#endif
#include <sys/time.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "ripright.h"
#include "x_mem.h"
#include "rip.h"
#include "log.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

#define NUM_EVENTS 14

/**************************************************************************
 * Types
 **************************************************************************/

struct rip
{
    cdrom_drive *cdrd;

    uint32_t eventCount[NUM_EVENTS];
};

/**************************************************************************
 * Local Variables
 **************************************************************************/

static const struct
{
    uint16_t    code;
    const char *description;
}
paranoiaCodes[NUM_EVENTS] =
{
    { PARANOIA_CB_READ,          "read" },
    { PARANOIA_CB_VERIFY,        "verifying jitter" },
    { PARANOIA_CB_FIXUP_EDGE,    "fixed edge jitter" },
    { PARANOIA_CB_FIXUP_ATOM,    "fixed atom jitter" },
    { PARANOIA_CB_SCRATCH,       "scratch" },
    { PARANOIA_CB_REPAIR,        "repair" },
    { PARANOIA_CB_SKIP,          "skip exhaused retrys" },
    { PARANOIA_CB_DRIFT,         "drift exhaused retrys" },
    { PARANOIA_CB_BACKOFF,       "backoff" },
    { PARANOIA_CB_OVERLAP,       "dynamic overlap adjust" },
    { PARANOIA_CB_FIXUP_DROPPED, "fixed dropped bytes" },
    { PARANOIA_CB_FIXUP_DUPED,   "fixed duplicated bytes" },
    { PARANOIA_CB_READERR,       "read error" },
  /*{ PARANOIA_CB_CACHEERR,      "cache error" }*/
};

static rip_t            *ripTask = NULL;

static pthread_mutex_t   ripLock = PTHREAD_MUTEX_INITIALIZER;

/**************************************************************************
 * Local Functions
 **************************************************************************/

static void ripCallback(long inpos __attribute__((__unused__)),
                        int  function)
{
    for(uint16_t t = 0; t < NUM_EVENTS; t++)
    {
        if(paranoiaCodes[t].code == function)
        {
            ripTask->eventCount[t]++;
            return;
        }
    }
}

/**************************************************************************
 * Global Functions
 **************************************************************************/

rip_t *RipNew(const char *dev)
{
    rip_t *r = x_calloc(sizeof(rip_t), 1);

    r->cdrd = cdda_identify(dev, STDOUT_FILENO, NULL);

    if(r->cdrd == NULL || cdda_open(r->cdrd) != 0)
    {
        free(r);
        return NULL;
    }

    return r;
}

uint16_t RipGetTrackCount(rip_t *r)
{
    return cdda_tracks(r->cdrd);
}

bool RipTrack(rip_t *r, const int32_t track, FILE *outfile, uint8_t *nChannels, uint64_t *samplesPerChannel)
{
    const int       maxRetries = 20;    /* Must be a multiple of 5 */
    struct timeval  timeStart, timeEnd;
    int16_t         mode;
    cdrom_paranoia *pn;

    pn = paranoia_init(r->cdrd);
    if(!pn)
    {
        return false;
    }

    mode = PARANOIA_MODE_FULL;
    if(gRipAllowSkip)
    {
        mode &= ~PARANOIA_MODE_NEVERSKIP;
    }
    paranoia_modeset(pn, mode);

    cdda_verbose_set(r->cdrd, CDDA_MESSAGE_LOGIT, CDDA_MESSAGE_LOGIT);

    assert(track > 0 && track < cdda_tracks(r->cdrd) + 1);

    /* Check it is an audio track */
    if(cdda_track_audiop(r->cdrd, track))
    {
        const long firstSec = cdda_track_firstsector(r->cdrd, track);
        const long lastSec = cdda_track_lastsector(r->cdrd, track);
        const long totalSec = (lastSec - firstSec) + 1;
        const long trackMs = (totalSec * 1000) / 75;

        LogInf("Track%02" PRIu32 ": Ripping %lu sectors, %5.1f seconds of audio\n", track, totalSec, (float)trackMs / 1000.0f);

        /* Get the number of channels and compute the sample count */
        *nChannels = cdda_track_channels(r->cdrd, track);
        *samplesPerChannel = (totalSec * CD_FRAMESIZE_RAW) / (sizeof(uint16_t) * *nChannels);

        /* Seek to the first sector */
        if(paranoia_seek(pn, firstSec, SEEK_SET) == -1)
        {
            LogErr("Failed to seek to sector %ld: skipping track", firstSec);
        }
        else
        {
            pthread_mutex_lock(&ripLock);

            ripTask = r;

            gettimeofday(&timeStart, NULL);

            /* Read each sector */
            for(long sec = firstSec; sec <= lastSec; sec++)
            {
                int16_t *data = paranoia_read_limited(pn, ripCallback, maxRetries);
                fwrite(data, CD_FRAMESIZE_RAW, 1, outfile);
            }

            gettimeofday(&timeEnd, NULL);

            ripTask = NULL;

            pthread_mutex_unlock(&ripLock);

            long ripMs;

            /* Compute how long the rip took */
            ripMs = (timeEnd.tv_sec - timeStart.tv_sec) * 1000;
            ripMs += timeEnd.tv_usec / 1000;
            ripMs -= timeStart.tv_usec / 1000;

            /* Display rip speed and stats */
            LogInf("Track%02" PRIu32 ": Ripped at %3.1fx\n", track, (float)trackMs / (float)ripMs);

            for(uint8_t t = 0; t < NUM_EVENTS; t++)
            {
                if(r->eventCount[t] != 0)
                {
                    LogInf(" %-24s: %6" PRIu32 "\n",
                           paranoiaCodes[t].description, r->eventCount[t]);
                }
            }
        }
    }

    paranoia_free(pn);

    return true;
}


void RipFree(rip_t *r)
{
    cdda_close(r->cdrd);
    free(r);
}

/* END OF FILE */

