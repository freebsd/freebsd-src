/* Memory allocator `malloc'.
   Copyright 1990, 1991, 1992, 1993 Free Software Foundation
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

/* How to really get more memory.  */
__ptr_t (*__morecore) __P ((ptrdiff_t __size)) = __default_morecore;

/* Debugging hook for `malloc'.  */
__ptr_t (*__malloc_hook) __P ((size_t __size));

/* Pointer to the base of the first block.  */
char *_heapbase;

/* Block information table.  Allocated with align/__free (not malloc/free).  */
malloc_info *_heapinfo;

/* Number of info entries.  */
static size_t heapsize;

/* Search index in the info table.  */
size_t _heapindex;

/* Limit of valid info table indices.  */
size_t _heaplimit;

/* Free lists for each fragment size.  */
struct list _fraghead[BLOCKLOG];

/* Instrumentation.  */
size_t _chunks_used;
size_t _bytes_used;
size_t _chunks_free;
size_t _bytes_free;

/* Are you experienced?  */
int __malloc_initialized;

void (*__after_morecore_hook) __P ((void));

/* Aligned allocation.  */
static __ptr_t align __P ((size_t));
static __ptr_t
align (size)
     size_t size;
{
  __ptr_t result;
  unsigned long int adj;

  result = (*__morecore) (size);
  adj = (unsigned long int) ((unsigned long int) ((char *) result -
						(char *) NULL)) % BLOCKSIZE;
  if (adj != 0)
    {
      adj = BLOCKSIZE - adj;
      (void) (*__morecore) (adj);
      result = (char *) result + adj;
    }

  if (__after_morecore_hook)
    (*__after_morecore_hook) ();

  return result;
}

/* Set everything up and remember that we have.  */
static int initialize __P ((void));
static int
initialize ()
{
  heapsize = HEAP / BLOCKSIZE;
  _heapinfo = (malloc_info *) align (heapsize * sizeof (malloc_info));
  if (_heapinfo == NULL)
    return 0;
  memset (_heapinfo, 0, heapsize * sizeof (malloc_info));
  _heapinfo[0].free.size = 0;
  _heapinfo[0].free.next = _heapinfo[0].free.prev = 0;
  _heapindex = 0;
  _heapbase = (char *) _heapinfo;
  __malloc_initialized = 1;
  return 1;
}

/* Get neatly aligned memory, initializing or
   growing the heap info table as necessary. */
static __ptr_t morecore __P ((size_t));
static __ptr_t
morecore (size)
     size_t size;
{
  __ptr_t result;
  malloc_info *newinfo, *oldinfo;
  size_t newsize;

  result = align (size);
  if (result == NULL)
    return NULL;

  /* Check if we need to grow the info table.  */
  if ((size_t) BLOCK ((char *) result + size) > heapsize)
    {
      newsize = heapsize;
      while ((size_t) BLOCK ((char *) result + size) > newsize)
	newsize *= 2;
      newinfo = (malloc_info *) align (newsize * sizeof (malloc_info));
      if (newinfo == NULL)
	{
	  (*__morecore) (-size);
	  return NULL;
	}
      memset (newinfo, 0, newsize * sizeof (malloc_info));
      memcpy (newinfo, _heapinfo, heapsize * sizeof (malloc_info));
      oldinfo = _heapinfo;
      newinfo[BLOCK (oldinfo)].busy.type = 0;
      newinfo[BLOCK (oldinfo)].busy.info.size
	= BLOCKIFY (heapsize * sizeof (malloc_info));
      _heapinfo = newinfo;
      _free_internal (oldinfo);
      heapsize = newsize;
    }

  _heaplimit = BLOCK ((char *) result + size);
  return result;
}

