/* rdlocs.c
   Get the locations of systems in the Taylor UUCP configuration files.

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
const char _uuconf_rdlocs_rcsid[] = "$FreeBSD$";
#endif

#include <errno.h>

static int itsystem P((pointer pglobal, int argc, char **argv,
		       pointer pvar, pointer pinfo));
static int itcalled_login P((pointer pglobal, int argc, char **argv,
			     pointer pvar, pointer pinfo));
static int itmyname P((pointer pglobal, int argc, char **argv,
		       pointer pvar, pointer pinfo));

/* This code scans through the Taylor UUCP system files in order to
   locate each system and to gather the login restrictions (since this
   information is held in additional arguments to the "called-login"
   command, it can appear anywhere in the systems files).  It also
   records whether any "myname" appears, as an optimization for
   uuconf_taylor_localname.

   This table is used to dispatch the appropriate commands.  Most
   commands are simply ignored.  Note that this is a uuconf_cmdtab,
   not a cmdtab_offset.  */

static const struct uuconf_cmdtab asTcmds[] =
{
  { "system", UUCONF_CMDTABTYPE_FN | 2, NULL, itsystem },
  { "alias", UUCONF_CMDTABTYPE_FN | 2, (pointer) asTcmds, itsystem },
  { "called-login", UUCONF_CMDTABTYPE_FN | 0, NULL, itcalled_login },
  { "myname", UUCONF_CMDTABTYPE_FN | 2, NULL, itmyname },
  { NULL, 0, NULL, NULL }
};

/* This structure is used to pass information into the command table
   functions.  */

struct sinfo
{
  /* The sys file name.  */
  const char *zname;
  /* The open sys file.  */
  FILE *e;
  /* The list of locations we are building.  */
  struct stsysloc *qlocs;
  /* The list of validation restrictions we are building.  */
  struct svalidate *qvals;
};

/* Look through the sys files to find the location and names of all
   the systems.  Since we're scanning the sys files, we also record
   the validation information specified by the additional arguments to
   the called-login command.  We don't use uuconf_cmd_file to avoid
   the overhead of breaking the line up into arguments if not
   necessary.  */

int
_uuconf_iread_locations (qglobal)
     struct sglobal *qglobal;
{
  char *zline;
  size_t cline;
  struct sinfo si;
  int iret;
  char **pz;

  if (qglobal->qprocess->fread_syslocs)
    return UUCONF_SUCCESS;

  zline = NULL;
  cline = 0;

  si.qlocs = NULL;
  si.qvals = NULL;

  iret = UUCONF_SUCCESS;

  for (pz = qglobal->qprocess->pzsysfiles; *pz != NULL; pz++)
    {
      FILE *e;
      int cchars;

      qglobal->ilineno = 0;

      e = fopen (*pz, "r");
      if (e == NULL)
	{
	  if (FNO_SUCH_FILE ())
	    continue;
	  qglobal->ierrno = errno;
	  iret = UUCONF_FOPEN_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

#ifdef CLOSE_ON_EXEC
      CLOSE_ON_EXEC (e);
#endif

      si.zname = *pz;
      si.e = e;

      while ((cchars = _uuconf_getline (qglobal, &zline, &cline, e)) > 0)
	{
	  char *zcmd;

	  ++qglobal->ilineno;

	  zcmd = zline + strspn (zline, " \t");
	  if (strncasecmp (zcmd, "system", sizeof "system" - 1) == 0
	      || strncasecmp (zcmd, "alias", sizeof "alias" - 1) == 0
	      || strncasecmp (zcmd, "called-login",
			      sizeof "called-login" - 1) == 0
	      || strncasecmp (zcmd, "myname", sizeof "myname" - 1) == 0)
	    {
	      iret = uuconf_cmd_line ((pointer) qglobal, zline, asTcmds,
				      (pointer) &si, (uuconf_cmdtabfn) NULL,
				      0, qglobal->pblock);
	      if ((iret & UUCONF_CMDTABRET_KEEP) != 0)
		{
		  iret &=~ UUCONF_CMDTABRET_KEEP;
		  zline = NULL;
		  cline = 0;
		}
	      if (iret != UUCONF_SUCCESS)
		{
		  iret &=~ UUCONF_CMDTABRET_EXIT;
		  break;
		}
	    }
	}

      if (iret != UUCONF_SUCCESS)
	break;
    }

  if (zline != NULL)
    free ((pointer) zline);

  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = *pz;
      iret |= UUCONF_ERROR_FILENAME | UUCONF_ERROR_LINENO;
      if (UUCONF_ERROR_VALUE (iret) != UUCONF_MALLOC_FAILED)
	qglobal->qprocess->fread_syslocs = TRUE;
    }
  else
    {
      qglobal->qprocess->qsyslocs = si.qlocs;
      qglobal->qprocess->qvalidate = si.qvals;
      qglobal->qprocess->fread_syslocs = TRUE;
    }	

