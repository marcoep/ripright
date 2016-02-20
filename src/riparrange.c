/***************************************************************************
 * riparrange.c: Program to re-arrange FLAC files based on tags.
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
#include <FLAC/stream_decoder.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include "format.h"
#include "x_mem.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

#define M_ArraySize(a)  ((sizeof(a)) / (sizeof(a[1])))

/**************************************************************************
 * Types
 **************************************************************************/

typedef enum
{
    OP_MODE_DRY_RUN,
    OP_MODE_TOUCH,
    OP_MODE_HARD_LINK,
    OP_MODE_COPY,
    OP_MODE_MOVE
}
op_mode_t;


typedef struct
{
    const char *file;
    bool        error;

    uint16_t    trackNumber;
    char       *artist;
    char       *artistSort;
    char       *albumArtist;
    char       *albumArtistSort;
    char       *albumName;
    char       *trackName;
    char       *releaseType;
}
flac_info_t;

/**************************************************************************
 * Local Variables
 **************************************************************************/

/** Map of musicbrainz release types to output sud-directory names.
 * \see http://musicbrainz.org/doc/Release_Attribute
 */
static const struct
{
    char *key, *path;
}
validReleaseTypes[] =
{
    { "=Other",       "Other" },        /* Default */
    { "=EP",          "EPs" },
    { "=Album",       "Albums" },
    { "=Single",      "Singles" },
    { "=Soundtrack",  "Soundtracks" },
    { "=Spokenword",  "Spokenword" },
    { "=Interview",   "Interviews" },
    { "=Audiobook",   "Audiobooks" },
    { "=Live",        "Live" },
    { "=Remix",       "Remixes" },
    { "=Compilation", "Compilations" }
};

/** The format string for the output filenames. */
static char *gFilenameFormat = "%Y/%B - %D/%N-%T.flac";

/** Operation mode. */
static op_mode_t gOpMode = OP_MODE_DRY_RUN;

/** Remove the destination if it already exists. */
static bool gRemoveDest = false;

/** Request verbose output. */
static bool gVerbose = false;

/**************************************************************************
 * Local Functions
 **************************************************************************/

static bool endsWith(const char *postfix, const char *string)
{
    size_t postfixLen = strlen(postfix);
    size_t stringLen = strlen(string);

    return stringLen >= postfixLen && strcmp(&string[stringLen - postfixLen], postfix) == 0;
}


static int copy(const char *oldpath, char *newpath)
{
    FILE *in, *out;

    if(!(in = fopen(oldpath, "rb")) || !(out = fopen(newpath, "wb")))
    {
        return -1;
    }

    do
    {
        char   buf[4096];
        size_t s;

        s = fread(buf, 1, sizeof(buf), in);
        if(fwrite(buf, s, 1, out) != 1)
        {
            int ec = errno;

            fclose(in);
            fclose(out);

            errno = ec;
            return -1;
        }
    }
    while(!feof(in));

    return 0;
}


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
            fprintf(stderr, "Warning: Could not create '%s': %m\n", pathFilename);
        }

        *slash = '/';
        slash++;
    }
}


static void addMetaComment(flac_info_t *fi, const FLAC__StreamMetadata_VorbisComment *info)
{
    const struct {
        const char  *tag;
        char       **valueStore;
    }
    tagTable[] = {
        { "ARTIST=",           &fi->artist      },
        { "ARTISTSORT=",       &fi->artistSort  },
        { "ALBUMARTIST=",      &fi->albumArtist },
        { "ALBUMARTISTSORT=",  &fi->albumArtistSort },
        { "ALBUM=",            &fi->albumName   },
        { "TITLE=",            &fi->trackName   }
    };

    /* Check each comment in turn */
    for(uint32_t c = 0; c < info->num_comments; c++)
    {
        char *comment =  (char *)info->comments[c].entry;

        if(strncmp("TRACKNUMBER=", comment, 12) == 0)
        {
            sscanf(&comment[12], "%" SCNu16, &fi->trackNumber);
        }
        else if(strncmp("MUSICBRAINZ_TYPE=", comment, 17) == 0)
        {
            for(uint32_t type = 0; type < M_ArraySize(validReleaseTypes); type++)
            {
                if(endsWith(validReleaseTypes[type].key, comment))
                {
                    if(fi->releaseType) free(fi->releaseType);

                    fi->releaseType = x_strdup(validReleaseTypes[type].path);
                }
            }
        }
        else
        {
            for(uint32_t tag = 0; tag < M_ArraySize(tagTable); tag++)
            {
                uint32_t  tagLen = strlen(tagTable[tag].tag);

                if(strncmp(tagTable[tag].tag, comment, tagLen) == 0)
                {
                    *tagTable[tag].valueStore = x_strdup(&comment[tagLen]);
                }
            }
        }
    }
}

