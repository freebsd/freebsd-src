/* tcp.c
   Code to handle TCP connections.

   Copyright (C) 1991, 1992, 1993 Ian Lance Taylor

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
const char tcp_rcsid[] = "$Id: tcp.c,v 1.2 1994/05/07 18:09:01 ache Exp $";
#endif

#if HAVE_TCP

#include "uudefs.h"
#include "uuconf.h"
#include "sysdep.h"
#include "conn.h"
#include "system.h"

#include <errno.h>

#if HAVE_SYS_TYPES_TCP_H
#include <sys/types.tcp.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

/* This code handles TCP connections.  It assumes a Berkeley socket
   interface.  */

/* The normal "uucp" port number.  */
#define IUUCP_PORT (540)

/* Local functions.  */
static void utcp_free P((struct sconnection *qconn));
static boolean ftcp_open P((struct sconnection *qconn, long ibaud,
			    boolean fwait));
static boolean ftcp_close P((struct sconnection *qconn,
			     pointer puuconf,
			     struct uuconf_dialer *qdialer,
			     boolean fsuccess));
static boolean ftcp_dial P((struct sconnection *qconn, pointer puuconf,
			    const struct uuconf_system *qsys,
			    const char *zphone,
			    struct uuconf_dialer *qdialer,
			    enum tdialerfound *ptdialer));
static int itcp_port_number P((const char *zport));

/* The command table for a TCP connection.  */
static const struct sconncmds stcpcmds =
{
  utcp_free,
  NULL, /* pflock */
  NULL, /* pfunlock */
  ftcp_open,
  ftcp_close,
  ftcp_dial,
  fsysdep_conn_read,
  fsysdep_conn_write,
  fsysdep_conn_io,
  NULL, /* pfbreak */
  NULL, /* pfset */
  NULL, /* pfcarrier */
  fsysdep_conn_chat,
  NULL /* pibaud */
};

/* Initialize a TCP connection.  */

boolean
fsysdep_tcp_init (qconn)
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
  q->ftli = FALSE;
  q->ibaud = 0;

  qconn->psysdep = (pointer) q;
  qconn->qcmds = &stcpcmds;
  return TRUE;
}

/* Free a TCP connection.  */

static void
utcp_free (qconn)
     struct sconnection *qconn;
{
  xfree (qconn->psysdep);
}

/* Open a TCP connection.  If the fwait argument is TRUE, we are
   running as a server.  Otherwise we are just trying to reach another
   system.  */

