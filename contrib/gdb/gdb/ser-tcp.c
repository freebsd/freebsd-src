/* Serial interface for raw TCP connections on Un*x like systems
   Copyright 1992, 1993 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "serial.h"
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include "signals.h"
#include "gdb_string.h"

struct tcp_ttystate
{
  int bogus;
};

static int tcp_open PARAMS ((serial_t scb, const char *name));
static void tcp_raw PARAMS ((serial_t scb));
static int wait_for PARAMS ((serial_t scb, int timeout));
static int tcp_readchar PARAMS ((serial_t scb, int timeout));
static int tcp_setbaudrate PARAMS ((serial_t scb, int rate));
static int tcp_setstopbits PARAMS ((serial_t scb, int num));
static int tcp_write PARAMS ((serial_t scb, const char *str, int len));
/* FIXME: static void tcp_restore PARAMS ((serial_t scb)); */
static void tcp_close PARAMS ((serial_t scb));
static serial_ttystate tcp_get_tty_state PARAMS ((serial_t scb));
static int tcp_set_tty_state PARAMS ((serial_t scb, serial_ttystate state));

/* Open up a raw tcp socket */

static int
tcp_open(scb, name)
     serial_t scb;
     const char *name;
{
  char *port_str;
  int port;
  struct hostent *hostent;
  struct sockaddr_in sockaddr;
  int tmp;
  char hostname[100];
  struct protoent *protoent;
  int i;

  port_str = strchr (name, ':');

  if (!port_str)
    error ("tcp_open: No colon in host name!"); /* Shouldn't ever happen */

  tmp = min (port_str - name, (int) sizeof hostname - 1);
  strncpy (hostname, name, tmp); /* Don't want colon */
  hostname[tmp] = '\000';	/* Tie off host name */
  port = atoi (port_str + 1);

  hostent = gethostbyname (hostname);

  if (!hostent)
    {
      fprintf_unfiltered (gdb_stderr, "%s: unknown host\n", hostname);
      errno = ENOENT;
      return -1;
    }

  for (i = 1; i <= 15; i++)
    {
      scb->fd = socket (PF_INET, SOCK_STREAM, 0);
      if (scb->fd < 0)
	return -1;

      /* Allow rapid reuse of this port. */
      tmp = 1;
      setsockopt (scb->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&tmp, sizeof(tmp));

      /* Enable TCP keep alive process. */
      tmp = 1;
      setsockopt (scb->fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&tmp, sizeof(tmp));

      sockaddr.sin_family = PF_INET;
      sockaddr.sin_port = htons(port);
      memcpy (&sockaddr.sin_addr.s_addr, hostent->h_addr,
	      sizeof (struct in_addr));

      if (!connect (scb->fd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)))
	break;

      close (scb->fd);
      scb->fd = -1;

/* We retry for ECONNREFUSED because that is often a temporary condition, which
   happens when the server is being restarted.  */

      if (errno != ECONNREFUSED)
	return -1;

      sleep (1);
    }

  protoent = getprotobyname ("tcp");
  if (!protoent)
    return -1;

  tmp = 1;
  if (setsockopt (scb->fd, protoent->p_proto, TCP_NODELAY,
		  (char *)&tmp, sizeof(tmp)))
    return -1;

  signal(SIGPIPE, SIG_IGN);	/* If we don't do this, then GDB simply exits
				   when the remote side dies.  */

  return 0;
}

static serial_ttystate
tcp_get_tty_state(scb)
     serial_t scb;
{
  struct tcp_ttystate *state;

  state = (struct tcp_ttystate *)xmalloc(sizeof *state);

  return (serial_ttystate)state;
}

static int
tcp_set_tty_state(scb, ttystate)
     serial_t scb;
     serial_ttystate ttystate;
{
  struct tcp_ttystate *state;

  state = (struct tcp_ttystate *)ttystate;

  return 0;
}

static int
tcp_return_0 (scb)
     serial_t scb;
{
  return 0;
}

static void
tcp_raw(scb)
     serial_t scb;
{
  return;			/* Always in raw mode */
}

