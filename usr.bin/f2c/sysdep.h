/****************************************************************
Copyright 1990, 1991, 1994 by AT&T, Lucent Technologies and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T, Bell Laboratories,
Lucent or Bellcore or any of their entities not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

AT&T, Lucent and Bellcore disclaim all warranties with regard to
this software, including all implied warranties of
merchantability and fitness.  In no event shall AT&T, Lucent or
Bellcore be liable for any special, indirect or consequential
damages or any damages whatsoever resulting from loss of use,
data or profits, whether in an action of contract, negligence or
other tortious action, arising out of or in connection with the
use or performance of this software.
****************************************************************/

/* This file is included at the start of defs.h; this file
 * is an initial attempt to gather in one place some declarations
 * that may need to be tweaked on some systems.
 */

#ifdef __STDC__
#undef KR_headers
#endif

#ifndef KR_headers
#ifndef ANSI_Libraries
#define ANSI_Libraries
#endif
#ifndef ANSI_Prototypes
#define ANSI_Prototypes
#endif
#endif

#ifdef __BORLANDC__
#define MSDOS
#endif

#ifdef __ZTC__	/* Zortech */
#define MSDOS
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
#ifndef atol
    long atol();
#endif

#ifdef ANSI_Prototypes
extern double atof(const char *);
extern double strtod(const char*, char**);
#else
extern double atof(), strtod();
#endif
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

#define Bits_per_Byte 8
#define Table_size (1 << Bits_per_Byte)
