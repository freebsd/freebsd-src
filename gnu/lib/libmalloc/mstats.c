/* Access the statistics maintained by `malloc'.
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

struct mstats
mstats ()
{
  struct mstats result;

  result.bytes_total = (char *) (*__morecore) (0) - _heapbase;
  result.chunks_used = _chunks_used;
  result.bytes_used = _bytes_used;
  result.chunks_free = _chunks_free;
  result.bytes_free = _bytes_free;
  return result;
}
