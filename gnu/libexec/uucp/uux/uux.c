/* uux.c
   Prepare to execute a command on a remote system.

   Copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor

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
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"

#if USE_RCS_ID
const char uux_rcsid[] = "$Id: uux.c,v 1.4 1994/05/07 18:14:35 ache Exp $";
#endif

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "sysdep.h"
#include "getopt.h"

#include <ctype.h>
#include <errno.h>

/* These character lists should, perhaps, be in sysdep.h.  */

/* This is the list of shell metacharacters that we check for.  If one
   of these is present, we request uuxqt to execute the command with
   /bin/sh.  Otherwise we let it execute using execve.  */

#define ZSHELLCHARS "\"'`*?[;&()|<>\\$"

/* This is the list of word separators.  We break filename arguments
   at these characters.  */
#define ZSHELLSEPS ";&*|<> \t"

/* This is the list of word separators without the redirection
   operators.  */
#define ZSHELLNONREDIRSEPS ";&*| \t"

/* The name of the execute file.  */
const char *zXxqt_name;

/* The execute file we are creating.  */
static FILE *eXxqt_file;

/* A list of commands to be spooled.  */
static struct scmd *pasXcmds;
static int cXcmds;

/* A file to close if we're forced to exit.  */
static FILE *eXclose;

/* A list of file names which will match the file names which appear
   in the uucico logs.  */
static char *zXnames;

/* Local functions.  */
static void uxusage P((void));
static void uxhelp P((void));
static void uxadd_xqt_line P((int bchar, const char *z1, const char *z2));
static void uxadd_send_file P((const char *zfrom, const char *zto,
			       const char *zoptions, const char *ztemp,
			       const char *zforward,
			       const struct uuconf_system *qxqtsys,
			       const char *zxqtloc,
			       int bgrade));
static void uxcopy_stdin P((FILE *e));
static void uxrecord_file P((const char *zfile));
static void uxabort P((void));
static void uxadd_name P((const char *));

/* Long getopt options.  */
static const struct option asXlongopts[] =
{
  { "requestor", required_argument, NULL, 'a' },
  { "return-stdin", no_argument, NULL, 'b' },
  { "nocopy", no_argument, NULL, 'c' },
  { "copy", no_argument, NULL, 'C' },
  { "grade", required_argument, NULL, 'g' },
  { "jobid", no_argument, NULL, 'j' },
  { "link", no_argument, NULL, 'l' },
  { "notification", required_argument, NULL, 2 },
  { "stdin", no_argument, NULL, 'p' },
  { "nouucico", no_argument, NULL, 'r' },
  { "status", required_argument, NULL, 's' },
  { "noexpand", no_argument, NULL, 'W' },
  { "config", required_argument, NULL, 'I' },
  { "debug", required_argument, NULL, 'x' },
  { "version", no_argument, NULL, 'v' },
  { "help", no_argument, NULL, 1 },
  { NULL, 0, NULL, 0 }
};

