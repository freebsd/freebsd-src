/* search-list.c

   Copyright 2000, 2001, 2002, 2004 Free Software Foundation, Inc.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "libiberty.h"
#include "gprof.h"
#include "search_list.h"


void
search_list_append (Search_List *list, const char *paths)
{
  Search_List_Elem *new_el;
  const char *beg, *colon;
  unsigned int len;

  colon = paths - 1;
  do
    {
      beg = colon + 1;
      colon = strchr (beg, PATH_SEP_CHAR);

      if (colon)
	len = colon - beg;
      else
	len = strlen (beg);

      new_el = (Search_List_Elem *) xmalloc (sizeof (*new_el) + len);
      memcpy (new_el->path, beg, len);
      new_el->path[len] = '\0';

      /* Append new path at end of list.  */
      new_el->next = 0;

      if (list->tail)
	list->tail->next = new_el;
      else
	list->head = new_el;

      list->tail = new_el;
    }
  while (colon);
}
