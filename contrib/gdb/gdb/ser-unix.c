/* Serial interface for local (hardwired) serial ports on Un*x like systems
   Copyright 1992, 1993, 1994, 1998 Free Software Foundation, Inc.

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
#include <fcntl.h>
#include <sys/types.h>
#include "terminal.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_TERMIOS

struct hardwire_ttystate
{
  struct termios termios;
};
#endif /* termios */

#ifdef HAVE_TERMIO

/* It is believed that all systems which have added job control to SVR3
   (e.g. sco) have also added termios.  Even if not, trying to figure out
   all the variations (TIOCGPGRP vs. TCGETPGRP, etc.) would be pretty
   bewildering.  So we don't attempt it.  */

struct hardwire_ttystate
{
  struct termio termio;
};
#endif /* termio */

#ifdef HAVE_SGTTY
/* Needed for the code which uses select().  We would include <sys/select.h>
   too if it existed on all systems.  */
#include <sys/time.h>

struct hardwire_ttystate
{
  struct sgttyb sgttyb;
  struct tchars tc;
  struct ltchars ltc;
  /* Line discipline flags.  */
  int lmode;
};
#endif /* sgtty */

static int hardwire_open PARAMS ((serial_t scb, const char *name));
static void hardwire_raw PARAMS ((serial_t scb));
static int wait_for PARAMS ((serial_t scb, int timeout));
static int hardwire_readchar PARAMS ((serial_t scb, int timeout));
static int rate_to_code PARAMS ((int rate));
static int hardwire_setbaudrate PARAMS ((serial_t scb, int rate));
static int hardwire_write PARAMS ((serial_t scb, const char *str, int len));
static void hardwire_close PARAMS ((serial_t scb));
static int get_tty_state PARAMS ((serial_t scb, struct hardwire_ttystate *state));
static int set_tty_state PARAMS ((serial_t scb, struct hardwire_ttystate *state));
static serial_ttystate hardwire_get_tty_state PARAMS ((serial_t scb));
static int hardwire_set_tty_state PARAMS ((serial_t scb, serial_ttystate state));
static int hardwire_noflush_set_tty_state PARAMS ((serial_t, serial_ttystate,
						   serial_ttystate));
static void hardwire_print_tty_state PARAMS ((serial_t, serial_ttystate));
static int hardwire_drain_output PARAMS ((serial_t));
static int hardwire_flush_output PARAMS ((serial_t));
static int hardwire_flush_input PARAMS ((serial_t));
static int hardwire_send_break PARAMS ((serial_t));
static int hardwire_setstopbits PARAMS ((serial_t, int));

void _initialize_ser_hardwire PARAMS ((void));

#ifdef __CYGWIN32__
extern void (*ui_loop_hook) PARAMS ((int));
#endif

/* Open up a real live device for serial I/O */

static int
hardwire_open(scb, name)
     serial_t scb;
     const char *name;
{
  scb->fd = open (name, O_RDWR);
  if (scb->fd < 0)
    return -1;

  return 0;
}

static int
get_tty_state (scb, state)
     serial_t scb;
     struct hardwire_ttystate *state;
{
#ifdef HAVE_TERMIOS
  if (tcgetattr(scb->fd, &state->termios) < 0)
    return -1;

  return 0;
#endif

#ifdef HAVE_TERMIO
  if (ioctl (scb->fd, TCGETA, &state->termio) < 0)
    return -1;
  return 0;
#endif

#ifdef HAVE_SGTTY
  if (ioctl (scb->fd, TIOCGETP, &state->sgttyb) < 0)
    return -1;
  if (ioctl (scb->fd, TIOCGETC, &state->tc) < 0)
    return -1;
  if (ioctl (scb->fd, TIOCGLTC, &state->ltc) < 0)
    return -1;
  if (ioctl (scb->fd, TIOCLGET, &state->lmode) < 0)
    return -1;

  return 0;
#endif
}

static int
set_tty_state(scb, state)
     serial_t scb;
     struct hardwire_ttystate *state;
{
#ifdef HAVE_TERMIOS
  if (tcsetattr(scb->fd, TCSANOW, &state->termios) < 0)
    return -1;

