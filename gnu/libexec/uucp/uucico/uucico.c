/* uucico.c
   This is the main UUCP communication program.

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
const char uucico_rcsid[] = "$FreeBSD$";
#endif

#include <ctype.h>

#if HAVE_LIMITS_H
#include <limits.h>
#else
#define LONG_MAX 2147483647L
#endif

#include "getopt.h"

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "prot.h"
#include "trans.h"
#include "system.h"

#if HAVE_ENCRYPTED_PASSWORDS
#ifndef crypt
extern char *crypt ();
#endif
#endif

/* Coherent already had a different meaning for the -c option.  What a
   pain.  */
#ifdef __COHERENT__
#define COHERENT_C_OPTION 1
#else
#define COHERENT_C_OPTION 0
#endif

/* Define the known protocols.  */

#define TCP_PROTO \
  (UUCONF_RELIABLE_ENDTOEND \
   | UUCONF_RELIABLE_RELIABLE \
   | UUCONF_RELIABLE_EIGHT)

static const struct sprotocol asProtocols[] =
{
  { 't', TCP_PROTO, 1, TRUE,
      asTproto_params, ftstart, ftshutdown, ftsendcmd, ztgetspace,
      ftsenddata, ftwait, ftfile },
  { 'e', TCP_PROTO, 1, TRUE,
      asEproto_params, festart, feshutdown, fesendcmd, zegetspace,
      fesenddata, fewait, fefile },
  { 'i', UUCONF_RELIABLE_EIGHT, 7, TRUE,
      asIproto_params, fistart, fishutdown, fisendcmd, zigetspace,
      fisenddata, fiwait, NULL },
  { 'a', UUCONF_RELIABLE_EIGHT, 1, TRUE,
      asZproto_params, fzstart, fzshutdown, fzsendcmd, zzgetspace,
      fzsenddata, fzwait, fzfile },
  { 'g', UUCONF_RELIABLE_EIGHT, 1, TRUE,
      asGproto_params, fgstart, fgshutdown, fgsendcmd, zggetspace,
      fgsenddata, fgwait, NULL },
  { 'G', UUCONF_RELIABLE_EIGHT, 1, TRUE,
      asGproto_params, fbiggstart, fgshutdown, fgsendcmd, zggetspace,
      fgsenddata, fgwait, NULL },
  { 'j', UUCONF_RELIABLE_EIGHT, 7, TRUE,
      asIproto_params, fjstart, fjshutdown, fisendcmd, zigetspace,
      fisenddata, fiwait, NULL },
  { 'f', UUCONF_RELIABLE_RELIABLE, 1, FALSE,
      asFproto_params, ffstart, ffshutdown, ffsendcmd, zfgetspace,
      ffsenddata, ffwait, fffile },
  { 'v', UUCONF_RELIABLE_EIGHT, 1, TRUE,
      asGproto_params, fvstart, fgshutdown, fgsendcmd, zggetspace,
      fgsenddata, fgwait, NULL },
  { 'y', UUCONF_RELIABLE_RELIABLE | UUCONF_RELIABLE_EIGHT, 1, TRUE,
      asYproto_params, fystart, fyshutdown, fysendcmd, zygetspace,
      fysenddata, fywait, fyfile }
};

#define CPROTOCOLS (sizeof asProtocols / sizeof asProtocols[0])

/* Locked system.  */
static boolean fLocked_system;
static struct uuconf_system sLocked_system;

/* Daemon structure holding information about the remote system (must
   be global so the error handler can see it.  */
static struct sdaemon sDaemon;

/* Open connection.  */
static struct sconnection *qConn;

/* uuconf global pointer; need to close the connection after a fatal
   error.  */
static pointer pUuconf;

/* This structure is passed to iuport_lock via uuconf_find_port.  */
struct spass
{
  boolean fmatched;
  boolean flocked;
  struct sconnection *qconn;
};

/* Local functions.  */

static void uusage P((void));
static void uhelp P((void));
static void uabort P((void));
static boolean fcall P((pointer puuconf, const char *zconfig, boolean fuuxqt,
			const struct uuconf_system *qsys,
			struct uuconf_port *qport, boolean fifwork,
			boolean fforce, boolean fdetach,
			boolean fquiet, boolean ftrynext));
static boolean fconn_call P((struct sdaemon *qdaemon,
			     struct uuconf_port *qport,
			     struct sstatus *qstat, int cretry,
			     boolean *pfcalled));
static boolean fdo_call P((struct sdaemon *qdaemon,
			   struct sstatus *qstat,
			   const struct uuconf_dialer *qdialer,
			   boolean *pfcalled, enum tstatus_type *pterr));
static int iuport_lock P((struct uuconf_port *qport, pointer pinfo));
static boolean flogin_prompt P((pointer puuconf, const char *zconfig,
				boolean fuuxqt, struct sconnection *qconn,
				const char *zlogin, const char **pzsystem));
static int icallin_cmp P((int iwhich, pointer pinfo, const char *zfile));
static boolean faccept_call P((pointer puuconf, const char *zconfig,
			       boolean fuuxqt, const char *zlogin,
			       struct sconnection *qconn,
			       const char **pzsystem));
static void uaccept_call_cleanup P((pointer puuconf,
				    struct uuconf_system *qfreesys,
				    struct uuconf_port *qport,
				    struct uuconf_port *qfreeport,
				    char *zloc));
static void uapply_proto_params P((pointer puuconf, int bproto,
				   struct uuconf_cmdtab *qcmds,
				   struct uuconf_proto_param *pas));
static boolean fsend_uucp_cmd P((struct sconnection *qconn,
				 const char *z));
static char *zget_uucp_cmd P((struct sconnection *qconn,
			      boolean frequired, boolean fstrip));
static char *zget_typed_line P((struct sconnection *qconn,
				boolean fstrip));

/* Long getopt options.  */
static const struct option asLongopts[] =
{
  { "quiet", no_argument, NULL, 2 },
  { "ifwork", no_argument, NULL, 'C' },
  { "nodetach", no_argument, NULL, 'D' },
  { "loop", no_argument, NULL, 'e' },
  { "force", no_argument, NULL, 'f'},
  { "stdin", required_argument, NULL, 'i' },
  { "prompt", no_argument, NULL, 'l' },
  { "port", required_argument, NULL, 'p' },
  { "nouuxqt", no_argument, NULL, 'q' },
  { "master", no_argument, NULL, 3 },
  { "slave", no_argument, NULL, 4 },
  { "system", required_argument, NULL, 's' },
  { "login", required_argument, NULL, 'u' },
  { "wait", no_argument, NULL, 'w' },
  { "try-next", no_argument, NULL, 'z' },
  { "config", required_argument, NULL, 'I' },
  { "debug", required_argument, NULL, 'x' },
  { "version", no_argument, NULL, 'v' },
  { "help", no_argument, NULL, 1 },
  { NULL, 0, NULL, 0 }
};

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* -c: Whether to be quiet.  */
  boolean fquiet = FALSE;
  /* -C: Only call the system if there is work.  */
  boolean fifwork = FALSE;
  /* -D: don't detach from controlling terminal.  */
  boolean fdetach = TRUE;
  /* -e: Whether to do an endless loop of accepting calls.  */
  boolean fendless = FALSE;
  /* -f: Whether to force a call despite status of previous call.  */
  boolean fforce = FALSE;
  /* -i type: type of port to use for stdin.  */
  enum uuconf_porttype tstdintype = UUCONF_PORTTYPE_STDIN;
  /* -I file: configuration file name.  */
  const char *zconfig = NULL;
  /* -l: Whether to give a single login prompt.  */
  boolean flogin = FALSE;
  /* -P port: port to use; in master mode, call out on this port.  In
     slave mode, accept logins on this port.  If port not specified,
     then in master mode figure it out for each system, and in slave
     mode use stdin and stdout.  */
  const char *zport = NULL;
  /* -q: Whether to start uuxqt when done.  */
  boolean fuuxqt = TRUE;
  /* -r1: Whether we are the master.  */
  boolean fmaster = FALSE;
  /* -s,-S system: system to call.  */
  const char *zsystem = NULL;
  /* -u: Login name to use.  */
  const char *zlogin = NULL;
  /* -w: Whether to wait for a call after doing one.  */
  boolean fwait = FALSE;
  /* -z: Try next alternate if call fails.  */
  boolean ftrynext = FALSE;
  const char *zopts;
  int iopt;
  struct uuconf_port *qport;
  struct uuconf_port sport;
  boolean fret = TRUE;
  pointer puuconf;
  int iuuconf;
#if DEBUG > 1
  int iholddebug;
#endif

  zProgram = argv[0];

  /* When uucico is invoked by login, the first character of the
     program will be a dash.  We don't want that.  */
  if (*zProgram == '-')
    ++zProgram;

#if COHERENT_C_OPTION
  zopts = "c:CDefi:I:lp:qr:s:S:u:x:X:vwz";
#else
  zopts = "cCDefi:I:lp:qr:s:S:u:x:X:vwz";
