/* cu.c
   Call up a remote system.

   Copyright (C) 1992, 1993, 1994 Ian Lance Taylor

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
const char cu_rcsid[] = "$Id: cu.c,v 1.2 1994/05/07 18:09:54 ache Exp $";
#endif

#include "cu.h"
#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "prot.h"
#include "system.h"
#include "sysdep.h"
#include "getopt.h"

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/* Here are the user settable variables.  The user is permitted to
   change these while running the program, using ~s.  */

/* The escape character used to introduce a special command.  The
   escape character is the first character of this string.  */
const char *zCuvar_escape = "~";

/* Whether to delay for a second before printing the host name after
   seeing an escape character.  */
boolean fCuvar_delay = TRUE;

/* The input characters which finish a line.  The escape character is
   only recognized following one of these characters.  The default is
   carriage return, ^U, ^C, ^O, ^D, ^S, ^Q, ^R, which I got from the
   Ultrix /etc/remote file.  */
const char *zCuvar_eol = "\r\025\003\017\004\023\021\022";

/* Whether to transfer binary data (nonprintable characters other than
   newline and tab) when sending a file.  If this is FALSE, then
   newline is changed to carriage return.  */
boolean fCuvar_binary = FALSE;

/* A prefix string to use before sending a binary character from a
   file; this is only used if fCuvar_binary is TRUE.  The default is
   ^V. */
const char *zCuvar_binary_prefix = "\026";

/* Whether to check for echoes of characters sent when sending a file.
   This is ignored if fCuvar_binary is TRUE.  */
boolean fCuvar_echocheck = FALSE;

/* A character to look for after each newline is sent when sending a
   file.  The character is the first character in this string, except
   that a '\0' means that no echo check is done.  */
const char *zCuvar_echonl = "\r";

/* The timeout to use when looking for an character.  */
int cCuvar_timeout = 30;

/* The character to use to kill a line if an echo check fails.  The
   first character in this string is sent.  The default is ^U.  */
const char *zCuvar_kill = "\025";

/* The number of times to try resending a line if the echo check keeps
   failing.  */
int cCuvar_resend = 10;

/* The string to send at the end of a file sent with ~>.  The default
   is ^D.  */
const char *zCuvar_eofwrite = "\004";

/* The string to look for to finish a file received with ~<.  For tip
   this is a collection of single characters, but I don't want to do
   that because it means that there are characters which cannot be
   received.  The default is a guess at a typical shell prompt.  */
const char *zCuvar_eofread = "$";

/* Whether to provide verbose information when sending or receiving a
   file.  */
boolean fCuvar_verbose = TRUE;

/* The table used to give a value to a variable, and to print all the
   variable values.  */

static const struct uuconf_cmdtab asCuvars[] =
{
  { "escape", UUCONF_CMDTABTYPE_STRING, (pointer) &zCuvar_escape, NULL },
  { "delay", UUCONF_CMDTABTYPE_BOOLEAN, (pointer) &fCuvar_delay, NULL },
  { "eol", UUCONF_CMDTABTYPE_STRING, (pointer) &zCuvar_eol, NULL },
  { "binary", UUCONF_CMDTABTYPE_BOOLEAN, (pointer) &fCuvar_binary, NULL },
  { "binary-prefix", UUCONF_CMDTABTYPE_STRING,
      (pointer) &zCuvar_binary_prefix, NULL },
  { "echocheck", UUCONF_CMDTABTYPE_BOOLEAN,
      (pointer) &fCuvar_echocheck, NULL },
  { "echonl", UUCONF_CMDTABTYPE_STRING, (pointer) &zCuvar_echonl, NULL },
  { "timeout", UUCONF_CMDTABTYPE_INT, (pointer) &cCuvar_timeout, NULL },
  { "kill", UUCONF_CMDTABTYPE_STRING, (pointer) &zCuvar_kill, NULL },
  { "resend", UUCONF_CMDTABTYPE_INT, (pointer) &cCuvar_resend, NULL },
  { "eofwrite", UUCONF_CMDTABTYPE_STRING, (pointer) &zCuvar_eofwrite, NULL },
  { "eofread", UUCONF_CMDTABTYPE_STRING, (pointer) &zCuvar_eofread, NULL },
  { "verbose", UUCONF_CMDTABTYPE_BOOLEAN, (pointer) &fCuvar_verbose, NULL },
  { NULL, 0, NULL, NULL}
};

/* The string printed at the initial connect.  */
#if ANSI_C
#define ZCONNMSG "\aConnected."
#else
#define ZCONNMSG "Connected."
#endif

/* The string printed when disconnecting.  */
#if ANSI_C
#define ZDISMSG "\aDisconnected."
#else
#define ZDISMSG "Disconnected."
#endif

/* Local variables.  */

/* The string we print when the user is once again connected to the
   port after transferring a file or taking some other action.  */
static const char abCuconnected[]
#if ANSI_C
  = "\a[connected]";
#else
  = "[connected]";
#endif

/* Global uuconf pointer.  */
static pointer pCuuuconf;

/* Connection.  */
static struct sconnection *qCuconn;

/* Whether to close the connection.  */
static boolean fCuclose_conn;

/* Dialer used to dial out.  */
static struct uuconf_dialer *qCudialer;

/* Whether we need to restore the terminal.  */
static boolean fCurestore_terminal;

/* Whether we are doing local echoing.  */
static boolean fCulocalecho;

/* Whether we need to call fsysdep_cu_finish.  */
static boolean fCustarted;

/* Whether ZCONNMSG has been printed yet.  */
static boolean fCuconnprinted = FALSE;

/* A structure used to pass information to icuport_lock.  */
struct sconninfo
{
  boolean fmatched;
  boolean flocked;
  struct sconnection *qconn;
  const char *zline;
};

/* Local functions.  */

static void ucuusage P((void));
static void ucuhelp P((void));
static void ucuabort P((void));
static void uculog_start P((void));
static void uculog_end P((void));
static int icuport_lock P((struct uuconf_port *qport, pointer pinfo));
static boolean fcudo_cmd P((pointer puuconf, struct sconnection *qconn,
			    int bcmd));
static boolean fcuset_var P((pointer puuconf, char *zline));
static int icuunrecogvar P((pointer puuconf, int argc, char **argv,
			    pointer pvar, pointer pinfo));
static int icuunrecogfn P((pointer puuconf, int argc, char **argv,
			   pointer pvar, pointer pinfo));
static void uculist_vars P((void));
static void uculist_fns P((const char *zescape));
static boolean fcudo_subcmd P((pointer puuconf, struct sconnection *qconn,
			       char *zline));
static boolean fcusend_buf P((struct sconnection *qconn, const char *zbuf,
			      size_t cbuf));

#define ucuputs(zline) \
       do { if (! fsysdep_terminal_puts (zline)) ucuabort (); } while (0)