static boolean
ftcp_open (qconn, ibaud, fwait)
     struct sconnection *qconn;
     long ibaud;
     boolean fwait;
{
  struct ssysdep_conn *qsysdep;
  struct sockaddr_in s;
  const char *zport;
  uid_t iuid, ieuid;

  ulog_device ("TCP");

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

  qsysdep->o = socket (AF_INET, SOCK_STREAM, 0);
  if (qsysdep->o < 0)
    {
      ulog (LOG_ERROR, "socket: %s", strerror (errno));
      return FALSE;
    }

  if (fcntl (qsysdep->o, F_SETFD,
	     fcntl (qsysdep->o, F_GETFD, 0) | FD_CLOEXEC) < 0)
    {
      ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
      (void) close (qsysdep->o);
      qsysdep->o = -1;
      return FALSE;
    }

  qsysdep->iflags = fcntl (qsysdep->o, F_GETFL, 0);
  if (qsysdep->iflags < 0)
    {
      ulog (LOG_ERROR, "fcntl: %s", strerror (errno));
      (void) close (qsysdep->o);
      qsysdep->o = -1;
      return FALSE;
    }

  /* We save our process ID in the qconn structure.  This is checked
     in ftcp_close.  */
  qsysdep->ipid = getpid ();

  /* If we aren't waiting for a connection, we're done.  */
  if (! fwait)
    return TRUE;

  /* Run as a server and wait for a new connection.  The code in
     uucico.c has already detached us from our controlling terminal.
     From this point on if the server gets an error we exit; we only
     return if we have received a connection.  It would be more robust
     to respawn the server if it fails; someday.  */
  bzero ((pointer) &s, sizeof s);
  s.sin_family = AF_INET;
  zport = qconn->qport->uuconf_u.uuconf_stcp.uuconf_zport;
  s.sin_port = itcp_port_number (zport);
  s.sin_addr.s_addr = htonl (INADDR_ANY);

  /* Swap to our real user ID when doing the bind call.  This will
     permit the server to use privileged TCP ports when invoked by
     root.  We only swap if our effective user ID is not root, so that
     the program can also be made suid root in order to get privileged
     ports when invoked by anybody.  */
  iuid = getuid ();
  ieuid = geteuid ();
  if (ieuid != 0)
    {
#if HAVE_SETREUID
      /* Swap the effective user id and the real user id.  We can then
	 swap them back again when we want to return to the uucp
	 user's permissions.  */
      if (setreuid (ieuid, iuid) < 0)
	{
	  ulog (LOG_ERROR, "setreuid (%ld, %ld): %s",
		(long) ieuid, (long) iuid, strerror (errno));
	  (void) close (qsysdep->o);
	  qsysdep->o = -1;
	  return FALSE;
	}
#else /* ! HAVE_SETREUID */
#if HAVE_SAVED_SETUID
      /* Set the effective user id to the real user id.  Since the
	 effective user id is the saved setuid we will able to set
	 back to it later.  If the real user id is root we will not be
	 able to switch back and forth, but that doesn't matter since
	 we only want to switch once.  */
      if (setuid (iuid) < 0)
	{
	  ulog (LOG_ERROR, "setuid (%ld): %s", (long) iuid,
		strerror (errno));
	  (void) close (qsysdep->o);
	  qsysdep->o = -1;
	  return FALSE;
	}
#else /* ! HAVE_SAVED_SETUID */
      /* There's no way to switch between real permissions and
	 effective permissions.  Just try the bind with the uucp
	 permissions.  */
#endif /* ! HAVE_SAVED_SETUID */
#endif /* ! HAVE_SETREUID */
    }

  if (bind (qsysdep->o, (struct sockaddr *) &s, sizeof s) < 0)
    ulog (LOG_FATAL, "bind: %s", strerror (errno));

  /* Now swap back to the uucp user ID.  */
  if (ieuid != 0)
    {
#if HAVE_SETREUID
      if (setreuid (iuid, ieuid) < 0)
	{
	  ulog (LOG_ERROR, "setreuid (%ld, %ld): %s",
		(long) iuid, (long) ieuid, strerror (errno));
	  (void) close (qsysdep->o);
	  qsysdep->o = -1;
	  return FALSE;
	}
#else /* ! HAVE_SETREUID */
#if HAVE_SAVED_SETUID
      /* Set ourselves back to our original effective user id.  */
      if (setuid ((uid_t) ieuid) < 0)
	{
	  ulog (LOG_ERROR, "setuid (%ld): %s", (long) ieuid,
		strerror (errno));
	  (void) close (qsysdep->o);
	  qsysdep->o = -1;
	  return FALSE;
	}
#else /* ! HAVE_SAVED_SETUID */
      /* We didn't switch, no need to switch back.  */
#endif /* ! HAVE_SAVED_SETUID */
#endif /* ! HAVE_SETREUID */
    }

  if (listen (qsysdep->o, 5) < 0)
    ulog (LOG_FATAL, "listen: %s", strerror (errno));

  while (! FGOT_SIGNAL ())
    {
      size_t clen;
      int onew;
      pid_t ipid;

      DEBUG_MESSAGE0 (DEBUG_PORT,
		      "ftcp_open: Waiting for connections");

      clen = sizeof s;
      onew = accept (qsysdep->o, (struct sockaddr *) &s, &clen);
      if (onew < 0)
	ulog (LOG_FATAL, "accept: %s", strerror (errno));

      DEBUG_MESSAGE0 (DEBUG_PORT,
		      "ftcp_open: Got connection; forking");

      ipid = ixsfork ();
      if (ipid < 0)
	ulog (LOG_FATAL, "fork: %s", strerror (errno));
      if (ipid == 0)
	{
	  (void) close (qsysdep->o);
	  qsysdep->o = onew;

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

      (void) close (onew);

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
ftcp_close (qconn, puuconf, qdialer, fsuccess)
     struct sconnection *qconn;
     pointer puuconf;
     struct uuconf_dialer *qdialer;
     boolean fsuccess;
{
  struct ssysdep_conn *qsysdep;
  boolean fret;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  fret = TRUE;
  if (qsysdep->o >= 0 && close (qsysdep->o) < 0)
    {
      ulog (LOG_ERROR, "close: %s", strerror (errno));
      fret = FALSE;
    }
  qsysdep->o = -1;

  /* If the current pid is not the one we used to open the port, then
     we must have forked up above and we are now the child.  In this
     case, we are being called from within the fendless loop in
     uucico.c.  We return FALSE to force the loop to end and the child
     to exit.  This should be handled in a cleaner fashion.  */
  if (qsysdep->ipid != getpid ())
    fret = FALSE;

  return fret;
}

/* Dial out on a TCP port, so to speak: connect to a remote computer.  */

/*ARGSUSED*/
static boolean
ftcp_dial (qconn, puuconf, qsys, zphone, qdialer, ptdialer)
     struct sconnection *qconn;
     pointer puuconf;
     const struct uuconf_system *qsys;
     const char *zphone;
     struct uuconf_dialer *qdialer;
     enum tdialerfound *ptdialer;
{
  struct ssysdep_conn *qsysdep;
  const char *zhost;
  struct hostent *q;
  struct sockaddr_in s;
  const char *zport;
  char **pzdialer;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

  *ptdialer = DIALERFOUND_FALSE;

  zhost = zphone;
  if (zhost == NULL)
    {
      if (qsys == NULL)
	{
	  ulog (LOG_ERROR, "No address for TCP connection");
	  return FALSE;
	}
      zhost = qsys->uuconf_zname;
    }

  errno = 0;
  q = gethostbyname ((char *) zhost);
  if (q == NULL)
    {
      if (errno == 0)
	ulog (LOG_ERROR, "%s: unknown host name", zhost);
      else
	ulog (LOG_ERROR, "gethostbyname (%s): %s", zhost, strerror (errno));
      return FALSE;
    }

  s.sin_family = q->h_addrtype;
  memcpy (&s.sin_addr.s_addr, q->h_addr, (size_t) q->h_length);
  zport = qconn->qport->uuconf_u.uuconf_stcp.uuconf_zport;
  s.sin_port = itcp_port_number (zport);

  if (connect (qsysdep->o, (struct sockaddr *) &s, sizeof s) < 0)
    {
      ulog (LOG_ERROR, "connect: %s", strerror (errno));
      return FALSE;
    }

  /* Handle the dialer sequence, if any.  */
  pzdialer = qconn->qport->uuconf_u.uuconf_stcp.uuconf_pzdialer;
  if (pzdialer != NULL && *pzdialer != NULL)
    {
      if (! fconn_dial_sequence (qconn, puuconf, pzdialer, qsys, zphone,
				 qdialer, ptdialer))
	return FALSE;
    }

  return TRUE;
}

/* Get the port number given a name.  The argument will almost always
   be "uucp" so we cache that value.  The return value is always in
   network byte order.  This returns -1 on error.  */

static int
itcp_port_number (zname)
     const char *zname;
{
  boolean fuucp;
  static int iuucp;
  int i;
  char *zend;
  struct servent *q;

  fuucp = strcmp (zname, "uucp") == 0;
  if (fuucp && iuucp != 0)
    return iuucp;

  /* Try it as a number first.  */
  i = strtol ((char *) zname, &zend, 10);
  if (i != 0 && *zend == '\0')
    return htons (i);

  q = getservbyname ((char *) zname, (char *) "tcp");
  if (q == NULL)
    {
      /* We know that the "uucp" service should be 540, even if isn't
	 in /etc/services.  */
      if (fuucp)
	{
	  iuucp = htons (IUUCP_PORT);
	  return iuucp;
	}
      ulog (LOG_ERROR, "getservbyname (%s): %s", zname, strerror (errno));
      return -1;
    }

  if (fuucp)
    iuucp = q->s_port;

  return q->s_port;
}

#endif /* HAVE_TCP */