#endif

  while ((iopt = getopt_long (argc, argv, zopts,
			      asLongopts, (int *) NULL)) != EOF)
    {
#if COHERENT_C_OPTION
      if (iopt == 'c')
	{
	  iopt = 's';
	  fifwork = TRUE;
	}
#endif
      switch (iopt)
	{
	case 2:
	case 'c':
	  /* Don't warn if a call is attempted at a bad time, and
	     don't print the "No work" message.  */
	  fquiet = TRUE;
	  break;

	case 'C':
	  fifwork = TRUE;
	  break;

	case 'D':
	  /* Don't detach from controlling terminal.  */
	  fdetach = FALSE;
	  break;

	case 'e':
	  /* Do an endless loop of accepting calls.  */
	  fendless = TRUE;
	  break;

	case 'f':
	  /* Force a call even if it hasn't been long enough since the last
	     failed call.  */
	  fforce = TRUE;
	  break;

	case 'i':
	  /* Type of port to use for standard input.  Only TLI is
	     supported here, and only if HAVE_TLI is true.  This
	     permits the Network Listener to tell uucico to use TLI
	     I/O calls.  */
	  if (strcasecmp (optarg, "tli") != 0)
	    fprintf (stderr, "%s: unsupported port type \"%s\"\n",
		     zProgram, optarg);
	  else
	    {
#if HAVE_TLI
	      tstdintype = UUCONF_PORTTYPE_TLI;
#else
	      fprintf (stderr, "%s: not compiled with TLI support\n",
		       zProgram);
#endif
	    }
	  break;

	case 'l':
	  /* Prompt for login name and password.  */
	  flogin = TRUE;
	  break;

	case 'p':
	  /* Port to use  */
	  zport = optarg;
	  break;

	case 'q':
	  /* Don't start uuxqt.  */
	  fuuxqt = FALSE;
	  break;

	case 'r':
	  /* Set mode: -r1 for master, -r0 for slave (default)  */
	  if (strcmp (optarg, "1") == 0)
	    fmaster = TRUE;
	  else if (strcmp (optarg, "0") == 0)
	    fmaster = FALSE;
	  else
	    uusage ();
	  break;
    
	case 's':
	  /* Set system name  */
	  zsystem = optarg;
	  fmaster = TRUE;
	  break;

	case 'S':
	  /* Set system name and force call like -f  */
	  zsystem = optarg;
	  fforce = TRUE;
	  fmaster = TRUE;
	  break;

	case 'u':
	  /* Some versions of uucpd invoke uucico with a -u argument
	     specifying the login name.  If invoked by a privileged
	     user, we use it instead of the result of
	     zsysdep_login_name.  */
	  if (fsysdep_privileged ())
	    zlogin = optarg;
	  else
	    fprintf (stderr,
		     "%s: ignoring command line login name: not a privileged user\n",
		     zProgram);
	  break;

	case 'w':
	  /* Call out and then wait for a call in  */
	  fwait = TRUE;
	  break;

	case 'z':
	  /* Try next alternate if call fails.  */
	  ftrynext = TRUE;
	  break;

	case 'I':
	  /* Set configuration file name (default is in sysdep.h).  */
	  if (fsysdep_other_config (optarg))
	    zconfig = optarg;
	  break;

	case 'x':
	case 'X':
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

	case 4:
	  /* --slave.  */
	  fmaster = FALSE;
	  break;

	case 3:
	  /* --master.  */
	  fmaster = TRUE;
	  break;

	case 1:
	  /* --help.  */
	  uhelp ();
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 0:
	  /* Long option found, and flag value set.  */
	  break;

	default:
	  uusage ();
	  /*NOTREACHED*/
	}
    }

  if (optind != argc)
    uusage ();

  if (fwait && zport == NULL)
    {
      fprintf (stderr, "%s: -w requires -p", zProgram);
      uusage ();
    }

  iuuconf = uuconf_init (&puuconf, (const char *) NULL, zconfig);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
  pUuconf = puuconf;

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

  /* If a port was named, get its information.  */
  if (zport == NULL)
    qport = NULL;
  else
    {
      iuuconf = uuconf_find_port (puuconf, zport, (long) 0, (long) 0,
				  (int (*) P((struct uuconf_port *,
					      pointer))) NULL,
				  (pointer) NULL, &sport);
      if (iuuconf == UUCONF_NOT_FOUND)
	ulog (LOG_FATAL, "%s: port not found", zport);
      else if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
      qport = &sport;
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

  usysdep_initialize (puuconf, INIT_SUID);

  ulog_to_file (puuconf, TRUE);
  ulog_fatal_fn (uabort);

  if (fmaster)
    {
      if (zsystem != NULL)
	{
	  /* A system was named.  Call it.  */
	  iuuconf = uuconf_system_info (puuconf, zsystem,
					&sLocked_system);
	  if (iuuconf == UUCONF_NOT_FOUND)
	    ulog (LOG_FATAL, "%s: System not found", zsystem);
	  else if (iuuconf != UUCONF_SUCCESS)
	    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

	  /* Detach from the controlling terminal for the call.  This
	     probably makes sense only on Unix.  We want the modem
	     line to become the controlling terminal.  */
	  if (fdetach &&
	      (qport == NULL
	       || qport->uuconf_ttype != UUCONF_PORTTYPE_STDIN))
	    usysdep_detach ();

	  ulog_system (sLocked_system.uuconf_zname);

#if DEBUG > 1
	  iholddebug = iDebug;
	  if (sLocked_system.uuconf_zdebug != NULL)
	    iDebug |= idebug_parse (sLocked_system.uuconf_zdebug);
#endif

	  if (! fsysdep_lock_system (&sLocked_system))
	    {
	      ulog (LOG_ERROR, "System already locked");
	      fret = FALSE;
	    }
	  else
	    {
	      fLocked_system = TRUE;
	      fret = fcall (puuconf, zconfig, fuuxqt, &sLocked_system, qport,
			    fifwork, fforce, fdetach, fquiet, ftrynext);
	      if (fLocked_system)
		{
		  (void) fsysdep_unlock_system (&sLocked_system);
		  fLocked_system = FALSE;
		}
	    }
#if DEBUG > 1
	  iDebug = iholddebug;
#endif
	  ulog_system ((const char *) NULL);
	  (void) uuconf_system_free (puuconf, &sLocked_system);
	}
      else
	{
	  char **pznames, **pz;
	  int c, i;
	  boolean fdidone;

	  /* Call all systems which have work to do.  */
	  fret = TRUE;
	  fdidone = FALSE;

	  iuuconf = uuconf_system_names (puuconf, &pznames, 0);
	  if (iuuconf != UUCONF_SUCCESS)
	    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

	  /* Randomize the order in which we call the systems.  */
	  c = 0;
	  for (pz = pznames; *pz != NULL; pz++)
	    c++;

	  srand ((unsigned int) ixsysdep_time ((long *) NULL));
	  for (i = c - 1; i > 0; i--)
	    {
	      int iuse;
	      char *zhold;

	      iuse = rand () % (i + 1);
	      zhold = pznames[i];
	      pznames[i] = pznames[iuse];
	      pznames[iuse] = zhold;
	    }

	  for (pz = pznames; *pz != NULL && ! FGOT_SIGNAL (); pz++)
	    {
	      iuuconf = uuconf_system_info (puuconf, *pz,
					    &sLocked_system);
	      if (iuuconf != UUCONF_SUCCESS)
		{
		  ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
		  xfree ((pointer) *pz);
		  continue;
		}

	      if (fsysdep_has_work (&sLocked_system))
		{
		  fdidone = TRUE;

		  /* Detach from the controlling terminal.  On Unix
		     this means that we will wind up forking a new
		     process for each system we call.  */
		  if (fdetach
		      && (qport == NULL
			  || qport->uuconf_ttype != UUCONF_PORTTYPE_STDIN))
		    usysdep_detach ();

		  ulog_system (sLocked_system.uuconf_zname);

#if DEBUG > 1
		  iholddebug = iDebug;
		  if (sLocked_system.uuconf_zdebug != NULL)
		    iDebug |= idebug_parse (sLocked_system.uuconf_zdebug);
#endif

		  if (! fsysdep_lock_system (&sLocked_system))
		    {
		      ulog (LOG_ERROR, "System already locked");
		      fret = FALSE;
		    }
		  else
		    {
		      fLocked_system = TRUE;
		      if (! fcall (puuconf, zconfig, fuuxqt, &sLocked_system,
				   qport, TRUE, fforce, fdetach, fquiet,
				   ftrynext))
			fret = FALSE;

		      /* Now ignore any SIGHUP that we got.  */
		      afSignal[INDEXSIG_SIGHUP] = FALSE;

		      if (fLocked_system)
			{
			  (void) fsysdep_unlock_system (&sLocked_system);
			  fLocked_system = FALSE;
			}
		    }
#if DEBUG > 1
		  iDebug = iholddebug;
#endif
		  ulog_system ((const char *) NULL);
		}

	      (void) uuconf_system_free (puuconf, &sLocked_system);
	      xfree ((pointer) *pz);
	    }

	  xfree ((pointer) pznames);

	  if (! fdidone && ! fquiet)
	    ulog (LOG_NORMAL, "No work");
	}

      /* If requested, wait for calls after dialing out.  */
      if (fwait)
	{
	  fendless = TRUE;
	  fmaster = FALSE;
	}
    }

  if (! fmaster)
    {
      struct sconnection sconn;
      boolean flocked;

      /* If a port was specified by name, we go into endless loop
	 mode.  In this mode, we wait for calls and prompt them with
	 "login:" and "Password:", so that they think we are a regular
	 UNIX system.  If we aren't in endless loop mode, we have been
	 called by some other system.  If flogin is TRUE, we prompt
	 with "login:" and "Password:" a single time.  */

      fret = TRUE;
      zsystem = NULL;

      if (! fconn_init (qport, &sconn, tstdintype))
	fret = FALSE;

      if (qport != NULL)
	{
	  /* We are not using standard input.  Detach from the
	     controlling terminal, so that the port we are about to
	     use becomes our controlling terminal.  */
	  if (fdetach
	      && qport->uuconf_ttype != UUCONF_PORTTYPE_STDIN)
	    usysdep_detach ();
	}

      if (fconn_lock (&sconn, TRUE))
	flocked = TRUE;
      else
	{
	  flocked = FALSE;
	  ulog (LOG_ERROR, "%s: Port already locked",
		qport->uuconf_zname);
	  fret = FALSE;
	}

      if (fret)
	{
	  if (! fconn_open (&sconn, (long) 0, (long) 0, TRUE))
	    fret = FALSE;
	  qConn = &sconn;
	}

      if (fret)
	{
	  if (fendless)
	    {
	      while (! FGOT_SIGNAL ()
		     && flogin_prompt (puuconf, zconfig, fuuxqt, &sconn,
				       (const char *) NULL,
				       (const char **) NULL))
		{
		  /* Close and reopen the port in between calls.  */
		  if (! fconn_close (&sconn, puuconf,
				     (struct uuconf_dialer *) NULL,
				     TRUE)
		      || ! fconn_open (&sconn, (long) 0, (long) 0, TRUE))
		    break;
		}
	      fret = FALSE;
	    }
	  else
	    {
	      if (flogin)
		fret = flogin_prompt (puuconf, zconfig, fuuxqt, &sconn,
				      zlogin, &zsystem);
	      else
		{
#if DEBUG > 1
		  iholddebug = iDebug;
#endif
		  if (zlogin == NULL)
		    zlogin = zsysdep_login_name ();
		  fret = faccept_call (puuconf, zconfig, fuuxqt, zlogin,
				       &sconn, &zsystem);
#if DEBUG > 1
		  iDebug = iholddebug;
#endif
		}
	    }
	}

      if (qConn != NULL)
	{
	  if (! fconn_close (&sconn, puuconf, (struct uuconf_dialer *) NULL,
			     fret))
	    fret = FALSE;
	  qConn = NULL;
	}

      if (flocked)
	(void) fconn_unlock (&sconn);

      uconn_free (&sconn);
    }

  ulog_close ();
  ustats_close ();

  /* If we got a SIGTERM, perhaps because the system is going down,
     don't run uuxqt.  We go ahead and run it for any other signal,
     since I think they indicate more temporary conditions.  */
  if (afSignal[INDEXSIG_SIGTERM])
    fuuxqt = FALSE;

  if (fuuxqt)
    {
      int irunuuxqt;

      iuuconf = uuconf_runuuxqt (puuconf, &irunuuxqt);
      if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      else if (irunuuxqt == UUCONF_RUNUUXQT_ONCE)
	{
	  /* Detach from the controlling terminal before starting up uuxqt,
	     so that it runs as a true daemon.  */
	  if (fdetach)
	    usysdep_detach ();

	  if (! fspawn_uuxqt (FALSE, zsystem, zconfig))
	    fret = FALSE;
	}
    }

  usysdep_exit (fret);

  /* Avoid complaints about not returning.  */
  return 0;
}

/* Print out a usage message and die.  */

static void
uusage ()
{
  fprintf (stderr, "Usage: %s [options]\n", zProgram);
  fprintf (stderr, "Use %s --help for help\n", zProgram);
  exit (EXIT_FAILURE);
}

/* Print a help message.  */

