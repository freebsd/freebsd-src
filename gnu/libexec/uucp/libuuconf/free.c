/* free.c
   Free a buffer from within a memory block.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_free_rcsid[] = "$Id: free.c,v 1.2 1994/05/07 18:12:17 ache Exp $";
#endif

#include "alloc.h"

/* Free memory allocated by uuconf_malloc.  If the memory block is
   NULL, this just calls free; this is convenient for a number of
   routines.  Otherwise, this will only do something if this was the
   last buffer allocated for one of the memory blocks in the list; in
   other cases, the memory is lost until the entire memory block is
   freed.  */

#if UUCONF_ANSI_C
void
#endif
uuconf_free (pblock, pbuf)
     pointer pblock;
     pointer pbuf;
{
  struct sblock *q = (struct sblock *) pblock;

  if (pbuf == NULL)
    return;

  if (q == NULL)
    {
      free (pbuf);
      return;
    }

  for (; q != NULL; q = q->qnext)
    {
      if (q->plast == pbuf)
	{
	  q->ifree = (char *) pbuf - q->u.ab;
	  /* We could reset q->plast here, but it doesn't matter.  */
	  return;
	}
    }
}