/* Wait for input on scb, with timeout seconds.  Returns 0 on success,
   otherwise SERIAL_TIMEOUT or SERIAL_ERROR.

   For termio{s}, we actually just setup VTIME if necessary, and let the
   timeout occur in the read() in tcp_read().
 */

static int
wait_for(scb, timeout)
     serial_t scb;
     int timeout;
{
  int numfds;
  struct timeval tv;
  fd_set readfds, exceptfds;

  FD_ZERO (&readfds);
  FD_ZERO (&exceptfds);

  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  FD_SET(scb->fd, &readfds);
  FD_SET(scb->fd, &exceptfds);

  while (1)
    {
      if (timeout >= 0)
	numfds = select(scb->fd+1, &readfds, 0, &exceptfds, &tv);
      else
	numfds = select(scb->fd+1, &readfds, 0, &exceptfds, 0);

      if (numfds <= 0)
	if (numfds == 0)
	  return SERIAL_TIMEOUT;
	else if (errno == EINTR)
	  continue;
	else
	  return SERIAL_ERROR;	/* Got an error from select or poll */

      return 0;
    }
}

/* Read a character with user-specified timeout.  TIMEOUT is number of seconds
   to wait, or -1 to wait forever.  Use timeout of 0 to effect a poll.  Returns
   char if successful.  Returns -2 if timeout expired, EOF if line dropped
   dead, or -3 for any other error (see errno in that case). */

static int
tcp_readchar(scb, timeout)
     serial_t scb;
     int timeout;
{
  int status;

  if (scb->bufcnt-- > 0)
    return *scb->bufp++;

  status = wait_for(scb, timeout);

  if (status < 0)
    return status;

  while (1)
    {
      scb->bufcnt = read(scb->fd, scb->buf, BUFSIZ);
      if (scb->bufcnt != -1 || errno != EINTR)
	break;
    }

  if (scb->bufcnt <= 0)
    if (scb->bufcnt == 0)
      return SERIAL_TIMEOUT;	/* 0 chars means timeout [may need to
				   distinguish between EOF & timeouts
				   someday] */
    else
      return SERIAL_ERROR;	/* Got an error from read */

  scb->bufcnt--;
  scb->bufp = scb->buf;
  return *scb->bufp++;
}

static int
tcp_noflush_set_tty_state (scb, new_ttystate, old_ttystate)
     serial_t scb;
     serial_ttystate new_ttystate;
     serial_ttystate old_ttystate;
{
  return 0;
}

static void
tcp_print_tty_state (scb, ttystate)
     serial_t scb;
     serial_ttystate ttystate;
{
  /* Nothing to print.  */
  return;
}

static int
tcp_setbaudrate(scb, rate)
     serial_t scb;
     int rate;
{
  return 0;			/* Never fails! */
}

static int
tcp_setstopbits(scb, num)
     serial_t scb;
     int num;
{
  return 0;			/* Never fails! */
}

static int
tcp_write(scb, str, len)
     serial_t scb;
     const char *str;
     int len;
{
  int cc;

  while (len > 0)
    {
      cc = write(scb->fd, str, len);

      if (cc < 0)
	return 1;
      len -= cc;
      str += cc;
    }
  return 0;
}

static void
tcp_close(scb)
     serial_t scb;
{
  if (scb->fd < 0)
    return;

  close(scb->fd);
  scb->fd = -1;
}

static struct serial_ops tcp_ops =
{
  "tcp",
  0,
  tcp_open,
  tcp_close,
  tcp_readchar,
  tcp_write,
  tcp_return_0, /* flush output */
  tcp_return_0, /* flush input */
  tcp_return_0, /* send break */
  tcp_raw,
  tcp_get_tty_state,
  tcp_set_tty_state,
  tcp_print_tty_state,
  tcp_noflush_set_tty_state,
  tcp_setbaudrate,
  tcp_setstopbits,
};

void
_initialize_ser_tcp ()
{
  serial_add_interface (&tcp_ops);
}