static void
uhelp ()
{
  printf ("Taylor UUCP %s, copyright (C) 1991, 92, 93, 94, 1995 Ian Lance Taylor\n",
	   VERSION);
  printf ("Usage: %s [options]\n", zProgram);
  printf (" -s,-S,--system system: Call system (-S implies -f)\n");
  printf (" -f,--force: Force call despite system status\n");
  printf (" -r state: 1 for master, 0 for slave (default)\n");
  printf (" --master: Act as master\n");
  printf (" --slave: Act as slave (default)\n");
  printf (" -p,--port port: Specify port\n");
  printf (" -l,--prompt: prompt for login name and password\n");
  printf (" -e,--loop: Endless loop of login prompts and daemon execution\n");
  printf (" -w,--wait: After calling out, wait for incoming calls\n");
  printf (" -q,--nouuxqt: Don't start uuxqt when done\n");
  printf (" -c,--quiet: Don't log bad time or no work warnings\n");
  printf (" -C,--ifwork: Only call named system if there is work\n");
  printf (" -D,--nodetach: Don't detach from controlling terminal\n");
  printf (" -u,--login: Set login name (privileged users only)\n");
  printf (" -i,--stdin type: Type of standard input (only TLI supported)\n");
  printf (" -z,--try-next: If a call fails, try the next alternate\n");
  printf (" -x,-X,--debug debug: Set debugging level\n");
#if HAVE_TAYLOR_CONFIG
  printf (" -I,--config file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  printf (" -v,--version: Print version and exit\n");
  printf (" --help: Print help and exit\n");
}

/* This function is called when a LOG_FATAL error occurs.  */

static void
uabort ()
{
  if (fLocked_system)
    ufailed (&sDaemon);

  ulog_user ((const char *) NULL);

  if (qConn != NULL)
    {
      (void) fconn_close (qConn, pUuconf, (struct uuconf_dialer *) NULL,
			  FALSE);
      (void) fconn_unlock (qConn);
      uconn_free (qConn);
    }

  if (fLocked_system)
    {
      (void) fsysdep_unlock_system (&sLocked_system);
      fLocked_system = FALSE;
    }

  ulog_system ((const char *) NULL);

  ulog_close ();
  ustats_close ();

  usysdep_exit (FALSE);
}

/* The number of seconds in one day.  We must cast to long for this
   to be calculated correctly on a machine with 16 bit ints.  */
#define SECS_PER_DAY ((long) 24 * (long) 60 * (long) 60)

/* Call another system, trying all the possible sets of calling
   instructions.  The qsys argument is the system to call.  The qport
   argument is the port to use, and may be NULL.  If the fifwork
   argument is TRUE, the call is only placed if there is work to be
   done.  If the fforce argument is TRUE, a call is forced even if not
   enough time has passed since the last failed call.  If the fquiet
   argument is FALSE (the normal case), then a warning is given if
   calls are not permitted at this time.  */

static boolean
fcall (puuconf, zconfig, fuuxqt, qorigsys, qport, fifwork, fforce, fdetach,
       fquiet, ftrynext)
     pointer puuconf;
     const char *zconfig;
     boolean fuuxqt;
     const struct uuconf_system *qorigsys;
     struct uuconf_port *qport;
     boolean fifwork;
     boolean fforce;
     boolean fdetach;
     boolean fquiet;
     boolean ftrynext;
{
  struct sstatus sstat;
  long inow;
  boolean fbadtime, fnevertime, ffoundwork;
  const struct uuconf_system *qsys;

  if (! fsysdep_get_status (qorigsys, &sstat, (boolean *) NULL))
    return FALSE;
  ubuffree (sstat.zstring);

  /* Make sure it's been long enough since the last failed call, and
     that we haven't exceeded the maximum number of retries.  Even if
     we are over the limit on retries, we permit a call to be made if
     24 hours have passed.  This 24 hour limit is still controlled by
     the retry time.  We ignore times in the future, presumably the
     result of some sort of error.  */
  inow = ixsysdep_time ((long *) NULL);
  if (! fforce)
    {
      if (qorigsys->uuconf_cmax_retries > 0
	  && sstat.cretries >= qorigsys->uuconf_cmax_retries
	  && sstat.ilast <= inow
	  && sstat.ilast + SECS_PER_DAY > inow)
	{
	  ulog (LOG_ERROR, "Too many retries");
	  return FALSE;
	}

      if ((sstat.ttype == STATUS_COMPLETE
	   ? sstat.ilast + qorigsys->uuconf_csuccess_wait > inow
	   : sstat.ilast + sstat.cwait > inow)
	  && sstat.ilast <= inow)
	{
	  ulog (LOG_NORMAL, "Retry time not reached");
	  return FALSE;
	}
    }

  sDaemon.puuconf = puuconf;
  sDaemon.zconfig = zconfig;
  if (! fuuxqt)
    sDaemon.irunuuxqt = UUCONF_RUNUUXQT_NEVER;
  else
    {
      int iuuconf;

      iuuconf = uuconf_runuuxqt (puuconf, &sDaemon.irunuuxqt);
      if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
    }

  fbadtime = TRUE;
  fnevertime = TRUE;
  ffoundwork = FALSE;

  for (qsys = qorigsys; qsys != NULL; qsys = qsys->uuconf_qalternate)
    {
      int cretry;
      boolean fany, fret, fcalled;

      if (FGOT_SIGNAL ())
	return FALSE;

      if (! qsys->uuconf_fcall || qsys->uuconf_qtimegrade == NULL)
	continue;

      fnevertime = FALSE;

      /* Make sure this is a legal time to call.  */
      if (! ftimespan_match (qsys->uuconf_qtimegrade, (long *) NULL,
			     &cretry))
	continue;

      fbadtime = FALSE;

      sDaemon.qsys = qsys;
      sDaemon.zlocalname = NULL;
      sDaemon.qconn = NULL;
      sDaemon.qproto = NULL;
      sDaemon.cchans = 1;
      sDaemon.clocal_size = -1;
      sDaemon.cremote_size = -1;
      sDaemon.cmax_ever = -2;
      sDaemon.cmax_receive = -1;
      sDaemon.csent = 0;
      sDaemon.creceived = 0;
      sDaemon.cxfiles_received = 0;
      sDaemon.ifeatures = 0;
      sDaemon.frequest_hangup = FALSE;
      sDaemon.fhangup_requested = FALSE;
      sDaemon.fhangup = FALSE;
      sDaemon.fmaster = TRUE;
      sDaemon.fcaller = TRUE;
      sDaemon.ireliable = 0;
      sDaemon.bgrade = '\0';

      /* Queue up any work there is to do.  */
      if (! fqueue (&sDaemon, &fany))
	return FALSE;

      /* If we are only supposed to call if there is work, and there
	 isn't any work, check the next alternates.  We can't give up
	 at this point because there might be some other alternates
	 with fewer restrictions on grade or file transfer size.  */
      if (fifwork && ! fany)
	{
	  uclear_queue (&sDaemon);
	  continue;
	}

      ffoundwork = TRUE;

      fret = fconn_call (&sDaemon, qport, &sstat, cretry, &fcalled);

      uclear_queue (&sDaemon);

      if (fret)
	return TRUE;
      if (fcalled && ! ftrynext)
	return FALSE;

      /* Now we have to dump that port so that we can aquire a new
	 one.  On Unix this means that we will fork and get a new
	 process ID, so we must unlock and relock the system.  */
      if (fdetach)
	{
	  (void) fsysdep_unlock_system (&sLocked_system);
	  fLocked_system = FALSE;
	  usysdep_detach ();
	  if (! fsysdep_lock_system (&sLocked_system))
	    return FALSE;
	  fLocked_system = TRUE;
	}
    }

  /* We only get here if no call succeeded.  If fbadtime is TRUE it
     was the wrong time for all the alternates.  Otherwise, if
     ffoundwork is FALSE there was no work for any of the alternates.
     Otherwise, we attempted a call and fconn_call logged an error
     message.  */

  if (fbadtime)
    {
      if (! fquiet)
	ulog (LOG_NORMAL, "Wrong time to call");

      /* Update the status, unless the system can never be called.  If
	 the system can never be called, there is little point to
	 putting in a ``wrong time to call'' message.  We don't change
	 the number of retries, although we do set the wait until the
	 next retry to 0.  */
      if (! fnevertime)
	{
	  sstat.ttype = STATUS_WRONG_TIME;
	  sstat.ilast = inow;
	  sstat.cwait = 0;
	  (void) fsysdep_set_status (qorigsys, &sstat);
	}
    }
  else if (! ffoundwork)
    {
      if (! fquiet)
	ulog (LOG_NORMAL, "No work");
      return TRUE;
    }

  return FALSE;
}

/* Find a port to use when calling a system, open a connection, and
   dial the system.  The actual call is done in fdo_call.  This
   routine is responsible for opening and closing the connection.  */

static boolean
fconn_call (qdaemon, qport, qstat, cretry, pfcalled)
     struct sdaemon *qdaemon;
     struct uuconf_port *qport;
     struct sstatus *qstat;
     int cretry;
     boolean *pfcalled;
{
  pointer puuconf;
  const struct uuconf_system *qsys;
  struct uuconf_port sport;
  struct sconnection sconn;
  enum tstatus_type terr;
  boolean fret;

  puuconf = qdaemon->puuconf;
  qsys = qdaemon->qsys;

  *pfcalled = FALSE;

  /* Ignore any SIGHUP signal we may have received up to this point.
     This is needed on Unix because we may have gotten one from the
     shell before we detached from the controlling terminal.  */
  afSignal[INDEXSIG_SIGHUP] = FALSE;

  /* If no port was specified on the command line, use any port
     defined for the system.  To select the system port: 1) see if
     port information was specified directly; 2) see if a port was
     named; 3) get an available port given the baud rate.  We don't
     change the system status if a port is unavailable; i.e. we don't
     force the system to wait for the retry time.  */
  if (qport == NULL)
    qport = qsys->uuconf_qport;
  if (qport != NULL)
    {
      if (! fconn_init (qport, &sconn, UUCONF_PORTTYPE_UNKNOWN))
	return FALSE;
      if (! fconn_lock (&sconn, FALSE))
	{
	  ulog (LOG_ERROR, "%s: Port already locked",
		qport->uuconf_zname);
	  return FALSE;
	}
    }
  else
    {
      struct spass s;
      int iuuconf;

      s.fmatched = FALSE;
      s.flocked = FALSE;
      s.qconn = &sconn;
      iuuconf = uuconf_find_port (puuconf, qsys->uuconf_zport,
				  qsys->uuconf_ibaud,
				  qsys->uuconf_ihighbaud,
				  iuport_lock, (pointer) &s,
				  &sport);
      if (iuuconf == UUCONF_NOT_FOUND)
	{
	  if (s.fmatched)
	    ulog (LOG_ERROR, "All matching ports in use");
	  else
	    ulog (LOG_ERROR, "No matching ports");
	  return FALSE;
	}
      else if (iuuconf != UUCONF_SUCCESS)
	{
	  ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	  if (s.flocked)
	    {
	      (void) fconn_unlock (&sconn);
	      uconn_free (&sconn);
	    }
	  return FALSE;
	}
    }

  if (! fconn_open (&sconn, qsys->uuconf_ibaud, qsys->uuconf_ihighbaud,
		    FALSE))
    {
      terr = STATUS_PORT_FAILED;
      fret = FALSE;
    }
  else
    {
      struct uuconf_dialer *qdialer;
      struct uuconf_dialer sdialer;
      enum tdialerfound tdialer;

      if (qsys->uuconf_zalternate == NULL)
	ulog (LOG_NORMAL, "Calling system %s (port %s)", qsys->uuconf_zname,
	      zLdevice == NULL ? (char *) "unknown" : zLdevice);
      else
	ulog (LOG_NORMAL, "Calling system %s (alternate %s, port %s)",
	      qsys->uuconf_zname, qsys->uuconf_zalternate,
	  zLdevice == NULL ? (char *) "unknown" : zLdevice);

      qdialer = NULL;

      if (! fconn_dial (&sconn, puuconf, qsys, qsys->uuconf_zphone,
			&sdialer, &tdialer))
	{
	  tdialer = DIALERFOUND_FALSE;
	  terr = STATUS_DIAL_FAILED;
	  fret = FALSE;
	}
      else
	{
	  qdaemon->qconn = &sconn;
	  if (tdialer == DIALERFOUND_FALSE)
	    qdialer = NULL;
	  else
	    qdialer = &sdialer;
	  fret = fdo_call (qdaemon, qstat, qdialer, pfcalled, &terr);
	}

      (void) fconn_close (&sconn, puuconf, qdialer, fret);

      if (tdialer == DIALERFOUND_FREE)
	(void) uuconf_dialer_free (puuconf, &sdialer);
    }

  if (! fret)
    {
      DEBUG_MESSAGE2 (DEBUG_HANDSHAKE, "Call failed: %d (%s)",
		      (int) terr, azStatus[(int) terr]);
      qstat->ttype = terr;
      qstat->cretries++;
      qstat->ilast = ixsysdep_time ((long *) NULL);
      if (cretry == 0)
	qstat->cwait = CRETRY_WAIT (qstat->cretries);
      else
	qstat->cwait = cretry * 60;
      (void) fsysdep_set_status (qsys, qstat);
    }

  (void) fconn_unlock (&sconn);
  uconn_free (&sconn);

  if (qport == NULL)
    (void) uuconf_port_free (puuconf, &sport);

  return fret;
}

/* Do the actual work of calling another system.  The qsys argument is
   the system to call, the qconn argument is the connection to use,
   the qstat argument holds the current status of the ssystem, and the
   qdialer argument holds the dialer being used (it may be NULL).  If
   we log in successfully, set *pfcalled to TRUE; this is used to
   distinguish a failed dial from a failure during the call.  If an
   error occurs *pterr is set to the status type to record.  */

static boolean
fdo_call (qdaemon, qstat, qdialer, pfcalled, pterr)
     struct sdaemon *qdaemon;
     struct sstatus *qstat;
     const struct uuconf_dialer *qdialer;
     boolean *pfcalled;
     enum tstatus_type *pterr;
{
  pointer puuconf;
  const struct uuconf_system *qsys;
  struct sconnection *qconn;
  int iuuconf;
  int istrip;
  boolean fstrip;
  const char *zport;
  char *zstr;
  long istart_time;
  char *zlog;

  puuconf = qdaemon->puuconf;
  qsys = qdaemon->qsys;
  qconn = qdaemon->qconn;

  iuuconf = uuconf_strip (puuconf, &istrip);
  if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      return FALSE;
    }
  fstrip = (istrip & UUCONF_STRIP_PROTO) != 0;

  *pterr = STATUS_LOGIN_FAILED;

  if (qconn->qport == NULL)
    zport = "unknown";
  else
    zport = qconn->qport->uuconf_zname;
  if (! fchat (qconn, puuconf, &qsys->uuconf_schat, qsys,
	       (const struct uuconf_dialer *) NULL,
	       (const char *) NULL, FALSE, zport,
	       iconn_baud (qconn)))
    return FALSE;

  *pfcalled = TRUE;
  istart_time = ixsysdep_time ((long *) NULL);

  *pterr = STATUS_HANDSHAKE_FAILED;

  /* We should now see "Shere" from the other system.  Newer systems
     send "Shere=foo" where foo is the remote name.  */
  zstr = zget_uucp_cmd (qconn, TRUE, fstrip);
  if (zstr == NULL)
    return FALSE;

  if (strncmp (zstr, "Shere", 5) != 0)
    {
      ulog (LOG_ERROR, "Bad startup string (expected \"Shere\" got \"%s\")",
	    zstr);
      ubuffree (zstr);
      return FALSE;
    }

  ulog (LOG_NORMAL, "Login successful");

  qstat->ttype = STATUS_TALKING;
  qstat->ilast = ixsysdep_time ((long *) NULL);
  qstat->cretries = 0;
  qstat->cwait = 0;
  if (! fsysdep_set_status (qsys, qstat))
    return FALSE;

  if (zstr[5] == '=')
    {
      const char *zheresys;
      size_t clen;
      int icmp;

      /* Some UUCP packages only provide seven characters in the Shere
	 machine name.  Others only provide fourteen.  */
      zheresys = zstr + 6;
      clen = strlen (zheresys);
      if (clen == 7 || clen == 14)
	icmp = strncmp (zheresys, qsys->uuconf_zname, clen);
      else
	icmp = strcmp (zheresys, qsys->uuconf_zname);
      if (icmp != 0)
	{
	  if (qsys->uuconf_pzalias != NULL)
	    {
	      char **pz;

	      for (pz = qsys->uuconf_pzalias; *pz != NULL; pz++)
		{
		  if (clen == 7 || clen == 14)
		    icmp = strncmp (zheresys, *pz, clen);
		  else
		    icmp = strcmp (zheresys, *pz);
		  if (icmp == 0)
		    break;
		}
	    }
	  if (icmp != 0)
	    {
	      ulog (LOG_ERROR, "Called wrong system (%s)", zheresys);
	      ubuffree (zstr);
	      return FALSE;
	    }
	}
    }
#if DEBUG > 1
  else if (zstr[5] != '\0')
    DEBUG_MESSAGE1 (DEBUG_HANDSHAKE,
		    "fdo_call: Strange Shere: %s", zstr);
#endif

  ubuffree (zstr);

  /* We now send "S" name switches, where name is our UUCP name.  If
     we are using sequence numbers with this system, we send a -Q
     argument with the sequence number.  If the call-timegrade command
     was used, we send a -p argument and a -vgrade= argument with the
     grade to send us (we send both argument to make it more likely
     that one is recognized).  We always send a -N (for new) switch
     indicating what new features we support.  */
  {
    long ival;
    char bgrade;
    char *zsend;
    boolean fret;

    /* Determine the grade we should request of the other system.  A
       '\0' means that no restrictions have been made.  */
    if (! ftimespan_match (qsys->uuconf_qcalltimegrade, &ival,
			   (int *) NULL))
      bgrade = '\0';
    else
      bgrade = (char) ival;

    /* Determine the name we will call ourselves.  */
    if (qsys->uuconf_zlocalname != NULL)
      qdaemon->zlocalname = qsys->uuconf_zlocalname;
    else
      {
	iuuconf = uuconf_localname (puuconf, &qdaemon->zlocalname);
	if (iuuconf == UUCONF_NOT_FOUND)
	  {
	    qdaemon->zlocalname = zsysdep_localname ();
	    if (qdaemon->zlocalname == NULL)
	      return FALSE;
	  }
	else if (iuuconf != UUCONF_SUCCESS)
	  {
	    ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	    return FALSE;
	  }
      }	    

    zsend = zbufalc (strlen (qdaemon->zlocalname) + 70);
    if (! qsys->uuconf_fsequence)
      {
	if (bgrade == '\0')
	  sprintf (zsend, "S%s -R -N0%o", qdaemon->zlocalname,
		   (unsigned int) (FEATURE_SIZES
				   | FEATURE_EXEC
				   | FEATURE_RESTART));
	else
	  sprintf (zsend, "S%s -p%c -vgrade=%c -R -N0%o",
		   qdaemon->zlocalname, bgrade, bgrade,
		   (unsigned int) (FEATURE_SIZES
				   | FEATURE_EXEC
				   | FEATURE_RESTART));
      }
    else
      {
	long iseq;

	iseq = ixsysdep_get_sequence (qsys);
	if (iseq < 0)
	  return FALSE;
	if (bgrade == '\0')
	  sprintf (zsend, "S%s -Q%ld -R -N0%o", qdaemon->zlocalname, iseq,
		   (unsigned int) (FEATURE_SIZES
				   | FEATURE_EXEC
				   | FEATURE_RESTART));
	else
	  sprintf (zsend, "S%s -Q%ld -p%c -vgrade=%c -R -N0%o",
		   qdaemon->zlocalname, iseq, bgrade, bgrade,
		   (unsigned int) (FEATURE_SIZES
				   | FEATURE_EXEC
				   | FEATURE_RESTART));
      }

    fret = fsend_uucp_cmd (qconn, zsend);
    ubuffree (zsend);
    if (! fret)
	return FALSE;
  }

  /* Now we should see ROK or Rreason where reason gives a cryptic
     reason for failure.  If we are talking to a counterpart, we will
     get back ROKN, possibly with a feature bitfield attached.  */
  zstr = zget_uucp_cmd (qconn, TRUE, fstrip);
  if (zstr == NULL)
    return FALSE;

  if (zstr[0] != 'R')
    {
      ulog (LOG_ERROR, "Bad response to handshake string (%s)",
	    zstr);
      ubuffree (zstr);
      return FALSE;
    }

  if (strncmp (zstr + 1, "OKN", sizeof "OKN" - 1) == 0)
    {
      if (zstr[sizeof "ROKN" - 1] == '\0')
	qdaemon->ifeatures |= FEATURE_SIZES | FEATURE_V103;
      else
	qdaemon->ifeatures |= (int) strtol (zstr + sizeof "ROKN" - 1,
					   (char **) NULL, 0);
    }
  else if (strncmp (zstr + 1, "OK", sizeof "OK" - 1) == 0)
    {
      if (zstr[sizeof "ROK" - 1] != '\0')
	{
	  char *zopt;

	  /* SVR4 UUCP returns options following the ROK string.  */
	  zopt = zstr + sizeof "ROK" - 1;
	  while (*zopt != '\0')
	    {
	      char b;
	      long c;
	      char *zend;

	      b = *zopt++;
	      if (isspace (b) || b != '-')
		continue;
	      switch (*zopt)
		{
		case 'R':
		  qdaemon->ifeatures |= (FEATURE_RESTART
					 | FEATURE_SVR4
					 | FEATURE_SIZES);
		  break;
		case 'U':
		  c = strtol (zopt, &zend, 0);
		  if (c > 0 && c <= LONG_MAX / (long) 512)
		    qdaemon->cmax_receive = c * (long) 512;
		  zopt = zend;
		  break;
		}
	      while (*zopt != '\0' && ! isspace (*zopt))
		++zopt;
	    }
	}
    }
  else if (strcmp (zstr + 1, "CB") == 0)
    {
      ulog (LOG_NORMAL, "Remote system will call back");
      qstat->ttype = STATUS_COMPLETE;
      (void) fsysdep_set_status (qsys, qstat);
      ubuffree (zstr);
      return TRUE;
    }
  else
    {
      ulog (LOG_ERROR, "Handshake failed (%s)", zstr + 1);
      ubuffree (zstr);
      return FALSE;
    }

  ubuffree (zstr);

  /* The slave should now send \020Pprotos\0 where protos is a list of
     supported protocols.  Each protocol is a single character.  */
  zstr = zget_uucp_cmd (qconn, TRUE, fstrip);
  if (zstr == NULL)
    return FALSE;

  if (zstr[0] != 'P')
    {
      ulog (LOG_ERROR, "Bad protocol handshake (%s)", zstr);
      ubuffree (zstr);
      return FALSE;
    }

  /* Determine the reliability characteristics of the connection by
     combining information for the port and the dialer.  If we have no
     information, default to a reliable eight-bit full-duplex
     connection.  */
  if (qconn->qport != NULL
      && (qconn->qport->uuconf_ireliable & UUCONF_RELIABLE_SPECIFIED) != 0)
    qdaemon->ireliable = qconn->qport->uuconf_ireliable;
  if (qdialer != NULL
      && (qdialer->uuconf_ireliable & UUCONF_RELIABLE_SPECIFIED) != 0)
    {
      if (qdaemon->ireliable != 0)
	qdaemon->ireliable &= qdialer->uuconf_ireliable;
      else
	qdaemon->ireliable = qdialer->uuconf_ireliable;
    }
  if (qdaemon->ireliable == 0)
    qdaemon->ireliable = (UUCONF_RELIABLE_RELIABLE
			  | UUCONF_RELIABLE_EIGHT
			  | UUCONF_RELIABLE_FULLDUPLEX
			  | UUCONF_RELIABLE_SPECIFIED);

  /* Now decide which protocol to use.  The system and the port may
     have their own list of protocols.  */
  {
    int i;
    char ab[5];

    i = CPROTOCOLS;
    if (qsys->uuconf_zprotocols != NULL
	|| (qconn->qport != NULL
	    && qconn->qport->uuconf_zprotocols != NULL))
      {
	const char *zproto;

	if (qsys->uuconf_zprotocols != NULL)
	  zproto = qsys->uuconf_zprotocols;
	else
	  zproto = qconn->qport->uuconf_zprotocols;
	for (; *zproto != '\0'; zproto++)
	  {
	    if (strchr (zstr + 1, *zproto) != NULL)
	      {
		for (i = 0; i < CPROTOCOLS; i++)
		  if (asProtocols[i].bname == *zproto)
		    break;
		if (i < CPROTOCOLS)
		  break;
	      }
	  }
      }
    else
      {
	/* If neither the system nor the port specified a list of
	   protocols, we want only protocols that match the known
	   reliability of the dialer and the port.  */
	for (i = 0; i < CPROTOCOLS; i++)
	  {
	    int ipr;

	    ipr = asProtocols[i].ireliable;
	    if ((ipr & qdaemon->ireliable) != ipr)
	      continue;
	    if (strchr (zstr + 1, asProtocols[i].bname) != NULL)
	      break;
	  }
      }

    ubuffree (zstr);

    if (i >= CPROTOCOLS)
      {
	(void) fsend_uucp_cmd (qconn, "UN");
	ulog (LOG_ERROR, "No mutually supported protocols");
	return FALSE;
      }

    qdaemon->qproto = &asProtocols[i];

    /* If we are using a half-duplex line, act as though we have only
       a single channel; otherwise we might start a send and a receive
       at the same time.  */
    if ((qdaemon->ireliable & UUCONF_RELIABLE_FULLDUPLEX) == 0)
      qdaemon->cchans = 1;
    else
      qdaemon->cchans = asProtocols[i].cchans;

    sprintf (ab, "U%c", qdaemon->qproto->bname);
    if (! fsend_uucp_cmd (qconn, ab))
      return FALSE;
  }

  /* Run any protocol parameter commands.  */
  if (qdaemon->qproto->qcmds != NULL)
    {
      if (qsys->uuconf_qproto_params != NULL)
	uapply_proto_params (puuconf, qdaemon->qproto->bname,
			     qdaemon->qproto->qcmds,
			     qsys->uuconf_qproto_params);
      if (qconn->qport != NULL
	  && qconn->qport->uuconf_qproto_params != NULL)
	uapply_proto_params (puuconf, qdaemon->qproto->bname,
			     qdaemon->qproto->qcmds,
			     qconn->qport->uuconf_qproto_params);
      if (qdialer != NULL
	  && qdialer->uuconf_qproto_params != NULL)
	uapply_proto_params (puuconf, qdaemon->qproto->bname,
			     qdaemon->qproto->qcmds,
			     qdialer->uuconf_qproto_params);
    }

  /* Turn on the selected protocol.  */
  if (! (*qdaemon->qproto->pfstart) (qdaemon, &zlog))
    return FALSE;
  if (zlog == NULL)
    {
      zlog = zbufalc (sizeof "protocol ''" + 1);
      sprintf (zlog, "protocol '%c'", qdaemon->qproto->bname);
    }
  ulog (LOG_NORMAL, "Handshake successful (%s)", zlog);
  ubuffree (zlog);

  *pterr = STATUS_FAILED;

  {
    boolean fret;
    long iend_time;

    fret = floop (qdaemon);

    /* Now send the hangup message.  As the caller, we send six O's
       and expect to receive seven O's.  We send the six O's twice to
       help the other side.  We don't worry about errors here.  */
    if (fsend_uucp_cmd (qconn, "OOOOOO")
	&& fsend_uucp_cmd (qconn, "OOOOOO"))
      {
	int i, fdone;

	/* We look for the remote hangup string to ensure that the
	   modem has sent out our hangup string.  This is only
	   necessary because some versions of UUCP complain if they
	   don't get the hangup string.  The remote site should send 7
	   O's, but some versions of UUCP only send 6.  We look for
	   the string several times because supposedly some
	   implementations send some garbage after the last packet but
	   before the hangup string.  */
	for (i = 0; i < 25; i++)
	  {
	    zstr = zget_uucp_cmd (qconn, FALSE, fstrip);
	    if (zstr == NULL)
	      break;
	    fdone = strstr (zstr, "OOOOOO") != NULL;
	    ubuffree (zstr);
	    if (fdone)
	      break;
	  }
      }

    iend_time = ixsysdep_time ((long *) NULL);

    ulog (LOG_NORMAL, "Call complete (%ld seconds %ld bytes %ld bps)",
	  iend_time - istart_time,
	  qdaemon->csent + qdaemon->creceived,
	  (iend_time != istart_time
	   ? (qdaemon->csent + qdaemon->creceived) / (iend_time - istart_time)
	   : 0));

    if (fret)
      {
	qstat->ttype = STATUS_COMPLETE;
	qstat->ilast = iend_time;
	(void) fsysdep_set_status (qsys, qstat);
      }

    if (qdaemon->irunuuxqt == UUCONF_RUNUUXQT_PERCALL
	|| (qdaemon->irunuuxqt > 0 && qdaemon->cxfiles_received > 0))
      (void) fspawn_uuxqt (TRUE, qdaemon->qsys->uuconf_zname,
			   qdaemon->zconfig);

    return fret;
  }
}

