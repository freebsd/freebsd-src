/* tli.c
   Code to handle TLI connections.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char tli_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libunix/tli.c,v 1.4 1999/08/27 23:33:10 peter Exp $";
#endif

#if HAVE_TLI

#include "sysdep.h"
#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "system.h"

#include <errno.h>

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if HAVE_TIUSER_H
#include <tiuser.h>
#else
#if HAVE_XTI_H
#include <xti.h>
#else
#if HAVE_SYS_TLI_H
#include <sys/tli.h>
#endif
#endif
#endif

#if HAVE_STROPTS_H
#include <stropts.h>
#endif

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

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

/* The arguments to t_alloca have two different names.  I want the
   SVID ones, not the XPG3 ones.  */
#ifndef T_BIND
#define T_BIND T_BIND_STR
#endif
#ifndef T_CALL
#define T_CALL T_CALL_STR
#endif

/* Hopefully these externs will not cause any trouble.  This is how
   they are shown in the SVID.  */
extern int t_errno;
extern char *t_errlist[];
extern int t_nerr;

#ifndef HAVE_TIUSER_H
#ifndef t_alloc
extern pointer t_alloc ();
#endif
#endif

/* This code handles TLI connections.  It's Unix specific.  It's
   largely based on code from Unix Network Programming, by W. Richard
   Stevens.  */

/* Local functions.  */
static const char *ztlierror P((void));
static void utli_free P((struct sconnection *qconn));
static boolean ftli_push P((struct sconnection *qconn));
static boolean ftli_open P((struct sconnection *qconn, long ibaud,
			    boolean fwait));
static boolean ftli_close P((struct sconnection *qconn,
			     pointer puuconf,
			     struct uuconf_dialer *qdialer,
			     boolean fsuccess));
static boolean ftli_dial P((struct sconnection *qconn, pointer puuconf,
			    const struct uuconf_system *qsys,
			    const char *zphone,
			    struct uuconf_dialer *qdialer,
			    enum tdialerfound *ptdialer));

/* The command table for a TLI connection.  */
static const struct sconncmds stlicmds =
{
  utli_free,
  NULL, /* pflock */
  NULL, /* pfunlock */
  ftli_open,
  ftli_close,
  ftli_dial,
  fsysdep_conn_read,
  fsysdep_conn_write,
  fsysdep_conn_io,
  NULL, /* pfbreak */
  NULL, /* pfset */
  NULL, /* pfcarrier */
  fsysdep_conn_chat,
  NULL /* pibaud */
};

/* Get a TLI error string.  */

static const char *
ztlierror ()
{
  if (t_errno == TSYSERR)
    return strerror (errno);
  if (t_errno < 0 || t_errno >= t_nerr)
    return "Unknown TLI error";
  return t_errlist[t_errno];
} 

/* Initialize a TLI connection.  This may be called with qconn->qport
   NULL, when opening standard input as a TLI connection.  */

boolean
fsysdep_tli_init (qconn)
     struct sconnection *qconn;
{
  struct ssysdep_conn *q;

  q = (struct ssysdep_conn *) xmalloc (sizeof (struct ssysdep_conn));
  q->o = -1;
  q->ord = -1;
  q->owr = -1;
  q->zdevice = NULL;
  q->iflags = -1;
  q->iwr_flags = -1;
  q->fterminal = FALSE;
  q->ftli = TRUE;
  q->ibaud = 0;

  qconn->psysdep = (pointer) q;
  qconn->qcmds = &stlicmds;
  return TRUE;
}

/* Free a TLI connection.  */

static void
utli_free (qconn)
     struct sconnection *qconn;
{
  xfree (qconn->psysdep);
}

/* Push all desired modules onto a TLI stream.  If the user requests a
   STREAMS connection without giving a list of modules, we just push
   tirdwr.  If the I_PUSH ioctl is not defined on this system, we just
   ignore any list of modules.  */

static boolean
ftli_push (qconn)
     struct sconnection *qconn;
{
#ifdef I_PUSH

  struct ssysdep_conn *qsysdep;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

  if (qconn->qport->uuconf_u.uuconf_stli.uuconf_pzpush != NULL)
    {
      char **pz;

      for (pz = qconn->qport->uuconf_u.uuconf_stli.uuconf_pzpush;
	   *pz != NULL;
	   pz++)
	{
	  if (ioctl (qsysdep->o, I_PUSH, *pz) < 0)
	    {
	      ulog (LOG_ERROR, "ioctl (I_PUSH, %s): %s", *pz,
		    strerror (errno));
	      return FALSE;
	    }
	}
    }
  else if (qconn->qport->uuconf_u.uuconf_stli.uuconf_fstream)
    {
      if (ioctl (qsysdep->o, I_PUSH, "tirdwr") < 0)
	{
	  ulog (LOG_ERROR, "ioctl (I_PUSH, tirdwr): %s",
		strerror (errno));
	  return FALSE;
	}
    }

