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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)parse.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: parse.c,v 1.1.1.1.8.1 1997/07/11 06:25:59 charnier Exp $";
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "hexdump.h"

FU *endfu;					/* format at end-of-data */

void
addfile(name)
	char *name;
{
	register unsigned char *p;
	FILE *fp;
	int ch;
	char buf[2048 + 1];

	if ((fp = fopen(name, "r")) == NULL)
		err(1, "%s", name);
	while (fgets(buf, sizeof(buf), fp)) {
		if (!(p = index(buf, '\n'))) {
			warnx("line too long");
			while ((ch = getchar()) != '\n' && ch != EOF);
			continue;
		}
		*p = '\0';
		for (p = buf; *p && isspace(*p); ++p);
		if (!*p || *p == '#')
			continue;
		add(p);
	}
	(void)fclose(fp);
}

void
add(fmt)
	char *fmt;
{
	unsigned char *p, *savep;
	static FS **nextfs;
	FS *tfs;
	FU *tfu, **nextfu;

	/* start new linked list of format units */
	tfs = emalloc(sizeof(FS));
	if (!fshead)
		fshead = tfs;
	else
		*nextfs = tfs;
	nextfs = &tfs->nextfs;
	nextfu = &tfs->nextfu;

	/* take the format string and break it up into format units */
	for (p = fmt;;) {
		/* skip leading white space */
		for (; isspace(*p); ++p);
		if (!*p)
			break;

		/* allocate a new format unit and link it in */
		tfu = emalloc(sizeof(FU));
		*nextfu = tfu;
		nextfu = &tfu->nextfu;
		tfu->reps = 1;

		/* if leading digit, repetition count */
		if (isdigit(*p)) {
			for (savep = p; isdigit(*p); ++p);
			if (!isspace(*p) && *p != '/')
				badfmt(fmt);
			/* may overwrite either white space or slash */
			tfu->reps = atoi(savep);
			tfu->flags = F_SETREP;
			/* skip trailing white space */
			for (++p; isspace(*p); ++p);
		}

		/* skip slash and trailing white space */
		if (*p == '/')
			while (isspace(*++p));

		/* byte count */
		if (isdigit(*p)) {
			for (savep = p; isdigit(*p); ++p);
			if (!isspace(*p))
				badfmt(fmt);
			tfu->bcnt = atoi(savep);
			/* skip trailing white space */
			for (++p; isspace(*p); ++p);
		}

		/* format */
		if (*p != '"')
			badfmt(fmt);
		for (savep = ++p; *p != '"';)
			if (*p++ == 0)
				badfmt(fmt);
		if (!(tfu->fmt = malloc(p - savep + 1)))
			nomem();
		(void) strncpy(tfu->fmt, savep, p - savep);
		tfu->fmt[p - savep] = '\0';
		escape(tfu->fmt);
		p++;
	}
}

static char *spec = ".#-+ 0123456789";

int
size(fs)
	FS *fs;
{
	register FU *fu;
	register int bcnt, cursize;
	register unsigned char *fmt;
	int prec;

	/* figure out the data block size needed for each format unit */
	for (cursize = 0, fu = fs->nextfu; fu; fu = fu->nextfu) {
		if (fu->bcnt) {
			cursize += fu->bcnt * fu->reps;
			continue;
		}
		for (bcnt = prec = 0, fmt = fu->fmt; *fmt; ++fmt) {
			if (*fmt != '%')
				continue;
			/*
			 * skip any special chars -- save precision in
			 * case it's a %s format.
			 */
			while (index(spec + 1, *++fmt));
			if (*fmt == '.' && isdigit(*++fmt)) {
				prec = atoi(fmt);
				while (isdigit(*++fmt));
			}
			switch(*fmt) {
			case 'c':
				bcnt += 1;
				break;
			case 'd': case 'i': case 'o': case 'u':
			case 'x': case 'X':
				bcnt += 4;
				break;
			case 'e': case 'E': case 'f': case 'g': case 'G':
				bcnt += 8;
				break;
			case 's':
				bcnt += prec;
				break;
			case '_':
				switch(*++fmt) {
				case 'c': case 'p': case 'u':
					bcnt += 1;
					break;
				}
			}
		}
		cursize += bcnt * fu->reps;
	}
	return (cursize);
}