/* This routine is called via uuconf_find_port when a matching port is
   found.  It tries to lock the port.  If it fails, it returns
   UUCONF_NOT_FOUND to force uuconf_find_port to continue searching
   for the next matching port.  */

static int
iuport_lock (qport, pinfo)
     struct uuconf_port *qport;
     pointer pinfo;
{
  struct spass *q = (struct spass *) pinfo;

  q->fmatched = TRUE;

  if (! fconn_init (qport, q->qconn, UUCONF_PORTTYPE_UNKNOWN))
    return UUCONF_NOT_FOUND;
  else if (! fconn_lock (q->qconn, FALSE))
    {
      uconn_free (q->qconn);
      return UUCONF_NOT_FOUND;
    }
  else
    {
      q->flocked = TRUE;
      return UUCONF_SUCCESS;
    }
}

/* The information structure used for the uuconf_callin comparison
   function.  */

struct scallin_info
{
  const char *zuser;
  const char *zpass;
};

/* Prompt for a login name and a password, and run as the slave.  */

static boolean
flogin_prompt (puuconf, zconfig, fuuxqt, qconn, zlogin, pzsystem)
     pointer puuconf;
     const char *zconfig;
     boolean fuuxqt;
     struct sconnection *qconn;
     const char *zlogin;
     const char **pzsystem;
{
  int iuuconf;
  int istrip;
  boolean fstrip;
  char *zuser, *zpass;
  boolean fret;
  struct scallin_info s;

  if (pzsystem != NULL)
    *pzsystem = NULL;

  DEBUG_MESSAGE0 (DEBUG_HANDSHAKE, "flogin_prompt: Waiting for login");

  iuuconf = uuconf_strip (puuconf, &istrip);
  if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      return FALSE;
    }
  fstrip = (istrip & UUCONF_STRIP_LOGIN) != 0;

  zuser = NULL;
  if (zlogin == NULL)
    {
      do
	{
	  ubuffree (zuser);
	  if (! fconn_write (qconn, "login: ", sizeof "login: " - 1))
	    return FALSE;
	  zuser = zget_typed_line (qconn, fstrip);
	}
      while (zuser != NULL && *zuser == '\0');

      if (zuser == NULL)
	return TRUE;

      zlogin = zuser;
    }

  if (! fconn_write (qconn, "Password:", sizeof "Password:" - 1))
    {
      ubuffree (zuser);
      return FALSE;
    }

  zpass = zget_typed_line (qconn, fstrip);
  if (zpass == NULL)
    {
      ubuffree (zuser);
      return TRUE;
    }

  fret = TRUE;

  s.zuser = zlogin;
  s.zpass = zpass;
  iuuconf = uuconf_callin (puuconf, icallin_cmp, &s);

  ubuffree (zpass);

  if (iuuconf == UUCONF_NOT_FOUND)
    ulog (LOG_ERROR, "Bad login");
  else if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      fret = FALSE;
    }
  else
    {
#if DEBUG > 1
      int iholddebug;
#endif

      /* We ignore the return value of faccept_call because we really
	 don't care whether the call succeeded or not.  We are going
	 to reset the port anyhow.  */
#if DEBUG > 1
      iholddebug = iDebug;
#endif
      (void) faccept_call (puuconf, zconfig, fuuxqt, zlogin, qconn, pzsystem);
#if DEBUG > 1
      iDebug = iholddebug;
#endif
    }

  ubuffree (zuser);

  return fret;
}

