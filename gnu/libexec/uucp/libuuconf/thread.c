/* thread.c
   Initialize for a new thread.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_thread_rcsid[] = "$FreeBSD$";
#endif

#include <errno.h>

/* Initialize for a new thread, by allocating a new sglobal structure
   which points to the same sprocess structure.  */

int
uuconf_init_thread (ppglobal)
     pointer *ppglobal;
{
  struct sglobal **pqglob = (struct sglobal **) ppglobal;
  pointer pblock;
  struct sglobal *qnew;

  pblock = uuconf_malloc_block ();
  if (pblock == NULL)
    {
      (*pqglob)->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  qnew = (struct sglobal *) uuconf_malloc (pblock,
					   sizeof (struct sglobal));
  if (qnew == NULL)
    {
      (*pqglob)->ierrno = errno;
      uuconf_free_block (pblock);
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  qnew->pblock = pblock;
  qnew->ierrno = 0;
  qnew->ilineno = 0;
  qnew->zfilename = NULL;
  qnew->qprocess = (*pqglob)->qprocess;

  *pqglob = qnew;

  return UUCONF_SUCCESS;
}