/* Allocate memory from the heap.  */
__ptr_t
malloc (size)
     size_t size;
{
  __ptr_t result;
  size_t block, blocks, lastblocks, start;
  register size_t i;
  struct list *next;

  /* ANSI C allows `malloc (0)' to either return NULL, or to return a
     valid address you can realloc and free (though not dereference).

     It turns out that some extant code (sunrpc, at least Ultrix's version)
     expects `malloc (0)' to return non-NULL and breaks otherwise.
     Be compatible.  */

#if	0
  if (size == 0)
    return NULL;
#endif

  if (__malloc_hook != NULL)
    return (*__malloc_hook) (size);

  if (!__malloc_initialized)
    if (!initialize ())
      return NULL;

  if (size < sizeof (struct list))
      size = sizeof (struct list);

  /* Determine the allocation policy based on the request size.  */
  if (size <= BLOCKSIZE / 2)
    {
      /* Small allocation to receive a fragment of a block.
	 Determine the logarithm to base two of the fragment size. */
      register size_t log = 1;
      --size;
      while ((size /= 2) != 0)
	++log;

      /* Look in the fragment lists for a
	 free fragment of the desired size. */
      next = _fraghead[log].next;
      if (next != NULL)
	{
	  /* There are free fragments of this size.
	     Pop a fragment out of the fragment list and return it.
	     Update the block's nfree and first counters. */
	  result = (__ptr_t) next;
	  next->prev->next = next->next;
	  if (next->next != NULL)
	    next->next->prev = next->prev;
	  block = BLOCK (result);
	  if (--_heapinfo[block].busy.info.frag.nfree != 0)
	    _heapinfo[block].busy.info.frag.first = (unsigned long int)
	      ((unsigned long int) ((char *) next->next - (char *) NULL)
	       % BLOCKSIZE) >> log;

	  /* Update the statistics.  */
	  ++_chunks_used;
	  _bytes_used += 1 << log;
	  --_chunks_free;
	  _bytes_free -= 1 << log;
	}
      else
	{
	  /* No free fragments of the desired size, so get a new block
	     and break it into fragments, returning the first.  */
	  result = malloc (BLOCKSIZE);
	  if (result == NULL)
	    return NULL;

	  /* Link all fragments but the first into the free list.  */
	  for (i = 1; i < (size_t) (BLOCKSIZE >> log); ++i)
	    {
	      next = (struct list *) ((char *) result + (i << log));
	      next->next = _fraghead[log].next;
	      next->prev = &_fraghead[log];
	      next->prev->next = next;
	      if (next->next != NULL)
		next->next->prev = next;
	    }

	  /* Initialize the nfree and first counters for this block.  */
	  block = BLOCK (result);
	  _heapinfo[block].busy.type = log;
	  _heapinfo[block].busy.info.frag.nfree = i - 1;
	  _heapinfo[block].busy.info.frag.first = i - 1;

	  _chunks_free += (BLOCKSIZE >> log) - 1;
	  _bytes_free += BLOCKSIZE - (1 << log);
	  _bytes_used -= BLOCKSIZE - (1 << log);
	}
    }
  else
    {
      /* Large allocation to receive one or more blocks.
	 Search the free list in a circle starting at the last place visited.
	 If we loop completely around without finding a large enough
	 space we will have to get more memory from the system.  */
      blocks = BLOCKIFY (size);
      start = block = _heapindex;
      while (_heapinfo[block].free.size < blocks)
	{
	  block = _heapinfo[block].free.next;
	  if (block == start)
	    {
	      /* Need to get more from the system.  Check to see if
		 the new core will be contiguous with the final free
		 block; if so we don't need to get as much.  */
	      block = _heapinfo[0].free.prev;
	      lastblocks = _heapinfo[block].free.size;
	      if (_heaplimit != 0 && block + lastblocks == _heaplimit &&
		  (*__morecore) (0) == ADDRESS (block + lastblocks) &&
		  (morecore ((blocks - lastblocks) * BLOCKSIZE)) != NULL)
		{
		  _heapinfo[block].free.size = blocks;
		  _bytes_free += (blocks - lastblocks) * BLOCKSIZE;
		  continue;
		}
	      result = morecore (blocks * BLOCKSIZE);
	      if (result == NULL)
		return NULL;
	      block = BLOCK (result);
	      _heapinfo[block].busy.type = 0;
	      _heapinfo[block].busy.info.size = blocks;
	      ++_chunks_used;
	      _bytes_used += blocks * BLOCKSIZE;
	      return result;
	    }
	}

      /* At this point we have found a suitable free list entry.
	 Figure out how to remove what we need from the list. */
      result = ADDRESS (block);
      if (_heapinfo[block].free.size > blocks)
	{
	  /* The block we found has a bit left over,
	     so relink the tail end back into the free list. */
	  _heapinfo[block + blocks].free.size
	    = _heapinfo[block].free.size - blocks;
	  _heapinfo[block + blocks].free.next
	    = _heapinfo[block].free.next;
	  _heapinfo[block + blocks].free.prev
	    = _heapinfo[block].free.prev;
	  _heapinfo[_heapinfo[block].free.prev].free.next
	    = _heapinfo[_heapinfo[block].free.next].free.prev
	    = _heapindex = block + blocks;
	}
      else
	{
	  /* The block exactly matches our requirements,
	     so just remove it from the list. */
	  _heapinfo[_heapinfo[block].free.next].free.prev
	    = _heapinfo[block].free.prev;
	  _heapinfo[_heapinfo[block].free.prev].free.next
	    = _heapindex = _heapinfo[block].free.next;
	  --_chunks_free;
	}

      _heapinfo[block].busy.type = 0;
      _heapinfo[block].busy.info.size = blocks;
      ++_chunks_used;
      _bytes_used += blocks * BLOCKSIZE;
      _bytes_free -= blocks * BLOCKSIZE;
    }

  return result;
}