  return 0;
#endif

#ifdef HAVE_TERMIO
  if (ioctl (scb->fd, TCSETA, &state->termio) < 0)
    return -1;
  return 0;
#endif

#ifdef HAVE_SGTTY
  if (ioctl (scb->fd, TIOCSETN, &state->sgttyb) < 0)
    return -1;
  if (ioctl (scb->fd, TIOCSETC, &state->tc) < 0)
    return -1;
  if (ioctl (scb->fd, TIOCSLTC, &state->ltc) < 0)
    return -1;
  if (ioctl (scb->fd, TIOCLSET, &state->lmode) < 0)
    return -1;

  return 0;
#endif
}

static serial_ttystate
hardwire_get_tty_state(scb)
     serial_t scb;
{
  struct hardwire_ttystate *state;

  state = (struct hardwire_ttystate *)xmalloc(sizeof *state);

  if (get_tty_state(scb, state))
    return NULL;

  return (serial_ttystate)state;
}

static int
hardwire_set_tty_state(scb, ttystate)
     serial_t scb;
     serial_ttystate ttystate;
{
  struct hardwire_ttystate *state;

  state = (struct hardwire_ttystate *)ttystate;

  return set_tty_state(scb, state);
}

static int
hardwire_noflush_set_tty_state (scb, new_ttystate, old_ttystate)
     serial_t scb;
     serial_ttystate new_ttystate;
     serial_ttystate old_ttystate;
{
  struct hardwire_ttystate new_state;
#ifdef HAVE_SGTTY
  struct hardwire_ttystate *state = (struct hardwire_ttystate *) old_ttystate;
#endif

  new_state = *(struct hardwire_ttystate *)new_ttystate;

  /* Don't change in or out of raw mode; we don't want to flush input.
     termio and termios have no such restriction; for them flushing input
     is separate from setting the attributes.  */

#ifdef HAVE_SGTTY
  if (state->sgttyb.sg_flags & RAW)
    new_state.sgttyb.sg_flags |= RAW;
  else
    new_state.sgttyb.sg_flags &= ~RAW;

  /* I'm not sure whether this is necessary; the manpage just mentions
     RAW not CBREAK.  */
  if (state->sgttyb.sg_flags & CBREAK)
    new_state.sgttyb.sg_flags |= CBREAK;
  else
    new_state.sgttyb.sg_flags &= ~CBREAK;
#endif

  return set_tty_state (scb, &new_state);
}

static void
hardwire_print_tty_state (scb, ttystate)
     serial_t scb;
     serial_ttystate ttystate;
{
  struct hardwire_ttystate *state = (struct hardwire_ttystate *) ttystate;
  int i;

#ifdef HAVE_TERMIOS
  printf_filtered ("c_iflag = 0x%x, c_oflag = 0x%x,\n",
		   state->termios.c_iflag, state->termios.c_oflag);
  printf_filtered ("c_cflag = 0x%x, c_lflag = 0x%x\n",
		   state->termios.c_cflag, state->termios.c_lflag);
#if 0
  /* This not in POSIX, and is not really documented by those systems
     which have it (at least not Sun).  */
  printf_filtered ("c_line = 0x%x.\n", state->termios.c_line);
#endif
  printf_filtered ("c_cc: ");
  for (i = 0; i < NCCS; i += 1)
    printf_filtered ("0x%x ", state->termios.c_cc[i]);
  printf_filtered ("\n");
#endif

#ifdef HAVE_TERMIO
  printf_filtered ("c_iflag = 0x%x, c_oflag = 0x%x,\n",
		   state->termio.c_iflag, state->termio.c_oflag);
  printf_filtered ("c_cflag = 0x%x, c_lflag = 0x%x, c_line = 0x%x.\n",
		   state->termio.c_cflag, state->termio.c_lflag,
		   state->termio.c_line);
  printf_filtered ("c_cc: ");
  for (i = 0; i < NCC; i += 1)
    printf_filtered ("0x%x ", state->termio.c_cc[i]);
  printf_filtered ("\n");
#endif

#ifdef HAVE_SGTTY
  printf_filtered ("sgttyb.sg_flags = 0x%x.\n", state->sgttyb.sg_flags);

  printf_filtered ("tchars: ");
  for (i = 0; i < (int)sizeof (struct tchars); i++)
    printf_filtered ("0x%x ", ((unsigned char *)&state->tc)[i]);
  printf_filtered ("\n");

  printf_filtered ("ltchars: ");
  for (i = 0; i < (int)sizeof (struct ltchars); i++)
    printf_filtered ("0x%x ", ((unsigned char *)&state->ltc)[i]);
  printf_filtered ("\n");

  printf_filtered ("lmode:  0x%x\n", state->lmode);
#endif
}

