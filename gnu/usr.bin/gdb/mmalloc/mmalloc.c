/* Memory allocator `malloc'.
   Copyright 1990, 1991, 1992 Free Software Foundation

   Written May 1989 by Mike Haertel.
   Heavily modified Mar 1992 by Fred Fish for mmap'd version.

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

/* Prototypes for local functions */

static int initialize PARAMS ((struct mdesc *));
static PTR morecore PARAMS ((struct mdesc *, size_t));
static PTR align PARAMS ((struct mdesc *, size_t));

/* Aligned allocation.  */

static PTR
align (mdp, size)
  struct mdesc *mdp;
  size_t size;
{
  PTR result;
  unsigned long int adj;

  result = mdp -> morecore (mdp, size);
  adj = RESIDUAL (result, BLOCKSIZE);
  if (adj != 0)
    {
      adj = BLOCKSIZE - adj;
      mdp -> morecore (mdp, adj);
      result = (char *) result + adj;
    }
  return (result);
}

/* Set everything up and remember that we have.  */

static int
initialize (mdp)
  struct mdesc *mdp;
{
  mdp -> heapsize = HEAP / BLOCKSIZE;
  mdp -> heapinfo = (malloc_info *) 
    align (mdp, mdp -> heapsize * sizeof (malloc_info));
  if (mdp -> heapinfo == NULL)
    {
      return (0);
    }
  memset ((PTR)mdp -> heapinfo, 0, mdp -> heapsize * sizeof (malloc_info));
  mdp -> heapinfo[0].free.size = 0;
  mdp -> heapinfo[0].free.next = mdp -> heapinfo[0].free.prev = 0;
  mdp -> heapindex = 0;
  mdp -> heapbase = (char *) mdp -> heapinfo;
  mdp -> flags |= MMALLOC_INITIALIZED;
  return (1);
}

/* Get neatly aligned memory, initializing or
   growing the heap info table as necessary. */

static PTR
morecore (mdp, size)
  struct mdesc *mdp;
  size_t size;
{
  PTR result;
  malloc_info *newinfo, *oldinfo;
  size_t newsize;

  result = align (mdp, size);
  if (result == NULL)
    {
      return (NULL);
    }

  /* Check if we need to grow the info table.  */
  if ((size_t) BLOCK ((char *) result + size) > mdp -> heapsize)
    {
      newsize = mdp -> heapsize;
      while ((size_t) BLOCK ((char *) result + size) > newsize)
	{
	  newsize *= 2;
	}
      newinfo = (malloc_info *) align (mdp, newsize * sizeof (malloc_info));
      if (newinfo == NULL)
	{
	  mdp -> morecore (mdp, -size);
	  return (NULL);
	}
      memset ((PTR) newinfo, 0, newsize * sizeof (malloc_info));
      memcpy ((PTR) newinfo, (PTR) mdp -> heapinfo,
	      mdp -> heapsize * sizeof (malloc_info));
      oldinfo = mdp -> heapinfo;
      newinfo[BLOCK (oldinfo)].busy.type = 0;
      newinfo[BLOCK (oldinfo)].busy.info.size
	= BLOCKIFY (mdp -> heapsize * sizeof (malloc_info));
      mdp -> heapinfo = newinfo;
      __mmalloc_free (mdp, (PTR)oldinfo);
      mdp -> heapsize = newsize;
    }

  mdp -> heaplimit = BLOCK ((char *) result + size);
  return (result);
}

/* Allocate memory from the heap.  */

