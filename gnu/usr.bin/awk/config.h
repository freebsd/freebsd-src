/*
 * config.h -- configuration definitions for gawk.
 *
 * For generic 4.4 alpha
 */

/* 
 * Copyright (C) 1991, 1992, 1993 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Progamming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GAWK; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * This file isolates configuration dependencies for gnu awk.
 * You should know something about your system, perhaps by having
 * a manual handy, when you edit this file.  You should copy config.h-dist
 * to config.h, and edit config.h.  Do not modify config.h-dist, so that
 * it will be easy to apply any patches that may be distributed.
 *
 * The general idea is that systems conforming to the various standards
 * should need to do the least amount of changing.  Definining the various
 * items in ths file usually means that your system is missing that
 * particular feature.
 *
 * The order of preference in standard conformance is ANSI C, POSIX,
 * and the SVID.
 *
 * If you have no clue as to what's going on with your system, try
 * compiling gawk without editing this file and see what shows up
 * missing in the link stage.  From there, you can probably figure out
 * which defines to turn on.
 */

/**************************/
/* Miscellanious features */
/**************************/

/*
 * BLKSIZE_MISSING
 *
 * Check your /usr/include/sys/stat.h file.  If the stat structure
 * does not have a member named st_blksize, define this.  (This will
 * most likely be the case on most System V systems prior to V.4.)
 */
/* #define BLKSIZE_MISSING	1 */

/*
 * SIGTYPE
 *
 * The return type of the routines passed to the signal function.
 * Modern systems use `void', older systems use `int'.
 * If left undefined, it will default to void.
 */
/* #define SIGTYPE	int */

/*
 * SIZE_T_MISSING
 *
 * If your system has no typedef for size_t, define this to get a default
 */
/* #define	SIZE_T_MISSING	1 */

/*
 * CHAR_UNSIGNED
 *
 * If your machine uses unsigned characters (IBM RT and RS/6000 and others)
 * then define this for use in regex.c
 */
/* #define CHAR_UNSIGNED	1 */

/*
 * HAVE_UNDERSCORE_SETJMP
 *
 * Check in your /usr/include/setjmp.h file.  If there are routines
 * there named _setjmp and _longjmp, then you should define this.
 * Typically only systems derived from Berkeley Unix have this.
 */
#define	HAVE_UNDERSCORE_SETJMP	1

/*
 * LIMITS_H_MISSING
 *
 * You don't have a <limits.h> include file.
 */
/* #define LIMITS_H_MISSING	1 */

/***********************************************/
/* Missing library subroutines or system calls */
/***********************************************/

/*
 * MEMCMP_MISSING
 * MEMCPY_MISSING
 * MEMSET_MISSING
 *
 * These three routines are for manipulating blocks of memory. Most
 * likely they will either all three be present or all three be missing,
 * so they're grouped together.
 */
/* #define MEMCMP_MISSING	1 */
/* #define MEMCPY_MISSING	1 */
/* #define MEMSET_MISSING	1 */

/*
 * RANDOM_MISSING
 *
 * Your system does not have the random(3) suite of random number
 * generating routines.  These are different than the old rand(3)
 * routines!
 */
/* #define RANDOM_MISSING	1 */

/*
 * STRCASE_MISSING
 *
 * Your system does not have the strcasemp() and strncasecmp()
 * routines that originated in Berkeley Unix.
 */
/* #define STRCASE_MISSING	1 */

/*
 * STRCHR_MISSING
 *
 * Your system does not have the strchr() and strrchr() functions.
 */
/* #define STRCHR_MISSING	1 */

/*
 * STRERROR_MISSING
 *
 * Your system lacks the ANSI C strerror() routine for returning the
 * strings associated with errno values.
 */
/* #define STRERROR_MISSING	1 */

/*
 * STRTOD_MISSING
 *
 * Your system does not have the strtod() routine for converting
 * strings to double precision floating point values.
 */
