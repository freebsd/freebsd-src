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

#include "file.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <time.h>

#ifndef lint
FILE_RCSID("@(#)$Id: print.c,v 1.33 2001/07/22 21:04:15 christos Exp $")
#endif  /* lint */

#define SZOF(a)	(sizeof(a) / sizeof(a[0]))

void
mdump(m)
	struct magic *m;
{
	static const char *typ[] = { "invalid", "byte", "short", "invalid",
				     "long", "string", "date", "beshort",
				     "belong", "bedate", "leshort", "lelong",
				     "ledate", "pstring", "ldate", "beldate",
				     "leldate" };
	static const char optyp[] = { '@', '&', '|', '^', '+', '-', 
				      '*', '/', '%' };
	(void) fputc('[', stderr);
	(void) fprintf(stderr, ">>>>>>>> %d" + 8 - (m->cont_level & 7),
		       m->offset);

	if (m->flag & INDIR) {
		(void) fprintf(stderr, "(%s,",
			       /* Note: type is unsigned */
			       (m->in_type < SZOF(typ)) ? 
					typ[m->in_type] : "*bad*");
		if (m->in_op & OPINVERSE)
			(void) fputc('~', stderr);
		(void) fprintf(stderr, "%c%d),",
			       ((m->in_op&0x7F) < SZOF(optyp)) ? 
					optyp[m->in_op&0x7F] : '?',
				m->in_offset);
	}
	(void) fprintf(stderr, " %s%s", (m->flag & UNSIGNED) ? "u" : "",
		       /* Note: type is unsigned */
		       (m->type < SZOF(typ)) ? typ[m->type] : "*bad*");
	if (m->mask_op & OPINVERSE)
		(void) fputc('~', stderr);
	if (m->mask) {
		((m->mask_op&0x7F) < SZOF(optyp)) ? 
			(void) fputc(optyp[m->mask_op&0x7F], stderr) :
			(void) fputc('?', stderr);
		if(STRING != m->type || PSTRING != m->type)
			(void) fprintf(stderr, "%.8x", m->mask);
		else {
			if (m->mask & STRING_IGNORE_LOWERCASE) 
				(void) fputc(CHAR_IGNORE_LOWERCASE, stderr);
			if (m->mask & STRING_COMPACT_BLANK) 
				(void) fputc(CHAR_COMPACT_BLANK, stderr);
			if (m->mask & STRING_COMPACT_OPTIONAL_BLANK) 
				(void) fputc(CHAR_COMPACT_OPTIONAL_BLANK,
				stderr);
		}
	}

	(void) fprintf(stderr, ",%c", m->reln);

	if (m->reln != 'x') {
		switch (m->type) {
		case BYTE:
		case SHORT:
		case LONG:
		case LESHORT:
		case LELONG:
		case BESHORT:
		case BELONG:
			(void) fprintf(stderr, "%d", m->value.l);
			break;
		case STRING:
		case PSTRING:
			showstr(stderr, m->value.s, -1);
			break;
		case DATE:
		case LEDATE:
		case BEDATE:
			(void)fprintf(stderr, "%s,", fmttime(m->value.l, 1));
			break;
		case LDATE:
		case LELDATE:
		case BELDATE:
			(void)fprintf(stderr, "%s,", fmttime(m->value.l, 0));
			break;
		default:
			(void) fputs("*bad*", stderr);
			break;
		}
	}
	(void) fprintf(stderr, ",\"%s\"]\n", m->desc);
}

/*
 * ckfputs - fputs, but with error checking
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
#ifdef __STDC__
ckfprintf(FILE *f, const char *fmt, ...)
#else
ckfprintf(va_alist)
	va_dcl
#endif
{
	va_list va;
#ifdef __STDC__
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
#ifdef __STDC__
error(const char *f, ...)
#else
error(va_alist)
	va_dcl
#endif
{
	va_list va;
#ifdef __STDC__
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
#ifdef __STDC__
magwarn(const char *f, ...)
#else
magwarn(va_alist)
	va_dcl
#endif
{
	va_list va;
#ifdef __STDC__
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


char *
fmttime(v, local)
	long v;
	int local;
{
	char *pp, *rt;
	time_t t = (time_t)v;
	if (local) {
		pp = ctime(&t);
	} else {
		struct tm *tm;
		if (daylight)
			t += 3600;
		tm = gmtime(&t);
		pp = asctime(tm);
	}

	if ((rt = strchr(pp, '\n')) != NULL)
		*rt = '\0';
	return pp;
}
