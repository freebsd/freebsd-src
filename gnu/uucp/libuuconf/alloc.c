/* alloc.c
   Allocate within a memory block.

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
const char _uuconf_alloc_rcsid[] = "$Id: alloc.c,v 1.1 1993/08/05 18:24:59 conklin Exp $";
#endif

#include "alloc.h"

/* Allocate some memory out of a memory block.  If the memory block is
   NULL, this just calls malloc; this is convenient for a number of
   routines.  If this fails, uuconf_errno will be set, and the calling
   routine may return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO.  */

pointer
uuconf_malloc (pblock, c)
     pointer pblock;
     size_t c;
{
  struct sblock *q = (struct sblock *) pblock;
  pointer pret;

  if (c == 0)
    return NULL;

  if (q == NULL)
    return malloc (c);

  /* Make sure that c is aligned to a double boundary.  */
  c = ((c + sizeof (double) - 1) / sizeof (double)) * sizeof (double);

  while (q->ifree + c > CALLOC_SIZE)
    {
      if (q->qnext != NULL)
	q = q->qnext;
      else
	{
	  if (c > CALLOC_SIZE)
	    q->qnext = (struct sblock *) malloc (sizeof (struct sblock)
						 + c - CALLOC_SIZE);
	  else
	    q->qnext = (struct sblock *) malloc (sizeof (struct sblock));
	  if (q->qnext == NULL)
	    return NULL;
	  q = q->qnext;
	  q->qnext = NULL;
	  q->ifree = 0;
	  q->qadded = NULL;
	  break;
	}
    }

  pret = q->u.ab + q->ifree;
  q->ifree += c;
  q->plast = pret;

  return pret;
}
