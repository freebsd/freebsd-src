
/* This file contains code for X-CHESS.
   Copyright (C) 1986 Free Software Foundation, Inc.

This file is part of X-CHESS.

X-CHESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the X-CHESS General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
X-CHESS, but only under the conditions described in the
X-CHESS General Public License.   A copy of this license is
supposed to have been given to you along with X-CHESS so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


/* RCS Info: $Revision: 1.2 $ on $Date: 86/11/23 17:18:32 $
 *		   $Source: /users/faustus/xchess/RCS/std.h,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *
 * Standard definitions.
 */

#define UNIX
#define BSD

#ifndef FILE
#include <stdio.h>
#endif
#ifndef isalpha
#include <ctype.h>
#endif
#ifndef HUGE
#include <math.h>
#endif
#include <strings.h>

typedef int bool;

#define false 0
#define true 1

/* Externs defined in std.c */

extern char *tmalloc();
extern char *trealloc();
extern char *copy();
extern char *datestring();
extern char *getusername();
extern char *gethome();
extern char *gettok();
extern char *tildexpand();
extern void fatal();
extern void setenv();
extern void appendc();
extern int scannum();
extern int seconds();
extern bool prefix();
extern bool ciprefix();
extern bool cieq();
extern bool substring();

/* Externs from libc */

extern char *getenv();
extern int errno;
extern char *sys_errlist[];

/* Should use BSIZE instead of BUFSIZ... */

#define BSIZE	   512

/* Some standard macros. */

#define eq(a,b)	 (!strcmp((a), (b)))
#define isalphanum(c)   (isalpha(c) || isdigit(c))
#define alloc(strname)  ((struct strname *) tmalloc(sizeof(struct strname)))
#define tfree(ptr)  { if (ptr) free((char *) ptr); ptr = 0; }
#define hexnum(c) ((((c) >= '0') && ((c) <= '9')) ? ((c) - '0') : ((((c) >= \
		'a') && ((c) <= 'f')) ? ((c) - 'a' + 10) : ((((c) >= 'A') && \
		((c) <= 'F')) ? ((c) - 'A' + 10) : 0)))

#ifndef BSD
#define random rand
#define srandom srand
#endif BSD

#ifdef VMS

#define EXIT_NORMAL	1
#define EXIT_BAD	0

#else VMS

#define EXIT_NORMAL	0
#define EXIT_BAD	1

#endif VMS

