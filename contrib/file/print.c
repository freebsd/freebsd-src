/*
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * print.c - debugging printout routines
 */

#include "file.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <time.h>

#ifndef lint
FILE_RCSID("@(#)$Id: print.c,v 1.46 2004/11/13 08:11:39 christos Exp $")
#endif  /* lint */

#define SZOF(a)	(sizeof(a) / sizeof(a[0]))

#ifndef COMPILE_ONLY
protected void
file_mdump(struct magic *m)
{
	private const char *typ[] = { "invalid", "byte", "short", "invalid",
				     "long", "string", "date", "beshort",
				     "belong", "bedate", "leshort", "lelong",
				     "ledate", "pstring", "ldate", "beldate",
				     "leldate", "regex" };
	private const char optyp[] = { '@', '&', '|', '^', '+', '-', 
				      '*', '/', '%' };
	(void) fputc('[', stderr);
	(void) fprintf(stderr, ">>>>>>>> %d" + 8 - (m->cont_level & 7),
		       m->offset);

	if (m->flag & INDIR) {
		(void) fprintf(stderr, "(%s,",
			       /* Note: type is unsigned */
			       (m->in_type < SZOF(typ)) ? 
					typ[m->in_type] : "*bad*");
		if (m->in_op & FILE_OPINVERSE)
			(void) fputc('~', stderr);
		(void) fprintf(stderr, "%c%d),",
			       ((m->in_op&0x7F) < SZOF(optyp)) ? 
					optyp[m->in_op&0x7F] : '?',
				m->in_offset);
	}
	(void) fprintf(stderr, " %s%s", (m->flag & UNSIGNED) ? "u" : "",
		       /* Note: type is unsigned */
		       (m->type < SZOF(typ)) ? typ[m->type] : "*bad*");
	if (m->mask_op & FILE_OPINVERSE)
		(void) fputc('~', stderr);
	if (m->mask) {
		if ((m->mask_op & 0x7F) < SZOF(optyp)) 
			fputc(optyp[m->mask_op&0x7F], stderr);
		else
			fputc('?', stderr);
		if(FILE_STRING != m->type || FILE_PSTRING != m->type)
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
		case FILE_BYTE:
		case FILE_SHORT:
		case FILE_LONG:
		case FILE_LESHORT:
		case FILE_LELONG:
		case FILE_BESHORT:
		case FILE_BELONG:
			(void) fprintf(stderr, "%d", m->value.l);
			break;
		case FILE_STRING:
		case FILE_PSTRING:
		case FILE_REGEX:
			file_showstr(stderr, m->value.s, ~0U);
			break;
		case FILE_DATE:
		case FILE_LEDATE:
		case FILE_BEDATE:
			(void)fprintf(stderr, "%s,",
			    file_fmttime(m->value.l, 1));
			break;
		case FILE_LDATE:
		case FILE_LELDATE:
		case FILE_BELDATE:
			(void)fprintf(stderr, "%s,",
			    file_fmttime(m->value.l, 0));
			break;
		default:
			(void) fputs("*bad*", stderr);
			break;
		}
	}
	(void) fprintf(stderr, ",\"%s\"]\n", m->desc);
}
#endif

/*VARARGS*/
protected void
file_magwarn(struct magic_set *ms, const char *f, ...)
{
	va_list va;
	va_start(va, f);

	/* cuz we use stdout for most, stderr here */
	(void) fflush(stdout); 

	(void) fprintf(stderr, "%s, %lu: Warning ", ms->file,
	    (unsigned long)ms->line);
	(void) vfprintf(stderr, f, va);
	va_end(va);
	fputc('\n', stderr);
}

protected char *
file_fmttime(uint32_t v, int local)
{
	char *pp, *rt;
	time_t t = (time_t)v;
	struct tm *tm;

	if (local) {
		pp = ctime(&t);
	} else {
#ifndef HAVE_DAYLIGHT
		private int daylight = 0;
#ifdef HAVE_TM_ISDST
		private time_t now = (time_t)0;

		if (now == (time_t)0) {
			struct tm *tm1;
			(void)time(&now);
			tm1 = localtime(&now);
			daylight = tm1->tm_isdst;
		}
#endif /* HAVE_TM_ISDST */
#endif /* HAVE_DAYLIGHT */
		if (daylight)
			t += 3600;
		tm = gmtime(&t);
		pp = asctime(tm);
	}

	if ((rt = strchr(pp, '\n')) != NULL)
		*rt = '\0';
	return pp;
}