/* Long getopt options.  */
static const struct option asCulongopts[] =
{
  { "phone", required_argument, NULL, 'c' },
  { "parity", required_argument, NULL, 2 },
  { "halfduplex", no_argument, NULL, 'h' },
  { "prompt", no_argument, NULL, 'n' },
  { "line", required_argument, NULL, 'l' },
  { "port", required_argument, NULL, 'p' },
  { "speed", required_argument, NULL, 's' },
  { "baud", required_argument, NULL, 's' },
  { "mapcr", no_argument, NULL, 't' },
  { "system", required_argument, NULL, 'z' },
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
  /* -c: phone number.  */
  char *zphone = NULL;
  /* -e: even parity.  */
  boolean feven = FALSE;
  /* -l: line.  */
  char *zline = NULL;
  /* -n: prompt for phone number.  */
  boolean fprompt = FALSE;
  /* -o: odd parity.  */
  boolean fodd = FALSE;
  /* -p: port name.  */
  const char *zport = NULL;
  /* -s: speed.  */
  long ibaud = 0L;
  /* -t: map cr to crlf.  */
  boolean fmapcr = FALSE;
  /* -z: system.  */
  const char *zsystem = NULL;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  int iopt;
  pointer puuconf;
  int iuuconf;
  const char *zlocalname;
  int i;
  struct uuconf_system ssys;
  const struct uuconf_system *qsys = NULL;
  boolean flooped;
  struct uuconf_port sport;
  struct sconnection sconn;
  struct sconninfo sinfo;
  long ihighbaud;
  struct uuconf_dialer sdialer;
  struct uuconf_dialer *qdialer;
  char bcmd;

  zProgram = argv[0];

  /* We want to accept -# as a speed.  It's easiest to look through
     the arguments, replace -# with -s#, and let getopt handle it.  */
  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-'
	  && isdigit (BUCHAR (argv[i][1])))
	{
	  size_t clen;
	  char *z;

	  clen = strlen (argv[i]);
	  z = zbufalc (clen + 2);
	  z[0] = '-';
	  z[1] = 's';
	  memcpy (z + 2, argv[i] + 1, clen);
 	  argv[i] = z;
	}
    }

  while ((iopt = getopt_long (argc, argv, "a:c:dehnI:l:op:s:tvx:z:",
			      asCulongopts, (int *) NULL)) != EOF)
    {
      switch (iopt)
	{
	case 'c':
	  /* Phone number.  */
	  zphone = optarg;
	  break;

	case 'd':
	  /* Set debugging level to maximum.  */
#if DEBUG > 1
	  iDebug = DEBUG_MAX;
#endif
	  break;

	case 'e':
	  /* Even parity.  */
	  feven = TRUE;
	  break;

	case 'h':
	  /* Local echo.  */
	  fCulocalecho = TRUE;
	  break;

	case 'n':
	  /* Prompt for phone number.  */
	  fprompt = TRUE;
	  break;

	case 'l':
	  /* Line name.  */
	  zline = optarg;
	  break;

	case 'o':
	  /* Odd parity.  */
	  fodd = TRUE;
	  break;

	case 'p':
	case 'a':
	  /* Port name (-a is for compatibility).  */
	  zport = optarg;
	  break;

	case 's':
	  /* Speed.  */
	  ibaud = strtol (optarg, (char **) NULL, 10);
	  break;

	case 't':
	  /* Map cr to crlf.  */
	  fmapcr = TRUE;
	  break;

	case 'z':
	  /* System name.  */
	  zsystem = optarg;
	  break;

	case 'I':
	  /* Configuration file name.  */
	  if (fsysdep_other_config (optarg))
	    zconfig = optarg;
	  break;

	case 'x':
#if DEBUG > 1
	  /* Set debugging level.  */
	  iDebug |= idebug_parse (optarg);
#endif
	  break;

	case 'v':
	  /* Print version and exit.  */
	  fprintf
	    (stderr,
	     "%s: Taylor UUCP %s, copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor\n",
	     zProgram, VERSION);
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 2:
	  /* --parity.  */
	  if (strncmp (optarg, "even", strlen (optarg)) == 0)
	    feven = TRUE;
	  else if (strncmp (optarg, "odd", strlen (optarg)) == 0)
	    fodd = TRUE;
	  else if (strncmp (optarg, "none", strlen (optarg)) == 0)
	    {
	      feven = TRUE;
	      fodd = TRUE;
	    }
	  else
	    {
	      fprintf (stderr, "%s: --parity requires even, odd or none\n",
		       zProgram);
	      ucuusage ();
	    }
	  break;

	case 1:
	  /* --help.  */
	  ucuhelp ();
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  ucuusage ();
	  /*NOTREACHED*/
	}
    }

  /* There can be one more argument, which is either a system name, a
     phone number, or "dir".  We decide which it is based on the first
     character.  To call a UUCP system whose name begins with a digit,
     or one which is named "dir", you must use -z.  */
  if (optind != argc)
    {
      if (optind != argc - 1
	  || zsystem != NULL
	  || zphone != NULL)
	{
	  fprintf (stderr, "%s: too many arguments\n", zProgram);
	  ucuusage ();
	}
      if (strcmp (argv[optind], "dir") != 0)
	{
	  if (isdigit (BUCHAR (argv[optind][0])))
	    zphone = argv[optind];
	  else
	    zsystem = argv[optind];
	}
    }

  /* If the user doesn't give a system, port, line or speed, then
     there's no basis on which to select a port.  */
  if (zsystem == NULL
      && zport == NULL
      && zline == NULL
      && ibaud == 0L)
    {
      fprintf (stderr, "%s: must specify system, line, port or speed\n",
	       zProgram);
      ucuusage ();
    }

  if (fprompt)
    {
      size_t cphone;

      printf ("Phone number: ");
      (void) fflush (stdout);
      zphone = NULL;
      cphone = 0;
      if (getline (&zphone, &cphone, stdin) <= 0
	  || *zphone == '\0')
	{
	  fprintf (stderr, "%s: no phone number entered\n", zProgram);
	  exit (EXIT_FAILURE);
	}
    }

  iuuconf = uuconf_init (&puuconf, "cu", zconfig);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
  pCuuuconf = puuconf;

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

  usysdep_initialize (puuconf, INIT_NOCHDIR | INIT_SUID);

  iuuconf = uuconf_localname (puuconf, &zlocalname);
  if (iuuconf == UUCONF_NOT_FOUND)
    {
      zlocalname = zsysdep_localname ();
      if (zlocalname == NULL)
	exit (EXIT_FAILURE);
    }
  else if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  ulog_fatal_fn (ucuabort);
  pfLstart = uculog_start;
  pfLend = uculog_end;

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

  if (zsystem != NULL)
    {
      iuuconf = uuconf_system_info (puuconf, zsystem, &ssys);
      if (iuuconf != UUCONF_SUCCESS)
	{
	  if (iuuconf != UUCONF_NOT_FOUND)
	    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
	  ulog (LOG_FATAL, "%s: System not found", zsystem);
	}
      qsys = &ssys;
    }

  /* This loop is used if a system is specified.  It loops over the
     various alternates until it finds one for which the dial
     succeeds.  This is an ugly spaghetti construction, and it should
     be broken up into different functions someday.  */
  flooped = FALSE;
  while (TRUE)
    {
      enum tparitysetting tparity;
      enum tstripsetting tstrip;

      /* The uuconf_find_port function only selects directly on a port
	 name and a speed.  To select based on the line name, we use a
	 function.  If we can't find any defined port, and the user
	 specified a line name but did not specify a port name or a
	 system or a phone number, then we fake a direct port with
	 that line name (we don't fake a port if a system or phone
	 number were given because if we fake a port we have no way to
	 place a call; perhaps we should automatically look up a
	 particular dialer).  This permits users to say cu -lttyd0
	 without having to put ttyd0 in the ports file, provided they
	 have read and write access to the port.  */
      sinfo.fmatched = FALSE;
      sinfo.flocked = FALSE;
      sinfo.qconn = &sconn;
      sinfo.zline = zline;
      if (zport != NULL || zline != NULL || ibaud != 0L)
	{
	  iuuconf = uuconf_find_port (puuconf, zport, ibaud, 0L,
				      icuport_lock, (pointer) &sinfo,
				      &sport);
	  if (iuuconf != UUCONF_SUCCESS)
	    {
	      if (iuuconf != UUCONF_NOT_FOUND)
		{
		  if (sinfo.flocked)
		    {
		      (void) fconn_unlock (&sconn);
		      uconn_free (&sconn);
		    }
		  ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
		}
	      if (zline == NULL
		  || zport != NULL
		  || zphone != NULL
		  || qsys != NULL)
		{
		  if (sinfo.fmatched)
		    ulog (LOG_FATAL, "All matching ports in use");
		  else
		    ulog (LOG_FATAL, "No matching ports");
		}

	      sport.uuconf_zname = zline;
	      sport.uuconf_ttype = UUCONF_PORTTYPE_DIRECT;
	      sport.uuconf_zprotocols = NULL;
	      sport.uuconf_qproto_params = NULL;
	      sport.uuconf_ireliable = 0;
	      sport.uuconf_zlockname = NULL;
	      sport.uuconf_palloc = NULL;
	      sport.uuconf_u.uuconf_sdirect.uuconf_zdevice = NULL;
	      sport.uuconf_u.uuconf_sdirect.uuconf_ibaud = ibaud;

	      if (! fconn_init (&sport, &sconn, UUCONF_PORTTYPE_UNKNOWN))
		ucuabort ();

	      if (! fconn_lock (&sconn, FALSE))
		ulog (LOG_FATAL, "%s: Line in use", zline);

	      qCuconn = &sconn;

	      /* Check user access after locking the port, because on
		 some systems shared lines affect the ownership and
		 permissions.  In such a case ``Line in use'' is more
		 clear than ``Permission denied.''  */
	      if (! fsysdep_port_access (&sport))
		ulog (LOG_FATAL, "%s: Permission denied", zline);
	    }
	  ihighbaud = 0L;
	}
      else
	{
	  for (; qsys != NULL; qsys = qsys->uuconf_qalternate)
	    {
	      if (! qsys->uuconf_fcall)
		continue;
	      if (qsys->uuconf_qport != NULL)
		{
		  if (fconn_init (qsys->uuconf_qport, &sconn,
				  UUCONF_PORTTYPE_UNKNOWN))
		    {
		      if (fconn_lock (&sconn, FALSE))
			{
			  qCuconn = &sconn;
			  break;
			}
		      uconn_free (&sconn);
		    }
		}
	      else
		{
		  sinfo.fmatched = FALSE;
		  sinfo.flocked = FALSE;
		  sinfo.qconn = &sconn;
		  iuuconf = uuconf_find_port (puuconf, qsys->uuconf_zport,
					      qsys->uuconf_ibaud,
					      qsys->uuconf_ihighbaud,
					      icuport_lock,
					      (pointer) &sinfo,
					      &sport);
		  if (iuuconf == UUCONF_SUCCESS)
		    break;
		  if (iuuconf != UUCONF_NOT_FOUND)
		    {
		      if (sinfo.flocked)
			{
			  (void) fconn_unlock (&sconn);
			  uconn_free (&sconn);
			}
		      ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
		    }
		}
	    }

	  if (qsys == NULL)
	    {
	      const char *zrem;

	      if (flooped)
		zrem = "remaining ";
	      else
		zrem = "";
	      if (sinfo.fmatched)
		ulog (LOG_FATAL, "%s: All %smatching ports in use",
		      zsystem, zrem);
	      else
		ulog (LOG_FATAL, "%s: No %smatching ports", zsystem, zrem);
	    }

	  ibaud = qsys->uuconf_ibaud;
	  ihighbaud = qsys->uuconf_ihighbaud;
	}

      /* Here we have locked a connection to use.  */
      if (! fconn_open (&sconn, ibaud, ihighbaud, FALSE))
	ucuabort ();

      fCuclose_conn = TRUE;

      if (FGOT_SIGNAL ())
	ucuabort ();

      /* Set up the connection.  */
      if (fodd && feven)
	{
	  tparity = PARITYSETTING_NONE;
	  tstrip = STRIPSETTING_SEVENBITS;
	}
      else if (fodd)
	{
	  tparity = PARITYSETTING_ODD;
	  tstrip = STRIPSETTING_SEVENBITS;
	}
      else if (feven)
	{
	  tparity = PARITYSETTING_EVEN;
	  tstrip = STRIPSETTING_SEVENBITS;
	}
      else
	{
	  tparity = PARITYSETTING_DEFAULT;
	  tstrip = STRIPSETTING_DEFAULT;
	}

      if (! fconn_set (&sconn, tparity, tstrip, XONXOFF_ON))
	ucuabort ();

      if (qsys != NULL)
	zphone = qsys->uuconf_zphone;

      if (qsys != NULL || zphone != NULL)
	{
	  enum tdialerfound tdialer;

	  if (! fconn_dial (&sconn, puuconf, qsys, zphone, &sdialer,
			    &tdialer))
	    {
	      if (zport != NULL
		  || zline != NULL
		  || ibaud != 0L
		  || qsys == NULL)
		ucuabort ();

	      qsys = qsys->uuconf_qalternate;
	      if (qsys == NULL)
		ulog (LOG_FATAL, "%s: No remaining alternates", zsystem);

	      fCuclose_conn = FALSE;
	      (void) fconn_close (&sconn, pCuuuconf, qCudialer, FALSE);
	      qCuconn = NULL;
	      (void) fconn_unlock (&sconn);
	      uconn_free (&sconn);

	      /* Loop around and try another alternate.  */
	      flooped = TRUE;
	      continue;
	    }
	  if (tdialer == DIALERFOUND_FALSE)
	    qdialer = NULL;
	  else
	    qdialer = &sdialer;
	}
      else
	{
	  /* If no system or phone number was specified, we connect
	     directly to the modem.  We only permit this if the user
	     has access to the port, since it permits various
	     shenanigans such as reprogramming the automatic
	     callbacks.  */
	  if (! fsysdep_port_access (sconn.qport))
	    ulog (LOG_FATAL, "Access to port denied");
	  qdialer = NULL;
	  if (! fconn_carrier (&sconn, FALSE))
	    ulog (LOG_FATAL, "Can't turn off carrier");
	}

      break;
    }

  qCudialer = qdialer;

  if (FGOT_SIGNAL ())
    ucuabort ();

  /* Here we have connected, and can start the main cu protocol.  The
     program spends most of its time in system dependent code, and
     only comes out when a special command is received from the
     terminal.  */
  printf ("%s\n", ZCONNMSG);
  fCuconnprinted = TRUE;

  if (! fsysdep_terminal_raw (fCulocalecho))
    ucuabort ();

  fCurestore_terminal = TRUE;

  if (! fsysdep_cu_init (&sconn))
    ucuabort ();

  fCustarted = TRUE;

  while (fsysdep_cu (&sconn, &bcmd, zlocalname))
    if (! fcudo_cmd (puuconf, &sconn, bcmd))
      break;

  fCustarted = FALSE;
  if (! fsysdep_cu_finish ())
    ucuabort ();

  fCurestore_terminal = FALSE;
  (void) fsysdep_terminal_restore ();

  (void) fconn_close (&sconn, puuconf, qdialer, TRUE);
  (void) fconn_unlock (&sconn);
  uconn_free (&sconn);

  if (fCuconnprinted)
    printf ("\n%s\n", ZDISMSG);

  ulog_close ();

  usysdep_exit (TRUE);

  /* Avoid errors about not returning a value.  */
  return 0;
}

