/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "n8.c	1.8	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)n8.c	1.44 (gritter) 9/26/10
 */

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#include	<stddef.h>
#include	<stdio.h>
#ifdef	EUC
#include	<wctype.h>
#endif
#include	<ctype.h>
#include	<stdlib.h>
#include	<string.h>
#include	"tdef.h"
#include "ext.h"
#include "pt.h"
#include "libhnj/hyphen.h"
#define	HY_BIT	0200	/* generic stuff in here only works for ascii */
#define	HY_BIT2	0x80000000

/*
 * troff8.c
 * 
 * hyphenation
 */

int	*hbuf;
int	NHEX;
int	*nexth;
tchar	*hyend;
#define THRESH 160 /*digram goodness threshold*/
int	thresh = THRESH;

static	void		hyphenhnj(void);

static int *
growhbuf(int **pp)
{
	int	*nhbuf;
	int	inc = 4;
	ptrdiff_t	j;

	if ((nhbuf = realloc(hbuf, (NHEX+inc) * sizeof *hbuf)) == NULL)
		return NULL;
	NHEX += inc;
	j = (char *)nhbuf - (char *)hbuf;
	nexth = (int *)((char *)nexth + j);
	if (pp)
		*pp = (int *)((char *)*pp + j);
	return hbuf = nhbuf;
}

void
hyphen(tchar *wp)
{
	register int j;
	register tchar *i;
	tchar	*_wdstart, *_wdend;

	i = wp;
	while (punct(*i++))
		;
	if (!alph(*--i))
		return;
	wdstart = i++;
	while (hyext ? *i++ : alph(*i++))
		;
	hyend = wdend = --i - 1;
	while (punct(*i++))
		;
	if (*--i)
		return;
	if (!(wdhyf & 060) && (wdend - wdstart - (hylen - 1)) < 0)
		return;
	hyp = hyptr;
	*hyp = 0;
	hyoff = 2;
	if (dicthnj) {
		i = _wdstart = wdstart;
		_wdend = wdend;
		do {
			if (cbits(*i) == '-' || cbits(*i) == EMDASH ||
					i == _wdend) {
				while (wdstart <= i && (punct(*wdstart) ||
						(cbits(*wdstart) >= '0' &&
						 cbits(*wdstart) <= '9')))
					wdstart++;
				for (wdend = wdstart; wdend <= i; wdend++) {
					if (!alph(*wdend) ||
							(cbits(*wdend) >= '0' &&
							 cbits(*wdend) <= '9'))
						break;
				}
				hyend = --wdend;
				if ((wdhyf & 060 || wdstart + 3 <= wdend) &&
						!exword())
					hyphenhnj();
				wdstart = &i[1];
				if (i < _wdend) {
					*hyp++ = &i[1];
					if (hyp > (hyptr + NHYP - 1))
						hyp = hyptr + NHYP - 1;
				}
			}
		} while (i++ <= _wdend);
		wdstart = _wdstart;
		wdend = _wdend;
	} else if (!exword() && !suffix())
		digram();
	*hyp++ = 0;
	if (*hyptr) 
		for (j = 1; j; ) {
			j = 0;
			for (hyp = hyptr + 1; *hyp != 0; hyp++) {
				if (*(hyp - 1) > *hyp) {
					j++;
					i = *hyp;
					*hyp = *(hyp - 1);
					*(hyp - 1) = i;
				}
			}
		}
}


int 
punct(tchar i)
{
	if (!cbits(i) || alph(i))
		return(0);
	else
		return(1);
}


int 
alph(tchar j)
{
	int i;
	int f;
	int	h;

	while (isxfunc(j, CHAR))
		j = charout[sbits(j)].ch;
	i = cbits(j);
	f = fbits(j);
	if (!ismot(j) && i < nhcode && (h = hcode[i]) != 0) {
		if (h & ~0177)
			h = tr2un(h, f);
#ifdef EUC
		return hyext ? iswalnum(h) : iswalpha(h);
	} else
#else	/* !EUC */
		i = h;
	}
#endif	/* !EUC */
#ifdef EUC
	if (!ismot(j) && i & ~0177) {
		int	u;
#ifndef	NROFF
		if (islig(j) && hyext &&
				lgrevtab && lgrevtab[f] && lgrevtab[f][i])
			return 1;
#endif	/* !NROFF */
		u = tr2un(i, f);
		if (u == 0x017F)	/* longs */
			u = 's';
		return hyext ? iswalnum(u) : iswalpha(u);
	} else
#endif	/* EUC */
	if ((!ismot(j) && i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z') ||
			(hyext && i >= '0' && i <= '9'))
		return(1);
	else
		return(0);
}


void
caseht(void)
{
	thresh = THRESH;
	if (skip(0))
		return;
	noscale++;
	thresh = hatoi();
	noscale = 0;
}


