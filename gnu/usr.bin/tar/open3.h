/* Defines for Sys V style 3-argument open call.
   Copyright (C) 1988 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * open3.h -- #defines for the various flags for the Sys V style 3-argument
 * open() call.  On BSD or System 5, the system already has this in an
 * include file.  This file is needed for V7 and MINIX systems for the
 * benefit of open3() in port.c, a routine that emulates the 3-argument
 * call using system calls available on V7/MINIX.
 *
 * This file is needed by PD tar even if we aren't using the
 * emulator, since the #defines for O_WRONLY, etc. are used in
 * a couple of places besides the open() calls, (e.g. in the assignment
 * to openflag in extract.c).  We just #include this rather than
 * #ifdef them out.
 *
 * Written 6/10/87 by rmtodd@uokmax (Richard Todd).
 *
 * The names have been changed by John Gilmore, 31 July 1987, since
 * Richard called it "bsdopen", and really this change was introduced in
 * AT&T Unix systems before BSD picked it up.
 */

/* Only one of the next three should be specified */
#define O_RDONLY	 0	/* only allow read */
#define	O_WRONLY	 1	/* only allow write */
#define	O_RDWR		 2	/* both are allowed */

/* The rest of these can be OR-ed in to the above. */
/*
 * O_NDELAY isn't implemented by the emulator.  It's only useful (to tar) on
 * systems that have named pipes anyway; it prevents tar's hanging by
 * opening a named pipe.  We #ifndef it because some systems already have
 * it defined.
 */
#ifndef O_NDELAY
#define O_NDELAY	 4	/* don't block on opening devices that would
			    * block on open -- ignored by emulator. */
#endif
#define O_CREAT		 8	/* create file if needed */
#define O_EXCL		16	/* file cannot already exist */
#define O_TRUNC		32	/* truncate file on open */
#define O_APPEND	64	/* always write at end of file -- ignored by emul */

#ifdef EMUL_OPEN3
/*
 * make emulation transparent to rest of file -- redirect all open() calls
 * to our routine
 */
#define open	open3
#endif
