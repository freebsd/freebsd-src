/* Implement a cached obstack.
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

#include "defs.h"
#include "obstack.h"
#include "bcache.h"
#include "gdb_string.h"		/* For memcpy declaration */

/* FIXME:  Incredibly simplistic hash generator.  Probably way too expensive
 (consider long strings) and unlikely to have good distribution across hash
 values for typical input. */

static unsigned int
hash (bytes, count)
     void *bytes;
     int count;
{
  unsigned int len;
  unsigned long hashval;
  unsigned int c;
  const unsigned char *data = bytes;

  hashval = 0;
  len = 0;
  while (count-- > 0)
    {
      c = *data++;
      hashval += c + (c << 17);
      hashval ^= hashval >> 2;
      ++len;
    }
  hashval += len + (len << 17);
  hashval ^= hashval >> 2;
  return (hashval % BCACHE_HASHSIZE);
}

static void *
lookup_cache (bytes, count, hashval, bcachep)
     void *bytes;
     int count;
     int hashval;
     struct bcache *bcachep;
{
  void *location = NULL;
  struct hashlink **hashtablep;
  struct hashlink *linkp;

  hashtablep = bcachep -> indextable[count];
  if (hashtablep != NULL)
    {
      linkp = hashtablep[hashval];
      while (linkp != NULL)
	{
	  if (memcmp (BCACHE_DATA (linkp), bytes, count) == 0)
	    {
	      location = BCACHE_DATA (linkp);
	      break;
	    }
	  linkp = linkp -> next;
	}
    }
  return (location);
}

void *
bcache (bytes, count, bcachep)
     void *bytes;
     int count;
     struct bcache *bcachep;
{
  int hashval;
  void *location;
  struct hashlink *newlink;
  struct hashlink **linkpp;
  struct hashlink ***hashtablepp;

  if (count >= BCACHE_MAXLENGTH)
    {
      /* Rare enough to just stash unique copies */
      location = (void *) obstack_alloc (&bcachep->cache, count);
      bcachep -> cache_bytes += count;
      memcpy (location, bytes, count);
      bcachep -> bcache_overflows++;
    }
  else
    {
      hashval = hash (bytes, count);
      location = lookup_cache (bytes, count, hashval, bcachep);
      if (location != NULL)
	{
	  bcachep -> cache_savings += count;
	  bcachep -> cache_hits++;
	}
      else
	{
	  bcachep -> cache_misses++;
	  hashtablepp = &bcachep -> indextable[count];
	  if (*hashtablepp == NULL)
	    {
	      *hashtablepp = (struct hashlink **)
		obstack_alloc (&bcachep->cache, BCACHE_HASHSIZE * sizeof (struct hashlink *));
	      bcachep -> cache_bytes += BCACHE_HASHSIZE * sizeof (struct hashlink *);
	      memset (*hashtablepp, 0, BCACHE_HASHSIZE * sizeof (struct hashlink *));
	    }
	  linkpp = &(*hashtablepp)[hashval];
	  newlink = (struct hashlink *)
	    obstack_alloc (&bcachep->cache, BCACHE_DATA_ALIGNMENT + count);
	  bcachep -> cache_bytes += BCACHE_DATA_ALIGNMENT + count;
	  memcpy (BCACHE_DATA (newlink), bytes, count);
	  newlink -> next = *linkpp;
	  *linkpp = newlink;
	  location = BCACHE_DATA (newlink);
	}
    }
  return (location);
}

#if MAINTENANCE_CMDS

void
print_bcache_statistics (bcachep, id)
     struct bcache *bcachep;
     char *id;
{
  struct hashlink **hashtablep;
  struct hashlink *linkp;
  int tidx, tcount, hidx, hcount, lcount, lmax, temp, lmaxt, lmaxh;

  for (lmax = lcount = tcount = hcount = tidx = 0; tidx < BCACHE_MAXLENGTH; tidx++)
    {
      hashtablep = bcachep -> indextable[tidx];
      if (hashtablep != NULL)
	{
	  tcount++;
	  for (hidx = 0; hidx < BCACHE_HASHSIZE; hidx++)
	    {
	      linkp = hashtablep[hidx];
	      if (linkp != NULL)
		{
		  hcount++;
		  for (temp = 0; linkp != NULL; linkp = linkp -> next)
		    {
		      lcount++;
		      temp++;
		    }
		  if (temp > lmax)
		    {
		      lmax = temp;
		      lmaxt = tidx;
		      lmaxh = hidx;
		    }
		}
	    }
	}
    }
  printf_filtered ("  Cached '%s' statistics:\n", id);
  printf_filtered ("    Cache hits: %d\n", bcachep -> cache_hits);
  printf_filtered ("    Cache misses: %d\n", bcachep -> cache_misses);
  printf_filtered ("    Cache hit ratio: %d%%\n", ((bcachep -> cache_hits) * 100) /  (bcachep -> cache_hits + bcachep -> cache_misses));
  printf_filtered ("    Space used for caching: %d\n", bcachep -> cache_bytes);
  printf_filtered ("    Space saved by cache hits: %d\n", bcachep -> cache_savings);
  printf_filtered ("    Number of bcache overflows: %d\n", bcachep -> bcache_overflows);
  printf_filtered ("    Number of index buckets used: %d\n", tcount);
  printf_filtered ("    Number of hash table buckets used: %d\n", hcount);
  printf_filtered ("    Number of chained items: %d\n", lcount);
  printf_filtered ("    Average hash table population: %d%%\n",
		   (hcount * 100) / (tcount * BCACHE_HASHSIZE));
  printf_filtered ("    Average chain length %d\n", lcount / hcount);
  printf_filtered ("    Maximum chain length %d at %d:%d\n", lmax, lmaxt, lmaxh);
}

#endif	/* MAINTENANCE_CMDS */
