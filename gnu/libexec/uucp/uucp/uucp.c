/* uucp.c
   Prepare to copy a file to or from a remote system.

   Copyright (C) 1991, 1992, 1993, 1994, 1995 Ian Lance Taylor

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char uucp_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/uucp/uucp.c,v 1.7 1999/08/27 23:33:56 peter Exp $";
#endif

#include <ctype.h>
#include <errno.h>

#include "getopt.h"

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"

/* Local functions.  */

static void ucusage P((void));
static void uchelp P((void));
static void ucdirfile P((const char *zdir, const char *zfile,
			 pointer pinfo));
static void uccopy P((const char *zfile, const char *zdest));
static void ucadd_cmd P((const struct uuconf_system *qsys,
			 const struct scmd *qcmd, const char *zlog));
static void ucspool_cmds P((boolean fjobid));
static const char *zcone_system P((boolean *pfany));
static void ucrecord_file P((const char *zfile));
static void ucabort P((void));

/* Long getopt options.  */
static const struct option asClongopts[] =
{
  { "copy", no_argument, NULL, 'C' },
  { "nocopy", no_argument, NULL, 'c' },
  { "directories", no_argument, NULL, 'd' },
  { "nodirectories", no_argument, NULL, 'f' },
  { "grade", required_argument, NULL, 'g' },
  { "jobid", no_argument, NULL, 'j' },
  { "mail", no_argument, NULL, 'm' },
  { "notify", required_argument, NULL, 'n' },
  { "nouucico", no_argument, NULL, 'r' },
  { "recursive", no_argument, NULL, 'R' },
  { "status", required_argument, NULL, 's' },
  { "uuto", no_argument, NULL, 't' },
  { "user", required_argument, NULL, 'u' },
  { "noexpand", no_argument, NULL, 'w' },
  { "config", required_argument, NULL, 'I' },
  { "debug", required_argument, NULL, 'x' },
  { "version", no_argument, NULL, 'v' },
  { "help", no_argument, NULL, 1 },
  { NULL, 0, NULL, 0 }
};

/* Local variables.  There are a bunch of these, mostly set by the
   options and the last (the destination) argument.  These have file
   scope so that they may be easily passed into uccopy; they could for
   the most part also be wrapped up in a structure and passed in.  */

/* The uuconf global pointer.  */
static pointer pCuuconf;

/* TRUE if source files should be copied to the spool directory.  */
static boolean fCcopy = TRUE;

/* Grade to use.  */
static char bCgrade = BDEFAULT_UUCP_GRADE;

/* Whether to send mail to the requesting user when the copy is
   complete.  */
static boolean fCmail = FALSE;

/* User to notify on remote system.  */
static const char *zCnotify = "";

/* TRUE if remote files should be prefixed with the current working
   directory.  */
static boolean fCexpand = TRUE;

/* TRUE if necessary directories should be created on the destination
   system.  */
static boolean fCmkdirs = TRUE;

/* Local name.  */
static const char *zClocalname;

/* User name.  */
static const char *zCuser = NULL;

/* TRUE if this is a remote request.  */
static boolean fCremote = FALSE;

/* TRUE if the destination is this system.  */
static boolean fClocaldest;

/* Destination system.  */
static struct uuconf_system sCdestsys;

/* Systems to forward to, if not NULL.  */
static char *zCforward;

/* Options to use when sending a file.  */
static char abCsend_options[20];

/* Options to use when receiving a file.  */
static char abCrec_options[20];

/* TRUE if the current file being copied from is in the cwd.  */
static boolean fCneeds_cwd;

