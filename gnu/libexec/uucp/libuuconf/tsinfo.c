/* tsinfo.c
   Get information about a system from the Taylor UUCP configuration files.

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
const char _uuconf_tsinfo_rcsid[] = "$Id: tsinfo.c,v 1.1 1993/08/05 18:26:13 conklin Exp $";
#endif

#include <errno.h>
#include <ctype.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

static void uiset_call P((struct uuconf_system *qsys));
static int iisizecmp P((long i1, long i2));

/* Local functions needed to parse the system information file.  */

#define CMDTABFN(z) \
  static int z P((pointer, int, char **, pointer, pointer))

CMDTABFN (iisystem);
CMDTABFN (iialias);
CMDTABFN (iialternate);
CMDTABFN (iidefault_alternates);
CMDTABFN (iitime);
CMDTABFN (iitimegrade);
CMDTABFN (iisize);
CMDTABFN (iibaud_range);
CMDTABFN (iiport);
CMDTABFN (iichat);
CMDTABFN (iicalled_login);
CMDTABFN (iiproto_param);
CMDTABFN (iirequest);
CMDTABFN (iitransfer);
CMDTABFN (iiforward);
CMDTABFN (iiunknown);

#undef CMDTABFN

/* We have to pass a fair amount of information in and out of the
   various system commands.  Using global variables would make the
   code non-reentrant, so we instead pass a pointer to single
   structure as the pinfo argument to the system commands.  */

struct sinfo
{
  /* The system information we're building up.  */
  struct uuconf_system *qsys;
  /* Whether any alternates have been used.  */
  boolean falternates;
  /* A list of the previous alternates.  */
  struct uuconf_system salternate;
  /* Whether to use extra alternates from the file wide defaults.  */
  int fdefault_alternates;
};

