/* search-list.h

   Copyright 2000, 2001, 2004 Free Software Foundation, Inc.

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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef search_list_h
#define search_list_h

/* Non-Posix systems use semi-colon as directory separator in lists,
   since colon is part of drive letter spec.  */
#if defined (__MSDOS__) || defined (_WIN32)
#define PATH_SEP_CHAR ';'
#else
#define PATH_SEP_CHAR ':'
#endif

typedef struct search_list_elem
  {
    struct search_list_elem *next;
    char path[1];
  }
Search_List_Elem;

typedef struct
  {
    struct search_list_elem *head;
    struct search_list_elem *tail;
  }
Search_List;

extern void search_list_append (Search_List *, const char *);

#endif /* search_list_h */