/* Print a usage message and die.  */

static void
ucuusage ()
{
  fprintf (stderr, "Usage: %s [options] [system or phone-number]\n",
	   zProgram);
  fprintf (stderr, "Use %s --help for help\n", zProgram);
  exit (EXIT_FAILURE);
}

/* Print a help message.  */

static void
ucuhelp ()
{
  fprintf (stderr,
	   "Taylor UUCP %s, copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor\n",
	   VERSION);
  fprintf (stderr,
	   "Usage: %s [options] [system or phone-number]\n", zProgram);
  fprintf (stderr,
	   " -a,-p,--port port: Use named port\n");
  fprintf (stderr,
	   " -l,--line line: Use named device (e.g. tty0)\n");
  fprintf (stderr,
	   " -s,--speed,--baud speed, -#: Use given speed\n");
  fprintf (stderr,
	   " -c,--phone phone: Phone number to call\n");
  fprintf (stderr,
	   " -z,--system system: System to call\n");
  fprintf (stderr,
	   " -e: Set even parity\n");
  fprintf (stderr,
	   " -o: Set odd parity\n");
  fprintf (stderr,
	   " --parity={odd,even}: Set parity\n");
  fprintf (stderr,
	   " -h,--halfduplex: Echo locally\n");
  fprintf (stderr,
	   " -t,--mapcr: Map carriage return to carriage return/linefeed\n");
  fprintf (stderr,
	   " -n,--prompt: Prompt for phone number\n");
  fprintf (stderr,
	   " -d: Set maximum debugging level\n");
  fprintf (stderr,
	   " -x,--debug debug: Set debugging type\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I,--config file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  fprintf (stderr,
	   " -v,--version: Print version and exit\n");
  fprintf (stderr,
	   " --help: Print help and exit\n");
}

