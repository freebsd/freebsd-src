/* uuxqt.c
   Run uux commands.

   Copyright (C) 1991, 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#if USE_RCS_ID
const char uuxqt_rcsid[] = "$Id: uuxqt.c,v 1.1 1993/08/05 18:28:27 conklin Exp $";
#endif

#include <errno.h>
#include <ctype.h>

#include "getopt.h"

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"

/* The program name.  */
char abProgram[] = "uuxqt";

/* Static variables used to unlock things if we get a fatal error.  */
static int iQlock_seq = -1;
static const char *zQunlock_cmd;
static const char *zQunlock_file;
static boolean fQunlock_directory;
int cQmaxuuxqts;

/* Static variables to free in uqcleanup.  */
static char *zQoutput;
static char *zQmail;

/* Local functions.  */
static void uqusage P((void));
static void uqabort P((void));
static void uqdo_xqt_file P((pointer puuconf, const char *zfile,
			     const char *zbase,
			     const struct uuconf_system *qsys,
			     const char *zlocalname,
			     const char *zcmd, boolean *pfprocessed));
static void uqcleanup P((const char *zfile, int iflags));
static boolean fqforward P((const char *zfile, char **pzallowed,
			    const char *zlog, const char *zmail));

/* Long getopt options.  */
static const struct option asQlongopts[] = { { NULL, 0, NULL, 0 } };

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* The type of command to execute (NULL for any type).  */
  const char *zcmd = NULL;
  /* The configuration file name.  */
  const char *zconfig = NULL;
  /* The system to execute commands for.  */
  const char *zdosys = NULL;
  int iopt;
  pointer puuconf;
  int iuuconf;
  const char *zlocalname;
  boolean fany;
  char *z, *zgetsys;
  boolean ferr;
  boolean fsys;
  struct uuconf_system ssys;

  while ((iopt = getopt_long (argc, argv, "c:I:s:x:", asQlongopts,
			      (int *) NULL)) != EOF)
    {
      switch (iopt)
	{
	case 'c':
	  /* Set the type of command to execute.  */
	  zcmd = optarg;
	  break;

	case 'I':
	  /* Set the configuration file name.  */
	  if (fsysdep_other_config (optarg))
	    zconfig = optarg;
	  break;

	case 's':
	  zdosys = optarg;
	  break;

	case 'x':
#if DEBUG > 1
	  /* Set the debugging level.  */
	  iDebug |= idebug_parse (optarg);
#endif
	  break;

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  uqusage ();
	  break;
	}
    }

  if (optind != argc)
    uqusage ();

  iuuconf = uuconf_init (&puuconf, (const char *) NULL, zconfig);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

#if DEBUG > 1
  {
    const char *zdebug;

    iuuconf = uuconf_debuglevel (puuconf, &zdebug);
    if (iuuconf != UUCONF_SUCCESS)
      ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
    if (zdebug != NULL)
      iDebug |= idebug_parse (zdebug);
  }
#endif

  iuuconf = uuconf_maxuuxqts (puuconf, &cQmaxuuxqts);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

#ifdef SIGINT
  usysdep_signal (SIGINT);
#endif
#ifdef SIGHUP
  usysdep_signal (SIGHUP);
#endif
#ifdef SIGQUIT
  usysdep_signal (SIGQUIT);
#endif
#ifdef SIGTERM
  usysdep_signal (SIGTERM);
#endif
#ifdef SIGPIPE
  usysdep_signal (SIGPIPE);
