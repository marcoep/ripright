/***************************************************************************
 * art.c: Cover art retrieval and processing.
 * Copyright (C) 2007-2015 Michael C McTernan, mike@mcternan.uk
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
#include <wand/MagickWand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curlfetch.h"
#include "x_mem.h"
#include "art.h"
#include "log.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

#define M_ArraySize(a)  (sizeof(a) / sizeof(a[0]))

/**************************************************************************
 * Types
 **************************************************************************/

struct art
{
    char    *data;
    size_t   size;
    uint32_t width, height, depth;
};


/**************************************************************************
 * Local Variables
 **************************************************************************/

static const char *artUrl[] = { "http://images.amazon.com/images/P/%s.02._SCLZZZZZZZ_.jpg", /* UK */
                                "http://images.amazon.com/images/P/%s.01._SCLZZZZZZZ_.jpg", /* US */
                                "http://images.amazon.com/images/P/%s.03._SCLZZZZZZZ_.jpg"  /* DE */
                                "http://images.amazon.com/images/P/%s.08._SCLZZZZZZZ_.jpg"  /* FR */
                                "http://images.amazon.com/images/P/%s.09._SCLZZZZZZZ_.jpg"  /* JP */
                              };

static const char *productUrl[] = { "http://amazon.co.uk/o/ASIN/%s/",
                                    "http://amazon.com/o/ASIN/%s/",
                                    "http://amazon.de/o/ASIN/%s/",
                                    "http://amazon.fr/o/ASIN/%s/",
                                    "http://amazon.co.jp/o/ASIN/%s/"
                                  };

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**************************************************************************
 * Global Functions
 **************************************************************************/

art_t ArtGet(const char *asin)
{
    uint8_t      attempt = 0;
    struct art  *art;

    /* Bail if no ASIN has been supplied */
    if(asin == NULL || strlen(asin) == 0)
    {
        return NULL;
    }

    art = x_calloc(sizeof(struct art),1);

    do
    {
        /* Free any previous attempt */
        if(art->data)
        {
            free(art->data);
        }

        /* Try to fetch the URL */
        art->data = CurlFetch(&art->size, artUrl[attempt++], asin);
        if(art->data)
        {
            MagickWand *mw;

            MagickWandGenesis();
            mw = NewMagickWand();

            if(MagickReadImageBlob(mw, art->data, art->size))
            {
                art->width  = MagickGetImageWidth(mw);
                art->height = MagickGetImageHeight(mw);
                art->depth  = MagickGetImageDepth(mw) * 3 /* RGB - no alpha in JPEG */;
            }

            DestroyMagickWand(mw);
            MagickWandTerminus();
        }


    }
    while((art->width < 10 || art->height < 10) && attempt < M_ArraySize(artUrl));

    /* If not okay, free the memory */
    if(attempt >= M_ArraySize(artUrl))
    {
        ArtFree(art);
        art = NULL;
    }
    else
    {
        char format[strlen(productUrl[attempt - 1]) + 16];

        sprintf(format, "Product URL: %s\n", productUrl[attempt - 1]);

        /* Log the product URL if we got the art */
        LogInf(format, asin);
    }

    return art;
}


/** Duplicate some art structure.
 * This takes a deep copy of data.
 */
struct art *ArtDup(struct art *art)
{
    struct art *newArt = x_malloc(sizeof(struct art));

    if(newArt)
    {
        memcpy(newArt, art, sizeof(struct art));

        newArt->data = x_malloc(art->size);
        if(newArt->data)
        {
            memcpy(newArt->data, art->data, art->size);
        }
        else
        {
            free(newArt);
            newArt = NULL;
        }

    }
    return newArt;
}

size_t ArtGetSizeBytes(struct art *art)
{
    return art->size;
}

void *ArtGetData(struct art *art)
{
    return art->data;
}


uint32_t ArtGetWidth(struct art *art)
{
    return art->width;
}


uint32_t ArtGetHeight(struct art *art)
{
    return art->height;
}


uint8_t ArtGetDepth(struct art *art)
{
    return art->depth;
}


void ArtFree(struct art *art)
{
    if(art->data)
    {
        free(art->data);
    }

    free(art);
}


/** Save art work to a file, performing type conversion if possible.
 */
bool ArtDumpToFile(struct art *art, const char *filename)
{
    const char *format = filename, *dot;
    MagickWand *mw;
    bool        ok;

    while((dot = strchr(format, '.')) != NULL)
    {
        format = dot + 1;
    }

    MagickWandGenesis();
    mw = NewMagickWand();

    ok = MagickReadImageBlob(mw, art->data, art->size) &&
         MagickSetImageFormat(mw, format) &&
         MagickWriteImage(mw, filename);

    DestroyMagickWand(mw);
    MagickWandTerminus();

    return ok;
}

/* END OF FILE */