/* Wait for the output to drain away, as opposed to flushing (discarding) it */

static int
hardwire_drain_output (scb)
     serial_t scb;
{
#ifdef HAVE_TERMIOS
  return tcdrain (scb->fd);
#endif

#ifdef HAVE_TERMIO
  return ioctl (scb->fd, TCSBRK, 1);
#endif

#ifdef HAVE_SGTTY
  /* Get the current state and then restore it using TIOCSETP,
     which should cause the output to drain and pending input
     to be discarded. */
  {
    struct hardwire_ttystate state;
    if (get_tty_state (scb, &state))
      {
	return (-1);
      }
    else
      {
	return (ioctl (scb->fd, TIOCSETP, &state.sgttyb));
      }
  }
#endif  
}

static int
hardwire_flush_output (scb)
     serial_t scb;
{
#ifdef HAVE_TERMIOS
  return tcflush (scb->fd, TCOFLUSH);
#endif

#ifdef HAVE_TERMIO
  return ioctl (scb->fd, TCFLSH, 1);
#endif

#ifdef HAVE_SGTTY
  /* This flushes both input and output, but we can't do better.  */
  return ioctl (scb->fd, TIOCFLUSH, 0);
#endif  
}

static int
hardwire_flush_input (scb)
     serial_t scb;
{
  scb->bufcnt = 0;
  scb->bufp = scb->buf;

#ifdef HAVE_TERMIOS
  return tcflush (scb->fd, TCIFLUSH);
#endif

#ifdef HAVE_TERMIO
  return ioctl (scb->fd, TCFLSH, 0);
#endif

#ifdef HAVE_SGTTY
  /* This flushes both input and output, but we can't do better.  */
  return ioctl (scb->fd, TIOCFLUSH, 0);
#endif  
}

static int
hardwire_send_break (scb)
     serial_t scb;
{
#ifdef HAVE_TERMIOS
  return tcsendbreak (scb->fd, 0);
#endif

#ifdef HAVE_TERMIO
  return ioctl (scb->fd, TCSBRK, 0);
#endif

#ifdef HAVE_SGTTY
  {
    int status;
    struct timeval timeout;

    status = ioctl (scb->fd, TIOCSBRK, 0);

    /* Can't use usleep; it doesn't exist in BSD 4.2.  */
    /* Note that if this select() is interrupted by a signal it will not wait
       the full length of time.  I think that is OK.  */
    timeout.tv_sec = 0;
    timeout.tv_usec = 250000;
    select (0, 0, 0, 0, &timeout);
    status = ioctl (scb->fd, TIOCCBRK, 0);
    return status;
  }
#endif  
}

static void
hardwire_raw(scb)
     serial_t scb;
{
  struct hardwire_ttystate state;

  if (get_tty_state(scb, &state))
    fprintf_unfiltered(gdb_stderr, "get_tty_state failed: %s\n", safe_strerror(errno));

#ifdef HAVE_TERMIOS
  state.termios.c_iflag = 0;
  state.termios.c_oflag = 0;
  state.termios.c_lflag = 0;
  state.termios.c_cflag &= ~(CSIZE|PARENB);
  state.termios.c_cflag |= CLOCAL | CS8;
  state.termios.c_cc[VMIN] = 0;
  state.termios.c_cc[VTIME] = 0;
#endif

#ifdef HAVE_TERMIO
  state.termio.c_iflag = 0;
  state.termio.c_oflag = 0;
  state.termio.c_lflag = 0;
  state.termio.c_cflag &= ~(CSIZE|PARENB);
  state.termio.c_cflag |= CLOCAL | CS8;
  state.termio.c_cc[VMIN] = 0;
  state.termio.c_cc[VTIME] = 0;
#endif

#ifdef HAVE_SGTTY
  state.sgttyb.sg_flags |= RAW | ANYP;
  state.sgttyb.sg_flags &= ~(CBREAK | ECHO);
#endif

  scb->current_timeout = 0;

  if (set_tty_state (scb, &state))
    fprintf_unfiltered(gdb_stderr, "set_tty_state failed: %s\n", safe_strerror(errno));
}

/* Wait for input on scb, with timeout seconds.  Returns 0 on success,
   otherwise SERIAL_TIMEOUT or SERIAL_ERROR.

   For termio{s}, we actually just setup VTIME if necessary, and let the
   timeout occur in the read() in hardwire_read().
 */

