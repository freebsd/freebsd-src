/* Support for sbrk() regions.
   Copyright 1992 Free Software Foundation, Inc.
   Contributed by Fred Fish at Cygnus Support.   fnf@cygnus.com

This file is part of the GNU C Library.

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
Cambridge, MA 02139, USA.  */

#include <string.h>	/* Prototypes for memcpy, memmove, memset, etc */

#include "mmalloc.h"

extern PTR sbrk ();

/* The mmalloc() package can use a single implicit malloc descriptor
   for mmalloc/mrealloc/mfree operations which do not supply an explicit
   descriptor.  For these operations, sbrk() is used to obtain more core
   from the system, or return core.  This allows mmalloc() to provide
   backwards compatibility with the non-mmap'd version. */

struct mdesc *__mmalloc_default_mdp;

/* Use sbrk() to get more core. */

static PTR
sbrk_morecore (mdp, size)
  struct mdesc *mdp;
  int size;
{
  PTR result;

  if ((result = sbrk (size)) == (PTR) -1)
    {
      result = NULL;
    }
  else
    {
      mdp -> breakval += size;
      mdp -> top += size;
    }
  return (result);
}

/* Initialize the default malloc descriptor if this is the first time
   a request has been made to use the default sbrk'd region.

   Since no alignment guarantees are made about the initial value returned
   by sbrk, test the initial value and (if necessary) sbrk enough additional
   memory to start off with alignment to BLOCKSIZE.  We actually only need
   it aligned to an alignment suitable for any object, so this is overkill.
   But at most it wastes just part of one BLOCKSIZE chunk of memory and
   minimizes portability problems by avoiding us having to figure out
   what the actual minimal alignment is.  The rest of the malloc code
   avoids this as well, by always aligning to the minimum of the requested
   size rounded up to a power of two, or to BLOCKSIZE.

   Note that we are going to use some memory starting at this initial sbrk
   address for the sbrk region malloc descriptor, which is a struct, so the
   base address must be suitably aligned. */

struct mdesc *
__mmalloc_sbrk_init ()
{
  PTR base;
  unsigned int adj;

  base = sbrk (0);
  adj = RESIDUAL (base, BLOCKSIZE);
  if (adj != 0)
    {
      sbrk (BLOCKSIZE - adj);
      base = sbrk (0);
    }
  __mmalloc_default_mdp = (struct mdesc *) sbrk (sizeof (struct mdesc));
  memset ((char *) __mmalloc_default_mdp, 0, sizeof (struct mdesc));
  __mmalloc_default_mdp -> morecore = sbrk_morecore;
  __mmalloc_default_mdp -> base = base;
  __mmalloc_default_mdp -> breakval = __mmalloc_default_mdp -> top = sbrk (0);
  __mmalloc_default_mdp -> fd = -1;
  return (__mmalloc_default_mdp);
}


