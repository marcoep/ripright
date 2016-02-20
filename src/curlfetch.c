/***************************************************************************
 * curlfetch.c: libcurl URL fetching wrapper.
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
#include <curl/curl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "curlfetch.h"
#include "x_mem.h"
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

struct cfetch
{
    char    *data;
    size_t   size;
};

/**************************************************************************
 * Local Variables
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

/** Callback for fetching URLs via CURL.
 */
static size_t curlCallback(void *ptr, size_t size, size_t nmemb, void *param)
{
    struct cfetch *mbr = (struct cfetch *)param;
    size_t realsize = size * nmemb;

    mbr->data = x_realloc(mbr->data, mbr->size + realsize + 1);
    memcpy(&mbr->data[mbr->size], ptr, realsize);
    mbr->size += realsize;
    mbr->data[mbr->size] = '\0';

    return realsize;
}

/**************************************************************************
 * Global Functions
 **************************************************************************/

/** Fetch some URL and return the contents in a memory buffer.
 * \param[in,out] size    Pointer to fill with the length of returned data.
 * \param[in]     urlFmt  The URL, or a printf-style format string for the URL.
 * \returns A pointer to the read data, or NULL if the fetch failed in any way.
 */
void *CurlFetch(size_t *size, const char *urlFmt, ...)
{
    char          buf[1024];
    struct cfetch cfdata;
    CURL         *ch;
    void         *res;
    va_list       ap;

    memset(&cfdata, 0, sizeof(cfdata));

    /* Formulate the URL */
    va_start(ap, urlFmt);
    vsnprintf(buf, sizeof(buf), urlFmt, ap);
    va_end(ap);

    ch = curl_easy_init();
    if(ch == NULL)
    {
        LogErr("Error: Failed to initialise libcurl\n");
        exit(EXIT_FAILURE);
    }

    /* Try to get the data */
    curl_easy_setopt(ch, CURLOPT_URL, buf);
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curlCallback);
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *)&cfdata);
    curl_easy_setopt(ch, CURLOPT_USERAGENT, "ripright/" VERSION);
    curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);

    if(curl_easy_perform(ch) == 0)
    {
        res = cfdata.data;
        if(size != NULL)
        {
            *size = cfdata.size;
        }
    }
    else
    {
        res = NULL;
        if(size != NULL)
        {
            *size = 0;
        }
        free(cfdata.data);
    }

    curl_easy_cleanup(ch);

    return res;
}

/* END OF FILE */