void
casehw(void)
{
	register int i, k;
	int	*j;
	tchar t;
	int	cnt = 0;

	lgf++;
	if (nexth == NULL)
		growhbuf(NULL);
	k = 0;
	while (!skip(!cnt++)) {
		if ((j = nexth) >= (hbuf + NHEX - 2) && growhbuf(&j) == NULL)
			goto full;
		for (; ; ) {
			if (ismot(t = getch()))
				continue;
			i = cbits(t);
			if (i == ' ' || i == '\n') {
				*j++ = 0;
				nexth = j;
				*j = 0;
				if (i == ' ')
					break;
				else
					return;
			}
			if (i == '-') {
				k = HY_BIT2;
				continue;
			}
			*j++ = maplow(t) | k;
			k = 0;
			if (j >= (hbuf + NHEX - 2) && growhbuf(&j) == NULL)
				goto full;
		}
	}
	return;
full:
	errprint("exception word list full.");
	*nexth = 0;
}


int 
exword(void)
{
	register tchar *w;
	register int	*e;
	int	*save;

	e = hbuf;
	while (1) {
		save = e;
		if (e == NULL || *e == 0)
			return(0);
		w = wdstart;
		while (*e && w <= hyend) {
#ifndef NROFF
			int	i, m, f;
			m = cbits(*w);
			f = fbits(*w);
			if (islig(*w) && lgrevtab && lgrevtab[f] &&
					lgrevtab[f][m]) {
				for (i = 0; lgrevtab[f][m][i]; i++) {
					if ((*e&~HY_BIT2) ==
					  maplow(lgrevtab[f][m][i])) {
						e++;
					} else
						goto end;
				}
				w++;
			} else
#endif
			{
				if ((*e&~HY_BIT2) == maplow(*w)) {
					e++; 
					w++;
				} else
					goto end;
			}
		}
	end:	if (!*e) {
			if (w-1 == hyend || (w == wdend && maplow(*w) == 's')) {
				w = wdstart;
				for (e = save; *e; e++) {
#ifndef NROFF
					int	i, m, f;
					m = cbits(*w);
					f = fbits(*w);
					if (islig(*w) && lgrevtab &&
							lgrevtab[f] &&
							lgrevtab[f][m]) {
						for (i = 0; lgrevtab[f][m][i];
								i++) {
							if (*e++ & HY_BIT2) {
								*hyp = (void *)
								  ((intptr_t)w |
								   i);
								hyp++;
							}
						}
						e--;
					} else
#endif
					{
						if (*e & HY_BIT2)
							*hyp++ = w;
					}
					w++;
					if (hyp > (hyptr + NHYP - 1))
						hyp = hyptr + NHYP - 1;
				}
				return(1);
			} else {
				e++; 
				continue;
			}
		} else 
			while (*e++)
				;
	}
}


int 
suffix(void)
{
	register tchar *w;
	register const char	*s, *s0;
	tchar i;
	extern const char	*suftab[];

again:
	i = cbits(*hyend);
	if (i >= 128 || !alph(*hyend))
		return(0);
	if (i < 'a')
		i -= 'A' - 'a';
	if ((s0 = suftab[i-'a']) == 0)
		return(0);
	for (; ; ) {
		if ((i = *s0 & 017) == 0)
			return(0);
		s = s0 + i - 1;
		w = hyend - 1;
		while (s > s0 && w >= wdstart && (*s & 0177) == maplow(*w)) {
			s--;
			w--;
		}
		if (s == s0)
			break;
		s0 += i;
	}
	s = s0 + i - 1;
	w = hyend;
	if (*s0 & HY_BIT) 
		goto mark;
	while (s > s0) {
		w--;
		if (*s-- & HY_BIT) {
mark:
			hyend = w - 1;
			if (*s0 & 0100)
				continue;
			if (!chkvow(w))
				return(0);
			*hyp++ = w;
		}
	}
	if (*s0 & 040)
		return(0);
	if (exword())
		return(1);
	goto again;
}


int 
maplow(tchar t)
{
	int	h, i, f;

	while (isxfunc(t, CHAR))
		t = charout[sbits(t)].ch;
	i = cbits(t);
	f = fbits(t);
	if (!ismot(t) && i < nhcode && (h = hcode[i]) != 0) {
		if (h & ~0177)
			h = tr2un(h, f);
		h = tr2un(h, f);
		return(h);
	} else
#ifdef EUC
	if (!ismot(t) && i & ~0177) {
		i = tr2un(i, f);
		if (i == 0x017F)	/* longs */
			i = 's';
		if (iswupper(i))
			i = towlower(i);
	} else
#endif	/* EUC */
	if (ischar(i) && isupper(i)) 
		i = tolower(i);
	return(i);
}


int 
vowel(tchar i)
{
	switch (maplow(i)) {
	case 'a':
	case 'e':
	case 'i':
	case 'o':
	case 'u':
	case 'y':
		return(1);
	default:
		return(0);
	}
}


tchar *
chkvow(tchar *w)
{
	while (--w >= wdstart)
		if (vowel(*w))
			return(w);
	return(0);
}


