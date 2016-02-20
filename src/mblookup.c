/***************************************************************************
 * mblookup.c: MusicBrainz lookup routines.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "curlfetch.h"
#include "xmlparse.h"
#include "mblookup.h"
#include "x_mem.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

#define M_ArrayElem(a) (sizeof(a) / sizeof(a[1]))

/**************************************************************************
 * Types
 **************************************************************************/

/**************************************************************************
 * Local Variables
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

/** Compare two strings, either of which maybe NULL.
 */
static int strcheck(const char *a, const char *b)
{
    if(a == NULL && b == NULL)
    {
        return 0;
    }
    else if(a == NULL)
    {
        return -1;
    }
    else if(b == NULL)
    {
        return 1;
    }
    else
    {
        return strcmp(a, b);
    }
}

/** Free memory associated with a mbartistcredit_t structure.
 */
static void freeArtist(mbartistcredit_t *ad)
{
    free(ad->artistName);
    free(ad->artistNameSort);

    for(uint8_t t = 0; t < ad->artistIdCount; t++)
    {
        free(ad->artistId[t]);
    }

    free(ad->artistId);
}


static void freeMedium(mbmedium_t *md)
{
    free(md->title);

    for(uint16_t t = 0; t < md->trackCount; t++)
    {
        mbtrack_t *td = &md->track[t];

        free(td->trackName);
        freeArtist(&td->trackArtist);

    }

    free(md->track);
}


static void freeRelease(mbrelease_t *rel)
{
    free(rel->releaseGroupId);
    free(rel->releaseId);
    free(rel->asin);
    free(rel->albumTitle);
    free(rel->releaseType);

    freeArtist(&rel->albumArtist);
    freeMedium(&rel->medium);
}


static bool artistsAreIdentical(const mbartistcredit_t *ma, const mbartistcredit_t *mb)
{
    if(strcheck(ma->artistName, mb->artistName) != 0 ||
       strcheck(ma->artistNameSort, mb->artistNameSort) != 0)
    {
        return false;
    }

    return true;
}

static bool mediumsAreIdentical(const mbmedium_t *ma, const mbmedium_t *mb)
{
    if(ma->discNum != mb->discNum ||
       ma->trackCount != mb->trackCount ||
       strcheck(ma->title, mb->title) != 0)
    {
        return false;
    }

    for(uint16_t t = 0; t < ma->trackCount; t++)
    {
        const mbtrack_t *mta = &ma->track[t], *mtb = &mb->track[t];

        if(strcheck(mta->trackName, mtb->trackName) != 0 ||
           !artistsAreIdentical(&mta->trackArtist, &mtb->trackArtist))
        {
            return false;
        }
    }

    return true;
}


static bool releasesAreIdentical(const mbrelease_t *ra, const mbrelease_t *rb)
{
    if(ra->discTotal != rb->discTotal ||
       !artistsAreIdentical(&ra->albumArtist, &rb->albumArtist) ||
       strcheck(ra->releaseGroupId, rb->releaseGroupId) != 0 ||
       strcheck(ra->albumTitle, rb->albumTitle) != 0 ||
       strcheck(ra->releaseType, rb->releaseType) != 0 ||
       strcheck(ra->asin, rb->asin) != 0 ||
       !mediumsAreIdentical(&ra->medium, &rb->medium))
    {
        return false;
    }

    return true;
}


static uint16_t dedupeMediums(uint16_t mediumCount, mbmedium_t *mr)
{
    for(uint16_t t = 0; t < mediumCount; t++)
    {
        bool dupe = false;

        for(uint16_t u = t + 1; u < mediumCount && !dupe; u++)
        {
            if(mediumsAreIdentical(&mr[t], &mr[u]))
            {
                dupe = true;
            }
        }

        if(dupe)
        {
            mediumCount--;
            freeMedium(&mr[t]);
            mr[t] = mr[mediumCount];
            t--;
        }
    }

    return mediumCount;
}


static void dedupeReleases(mbresult_t *mr)
{
    for(uint8_t t = 0; t < mr->releaseCount; t++)
    {
        bool dupe = false;

        for(uint8_t u = t + 1; u < mr->releaseCount && !dupe; u++)
        {
            if(releasesAreIdentical(&mr->release[t], &mr->release[u]))
            {
                dupe = true;
            }
        }

        if(dupe)
        {
            mr->releaseCount--;
            freeRelease(&mr->release[t]);
            mr->release[t] = mr->release[mr->releaseCount];
            t--;
        }
    }
}


