/* Macro definitions for running gdb on a Sun 4 running Solaris 2.
   Copyright 1989, 1992, 1993, 1994, 1995, 1996, 1998, 2000
   Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Pick up more stuff from the generic SVR4 host include file. */

#include "xm-sysv4.h"

/* These are not currently used in SVR4 (but should be, FIXME!).  */
#undef	DO_DEFERRED_STORES
#undef	CLEAR_DEFERRED_STORES

/* solaris doesn't have siginterrupt, though it has sigaction; however,
   in this case siginterrupt would just be setting the default. */
#define NO_SIGINTERRUPT

/* On sol2.7, <curses.h> emits a bunch of 'macro redefined'
   warnings, which makes autoconf think curses.h doesn't
   exist.  Compensate for that here. */
#define HAVE_CURSES_H 1
