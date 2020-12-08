/***************************************************************************
 * ripright.c: CD ripper, done right.
 * Copyright (C) 2013-2015 Michael C McTernan, mike@mcternan.uk
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
#include <discid/discid.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "encodetask.h"
#include "ripright.h"
#include "mblookup.h"
#include "format.h"
#include "eject.h"
#include "x_mem.h"
#include "bbuf.h"
#include "enc.h"
#include "art.h"
#include "rip.h"
#include "log.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

#define M_ArraySize(a) (sizeof(a) / sizeof(a[1]))

/**************************************************************************
 * Types
 **************************************************************************/

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
    { "Other",       "Other" },        /* Default */
    { "EP",          "EPs" },
    { "Album",       "Albums" },
    { "Single",      "Singles" },
    { "Soundtrack",  "Soundtracks" },
    { "Spokenword",  "Spokenword" },
    { "Interview",   "Interviews" },
    { "Audiobook",   "Audiobooks" },
    { "Live",        "Live" },
    { "Remix",       "Remixes" },
    { "Compilation", "Compilations" }
};

/** If set, ripping will be aborted if the cover art cannot be retrieved. */
static bool gNeedArt = false;

/** If non-NULL, filename root into which artwork will additionally be saved. */
static char *gFolderArt = NULL;

/** If set, rip CD under all names. */
static bool gRipAsAll = false;

/** Run in the background as a daemon. */
static bool gDaemon = false;

/** The format string for the output filenames. */
static char *gFilenameFormat = "%Y/%B - %D/%N-%T.flac";

/** If set, allow skipping of bad sectors when ripping. */
bool gRipAllowSkip = false;

/** execute external script after completion */
static char *gExecAfterComplPath = "";

/** The CD-ROM device used for reading. */
const char *gCdromDevice = "/dev/cdrom";

/**************************************************************************
 * Local Functions
 **************************************************************************/

static bool cdromDevIsReadable(void)
{
    int fd;

    fd = open(gCdromDevice, O_RDONLY);

    if(fd == -1)
    {
        if(errno != ENOMEDIUM)
        {
            LogErr("Failed to open %s: %s\n", gCdromDevice, strerror(errno));
            return false;
        }
    }
    else
    {
        close(fd);
    }

    return true;
}

