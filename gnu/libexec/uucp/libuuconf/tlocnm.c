/* tlocnm.c
   Get the local name to use from the Taylor UUCP configuration files.

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
const char _uuconf_tlocnm_rcsid[] = "$Id: tlocnm.c,v 1.2 1994/05/07 18:13:08 ache Exp $";
#endif

#include <errno.h>

/* Get the local name to use, based on the login name, from the Taylor
   UUCP configuration files.  This could probably be done in a
   slightly more intelligent fashion, but no matter what it has to
   read the systems files.  */

int
uuconf_taylor_login_localname (pglobal, zlogin, pzname)
     pointer pglobal;
     const char *zlogin;
     char **pzname;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char **pznames, **pz;
  int iret;

  if (! qglobal->qprocess->fread_syslocs)
    {
      iret = _uuconf_iread_locations (qglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  /* As a simple optimization, if there is no "myname" command we can
     simply return immediately.  */
  if (! qglobal->qprocess->fuses_myname)
    {
      *pzname = NULL;
      return UUCONF_NOT_FOUND;
    }

  iret = uuconf_taylor_system_names (pglobal, &pznames, 0);
  if (iret != UUCONF_SUCCESS)
    return iret;

  *pzname = NULL;
  iret = UUCONF_NOT_FOUND;

  for (pz = pznames; *pz != NULL; pz++)
    {
      struct uuconf_system ssys;
      struct uuconf_system *qsys;

      iret = uuconf_system_info (pglobal, *pz, &ssys);
      if (iret != UUCONF_SUCCESS)
	break;

      for (qsys = &ssys; qsys != NULL; qsys = qsys->uuconf_qalternate)
	{
	  if (qsys->uuconf_zlocalname != NULL
	      && qsys->uuconf_fcalled
	      && qsys->uuconf_zcalled_login != NULL
	      && strcmp (qsys->uuconf_zcalled_login, zlogin) == 0)
	    {
	      *pzname = strdup (qsys->uuconf_zlocalname);
	      if (*pzname != NULL)
		iret = UUCONF_SUCCESS;
	      else
		{
		  qglobal->ierrno = errno;
		  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		}
	      break;
	    }
	}

      (void) uuconf_system_free (pglobal, &ssys);

      if (qsys != NULL)
	break;

      iret = UUCONF_NOT_FOUND;
    }

  for (pz = pznames; *pz != NULL; pz++)
    free ((pointer) *pz);
  free ((pointer) pznames);

  return iret;
}