/* The main program.  */

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  /* -j: output job id.  */
  boolean fjobid = FALSE;
  /* -r: don't start uucico when finished.  */
  boolean fuucico = TRUE;
  /* -R: copy directories recursively.  */
  boolean frecursive = FALSE;
  /* -s: report status to named file.  */
  const char *zstatus_file = NULL;
  /* -t: emulate uuto.  */
  boolean fuuto = FALSE;
  int iopt;
  pointer puuconf;
  int iuuconf;
  int i;
  boolean fgetcwd;
  struct uuconf_system slocalsys;
  char *zexclam;
  char *zdestfile;
  const char *zdestsys;
  char *zoptions;
  boolean fexit;

  zProgram = argv[0];

  while ((iopt = getopt_long (argc, argv, "cCdfg:I:jmn:prRs:tu:Wvx:",
			      asClongopts, (int *) NULL)) != EOF)
    {
      switch (iopt)
	{
	case 'c':
	  /* Do not copy local files to spool directory.  */
	  fCcopy = FALSE;
	  break;

	case 'p':
	case 'C':
	  /* Copy local files to spool directory.  */
	  fCcopy = TRUE;
	  break;

	case 'd':
	  /* Create directories if necessary.  */
	  fCmkdirs = TRUE;
	  break;

	case 'f':
	  /* Do not create directories if they don't exist.  */
	  fCmkdirs = FALSE;
	  break;

	case 'g':
	  /* Set job grade.  */
	  bCgrade = optarg[0];
	  break;

	case 'I':
	  /* Name configuration file.  */
	  if (fsysdep_other_config (optarg))
	    zconfig = optarg;
	  break;

	case 'j':
	  /* Output job id.  */
	  fjobid = TRUE;
	  break;

	case 'm':
	  /* Mail to requesting user.  */
	  fCmail = TRUE;
	  break;

	case 'n':
	  /* Notify remote user.  */
	  zCnotify = optarg;
	  break;

	case 'r':
	  /* Don't start uucico when finished.  */
	  fuucico = FALSE;
	  break;

	case 'R':
	  /* Copy directories recursively.  */
	  frecursive = TRUE;
	  break;

	case 's':
	  /* Report status to named file.  */
	  zstatus_file = optarg;
	  break;

	case 't':
	  /* Emulate uuto.  */
	  fuuto = TRUE;
	  break;

	case 'u':
	  /* Set user name.  */
	  zCuser = optarg;
	  break;

	case 'W':
	  /* Expand only local file names.  */
	  fCexpand = FALSE;
	  break;

	case 'x':
#if DEBUG > 1
	  /* Set debugging level.  */
	  iDebug |= idebug_parse (optarg);
#endif
	  break;

	case 'v':
	  /* Print version and exit.  */
	  printf ("%s: Taylor UUCP %s, copyright (C) 1991, 92, 93, 94, 1995 Ian Lance Taylor\n",
		  zProgram, VERSION);
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 1:
	  /* --help.  */
	  uchelp ();
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  ucusage ();
	  /*NOTREACHED*/
	}
    }

  if (! UUCONF_GRADE_LEGAL (bCgrade)
      || ((bCgrade < '0' || bCgrade > '9')
	  && (bCgrade < 'a' || bCgrade > 'z')
	  && (bCgrade < 'A' || bCgrade > 'Z')))
    {
      ulog (LOG_ERROR, "Ignoring illegal grade");
      bCgrade = BDEFAULT_UUCP_GRADE;
    }

  /* The user name must contain a '!', which is treated as a remote
     name, to avoid spoofing of other users (there is no advantage to
     spoofing remote users, except to send them random bits of mail,
     which you can do anyhow).  */
  if (zCuser != NULL)
    {
      if (strchr (zCuser, '!') != NULL)
	fCremote = TRUE;
      else
	{
	  ulog (LOG_ERROR, "Ignoring local user name");
	  zCuser = NULL;
	}
    }

  if (argc - optind < 2)
    ucusage ();

  iuuconf = uuconf_init (&puuconf, (const char *) NULL, zconfig);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
  pCuuconf = puuconf;

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

  /* See if we are going to need to know the current directory.  We
     just check each argument to see whether it's an absolute
     pathname.  We actually aren't going to need the cwd if fCexpand
     is FALSE and the file is remote, but so what.  */
  fgetcwd = FALSE;
  for (i = optind; i < argc; i++)
    {
      zexclam = strrchr (argv[i], '!');
      if (zexclam == NULL)
	zexclam = argv[i];
      else
	++zexclam;
      if (fsysdep_needs_cwd (zexclam))
	{
	  fgetcwd = TRUE;
	  break;
	}
    }

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

  usysdep_initialize (puuconf, INIT_SUID | (fgetcwd ? INIT_GETCWD : 0));

  ulog_fatal_fn (ucabort);

  if (zCuser == NULL)
    zCuser = zsysdep_login_name ();

  iuuconf = uuconf_localname (puuconf, &zClocalname);
  if (iuuconf == UUCONF_NOT_FOUND)
    {
      zClocalname = zsysdep_localname ();
      if (zClocalname == NULL)
	exit (EXIT_FAILURE);
    }
  else if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  /* Get the local system information.  */
  iuuconf = uuconf_system_info (puuconf, zClocalname, &slocalsys);
  if (iuuconf != UUCONF_SUCCESS)
    {
      if (iuuconf != UUCONF_NOT_FOUND)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
      iuuconf = uuconf_system_local (puuconf, &slocalsys);
      if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
      slocalsys.uuconf_zname = (char *) zClocalname;
    }

  /* If we are emulating uuto, translate the destination argument, and
     notify the destination user.  This had better not turn into
     something that requires the current directory, or we may have
     passed INIT_GETCWD incorrectly.  */
  if (fuuto)
    {
      if (*zCnotify == '\0')
	{
	  zexclam = strrchr (argv[argc - 1], '!');
	  if (zexclam == NULL)
	    ucusage ();
	  zCnotify = zexclam + 1;
	}
      argv[argc - 1] = zsysdep_uuto (argv[argc - 1], zClocalname);
      if (argv[argc - 1] == NULL)
	ucusage ();
    }

  /* Set up the file transfer options.  */
  zoptions = abCsend_options;
  if (fCcopy)
    *zoptions++ = 'C';
  else
    *zoptions++ = 'c';
  if (fCmkdirs)
    *zoptions++ = 'd';
  else
    *zoptions++ = 'f';
  if (fCmail)
    *zoptions++ = 'm';
  if (*zCnotify != '\0')
    *zoptions++ = 'n';
  *zoptions = '\0';

  zoptions = abCrec_options;
  if (fCmkdirs)
    *zoptions++ = 'd';
  else
    *zoptions++ = 'f';
  if (fCmail)
    *zoptions++ = 'm';
  *zoptions = '\0';

  argv[argc - 1] = zremove_local_sys (&slocalsys, argv[argc - 1]);

  zexclam = strchr (argv[argc - 1], '!');
  if (zexclam == NULL)
    {
      zdestsys = zClocalname;
      zdestfile = argv[argc - 1];
      fClocaldest = TRUE;
    }
  else
    {
      size_t clen;
      char *zcopy;

      clen = zexclam - argv[argc - 1];
      zcopy = zbufalc (clen + 1);
      memcpy (zcopy, argv[argc - 1], clen);
      zcopy[clen] = '\0';
      zdestsys = zcopy;

      zdestfile = zexclam + 1;

      fClocaldest = FALSE;
    }

  iuuconf = uuconf_system_info (puuconf, zdestsys, &sCdestsys);
  if (iuuconf != UUCONF_SUCCESS)
    {
      if (iuuconf != UUCONF_NOT_FOUND)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
      if (fClocaldest)
	{
	  iuuconf = uuconf_system_local (puuconf, &sCdestsys);
	  if (iuuconf != UUCONF_SUCCESS)
	    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
	  sCdestsys.uuconf_zname = (char *) zClocalname;
	}
      else
	{
	  if (! funknown_system (puuconf, zdestsys, &sCdestsys))
	    ulog (LOG_FATAL, "%s: System not found", zdestsys);
	}
    }

  /* Here zdestfile is the destination file name following the
     destination system name (if any); it may contain other systems to
     forward the files through.  Isolate the file from the list of
     systems.  */
  zexclam = strrchr (zdestfile, '!');
  if (zexclam == NULL)
    zCforward = NULL;
  else
    {
      size_t clen;

#if DEBUG > 0
      if (fClocaldest)
	ulog (LOG_FATAL, "Can't happen");
#endif
      clen = zexclam - zdestfile;
      zCforward = zbufalc (clen + 1);
      memcpy (zCforward, zdestfile, clen);
      zCforward[clen] = '\0';
      zdestfile = zexclam + 1;
    }

  /* Turn the destination into an absolute path, unless it is on a
     remote system and -W was used.  */
  if (fClocaldest)
    zdestfile = zsysdep_local_file_cwd (zdestfile, sCdestsys.uuconf_zpubdir,
					(boolean *) NULL);
  else if (fCexpand)
    zdestfile = zsysdep_add_cwd (zdestfile);
  if (zdestfile == NULL)
    {
      ulog_close ();
      usysdep_exit (FALSE);
    }

  /* Process each source argument.  */
  for (i = optind; i < argc - 1 && ! FGOT_SIGNAL (); i++)
    {
      boolean flocal;
      char *zfrom;

      fCneeds_cwd = FALSE;

      argv[i] = zremove_local_sys (&slocalsys, argv[i]);

      if (strchr (argv[i], '!') != NULL)
	{
	  flocal = FALSE;
	  zfrom = zbufcpy (argv[i]);
	}
      else
	{
	  /* This is a local file.  Make sure we get it out of the
	     original directory.  We don't support local wildcards,
	     leaving that to the shell.  */
	  flocal = TRUE;
	  if (fsysdep_needs_cwd (argv[i]))
	    fCneeds_cwd = TRUE;
	  zfrom = zsysdep_local_file_cwd (argv[i],
					  sCdestsys.uuconf_zpubdir,
					  (boolean *) NULL);
	  if (zfrom == NULL)
	    ucabort ();
	}

      if (! flocal || ! fsysdep_directory (zfrom))
	uccopy (zfrom, zdestfile);
      else
	{
	  char *zbase, *zindir;

	  if (! frecursive)
	    ulog (LOG_FATAL, "%s: directory without -R", zfrom);

	  zbase = zsysdep_base_name (zfrom);
	  if (zbase == NULL)
	    ucabort ();
	  zindir = zsysdep_in_dir (zdestfile, zbase);
	  ubuffree (zbase);
	  if (zindir == NULL)
	    ucabort ();
	  usysdep_walk_tree (zfrom, ucdirfile, zindir);
	  ubuffree (zindir);
	}

      ubuffree (zfrom);
    }

  /* See if we got an interrupt, presumably from the user.  */
  if (FGOT_SIGNAL ())
    ucabort ();

  /* Now push out the actual commands, making log entries for them.  */
  ulog_to_file (puuconf, TRUE);
  ulog_user (zCuser);

  ucspool_cmds (fjobid);

  ulog_close ();

  if (! fuucico)
    fexit = TRUE;
  else
    {
      const char *zsys;
      boolean fany;

      zsys = zcone_system (&fany);

      if (zsys == NULL && ! fany)
	fexit = TRUE;
      else
	{
	  const char *zarg;
	  char *zconfigarg;

	  if (zsys == NULL)
	    zarg = "-r1";
	  else
	    {
	      char *z;

	      z = zbufalc (sizeof "-Cs" + strlen (zsys));
	      sprintf (z, "-Cs%s", zsys);
	      zarg = z;
	    }

	  if (zconfig == NULL)
	    zconfigarg = NULL;
	  else
	    {
	      zconfigarg = zbufalc (sizeof "-I" + strlen (zconfig));
	      sprintf (zconfigarg, "-I%s", zconfig);
	    }

	  fexit = fsysdep_run (FALSE, "uucico", zarg, zconfigarg);
	}
    }

  usysdep_exit (fexit);

  /* Avoid error about not returning.  */
  return 0;
}

