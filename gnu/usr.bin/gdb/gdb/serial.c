/* Generic serial interface routines
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "serial.h"

/* Linked list of serial I/O handlers */

static struct serial_ops *serial_ops_list = NULL;

/* This is the last serial stream opened.  Used by connect command. */

static serial_t last_serial_opened = NULL;

static struct serial_ops *
serial_interface_lookup (name)
     char *name;
{
  struct serial_ops *ops;

  for (ops = serial_ops_list; ops; ops = ops->next)
    if (strcmp (name, ops->name) == 0)
      return ops;

  return NULL;
}

void
serial_add_interface(optable)
     struct serial_ops *optable;
{
  optable->next = serial_ops_list;
  serial_ops_list = optable;
}

/* Open up a device or a network socket, depending upon the syntax of NAME. */

serial_t
serial_open (name)
     const char *name;
{
  serial_t scb;
  struct serial_ops *ops;

  if (strcmp (name, "pc") == 0)
    ops = serial_interface_lookup ("pc");
  else if (strchr (name, ':'))
    ops = serial_interface_lookup ("tcp");
  else
    ops = serial_interface_lookup ("hardwire");

  if (!ops)
    return NULL;

  scb = (serial_t)xmalloc (sizeof (struct _serial_t));

  scb->ops = ops;

  scb->bufcnt = 0;
  scb->bufp = scb->buf;

  if (scb->ops->open(scb, name))
    {
      free (scb);
      return NULL;
    }

  last_serial_opened = scb;

  return scb;
}

serial_t
serial_fdopen(fd)
     const int fd;
{
  serial_t scb;
  struct serial_ops *ops;

  ops = serial_interface_lookup ("hardwire");

  if (!ops)
    return NULL;

  scb = (serial_t)xmalloc (sizeof (struct _serial_t));

  scb->ops = ops;

  scb->bufcnt = 0;
  scb->bufp = scb->buf;

  scb->fd = fd;

  last_serial_opened = scb;

  return scb;
}

void
serial_close(scb)
     serial_t scb;
{
  last_serial_opened = NULL;

/* This is bogus.  It's not our fault if you pass us a bad scb...!  Rob, you
   should fix your code instead.  */

  if (!scb)
    return;

  scb->ops->close(scb);
  free(scb);
}

#if 0
/*
The connect command is #if 0 because I hadn't thought of an elegant
way to wait for I/O on two serial_t's simultaneously.  Two solutions
came to mind:

	1) Fork, and have have one fork handle the to user direction,
	   and have the other hand the to target direction.  This
	   obviously won't cut it for MSDOS.

	2) Use something like select.  This assumes that stdin and
	   the target side can both be waited on via the same
	   mechanism.  This may not be true for DOS, if GDB is
	   talking to the target via a TCP socket.
-grossman, 8 Jun 93
*/

/* Connect the user directly to the remote system.  This command acts just like
   the 'cu' or 'tip' command.  Use <CR>~. or <CR>~^D to break out.  */

static serial_t tty_desc;		/* Controlling terminal */

static void
cleanup_tty(ttystate)
     serial_ttystate ttystate;
{
  printf_unfiltered ("\r\n[Exiting connect mode]\r\n");
  SERIAL_SET_TTY_STATE (tty_desc, ttystate);
  free (ttystate);
  SERIAL_CLOSE (tty_desc);
}

static void
connect_command (args, fromtty)
     char	*args;
     int	fromtty;
{
  int c;
  char cur_esc = 0;
  serial_ttystate ttystate;
  serial_t port_desc;		/* TTY port */

  dont_repeat();

  if (args)
    fprintf_unfiltered(gdb_stderr, "This command takes no args.  They have been ignored.\n");
	
  printf_unfiltered("[Entering connect mode.  Use ~. or ~^D to escape]\n");

  tty_desc = SERIAL_FDOPEN (0);
  port_desc = last_serial_opened;

  ttystate = SERIAL_GET_TTY_STATE (tty_desc);

  SERIAL_RAW (tty_desc);
  SERIAL_RAW (port_desc);

  make_cleanup (cleanup_tty, ttystate);

  while (1)
    {
      int mask;

      mask = SERIAL_WAIT_2 (tty_desc, port_desc, -1);

      if (mask & 2)
	{			/* tty input */
	  char cx;

	  while (1)
	    {
	      c = SERIAL_READCHAR(tty_desc, 0);

	      if (c == SERIAL_TIMEOUT)
		  break;

	      if (c < 0)
		perror_with_name("connect");

	      cx = c;
	      SERIAL_WRITE(port_desc, &cx, 1);

	      switch (cur_esc)
		{
		case 0:
		  if (c == '\r')
		    cur_esc = c;
		  break;
		case '\r':
		  if (c == '~')
		    cur_esc = c;
		  else
		    cur_esc = 0;
		  break;
		case '~':
		  if (c == '.' || c == '\004')
		    return;
		  else
		    cur_esc = 0;
		}
	    }
	}

      if (mask & 1)
	{			/* Port input */
	  char cx;

	  while (1)
	    {
	      c = SERIAL_READCHAR(port_desc, 0);

	      if (c == SERIAL_TIMEOUT)
		  break;

	      if (c < 0)
		perror_with_name("connect");

	      cx = c;

	      SERIAL_WRITE(tty_desc, &cx, 1);
	    }
	}
    }
}
#endif /* 0 */

void
_initialize_serial ()
{
#if 0
  add_com ("connect", class_obscure, connect_command,
	   "Connect the terminal directly up to the command monitor.\n\
Use <CR>~. or <CR>~^D to break out.");
#endif /* 0 */
}