void
rewrite(fs)
	FS *fs;
{
	enum { NOTOKAY, USEBCNT, USEPREC } sokay;
	register PR *pr, **nextpr;
	register FU *fu;
	unsigned char *p1, *p2, *fmtp;
	char savech, cs[3];
	int nconv, prec;

	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		/*
		 * Break each format unit into print units; each conversion
		 * character gets its own.
		 */
		for (nconv = 0, fmtp = fu->fmt; *fmtp; nextpr = &pr->nextpr) {
			pr = emalloc(sizeof(PR));
			if (!fu->nextpr)
				fu->nextpr = pr;
			else
				*nextpr = pr;

			/* Skip preceding text and up to the next % sign. */
			for (p1 = fmtp; *p1 && *p1 != '%'; ++p1);

			/* Only text in the string. */
			if (!*p1) {
				pr->fmt = fmtp;
				pr->flags = F_TEXT;
				break;
			}

			/*
			 * Get precision for %s -- if have a byte count, don't
			 * need it.
			 */
			if (fu->bcnt) {
				sokay = USEBCNT;
				/* Skip to conversion character. */
				for (++p1; index(spec, *p1); ++p1);
			} else {
				/* Skip any special chars, field width. */
				while (index(spec + 1, *++p1));
				if (*p1 == '.' && isdigit(*++p1)) {
					sokay = USEPREC;
					prec = atoi(p1);
					while (isdigit(*++p1));
				} else
					sokay = NOTOKAY;
			}

			p2 = p1 + 1;		/* Set end pointer. */
			cs[0] = *p1;		/* Set conversion string. */
			cs[1] = '\0';

			/*
			 * Figure out the byte count for each conversion;
			 * rewrite the format as necessary, set up blank-
			 * padding for end of data.
			 */
			switch(cs[0]) {
			case 'c':
				pr->flags = F_CHAR;
				switch(fu->bcnt) {
				case 0: case 1:
					pr->bcnt = 1;
					break;
				default:
					p1[1] = '\0';
					badcnt(p1);
				}
				break;
			case 'd': case 'i':
				pr->flags = F_INT;
				goto isint;
			case 'o': case 'u': case 'x': case 'X':
				pr->flags = F_UINT;
isint:				cs[2] = '\0';
				cs[1] = cs[0];
				cs[0] = 'q';
				switch(fu->bcnt) {
				case 0: case 4:
					pr->bcnt = 4;
					break;
				case 1:
					pr->bcnt = 1;
					break;
				case 2:
					pr->bcnt = 2;
					break;
				default:
					p1[1] = '\0';
					badcnt(p1);
				}
				break;
			case 'e': case 'E': case 'f': case 'g': case 'G':
				pr->flags = F_DBL;
				switch(fu->bcnt) {
				case 0: case 8:
					pr->bcnt = 8;
					break;
				case 4:
					pr->bcnt = 4;
					break;
				default:
					p1[1] = '\0';
					badcnt(p1);
				}
				break;
			case 's':
				pr->flags = F_STR;
				switch(sokay) {
				case NOTOKAY:
					badsfmt();
				case USEBCNT:
					pr->bcnt = fu->bcnt;
					break;
				case USEPREC:
					pr->bcnt = prec;
					break;
				}
				break;
			case '_':
				++p2;
				switch(p1[1]) {
				case 'A':
					endfu = fu;
					fu->flags |= F_IGNORE;
					/* FALLTHROUGH */
				case 'a':
					pr->flags = F_ADDRESS;
					++p2;
					switch(p1[2]) {
					case 'd': case 'o': case'x':
						cs[0] = 'q';
						cs[1] = p1[2];
						cs[2] = '\0';
						break;
					default:
						p1[3] = '\0';
						badconv(p1);
					}
					break;
				case 'c':
					pr->flags = F_C;
					/* cs[0] = 'c';	set in conv_c */
					goto isint2;
				case 'p':
					pr->flags = F_P;
					cs[0] = 'c';
					goto isint2;
				case 'u':
					pr->flags = F_U;
					/* cs[0] = 'c';	set in conv_u */
isint2:					switch(fu->bcnt) {
					case 0: case 1:
						pr->bcnt = 1;
						break;
					default:
						p1[2] = '\0';
						badcnt(p1);
					}
					break;
				default:
					p1[2] = '\0';
					badconv(p1);
				}
				break;
			default:
				p1[1] = '\0';
				badconv(p1);
			}

			/*
			 * Copy to PR format string, set conversion character
			 * pointer, update original.
			 */
			savech = *p2;
			p1[0] = '\0';
			pr->fmt = emalloc(strlen(fmtp) + 2);
			(void)strcpy(pr->fmt, fmtp);
			(void)strcat(pr->fmt, cs);
			*p2 = savech;
			pr->cchar = pr->fmt + (p1 - fmtp);
			fmtp = p2;

			/* Only one conversion character if byte count. */
			if (!(pr->flags&F_ADDRESS) && fu->bcnt && nconv++)
	    errx(1, "byte count with multiple conversion characters");
		}
		/*
		 * If format unit byte count not specified, figure it out
		 * so can adjust rep count later.
		 */
		if (!fu->bcnt)
			for (pr = fu->nextpr; pr; pr = pr->nextpr)
				fu->bcnt += pr->bcnt;
	}
	/*
	 * If the format string interprets any data at all, and it's
	 * not the same as the blocksize, and its last format unit
	 * interprets any data at all, and has no iteration count,
	 * repeat it as necessary.
	 *
	 * If, rep count is greater than 1, no trailing whitespace
	 * gets output from the last iteration of the format unit.
	 */
	for (fu = fs->nextfu;; fu = fu->nextfu) {
		if (!fu->nextfu && fs->bcnt < blocksize &&
		    !(fu->flags&F_SETREP) && fu->bcnt)
			fu->reps += (blocksize - fs->bcnt) / fu->bcnt;
		if (fu->reps > 1) {
			for (pr = fu->nextpr;; pr = pr->nextpr)
				if (!pr->nextpr)
					break;
			for (p1 = pr->fmt, p2 = NULL; *p1; ++p1)
				p2 = isspace(*p1) ? p1 : NULL;
			if (p2)
				pr->nospace = p2;
		}
		if (!fu->nextfu)
			break;
	}