  return iret;
}

/* Handle a "system" or "alias" command by recording the file and
   location.  If pvar is not NULL, this is an "alias" command.  */

/*ARGSUSED*/
static int
itsystem (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;
  struct stsysloc *q;
  size_t csize;

  q = (struct stsysloc *) uuconf_malloc (qglobal->pblock,
					 sizeof (struct stsysloc));
  if (q == NULL)
    {
      qglobal->ierrno = errno;
      return (UUCONF_MALLOC_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_CMDTABRET_EXIT);
    }

  csize = strlen (argv[1]) + 1;
  q->zname = uuconf_malloc (qglobal->pblock, csize);
  if (q->zname == NULL)
    {
      qglobal->ierrno = errno;
      return (UUCONF_MALLOC_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_CMDTABRET_EXIT);
    }

  q->qnext = qinfo->qlocs;
  memcpy ((pointer) q->zname, (pointer) argv[1], csize);
  q->falias = pvar != NULL;
  q->zfile = qinfo->zname;
  q->e = qinfo->e;
  q->iloc = ftell (qinfo->e);
  q->ilineno = qglobal->ilineno;

  qinfo->qlocs = q;

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Handle the "called-login" command.  This just records any extra
   arguments, so that uuconf_validate can check them later if
   necessary.  */

/*ARGSUSED*/
static int
itcalled_login (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;
  register struct svalidate *qval;
  int i;

  if (argc <= 2)
    return UUCONF_CMDTABRET_CONTINUE;

  for (qval = qinfo->qvals; qval != NULL; qval = qval->qnext)
    if (strcmp (argv[1], qval->zlogname) == 0)
      break;

  if (qval == NULL)
    {
      qval = (struct svalidate *) uuconf_malloc (qglobal->pblock,
						 sizeof (struct svalidate));
      if (qval == NULL)
	{
	  qglobal->ierrno = errno;
	  return (UUCONF_MALLOC_FAILED
		  | UUCONF_ERROR_ERRNO
		  | UUCONF_CMDTABRET_EXIT);
	}

      qval->qnext = qinfo->qvals;
      qval->zlogname = argv[1];
      qval->pzmachines = NULL;

      qinfo->qvals = qval;
    }

  for (i = 2; i < argc; i++)
    {
      int iret;

      iret = _uuconf_iadd_string (qglobal, argv[i], FALSE, TRUE,
				  &qval->pzmachines, qglobal->pblock);
      if (iret != UUCONF_SUCCESS)
	return iret | UUCONF_CMDTABRET_EXIT;
    }

  return UUCONF_CMDTABRET_KEEP;
}

/* Handle the "myname" command by simply recording that it appears.
   This information is used by uuconf_taylor_localname.  */

/*ARGSUSED*/
static int
itmyname (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;

  qglobal->qprocess->fuses_myname = TRUE;
  return UUCONF_CMDTABRET_CONTINUE;
}
