/* snams.c
   Get all known system names.

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
const char _uuconf_snams_rcsid[] = "$Id: snams.c,v 1.1 1993/08/05 18:25:53 conklin Exp $";
#endif

/* Get all known system names.  */

int
uuconf_system_names (pglobal, ppzsystems, falias)
     pointer pglobal;
     char ***ppzsystems;
     int falias;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char **pztaylor;
  char **pzv2;
  char **pzhdb;
  int iret;

  *ppzsystems = NULL;
  pztaylor = NULL;
  pzv2 = NULL;
  pzhdb = NULL;

#if HAVE_TAYLOR_CONFIG
  iret = uuconf_taylor_system_names (pglobal, &pztaylor, falias);
  if (iret != UUCONF_SUCCESS)
    return iret;
#endif

#if HAVE_V2_CONFIG
  if (qglobal->qprocess->fv2)
    {
      iret = uuconf_v2_system_names (pglobal, &pzv2, falias);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }
#endif

#if HAVE_HDB_CONFIG
  if (qglobal->qprocess->fhdb)
    {
      iret = uuconf_hdb_system_names (pglobal, &pzhdb, falias);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }
#endif

  if (pzv2 == NULL && pzhdb == NULL)
    *ppzsystems = pztaylor;
  else if (pztaylor == NULL && pzhdb == NULL)
    *ppzsystems = pzv2;
  else if (pztaylor == NULL && pzv2 == NULL)
    *ppzsystems = pzhdb;
  else
    {
      char **pz;

      iret = UUCONF_SUCCESS;

      if (pztaylor != NULL)
	{
	  for (pz = pztaylor; *pz != NULL; pz++)
	    {
	      iret = _uuconf_iadd_string (qglobal, *pz, FALSE, TRUE,
					  ppzsystems, (pointer) NULL);
	      if (iret != UUCONF_SUCCESS)
		break;
	    }
	}

      if (pzv2 != NULL && iret == UUCONF_SUCCESS)
	{
	  for (pz = pzv2; *pz != NULL; pz++)
	    {
	      iret = _uuconf_iadd_string (qglobal, *pz, FALSE, TRUE,
					  ppzsystems, (pointer) NULL);
	      if (iret != UUCONF_SUCCESS)
		break;
	    }
	}

      if (pzhdb != NULL && iret == UUCONF_SUCCESS)
	{
	  for (pz = pzhdb; *pz != NULL; pz++)
	    {
	      iret = _uuconf_iadd_string (qglobal, *pz, FALSE, TRUE,
					  ppzsystems, (pointer) NULL);
	      if (iret != UUCONF_SUCCESS)
		break;
	    }
	}

      if (pztaylor != NULL)
	free ((pointer) pztaylor);
      if (pzv2 != NULL)
	free ((pointer) pzv2);
      if (pzhdb != NULL)
	free ((pointer) pzhdb);
    }

  if (iret == UUCONF_SUCCESS && *ppzsystems == NULL)
    iret = _uuconf_iadd_string (qglobal, (char *) NULL, FALSE, FALSE,
				ppzsystems, (pointer) NULL);

  return iret;
}
