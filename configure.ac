#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(ripright, 0.12, mail@mebg.ch)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE([disable])

# Checks for programs.
AC_PROG_CC
AC_PROG_SED
PKG_PROG_PKG_CONFIG

# Checks for libraries.
PKG_CHECK_MODULES([flac], [flac >= 1.1.4])
PKG_CHECK_MODULES([MagickWand], [Wand])
PKG_CHECK_MODULES([libcurl], [libcurl])
PKG_CHECK_MODULES([libdiscid], [libdiscid])
AC_CHECK_LIB([cdda_paranoia], [paranoia_init])
AC_CHECK_LIB([cdda_interface], [cdda_open])
AC_CHECK_LIB([pthread], [pthread_create])

# Make sure the FLAC_CFLAGS value is sane, strip trailing '/FLAC'.
flac_CFLAGS=`echo $flac_CFLAGS | $SED "s/FLAC$//"`

# Check cdda-paranoia include location
AC_CHECK_HEADERS([cdda_interface.h],, 
                 [AC_CHECK_HEADERS([cdda/cdda_interface.h], [], AC_MSG_ERROR([cannot find cdda_interface.h.  Please ensure the cdparanoia development package is installed or set CPPFLAGS and re-run configure]))])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h stdint.h stdlib.h string.h sys/ioctl.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memmove memset mkdir strchr strdup strerror strncasecmp strstr])

AC_CONFIG_FILES([Makefile
                 man/Makefile
                 src/Makefile
                 test/Makefile])
AC_OUTPUT
