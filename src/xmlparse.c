/***************************************************************************
 * xmlparse.c: Minimal XML parsing library.
 * Copyright (C) 2001-2015 Michael C McTernan, mike@mcternan.uk
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "xmlparse.h"
#include "x_mem.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

#ifdef NDEBUG
  #define dprintf(s)  printf("%s:%d: %s: %s\n",__FILE__,__LINE__,__func__,s)
#else
  #define dprintf(s)
#endif

#define M_ArrayLen(a) (sizeof(a) / sizeof(a[0]))

/**************************************************************************
 * Types
 **************************************************************************/

/**************************************************************************
 * Local Variables
 **************************************************************************/

struct xmlnode {
    char *tag;
    char *content;
    char *attributes;

    void    *freeList[16];
    uint32_t freeListLen;

    bool        converted;
    int         fd;
    const char *array;
};

/**************************************************************************
 * Local Functions
 **************************************************************************/

/** Skip space characters in the passed string.
 *
 * \returns Pointer to the first non-space character in \a s, which maybe a nul.
 */
static const char *skipSpace(const char *s)
{
    while(isspace(*s))
    {
        s++;
    }

    return s;
}


static void convert(char *s)
{
    char *sIn = s;
    char *sOut = s;

    do
    {
        while(*sIn != '&' && *sIn != '\0')
        {
            *sOut = *sIn;
            sIn++;
            sOut++;
        }

        if(*sIn == '&')
        {
            static const struct {
                const char    *token;
                const uint8_t  len;
                const char    *substitution;
            }
            replacements[] =
            {
                { "&quot;",  6, "\"" },
                { "&apos;",  6, "'" },
                { "&amp;",   5, "&" },
                { "&lt;",    4, "<" },
                { "&gt;",    4, ">" }
            };

            uint8_t t;

            for(t = 0; t < M_ArrayLen(replacements); t++)
            {
                if(strncasecmp(sIn, replacements[t].token, replacements[t].len) == 0)
                {
                    break;
                }
            }

            if(t < M_ArrayLen(replacements))
            {
                sIn += replacements[t].len;
                strcpy(sOut, replacements[t].substitution);
                sOut += strlen(replacements[t].substitution);
            }
            else
            {
                *sOut = *sIn;
                sIn++;
                sOut++;
            }
        }
    }
    while(*sIn != '\0');

    *sOut = '\0';
}


/** Get another character from wherever we are reading.
 */
static int getnextchar(struct xmlnode *n,char *c)
{
    /* Reading from fd */
    if(n->fd!=-1 && n->array==NULL)
        return read(n->fd,c,sizeof(char));

    /* Reading from array */
    if(n->fd == -1 && n->array != NULL)
    {
        *c=*(n->array);
        n->array++;
        return (*c=='\0') ? 0 : 1;
    }

    return 0;
}


/** Parse to find a tag enclosure.
 */
static bool Parse(struct xmlnode *n)
{
    char c;
    int  r,l,m;
    int  tagcount;

    /* Search for a < */
    do
    {
        r = getnextchar(n,&c);
    }
    while(r == 1 && c != '<');

    if(c != '<')
    {
        return false;
    }

    n->tag = x_malloc(l = 64);
    m = 0;

    /* Read until a > */
    do
    {
        r = getnextchar(n, &n->tag[m++]);

        /* Skip space at the start of a tag eg. < tag> */
        if(m == 1 && isspace(n->tag[0]))
            m = 0;

        if(m == l)
            n->tag = x_realloc(n->tag, l += 256);

    }
    while(r == 1 && n->tag[m-1] != '>');

    if(n->tag[m-1] != '>')
    {
        dprintf("Failed to find closing '>'");
        free(n->tag);
        return false;
    }

    /* Null terminate the tag */
    n->tag[m-1] = '\0';

    /* Check if it is a <?...> or <!...> tag */
    if(*n->tag == '?' || *n->tag == '!')
    {
        dprintf("Found '<? ...>' or '<! ...>' tag");
        dprintf(n->tag);
        n->content=NULL;
        return true;
    }

    /* Check if it is a < /> tag */
    if(m >= 2 && n->tag[m - 2] == '/')
    {
        n->tag[m-2] = '\0';
        dprintf("Found '<.../>' tag");
        dprintf(n->tag);
        n->content = NULL;
        return true;
    }

    /* May need to truncate tag to remove attributes */
    r = 0;
    do
    {
        r++;
    }
    while(n->tag[r] != '\0' && !isspace(n->tag[r]));
    n->tag[r] = '\0';
    n->attributes = &n->tag[r + 1];

    dprintf("Found tag:");
    dprintf(n->tag);

    n->content = x_malloc(l = 256);
    tagcount = 1;
    m = 0;

    /* Now read until the closing tag */
    do
    {
        r = getnextchar(n,&n->content[m++]);

        if(m > 1)
        {
            /* Is there a new tag opening? */
            if(n->content[m-2]=='<')
            {
                /* Is the tag opening or closing */
                if(n->content[m-1]=='/')
                    tagcount--;
                else
                    tagcount++;
            }

            /* Catch any <blah/> style tags */
            if(n->content[m-2]=='/' && n->content[m-1]=='>')
            {
                tagcount--;
            }
        }

        if(m==l)
            n->content = x_realloc(n->content, l += 256);

    }
    while(r == 1 && tagcount > 0);

    if(r != 1)
    {
        dprintf("Failed to find closing tag");
        dprintf(n->tag);
        free(n->tag);
        free(n->content);
        return false;
    }

    /* Null terminate the content */
    n->content[m-2] = '\0';

    return true;
}

