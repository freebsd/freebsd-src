/* hlocnm.c
   Get the local name to use from the HDB configuration files.

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
const char _uuconf_hlocnm_rcsid[] = "$Id: hlocnm.c,v 1.2 1994/05/07 18:12:23 ache Exp $";
#endif

#include <errno.h>

/* Get the local name to use, based on the login name, from the HDB
   configuration files.  */

int
uuconf_hdb_login_localname (pglobal, zlogin, pzname)
     pointer pglobal;
     const char *zlogin;
     char **pzname;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct shpermissions *qperm;

  if (! qglobal->qprocess->fhdb_read_permissions)
    {
      int iret;

      iret = _uuconf_ihread_permissions (qglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  for (qperm = qglobal->qprocess->qhdb_permissions;
       qperm != NULL;
       qperm = qperm->qnext)
    {
      if (qperm->zmyname != NULL
	  && qperm->zmyname != (char *) &_uuconf_unset
	  && qperm->pzlogname != NULL
	  && qperm->pzlogname != (char **) &_uuconf_unset)
	{
	  char **pz;

	  for (pz = qperm->pzlogname; *pz != NULL; pz++)
	    {
	      if (strcmp (*pz, zlogin) == 0)
		{
		  *pzname = strdup (qperm->zmyname);
		  if (*pzname == NULL)
		    {
		      qglobal->ierrno = errno;
		      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		    }
		  return UUCONF_SUCCESS;
		}
	    }
	}
    }

  *pzname = NULL;
  return UUCONF_NOT_FOUND;
}