/* This function is called when a fatal error occurs.  */

static void
ucuabort ()
{
  if (fCustarted)
    {
      fCustarted = FALSE;
      (void) fsysdep_cu_finish ();
    }

  if (fCurestore_terminal)
    {
      fCurestore_terminal = FALSE;
      (void) fsysdep_terminal_restore ();
    }

  if (qCuconn != NULL)
    {
      struct sconnection *qconn;

      if (fCuclose_conn)
	{
	  fCuclose_conn = FALSE;
	  (void) fconn_close (qCuconn, pCuuuconf, qCudialer, FALSE);
	}
      qconn = qCuconn;
      qCuconn = NULL;
      (void) fconn_unlock (qconn);
      uconn_free (qconn);
    }

  ulog_close ();

  if (fCuconnprinted)
    printf ("\n%s\n", ZDISMSG);

  usysdep_exit (FALSE);
}

/* This variable is just used to communicate between uculog_start and
   uculog_end.  */
static boolean fCulog_restore;

/* This function is called by ulog before it output anything.  We use
   it to restore the terminal, if necessary.  ulog is only called for
   errors or debugging in cu, so it's not too costly to do this.  If
   we didn't do it, then at least on Unix each line would leave the
   cursor in the same column rather than wrapping back to the start,
   since CRMOD will not be on.  */

static void
uculog_start ()
{
  if (! fCurestore_terminal)
    fCulog_restore = FALSE;
  else
    {
      fCulog_restore = TRUE;
      fCurestore_terminal = FALSE;
      if (! fsysdep_terminal_restore ())
	ucuabort ();
    }
}

/* This function is called by ulog after everything is output.  It
   sets the terminal back, if necessary.  */

static void
uculog_end ()
{
  if (fCulog_restore)
    {
      if (! fsysdep_terminal_raw (fCulocalecho))
	ucuabort ();
      fCurestore_terminal = TRUE;
    }
}

/* Check to see if this port has the desired line, to handle the -l
   option.  If it does, or if no line was specified, set up a
   connection and lock it.  */

static int
icuport_lock (qport, pinfo)
     struct uuconf_port *qport;
     pointer pinfo;
{
  struct sconninfo *q = (struct sconninfo *) pinfo;

  if (q->zline != NULL
      && ! fsysdep_port_is_line (qport, q->zline))
    return UUCONF_NOT_FOUND;

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
      qCuconn = q->qconn;
      q->flocked = TRUE;
      return UUCONF_SUCCESS;
    }
}

/* Execute a cu escape command.  Return TRUE if the connection should
   continue, or FALSE if the connection should be terminated.  */

