/* llocnm.c
   Get the local name to use, given a login name.

   Copyright (C) 1992, 1993 Ian Lance Taylor

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
const char _uuconf_llocnm_rcsid[] = "$Id: llocnm.c,v 1.2 1994/05/07 18:12:36 ache Exp $";
#endif

#include <errno.h>

/* Get the local name to use, given a login name.  */

int
uuconf_login_localname (pglobal, zlogin, pzname)
     pointer pglobal;
     const char *zlogin;
     char **pzname;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;

#if HAVE_TAYLOR_CONFIG
  iret = uuconf_taylor_login_localname (pglobal, zlogin, pzname);
  if (iret != UUCONF_NOT_FOUND)
    return iret;
#endif

#if HAVE_HDB_CONFIG
  if (qglobal->qprocess->fhdb)
    {
      iret = uuconf_hdb_login_localname (pglobal, zlogin, pzname);
      if (iret != UUCONF_NOT_FOUND)
	return iret;
    }
#endif

  if (qglobal->qprocess->zlocalname != NULL)
    {
      *pzname = strdup ((char *) qglobal->qprocess->zlocalname);
      if (*pzname == NULL)
	{
	  qglobal->ierrno = errno;
	  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	}
      return UUCONF_SUCCESS;
    }

  *pzname = NULL;
  return UUCONF_NOT_FOUND;
}