/* The main routine.  */

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* -a: requestor address for status reports.  */
  const char *zrequestor = NULL;
  /* -b: if true, return standard input on error.  */
  boolean fretstdin = FALSE;
  /* -c,-C: if true, copy to spool directory.  */
  boolean fcopy = FALSE;
  /* -c: set if -c appears explicitly; if it and -l appear, then if the
     link fails we don't copy the file.  */
  boolean fdontcopy = FALSE;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  /* -j: output job id.  */
  boolean fjobid = FALSE;
  /* -g: job grade.  */
  char bgrade = BDEFAULT_UUX_GRADE;
  /* -l: link file to spool directory.  */
  boolean flink = FALSE;
  /* -n: do not notify upon command completion.  */
  boolean fno_ack = FALSE;
  /* -p: read standard input for command standard input.  */
  boolean fread_stdin = FALSE;
  /* -r: do not start uucico when finished.  */
  boolean fuucico = TRUE;
  /* -s: report status to named file.  */
  const char *zstatus_file = NULL;
  /* -W: only expand local file names.  */
  boolean fexpand = TRUE;
  /* -z: report status only on error.  */
  boolean ferror_ack = FALSE;
  int iopt;
  pointer puuconf;
  int iuuconf;
  const char *zlocalname;
  const char *zxqtloc;
  int i;
  size_t clen;
  char *zargs;
  char *zarg;
  char *zcmd;
  const char *zsys;
  char *zexclam;
  boolean fgetcwd;
  const char *zuser;
  struct uuconf_system sxqtsys;
  boolean fxqtlocal;
  char *zforward;
  char **pzargs;
  int calloc_args;
  int cargs;
  char abxqt_tname[CFILE_NAME_LEN];
  char abxqt_xname[CFILE_NAME_LEN];
  const char *zinput_from;
  const char *zinput_to;
  const char *zinput_temp;
  boolean finputcopied;
  char *zcall_system;
  boolean fcall_any;
  struct uuconf_system slocalsys;
  boolean fneedshell;
  char *zfullcmd;
  char aboptions[10];
  boolean fexit;

  zProgram = argv[0];

  /* We need to be able to read a single - as an option, which getopt
     won't do.  We handle this by using getopt to scan the argument
     list multiple times, replacing any single "-" with "-p".  */
  opterr = 0;
  while (1)
    {
      while (getopt_long (argc, argv, "+a:bcCg:I:jlnprs:Wvx:z",
			  asXlongopts, (int *) NULL) != EOF)
	;
      if (optind >= argc || strcmp (argv[optind], "-") != 0)
	break;
      argv[optind] = zbufcpy ("-p");
      optind = 0;
    }
  opterr = 1;
  optind = 0;

  /* The leading + in the getopt string means to stop processing
     options as soon as a non-option argument is seen.  */
  while ((iopt = getopt_long (argc, argv, "+a:bcCg:I:jlnprs:Wvx:z",
			      asXlongopts, (int *) NULL)) != EOF)
    {
      switch (iopt)
	{
	case 'a':
	  /* Set requestor name: mail address to which status reports
	     should be sent.  */
	  zrequestor = optarg;
	  break;

	case 'b':
	  /* Return standard input on error.  */
	  fretstdin = TRUE;
	  break;

	case 'c':
	  /* Do not copy local files to spool directory.  */
	  fcopy = FALSE;
	  fdontcopy = TRUE;
	  break;

	case 'C':
	  /* Copy local files to spool directory.  */
	  fcopy = TRUE;
	  break;

	case 'I':
	  /* Configuration file name.  */ 
	  if (fsysdep_other_config (optarg))
	    zconfig = optarg;
	  break;

	case 'j':
	  /* Output jobid.  */
	  fjobid = TRUE;
	  break;

	case 'g':
	  /* Set job grade.  */
	  bgrade = optarg[0];
	  break;

	case 'l':
	  /* Link file to spool directory.  */
	  flink = TRUE;
	  break;

	case 'n':
	  /* Do not notify upon command completion.  */
	  fno_ack = TRUE;
	  break;

	case 'p':
	  /* Read standard input for command standard input.  */
	  fread_stdin = TRUE;
	  break;

	case 'r':
	  /* Do not start uucico when finished.  */
	  fuucico = FALSE;
	  break;

	case 's':
	  /* Report status to named file.  */
	  zstatus_file = optarg;
	  break;

	case 'W':
	  /* Only expand local file names.  */
	  fexpand = FALSE;
	  break;

	case 'x':
#if DEBUG > 1
	  /* Set debugging level.  */
	  iDebug |= idebug_parse (optarg);
#endif
	  break;

	case 'z':
	  /* Report status only on error.  */
	  ferror_ack = TRUE;
	  break;

	case 2:
	  /* --notify={true,false,error}.  */
	  if (*optarg == 't'
	      || *optarg == 'T'
	      || *optarg == 'y'
	      || *optarg == 'Y'
	      || *optarg == 'e'
	      || *optarg == 'E')
	    {
	      ferror_ack = TRUE;
	      fno_ack = FALSE;
	    }
	  else if (*optarg == 'f'
		   || *optarg == 'F'
		   || *optarg == 'n'
		   || *optarg == 'N')
	    {
	      ferror_ack = FALSE;
	      fno_ack = TRUE;
	    }
	  break;

	case 'v':
	  /* Print version and exit.  */
	  printf ("%s: Taylor UUCP %s, copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor\n",
		  zProgram, VERSION);
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 1:
	  /* --help.  */
	  uxhelp ();
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  uxusage ();
	  break;
	}
    }

  if (! UUCONF_GRADE_LEGAL (bgrade))
    {
      ulog (LOG_ERROR, "Ignoring illegal grade");
      bgrade = BDEFAULT_UUX_GRADE;
    }

  if (optind == argc)
    uxusage ();

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

  /* The command and files arguments could be quoted in any number of
     ways, so we split them apart ourselves.  We do this before
     calling usysdep_initialize because we want to set fgetcwd
     correctly.  */
  clen = 1;
  for (i = optind; i < argc; i++)
    clen += strlen (argv[i]) + 1;

  zargs = zbufalc (clen);
  *zargs = '\0';
  for (i = optind; i < argc; i++)
    {
      strcat (zargs, argv[i]);
      strcat (zargs, " ");
    }

  /* The first argument is the command to execute.  */
  clen = strcspn (zargs, ZSHELLSEPS);
  zcmd = zbufalc (clen + 1);
  strncpy (zcmd, zargs, clen);
  zcmd[clen] = '\0';
  zargs += clen;

  /* Split the arguments out into an array.  We break the arguments
     into alternating sequences of characters not in ZSHELLSEPS
     and characters in ZSHELLSEPS.  We remove whitespace.  We
     separate the redirection characters '>' and '<' into their
     own arguments to make them easier to process below.  */
  calloc_args = 10;
  pzargs = (char **) xmalloc (calloc_args * sizeof (char *));
  cargs = 0;

  for (zarg = strtok (zargs, " \t");
       zarg != NULL;
       zarg = strtok ((char *) NULL, " \t"))
    {
      while (*zarg != '\0')
	{
	  if (cargs + 1 >= calloc_args)
	    {
	      calloc_args += 10;
	      pzargs = (char **) xrealloc ((pointer) pzargs,
					   calloc_args * sizeof (char *));
	    }

	  clen = strcspn (zarg, ZSHELLSEPS);
	  if (clen > 0)
	    {
	      pzargs[cargs] = zbufalc (clen + 1);
	      memcpy (pzargs[cargs], zarg, clen);
	      pzargs[cargs][clen] = '\0';
	      ++cargs;
	      zarg += clen;
	    }

	  /* We deliberately separate '>' and '<' out.  */
	  if (*zarg != '\0')
	    {
	      clen = strspn (zarg, ZSHELLNONREDIRSEPS);
	      if (clen == 0)
		clen = 1;
	      pzargs[cargs] = zbufalc (clen + 1);
	      memcpy (pzargs[cargs], zarg, clen);
	      pzargs[cargs][clen] = '\0';
	      ++cargs;
	      zarg += clen;
	    }
	}
    }

  /* Now look through the arguments to see if we are going to need the
     current working directory.  We don't try to make a precise
     determination, just a conservative one.  The basic idea is that
     we don't want to get the cwd for 'foo!rmail - user' (note that we
     don't examine the command itself).  */
  fgetcwd = FALSE;
  for (i = 0; i < cargs; i++)
    {
      if (pzargs[i][0] == '(')
	continue;
      zexclam = strrchr (pzargs[i], '!');
      if (zexclam != NULL && fsysdep_needs_cwd (zexclam + 1))
	{
	  fgetcwd = TRUE;
	  break;
	}
      if ((pzargs[i][0] == '<' || pzargs[i][0] == '>')
	  && i + 1 < cargs
	  && strchr (pzargs[i + 1], '!') == NULL
	  && fsysdep_needs_cwd (pzargs[i + 1]))
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

  ulog_fatal_fn (uxabort);

  zuser = zsysdep_login_name ();

  /* Get the local system name.  */
  iuuconf = uuconf_localname (puuconf, &zlocalname);
  if (iuuconf == UUCONF_NOT_FOUND)
    {
      zlocalname = zsysdep_localname ();
      if (zlocalname == NULL)
	exit (EXIT_FAILURE);
    }
  else if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  /* Get the local system information.  */
  iuuconf = uuconf_system_info (puuconf, zlocalname, &slocalsys);
  if (iuuconf != UUCONF_SUCCESS)
    {
      if (iuuconf != UUCONF_NOT_FOUND)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
      iuuconf = uuconf_system_local (puuconf, &slocalsys);
      if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
      slocalsys.uuconf_zname = (char *) zlocalname;
    }

  /* Figure out which system the command is to be executed on.  */
  zcmd = zremove_local_sys (&slocalsys, zcmd);
  zexclam = strchr (zcmd, '!');
  if (zexclam == NULL)
    {
      zsys = zlocalname;
      fxqtlocal = TRUE;
      zforward = NULL;
    }
  else
    {
      *zexclam = '\0';
      zsys = zcmd;
      zcmd = zexclam + 1;
      fxqtlocal = FALSE;

      /* See if we must forward this command through other systems
	 (e.g. uux a!b!cmd).  */
      zexclam = strrchr (zcmd, '!');
      if (zexclam == NULL)
	zforward = NULL;
      else
	{
	  clen = zexclam - zcmd;
	  zforward = zbufalc (clen);
	  memcpy (zforward, zcmd, clen);
	  zforward[clen] = '\0';
	  zcmd = zexclam + 1;
	}
    }

  if (fxqtlocal)
    sxqtsys = slocalsys;
  else
    {
      iuuconf = uuconf_system_info (puuconf, zsys, &sxqtsys);
      if (iuuconf != UUCONF_SUCCESS)
	{
	  if (iuuconf != UUCONF_NOT_FOUND)
	    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
	  if (! funknown_system (puuconf, zsys, &sxqtsys))
	    ulog (LOG_FATAL, "%s: System not found", zsys);
	}
    }

  /* Get the local name the remote system know us as.  */
  zxqtloc = sxqtsys.uuconf_zlocalname;
  if (zxqtloc == NULL)
    zxqtloc = zlocalname;

  /* We can send this as an E command if the execution is on a
     different, directly connected, system and the only file used is
     the standard input and comes from this system.  This is true of
     the common cases of rmail and rnews.  We get an execute file name
     here in case we need it.  */
  if (fxqtlocal)
    zXxqt_name = zsysdep_xqt_file_name ();
  else
    zXxqt_name = zsysdep_data_file_name (&sxqtsys, zxqtloc, bgrade, TRUE,
					 abxqt_tname, (char *) NULL,
					 abxqt_xname);
  if (zXxqt_name == NULL)
    uxabort ();

  uxrecord_file (zXxqt_name);

  /* Look through the arguments.  Any argument containing an
     exclamation point character is interpreted as a file name, and is
     sent to the appropriate system.  */
  zinput_from = NULL;
  zinput_to = NULL;
  zinput_temp = NULL;
  finputcopied = FALSE;
  zcall_system = NULL;
  fcall_any = FALSE;

  for (i = 0; i < cargs; i++)
    {
      const char *zsystem;
      char *zfile;
      char *zforw;
      boolean finput, foutput;
      boolean flocal, fonxqt;

      /* Check for a parenthesized argument; remove the parentheses
	 and otherwise ignore it (this is how an exclamation point is
	 quoted).  */
      if (pzargs[i][0] == '(')
	{
	  clen = strlen (pzargs[i]);
	  if (pzargs[i][clen - 1] != ')')
	    ulog (LOG_ERROR, "Mismatched parentheses");
	  else
	    pzargs[i][clen - 1] = '\0';
	  ++pzargs[i];
	  continue;
	}

      /* Check whether we are doing a redirection.  */
      finput = FALSE;
      foutput = FALSE;
      if (i + 1 < cargs)
	{
	  if (pzargs[i][0] == '<')
	    finput = TRUE;
	  else if (pzargs[i][0] == '>')
	    foutput = TRUE;
	  if (finput || foutput)
	    {
	      pzargs[i] = NULL;
	      i++;
	    }
	}

      zexclam = strchr (pzargs[i], '!');

      /* If there is no exclamation point and no redirection, this
	 argument is left untouched.  */
      if (zexclam == NULL && ! finput && ! foutput)
	continue;

      if (zexclam != NULL)
	{
	  pzargs[i] = zremove_local_sys (&slocalsys, pzargs[i]);
	  zexclam = strchr (pzargs[i], '!');
	}

      /* Get the system name and file name for this file.  */
      if (zexclam == NULL)
	{
	  zsystem = zlocalname;
	  zfile = pzargs[i];
	  flocal = TRUE;
	  zforw = NULL;
	}
      else
	{
	  *zexclam = '\0';
	  zsystem = pzargs[i];
	  zfile = zexclam + 1;
	  flocal = FALSE;
	  zexclam = strrchr (zfile, '!');
	  if (zexclam == NULL)
	    zforw = NULL;
	  else
	    {
	      *zexclam = '\0';
	      zforw = zfile;
	      zfile = zexclam + 1;
	    }
	}

      /* Check if the file is already on the execution system.  */
      if (flocal)
	fonxqt = fxqtlocal;
      else if (fxqtlocal)
	fonxqt = FALSE;
      else if (zforward == NULL ? zforw != NULL : zforw == NULL)
	fonxqt = FALSE;
      else if (zforward != NULL
	       && zforw != NULL
	       && strcmp (zforward, zforw) != 0)
	fonxqt = FALSE;
      else if (strcmp (zsystem, sxqtsys.uuconf_zname) == 0)
	fonxqt = TRUE;
      else if (sxqtsys.uuconf_pzalias == NULL)
	fonxqt = FALSE;
      else
	{
	  char **pzal;

	  fonxqt = FALSE;
	  for (pzal = sxqtsys.uuconf_pzalias; *pzal != NULL; pzal++)
	    {
	      if (strcmp (zsystem, *pzal) == 0)
		{
		  fonxqt = TRUE;
		  break;
		}
	    }
	}

      /* Turn the file into an absolute path.  */
      if (flocal)
	zfile = zsysdep_local_file_cwd (zfile, sxqtsys.uuconf_zpubdir,
					(boolean *) NULL);
      else if (fexpand)
	zfile = zsysdep_add_cwd (zfile);
      if (zfile == NULL)
	uxabort ();

      /* Check for output redirection.  */
      if (foutput)
	{
	  if (flocal)
	    {
	      if (! fin_directory_list (zfile,
					sxqtsys.uuconf_pzremote_receive,
					sxqtsys.uuconf_zpubdir, TRUE,
					FALSE, (const char *) NULL))
		ulog (LOG_FATAL, "Not permitted to create %s", zfile);
	    }

	  /* There are various cases of output redirection.

	     uux cmd >out: The command is executed on the local
		 system, and the output file is placed on the local
		 system (fonxqt is TRUE).

	     uux cmd >a!out: The command is executed on the local
		 system, and the output file is sent to a.

	     uux a!cmd >out: The command is executed on a, and the
		 output file is returned to the local system (flocal
		 is TRUE).

	     uux a!cmd >a!out: The command is executed on a, and the
		 output file is left on a (fonxqt is TRUE).

	     uux a!cmd >b!out: The command is executed on a, and the
		 output file is sent to b; traditionally, I believe
		 that b is relative to a, rather than to the local
		 system.  However, this essentially contradicts the
		 previous two cases, in which the output file is
		 relative to the local system.

	     Now, the cases that we don't handle.

	     uux cmd >a!b!out: The command is executed on the local
		 system, and the output file is sent to b via a.  This
		 requires the local uuxqt to support forwarding of the
		 output file.

	     uux a!b!cmd >out: The command is executed on b, which is
		 reached via a.  Probably the output file is intended
		 for the local system, in which case the uuxqt on b
		 must support forwarding of the output file.

	     uux a!b!cmd >c!out: Is c relative to b or to the local
		 system?  If it's relative to b this is easy to
		 handle.  Otherwise, we must arrange for the file to
		 be sent back to the local system and for the local
		 system to send it on to c.

	     There are many variations of the last case.  It's not at
	     all clear to me how they should be handled.  */
	  if (zforward != NULL || zforw != NULL)
	    ulog (LOG_FATAL, "May not forward standard output");

	  if (fonxqt)
	    uxadd_xqt_line ('O', zfile, (const char *) NULL);
	  else if (flocal)
	    uxadd_xqt_line ('O', zfile, zxqtloc);
	  else
	    uxadd_xqt_line ('O', zfile, zsystem);
	  pzargs[i] = NULL;
	  continue;
	}

      if (finput)
	{
	  if (fread_stdin)
	    ulog (LOG_FATAL, "Standard input specified twice");
	  pzargs[i] = NULL;
	}

      if (flocal)
	{
	  char *zuse;
	  char *zdata;
	  char abtname[CFILE_NAME_LEN];
	  char abdname[CFILE_NAME_LEN];

	  /* It's a local file.  If requested by -C, copy the file to
	     the spool directory.  If requested by -l, link the file
	     to the spool directory; if the link fails, we copy the
	     file, unless -c was explictly used.  If the execution is
	     occurring on the local system, we force the copy as well,
	     because otherwise we would have to have some way to tell
	     uuxqt not to move the file.  If the file is being shipped
	     to another system, we must set up a transfer request.
	     First make sure the user has legitimate access, since we
	     are running setuid.  */
	  if (! fsysdep_access (zfile))
	    uxabort ();

	  zdata = zsysdep_data_file_name (&sxqtsys, zxqtloc, bgrade, FALSE,
					  abtname, abdname, (char *) NULL);
	  if (zdata == NULL)
	    uxabort ();

	  if (fcopy || flink || fxqtlocal)
	    {
	      boolean fdid;

	      uxrecord_file (zdata);

	      fdid = FALSE;
	      if (flink)
		{
		  boolean fworked;

		  if (! fsysdep_link (zfile, zdata, &fworked))
		    uxabort ();

		  if (fworked)
		    fdid = TRUE;
		  else if (fdontcopy)
		    ulog (LOG_FATAL, "%s: Can't link to spool directory",
			  zfile);
		}

	      if (! fdid)
		{
		  openfile_t efile;

		  efile = esysdep_user_fopen (zfile, TRUE, TRUE);
		  if (! ffileisopen (efile))
		    uxabort ();
		  if (! fcopy_open_file (efile, zdata, FALSE, TRUE))
		    uxabort ();
		  (void) ffileclose (efile);
		}

	      zuse = abtname;
	    }
	  else
	    {
	      /* We don't actually use the spool file name, but we
		 need a name to use as the destination.  */
	      ubuffree (zdata);
	      /* Make sure the daemon can access the file.  */
	      if (! fsysdep_daemon_access (zfile))
		uxabort ();
	      if (! fin_directory_list (zfile, sxqtsys.uuconf_pzlocal_send,
					sxqtsys.uuconf_zpubdir, TRUE,
					TRUE, zuser))
		ulog (LOG_FATAL, "Not permitted to send from %s",
		      zfile);

	      zuse = zfile;
	    }

	  if (fxqtlocal)
	    {
	      if (finput)
		uxadd_xqt_line ('I', zuse, (char *) NULL);
	      else
		pzargs[i] = zuse;
	    }
	  else
	    {
	      finputcopied = fcopy || flink;

	      if (finput)
		{
		  zinput_from = zuse;
		  zinput_to = zbufcpy (abdname);
		  zinput_temp = zbufcpy (abtname);
		}
	      else
		{
		  char *zbase;

		  uxadd_send_file (zuse, abdname,
				   finputcopied ? "C" : "c",
				   abtname, zforward, &sxqtsys,
				   zxqtloc, bgrade);
		  zbase = zsysdep_base_name (zfile);
		  if (zbase == NULL)
		    uxabort ();
		  uxadd_xqt_line ('F', abdname, zbase);
		  pzargs[i] = zbase;
		}
	    }
	}
      else if (fonxqt)
	{
	  /* The file is already on the system where the command is to
	     be executed.  */
	  if (finput)
	    uxadd_xqt_line ('I', zfile, (const char *) NULL);
	  else
	    pzargs[i] = zfile;
	}
      else
	{
	  struct uuconf_system sfromsys;
	  char abtname[CFILE_NAME_LEN];
	  struct scmd s;
	  char *zjobid;

	  /* We need to request a remote file.  */
	  iuuconf = uuconf_system_info (puuconf, zsystem, &sfromsys);
	  if (iuuconf != UUCONF_SUCCESS)
	    {
	      if (iuuconf != UUCONF_NOT_FOUND)
		ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
	      if (! funknown_system (puuconf, zsystem, &sfromsys))
		ulog (LOG_FATAL, "%s: System not found", zsystem);
	    }

	  if (fonxqt)
	    {
	      /* The file is already on the system where the command is to
		 be executed.  */
	      if (finput)
		uxadd_xqt_line ('I', zfile, (const char *) NULL);
	      else
		pzargs[i] = zfile;
	    }
	  else
	    {
	      char *zdata;

	      if (! sfromsys.uuconf_fcall_transfer
		  && ! sfromsys.uuconf_fcalled_transfer)
		ulog (LOG_FATAL,
		      "Not permitted to transfer files to or from %s",
		      sfromsys.uuconf_zname);

	      if (zforw != NULL)
		{
		  /* This is ``uux cmd a!b!file''.  To make this work,
		     we would have to be able to set up a request to a
		     to fetch file from b and send it to us.  But it
		     turns out that that will not work, because when a
		     sends us the file we will put it in a's spool
		     directory, not the local system spool directory.
		     So we won't have any way to find it.  This is not
		     a conceptual problem, and it could doubtless be
		     solved.  Please feel free to solve it and send me
		     the solution.  */
		  ulog (LOG_FATAL, "File forwarding not supported");
		}

	      /* We must request the file from the remote system to
		 this one.  */
	      zdata = zsysdep_data_file_name (&slocalsys, zxqtloc, bgrade,
					      FALSE, abtname, (char *) NULL,
					      (char *) NULL);
	      if (zdata == NULL)
		uxabort ();
	      ubuffree (zdata);

	      /* Request the file.  The special option '9' is a signal
		 to uucico that it's OK to receive a file into the
		 spool directory; normally such requests are rejected.
		 This privilege is easy to abuse.  */
	      s.bcmd = 'R';
	      s.bgrade = bgrade;
	      s.pseq = NULL;
	      s.zfrom = zfile;
	      s.zto = zbufcpy (abtname);
	      s.zuser = zuser;
	      s.zoptions = "9";
	      s.ztemp = "";
	      s.imode = 0600;
	      s.znotify = "";
	      s.cbytes = -1;
	      s.zcmd = NULL;
	      s.ipos = 0;

	      zjobid = zsysdep_spool_commands (&sfromsys, bgrade, 1, &s);
	      if (zjobid == NULL)
		uxabort ();

	      if (fjobid)
		printf ("%s\n", zjobid);

	      ubuffree (zjobid);

	      if (fcall_any)
		{
		  ubuffree (zcall_system);
		  zcall_system = NULL;
		}
	      else
		{
		  fcall_any = TRUE;
		  zcall_system = zbufcpy (sfromsys.uuconf_zname);
		}

	      if (fxqtlocal)
		{
		  /* Tell the command execution to wait until the file
		     has been received, and tell it the real file
		     name.  */
		  if (finput)
		    {
		      uxadd_xqt_line ('F', abtname, (char *) NULL);
		      uxadd_xqt_line ('I', abtname, (char *) NULL);
		    }
		  else
		    {
		      char *zbase;

		      zbase = zsysdep_base_name (zfile);
		      if (zbase == NULL)
			uxabort ();
		      uxadd_xqt_line ('F', abtname, zbase);
		      pzargs[i] = zbase;
		    }
		}
	      else
		{
		  char abxtname[CFILE_NAME_LEN];
		  char *zbase;
		  char *zxqt;
		  FILE *e;

		  /* Now we must arrange to forward the file on to the
		     execution system.  We need to get a name to give
		     the file on the execution system (abxtname).  */
		  zdata = zsysdep_data_file_name (&sxqtsys, zxqtloc,
						  bgrade, TRUE, abxtname,
						  (char *) NULL,
						  (char *) NULL);
		  if (zdata == NULL)
		    uxabort ();
		  ubuffree (zdata);

		  zbase = zsysdep_base_name (zfile);
		  if (zbase == NULL)
		    uxabort ();

		  zxqt = zsysdep_xqt_file_name ();
		  if (zxqt == NULL)
		    uxabort ();
		  e = esysdep_fopen (zxqt, FALSE, FALSE, TRUE);
		  if (e == NULL)
		    uxabort ();
		  uxrecord_file (zxqt);

		  fprintf (e, "U %s %s\n", zsysdep_login_name (),
			   zlocalname);
		  fprintf (e, "F %s %s\n", abtname, zbase);
		  fprintf (e, "C uucp -C -W -d -g %c %s %s!", bgrade,
			   zbase, sxqtsys.uuconf_zname);
		  if (zforward != NULL)
		    fprintf (e, "%s!", zforward);
		  fprintf (e, "%s\n", abxtname);

		  if (fclose (e) != 0)
		    ulog (LOG_FATAL, "fclose: %s", strerror (errno));

		  if (finput)
		    {
		      uxadd_xqt_line ('F', abxtname, (char *) NULL);
		      uxadd_xqt_line ('I', abxtname, (char *) NULL);
		      ubuffree (zbase);
		    }
		  else
		    {
		      uxadd_xqt_line ('F', abxtname, zbase);
		      pzargs[i] = zbase;
		    }
		}
	    }

	  (void) uuconf_system_free (puuconf, &sfromsys);
	}
    }

  /* If standard input is to be read from the stdin of uux, we read it
     here into a temporary file and send it to the execute system.  */
  if (fread_stdin)
    {
      char *zdata;
      char abtname[CFILE_NAME_LEN];
      char abdname[CFILE_NAME_LEN];
      FILE *e;

      zdata = zsysdep_data_file_name (&sxqtsys, zxqtloc, bgrade, FALSE,
				      abtname, abdname, (char *) NULL);
      if (zdata == NULL)
	uxabort ();

      e = esysdep_fopen (zdata, FALSE, FALSE, TRUE);
      if (e == NULL)
	uxabort ();

      eXclose = e;
      uxrecord_file (zdata);

      uxcopy_stdin (e);

      eXclose = NULL;
      if (fclose (e) != 0)
	ulog (LOG_FATAL, "fclose: %s", strerror (errno));

      if (fxqtlocal)
	uxadd_xqt_line ('I', abtname, (const char *) NULL);
      else
	{
	  zinput_from = zbufcpy (abtname);
	  zinput_to = zbufcpy (abdname);
	  zinput_temp = zinput_from;
	  finputcopied = TRUE;
	}
    }

  /* If we are returning standard input, or we're putting the status
     in a file, we can't use an E command.  */
  if (fretstdin)
    uxadd_xqt_line ('B', (const char *) NULL, (const char *) NULL);

  if (zstatus_file != NULL)
    uxadd_xqt_line ('M', zstatus_file, (const char *) NULL);

  /* Get the complete command line, and decide whether the command
     needs to be executed by the shell.  */
  fneedshell = FALSE;

  if (zcmd[strcspn (zcmd, ZSHELLCHARS)] != '\0')
    fneedshell = TRUE;

  clen = strlen (zcmd) + 1;
  for (i = 0; i < cargs; i++)
    {
      if (pzargs[i] != NULL)
	{
	  clen += strlen (pzargs[i]) + 1;
	  if (pzargs[i][strcspn (pzargs[i], ZSHELLCHARS)] != '\0')
	    fneedshell = TRUE;
	}
    }

  zfullcmd = zbufalc (clen);

  strcpy (zfullcmd, zcmd);
  for (i = 0; i < cargs; i++)
    {
      if (pzargs[i] != NULL)
	{
	  strcat (zfullcmd, " ");
	  strcat (zfullcmd, pzargs[i]);
	}
    }

  /* If we haven't written anything to the execution file yet, and we
     have a standard input file, and we're not forwarding, then every
     other option can be handled in an E command.  */
  if (eXxqt_file == NULL && zinput_from != NULL && zforward == NULL)
    {
      struct scmd s;
      char *zoptions;

      /* Set up an E command.  */
      s.bcmd = 'E';
      s.bgrade = bgrade;
      s.pseq = NULL;
      s.zuser = zuser;
      s.zfrom = zinput_from;
      s.zto = zinput_to;
      s.zoptions = aboptions;
      zoptions = aboptions;
      *zoptions++ = finputcopied ? 'C' : 'c';
      if (fno_ack)
	*zoptions++ = 'N';
      if (ferror_ack)
	*zoptions++ = 'Z';
      if (zrequestor != NULL)
	*zoptions++ = 'R';
      if (fneedshell)
	*zoptions++ = 'e';
      *zoptions = '\0';
      s.ztemp = zinput_temp;
      s.imode = 0666;
      if (zrequestor == NULL)
	zrequestor = "\"\"";
      s.znotify = zrequestor;
      s.cbytes = -1;
      s.zcmd = zfullcmd;
      s.ipos = 0;
      
      ++cXcmds;
      pasXcmds = (struct scmd *) xrealloc ((pointer) pasXcmds,
					   cXcmds * sizeof (struct scmd));
      pasXcmds[cXcmds - 1] = s;

      uxadd_name (zinput_from);
    }
  else
    {
      /* Finish up the execute file.  */
      uxadd_xqt_line ('U', zuser, zxqtloc);
      if (zinput_from != NULL)
	{
	  uxadd_xqt_line ('F', zinput_to, (char *) NULL);
	  uxadd_xqt_line ('I', zinput_to, (char *) NULL);
	  uxadd_send_file (zinput_from, zinput_to,
			   finputcopied ? "C" : "c",
			   zinput_temp, zforward, &sxqtsys, zxqtloc,
			   bgrade);
	}
      if (fno_ack)
	uxadd_xqt_line ('N', (const char *) NULL, (const char *) NULL);
      if (ferror_ack)
	uxadd_xqt_line ('Z', (const char *) NULL, (const char *) NULL);
      if (zrequestor != NULL)
	uxadd_xqt_line ('R', zrequestor, (const char *) NULL);
      if (fneedshell)
	uxadd_xqt_line ('e', (const char *) NULL, (const char *) NULL);
      uxadd_xqt_line ('C', zfullcmd, (const char *) NULL);
      if (fclose (eXxqt_file) != 0)
	ulog (LOG_FATAL, "fclose: %s", strerror (errno));
      eXxqt_file = NULL;

      /* If the execution is to occur on another system, we must now
	 arrange to copy the execute file to this system.  */
      if (! fxqtlocal)
	uxadd_send_file (abxqt_tname, abxqt_xname, "C", abxqt_tname,
			 zforward, &sxqtsys, zxqtloc, bgrade);
    }

  /* If we got a signal, get out before spooling anything.  */
  if (FGOT_SIGNAL ())
    uxabort ();

  /* From here on in, it's too late.  We don't call uxabort.  */
  if (cXcmds > 0)
    {
      char *zjobid;

      if (! sxqtsys.uuconf_fcall_transfer
	  && ! sxqtsys.uuconf_fcalled_transfer)
	ulog (LOG_FATAL, "Not permitted to transfer files to or from %s",
	      sxqtsys.uuconf_zname);

      zjobid = zsysdep_spool_commands (&sxqtsys, bgrade, cXcmds, pasXcmds);
      if (zjobid == NULL)
	{
	  ulog_close ();
	  usysdep_exit (FALSE);
	}

      if (fjobid)
	printf ("%s\n", zjobid);

      ubuffree (zjobid);

      if (fcall_any)
	{
	  ubuffree (zcall_system);
	  zcall_system = NULL;
	}
      else
	{
	  fcall_any = TRUE;
	  zcall_system = zbufcpy (sxqtsys.uuconf_zname);
	}
    }

  /* If all that worked, make a log file entry.  All log file reports
     up to this point went to stderr.  */
  ulog_to_file (puuconf, TRUE);
  ulog_system (sxqtsys.uuconf_zname);
  ulog_user (zuser);

  if (zXnames == NULL)
    ulog (LOG_NORMAL, "Queuing %s", zfullcmd);
  else
    ulog (LOG_NORMAL, "Queuing %s (%s)", zfullcmd, zXnames);

  ulog_close ();

  if (! fuucico
      || (zcall_system == NULL && ! fcall_any))
    fexit = TRUE;
  else
    {
      const char *zcicoarg;
      char *zconfigarg;

      if (zcall_system == NULL)
	zcicoarg = "-r1";
      else
	{
	  char *z;

	  z = zbufalc (sizeof "-Cs" + strlen (zcall_system));
	  sprintf (z, "-Cs%s", zcall_system);
	  zcicoarg = z;
	}

      if (zconfig == NULL)
	zconfigarg = NULL;
      else
	{
	  zconfigarg = zbufalc (sizeof "-I" + strlen (zconfig));
	  sprintf (zconfigarg, "-I%s", zconfig);
	}

      fexit = fsysdep_run (FALSE, "uucico", zcicoarg, zconfigarg);
    }

  usysdep_exit (fexit);

  /* Avoid error about not returning a value.  */
  return 0;
}