/* Print usage message and die.  */

static void
ucusage ()
{
  fprintf (stderr,
	   "Usage: %s [options] file1 [file2 ...] dest\n", zProgram);
  fprintf (stderr, "Use %s --help for help\n", zProgram);
  exit (EXIT_FAILURE);
}

/* Print help message.  */

static void
uchelp ()
{
  printf ("Taylor UUCP %s, copyright (C) 1991, 92, 93, 94, 1995 Ian Lance Taylor\n",
	   VERSION);
  printf ("Usage: %s [options] file1 [file2 ...] dest\n", zProgram);
  printf (" -c,--nocopy: Do not copy local files to spool directory\n");
  printf (" -C,-p,--copy: Copy local files to spool directory (default)\n");
  printf (" -d,--directories: Create necessary directories (default)\n");
  printf (" -f,--nodirectories: Do not create directories (fail if they do not exist)\n");
  printf (" -g,--grade grade: Set job grade (must be alphabetic)\n");
  printf (" -m,--mail: Report status of copy by mail\n");
  printf (" -n,--notify user: Report status of copy by mail to remote user\n");
  printf (" -R,--recursive: Copy directories recursively\n");
  printf (" -r,--nouucico: Do not start uucico daemon\n");
  printf (" -s,--status file: Report completion status to file\n");
  printf (" -j,--jobid: Report job id\n");
  printf (" -W,--noexpand: Do not add current directory to remote filenames\n");
  printf (" -t,--uuto: Emulate uuto\n");
  printf (" -u,--usage name: Set user name\n");
  printf (" -x,--debug debug: Set debugging level\n");
#if HAVE_TAYLOR_CONFIG
  printf (" -I,--config file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  printf (" -v,--version: Print version and exit\n");
  printf (" --help: Print help and exit\n");
}

/* This is called for each file in a directory heirarchy.  */

static void
ucdirfile (zfull, zrelative, pinfo)
     const char *zfull;
     const char *zrelative;
     pointer pinfo;
{
  const char *zdestfile = (const char *) pinfo;
  char *zto;

  zto = zsysdep_in_dir (zdestfile, zrelative);
  if (zto == NULL)
    ucabort ();

  uccopy (zfull, zto);

  ubuffree (zto);
}

/* Handle the copying of one regular file.  The zdest argument is the
   destination file; if we are recursively copying a directory, it
   will be extended by any subdirectory names.  Note that zdest is an
   absolute path.  */

static void
uccopy (zfile, zdest)
     const char *zfile;
     const char *zdest;
{
  struct scmd s;
  char *zexclam;
  char *zto;

  zexclam = strchr (zfile, '!');

  if (zexclam == NULL)
    {
      openfile_t efrom;

      /* Copy from a local file.  Make sure the user has access to
	 this file, since we are running setuid.  */
      if (! fsysdep_access (zfile))
	ucabort ();

      /* If this copy is being requested by a remote system, we may
	 transfer the file if it needs the current working directory
	 (meaning, I hope, that it is in the execution directory) or
	 it is on the permitted transfer list.  Note that unlike most
	 of the other checks, this one is not double-checked by
	 uucico.  */
      if (fCremote
	  && ! fCneeds_cwd
	  && ! fin_directory_list (zfile, sCdestsys.uuconf_pzremote_send,
				   sCdestsys.uuconf_zpubdir, TRUE,
				   TRUE, (const char *) NULL))
	ulog (LOG_FATAL, "Not permitted to send %s", zfile);

      if (fClocaldest)
	{
	  boolean fok;

	  /* Copy one local file to another.  */

	  /* Check that we have permission to receive into the desired
	     directory.  */
	  if (fCremote)
	    fok = fin_directory_list (zdest,
				      sCdestsys.uuconf_pzremote_receive,
				      sCdestsys.uuconf_zpubdir, TRUE,
				      FALSE, (const char *) NULL);
	  else
	    fok = fin_directory_list (zdest,
				      sCdestsys.uuconf_pzlocal_receive,
				      sCdestsys.uuconf_zpubdir, TRUE,
				      FALSE, zCuser);
	  if (! fok)
	    ulog (LOG_FATAL, "Not permitted to receive to %s", zdest);

	  zto = zsysdep_add_base (zdest, zfile);
	  if (zto == NULL)
	    ucabort ();

	  efrom = esysdep_user_fopen (zfile, TRUE, TRUE);
	  if (! ffileisopen (efrom))
	    ucabort ();
	  if (! fcopy_open_file (efrom, zto, FALSE, fCmkdirs, TRUE))
	    ucabort ();
	  (void) ffileclose (efrom);
	  ubuffree (zto);
	}
      else
	{
	  const char *zloc;
	  char abtname[CFILE_NAME_LEN];
	  unsigned int imode;
	  char *ztemp;

	  /* Copy a local file to a remote file.  We may have to
	     copy the local file to the spool directory.  */
	  imode = ixsysdep_file_mode (zfile);
	  if (imode == 0)
	    ucabort ();

	  zloc = sCdestsys.uuconf_zlocalname;
	  if (zloc == NULL)
	    zloc = zClocalname;

	  ztemp = zsysdep_data_file_name (&sCdestsys, zloc, bCgrade,
					  FALSE, abtname, (char *) NULL,
					  (char *) NULL);
	  if (ztemp == NULL)
	    ucabort ();

	  if (! fCcopy)
	    {
	      /* If we are copying the file, we don't actually use the
		 temporary file; we still want to get a name for the
		 other system to use as a key for file restart.  */
	      ubuffree (ztemp);

	      /* Make sure the daemon will be permitted to send
		 this file.  */
	      if (! fsysdep_daemon_access (zfile))
		ucabort ();
	      if (! fin_directory_list (zfile, sCdestsys.uuconf_pzlocal_send,
					sCdestsys.uuconf_zpubdir, TRUE, TRUE,
					(fCremote
					 ? (const char *) NULL
					 : zCuser)))
		ulog (LOG_FATAL,
		      "Daemon not permitted to send %s (suggest --copy)",
		      zfile);
	    }
	  else
	    {
	      efrom = esysdep_user_fopen (zfile, TRUE, TRUE);
	      if (! ffileisopen (efrom))
		ucabort ();
	      ucrecord_file (ztemp);
	      if (! fcopy_open_file (efrom, ztemp, FALSE, TRUE, TRUE))
		ucabort ();
	      (void) ffileclose (efrom);
	    }

	  if (zCforward == NULL)
	    {
	      /* We're not forwarding.  Just send the file.  */
	      s.bcmd = 'S';
	      s.bgrade = bCgrade;
	      s.pseq = NULL;
	      s.zfrom = zbufcpy (zfile);
	      s.zto = zbufcpy (zdest);
	      s.zuser = zCuser;
	      s.zoptions = abCsend_options;
	      s.ztemp = zbufcpy (abtname);
	      s.imode = imode;
	      s.znotify = zCnotify;
	      s.cbytes = -1;
	      s.zcmd = NULL;
	      s.ipos = 0;

	      ucadd_cmd (&sCdestsys, &s, (const char *) NULL);
	    }
	  else
	    {
	      char *zbase;
	      char *zxqt;
	      char abxtname[CFILE_NAME_LEN];
	      char abdname[CFILE_NAME_LEN];
	      char abxname[CFILE_NAME_LEN];
	      FILE *e;
	      char *zlog;

	      /* We want to forward this file through sCdestsys to
		 some other system(s).  We set up a remote execution
		 of uucp on sCdestsys to forward the file along.  */
	      zbase = zsysdep_base_name (zfile);
	      if (zbase == NULL)
		ucabort ();

	      zxqt = zsysdep_data_file_name (&sCdestsys, zloc, bCgrade,
					     TRUE, abxtname, abdname,
					     abxname);
	      if (zxqt == NULL)
		ucabort ();
	      e = esysdep_fopen (zxqt, FALSE, FALSE, TRUE);
	      if (e == NULL)
		ucabort ();
	      ucrecord_file (zxqt);

	      fprintf (e, "U %s %s\n", zCuser, zloc);
	      fprintf (e, "F %s %s\n", abdname, zbase);
	      fprintf (e, "C uucp -C");
	      if (fCmkdirs)
		fprintf (e, " -d");
	      else
		fprintf (e, " -f");
	      fprintf (e, " -g %c", bCgrade);
	      if (fCmail)
		fprintf (e, " -m");
	      if (*zCnotify != '\0')
		fprintf (e, " -n %s", zCnotify);
	      if (! fCexpand)
		fprintf (e, " -W");
	      fprintf (e, " %s %s!%s\n", zbase, zCforward, zdest);

	      ubuffree (zbase);

	      if (! fstdiosync (e, zxqt))
		ulog (LOG_FATAL, "fsync failed");
	      if (fclose (e) != 0)
		ulog (LOG_FATAL, "fclose: %s", strerror (errno));

	      /* Send the execution file.  */
	      s.bcmd = 'S';
	      s.bgrade = bCgrade;
	      s.pseq = NULL;
	      s.zfrom = zbufcpy (abxtname);
	      s.zto = zbufcpy (abxname);
	      s.zuser = zCuser;
 	      s.zoptions = "C";
	      s.ztemp = s.zfrom;
	      s.imode = 0666;
	      s.znotify = NULL;
	      s.cbytes = -1;
	      s.zcmd = NULL;
	      s.ipos = 0;

	      zlog = zbufalc (sizeof "Queuing uucp  !" + strlen (zfile)
			      + strlen (zCforward) + strlen (zdest));
	      sprintf (zlog, "Queuing uucp %s %s!%s", zfile, zCforward,
		       zdest);

	      ucadd_cmd (&sCdestsys, &s, zlog);

	      /* Send the data file.  */
	      s.bcmd = 'S';
	      s.bgrade = bCgrade;
	      s.pseq = NULL;
	      s.zfrom = zbufcpy (zfile);
	      s.zto = zbufcpy (abdname);
	      s.zuser = zCuser;
 	      s.zoptions = fCcopy ? "C" : "c";
	      s.ztemp = zbufcpy (abtname);
	      s.imode = 0666;
	      s.znotify = NULL;
	      s.cbytes = -1;
	      s.zcmd = NULL;
	      s.ipos = 0;

	      ucadd_cmd (&sCdestsys, &s, "");
	    }
	}
    }
  else
    {
      char *zfrom;
      char *zforward;
      size_t clen;
      char *zcopy;
      struct uuconf_system *qfromsys;
      int iuuconf;
      const char *zloc;

      /* Copy from a remote file.  Get the file name after any systems
	 we may need to forward the file from.  */
      zfrom = strrchr (zfile, '!');
      if (zfrom == zexclam)
	zforward = NULL;
      else
	{
	  clen = zfrom - zexclam - 1;
	  zforward = zbufalc (clen + 1);
	  memcpy (zforward, zexclam + 1, clen);
	  zforward[clen] = '\0';
	}

      ++zfrom;
      if (fCexpand)
	{
	  /* Add the current directory to the filename if it's not
	     already there.  */
	  zfrom = zsysdep_add_cwd (zfrom);
	  if (zfrom == NULL)
	    ucabort ();
	}

      /* Read the system information.  */
      clen = zexclam - zfile;
      zcopy = zbufalc (clen + 1);
      memcpy (zcopy, zfile, clen);
      zcopy[clen] = '\0';

      qfromsys = ((struct uuconf_system *)
		  xmalloc (sizeof (struct uuconf_system)));

      iuuconf = uuconf_system_info (pCuuconf, zcopy, qfromsys);
      if (iuuconf == UUCONF_NOT_FOUND)
	{
	  if (! funknown_system (pCuuconf, zcopy, qfromsys))
	    ulog (LOG_FATAL, "%s: System not found", zcopy);
	}
      else if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_FATAL, pCuuconf, iuuconf);
      ubuffree (zcopy);

      zloc = qfromsys->uuconf_zlocalname;
      if (zloc == NULL)
	zloc = zClocalname;

      if (zforward == NULL && fClocaldest)
	{
	  boolean fok;

	  /* The file is to come directly from qfromsys to the local
	     system.  */

	  /* Check that we have permission to receive into the desired
	     directory.  If we don't have permission, uucico will
	     fail.  */
	  if (fCremote)
	    fok = fin_directory_list (zdest,
				      qfromsys->uuconf_pzremote_receive,
				      qfromsys->uuconf_zpubdir, TRUE,
				      FALSE, (const char *) NULL);
	  else
	    fok = fin_directory_list (zdest,
				      qfromsys->uuconf_pzlocal_receive,
				      qfromsys->uuconf_zpubdir, TRUE,
				      FALSE, zCuser);
	  if (! fok)
	    ulog (LOG_FATAL, "Not permitted to receive to %s", zdest);

	  /* If the remote filespec is wildcarded, we must generate an
	     'X' request.  We currently check for Unix shell
	     wildcards.  Note that it should do no harm to mistake a
	     non-wildcard for a wildcard.  */
	  if (zfrom[strcspn (zfrom, "*?[")] != '\0')
	    {
	      s.bcmd = 'X';
	      zto = zbufalc (strlen (zloc) + strlen (zdest) + sizeof "!");
	      sprintf (zto, "%s!%s", zloc, zdest);
	    }
	  else
	    {
	      s.bcmd = 'R';
	      zto = zbufcpy (zdest);
	    }

	  s.bgrade = bCgrade;
	  s.pseq = NULL;
	  s.zfrom = zfrom;
	  s.zto = zto;
	  s.zuser = zCuser;
	  s.zoptions = abCrec_options;
	  s.ztemp = "";
	  s.imode = 0;
	  s.znotify = "";
	  s.cbytes = -1;
	  s.zcmd = NULL;
	  s.ipos = 0;

	  ucadd_cmd (qfromsys, &s, (const char *) NULL);
	}
      else
	{
	  char *zxqt;
	  char abtname[CFILE_NAME_LEN];
	  char abxname[CFILE_NAME_LEN];
	  FILE *e;
	  char *zcmd;
	  char *zlog;

	  /* The file either comes from some other system through
	     qfromsys or is intended for some other system.  Send an
	     execution request to qfromsys to handle everything.  */
	  zxqt = zsysdep_data_file_name (qfromsys, zloc, bCgrade, TRUE,
					 abtname, (char *) NULL,
					 abxname);
	  if (zxqt == NULL)
	    ucabort ();
	  e = esysdep_fopen (zxqt, FALSE, FALSE, TRUE);
	  if (e == NULL)
	    ucabort ();
	  ucrecord_file (zxqt);

	  fprintf (e, "U %s %s\n", zCuser, zloc);
	  fprintf (e, "C uucp -C");
	  if (fCmkdirs)
	    fprintf (e, " -d");
	  else
	    fprintf (e, " -f");
	  fprintf (e, " -g %c", bCgrade);
	  if (fCmail)
	    fprintf (e, " -m");
	  if (*zCnotify != '\0')
	    fprintf (e, " -n %s", zCnotify);
	  if (! fCexpand)
	    fprintf (e, " -W");

	  clen = (strlen (zfrom) + strlen (zloc)
		  + strlen (sCdestsys.uuconf_zname) + strlen (zdest));
	  if (zforward != NULL)
	    clen += strlen (zforward);
	  if (zCforward != NULL)
	    clen += strlen (zCforward);
	  zcmd = zbufalc (sizeof "! !!!" + clen);
	  *zcmd = '\0';
	  if (zforward != NULL)
	    sprintf (zcmd + strlen (zcmd), "%s!", zforward);
	  sprintf (zcmd + strlen (zcmd), "%s %s!", zfrom, zloc);
	  if (! fClocaldest)
	    sprintf (zcmd + strlen (zcmd), "%s!", sCdestsys.uuconf_zname);
	  if (zCforward != NULL)
	    sprintf (zcmd + strlen (zcmd), "%s!", zCforward);
	  sprintf (zcmd + strlen (zcmd), "%s", zdest);

	  fprintf (e, " %s\n", zcmd);

	  if (! fstdiosync (e, zxqt))
	    ulog (LOG_FATAL, "fsync failed");
	  if (fclose (e) != 0)
	    ulog (LOG_FATAL, "fclose: %s", strerror (errno));

	  /* Send the execution file.  */
	  s.bcmd = 'S';
	  s.bgrade = bCgrade;
	  s.pseq = NULL;
	  s.zfrom = zbufcpy (abtname);
	  s.zto = zbufcpy (abxname);
	  s.zuser = zCuser;
	  s.zoptions = "C";
	  s.ztemp = s.zfrom;
	  s.imode = 0666;
	  s.znotify = NULL;
	  s.cbytes = -1;
	  s.zcmd = NULL;
	  s.ipos = 0;

	  zlog = zbufalc (sizeof "Queueing uucp " + strlen (zcmd));
	  sprintf (zlog, "Queueing uucp %s", zcmd);

	  ucadd_cmd (qfromsys, &s, zlog);

	  ubuffree (zcmd);
	  ubuffree (zforward);
	}
    }
}