/* The comparison function which we pass to uuconf_callin.  This
   expands escape sequences in the login name, and either encrypts or
   expands escape sequences in the password.  */

static int
icallin_cmp (iwhich, pinfo, zfile)
     int iwhich;
     pointer pinfo;
     const char *zfile;
{
  struct scallin_info *qinfo = (struct scallin_info *) pinfo;
  char *zcopy;
  int icmp;

#if HAVE_ENCRYPTED_PASSWORDS
  if (iwhich != 0)
    return strcmp (crypt (qinfo->zpass, zfile), zfile) == 0;
#endif

  zcopy = zbufcpy (zfile);
  (void) cescape (zcopy);
  if (iwhich == 0)
    icmp = strcmp (qinfo->zuser, zcopy);
  else
    icmp = strcmp (qinfo->zpass, zcopy);
  ubuffree (zcopy);
  return icmp == 0;
}

/* Accept a call from a remote system.  If pqsys is not NULL, *pqsys
   will be set to the system that called in if known.  */

static boolean
faccept_call (puuconf, zconfig, fuuxqt, zlogin, qconn, pzsystem)
     pointer puuconf;
     const char *zconfig;
     boolean fuuxqt;
     const char *zlogin;
     struct sconnection *qconn;
     const char **pzsystem;
{
  long istart_time;
  int iuuconf;
  int istrip;
  boolean fstrip;
  const char *zport;
  struct uuconf_port *qport;
  struct uuconf_port sport;
  struct uuconf_dialer *qdialer;
  struct uuconf_dialer sdialer;
  boolean ftcp_port;
  char *zsend, *zspace;
  boolean fret;
  char *zstr;
  struct uuconf_system ssys;
  const struct uuconf_system *qsys;
  const struct uuconf_system *qany;
  char *zloc;
  struct sstatus sstat;
  boolean fgotseq, fgotn;
  int i;
  char *zlog;
  char *zgrade;

  if (pzsystem != NULL)
    *pzsystem = NULL;

  ulog (LOG_NORMAL, "Incoming call (login %s port %s)", zlogin,
	zLdevice == NULL ? (char *) "unknown" : zLdevice);

  istart_time = ixsysdep_time ((long *) NULL);

  iuuconf = uuconf_strip (puuconf, &istrip);
  if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
			    (struct uuconf_port *) NULL,
			    &sport, (char *) NULL);
      return FALSE;
    }
  fstrip = (istrip & UUCONF_STRIP_PROTO) != 0;

  /* Figure out protocol parameters determined by the port.  If no
     port was specified we're reading standard input, so try to get
     the port name and read information from the port file.  We only
     use the port information to get protocol parameters; we don't
     want to start treating the port as though it were a modem, for
     example.  */
  if (qconn->qport != NULL)
    {
      qport = qconn->qport;
      zport = qport->uuconf_zname;
      ftcp_port = FALSE;
    }
  else
    {
      zport = zsysdep_port_name (&ftcp_port);
      if (zport == NULL)
	{
	  qport = NULL;
	  zport = "unknown";
	}
      else
	{
	  iuuconf = uuconf_find_port (puuconf, zport, (long) 0, (long) 0,
				      (int (*) P((struct uuconf_port *,
						  pointer pinfo))) NULL,
				      (pointer) NULL,
				      &sport);
	  if (iuuconf == UUCONF_NOT_FOUND)
	    qport = NULL;
	  else if (iuuconf != UUCONF_SUCCESS)
	    {
	      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	      uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
				    (struct uuconf_port *) NULL,
				    &sport, (char *) NULL);
	      return FALSE;
	    }
	  else
	    qport = &sport;
	}
    }

  /* If we've managed to figure out that this is a modem port, now try
     to get protocol parameters from the dialer.  */
  qdialer = NULL;
  if (qport != NULL)
    {
      if (qport->uuconf_ttype == UUCONF_PORTTYPE_MODEM)
	{
	  if (qport->uuconf_u.uuconf_smodem.uuconf_pzdialer != NULL)
	    {
	      const char *zdialer;

	      zdialer = qport->uuconf_u.uuconf_smodem.uuconf_pzdialer[0];
	      iuuconf = uuconf_dialer_info (puuconf, zdialer, &sdialer);
	      if (iuuconf == UUCONF_SUCCESS)
		qdialer = &sdialer;
	    }
	  else
	    qdialer = qport->uuconf_u.uuconf_smodem.uuconf_qdialer;
	}	  
      else if (qport->uuconf_ttype == UUCONF_PORTTYPE_TCP
	       || (qport->uuconf_ttype == UUCONF_PORTTYPE_TLI
		   && (qport->uuconf_ireliable
		       & UUCONF_RELIABLE_SPECIFIED) == 0))
	ftcp_port = TRUE;
    }

  sDaemon.puuconf = puuconf;
  sDaemon.zconfig = zconfig;
  if (! fuuxqt)
    sDaemon.irunuuxqt = UUCONF_RUNUUXQT_NEVER;
  else
    {
      iuuconf = uuconf_runuuxqt (puuconf, &sDaemon.irunuuxqt);
      if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
    }
  sDaemon.qsys = NULL;
  sDaemon.zlocalname = NULL;
  sDaemon.qconn = qconn;
  sDaemon.qproto = NULL;
  sDaemon.cchans = 1;
  sDaemon.clocal_size = -1;
  sDaemon.cremote_size = -1;
  sDaemon.cmax_ever = -2;
  sDaemon.cmax_receive = -1;
  sDaemon.csent = 0;
  sDaemon.creceived = 0;
  sDaemon.cxfiles_received = 0;
  sDaemon.ifeatures = 0;
  sDaemon.frequest_hangup = FALSE;
  sDaemon.fhangup_requested = FALSE;
  sDaemon.fhangup = FALSE;
  sDaemon.fmaster = FALSE;
  sDaemon.fcaller = FALSE;
  sDaemon.ireliable = 0;
  sDaemon.bgrade = UUCONF_GRADE_LOW;

  /* Get the local name to use.  If uuconf_login_localname returns a
     value, it is not always freed up, although it should be.  */
  iuuconf = uuconf_login_localname (puuconf, zlogin, &zloc);
  if (iuuconf == UUCONF_SUCCESS)
    sDaemon.zlocalname = zloc;
  else if (iuuconf == UUCONF_NOT_FOUND)
    {
      sDaemon.zlocalname = zsysdep_localname ();
      if (sDaemon.zlocalname == NULL)
	{
	  uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
				qport, &sport, (char *) NULL);
	  return FALSE;
	}
    }
  else
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
			    qport, &sport, (char *) NULL);
      return FALSE;
    }

  /* Tell the remote system who we are.   */
  zsend = zbufalc (strlen (sDaemon.zlocalname) + sizeof "Shere=");
  sprintf (zsend, "Shere=%s", sDaemon.zlocalname);
  fret = fsend_uucp_cmd (qconn, zsend);
  ubuffree (zsend);
  if (! fret)
    {
      uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
			    qport, &sport, zloc);
      return FALSE;
    }

  zstr = zget_uucp_cmd (qconn, TRUE, fstrip);
  if (zstr == NULL)
    {
      uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
			    qport, &sport, zloc);
      return FALSE;
    }

  if (zstr[0] != 'S')
    {
      ulog (LOG_ERROR, "Bad introduction string");
      ubuffree (zstr);
      uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
			    qport, &sport, zloc);
      return FALSE;
    }

  zspace = strchr (zstr, ' ');
  if (zspace != NULL)
    *zspace = '\0';

  iuuconf = uuconf_system_info (puuconf, zstr + 1, &ssys);
  if (iuuconf == UUCONF_NOT_FOUND)
    {
      char *zscript;

      /* Run the remote.unknown script, if appropriate.  */
      iuuconf = uuconf_remote_unknown (puuconf, &zscript);
      if (iuuconf == UUCONF_SUCCESS)
	{
	  if (! fsysdep_unknown_caller (zscript, zstr + 1))
	    {
	      xfree ((pointer) zscript);
	      (void) fsend_uucp_cmd (qconn, "RYou are unknown to me");
	      ubuffree (zstr);
	      uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
				    qport, &sport, zloc);
	      return FALSE;
	    }
	  xfree ((pointer) zscript);
	}
      else if (iuuconf != UUCONF_NOT_FOUND)
	{
	  ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	  ubuffree (zstr);
	  uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
				qport, &sport, zloc);
	  return FALSE;
	}

      if (! funknown_system (puuconf, zstr + 1, &ssys))
	{
	  (void) fsend_uucp_cmd (qconn, "RYou are unknown to me");
	  ulog (LOG_ERROR, "Call from unknown system %s", zstr + 1);
	  ubuffree (zstr);
	  uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
				qport, &sport, zloc);
	  return FALSE;
	}
    }
  else if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      ubuffree (zstr);
      uaccept_call_cleanup (puuconf, (struct uuconf_system *) NULL,
			    qport, &sport, zloc);
      return FALSE;
    }

  qany = NULL;
  for (qsys = &ssys; qsys != NULL; qsys = qsys->uuconf_qalternate)
    {
      if (! qsys->uuconf_fcalled)
	continue;

      if (qsys->uuconf_zcalled_login == NULL
	  || strcmp (qsys->uuconf_zcalled_login, "ANY") == 0)
	{
	  if (qany == NULL)
	    qany = qsys;
	}
      else if (strcmp (qsys->uuconf_zcalled_login, zlogin) == 0)
	break;
    }

  if (qsys == NULL && qany != NULL)
    {
      iuuconf = uuconf_validate (puuconf, qany, zlogin);
      if (iuuconf == UUCONF_SUCCESS)
	qsys = qany;
      else if (iuuconf != UUCONF_NOT_FOUND)
	{
	  ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	  ubuffree (zstr);
	  uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
	  return FALSE;
	}
    }

  if (qsys == NULL)
    {
      (void) fsend_uucp_cmd (qconn, "RLOGIN");
      ulog (LOG_ERROR, "System %s used wrong login name %s",
	    zstr + 1, zlogin);
      ubuffree (zstr);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }

  sDaemon.qsys = qsys;

  if (pzsystem != NULL)
    *pzsystem = zbufcpy (qsys->uuconf_zname);

  ulog_system (qsys->uuconf_zname);