static int doRip(void)
{
    DiscId         *disc = discid_new();
    mbresult_t      mbresult;
    bbuf_t          encTaskBBuf;

    // track logging
    bool    logTracks = false;
    char    trackLogName[42]; // sufficient, since musicbrainz disc-ids are 28 chars long

    /* check if we must log the tracks to a log file */
    if (strlen(gExecAfterComplPath) > 0) {
        logTracks = true;
        LogInf("Tracklog enabled\n");
    }

	/* Ensure we have a structure */
    if(disc == NULL)
    {
        LogErr("Failed to allocate for a disc ID: skipping rip\n");
        return EXIT_FAILURE;
    }

    /* Create a bounded buffer */
    encTaskBBuf = BBufNew(16);

    /* Create encoder threads */
    for(uint32_t c = sysconf(_SC_NPROCESSORS_ONLN); c > 0; c--)
    {
        EncNew(encTaskBBuf);
    }

    LogInf("Waiting for a CD (%s)\n", gCdromDevice);

    /* Poll until a CD is found */
    while(!discid_read(disc, gCdromDevice))
    {
        sleep(3);
    }

    /* Get the discId */
    const char *discId = discid_get_id(disc);

    LogInf("Got disk Id %s\n", discId);

    /* set tracklog filename */
    int res;
    res = snprintf(trackLogName, 42, "./%s.tracklog", discId);
    if(res < 0 || res > 42) {
        LogErr("trackLogName could not be set. Call snprintf returned %d.\n", res);
        return EXIT_FAILURE;
    }

    if(!MbLookup(discId, &mbresult))
    {
        LogErr("No result for discid=%s: Skipping rip\n"
               "Please submit this disc to Musicbrainz: %s\n",
               discId, discid_get_submission_url(disc));
        Eject(gCdromDevice);
        MbFree(&mbresult);
        return EXIT_FAILURE;

    }
    else if(mbresult.releaseCount != 1 && !gRipAsAll)
    {
        LogErr("No unique result for disc: results=%u (discid=%s): skipping rip\n",
               mbresult.releaseCount, discId);

        MbPrint(&mbresult);
        MbFree(&mbresult);

        Eject(gCdromDevice);
        return EXIT_FAILURE;
    }
    else if(mbresult.releaseCount == 0)
    {
        LogErr("Only a CD Stub for discid=%s: Skipping rip\n"
               "Please submit this disc to Musicbrainz: %s\n",
               discId, discid_get_submission_url(disc));
        Eject(gCdromDevice);
        MbFree(&mbresult);
        return EXIT_FAILURE;
    }

    art_t *coverArt = x_calloc(sizeof(art_t), mbresult.releaseCount);
    bool   noArt = true;

    for(int32_t i = 0; i < mbresult.releaseCount; i++)
    {
        coverArt[i] = ArtGet(mbresult.release[i].asin);
        if(coverArt[i] != NULL)
        {
            noArt = false;
        }
    }

    if(gNeedArt && noArt)
    {
        LogWarn("Warning: No cover art found for disk %s: skipping rip\n", discId);
    }
    else
    {
        rip_t   *ripper;
        uint8_t  nChannels;
        uint64_t totalSamples;
        uint16_t cdTrack, cdTrackCount;

        FILE *trackLogfp = NULL;

        /* if needed, open tracklog */
        if (logTracks) {
            trackLogfp = fopen(trackLogName, "w");
            if (trackLogfp == NULL) {
                LogErr("Could not open %s!\n", trackLogName);
                return EXIT_FAILURE;
            }
        }

        /* Create the ripper and get the count of tracks on the CD */
        ripper = RipNew(gCdromDevice);
        cdTrackCount = RipGetTrackCount(ripper);

        /* Process each track in turn */
        for(cdTrack = 0; cdTrack < cdTrackCount; cdTrack++)
        {
            char  tempFile[] = "/tmp/rrXXXXXX";
            FILE *out;

            out = fdopen(mkstemp(tempFile), "wb");
            if(out == NULL)
            {
                LogErr("Error: Failed to open temporary file: %s\n", tempFile);
                exit(EXIT_FAILURE);
            }

            /* Rip the track (counting from track '1') */
            RipTrack(ripper, cdTrack + 1, out, &nChannels, &totalSamples);
            fclose(out);

            /* Process the results in turn */
            for(int32_t i = 0; i < mbresult.releaseCount; i++)
            {
                const mbrelease_t *release = &mbresult.release[i];
                char               albumTitle[1024];
                const char        *albumType = validReleaseTypes[0].path;
                char              *outputPrefix;

                /* Print a note if ripping multiple times */
                if(mbresult.releaseCount != 1 && gRipAsAll)
                {
                    static char prefixBuf[64];

                    LogWarn("Rip-to-all specified, encoding as result %u/%u\n",
                            i + 1, mbresult.releaseCount);

                    snprintf(prefixBuf, sizeof(prefixBuf), "Ambiguous/%s/", release->releaseId);
                    outputPrefix = prefixBuf;
                }
                else
                {
                    outputPrefix = NULL;
                }

                /* Construct the album title */
                if(release->medium.title)
                {
                    if(release->discTotal == 1)
                    {
                        snprintf(albumTitle, sizeof(albumTitle), "%s (%s)",
                                release->albumTitle, release->medium.title);
                    }
                    else
                    {
                        snprintf(albumTitle, sizeof(albumTitle), "%s (disc %" PRIu16 ": %s)",
                                release->albumTitle, release->medium.discNum, release->medium.title);
                    }
                }
                else if(release->discTotal == 1)
                {
                    snprintf(albumTitle, sizeof(albumTitle), "%s",
                            release->albumTitle);
                }
                else
                {
                    snprintf(albumTitle, sizeof(albumTitle), "%s (disc %" PRIu16 ")",
                            release->albumTitle, release->medium.discNum);
                }

                /* Check if we have a release type */
                if(release->releaseType)
                {
                    /* Check which valid type matches */
                    for(uint8_t vt = 0; vt < M_ArraySize(validReleaseTypes); vt++)
                    {
                        if(strcmp(validReleaseTypes[vt].key, release->releaseType) == 0)
                        {
                            albumType = validReleaseTypes[vt].path;
                        }
                    }
                }

                /* Log some information about the CD */
                if(release->albumArtist.artistName)
                {
                    LogInf("     Artist: %s\n", release->albumArtist.artistName);
                }
                LogInf("      Album: %s\n", albumTitle);
                LogInf("     Tracks: %u\n", release->medium.trackCount);

                if(coverArt[i] == NULL && gNeedArt)
                {
                    LogWarn("Warning: No cover art found: skipping\n");
                }
                else if(cdTrack < release->medium.trackCount)
                {
                    const mbtrack_t *track = &release->medium.track[cdTrack];
                    encodetask_t    *etask;

                    /* Allocate the encoding task */
                    etask = EncTaskNew(tempFile, nChannels, totalSamples);

                    if(coverArt[i])
                    {
                        EncTaskSetArt(etask, coverArt[i]);
                    }

                    etask->trackNum = cdTrack + 1;
                    etask->bitsPerSample = 16;
                    etask->sampleRateHz = 44100;

                    /* Add tags */
                    EncTaskAddTag(etask, "TRACKNUMBER=%" PRIu32 "/%" PRIu32,
                                cdTrack + 1, release->medium.trackCount);
                    EncTaskAddTag(etask, "DISCNUMBER=%" PRIu32 "/%" PRIu32,
                                release->medium.discNum, release->discTotal);

                    EncTaskAddTag(etask, "TITLE=%s", track->trackName);
                    if(release->asin)
                    {
                        EncTaskAddTag(etask, "ASIN=%s", release->asin);
                    }

                    EncTaskAddTag(etask, "ALBUM=%s", albumTitle);

                    if(track->trackId)
                    {
                        EncTaskAddTag(etask, "MUSICBRAINZ_TRACKID=%s", track->trackId);
                    }

                    if(release->releaseGroupId)
                    {
                        EncTaskAddTag(etask, "MUSICBRAINZ_ALBUMID=%s", release->releaseGroupId);
                    }

                    EncTaskAddTag(etask, "MUSICBRAINZ_DISCID=%s", discId);

                    /* Check if we have a release type */
                    if(release->releaseType)
                    {
                        EncTaskAddTag(etask, "MUSICBRAINZ_TYPE=%s", release->releaseType);

                        if(strcmp("Compilation", release->releaseType) == 0)
                        {
                            EncTaskAddTag(etask, "COMPILATION=1");
                        }
                    }

                    if(track->trackArtist.artistName)
                    {
                        EncTaskAddTag(etask, "ARTIST=%s", track->trackArtist.artistName);
                        EncTaskAddTag(etask, "ARTISTSORT=%s", track->trackArtist.artistNameSort);

                        for(uint8_t a = 0; a < track->trackArtist.artistIdCount; a++)
                        {
                            EncTaskAddTag(etask, "MUSICBRAINZ_ARTISTID=%s", track->trackArtist.artistId[a]);
                        }

                        if(release->albumArtist.artistName)
                        {
                            EncTaskAddTag(etask, "ALBUMARTIST=%s", release->albumArtist.artistName);
                            EncTaskAddTag(etask, "ALBUMARTISTSORT=%s", release->albumArtist.artistNameSort);

                            for(uint8_t a = 0; a < release->albumArtist.artistIdCount; a++)
                            {
                                EncTaskAddTag(etask, "MUSICBRAINZ_ALBUMARTISTID=%s", release->albumArtist.artistId[a]);
                            }
                        }
                    }
                    else if(release->albumArtist.artistName)
                    {
                        EncTaskAddTag(etask, "ARTIST=%s", release->albumArtist.artistName);
                        EncTaskAddTag(etask, "ARTISTSORT=%s", release->albumArtist.artistNameSort);
                        EncTaskAddTag(etask, "ALBUMARTIST=%s", release->albumArtist.artistName);
                        EncTaskAddTag(etask, "ALBUMARTISTSORT=%s", release->albumArtist.artistNameSort);

                        for(uint8_t a = 0; a < release->albumArtist.artistIdCount; a++)
                        {
                            EncTaskAddTag(etask, "MUSICBRAINZ_ARTISTID=%s", release->albumArtist.artistId[a]);
                            EncTaskAddTag(etask, "MUSICBRAINZ_ALBUMARTISTID=%s", release->albumArtist.artistId[a]);
                        }
                    }

                    /* Escape the trackname and artist*/
                    char *fileName = Format(outputPrefix,
                                            gFilenameFormat,
                                            cdTrack + 1,
                                            track->trackArtist.artistName,
                                            track->trackArtist.artistNameSort,
                                            release->albumArtist.artistName,
                                            release->albumArtist.artistNameSort,
                                            albumTitle,
                                            track->trackName,
                                            albumType);

                    /* log fileName to tracklog */
                    if (logTracks) {
                        int res = -1;
                        res = fprintf(trackLogfp, "%s\n", fileName);
                        if (res < 0) {
                            LogWarn("Could not print to tracklog!\n");
                        } else {
                            LogInf("tracklog new entry: %s\n", fileName);
                        }
                    }

                    EncTaskSetOutputFilename(etask, fileName);


                    EncTaskPrint(etask, stdout);

                    /* Add to the encoding queue */
                    BBufPut(encTaskBBuf, etask);

                    /* Check if the art file should be saved to the output directory */
                    if(gFolderArt && !noArt && coverArt[i] != NULL)
                    {
                        char        *end = &fileName[strlen(fileName)];
                        char         artName[strlen(fileName) + strlen(gFolderArt) + 1];
                        struct stat  sb;

                        /* Find last '/' in the string */
                        while(end >= fileName && *end != '/')
                        {
                            end--;
                        }

                        if(end >= fileName)
                        {
                            *end = '\0';
                        }

                        /* Format the filename for the cover art */
                        snprintf(artName, sizeof(artName), "%s/%s", fileName, gFolderArt);

                        /* Output the cover art if the file doesn't already exist */
                        if(stat(artName, &sb))
                        {
                            if(ArtDumpToFile(coverArt[i], artName))
                            {
                                LogInf("Cover art saved to %s\n", artName);
                            }
                            else
                            {
                                LogErr("Failed to save cover art to %s\n", artName);
                            }
                        }
                    }

                    free(fileName);
                }
            }

            /* Remove the raw file */
            unlink(tempFile);
        }

        RipFree(ripper);

        /* close tracklog */
        if (logTracks) {
            fclose(trackLogfp);
        }
    }

    /* Free any art */
    for(int32_t i = 0; i < mbresult.releaseCount; i++)
    {
        if(coverArt[i] != NULL)
        {
            ArtFree(coverArt[i]);
        }
    }

    free(coverArt);

    MbFree(&mbresult);

    /* Try to eject the CD
     *  If this fails, keep trying, otherwise we might end up trying to
     *  re-rip the CD.
     */
    while(!Eject(gCdromDevice))
    {
        LogErr("Error: Failed to eject CD: retrying in 3 seconds");
        sleep(3);
    }

    /* Create encoder threads */
    for(uint32_t c = sysconf(_SC_NPROCESSORS_ONLN); c > 0; c--)
    {
        BBufPut(encTaskBBuf, NULL);
    }

    BBufWaitUntilEmpty(encTaskBBuf);

    /* call external script desired */
    if (logTracks) {
        char execcmd[1024];
        int n;
        n = snprintf(execcmd, 1024, "%s %s >> ./out.log", gExecAfterComplPath, trackLogName);
        if (n < 0 || n > 1024) {
            LogErr("Could not execute user-defined command. Note that a maximum of 1024 chars in the command are supported!\n");
        }
        n = system(execcmd);
        LogInf("User-defined script returned %d.\n", n);
    }

    return EXIT_SUCCESS;
}


