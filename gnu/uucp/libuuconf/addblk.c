/* addblk.c
   Add an malloc block to a memory block.

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
const char _uuconf_addblk_rcsid[] = "$Id: addblk.c,v 1.1 1993/08/05 18:24:56 conklin Exp $";
#endif

#include "alloc.h"

/* Add a memory buffer allocated by malloc to a memory block.  This is
   used by the uuconf_cmd functions so that they don't have to
   constantly copy data into memory.  Returns 0 on success, non 0 on
   failure. */

int
uuconf_add_block (pblock, padd)
     pointer pblock;
     pointer padd;
{
  struct sblock *q = (struct sblock *) pblock;
  struct sadded *qnew;

  qnew = (struct sadded *) uuconf_malloc (pblock, sizeof (struct sadded));
  if (qnew == NULL)
    return 1;

  qnew->qnext = q->qadded;
  qnew->padded = padd;
  q->qadded = qnew;

  return 0;
}
