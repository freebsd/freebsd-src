/* cusub.c
   System dependent routines for cu.

   Copyright (C) 1992, 1993, 1995 Ian Lance Taylor

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
const char cusub_rcsid[] = "$FreeBSD$";
#endif

#include "uudefs.h"
#include "uuconf.h"
#include "sysdep.h"
#include "system.h"
#include "cu.h"
#include "conn.h"
#include "prot.h"

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

/* Get definitions for both O_NONBLOCK and O_NDELAY.  */
#ifndef O_NDELAY
#ifdef FNDELAY
#define O_NDELAY FNDELAY
#else /* ! defined (FNDELAY) */
#define O_NDELAY 0
#endif /* ! defined (FNDELAY) */
#endif /* ! defined (O_NDELAY) */

#ifndef O_NONBLOCK
#ifdef FNBLOCK
#define O_NONBLOCK FNBLOCK
#else /* ! defined (FNBLOCK) */
#define O_NONBLOCK 0
#endif /* ! defined (FNBLOCK) */
#endif /* ! defined (O_NONBLOCK) */

#include <errno.h>

/* 4.2 systems don't define SIGUSR2.  This should work for them.  On
   systems which are missing SIGUSR1, or SIGURG, you must find two
   signals which you can safely use.  */
#ifndef SIGUSR2
#define SIGUSR2 SIGURG
#endif

/* Get definitions for EAGAIN, EWOULDBLOCK and ENODATA.  */
#ifndef EAGAIN
#ifndef EWOULDBLOCK
#define EAGAIN (-1)
#define EWOULDBLOCK (-1)
#else /* defined (EWOULDBLOCK) */
#define EAGAIN EWOULDBLOCK
#endif /* defined (EWOULDBLOCK) */
#else /* defined (EAGAIN) */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif /* ! defined (EWOULDBLOCK) */
#endif /* defined (EAGAIN) */

#ifndef ENODATA
#define ENODATA EAGAIN
#endif

/* Local variables.  */

/* The EOF character, as set by fsysdep_terminal_raw.  */
static char bSeof;

/* The SUSP character, as set by fsysdep_terminal_raw.  */
static char bStstp;

/* Local functions.  */

static const char *zsport_line P((const struct uuconf_port *qport));
static void uscu_child P((struct sconnection *qconn, int opipe));
static RETSIGTYPE uscu_child_handler P((int isig));
static RETSIGTYPE uscu_alarm P((int isig));
static int cscu_escape P((char *pbcmd, const char *zlocalname));
static RETSIGTYPE uscu_alarm_kill P((int isig));

/* Return the device name for a port, or NULL if none.  */

static const char *
zsport_line (qport)
     const struct uuconf_port *qport;
{
  const char *zline;

  if (qport == NULL)
    return NULL;

  switch (qport->uuconf_ttype)
    {
    default:
    case UUCONF_PORTTYPE_STDIN:
      return NULL;
    case UUCONF_PORTTYPE_MODEM:
      zline = qport->uuconf_u.uuconf_smodem.uuconf_zdevice;
      break;
    case UUCONF_PORTTYPE_DIRECT:
      zline = qport->uuconf_u.uuconf_sdirect.uuconf_zdevice;
      break;
    case UUCONF_PORTTYPE_TCP:
    case UUCONF_PORTTYPE_TLI:
    case UUCONF_PORTTYPE_PIPE:
      return NULL;
    }

  if (zline == NULL)
    zline = qport->uuconf_zname;
  return zline;
}

/* Check whether the user has legitimate access to a port.  */

boolean
fsysdep_port_access (qport)
     struct uuconf_port *qport;
{
  const char *zline;
  char *zfree;
  boolean fret;

  zline = zsport_line (qport);
  if (zline == NULL)
    return TRUE;

  zfree = NULL;
  if (*zline != '/')
    {
      zfree = zbufalc (sizeof "/dev/" + strlen (zline));
      sprintf (zfree, "/dev/%s", zline);
      zline = zfree;
    }

  fret = access (zline, R_OK | W_OK) == 0;
  ubuffree (zfree);
  return fret;
}

/* Return whether the given port is named by the given line.  */

