/* serial.c
   The serial port communication routines for Unix.

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
const char serial_rcsid[] = "$Id: serial.c,v 1.4 1994/05/07 18:11:09 ache Exp $";
#endif

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "conn.h"
#include "sysdep.h"

#include <errno.h>
#include <ctype.h>

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_TLI
#if HAVE_TIUSER_H
#include <tiuser.h>
#else /* ! HAVE_TIUSER_H */
#if HAVE_XTI_H
#include <xti.h>
#endif /* HAVE_XTI_H */
#endif /* ! HAVE_TIUSER_H */
#endif /* HAVE_TLI */

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#if HAVE_SYS_IOCTL_H || HAVE_TXADDCD
#include <sys/ioctl.h>
#endif

#if HAVE_BSD_TTY
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif

#if HAVE_TIME_H
#if ! HAVE_SYS_TIME_H || ! HAVE_BSD_TTY || TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif

#if HAVE_STRIP_BUG && HAVE_BSD_TTY
#include <termio.h>
#endif

#if HAVE_SVR4_LOCKFILES
/* Get the right definitions for major and minor.  */
#if MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#endif /* MAJOR_IN_MKDEV */
#if MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif /* MAJOR_IN_SYSMACROS */
#if ! MAJOR_IN_MKDEV && ! MAJOR_IN_SYSMACROS
#ifndef major
#define major(i) (((i) >> 8) & 0xff)
#endif
#ifndef minor
#define minor(i) ((i) & 0xff)
#endif
#endif /* ! MAJOR_IN_MKDEV && ! MAJOR_IN_SYSMACROS */
#endif /* HAVE_SVR4_LOCKFILES */

#if HAVE_DEV_INFO
#include <sys/dev.h>
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

#if O_NDELAY == 0 && O_NONBLOCK == 0
 #error No way to do nonblocking I/O
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

/* Make sure we have a definition for MAX_INPUT.  */
#ifndef MAX_INPUT
#define MAX_INPUT (256)
#endif

/* If we have the TIOCSINUSE ioctl call, we use it to lock a terminal.
   Otherwise, if we have the TIOCEXCL ioctl call, we have to open the
   terminal before we know that it is unlocked.  */
#ifdef TIOCSINUSE
#define HAVE_TIOCSINUSE 1
#else
#ifdef TIOCEXCL
#define HAVE_TIOCEXCL 1
#endif
#endif

#if HAVE_TLI
extern int t_errno;
extern char *t_errlist[];
extern int t_nerr;
#endif

/* Determine bits to clear for the various terminal control fields for
   HAVE_SYSV_TERMIO and HAVE_POSIX_TERMIOS.  */

/* These fields are defined on some systems, and I am told that it
   does not hurt to clear them, and it sometimes helps.  */
#ifndef IMAXBEL
#define IMAXBEL 0
#endif

#ifndef PENDIN
#define PENDIN 0
#endif

#if HAVE_SYSV_TERMIO
#define ICLEAR_IFLAG (IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK \
		      | ISTRIP | INLCR | IGNCR | ICRNL | IUCLC \
		      | IXON | IXANY | IXOFF | IMAXBEL)
#define ICLEAR_OFLAG (OPOST | OLCUC | ONLCR | OCRNL | ONOCR | ONLRET \
		      | OFILL | OFDEL | NLDLY | CRDLY | TABDLY | BSDLY \
		      | VTDLY | FFDLY)
#define ICLEAR_CFLAG (CBAUD | CSIZE | PARENB | PARODD)
#define ISET_CFLAG (CS8 | CREAD | HUPCL)
#define ICLEAR_LFLAG (ISIG | ICANON | XCASE | ECHO | ECHOE | ECHOK \
		      | ECHONL | NOFLSH | PENDIN)
#endif
#if HAVE_POSIX_TERMIOS
#define ICLEAR_IFLAG (BRKINT | ICRNL | IGNBRK | IGNCR | IGNPAR \
		      | INLCR | INPCK | ISTRIP | IXOFF | IXON \
		      | PARMRK | IMAXBEL)
#define ICLEAR_OFLAG (OPOST)
#define ICLEAR_CFLAG (CSIZE | PARENB | PARODD)
#define ISET_CFLAG (CS8 | CREAD | HUPCL)
#define ICLEAR_LFLAG (ECHO | ECHOE | ECHOK | ECHONL | ICANON | IEXTEN \
		      | ISIG | NOFLSH | TOSTOP | PENDIN)
#endif

enum tclocal_setting
{
  SET_CLOCAL,
  CLEAR_CLOCAL,
  IGNORE_CLOCAL
};

/* Local functions.  */

static RETSIGTYPE usalarm P((int isig));
static boolean fsserial_init P((struct sconnection *qconn,
				const struct sconncmds *qcmds,
				const char *zdevice));
static void usserial_free P((struct sconnection *qconn));
static boolean fsserial_lockfile P((boolean flok,
				    const struct sconnection *));
static boolean fsserial_lock P((struct sconnection *qconn,
				boolean fin));
static boolean fsserial_unlock P((struct sconnection *qconn));
static boolean fsserial_open P((struct sconnection *qconn, long ibaud,
				boolean fwait, enum tclocal_setting tlocal));
static boolean fsstdin_open P((struct sconnection *qconn, long ibaud,
			       boolean fwait));
static boolean fsmodem_open P((struct sconnection *qconn, long ibaud,
			       boolean fwait));
static boolean fsdirect_open P((struct sconnection *qconn, long ibaud,
				boolean fwait));
static boolean fsblock P((struct ssysdep_conn *q, boolean fblock));
static boolean fsserial_close P((struct ssysdep_conn *q));
static boolean fsstdin_close P((struct sconnection *qconn,
				pointer puuconf,
				struct uuconf_dialer *qdialer,
				boolean fsuccess));
static boolean fsmodem_close P((struct sconnection *qconn,
				pointer puuconf,
				struct uuconf_dialer *qdialer,
				boolean fsuccess));
static boolean fsdirect_close P((struct sconnection *qconn,
				 pointer puuconf,
				 struct uuconf_dialer *qdialer,
				 boolean fsuccess));
static boolean fsserial_break P((struct sconnection *qconn));
static boolean fsstdin_break P((struct sconnection *qconn));
static boolean fsserial_set P((struct sconnection *qconn,
			       enum tparitysetting tparity,
			       enum tstripsetting tstrip,
			       enum txonxoffsetting txonxoff));
static boolean fsstdin_set P((struct sconnection *qconn,
			       enum tparitysetting tparity,
			       enum tstripsetting tstrip,
			       enum txonxoffsetting txonxoff));
static boolean fsmodem_carrier P((struct sconnection *qconn,
				  boolean fcarrier));
static boolean fsserial_hardflow P((struct sconnection *qconn,
				    boolean fhardflow));
static boolean fsrun_chat P((int oread, int owrite, char **pzprog));
static long isserial_baud P((struct sconnection *qconn));

/* The command table for standard input ports.  */

static const struct sconncmds sstdincmds =
{
  usserial_free,
  NULL, /* pflock */
  NULL, /* pfunlock */
  fsstdin_open,
  fsstdin_close,
  NULL, /* pfdial */
  fsdouble_read,
  fsdouble_write,
  fsysdep_conn_io,
  fsstdin_break,
  fsstdin_set,
  NULL, /* pfcarrier */
  fsdouble_chat,
  isserial_baud
};

/* The command table for modem ports.  */

static const struct sconncmds smodemcmds =
{
  usserial_free,
  fsserial_lock,
  fsserial_unlock,
  fsmodem_open,
  fsmodem_close,
  fmodem_dial,
  fsysdep_conn_read,
  fsysdep_conn_write,
  fsysdep_conn_io,
  fsserial_break,
  fsserial_set,
  fsmodem_carrier,
  fsysdep_conn_chat,
  isserial_baud
};

/* The command table for direct ports.  */

static const struct sconncmds sdirectcmds =
{
  usserial_free,
  fsserial_lock,
  fsserial_unlock,
  fsdirect_open,
  fsdirect_close,
  NULL, /* pfdial */
  fsysdep_conn_read,
  fsysdep_conn_write,
  fsysdep_conn_io,
  fsserial_break,
  fsserial_set,
  NULL, /* pfcarrier */
  fsysdep_conn_chat,
  isserial_baud
};

/* If the system will let us set both O_NDELAY and O_NONBLOCK, we do
   so.  This is because some ancient drivers on some systems appear to
   look for one but not the other.  Some other systems will give an
   EINVAL error if we attempt to set both, so we use a static global
   to hold the value we want to set.  If we get EINVAL, we change the
   global and try again (if some system gives an error other than
   EINVAL, the code will have to be modified).  */
static int iSunblock = O_NDELAY | O_NONBLOCK;

/* This code handles SIGALRM.  See the discussion above
   fsysdep_conn_read.  Normally we ignore SIGALRM, but the handler
   will temporarily be set to this function, which should set fSalarm
   and then either longjmp or schedule another SIGALRM.  fSalarm is
   never referred to outside of this file, but we don't make it static
   to try to fool compilers which don't understand volatile.  */

volatile sig_atomic_t fSalarm;

static RETSIGTYPE
usalarm (isig)
     int isig;
{
#if ! HAVE_SIGACTION && ! HAVE_SIGVEC && ! HAVE_SIGSET
  (void) signal (isig, usalarm);
#endif

  fSalarm = TRUE;

#if HAVE_RESTARTABLE_SYSCALLS
  longjmp (sSjmp_buf, 1);
#else
  alarm (1);
#endif
}

/* We need a simple routine to block SIGINT, SIGQUIT, SIGTERM and
   SIGPIPE and another to restore the original state.  When these
   functions are called (in fsysdep_modem_close) SIGHUP is being
   ignored.  The routines are isblocksigs, which returns a value of
   type HELD_SIG_MASK and usunblocksigs which takes a single argument
   of type HELD_SIG_MASK.  */

#if HAVE_SIGPROCMASK

/* Use the POSIX sigprocmask call.  */

#define HELD_SIG_MASK sigset_t

static sigset_t isblocksigs P((void));

static sigset_t
isblocksigs ()
{
  sigset_t sblock, sold;

  /* These expressions need an extra set of parentheses to avoid a bug
     in SCO 3.2.2.  */
  (void) (sigemptyset (&sblock));
  (void) (sigaddset (&sblock, SIGINT));
  (void) (sigaddset (&sblock, SIGQUIT));
  (void) (sigaddset (&sblock, SIGTERM));
  (void) (sigaddset (&sblock, SIGPIPE));

  (void) sigprocmask (SIG_BLOCK, &sblock, &sold);
  return sold;
}

#define usunblocksigs(s) \
  ((void) sigprocmask (SIG_SETMASK, &(s), (sigset_t *) NULL))

#else /* ! HAVE_SIGPROCMASK */
#if HAVE_SIGBLOCK

/* Use the BSD sigblock and sigsetmask calls.  */

#define HELD_SIG_MASK int

#ifndef sigmask
#define sigmask(i) (1 << ((i) - 1))
#endif

#define isblocksigs() \
  sigblock (sigmask (SIGINT) | sigmask (SIGQUIT) \
	    | sigmask (SIGTERM) | sigmask (SIGPIPE))

#define usunblocksigs(i) ((void) sigsetmask (i))

#else /* ! HAVE_SIGBLOCK */

#if HAVE_SIGHOLD

/* Use the SVR3 sighold and sigrelse calls.  */

#define HELD_SIG_MASK int

static int isblocksigs P((void));

static int
isblocksigs ()
{
  sighold (SIGINT);
  sighold (SIGQUIT);
  sighold (SIGTERM);
  sighold (SIGPIPE);
  return 0;
}

static void usunblocksigs P((int));

