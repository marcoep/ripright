/* xmlparse.h
 *
 * Minimal XML parser.
 *   Copyright (C) 2001-2015 Michael McTernan, mike@mcternan.uk
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
 */


#ifndef XMLPARSE_HEADER
#define XMLPARSE_HEADER

#include <stdbool.h>

typedef struct xmlnode *xmlnode_t;

bool        XmlParseFd(struct xmlnode **n,int fd);
const char *XmlParseStr(struct xmlnode **n,const char *s);

const char *XmlGetTag(struct xmlnode *n);
const char *XmlGetAttribute(struct xmlnode *n, const char *attr);
const char *XmlGetContent(struct xmlnode *n);

char       *XmlGetAttributeDup(struct xmlnode *n, const char *attr);

int         XmlTagStrcmp(struct xmlnode *n, const char *str);
struct xmlnode *XmlFindSubNode(struct xmlnode *n, const char *tag);
struct xmlnode *XmlFindSubNodeFree(struct xmlnode *n, const char *tag);

void XmlDestroy(struct xmlnode **n);

#endif