boolean
fsysdep_port_is_line (qport, zline)
     struct uuconf_port *qport;
     const char *zline;
{
  const char *zpline;
  char *zfree1, *zfree2;
  boolean fret;

  zpline = zsport_line (qport);
  if (zpline == NULL)
    return FALSE;

  if (strcmp (zline, zpline) == 0)
    return TRUE;

  zfree1 = NULL;
  zfree2 = NULL;
  if (*zline != '/')
    {
      zfree1 = zbufalc (sizeof "/dev/" + strlen (zline));
      sprintf (zfree1, "/dev/%s", zline);
      zline = zfree1;
    }
  if (*zpline != '/')
    {
      zfree2 = zbufalc (sizeof "/dev/" + strlen (zpline));
      sprintf (zfree2, "/dev/%s", zpline);
      zpline = zfree2;
    }

  fret = strcmp (zline, zpline) == 0;
  ubuffree (zfree1);
  ubuffree (zfree2);
  return fret;
}

/* The cu program wants the system dependent layer to handle the
   details of copying data from the communications port to the
   terminal.  This copying need only be done while executing
   fsysdep_cu.  On Unix, however, we set up a subprocess to do it all
   the time.  This subprocess must be controllable via the
   fsysdep_cu_copy function.

   We keep a pipe open to the subprocess.  When we want it to stop we
   send it a signal, and then wait for it to write a byte to us over
   the pipe.  */

/* The subprocess pid.  */
static volatile pid_t iSchild;

/* The pipe from the subprocess.  */
static int oSpipe;

/* When we tell the child to stop, it sends this.  */
#define CHILD_STOPPED ('S')

/* When we tell the child to start, it sends this.  */
#define CHILD_STARTED ('G')

/* Initialize the subprocess, and have it start copying data.  */

boolean
fsysdep_cu_init (qconn)
     struct sconnection *qconn;
{
  int ai[2];

  /* Write out anything we may have buffered up during the chat
     script.  We do this before forking the child only to make it easy
     to move the child into a separate executable.  */
  while (iPrecend != iPrecstart)
    {
      char *z;
      int c;

      z = abPrecbuf + iPrecstart;
      if (iPrecend > iPrecstart)
	c = iPrecend - iPrecstart;
      else
	c = CRECBUFLEN - iPrecstart;

      iPrecstart = (iPrecstart + c) % CRECBUFLEN;

      while (c > 0)
	{
	  int cwrote;

	  cwrote = write (1, z, c);
	  if (cwrote <= 0)
	    {
	      if (cwrote < 0)
		ulog (LOG_ERROR, "write: %s", strerror (errno));
	      else
		ulog (LOG_ERROR, "Line disconnected");
	      return FALSE;
	    }
	  c -= cwrote;
	  z += cwrote;
	}
    }

  if (pipe (ai) < 0)
    {
      ulog (LOG_ERROR, "pipe: %s", strerror (errno));
      return FALSE;
    }

  iSchild = ixsfork ();
  if (iSchild < 0)
    {
      ulog (LOG_ERROR, "fork: %s", strerror (errno));
      return FALSE;
    }

  if (iSchild == 0)
    {
      (void) close (ai[0]);
      uscu_child (qconn, ai[1]);
      /*NOTREACHED*/
    }

  (void) close (ai[1]);

  oSpipe = ai[0];

  return TRUE;
}

/* Copy all data from the terminal to the communications port.  If we
   see an escape character following a newline character, read the
   next character and return it.  */

