#
# $Id: Makefile.am 209 2011-07-11 19:26:56Z mmcternan $
#

bin_PROGRAMS = ripright riparrange
ripright_SOURCES = \
art.c   eject.c  encodetask.c  log.c         rip.c       curlfetch.c \
art.h   eject.h  encodetask.h  log.h         rip.h       curlfetch.h \
bbuf.c  enc.c    format.c      ripright.c    xmlparse.c  mblookup.c \
bbuf.h  enc.h    format.h      ripright.h    xmlparse.h  mblookup.h \
x_mem.c \
x_mem.h

ripright_CFLAGS = -Wall -Wextra -std=gnu99 -O2 $(flac_CFLAGS) $(MagickWand_CFLAGS) $(libcurl_CFLAGS) $(libdiscid_CFLAGS)
ripright_LDADD = $(flac_LIBS) $(MagickWand_LIBS) $(libcurl_LIBS) $(libdiscid_LIBS) -lpthread


riparrange_SOURCES = \
riparrange.c \
format.c  x_mem.c \
format.h  x_mem.h

riparrange_CFLAGS = -Wall -Wextra -std=gnu99 -O2 $(flac_CFLAGS)
riparrange_LDADD = $(flac_LIBS)

# END OF FILE
