/* hdnams.c
   Get all known dialer names from the HDB configuration files.

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
const char _uuconf_hdnams_rcsid[] = "$Id: hdnams.c,v 1.2 1994/05/07 18:12:21 ache Exp $";
#endif

#include <errno.h>
#include <ctype.h>

/* Get all the dialer names from the HDB Dialers file.  */

int
uuconf_hdb_dialer_names (pglobal, ppzdialers)
     pointer pglobal;
     char ***ppzdialers;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;
  char *zline;
  size_t cline;
  char **pz;

  *ppzdialers = NULL;

  iret = UUCONF_SUCCESS;

  zline = NULL;
  cline = 0;

  for (pz = qglobal->qprocess->pzhdb_dialers; *pz != NULL; pz++)
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
	     No dialer name can contain a '#', which is another
	     comment character, so eliminating the first '#' does no
	     harm and catches comments.  */
	  zline[strcspn (zline, " \t#\n")] = '\0';
	  if (*zline == '\0')
	    continue;

	  iret = _uuconf_iadd_string (qglobal, zline, TRUE, TRUE,
				      ppzdialers, (pointer) NULL);
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

  if (*ppzdialers == NULL)
    iret = _uuconf_iadd_string (qglobal, (char *) NULL, FALSE, FALSE,
				ppzdialers, (pointer) NULL);

  return UUCONF_SUCCESS;
}