boolean
fsysdep_cu (qconn, pbcmd, zlocalname)
     struct sconnection *qconn;
     char *pbcmd;
     const char *zlocalname;
{
  boolean fstart;
  char b;
  int c;

  fstart = TRUE;

  while (TRUE)
    {
      if (fsysdep_catch ())
	usysdep_start_catch ();
      else
	{
	  ulog (LOG_ERROR, (const char *) NULL);
	  return FALSE;
	}

      c = read (0, &b, 1);

      usysdep_end_catch ();

      if (c <= 0)
	break;

      if (fstart && b == *zCuvar_escape && b != '\0')
	{
	  c = cscu_escape (pbcmd, zlocalname);
	  if (c <= 0)
	    break;
	  if (*pbcmd != b)
	    {
	      write (1, pbcmd, 1);

	      /* For Unix, we let the eof character be the same as
		 '.', and we let the suspend character (if any) be the
		 same as 'z'.  */
	      if (*pbcmd == bSeof)
		*pbcmd = '.';
	      if (*pbcmd == bStstp)
		*pbcmd = 'z';
	      return TRUE;
	    }
	}
      if (! fconn_write (qconn, &b, (size_t) 1))
	return FALSE;
      fstart = strchr (zCuvar_eol, b) != NULL;
    }

  if (c < 0)
    {
      if (errno != EINTR)
	ulog (LOG_ERROR, "read: %s", strerror (errno));
      else
	ulog (LOG_ERROR, (const char *) NULL);
      return FALSE;
    }

  /* I'm not sure what's best in this case.  */
  ulog (LOG_ERROR, "End of file on terminal");
  return FALSE;
}

/* A SIGALRM handler that sets fScu_alarm and optionally longjmps.  */

volatile sig_atomic_t fScu_alarm;

static RETSIGTYPE
uscu_alarm (isig)
     int isig;
{
#if ! HAVE_SIGACTION && ! HAVE_SIGVEC && ! HAVE_SIGSET
  (void) signal (isig, uscu_alarm);
#endif

  fScu_alarm = TRUE;

#if HAVE_RESTARTABLE_SYSCALLS
  if (fSjmp)
    longjmp (sSjmp_buf, 1);
#endif
}

/* We've just seen an escape character.  We print the host name,
   optionally after a 1 second delay.  We read the next character from
   the terminal and return it.  The 1 second delay on the host name is
   mostly to be fancy; it lets ~~ look smoother.  */

static int
cscu_escape (pbcmd, zlocalname)
     char *pbcmd;
     const char *zlocalname;
{
  CATCH_PROTECT int c;

  write (1, zCuvar_escape, 1);

  fScu_alarm = FALSE;
  usset_signal (SIGALRM, uscu_alarm, TRUE, (boolean *) NULL);

  if (fsysdep_catch ())
    {
      usysdep_start_catch ();
      alarm (1);
    }
      
  c = 0;

  while (TRUE)
    {
      if (fScu_alarm)
	{
	  char b;

	  fScu_alarm = FALSE;
	  b = '[';
	  write (1, &b, 1);
	  write (1, zlocalname, strlen (zlocalname));
	  b = ']';
	  write (1, &b, 1);
	}
	  
      if (c <= 0)
	c = read (0, pbcmd, 1);
      if (c >= 0 || errno != EINTR)
	{
	  usysdep_end_catch ();
	  usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
	  alarm (0);
	  return c;
	}
    }
}  

/* A SIGALRM handler which does nothing but send a signal to the child
   process and schedule another alarm.  POSIX.1 permits kill and alarm
   from a signal handler.  The reference to static data may or may not
   be permissible.  */

static volatile sig_atomic_t iSsend_sig;

static RETSIGTYPE
uscu_alarm_kill (isig)
     int isig;
{
#if ! HAVE_SIGACTION && ! HAVE_SIGVEC && ! HAVE_SIGSET
  (void) signal (isig, uscu_alarm_kill);
#endif

  (void) kill (iSchild, iSsend_sig);

  alarm (1);
}

/* Start or stop copying data from the communications port to the
   terminal.  We send a signal to the child process to tell it what to
   do.  Unfortunately, there are race conditions in the child, so we
   keep sending it a signal once a second until it responds.  We send
   SIGUSR1 to make it start copying, and SIGUSR2 to make it stop.  */

