/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.

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
Cambridge, MA 02139, USA.  */

#ifndef	_MALLOC_INTERNAL
#define _MALLOC_INTERNAL
#include <malloc.h>
#endif

__ptr_t
memalign (alignment, size)
     size_t alignment;
     size_t size;
{
  __ptr_t result;
  unsigned long int adj;

  size = ((size + alignment - 1) / alignment) * alignment;

  result = malloc (size);
  if (result == NULL)
    return NULL;
  adj = (unsigned long int) ((unsigned long int) ((char *) result -
						(char *) NULL)) % alignment;
  if (adj != 0)
    {
      struct alignlist *l;
      for (l = _aligned_blocks; l != NULL; l = l->next)
	if (l->aligned == NULL)
	  /* This slot is free.  Use it.  */
	  break;
      if (l == NULL)
	{
	  l = (struct alignlist *) malloc (sizeof (struct alignlist));
	  if (l == NULL)
	    {
	      free (result);
	      return NULL;
	    }
	}
      l->exact = result;
      result = l->aligned = (char *) result + alignment - adj;
      l->next = _aligned_blocks;
      _aligned_blocks = l;
    }

  return result;
}