static boolean
fcudo_cmd (puuconf, qconn, bcmd)
     pointer puuconf;
     struct sconnection *qconn;
     int bcmd;
{
  char *zline;
  char *z;
  char abescape[5];
  boolean fret;
  size_t clen;
  char abbuf[100];

  /* Some commands take a string up to the next newline character.  */
  switch (bcmd)
    {
    default:
      zline = NULL;
      break;
    case '!':
    case '$':
    case '|':
    case '+':
    case '%':
    case 'c':
    case '>':
    case '<':
    case 'p':
    case 't':
    case 's':
      {
	zline = zsysdep_terminal_line ((const char *) NULL);
	if (zline == NULL)
	  ucuabort ();
	zline[strcspn (zline, "\n")] = '\0';
      }
      break;
    }

  switch (bcmd)
    {
    default:
      if (! isprint (*zCuvar_escape))
	sprintf (abescape, "\\%03o", (unsigned int) *zCuvar_escape);
      else
	{
	  abescape[0] = *zCuvar_escape;
	  abescape[1] = '\0';
	}
      sprintf (abbuf, "[Unrecognized.  Use %s%s to send %s]",
	       abescape, abescape, abescape);
      ucuputs (abbuf);
      return TRUE;

    case '.':
      /* Hangup.  */
      return FALSE;

    case '!':
    case '$':
    case '|':
    case '+':
      /* Shell out.  */
      if (! fsysdep_cu_copy (FALSE)
	  || ! fsysdep_terminal_restore ())
	ucuabort ();
      fCurestore_terminal = FALSE;
      {
	enum tshell_cmd t;

	switch (bcmd)
	  {
	  default:
	  case '!': t = SHELL_NORMAL; break;
	  case '$': t = SHELL_STDOUT_TO_PORT; break;
	  case '|': t = SHELL_STDIN_FROM_PORT; break;
	  case '+': t = SHELL_STDIO_ON_PORT; break;
	  }
	  
	(void) fsysdep_shell (qconn, zline, t);
      }
      if (! fsysdep_cu_copy (TRUE)
	  || ! fsysdep_terminal_raw (fCulocalecho))
	ucuabort ();
      fCurestore_terminal = TRUE;
      ubuffree (zline);
      return TRUE;

    case '%':
      fret = fcudo_subcmd (puuconf, qconn, zline);
      ubuffree (zline);
      return fret;

    case '#':
      if (! fconn_break (qconn))
	ucuabort ();
      return TRUE;

    case 'c':
      (void) fsysdep_chdir (zline);
      ubuffree (zline);
      return TRUE;

    case '>':
    case '<':
    case 'p':
    case 't':
      clen = strlen (zline);
      z = zbufalc (clen + 3);
      z[0] = bcmd;
      z[1] = ' ';
      memcpy (z + 2, zline, clen + 1);
      ubuffree (zline);
      fret = fcudo_subcmd (puuconf, qconn, z);
      ubuffree (z);
      return fret;

    case 'z':
      if (! fsysdep_cu_copy (FALSE)
	  || ! fsysdep_terminal_restore ())
	ucuabort ();
      fCurestore_terminal = FALSE;
      if (! fsysdep_suspend ())
	ucuabort ();
      if (! fsysdep_cu_copy (TRUE)
	  || ! fsysdep_terminal_raw (fCulocalecho))
	ucuabort ();
      fCurestore_terminal = TRUE;
      return TRUE;
      
    case 's':
      fret = fcuset_var (puuconf, zline);
      ubuffree (zline);
      return fret;

    case 'v':
      uculist_vars ();
      return TRUE;

    case '?':
      if (! isprint (*zCuvar_escape))
	sprintf (abescape, "\\%03o", (unsigned int) *zCuvar_escape);
      else
	{
	  abescape[0] = *zCuvar_escape;
	  abescape[1] = '\0';
	}
      ucuputs ("");
      ucuputs ("[Escape sequences]");
      sprintf (abbuf,
	       "[%s. hangup]                   [%s!CMD run shell]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s$CMD stdout to remote]      [%s|CMD stdin from remote]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s+CMD stdin and stdout to remote]",
	       abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s# send break]               [%scDIR change directory]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s> send file]                [%s< receive file]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%spFROM TO send to Unix]      [%stFROM TO receive from Unix]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%ssVAR VAL set variable]      [%ssVAR set boolean]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%ss!VAR unset boolean]        [%sv list variables]",
	       abescape, abescape);
      ucuputs (abbuf);
#ifdef SIGTSTP
      sprintf (abbuf,
	       "[%sz suspend]",
	       abescape);
      ucuputs (abbuf);
#endif
      uculist_fns (abescape);
      return TRUE;
    }
}

/* List ~% functions.  */

static void
uculist_fns (zescape)
     const char *zescape;
{
  char abbuf[100];

  sprintf (abbuf,
	   "[%s%%break send break]         [%s%%cd DIR change directory]",
	   zescape, zescape);
  ucuputs (abbuf);
  sprintf (abbuf,
	   "[%s%%put FROM TO send file]    [%s%%take FROM TO receive file]",
	   zescape, zescape);
  ucuputs (abbuf);
  sprintf (abbuf,
	   "[%s%%nostop no XON/XOFF]       [%s%%stop use XON/XOFF]",
	   zescape, zescape);
  ucuputs (abbuf);
}

/* Set a variable.  */

static boolean
fcuset_var (puuconf, zline)
     pointer puuconf;
     char *zline;
{
  char *zvar, *zval;
  char *azargs[2];
  char azbool[2];
  int iuuconf;

  zvar = strtok (zline, "= \t");
  if (zvar == NULL)
    {
      ucuputs (abCuconnected);
      return TRUE;
    }

  zval = strtok ((char *) NULL, " \t");

  if (zval == NULL)
    {
      azargs[0] = zvar;
      if (azargs[0][0] != '!')
	azbool[0] = 't';
      else
	{
	  ++azargs[0];
	  azbool[0] = 'f';
	}
      azbool[1] = '\0';
      azargs[1] = azbool;
    }
  else
    {
      azargs[0] = zvar;
      azargs[1] = zval;
    }

  iuuconf = uuconf_cmd_args (puuconf, 2, azargs, asCuvars,
			     (pointer) NULL, icuunrecogvar, 0,
			     (pointer) NULL);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_ERROR, puuconf, iuuconf);

  return TRUE;
}

/* Warn about an unknown variable.  */

/*ARGSUSED*/
static int
icuunrecogvar (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  char abescape[5];

  if (! isprint (*zCuvar_escape))
    sprintf (abescape, "\\%03o", (unsigned int) *zCuvar_escape);
  else
    {
      abescape[0] = *zCuvar_escape;
      abescape[1] = '\0';
    }
  ulog (LOG_ERROR, "%s: unknown variable (%sv lists variables)",
	argv[0], abescape);
  return UUCONF_CMDTABRET_CONTINUE;
}

/* List all the variables with their values.  */

static void
uculist_vars ()
{
  const struct uuconf_cmdtab *q;
  char abbuf[100];

  ucuputs ("");
  for (q = asCuvars; q->uuconf_zcmd != NULL; q++)
    {
      switch (UUCONF_TTYPE_CMDTABTYPE (q->uuconf_itype))
	{
	case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_BOOLEAN):
	  if (*(boolean *) q->uuconf_pvar)
	    sprintf (abbuf, "%s true", q->uuconf_zcmd);
	  else
	    sprintf (abbuf, "%s false", q->uuconf_zcmd);
	  break;

	case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_INT):
	  sprintf (abbuf, "%s %d", q->uuconf_zcmd, *(int *) q->uuconf_pvar);
	  break;

	case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_LONG):
	  sprintf (abbuf, "%s %ld", q->uuconf_zcmd,
		   *(long *) q->uuconf_pvar);
	  break;

	case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_STRING):
	case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_FULLSTRING):
	  {
	    const char *z;
	    char abchar[5];
	    size_t clen;

	    sprintf (abbuf, "%s ", q->uuconf_zcmd);
	    clen = strlen (abbuf);
	    for (z = *(const char **) q->uuconf_pvar; *z != '\0'; z++)
	      {
		int cchar;

		if (! isprint (*z))
		  {
		    sprintf (abchar, "\\%03o", (unsigned int) *z);
		    cchar = 4;
		  }
		else
		  {
		    abchar[0] = *z;
		    abchar[1] = '\0';
		    cchar = 1;
		  }
		if (clen + cchar < sizeof (abbuf))
		  strcat (abbuf, abchar);
		clen += cchar;
	      }
	  }
	  break;

	default:
	  sprintf (abbuf, "%s [unprintable type]", q->uuconf_zcmd);
	  break;
	}

      ucuputs (abbuf);
    }
}

/* Subcommands.  These are commands that begin with ~%.  */

/* This variable is only used so that we can pass a non-NULL address
   in pvar.  It is never assigned to or examined.  */

static char bCutype;

/* The command table for the subcommands.  */

static int icubreak P((pointer puuconf, int argc, char **argv, pointer pvar,
		       pointer pinfo));
static int icudebug P((pointer puuconf, int argc, char **argv, pointer pvar,
		       pointer pinfo));
static int icuchdir P((pointer puuconf, int argc, char **argv, pointer pvar,
		       pointer pinfo));
static int icuput P((pointer puuconf, int argc, char **argv, pointer pvar,
		     pointer pinfo));
static int icutake P((pointer puuconf, int argc, char **argv, pointer pvar,
		      pointer pinfo));