boolean
fsysdep_cu_copy (fcopy)
     boolean fcopy;
{
  int ierr;
  int c;

  usset_signal (SIGALRM, uscu_alarm_kill, TRUE, (boolean *) NULL);
  if (fcopy)
    iSsend_sig = SIGUSR1;
  else
    iSsend_sig = SIGUSR2;

  uscu_alarm_kill (SIGALRM);

  alarm (1);

  while (TRUE)
    {
      char b;

      c = read (oSpipe, &b, 1);

#if DEBUG > 1
      if (c > 0)
	DEBUG_MESSAGE1 (DEBUG_INCOMING,
			"fsysdep_cu_copy: Got '%d'", b);
#endif

      if ((c < 0 && errno != EINTR)
	  || c == 0
	  || (c > 0 && b == (fcopy ? CHILD_STARTED : CHILD_STOPPED)))
	break;

      /* If none of the above conditions were true, then we either got
	 an EINTR error, in which case we probably timed out and the
	 SIGALRM handler resent the signal, or we read the wrong
	 character, in which case we will just read again from the
	 pipe.  */
    }

  ierr = errno;

  usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
  alarm (0);

  if (c > 0)
    return TRUE;

  if (c == 0)
    ulog (LOG_ERROR, "EOF on child pipe");
  else
    ulog (LOG_ERROR, "read: %s", strerror (ierr));

  return FALSE;
}

/* Shut down cu by killing the child process.  */

boolean
fsysdep_cu_finish ()
{
  (void) close (oSpipe);

  /* We hit the child with SIGTERM, give it two seconds to die, and
     then send a SIGKILL.  */
  if (kill (iSchild, SIGTERM) < 0)
    {
      /* Don't give an error if the child has already died.  */
      if (errno != ESRCH)
	ulog (LOG_ERROR, "kill: %s", strerror (errno));
    }

  usset_signal (SIGALRM, uscu_alarm_kill, TRUE, (boolean *) NULL);
  iSsend_sig = SIGKILL;
  alarm (2);

  (void) ixswait ((unsigned long) iSchild, "child");

  usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
  alarm (0);

  return TRUE;
}

/* Code for the child process.  */

/* This signal handler just records the signal.  In this case we only
   care about which signal we received most recently.  */

static volatile sig_atomic_t iSchild_sig;

static RETSIGTYPE
uscu_child_handler (isig)
     int isig;
{
#if ! HAVE_SIGACTION && ! HAVE_SIGVEC && ! HAVE_SIGSET
  (void) signal (isig, uscu_child_handler);
#endif

  iSchild_sig = isig;

#if HAVE_RESTARTABLE_SYSCALLS
  if (fSjmp)
    longjmp (sSjmp_buf, 1);
#endif /* HAVE_RESTARTABLE_SYSCALLS */
}

/* The child process.  This copies the port to the terminal, except
   when it is stopped by a signal.  It would be reasonable to write a
   separate program for this, probably passing it the port on stdin.
   This would reduce the memory requirements, since we wouldn't need a
   second process holding all the configuration stuff, and also let it
   work reasonably on 680x0 versions of MINIX.  */

