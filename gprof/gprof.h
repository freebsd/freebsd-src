/*
 * Copyright (c) 1983, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */
#ifndef gprof_h
#define gprof_h

/* Include the BFD sysdep.h file.  */
#include "sysdep.h"
#include "bfd.h"

/* Undefine the BFD PACKAGE and VERSION macros before including the
   gprof config.h file.  */
#undef PACKAGE
#undef VERSION

#include "gconfig.h"

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#endif

/* AIX defines hz as a macro.  */
#undef hz

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#define	A_OUTNAME	"a.out"		/* default core filename */
#define	GMONNAME	"gmon.out"	/* default profile filename */
#define	GMONSUM		"gmon.sum"	/* profile summary filename */

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#ifdef ENABLE_NLS
/* Undefine BFD's `_' macro - it uses dgetext() and we want to use gettext().  */
#undef  _
#define _(String) gettext (String)
#endif

#include "bin-bugs.h"

#define STYLE_FLAT_PROFILE	(1<<0)
#define STYLE_CALL_GRAPH	(1<<1)
#define STYLE_SUMMARY_FILE	(1<<2)
#define STYLE_EXEC_COUNTS	(1<<3)
#define STYLE_ANNOTATED_SOURCE	(1<<4)
#define STYLE_GMON_INFO		(1<<5)
#define STYLE_FUNCTION_ORDER	(1<<6)
#define STYLE_FILE_ORDER	(1<<7)

#define	ANYDEBUG	(1<<0)	/*    1 */
#define	DFNDEBUG	(1<<1)	/*    2 */
#define	CYCLEDEBUG	(1<<2)	/*    4 */
#define	ARCDEBUG	(1<<3)	/*    8 */
#define	TALLYDEBUG	(1<<4)	/*   16 */
#define	TIMEDEBUG	(1<<5)	/*   32 */
#define	SAMPLEDEBUG	(1<<6)	/*   64 */
#define	AOUTDEBUG	(1<<7)	/*  128 */
#define	CALLDEBUG	(1<<8)	/*  256 */
#define	LOOKUPDEBUG	(1<<9)	/*  512 */
#define	PROPDEBUG	(1<<10)	/* 1024 */
#define BBDEBUG		(1<<11)	/* 2048 */
#define IDDEBUG		(1<<12)	/* 4096 */
#define SRCDEBUG	(1<<13)	/* 8192 */

#ifdef DEBUG
#define DBG(l,s)	if (debug_level & (l)) {s;}
#else
#define DBG(l,s)
#endif

typedef enum
  {
    FF_AUTO = 0, FF_MAGIC, FF_BSD, FF_BSD44, FF_PROF
  }
File_Format;

typedef unsigned char UNIT[2];	/* unit of profiling */

extern const char *whoami;	/* command-name, for error messages */
extern const char *function_mapping_file; /* file mapping functions to files */
extern const char *a_out_name;	/* core filename */
extern long hz;			/* ticks per second */

/*
 * Command-line options:
 */
extern int debug_level;			/* debug level */
extern int output_style;
extern int output_width;		/* controls column width in index */
extern bfd_boolean bsd_style_output;	/* as opposed to FSF style output */
extern bfd_boolean demangle;		/* demangle symbol names? */
extern bfd_boolean discard_underscores;	/* discard leading underscores? */
extern bfd_boolean ignore_direct_calls;	/* don't count direct calls */
extern bfd_boolean ignore_static_funcs;	/* suppress static functions */
extern bfd_boolean ignore_zeros;	/* ignore unused symbols/files */
extern bfd_boolean line_granularity;	/* function or line granularity? */
extern bfd_boolean print_descriptions;	/* output profile description */
extern bfd_boolean print_path;		/* print path or just filename? */
extern bfd_boolean ignore_non_functions; /* Ignore non-function symbols.  */

extern File_Format file_format;		/* requested file format */

extern bfd_boolean first_output;	/* no output so far? */

extern void done PARAMS ((int status)) ATTRIBUTE_NORETURN;

#endif /* gprof_h */