/*ARGSUSED*/
static void
usunblocksigs (i)
     int i;
{
  sigrelse (SIGINT);
  sigrelse (SIGQUIT);
  sigrelse (SIGTERM);
  sigrelse (SIGPIPE);
}

#else /* ! HAVE_SIGHOLD */

/* We have no way to block signals.  This system will suffer from a
   race condition in fsysdep_modem_close.  */

#define HELD_SIG_MASK int

#define isblocksigs() 0

#define usunblocksigs(i)

#endif /* ! HAVE_SIGHOLD */
#endif /* ! HAVE_SIGBLOCK */
#endif /* ! HAVE_SIGPROCMASK */

/* Initialize a connection for use on a serial port.  */

static boolean
fsserial_init (qconn, qcmds, zdevice)
     struct sconnection *qconn;
     const struct sconncmds *qcmds;
     const char *zdevice;
{
  struct ssysdep_conn *q;

  q = (struct ssysdep_conn *) xmalloc (sizeof (struct ssysdep_conn));
  if (zdevice == NULL
      && qconn->qport != NULL
      && qconn->qport->uuconf_ttype != UUCONF_PORTTYPE_STDIN)
    zdevice = qconn->qport->uuconf_zname;
  if (zdevice == NULL)
    q->zdevice = NULL;
  else if (*zdevice == '/')
    q->zdevice = zbufcpy (zdevice);
  else
    {
      size_t clen;

      clen = strlen (zdevice);
      q->zdevice = zbufalc (sizeof "/dev/" + clen);
      memcpy (q->zdevice, "/dev/", sizeof "/dev/" - 1);
      memcpy (q->zdevice + sizeof "/dev/" - 1, zdevice, clen);
      q->zdevice[sizeof "/dev/" + clen - 1] = '\0';
    }
  q->o = -1;
  q->ord = -1;
  q->owr = -1;
  q->ftli = FALSE;
  qconn->psysdep = (pointer) q;
  qconn->qcmds = qcmds;
  return TRUE;
}

/* Initialize a connection for use on standard input.  */

boolean
fsysdep_stdin_init (qconn)
     struct sconnection *qconn;
{
  return fsserial_init (qconn, &sstdincmds, (const char *) NULL);
}

/* Initialize a connection for use on a modem port.  */

boolean
fsysdep_modem_init (qconn)
     struct sconnection *qconn;
{
  return fsserial_init (qconn, &smodemcmds,
			qconn->qport->uuconf_u.uuconf_smodem.uuconf_zdevice);
}

/* Initialize a connection for use on a direct port.  */

boolean
fsysdep_direct_init (qconn)
     struct sconnection *qconn;
{
  return fsserial_init (qconn, &sdirectcmds,
			qconn->qport->uuconf_u.uuconf_sdirect.uuconf_zdevice);
}

/* Free up a serial port.  */

static void
usserial_free (qconn)
     struct sconnection *qconn;
{
  struct ssysdep_conn *qsysdep;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  ubuffree (qsysdep->zdevice);
  xfree ((pointer) qsysdep);
  qconn->psysdep = NULL;
}

#if HAVE_SEQUENT_LOCKFILES
#define LCK_TEMPLATE "LCK..tty"
#else
#define LCK_TEMPLATE "LCK.."
#endif

/* This routine is used for both locking and unlocking.  It is the
   only routine which knows how to translate a device name into the
   name of a lock file.  If it can't figure out a name, it does
   nothing and returns TRUE.  */

static boolean
fsserial_lockfile (flok, qconn)
     boolean flok;
     const struct sconnection *qconn;
{
  struct ssysdep_conn *qsysdep;
  const char *z;
  char *zalc;
  boolean fret;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  if (qconn->qport == NULL)
    z = NULL;
  else
    z = qconn->qport->uuconf_zlockname;
  zalc = NULL;
  if (z == NULL)
    {
#if HAVE_QNX_LOCKFILES
      {
	nid_t idevice_nid;
	char abdevice_nid[13]; /* length of long, a period, and a NUL */
	size_t cdevice_nid;
	const char *zbase;
	size_t clen;

        /* If the node ID is explicitly specified as part of the
           pathname to the device, use that.  Otherwise, presume the
           device is local to the current node. */
        if (qsysdep->zdevice[0] == '/' && qsysdep->zdevice[1] == '/')
          idevice_nid = (nid_t) strtol (qsysdep->zdevice + 2,
					(char **) NULL, 10);
        else
          idevice_nid = getnid ();

        sprintf (abdevice_nid, "%ld.", (long) idevice_nid);
        cdevice_nid = strlen (abdevice_nid);

 	zbase = strrchr (qsysdep->zdevice, '/') + 1;
 	clen = strlen (zbase);

        zalc = zbufalc (sizeof LCK_TEMPLATE + cdevice_nid + clen);

	memcpy (zalc, LCK_TEMPLATE, sizeof LCK_TEMPLATE - 1);
	memcpy (zalc + sizeof LCK_TEMPLATE - 1, abdevice_nid, cdevice_nid);
	memcpy (zalc + sizeof LCK_TEMPLATE - 1 + cdevice_nid,
		zbase, clen + 1);

	z = zalc;
      }
#else /* ! HAVE_QNX_LOCKFILES */
#if ! HAVE_SVR4_LOCKFILES
      {
	const char *zbase;
	size_t clen;

	zbase = strrchr (qsysdep->zdevice, '/') + 1;
	clen = strlen (zbase);
	zalc = zbufalc (sizeof LCK_TEMPLATE + clen);
	memcpy (zalc, LCK_TEMPLATE, sizeof LCK_TEMPLATE - 1);
	memcpy (zalc + sizeof LCK_TEMPLATE - 1, zbase, clen + 1);
#if HAVE_SCO_LOCKFILES
	{
	  char *zl;

	  for (zl = zalc + sizeof LCK_TEMPLATE - 1; *zl != '\0'; zl++)
	    if (isupper (*zl))
	      *zl = tolower (*zl);
	}
#endif
	z = zalc;
      }
#else /* HAVE_SVR4_LOCKFILES */
      {
	struct stat s;

	if (stat (qsysdep->zdevice, &s) != 0)
	  {
	    ulog (LOG_ERROR, "stat (%s): %s", qsysdep->zdevice,
		  strerror (errno));
	    return FALSE;
	  }
	zalc = zbufalc (sizeof "LK.123.123.123");
	sprintf (zalc, "LK.%03d.%03d.%03d", major (s.st_dev),
		 major (s.st_rdev), minor (s.st_rdev));
	z = zalc;
      }
#endif /* HAVE_SVR4_LOCKFILES */
#endif /* ! HAVE_QNX_LOCKFILES */
    }

  if (flok)
    fret = fsdo_lock (z, FALSE, (boolean *) NULL);
  else
    fret = fsdo_unlock (z, FALSE);

#if HAVE_COHERENT_LOCKFILES
  if (fret)
    {
      if (flok)
	{
	  if (lockttyexist (z + sizeof LCK_TEMPLATE - 1))
	    {
	      ulog (LOG_NORMAL, "%s: port already locked",
		    z + sizeof LCK_TEMPLATE - 1);
	      fret = FALSE;
	    }
	  else
	    fret = fscoherent_disable_tty (z + sizeof LCK_TEMPLATE - 1,
					   &qsysdep->zenable);
	}
      else
	{
	  fret = TRUE;
	  if (qsysdep->zenable != NULL)
	    {
	      const char *azargs[3];
	      int aidescs[3];
	      pid_t ipid;

	      azargs[0] = "/etc/enable";
	      azargs[1] = qsysdep->zenable;
	      azargs[2] = NULL;
	      aidescs[0] = SPAWN_NULL;
	      aidescs[1] = SPAWN_NULL;
	      aidescs[2] = SPAWN_NULL;

	      ipid = ixsspawn (azargs, aidescs, TRUE, FALSE,
			       (const char *) NULL, TRUE, TRUE,
			       (const char *) NULL, (const char *) NULL,
			       (const char *) NULL);
	      if (ipid < 0)
		{
		  ulog (LOG_ERROR, "ixsspawn (/etc/enable %s): %s",
			qsysdep->zenable, strerror (errno));
		  fret = FALSE;
		}
	      else
		{
		  if (ixswait ((unsigned long) ipid, (const char *) NULL)
		      == 0)
		    fret = TRUE;
		  else
		    fret = FALSE;
		}
	      ubuffree (qsysdep->zenable);
	      qsysdep->zenable = NULL;
	    }
	}
    }
#endif /* HAVE_COHERENT_LOCKFILES */

  ubuffree (zalc);
  return fret;
}

/* If we can mark a modem line in use, then when we lock a port we
   must open it and mark it in use.  We can't wait until the actual
   open because we can't fail out if it is locked then.  */

static boolean
fsserial_lock (qconn, fin)
     struct sconnection *qconn;
     boolean fin;
{
  if (! fsserial_lockfile (TRUE, qconn))
    return FALSE;

#if HAVE_TIOCSINUSE || HAVE_TIOCEXCL || HAVE_DEV_INFO
  /* Open the line and try to mark it in use.  */
  {
    struct ssysdep_conn *qsysdep;
    int iflag;

    qsysdep = (struct ssysdep_conn *) qconn->psysdep;

    if (fin)
      iflag = 0;
    else
      iflag = iSunblock;

    qsysdep->o = open (qsysdep->zdevice, O_RDWR | iflag);
    if (qsysdep->o < 0)
      {
#if O_NONBLOCK != 0
	if (! fin && iSunblock != O_NONBLOCK && errno == EINVAL)
	  {
	    iSunblock = O_NONBLOCK;
	    qsysdep->o = open (qsysdep->zdevice,
			       O_RDWR | O_NONBLOCK);
	  }
#endif
	if (qsysdep->o < 0)
	  {
	    if (errno != EBUSY)
	      ulog (LOG_ERROR, "open (%s): %s", qsysdep->zdevice,
		    strerror (errno));
	    (void) fsserial_lockfile (FALSE, qconn);
	    return FALSE;
	  }
      }

#if HAVE_TIOCSINUSE
    /* If we can't mark it in use, return FALSE to indicate that the
       lock failed.  */
    if (ioctl (qsysdep->o, TIOCSINUSE, 0) < 0)
      {
	if (errno != EALREADY)
	  ulog (LOG_ERROR, "ioctl (TIOCSINUSE): %s", strerror (errno));
#ifdef TIOCNOTTY
	(void) ioctl (qsysdep->o, TIOCNOTTY, (char *) NULL);
#endif
	(void) close (qsysdep->o);
	qsysdep->o = -1;
	(void) fsserial_lockfile (FALSE, qconn);
	return FALSE;
      }
#endif

#if HAVE_DEV_INFO
    /* QNX programs "lock" a serial port by simply opening it and
       checking if some other program also has the port open.  If the
       count of openers is greater than one, the program presumes the
       port is "locked" and backs off.  This isn't really "locking" of
       course, but it pretty much seems to work.  This can result in
       dropping incoming connections if an outgoing connection is
       started at exactly the same time.  It would probably be better
       to stop using the lock files at all for this case, but that
       would involve more complex changes to the code, and I'm afraid
       I would break something.  -- Joe Wells <jbw@cs.bu.edu>  */
    {
      struct _dev_info_entry sdevinfo;

      if (dev_info (qsysdep->o, &sdevinfo) == -1)
        {
          ulog (LOG_ERROR, "dev_info: %s", strerror (errno));
          sdevinfo.open_count = 2; /* force presumption of "locked" */
        }
      if (sdevinfo.open_count != 1)
        {
#ifdef TIOCNOTTY
          (void) ioctl (qsysdep->o, TIOCNOTTY, (char *) NULL);
#endif /* TIOCNOTTY */
          (void) close (qsysdep->o);
          qsysdep->o = -1;
          (void) fsserial_lockfile (FALSE, qconn);
          return FALSE;
        }
    }
#endif /* HAVE_DEV_INFO */

    if (fcntl (qsysdep->o, F_SETFD,
	       fcntl (qsysdep->o, F_GETFD, 0) | FD_CLOEXEC) < 0)
      {
	ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
#ifdef TIOCNOTTY
	(void) ioctl (qsysdep->o, TIOCNOTTY, (char *) NULL);
#endif
	(void) close (qsysdep->o);
	qsysdep->o = -1;
	(void) fsserial_lockfile (FALSE, qconn);
	return FALSE;
      }
  }
#endif /* HAVE_TIOCSINUSE || HAVE_TIOCEXCL */

  return TRUE;
}

