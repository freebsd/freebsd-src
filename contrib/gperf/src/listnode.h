/* Data and function members for defining values and operations of a list node.

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

#ifndef _listnode_h
#define _listnode_h
#include "prototype.h"

#define ALPHABET_SIZE 128

typedef struct list_node 
{ 
  struct list_node *link;       /* TRUE if key has an identical KEY_SET as another key. */
  struct list_node *next;       /* Points to next element on the list. */  
  int        length;            /* Length of the key. */
  int        hash_value;        /* Hash value for the key. */
  int        occurrence;        /* A metric for frequency of key set occurrences. */
  int        index;             /* Position of this node relative to other nodes. */
  char      *key;               /* Key string. */
  char      *rest;              /* Additional information for building hash function. */
  char       char_set[1];       /* Set of characters to hash, specified by user. */
} LIST_NODE;

extern LIST_NODE *make_list_node P ((char *k, int len));

#endif _listnode_h