#endif

  usysdep_initialize (puuconf, INIT_SUID);

  ulog_to_file (puuconf, TRUE);
  ulog_fatal_fn (uqabort);

  iuuconf = uuconf_localname (puuconf, &zlocalname);
  if (iuuconf == UUCONF_NOT_FOUND)
    {
      zlocalname = zsysdep_localname ();
      if (zlocalname == NULL)
	exit (EXIT_FAILURE);
    }
  else if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  fsys = FALSE;

  /* If we were given a system name, canonicalize it, since the system
     dependent layer will not be returning aliases.  */
  if (zdosys != NULL)
    {
      iuuconf = uuconf_system_info (puuconf, zdosys, &ssys);
      if (iuuconf == UUCONF_NOT_FOUND)
	ulog (LOG_FATAL, "%s: System not found", zdosys);
      else if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

      zdosys = zbufcpy (ssys.uuconf_zname);
      fsys = TRUE;
    }

  /* Limit the number of uuxqt processes, and make sure we're the only
     uuxqt daemon running for this command.  */
  iQlock_seq = ixsysdep_lock_uuxqt (zcmd, cQmaxuuxqts);
  if (iQlock_seq < 0)
    {
      ulog_close ();
      usysdep_exit (TRUE);
    }
  zQunlock_cmd = zcmd;

  /* Keep scanning the execute files until we don't process any of
     them.  */
  do
    {
      fany = FALSE;

      /* Look for each execute file, and run it.  */

      if (! fsysdep_get_xqt_init ())
	{
	  ulog_close ();
	  usysdep_exit (FALSE);
	}

      while ((z = zsysdep_get_xqt (&zgetsys, &ferr)) != NULL)
	{
	  const char *zloc;
	  boolean fprocessed;
	  char *zbase;

	  /* It would be more efficient to pass zdosys down to the
	     routines which retrieve execute files.  */
	  if (zdosys != NULL && strcmp (zdosys, zgetsys) != 0)
	    {
	      ubuffree (z);
	      ubuffree (zgetsys);
	      continue;
	    }

	  if (! fsys || strcmp (ssys.uuconf_zname, zgetsys) != 0)
	    {
	      if (fsys)
		(void) uuconf_system_free (puuconf, &ssys);

	      iuuconf = uuconf_system_info (puuconf, zgetsys,
					    &ssys);
	      if (iuuconf != UUCONF_SUCCESS)
		{
		  if (iuuconf != UUCONF_NOT_FOUND)
		    {
		      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
		      ubuffree (z);
		      ubuffree (zgetsys);
		      continue;
		    }
		  else if (strcmp (zgetsys, zlocalname) == 0)
		    {
		      iuuconf = uuconf_system_local (puuconf, &ssys);
		      if (iuuconf != UUCONF_SUCCESS)
			{
			  ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
			  ubuffree (z);
			  ubuffree (zgetsys);
			  continue;
			}
		    }
		  else
		    {
		      if (! funknown_system (puuconf, zgetsys, &ssys))
			{
			  ulog (LOG_ERROR,
				"%s: Execute file for unknown system %s",
				z, zgetsys);
			  (void) remove (z);
			  ubuffree (z);
			  ubuffree (zgetsys);
			  continue;
			}
		    }
		}

	      fsys = TRUE;
	    }

	  /* If we've received a signal, get out of the loop.  */
	  if (FGOT_SIGNAL ())
	    {
	      ubuffree (z);
	      ubuffree (zgetsys);
	      break;
	    }

	  zloc = ssys.uuconf_zlocalname;
	  if (zloc == NULL)
	    zloc = zlocalname;

	  ulog_system (ssys.uuconf_zname);
	  zbase = zsysdep_base_name (z);
	  uqdo_xqt_file (puuconf, z, zbase, &ssys, zloc, zcmd, &fprocessed);
	  ubuffree (zbase);
	  ulog_system ((const char *) NULL);
	  ulog_user ((const char *) NULL);

	  if (fprocessed)
	    fany = TRUE;
	  ubuffree (z);
	  ubuffree (zgetsys);
	}

      usysdep_get_xqt_free ();
    }
  while (fany && ! FGOT_SIGNAL ());

  (void) fsysdep_unlock_uuxqt (iQlock_seq, zcmd, cQmaxuuxqts);
  iQlock_seq = -1;

  ulog_close ();

  if (FGOT_SIGNAL ())
    ferr = TRUE;

  usysdep_exit (! ferr);

  /* Avoid errors about not returning a value.  */
  return 0;
}

static void
uqusage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991, 1992 Ian Lance Taylor\n",
	   VERSION);
  fprintf (stderr,
	   "Usage: uuxqt [-c cmd] [-I file] [-s system] [-x debug]\n");
  fprintf (stderr,
	   " -c cmd: Set type of command to execute\n");
  fprintf (stderr,
	   " -s system: Execute commands only for named system\n");
  fprintf (stderr,
	   " -x debug: Set debugging level (0 for none, 9 is max)\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  exit (EXIT_FAILURE);
}

/* This is the abort function called when we get a fatal error.  */

static void
uqabort ()
{
#if ! HAVE_HDB_LOGGING
  /* When using HDB logging, it's a pain to have no system name.  */
  ulog_system ((const char *) NULL);
#endif

  ulog_user ((const char *) NULL);

  if (fQunlock_directory)
    (void) fsysdep_unlock_uuxqt_dir (iQlock_seq);

  if (zQunlock_file != NULL)
    (void) fsysdep_unlock_uuxqt_file (zQunlock_file);

  if (iQlock_seq >= 0)
    (void) fsysdep_unlock_uuxqt (iQlock_seq, zQunlock_cmd, cQmaxuuxqts);

  ulog_close ();

  usysdep_exit (FALSE);
}

/* An execute file is a series of lines.  The first character of each
   line is a command.  The following commands are defined:

   C command-line
   I standard-input
   O standard-output [ system ]
   F required-file filename-to-use
   R requestor-address
   U user system
   Z (acknowledge if command failed; default)
   N (no acknowledgement on failure)
   n (acknowledge if command succeeded)
   B (return command input on error)
   e (process with sh)
   E (process with exec)
   M status-file
   # comment

   Unrecognized commands are ignored.  We actually do not recognize
   the Z command, since it requests default behaviour.  We always send
   mail on failure, unless the N command appears.  We never send mail
   on success, unless the n command appears.

   This code does not currently support the B or M commands.  */

/* Command arguments.  */
static char **azQargs;
/* Command as a complete string.  */
static char *zQcmd;
/* Standard input file name.  */
static char *zQinput;
/* Standard output file name.  */
static char *zQoutfile;
/* Standard output system.  */
static char *zQoutsys;
/* Number of required files.  */
static int cQfiles;
/* Names of required files.  */
static char **azQfiles;
/* Names required files should be renamed to (NULL if original is OK).  */
static char **azQfiles_to;
/* Requestor address (this is where mail should be sent).  */
static char *zQrequestor;
/* User name.  */
static const char *zQuser;
/* System name.  */
static const char *zQsystem;
/* This is set by the N flag, meaning that no acknowledgement should
   be mailed on failure.  */
static boolean fQno_ack;
/* This is set by the n flag, meaning that acknowledgement should be
   mailed if the command succeeded.  */
static boolean fQsuccess_ack;
/* This is set by the B flag, meaning that command input should be
   mailed to the requestor if an error occurred.  */
static boolean fQsend_input;
/* This is set by the E flag, meaning that exec should be used to
   execute the command.  */
