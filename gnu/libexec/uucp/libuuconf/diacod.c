/* diacod.c
   Translate a dialcode.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_diacod_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/diacod.c,v 1.6 1999/08/27 23:33:17 peter Exp $";
#endif

#include <errno.h>

static int idcode P((pointer pglobal, int argc, char **argv,
		     pointer pinfo, pointer pvar));

/* Get the name of the UUCP log file.  */

int
uuconf_dialcode (pglobal, zdial, pznum)
     pointer pglobal;
     const char *zdial;
     char **pznum;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_cmdtab as[2];
  char **pz;
  int iret;

  as[0].uuconf_zcmd = zdial;
  as[0].uuconf_itype = UUCONF_CMDTABTYPE_FN | 0;
  as[0].uuconf_pvar = (pointer) pznum;
  as[0].uuconf_pifn = idcode;

  as[1].uuconf_zcmd = NULL;

  *pznum = NULL;

  iret = UUCONF_SUCCESS;

  for (pz = qglobal->qprocess->pzdialcodefiles; *pz != NULL; pz++)
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

      iret = uuconf_cmd_file (pglobal, e, as, (pointer) NULL,
			      (uuconf_cmdtabfn) NULL, 0, (pointer) NULL);
      (void) fclose (e);

      if (iret != UUCONF_SUCCESS || *pznum != NULL)
	break;
    }

  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = *pz;
      iret |= UUCONF_ERROR_FILENAME;
    }
  else if (*pznum == NULL)
    iret = UUCONF_NOT_FOUND;

  return iret;
}

/* This is called if the dialcode is found.  It copies the number into
   the heap and gets out of reading the file.  */

/*ARGSUSED*/
static int
idcode (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char **pznum = (char **) pvar;

  if (argc == 1)
    {
      *pznum = malloc (1);
      if (*pznum != NULL)
	**pznum = '\0';
    }
  else if (argc == 2)
    *pznum = strdup (argv[1]);
  else
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

  if (*pznum == NULL)
    {
      qglobal->ierrno = errno;
      return (UUCONF_MALLOC_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_CMDTABRET_EXIT);
    }

  return UUCONF_CMDTABRET_EXIT;
}
