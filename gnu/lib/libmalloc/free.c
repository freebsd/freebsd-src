/* Free a block of memory allocated by `malloc'.
   Copyright 1990, 1991, 1992 Free Software Foundation
		  Written May 1989 by Mike Haertel.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation.  */

#ifndef	_MALLOC_INTERNAL
#define _MALLOC_INTERNAL
#include <malloc.h>
#endif

/* Debugging hook for free.  */
void (*__free_hook) __P ((__ptr_t __ptr));

/* List of blocks allocated by memalign.  */
struct alignlist *_aligned_blocks = NULL;

/* Return memory to the heap.
   Like `free' but don't call a __free_hook if there is one.  */
void
_free_internal (ptr)
     __ptr_t ptr;
{
  int type;
  size_t block, blocks;
  register size_t i;
  struct list *prev, *next;

  block = BLOCK (ptr);

  type = _heapinfo[block].busy.type;
  switch (type)
    {
    case 0:
      /* Get as many statistics as early as we can.  */
      --_chunks_used;
      _bytes_used -= _heapinfo[block].busy.info.size * BLOCKSIZE;
      _bytes_free += _heapinfo[block].busy.info.size * BLOCKSIZE;

      /* Find the free cluster previous to this one in the free list.
	 Start searching at the last block referenced; this may benefit
	 programs with locality of allocation.  */
      i = _heapindex;
      if (i > block)
	while (i > block)
	  i = _heapinfo[i].free.prev;
      else
	{
	  do
	    i = _heapinfo[i].free.next;
	  while (i > 0 && i < block);
	  i = _heapinfo[i].free.prev;
	}

      /* Determine how to link this block into the free list.  */
      if (block == i + _heapinfo[i].free.size)
	{
	  /* Coalesce this block with its predecessor.  */
	  _heapinfo[i].free.size += _heapinfo[block].busy.info.size;
	  block = i;
	}
      else
	{
	  /* Really link this block back into the free list.  */
	  _heapinfo[block].free.size = _heapinfo[block].busy.info.size;
	  _heapinfo[block].free.next = _heapinfo[i].free.next;
	  _heapinfo[block].free.prev = i;
	  _heapinfo[i].free.next = block;
	  _heapinfo[_heapinfo[block].free.next].free.prev = block;
	  ++_chunks_free;
	}

      /* Now that the block is linked in, see if we can coalesce it
	 with its successor (by deleting its successor from the list
	 and adding in its size).  */
      if (block + _heapinfo[block].free.size == _heapinfo[block].free.next)
	{
	  _heapinfo[block].free.size
	    += _heapinfo[_heapinfo[block].free.next].free.size;
	  _heapinfo[block].free.next
	    = _heapinfo[_heapinfo[block].free.next].free.next;
	  _heapinfo[_heapinfo[block].free.next].free.prev = block;
	  --_chunks_free;
	}

      /* Now see if we can return stuff to the system.  */
      blocks = _heapinfo[block].free.size;
      if (blocks >= FINAL_FREE_BLOCKS && block + blocks == _heaplimit
	  && (*__morecore) (0) == ADDRESS (block + blocks))
	{
	  register size_t bytes = blocks * BLOCKSIZE;
	  _heaplimit -= blocks;
	  (*__morecore) (-bytes);
	  _heapinfo[_heapinfo[block].free.prev].free.next
	    = _heapinfo[block].free.next;
	  _heapinfo[_heapinfo[block].free.next].free.prev
	    = _heapinfo[block].free.prev;
	  block = _heapinfo[block].free.prev;
	  --_chunks_free;
	  _bytes_free -= bytes;
	}

      /* Set the next search to begin at this block.  */
      _heapindex = block;
      break;

    default:
      /* Do some of the statistics.  */
      --_chunks_used;
      _bytes_used -= 1 << type;
      ++_chunks_free;
      _bytes_free += 1 << type;

      /* Get the address of the first free fragment in this block.  */
      prev = (struct list *) ((char *) ADDRESS (block) +
			   (_heapinfo[block].busy.info.frag.first << type));

      if (_heapinfo[block].busy.info.frag.nfree == (BLOCKSIZE >> type) - 1)
	{
	  /* If all fragments of this block are free, remove them
	     from the fragment list and free the whole block.  */
	  next = prev;
	  for (i = 1; i < (size_t) (BLOCKSIZE >> type); ++i)
	    next = next->next;
	  prev->prev->next = next;
	  if (next != NULL)
	    next->prev = prev->prev;
	  _heapinfo[block].busy.type = 0;
	  _heapinfo[block].busy.info.size = 1;

	  /* Keep the statistics accurate.  */
	  ++_chunks_used;
	  _bytes_used += BLOCKSIZE;
	  _chunks_free -= BLOCKSIZE >> type;
	  _bytes_free -= BLOCKSIZE;

	  free (ADDRESS (block));
	}
      else if (_heapinfo[block].busy.info.frag.nfree != 0)
	{
	  /* If some fragments of this block are free, link this
	     fragment into the fragment list after the first free
	     fragment of this block. */
	  next = (struct list *) ptr;
	  next->next = prev->next;
	  next->prev = prev;
	  prev->next = next;
	  if (next->next != NULL)
	    next->next->prev = next;
	  ++_heapinfo[block].busy.info.frag.nfree;
	}
      else
	{
	  /* No fragments of this block are free, so link this
	     fragment into the fragment list and announce that
	     it is the first free fragment of this block. */
	  prev = (struct list *) ptr;
	  _heapinfo[block].busy.info.frag.nfree = 1;
	  _heapinfo[block].busy.info.frag.first = (unsigned long int)
	    ((unsigned long int) ((char *) ptr - (char *) NULL)
	     % BLOCKSIZE >> type);
	  prev->next = _fraghead[type].next;
	  prev->prev = &_fraghead[type];
	  prev->prev->next = prev;
	  if (prev->next != NULL)
	    prev->next->prev = prev;
	}
      break;
    }
}

/* Return memory to the heap.  */
void
free (ptr)
     __ptr_t ptr;
{
  register struct alignlist *l;

  if (ptr == NULL)
    return;

  for (l = _aligned_blocks; l != NULL; l = l->next)
    if (l->aligned == ptr)
      {
	l->aligned = NULL;	/* Mark the slot in the list as free.  */
	ptr = l->exact;
	break;
      }

  if (__free_hook != NULL)
    (*__free_hook) (ptr);
  else
    _free_internal (ptr);
}