static boolean fQuse_exec;
/* The status should be copied to this file on the requesting host.  */
static const char *zQstatus_file;
#if ALLOW_SH_EXECUTION
/* This is set by the e flag, meaning that sh should be used to
   execute the command.  */
static boolean fQuse_sh;
#endif /* ALLOW_SH_EXECUTION */

static int iqcmd P((pointer puuconf, int argc, char **argv, pointer pvar,
		    pointer pinfo));
static int iqout P((pointer puuconf, int argc, char **argv, pointer pvar,
		    pointer pinfo));
static int iqfile P((pointer puuconf, int argc, char **argv, pointer pvar,
		     pointer pinfo));
static int iqrequestor P((pointer puuconf, int argc, char **argv,
			  pointer pvar, pointer pinfo));
static int iquser P((pointer puuconf, int argc, char **argv, pointer pvar,
		     pointer pinfo));
static int iqset P((pointer puuconf, int argc, char **argv, pointer pvar,
		    pointer pinfo));

static const struct uuconf_cmdtab asQcmds[] =
{
  { "C", UUCONF_CMDTABTYPE_FN | 0, NULL, iqcmd },
  { "I", UUCONF_CMDTABTYPE_STRING, (pointer) &zQinput, NULL },
  { "O", UUCONF_CMDTABTYPE_FN | 0, NULL, iqout },
  { "F", UUCONF_CMDTABTYPE_FN | 0, NULL, iqfile },
  { "R", UUCONF_CMDTABTYPE_FN, NULL, iqrequestor },
  { "U", UUCONF_CMDTABTYPE_FN | 3, NULL, iquser },
  { "N", UUCONF_CMDTABTYPE_FN | 1, (pointer) &fQno_ack, iqset },
  { "n", UUCONF_CMDTABTYPE_FN | 1, (pointer) &fQsuccess_ack, iqset },
  /* Some systems create execution files in which B takes an argument;
     I don't know what it means, so I just ignore it.  */
  { "B", UUCONF_CMDTABTYPE_FN | 0, (pointer) &fQsend_input, iqset },
#if ALLOW_SH_EXECUTION
  { "e", UUCONF_CMDTABTYPE_FN | 1, (pointer) &fQuse_sh, iqset },
#endif
  { "E", UUCONF_CMDTABTYPE_FN | 1, (pointer) &fQuse_exec, iqset },
  { "M", UUCONF_CMDTABTYPE_STRING, (pointer) &zQstatus_file, NULL },
  { NULL, 0, NULL, NULL }
};

/* Handle the C command: store off the arguments.  */