#if DEBUG > 1
  if (qsys->uuconf_zdebug != NULL)
    iDebug |= idebug_parse (qsys->uuconf_zdebug);
#endif

  /* See if we are supposed to call the system back.  This will queue
     up an empty command.  It would be better to actually call back
     directly at this point as well.  */
  if (qsys->uuconf_fcallback)
    {
      (void) fsend_uucp_cmd (qconn, "RCB");
      ulog (LOG_NORMAL, "Will call back");

      /* Clear any existing status.  */
      sstat.ttype = STATUS_COMPLETE;
      sstat.cretries = 0;
      sstat.ilast = ixsysdep_time ((long *) NULL);
      sstat.cwait = 0;
      (void) fsysdep_set_status (qsys, &sstat);

      ubuffree (zsysdep_spool_commands (qsys, UUCONF_GRADE_HIGH, 0,
					(const struct scmd *) NULL));
      ubuffree (zstr);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return TRUE;
    }

  /* We only permit one call at a time from a remote system.  Lock it.  */
  if (! fsysdep_lock_system (qsys))
    {
      if (qsys->uuconf_fsequence)
	{
	  /* At this point the calling system has already incremented
	     its sequence number, so we increment ours.  This will
	     only cause a mismatch if the other system is not what it
	     says it is.  */
	  (void) ixsysdep_get_sequence (qsys);
	}
      (void) fsend_uucp_cmd (qconn, "RLCK");
      ulog (LOG_ERROR, "System already locked");
      ubuffree (zstr);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }
  sLocked_system = *qsys;
  fLocked_system = TRUE;

  /* Set the system status.  We don't care what the status was before.
     We also don't want to kill the conversation just because we can't
     output the .Status file, so we ignore any errors.  */
  sstat.ttype = STATUS_TALKING;
  sstat.cretries = 0;
  sstat.ilast = ixsysdep_time ((long *) NULL);
  sstat.cwait = 0;
  (void) fsysdep_set_status (qsys, &sstat);

  /* Check the arguments of the remote system, if any.  */
  fgotseq = FALSE;
  fgotn = FALSE;
  if (zspace != NULL)
    {
      char **paz;
      char **pzset;

      ++zspace;

      /* Break the introduction line up into arguments.  */
      paz = (char **) xmalloc ((strlen (zspace) / 2 + 2) * sizeof (char *));
      pzset = paz;
      *pzset++ = NULL;
      while (TRUE)
	{
	  while (*zspace != '\0' && isspace (BUCHAR (*zspace)))
	    ++zspace;
	  if (*zspace == '\0')
	    break;
	  *pzset++ = zspace;
	  ++zspace;
	  while (*zspace != '\0' && ! isspace (BUCHAR (*zspace)))
	    ++zspace;
	  if (*zspace == '\0')
	    break;
	  *zspace++ = '\0';
	}

      if (pzset != paz + 1)
	{
	  int iopt;

	  *pzset = NULL;

	  /* We are going to use getopt to parse the arguments.  We
	     must clear optind to force getopt to reinitialize, and
	     clear opterr to prevent getopt from printing an error
	     message.  This approach assumes we are using the GNU
	     getopt, which is distributed with the program anyhow.  */
	  optind = 0;
	  opterr = 0;
	  
	  while ((iopt = getopt (pzset - paz, paz,
				 "N::p:Q:RU:v:x:")) != EOF)
	    {
	      long iseq;
	      long c;
	      char b;
	      int iwant;

	      switch (iopt)
		{
		case 'N':
		  /* This is used to indicate support for Taylor UUCP
		     extensions.  An plain -N mean support for size
		     negotiation.  If -N is followed by a number (with
		     no intervening space), the number is a bit field
		     of feature flags as defined in trans.h.  Note
		     that the argument may start with 0x for hex or 0
		     for octal.  */
		  fgotn = TRUE;
		  if (optarg == NULL)
		    sDaemon.ifeatures |= FEATURE_SIZES | FEATURE_V103;
		  else
		    sDaemon.ifeatures |= (int) strtol (optarg,
						       (char **) NULL,
						       0);
		  break;

		case 'p':
		  /* The argument is the lowest grade of work the
		     local system should send.  */
		  if (UUCONF_GRADE_LEGAL (optarg[0]))
		    sDaemon.bgrade = optarg[0];
		  break;

		case 'Q':
		  /* The conversation sequence number.  */
		  iseq = strtol (optarg, (char **) NULL, 10);
		  if (qsys->uuconf_fsequence
		      && iseq != ixsysdep_get_sequence (qsys))
		    {
		      (void) fsend_uucp_cmd (qconn, "RBADSEQ");
		      ulog (LOG_ERROR, "Out of sequence call rejected");
		      sstat.ttype = STATUS_FAILED;
		      (void) fsysdep_set_status (qsys, &sstat);
		      xfree ((pointer) paz);
		      ubuffree (zstr);
		      uaccept_call_cleanup (puuconf, &ssys, qport, &sport,
					    zloc);
		      return FALSE;
		    }
		  fgotseq = TRUE;
		  break;

		case 'R':
		  /* The remote system supports file restart.  */
		  sDaemon.ifeatures |= FEATURE_RESTART;
		  break;

		case 'U':
		  /* The maximum file size the remote system is
		     prepared to received, in blocks where each block
		     is 512 bytes.  */
		  c = strtol (optarg, (char **) NULL, 0);
		  if (c > 0 && c < LONG_MAX / (long) 512)
		    sDaemon.cmax_receive = c * (long) 512;
		  break;

		case 'v':
		  /* -vgrade=X can be used to set the lowest grade of
		     work the local system should send.  */
		  if (strncmp (optarg, "grade=", sizeof "grade=" - 1) == 0)
		    {
		      b = optarg[sizeof "grade=" - 1];
		      if (UUCONF_GRADE_LEGAL (b))
			sDaemon.bgrade = b;
		    }
		  break;

		case 'x':
		  iwant = (int) strtol (optarg, (char **) NULL, 10);
#if DEBUG > 1
		  if (iwant <= 9)
		    iwant = (1 << iwant) - 1;
		  if (qsys->uuconf_zmax_remote_debug != NULL)
		    iwant &= idebug_parse (qsys->uuconf_zmax_remote_debug);
		  else
		    iwant &= DEBUG_ABNORMAL | DEBUG_CHAT | DEBUG_HANDSHAKE;
		  if ((iDebug | iwant) != iDebug)
		    {
		      iDebug |= iwant;
		      ulog (LOG_NORMAL, "Setting debugging mode to 0%o",
			    iDebug);
		    }
#endif
		  break;

		default:
		  break;
		}
	    }
	}

      xfree ((pointer) paz);
    }

  ubuffree (zstr);

  if (qsys->uuconf_fsequence && ! fgotseq)
    {
      (void) fsend_uucp_cmd (qconn, "RBADSEQ");
      ulog (LOG_ERROR, "No sequence number (call rejected)");
      sstat.ttype = STATUS_FAILED;
      (void) fsysdep_set_status (qsys, &sstat);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }

  /* We recognized the system, and the sequence number (if any) was
     OK.  Send an ROK, and send a list of protocols.  If we got the -N
     switch, send ROKN to confirm it; if the -N switch was followed by
     a feature bitfield, return our own feature bitfield.  */
  {
    char ab[20];
    const char *zreply;

    if (! fgotn)
      {
	if ((sDaemon.ifeatures & FEATURE_RESTART) == 0)
	  zreply = "ROK";
	else
	  {
	    /* We got -R without -N, so assume that this is SVR4 UUCP.
	       SVR4 UUCP expects ROK -R to signal support for file
	       restart.  */
	    sDaemon.ifeatures |= FEATURE_SVR4 | FEATURE_SIZES;
	    zreply = "ROK -R";
	  }
      }
    else if ((sDaemon.ifeatures & FEATURE_V103) != 0)
      zreply = "ROKN";
    else
      {
	sprintf (ab, "ROKN0%o",
		 (unsigned int) (FEATURE_SIZES
				 | FEATURE_EXEC
				 | FEATURE_RESTART));
	zreply = ab;
      }
    if (! fsend_uucp_cmd (qconn, zreply))
      {
	sstat.ttype = STATUS_FAILED;
	(void) fsysdep_set_status (qsys, &sstat);
	uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
	return FALSE;
      }
  }

  /* Determine the reliability of the connection based on the
     reliability of the port and the dialer.  If we have no
     information, default to a reliable eight-bit full-duplex
     connection.  */
  if (ftcp_port)
    sDaemon.ireliable = (UUCONF_RELIABLE_SPECIFIED
			 | UUCONF_RELIABLE_ENDTOEND
			 | UUCONF_RELIABLE_RELIABLE
			 | UUCONF_RELIABLE_EIGHT
			 | UUCONF_RELIABLE_FULLDUPLEX);
  else
    {
      if (qport != NULL
	  && (qport->uuconf_ireliable & UUCONF_RELIABLE_SPECIFIED) != 0)
	sDaemon.ireliable = qport->uuconf_ireliable;
      if (qdialer != NULL
	  && (qdialer->uuconf_ireliable & UUCONF_RELIABLE_SPECIFIED) != 0)
	{
	  if (sDaemon.ireliable != 0)
	    sDaemon.ireliable &= qdialer->uuconf_ireliable;
	  else
	    sDaemon.ireliable = qdialer->uuconf_ireliable;
	}
      if (sDaemon.ireliable == 0)
	sDaemon.ireliable = (UUCONF_RELIABLE_RELIABLE
			     | UUCONF_RELIABLE_EIGHT
			     | UUCONF_RELIABLE_FULLDUPLEX
			     | UUCONF_RELIABLE_SPECIFIED);
    }

  if (qsys->uuconf_zprotocols != NULL ||
      (qport != NULL && qport->uuconf_zprotocols != NULL))
    {
      const char *zprotos;

      if (qsys->uuconf_zprotocols != NULL)
	zprotos = qsys->uuconf_zprotocols;
      else
	zprotos = qport->uuconf_zprotocols;
      zsend = zbufalc (strlen (zprotos) + 2);
      sprintf (zsend, "P%s", zprotos);
    }
  else
    {
      char *zset;

      zsend = zbufalc (CPROTOCOLS + 2);
      zset = zsend;
      *zset++ = 'P';

      /* If the system did not specify a list of protocols, we want
	 only protocols that match the known reliability of the dialer
	 and the port.  */
      for (i = 0; i < CPROTOCOLS; i++)
	{
	  int ipr;

	  ipr = asProtocols[i].ireliable;
	  if ((ipr & sDaemon.ireliable) != ipr)
	    continue;
	  *zset++ = asProtocols[i].bname;
	}
      *zset = '\0';
    }

  fret = fsend_uucp_cmd (qconn, zsend);
  ubuffree (zsend);
  if (! fret)
    {
      sstat.ttype = STATUS_FAILED;
      (void) fsysdep_set_status (qsys, &sstat);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }
    
  /* The master will now send back the selected protocol.  */
  zstr = zget_uucp_cmd (qconn, TRUE, fstrip);
  if (zstr == NULL)
    {
      sstat.ttype = STATUS_FAILED;
      (void) fsysdep_set_status (qsys, &sstat);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }

  if (zstr[0] != 'U')
    {
      ulog (LOG_ERROR, "Bad protocol response string");
      sstat.ttype = STATUS_FAILED;
      (void) fsysdep_set_status (qsys, &sstat);
      ubuffree (zstr);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }

  if (zstr[1] == 'N')
    {
      ulog (LOG_ERROR, "No supported protocol");
      sstat.ttype = STATUS_FAILED;
      (void) fsysdep_set_status (qsys, &sstat);
      ubuffree (zstr);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }

  for (i = 0; i < CPROTOCOLS; i++)
    if (asProtocols[i].bname == zstr[1])
      break;

  ubuffree (zstr);

  if (i >= CPROTOCOLS)
    {
      ulog (LOG_ERROR, "No supported protocol");
      sstat.ttype = STATUS_FAILED;
      (void) fsysdep_set_status (qsys, &sstat);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }

  sDaemon.qproto = &asProtocols[i];

  /* If we are using a half-duplex line, act as though we have only a
     single channel; otherwise we might start a send and a receive at
     the same time.  */
  if ((sDaemon.ireliable & UUCONF_RELIABLE_FULLDUPLEX) == 0)
    sDaemon.cchans = 1;
  else
    sDaemon.cchans = asProtocols[i].cchans;

  /* Run the chat script for when a call is received.  */
  if (! fchat (qconn, puuconf, &qsys->uuconf_scalled_chat, qsys,
	       (const struct uuconf_dialer *) NULL, (const char *) NULL,
	       FALSE, zport, iconn_baud (qconn)))
    {
      sstat.ttype = STATUS_FAILED;
      sstat.ilast = ixsysdep_time ((long *) NULL);
      (void) fsysdep_set_status (qsys, &sstat);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }

  /* Run any protocol parameter commands.  */
  if (sDaemon.qproto->qcmds != NULL)
    {
      if (qsys->uuconf_qproto_params != NULL)
	uapply_proto_params (puuconf, sDaemon.qproto->bname,
			     sDaemon.qproto->qcmds,
			     qsys->uuconf_qproto_params);
      if (qport != NULL
	  && qport->uuconf_qproto_params != NULL)
	uapply_proto_params (puuconf, sDaemon.qproto->bname,
			     sDaemon.qproto->qcmds,
			     qport->uuconf_qproto_params);
      if (qdialer != NULL
	  && qdialer->uuconf_qproto_params != NULL)
	uapply_proto_params (puuconf, sDaemon.qproto->bname,
			     sDaemon.qproto->qcmds,
			     qdialer->uuconf_qproto_params);
    }

  /* We don't need the dialer information any more.  */
  if (qdialer == &sdialer)
    (void) uuconf_dialer_free (puuconf, &sdialer);

  /* Turn on the selected protocol and get any jobs queued for the
     system.  */
  if (! (*sDaemon.qproto->pfstart) (&sDaemon, &zlog)
      || ! fqueue (&sDaemon, (boolean *) NULL))
    {
      uclear_queue (&sDaemon);
      sstat.ttype = STATUS_FAILED;
      sstat.ilast = ixsysdep_time ((long *) NULL);
      (void) fsysdep_set_status (qsys, &sstat);
      uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);
      return FALSE;
    }

  if (zlog == NULL)
    {
      zlog = zbufalc (sizeof "protocol ''" + 1);
      sprintf (zlog, "protocol '%c'", sDaemon.qproto->bname);
    }

  zgrade = zbufalc (sizeof "grade  " + 1);
  if (sDaemon.bgrade == UUCONF_GRADE_LOW)
    *zgrade = '\0';
  else
    sprintf (zgrade, "grade %c ", sDaemon.bgrade);

  /* If we are using HAVE_HDB_LOGGING, then the previous ``incoming
     call'' message went to the general log, since we didn't know the
     system name at that point.  In that case, we repeat the port and
     login names.  */