/* Report command usage.  */

static void
uxhelp ()
{
  printf ("Taylor UUCP %s, copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor\n",
	  VERSION);
  printf ("Usage: %s [options] [-] command\n", zProgram);
  printf (" -,-p,--stdin: Read standard input for standard input of command\n");
  printf (" -c,--nocopy: Do not copy local files to spool directory (default)\n");
  printf (" -C,--copy: Copy local files to spool directory\n");
  printf (" -l,--link: link local files to spool directory\n");
  printf (" -g,--grade grade: Set job grade (must be alphabetic)\n");
  printf (" -n,--notification=no: Do not report completion status\n");
  printf (" -z,--notification=error: Report completion status only on error\n");
  printf (" -r,--nouucico: Do not start uucico daemon\n");
  printf (" -a,--requestor address: Address to mail status report to\n");
  printf (" -b,--return-stdin: Return standard input with status report\n");
  printf (" -s,--status file: Report completion status to file\n");
  printf (" -j,--jobid: Report job id\n");
  printf (" -x,--debug debug: Set debugging level\n");
#if HAVE_TAYLOR_CONFIG
  printf (" -I,--config file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  printf (" -v,--version: Print version and exit\n");
  printf (" --help: Print help and exit\n");
}

static void
uxusage ()
{
  fprintf (stderr,
	   "Usage: %s [options] [-] command\n", zProgram);
  fprintf (stderr, "Use %s --help for help\n", zProgram);
  exit (EXIT_FAILURE);
}

/* Add a line to the execute file.  */

static void
uxadd_xqt_line (bchar, z1, z2)
     int bchar;
     const char *z1;
     const char *z2;
{
  if (eXxqt_file == NULL)
    {
      eXxqt_file = esysdep_fopen (zXxqt_name, FALSE, FALSE, TRUE);
      if (eXxqt_file == NULL)
	uxabort ();
    }

  if (z1 == NULL)
    fprintf (eXxqt_file, "%c\n", bchar);
  else if (z2 == NULL)
    fprintf (eXxqt_file, "%c %s\n", bchar, z1);
  else
    fprintf (eXxqt_file, "%c %s %s\n", bchar, z1, z2);
}

/* Add a file to be sent to the execute system.  */

static void
uxadd_send_file (zfrom, zto, zoptions, ztemp, zforward, qxqtsys, zxqtloc,
		 bgrade)
     const char *zfrom;
     const char *zto;
     const char *zoptions;
     const char *ztemp;
     const char *zforward;
     const struct uuconf_system *qxqtsys;
     const char *zxqtloc;
     int bgrade;
{
  struct scmd s;

  if (zforward != NULL)
    {
      char *zbase;
      char *zxqt;
      char abtname[CFILE_NAME_LEN];
      char abdname[CFILE_NAME_LEN];
      char abxname[CFILE_NAME_LEN];
      FILE *e;

      /* We want to forward this file through the first execution
	 system to other systems.  We set up a remote execution of
	 uucp to forward the file.  */
      zbase = zsysdep_base_name (zfrom);
      if (zbase == NULL)
	uxabort ();

      zxqt = zsysdep_data_file_name (qxqtsys, zxqtloc, bgrade, TRUE, abtname,
				     abdname, abxname);
      if (zxqt == NULL)
	uxabort ();
      e = esysdep_fopen (zxqt, FALSE, FALSE, TRUE);
      if (e == NULL)
	uxabort ();
      uxrecord_file (zxqt);

      fprintf (e, "U %s %s\n", zsysdep_login_name (), zxqtloc);
      fprintf (e, "F %s %s\n", abdname, zbase);
      fprintf (e, "C uucp -C -W -d -g %c %s %s!%s\n",
	       bgrade, zbase, zforward, zto);

      ubuffree (zbase);

      if (fclose (e) != 0)
	ulog (LOG_FATAL, "fclose: %s", strerror (errno));

      /* Send the execution file.  */
      s.bcmd = 'S';
      s.bgrade = bgrade;
      s.pseq = NULL;
      s.zfrom = zbufcpy (abtname);
      s.zto = zbufcpy (abxname);
      s.zuser = zsysdep_login_name ();
      s.zoptions = "C";
      s.ztemp = s.zfrom;
      s.imode = 0666;
      s.znotify = NULL;
      s.cbytes = -1;
      s.zcmd = NULL;
      s.ipos = 0;

      ++cXcmds;
      pasXcmds = (struct scmd *) xrealloc ((pointer) pasXcmds,
					   cXcmds * sizeof (struct scmd));
      pasXcmds[cXcmds - 1] = s;

      uxadd_name (abtname);

      /* Send the data file to abdname where the execution file will
	 expect it.  */
      zto = abdname;
    }

  s.bcmd = 'S';
  s.bgrade = bgrade;
  s.pseq = NULL;
  s.zfrom = zbufcpy (zfrom);
  s.zto = zbufcpy (zto);
  s.zuser = zsysdep_login_name ();
  s.zoptions = zbufcpy (zoptions);
  s.ztemp = zbufcpy (ztemp);
  s.imode = 0666;
  s.znotify = "";
  s.cbytes = -1;
  s.zcmd = NULL;
  s.ipos = 0;

  ++cXcmds;
  pasXcmds = (struct scmd *) xrealloc ((pointer) pasXcmds,
				       cXcmds * sizeof (struct scmd));
  pasXcmds[cXcmds - 1] = s;

  uxadd_name (zfrom);
}

/* Copy stdin to a file.  This is a separate function because it may
   call setjmp.  */

static void
uxcopy_stdin (e)
     FILE *e;
{
  CATCH_PROTECT size_t cread;
  char ab[1024];

  do
    {
      size_t cwrite;

      /* I want to use fread here, but there is a bug in some versions
	 of SVR4 which causes fread to return less than a complete
	 buffer even if EOF has not been reached.  This is not online
	 time, so speed is not critical, but it's still quite annoying
	 to have to use an inefficient algorithm.  */
      cread = 0;
      if (fsysdep_catch ())
	{
	  usysdep_start_catch ();

	  while (cread < sizeof (ab))
	    {
	      int b;

	      if (FGOT_SIGNAL ())
		uxabort ();

	      /* There's an unimportant race here.  If the user hits
		 ^C between the FGOT_SIGNAL we just did and the time
		 we enter getchar, we won't know about the signal
		 (unless we're doing a longjmp, but we normally
		 aren't).  It's not a big problem, because the user
		 can just hit ^C again.  */
	      b = getchar ();
	      if (b == EOF)
		break;
	      ab[cread] = b;
	      ++cread;
	    }
	}

      usysdep_end_catch ();

      if (FGOT_SIGNAL ())
	uxabort ();

      if (cread > 0)
	{
	  cwrite = fwrite (ab, sizeof (char), cread, e);
	  if (cwrite != cread)
	    ulog (LOG_FATAL, "fwrite: Wrote %d when attempted %d",
		  (int) cwrite, (int) cread);
	}
    }
  while (cread == sizeof ab);
}

/* Keep track of all files we have created so that we can delete them
   if we get a signal.  The argument will be on the heap.  */

static int cXfiles;
static const char **pXaz;

static void
uxrecord_file (zfile)
     const char *zfile;
{
  pXaz = (const char **) xrealloc ((pointer) pXaz,
				   (cXfiles + 1) * sizeof (const char *));
  pXaz[cXfiles] = zfile;
  ++cXfiles;
}

/* Delete all the files we have recorded and exit.  */

static void
uxabort ()
{
  int i;

  if (eXxqt_file != NULL)
    (void) fclose (eXxqt_file);
  if (eXclose != NULL)
    (void) fclose (eXclose);
  for (i = 0; i < cXfiles; i++)
    (void) remove (pXaz[i]);
  ulog_close ();
  usysdep_exit (FALSE);
}

/* Add a name to the list of file names we are going to log.  We log
   all the file names which will appear in the uucico log file.  This
   permits people to associate the file send in the uucico log with
   the uux entry which created the file.  Normally only one file name
   will appear.  */

static void
uxadd_name (z)
     const char *z;
{
  if (zXnames == NULL)
    zXnames = zbufcpy (z);
  else
    {
      size_t cold, cadd;
      char *znew;

      cold = strlen (zXnames);
      cadd = strlen (z);
      znew = zbufalc (cold + 2 + cadd);
      memcpy (znew, zXnames, cold);
      znew[cold] = ' ';
      memcpy (znew + cold + 1, z, cadd + 1);
      ubuffree (zXnames);
      zXnames = znew;
    }
}