/* Unlock a modem or direct port.  */

static boolean
fsserial_unlock (qconn)
     struct sconnection *qconn;
{
  boolean fret;
  struct ssysdep_conn *qsysdep;

  fret = TRUE;

  /* The file may have been opened by fsserial_lock, so close it here
     if necessary.  */
  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  if (qsysdep->o >= 0)
    {
#ifdef TIOCNOTTY
      (void) ioctl (qsysdep->o, TIOCNOTTY, (char *) NULL);
#endif
      if (close (qsysdep->o) < 0)
	{
	  ulog (LOG_ERROR, "close: %s", strerror (errno));
	  fret = FALSE;
	}
      qsysdep->o = -1;
    }
    
  if (! fsserial_lockfile (FALSE, qconn))
    fret = FALSE;

  return fret;
}

/* A table to map baud rates into index numbers.  */

#if HAVE_POSIX_TERMIOS
typedef speed_t baud_code;
#else
typedef int baud_code;
#endif

static struct sbaud_table
{
  baud_code icode;
  long ibaud;
} asSbaud_table[] =
{
  { B50, 50 },
  { B75, 75 },
  { B110, 110 },
  { B134, 134 },
  { B150, 150 },
  { B200, 200 },
  { B300, 300 },
  { B600, 600 },
  { B1200, 1200 },
  { B1800, 1800 },
  { B2400, 2400 },
  { B4800, 4800 },
  { B9600, 9600 },
#ifdef B19200
  { B19200, 19200 },
#else /* ! defined (B19200) */
#ifdef EXTA
  { EXTA, 19200 },
#endif /* EXTA */
#endif /* ! defined (B19200) */
#ifdef B38400
  { B38400, 38400 },
#else /* ! defined (B38400) */
#ifdef EXTB
  { EXTB, 38400 },
#endif /* EXTB */
#endif /* ! defined (B38400) */
#ifdef B57600
  { B57600, 57600 },
#endif
#ifdef B76800
  { B76800, 76800 },
#endif
#ifdef B115200
  { B115200, 115200 },
#endif
  { B0, 0 }
};

#define CBAUD_TABLE (sizeof asSbaud_table / sizeof asSbaud_table[0])

#if HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS
/* Hold the MIN value for the terminal to avoid setting it
   unnecessarily.  */
static int cSmin;
#endif /* HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS */

/* Open a serial line.  This sets the terminal settings.  We begin in
   seven bit mode and let the protocol change if necessary.  If fwait
   is FALSE we open the terminal in non-blocking mode.  If flocal is
   TRUE we set CLOCAL on the terminal when using termio[s]; this is
   supposedly required on some versions of BSD/386.  */

static boolean
fsserial_open (qconn, ibaud, fwait, tlocal)
     struct sconnection *qconn;
     long ibaud;
     boolean fwait;
     enum tclocal_setting tlocal;
{
  struct ssysdep_conn *q;
  baud_code ib;

  q = (struct ssysdep_conn *) qconn->psysdep;

  if (q->zdevice != NULL)
    {
#if LOG_DEVICE_PREFIX
      ulog_device (q->zdevice);
#else
      const char *z;

      if (strncmp (q->zdevice, "/dev/", sizeof "/dev/" - 1) == 0)
	z = q->zdevice + sizeof "/dev/" - 1;
      else
	z = q->zdevice;
      ulog_device (z);
#endif
    }
  else
    {
      const char *zport;
      boolean fdummy;

#if DEBUG > 0
      if (qconn->qport != NULL &&
	  qconn->qport->uuconf_ttype != UUCONF_PORTTYPE_STDIN)
	ulog (LOG_FATAL, "fsserial_open: Can't happen");
#endif
      zport = zsysdep_port_name (&fdummy);
      if (zport != NULL)
	ulog_device (zport);
    }

  ib = B0;
  if (ibaud != 0)
    {
      int i;

      for (i = 0; i < CBAUD_TABLE; i++)
	if (asSbaud_table[i].ibaud == ibaud)
	  break;
      if (i >= CBAUD_TABLE)
	{
	  ulog (LOG_ERROR, "Unsupported baud rate %ld", ibaud);
	  return FALSE;
	}
      ib = asSbaud_table[i].icode;
    }

  /* The port may have already been opened by the locking routine.  */
  if (q->o < 0)
    {
      int iflag;

      if (fwait)
	iflag = 0;
      else
	iflag = iSunblock;

      q->o = open (q->zdevice, O_RDWR | iflag);
      if (q->o < 0)
	{
#if O_NONBLOCK != 0
	  if (! fwait && iSunblock != O_NONBLOCK && errno == EINVAL)
	    {
	      iSunblock = O_NONBLOCK;
	      q->o = open (q->zdevice, O_RDWR | O_NONBLOCK);
	    }
#endif
	  if (q->o < 0)
	    {
	      ulog (LOG_ERROR, "open (%s): %s", q->zdevice,
		    strerror (errno));
	      return FALSE;
	    }
	}

      if (fcntl (q->o, F_SETFD, fcntl (q->o, F_GETFD, 0) | FD_CLOEXEC) < 0)
	{
	  ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
	  return FALSE;
	}
    }

  /* Get the port flags, and make sure the ports are blocking.  */

  q->iflags = fcntl (q->o, F_GETFL, 0);
  if (q->iflags < 0)
    {
      ulog (LOG_ERROR, "fcntl: %s", strerror (errno));
      return FALSE;
    }
  q->iwr_flags = -1;

  if (! fgetterminfo (q->o, &q->sorig))
    {
      q->fterminal = FALSE;
      return TRUE;
    }

  q->fterminal = TRUE;

  q->snew = q->sorig;

#if HAVE_BSD_TTY

  q->snew.stty.sg_flags = RAW | ANYP;
  if (ibaud == 0)
    ib = q->snew.stty.sg_ospeed;
  else
    {
      q->snew.stty.sg_ispeed = ib;
      q->snew.stty.sg_ospeed = ib;
    }

  /* We don't want to receive any interrupt characters.  */
  q->snew.stchars.t_intrc = -1;
  q->snew.stchars.t_quitc = -1;
  q->snew.stchars.t_eofc = -1;
  q->snew.stchars.t_brkc = -1;
  q->snew.sltchars.t_suspc = -1;
  q->snew.sltchars.t_rprntc = -1;
  q->snew.sltchars.t_dsuspc = -1;
  q->snew.sltchars.t_flushc = -1;
  q->snew.sltchars.t_werasc = -1;
  q->snew.sltchars.t_lnextc = -1;

#ifdef NTTYDISC
  /* We want to use the ``new'' terminal driver so that we can use the
     local mode bits to control XON/XOFF.  */
  {
    int iparam;

    if (ioctl (q->o, TIOCGETD, &iparam) >= 0
	&& iparam != NTTYDISC)
      {
	iparam = NTTYDISC;
	(void) ioctl (q->o, TIOCSETD, &iparam);
      }
  }
#endif

#ifdef TIOCHPCL
  /* When the file is closed, hang up the line.  This is a safety
     measure in case the program crashes.  */
  (void) ioctl (q->o, TIOCHPCL, 0);
#endif

#ifdef TIOCFLUSH
  {
    int iparam;

    /* Flush pending input.  */
#ifdef FREAD
    iparam = FREAD;
#else
    iparam = 0;
#endif
    (void) ioctl (q->o, TIOCFLUSH, &iparam);
  }
#endif /* TIOCFLUSH */

#endif /* HAVE_BSD_TTY */

#if HAVE_SYSV_TERMIO

  if (ibaud == 0)
    ib = q->snew.c_cflag & CBAUD;

  q->snew.c_iflag &=~ ICLEAR_IFLAG;
  q->snew.c_oflag &=~ ICLEAR_OFLAG;
  q->snew.c_cflag &=~ ICLEAR_CFLAG;
  q->snew.c_cflag |= ib | ISET_CFLAG;
  q->snew.c_lflag &=~ ICLEAR_LFLAG;
  cSmin = 1;
  q->snew.c_cc[VMIN] = cSmin;
  q->snew.c_cc[VTIME] = 1;

#ifdef TCFLSH
  /* Flush pending input.  */
  (void) ioctl (q->o, TCFLSH, 0);
#endif

#endif /* HAVE_SYSV_TERMIO */

#if HAVE_POSIX_TERMIOS

  if (ibaud == 0)
    ib = cfgetospeed (&q->snew);

  q->snew.c_iflag &=~ ICLEAR_IFLAG;
  q->snew.c_oflag &=~ ICLEAR_OFLAG;
  q->snew.c_cflag &=~ ICLEAR_CFLAG;
  q->snew.c_cflag |= ISET_CFLAG;
  q->snew.c_lflag &=~ ICLEAR_LFLAG;
  cSmin = 1;
  q->snew.c_cc[VMIN] = cSmin;
  q->snew.c_cc[VTIME] = 1;

  (void) cfsetospeed (&q->snew, ib);
  (void) cfsetispeed (&q->snew, ib);

  /* Flush pending input.  */
  (void) tcflush (q->o, TCIFLUSH);

#endif /* HAVE_POSIX_TERMIOS */

#if HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS
  switch (tlocal)
    {
    case SET_CLOCAL:
      q->snew.c_cflag |= CLOCAL;
      break;
    case CLEAR_CLOCAL:
      q->snew.c_cflag &=~ CLOCAL;
      break;
    case IGNORE_CLOCAL:
      break;
    }
#endif /* HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS */

  if (! fsetterminfo (q->o, &q->snew))
    {
      ulog (LOG_ERROR, "Can't set terminal settings: %s", strerror (errno));
      return FALSE;
    }

#ifdef TIOCSCTTY
  /* On BSD 4.4, make it our controlling terminal.  */
  (void) ioctl (q->o, TIOCSCTTY, 0);
#endif

  if (ibaud != 0)
    q->ibaud = ibaud;
  else
    {
      int i;

      q->ibaud = (long) 1200;
      for (i = 0; i < CBAUD_TABLE; i++)
	{
	  if (asSbaud_table[i].icode == ib)
	    {
	      q->ibaud = asSbaud_table[i].ibaud;
	      break;
	    }
	}

      DEBUG_MESSAGE1 (DEBUG_PORT,
		      "fsserial_open: Baud rate is %ld", q->ibaud);
    }

  return TRUE;
}

/* Open a standard input port.  The code alternates q->o between
   q->ord and q->owr as appropriate.  It is always q->ord before any
   call to fsblock.  */

static boolean
fsstdin_open (qconn, ibaud, fwait)
     struct sconnection *qconn;
     long ibaud;
     boolean fwait;
{
  struct ssysdep_conn *q;

  q = (struct ssysdep_conn *) qconn->psysdep;
  q->ord = 0;
  q->owr = 1;

  q->o = q->ord;
  if (! fsserial_open (qconn, ibaud, fwait, IGNORE_CLOCAL))
    return FALSE;
  q->iwr_flags = fcntl (q->owr, F_GETFL, 0);
  if (q->iwr_flags < 0)
    {
      ulog (LOG_ERROR, "fcntl: %s", strerror (errno));
      return FALSE;
    }
  return TRUE;
}

/* Open a modem port.  */