static void
uscu_child (qconn, opipe)
     struct sconnection *qconn;
     int opipe;
{
  CATCH_PROTECT int oport;
  CATCH_PROTECT boolean fstopped, fgot;
  CATCH_PROTECT int cwrite;
  CATCH_PROTECT char abbuf[1024];

  fgot = FALSE;

  /* It would be nice if we could just use fsysdep_conn_read, but that
     will log signals that we don't want logged.  There should be a
     generic way to extract the file descriptor from the port.  */
  if (qconn->qport == NULL)
    oport = 0;
  else
    {
      switch (qconn->qport->uuconf_ttype)
	{
#if DEBUG > 0
	default:
	  ulog (LOG_FATAL, "uscu_child: Can't happen");
	  oport = -1;
	  break;
#endif
	case UUCONF_PORTTYPE_PIPE:
	  /* A read of 0 on a pipe always means EOF (see below).  */
	  fgot = TRUE;
	  /* Fall through.  */
	case UUCONF_PORTTYPE_STDIN:
	  oport = ((struct ssysdep_conn *) qconn->psysdep)->ord;
	  break;
	case UUCONF_PORTTYPE_MODEM:
	case UUCONF_PORTTYPE_DIRECT:
	case UUCONF_PORTTYPE_TCP:
	case UUCONF_PORTTYPE_TLI:
	  oport = ((struct ssysdep_conn *) qconn->psysdep)->o;
	  break;
	}
    }

  /* Force the descriptor into blocking mode.  */
  (void) fcntl (oport, F_SETFL,
		fcntl (oport, F_GETFL, 0) &~ (O_NDELAY | O_NONBLOCK));

  usset_signal (SIGUSR1, uscu_child_handler, TRUE, (boolean *) NULL);
  usset_signal (SIGUSR2, uscu_child_handler, TRUE, (boolean *) NULL);
  usset_signal (SIGINT, SIG_IGN, TRUE, (boolean *) NULL);
  usset_signal (SIGQUIT, SIG_IGN, TRUE, (boolean *) NULL);
  usset_signal (SIGPIPE, SIG_DFL, TRUE, (boolean *) NULL);
  usset_signal (SIGTERM, uscu_child_handler, TRUE, (boolean *) NULL);

  fstopped = FALSE;
  iSchild_sig = 0;
  cwrite = 0;

  if (fsysdep_catch ())
    usysdep_start_catch ();

  while (TRUE)
    {
      int isig;
      int c;

      /* There is a race condition here between checking the signal
	 and receiving a new and possibly different one.  This is
	 solved by having the parent resend the signal until it gets a
	 response.  */
      isig = iSchild_sig;
      iSchild_sig = 0;
      if (isig != 0)
	{
	  char b;

	  if (isig == SIGTERM)
	    exit (EXIT_SUCCESS);

	  if (isig == SIGUSR1)
	    {
	      fstopped = FALSE;
	      b = CHILD_STARTED;
	    }
	  else
	    {
	      fstopped = TRUE;
	      b = CHILD_STOPPED;
	      cwrite = 0;
	    }

	  c = write (opipe, &b, 1);

	  /* Apparently on some systems we can get EAGAIN here.  */
	  if (c < 0 &&
	      (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENODATA))
	    c = 0;

	  if (c <= 0)
	    {
	      /* Should we give an error message here?  */
	      (void) kill (getppid (), SIGHUP);
	      exit (EXIT_FAILURE);
	    }
	}

      if (fstopped)
	pause ();
      else if (cwrite > 0)
	{
	  char *zbuf;

	  zbuf = abbuf;
	  while (cwrite > 0)
	    {
	      c = write (1, zbuf, cwrite);

	      /* Apparently on some systems we can get EAGAIN here.  */
	      if (c < 0 &&
		  (errno == EAGAIN
		   || errno == EWOULDBLOCK
		   || errno == ENODATA))
		c = 0;

	      if (c < 0 && errno == EINTR)
		break;
	      if (c <= 0)
		{
		  /* Should we give an error message here?  */
		  (void) kill (getppid (), SIGHUP);
		  exit (EXIT_FAILURE);
		}
	      cwrite -= c;
	      zbuf += c;
	    }
	}	    
      else
	{
	  /* On some systems apparently read will return 0 until
	     something has been written to the port.  We therefore
	     accept a 0 return until after we have managed to read
	     something.  Setting errno to 0 apparently avoids a
	     problem on Coherent.  */
	  errno = 0;
	  c = read (oport, abbuf, sizeof abbuf);

	  /* Apparently on some systems we can get EAGAIN here.  */
	  if (c < 0 &&
	      (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENODATA))
	    c = 0;

	  if ((c == 0 && fgot)
	      || (c < 0 && errno != EINTR))
	    {
	      /* This can be a normal way to exit, depending on just
		 how the connection is dropped.  */
	      (void) kill (getppid (), SIGHUP);
	      exit (EXIT_SUCCESS);
	    }
	  if (c > 0)
	    {
	      fgot = TRUE;
	      cwrite = c;
	    }
	}
    }
}

/* Terminal control routines.  */

/* Whether file descriptor 0 is attached to a terminal or not.  */
static boolean fSterm;

/* Whether we are doing local echoing.  */
static boolean fSlocalecho;

/* The original state of the terminal.  */
static sterminal sSterm_orig;

/* The new state of the terminal.  */
static sterminal sSterm_new;

#if ! HAVE_BSD_TTY
#ifdef SIGTSTP
/* Whether SIGTSTP is being ignored.  */
static boolean fStstp_ignored;
#endif
#endif

/* Set the terminal into raw mode.  */

