/* callin.c
   Check a login name and password against the UUCP password file.

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
const char _uuconf_callin_rcsid[] = "$Id: callin.c,v 1.1 1993/08/05 18:25:04 conklin Exp $";
#endif

#include <errno.h>

static int iplogin P((pointer pglobal, int argc, char **argv,
		      pointer pvar, pointer pinfo));

/* Check a login name and password against the UUCP password file.
   This looks at the Taylor UUCP password file, but will work even if
   uuconf_taylor_init was not called.  */

int
uuconf_callin (pglobal, zlogin, zpassword)
     pointer pglobal;
     const char *zlogin;
     const char *zpassword;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;
  char **pz;
  struct uuconf_cmdtab as[2];
  char *zfilepass;

  /* If we have no password file names, fill in the default name.  */
  if (qglobal->qprocess->pzpwdfiles == NULL)
    {
      char ab[sizeof NEWCONFIGLIB + sizeof PASSWDFILE - 1];

      memcpy ((pointer) ab, (pointer) NEWCONFIGLIB,
	      sizeof NEWCONFIGLIB - 1);
      memcpy ((pointer) (ab + sizeof NEWCONFIGLIB - 1), (pointer) PASSWDFILE,
	      sizeof PASSWDFILE);
      iret = _uuconf_iadd_string (qglobal, ab, TRUE, FALSE,
				  &qglobal->qprocess->pzpwdfiles,
				  qglobal->pblock);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  as[0].uuconf_zcmd = zlogin;
  as[0].uuconf_itype = UUCONF_CMDTABTYPE_FN | 2;
  as[0].uuconf_pvar = (pointer) &zfilepass;
  as[0].uuconf_pifn = iplogin;

  as[1].uuconf_zcmd = NULL;

  zfilepass = NULL;

  iret = UUCONF_SUCCESS;

  for (pz = qglobal->qprocess->pzpwdfiles; *pz != NULL; pz++)
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
			      (uuconf_cmdtabfn) NULL,
			      UUCONF_CMDTABFLAG_CASE, (pointer) NULL);
      (void) fclose (e);

      if (iret != UUCONF_SUCCESS || zfilepass != NULL)
	break;
    }

  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = *pz;
      iret |= UUCONF_ERROR_FILENAME;
    }
  else if (zfilepass == NULL
	   || strcmp (zfilepass, zpassword) != 0)
    iret = UUCONF_NOT_FOUND;

  if (zfilepass != NULL)
    free ((pointer) zfilepass);

  return iret;
}

/* This is called if it is the name we are looking for.  The pvar
   argument points to zfilepass, and we set it to the password.  */

static int
iplogin (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char **pzpass = (char **) pvar;

  *pzpass = strdup (argv[1]);
  if (*pzpass == NULL)
    {
      qglobal->ierrno = errno;
      return (UUCONF_MALLOC_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_CMDTABRET_EXIT);
    }

  return UUCONF_CMDTABRET_EXIT;
}
