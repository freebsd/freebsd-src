/* Free a block of memory allocated by `mmalloc'.
   Copyright 1990, 1991, 1992 Free Software Foundation

   Written May 1989 by Mike Haertel.
   Heavily modified Mar 1992 by Fred Fish.  (fnf@cygnus.com)

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

#include "mmalloc.h"

/* Return memory to the heap.
   Like `mfree' but don't call a mfree_hook if there is one.  */

void
__mmalloc_free (mdp, ptr)
  struct mdesc *mdp;
  PTR ptr;
{
  int type;
  size_t block, blocks;
  register size_t i;
  struct list *prev, *next;

  block = BLOCK (ptr);

  type = mdp -> heapinfo[block].busy.type;
  switch (type)
    {
    case 0:
      /* Get as many statistics as early as we can.  */
      mdp -> heapstats.chunks_used--;
      mdp -> heapstats.bytes_used -=
	  mdp -> heapinfo[block].busy.info.size * BLOCKSIZE;
      mdp -> heapstats.bytes_free +=
	  mdp -> heapinfo[block].busy.info.size * BLOCKSIZE;

      /* Find the free cluster previous to this one in the free list.
	 Start searching at the last block referenced; this may benefit
	 programs with locality of allocation.  */
      i = mdp -> heapindex;
      if (i > block)
	{
	  while (i > block)
	    {
	      i = mdp -> heapinfo[i].free.prev;
	    }
	}
      else
	{
	  do
	    {
	      i = mdp -> heapinfo[i].free.next;
	    }
	  while ((i != 0) && (i < block));
	  i = mdp -> heapinfo[i].free.prev;
	}

      /* Determine how to link this block into the free list.  */
      if (block == i + mdp -> heapinfo[i].free.size)
	{
	  /* Coalesce this block with its predecessor.  */
	  mdp -> heapinfo[i].free.size +=
	    mdp -> heapinfo[block].busy.info.size;
	  block = i;
	}
      else
	{
	  /* Really link this block back into the free list.  */
	  mdp -> heapinfo[block].free.size =
	    mdp -> heapinfo[block].busy.info.size;
	  mdp -> heapinfo[block].free.next = mdp -> heapinfo[i].free.next;
	  mdp -> heapinfo[block].free.prev = i;
	  mdp -> heapinfo[i].free.next = block;
	  mdp -> heapinfo[mdp -> heapinfo[block].free.next].free.prev = block;
	  mdp -> heapstats.chunks_free++;
	}

      /* Now that the block is linked in, see if we can coalesce it
	 with its successor (by deleting its successor from the list
	 and adding in its size).  */
      if (block + mdp -> heapinfo[block].free.size ==
	  mdp -> heapinfo[block].free.next)
	{
	  mdp -> heapinfo[block].free.size
	    += mdp -> heapinfo[mdp -> heapinfo[block].free.next].free.size;
	  mdp -> heapinfo[block].free.next
	    = mdp -> heapinfo[mdp -> heapinfo[block].free.next].free.next;
	  mdp -> heapinfo[mdp -> heapinfo[block].free.next].free.prev = block;
	  mdp -> heapstats.chunks_free--;
	}

      /* Now see if we can return stuff to the system.  */
      blocks = mdp -> heapinfo[block].free.size;
      if (blocks >= FINAL_FREE_BLOCKS && block + blocks == mdp -> heaplimit
	  && mdp -> morecore (mdp, 0) == ADDRESS (block + blocks))
	{
	  register size_t bytes = blocks * BLOCKSIZE;
	  mdp -> heaplimit -= blocks;
	  mdp -> morecore (mdp, -bytes);
	  mdp -> heapinfo[mdp -> heapinfo[block].free.prev].free.next
	    = mdp -> heapinfo[block].free.next;
	  mdp -> heapinfo[mdp -> heapinfo[block].free.next].free.prev
	    = mdp -> heapinfo[block].free.prev;
	  block = mdp -> heapinfo[block].free.prev;
	  mdp -> heapstats.chunks_free--;
	  mdp -> heapstats.bytes_free -= bytes;
	}

      /* Set the next search to begin at this block.  */
      mdp -> heapindex = block;
      break;

    default:
      /* Do some of the statistics.  */
      mdp -> heapstats.chunks_used--;
      mdp -> heapstats.bytes_used -= 1 << type;
      mdp -> heapstats.chunks_free++;
      mdp -> heapstats.bytes_free += 1 << type;

      /* Get the address of the first free fragment in this block.  */
      prev = (struct list *)
	((char *) ADDRESS(block) +
	 (mdp -> heapinfo[block].busy.info.frag.first << type));

      if (mdp -> heapinfo[block].busy.info.frag.nfree ==
	  (BLOCKSIZE >> type) - 1)
	{
	  /* If all fragments of this block are free, remove them
	     from the fragment list and free the whole block.  */
	  next = prev;
	  for (i = 1; i < (size_t) (BLOCKSIZE >> type); ++i)
	    {
	      next = next -> next;
	    }
	  prev -> prev -> next = next;
	  if (next != NULL)
	    {
	      next -> prev = prev -> prev;
	    }
	  mdp -> heapinfo[block].busy.type = 0;
	  mdp -> heapinfo[block].busy.info.size = 1;

	  /* Keep the statistics accurate.  */
	  mdp -> heapstats.chunks_used++;
	  mdp -> heapstats.bytes_used += BLOCKSIZE;
	  mdp -> heapstats.chunks_free -= BLOCKSIZE >> type;
	  mdp -> heapstats.bytes_free -= BLOCKSIZE;

	  mfree ((PTR) mdp, (PTR) ADDRESS(block));
	}
      else if (mdp -> heapinfo[block].busy.info.frag.nfree != 0)
	{
	  /* If some fragments of this block are free, link this
	     fragment into the fragment list after the first free
	     fragment of this block. */
	  next = (struct list *) ptr;
	  next -> next = prev -> next;
	  next -> prev = prev;
	  prev -> next = next;
	  if (next -> next != NULL)
	    {
	      next -> next -> prev = next;
	    }
	  ++mdp -> heapinfo[block].busy.info.frag.nfree;
	}
      else
	{
	  /* No fragments of this block are free, so link this
	     fragment into the fragment list and announce that
	     it is the first free fragment of this block. */
	  prev = (struct list *) ptr;
	  mdp -> heapinfo[block].busy.info.frag.nfree = 1;
	  mdp -> heapinfo[block].busy.info.frag.first =
	    RESIDUAL (ptr, BLOCKSIZE) >> type;
	  prev -> next = mdp -> fraghead[type].next;
	  prev -> prev = &mdp -> fraghead[type];
	  prev -> prev -> next = prev;
	  if (prev -> next != NULL)
	    {
	      prev -> next -> prev = prev;
	    }
	}
      break;
    }
}

/* Return memory to the heap.  */

void
mfree (md, ptr)
  PTR md;
  PTR ptr;
{
  struct mdesc *mdp;
  register struct alignlist *l;

  if (ptr != NULL)
    {
      mdp = MD_TO_MDP (md);
      for (l = mdp -> aligned_blocks; l != NULL; l = l -> next)
	{
	  if (l -> aligned == ptr)
	    {
	      l -> aligned = NULL;  /* Mark the slot in the list as free. */
	      ptr = l -> exact;
	      break;
	    }
	}      
      if (mdp -> mfree_hook != NULL)
	{
	  (*mdp -> mfree_hook) (md, ptr);
	}
      else
	{
	  __mmalloc_free (mdp, ptr);
	}
    }
}

/* When using this package, provide a version of malloc/realloc/free built
   on top of it, so that if we use the default sbrk() region we will not
   collide with another malloc package trying to do the same thing, if
   the application contains any "hidden" calls to malloc/realloc/free (such
   as inside a system library). */

void
free (ptr)
  PTR ptr;
{
  mfree ((PTR) NULL, ptr);
}