boolean
fsysdep_terminal_raw (flocalecho)
     boolean flocalecho;
{
  fSlocalecho = flocalecho;

  /* This defaults may be overriden below.  */
  bSeof = '\004';
  bStstp = '\032';

  if (! fgetterminfo (0, &sSterm_orig))
    {
      fSterm = FALSE;
      return TRUE;
    }

  fSterm = TRUE;
  
  sSterm_new = sSterm_orig;

#if HAVE_BSD_TTY

  /* We use CBREAK mode rather than RAW mode, because RAW mode turns
     off all output processing, which we don't want to do.  This means
     that we have to disable the interrupt characters, which we do by
     setting them to -1.  */
  bSeof = sSterm_orig.stchars.t_eofc;

  sSterm_new.stchars.t_intrc = -1;
  sSterm_new.stchars.t_quitc = -1;
  sSterm_new.stchars.t_startc = -1;
  sSterm_new.stchars.t_stopc = -1;
  sSterm_new.stchars.t_eofc = -1;
  sSterm_new.stchars.t_brkc = -1;

  bStstp = sSterm_orig.sltchars.t_suspc;

  sSterm_new.sltchars.t_suspc = -1;
  sSterm_new.sltchars.t_dsuspc = -1;
  sSterm_new.sltchars.t_rprntc = -1;
  sSterm_new.sltchars.t_flushc = -1;
  sSterm_new.sltchars.t_werasc = -1;
  sSterm_new.sltchars.t_lnextc = -1;

  if (! flocalecho)
    {
      sSterm_new.stty.sg_flags |= (CBREAK | ANYP);
      sSterm_new.stty.sg_flags &=~ (ECHO | CRMOD | TANDEM);
    }
  else
    {
      sSterm_new.stty.sg_flags |= (CBREAK | ANYP | ECHO);
      sSterm_new.stty.sg_flags &=~ (CRMOD | TANDEM);
    }

#endif /* HAVE_BSD_TTY */

#if HAVE_SYSV_TERMIO

  bSeof = sSterm_new.c_cc[VEOF];
  if (! flocalecho)
    sSterm_new.c_lflag &=~ (ICANON | ISIG | ECHO | ECHOE | ECHOK | ECHONL);
  else
    sSterm_new.c_lflag &=~ (ICANON | ISIG);
  sSterm_new.c_iflag &=~ (INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
  sSterm_new.c_oflag &=~ (OPOST);
  sSterm_new.c_cc[VMIN] = 1;
  sSterm_new.c_cc[VTIME] = 0;

#endif /* HAVE_SYSV_TERMIO */

#if HAVE_POSIX_TERMIOS

  bSeof = sSterm_new.c_cc[VEOF];
  bStstp = sSterm_new.c_cc[VSUSP];
  if (! flocalecho)
    sSterm_new.c_lflag &=~
      (ICANON | IEXTEN | ISIG | ECHO | ECHOE | ECHOK | ECHONL);
  else
    sSterm_new.c_lflag &=~ (ICANON | IEXTEN | ISIG);
  sSterm_new.c_iflag &=~ (INLCR | IGNCR | ICRNL | IXON | IXOFF);
  sSterm_new.c_oflag &=~ (OPOST);
  sSterm_new.c_cc[VMIN] = 1;
  sSterm_new.c_cc[VTIME] = 0;

#endif /* HAVE_POSIX_TERMIOS */

  if (! fsetterminfo (0, &sSterm_new))
    {
      ulog (LOG_ERROR, "Can't set terminal settings: %s", strerror (errno));
      return FALSE;
    }

  return TRUE;
}

/* Restore the terminal to its original setting.  */

boolean
fsysdep_terminal_restore ()
{
  if (! fSterm)
    return TRUE;

  if (! fsetterminfo (0, &sSterm_orig))
    {
      ulog (LOG_ERROR, "Can't restore terminal: %s", strerror (errno));
      return FALSE;
    }
  return TRUE;
}

/* Read a line from the terminal.  This will be called after
   fsysdep_terminal_raw has been called.  */

char *
zsysdep_terminal_line (zprompt)
     const char *zprompt;
{
  CATCH_PROTECT size_t cbuf = 0;
  CATCH_PROTECT char *zbuf = NULL;
  CATCH_PROTECT size_t cgot = 0;

  if (zprompt != NULL && *zprompt != '\0')
    (void) write (1, zprompt, strlen (zprompt));

  /* Forgot about any previous SIGINT or SIGQUIT signals we may have
     received.  We don't worry about the race condition here, since we
     can't get these signals from the terminal at the moment and it's
     not too likely that somebody else will be sending them to us.  */
  afSignal[INDEXSIG_SIGINT] = 0;
  afSignal[INDEXSIG_SIGQUIT] = 0;

  if (! fsysdep_terminal_restore ())
    return NULL;

  if (fsysdep_catch ())
    {
      usysdep_start_catch ();
      cbuf = 0;
      zbuf = NULL;
      cgot = 0;
    }

  while (TRUE)
    {
      char b;
      int c;

      if (afSignal[INDEXSIG_SIGINT]
	  || afSignal[INDEXSIG_SIGQUIT])
	{
	  usysdep_end_catch ();
	  /* Make sure the signal is logged.  */
	  ulog (LOG_ERROR, (const char *) NULL);
	  /* Return an empty string.  */
	  cgot = 0;
	  break;
	}

      /* There's a race here between checking the signals and calling
	 read.  It just means that the user will have to hit ^C more
	 than once.  */

      c = read (0, &b, 1);
      if (c < 0)
	{
	  if (errno == EINTR)
	    continue;
	  usysdep_end_catch ();
	  ulog (LOG_ERROR, "read: %s", strerror (errno));
	  (void) fsysdep_terminal_raw (fSlocalecho);
	  return NULL;
	}
      if (c == 0)
	{
	  /* I'm not quite sure what to do here.  */
	  usysdep_end_catch ();
	  ulog (LOG_ERROR, "EOF on terminal");
	  (void) fsysdep_terminal_raw (fSlocalecho);
	  return NULL;
	}

      if (cgot >= cbuf)
	{
	  char *znew;

	  cbuf += 64;
	  znew = zbufalc (cbuf);
	  if (zbuf != NULL)
	    {
	      memcpy (znew, zbuf, cgot);
	      ubuffree (zbuf);
	    }
	  zbuf = znew;
	}

      zbuf[cgot] = b;

      ++cgot;

      if (b == '\n')
	{
	  usysdep_end_catch ();
	  break;
	}
    }

  if (cgot >= cbuf)
    {
      char *znew;

      ++cbuf;
      znew = zbufalc (cbuf);
      if (zbuf != NULL)
	{
	  memcpy (znew, zbuf, cgot);
	  ubuffree (zbuf);
	}
      zbuf = znew;
    }

  zbuf[cgot] = '\0';

  if (! fsysdep_terminal_raw (fSlocalecho))
    return NULL;

  return zbuf;
}

/* Write a line to the terminal with a trailing newline.  */

boolean
fsysdep_terminal_puts (zline)
     const char *zline;
{
  char *zalc, *zprint;
  size_t clen;

  if (zline == NULL)
    {
      zalc = zbufalc (2);
      clen = 0;
    }
  else
    {
      clen = strlen (zline);
      zalc = zbufalc (clen + 2);
      memcpy (zalc, zline, clen);
    }

  if (fSterm)
    {
      zalc[clen] = '\r';
      ++clen;
    }
  zalc[clen] = '\n';
  ++clen;

  zprint = zalc;
  while (clen > 0)
    {
      int c;

      c = write (1, zprint, clen);
      if (c <= 0)
	{
	  ubuffree (zalc);
	  ulog (LOG_ERROR, "write: %s", strerror (errno));
	  return FALSE;
	}
      clen -= c;
      zprint += c;
    }

  ubuffree (zalc);

  return TRUE;
}

/* Allow or disallow signals from the terminal.  */

boolean
fsysdep_terminal_signals (faccept)
     boolean faccept;
{
#if HAVE_BSD_TTY

  if (faccept)
    {
      sSterm_new.stchars.t_intrc = sSterm_orig.stchars.t_intrc;
      sSterm_new.stchars.t_quitc = sSterm_orig.stchars.t_quitc;
    }
  else
    {
      sSterm_new.stchars.t_intrc = -1;
      sSterm_new.stchars.t_quitc = -1;
    }

#else /* ! HAVE_BSD_TTY */

  if (faccept)
    sSterm_new.c_lflag |= ISIG;
  else
    sSterm_new.c_lflag &=~ ISIG;

#ifdef SIGTSTP
  /* We only want to get SIGINT and SIGQUIT, not SIGTSTP.  This
     function will be called with faccept TRUE before it is called
     with faccept FALSE, so fStstp_ignored will be correctly
     initialized.  */
  if (faccept)
    usset_signal (SIGTSTP, SIG_IGN, FALSE, &fStstp_ignored);
  else if (! fStstp_ignored)
    usset_signal (SIGTSTP, SIG_DFL, TRUE, (boolean *) NULL);
#endif

#endif /* ! HAVE_BSD_TTY */

  if (! fsetterminfo (0, &sSterm_new))
    {
      ulog (LOG_ERROR, "Can't set terminal: %s", strerror (errno));
      return FALSE;
    }

  return TRUE;
}

/* Start up a command, or possibly just a shell.  Optionally attach
   stdin or stdout to the port.  We attach directly to the port,
   rather than copying the data ourselves.  */

boolean
fsysdep_shell (qconn, zcmd, tcmd)
     struct sconnection *qconn;
     const char *zcmd;
     enum tshell_cmd tcmd;
{
  const char *azargs[4];
  int oread, owrite;
  int aidescs[3];
  pid_t ipid;

  if (tcmd != SHELL_NORMAL)
    azargs[0] = "/bin/sh";
  else
    {
      azargs[0] = getenv ("SHELL");
      if (azargs[0] == NULL)
	azargs[0] = "/bin/sh";
    }
  if (zcmd == NULL || *zcmd == '\0')
    azargs[1] = NULL;
  else
    {
      azargs[1] = "-c";
      azargs[2] = zcmd;
      azargs[3] = NULL;
    }

  if (qconn->qport == NULL)
    {
      oread = 0;
      owrite = 1;
    }
  else
    {
      switch (qconn->qport->uuconf_ttype)
	{
	default:
	  oread = owrite = -1;
	  break;
	case UUCONF_PORTTYPE_STDIN:
	case UUCONF_PORTTYPE_PIPE:
	  oread = ((struct ssysdep_conn *) qconn->psysdep)->ord;
	  owrite = ((struct ssysdep_conn *) qconn->psysdep)->owr;
	  break;
	case UUCONF_PORTTYPE_MODEM:
	case UUCONF_PORTTYPE_DIRECT:
	case UUCONF_PORTTYPE_TCP:
	case UUCONF_PORTTYPE_TLI:
	  oread = owrite = ((struct ssysdep_conn *) qconn->psysdep)->o;
	  break;
	}
    }

  aidescs[0] = 0;
  aidescs[1] = 1;
  aidescs[2] = 2;

  if (tcmd == SHELL_STDIN_FROM_PORT || tcmd == SHELL_STDIO_ON_PORT)
    aidescs[0] = oread;
  if (tcmd == SHELL_STDOUT_TO_PORT || tcmd == SHELL_STDIO_ON_PORT)
    aidescs[1] = owrite;
    
  ipid = ixsspawn (azargs, aidescs, FALSE, TRUE, (const char *) NULL,
		   FALSE, FALSE, (const char *) NULL,
		   (const char *) NULL, (const char *) NULL);
  if (ipid < 0)
    {
      ulog (LOG_ERROR, "ixsspawn (/bin/sh): %s", strerror (errno));
      return FALSE;
    }

  return ixswait ((unsigned long) ipid, "shell") == 0;
}

/* Change directories.  */

boolean
fsysdep_chdir (zdir)
     const char *zdir;
{
  if (zdir == NULL || *zdir == '\0')
    {
      zdir = getenv ("HOME");
      if (zdir == NULL)
	{
	  ulog (LOG_ERROR, "HOME not defined");
	  return FALSE;
	}
    }
  if (chdir (zdir) < 0)
    {
      ulog (LOG_ERROR, "chdir (%s): %s", zdir, strerror (errno));
      return FALSE;
    }
  return TRUE;
}

/* Suspend the current process.  */

boolean
fsysdep_suspend ()
{
#ifndef SIGTSTP
  return fsysdep_terminal_puts ("[process suspension not supported]");
#else
  return kill (getpid (), SIGTSTP) == 0;
#endif
}
