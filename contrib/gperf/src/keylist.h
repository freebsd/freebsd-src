/* Data and function member declarations for the keyword list class.

   Copyright (C) 1989 Free Software Foundation, Inc.
   written by Douglas C. Schmidt (schmidt@ics.uci.edu)

This file is part of GNU GPERF.

GNU GPERF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU GPERF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU GPERF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* The key word list is a useful abstraction that keeps track of
   various pieces of information that enable that fast generation
   of the Perfect.hash function.  A Key_List is a singly-linked
   list of List_Nodes. */

#ifndef _keylist_h
#define _keylist_h
#include <stdio.h>
#include "listnode.h"

typedef struct key_list
{
  LIST_NODE *head;                  /* Points to the head of the linked list. */
  char      *array_type;            /* Pointer to the type for word list. */
  char      *return_type;           /* Pointer to return type for lookup function. */
  char      *struct_tag;            /* Shorthand for user-defined struct tag type. */
  char      *include_src;           /* C source code to be included verbatim. */
  int        list_len;              /* Length of head's Key_List, not counting duplicates. */
  int        total_keys;            /* Total number of keys, counting duplicates. */
  int        max_key_len;           /* Maximum length of the longest keyword. */
  int        min_key_len;           /* Minimum length of the shortest keyword. */
  bool       occurrence_sort;       /* True if sorting by occurrence. */
  bool       hash_sort;             /* True if sorting by hash value. */
  bool       additional_code;       /* True if any additional C code is included. */
} KEY_LIST;

extern void       key_list_init P ((void));
extern void       key_list_destroy P ((void));
extern void       print_output P ((void));
extern int        keyword_list_length P ((void));
extern int        max_key_length P ((void));
extern KEY_LIST   key_list;
#endif /* _keylist_h */