static boolean
fsmodem_open (qconn, ibaud, fwait)
     struct sconnection *qconn;
     long ibaud;
     boolean fwait;
{
  struct uuconf_modem_port *qm;

  qm = &qconn->qport->uuconf_u.uuconf_smodem;
  if (ibaud == (long) 0)
    ibaud = qm->uuconf_ibaud;

  if (! fsserial_open (qconn, ibaud, fwait,
		       fwait ? CLEAR_CLOCAL : SET_CLOCAL))
    return FALSE;

  /* If we are waiting for carrier, then turn on hardware flow
     control.  We don't turn on hardware flow control when dialing
     out, because some modems don't assert the necessary signals until
     they see carrier.  Instead, we turn on hardware flow control in
     fsmodem_carrier.  */
  if (fwait
      && ! fsserial_hardflow (qconn, qm->uuconf_fhardflow))
    return FALSE;

  return TRUE;
}

/* Open a direct port.  */

static boolean
fsdirect_open (qconn, ibaud, fwait)
     struct sconnection *qconn;
     long ibaud;
     boolean fwait;
{
  struct uuconf_direct_port *qd;

  qd = &qconn->qport->uuconf_u.uuconf_sdirect;
  if (ibaud == (long) 0)
    ibaud = qd->uuconf_ibaud;
  if (! fsserial_open (qconn, ibaud, fwait,
		       qd->uuconf_fcarrier ? CLEAR_CLOCAL : SET_CLOCAL))
    return FALSE;

  /* Always turn on hardware flow control for a direct port when it is
     opened.  There is no other sensible time to turn it on.  */
  return fsserial_hardflow (qconn, qd->uuconf_fhardflow);
}

/* Change the blocking status of the port.  We keep track of the
   current blocking status to avoid calling fcntl unnecessarily; fcntl
   turns out to be surprisingly expensive, at least on Ultrix.  */

static boolean
fsblock (qs, fblock)
     struct ssysdep_conn *qs;
     boolean fblock;
{
  int iwant;
  int isys;

  if (fblock)
    iwant = qs->iflags &~ (O_NDELAY | O_NONBLOCK);
  else
    iwant = qs->iflags | iSunblock;

  if (iwant == qs->iflags)
    return TRUE;

  isys = fcntl (qs->o, F_SETFL, iwant);
  if (isys < 0)
    {
#if O_NONBLOCK != 0
      if (! fblock && iSunblock != O_NONBLOCK && errno == EINVAL)
	{
	  iSunblock = O_NONBLOCK;
	  iwant = qs->iflags | O_NONBLOCK;
	  isys = fcntl (qs->o, F_SETFL, iwant);
	}
#endif
      if (isys < 0)
	{
	  ulog (LOG_ERROR, "fcntl: %s", strerror (errno));
	  return FALSE;
	}
    }

  qs->iflags = iwant;

  if (qs->iwr_flags >= 0 && qs->ord != qs->owr)
    {
      if (fblock)
	iwant = qs->iwr_flags &~ (O_NDELAY | O_NONBLOCK);
      else
	iwant = qs->iwr_flags | iSunblock;

      if (fcntl (qs->owr, F_SETFL, iwant) < 0)
	{
	  /* We don't bother to fix up iSunblock here, since we
	     succeeded above.  */
	  ulog (LOG_ERROR, "fcntl: %s", strerror (errno));
	  return FALSE;
	}

      qs->iwr_flags = iwant;
    }

  return TRUE;
}

/* Close a serial port.  */

static boolean
fsserial_close (q)
     struct ssysdep_conn *q;
{
  if (q->o >= 0)
    {
      /* Use a 30 second timeout to avoid hanging while draining
	 output.  */
      if (q->fterminal)
	{
	  fSalarm = FALSE;

	  if (fsysdep_catch ())
	    {
	      usysdep_start_catch ();
	      usset_signal (SIGALRM, usalarm, TRUE, (boolean *) NULL);
	      (void) alarm (30);

	      (void) fsetterminfodrain (q->o, &q->sorig);
	    }

	  usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
	  (void) alarm (0);
	  usysdep_end_catch ();

	  /* If we timed out, use the non draining call.  Hopefully
	     this can't hang.  */
	  if (fSalarm)
	    (void) fsetterminfo (q->o, &q->sorig);
	}

#ifdef TIOCNOTTY
      /* We don't want this as our controlling terminal any more, so
	 get rid of it.  This is necessary because we don't want to
	 open /dev/tty, since that can confuse the serial port locking
	 on some computers.  */
      (void) ioctl (q->o, TIOCNOTTY, (char *) NULL);
#endif

      (void) close (q->o);
      q->o = -1;

      /* Sleep to give the terminal a chance to settle, in case we are
	 about to call out again.  */
      sleep (2);
    }

  return TRUE;
}

/* Close a stdin port.  */

/*ARGSUSED*/
static boolean
fsstdin_close (qconn, puuconf, qdialer, fsuccess)
     struct sconnection *qconn;
     pointer puuconf;
     struct uuconf_dialer *qdialer;
     boolean fsuccess;
{
  struct ssysdep_conn *qsysdep;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  (void) close (qsysdep->owr);
  (void) close (2);
  qsysdep->o = qsysdep->ord;
  return fsserial_close (qsysdep);
}

/* Close a modem port.  */

static boolean
fsmodem_close (qconn, puuconf, qdialer, fsuccess)
     struct sconnection *qconn;
     pointer puuconf;
     struct uuconf_dialer *qdialer;
     boolean fsuccess;
{
  struct ssysdep_conn *qsysdep;
  boolean fret;
  struct uuconf_dialer sdialer;
  const struct uuconf_chat *qchat;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

  fret = TRUE;

  /* Figure out the dialer so that we can run the complete or abort
     chat scripts.  */
  if (qdialer == NULL)
    {
      if (qconn->qport->uuconf_u.uuconf_smodem.uuconf_pzdialer != NULL)
	{
	  const char *zdialer;
	  int iuuconf;

	  zdialer = qconn->qport->uuconf_u.uuconf_smodem.uuconf_pzdialer[0];
	  iuuconf = uuconf_dialer_info (puuconf, zdialer, &sdialer);
	  if (iuuconf == UUCONF_SUCCESS)
	    qdialer = &sdialer;
	  else
	    {
	      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	      fret = FALSE;
	    }
	}
      else
	qdialer = qconn->qport->uuconf_u.uuconf_smodem.uuconf_qdialer;
    }

  /* Get the complete or abort chat script to use.  */
  qchat = NULL;
  if (qdialer != NULL)
    {
      if (fsuccess)
	qchat = &qdialer->uuconf_scomplete;
      else
	qchat = &qdialer->uuconf_sabort;
    }

  if (qchat != NULL
      && (qchat->uuconf_pzprogram != NULL
	  || qchat->uuconf_pzchat != NULL))
    {
      boolean fsighup_ignored;
      HELD_SIG_MASK smask;
      int i;
      sig_atomic_t afhold[INDEXSIG_COUNT];

      /* We're no longer interested in carrier.  */
      (void) fsmodem_carrier (qconn, FALSE);

      /* The port I/O routines check whether any signal has been
	 received, and abort if one has.  While we are closing down
	 the modem, we don't care if we received a signal in the past,
	 but we do care if we receive a new signal (otherwise it would
	 be difficult to kill a uucico which was closing down a
	 modem).  We never care if we get SIGHUP at this point.  So we
	 turn off SIGHUP, remember what signals we've already seen,
	 and clear our notion of what signals we've seen.  We have to
	 block the signals while we remember and clear the array,
	 since we might otherwise miss a signal which occurred between
	 the copy and the clear (old systems can't block signals; they
	 will just have to suffer the race).  */
      usset_signal (SIGHUP, SIG_IGN, FALSE, &fsighup_ignored);
      smask = isblocksigs ();
      for (i = 0; i < INDEXSIG_COUNT; i++)
	{
	  afhold[i] = afSignal[i];
	  afSignal[i] = FALSE;
	}
      usunblocksigs (smask);

      if (! fchat (qconn, puuconf, qchat, (const struct uuconf_system *) NULL,
		   (const struct uuconf_dialer *) NULL, (const char *) NULL,
		   FALSE, qconn->qport->uuconf_zname,
		   qsysdep->ibaud))
	fret = FALSE;

      /* Restore the old signal array and the SIGHUP handler.  It is
	 not necessary to block signals here, since all we are doing
	 is exactly what the signal handler itself would do if the
	 signal occurred.  */
      for (i = 0; i < INDEXSIG_COUNT; i++)
	if (afhold[i])
	  afSignal[i] = TRUE;
      if (! fsighup_ignored)
	usset_signal (SIGHUP, ussignal, TRUE, (boolean *) NULL);
    }

  if (qdialer != NULL
      && qdialer == &sdialer)
    (void) uuconf_dialer_free (puuconf, &sdialer);

#if ! HAVE_RESET_BUG
  /* Reset the terminal to make sure we drop DTR.  It should be
     dropped when we close the descriptor, but that doesn't seem to
     happen on some systems.  Use a 30 second timeout to avoid hanging
     while draining output.  */
  if (qsysdep->fterminal)
    {
#if HAVE_BSD_TTY
      qsysdep->snew.stty.sg_ispeed = B0;
      qsysdep->snew.stty.sg_ospeed = B0;
#endif
#if HAVE_SYSV_TERMIO
      qsysdep->snew.c_cflag = (qsysdep->snew.c_cflag &~ CBAUD) | B0;
#endif
#if HAVE_POSIX_TERMIOS
      (void) cfsetospeed (&qsysdep->snew, B0);
#endif

      fSalarm = FALSE;

      if (fsysdep_catch ())
	{
	  usysdep_start_catch ();
	  usset_signal (SIGALRM, usalarm, TRUE, (boolean *) NULL);
	  (void) alarm (30);

	  (void) fsetterminfodrain (qsysdep->o, &qsysdep->snew);
	}

      usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
      (void) alarm (0);
      usysdep_end_catch ();

      /* Let the port settle.  */
      sleep (2);
    }
#endif /* ! HAVE_RESET_BUG */

  if (! fsserial_close (qsysdep))
    fret = FALSE;

  return fret;
}

/* Close a direct port.  */

/*ARGSUSED*/
static boolean
fsdirect_close (qconn, puuconf, qdialer, fsuccess)
     struct sconnection *qconn;
     pointer puuconf;
     struct uuconf_dialer *qdialer;
     boolean fsuccess;
{
  return fsserial_close ((struct ssysdep_conn *) qconn->psysdep);
}

/* Begin dialing out on a modem port.  This opens the dialer device if
   there is one.  */

boolean
fsysdep_modem_begin_dial (qconn, qdial)
     struct sconnection *qconn;
     struct uuconf_dialer *qdial;
{
  struct ssysdep_conn *qsysdep;
  const char *z;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

#ifdef TIOCMODEM
  /* If we can tell the modem to obey modem control, do so.  */
  {
    int iperm;

    iperm = 0;
    (void) ioctl (qsysdep->o, TIOCMODEM, &iperm);
  }
#endif /* TIOCMODEM */

  /* If we supposed to toggle DTR, do so.  */

  if (qdial->uuconf_fdtr_toggle)
    {
#ifdef TIOCCDTR
      (void) ioctl (qsysdep->o, TIOCCDTR, 0);
      sleep (2);
      (void) ioctl (qsysdep->o, TIOCSDTR, 0);
#else /* ! defined (TIOCCDTR) */
      if (qsysdep->fterminal)
	{
	  sterminal sbaud;

	  sbaud = qsysdep->snew;

#if HAVE_BSD_TTY
	  sbaud.stty.sg_ispeed = B0;
	  sbaud.stty.sg_ospeed = B0;
#endif
#if HAVE_SYSV_TERMIO
	  sbaud.c_cflag = (sbaud.c_cflag &~ CBAUD) | B0;
#endif
#if HAVE_POSIX_TERMIOS
	  (void) cfsetospeed (&sbaud, B0);
#endif

	  (void) fsetterminfodrain (qsysdep->o, &sbaud);
	  sleep (2);
	  (void) fsetterminfo (qsysdep->o, &qsysdep->snew);
	}
#endif /* ! defined (TIOCCDTR) */

      if (qdial->uuconf_fdtr_toggle_wait)
	sleep (2);
    }

