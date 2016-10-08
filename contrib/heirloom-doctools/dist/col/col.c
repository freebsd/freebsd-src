/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 *	col - filter reverse carraige motions
 *
 */


#include <stdio.h>
#include <ctype.h>
#include <locale.h>
#include <limits.h>
#include <stdlib.h>
#include <wctype.h>
#include <unistd.h>
#include <wchar.h>
#include "global.h"

#define	PL 256
#define	ESC '\033'
#define	RLF '\013'
#define	SI '\017'
#define	SO '\016'
#define	GREEK 0200
#define	LINELN 4096
#define gettext(x) x
#ifndef iswascii
# define iswascii(c) isascii(c)
#endif

wchar_t	*page[PL];
wchar_t	lbuff[LINELN], *line;
wchar_t	*lbuffend = lbuff + LINELN - 1;
wchar_t	ws_blank[2] = {' ', 0};
char	esc_chars, underline, temp_off, smart;
int	bflag, xflag, fflag, pflag;
int	greeked;
int	half;
int	cp, lp;
int	ll, llh, mustwr;
int	pcp = 0;
char	*pgmname;

#define	USAGEMSG	"usage:\tcol [-bfxp]\n"

static void	outc(wchar_t);
static void	store(int);
static void	fetch(int);
static void	emit(wchar_t *, int);
static void	incr(void);
static void	decr(void);
static void	wsinsert(wchar_t *, int);
static void	incr_line(int);
static int	wcscrwidth(wchar_t);

int
main(int argc, char **argv)
{
	int	i, n;
	int	opt;
	int	greek;
	int	c;
	wchar_t	wc;
	char	byte;
	static char	fbuff[BUFSIZ];

	setbuf(stdout, fbuff);
	(void) setlocale(LC_ALL, "");
	pgmname = argv[0];

	while ((opt = getopt(argc, argv, "bfxp")) != EOF)
		switch (opt) {
		case 'b':
			bflag++;
			break;
		case 'x':
			xflag++;
			break;
		case 'f':
			fflag++;
			break;
		case 'p':
			pflag++;
			break;
		case '?':
		default:
			(void) fprintf(stderr, gettext(USAGEMSG));
			exit(2);
		}

	argc -= optind;
	if (argc >= 1) {
		(void) fprintf(stderr, gettext(USAGEMSG));
		exit(2);
	}

	for (ll = 0; ll < PL; ll++)
		page[ll] = 0;

	smart = temp_off = underline = esc_chars = '\0';
	cp = 0;
	ll = 0;
	greek = 0;
	mustwr = PL;
	line = lbuff;

	while ((c = getwchar()) != EOF) {
		if (underline && temp_off && c > ' ') {
			outc(ESC);
			if (*line) {
				incr_line(1);
			}
			*line = 'X';
			incr_line(1);
			*line = temp_off = '\0';
		}
		if (c != '\b')
			if (esc_chars)
				esc_chars = '\0';
		switch (c) {
		case '\n':
			if (underline && !temp_off) {
				if (*line)
					incr_line(1);
				*line = ESC;
				incr_line(1);
				*line = 'Y';
				incr_line(1);
				*line = '\0';
				temp_off = '1';
			}
			incr();
			incr();
			cp = 0;
			continue;

		case '\0':
			continue;

		case ESC:
			c = getwchar();
			switch (c) {
			case '7':	/* reverse full line feed */
				decr();
				decr();
				break;

			case '8':	/* reverse half line feed */
				if (fflag)
					decr();
				else {
					if (--half < -1) {
						decr();
						decr();
						half += 2;
					}
				}
				break;

			case '9':	/* forward half line feed */
				if (fflag)
					incr();
				else {
					if (++half > 0) {
						incr();
						incr();
						half -= 2;
					}
				}
				break;

			default:
				if (pflag)	{	/* pass through esc */
					outc(ESC);
					incr_line(1);
					*line = c;
					incr_line(1);
					*line = '\0';
					esc_chars = 1;
					if (c == 'X')
						underline = 1;
					if (c == 'Y' && underline)
						underline = temp_off = '\0';
					if (c == ']')
						smart = 1;
					if (c == '[')
						smart = '\0';
					}
				break;
			}
			continue;

		case SO:
			greek = GREEK;
			/* greeked++; FIXME: temporarily disabled */
			continue;

		case SI:
			greek = 0;
			continue;

		case RLF:
			decr();
			decr();
			continue;

		case '\r':
			cp = 0;
			continue;

		case '\t':
			cp = (cp + 8) & -8;
			continue;

		case '\b':
			if (esc_chars) {
				*line = '\b';
				incr_line(1);
				*line = '\0';
			} else if (cp > 0)
				cp--;
			continue;

		case ' ':
			cp++;
			continue;

		default:
			if (iswprint(c)) {	/* if printable */
				if (!greek) {
					outc((wchar_t)c);
					cp += wcscrwidth(c);
				}
				/*
				 * EUC (apply SO only when there can
				 * be corresponding character in CS1)
				 */
				else if (iswascii(c)) {
					byte = (c | greek);
					n = mbtowc(&wc, &byte, 1);
					if (!iswcntrl(c) && !iswspace(c) &&
					    n == 1) {
						outc(wc);
						cp += wcscrwidth(wc);
					} else {
						outc((wchar_t)c);
						cp += wcscrwidth(c);
					}
				} else {
					outc((wchar_t)c);
					cp += wcscrwidth(c);
				}

				if ((cp + 1) > LINELN) {
					(void) fprintf(stderr,
					    gettext("col: Line too long\n"));
					exit(2);
				}
			}
			continue;
		}
	}

	for (i = 0; i < PL; i++)
		if (page[(mustwr+i)%PL] != 0)
			emit(page[(mustwr+i) % PL], mustwr+i-PL);
	emit(ws_blank, (llh + 1) & -2);
	return (0);
}

