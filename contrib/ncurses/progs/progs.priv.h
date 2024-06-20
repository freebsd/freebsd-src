/****************************************************************************
 * Copyright 2019-2022,2024 Thomas E. Dickey                                *
 * Copyright 1998-2015,2017 Free Software Foundation, Inc.                  *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Thomas E. Dickey                    1997-on                     *
 ****************************************************************************/
/*
 * $Id: progs.priv.h,v 1.62 2024/04/08 17:28:28 tom Exp $
 *
 *	progs.priv.h
 *
 *	Header file for curses utility programs
 */

#ifndef PROGS_PRIV_H
#define PROGS_PRIV_H 1

#include <curses.priv.h>

#include <ctype.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
# if defined(_FILE_OFFSET_BITS) && defined(HAVE_STRUCT_DIRENT64)
#  if !defined(_LP64) && (_FILE_OFFSET_BITS == 64)
#   define	DIRENT	struct dirent64
#  else
#   define	DIRENT	struct dirent
#  endif
# else
#  define	DIRENT	struct dirent
# endif
#else
# define DIRENT struct direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#if HAVE_GETOPT_H
#include <getopt.h>
#elif !defined(HAVE_GETOPT_HEADER)
/* 'getopt()' may be prototyped in <stdlib.h>, but declaring its
 * variables doesn't hurt.
 */
extern char *optarg;
extern int optind;
#endif /* HAVE_GETOPT_H */

#include <tic.h>

#if HAVE_NC_FREEALL
#undef ExitProgram
#ifdef USE_LIBTINFO
#define ExitProgram(code) exit_terminfo(code)
#else
#define ExitProgram(code) _nc_free_tic(code)
#endif
#endif

/* error-returns for tput */
#define ErrUsage	2
#define ErrTermType	3
#define ErrCapName	4
#define ErrSystem(n)	(4 + (n))

/* We use isascii only to guard against use of 7-bit ctype tables in the
 * isprint test in infocmp.
 */
#if !HAVE_ISASCII
# undef isascii
# if ('z'-'a' == 25) && ('z' < 127) && ('Z'-'A' == 25) && ('Z' < 127) && ('9' < 127)
#  define isascii(c) (UChar(c) <= 127)
# else
#  define isascii(c) 1		/* not really ascii anyway */
# endif
#endif

#define VtoTrace(opt) (unsigned) ((opt > 0) ? opt : (opt == 0))

/*
 * If configured for tracing, the debug- and trace-output are merged together
 * in the trace file for "upper" levels of the verbose option.
 */
#ifdef TRACE
#define use_verbosity(level) do { \
 		set_trace_level(level); \
		if (_nc_tracing > DEBUG_LEVEL(2)) \
		    _nc_tracing |= TRACE_MAXIMUM; \
		else if (_nc_tracing == DEBUG_LEVEL(2)) \
		    _nc_tracing |= TRACE_ORDINARY; \
		if (level >= 2) \
		    curses_trace(_nc_tracing); \
	} while (0)
#else
#define use_verbosity(level) do { set_trace_level(level); } while (0)
#endif

#ifndef CUR
#define CUR ((TERMTYPE *)(cur_term))->
#endif

#endif /* PROGS_PRIV_H */