/* The command table for system commands.  */
static const struct cmdtab_offset asIcmds[] =
{
  { "system", UUCONF_CMDTABTYPE_FN | 2, (size_t) -1, iisystem },
  { "alias", UUCONF_CMDTABTYPE_FN | 2, (size_t) -1, iialias },
  { "alternate", UUCONF_CMDTABTYPE_FN | 0, (size_t) -1, iialternate },
  { "default-alternates", UUCONF_CMDTABTYPE_FN | 2, (size_t) -1,
      iidefault_alternates },
  { "time", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct uuconf_system, uuconf_qtimegrade), iitime },
  { "timegrade", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct uuconf_system, uuconf_qtimegrade), iitimegrade },
  { "max-retries", UUCONF_CMDTABTYPE_INT,
      offsetof (struct uuconf_system, uuconf_cmax_retries), NULL },
  { "success-wait", UUCONF_CMDTABTYPE_INT,
      offsetof (struct uuconf_system, uuconf_csuccess_wait), NULL },
  { "call-timegrade", UUCONF_CMDTABTYPE_FN | 3,
      offsetof (struct uuconf_system, uuconf_qcalltimegrade), iitimegrade },
  { "call-local-size", UUCONF_CMDTABTYPE_FN | 3,
      offsetof (struct uuconf_system, uuconf_qcall_local_size), iisize },
  { "call-remote-size", UUCONF_CMDTABTYPE_FN | 3,
      offsetof (struct uuconf_system, uuconf_qcall_remote_size), iisize },
  { "called-local-size", UUCONF_CMDTABTYPE_FN | 3,
      offsetof (struct uuconf_system, uuconf_qcalled_local_size), iisize },
  { "called-remote-size", UUCONF_CMDTABTYPE_FN | 3,
      offsetof (struct uuconf_system, uuconf_qcalled_remote_size), iisize },
  { "timetable", UUCONF_CMDTABTYPE_FN | 3, (size_t) -1, _uuconf_itimetable },
  { "baud", UUCONF_CMDTABTYPE_LONG,
      offsetof (struct uuconf_system, uuconf_ibaud), NULL },
  { "speed", UUCONF_CMDTABTYPE_LONG,
      offsetof (struct uuconf_system, uuconf_ibaud), NULL },
  { "baud-range", UUCONF_CMDTABTYPE_FN | 3, 0, iibaud_range },
  { "speed-range", UUCONF_CMDTABTYPE_FN | 3, 0, iibaud_range },
  { "port", UUCONF_CMDTABTYPE_FN | 0, (size_t) -1, iiport },
  { "phone", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_system, uuconf_zphone), NULL },
  { "address", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_system, uuconf_zphone), NULL },
  { "chat", UUCONF_CMDTABTYPE_PREFIX | 0,
      offsetof (struct uuconf_system, uuconf_schat), iichat },
  { "call-login", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_system, uuconf_zcall_login), NULL },
  { "call-password", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_system, uuconf_zcall_password), NULL },
  { "called-login", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct uuconf_system, uuconf_zcalled_login), iicalled_login },
  { "callback", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_system, uuconf_fcallback), NULL },
  { "sequence", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_system, uuconf_fsequence), NULL },
  { "protocol", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_system, uuconf_zprotocols), NULL },
  { "protocol-parameter", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct uuconf_system, uuconf_qproto_params), iiproto_param },
  { "called-chat", UUCONF_CMDTABTYPE_PREFIX | 0,
      offsetof (struct uuconf_system, uuconf_scalled_chat), iichat },
  { "debug", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_system, uuconf_zdebug), NULL },
  { "max-remote-debug", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_system, uuconf_zmax_remote_debug), NULL },
  { "send-request", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_system, uuconf_fsend_request), NULL },
  { "receive-request", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_system, uuconf_frec_request), NULL },
  { "request", UUCONF_CMDTABTYPE_FN | 2, (size_t) -1, iirequest },
  { "call-transfer", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_system, uuconf_fcall_transfer), NULL },
  { "called-transfer", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_system, uuconf_fcalled_transfer), NULL },
  { "transfer", UUCONF_CMDTABTYPE_FN | 2, (size_t) -1, iitransfer },
  { "local-send", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_system, uuconf_pzlocal_send), NULL },
  { "remote-send", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_system, uuconf_pzremote_send), NULL },
  { "local-receive", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_system, uuconf_pzlocal_receive), NULL },
  { "remote-receive", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_system, uuconf_pzremote_receive), NULL },
  { "command-path", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_system, uuconf_pzpath), NULL },
  { "commands", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_system, uuconf_pzcmds), NULL },
  { "free-space", UUCONF_CMDTABTYPE_LONG,
      offsetof (struct uuconf_system, uuconf_cfree_space), NULL },
  { "forward-from", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_system, uuconf_pzforward_from), NULL },
  { "forward-to", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_system, uuconf_pzforward_to), NULL },
  { "forward", UUCONF_CMDTABTYPE_FN | 0, (size_t) -1, iiforward },
  { "pubdir", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_system, uuconf_zpubdir), NULL },
  { "myname", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_system, uuconf_zlocalname), NULL },
  { NULL, 0, 0, NULL }
};

#define CSYSTEM_CMDS (sizeof asIcmds / sizeof asIcmds[0])

/* Get information about the system zsystem from the Taylor UUCP
   configuration files.  Sets *qsys.  This does not ensure that all
   default information is set.  */

int
_uuconf_itaylor_system_internal (qglobal, zsystem, qsys)
     struct sglobal *qglobal;
     const char *zsystem;
     struct uuconf_system *qsys;
{
  int iret;
  struct stsysloc *qloc;
  struct uuconf_cmdtab as[CSYSTEM_CMDS];
  struct sinfo si;
  struct uuconf_system sdefaults;