  if (! fsmodem_carrier (qconn, FALSE))
    return FALSE;

  /* Open the dial device if there is one.  */
  z = qconn->qport->uuconf_u.uuconf_smodem.uuconf_zdial_device;
  if (z != NULL)
    {
      char *zfree;
      int o;

      qsysdep->ohold = qsysdep->o;

      zfree = NULL;
      if (*z != '/')
	{
	  zfree = zbufalc (sizeof "/dev/" + strlen (z));
	  sprintf (zfree, "/dev/%s", z);
	  z = zfree;
	}

      o = open ((char *) z, O_RDWR | O_NOCTTY);
      if (o < 0)
	{
	  ulog (LOG_ERROR, "open (%s): %s", z, strerror (errno));
	  ubuffree (zfree);
	  return FALSE;
	}
      ubuffree (zfree);

      if (fcntl (o, F_SETFD, fcntl (o, F_GETFD, 0) | FD_CLOEXEC) < 0)
	{
	  ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
	  (void) close (o);
	  return FALSE;
	}

      qsysdep->o = o;
    }

  return TRUE;
}

/* Tell the port to require or not require carrier.  On BSD this uses
   TIOCCAR and TIOCNCAR, which I assume are generally supported (it
   can also use the LNOMDM bit supported by IS68K Unix).  On System V
   it resets or sets CLOCAL.  We only require carrier if the port
   supports it.  This will only be called with fcarrier TRUE if the
   dialer supports carrier.  */

static boolean
fsmodem_carrier (qconn, fcarrier)
     struct sconnection *qconn;
     boolean fcarrier;
{
  register struct ssysdep_conn *q;
  struct uuconf_modem_port *qm;

  q = (struct ssysdep_conn *) qconn->psysdep;

  if (! q->fterminal)
    return TRUE;

  qm = &qconn->qport->uuconf_u.uuconf_smodem;
  if (fcarrier)
    {
      if (qm->uuconf_fcarrier)
	{
#ifdef TIOCCAR
	  /* Tell the modem to pay attention to carrier.  */
	  if (ioctl (q->o, TIOCCAR, 0) < 0)
	    {
	      ulog (LOG_ERROR, "ioctl (TIOCCAR): %s", strerror (errno));
	      return FALSE;
	    }
#endif /* TIOCCAR */

#if HAVE_BSD_TTY
#ifdef LNOMDM
	  /* IS68K Unix uses a local LNOMDM bit.  */
	  {
	    int iparam;

	    iparam = LNOMDM;
	    if (ioctl (q->o, TIOCLBIC, &iparam) < 0)
	      {
		ulog (LOG_ERROR, "ioctl (TIOCLBIC, LNOMDM): %s",
		      strerror (errno));
		return FALSE;
	      }
	  }
#endif /* LNOMDM */
#endif /* HAVE_BSD_TTY */

#if HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS
	  /* Put the modem into nonlocal mode.  */
	  q->snew.c_cflag &=~ CLOCAL;
	  if (! fsetterminfo (q->o, &q->snew))
	    {
	      ulog (LOG_ERROR, "Can't clear CLOCAL: %s", strerror (errno));
	      return FALSE;
	    }
#endif /* HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS */
	}

      /* Turn on hardware flow control after turning on carrier.  We
	 don't do it until now because some modems don't assert the
	 right signals until they see carrier.  */
      if (! fsserial_hardflow (qconn, qm->uuconf_fhardflow))
	return FALSE;
    }
  else
    {
      /* Turn off any hardware flow control before turning off
	 carrier.  */
      if (! fsserial_hardflow (qconn, FALSE))
	return FALSE;

#ifdef TIOCNCAR
      /* Tell the modem to ignore carrier.  */ 
      if (ioctl (q->o, TIOCNCAR, 0) < 0)
	{
	  ulog (LOG_ERROR, "ioctl (TIOCNCAR): %s", strerror (errno));
	  return FALSE;
	}
#endif /* TIOCNCAR */

#if HAVE_BSD_TTY
#ifdef LNOMDM
      /* IS68K Unix uses a local LNOMDM bit.  */
      {
	int iparam;

	iparam = LNOMDM;
	if (ioctl (q->o, TIOCLBIS, &iparam) < 0)
	  {
	    ulog (LOG_ERROR, "ioctl (TIOCLBIS, LNOMDM): %s",
		  strerror (errno));
	    return FALSE;
	  }
      }
#endif /* LNOMDM */
#endif /* HAVE_BSD_TTY */

#if HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS
      /* Put the modem into local mode (ignore carrier) to start the chat
	 script.  */
      q->snew.c_cflag |= CLOCAL;
      if (! fsetterminfo (q->o, &q->snew))
	{
	  ulog (LOG_ERROR, "Can't set CLOCAL: %s", strerror (errno));
	  return FALSE;
	}
  
#if HAVE_CLOCAL_BUG
      /* On SCO and AT&T UNIX PC you have to reopen the port.  */
      {
	int onew;
 
	onew = open (q->zdevice, O_RDWR);
	if (onew < 0)
	  {
	    ulog (LOG_ERROR, "open (%s): %s", q->zdevice, strerror (errno));
	    return FALSE;
	  }
 
	if (fcntl (onew, F_SETFD,
		   fcntl (onew, F_GETFD, 0) | FD_CLOEXEC) < 0)
	  {
	    ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
	    (void) close (onew);
	    return FALSE;
	  }

	(void) close (q->o);
	q->o = onew;
      }
#endif /* HAVE_CLOCAL_BUG */

#endif /* HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS */
    }

  return TRUE;
}

/* Tell the port to use hardware flow control.  There is no standard
   mechanism for controlling this.  This implementation supports
   CRTSCTS on SunOS, RTS/CTSFLOW on 386(ish) unix, CTSCD on the 3b1,
   and TXADDCD/TXDELCD on AIX.  If you know how to do it on other
   systems, please implement it and send me the patches.  */

static boolean
fsserial_hardflow (qconn, fhardflow)
     struct sconnection *qconn;
     boolean fhardflow;
{
  register struct ssysdep_conn *q;

  q = (struct ssysdep_conn *) qconn->psysdep;

  if (! q->fterminal)
    return TRUE;

  /* Don't do anything if we don't know what to do.  */
#if HAVE_BSD_TTY
#define HAVE_HARDFLOW 0
#endif
#if HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS
#if ! HAVE_TXADDCD
#ifndef CRTSFL
#ifndef CRTSCTS
#ifndef CTSCD
#define HAVE_HARDFLOW 0
#endif
#endif
#endif
#endif
#endif

#ifndef HAVE_HARDFLOW
#define HAVE_HARDFLOW 1
#endif

#if HAVE_HARDFLOW
  if (fhardflow)
    {
#if HAVE_TXADDCD
      /* The return value does not reliably indicate whether this
	 actually succeeded.  */
      (void) ioctl (q->o, TXADDCD, "rts");
#else /* ! HAVE_TXADDCD */
#if HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS
#ifdef CRTSFL
      q->snew.c_cflag |= CRTSFL;
      q->snew.c_cflag &=~ (RTSFLOW | CTSFLOW);
#endif /* defined (CRTSFL) */
#ifdef CRTSCTS
      q->snew.c_cflag |= CRTSCTS;
#endif /* defined (CRTSCTS) */
#ifdef CTSCD
      q->snew.c_cflag |= CTSCD;
#endif /* defined (CTSCD) */
#endif /* HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS */
      if (! fsetterminfo (q->o, &q->snew))
	{
	  ulog (LOG_ERROR, "Can't enable hardware flow control: %s",
		strerror (errno));
	  return FALSE;
	}
#endif /* ! HAVE_TXADDCD */
    }
  else
    {
#if HAVE_TXADDCD
      /* The return value does not reliably indicate whether this
	 actually succeeded.  */
      (void) ioctl (q->o, TXDELCD, "rts");
#else /* ! HAVE_TXADDCD */
#if HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS
#ifdef CRTSFL
      q->snew.c_cflag &=~ CRTSFL;
      q->snew.c_cflag &=~ (RTSFLOW | CTSFLOW);
#endif /* defined (CRTSFL) */
#ifdef CRTSCTS
      q->snew.c_cflag &=~ CRTSCTS;
#endif /* defined (CRTSCTS) */
#ifdef CTSCD
      q->snew.c_cflag &=~ CTSCD;
#endif /* defined (CTSCD) */
#endif /* HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS */
      if (! fsetterminfo (q->o, &q->snew))
	{
	  ulog (LOG_ERROR, "Can't disable hardware flow control: %s",
		strerror (errno));
	  return FALSE;
	}
#endif /* ! HAVE_TXADDCD */
    }
#endif /* HAVE_HARDFLOW */

  return TRUE;
}

/* Finish dialing out on a modem by closing any dialer device and waiting
   for carrier.  */

boolean
fsysdep_modem_end_dial (qconn, qdial)
     struct sconnection *qconn;
     struct uuconf_dialer *qdial;
{
  struct ssysdep_conn *q;

  q = (struct ssysdep_conn *) qconn->psysdep;

  if (qconn->qport->uuconf_u.uuconf_smodem.uuconf_zdial_device != NULL)
    {
      (void) close (q->o);
      q->o = q->ohold;
    }

  if (qconn->qport->uuconf_u.uuconf_smodem.uuconf_fcarrier
      && qdial->uuconf_fcarrier)
    {
      /* Tell the port that we need carrier.  */
      if (! fsmodem_carrier (qconn, TRUE))
	return FALSE;

#ifdef TIOCWONLINE

      /* We know how to wait for carrier, so do so.  */

      /* If we already got a signal, just quit now.  */
      if (FGOT_QUIT_SIGNAL ())
	return FALSE;

      /* This bit of code handles signals just like fsysdep_conn_read
	 does.  See that function for a longer explanation.  */

      /* Use fsysdep_catch to handle a longjmp from the signal
	 handler.  */

      fSalarm = FALSE;

      if (fsysdep_catch ())
	{
	  /* Start catching SIGALRM; normally we ignore it.  */
	  usysdep_start_catch ();
	  usset_signal (SIGALRM, usalarm, TRUE, (boolean *) NULL);
	  (void) alarm (qdial->uuconf_ccarrier_wait);

	  /* We really don't care if we get an error, since that will
	     probably just mean that TIOCWONLINE isn't supported in
	     which case there's nothing we can do anyhow.  If we get
	     SIGINT we want to keep waiting for carrier, because
	     SIGINT just means don't start any new sessions.  We don't
	     handle SIGINT correctly if we do a longjmp in the signal
	     handler; too bad.  */
	  while (ioctl (q->o, TIOCWONLINE, 0) < 0
		 && errno == EINTR)
	    {
	      /* Log the signal.  */
	      ulog (LOG_ERROR, (const char *) NULL);
	      if (FGOT_QUIT_SIGNAL () || fSalarm)
		break;
	    }
	}

      /* Turn off the pending SIGALRM and ignore SIGALARM again.  */
      usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
      (void) alarm (0);
      usysdep_end_catch ();

      /* If we got a random signal, just return FALSE.  */
      if (FGOT_QUIT_SIGNAL ())
	return FALSE;

      /* If we timed out, give an error.  */
      if (fSalarm)
	{
	  ulog (LOG_ERROR, "Timed out waiting for carrier");
	  return FALSE;
	}

#else /* ! defined (TIOCWONLINE) */

      /* Try to open the port again without using O_NDELAY.  In
	 principle, the open should delay until carrier is available.
	 This may not work on some systems, so we just ignore any
	 errors.  */
      {
	int onew;
 
	onew = open (q->zdevice, O_RDWR);
	if (onew >= 0)
	  {
	    boolean fbad;
	    int iflags;

	    fbad = FALSE;

	    if (fcntl (onew, F_SETFD,
		       fcntl (onew, F_GETFD, 0) | FD_CLOEXEC) < 0)
	      fbad = TRUE;

	    if (! fbad)
	      {
		iflags = fcntl (onew, F_GETFL, 0);
		if (iflags < 0
		    || ! fsetterminfo (onew, &q->snew))
		  fbad = TRUE;
	      }

	    if (fbad)
	      (void) close (onew);
	    else
	      {
		(void) close (q->o);
		q->o = onew;
		q->iflags = iflags;
#if HAVE_TIOCSINUSE
		(void) ioctl (onew, TIOCSINUSE, 0);
#endif
	      }
	  }
      }

#endif /* ! defined (TIOCWONLINE) */
    }

