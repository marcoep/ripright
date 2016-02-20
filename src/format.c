/***************************************************************************
 * format.c: Output file name formatting for ripright.
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

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "format.h"
#include "x_mem.h"

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
 * Global Variables
 **************************************************************************/

/** Escape Win32 illegal filename characters. */
bool gWin32Escapes = false;

/**************************************************************************
 * Local Functions
 **************************************************************************/

static void expandAndPrint(char      **out,
                           uint32_t   *outLen,
                           const char *str,
                           bool        escape)
{
    uint32_t l = strlen(str) + 1;
    char    *s;

    while(l > *outLen)
    {
        *outLen += 1024;
        *out = x_realloc(*out, *outLen);
    }

    /* Copy string, escaping slashes */
    while(*str != '\0')
    {
        s = *out;

        if(escape && *str == '/')
        {
            /* UTF-8 forward slash, legal in filenames */
            s[0] = 0xe2;
            s[1] = 0x88;
            s[2] = 0x95;
            *out += 3;
            *outLen -= 3;
        }
        else if(gWin32Escapes && escape && *str == ':')
        {
            s[0] = 0xcb;
            s[1] = 0x90;
            *out += 2;
            *outLen -= 2;
        }
        else if(gWin32Escapes && escape && *str == '?')
        {
            s[0] = 0xca;
            s[1] = 0x94;
            *out += 2;
            *outLen -= 2;
        }
        else if(gWin32Escapes && escape && *str == '"')
        {
            s[0] = 0xc2;
            s[1] = 0xa8;
            *out += 2;
            *outLen -= 2;
        }
        else if(gWin32Escapes && escape && *str == '|')
        {
            s[0] = 0xc7;
            s[1] = 0x80;
            *out += 2;
            *outLen -= 2;
        }
        else if(gWin32Escapes && escape && *str == '*')
        {
            s[0] = 0xd3;
            s[1] = 0xbf;
            *out += 2;
            *outLen -= 2;
        }
        else
        {
            s[0] = *str;
            *out += 1;
            *outLen -= 1;
        }

        str++;
    }

    s = *out;
    s[0] = '\0';
}

/**************************************************************************
 * Global Functions
 **************************************************************************/

/** Format information into a filename.
 *
 * %N   = track number
 * %A   = Artist
 * %a   = Artist sort name
 * %B   = Album artist
 * %b   = Album artist sort name
 * %C   = Artist, else album artist
 * %c   = Artist sort name, else album artist sort name
 * %D   = Album name (disc)
 * %T   = Trackname
 * %Y   = Release type (album, single, compilation etc...)
 */
char *Format(const char *prefix,
             const char *format,
             uint16_t    trackNum,
             const char *artist,
             const char *artistSort,
             const char *albumArtist,
             const char *albumArtistSort,
             const char *albumName,
             const char *trackName,
             const char *releaseType)
{
    uint32_t outLen = 1024;
    char    *r, *out = x_malloc(outLen);
    char     trackString[4];

    r = out;

    if(artistSort == NULL || *artistSort == '\0')
    {
        artistSort = artist;
    }

    if(albumArtistSort == NULL || *albumArtistSort == '\0')
    {
        albumArtistSort = albumArtist;
    }

    snprintf(trackString, 4, "%02" PRIu16, trackNum);

    if(prefix != NULL)
    {
        expandAndPrint(&out, &outLen, prefix, false);
    }

    while(out != NULL && *format != '\0')
    {
        if(*format == '%')
        {
            format++;
            switch(*format)
            {
                case 'N':
                    expandAndPrint(&out, &outLen, trackString, true);
                    break;
                case 'A':
                    expandAndPrint(&out, &outLen, artist, true);
                    break;
                case 'a':
                    expandAndPrint(&out, &outLen, artistSort, true);
                    break;
                case 'B':
                    expandAndPrint(&out, &outLen, albumArtist, true);
                    break;
                case 'b':
                    expandAndPrint(&out, &outLen, albumArtistSort, true);
                    break;
                case 'C':
                    if(*artist != '\0')
                        expandAndPrint(&out, &outLen, artist, true);
                    else
                        expandAndPrint(&out, &outLen, albumArtist, true);
                    break;
                case 'c':
                    if(*artistSort != '\0')
                        expandAndPrint(&out, &outLen, artistSort, true);
                    else
                        expandAndPrint(&out, &outLen, albumArtistSort, true);
                    break;
                case 'D':
                    expandAndPrint(&out, &outLen, albumName, true);
                    break;
                case 'T':
                    expandAndPrint(&out, &outLen, trackName, true);
                    break;
                case 'Y':
                    expandAndPrint(&out, &outLen, releaseType, true);
                    break;
                case '%':
                    expandAndPrint(&out, &outLen, "%", false);
                    break;
                default:
                    fprintf(stderr, "Error: Unknown format specifier '%%%c'\n", *format);
                    free(r);
                    return NULL;
            }
        }
        else
        {
            char c[2] = { *format, '\0' };
            expandAndPrint(&out, &outLen, c, false);
        }

        format++;
    }

    return r;
}


/** Check if some format string is valid.
 */
bool FormatIsValid(const char *format)
{
    char *f;

    f = Format(NULL, format, 0, "", "", "", "", "", "", "");
    free(f);

    return f != NULL;
}

/**************************************************************************
 * Module Test
 **************************************************************************/

#ifdef MODULE_TEST

int main(int argc, char *argv[])
{
    char *f = Format(argv[1], 1, "Artist", "AlbumArtist", "Al:bum", "TrackName", "ReleaseType");

    printf("%s\n", f ? f : "(null)");

    return EXIT_SUCCESS;
}

#endif

/* END OF FILE */
