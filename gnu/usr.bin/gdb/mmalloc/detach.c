/* Finish access to a mmap'd malloc managed region.
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

#include <sys/types.h>
#include <fcntl.h> /* After sys/types.h, at least for dpx/2.  */
#include "mmalloc.h"

/* Terminate access to a mmalloc managed region by unmapping all memory pages
   associated with the region, and closing the file descriptor if it is one
   that we opened.

   Returns NULL on success.

   Returns the malloc descriptor on failure, which can subsequently be used
   for further action, such as obtaining more information about the nature of
   the failure by examining the preserved errno value.

   Note that the malloc descriptor that we are using is currently located in
   region we are about to unmap, so we first make a local copy of it on the
   stack and use the copy. */

PTR
mmalloc_detach (md)
     PTR md;
{
  struct mdesc mtemp;

  if (md != NULL)
    {

      mtemp = *(struct mdesc *) md;
      
      /* Now unmap all the pages associated with this region by asking for a
	 negative increment equal to the current size of the region. */
      
      if ((mtemp.morecore (&mtemp, mtemp.base - mtemp.top)) == NULL)
	{
	  /* Update the original malloc descriptor with any changes */
	  *(struct mdesc *) md = mtemp;
	}
      else
	{
	  if (mtemp.flags & MMALLOC_DEVZERO)
	    {
	      close (mtemp.fd);
	    }
	  md = NULL;
	}
    }

  return (md);
}