  return TRUE; 
}

/* Read data from a connection, with a timeout.  This routine handles
   all types of connections, including TLI.

   This function should return when we have read cmin characters or
   the timeout has occurred.  We have to work a bit to get Unix to do
   this efficiently on a terminal.  The simple implementation
   schedules a SIGALRM signal and then calls read; if there is a
   single character available, the call to read will return
   immediately, so there must be a loop which terminates when the
   SIGALRM is delivered or the correct number of characters has been
   read.  This can be very inefficient with a fast CPU or a low baud
   rate (or both!), since each call to read may return only one or two
   characters.

   Under POSIX or System V, we can specify a minimum number of
   characters to read, so there is no serious trouble.

   Under BSD, we figure out how many characters we have left to read,
   how long it will take for them to arrive at the current baud rate,
   and sleep that long.

   Doing this with a timeout and avoiding all possible race conditions
   get very hairy, though.  Basically, we're going to schedule a
   SIGALRM for when the timeout expires.  I don't really want to do a
   longjmp in the SIGALRM handler, though, because that may lose data.
   Therefore, I have the signal handler set a variable.  However, this
   means that there will be a span of time between the time the code
   checks the variable and the time it calls the read system call; if
   the SIGALRM occurs during that time, the read might hang forever.
   To avoid this, the SIGALRM handler not only sets a global variable,
   it also schedules another SIGALRM for one second in the future
   (POSIX specifies that a signal handler is permitted to safely call
   alarm).  To avoid getting a continual sequence of SIGALRM
   interrupts, we change the signal handler to ignore SIGALRM when
   we're about to exit the function.  This means that every time we
   execute fsysdep_conn_read we make at least five system calls.  It's
   the best I've been able to come up with, though.

   When fsysdep_conn_read finishes, there will be no SIGALRM scheduled
   and SIGALRM will be ignored.  */

boolean
fsysdep_conn_read (qconn, zbuf, pclen, cmin, ctimeout, freport)
     struct sconnection *qconn;
     char *zbuf;
     size_t *pclen;
     size_t cmin;
     int ctimeout;
     boolean freport;
{
  CATCH_PROTECT size_t cwant;
  boolean fret;
  register struct ssysdep_conn * const q
    = (struct ssysdep_conn *) qconn->psysdep;
  int cwouldblock;

  cwant = *pclen;
  *pclen = 0;

  /* Guard against a bad timeout.  We return TRUE when a timeout
     expires.  It is possible to get a negative timeout here because
     the calling code does not check user supplied timeouts for
     plausibility.  */
  if (ctimeout <= 0)
    return TRUE;

  /* We want to do a blocking read.  */
  if (! fsblock (q, TRUE))
    return FALSE;

  fSalarm = FALSE;

  /* We're going to set up an alarm signal to last for the entire
     read.  If the read system call cannot be interrupted, the signal
     handler will do a longjmp causing fsysdep_catch (a macro) to
     return FALSE.  We handle that here.  If read can be interrupted,
     fsysdep_catch will be defined to TRUE.  */
  if (fsysdep_catch ())
    {
      /* Prepare to catch SIGALRM and schedule the signal.  */
      usysdep_start_catch ();
      usset_signal (SIGALRM, usalarm, TRUE, (boolean *) NULL);
      alarm (ctimeout);
    }
  else
    {
      /* We caught a signal.  We don't actually have to do anything,
	 as all the appropriate checks are made at the start of the
	 following loop.  */
    }

  fret = FALSE;

  cwouldblock = 0;
  while (TRUE)
    {
      int cgot;

#if HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS
      /* If we can tell the terminal not to return until we have a
	 certain number of characters, do so.  */
      if (q->fterminal)
	{
	  int csetmin;

	  /* I'm not that confident about setting MIN to values larger
	     than 127, although up to 255 would probably work.  */
	  if (cmin < 127)
	    csetmin = cmin;
	  else
	    csetmin = 127;

	  if (csetmin != cSmin)
	    {
	      q->snew.c_cc[VMIN] = csetmin;
	      while (! fsetterminfo (q->o, &q->snew))
		{
		  if (errno != EINTR
		      || FGOT_QUIT_SIGNAL ())
		    {
		      int ierr;

		      /* We turn off the signal before reporting the
			 error to minimize any problems with
			 interrupted system calls.  */
		      ierr = errno;
		      usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
		      alarm (0);
		      usysdep_end_catch ();
		      ulog (LOG_ERROR, "Can't set MIN for terminal: %s",
			    strerror (ierr));
		      return FALSE;
		    }

		  if (fSalarm)
		    {
		      ulog (LOG_ERROR,
			    "Timed out when setting MIN to %d; retrying",
			    csetmin);
		      fSalarm = FALSE;
		      alarm (ctimeout);
		    }
		}
	      cSmin = csetmin;
	    }
	}
#endif /* HAVE_SYSV_TERMIO || HAVE_POSIX_TERMIOS */

      /* If we've received a signal, get out now.  */
      if (FGOT_QUIT_SIGNAL ())
	break;

      /* If we've already gotten a SIGALRM, get out with whatever
	 we've accumulated.  */
      if (fSalarm)
	{
	  fret = TRUE;
	  break;
	}

      /* Right here is the race condition which we avoid by having the
	 SIGALRM handler schedule another SIGALRM.  */
#if HAVE_TLI
      if (q->ftli)
	{
	  int iflags;

	  cgot = t_rcv (q->o, zbuf, cwant, &iflags);
	  if (cgot < 0 && t_errno != TSYSERR)
	    {
	      usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
	      alarm (0);
	      usysdep_end_catch ();

	      if (freport)
		ulog (LOG_ERROR, "t_rcv: %s",
		      (t_errno >= 0 && t_errno < t_nerr
		       ? t_errlist[t_errno]
		       : "unknown TLI error"));

	      return FALSE;
	    }
	}
      else
#endif
	cgot = read (q->o, zbuf, cwant);

      /* If the read returned an error, check for signals.  */
      if (cgot < 0)
	{
	  if (errno == EINTR)
	    {
	      /* Log the signal.  */
	      ulog (LOG_ERROR, (const char *) NULL);
	    }
	  if (fSalarm)
	    {
	      fret = TRUE;
	      break;
	    }
	  if (FGOT_QUIT_SIGNAL ())
	    break;
	}

      /* If read returned an error, get out.  We just ignore EINTR
	 here, since it must be from some signal we don't care about.
	 If the read returned 0 then the line must have been hung up
	 (normally we would have received SIGHUP, but we can't count
	 on that).  We turn off the signals before calling ulog to
	 reduce problems with interrupted system calls.  */
      if (cgot > 0)
	cwouldblock = 0;
      else
	{
	  if (cgot < 0 && errno == EINTR)
	    cgot = 0;
	  else if (cgot < 0
		   && (errno == EAGAIN || errno == EWOULDBLOCK)
		   && cwouldblock < 2)
	    {
	      /* Incomprehensibly, on some systems the read will
		 return EWOULDBLOCK even though the descriptor has
		 been set to blocking mode.  We permit the read call
		 to do this twice in a row, and then error out.  We
		 don't want to permit an arbitrary number of
		 EWOULDBLOCK errors, since that could hang us up
		 indefinitely.  */
	      ++cwouldblock;
	      cgot = 0;
	    }
	  else
	    {
	      int ierr;

	      ierr = errno;

	      usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
	      alarm (0);
	      usysdep_end_catch ();

	      if (freport)
		{
		  if (cgot == 0)
		    ulog (LOG_ERROR, "Line disconnected");
		  else
		    ulog (LOG_ERROR, "read: %s", strerror (ierr));
		}

	      return FALSE;
	    }
	}

      cwant -= cgot;
      if (cgot >= cmin)
	cmin = 0;
      else
	cmin -= cgot;
      zbuf += cgot;
      *pclen += cgot;

      /* If we have enough data, get out now.  */
      if (cmin == 0)
	{
	  fret = TRUE;
	  break;
	}

#if HAVE_BSD_TTY
      /* We still want more data, so sleep long enough for the rest of
	 it to arrive.  We don't this for System V or POSIX because
	 setting MIN is good enough (we can't sleep longer than it
	 takes to get MAX_INPUT characters anyhow).

	 The baud rate is approximately 10 times the number of
	 characters which will arrive in one second, so the number of
	 milliseconds to sleep ==
	 characters * (milliseconds / character) ==
	 characters * (1000 * (seconds / character)) ==
	 characters * (1000 * (1 / (baud / 10))) ==
	 characters * (10000 / baud)

	 We arbitrarily reduce the sleep amount by 10 milliseconds to
	 attempt to account for the amount of time it takes to set up
	 the sleep.  This is how long it takes to get half a character
	 at 19200 baud.  We then don't bother to sleep for less than
	 10 milliseconds.  We don't sleep if the read was interrupted.

	 We use select to sleep.  It would be easy to use poll as
	 well, but it's unlikely that any system with BSD ttys would
	 have poll but not select.  Using select avoids hassles with
	 the pending SIGALRM; if it hits the select will be
	 interrupted, and otherwise the select will not affect it.  */

#if ! HAVE_SELECT
 #error This code requires select; feel free to extend it
#endif

      if (q->fterminal && cmin > 1 && cgot > 0)
	{
	  int csleepchars;
	  int isleep;

	  /* We don't try to read all the way up to MAX_INPUT,
	     since that might drop a character.  */
	  if (cmin <= MAX_INPUT - 10)
	    csleepchars = cmin;
	  else
	    csleepchars = MAX_INPUT - 10;

	  isleep = (int) (((long) csleepchars * 10000L) / q->ibaud);
	  isleep -= 10;

	  if (isleep > 10)
	    {
	      struct timeval s;

	      s.tv_sec = isleep / 1000;
	      s.tv_usec = (isleep % 1000) * 1000;

	      /* Some versions of select take a pointer to an int,
		 while some take a pointer to an fd_set.  I just cast
		 the arguments to a generic pointer, and assume that
		 any machine which distinguishes int * from fd_set *
		 (I would be amazed if there are any such machines)
		 have an appropriate prototype somewhere or other.  */
	      (void) select (0, (pointer) NULL, (pointer) NULL,
			     (pointer) NULL, &s);

	      /* Here either the select finished sleeping or we got a
		 SIGALRM.  If the latter occurred, fSalarm was set to
		 TRUE; it will be checked at the top of the loop.  */
	    }
	}
#endif /* HAVE_BSD_TTY */
    }

  /* Turn off the pending SIGALRM and return.  */

  usset_signal (SIGALRM, SIG_IGN, TRUE, (boolean *) NULL);
  alarm (0);
  usysdep_end_catch ();

  return fret;
}

/* Read from a port with separate read/write file descriptors.  */

