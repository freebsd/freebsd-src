/* tval.c
   Validate a login name for a system using Taylor UUCP files.

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
const char _uuconf_tval_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/tval.c,v 1.6 1999/08/27 23:33:35 peter Exp $";
#endif

/* Validate a login name for a system using Taylor UUCP configuration
   files.  This assumes that the zcalled_login field is either NULL or
   "ANY".  If makes sure that the login name does not appear in some
   other "called-login" command listing systems not including this
   one.  */

int
uuconf_taylor_validate (pglobal, qsys, zlogin)
     pointer pglobal;
     const struct uuconf_system *qsys;
     const char *zlogin;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct svalidate *q;

  if (! qglobal->qprocess->fread_syslocs)
    {
      int iret;

      iret = _uuconf_iread_locations (qglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  for (q = qglobal->qprocess->qvalidate; q != NULL; q = q->qnext)
    {
      if (strcmp (q->zlogname, zlogin) == 0)
	{
	  char **pz;

	  for (pz = q->pzmachines; *pz != NULL; pz++)
	    if (strcmp (*pz, qsys->uuconf_zname) == 0)
	      return UUCONF_SUCCESS;

	  return UUCONF_NOT_FOUND;
	}
    }

  return UUCONF_SUCCESS;
}
