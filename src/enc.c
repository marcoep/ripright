/***************************************************************************
 * enc.c: Encoding module which uses libFLAC.
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
#include <FLAC/stream_encoder.h>
#include <FLAC/metadata.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include "encodetask.h"
#include "x_mem.h"
#include "bbuf.h"
#include "enc.h"
#include "log.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

/** Count of samples to read and send to the encoder.
 * This is simply the number of samples per channel which are read and
 * passed to the decoder in one go.
 */
#define ENC_BLOCK_SAMPLES 4096

/**************************************************************************
 * Types
 **************************************************************************/

/**************************************************************************
 * Local Variables
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

/** Process the passed pathFilename and create each path element.
 * e.g. passed a/b/file.flac, this will create a/ then a/b.
 */
static void createPath(char *pathFilename)
{
    char *slash = pathFilename;

    while((slash = strstr(slash, "/")) != NULL)
    {
        *slash = '\0';

        if(mkdir(pathFilename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) &&
           errno != EEXIST)
        {
            LogWarn("Warning: Could not create '%s': %m\n", pathFilename);
        }

        *slash = '/';
        slash++;
    }
}


static void *encWorker(void *param)
{
    bbuf_t                bb = param;
    struct timeval        timeStart, timeEnd;

    prctl(PR_SET_NAME, "ripright: enc");

    while(1)
    {
        FLAC__StreamEncoderInitStatus  status;
        FLAC__StreamEncoder           *fse;
        encodetask_t                  *et;
        uint8_t                        mdCount;
        FLAC__StreamMetadata          *md[2], *vc, ca;

        /* Wait for an encoding task */
        et = BBufGet(bb);

        /* If a null is recieved, it means the thread should exit */
        if(et == NULL)
        {
            return NULL;
        }

        LogInf("Track%02" PRIu32 ": Encoding to '%s'\n", et->trackNum, et->outFilename);

        mdCount = 0;

        gettimeofday(&timeStart, NULL);

        createPath(et->outFilename);
#if 1
        /* Create and setup a new decoder */
        fse = FLAC__stream_encoder_new();
        FLAC__stream_encoder_set_channels(fse, et->nChannels);
        FLAC__stream_encoder_set_bits_per_sample(fse, et->bitsPerSample);
        FLAC__stream_encoder_set_sample_rate(fse, et->sampleRateHz);
        FLAC__stream_encoder_set_total_samples_estimate(fse, et->totalSamples);
        FLAC__stream_encoder_set_compression_level(fse, 8);

        /* Create the Vorbis comment block */
        vc = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
        if(vc == NULL || !FLAC__metadata_object_vorbiscomment_resize_comments(vc, et->metaTagCount))
        {
            LogErr("Failed to allocate memory for tags");
        }
        else
        {
            /* Setup each tag */
            for(uint32_t t = 0; t < et->metaTagCount; t++)
            {
                FLAC__StreamMetadata_VorbisComment_Entry entry;

                entry.entry = (FLAC__byte *)et->metaTags[t];
                entry.length = strlen(et->metaTags[t]);

                FLAC__metadata_object_vorbiscomment_set_comment(vc, t,entry, true);
            }

            md[mdCount++] = vc;
        }

        /* Create the cover art block if art is present */
        if(et->coverArt != NULL)
        {
            memset(&ca, 0, sizeof(ca));

            ca.type = FLAC__METADATA_TYPE_PICTURE;
            ca.length = (sizeof(uint32_t) * 8) + strlen("image/jpeg") + strlen("Cover image") +
                        ArtGetSizeBytes(et->coverArt);
            ca.data.picture.type = FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER;
            ca.data.picture.mime_type = "image/jpeg";
            ca.data.picture.description = (FLAC__byte*)"Cover image";
            ca.data.picture.colors = 0;
            ca.data.picture.width = ArtGetWidth(et->coverArt);
            ca.data.picture.height = ArtGetHeight(et->coverArt);
            ca.data.picture.depth = ArtGetDepth(et->coverArt);
            ca.data.picture.data_length = ArtGetSizeBytes(et->coverArt);
            ca.data.picture.data = ArtGetData(et->coverArt);

            md[mdCount++] = &ca;
        }

        /* Apply any meta-data */
        if(mdCount > 0)
        {
            FLAC__stream_encoder_set_metadata(fse, md, mdCount);
        }

        status = FLAC__stream_encoder_init_file(fse, et->outTempFilename, NULL, NULL);
        if(status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
        {
            LogErr("Error: Failed to setup FLAC encoder: %s\n",
                   FLAC__StreamEncoderInitStatusString[status]);
        }
        else
        {
            uint64_t sampleCount = 0;

            rewind(et->rawData);
            while(!feof(et->rawData))
            {
                int16_t     buffer16[ENC_BLOCK_SAMPLES];
                FLAC__int32 buffer32[ENC_BLOCK_SAMPLES];
                size_t  n;

                n = fread(buffer16, sizeof(int16_t) * et->nChannels, ENC_BLOCK_SAMPLES / et->nChannels, et->rawData);
                if(n != 0)
                {
                    /* Convert from 16bits per sample to 32bits */
                    for(size_t c = 0; c < n * et->nChannels; c++)
                    {
                        buffer32[c] = buffer16[c];
                    }

                    /* Now encode the data */
                    FLAC__stream_encoder_process_interleaved(fse, buffer32, n);

                    sampleCount += n;
                }
            }

            FLAC__stream_encoder_finish(fse);

            assert(sampleCount == et->totalSamples);

            if(rename(et->outTempFilename, et->outFilename) != 0)
            {
                LogErr("Error: Failed to rename encode output file: %m\n");
            }

            unlink(et->outTempFilename);

            gettimeofday(&timeEnd, NULL);

            long ripMs, trackMs;

            /* Compute how long the rip took */
            ripMs = (timeEnd.tv_sec - timeStart.tv_sec) * 1000;
            ripMs += timeEnd.tv_usec / 1000;
            ripMs -= timeStart.tv_usec / 1000;

            /* Compute track length */
            trackMs = (sampleCount * 1000) / 44100;

            LogInf("Track%02" PRIu32 ": Encoded at %3.1fx\n", et->trackNum, (float)trackMs / (float)ripMs);
        }

        FLAC__metadata_object_delete(vc);
#endif
        EncTaskFree(et);
    }

    return NULL;
}

/**************************************************************************
 * Global Functions
 **************************************************************************/

void EncNew(bbuf_t bbuf)
{
    struct sched_param scparam;
    int                policy;
    pthread_t          tid;

    pthread_create(&tid, NULL, encWorker, bbuf);

    /* Get the scheduling for the thread and make it lower priority.
     *  We prefer the ripping thread to get the CPU if there is contention,
     *  since we can rip offline later.
     */
    pthread_getschedparam(tid, &policy, &scparam);

    if(sched_get_priority_min(policy) < sched_get_priority_max(policy))
    {
        if(sched_get_priority_min(policy) < scparam.sched_priority)
        {
            scparam.sched_priority--;
        }
    }
    else
    {
        if(sched_get_priority_min(policy) > scparam.sched_priority)
        {
            scparam.sched_priority++;
        }
    }

    pthread_setschedparam(tid, policy, &scparam);
}

/* END OF FILE */
