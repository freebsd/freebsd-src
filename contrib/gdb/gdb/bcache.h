/* Include file cached obstack implementation.
   Written by Fred Fish <fnf@cygnus.com>
   Rewritten by Jim Blandy <jimb@cygnus.com>
   Copyright 1999, 2000 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef BCACHE_H
#define BCACHE_H 1

/* A bcache is a data structure for factoring out duplication in
   read-only structures.  You give the bcache some string of bytes S.
   If the bcache already contains a copy of S, it hands you back a
   pointer to its copy.  Otherwise, it makes a fresh copy of S, and
   hands you back a pointer to that.  In either case, you can throw
   away your copy of S, and use the bcache's.

   The "strings" in question are arbitrary strings of bytes --- they
   can contain zero bytes.  You pass in the length explicitly when you
   call the bcache function.

   This means that you can put ordinary C objects in a bcache.
   However, if you do this, remember that structs can contain `holes'
   between members, added for alignment.  These bytes usually contain
   garbage.  If you try to bcache two objects which are identical from
   your code's point of view, but have different garbage values in the
   structure's holes, then the bcache will treat them as separate
   strings, and you won't get the nice elimination of duplicates you
   were hoping for.  So, remember to memset your structures full of
   zeros before bcaching them!

   You shouldn't modify the strings you get from a bcache, because:

   - You don't necessarily know who you're sharing space with.  If I
     stick eight bytes of text in a bcache, and then stick an
     eight-byte structure in the same bcache, there's no guarantee
     those two objects don't actually comprise the same sequence of
     bytes.  If they happen to, the bcache will use a single byte
     string for both of them.  Then, modifying the structure will
     change the string.  In bizarre ways.

   - Even if you know for some other reason that all that's okay,
     there's another problem.  A bcache stores all its strings in a
     hash table.  If you modify a string's contents, you will probably
     change its hash value.  This means that the modified string is
     now in the wrong place in the hash table, and future bcache
     probes will never find it.  So by mutating a string, you give up
     any chance of sharing its space with future duplicates.  */


/* The type used to hold a single bcache string.  The user data is
   stored in d.data.  Since it can be any type, it needs to have the
   same alignment as the most strict alignment of any type on the host
   machine.  I don't know of any really correct way to do this in
   stock ANSI C, so just do it the same way obstack.h does.

   It would be nicer to have this stuff hidden away in bcache.c, but
   struct objstack contains a struct bcache directly --- not a pointer
   to one --- and then the memory-mapped stuff makes this a real pain.
   We don't strictly need to expose struct bstring, but it's better to
   have it all in one place.  */

struct bstring {
  struct bstring *next;
  size_t length;

  union
  {
    char data[1];
    double dummy;
  }
  d;
};


/* The structure for a bcache itself.
   To initialize a bcache, just fill it with zeros.  */
struct bcache {
  /* All the bstrings are allocated here.  */
  struct obstack cache;

  /* How many hash buckets we're using.  */
  unsigned int num_buckets;
  
  /* Hash buckets.  This table is allocated using malloc, so when we
     grow the table we can return the old table to the system.  */
  struct bstring **bucket;

  /* Statistics.  */
  unsigned long unique_count;	/* number of unique strings */
  long total_count;	/* total number of strings cached, including dups */
  long unique_size;	/* size of unique strings, in bytes */
  long total_size;      /* total number of bytes cached, including dups */
  long structure_size;	/* total size of bcache, including infrastructure */
};


/* Find a copy of the LENGTH bytes at ADDR in BCACHE.  If BCACHE has
   never seen those bytes before, add a copy of them to BCACHE.  In
   either case, return a pointer to BCACHE's copy of that string.  */
extern void *bcache (const void *addr, int length, struct bcache *bcache);

/* Free all the storage that BCACHE refers to.  The result is a valid,
   but empty, bcache.  This does not free BCACHE itself, since that
   might be part of some larger object.  */
extern void free_bcache (struct bcache *bcache);

/* Print statistics on BCACHE's memory usage and efficacity at
   eliminating duplication.  TYPE should be a string describing the
   kind of data BCACHE holds.  Statistics are printed using
   `printf_filtered' and its ilk.  */
extern void print_bcache_statistics (struct bcache *bcache, char *type);
/* The hash function */
extern unsigned long hash(const void *addr, int length);
#endif /* BCACHE_H */