static int icunostop P((pointer puuconf, int argc, char **argv, pointer pvar,
			pointer pinfo));

static const struct uuconf_cmdtab asCucmds[] =
{
  { "break", UUCONF_CMDTABTYPE_FN | 1, NULL, icubreak },
  { "b", UUCONF_CMDTABTYPE_FN | 1, NULL, icubreak },
  { "cd", UUCONF_CMDTABTYPE_FN | 0, NULL, icuchdir },
  { "d", UUCONF_CMDTABTYPE_FN | 1, NULL, icudebug },
  { "put", UUCONF_CMDTABTYPE_FN | 0, NULL, icuput },
  { "take", UUCONF_CMDTABTYPE_FN | 0, NULL, icutake },
  { "nostop", UUCONF_CMDTABTYPE_FN | 1, NULL, icunostop },
  { "stop", UUCONF_CMDTABTYPE_FN | 1, &bCutype, icunostop },
  { ">", UUCONF_CMDTABTYPE_FN | 0, &bCutype, icuput },
  { "<", UUCONF_CMDTABTYPE_FN | 0, &bCutype, icutake },
  { "p", UUCONF_CMDTABTYPE_FN | 0, NULL, icuput },
  { "t", UUCONF_CMDTABTYPE_FN | 0, NULL, icutake },
  { NULL, 0, NULL, NULL }
};

/* Do a subcommand.  This is called by commands beginning with ~%.  */

static boolean
fcudo_subcmd (puuconf, qconn, zline)
     pointer puuconf;
     struct sconnection *qconn;
     char *zline;
{
  char *azargs[3];
  int iarg;
  int iuuconf;

  for (iarg = 0; iarg < 3; iarg++)
    {
      azargs[iarg] = strtok (iarg == 0 ? zline : (char *) NULL, " \t\n");
      if (azargs[iarg] == NULL)
	break;
    }

  if (iarg == 0)
    {
      ucuputs (abCuconnected);
      return TRUE;
    }

  iuuconf = uuconf_cmd_args (puuconf, iarg, azargs, asCucmds,
			     (pointer) qconn, icuunrecogfn,
			     0, (pointer) NULL);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_ERROR, puuconf, iuuconf);

  return TRUE;
}

/* Warn about an unknown function.  */

/*ARGSUSED*/
static int
icuunrecogfn (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  char abescape[5];

  if (! isprint (*zCuvar_escape))
    sprintf (abescape, "\\%03o", (unsigned int) *zCuvar_escape);
  else
    {
      abescape[0] = *zCuvar_escape;
      abescape[1] = '\0';
    }
  if (argv[0][0] == '?')
    uculist_fns (abescape);
  else
    ulog (LOG_ERROR, "%s: unknown (%s%%? lists choices)",
	  argv[0], abescape);
  return UUCONF_CMDTABRET_CONTINUE;
}

/* Send a break.  */

/*ARGSUSED*/
static int
icubreak (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sconnection *qconn = (struct sconnection *) pinfo;

  if (! fconn_break (qconn))
    ucuabort ();
  return UUCONF_CMDTABRET_CONTINUE;
}

/* Change directories.  */

/*ARGSUSED*/
static int
icuchdir (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  const char *zarg;

  if (argc <= 1)
    zarg = NULL;
  else
    zarg = argv[1];
  (void) fsysdep_chdir (zarg);
  return UUCONF_CMDTABRET_CONTINUE;
}

/* Toggle debugging.  */

/*ARGSUSED*/
static int
icudebug (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
#if DEBUG > 1
  if (iDebug != 0)
    iDebug = 0;
  else
    iDebug = DEBUG_MAX;
#else
  ucuputs ("[compiled without debugging]");
#endif
  return UUCONF_CMDTABRET_CONTINUE;
}

/* Control whether the port does xon/xoff handshaking.  If pvar is not
   NULL, this is "stop"; otherwise it is "nostop".  */

/*ARGSUSED*/
static int
icunostop (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sconnection *qconn = (struct sconnection *) pinfo;

  if (! fconn_set (qconn, PARITYSETTING_DEFAULT, STRIPSETTING_DEFAULT,
		   pvar == NULL ? XONXOFF_OFF : XONXOFF_ON))
    ucuabort ();
  return UUCONF_CMDTABRET_CONTINUE;
}

/* Send a file to the remote system.  The first argument is the file
   to send.  If that argument is not present, it is prompted for.  The
   second argument is to file name to use on the remote system.  If
   that argument is not present, the basename of the local filename is
   used.  If pvar is not NULL, then this is ~>, which is used to send
   a command to a non-Unix system.  We treat is the same as ~%put,
   except that we assume the user has already entered the appropriate
   command (for ~%put, we force ``cat >to'' to the other side).  */