/* #define STRTOD_MISSING	1 */

/*
 * STRFTIME_MISSING
 *
 * Your system lacks the ANSI C strftime() routine for formatting
 * broken down time values.
 */
/* #define STRFTIME_MISSING	1 */

/*
 * TZSET_MISSING
 *
 * If you have a 4.2 BSD vintage system, then the strftime() routine
 * supplied in the missing directory won't be enough, because it relies on the
 * tzset() routine from System V / Posix.  Fortunately, there is an
 * emulation for tzset() too that should do the trick.  If you don't
 * have tzset(), define this.
 */
/* #define TZSET_MISSING	1 */

/*
 * TZNAME_MISSING
 *
 * Some systems do not support the external variables tzname and daylight.
 * If this is the case *and* strftime() is missing, define this.
 */
/* #define TZNAME_MISSING	1 */

/*
 * TM_ZONE_MISSING
 *
 * Your "struct tm" is missing the tm_zone field.
 * If this is the case *and* strftime() is missing *and* tzname is missing,
 * define this.
 */
/* #define TM_ZONE_MISSING	1 */

/*
 * STDC_HEADERS
 *
 * If your system does have ANSI compliant header files that
 * provide prototypes for library routines, then define this.
 */
#define	STDC_HEADERS    1

/*
 * NO_TOKEN_PASTING
 *
 * If your compiler define's __STDC__ but does not support token
 * pasting (tok##tok), then define this.
 */
/* #define NO_TOKEN_PASTING	1 */

/*****************************************************************/
/* Stuff related to the Standard I/O Library.			 */
/*****************************************************************/
/* Much of this is (still, unfortunately) black magic in nature. */
/* You may have to use some or all of these together to get gawk */
/* to work correctly.						 */
/*****************************************************************/

/*
 * NON_STD_SPRINTF
 *
 * Look in your /usr/include/stdio.h file.  If the return type of the
 * sprintf() function is NOT `int', define this.
 */
/* #define NON_STD_SPRINTF	1 */

/*
 * VPRINTF_MISSING
 *
 * Define this if your system lacks vprintf() and the other routines
 * that go with it.  This will trigger an attempt to use _doprnt().
 * If you don't have that, this attempt will fail and you are on your own.
 */
/* #define VPRINTF_MISSING	1 */

/*
 * Casts from size_t to int and back.  These will become unnecessary
 * at some point in the future, but for now are required where the
 * two types are a different representation.
 */
/* #define SZTC */
/* #define INTC */

/*
 * SYSTEM_MISSING
 *
 * Define this if your library does not provide a system function
 * or you are not entirely happy with it and would rather use
 * a provided replacement (atari only).
 */
/* #define SYSTEM_MISSING   1 */

/*
 * FMOD_MISSING
 *
 * Define this if your system lacks the fmod() function and modf() will
 * be used instead.
 */
/* #define FMOD_MISSING	1 */


/*******************************/
/* Gawk configuration options. */
/*******************************/

/*
 * DEFPATH
 *
 * The default search path for the -f option of gawk.  It is used
 * if the AWKPATH environment variable is undefined.  The default
 * definition is provided here.  Most likely you should not change
 * this.
 */

/* #define DEFPATH	".:/usr/lib/awk:/usr/local/lib/awk" */
/* #define ENVSEP	':' */

/*
 * alloca already has a prototype defined - don't redefine it
 */
#define	ALLOCA_PROTO	1

/*
 * srandom already has a prototype defined - don't redefine it
 */
#define	SRANDOM_PROTO	1

/*
 * getpgrp() in sysvr4 and POSIX takes no argument
 */
/* #define GETPGRP_NOARG	0 */

/*
 * define const to nothing if not __STDC__
 */
#ifndef __STDC__
#define const
#endif

/* If svr4 and not gcc */
/* #define SVR4		0 */
#ifdef SVR4
#define __svr4__	1
#endif

/* anything that follows is for system-specific short-term kludges */