#ifdef DEBUG
	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		(void)printf("fmt:");
		for (pr = fu->nextpr; pr; pr = pr->nextpr)
			(void)printf(" {%s}", pr->fmt);
		(void)printf("\n");
	}
#endif
}

void
escape(p1)
	register char *p1;
{
	register char *p2;

	/* alphabetic escape sequences have to be done in place */
	for (p2 = p1;; ++p1, ++p2) {
		if (!*p1) {
			*p2 = *p1;
			break;
		}
		if (*p1 == '\\')
			switch(*++p1) {
			case 'a':
			     /* *p2 = '\a'; */
				*p2 = '\007';
				break;
			case 'b':
				*p2 = '\b';
				break;
			case 'f':
				*p2 = '\f';
				break;
			case 'n':
				*p2 = '\n';
				break;
			case 'r':
				*p2 = '\r';
				break;
			case 't':
				*p2 = '\t';
				break;
			case 'v':
				*p2 = '\v';
				break;
			default:
				*p2 = *p1;
				break;
			}
	}
}

void
badcnt(s)
	char *s;
{
	errx(1, "%s: bad byte count", s);
}

void
badsfmt()
{
	errx(1, "%%s: requires a precision or a byte count");
}

void
badfmt(fmt)
	char *fmt;
{
	errx(1, "\"%s\": bad format", fmt);
}

void
badconv(ch)
	char *ch;
{
	errx(1, "%%%s: bad conversion character", ch);
}
