/*
 * print.c - debugging printout routines
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#if __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include "file.h"

#ifndef lint
static char *moduleid =
	"@(#)print.c,v 1.2 1993/06/10 00:38:17 jtc Exp";
#endif  /* lint */

void
mdump(m)
struct magic *m;
{
	static char *offs[] = {  "absolute", "offset", 
				 "indirect", "indirect-offset" };
	static char *typ[] = {   "invalid", "byte", "short", "invalid",
				 "long", "string", "date", "beshort",
				 "belong", "bedate", "leshort", "lelong",
				 "ledate" };
	(void) fprintf(stderr, "[%s,%d,%s,%s%c,",
		(m->flag >= 0 && m->flag < 4 ? offs[m->flag]: "*bad*"),
		m->offset,
		(m->type >= 0 && m->type < 13 ? 
				typ[(unsigned char) m->type] : "*bad*"),
		m->reln & MASK ? "&" : "",
		m->reln & ~MASK);
	if (m->flag & INDIR)
	    (void) fprintf(stderr, "(%s,%d)",
		(m->in.type >= 0 && 
		m->in.type < 6 ? typ[(unsigned char) m->in.type] : "*bad*"),
		m->in.offset);

	if (m->type == STRING)
		showstr(m->value.s);
	else
		(void) fprintf(stderr, "%d",m->value.l);
	(void) fprintf(stderr, ",%s", m->desc);
	(void) fputs("]\n", stderr);
}

/*
 * ckfputs - futs, but with error checking
 * ckfprintf - fprintf, but with error checking
 */
void
ckfputs(str, fil) 	
    const char *str;
    FILE *fil;
{
	if (fputs(str,fil) == EOF)
		error("write failed.\n");
}

/*VARARGS*/
void
#if __STDC__
ckfprintf(FILE *f, const char *fmt, ...)
#else
ckfprintf(va_alist)
	va_dcl
#endif
{
	va_list va;
#if __STDC__
	va_start(va, fmt);
#else
	FILE *f;
	const char *fmt;
	va_start(va);
	f = va_arg(va, FILE *);
	fmt = va_arg(va, const char *);
#endif
	(void) vfprintf(f, fmt, va);
	if (ferror(f))
		error("write failed.\n");
	va_end(va);
}

/*
 * error - print best error message possible and exit
 */
/*VARARGS*/
void
#if __STDC__
error(const char *f, ...)
#else
error(va_alist)
	va_dcl
#endif
{
	va_list va;
#if __STDC__
	va_start(va, f);
#else
	const char *f;
	va_start(va);
	f = va_arg(va, const char *);
#endif
	/* cuz we use stdout for most, stderr here */
	(void) fflush(stdout); 

	if (progname != NULL) 
		(void) fprintf(stderr, "%s: ", progname);
	(void) vfprintf(stderr, f, va);
	va_end(va);
	exit(1);
}

/*VARARGS*/
void
#if __STDC__
magwarn(const char *f, ...)
#else
magwarn(va_alist)
	va_dcl
#endif
{
	va_list va;
#if __STDC__
	va_start(va, f);
#else
	const char *f;
	va_start(va);
	f = va_arg(va, const char *);
#endif
	/* cuz we use stdout for most, stderr here */
	(void) fflush(stdout); 

	if (progname != NULL) 
		(void) fprintf(stderr, "%s: %s, %d: ", 
			       progname, magicfile, lineno);
	(void) vfprintf(stderr, f, va);
	va_end(va);
	fputc('\n', stderr);
}