/*ARGSUSED*/
static int
icuput (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sconnection *qconn = (struct sconnection *) pinfo;
  char *zfrom;
  char *zto = NULL;
  char *zalc;
  openfile_t e;
  int cline;
  char *zbuf;
  size_t cbuf;

  if (argc > 1)
    zfrom = zbufcpy (argv[1]);
  else
    {
      zfrom = zsysdep_terminal_line ("File to send: ");
      if (zfrom == NULL)
	ucuabort ();
      zfrom[strcspn (zfrom, " \t\n")] = '\0';

      if (*zfrom == '\0')
	{
	  ubuffree (zfrom);
	  ucuputs (abCuconnected);
	  return UUCONF_CMDTABRET_CONTINUE;
	}
    }

  if (pvar == NULL)
    {
      if (argc > 2)
	zto = zbufcpy (argv[2]);
      else
	{
	  char *zbase;
	  char *zprompt;

	  zbase = zsysdep_base_name (zfrom);
	  if (zbase == NULL)
	    ucuabort ();

	  zprompt = zbufalc (sizeof "Remote file name []: " +
			     strlen (zbase));
	  sprintf (zprompt, "Remote file name [%s]: ", zbase);
	  zto = zsysdep_terminal_line (zprompt);
	  ubuffree (zprompt);
	  if (zto == NULL)
	    ucuabort ();

	  zto[strcspn (zto, " \t\n")] = '\0';
	  if (*zto != '\0')
	    ubuffree (zbase);
	  else
	    {
	      ubuffree (zto);
	      zto = zbase;
	    }
	}
    }

  e = esysdep_user_fopen (zfrom, TRUE, fCuvar_binary);
  if (! ffileisopen (e))
    {
      const char *zerrstr;

      if (pvar == NULL)
	ubuffree (zto);
      zerrstr = strerror (errno);
      zalc = zbufalc (strlen (zfrom) + sizeof ": " + strlen (zerrstr));
      sprintf (zalc, "%s: %s", zfrom, zerrstr);
      ubuffree (zfrom);
      ucuputs (zalc);
      ubuffree (zalc);
      ucuputs (abCuconnected);
      return UUCONF_CMDTABRET_CONTINUE;
    }

  ubuffree (zfrom);

  /* Tell the system dependent layer to stop copying data from the
     port to the terminal.  We want to read the echoes ourself.  Also
     permit the local user to generate signals.  */
  if (! fsysdep_cu_copy (FALSE)
      || ! fsysdep_terminal_signals (TRUE))
    ucuabort ();

  /* If pvar is NULL, then we are sending a file to a Unix system.  We
     send over the command "cat > TO" to prepare it to receive.  If
     pvar is not NULL, the user is assumed to have set up whatever
     action was needed to receive the file.  */
  if (pvar == NULL)
    {
      boolean fret;

      zalc = zbufalc (sizeof "cat > \n" + strlen (zto));
      sprintf (zalc, "cat > %s\n", zto);
      ubuffree (zto);
      fret = fcusend_buf (qconn, zalc, strlen (zalc));
      ubuffree (zalc);
      if (! fret)
	{
	  (void) ffileclose (e);
	  if (! fsysdep_cu_copy (TRUE)
	      || ! fsysdep_terminal_signals (FALSE))
	    ucuabort ();
	  ucuputs (abCuconnected);
	  return UUCONF_CMDTABRET_CONTINUE;
	}
    }

  cline = 0;

  zbuf = NULL;
  cbuf = 0;

  while (TRUE)
    {
      char abbuf[512];
      size_t c;

#if USE_STDIO
      if (fCuvar_binary)
#endif
	{
	  if (ffileeof (e))
	    break;
	  c = cfileread (e, abbuf, sizeof abbuf);
	  if (ffilereaderror (e, c))
	    {
	      ucuputs ("[file read error]");
	      break;
	    }
	  if (c == 0)
	    break;
	  zbuf = abbuf;
	}
#if USE_STDIO
      else
	{
	  if (getline (&zbuf, &cbuf, e) <= 0)
	    {
	      xfree ((pointer) zbuf);
	      break;
	    }
	  c = strlen (zbuf);
	}
#endif

      if (fCuvar_verbose)
	{
	  ++cline;
	  printf ("%d ", cline);
	  (void) fflush (stdout);
	}

      if (! fcusend_buf (qconn, zbuf, c))
	{
	  if (! fCuvar_binary)
	    xfree ((pointer) zbuf);
	  (void) fclose (e);
	  if (! fsysdep_cu_copy (TRUE)
	      || ! fsysdep_terminal_signals (FALSE))
	    ucuabort ();
	  ucuputs (abCuconnected);
	  return UUCONF_CMDTABRET_CONTINUE;
	}
    }

  (void) ffileclose (e);

  if (pvar == NULL)
    {
      char beof;

      beof = '\004';
      if (! fconn_write (qconn, &beof, 1))
	ucuabort ();
    }
  else
    {
      if (*zCuvar_eofwrite != '\0')
	{
	  if (! fconn_write (qconn, zCuvar_eofwrite,
			     strlen (zCuvar_eofwrite)))
	    ucuabort ();
	}
    }

  if (fCuvar_verbose)
    ucuputs ("");

  ucuputs ("[file transfer complete]");

  if (! fsysdep_cu_copy (TRUE)
      || ! fsysdep_terminal_signals (FALSE))
    ucuabort ();

  ucuputs (abCuconnected);
  return UUCONF_CMDTABRET_CONTINUE;
}

/* Get a file from the remote side.  This is ~%take, or ~t, or ~<.
   The first two are assumed to be taking the file from a Unix system,
   so we force the command "cat FROM; echo  */

