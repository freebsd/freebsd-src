/* hsnams.c
   Get all known system names from the HDB configuration files.

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
const char _uuconf_hsnams_rcsid[] = "$Id: hsnams.c,v 1.2 1994/05/07 18:12:28 ache Exp $";
#endif

#include <errno.h>
#include <ctype.h>

/* Get all the system names from the HDB Systems file.  We have to
   read the Permissions file in order to support aliases.  */

int
uuconf_hdb_system_names (pglobal, ppzsystems, falias)
     pointer pglobal;
     char ***ppzsystems;
     int falias;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;
  char *zline;
  size_t cline;
  char **pz;

  *ppzsystems = NULL;

  iret = UUCONF_SUCCESS;

  zline = NULL;
  cline = 0;

  for (pz = qglobal->qprocess->pzhdb_systems; *pz != NULL; pz++)
    {
      FILE *e;

      e = fopen (*pz, "r");
      if (e == NULL)
	{
	  if (FNO_SUCH_FILE ())
	    continue;
	  qglobal->ierrno = errno;
	  iret = UUCONF_FOPEN_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}
      
      qglobal->ilineno = 0;

      while (_uuconf_getline (qglobal, &zline, &cline, e) > 0)
	{
	  ++qglobal->ilineno;

	  /* Lines beginning with whitespace are treated as comments.
	     No system name can contain a '#', which is another
	     comment character, so eliminating the first '#' does no
	     harm and catches comments.  */
	  zline[strcspn (zline, " \t#\n")] = '\0';
	  if (*zline == '\0')
	    continue;

	  iret = _uuconf_iadd_string (qglobal, zline, TRUE, TRUE,
				      ppzsystems, (pointer) NULL);
	  if (iret != UUCONF_SUCCESS)
	    {
	      iret |= UUCONF_ERROR_LINENO;
	      break;
	    }
	}

      (void) fclose (e);
    }

  if (zline != NULL)
    free ((pointer) zline);

  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = *pz;
      return iret | UUCONF_ERROR_FILENAME;
    }

  /* If we are supposed to return aliases, we must read the
     Permissions file.  */
  if (falias)
    {
      struct shpermissions *q;

      if (! qglobal->qprocess->fhdb_read_permissions)
	{
	  iret = _uuconf_ihread_permissions (qglobal);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	}

      for (q = qglobal->qprocess->qhdb_permissions;
	   q != NULL;
	   q = q->qnext)
	{
	  pz = q->pzalias;
	  if (pz == NULL || pz == (char **) &_uuconf_unset)
	    continue;

	  for (; *pz != NULL; pz++)
	    {
	      iret = _uuconf_iadd_string (qglobal, *pz, TRUE, TRUE,
					  ppzsystems, (pointer) NULL);
	      if (iret != UUCONF_SUCCESS)
		return iret;
	    }
	}
    }

  if (*ppzsystems == NULL)
    iret = _uuconf_iadd_string (qglobal, (char *) NULL, FALSE, FALSE,
				ppzsystems, (pointer) NULL);

  return iret;
}