  if (! qglobal->qprocess->fread_syslocs)
    {
      iret = _uuconf_iread_locations (qglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  /* Find the system in the list of locations.  */
  for (qloc = qglobal->qprocess->qsyslocs; qloc != NULL; qloc = qloc->qnext)
    if (qloc->zname[0] == zsystem[0]
	&& strcmp (qloc->zname, zsystem) == 0)
      break;
  if (qloc == NULL)
    return UUCONF_NOT_FOUND;

  /* If this is an alias, then the real system is the next non-alias
     in the list.  */
  while (qloc->falias)
    {
      qloc = qloc->qnext;
      if (qloc == NULL)
	return UUCONF_NOT_FOUND;
    }

  _uuconf_ucmdtab_base (asIcmds, CSYSTEM_CMDS, (char *) qsys, as);

  rewind (qloc->e);

  /* Read the file wide defaults from the start of the file.  */
  _uuconf_uclear_system (qsys);

  si.qsys = qsys;
  si.falternates = FALSE;
  si.fdefault_alternates = TRUE;
  qsys->uuconf_palloc = uuconf_malloc_block ();
  if (qsys->uuconf_palloc == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  iret = uuconf_cmd_file ((pointer) qglobal, qloc->e, as, (pointer) &si,
			  iiunknown, UUCONF_CMDTABFLAG_BACKSLASH,
			  qsys->uuconf_palloc);
  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = qloc->zfile;
      return iret | UUCONF_ERROR_FILENAME;
    }

  if (! si.falternates)
    uiset_call (qsys);
  else
    {
      /* Attach the final alternate.  */
      iret = iialternate ((pointer) qglobal, 0, (char **) NULL,
			  (pointer) NULL, (pointer) &si);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  /* Save off the defaults.  */
  sdefaults = *qsys;

  /* Advance to the information for the system we want.  */
  if (fseek (qloc->e, qloc->iloc, SEEK_SET) != 0)
    {
      qglobal->ierrno = errno;
      qglobal->zfilename = qloc->zfile;
      return (UUCONF_FSEEK_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_ERROR_FILENAME);
    }

  /* Read in the system we want.  */
  _uuconf_uclear_system (qsys);
  qsys->uuconf_zname = (char *) qloc->zname;
  qsys->uuconf_palloc = sdefaults.uuconf_palloc;

  si.falternates = FALSE;

  iret = uuconf_cmd_file (qglobal, qloc->e, as, (pointer) &si, iiunknown,
			  UUCONF_CMDTABFLAG_BACKSLASH, qsys->uuconf_palloc);
  qglobal->ilineno += qloc->ilineno;

  if (iret == UUCONF_SUCCESS)
    {
      if (! si.falternates)
	uiset_call (qsys);
      else
	iret = iialternate ((pointer) qglobal, 0, (char **) NULL,
			    (pointer) NULL, (pointer) &si);
    }

  /* Merge in the defaults.  */
  if (iret == UUCONF_SUCCESS)
    iret = _uuconf_isystem_default (qglobal, qsys, &sdefaults,
				    si.fdefault_alternates);

  /* The first alternate is always available for calling in.  */
  if (iret == UUCONF_SUCCESS)
    qsys->uuconf_fcalled = TRUE;

  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = qloc->zfile;
      iret |= UUCONF_ERROR_FILENAME;
    }

  return iret;
}

/* Set the fcall and fcalled field for the system.  This marks a
   particular alternate for use when calling out or calling in.  This
   is where we implement the semantics described in the documentation:
   a change to a relevant field implies that the alternate is used.
   If all the relevant fields are unchanged, the alternate is not
   used.  */

static void
uiset_call (qsys)
     struct uuconf_system *qsys;
{
  qsys->uuconf_fcall =
    (qsys->uuconf_qtimegrade != (struct uuconf_timespan *) &_uuconf_unset
     || qsys->uuconf_zport != (char *) &_uuconf_unset
     || qsys->uuconf_qport != (struct uuconf_port *) &_uuconf_unset
     || qsys->uuconf_ibaud >= 0
     || qsys->uuconf_zphone != (char *) &_uuconf_unset
     || qsys->uuconf_schat.uuconf_pzchat != (char **) &_uuconf_unset
     || qsys->uuconf_schat.uuconf_pzprogram != (char **) &_uuconf_unset);

  qsys->uuconf_fcalled =
    qsys->uuconf_zcalled_login != (char *) &_uuconf_unset;
}

/* Handle the "system" command.  Because we skip directly to the
   system we want to read, a "system" command means we've reached the
   end of it.  */

static int
iisystem (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  return UUCONF_CMDTABRET_EXIT;
}

/* Handle the "alias" command.  */

/*ARGSUSED*/
static int
iialias (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;
  int iret;

  iret = _uuconf_iadd_string (qglobal, argv[1], TRUE, FALSE,
			      &qinfo->qsys->uuconf_pzalias,
			      qinfo->qsys->uuconf_palloc);
  if (iret != UUCONF_SUCCESS)
    iret |= UUCONF_CMDTABRET_EXIT;
  return iret;
}

/* Handle the "alternate" command.  The information just read is in
   sIhold.  If this is the first "alternate" command for this system,
   we save off the current information in sIalternate.  Otherwise we
   default this information to sIalternate, and then add it to the end
   of the list of alternates in sIalternate.  */

static int
iialternate (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;

  uiset_call (qinfo->qsys);

  if (! qinfo->falternates)
    {
      qinfo->salternate = *qinfo->qsys;
      qinfo->falternates = TRUE;
    }
  else
    {
      int iret;
      struct uuconf_system *qnew, **pq;

      iret = _uuconf_isystem_default (qglobal, qinfo->qsys,
				      &qinfo->salternate, FALSE);
      if (iret != UUCONF_SUCCESS)
	return iret | UUCONF_CMDTABRET_EXIT;
      qnew = ((struct uuconf_system *)
	      uuconf_malloc (qinfo->qsys->uuconf_palloc,
			      sizeof (struct uuconf_system)));
      if (qnew == NULL)
	{
	  qglobal->ierrno = errno;;
	  return (UUCONF_MALLOC_FAILED
		  | UUCONF_ERROR_ERRNO
		  | UUCONF_CMDTABRET_EXIT);
	}
      *qnew = *qinfo->qsys;
      for (pq = &qinfo->salternate.uuconf_qalternate;
	   *pq != NULL;
	   pq = &(*pq)->uuconf_qalternate)
	;
      *pq = qnew;
    }

  /* If this is the last alternate command, move the information back
     to qinfo->qsys.  */
  if (argc == 0)
    *qinfo->qsys = qinfo->salternate;
  else
    {
      _uuconf_uclear_system (qinfo->qsys);
      qinfo->qsys->uuconf_zname = qinfo->salternate.uuconf_zname;
      qinfo->qsys->uuconf_palloc = qinfo->salternate.uuconf_palloc;
      if (argc > 1)
	{
	  qinfo->qsys->uuconf_zalternate = argv[1];
	  return UUCONF_CMDTABRET_KEEP;
	}
    }

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Handle the "default-alternates" command.  This just takes a boolean
   argument which is used to set the fdefault_alternates field of the
   sinfo structure.  */

/*ARGSUSED*/
static int
iidefault_alternates (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;

  return _uuconf_iboolean (qglobal, argv[1], &qinfo->fdefault_alternates);
}

/* Handle the "time" command.  We do this by turning it into a
   "timegrade" command with a grade of BGRADE_LOW.  The first argument
   is a time string, and the optional second argument is the retry
   time.  */

/*ARGSUSED*/
static int
iitime (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  char *aznew[4];
  char ab[2];

  if (argc != 2 && argc != 3)
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

  aznew[0] = argv[0];
  ab[0] = UUCONF_GRADE_LOW;
  ab[1] = '\0';
  aznew[1] = ab;
  aznew[2] = argv[1];
  if (argc > 2)
    aznew[3] = argv[2];

  return iitimegrade (pglobal, argc + 1, aznew, pvar, pinfo);
}

/* Handle the "timegrade" command by calling _uuconf_itime_parse with
   appropriate ival (the work grade) and cretry (the retry time)
   arguments.  */

static int
iitimegrade (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_timespan **pqspan = (struct uuconf_timespan **) pvar;
  struct sinfo *qinfo = (struct sinfo *) pinfo;
  int cretry;
  int iret;

  if (argc < 3 || argc > 4)
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

  if (argv[1][1] != '\0' || ! UUCONF_GRADE_LEGAL (argv[1][0]))
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

  if (argc == 3)
    cretry = 0;
  else
    {
      iret = _uuconf_iint (qglobal, argv[3], (pointer) &cretry, TRUE);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  iret = _uuconf_itime_parse (qglobal, argv[2], (long) argv[1][0],
			      cretry, _uuconf_itime_grade_cmp, pqspan,
			      qinfo->qsys->uuconf_palloc);
  if (iret != UUCONF_SUCCESS)
    iret |= UUCONF_CMDTABRET_EXIT;
  return iret;
}

/* Handle the "baud-range" command, also known as "speed-range".  */

static int
iibaud_range (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_system *qsys = (struct uuconf_system *) pvar;
  int iret;

  iret = _uuconf_iint (qglobal, argv[1], (pointer) &qsys->uuconf_ibaud,
		       FALSE);
  if (iret != UUCONF_SUCCESS)
    return iret;
  return _uuconf_iint (qglobal, argv[2], (pointer) &qsys->uuconf_ihighbaud,
		       FALSE);
}

/* Handle one of the size commands ("call-local-size", etc.).  The
   first argument is a number of bytes, and the second argument is a
   time string.  The pvar argument points to the string array to which
   we add this new string.  */

/*ARGSUSED*/
static int
iisize (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_timespan **pqspan = (struct uuconf_timespan **) pvar;
  struct sinfo *qinfo = (struct sinfo *) pinfo;
  long ival;
  int iret;

  iret = _uuconf_iint (qglobal, argv[1], (pointer) &ival, FALSE);
  if (iret != UUCONF_SUCCESS)
    return iret;

  iret = _uuconf_itime_parse (qglobal, argv[2], ival, 0, iisizecmp,
			      pqspan, qinfo->qsys->uuconf_palloc);
  if (iret != UUCONF_SUCCESS)
    iret |= UUCONF_CMDTABRET_EXIT;
  return iret;
}

/* A comparison function for sizes to pass to _uuconf_itime_parse.  */

static int
iisizecmp (i1, i2)
     long i1;
     long i2;
{
  /* We can't just return i1 - i2 because that would be a long.  */
  if (i1 < i2)
    return -1;
  else if (i1 == i2)
    return 0;
  else
    return 1;
}

/* Handle the "port" command.  If there is one argument, this names a
   port.  Otherwise, the remaining arguments form a command describing
   the port.  */

/*ARGSUSED*/
static int
iiport (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;

  if (argc < 2)
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;
  else if (argc == 2)
    {
      qinfo->qsys->uuconf_zport = argv[1];
      return UUCONF_CMDTABRET_KEEP;
    }
  else
    {
      int iret;

      if (qinfo->qsys->uuconf_qport
	  == (struct uuconf_port *) &_uuconf_unset)
	{
	  struct uuconf_port *qnew;

	  qnew = ((struct uuconf_port *)
		  uuconf_malloc (qinfo->qsys->uuconf_palloc,
				  sizeof (struct uuconf_port)));
	  if (qnew == NULL)
	    {
	      qglobal->ierrno = errno;
	      return (UUCONF_MALLOC_FAILED
		      | UUCONF_ERROR_ERRNO
		      | UUCONF_CMDTABRET_EXIT);
	    }

	  _uuconf_uclear_port (qnew);

	  if (qinfo->qsys->uuconf_zname == NULL)
	    qnew->uuconf_zname = (char *) "default system file port";
	  else
	    {
	      char *zname;
	      size_t clen;

	      clen = strlen (qinfo->qsys->uuconf_zname);
	      zname = (char *) uuconf_malloc (qinfo->qsys->uuconf_palloc,
					      clen + sizeof "system  port");
	      if (zname == NULL)
		{
		  qglobal->ierrno = errno;
		  return (UUCONF_MALLOC_FAILED
			  | UUCONF_ERROR_ERRNO
			  | UUCONF_CMDTABRET_EXIT);
		}

	      memcpy ((pointer) zname, (pointer) "system ",
		      sizeof "system " - 1);
	      memcpy ((pointer) (zname + sizeof "system " - 1),
		      (pointer) qinfo->qsys->uuconf_zname,
		      clen);
	      memcpy ((pointer) (zname + sizeof "system " - 1 + clen),
		      (pointer) " port", sizeof " port");

	      qnew->uuconf_zname = zname;
	    }

	  qnew->uuconf_palloc = qinfo->qsys->uuconf_palloc;

	  qinfo->qsys->uuconf_qport = qnew;
	}

      iret = _uuconf_iport_cmd (qglobal, argc - 1, argv + 1,
				qinfo->qsys->uuconf_qport);
      if (UUCONF_ERROR_VALUE (iret) != UUCONF_SUCCESS)
	iret |= UUCONF_CMDTABRET_EXIT;
      return iret;
    }
}

/* Handle the "chat" and "called-chat" set of commands.  These just
   hand off to the generic chat script function.  */

static int
iichat (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;
  struct uuconf_chat *qchat = (struct uuconf_chat *) pvar;
  int iret;

  iret = _uuconf_ichat_cmd (qglobal, argc, argv, qchat,
			    qinfo->qsys->uuconf_palloc);
  if (UUCONF_ERROR_VALUE (iret) != UUCONF_SUCCESS)
    iret |= UUCONF_CMDTABRET_EXIT;
  return iret;
}

/* Handle the "called-login" command.  This only needs to be in a
   function because there can be additional arguments listing the
   remote systems which are permitted to use this login name.  The
   additional arguments are not actually handled here; they are
   handled by uuconf_taylor_system_names, which already has to go
   through all the system files.  */

/*ARGSUSED*/
static int
iicalled_login (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  char **pz = (char **) pvar;

  if (argc < 2)
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;
  *pz = argv[1];
  return UUCONF_CMDTABRET_KEEP;
}

/* Handle the "protocol-parameter" command.  This just hands off to
   the generic protocol parameter handler.  */

static int
iiproto_param (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_proto_param **pqparam = (struct uuconf_proto_param **) pvar;
  struct sinfo *qinfo = (struct sinfo *) pinfo;

  if (*pqparam == (struct uuconf_proto_param *) &_uuconf_unset)
    *pqparam = NULL;
  return _uuconf_iadd_proto_param (qglobal, argc - 1, argv + 1, pqparam,
				   qinfo->qsys->uuconf_palloc);
}

/* Handle the "request" command.  This is equivalent to specifying
   both "call-request" and "called-request".  */

/*ARGSUSED*/
static int
iirequest (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;
  int iret;

  iret = _uuconf_iboolean (qglobal, argv[1],
			   &qinfo->qsys->uuconf_fsend_request);
  if (UUCONF_ERROR_VALUE (iret) == UUCONF_SUCCESS)
    qinfo->qsys->uuconf_frec_request = qinfo->qsys->uuconf_fsend_request;

  return iret;
}

/* Handle the "transfer" command.  This is equivalent to specifying
   both "call-transfer" and "called-transfer".  */

/*ARGSUSED*/
static int
iitransfer (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;
  int iret;

  iret = _uuconf_iboolean (qglobal, argv[1],
			   &qinfo->qsys->uuconf_fcall_transfer);
  if (UUCONF_ERROR_VALUE (iret) == UUCONF_SUCCESS)
    qinfo->qsys->uuconf_fcalled_transfer = qinfo->qsys->uuconf_fcall_transfer;

  return iret;
}

/* Handle the "forward" command.  This is equivalent to specifying
   both "forward-from" and "forward-to".  */

/*ARGSUSED*/
static int
iiforward (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *qinfo = (struct sinfo *) pinfo;
  struct uuconf_system *qsys;
  int i;
  int iret;

  qsys = qinfo->qsys;
  qsys->uuconf_pzforward_from = NULL;
  qsys->uuconf_pzforward_to = NULL;
  for (i = 1; i < argc; i++)
    {
      iret = _uuconf_iadd_string (qglobal, argv[i], FALSE, FALSE,
				  &qsys->uuconf_pzforward_to,
				  qsys->uuconf_palloc);
      if (iret != UUCONF_SUCCESS)
	return iret | UUCONF_CMDTABRET_KEEP | UUCONF_CMDTABRET_EXIT;
      iret = _uuconf_iadd_string (qglobal, argv[i], FALSE, FALSE,
				  &qsys->uuconf_pzforward_from,
				  qsys->uuconf_palloc);
      if (iret != UUCONF_SUCCESS)
	return iret | UUCONF_CMDTABRET_KEEP | UUCONF_CMDTABRET_EXIT;
    }

  return UUCONF_CMDTABRET_KEEP;
}

/* Handle an unknown command.  This should probably be done more
   intelligently.  */

/*ARGSUSED*/
static int
iiunknown (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;
}

/* Return information for an unknown system.  It would be better to
   put this in a different file, but it would require breaking several
   functions out of this file.  Perhaps I will do it sometime.  */

int
uuconf_taylor_system_unknown (pglobal, qsys)
     pointer pglobal;
     struct uuconf_system *qsys;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_cmdtab as[CSYSTEM_CMDS];
  struct sinfo si;
  struct sunknown *q;
  int iret;

  if (qglobal->qprocess->qunknown == NULL)
    return UUCONF_NOT_FOUND;

  _uuconf_ucmdtab_base (asIcmds, CSYSTEM_CMDS, (char *) qsys, as);

  _uuconf_uclear_system (qsys);

  si.qsys = qsys;
  si.falternates = FALSE;
  si.fdefault_alternates = TRUE;
  qsys->uuconf_palloc = uuconf_malloc_block ();
  if (qsys->uuconf_palloc == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  for (q = qglobal->qprocess->qunknown; q != NULL; q = q->qnext)
    {
      iret = uuconf_cmd_args (pglobal, q->cargs, q->pzargs, as,
			      (pointer) &si, iiunknown,
			      UUCONF_CMDTABFLAG_BACKSLASH,
			      qsys->uuconf_palloc);
      iret &=~ UUCONF_CMDTABRET_KEEP;
      if (UUCONF_ERROR_VALUE (iret) != UUCONF_SUCCESS)
	{
	  qglobal->zfilename = qglobal->qprocess->zconfigfile;
	  qglobal->ilineno = q->ilineno;
	  return ((iret &~ UUCONF_CMDTABRET_EXIT)
		  | UUCONF_ERROR_FILENAME
		  | UUCONF_ERROR_LINENO);
	}
      if ((iret & UUCONF_CMDTABRET_EXIT) != 0)
	break;
    }

  if (! si.falternates)
    uiset_call (qsys);
  else
    {
      iret = iialternate (pglobal, 0, (char **) NULL, (pointer) NULL,
			  (pointer) &si);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  /* The first alternate is always available for calling in.  */
  qsys->uuconf_fcalled = TRUE;

  return _uuconf_isystem_basic_default (qglobal, qsys);
}