static void usage(void)
{
    printf("Usage: ripright [-d] [-a] [-r] [-s] [-e exec-script] [-c device] [-o format] [outpath]\n"
           "\n"
           "Where:\n"
           "  -d, --daemon\n"
           "     Run in the background, detached from the controlling terminal.\n"
           "\n"
           "  -r, --require-art\n"
           "     Refuse to rip a CD if the cover art cannot be retrieved.\n"
           "\n"
           "  -f <file>, --folder-art <file>\n"
           "     Save cover art (if available) to <file>, relative to the output\n"
           "     directory.  The art file will be converted to the format specified\n"
           "     by the filename e.g. -f folder.jpg or -f folder.gif or -f folder.png\n"
           "\n"
           "  -w, --w32-filenames\n"
           "     Covert characters that are illegal on Windows filesystems to\n"
           "     UTF-8 alternatives.  If accessing files over Samba, this avoids\n"
           "     name mangling which can lose the file extension.  Mapped characters\n"
           "     are *, ?, \" and |  which are replaced by \xD3\xBF, \xCA\x94, \xC2\xA8 and \xC7\x80 respectively.\n"
           "\n"
           "  -s, --allow-skips\n"
           "     Normally ripping attempts to make a perfect copy of a CD by using\n"
           "     all data verification and correction features of the cdparanoia\n"
           "     library.  With this option, each bad sectors will be skipped after\n"
           "     20 failed attempts to read the data.  Without this option, ripping\n"
           "     of scratched or damaged CDs may take a very long time and possibly\n"
           "     may not complete.\n"
           "\n"
           "  -a, --rip-to-all\n"
           "     Normally exactly 1 result is required from Musicbrainz,\n"
           "     otherwise the CD will be refused.  With this option, the CD will\n"
           "     be ripped and tagged to multiple files, once as each result from\n"
           "     Musicbrainz.  Each rip will be output under a directory named\n"
           "     Ambiguous/<mb-release-id>/ where the the Musicbrainz release Id\n"
           "     is used to identify each possible release.\n"
           "\n"
           "  -c, --cd-device\n"
           "     Path to the CD-ROM device to use.  This defaults to /dev/cdrom if\n"
           "     not otherwise specified.\n"
           "\n"
           "  -o, --output-file\n"
           "     Set the format used to produce output filenames and paths.  This\n"
           "     should be a string containing the following special tokens:\n"
           "\n"
           "       %%N   = track number\n"
           "       %%A   = Track artist\n"
           "       %%a   = Track artist sort name\n"
           "       %%B   = Album artist\n"
           "       %%b   = Album artist sort name\n"
           "       %%C   = Track artist if present, else album artist\n"
           "       %%c   = Track artist sort name if present, else album artist\n"
           "                sort name\n"
           "       %%D   = Album/CD name\n"
           "       %%T   = Trackname\n"
           "       %%Y   = Release type (album, single, EP, compilation etc...)\n"
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
           "  -e, --exec-after\n"
           "     After conversion completed, execute the given command in a system\n"
           "     shell and pass the path of a log file as first argument and log the\n"
           "     output to out.log. All files are created in the directory where\n"
           "     ripright was started from.\n"
           "\n"
           "     Example:\n"
           "       -e \"python3 email_owner.py\"\n"
           "     calls\n"
           "       python3 email_owner.py <discid>.tracklog >> out.log\n"
           "     where <discid>.tracklog contains paths to each converted track.\n"
           "\n"
           "     To see an example of <discid>.tracklog, run ripright with the argument\n"
           "       -e cat\n"
           "     and investigate out.log.\n"
           "\n"           
           "  outpath\n"
           "     If supplied, write ripped CDs to this directory.\n"
           "\n"
           "  RipRight " VERSION " (c) 2011 Michael McTernan\n"
           "                         mike@mcternan.uk\n"
           "  Eject routines (c) 1994-2005 Jeff Tranter (tranter@pobox.com)\n"
           "\n",
           gFilenameFormat);
}