/*ARGSUSED*/
static int
icutake (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sconnection *qconn = (struct sconnection *) pinfo;
  const char *zeof;
  char *zfrom, *zto, *zcmd;
  char *zalc;
  openfile_t e;
  char bcr;
  size_t ceoflen;
  char *zlook = NULL;
  size_t ceofhave;
  boolean ferr;

  if (argc > 1)
    zfrom = zbufcpy (argv[1]);
  else
    {
      zfrom = zsysdep_terminal_line ("Remote file to retreive: ");
      if (zfrom == NULL)
	ucuabort ();
      zfrom[strcspn (zfrom, " \t\n")] = '\0';
      if (*zfrom == '\0')
	{
	  ubuffree (zfrom);
	  ucuputs (abCuconnected);
	  return UUCONF_CMDTABRET_CONTINUE;
	}
    }

  if (argc > 2)
    zto = zbufcpy (argv[2]);
  else
    {
      char *zbase;
      char *zprompt;

      zbase = zsysdep_base_name (zfrom);
      if (zbase == NULL)
	ucuabort ();

      zprompt = zbufalc (sizeof "Local file name []: " + strlen (zbase));
      sprintf (zprompt, "Local file name [%s]: ", zbase);
      zto = zsysdep_terminal_line (zprompt);
      ubuffree (zprompt);
      if (zto == NULL)
	ucuabort ();

      zto[strcspn (zto, " \t\n")] = '\0';
      if (*zto != '\0')
	ubuffree (zbase);
      else
	{
	  ubuffree (zto);
	  zto = zbase;
	}
    }

  if (pvar != NULL)
    {
      zcmd = zsysdep_terminal_line ("Remote command to execute: ");
      if (zcmd == NULL)
	ucuabort ();
      zcmd[strcspn (zcmd, "\n")] = '\0';
      zeof = zCuvar_eofread;
    }
  else
    {
      zcmd = zbufalc (sizeof "cat ; echo; echo ////cuend////"
		      + strlen (zfrom));
      sprintf (zcmd, "cat %s; echo; echo ////cuend////", zfrom);
      zeof = "\n////cuend////\n";
    }

  ubuffree (zfrom);

  e = esysdep_user_fopen (zto, FALSE, fCuvar_binary);
  if (! ffileisopen (e))
    {
      const char *zerrstr;

      ubuffree (zcmd);
      zerrstr = strerror (errno);
      zalc = zbufalc (strlen (zto) + sizeof ": " + strlen (zerrstr));
      sprintf (zalc, "%s: %s\n", zto, zerrstr);
      ucuputs (zalc);
      ubuffree (zalc);
      ucuputs (abCuconnected);
      ubuffree (zto);
      return UUCONF_CMDTABRET_CONTINUE;
    }

  ubuffree (zto);

  if (! fsysdep_cu_copy (FALSE)
      || ! fsysdep_terminal_signals (TRUE))
    ucuabort ();

  if (! fconn_write (qconn, zcmd, strlen (zcmd)))
    ucuabort ();
  bcr = '\r';
  if (! fconn_write (qconn, &bcr, 1))
    ucuabort ();

  ubuffree (zcmd);

  /* Eliminated any previously echoed data to avoid confusion.  */
  iPrecstart = 0;
  iPrecend = 0;

  /* If we're dealing with a Unix system, we can reliably discard the
     command.  Otherwise, the command will probably wind up in the
     file; too bad.  */
  if (pvar == NULL)
    {
      int b;

      while ((b = breceive_char (qconn, cCuvar_timeout, TRUE)) != '\n')
	{
	  if (b == -2)
	    ucuabort ();
	  if (b < 0)
	    {
	      ucuputs ("[timed out waiting for newline]");
	      ucuputs (abCuconnected);
	      return UUCONF_CMDTABRET_CONTINUE;
	    }
	}
    }

  ceoflen = strlen (zeof);
  zlook = zbufalc (ceoflen);
  ceofhave = 0;
  ferr = FALSE;

  while (TRUE)
    {
      int b;

      if (FGOT_SIGNAL ())
	{
	  /* Make sure the signal is logged.  */
	  ulog (LOG_ERROR, (const char *) NULL);
	  ucuputs ("[file receive aborted]");
	  /* Reset the SIGINT flag so that it does not confuse us in
	     the future.  */
	  afSignal[INDEXSIG_SIGINT] = FALSE;
	  break;
	}	

      b = breceive_char (qconn, cCuvar_timeout, TRUE);
      if (b == -2)
	ucuabort ();
      if (b < 0)
	{
	  if (ceofhave > 0)
	    (void) fwrite (zlook, sizeof (char), ceofhave, e);
	  ucuputs ("[timed out]");
	  break;
	}

      if (ceoflen == 0)
	{
	  if (cfilewrite (e, &b, 1) != 1)
	    {
	      ferr = TRUE;
	      break;
	    }
	}
      else
	{
	  zlook[ceofhave] = b;
	  ++ceofhave;
	  if (ceofhave == ceoflen)
	    {
	      size_t cmove;
	      char *zmove;

	      if (memcmp (zeof, zlook, ceoflen) == 0)
		{
		  ucuputs ("[file transfer complete]");
		  break;
		}

	      if (cfilewrite (e, zlook, 1) != 1)
		{
		  ferr = TRUE;
		  break;
		}

	      zmove = zlook;
	      for (cmove = ceoflen - 1, zmove = zlook;
		   cmove > 0;
		   cmove--, zmove++)
		zmove[0] = zmove[1];

	      --ceofhave;
	    }
	}
    }

  ubuffree (zlook);

  if (! ffileclose (e))
    ferr = TRUE;
  if (ferr)
    ucuputs ("[file write error]");

  if (! fsysdep_cu_copy (TRUE)
      || ! fsysdep_terminal_signals (FALSE))
    ucuabort ();

  ucuputs (abCuconnected);

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Send a buffer to the remote system.  If fCuvar_binary is FALSE,
   each buffer passed in will be a single line; in this case we can
   check the echoed characters and kill the line if they do not match.
   This returns FALSE if an echo check fails.  If a port error
   occurrs, it calls ucuabort.  */

static boolean
fcusend_buf (qconn, zbufarg, cbufarg)
     struct sconnection *qconn;
     const char *zbufarg;
     size_t cbufarg;
{
  const char *zbuf;
  size_t cbuf;
  int ctries;
  size_t cbplen;
  char *zsendbuf;

  zbuf = zbufarg;
  cbuf = cbufarg;
  ctries = 0;

  if (fCuvar_binary)
    cbplen = strlen (zCuvar_binary_prefix);
  else
    cbplen = 1;
  zsendbuf = zbufalc (64 * (cbplen + 1));

  /* Loop while we still have characters to send.  The value of cbuf
     will be reset to cbufarg if an echo failure occurs while sending
     a line in non-binary mode.  */
  while (cbuf > 0)
    {
      int csend;
      char *zput;
      const char *zget;
      boolean fnl;
      int i;

      if (FGOT_SIGNAL ())
	{
	  /* Make sure the signal is logged.  */
	  ubuffree (zsendbuf);
	  ulog (LOG_ERROR, (const char *) NULL);
	  ucuputs ("[file send aborted]");
	  /* Reset the SIGINT flag so that it does not confuse us in
	     the future.  */
	  afSignal[INDEXSIG_SIGINT] = FALSE;
	  return FALSE;
	}

      /* Discard anything we've read from the port up to now, to avoid
	 confusing the echo checking.  */
      iPrecstart = 0;
      iPrecend = 0;

      /* Send all characters up to a newline before actually sending
	 the newline.  This makes it easier to handle the special
	 newline echo checking.  Send up to 64 characters at a time
	 before doing echo checking.  */
      if (*zbuf == '\n')
	csend = 1;
      else
	{
	  const char *znl;

	  znl = memchr (zbuf, '\n', cbuf);
	  if (znl == NULL)
	    csend = cbuf;
	  else
	    csend = znl - zbuf;
	  if (csend > 64)
	    csend = 64;
	}

      /* Translate this part of the buffer.  If we are not in binary
	 mode, we translate \n to \r, and ignore any nonprintable
	 characters.  */
      zput = zsendbuf;
      fnl = FALSE;
      for (i = 0, zget = zbuf; i < csend; i++, zget++)
	{
	  if (isprint (*zget)
	      || *zget == '\t')
	    *zput++ = *zget;
	  else if (*zget == '\n')
	    {
	      if (fCuvar_binary)
		*zput++ = '\n';
	      else
		*zput++ = '\r';
	      fnl = TRUE;
	    }
	  else if (fCuvar_binary)
	    {
	      strcpy (zput, zCuvar_binary_prefix);
	      zput += cbplen;
	      *zput++ = *zget;
	    }
	}
		
      zbuf += csend;
      cbuf -= csend;

      if (zput == zsendbuf)
	continue;

      /* Send the data over the port.  */
      if (! fsend_data (qconn, zsendbuf, (size_t) (zput - zsendbuf), TRUE))
	ucuabort ();

      /* We do echo checking if requested, unless we are in binary
	 mode.  Echo checking of a newline is different from checking
	 of normal characters; when we send a newline we look for
	 *zCuvar_echonl.  */
      if ((fCuvar_echocheck && ! fCuvar_binary)
	  || (fnl && *zCuvar_echonl != '\0'))
	{
	  long iend;

	  iend = ixsysdep_time ((long *) NULL) + (long) cCuvar_timeout;
	  for (zget = zsendbuf; zget < zput; zget++)
	    {
	      int bread;
	      int bwant;

	      if (fCuvar_binary ? *zget == '\n' : *zget == '\r')
		{
		  bwant = *zCuvar_echonl;
		  if (bwant == '\0')
		    continue;
		}
	      else
		{
		  if (! fCuvar_echocheck || ! isprint (*zget))
		    continue;
		  bwant = *zget;
		}

	      do
		{
		  if (FGOT_SIGNAL ())
		    {
		      /* Make sure the signal is logged.  */
		      ubuffree (zsendbuf);
		      ulog (LOG_ERROR, (const char *) NULL);
		      ucuputs ("[file send aborted]");
		      /* Reset the SIGINT flag so that it does not
			 confuse us in the future.  */
		      afSignal[INDEXSIG_SIGINT] = FALSE;
		      return FALSE;
		    }

		  bread = breceive_char (qconn,
					 iend - ixsysdep_time ((long *) NULL),
					 TRUE);
		  if (bread < 0)
		    {
		      if (bread == -2)
			ucuabort ();

		      /* If we timed out, and we're not in binary
			 mode, we kill the line and try sending it
			 again from the beginning.  */
		      if (! fCuvar_binary && *zCuvar_kill != '\0')
			{
			  ++ctries;
			  if (ctries < cCuvar_resend)
			    {
			      if (fCuvar_verbose)
				{
				  printf ("R ");
				  (void) fflush (stdout);
				}
			      if (! fsend_data (qconn, zCuvar_kill, 1,
						TRUE))
				ucuabort ();
			      zbuf = zbufarg;
			      cbuf = cbufarg;
			      break;
			    }
			}
		      ubuffree (zsendbuf);
		      ucuputs ("[timed out looking for echo]");
		      return FALSE;
		    }
		}
	      while (bread != *zget);

	      if (bread < 0)
		break;
	    }
	}
    }

  ubuffree (zsendbuf);

  return TRUE;
}