  /* If we have just put the connection into stream mode, we must turn
     off the TLI flag to avoid using TLI calls on it.  */
  if (qconn->qport->uuconf_u.uuconf_stli.uuconf_fstream)
    qsysdep->ftli = FALSE;

#endif /* defined (I_PUSH) */
  
  return TRUE;
}

/* Open a TLI connection.  If the fwait argument is TRUE, we are
   running as a server.  Otherwise we are just trying to reach another
   system.  */

static boolean
ftli_open (qconn, ibaud, fwait)
     struct sconnection *qconn;
     long ibaud;
     boolean fwait;
{
  struct ssysdep_conn *qsysdep;
  const char *zdevice;
  char *zfreedev;
  const char *zservaddr;
  char *zfreeaddr;
  uid_t ieuid;
  boolean fswap;
  struct t_bind *qtbind;
  struct t_call *qtcall;

  /* Unlike most other device types, we don't bother to call
     ulog_device here, because fconn_open calls it with the name of
     the port anyhow.  */

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

  zdevice = qconn->qport->uuconf_u.uuconf_stli.uuconf_zdevice;
  if (zdevice == NULL)
    zdevice = qconn->qport->uuconf_zname;

  zfreedev = NULL;
  if (*zdevice != '/')
    {
      zfreedev = zbufalc (sizeof "/dev/" + strlen (zdevice));
      sprintf (zfreedev, "/dev/%s", zdevice);
      zdevice = zfreedev;
    }

  /* If we are acting as a server, swap to our real user ID before
     calling t_open.  This will permit the server to use privileged
     TCP ports when invoked by root.  We only swap if our effective
     user ID is not root, so that the program can also be made suid
     root in order to get privileged ports when invoked by anybody.  */
  fswap = fwait && geteuid () != 0;
  if (fswap)
    {
      if (! fsuser_perms (&ieuid))
	{
	  ubuffree (zfreedev);
	  return FALSE;
	}
    }

  qsysdep->o = t_open (zdevice, O_RDWR, (struct t_info *) NULL);
  if (qsysdep->o < 0)
    {
      if (fswap)
	(void) fsuucp_perms ((long) ieuid);
      ulog (LOG_ERROR, "t_open (%s): %s", zdevice, ztlierror ());
      ubuffree (zfreedev);
      return FALSE;
    }

  if (fcntl (qsysdep->o, F_SETFD,
	     fcntl (qsysdep->o, F_GETFD, 0) | FD_CLOEXEC) < 0)
    {
      if (fswap)
	(void) fsuucp_perms ((long) ieuid);
      ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
      ubuffree (zfreedev);
      (void) t_close (qsysdep->o);
      qsysdep->o = -1;
      return FALSE;
    }

  qsysdep->iflags = fcntl (qsysdep->o, F_GETFL, 0);
  if (qsysdep->iflags < 0)
    {
      if (fswap)
	(void) fsuucp_perms ((long) ieuid);
      ulog (LOG_ERROR, "fcntl: %s", strerror (errno));
      ubuffree (zfreedev);
      (void) t_close (qsysdep->o);
      qsysdep->o = -1;
      return FALSE;
    }

  /* We save our process ID in the qconn structure.  This is checked
     in ftli_close.  */
  qsysdep->ipid = getpid ();

  /* If we aren't waiting for a connection, we can bind to any local
     address, and then we're finished.  */
  if (! fwait)
    {
      /* fswap is known to be FALSE here.  */
      ubuffree (zfreedev);
      if (t_bind (qsysdep->o, (struct t_bind *) NULL,
		  (struct t_bind *) NULL) < 0)
	{
	  ulog (LOG_ERROR, "t_bind: %s", ztlierror ());
	  (void) t_close (qsysdep->o);
	  qsysdep->o = -1;
	  return FALSE;
	}
      return TRUE;
    }

  /* Run as a server and wait for a new connection.  The code in
     uucico.c has already detached us from our controlling terminal.
     From this point on if the server gets an error we exit; we only
     return if we have received a connection.  It would be more robust
     to respawn the server if it fails; someday.  */
  qtbind = (struct t_bind *) t_alloc (qsysdep->o, T_BIND, T_ALL);
  if (qtbind == NULL)
    {
      if (fswap)
	(void) fsuucp_perms ((long) ieuid);
      ulog (LOG_FATAL, "t_alloc (T_BIND): %s", ztlierror ());
    }

  zservaddr = qconn->qport->uuconf_u.uuconf_stli.uuconf_zservaddr;
  if (zservaddr == NULL)
    {
      if (fswap)
	(void) fsuucp_perms ((long) ieuid);
      ulog (LOG_FATAL, "Can't run as TLI server; no server address");
    }

  zfreeaddr = zbufcpy (zservaddr);
  qtbind->addr.len = cescape (zfreeaddr);
  if (qtbind->addr.len > qtbind->addr.maxlen)
    {
      if (fswap)
	(void) fsuucp_perms ((long) ieuid);
      ulog (LOG_FATAL, "%s: TLI server address too long (max %d)",
	    zservaddr, qtbind->addr.maxlen);
    }
  memcpy (qtbind->addr.buf, zfreeaddr, qtbind->addr.len);
  ubuffree (zfreeaddr);

  qtbind->qlen = 5;

  if (t_bind (qsysdep->o, qtbind, (struct t_bind *) NULL) < 0)
    {
      if (fswap)
	(void) fsuucp_perms ((long) ieuid);
      ulog (LOG_FATAL, "t_bind (%s): %s", zservaddr, ztlierror ());
    }

  if (fswap)
    {
      if (! fsuucp_perms ((long) ieuid))
	ulog (LOG_FATAL, "Could not swap back to UUCP user permissions");
    }

  (void) t_free ((pointer) qtbind, T_BIND);

  qtcall = (struct t_call *) t_alloc (qsysdep->o, T_CALL, T_ALL);
  if (qtcall == NULL)
    ulog (LOG_FATAL, "t_alloc (T_CALL): %s", ztlierror ());

  while (! FGOT_SIGNAL ())
    {
      int onew;
      pid_t ipid;

      DEBUG_MESSAGE0 (DEBUG_PORT,
		      "ftli_open: Waiting for connections");

      if (t_listen (qsysdep->o, qtcall) < 0)
	ulog (LOG_FATAL, "t_listen: %s", ztlierror ());

      onew = t_open (zdevice, O_RDWR, (struct t_info *) NULL);
      if (onew < 0)
	ulog (LOG_FATAL, "t_open (%s): %s", zdevice, ztlierror ());
	  
      if (fcntl (onew, F_SETFD,
		 fcntl (onew, F_GETFD, 0) | FD_CLOEXEC) < 0)
	ulog (LOG_FATAL, "fcntl (FD_CLOEXEC): %s", strerror (errno));

      if (t_bind (onew, (struct t_bind *) NULL, (struct t_bind *) NULL) < 0)
	ulog (LOG_FATAL, "t_bind: %s", ztlierror ());

      if (t_accept (qsysdep->o, onew, qtcall) < 0)
	{
	  /* We may have received a disconnect.  */
	  if (t_errno != TLOOK)
	    ulog (LOG_FATAL, "t_accept: %s", ztlierror ());
	  if (t_rcvdis (qsysdep->o, (struct t_discon *) NULL) < 0)
	    ulog (LOG_FATAL, "t_rcvdis: %s", ztlierror ());
	  (void) t_close (onew);
	  continue;
	}

      DEBUG_MESSAGE0 (DEBUG_PORT,
		      "ftli_open: Got connection; forking");

      ipid = ixsfork ();
      if (ipid < 0)
	ulog (LOG_FATAL, "fork: %s", strerror (errno));
      if (ipid == 0)
	{
	  ulog_close ();

	  (void) t_close (qsysdep->o);
	  qsysdep->o = onew;

	  /* Push any desired modules.  */
	  if (! ftli_push (qconn))
	    _exit (EXIT_FAILURE);

	  /* Now we fork and let our parent die, so that we become
	     a child of init.  This lets the main server code wait
	     for its child and then continue without accumulating
	     zombie children.  */
	  ipid = ixsfork ();
	  if (ipid < 0)
	    {
	      ulog (LOG_ERROR, "fork: %s", strerror (errno));
	      _exit (EXIT_FAILURE);
	    }
	      
	  if (ipid != 0)
	    _exit (EXIT_SUCCESS);

	  ulog_id (getpid ());

	  return TRUE;
	}

      (void) t_close (onew);

      /* Now wait for the child.  */
      (void) ixswait ((unsigned long) ipid, (const char *) NULL);
    }

  /* We got a signal.  */
  usysdep_exit (FALSE);

  /* Avoid compiler warnings.  */
  return FALSE;
}

