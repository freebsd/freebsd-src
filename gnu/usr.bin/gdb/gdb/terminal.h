/* Terminal interface definitions for GDB, the GNU Debugger.
   Copyright 1986, 1989, 1991, 1992 Free Software Foundation, Inc.

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

#if !defined (TERMINAL_H)
#define TERMINAL_H 1

#if !defined(__GO32__) && !defined (HAVE_TERMIOS)

/* Define a common set of macros -- BSD based -- and redefine whatever
   the system offers to make it look like that.  FIXME: serial.h and
   ser-*.c deal with this in a much cleaner fashion; as soon as stuff
   is converted to use them, can get rid of this crap.  */

#ifdef HAVE_TERMIO

#include <termio.h>

#undef TIOCGETP
#define TIOCGETP TCGETA
#undef TIOCSETN
#define TIOCSETN TCSETA
#undef TIOCSETP
#define TIOCSETP TCSETAF
#define TERMINAL struct termio

#else /* sgtty */

#include <fcntl.h>
#include <sgtty.h>
#include <sys/ioctl.h>
#define TERMINAL struct sgttyb

#endif /* sgtty */
#endif /* termio or sgtty */

extern void new_tty PARAMS ((void));

/* Do we have job control?  Can be assumed to always be the same within
   a given run of GDB.  In inflow.c.  */
extern int job_control;

/* Set the process group of the caller to its own pid, or do nothing if
   we lack job control.  */
extern int gdb_setpgid PARAMS ((void));

#endif	/* !defined (TERMINAL_H) */