static int
wait_for(scb, timeout)
     serial_t scb;
     int timeout;
{
#ifndef __CYGWIN32__
  scb->timeout_remaining = 0;
#endif

#ifdef HAVE_SGTTY
  {
    struct timeval tv;
    fd_set readfds;

    FD_ZERO (&readfds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    FD_SET(scb->fd, &readfds);

    while (1)
      {
	int numfds;

	if (timeout >= 0)
	  numfds = select(scb->fd+1, &readfds, 0, 0, &tv);
	else
	  numfds = select(scb->fd+1, &readfds, 0, 0, 0);

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
#endif	/* HAVE_SGTTY */

#if defined HAVE_TERMIO || defined HAVE_TERMIOS
  if (timeout == scb->current_timeout)
    return 0;

  scb->current_timeout = timeout;

  {
    struct hardwire_ttystate state;

    if (get_tty_state(scb, &state))
      fprintf_unfiltered(gdb_stderr, "get_tty_state failed: %s\n", safe_strerror(errno));

#ifdef HAVE_TERMIOS
    if (timeout < 0)
      {
	/* No timeout.  */
	state.termios.c_cc[VTIME] = 0;
	state.termios.c_cc[VMIN] = 1;
      }
    else
      {
	state.termios.c_cc[VMIN] = 0;
	state.termios.c_cc[VTIME] = timeout * 10;
	if (state.termios.c_cc[VTIME] != timeout * 10)
	  {

	    /* If c_cc is an 8-bit signed character, we can't go 
	       bigger than this.  If it is always unsigned, we could use
	       25.  */

	    scb->current_timeout = 12;
	    state.termios.c_cc[VTIME] = scb->current_timeout * 10;
	    scb->timeout_remaining = timeout - scb->current_timeout;
	  }
      }
#endif

#ifdef HAVE_TERMIO
    if (timeout < 0)
      {
	/* No timeout.  */
	state.termio.c_cc[VTIME] = 0;
	state.termio.c_cc[VMIN] = 1;
      }
    else
      {
	state.termio.c_cc[VMIN] = 0;
	state.termio.c_cc[VTIME] = timeout * 10;
	if (state.termio.c_cc[VTIME] != timeout * 10)
	  {
	    /* If c_cc is an 8-bit signed character, we can't go 
	       bigger than this.  If it is always unsigned, we could use
	       25.  */

	    scb->current_timeout = 12;
	    state.termio.c_cc[VTIME] = scb->current_timeout * 10;
	    scb->timeout_remaining = timeout - scb->current_timeout;
	  }
      }
#endif

    if (set_tty_state (scb, &state))
      fprintf_unfiltered(gdb_stderr, "set_tty_state failed: %s\n", safe_strerror(errno));

    return 0;
  }
#endif	/* HAVE_TERMIO || HAVE_TERMIOS */
}

/* Read a character with user-specified timeout.  TIMEOUT is number of seconds
   to wait, or -1 to wait forever.  Use timeout of 0 to effect a poll.  Returns
   char if successful.  Returns SERIAL_TIMEOUT if timeout expired, EOF if line
   dropped dead, or SERIAL_ERROR for any other error (see errno in that case).  */
static int
hardwire_readchar (scb, timeout)
     serial_t scb;
     int timeout;
{
  int status;
#ifdef __CYGWIN32__
  int t;
#endif

  if (scb->bufcnt-- > 0)
    return *scb->bufp++;

#ifdef __CYGWIN32__
  if (timeout > 0)
    timeout++;
#endif

  while (1)
    {
#ifdef __CYGWIN32__
      t = timeout == 0 ? 0 : 1;
      scb->timeout_remaining = timeout < 0 ? timeout : timeout - t;
      status = wait_for (scb, t);

      /* -2 means disable timer */
      if (ui_loop_hook)
        ui_loop_hook (-2);
#else
      status = wait_for (scb, timeout);
#endif
      if (status < 0)
	return status;

      scb->bufcnt = read (scb->fd, scb->buf, BUFSIZ);

      if (scb->bufcnt <= 0)
	{
	  if (scb->bufcnt == 0)
	    {
	      /* Zero characters means timeout (it could also be EOF, but
		 we don't (yet at least) distinguish).  */
	      if (scb->timeout_remaining > 0)
		{
		  timeout = scb->timeout_remaining;
		  continue;
		}
#ifdef __CYGWIN32__
          else if (scb->timeout_remaining < 0)
            continue;
#endif
	      else
		return SERIAL_TIMEOUT;
	    }
	  else if (errno == EINTR)
	    continue;
	  else
	    return SERIAL_ERROR;	/* Got an error from read */
	}

      scb->bufcnt--;
      scb->bufp = scb->buf;
      return *scb->bufp++;
    }
}

#ifndef B19200
#define B19200 EXTA
#endif

#ifndef B38400
#define B38400 EXTB
#endif

/* Translate baud rates from integers to damn B_codes.  Unix should
   have outgrown this crap years ago, but even POSIX wouldn't buck it.  */

static struct
{
  int rate;
  int code;
}
baudtab[] =
{
  {50, B50},
  {75, B75},
  {110, B110},
  {134, B134},
  {150, B150},
  {200, B200},
  {300, B300},
  {600, B600},
  {1200, B1200},
  {1800, B1800},
  {2400, B2400},
  {4800, B4800},
  {9600, B9600},
  {19200, B19200},
  {38400, B38400},
#ifdef B57600
  {57600, B57600},
#endif
#ifdef B76800
  {76800, B76800},
#endif
#ifdef B115200
  {115200, B115200},
#endif
#ifdef B230400
  {230400, B230400},
#endif
#ifdef B460800
  {460800, B460800},
#endif
  {-1, -1},
};

static int 
rate_to_code(rate)
     int rate;
{
  int i;

  for (i = 0; baudtab[i].rate != -1; i++)
    if (rate == baudtab[i].rate)  
      return baudtab[i].code;

  return -1;
}

static int
hardwire_setbaudrate(scb, rate)
     serial_t scb;
     int rate;
{
  struct hardwire_ttystate state;

  if (get_tty_state(scb, &state))
    return -1;

#ifdef HAVE_TERMIOS
  cfsetospeed (&state.termios, rate_to_code (rate));
  cfsetispeed (&state.termios, rate_to_code (rate));
#endif

#ifdef HAVE_TERMIO
#ifndef CIBAUD
#define CIBAUD CBAUD
#endif

  state.termio.c_cflag &= ~(CBAUD | CIBAUD);
  state.termio.c_cflag |= rate_to_code (rate);
#endif

#ifdef HAVE_SGTTY
  state.sgttyb.sg_ispeed = rate_to_code (rate);
  state.sgttyb.sg_ospeed = rate_to_code (rate);
#endif

  return set_tty_state (scb, &state);
}

static int
hardwire_setstopbits(scb, num)
     serial_t scb;
     int num;
{
  struct hardwire_ttystate state;
  int newbit;

  if (get_tty_state(scb, &state))
    return -1;

  switch (num)
    {
    case SERIAL_1_STOPBITS:
      newbit = 0;
      break;
    case SERIAL_1_AND_A_HALF_STOPBITS:
    case SERIAL_2_STOPBITS:
      newbit = 1;
      break;
    default:
      return 1;
    }

#ifdef HAVE_TERMIOS
  if (!newbit)
    state.termios.c_cflag &= ~CSTOPB;
  else
    state.termios.c_cflag |= CSTOPB; /* two bits */
#endif

#ifdef HAVE_TERMIO
  if (!newbit)
    state.termio.c_cflag &= ~CSTOPB;
  else
    state.termio.c_cflag |= CSTOPB; /* two bits */
#endif

#ifdef HAVE_SGTTY
  return 0;			/* sgtty doesn't support this */
#endif

  return set_tty_state (scb, &state);
}

static int
hardwire_write(scb, str, len)
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
hardwire_close(scb)
     serial_t scb;
{
  if (scb->fd < 0)
    return;

  close(scb->fd);
  scb->fd = -1;
}

static struct serial_ops hardwire_ops =
{
  "hardwire",
  0,
  hardwire_open,
  hardwire_close,
  hardwire_readchar,
  hardwire_write,
  hardwire_flush_output,
  hardwire_flush_input,
  hardwire_send_break,
  hardwire_raw,
  hardwire_get_tty_state,
  hardwire_set_tty_state,
  hardwire_print_tty_state,
  hardwire_noflush_set_tty_state,
  hardwire_setbaudrate,
  hardwire_setstopbits,
  hardwire_drain_output,	/* wait for output to drain */
};

void
_initialize_ser_hardwire ()
{
  serial_add_interface (&hardwire_ops);
}