void
digram(void) 
{
	register tchar *w;
	register int val;
	tchar * nhyend, *maxw = 0;
	int	maxval;
	extern const char	bxh[26][13], bxxh[26][13], xxh[26][13], xhx[26][13], hxx[26][13];

	for (w = wdstart; w <= wdend; w++)
		if (cbits(*w) & ~0177)
			return;

again:
	if (!(w = chkvow(hyend + 1)))
		return;
	hyend = w;
	if (!(w = chkvow(hyend)))
		return;
	nhyend = w;
	maxval = 0;
	w--;
	while ((++w < hyend) && (w < (wdend - 1))) {
		val = 1;
		if (w == wdstart)
			val *= dilook('a', *w, bxh);
		else if (w == wdstart + 1)
			val *= dilook(*(w-1), *w, bxxh);
		else 
			val *= dilook(*(w-1), *w, xxh);
		val *= dilook(*w, *(w+1), xhx);
		val *= dilook(*(w+1), *(w+2), hxx);
		if (val > maxval) {
			maxval = val;
			maxw = w + 1;
		}
	}
	hyend = nhyend;
	if (maxval > thresh)
		*hyp++ = maxw;
	goto again;
}


int 
dilook(tchar a, tchar b, const char t[26][13])
{
	register int i, j;

	i = t[maplow(a)-'a'][(j = maplow(b)-'a')/2];
	if (!(j & 01))
		i >>= 4;
	return(i & 017);
}

void
casehylang(void)
{
	int	c, i = 0, sz = 0;
	char	*path = NULL;
	size_t	l;

	dicthnj = NULL;
	free(hylang);
	hylang = NULL;
	hyext = 0;
	skip(0);
	do {
		c = getach();
		if (i >= sz)
			hylang = realloc(hylang, (sz += 8) * sizeof *hylang);
		hylang[i++] = c;
	} while (c);
	if (i == 1) {
		free(hylang);
		hylang = NULL;
		return;
	}
	if (strchr(hylang, '/') == NULL) {
		l = strlen(hylang) + strlen(HYPDIR) + 12;
		path = malloc(l);
		snprintf(path, l, "%s/hyph_%s.dic", HYPDIR, hylang);
	} else {
		l = strlen(hylang) + 1;
		path = malloc(l);
		n_strcpy(path, hylang, l);
	}
	if ((dicthnj = hnj_hyphen_load(path)) == NULL) {
		errprint("Can't load %s", path);
		free(hylang);
		hylang = NULL;
		free(path);
		return;
	}
	free(path);
	hyext = 1;
}

static int
addc(int m, char **cp, tchar **wp, int **wpp, int distance)
{
	tchar	t;

	t = m ? m | sfmask(**wp) : **wp;
	m = maplow(t);
	if (m > 0 && m <= 0x7f) {
		*(*cp)++ = m;
		*(*wpp)++ = distance;
	} else if (m >= 0x80 && m <= 0x7ff) {
		*(*cp)++ = (m >> 6 & 037) | 0300;
		*(*wpp)++ = distance;
		*(*cp)++ = (m & 077) | 0200;
		*(*wpp)++ = -1000;
	} else if (m >= 0x800 && m <= 0xffff) {
		*(*cp)++ = (m >> 12 & 017) | 0340;
		*(*wpp)++ = distance;
		*(*cp)++ = (m >> 6 & 077) | 0200;
		*(*wpp)++ = -1000;
		*(*cp)++ = (m & 077) | 0200;
		*(*wpp)++ = -1000;
	} else
		return 0;
	return 1;
}

static void
hyphenhnj(void)
{
	tchar	*wp;
	char	*cb, *cp, *hb;
	int	*wpos, *wpp;
	int	i, j, k;

	i = 12 * (wdend - wdstart) + 1;
	cb = malloc(i * sizeof *cb);
	hb = malloc(i * sizeof *hb);
	wpos = malloc(i * sizeof *wpos);
	cp = cb;
	wpp = wpos;
	for (wp = wdstart; wp <= wdend; wp++) {
#ifndef	NROFF
		int m = cbits(*wp);
		int f = fbits(*wp);
		if (islig(*wp) && lgrevtab && lgrevtab[f] && lgrevtab[f][m]) {
			for (i = 0; lgrevtab[f][m][i]; i++) {
				if (addc(lgrevtab[f][m][i], &cp, &wp, &wpp,
						i ? -i : wp-wdstart) == 0)
					goto retn;
			}
		} else
#endif
		{
			if (addc(0, &cp, &wp, &wpp, wp - wdstart) == 0)
				goto retn;
		}
	}
	*cp = '\0';
	j = cp - cb;
	while (wpp <= &wpos[j])
		*wpp++ = -1000;
	hnj_hyphen_hyphenate(dicthnj, cb, j, hb);
	k = 0;
	for (i = 0; i < j; i++) {
		if (wpos[i+1] >= 0)
			k = wpos[i+1];
		if ((hb[i] - '0') & 1 && wpos[i+1] >= -3) {
			if (wpos[i+1] >= 0)
				*hyp = &wdstart[wpos[i+1]];
			else {
				*hyp = &wdstart[k];
				*hyp = (void *)((intptr_t)*hyp | -wpos[i+1]);
			}
			if (++hyp > (hyptr + NHYP - 1))
				hyp = hyptr + NHYP - 1;
		}
	}
retn:
	free(cb);
	free(hb);
	free(wpos);
}
