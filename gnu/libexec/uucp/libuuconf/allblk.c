/* allblk.c
   Allocate a memory block.

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
const char _uuconf_allblk_rcsid[] = "$Id: allblk.c,v 1.2 1994/05/07 18:11:53 ache Exp $";
#endif

#include "alloc.h"

/* Allocate a new memory block.  If this fails, uuconf_errno will be
   set, and the calling routine may return UUCONF_MALLOC_FAILED |
   UUCONF_ERROR_ERRNO.  */

pointer
uuconf_malloc_block ()
{
  struct sblock *qret;

  qret = (struct sblock *) malloc (sizeof (struct sblock));
  if (qret == NULL)
    return NULL;
  qret->qnext = NULL;
  qret->ifree = 0;
  qret->plast = NULL;
  qret->qadded = NULL;
  return (pointer) qret;
}
