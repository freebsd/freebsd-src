/* Change the size of a block allocated by `mmalloc'.
   Copyright 1990, 1991 Free Software Foundation
		  Written May 1989 by Mike Haertel.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#include <string.h>	/* Prototypes for memcpy, memmove, memset, etc */

#include "mmalloc.h"

/* Resize the given region to the new size, returning a pointer
   to the (possibly moved) region.  This is optimized for speed;
   some benchmarks seem to indicate that greater compactness is
   achieved by unconditionally allocating and copying to a
   new region.  This module has incestuous knowledge of the
   internals of both mfree and mmalloc. */

PTR
mrealloc (md, ptr, size)
  PTR md;
  PTR ptr;
  size_t size;
{
  struct mdesc *mdp;
  PTR result;
  int type;
  size_t block, blocks, oldlimit;

  if (size == 0)
    {
      mfree (md, ptr);
      return (mmalloc (md, 0));
    }
  else if (ptr == NULL)
    {
      return (mmalloc (md, size));
    }

  mdp = MD_TO_MDP (md);

  if (mdp -> mrealloc_hook != NULL)
    {
      return ((*mdp -> mrealloc_hook) (md, ptr, size));
    }

  block = BLOCK (ptr);

  type = mdp -> heapinfo[block].busy.type;
  switch (type)
    {
    case 0:
      /* Maybe reallocate a large block to a small fragment.  */
      if (size <= BLOCKSIZE / 2)
	{
	  result = mmalloc (md, size);
	  if (result != NULL)
	    {
	      memcpy (result, ptr, size);
	      mfree (md, ptr);
	      return (result);
	    }
	}

      /* The new size is a large allocation as well;
	 see if we can hold it in place. */
      blocks = BLOCKIFY (size);
      if (blocks < mdp -> heapinfo[block].busy.info.size)
	{
	  /* The new size is smaller; return excess memory to the free list. */
	  mdp -> heapinfo[block + blocks].busy.type = 0;
	  mdp -> heapinfo[block + blocks].busy.info.size
	    = mdp -> heapinfo[block].busy.info.size - blocks;
	  mdp -> heapinfo[block].busy.info.size = blocks;
	  mfree (md, ADDRESS (block + blocks));
	  result = ptr;
	}
      else if (blocks == mdp -> heapinfo[block].busy.info.size)
	{
	  /* No size change necessary.  */
	  result = ptr;
	}
      else
	{
	  /* Won't fit, so allocate a new region that will.
	     Free the old region first in case there is sufficient
	     adjacent free space to grow without moving. */
	  blocks = mdp -> heapinfo[block].busy.info.size;
	  /* Prevent free from actually returning memory to the system.  */
	  oldlimit = mdp -> heaplimit;
	  mdp -> heaplimit = 0;
	  mfree (md, ptr);
	  mdp -> heaplimit = oldlimit;
	  result = mmalloc (md, size);
	  if (result == NULL)
	    {
	      mmalloc (md, blocks * BLOCKSIZE);
	      return (NULL);
	    }
	  if (ptr != result)
	    {
	      memmove (result, ptr, blocks * BLOCKSIZE);
	    }
	}
      break;

    default:
      /* Old size is a fragment; type is logarithm
	 to base two of the fragment size.  */
      if (size > (size_t) (1 << (type - 1)) && size <= (size_t) (1 << type))
	{
	  /* The new size is the same kind of fragment.  */
	  result = ptr;
	}
      else
	{
	  /* The new size is different; allocate a new space,
	     and copy the lesser of the new size and the old. */
	  result = mmalloc (md, size);
	  if (result == NULL)
	    {
	      return (NULL);
	    }
	  memcpy (result, ptr, MIN (size, (size_t) 1 << type));
	  mfree (md, ptr);
	}
      break;
    }

  return (result);
}

/* When using this package, provide a version of malloc/realloc/free built
   on top of it, so that if we use the default sbrk() region we will not
   collide with another malloc package trying to do the same thing, if
   the application contains any "hidden" calls to malloc/realloc/free (such
   as inside a system library). */

PTR
realloc (ptr, size)
  PTR ptr;
  size_t size;
{
  return (mrealloc ((PTR) NULL, ptr, size));
}
