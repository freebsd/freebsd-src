/* tinit.c
   Initialize for reading Taylor UUCP configuration files.

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
const char _uuconf_tinit_rcsid[] = "$Id: tinit.c,v 1.1 1993/08/05 18:26:08 conklin Exp $";
#endif

#include <errno.h>

/* Local functions.  */

static int itset_default P((struct sglobal *qglobal, char ***ppzvar,
			    const char *zfile));
static int itadd P((pointer pglobal, int argc, char **argv, pointer pvar,
		    pointer pinfo));
static int itunknown P((pointer pglobal, int argc, char **argv, pointer pvar,
			pointer pinfo));
static int itprogram P((pointer pglobal, int argc, char **argv, pointer pvar,
			pointer pinfo));

static const struct cmdtab_offset asCmds[] =
{
  { "nodename", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zlocalname), NULL },
  { "hostname", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zlocalname), NULL },
  { "uuname", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zlocalname), NULL },
  { "spool", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zspooldir), NULL },
  { "pubdir", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zpubdir), NULL },
  { "lockdir", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zlockdir), NULL },
  { "logfile", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zlogfile), NULL },
  { "statfile", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zstatsfile), NULL },
  { "debugfile", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zdebugfile), NULL },
  { "debug", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct sprocess, zdebug), NULL },
  { "max-uuxqts", UUCONF_CMDTABTYPE_INT,
      offsetof (struct sprocess, cmaxuuxqts), NULL },
  { "sysfile", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct sprocess, pzsysfiles), itadd },
  { "portfile", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct sprocess, pzportfiles), itadd },
  { "dialfile", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct sprocess, pzdialfiles), itadd },
  { "dialcodefile", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct sprocess, pzdialcodefiles), itadd },
  { "callfile", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct sprocess, pzcallfiles), itadd },
  { "passwdfile", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct sprocess, pzpwdfiles), itadd },
  { "unknown", UUCONF_CMDTABTYPE_FN, offsetof (struct sprocess, qunknown),
      itunknown },
  { "v2-files", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct sprocess, fv2), NULL },
  { "hdb-files", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct sprocess, fhdb), NULL },
  { "bnu-files", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct sprocess, fhdb), NULL },
  { "timetable", UUCONF_CMDTABTYPE_FN | 3,
      offsetof (struct sprocess, pztimetables), _uuconf_itimetable },
  { NULL, 0, 0, NULL }
};

#define CCMDS (sizeof asCmds / sizeof asCmds[0])

/* This structure is used to pass information into the command table
   functions.  */

struct sinfo
{
  /* The program name.  */
  const char *zname;
  /* A pointer to the command table being used, passed to isystem so
     it can call uuconf_cmd_args.  */
  struct uuconf_cmdtab *qcmds;
};

/* Initialize the routines which read the Taylor UUCP configuration
   files.  */