/** Process an %lt;artist-credit&gt; node.
 */
static void processArtistCredit(struct xmlnode *artistCreditNode, mbartistcredit_t *cd)
{

    const char     *s = XmlGetContent(artistCreditNode);
    struct xmlnode *n = NULL;

    assert(strcmp(XmlGetTag(artistCreditNode), "artist-credit") == 0);

    /* Artist credit can have multiple entries... */
    while((s = XmlParseStr(&n, s)) != NULL)
    {
        /* Process name-credits only at present */
        if(XmlTagStrcmp(n, "name-credit") == 0)
        {
            struct xmlnode *o, *artistNode = XmlFindSubNode(n, "artist");

            cd->artistIdCount++;
            cd->artistId = x_realloc(cd->artistId, sizeof(char *) * cd->artistIdCount);

            cd->artistId[cd->artistIdCount - 1] = XmlGetAttributeDup(artistNode, "id");

            o = XmlFindSubNode(artistNode, "name");
            if(o)
            {
                const char *aa = XmlGetContent(o);

                if(cd->artistName == NULL)
                {
                    cd->artistName = x_strdup(aa);
                }
                else
                {
                    /* Concatenate multiple artists if needed */
                    cd->artistName = x_realloc(cd->artistName, strlen(aa) + strlen(cd->artistName) + 3);

                    strcat(cd->artistName, ", ");
                    strcat(cd->artistName, aa);
                }

                XmlDestroy(&o);
            }

            o = XmlFindSubNode(artistNode, "sort-name");
            if(o)
            {
                cd->artistNameSort = x_strdup(XmlGetContent(o));
                XmlDestroy(&o);
            }

            XmlDestroy(&artistNode);
        }
    }
}


/** Process a &lt;track&gt; track node.
 */
static void processTrackNode(struct xmlnode *trackNode, mbmedium_t *md)
{
    struct xmlnode *n = NULL;

    assert(strcmp(XmlGetTag(trackNode), "track") == 0);

    n = XmlFindSubNode(trackNode, "position");
    if(n)
    {
        uint16_t position = atoi(XmlGetContent(n));

        if(position >= 1 && position <= md->trackCount)
        {
            struct xmlnode *recording;
            mbtrack_t      *td = &md->track[position - 1];

            recording = XmlFindSubNode(trackNode, "recording");
            if(recording)
            {
                struct xmlnode *m = NULL;

                td->trackId = XmlGetAttributeDup(recording, "id");

                m = XmlFindSubNode(recording, "title");
                if(m)
                {
                    td->trackName = x_strdup(XmlGetContent(m));
                    XmlDestroy(&m);
                }

                m = XmlFindSubNode(recording, "artist-credit");
                if(m)
                {
                    processArtistCredit(m, &td->trackArtist);
                    XmlDestroy(&m);
                }

                XmlDestroy(&recording);
            }
        }

        XmlDestroy(&n);
    }
}


/** Process a &lt;medium&gt; node.
 */
static bool processMediumNode(struct xmlnode *mediumNode, const char *discId, mbmedium_t *md)
{
    bool            mediumValid = false;
    struct xmlnode *n = NULL;

    assert(strcmp(XmlGetTag(mediumNode), "medium") == 0);

    memset(md, 0, sizeof(mbmedium_t));

    /* First find the disc-list to check if the discId is present */
    n = XmlFindSubNode(mediumNode, "disc-list");
    if(n)
    {
        struct xmlnode *disc = NULL;
        const char     *s;

        s = XmlGetContent(n);
        while((s = XmlParseStr(&disc, s)) != NULL)
        {
            if(XmlTagStrcmp(disc, "disc") == 0)
            {
                const char *id = XmlGetAttribute(disc, "id");

                if(id && strcmp(id, discId) == 0)
                {
                    mediumValid = true;
                }
            }
        }

        XmlDestroy(&disc);
        XmlDestroy(&n);
    }

    /* Check if the medium should be processed */
    if(mediumValid)
    {
        memset(md, 0, sizeof(mbmedium_t));

        n = XmlFindSubNode(mediumNode, "position");
        if(n)
        {
            md->discNum = atoi(XmlGetContent(n));
            XmlDestroy(&n);
        }

        n = XmlFindSubNode(mediumNode, "title");
        if(n)
        {
            md->title = x_strdup(XmlGetContent(n));
            XmlDestroy(&n);
        }

        n = XmlFindSubNode(mediumNode, "track-list");
        if(n)
        {
            const char *count = XmlGetAttribute(n, "count");

            if(count)
            {
                struct xmlnode *track = NULL;
                const char     *s;

                md->trackCount = atoi(count);
                md->track = x_calloc(sizeof(mbtrack_t), md->trackCount);

                s = XmlGetContent(n);
                while((s = XmlParseStr(&track, s)) != NULL)
                {
                    if(XmlTagStrcmp(track, "track") == 0)
                    {
                        processTrackNode(track, md);
                    }
                }

                mediumValid = true;

                XmlDestroy(&track);
            }

            XmlDestroy(&n);
        }
    }

    return mediumValid;
}