static void
outc(wchar_t c)
{
	int	n, i;
	int	width, widthl, widthc;
	wchar_t	*p1;
	wchar_t c1;
	char esc_chars = '\0';
	if (lp > cp) {
		line = lbuff;
		lp = 0;
	}

	while (lp < cp) {
		if (*line != '\b')
			if (esc_chars)
				esc_chars = '\0';
			switch (*line)	{
			case ESC:
				incr_line(1);
				esc_chars = 1;
				break;
			case '\0':
				*line = ' ';
				lp++;
				break;
			case '\b':
				/* if ( ! esc_chars ) */
					lp--;
				break;
			default:
				lp += wcscrwidth(*line);
			}
		incr_line(1);
	}
	while (*line == '\b') {
		/*
		 * EUC (For a multi-column character, backspace characters
		 * are assumed to be used like "__^H^HXX", where "XX"
		 * represents a two-column character, and a backspace
		 * always goes back by one column.)
		 */
		for (n = 0; *line == '\b'; incr_line(1)) {
			n++;
			lp--;
		}
		while (n > 0 && lp < cp) {
			i = *line;
			incr_line(1);
			i = wcscrwidth(i);
			n -= i;
			lp += i;
		}
	}
	while (*line == ESC)
		incr_line(6);
	widthc = wcscrwidth(c);
	widthl = wcscrwidth(*line);
	if (bflag || (*line == '\0') || *line == ' ') {
		if (*line == '\0' || widthl == widthc) {
			*line = c;
		} else if (widthl > widthc) {
			n = widthl - widthc;
			wsinsert(line, n);
			*line = c;
			incr_line(1);
			for (i = 0; i < n; i++) {
				*line = ' ';
				incr_line(1);
			}
			line = lbuff;
			lp = 0;
		} else {
			n = widthc - widthl;
			if (line < lbuffend) {
				for (p1 = line+1; n > 0 && p1 < lbuffend;
				    n -= wcscrwidth(i)) {
						i = *p1++;
				}
				*line = c;
				if (p1 < lbuffend) {
					n_wcscpy(line+1, p1, lbuffend -
					    line - 1);
				} else {
					(void) fprintf(stderr,
					    gettext("col: Line too long.\n"));
					exit(1);
				}
			} else {
				(void) fprintf(stderr,
				    gettext("col: Line too long.\n"));
				exit(1);
			}
		}
	} else {
		if (smart && (widthl == 1) && (widthc == 1)) {
			wchar_t	c1, c2, c3, c4, c5, c6, c7;
			incr_line(1);
			c1 = *line;
			*line = ESC;
			incr_line(1);
			c2 = *line;
			*line = '[';
			incr_line(1);
			c3 = *line;
			*line = '\b';
			incr_line(1);
			c4 = *line;
			*line = ESC;
			incr_line(1);
			c5 = *line;
			*line = ']';
			incr_line(1);
			c6 = *line;
			*line = c;
			incr_line(1);
			while (c1) {
				c7 = *line;
				*line = c1;
				incr_line(1);
				c1 = c2;
				c2 = c3;
				c3 = c4;
				c4 = c5;
				c5 = c6;
				c6 = c7;
			}
		} else	{
			if ((widthl == 1) && (widthc == 1)) {
				wchar_t	c1, c2, c3;
				incr_line(1);
				c1 = *line;
				*line = '\b';
				incr_line(1);
				c2 = *line;
				*line = c;
				incr_line(1);
				while (c1) {
					c3 = *line;
					*line = c1;
					incr_line(1);
					c1 = c2;
					c2 = c3;
				}
			} else {
				width = (widthc > widthl) ? widthc : widthl;
				for (i = 0; i < width; i += wcscrwidth(c1)) {
					c1 = *line;
					incr_line(1);
				}
				wsinsert(line, width + (width - widthc + 1));
				for (i = 0; i < width; i++) {
					*line = '\b';
					incr_line(1);
				}
				*line = c;
				incr_line(1);
				for (i = widthc; i < width; i++) {
					*line = ' ';
					incr_line(1);
				}
			}
		}
		lp = 0;
		line = lbuff;
	}
}