int
uuconf_taylor_init (ppglobal, zprogram, zname)
     pointer *ppglobal;
     const char *zprogram;
     const char *zname;
{
  struct sglobal **pqglobal = (struct sglobal **) ppglobal;
  int iret;
  char *zcopy;
  struct sglobal *qglobal;
  boolean fdefault;
  FILE *e;
  struct sinfo si;

  if (*pqglobal == NULL)
    {
      iret = _uuconf_iinit_global (pqglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  qglobal = *pqglobal;

  if (zname != NULL)
    {
      size_t csize;

      csize = strlen (zname) + 1;
      zcopy = uuconf_malloc (qglobal->pblock, csize);
      if (zcopy == NULL)
	{
	  qglobal->ierrno = errno;
	  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	}
      memcpy ((pointer) zcopy, (pointer) zname, csize);
      fdefault = FALSE;
    }
  else
    {
      zcopy = uuconf_malloc (qglobal->pblock,
			     sizeof NEWCONFIGLIB + sizeof CONFIGFILE - 1);
      if (zcopy == NULL)
	{
	  qglobal->ierrno = errno;
	  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	}
      memcpy ((pointer) zcopy, (pointer) NEWCONFIGLIB,
	      sizeof NEWCONFIGLIB - 1);
      memcpy ((pointer) (zcopy + sizeof NEWCONFIGLIB - 1),
	      (pointer) CONFIGFILE, sizeof CONFIGFILE);
      fdefault = TRUE;
    }

  qglobal->qprocess->zconfigfile = zcopy;

  e = fopen (zcopy, "r");
  if (e == NULL)
    {
      if (! fdefault)
	{
	  qglobal->ierrno = errno;
	  qglobal->zfilename = zcopy;
	  return (UUCONF_FOPEN_FAILED
		  | UUCONF_ERROR_ERRNO
		  | UUCONF_ERROR_FILENAME);
	}

      /* There is no config file, so just use the default values.  */
    }
  else
    {
      struct uuconf_cmdtab as[CCMDS];

      _uuconf_ucmdtab_base (asCmds, CCMDS, (char *) qglobal->qprocess,
			    as);

      if (zprogram == NULL)
	zprogram = "uucp";

      si.zname = zprogram;
      si.qcmds = as;
      iret = uuconf_cmd_file (qglobal, e, as, (pointer) &si, itprogram,
			      UUCONF_CMDTABFLAG_BACKSLASH,
			      qglobal->pblock);

      (void) fclose (e);

      if (iret != UUCONF_SUCCESS)
	{
	  qglobal->zfilename = zcopy;
	  return iret | UUCONF_ERROR_FILENAME;
	}
    }

  /* Get the defaults for the file names.  */

  iret = itset_default (qglobal, &qglobal->qprocess->pzsysfiles, SYSFILE);
  if (iret != UUCONF_SUCCESS)
    return iret;
  iret = itset_default (qglobal, &qglobal->qprocess->pzportfiles, PORTFILE);
  if (iret != UUCONF_SUCCESS)
    return iret;
  iret = itset_default (qglobal, &qglobal->qprocess->pzdialfiles, DIALFILE);
  if (iret != UUCONF_SUCCESS)
    return iret;
  iret = itset_default (qglobal, &qglobal->qprocess->pzdialcodefiles,
			DIALCODEFILE);
  if (iret != UUCONF_SUCCESS)
    return iret;
  iret = itset_default (qglobal, &qglobal->qprocess->pzpwdfiles, PASSWDFILE);
  if (iret != UUCONF_SUCCESS)
    return iret;
  iret = itset_default (qglobal, &qglobal->qprocess->pzcallfiles, CALLFILE);
  if (iret != UUCONF_SUCCESS)
    return iret;

  return UUCONF_SUCCESS;
}

/* Add new strings to a variable.  */

/*ARGSUSED*/
static int
itadd (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char ***ppz = (char ***) pvar;
  int i;
  int iret;

  if (argc == 1)
    {
      iret = _uuconf_iadd_string (qglobal, NULL, FALSE, FALSE, ppz,
				  qglobal->pblock);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }
  else
    {
      for (i = 1; i < argc; i++)
	{
	  iret = _uuconf_iadd_string (qglobal, argv[i], TRUE, FALSE, ppz,
				      qglobal->pblock);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	}
    }

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Handle an "unknown" command.  We accumulate this into a linked
   list, and only parse them later in uuconf_unknown_system_info.  */

/*ARGSUSED*/
static int
itunknown (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sunknown **pq = (struct sunknown **) pvar;
  struct sunknown *q;

  q = (struct sunknown *) uuconf_malloc (qglobal->pblock,
					 sizeof (struct sunknown));
  if (q == NULL)
    {
      qglobal->ierrno = errno;
      return (UUCONF_MALLOC_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_CMDTABRET_EXIT);
    }
  q->qnext = NULL;
  q->ilineno = qglobal->ilineno;
  q->cargs = argc - 1;
  q->pzargs = (char **) uuconf_malloc (qglobal->pblock,
				       (argc - 1) * sizeof (char *));
  if (q->pzargs == NULL)
    {
      qglobal->ierrno = errno;
      return (UUCONF_MALLOC_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_CMDTABRET_EXIT);
    }
  memcpy ((pointer) q->pzargs, (pointer) (argv + 1),
	  (argc - 1) * sizeof (char *));

  while (*pq != NULL)
    pq = &(*pq)->qnext;

  *pq = q;

  return UUCONF_CMDTABRET_KEEP;
}

/* If we encounter an unknown command, see if it is the program with
   which we were invoked.  If it was, pass the remaining arguments
   back through the table.  */

/*ARGSUSED*/
static int
itprogram (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;

  if (argc <= 1
      || strcasecmp (qinfo->zname, argv[0]) != 0)
    return UUCONF_CMDTABRET_CONTINUE;

  return uuconf_cmd_args (pglobal, argc - 1, argv + 1, qinfo->qcmds,
			  (pointer) NULL, (uuconf_cmdtabfn) NULL, 0,
			  qglobal->pblock);
}

/* If a filename was not set by the configuration file, add in the
   default value.  */

static int
itset_default (qglobal, ppzvar, zfile)
     struct sglobal *qglobal;
     char ***ppzvar;
     const char *zfile;
{
  size_t clen;
  char *zadd;

  if (*ppzvar != NULL)
    return UUCONF_SUCCESS;

  clen = strlen (zfile);
  zadd = (char *) uuconf_malloc (qglobal->pblock,
				 sizeof NEWCONFIGLIB + clen);
  if (zadd == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  memcpy ((pointer) zadd, (pointer) NEWCONFIGLIB, sizeof NEWCONFIGLIB - 1);
  memcpy ((pointer) (zadd + sizeof NEWCONFIGLIB - 1), (pointer) zfile,
	  clen + 1);

  return _uuconf_iadd_string (qglobal, zadd, FALSE, FALSE, ppzvar,
			      qglobal->pblock);
}