boolean
fsdouble_read (qconn, zbuf, pclen, cmin, ctimeout, freport)
     struct sconnection *qconn;
     char *zbuf;
     size_t *pclen;
     size_t cmin;
     int ctimeout;
     boolean freport;
{
  struct ssysdep_conn *qsysdep;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  qsysdep->o = qsysdep->ord;
  return fsysdep_conn_read (qconn, zbuf, pclen, cmin, ctimeout, freport);
}

/* Write data to a connection.  This routine handles all types of
   connections, including TLI.  */

boolean
fsysdep_conn_write (qconn, zwrite, cwrite)
     struct sconnection *qconn;
     const char *zwrite;
     size_t cwrite;
{
  struct ssysdep_conn *q;
  int czero;

  q = (struct ssysdep_conn *) qconn->psysdep;

  /* We want blocking writes here.  */
  if (! fsblock (q, TRUE))
    return FALSE;

  czero = 0;

  while (cwrite > 0)
    {
      int cdid;

      /* Loop until we don't get an interrupt.  */
      while (TRUE)
	{
	  /* If we've received a signal, don't continue.  */
	  if (FGOT_QUIT_SIGNAL ())
	    return FALSE;

#if HAVE_TLI
	  if (q->ftli)
	    {
	      cdid = t_snd (q->o, (char *) zwrite, cwrite, 0);
	      if (cdid < 0 && t_errno != TSYSERR)
		{
		  ulog (LOG_ERROR, "t_snd: %s",
			(t_errno >= 0 && t_errno < t_nerr
			 ? t_errlist[t_errno]
			 : "unknown TLI error"));
		  return FALSE;
		}
	    }
	  else
#endif
	    cdid = write (q->o, zwrite, cwrite);

	  if (cdid >= 0)
	    break;
	  if (errno != EINTR)
	    break;

	  /* We were interrupted by a signal.  Log it.  */
	  ulog (LOG_ERROR, (const char *) NULL);
	}

      if (cdid < 0)
	{
	  if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENODATA)
	    {
	      ulog (LOG_ERROR, "write: %s", strerror (errno));
	      return FALSE;
	    }
	  cdid = 0;
	}

      if (cdid == 0)
	{
	  /* On some systems write will return 0 if carrier is lost.
	     If we fail to write anything ten times in a row, we
	     assume that this has happened.  This is hacked in like
	     this because there seems to be no reliable way to tell
	     exactly why the write returned 0.  */
	  ++czero;
	  if (czero >= 10)
	    {
	      ulog (LOG_ERROR, "Line disconnected");
	      return FALSE;
	    }
	}
      else
	{
	  czero = 0;

	  cwrite -= cdid;
	  zwrite += cdid;
	}
    }

  return TRUE;
}

/* Write to a port with separate read/write file descriptors.  */

boolean
fsdouble_write (qconn, zwrite, cwrite)
     struct sconnection *qconn;
     const char *zwrite;
     size_t cwrite;
{
  struct ssysdep_conn *qsysdep;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  qsysdep->o = qsysdep->ord;
  if (! fsblock (qsysdep, TRUE))
    return FALSE;
  qsysdep->o = qsysdep->owr;
  return fsysdep_conn_write (qconn, zwrite, cwrite);
}

/* The fsysdep_conn_io routine is supposed to both read and write data
   until it has either filled its read buffer or written out all the
   data it was given.  This lets us write out large packets without
   losing incoming data.  It handles all types of connections,
   including TLI.  */

boolean
fsysdep_conn_io (qconn, zwrite, pcwrite, zread, pcread)
     struct sconnection *qconn;
     const char *zwrite;
     size_t *pcwrite;
     char *zread;
     size_t *pcread;
{
  struct ssysdep_conn *q;
  size_t cwrite, cread;
  int czero;

  q = (struct ssysdep_conn *) qconn->psysdep;

  cwrite = *pcwrite;
  *pcwrite = 0;
  cread = *pcread;
  *pcread = 0;

  czero = 0;

  while (TRUE)
    {
      int cgot, cdid;
      size_t cdo;

      /* This used to always use nonblocking writes, but it turns out
	 that some systems don't support them on terminals.

	 The current algorithm is:
	     loop:
	       unblocked read
	       if read buffer full, return
	       if nothing to write, return
	       if HAVE_UNBLOCKED_WRITES
	         write all data
	       else
	         write up to SINGLE_WRITE bytes
	       if all data written, return
	       if no data written
	         blocked write of up to SINGLE_WRITE bytes

	 This algorithm should work whether the system supports
	 unblocked writes on terminals or not.  If the system supports
	 unblocked writes but HAVE_UNBLOCKED_WRITES is 0, then it will
	 call write more often than it needs to.  If the system does
	 not support unblocked writes but HAVE_UNBLOCKED_WRITES is 1,
	 then the write may hang so long that incoming data is lost.
	 This is actually possible at high baud rates on any system
	 when a blocking write is done; there is no solution, except
	 hardware handshaking.  */

      /* If we are running on standard input, we switch the file
	 descriptors by hand.  */
      if (q->ord >= 0)
	q->o = q->ord;

      /* Do an unblocked read.  */
      if (! fsblock (q, FALSE))
	return FALSE;

      /* Loop until we get something (error or data) other than an
	 acceptable EINTR.  */
      while (TRUE)
	{
	  /* If we've received a signal, don't continue.  */
	  if (FGOT_QUIT_SIGNAL ())
	    return FALSE;

#if HAVE_TLI
	  if (q->ftli)
	    {
	      int iflags;

	      cgot = t_rcv (q->o, zread, cread, &iflags);
	      if (cgot < 0)
		{
		  if (t_errno == TNODATA)
		    errno = EAGAIN;
		  else if (t_errno != TSYSERR)
		    {
		      ulog (LOG_ERROR, "t_rcv: %s",
			    (t_errno >= 0 && t_errno < t_nerr
			     ? t_errlist[t_errno]
			     : "unknown TLI error"));
		      return FALSE;
		    }
		}
	    }
	  else
#endif
	    cgot = read (q->o, zread, cread);

	  if (cgot >= 0)
	    break;
	  if (errno != EINTR)
	    break;

	  /* We got interrupted by a signal.  Log it.  */
	  ulog (LOG_ERROR, (const char *) NULL);
	}

      if (cgot < 0)
	{
	  if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENODATA)
	    {
	      ulog (LOG_ERROR, "read: %s", strerror (errno));
	      return FALSE;
	    }
	  cgot = 0;
	}

      cread -= cgot;
      zread += cgot;
      *pcread += cgot;

      /* If we've filled the read buffer, or we have nothing left to
	 write, return out.  */
      if (cread == 0 || cwrite == 0)
	return TRUE;

      /* The port is currently unblocked.  Do a write.  */
      cdo = cwrite;

#if ! HAVE_UNBLOCKED_WRITES
      if (q->fterminal && cdo > SINGLE_WRITE)
	cdo = SINGLE_WRITE;
#endif

      if (q->owr >= 0)
	q->o = q->owr;

      /* Loop until we get something besides EINTR.  */
      while (TRUE)
	{
	  /* If we've received a signal, don't continue.  */
	  if (FGOT_QUIT_SIGNAL ())
	    return FALSE;

#if HAVE_TLI
	  if (q->ftli)
	    {
	      cdid = t_snd (q->o, (char *) zwrite, cdo, 0);
	      if (cdid < 0)
		{
		  if (t_errno == TFLOW)
		    errno = EAGAIN;
		  else if (t_errno != TSYSERR)
		    {
		      ulog (LOG_ERROR, "t_snd: %s",
			    (t_errno >= 0 && t_errno < t_nerr
			     ? t_errlist[t_errno]
			     : "unknown TLI error"));
		      return FALSE;
		    }
		}
	    }
	  else
#endif
	    cdid = write (q->o, zwrite, cdo);

	  if (cdid >= 0)
	    break;
	  if (errno != EINTR)
	    break;

	  /* We got interrupted by a signal.  Log it.  */
	  ulog (LOG_ERROR, (const char *) NULL);
	}

      if (cdid < 0)
	{
	  if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENODATA)
	    {
	      ulog (LOG_ERROR, "write: %s", strerror (errno));
	      return FALSE;
	    }
	  cdid = 0;
	}

      if (cdid > 0)
	{
	  /* We wrote some data.  If we wrote everything, return out.
	     Otherwise loop around and do another read.  */
	  cwrite -= cdid;
	  zwrite += cdid;
	  *pcwrite += cdid;

	  if (cwrite == 0)
	    return TRUE;

	  czero = 0;
	}
      else
	{
	  /* We didn't write any data.  Do a blocking write.  */

	  if (q->ord >= 0)
	    q->o = q->ord;

	  if (! fsblock (q, TRUE))
	    return FALSE;

	  cdo = cwrite;
	  if (cdo > SINGLE_WRITE)
	    cdo = SINGLE_WRITE;

	  DEBUG_MESSAGE1 (DEBUG_PORT,
			  "fsysdep_conn_io: Blocking write of %lu",
			  (unsigned long) cdo);

	  if (q->owr >= 0)
	    q->o = q->owr;

	  /* Loop until we get something besides EINTR.  */
	  while (TRUE)
	    {
	      /* If we've received a signal, don't continue.  */
	      if (FGOT_QUIT_SIGNAL ())
		return FALSE;

#if HAVE_TLI
	      if (q->ftli)
		{
		  cdid = t_snd (q->o, (char *) zwrite, cdo, 0);
		  if (cdid < 0 && t_errno != TSYSERR)
		    {
		      ulog (LOG_ERROR, "t_snd: %s",
			    (t_errno >= 0 && t_errno < t_nerr
			     ? t_errlist[t_errno]
			     : "unknown TLI error"));
		      return FALSE;
		    }
		}
	      else
#endif
		cdid = write (q->o, zwrite, cdo);

	      if (cdid >= 0)
		break;
	      if (errno != EINTR)
		break;

	      /* We got interrupted by a signal.  Log it.  */
	      ulog (LOG_ERROR, (const char *) NULL);
	    }
	  
	  if (cdid < 0)
	    {
	      ulog (LOG_ERROR, "write: %s", strerror (errno));
	      return FALSE;
	    }

	  if (cdid == 0)
	    {
	      /* On some systems write will return 0 if carrier is
		 lost.  If we fail to write anything ten times in a
		 row, we assume that this has happened.  This is
		 hacked in like this because there seems to be no
		 reliable way to tell exactly why the write returned
		 0.  */
	      ++czero;
	      if (czero >= 10)
		{
		  ulog (LOG_ERROR, "Line disconnected");
		  return FALSE;
		}
	    }
	  else
	    {
	      cwrite -= cdid;
	      zwrite += cdid;
	      *pcwrite += cdid;
	      czero = 0;
	    }
	}
    }
}

/* Send a break character to a serial port.  */

static boolean
fsserial_break (qconn)
     struct sconnection *qconn;
{
  struct ssysdep_conn *q;

  q = (struct ssysdep_conn *) qconn->psysdep;

#if HAVE_BSD_TTY
  (void) ioctl (q->o, TIOCSBRK, 0);
  sleep (2);
  (void) ioctl (q->o, TIOCCBRK, 0);
  return TRUE;
#endif /* HAVE_BSD_TTY */
#if HAVE_SYSV_TERMIO
  (void) ioctl (q->o, TCSBRK, 0);
  return TRUE;
#endif /* HAVE_SYSV_TERMIO */
#if HAVE_POSIX_TERMIOS
  return tcsendbreak (q->o, 0) == 0;
#endif /* HAVE_POSIX_TERMIOS */
}

/* Send a break character to a stdin port.  */

static boolean
fsstdin_break (qconn)
     struct sconnection *qconn;
{
  struct ssysdep_conn *qsysdep;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  qsysdep->o = qsysdep->owr;
  return fsserial_break (qconn);
}

/* Change the setting of a serial port.  */