#define min(A, B) ((A) < (B) ? (A) : (B))

/* Debugging hook for realloc.  */
__ptr_t (*__realloc_hook) __P ((__ptr_t __ptr, size_t __size));

/* Resize the given region to the new size, returning a pointer
   to the (possibly moved) region.  This is optimized for speed;
   some benchmarks seem to indicate that greater compactness is
   achieved by unconditionally allocating and copying to a
   new region.  This module has incestuous knowledge of the
   internals of both free and malloc. */
__ptr_t
realloc (ptr, size)
     __ptr_t ptr;
     size_t size;
{
  __ptr_t result;
  int type;
  size_t block, blocks, oldlimit;

  if (size == 0)
    {
      free (ptr);
      return malloc (0);
    }
  else if (ptr == NULL)
    return malloc (size);

  if (__realloc_hook != NULL)
    return (*__realloc_hook) (ptr, size);

  block = BLOCK (ptr);

  type = _heapinfo[block].busy.type;
  switch (type)
    {
    case 0:
      /* Maybe reallocate a large block to a small fragment.  */
      if (size <= BLOCKSIZE / 2)
	{
	  result = malloc (size);
	  if (result != NULL)
	    {
	      memcpy (result, ptr, size);
	      free (ptr);
	      return result;
	    }
	}

      /* The new size is a large allocation as well;
	 see if we can hold it in place. */
      blocks = BLOCKIFY (size);
      if (blocks < _heapinfo[block].busy.info.size)
	{
	  /* The new size is smaller; return
	     excess memory to the free list. */
	  _heapinfo[block + blocks].busy.type = 0;
	  _heapinfo[block + blocks].busy.info.size
	    = _heapinfo[block].busy.info.size - blocks;
	  _heapinfo[block].busy.info.size = blocks;
	  free (ADDRESS (block + blocks));
	  result = ptr;
	}
      else if (blocks == _heapinfo[block].busy.info.size)
	/* No size change necessary.  */
	result = ptr;
      else
	{
	  /* Won't fit, so allocate a new region that will.
	     Free the old region first in case there is sufficient
	     adjacent free space to grow without moving. */
	  blocks = _heapinfo[block].busy.info.size;
	  /* Prevent free from actually returning memory to the system.  */
	  oldlimit = _heaplimit;
	  _heaplimit = 0;
	  free (ptr);
	  _heaplimit = oldlimit;
	  result = malloc (size);
	  if (result == NULL)
	    {
	      /* Now we're really in trouble.  We have to unfree
		 the thing we just freed.  Unfortunately it might
		 have been coalesced with its neighbors.  */
	      if (_heapindex == block)
	        (void) malloc (blocks * BLOCKSIZE);
	      else
		{
		  __ptr_t previous = malloc ((block - _heapindex) * BLOCKSIZE);
		  (void) malloc (blocks * BLOCKSIZE);
		  free (previous);
		}
	      return NULL;
	    }
	  if (ptr != result)
	    memmove (result, ptr, blocks * BLOCKSIZE);
	}
      break;

    default:
      /* Old size is a fragment; type is logarithm
	 to base two of the fragment size.  */
      if (size > (size_t) (1 << (type - 1)) && size <= (size_t) (1 << type))
	/* The new size is the same kind of fragment.  */
	result = ptr;
      else
	{
	  /* The new size is different; allocate a new space,
	     and copy the lesser of the new size and the old. */
	  result = malloc (size);
	  if (result == NULL)
	    return NULL;
	  memcpy (result, ptr, min (size, (size_t) 1 << type));
	  free (ptr);
	}
      break;
    }

  return result;
}

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