static uint16_t processReleaseNode(struct xmlnode *releaseNode, const char *discId, mbrelease_t *cd, mbmedium_t **md)
{
    struct xmlnode *n;

    assert(strcmp(XmlGetTag(releaseNode), "release") == 0);

    memset(cd, 0, sizeof(mbrelease_t));

    cd->releaseId = XmlGetAttributeDup(releaseNode, "id");

    n = XmlFindSubNode(releaseNode, "asin");
    if(n)
    {
        cd->asin = x_strdup(XmlGetContent(n));
        XmlDestroy(&n);
    }

    n = XmlFindSubNode(releaseNode, "title");
    if(n)
    {
        cd->albumTitle = x_strdup(XmlGetContent(n));
        XmlDestroy(&n);
    }

    n = XmlFindSubNode(releaseNode, "release-group");
    if(n)
    {
        cd->releaseType = XmlGetAttributeDup(n, "type");
        cd->releaseGroupId = XmlGetAttributeDup(n, "id");
        XmlDestroy(&n);
    }

    n = XmlFindSubNode(releaseNode, "artist-credit");
    if(n)
    {
        processArtistCredit(n, &cd->albumArtist);
        XmlDestroy(&n);
    }

    n = XmlFindSubNode(releaseNode, "medium-list");
    if(n)
    {
        const char     *s, *count = XmlGetAttribute(n, "count");
        struct xmlnode *m = NULL;
        mbmedium_t     *mediums = NULL;
        uint16_t        mediumCount = 0;

        if(count)
        {
            cd->discTotal = atoi(count);
        }

        s = XmlGetContent(n);

        while((s = XmlParseStr(&m, s)) != NULL)
        {
            if(XmlTagStrcmp(m, "medium") == 0)
            {
                mediums = x_realloc(mediums, sizeof(mbmedium_t) * (mediumCount + 1));
                if(processMediumNode(m, discId, &mediums[mediumCount]))
                {
                    mediumCount++;
                }
            }
        }

        XmlDestroy(&n);
        XmlDestroy(&m);

        mediumCount = dedupeMediums(mediumCount, mediums);

        *md = mediums;
        return mediumCount;
    }
    else
    {
        *md = NULL;
        return 0;
    }
}


/** Process a release.
 */
static bool processRelease(const char *releaseId, const char *discId, mbresult_t *res)
{
    struct xmlnode *metaNode = NULL, *releaseNode = NULL;
    void           *buf;
    const char     *s;

    s = buf = CurlFetch(NULL, "http://musicbrainz.org/ws/2/release/%s?inc=recordings+artists+release-groups+discids+artist-credits", releaseId);
    if(buf == NULL)
    {
        return false;
    }

    /* Find the metadata node */
    do
    {
        s = XmlParseStr(&metaNode, s);
    }
    while(metaNode != NULL && XmlTagStrcmp(metaNode, "metadata") != 0);

    free(buf);

    if(metaNode == NULL)
    {
        return false;
    }

    releaseNode = XmlFindSubNodeFree(metaNode, "release");
    if(releaseNode)
    {
        mbmedium_t  *medium = NULL;
        uint16_t     mediumCount;
        mbrelease_t  release;

        mediumCount = processReleaseNode(releaseNode, discId, &release, &medium);

        for(uint16_t m = 0; m < mediumCount; m++)
        {
            mbrelease_t *newRel;

            res->releaseCount++;
            res->release = x_realloc(res->release, sizeof(mbrelease_t) * res->releaseCount);

            newRel = &res->release[res->releaseCount -1];

            memcpy(newRel, &release, sizeof(mbrelease_t));
            memcpy(&newRel->medium, &medium[m], sizeof(mbmedium_t));
        }

        XmlDestroy(&releaseNode);

        return true;
    }

    return false;
}

