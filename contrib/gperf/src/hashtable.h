/* Hash table used to check for duplicate keyword entries.

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

#ifndef _hashtable_h
#define _hashtable_h
#include "keylist.h"
#include "prototype.h"

typedef struct hash_table 
{
  LIST_NODE **table; /* Vector of pointers to linked lists of List_Node's. */
  int         size;  /* Size of the vector. */
} HASH_TABLE;

extern void       hash_table_init P ((LIST_NODE **table, int size));
extern void       hash_table_destroy P ((void));
extern LIST_NODE *retrieve P ((LIST_NODE *item, int ignore_length));

#endif /* _hashtable_h */