/**************************************************************************
 * Global Functions
 **************************************************************************/

/** Return some attribute value for a node in new memory.
 * This is the same as XmlGetAttribute(), but the returned string has been
 * allocated by malloc() and so must be free()'d at some point in the future.
 * \see XmlGetAttribute()
 */
char *XmlGetAttributeDup(struct xmlnode *n, const char *attr)
{
    const char *a = XmlGetAttribute(n, attr);

    if(a)
    {
        return x_strdup(a);
    }

    return NULL;
}


/** Return some attribute value for a node.
 */
const char *XmlGetAttribute(struct xmlnode *n, const char *attr)
{
    const char    *s = n->attributes;
    const uint32_t attrLen = strlen(attr);

    while((s = strstr(s, attr)) != NULL)
    {
        const char *preS = s - 1;
        const char *postS = s + attrLen;

        /* Check if the attribute was found in isolation
         *  i.e. not part of another attribute name.
         */
        if((s == n->attributes || isspace(*preS)) && (*postS == '=' || isspace(*postS)))
        {
            postS = skipSpace(postS);

            if(*postS == '=')
            {
                postS++;
                postS = skipSpace(postS);
                if(*postS == '"')
                {
                    char *c, *r = x_strdup(postS + 1);

                    if((c = strchr(r, '"')) != NULL)
                        *c = '\0';

                    n->freeList[n->freeListLen++] = r;

                    return r;
                }
            }
        }

        /* Skip the match */
        s++;
    }

    return NULL;
}

/** Return the tag.
 */
const char *XmlGetTag(struct xmlnode *n)
{
  return n->tag;
}


/** Compare the tag name with some string.
 */
int XmlTagStrcmp(struct xmlnode *n, const char *str)
{
    return strcmp(n->tag, str);
}

/** Get the content.
 */
const char *XmlGetContent(struct xmlnode *n)
{
    if(!n->converted)
    {
        convert(n->content);
        n->converted = true;
    }
    return n->content;
}

/** Free a node and its storage.
 * \param[in,out] n  Pointer to node pointer to be freed.  If *n == NULL,
 *                    no action is taken.  *n is always set to NULL when
 *                    returning.
 */
void XmlDestroy(struct xmlnode **n)
{
    struct xmlnode *nn = *n;

    if(nn)
    {
        if(nn->tag)
        {
            free(nn->tag);
        }

        if(nn->content)
        {
            free(nn->content);
        }

        while(nn->freeListLen-- > 0)
        {
            free(nn->freeList[nn->freeListLen]);
        }

        free(nn);
        *n = NULL;
    }
}

/** Parse some XML from a file descriptor.
 * \param[in,out] n  Pointer to a node pointer. If *n != NULL, it will be freed.
 */
bool XmlParseFd(struct xmlnode **n, int fd)
{
    XmlDestroy(n);

    *n = x_zalloc(sizeof(struct xmlnode));
    (*n)->fd = fd;

    if(!Parse(*n))
    {
        free(*n);
        *n=NULL;
        return false;
    }

    return true;
}


/** Parse from a string buffer.
 * Parse some XML in a string, returning the position in the string reached,
 * or NULL if the string was exhausted and no node was found.
 * \param[in,out] n  Pointer to a node pointer. If *n != NULL, it will be freed.
 */
const char *XmlParseStr(struct xmlnode **n, const char *s)
{
    XmlDestroy(n);

    if(s == NULL)
    {
        return NULL;
    }

    *n = x_zalloc(sizeof(struct xmlnode));
    (*n)->fd = -1;
    (*n)->array = s;
    if(!Parse(*n))
    {
        free(*n);
        *n = NULL;
        return NULL;
    }
    else
    {
        return (*n)->array;
    }
}


/** Find a sub-node whose name matches the passed tag.
 */
struct xmlnode *XmlFindSubNode(struct xmlnode *n, const char *tag)
{
    struct xmlnode *r = NULL;
    const char     *s;

    if(n == NULL)
    {
        return NULL;
    }

    s = XmlGetContent(n);
    do
    {
        XmlDestroy(&r);
        s = XmlParseStr(&r, s);
    }
    while(r && XmlTagStrcmp(r, tag) != 0);

    return r;
}


/** Same as XmlFindSubNode(), but frees the past node before returning.
 */
struct xmlnode *XmlFindSubNodeFree(struct xmlnode *n, const char *tag)
{
    struct xmlnode *r = XmlFindSubNode(n, tag);

    XmlDestroy(&n);

    return r;
}

/* END OF FILE */