/**************************************************************************
 * Global Functions
 **************************************************************************/

int main(int argc, char *argv[])
{
    while(argc > 1)
    {
        if(strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--require-art") == 0)
        {
            gNeedArt = true;
            argc--;
            argv++;
        }
        else if(strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "--daemon") == 0)
        {
            gDaemon = true;
            argc--;
            argv++;
        }
        else if(strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "--rip-to-all") == 0)
        {
            gRipAsAll = true;
            argc--;
            argv++;
        }
        else if(argc > 2 && (strcmp(argv[1], "-f") == 0 || strcmp(argv[1], "--folder-art") == 0))
        {
            gFolderArt = argv[2];
            argc -= 2;
            argv += 2;
        }
        else if(argc > 1 && (strcmp(argv[1], "-w") == 0 || strcmp(argv[1], "--w32-filenames") == 0))
        {
            gWin32Escapes = true;
            argc--; argv++;
        }
        else if(strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--allow-skips") == 0)
        {
            gRipAllowSkip = true;
            argc--;
            argv++;
        }
        else if((strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--cd-device") == 0) &&
                argc > 2)
        {
            gCdromDevice = argv[2];
            argc -= 2;
            argv += 2;
        }
        else if((strcmp(argv[1], "-o") == 0 || strcmp(argv[1], "--output-file") == 0) &&
                argc > 2)
        {
            gFilenameFormat = argv[2];
            argc -= 2;
            argv += 2;
        }
        else if((strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--exec-after") == 0) &&
                argc > 2)
        {
            gExecAfterComplPath = argv[2];
            argc -= 2;
            argv += 2;
        }
        else if(argc == 2 && *argv[1] != '-')
        {
            if(chdir(argv[1]) != 0)
            {
                fprintf(stderr, "Error: Failed to access output directory: %m\n");
                return EXIT_FAILURE;
            }
            argc--;
            argv++;
        }
        else
        {
            usage();
            return EXIT_FAILURE;
        }
    }

    LogInit();

    /* Check the output filename format is okay */
    if(!FormatIsValid(gFilenameFormat))
    {
        return EXIT_FAILURE;
    }

    /* Check the CD-ROM device can be opened for read */
    if(!cdromDevIsReadable())
    {
        return EXIT_FAILURE;
    }

    /* Daemonise if requested to do so */
    if(gDaemon)
    {
        if(daemon(1, 0) == 0)
        {
            LogInf("Started daemon mode (v" VERSION ")\n");
        }
        else
        {
            LogWarn("Started daemon mode failed: %m\n");
        }
    }

    /* We spawn a process per CD to rip.
     *  This shouldn't be required, but it ensures 2 things:
     *   1) Memory leaks don't accumulate.
     *   2) A crashed CD rip doesn't hose the ripright.
     *
     * Some of the sub-libraries appear to leak memory so point 1 is
     *  particularly pertinent.
     */
    while(1)
    {
        pid_t p = fork();

        if(p == 0)
        {
            /* Child */
            return doRip();
        }

        /* Wait for the child */
        waitpid(p, NULL, 0);
    }

    return EXIT_SUCCESS;
}

/* END OF FILE */