/* We keep a list of jobs for each system.  */

struct sjob
{
  struct sjob *qnext;
  const struct uuconf_system *qsys;
  int ccmds;
  struct scmd *pascmds;
  const char **pazlogs;
};

static struct sjob *qCjobs;

static void
ucadd_cmd (qsys, qcmd, zlog)
     const struct uuconf_system *qsys;
     const struct scmd *qcmd;
     const char *zlog;
{
  struct sjob *qjob;

  if (! qsys->uuconf_fcall_transfer
      && ! qsys->uuconf_fcalled_transfer)
    ulog (LOG_FATAL, "Not permitted to transfer files to or from %s",
	  qsys->uuconf_zname);

  for (qjob = qCjobs; qjob != NULL; qjob = qjob->qnext)
    if (strcmp (qjob->qsys->uuconf_zname, qsys->uuconf_zname) == 0)
      break;

  if (qjob == NULL)
    {
      qjob = (struct sjob *) xmalloc (sizeof (struct sjob));
      qjob->qnext = qCjobs;
      qjob->qsys = qsys;
      qjob->ccmds = 0;
      qjob->pascmds = NULL;
      qjob->pazlogs = NULL;
      qCjobs = qjob;
    }

  qjob->pascmds = ((struct scmd *)
		   xrealloc ((pointer) qjob->pascmds,
			     (qjob->ccmds + 1) * sizeof (struct scmd)));
  qjob->pascmds[qjob->ccmds] = *qcmd;
  qjob->pazlogs = ((const char **)
		   xrealloc ((pointer) qjob->pazlogs,
			     (qjob->ccmds + 1) * sizeof (const char *)));
  qjob->pazlogs[qjob->ccmds] = zlog;
  ++qjob->ccmds;
}