/* Close the port.  */

/*ARGSUSED*/
static boolean
ftli_close (qconn, puuconf, qdialer, fsuccess)
     struct sconnection *qconn;
     pointer puuconf;
     struct uuconf_dialer *qdialer;
     boolean fsuccess;
{
  struct ssysdep_conn *qsysdep;
  boolean fret;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

  fret = TRUE;
  if (qsysdep->o >= 0)
    {
      if (qsysdep->ftli)
	{
	  if (t_close (qsysdep->o) < 0)
	    {
	      ulog (LOG_ERROR, "t_close: %s", ztlierror ());
	      fret = FALSE;
	    }
	}
      else
	{
	  if (close (qsysdep->o) < 0)
	    {
	      ulog (LOG_ERROR, "close: %s", strerror (errno));
	      fret = FALSE;
	    }
	}

      qsysdep->o = -1;
    }

  /* If the current pid is not the one we used to open the port, then
     we must have forked up above and we are now the child.  In this
     case, we are being called from within the fendless loop in
     uucico.c.  We return FALSE to force the loop to end and the child
     to exit.  This should be handled in a cleaner fashion.  */
  if (qsysdep->ipid != getpid ())
    fret = FALSE;

  return fret;
}

/* Dial out on a TLI port, so to speak: connect to a remote computer.  */