static void callbackMetadata(const FLAC__StreamDecoder  *decoder __attribute__((unused)),
                             const FLAC__StreamMetadata *metadata,
                             void                       *data)
{
    switch(metadata->type)
    {
        case FLAC__METADATA_TYPE_STREAMINFO:
            break;
        case FLAC__METADATA_TYPE_VORBIS_COMMENT:
            addMetaComment((flac_info_t *)data, &metadata->data.vorbis_comment);
            break;
        case FLAC__METADATA_TYPE_PICTURE:
            break;
        case FLAC__METADATA_TYPE_SEEKTABLE:
            break;
        default:
            break;
    }
}


static void callbackError(const FLAC__StreamDecoder      *decoder __attribute__((unused)),
                          FLAC__StreamDecoderErrorStatus  status,
                          void                           *data __attribute__((unused)))
{
    flac_info_t *fi = (flac_info_t *)data;

    fi->error = true;

    fprintf(stderr, "Error: Failed to process file '%s': %s\n",
            fi->file,
            FLAC__StreamDecoderErrorStatusString[status]);
}

FLAC__StreamDecoderWriteStatus callbackWrite(const FLAC__StreamDecoder *decoder __attribute__((unused)),
                                             const FLAC__Frame         *frame __attribute__((unused)),
                                             const FLAC__int32   *const buffer[] __attribute__((unused)),
                                             void                      *data __attribute__((unused)))
{
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


static void processFile(const char *file, flac_info_t *const fi)
{
    FLAC__StreamDecoder           *sd;
    FLAC__StreamDecoderInitStatus  status;

    /* Setup the info structure */
    memset(fi, 0, sizeof(flac_info_t));
    fi->file = file;

    /* Create the decoder */
    sd = FLAC__stream_decoder_new();
    if(sd != NULL)
    {
        /* Configure the decoder */
        FLAC__stream_decoder_set_md5_checking(sd, true);
        FLAC__stream_decoder_set_metadata_respond_all(sd);

        status = FLAC__stream_decoder_init_file(sd, fi->file,
                                                callbackWrite,
                                                callbackMetadata,
                                                callbackError, fi);
        if(status != FLAC__STREAM_DECODER_INIT_STATUS_OK ||
           !FLAC__stream_decoder_process_until_end_of_metadata(sd))
        {
            fi->error = true;
        }

        FLAC__stream_decoder_delete(sd);
    }
    else
    {
        fi->error = true;
    }

    /* Check all the required data was populated */
    fi->error = fi->error || fi->trackNumber == 0;
    fi->error = fi->error || (fi->artist == NULL && fi->albumArtist == NULL);
    fi->error = fi->error || fi->albumName == NULL;
    fi->error = fi->error || fi->trackName == NULL;
    fi->error = fi->error || fi->releaseType == NULL;
}

#if 0
static void dumpInfo(flac_info_t *fi)
{
    printf("%s:\n"
           "Track:       %02" PRIu16 "\n"
           "Artist:      %s\n"
           "AlbumArtist: %s\n"
           "Album:       %s\n"
           "Track:       %s\n"
           "ReleaseType: %s\n"
           "%s",
           fi->file, fi->trackNumber, fi->artist, fi->albumArtist,
           fi->albumName, fi->trackName, fi->releaseType,
           fi->error ? "(error)\n" : "");
}
#endif

static void freeInfo(flac_info_t *fi)
{
    if(fi->artist)          free(fi->artist);
    if(fi->artistSort)      free(fi->artistSort);
    if(fi->albumArtist)     free(fi->albumArtist);
    if(fi->albumArtistSort) free(fi->albumArtistSort);
    if(fi->albumName)       free(fi->albumName);
    if(fi->trackName)       free(fi->trackName);
    if(fi->releaseType)     free(fi->releaseType);
}


static void usage(void)
{
    printf("Usage: riparrange [-m|-c|-h] [-f] [-v] [-w] [-o format] <file.flac> ...\n"
           "\n"
           "Where:\n"
           "  -m\n"
           "     Move files to new location.  This only works within a mount.\n"
           "  -c\n"
           "     Copy files to new location.\n"
           "  -h\n"
           "     Hard link files to new location.  This only works within a filesystem.\n"
           "  -w, --w32-filenames\n"
           "     Covert characters that are illegal on Windows filesystems to\n"
           "     UTF-8 alternatives.  If accessing files over Samba, this avoids\n"
           "     name mangling which can lose the file extension.  Mapped characters\n"
           "     are *, ?, \" and |  which are replaced by \xD3\xBF, \xCA\x94, \xC2\xA8 and \xC7\x80 respectively.\n"
           "  -f, --force\n"
           "     Remove existing destination files before operation.\n"
           "  -v, --verbose\n"
           "     Output information about files which will not be moved because they are\n"
           "     already in the correct location.  Normally only things that would be\n"
           "     changed are reported.\n"
           "  -o, --output-file <format>\n"
           "     Set the format used to produce output filenames and paths.  This\n"
           "     should be a string containing the following special tokens:\n"
           "\n"
           "       %%N   = Track number (TRACKNUMBER)\n"
           "       %%A   = Track artist (ARTIST)\n"
           "       %%A   = Track artist sort name (ARTISTSORT)\n"
           "       %%B   = Album artist (ALBUMARTIST)\n"
           "       %%b   = Album artist sort name (ALBUMARTISTSORT)\n"
           "       %%C   = Track artist if present, else album artist\n"
           "       %%c   = Track artist sort name if present, else album artist\n"
           "                sort name\n"
           "       %%D   = Album/CD name (ALBUM)\n"
           "       %%T   = Trackname (TITLE)\n"
           "       %%Y   = Release type (MUSICBRAINZ_TYPE)\n"
           "       %%%%   = A single percent sign\n"
           "\n"
           "     The default output string is \"%s\".\n"
           "\n"
           "     The release type is one of Albums, Audiobooks, Compilations, EPs,\n"
           "     Interviews, Live, Remixes, Singles, Soundtracks, Spokenword and\n"
           "     Other.\n"
           "\n"
           "     Slashes and colons in the '%%' output fields are converted to UTF-8\n"
           "     equivalents to avoid creating subdirectories or causing problems\n"
           "     with Windows shares mounted via Samba.  Slashes and colons given in\n"
           "     the format string will be literally preserved.\n"
           "\n"
           "\n"
           "RipArrange is a tool for automatically moving flac files to a directory\n"
           "structure based upon Vorbis Comment tags.  A format string is used to\n"
           "define how the comment tags are mapped into a filename and path.\n"
           "\n"
           "The default operation is to just print what would be changed if the tool\n"
           "were to be ran with -m, -c or -h specified.\n"
           "\n"
           "  RipArrange " VERSION " (c) Michael McTernan\n"
           "                      mike@mcternan.uk\n"
           "\n",
           gFilenameFormat);
}

/**************************************************************************
 * Global Functions
 **************************************************************************/

int main(int argc, char *argv[])
{
    bool done = false;

    while(!done)
    {
        if(argc > 1 && (strcmp(argv[1], "-f") == 0 || strcmp(argv[1], "--force") == 0))
        {
            gRemoveDest = true;
            argc--; argv++;
        }
        else if(argc > 1 && strcmp(argv[1], "-t") == 0)
        {
            gOpMode = OP_MODE_TOUCH;
            argc--; argv++;
        }
        else if(argc > 1 && strcmp(argv[1], "-h") == 0)
        {
            gOpMode = OP_MODE_HARD_LINK;
            argc--; argv++;
        }
        else if(argc > 1 && strcmp(argv[1], "-c") == 0)
        {
            gOpMode = OP_MODE_COPY;
            argc--; argv++;
        }
        else if(argc > 1 && strcmp(argv[1], "-m") == 0)
        {
            gOpMode = OP_MODE_MOVE;
            argc--; argv++;
        }
        else if(argc > 1 && (strcmp(argv[1], "-w") == 0 || strcmp(argv[1], "--w32-filenames") == 0))
        {
            gWin32Escapes = true;
            argc--; argv++;
        }
        else if(argc > 1 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--verbose") == 0))
        {
            gVerbose = true;
            argc--; argv++;
        }
        else if(argc > 2 && (strcmp(argv[1], "-o") == 0 || strcmp(argv[1], "--output-file") == 0))
        {
            gFilenameFormat = argv[2];
            argc -= 2; argv += 2;
        }
        else
        {
            done = true;
        }
    }

    if(argc == 1)
    {
        usage();
        return EXIT_FAILURE;
    }


    /* Check the output filename format is okay */
    if(!FormatIsValid(gFilenameFormat))
    {
        return EXIT_FAILURE;
    }

    while(argc > 1)
    {
        flac_info_t fi;

        processFile(argv[1], &fi);

        if(fi.error)
        {
            fprintf(stderr, "Error: Failed to process '%s'\n", fi.file);
        }
        else
        {
            bool         skip = false;
            struct stat  sbufSrc, sbufDest;

            /* Stat the source file */
            if(stat(fi.file, &sbufSrc) != 0)
            {
                fprintf(stderr, "Error: Failed to stat source file '%s': %m\n", fi.file);
            }
            else
            {
                char *outFile;

                outFile = Format(NULL,
                                 gFilenameFormat,
                                 fi.trackNumber,
                                 fi.artist,
                                 fi.artistSort,
                                 fi.albumArtist,
                                 fi.albumArtistSort,
                                 fi.albumName,
                                 fi.trackName,
                                 fi.releaseType);

                if(stat(outFile, &sbufDest) == 0)
                {
                    if(sbufSrc.st_dev == sbufDest.st_dev &&
                       sbufSrc.st_ino == sbufDest.st_ino)
                    {
                        if(gVerbose)
                        {
                            printf("%s\n  -> %s\n", fi.file, outFile);
                            printf("     (same file; skipped)\n");
                        }
                        skip = true;
                    }
                    else if(gOpMode == OP_MODE_DRY_RUN || !gRemoveDest)
                    {
                        printf("%s\n  -> %s\n", fi.file, outFile);
                        printf("     (exists; skipped)\n");
                        skip = true;
                    }
                    else
                    {
                        printf("%s\n  -> %s\n", fi.file, outFile);
                        printf("     (exists; removed)\n");
                        unlink(outFile);
                    }
                }
                else
                {
                    printf("%s\n  -> %s\n", fi.file, outFile);
                }

                if(!skip && gOpMode != OP_MODE_DRY_RUN)
                {
                    createPath(outFile);

                    switch(gOpMode)
                    {
                        case OP_MODE_TOUCH:
                        {
                            FILE *f = fopen(outFile, "w");
                            if(!f)
                            {
                                fprintf(stderr, "Error: Failed to touch '%s': %m\n", outFile);
                            }
                            fclose(f);
                            break;
                        }
                        case OP_MODE_HARD_LINK:
                            if(link(fi.file, outFile) != 0)
                            {
                                fprintf(stderr, "Error: Failed to link '%s': %m\n", outFile);
                            }
                            break;

                        case OP_MODE_MOVE:
                            if(rename(fi.file, outFile) != 0)
                            {
                                fprintf(stderr, "Error: Failed to rename '%s': %m\n", outFile);
                            }
                            break;

                        case OP_MODE_COPY:
                            if(copy(fi.file, outFile) != 0)
                            {
                                fprintf(stderr, "Error: Failed to rename '%s': %m\n", outFile);
                            }
                            break;

                        default:
                            assert(false);
                            break;
                    }
                }

                free(outFile);
            }
        }

        //dumpInfo(&fi);
        freeInfo(&fi);

        argv++;
        argc--;
    }

    return EXIT_SUCCESS;
}

/* END OF FILE */