static void
ucspool_cmds (fjobid)
     boolean fjobid;
{
  struct sjob *qjob;
  char *zjobid;

  for (qjob = qCjobs; qjob != NULL; qjob = qjob->qnext)
    {
      ulog_system (qjob->qsys->uuconf_zname);
      zjobid = zsysdep_spool_commands (qjob->qsys, bCgrade, qjob->ccmds,
				       qjob->pascmds);
      if (zjobid != NULL)
	{
	  int i;
	  struct scmd *qcmd;
	  const char **pz;

	  for (i = 0, qcmd = qjob->pascmds, pz = qjob->pazlogs;
	       i < qjob->ccmds;
	       i++, qcmd++, pz++)
	    {
	      if (*pz != NULL)
		{
		  if (**pz != '\0')
		    ulog (LOG_NORMAL, "%s", *pz);
		}
	      else if (qcmd->bcmd == 'S')
		ulog (LOG_NORMAL, "Queuing send of %s to %s",
		      qcmd->zfrom, qcmd->zto);
	      else if (qcmd->bcmd == 'R')
		ulog (LOG_NORMAL, "Queuing request of %s to %s",
		      qcmd->zfrom, qcmd->zto);
	      else
		{
		  const char *zto;

		  zto = strrchr (qcmd->zto, '!');
		  if (zto != NULL)
		    ++zto;
		  else
		    zto = qcmd->zto;
		  ulog (LOG_NORMAL, "Queuing request of %s to %s",
			qcmd->zfrom, zto);
		}
	    }

	  if (fjobid)
	    printf ("%s\n", zjobid);

	  ubuffree (zjobid);
	}
    }
}

/* Return the system name for which we have created commands, or NULL
   if we've created commands for more than one system.  Set *pfany to
   FALSE if we didn't create work for any system.  */

static const char *
zcone_system (pfany)
     boolean *pfany;
{
  if (qCjobs == NULL)
    {
      *pfany = FALSE;
      return NULL;
    }

  *pfany = TRUE;

  if (qCjobs->qnext == NULL)
    return qCjobs->qsys->uuconf_zname;
  else
    return NULL;
}

/* Keep track of all files we have created so that we can delete them
   if we get a signal.  The argument will be on the heap.  */

static int cCfiles;
static const char **pCaz;

static void
ucrecord_file (zfile)
     const char *zfile;
{
  pCaz = (const char **) xrealloc ((pointer) pCaz,
				   (cCfiles + 1) * sizeof (const char *));
  pCaz[cCfiles] = zfile;
  ++cCfiles;
}

/* Delete all the files we have recorded and exit.  */

static void
ucabort ()
{
  int i;

  for (i = 0; i < cCfiles; i++)
    (void) remove (pCaz[i]);
  ulog_close ();
  usysdep_exit (FALSE);
}
