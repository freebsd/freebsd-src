/****************************************************************
Copyright 1990, 1991 by AT&T Bell Laboratories, Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
Bellcore or any of their entities not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

AT&T and Bellcore disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall AT&T or Bellcore be liable for
any special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

/* This file is included at the start of defs.h; this file
 * is an initial attempt to gather in one place some declarations
 * that may need to be tweaked on some systems.
 */

#ifdef __STDC__
#ifndef ANSI_Libraries
#define ANSI_Libraries
#endif
#ifndef ANSI_Prototypes
#define ANSI_Prototypes
#endif
#endif

#ifdef __BORLANDC__
#define MSDOS
extern int ind_printf(), nice_printf();
#endif

#ifdef __ZTC__	/* Zortech */
#define MSDOS
extern int ind_printf(...), nice_printf(...);
#endif

#ifdef MSDOS
#define ANSI_Libraries
#define ANSI_Prototypes
#define LONG_CAST (long)
#else
#define LONG_CAST
#endif

#include <stdio.h>

#ifdef ANSI_Libraries
#include <stddef.h>
#include <stdlib.h>
#else
char *calloc(), *malloc(), *memcpy(), *memset(), *realloc();
typedef int size_t;
#ifdef ANSI_Prototypes
extern double atof(const char *);
#else
extern double atof();
#endif
#endif

#ifdef ANSI_Prototypes
extern char *gmem(int, int);
extern char *mem(int, int);
extern char *Alloc(int);
extern int* ckalloc(int);
#else
extern char *Alloc(), *gmem(), *mem();
int *ckalloc();
#endif

/* On systems like VMS where fopen might otherwise create
 * multiple versions of intermediate files, you may wish to
 * #define scrub(x) unlink(x)
 */
#ifndef scrub
#define scrub(x) /* do nothing */
#endif

/* On systems that severely limit the total size of statically
 * allocated arrays, you may need to change the following to
 *	extern char **chr_fmt, *escapes, **str_fmt;
 * and to modify sysdep.c appropriately
 */
extern char *chr_fmt[], escapes[], *str_fmt[];

#include <string.h>

#include "ctype.h"

#define Table_size 256
/* Table_size should be 1 << (bits/byte) */
