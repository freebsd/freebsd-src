/* local.c
   Get default information for the local system.

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
const char _uuconf_local_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/local.c,v 1.6 1999/08/27 23:33:25 peter Exp $";
#endif

#include <errno.h>

/* Get default information about the local system.  */

int
uuconf_system_local (pglobal, qsys)
     pointer pglobal;
     struct uuconf_system *qsys;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;

  _uuconf_uclear_system (qsys);
  qsys->uuconf_palloc = uuconf_malloc_block ();
  if (qsys->uuconf_palloc == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  qsys->uuconf_zname = (char *) qglobal->qprocess->zlocalname;

  /* By default, we permit the local system to forward to and from any
     system.  */
  iret = _uuconf_iadd_string (qglobal, (char *) "ANY", FALSE, FALSE,
			      &qsys->uuconf_pzforward_from,
			      qsys->uuconf_palloc);
  if (iret == UUCONF_SUCCESS)
    iret = _uuconf_iadd_string (qglobal, (char *) "ANY", FALSE, FALSE,
				&qsys->uuconf_pzforward_to,
				qsys->uuconf_palloc);
  if (iret != UUCONF_SUCCESS)
    {
      uuconf_free_block (qsys->uuconf_palloc);
      return iret;
    }

  return _uuconf_isystem_basic_default (qglobal, qsys);
}
