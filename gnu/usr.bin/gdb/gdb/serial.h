/* Remote serial support interface definitions for GDB, the GNU Debugger.
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

#ifndef SERIAL_H
#define SERIAL_H

/* Terminal state pointer.  This is specific to each type of interface. */

typedef PTR serial_ttystate;

struct _serial_t
{
  int fd;			/* File descriptor */
  struct serial_ops *ops;	/* Function vector */
  serial_ttystate ttystate;	/* Not used (yet) */
  int bufcnt;			/* Amount of data in receive buffer */
  unsigned char *bufp;		/* Current byte */
  unsigned char buf[BUFSIZ];	/* Da buffer itself */
  int current_timeout;		/* (termio{s} only), last value of VTIME */
  /* ser-unix.c termio{,s} only, we still need to wait for this many more
     seconds.  */
  int timeout_remaining;
};

typedef struct _serial_t *serial_t;

struct serial_ops {
  char *name;
  struct serial_ops *next;
  int (*open) PARAMS ((serial_t, const char *name));
  void (*close) PARAMS ((serial_t));
  int (*readchar) PARAMS ((serial_t, int timeout));
  int (*write) PARAMS ((serial_t, const char *str, int len));
  int (*flush_output) PARAMS ((serial_t));
  int (*flush_input) PARAMS ((serial_t));
  int (*send_break) PARAMS ((serial_t));
  void (*go_raw) PARAMS ((serial_t));
  serial_ttystate (*get_tty_state) PARAMS ((serial_t));
  int (*set_tty_state) PARAMS ((serial_t, serial_ttystate));
  void (*print_tty_state) PARAMS ((serial_t, serial_ttystate));
  int (*noflush_set_tty_state)
    PARAMS ((serial_t, serial_ttystate, serial_ttystate));
  int (*setbaudrate) PARAMS ((serial_t, int rate));
};

/* Add a new serial interface to the interface list */

void serial_add_interface PARAMS ((struct serial_ops *optable));

serial_t serial_open PARAMS ((const char *name));

serial_t serial_fdopen PARAMS ((const int fd));

/* For most routines, if a failure is indicated, then errno should be
   examined.  */

/* Try to open NAME.  Returns a new serial_t on success, NULL on failure.
 */

#define SERIAL_OPEN(NAME) serial_open(NAME)

/* Open a new serial stream using a file handle.  */

#define SERIAL_FDOPEN(FD) serial_fdopen(FD)

/* Flush pending output.  Might also flush input (if this system can't flush
   only output).  */

#define SERIAL_FLUSH_OUTPUT(SERIAL_T) \
  ((SERIAL_T)->ops->flush_output((SERIAL_T)))

/* Flush pending input.  Might also flush output (if this system can't flush
   only input).  */

#define SERIAL_FLUSH_INPUT(SERIAL_T)\
  ((*(SERIAL_T)->ops->flush_input) ((SERIAL_T)))

/* Send a break between 0.25 and 0.5 seconds long.  */

#define SERIAL_SEND_BREAK(SERIAL_T) \
  ((*(SERIAL_T)->ops->send_break) (SERIAL_T))

/* Turn the port into raw mode. */

#define SERIAL_RAW(SERIAL_T) (SERIAL_T)->ops->go_raw((SERIAL_T))

/* Return a pointer to a newly malloc'd ttystate containing the state
   of the tty.  */
#define SERIAL_GET_TTY_STATE(SERIAL_T) (SERIAL_T)->ops->get_tty_state((SERIAL_T))

/* Set the state of the tty to TTYSTATE.  The change is immediate.
   When changing to or from raw mode, input might be discarded.
   Returns 0 for success, negative value for error (in which case errno
   contains the error).  */
#define SERIAL_SET_TTY_STATE(SERIAL_T, TTYSTATE) (SERIAL_T)->ops->set_tty_state((SERIAL_T), (TTYSTATE))

/* printf_filtered a user-comprehensible description of ttystate.  */
#define SERIAL_PRINT_TTY_STATE(SERIAL_T, TTYSTATE) \
  ((*((SERIAL_T)->ops->print_tty_state)) ((SERIAL_T), (TTYSTATE)))

/* Set the tty state to NEW_TTYSTATE, where OLD_TTYSTATE is the
   current state (generally obtained from a recent call to
   SERIAL_GET_TTY_STATE), but be careful not to discard any input.
   This means that we never switch in or out of raw mode, even
   if NEW_TTYSTATE specifies a switch.  */
#define SERIAL_NOFLUSH_SET_TTY_STATE(SERIAL_T, NEW_TTYSTATE, OLD_TTYSTATE) \
  ((*((SERIAL_T)->ops->noflush_set_tty_state)) \
    ((SERIAL_T), (NEW_TTYSTATE), (OLD_TTYSTATE)))

/* Read one char from the serial device with TIMEOUT seconds to wait
   or -1 to wait forever.  Use timeout of 0 to effect a poll. Returns
   char if ok, else one of the following codes.  Note that all error
   codes are guaranteed to be < 0.  */

#define SERIAL_ERROR -1		/* General error, see errno for details */
#define SERIAL_TIMEOUT -2
#define SERIAL_EOF -3

#define SERIAL_READCHAR(SERIAL_T, TIMEOUT) ((SERIAL_T)->ops->readchar((SERIAL_T), TIMEOUT))

/* Set the baudrate to the decimal value supplied.  Returns 0 for success,
   -1 for failure.  */

#define SERIAL_SETBAUDRATE(SERIAL_T, RATE) ((SERIAL_T)->ops->setbaudrate((SERIAL_T), RATE))

/* Write LEN chars from STRING to the port SERIAL_T.  Returns 0 for
   success, non-zero for failure.  */

#define SERIAL_WRITE(SERIAL_T, STRING, LEN) ((SERIAL_T)->ops->write((SERIAL_T), STRING, LEN))

/* Push out all buffers, close the device and destroy SERIAL_T. */

void serial_close PARAMS ((serial_t));

#define SERIAL_CLOSE(SERIAL_T) serial_close(SERIAL_T)

/* Destroy SERIAL_T without doing the rest of the stuff that SERIAL_CLOSE
   does.  */

#define SERIAL_UN_FDOPEN(SERIAL_T) (free (SERIAL_T))

#endif /* SERIAL_H */
