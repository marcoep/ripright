/***************************************************************************
 * log.c: Logging functions for RipRight.
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

#include <syslog.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
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
 * Local Functions
 **************************************************************************/

/**************************************************************************
 * Global Functions
 **************************************************************************/

void LogInit(void)
{
    openlog("ripright", 0, LOG_DAEMON);
}


void LogErr(const char *format, ...)
{
    va_list ap, aq;

    va_start(ap, format);
    va_copy(aq, ap);

    vsyslog(LOG_USER | LOG_ERR, format, ap);
    va_end(ap);

    vfprintf(stdout, format, aq);
    va_end(aq);
}


void LogWarn(const char *format, ...)
{
    va_list ap, aq;

    va_start(ap, format);
    va_copy(aq, ap);

    vsyslog(LOG_USER | LOG_WARNING, format, ap);
    va_end(ap);

    vfprintf(stdout, format, aq);
    va_end(aq);
}


void LogInf(const char *format, ...)
{
    va_list ap, aq;

    va_start(ap, format);
    va_copy(aq, ap);

    vsyslog(LOG_USER | LOG_INFO, format, ap);
    va_end(ap);

    vfprintf(stdout, format, aq);
    va_end(aq);
}

/* END OF FILE */
