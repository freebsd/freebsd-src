/* Host definitions for GDB running on an a29k NYU Ultracomputer
   Copyright (C) 1986, 1987, 1989, 1991 Free Software Foundation, Inc.
   Contributed by David Wood (wood@lab.ultra.nyu.edu).

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

/* Here at NYU we have what we call an ULTRA3 PE board.  So
   ifdefs for ULTRA3 are my doing.  At this point in time,
   I don't know of any other Unixi running on the a29k.  */

#define HOST_BYTE_ORDER BIG_ENDIAN

#define HAVE_WAIT_STRUCT

#ifndef L_SET
# define L_SET   0 /* set the seek pointer */
# define L_INCR  1 /* increment the seek pointer */
# define L_XTND  2 /* extend the file size */
#endif

#ifndef O_RDONLY
# define O_RDONLY 0
# define O_WRONLY 1
# define O_RDWR	  2
#endif

#ifndef F_OK
# define R_OK 4
# define W_OK 2
# define X_OK 1
# define F_OK 0
#endif

/* System doesn't provide siginterrupt().  */
#define	NO_SIGINTERRUPT

/* System uses a `short' to hold a process group ID.  */
#define	SHORT_PGRP