#if HAVE_HDB_LOGGING
  ulog (LOG_NORMAL, "Handshake successful (login %s port %s %s%s)",
	zlogin,
	zLdevice == NULL ? "unknown" : zLdevice,
	zgrade, zlog);
#else /* ! HAVE_HDB_LOGGING */
  ulog (LOG_NORMAL, "Handshake successful (%s%s)", zgrade, zlog);
#endif /* ! HAVE_HDB_LOGGING */

  ubuffree (zlog);
  ubuffree (zgrade);

  {
    long iend_time;

    fret = floop (&sDaemon);

    /* Hangup.  As the answerer, we send seven O's and expect to
       receive six O's.  We send the seven O's twice to help the other
       side.  We don't worry about errors here.  */
    if (fsend_uucp_cmd (qconn, "OOOOOOO")
	&& fsend_uucp_cmd (qconn, "OOOOOOO"))
      {
	int fdone;

	/* We look for the remote hangup string to ensure that the
	   modem has sent out our hangup string.  This is only
	   necessary because some versions of UUCP complain if they
	   don't get the hangup string.  We look for the string
	   several times because supposedly some implementations send
	   some garbage after the last packet but before the hangup
	   string.  */
	for (i = 0; i < 25; i++)
	  {
	    zstr = zget_uucp_cmd (qconn, FALSE, fstrip);
	    if (zstr == NULL)
	      break;
	    fdone = strstr (zstr, "OOOOOO") != NULL;
	    ubuffree (zstr);
	    if (fdone)
	      break;
	  }
      }

    iend_time = ixsysdep_time ((long *) NULL);

    ulog (LOG_NORMAL, "Call complete (%ld seconds %ld bytes %ld bps)",
	  iend_time - istart_time,
	  sDaemon.csent + sDaemon.creceived,
	  (iend_time != istart_time
	   ? (sDaemon.csent + sDaemon.creceived) / (iend_time - istart_time)
	   : 0));

    uclear_queue (&sDaemon);

    if (fret)
      sstat.ttype = STATUS_COMPLETE;
    else
      sstat.ttype = STATUS_FAILED;
    sstat.ilast = iend_time;
    (void) fsysdep_set_status (qsys, &sstat);

    if (sDaemon.irunuuxqt == UUCONF_RUNUUXQT_PERCALL
	|| (sDaemon.irunuuxqt > 0 && sDaemon.cxfiles_received > 0))
      (void) fspawn_uuxqt (TRUE, qsys->uuconf_zname, zconfig);

    uaccept_call_cleanup (puuconf, &ssys, qport, &sport, zloc);

    return fret;
  }
}

