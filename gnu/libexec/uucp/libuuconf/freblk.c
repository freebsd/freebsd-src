/* freblk.c
   Free up an entire memory block.

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
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_freblk_rcsid[] = "$Id: freblk.c,v 1.1 1993/08/05 18:25:18 conklin Exp $";
#endif

#include "alloc.h"

/* Free up an entire memory block.  */

#if UUCONF_ANSI_C
void
#endif
uuconf_free_block (pblock)
     pointer pblock;
{
  struct sblock *q = (struct sblock *) pblock;
  struct sblock *qloop;

  /* We have to free the added blocks first because the list may link
     into blocks that are earlier on the list.  */
  for (qloop = q; qloop != NULL; qloop = qloop->qnext)
    {
      struct sadded *qadd;

      for (qadd = qloop->qadded; qadd != NULL; qadd = qadd->qnext)
	free (qadd->padded);
    }

  while (q != NULL)
    {
      struct sblock *qnext;

      qnext = q->qnext;
      free ((pointer) q);
      q = qnext;
    }
}
