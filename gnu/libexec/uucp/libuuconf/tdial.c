/* tdial.c
   Find a dialer in the Taylor UUCP configuration files.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_tdial_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/tdial.c,v 1.6 1999/08/27 23:33:31 peter Exp $";
#endif

#include <errno.h>

static int iddialer P((pointer pglobal, int argc, char **argv, pointer pvar,
		       pointer pinfo));
static int idunknown P((pointer pglobal, int argc, char **argv, pointer pvar,
			pointer pinfo));

/* Find a dialer in the Taylor UUCP configuration files by name.  */

int
uuconf_taylor_dialer_info (pglobal, zname, qdialer)
     pointer pglobal;
     const char *zname;
     struct uuconf_dialer *qdialer;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  FILE *e;
  pointer pblock;
  int iret;
  char **pz;

  e = NULL;
  pblock = NULL;
  iret = UUCONF_NOT_FOUND;

  for (pz = qglobal->qprocess->pzdialfiles; *pz != NULL; pz++)
    {
      struct uuconf_cmdtab as[2];
      char *zdialer;
      struct uuconf_dialer sdefault;
      int ilineno;

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

      /* Gather the default information from the top of the file.  We
	 do this by handling the "dialer" command ourselves and
	 passing every other command to _uuconf_idialer_cmd via
	 idunknown.  The value of zdialer will be an malloc block.  */
      as[0].uuconf_zcmd = "dialer";
      as[0].uuconf_itype = UUCONF_CMDTABTYPE_FN | 2;
      as[0].uuconf_pvar = (pointer) &zdialer;
      as[0].uuconf_pifn = iddialer;

      as[1].uuconf_zcmd = NULL;

      pblock = uuconf_malloc_block ();
      if (pblock == NULL)
	{
	  qglobal->ierrno = errno;
	  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

      _uuconf_uclear_dialer (&sdefault);
      sdefault.uuconf_palloc = pblock;
      zdialer = NULL;
      iret = uuconf_cmd_file (pglobal, e, as, (pointer) &sdefault,
			      idunknown, UUCONF_CMDTABFLAG_BACKSLASH,
			      pblock);

      /* Now skip until we find a dialer with a matching name.  */
      while (iret == UUCONF_SUCCESS
	     && zdialer != NULL
	     && strcmp (zname, zdialer) != 0)
	{
	  free ((pointer) zdialer);
	  zdialer = NULL;
	  ilineno = qglobal->ilineno;
	  iret = uuconf_cmd_file (pglobal, e, as, (pointer) NULL,
				  (uuconf_cmdtabfn) NULL,
				  UUCONF_CMDTABFLAG_BACKSLASH,
				  pblock);
	  qglobal->ilineno += ilineno;
	}

      if (iret != UUCONF_SUCCESS)
	{
	  if (zdialer != NULL)
	    free ((pointer) zdialer);
	  break;
	}

      if (zdialer != NULL)
	{
	  size_t csize;

	  /* We've found the dialer we're looking for.  Read the rest
	     of the commands for it.  */
	  as[0].uuconf_pvar = NULL;

	  *qdialer = sdefault;
	  csize = strlen (zdialer) + 1;
	  qdialer->uuconf_zname = uuconf_malloc (pblock, csize);
	  if (qdialer->uuconf_zname == NULL)
	    {
	      qglobal->ierrno = errno;
	      free ((pointer) zdialer);
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }
	  memcpy ((pointer) qdialer->uuconf_zname, (pointer) zdialer,
		  csize);
	  free ((pointer) zdialer);

	  ilineno = qglobal->ilineno;
	  iret = uuconf_cmd_file (pglobal, e, as, qdialer, idunknown,
				  UUCONF_CMDTABFLAG_BACKSLASH, pblock);
	  qglobal->ilineno += ilineno;
	  break;
	}

      (void) fclose (e);
      e = NULL;
      uuconf_free_block (pblock);
      pblock = NULL;

      iret = UUCONF_NOT_FOUND;
    }

  if (e != NULL)
    (void) fclose (e);
  if (iret != UUCONF_SUCCESS && pblock != NULL)
    uuconf_free_block (pblock);

  if (iret != UUCONF_SUCCESS && iret != UUCONF_NOT_FOUND)
    {
      qglobal->zfilename = *pz;
      iret |= UUCONF_ERROR_FILENAME;
    }

  return iret;
}

/* Handle a "dialer" command.  This copies the string onto the heap
   and returns the pointer in *pvar, unless pvar is NULL.  It returns
   UUCONF_CMDTABRET_EXIT to force _uuconf_icmd_file_internal to stop
   reading and return to the code above, which will then check the
   dialer name just read to see if it matches.  */

/*ARGSUSED*/
static int
iddialer (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char **pz = (char **) pvar;

  if (pz != NULL)
    {
      size_t csize;

      csize = strlen (argv[1]) + 1;
      *pz = malloc (csize);
      if (*pz == NULL)
	{
	  qglobal->ierrno = errno;
	  return (UUCONF_MALLOC_FAILED
		  | UUCONF_ERROR_ERRNO
		  | UUCONF_CMDTABRET_EXIT);
	}
      memcpy ((pointer) *pz, (pointer) argv[1], csize);
    }
  return UUCONF_CMDTABRET_EXIT;
}

/* Handle an unknown command by passing it on to _uuconf_idialer_cmd,
   which will parse it into the dialer structure. */

/*ARGSUSED*/
static int
idunknown (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_dialer *qdialer = (struct uuconf_dialer *) pinfo;

  return _uuconf_idialer_cmd (qglobal, argc, argv, qdialer);
}