/*ARGSUSED*/
static boolean
ftli_dial (qconn, puuconf, qsys, zphone, qdialer, ptdialerfound)
     struct sconnection *qconn;
     pointer puuconf;
     const struct uuconf_system *qsys;
     const char *zphone;
     struct uuconf_dialer *qdialer;
     enum tdialerfound *ptdialerfound;
{
  struct ssysdep_conn *qsysdep;
  char **pzdialer;
  const char *zaddr;
  struct t_call *qtcall;
  char *zescape;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

  *ptdialerfound = DIALERFOUND_FALSE;

  pzdialer = qconn->qport->uuconf_u.uuconf_stli.uuconf_pzdialer;
  if (*pzdialer == NULL)
    pzdialer = NULL;

  /* If the first dialer is "TLI" or "TLIS", we use the first token
     (pzdialer[1]) as the address to connect to.  */
  zaddr = zphone;
  if (pzdialer != NULL
      && (strcmp (pzdialer[0], "TLI") == 0
	  || strcmp (pzdialer[0], "TLIS") == 0))
    {
      if (pzdialer[1] == NULL)
	++pzdialer;
      else
	{
	  if (strcmp (pzdialer[1], "\\D") != 0
	      && strcmp (pzdialer[1], "\\T") != 0)
	    zaddr = pzdialer[1];
	  pzdialer += 2;
	}
    }
  
  if (zaddr == NULL)
    {
      ulog (LOG_ERROR, "No address for TLI connection");
      return FALSE;
    }

  qtcall = (struct t_call *) t_alloc (qsysdep->o, T_CALL, T_ADDR);
  if (qtcall == NULL)
    {
      ulog (LOG_ERROR, "t_alloc (T_CALL): %s", ztlierror ());
      return FALSE;
    }

  zescape = zbufcpy (zaddr);
  qtcall->addr.len = cescape (zescape);
  if (qtcall->addr.len > qtcall->addr.maxlen)
    {
      ulog (LOG_ERROR, "%s: TLI address too long (max %d)", zaddr,
	    qtcall->addr.maxlen);
      ubuffree (zescape);
      return FALSE;
    }
  memcpy (qtcall->addr.buf, zescape, qtcall->addr.len);
  ubuffree (zescape);

  if (t_connect (qsysdep->o, qtcall, (struct t_call *) NULL) < 0)
    {
      if (t_errno != TLOOK)
	ulog (LOG_ERROR, "t_connect: %s", ztlierror ());
      else
	{
	  if (t_rcvdis (qsysdep->o, (struct t_discon *) NULL) < 0)
	    ulog (LOG_ERROR, "t_rcvdis: %s", ztlierror ());
	  else
	    ulog (LOG_ERROR, "Connection refused");
	}
      return FALSE;
    }

  /* We've connected to the remote.  Push any desired modules.  */
  if (! ftli_push (qconn))
    return FALSE;      

  /* Handle the rest of the dialer sequence.  */
  if (pzdialer != NULL && *pzdialer != NULL)
    {
      if (! fconn_dial_sequence (qconn, puuconf, pzdialer, qsys, zphone,
				 qdialer, ptdialerfound))
	return FALSE;
    }

  return TRUE;
}

#endif /* HAVE_TLI */
