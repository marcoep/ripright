/***************************************************************************
 * x_mem.c: Wrapper memory functions which exit.
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

#include <string.h>
#include <stdio.h>
#define X_MEM_NO_POISON
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

/** Malloc that will exit instead of returning NULL.
 */
void *x_malloc(size_t size)
{
    void *r = malloc(size);

    if(!r)
    {
        fprintf(stderr, "Fatal: malloc(%zu) failed: %m\n", size);
        exit(EXIT_FAILURE);
    }

    return r;
}


/** Malloc zero'd memory which will exit instead of returning NULL.
 */
void *x_zalloc(size_t size)
{
    return calloc(1, size);
}


/** Calloc which will exit instead of returning NULL.
 */
void *x_calloc(size_t nmemb, size_t size)
{
    void *r = calloc(nmemb, size);

    if(!r)
    {
        fprintf(stderr, "Fatal: calloc(%zu, %zu) failed: %m\n", nmemb, size);
        exit(EXIT_FAILURE);
    }

    return r;
}


/** Safe realloc that will exit instead of returning NULL.
 */
void *x_realloc(void *ptr, size_t size)
{
    void *r = realloc(ptr, size);

    if(!r)
    {
        fprintf(stderr, "Fatal: realloc(%p, %zu) failed: %m\n", ptr, size);
        exit(EXIT_FAILURE);
    }

    return r;
}


/** Safe strdup that will exit instead of returning NULL.
 */
char *x_strdup(const char *s)
{
    char *r = strdup(s);

    if(!r)
    {
        fprintf(stderr, "Fatal: strdup() failed: %m\n");
        exit(EXIT_FAILURE);
    }

    return r;
}

/* END OF FILE */