/**************************************************************************
 * Global Functions
 **************************************************************************/

/** Lookup some CD.
 * \param[in] discId  The ID of the CD to lookup.
 * \param[in] res     Pointer to populate with the results.
 * \retval true   If the CD found and the results returned.
 * \retval false  If the CD is unknown to MusicBrainz.
 */
bool MbLookup(const char *discId, mbresult_t *res)
{
    struct xmlnode *metaNode = NULL, *releaseListNode = NULL;
    void           *buf;
    const char     *s;

    memset(res, 0, sizeof(mbresult_t));

    s = buf = CurlFetch(NULL, "http://musicbrainz.org/ws/2/discid/%s", discId);
    if(!buf)
    {
        return false;
    }

    /* Find the metadata node */
    do
    {
        s = XmlParseStr(&metaNode, s);
    }
    while(metaNode != NULL && XmlTagStrcmp(metaNode, "metadata") != 0);

    free(buf);

    if(metaNode == NULL)
    {
        return false;
    }

    releaseListNode = XmlFindSubNodeFree(XmlFindSubNodeFree(metaNode, "disc"), "release-list");
    if(releaseListNode)
    {
        struct xmlnode *releaseNode = NULL;
        const char     *s;

        s = XmlGetContent(releaseListNode);
        while((s = XmlParseStr(&releaseNode, s)) != NULL)
        {
            const char *releaseId;

            if((releaseId = XmlGetAttribute(releaseNode, "id")) != NULL)
            {
                processRelease(releaseId, discId, res);
            }
        }

        XmlDestroy(&releaseNode);

        dedupeReleases(res);
    }

    XmlDestroy(&releaseListNode);

    return true;
}


void MbFree(mbresult_t *res)
{
    for(uint16_t r = 0; r < res->releaseCount; r++)
    {
        freeRelease(&res->release[r]);
    }

    free(res->release);
}


/** Print a result structure to stdout.
 */
void MbPrint(const mbresult_t *res)
{
    for(uint16_t r = 0; r < res->releaseCount; r++)
    {
        mbrelease_t *rel = &res->release[r];

        printf("Release=%s\n",           rel->releaseId);
        printf("  ASIN=%s\n",            rel->asin);
        printf("  Album=%s\n",           rel->albumTitle);
        printf("  AlbumArtist=%s\n",     rel->albumArtist.artistName);
        printf("  AlbumArtistSort=%s\n", rel->albumArtist.artistNameSort);
        printf("  ReleaseType=%s\n",     rel->releaseType);
        printf("  ReleaseGroupId=%s\n",  rel->releaseGroupId);

        for(uint8_t t = 0; t < rel->albumArtist.artistIdCount; t++)
        {
            printf("  ArtistId=%s\n", rel->albumArtist.artistId[t]);
        }

        printf("  Total Disc=%u\n", rel->discTotal);

        const mbmedium_t *mb = &rel->medium;

        printf("  Medium\n");
        printf("    DiscNum=%u\n", mb->discNum);
        printf("    Title=%s\n", mb->title);

        for(uint16_t u = 0; u < mb->trackCount; u++)
        {
            const mbtrack_t *td = &mb->track[u];

            printf("    Track %u\n", u);
            printf("      Id=%s\n", td->trackId);
            printf("      Title=%s\n", td->trackName);
            printf("      Artist=%s\n", td->trackArtist.artistName);
            printf("      ArtistSort=%s\n", td->trackArtist.artistNameSort);
            for(uint8_t t = 0; t < td->trackArtist.artistIdCount; t++)
            {
                printf("      ArtistId=%s\n", td->trackArtist.artistId[t]);
            }
        }
    }
}

/**************************************************************************
 * Module Test
 **************************************************************************/

#ifdef MODULE_TEST

/*
 * gcc -std=gnu99 -ggdb -DMODULE_TEST -DVERSION=\"0.5beta\" mblookup.c xmlparse.c curlfetch.c log.c x_mem.c -lcurl
 */

int main(int argc, char *argv[])
{
    mbresult_t res;

    if(argc > 1)
    {
        MbLookup(argv[1], &res);
        MbPrint(&res);

        MbFree(&res);
    }

    return 0;
}
#endif

/* END OF FILE */
