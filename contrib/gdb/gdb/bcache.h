/* Include file cached obstack implementation.
   Written by Fred Fish (fnf@cygnus.com)
   Copyright 1995 Free Software Foundation, Inc.

This file is part of GDB.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef BCACHE_H
#define BCACHE_H 1

#define BCACHE_HASHLENGTH	12	/* Number of bits in hash value */
#define BCACHE_HASHSIZE	(1 << BCACHE_HASHLENGTH)
#define BCACHE_MAXLENGTH	128

/* Note that the user data is stored in data[].  Since it can be any type,
   it needs to have the same alignment  as the most strict alignment of 
   any type on the host machine.  So do it the same way obstack does. */

struct hashlink {
  struct hashlink *next;
  union {
    char data[1];
    double dummy;
  } d;
};

/* BCACHE_DATA is used to get the address of the cached data. */

#define BCACHE_DATA(p) ((p)->d.data)

/* BCACHE_DATA_ALIGNMENT is used to get the offset of the start of
   cached data within the hashlink struct.  This value, plus the
   size of the cached data, is the amount of space to allocate for
   a hashlink struct to hold the next pointer and the data. */

#define BCACHE_DATA_ALIGNMENT \
	(((char *) BCACHE_DATA((struct hashlink*) 0) - (char *) 0))

struct bcache {
  struct obstack cache;
  struct hashlink **indextable[BCACHE_MAXLENGTH];
  int cache_hits;
  int cache_misses;
  int cache_bytes;
  int cache_savings;
  int bcache_overflows;
};

extern void *
bcache PARAMS ((void *bytes, int count, struct bcache *bcachep));

#if MAINTENANCE_CMDS

extern void
print_bcache_statistics PARAMS ((struct bcache *, char *));

#endif	/* MAINTENANCE_CMDS */

#endif /* BCACHE_H */
