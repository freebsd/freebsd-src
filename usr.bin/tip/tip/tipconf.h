/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tipconf.h	8.1 (Berkeley) 3/25/95
 */

#ifndef tipconf_h_included
#define tipconf_h_included

/*
	Define constness
*/
#define CONST const

/*
	Specify default bit rate for connections
*/
#define DEFBR 1200

/*
	Default frame size for file transfer buffering of writes
	on local side
*/
#ifndef BUFSIZ
#define DEFFS 1024
#else
#define DEFFS BUFSIZ
#endif

/*
	Enable logging of ACU use
*/
#define ACULOG             1

/*
	Strip phone #s from ACU log file
*/
#define PRISTINE           1

/*
	Enable command to "connect" remote with local process
*/
#define CONNECT            1

/*
	Specify style of UUCP lock files
*/
#define HAVE_V2_LOCKFILES  0
#define HAVE_HDB_LOCKFILES 1

/*
	System has a millisecond based sleep function
*/
#define HAVE_USLEEP        0

/*
	System has select
*/
#define HAVE_SELECT        1

/*
	System has termios tty interface
*/
#define HAVE_TERMIOS       1

/*
	Include configurable modem driver
*/
#define UNIDIALER          1

/*
	Specify builtin modem drivers to include
*/
#define BIZ1031            0
#define BIZ1022            0
#define COURIER            0
#define DF02               0
#define DF03               0
#define DN11               0
#define HAYES              0
#define MULTITECH          0
#define T3000              0
#define V3451              0
#define V831               0
#define VENTEL             0

/*
	Include cu interface so that, when tip is linked to cu and then
	invoked as cu, it behaves like cu.
*/
#define INCLUDE_CU_INTERFACE 0

#endif

/* end of tipconf.h */
