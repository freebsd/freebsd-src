/* Macros for copying by pages; used in memcpy, memmove.  Generic macros.
   Copyright (C) 1995, 1997 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

/* This file defines the macro:

   PAGE_COPY_FWD_MAYBE (dstp, srcp, nbytes_left, nbytes)

   which is invoked like WORD_COPY_FWD et al.  The pointers should be at
   least word aligned.  This will check if virtual copying by pages can and
   should be done and do it if so.

   System-specific pagecopy.h files should define these macros and then
   #include this file:

   PAGE_COPY_THRESHOLD
   -- Minimum size for which virtual copying by pages is worthwhile.

   PAGE_SIZE
   -- Size of a page.

   PAGE_COPY_FWD (dstp, srcp, nbytes_left, nbytes)
   -- Macro to perform the virtual copy operation.
   The pointers will be aligned to PAGE_SIZE bytes.
*/


#if PAGE_COPY_THRESHOLD

#include <assert.h>

#define PAGE_COPY_FWD_MAYBE(dstp, srcp, nbytes_left, nbytes)		      \
  do									      \
    {									      \
      if ((nbytes) >= PAGE_COPY_THRESHOLD &&				      \
	  PAGE_OFFSET ((dstp) - (srcp)) == 0) 				      \
	{								      \
	  /* The amount to copy is past the threshold for copying	      \
	     pages virtually with kernel VM operations, and the		      \
	     source and destination addresses have the same alignment.  */    \
	  size_t nbytes_before = PAGE_OFFSET (-(dstp));			      \
	  if (nbytes_before != 0)					      \
	    {								      \
	      /* First copy the words before the first page boundary.  */     \
	      WORD_COPY_FWD (dstp, srcp, nbytes_left, nbytes_before);	      \
	      assert (nbytes_left == 0);				      \
	      nbytes -= nbytes_before;					      \
	    }								      \
	  PAGE_COPY_FWD (dstp, srcp, nbytes_left, nbytes);		      \
	}								      \
    } while (0)

/* The page size is always a power of two, so we can avoid modulo division.  */
#define PAGE_OFFSET(n)	((n) & (PAGE_SIZE - 1))

#else

#define PAGE_COPY_FWD_MAYBE(dstp, srcp, nbytes_left, nbytes) /* nada */

#endif
