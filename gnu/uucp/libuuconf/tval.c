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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_tval_rcsid[] = "$Id: tval.c,v 1.1 1993/08/05 18:26:16 conklin Exp $";
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