/*ARGSUSED*/
static int
iqcmd (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  int i;
  size_t clen;

  if (argc <= 1)
    return UUCONF_CMDTABRET_CONTINUE;

  azQargs = (char **) xmalloc (argc * sizeof (char *));
  clen = 0;
  for (i = 1; i < argc; i++)
    {
      azQargs[i - 1] = zbufcpy (argv[i]);
      clen += strlen (argv[i]) + 1;
    }
  azQargs[i - 1] = NULL;

  zQcmd = (char *) xmalloc (clen);
  zQcmd[0] = '\0';
  for (i = 1; i < argc - 1; i++)
    {
      strcat (zQcmd, argv[i]);
      strcat (zQcmd, " ");
    }
  strcat (zQcmd, argv[i]);

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Handle the O command, which may have one or two arguments.  */

/*ARGSUSED*/
static int
iqout (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  const char *zbase = (const char *) pinfo;

  if (argc != 2 && argc != 3)
    {
      ulog (LOG_ERROR, "%s: %s: Wrong number of arguments",
	    zbase, argv[0]);
      return UUCONF_CMDTABRET_CONTINUE;
    }

  zQoutfile = zbufcpy (argv[1]);
  if (argc == 3)
    zQoutsys = zbufcpy (argv[2]);

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Handle the F command, which may have one or two arguments.  */

/*ARGSUSED*/
static int
iqfile (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  const char *zbase = (const char *) pinfo;

  if (argc != 2 && argc != 3)
    {
      ulog (LOG_ERROR, "%s: %s: Wrong number of arguments",
	    zbase, argv[0]);
      return UUCONF_CMDTABRET_CONTINUE;
    }

  /* If this file is not in the spool directory, just ignore it.  */
  if (! fspool_file (argv[1]))
    return UUCONF_CMDTABRET_CONTINUE;

  ++cQfiles;
  azQfiles = (char **) xrealloc ((pointer) azQfiles,
				 cQfiles * sizeof (char *));
  azQfiles_to = (char **) xrealloc ((pointer) azQfiles_to,
				    cQfiles * sizeof (char *));

  azQfiles[cQfiles - 1] = zbufcpy (argv[1]);
  if (argc == 3)
    azQfiles_to[cQfiles - 1] = zbufcpy (argv[2]);
  else
    azQfiles_to[cQfiles - 1] = NULL;

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Handle the R command, which may have one or two arguments.  */

/*ARGSUSED*/
static int
iqrequestor (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  const char *zbase = (const char *) pinfo;

  if (argc != 2 && argc != 3)
    {
      ulog (LOG_ERROR, "%s: %s: Wrong number of arguments",
	    zbase, argv[0]);
      return UUCONF_CMDTABRET_CONTINUE;
    }

  /* We normally have a single argument, which is the ``requestor''
     address, to which we should send any success or error messages.
     Apparently the DOS program UUPC sends two arguments, which are
     the username and the host.  */
  if (argc == 2)
    zQrequestor = zbufcpy (argv[1]);
  else
    {
      zQrequestor = zbufalc (strlen (argv[1]) + strlen (argv[2])
			     + sizeof "!");
      sprintf (zQrequestor, "%s!%s", argv[2], argv[1]);
    }

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Handle the U command, which takes two arguments.  */

/*ARGSUSED*/
static int
iquser (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  zQuser = argv[1];
  zQsystem = argv[2];
  return UUCONF_CMDTABRET_KEEP;
}

/* Handle various commands which just set boolean variables.  */

/*ARGSUSED*/
static int
iqset (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;     
{
  boolean *pf = (boolean *) pvar;

  *pf = TRUE;
  return UUCONF_CMDTABRET_CONTINUE;
}

/* The execution processing does a lot of things that have to be
   cleaned up.  Rather than try to add the appropriate statements
   to each return point, we keep a set of flags indicating what
   has to be cleaned up.  The actual clean up is done by the
   function uqcleanup.  */
#define REMOVE_FILE (01)
#define REMOVE_NEEDED (02)
#define FREE_QINPUT (04)
#define FREE_OUTPUT (010)
#define FREE_MAIL (020)

/* Process an execute file.  The zfile argument is the name of the
   execute file.  The zbase argument is the base name of zfile.  The
   qsys argument describes the system it came from.  The zcmd argument
   is the name of the command we are executing (from the -c option) or
   NULL if any command is OK.  This sets *pfprocessed to TRUE if the
   file is ready to be executed.  */

static void
uqdo_xqt_file (puuconf, zfile, zbase, qsys, zlocalname, zcmd, pfprocessed)
     pointer puuconf;
     const char *zfile;
     const char *zbase;
     const struct uuconf_system *qsys;
     const char *zlocalname;
     const char *zcmd;
     boolean *pfprocessed;
{
  char *zabsolute;
  boolean ferr;
  FILE *e;
  int iuuconf;
  int i;
  int iclean;
  const char *zmail;
  char *zoutput;
  char *zinput;
  char abtemp[CFILE_NAME_LEN];
  char abdata[CFILE_NAME_LEN];
  char *zerror;
  struct uuconf_system soutsys;
  const struct uuconf_system *qoutsys;
  boolean fshell;
  size_t clen;
  char *zfullcmd;
  boolean ftemp;

  *pfprocessed = FALSE;

  e = fopen (zfile, "r");
  if (e == NULL)
    return;

  azQargs = NULL;
  zQcmd = NULL;
  zQinput = NULL;
  zQoutfile = NULL;
  zQoutsys = NULL;
  cQfiles = 0;
  azQfiles = NULL;
  azQfiles_to = NULL;
  zQrequestor = NULL;
  zQuser = NULL;
  zQsystem = NULL;
  fQno_ack = FALSE;
  fQsuccess_ack = FALSE;
  fQsend_input = FALSE;
  fQuse_exec = FALSE;
  zQstatus_file = NULL;
#if ALLOW_SH_EXECUTION
  fQuse_sh = FALSE;
#endif

  iuuconf = uuconf_cmd_file (puuconf, e, asQcmds, (pointer) zbase,
			     (uuconf_cmdtabfn) NULL,
			     UUCONF_CMDTABFLAG_CASE, (pointer) NULL);
  (void) fclose (e);

  if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      return;
    }

  iclean = 0;

  if (azQargs == NULL)
    {
      ulog (LOG_ERROR, "%s: No command given", zbase);
      uqcleanup (zfile, iclean | REMOVE_FILE);
      return;
    }

  if (zcmd != NULL)
    {
      if (strcmp (zcmd, azQargs[0]) != 0)
	{
	  uqcleanup (zfile, iclean);
	  return;
	}
    }
  else
    {
      /* If there is a lock file for this particular command already,
	 it means that some other uuxqt is supposed to handle it.  */
      if (fsysdep_uuxqt_locked (azQargs[0]))
	{
	  uqcleanup (zfile, iclean);
	  return;
	}
    }

  /* Lock this particular file.  */
  if (! fsysdep_lock_uuxqt_file (zfile))
    {
      uqcleanup (zfile, iclean);
      return;
    }

  zQunlock_file = zfile;

  /* Now that we have the file locked, make sure it still exists.
     Otherwise another uuxqt could have just finished processing it
     and removed the lock file.  */
  if (! fsysdep_file_exists (zfile))
    {
      uqcleanup (zfile, iclean);
      return;
    }

  if (zQuser != NULL)
    ulog_user (zQuser);
  else if (zQrequestor != NULL)
    ulog_user (zQrequestor);
  else
    ulog_user ("unknown");

  /* Make sure that all the required files exist, and get their
     full names in the spool directory.  */
  for (i = 0; i < cQfiles; i++)
    {
      char *zreal;

      zreal = zsysdep_spool_file_name (qsys, azQfiles[i], (pointer) NULL);
      if (zreal == NULL)
	{
	  uqcleanup (zfile, iclean);
	  return;
	}
      if (! fsysdep_file_exists (zreal))
	{
	  uqcleanup (zfile, iclean);
	  return;
	}
      ubuffree (azQfiles[i]);
      azQfiles[i] = zbufcpy (zreal);
      ubuffree (zreal);
    }

  /* Lock the execution directory.  */
  if (! fsysdep_lock_uuxqt_dir (iQlock_seq))
    {
      ulog (LOG_ERROR, "Could not lock execute directory");
      uqcleanup (zfile, iclean);
      return;
    }
  fQunlock_directory = TRUE;

  iclean |= REMOVE_FILE | REMOVE_NEEDED;
  *pfprocessed = TRUE;

  /* Get the address to mail results to.  Prepend the system from
     which the execute file originated, since mail addresses are
     relative to it.  */
  zmail = NULL;
  if (zQrequestor != NULL)
    zmail = zQrequestor;
  else if (zQuser != NULL)
    zmail = zQuser;
  if (zmail != NULL
      && zQsystem != NULL
#if HAVE_INTERNET_MAIL
      && strchr (zmail, '@') == NULL
#endif
      && strcmp (zQsystem, zlocalname) != 0)
    {
      char *zset;

      zset = zbufalc (strlen (zQsystem) + strlen (zmail) + 2);
      sprintf (zset, "%s!%s", zQsystem, zmail);
      zmail = zset;
      zQmail = zset;
      iclean |= FREE_MAIL;
    }

  /* The command "uucp" is handled specially.  We make sure that the
     appropriate forwarding is permitted, and we add a -u argument to
     specify the user.  */
  if (strcmp (azQargs[0], "uucp") == 0)
    {
      char *zfrom, *zto;
      boolean fmany;
      char **azargs;
      const char *zuser, *zsystem;

      zfrom = NULL;
      zto = NULL;
      fmany = FALSE;

      /* Skip all the options, and get the from and to specs.  We
	 don't permit multiple arguments.  */
      for (i = 1; azQargs[i] != NULL; i++)
	{
	  if (azQargs[i][0] == '-')
	    {
	      char *zopts;

	      for (zopts = azQargs[i] + 1; *zopts != '\0'; zopts++)
		{
		  /* The -g, -n, and -s options take an argument.  */
		  if (*zopts == 'g' || *zopts == 'n' || *zopts == 's')
		    {
		      if (zopts[1] == '\0')
			++i;
		      break;
		    }
		  /* The -I, -u and -x options are not permitted.  */
		  if (*zopts == 'I' || *zopts == 'u' || *zopts == 'x')
		    {
		      *zopts = 'r';
		      if (zopts[1] != '\0')
			zopts[1] = '\0';
		      else
			{
			  ++i;
			  azQargs[i] = zbufcpy ("-r");
			}
		      break;
		    }
		}
	    }
	  else if (zfrom == NULL)
	    zfrom = azQargs[i];
	  else if (zto == NULL)
	    zto = azQargs[i];
	  else
	    {
	      fmany = TRUE;
	      break;
	    }
	}

      /* Add the -u argument.  This is required to let uucp do the
	 correct permissions checking on the file transfer.  */
      for (i = 0; azQargs[i] != NULL; i++)
	;
      azargs = (char **) xmalloc ((i + 2) * sizeof (char *));
      azargs[0] = azQargs[0];
      zuser = zQuser;
      if (zuser == NULL)
	zuser = "uucp";
      zsystem = zQsystem;
      if (zsystem == NULL)
	zsystem = qsys->uuconf_zname;
      azargs[1] = zbufalc (strlen (zsystem) + strlen (zuser)
			   + sizeof "-u!");
      sprintf (azargs[1], "-u%s!%s", zsystem, zuser);
      memcpy (azargs + 2, azQargs + 1, i * sizeof (char *));
      xfree ((pointer) azQargs);
      azQargs = azargs;

      /* Find the uucp binary.  */
      zabsolute = zsysdep_find_command ("uucp", qsys->uuconf_pzcmds,
					qsys->uuconf_pzpath, &ferr);
      if (zabsolute == NULL && ! ferr)
	{
	  const char *azcmds[2];

	  /* If "uucp" is not a permitted command, then the forwarding
	     entries must be set.  */
	  if (! fqforward (zfrom, qsys->uuconf_pzforward_from, "from", zmail)
	      || ! fqforward (zto, qsys->uuconf_pzforward_to, "to", zmail))
	    {
	      uqcleanup (zfile, iclean);
	      return;
	    }

	  /* If "uucp" is not a permitted command, then only uucp
	     requests with a single source are permitted, since that
	     is all that will be generated by uucp or uux.  */
	  if (fmany)
	    {
	      ulog (LOG_ERROR, "Bad uucp request %s", zQcmd);

	      if (zmail != NULL && ! fQno_ack)
		{
		  const char *az[20];

		  i = 0;
		  az[i++] = "Your execution request failed because it was an";
		  az[i++] = " unsupported uucp request.\n";
		  az[i++] = "Execution requested was:\n\t";
		  az[i++] = zQcmd;
		  az[i++] = "\n";

		  (void) fsysdep_mail (zmail, "Execution failed", i, az);
		}

	      uqcleanup (zfile, iclean);
	      return;
	    }

	  azcmds[0] = "uucp";
	  azcmds[1] = NULL;
	  zabsolute = zsysdep_find_command ("uucp", (char **) azcmds,
					    qsys->uuconf_pzpath, &ferr);
	}
      if (zabsolute == NULL)
	{
	  if (! ferr)
	    ulog (LOG_ERROR, "Can't find uucp executable");

	  uqcleanup (zfile, iclean &~ (REMOVE_FILE | REMOVE_NEEDED));
	  *pfprocessed = FALSE;
	  return;
	}
    }
  else
    {
      /* Get the pathname to execute.  */
      zabsolute = zsysdep_find_command (azQargs[0], qsys->uuconf_pzcmds,
					qsys->uuconf_pzpath,
					&ferr);
      if (zabsolute == NULL)
	{
	  if (ferr)
	    {
	      /* If we get an error, try again later.  */
	      uqcleanup (zfile, iclean &~ (REMOVE_FILE | REMOVE_NEEDED));
	      *pfprocessed = FALSE;
	      return;
	    }

	  /* Not permitted.  Send mail to requestor.  */
	  ulog (LOG_ERROR, "Not permitted to execute %s",
		azQargs[0]);

	  if (zmail != NULL && ! fQno_ack)
	    {
	      const char *az[20];

	      i = 0;
	      az[i++] = "Your execution request failed because you are not";
	      az[i++] = " permitted to execute\n\t";
	      az[i++] = azQargs[0];
	      az[i++] = "\non this system.\n";
	      az[i++] = "Execution requested was:\n\t";
	      az[i++] = zQcmd;
	      az[i++] = "\n";

	      (void) fsysdep_mail (zmail, "Execution failed", i, az);
	    }

	  uqcleanup (zfile, iclean);
	  return;
	}
    }

  ubuffree (azQargs[0]);
  azQargs[0] = zabsolute;

  for (i = 1; azQargs[i] != NULL; i++)
    {
      char *zlocal;

      zlocal = zsysdep_xqt_local_file (qsys, azQargs[i]);
      if (zlocal != NULL)
	{
	  ubuffree (azQargs[i]);
	  azQargs[i] = zlocal;
	}
    }

#if ! ALLOW_FILENAME_ARGUMENTS

  /* Check all the arguments to make sure they don't try to specify
     files they are not permitted to access.  */
  for (i = 1; azQargs[i] != NULL; i++)
    {
      if (! fsysdep_xqt_check_file (qsys, azQargs[i]))
	{
	  if (zmail != NULL && ! fQno_ack)
	    {
	      const char *az[20];
	      const char *zfailed;

	      zfailed = azQargs[i];
	      i = 0;
	      az[i++] = "Your execution request failed because you are not";
	      az[i++] = " permitted to refer to file\n\t";
	      az[i++] = zfailed;
	      az[i++] = "\non this system.\n";
	      az[i++] = "Execution requested was:\n\t";
	      az[i++] = zQcmd;
	      az[i++] = "\n";

	      (void) fsysdep_mail (zmail, "Execution failed", i, az);
	    }

	  uqcleanup (zfile, iclean);
	  return;
	}
    }

#endif /* ! ALLOW_FILENAME_ARGUMENTS */

  ulog (LOG_NORMAL, "Executing %s (%s)", zbase, zQcmd);

  if (zQinput != NULL)
    {
      boolean fspool;
      char *zreal;

      fspool = fspool_file (zQinput);
      if (fspool)
	zreal = zsysdep_spool_file_name (qsys, zQinput, (pointer) NULL);
      else
	zreal = zsysdep_local_file (zQinput, qsys->uuconf_zpubdir);
      if (zreal == NULL)
	{
	  /* If we get an error, try again later.  */
	  uqcleanup (zfile, iclean &~ (REMOVE_FILE | REMOVE_NEEDED));
	  *pfprocessed = FALSE;
	  return;
	}

      zQinput = zreal;
      iclean |= FREE_QINPUT;

      if (! fspool
	  && ! fin_directory_list (zQinput, qsys->uuconf_pzremote_send,
				   qsys->uuconf_zpubdir, TRUE, TRUE,
				   (const char *) NULL))
	{
	  ulog (LOG_ERROR, "Not permitted to read %s", zQinput);
	      
	  if (zmail != NULL && ! fQno_ack)
	    {
	      const char *az[20];

	      i = 0;
	      az[i++] = "Your execution request failed because you are";
	      az[i++] = " not permitted to read\n\t";
	      az[i++] = zQinput;
	      az[i++] = "\non this system.\n";
	      az[i++] = "Execution requested was:\n\t";
	      az[i++] = zQcmd;
	      az[i++] = "\n";

	      (void) fsysdep_mail (zmail, "Execution failed", i, az);
	    }

	  uqcleanup (zfile, iclean);
	  return;
	}
    }

  zoutput = NULL;
  if (zQoutfile == NULL)
    qoutsys = NULL;
  else if (zQoutsys != NULL
	   && strcmp (zQoutsys, zlocalname) != 0)
    {
      char *zdata;
	 
      /* The output file is destined for some other system, so we must
	 use a temporary file to catch standard output.  */
      if (strcmp (zQoutsys, qsys->uuconf_zname) == 0)
	qoutsys = qsys;
      else
	{
	  iuuconf = uuconf_system_info (puuconf, zQoutsys, &soutsys);
	  if (iuuconf != UUCONF_SUCCESS)
	    {
	      if (iuuconf != UUCONF_NOT_FOUND)
		{
		  ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
		  uqcleanup (zfile, iclean &~ (REMOVE_FILE | REMOVE_NEEDED));
		  *pfprocessed = FALSE;
		  return;
		}

	      if (! funknown_system (puuconf, zQoutsys, &soutsys))
		{
		  ulog (LOG_ERROR,
			"Can't send standard output to unknown system %s",
			zQoutsys);
		  /* We don't send mail to unknown systems, either.
		     Maybe we should.  */
		  uqcleanup (zfile, iclean);
		  return;
		}
	    }

	  qoutsys = &soutsys;
	}

      zdata = zsysdep_data_file_name (qoutsys, zlocalname,
				      BDEFAULT_UUX_GRADE, FALSE, abtemp,
				      abdata, (char *) NULL);
      if (zdata == NULL)
	{
	  /* If we get an error, try again later.  */
	  uqcleanup (zfile, iclean &~ (REMOVE_FILE | REMOVE_NEEDED));
	  *pfprocessed = FALSE;
	  return;
	}
      zoutput = zdata;
      zQoutput = zoutput;
      iclean |= FREE_OUTPUT;
    }
  else
    {
      boolean fok;
	 
      qoutsys = NULL;

      /* If we permitted the standard output to be redirected into
	 the spool directory, people could set up phony commands.  */
      if (fspool_file (zQoutfile))
	fok = FALSE;
      else
	{
	  zoutput = zsysdep_local_file (zQoutfile, qsys->uuconf_zpubdir);
	  if (zoutput == NULL)
	    {
	      /* If we get an error, try again later.  */
	      uqcleanup (zfile, iclean &~ (REMOVE_FILE | REMOVE_NEEDED));
	      *pfprocessed = FALSE;
	      return;
	    }
	  ubuffree (zQoutfile);
	  zQoutfile = zoutput;

	  /* Make sure it's OK to receive this file.  */
	  fok = fin_directory_list (zQoutfile,
				    qsys->uuconf_pzremote_receive,
				    qsys->uuconf_zpubdir, TRUE, FALSE,
				    (const char *) NULL);
	}

      if (! fok)
	{
	  ulog (LOG_ERROR, "Not permitted to write to %s", zQoutfile);
	      
	  if (zmail != NULL && ! fQno_ack)
	    {
	      const char *az[20];

	      i = 0;
	      az[i++] = "Your execution request failed because you are";
	      az[i++] = " not permitted to write to\n\t";
	      az[i++] = zQoutfile;
	      az[i++] = "\non this system.\n";
	      az[i++] = "Execution requested was:\n\t";
	      az[i++] = zQcmd;
	      az[i++] = "\n";

	      (void) fsysdep_mail (zmail, "Execution failed", i, az);
	    }

	  uqcleanup (zfile, iclean);
	  return;
	}
    }

  /* Move the required files to the execution directory if necessary.  */
  zinput = zQinput;
  if (! fsysdep_move_uuxqt_files (cQfiles, (const char **) azQfiles,
				  (const char **) azQfiles_to,
				  TRUE, iQlock_seq, &zinput))
    {
      /* If we get an error, try again later.  */
      uqcleanup (zfile, iclean &~ (REMOVE_FILE | REMOVE_NEEDED));
      *pfprocessed = FALSE;
      return;
    }
  if (zQinput != NULL && strcmp (zQinput, zinput) != 0)
    {
      if ((iclean & FREE_QINPUT) != 0)
	ubuffree (zQinput);
      zQinput = zinput;
      iclean |= FREE_QINPUT;
    }

#if ALLOW_SH_EXECUTION
  fshell = fQuse_sh;
#else
  fshell = FALSE;
#endif

  /* Get a shell command which uses the full path of the command to
     execute.  */
  clen = 0;
  for (i = 0; azQargs[i] != NULL; i++)
    clen += strlen (azQargs[i]) + 1;
  zfullcmd = zbufalc (clen);
  strcpy (zfullcmd, azQargs[0]);
  for (i = 1; azQargs[i] != NULL; i++)
    {
      strcat (zfullcmd, " ");
      strcat (zfullcmd, azQargs[i]);
    }

  if (! fsysdep_execute (qsys,
			 zQuser == NULL ? (const char *) "uucp" : zQuser,
			 (const char **) azQargs, zfullcmd, zQinput,
			 zoutput, fshell, iQlock_seq, &zerror, &ftemp))
    {
      ubuffree (zfullcmd);

      if (ftemp)
	{
	  ulog (LOG_NORMAL, "Will retry later (%s)", zbase);
	  if (zoutput != NULL)
	    (void) remove (zoutput);
	  if (zerror != NULL)
	    {
	      (void) remove (zerror);
	      ubuffree (zerror);
	    }
	  (void) fsysdep_move_uuxqt_files (cQfiles, (const char **) azQfiles,
					   (const char **) azQfiles_to,
					   FALSE, iQlock_seq,
					   (char **) NULL);
	  uqcleanup (zfile, iclean &~ (REMOVE_FILE | REMOVE_NEEDED));
	  *pfprocessed = FALSE;
	  return;
	}

      ulog (LOG_NORMAL, "Execution failed (%s)", zbase);

      if (zmail != NULL && ! fQno_ack)
	{
	  const char **pz;
	  int cgot;
	  FILE *eerr;
	  int istart;

	  cgot = 20;
	  pz = (const char **) xmalloc (cgot * sizeof (const char *));
	  i = 0;
	  pz[i++] = "Execution request failed:\n\t";
	  pz[i++] = zQcmd;
	  pz[i++] = "\n";

	  if (zerror == NULL)
	    eerr = NULL;
	  else
	    eerr = fopen (zerror, "r");
	  if (eerr == NULL)
	    {
	      pz[i++] = "There was no output on standard error\n";
	      istart = i;
	    }
	  else
	    {
	      char *zline;
	      size_t cline;

	      pz[i++] = "Standard error output was:\n";
	      istart = i;

	      zline = NULL;
	      cline = 0;
	      while (getline (&zline, &cline, eerr) > 0)
		{
		  if (i >= cgot)
		    {
		      cgot += 20;
		      pz = ((const char **)
			    xrealloc ((pointer) pz,
				      cgot * sizeof (const char *)));
		    }
		  pz[i++] = zbufcpy (zline);
		}

	      (void) fclose (eerr);
	      xfree ((pointer) zline);
	    }

	  (void) fsysdep_mail (zmail, "Execution failed", i, pz);

	  for (; istart < i; istart++)
	    ubuffree ((char *) pz[istart]);
	  xfree ((pointer) pz);
	}

      if (qoutsys != NULL)
	(void) remove (zoutput);
    }
  else
    {
      ubuffree (zfullcmd);

      if (zmail != NULL && fQsuccess_ack)
	{
	  const char *az[20];

	  i = 0;
	  az[i++] = "\nExecution request succeeded:\n\t";
	  az[i++] = zQcmd;
	  az[i++] = "\n";

	  (void) fsysdep_mail (zmail, "Execution succeded", i, az);
	}

      /* Now we may have to uucp the output to some other machine.  */

      if (qoutsys != NULL)
	{
	  struct scmd s;

	  /* Fill in the command structure.  */

	  s.bcmd = 'S';
	  s.pseq = NULL;
	  s.zfrom = abtemp;
	  s.zto = zQoutfile;
	  if (zQuser != NULL)
	    s.zuser = zQuser;
	  else
	    s.zuser = "uucp";
	  if (zmail != NULL && fQsuccess_ack)
	    s.zoptions = "Cn";
	  else
	    s.zoptions = "C";
	  s.ztemp = abtemp;
	  s.imode = 0666;
	  if (zmail != NULL && fQsuccess_ack)
	    s.znotify = zmail;
	  else
	    s.znotify = "";
	  s.cbytes = -1;
	  s.zcmd = NULL;
	  s.ipos = 0;

	  ubuffree (zsysdep_spool_commands (qoutsys, BDEFAULT_UUX_GRADE,
					    1, &s));
	}
    }

  if (zerror != NULL)
    {
      (void) remove (zerror);
      ubuffree (zerror);
    }

  uqcleanup (zfile, iclean);
}

/* Clean up the results of uqdo_xqt_file.  */

static void
uqcleanup (zfile, iflags)
     const char *zfile;
     int iflags;
{
  int i;

  DEBUG_MESSAGE2 (DEBUG_SPOOLDIR,
		  "uqcleanup: %s, %d", zfile, iflags);

  if (zQunlock_file != NULL)
    {
      (void) fsysdep_unlock_uuxqt_file (zQunlock_file);
      zQunlock_file = NULL;
    }

  if ((iflags & REMOVE_FILE) != 0)
    (void) remove (zfile);

  if ((iflags & REMOVE_NEEDED) != 0)
    {
      for (i = 0; i < cQfiles; i++)
	{
	  if (azQfiles[i] != NULL)
	    (void) remove (azQfiles[i]);
	}
    }

  if ((iflags & FREE_QINPUT) != 0)
    ubuffree (zQinput);

  if ((iflags & FREE_OUTPUT) != 0)
    ubuffree (zQoutput);
  if ((iflags & FREE_MAIL) != 0)
    ubuffree (zQmail);

  if (fQunlock_directory)
    {
      (void) fsysdep_unlock_uuxqt_dir (iQlock_seq);
      fQunlock_directory = FALSE;
    }

  for (i = 0; i < cQfiles; i++)
    {
      ubuffree (azQfiles[i]);
      ubuffree (azQfiles_to[i]);
    }

  ubuffree (zQoutfile);
  ubuffree (zQoutsys);
  ubuffree (zQrequestor);

  if (azQargs != NULL)
    {
      for (i = 0; azQargs[i] != NULL; i++)
	ubuffree (azQargs[i]);
      xfree ((pointer) azQargs);
      azQargs = NULL;
    }

  xfree ((pointer) zQcmd);
  zQcmd = NULL;

  xfree ((pointer) azQfiles);
  azQfiles = NULL;

  xfree ((pointer) azQfiles_to);
  azQfiles_to = NULL;
}

/* Check whether forwarding is permitted.  */

static boolean
fqforward (zfile, pzallowed, zlog, zmail)
     const char *zfile;
     char **pzallowed;
     const char *zlog;
     const char *zmail;
{
  const char *zexclam;

  zexclam = strchr (zfile, '!');
  if (zexclam != NULL)
    {
      size_t clen;
      char *zsys;
      boolean fret;

      clen = zexclam - zfile;
      zsys = zbufalc (clen + 1);
      memcpy (zsys, zfile, clen);
      zsys[clen] = '\0';

      fret = FALSE;
      if (pzallowed != NULL)
	{
	  char **pz;

	  for (pz = pzallowed; *pz != NULL; pz++)
	    {
	      if (strcmp (*pz, "ANY") == 0
		  || strcmp (*pz, zsys) == 0)
		{
		  fret = TRUE;
		  break;
		}
	    }
	}

      if (! fret)
	{
	  ulog (LOG_ERROR, "Not permitted to forward %s %s (%s)",
		zlog, zsys, zQcmd);

	  if (zmail != NULL && ! fQno_ack)
	    {
	      int i;
	      const char *az[20];

	      i = 0;
	      az[i++] = "Your execution request failed because you are";
	      az[i++] = " not permitted to forward files\n";
	      az[i++] = zlog;
	      az[i++] = " the system\n\t";
	      az[i++] = zsys;
	      az[i++] = "\n";
	      az[i++] = "Execution requested was:\n\t";
	      az[i++] = zQcmd;
	      az[i++] = "\n";

	      (void) fsysdep_mail (zmail, "Execution failed", i, az);
	    }
	}

      ubuffree (zsys);

      return fret;
    }

  return TRUE;
}
