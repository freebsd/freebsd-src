/* vsnams.c
   Get all known system names from the V2 configuration files.

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
const char _uuconf_vsnams_rcsid[] = "$Id: vsnams.c,v 1.1 1993/08/05 18:26:24 conklin Exp $";
#endif

#include <errno.h>

/* Get all the system names from the V2 L.sys file.  This code does
   not support aliases, although some V2 versions do have an L-aliases
   file.  */

/*ARGSUSED*/
int
uuconf_v2_system_names (pglobal, ppzsystems, falias)
     pointer pglobal;
     char ***ppzsystems;
     int falias;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  FILE *e;
  int iret;
  char *zline;
  size_t cline;

  *ppzsystems = NULL;

  e = fopen (qglobal->qprocess->zv2systems, "r");
  if (e == NULL)
    {
      if (FNO_SUCH_FILE ())
	return _uuconf_iadd_string (qglobal, (char *) NULL, FALSE, FALSE,
				    ppzsystems, (pointer) NULL);
      qglobal->ierrno = errno;
      qglobal->zfilename = qglobal->qprocess->zv2systems;
      return (UUCONF_FOPEN_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_ERROR_FILENAME);
    }

  qglobal->ilineno = 0;
  iret = UUCONF_SUCCESS;

  zline = NULL;
  cline = 0;
  while (getline (&zline, &cline, e) > 0)
    {
      char *zname;

      ++qglobal->ilineno;

      /* Skip leading whitespace to get to the system name.  Then cut
	 the system name off at the first whitespace, comment, or
	 newline.  */
      zname = zline + strspn (zline, " \t");
      zname[strcspn (zname, " \t#\n")] = '\0';
      if (*zname == '\0')
	continue;

      iret = _uuconf_iadd_string (qglobal, zname, TRUE, TRUE, ppzsystems,
				  (pointer) NULL);
      if (iret != UUCONF_SUCCESS)
	break;
    }

  (void) fclose (e);
  if (zline != NULL)
    free ((pointer) zline);

  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = qglobal->qprocess->zv2systems;
      return iret | UUCONF_ERROR_FILENAME | UUCONF_ERROR_LINENO;
    }

  if (*ppzsystems == NULL)
    iret = _uuconf_iadd_string (qglobal, (char *) NULL, FALSE, FALSE,
				ppzsystems, (pointer) NULL);

  return iret;
}