static void
store(int lno)
{
	size_t bufsiz;
	lno %= PL;
	if (page[lno] != 0)
		free((char *)page[lno]);
	bufsiz = wcslen(lbuff) + 2;
	page[lno] = (wchar_t *)malloc(bufsiz * sizeof(wchar_t));
	if (page[lno] == 0) {
		/* fprintf(stderr, "%s: no storage\n", pgmname); */
		exit(2);
	}
	n_wcscpy(page[lno], lbuff, bufsiz);
}

static void
fetch(int lno)
{
	wchar_t	*p;

	lno %= PL;
	p = lbuff;
	while (*p)
		*p++ = '\0';
	line = lbuff;
	lp = 0;
	if (page[lno])
		n_wcscpy(line, page[lno], LINELN);
}

static void
emit(wchar_t *s, int lineno)
{
	static int	cline = 0;
	int	ncp;
	wchar_t	*p, *begin = s;
	int	cshifted;
	char	chr[MB_LEN_MAX + 1];

	int	c;
	static int	gflag = 0;

	if (*s) {
		if (gflag) {
			(void) putwchar(SI);
			gflag = 0;
		}
		while (cline < lineno - 1) {
			(void) putwchar('\n');
			pcp = 0;
			cline += 2;
		}
		if (cline != lineno) {
			(void) putwchar(ESC);
			(void) putwchar('9');
			cline++;
		}
		if (pcp)
			(void) putwchar('\r');
		pcp = 0;
		p = s;
		while (*p) {
			ncp = pcp;
			while (*p++ == ' ') {
				if ((++ncp & 7) == 0 && !xflag) {
					pcp = ncp;
					(void) putwchar('\t');
				}
			}
			if (!*--p)
				break;
			while (pcp < ncp) {
				(void) putwchar(' ');
				pcp++;
			}
			if (greeked) {
				if (wctomb(chr, *p) == 1) {
					if (gflag != (*chr & GREEK) &&
					    *p != '\b' &&
					    isascii(*chr ^ (gflag ^ GREEK)) &&
					    !iscntrl(*chr ^ (gflag ^ GREEK)) &&
					    !isspace(*chr ^ (gflag ^ GREEK))) {
						if (gflag)
							(void) putwchar(SI);
						else
							(void) putwchar(SO);
						gflag ^= GREEK;
					}
				}
			}
			c = *p;
			if (greeked) {
				if (wctomb(chr, (wchar_t)c) == 1) {
					cshifted = (*chr ^ GREEK);
					if (isascii(cshifted) &&
					    !iscntrl(cshifted) &&
					    !isspace(cshifted))
						(void) putwchar(*chr & ~GREEK);
				} else
					(void) putwchar(c);
			} else
				(void) putwchar(c);
			if (c == '\b') {
				if (p-begin >= 2 && *(p-2) && *(p-2) == ESC) {
					pcp++;
				} else
					pcp--;
			} else {
				pcp += wcscrwidth(c);
			}
			p++;
		}
	}
}

static void
incr(void)
{
	store(ll++);
	if (ll > llh)
		llh = ll;
	if (ll >= mustwr && page[ll%PL]) {
		emit(page[ll%PL], ll - PL);
		mustwr++;
		free((char *)page[ll%PL]);
		page[ll%PL] = 0;
	}
	fetch(ll);
}

static void
decr(void)
{
	if (ll > mustwr - PL) {
		store(ll--);
		fetch(ll);
	}
}

static void
wsinsert(wchar_t *s, int n)
{
	wchar_t	*p1, *p2;


	p1 = s + wcslen(s);
	p2 = p1 + n;
	while (p1 >= s)
		*p2-- = *p1--;
}

/*
 * incr_line - increments line pointer and checks for array out of bounds
 * amt: assumed to be >= 1
 * exit on error to avoid line pointer accessing out of the array
 */
static void
incr_line(int amt)
{
	if (line < lbuffend - amt + 1) {
		line += amt;
	} else {
		(void) fprintf(stderr, gettext("col: Line too long.\n"));
		exit(1);
	}
}


static int
wcscrwidth(wchar_t wc)
{
	int	nc;

	if (wc == 0) {
		/*
		 * if wc is a null character, needs to
		 * return 1 instead of 0.
		 */
		return (1);
	}
	nc = wcwidth(wc);
	if (nc > 0) {
		return (nc);
	} else {
		return (0);
	}
}