/* Clean up after faccept_call.  */

static void
uaccept_call_cleanup (puuconf, qfreesys, qport, qfreeport, zloc)
     pointer puuconf;
     struct uuconf_system *qfreesys;
     struct uuconf_port *qport;
     struct uuconf_port *qfreeport;
     char *zloc;
{
  if (fLocked_system)
    {
      (void) fsysdep_unlock_system (&sLocked_system);
      fLocked_system = FALSE;
    }
  if (qfreesys != NULL)
    (void) uuconf_system_free (puuconf, qfreesys);
  if (qport == qfreeport)
    (void) uuconf_port_free (puuconf, qfreeport);
  xfree ((pointer) zloc);
  ulog_system ((const char *) NULL);
}

/* Apply protocol parameters, once we know the protocol.  */

static void
uapply_proto_params (puuconf, bproto, qcmds, pas)
     pointer puuconf;
     int bproto;
     struct uuconf_cmdtab *qcmds;
     struct uuconf_proto_param *pas;
{
  struct uuconf_proto_param *qp;

  for (qp = pas; qp->uuconf_bproto != '\0'; qp++)
    {
      if (qp->uuconf_bproto == bproto)
	{
	  struct uuconf_proto_param_entry *qe;

	  for (qe = qp->uuconf_qentries; qe->uuconf_cargs > 0; qe++)
	    {
	      int iuuconf;

	      iuuconf = uuconf_cmd_args (puuconf, qe->uuconf_cargs,
					 qe->uuconf_pzargs, qcmds,
					 (pointer) NULL,
					 (uuconf_cmdtabfn) NULL, 0,
					 (pointer) NULL);
	      if (UUCONF_ERROR_VALUE (iuuconf) != UUCONF_SUCCESS)
		{
		  ulog (LOG_ERROR, "Error in %c protocol parameters",
			bproto);
		  ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
		}
	    }

	  break;
	}
    }
}

/* Send a string to the other system beginning with a DLE
   character and terminated with a null byte.  This is only
   used when no protocol is in force.  */

static boolean
fsend_uucp_cmd (qconn, z)
     struct sconnection *qconn;
     const char *z;
{
  size_t cwrite;
  char *zalc;
  boolean fret;

  DEBUG_MESSAGE1 (DEBUG_HANDSHAKE, "fsend_uucp_cmd: Sending \"%s\"", z);

  cwrite = strlen (z) + 2;

  zalc = zbufalc (cwrite);
  zalc[0] = '\020';
  memcpy (zalc + 1, z, cwrite - 1);

  fret = fconn_write (qconn, zalc, cwrite);
  ubuffree (zalc);
  return fret;
}

/* Get a UUCP command beginning with a DLE character and ending with a
   null byte.  This is only used when no protocol is in force.  This
   implementation has the potential of being seriously slow.  It also
   doesn't have any real error recovery.  The frequired argument is
   passed as TRUE if we need the string; we don't care that much if
   we're closing down the connection anyhow.  */

#define CTIMEOUT (120)
#define CSHORTTIMEOUT (10)
#define CINCREMENT (100)

static char *
zget_uucp_cmd (qconn, frequired, fstrip)
     struct sconnection *qconn;
     boolean frequired;
     boolean fstrip;
{
  char *zalc;
  size_t calc;
  size_t cgot;
  boolean fintro;
  long iendtime;
  int ctimeout;
#if DEBUG > 1
  int cchars;
  int iolddebug;
#endif

  iendtime = ixsysdep_time ((long *) NULL);
  if (frequired)
    iendtime += CTIMEOUT;
  else
    iendtime += CSHORTTIMEOUT;

#if DEBUG > 1
  cchars = 0;
  iolddebug = iDebug;
  if (FDEBUGGING (DEBUG_HANDSHAKE))
    {
      ulog (LOG_DEBUG_START, "zget_uucp_cmd: Got \"");
      iDebug &=~ (DEBUG_INCOMING | DEBUG_PORT);
    }
#endif

  zalc = NULL;
  calc = 0;
  cgot = 0;
  fintro = FALSE;
  while ((ctimeout = (int) (iendtime - ixsysdep_time ((long *) NULL))) > 0)
    {
      int b;
      
      b = breceive_char (qconn, ctimeout, frequired);
      /* Now b == -1 on timeout, -2 on error.  */
      if (b < 0)
	{
#if DEBUG > 1
	  if (FDEBUGGING (DEBUG_HANDSHAKE))
	    {
	      ulog (LOG_DEBUG_END, "\" (%s)",
		    b == -1 ? "timeout" : "error");
	      iDebug = iolddebug;
	    }
#endif
	  if (b == -1 && frequired)
	    ulog (LOG_ERROR, "Timeout");
	  ubuffree (zalc);
	  return NULL;
	}

      /* Apparently some systems use parity on these strings, so we
	 optionally strip the parity bit.  */
      if (fstrip)
	b &= 0x7f;

#if DEBUG > 1
      if (FDEBUGGING (DEBUG_HANDSHAKE))
	{
	  char ab[5];

	  ++cchars;
	  if (cchars > 60)
	    {
	      ulog (LOG_DEBUG_END, "\"");
	      ulog (LOG_DEBUG_START, "zget_uucp_cmd: Got \"");
	      cchars = 0;
	    }
	  (void) cdebug_char (ab, b);
	  ulog (LOG_DEBUG_CONTINUE, "%s", ab);
	}
#endif

      if (! fintro)
	{
	  if (b == '\020')
	    fintro = TRUE;
	  continue;
	}

      /* If we see another DLE, something has gone wrong; continue
	 as though this were the first one we saw.  */
      if (b == '\020')
	{
	  cgot = 0;
	  continue;
	}

      /* Some systems send a trailing \n on the Shere line.  As far as
	 I can tell this line can never contain a \n, so this
	 modification should be safe enough.  */
      if (b == '\r' || b == '\n')
	b = '\0';

      if (cgot >= calc)
	{
	  char *znew;

	  calc += CINCREMENT;
	  znew = zbufalc (calc);
	  if (cgot > 0)
	    memcpy (znew, zalc, cgot);
	  ubuffree (zalc);
	  zalc = znew;
	}

      zalc[cgot] = (char) b;
      ++cgot;

      if (b == '\0')
	{
#if DEBUG > 1
	  if (FDEBUGGING (DEBUG_HANDSHAKE))
	    {
	      ulog (LOG_DEBUG_END, "\"");
	      iDebug = iolddebug;
	    }
#endif
	  return zalc;
	}
    }

#if DEBUG > 1
  if (FDEBUGGING (DEBUG_HANDSHAKE))
    {
      ulog (LOG_DEBUG_END, "\" (timeout)");
      iDebug = iolddebug;
    }
#endif

  ubuffree (zalc);

  if (frequired)
    ulog (LOG_ERROR, "Timeout");
  return NULL;
}

/* Read a sequence of characters up to a newline or carriage return,
   and return the line without the line terminating character.
   Remember whether the last string we returned ended in \r; if it
   did, ignore a leading \n to account for \r\n pairs.  */

static char *
zget_typed_line (qconn, fstrip)
     struct sconnection *qconn;
     boolean fstrip;
{
  static boolean flastcr; 
  char *zalc;
  size_t calc;
  size_t cgot;

#if DEBUG > 1
  int cchars;
  int iolddebug;

  cchars = 0;
  iolddebug = iDebug;
  if (FDEBUGGING (DEBUG_CHAT))
    {
      ulog (LOG_DEBUG_START, "zget_typed_line: Got \"");
      iDebug &=~ (DEBUG_INCOMING | DEBUG_PORT);
    }
#endif

  zalc = NULL;
  calc = 0;
  cgot = 0;
  while (TRUE)
    {
      int b;
      
      b = breceive_char (qconn, CTIMEOUT, FALSE);

      /* Now b == -1 on timeout, -2 on error.  */

      if (b == -2 || FGOT_SIGNAL ())
	{
#if DEBUG > 1
	  if (FDEBUGGING (DEBUG_CHAT))
	    {
	      ulog (LOG_DEBUG_END, "\" (error)");
	      iDebug = iolddebug;
	    }
#endif
	  ubuffree (zalc);
	  flastcr = FALSE;
	  return NULL;
	}

      if (b == -1)
	{
	  flastcr = FALSE;
	  continue;
	}

      /* Optionally strip the parity bit.  */
      if (fstrip)
	b &= 0x7f;

#if DEBUG > 1
      if (FDEBUGGING (DEBUG_CHAT))
	{
	  char ab[5];

	  ++cchars;
	  if (cchars > 60)
	    {
	      ulog (LOG_DEBUG_END, "\"");
	      ulog (LOG_DEBUG_START, "zget_typed_line: Got \"");
	      cchars = 0;
	    }
	  (void) cdebug_char (ab, b);
	  ulog (LOG_DEBUG_CONTINUE, "%s", ab);
	}
#endif

      if (b == '\n' && cgot == 0 && flastcr)
	{
	  /* Ignore \n in \r\n pair.  */
	  flastcr = FALSE;
	  continue;
	}

      flastcr = FALSE;

      if (cgot >= calc)
	{
	  char *znew;

	  calc += CINCREMENT;
	  znew = zbufalc (calc);
	  if (cgot > 0)
	    memcpy (znew, zalc, cgot);
	  ubuffree (zalc);
	  zalc = znew;
	}

      if (b == '\n')
	b = '\0';
      else if (b == '\r')
	{
	  flastcr = TRUE;
	  b = '\0';
	}

      zalc[cgot] = (char) b;
      ++cgot;

      if (b == '\0')
	{
#if DEBUG > 1
	  if (FDEBUGGING (DEBUG_CHAT))
	    {
	      ulog (LOG_DEBUG_END, "\"");
	      iDebug = iolddebug;
	    }
#endif
	  return zalc;
	}
    }
}

/* Spawn a uuxqt job.  This probably belongs in some other file, but I
   don't have a good place for it.  */

boolean
fspawn_uuxqt (ffork, zsys, zconfig)
     boolean ffork;
     const char *zsys;
     const char *zconfig;
{
  char *zsysarg;
  char *zconfigarg;
  boolean fret;

  if (zsys == NULL)
    zsysarg = NULL;
  else
    {
      zsysarg = zbufalc (sizeof "-s" + strlen (zsys));
      sprintf (zsysarg, "-s%s", zsys);
    }

  if (zconfig == NULL)
    zconfigarg = NULL;
  else
    {
      zconfigarg = zbufalc (sizeof "-I" + strlen (zconfig));
      sprintf (zconfigarg, "-I%s", zconfig);
      if (zsysarg == NULL)
	{
	  zsysarg = zconfigarg;
	  zconfigarg = NULL;
	}
    }

  fret = fsysdep_run (ffork, "uuxqt", zsysarg, zconfigarg);

  ubuffree (zsysarg);
  ubuffree (zconfigarg);

  return fret;
}