/*ARGSUSED*/
static boolean
fsserial_set (qconn, tparity, tstrip, txonxoff)
     struct sconnection *qconn;
     enum tparitysetting tparity;
     enum tstripsetting tstrip;
     enum txonxoffsetting txonxoff;
{
  register struct ssysdep_conn *q;
  boolean fchanged, fdo;
  int iset = 0;
  int iclear = 0;

  q = (struct ssysdep_conn *) qconn->psysdep;

  if (! q->fterminal)
    return TRUE;

  fchanged = FALSE;

  /* Set the parity for output characters.  */

#if HAVE_BSD_TTY

  /* This will also cause parity detection on input characters.  */

  fdo = FALSE;
  switch (tparity)
    {
    case PARITYSETTING_DEFAULT:
      break;
    case PARITYSETTING_NONE:
#if HAVE_PARITY_BUG
      /* The Sony NEWS mishandles this for some reason.  */
      iset = 0;
      iclear = ANYP;
#else
      iset = ANYP;
      iclear = 0;
#endif
      fdo = TRUE;
      break;
    case PARITYSETTING_EVEN:
      iset = EVENP;
      iclear = ODDP;
      fdo = TRUE;
      break;
    case PARITYSETTING_ODD:
      iset = ODDP;
      iclear = EVENP;
      fdo = TRUE;
      break;
    case PARITYSETTING_MARK:
    case PARITYSETTING_SPACE:
      /* Not supported.  */
      break;
    }

  if (fdo)
    {
      if ((q->snew.stty.sg_flags & iset) != iset
	  || (q->snew.stty.sg_flags & iclear) != 0)
	{
	  q->snew.stty.sg_flags |= iset;
	  q->snew.stty.sg_flags &=~ iclear;
	  fchanged = TRUE;
	}
    }

#else /* ! HAVE_BSD_TTY */

  fdo = FALSE;
  switch (tparity)
    {
    case PARITYSETTING_DEFAULT:
      break;
    case PARITYSETTING_NONE:
      iset = CS8;
      iclear = PARENB | PARODD | (CSIZE &~ CS8);
      fdo = TRUE;
      break;
    case PARITYSETTING_EVEN:
      iset = PARENB | CS7;
      iclear = PARODD | (CSIZE &~ CS7);
      fdo = TRUE;
      break;
    case PARITYSETTING_ODD:
      iset = PARENB | PARODD | CS7;
      iclear = CSIZE &~ CS7;
      fdo = TRUE;
      break;
    case PARITYSETTING_MARK:
    case PARITYSETTING_SPACE:
      /* Not supported.  */
      break;
    }
	  
  if (fdo)
    {
      if ((q->snew.c_cflag & iset) != iset
	  || (q->snew.c_cflag & iclear) != 0)
	{
	  q->snew.c_cflag |= iset;
	  q->snew.c_cflag &=~ iclear;
	  fchanged = TRUE;
	}
    }

#endif /* ! HAVE_BSD_TTY */

  /* Set whether input characters are stripped to seven bits.  */

#if HAVE_BSD_TTY

#ifdef LPASS8
  {
    int i;

    i = LPASS8;
    if (tstrip == STRIPSETTING_EIGHTBITS)
      {
	i = LPASS8;
	(void) ioctl (q->o, TIOCLBIS, &i);
      }
    else if (tstrip == STRIPSETTING_SEVENBITS)
      {
	i = LPASS8;
	(void) ioctl (q->o, TIOCLBIC, &i);
      }
  }
#endif

#else /* ! HAVE_BSD_TTY */      

  fdo = FALSE;
  switch (tstrip)
    {
    case STRIPSETTING_DEFAULT:
      break;
    case STRIPSETTING_EIGHTBITS:
      iset = 0;
      iclear = ISTRIP;
      fdo = TRUE;
      break;
    case STRIPSETTING_SEVENBITS:
      iset = ISTRIP;
      iclear = 0;
      fdo = TRUE;
      break;
    }

  if (fdo)
    {
      if ((q->snew.c_iflag & iset) != iset
	  || (q->snew.c_iflag & iclear) != 0)
	{
	  q->snew.c_iflag |= iset;
	  q->snew.c_iflag &=~ iclear;
	  fchanged = TRUE;
	}
    }

#endif /* ! HAVE_BSD_TTY */

  /* Set XON/XOFF handshaking.  */

#if HAVE_BSD_TTY

  fdo = FALSE;
  switch (txonxoff)
    {
    case XONXOFF_DEFAULT:
      break;
    case XONXOFF_OFF:
      iset = RAW;
      iclear = TANDEM | CBREAK;
      fdo = TRUE;
      break;
    case XONXOFF_ON:
      iset = CBREAK | TANDEM;
      iclear = RAW;
      fdo = TRUE;
      break;
    }

  if (fdo)
    {
      if ((q->snew.stty.sg_flags & iset) != iset
	  || (q->snew.stty.sg_flags & iclear) != 0)
	{
	  q->snew.stty.sg_flags |= iset;
	  q->snew.stty.sg_flags &=~ iclear;
	  fchanged = TRUE;
	}
    }

#else /* ! HAVE_BSD_TTY */

  fdo = FALSE;
  switch (txonxoff)
    {
    case XONXOFF_DEFAULT:
      break;
    case XONXOFF_OFF:
      iset = 0;
      iclear = IXON | IXOFF;
      fdo = TRUE;
      break;
    case XONXOFF_ON:
#ifdef CRTSCTS
#if HAVE_POSIX_TERMIOS
      /* This is system dependent, but I haven't figured out a good
	 way around it yet.  If we are doing hardware flow control, we
	 don't send XON/XOFF characters but we do recognize them.  */
      if ((q->snew.c_cflag & CRTSCTS) != 0)
	{
	  iset = IXON;
	  iclear = IXOFF;
	  fdo = TRUE;
	  break;
	}
#endif /* HAVE_POSIX_TERMIOS */
#endif /* defined (CRTSCTS) */
#ifdef CRTSFL
      if ((q->snew.c_cflag & CRTSFL) != 0)
	{
	  iset = IXON;
	  iclear = IXOFF;
	  /* SCO says we cant have CRTSFL **and** RTSFLOW/CTSFLOW */
#ifdef RTSFLOW
	  iclear |= RTSFLOW;
#endif
#ifdef CTSFLOW
	  iclear |= CTSFLOW;
#endif
	  fdo = TRUE;
	  break;
	}
#endif /* defined(CRTSFL) */
      iset = IXON | IXOFF;
      iclear = 0;
      fdo = TRUE;
      break;
    }

  if (fdo)
    {
      if ((q->snew.c_iflag & iset) != iset
	  || (q->snew.c_iflag & iclear) != 0)
	{
	  q->snew.c_iflag |= iset;
	  q->snew.c_iflag &=~ iclear;
	  fchanged = TRUE;
	}
    }

#endif /* ! HAVE_BSD_TTY */

  if (fchanged)
    {
      if (! fsetterminfodrain (q->o, &q->snew))
	{
	  ulog (LOG_ERROR, "Can't change terminal settings: %s",
		strerror (errno));
	  return FALSE;
	}
    }

#if HAVE_BSD_TTY
  if (txonxoff == XONXOFF_ON
      && (q->snew.stty.sg_flags & ANYP) == ANYP)
    {
      int i;

      /* At least on Ultrix, we seem to have to set LLITOUT and
	 LPASS8.  This shouldn't foul things up anywhere else.  As far
	 as I can tell, this has to be done after setting the terminal
	 into cbreak mode, not before.  */
#ifndef LLITOUT
#define LLITOUT 0
#endif
#ifndef LPASS8
#define LPASS8 0
#endif
#ifndef LAUTOFLOW
#define LAUTOFLOW 0
#endif
      i = LLITOUT | LPASS8 | LAUTOFLOW;
      (void) ioctl (q->o, TIOCLBIS, &i);

#if HAVE_STRIP_BUG
      /* Ultrix 4.0 has a peculiar problem: setting CBREAK always
	 causes input characters to be stripped.  I hope this does not
	 apply to other BSD systems.  It is possible to work around
	 this by using the termio call.  I wish this sort of stuff was
	 not necessary!!!  */
      {
	struct termio s;

	if (ioctl (q->o, TCGETA, &s) >= 0)
	  {
	    s.c_iflag &=~ ISTRIP;
	    (void) ioctl (q->o, TCSETA, &s);
	  }
      }
#endif /* HAVE_STRIP_BUG */
    }
#endif /* HAVE_BSD_TTY */

  return TRUE;
}

/* Change settings of a stdin port.  */

static boolean
fsstdin_set (qconn, tparity, tstrip, txonxoff)
     struct sconnection *qconn;
     enum tparitysetting tparity;
     enum tstripsetting tstrip;
     enum txonxoffsetting txonxoff;
{
  struct ssysdep_conn *qsysdep;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  qsysdep->o = qsysdep->ord;
  return fsserial_set (qconn, tparity, tstrip, txonxoff);
}

/* Run a chat program.  */

static boolean
fsrun_chat (oread, owrite, pzprog)
     int oread;
     int owrite;
     char **pzprog;
{
  int aidescs[3];
  FILE *e;
  pid_t ipid;
  char *z;
  size_t c;

  aidescs[0] = oread;
  aidescs[1] = owrite;
  aidescs[2] = SPAWN_READ_PIPE;

  /* Pass fkeepuid, fkeepenv and fshell as TRUE.  This puts the
     responsibility of maintaing security on the chat program.  */
  ipid = ixsspawn ((const char **) pzprog, aidescs, TRUE, TRUE,
		   (const char *) NULL, FALSE, TRUE, (const char *) NULL,
		   (const char *) NULL, (const char *) NULL);
  if (ipid < 0)
    {
      ulog (LOG_ERROR, "ixsspawn (%s): %s", pzprog[0], strerror (errno));
      return FALSE;
    }

  e = fdopen (aidescs[2], (char *) "r");
  if (e == NULL)
    {
      ulog (LOG_ERROR, "fdopen: %s", strerror (errno));
      (void) close (aidescs[2]);
      (void) kill (ipid, SIGKILL);
      (void) ixswait ((unsigned long) ipid, (const char *) NULL);
      return FALSE;
    }

  /* The FILE e now is attached to stderr of the program.  Forward
     every line the program outputs to the log file.  */
  z = NULL;
  c = 0;
  while (getline (&z, &c, e) > 0)
    {
      size_t clen;

      clen = strlen (z);
      if (z[clen - 1] == '\n')
	z[clen - 1] = '\0';
      if (*z != '\0')
	ulog (LOG_NORMAL, "chat: %s", z);
    }

  xfree ((pointer) z);
  (void) fclose (e);

  return ixswait ((unsigned long) ipid, "Chat program") == 0;
}

/* Run a chat program on a port using separate read/write file
   descriptors.  */

boolean
fsdouble_chat (qconn, pzprog)
     struct sconnection *qconn;
     char **pzprog;
{
  struct ssysdep_conn *qsysdep;
  boolean fret;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  fret = fsrun_chat (qsysdep->ord, qsysdep->owr, pzprog);
  if (qsysdep->fterminal)
    (void) fgetterminfo (qsysdep->ord, &qsysdep->snew);
  return fret;
}

/* Run a chat program on any general type of connection.  */

boolean
fsysdep_conn_chat (qconn, pzprog)
     struct sconnection *qconn;
     char **pzprog;
{
  struct ssysdep_conn *qsysdep;
  boolean fret;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  fret = fsrun_chat (qsysdep->o, qsysdep->o, pzprog);
  if (qsysdep->fterminal)
    (void) fgetterminfo (qsysdep->o, &qsysdep->snew);
  return fret;
}

/* Return baud rate of a serial port.  */

static long
isserial_baud (qconn)
     struct sconnection *qconn;
{
  struct ssysdep_conn *qsysdep;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  return qsysdep->ibaud;
}