PTR
mmalloc (md, size)
  PTR md;
  size_t size;
{
  struct mdesc *mdp;
  PTR result;
  size_t block, blocks, lastblocks, start;
  register size_t i;
  struct list *next;
  register size_t log;

  if (size == 0)
    {
      return (NULL);
    }

  mdp = MD_TO_MDP (md);
      
  if (mdp -> mmalloc_hook != NULL)
    {
      return ((*mdp -> mmalloc_hook) (md, size));
    }

  if (!(mdp -> flags & MMALLOC_INITIALIZED))
    {
      if (!initialize (mdp))
	{
	  return (NULL);
	}
    }

  if (size < sizeof (struct list))
    {
      size = sizeof (struct list);
    }

  /* Determine the allocation policy based on the request size.  */
  if (size <= BLOCKSIZE / 2)
    {
      /* Small allocation to receive a fragment of a block.
	 Determine the logarithm to base two of the fragment size. */
      log = 1;
      --size;
      while ((size /= 2) != 0)
	{
	  ++log;
	}

      /* Look in the fragment lists for a
	 free fragment of the desired size. */
      next = mdp -> fraghead[log].next;
      if (next != NULL)
	{
	  /* There are free fragments of this size.
	     Pop a fragment out of the fragment list and return it.
	     Update the block's nfree and first counters. */
	  result = (PTR) next;
	  next -> prev -> next = next -> next;
	  if (next -> next != NULL)
	    {
	      next -> next -> prev = next -> prev;
	    }
	  block = BLOCK (result);
	  if (--mdp -> heapinfo[block].busy.info.frag.nfree != 0)
	    {
	      mdp -> heapinfo[block].busy.info.frag.first =
		RESIDUAL (next -> next, BLOCKSIZE) >> log;
	    }

	  /* Update the statistics.  */
	  mdp -> heapstats.chunks_used++;
	  mdp -> heapstats.bytes_used += 1 << log;
	  mdp -> heapstats.chunks_free--;
	  mdp -> heapstats.bytes_free -= 1 << log;
	}
      else
	{
	  /* No free fragments of the desired size, so get a new block
	     and break it into fragments, returning the first.  */
	  result = mmalloc (md, BLOCKSIZE);
	  if (result == NULL)
	    {
	      return (NULL);
	    }

	  /* Link all fragments but the first into the free list.  */
	  for (i = 1; i < (size_t) (BLOCKSIZE >> log); ++i)
	    {
	      next = (struct list *) ((char *) result + (i << log));
	      next -> next = mdp -> fraghead[log].next;
	      next -> prev = &mdp -> fraghead[log];
	      next -> prev -> next = next;
	      if (next -> next != NULL)
		{
		  next -> next -> prev = next;
		}
	    }

	  /* Initialize the nfree and first counters for this block.  */
	  block = BLOCK (result);
	  mdp -> heapinfo[block].busy.type = log;
	  mdp -> heapinfo[block].busy.info.frag.nfree = i - 1;
	  mdp -> heapinfo[block].busy.info.frag.first = i - 1;

	  mdp -> heapstats.chunks_free += (BLOCKSIZE >> log) - 1;
	  mdp -> heapstats.bytes_free += BLOCKSIZE - (1 << log);
 	  mdp -> heapstats.bytes_used -= BLOCKSIZE - (1 << log);
	}
    }
  else
    {
      /* Large allocation to receive one or more blocks.
	 Search the free list in a circle starting at the last place visited.
	 If we loop completely around without finding a large enough
	 space we will have to get more memory from the system.  */
      blocks = BLOCKIFY(size);
      start = block = MALLOC_SEARCH_START;
      while (mdp -> heapinfo[block].free.size < blocks)
	{
	  block = mdp -> heapinfo[block].free.next;
	  if (block == start)
	    {
	      /* Need to get more from the system.  Check to see if
		 the new core will be contiguous with the final free
		 block; if so we don't need to get as much.  */
	      block = mdp -> heapinfo[0].free.prev;
	      lastblocks = mdp -> heapinfo[block].free.size;
	      if (mdp -> heaplimit != 0 &&
		  block + lastblocks == mdp -> heaplimit &&
		  mdp -> morecore (mdp, 0) == ADDRESS(block + lastblocks) &&
		  (morecore (mdp, (blocks - lastblocks) * BLOCKSIZE)) != NULL)
		{
		  /* Which block we are extending (the `final free
		     block' referred to above) might have changed, if
		     it got combined with a freed info table.  */
		  block = mdp -> heapinfo[0].free.prev;

		  mdp -> heapinfo[block].free.size += (blocks - lastblocks);
		  mdp -> heapstats.bytes_free +=
		      (blocks - lastblocks) * BLOCKSIZE;
		  continue;
		}
	      result = morecore(mdp, blocks * BLOCKSIZE);
	      if (result == NULL)
		{
		  return (NULL);
		}
	      block = BLOCK (result);
	      mdp -> heapinfo[block].busy.type = 0;
	      mdp -> heapinfo[block].busy.info.size = blocks;
	      mdp -> heapstats.chunks_used++;
	      mdp -> heapstats.bytes_used += blocks * BLOCKSIZE;
	      return (result);
	    }
	}

      /* At this point we have found a suitable free list entry.
	 Figure out how to remove what we need from the list. */
      result = ADDRESS(block);
      if (mdp -> heapinfo[block].free.size > blocks)
	{
	  /* The block we found has a bit left over,
	     so relink the tail end back into the free list. */
	  mdp -> heapinfo[block + blocks].free.size
	    = mdp -> heapinfo[block].free.size - blocks;
	  mdp -> heapinfo[block + blocks].free.next
	    = mdp -> heapinfo[block].free.next;
	  mdp -> heapinfo[block + blocks].free.prev
	    = mdp -> heapinfo[block].free.prev;
	  mdp -> heapinfo[mdp -> heapinfo[block].free.prev].free.next
	    = mdp -> heapinfo[mdp -> heapinfo[block].free.next].free.prev
	      = mdp -> heapindex = block + blocks;
	}
      else
	{
	  /* The block exactly matches our requirements,
	     so just remove it from the list. */
	  mdp -> heapinfo[mdp -> heapinfo[block].free.next].free.prev
	    = mdp -> heapinfo[block].free.prev;
	  mdp -> heapinfo[mdp -> heapinfo[block].free.prev].free.next
	    = mdp -> heapindex = mdp -> heapinfo[block].free.next;
	  mdp -> heapstats.chunks_free--;
	}

      mdp -> heapinfo[block].busy.type = 0;
      mdp -> heapinfo[block].busy.info.size = blocks;
      mdp -> heapstats.chunks_used++;
      mdp -> heapstats.bytes_used += blocks * BLOCKSIZE;
      mdp -> heapstats.bytes_free -= blocks * BLOCKSIZE;
    }

  return (result);
}

/* When using this package, provide a version of malloc/realloc/free built
   on top of it, so that if we use the default sbrk() region we will not
   collide with another malloc package trying to do the same thing, if
   the application contains any "hidden" calls to malloc/realloc/free (such
   as inside a system library). */

PTR
malloc (size)
  size_t size;
{
  return (mmalloc ((PTR) NULL, size));
}
