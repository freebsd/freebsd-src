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


/*	from OpenSolaris "n6.c	1.12	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)n6.c	1.51 (gritter) 6/19/11
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze <carsten.kunze at arcor.de>
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

#include <stdio.h>
#include <string.h>
#ifdef	EUC
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>
#endif
#include <ctype.h>
#include "tdef.h"
#include "tw.h"
#include "pt.h"
#include "ext.h"

/*
 * n6.c -- width functions, sizes and fonts
*/

int	initbdtab[NFONT+1] ={ 0, 0, 0, 3, 3, 0, };
int	sbold = 0;
int	initfontlab[NFONT+1] = { 0, 'R', 'I', 'B', PAIR('B','I'), 'S', 0 };

extern	int	nchtab;

int 
width(register tchar j)
{
	register int i, k;

	if (isadjspc(j))
		return(0);
	if (j & (ZBIT|MOT)) {
		if (iszbit(j))
			return(0);
		if (isvmot(j))
			return(0);
		k = absmot(j);
		if (isnmot(j))
			k = -k;
		return(k);
	}
	i = cbits(j);
	if (isxfunc(j, CHAR))
		return(charout[sbits(j)].width);
	if (i < ' ') {
		if (i == '\b')
			return(-widthp);
		if (i == PRESC)
			i = eschar;
		else if (iscontrol(i))
			return(0);
	}
	if (i==ohc)
		return(0);
#ifdef EUC
	if (multi_locale && i >= nchtab + _SPECCHAR_ST) {
		i = tr2un(i, fbits(j));
		if ((i = wcwidth(i)) < 0)
			i = 0;
		k = t.Char * csi_width[i];
		widthp = k;
		return(k);
	}
#endif /* EUC */
	i = trtab[i];
	if (i < 32)
		return(0);
	k = t.width[i] * t.Char;
	widthp = k;
	return(k);
}


tchar 
setch(int delim)
{
	register int j;
	char	temp[40];
	register char	*s;

	s = temp;
	if (delim == 'C') {
		do {
			j = getach();
			if (s < &temp[sizeof temp - 1])
				*s++ = j;
		} while (j != 0 && (s == &temp[1] || j != temp[0]));
		if (s - temp == 3)
			return temp[1];
		else if (s - temp == 4) {
			temp[0] = temp[1];
			temp[1] = temp[2];
			s = &temp[2];
		} else {
			*s = 0;
			if (j != temp[0])
				nodelim(temp[0]);
			else if (warn & WARN_CHAR) {
				errprint("missing glyph \\C%s", temp);
			}
			return 0;
		}
	} else if (delim == '[' && (j = getach()) != ']') {
		*s++ = j;
		while ((j = getach()) != ']' && j != 0)
			if (s < &temp[sizeof temp - 1])
				*s++ = j;
		if (s - temp == 1)
			return temp[0];
		else if (s - temp != 2) {
			*s = '\0';
			if (j != ']')
				nodelim(']');
			else {
				size_t l = strlen(temp);
				if (gemu) {
					if (l == 5 && *temp == 'u'
					    && isxdigit((unsigned)temp[1])
					    && isxdigit((unsigned)temp[2])
					    && isxdigit((unsigned)temp[3])
					    && isxdigit((unsigned)temp[4])) {
						int n;
						n = strtol(temp + 1, NULL, 16);
						if (n)
							return setuc0(n);
					} else if ((l == 6 || (l == 7
					    && isdigit((unsigned)temp[6])))
					    && isdigit((unsigned)temp[5])
					    && isdigit((unsigned)temp[4])
					    && !strncmp(temp, "char", 4)) {
						int i = atoi(temp + 4);
						if (i <= 255)
							return i + nchtab +
							    _SPECCHAR_ST;
					}
				}
				if ((j = findch(temp)) > 0)
					return j | chbits;
				else if (warn & WARN_CHAR)
					errprint("missing glyph \\[%s]", temp);
				return 0;
			}
		}
	} else {
		if ((*s++ = getach()) == 0 || (*s++ = getach()) == 0)
			return(0);
	}
	*s = '\0';
	if ((j = findch(temp)) > 0)
		return j | chbits;
	else {
		if (warn & WARN_CHAR)
			errprint("missing glyph \\%c%s", delim, temp);
		return 0;
	}
}

tchar 
setabs (void)		/* set absolute char from \C'...' */
{			/* for now, a no-op */
	int n;

	getch();
	n = 0;
	n = inumb(&n);
	getch();
	if (nonumb || n + nchtab + _SPECCHAR_ST >= NCHARS)
		return 0;
	return n + nchtab + _SPECCHAR_ST;
}

int
tr2un(tchar c, int f)
{
	int	k;

	k = cbits(c);
	if (k >= nchtab + _SPECCHAR_ST)
		return k - nchtab - _SPECCHAR_ST;
	if (k & ~0177)
		return 0;
	return k;
}

int 
findft(register int i, int required)
{
	register int k;

	if ((k = i - '0') >= 0 && k <= nfonts && k < smnt)
		return(k);
	for (k = 0; fontlab[k] != i; k++)
		if (k > nfonts) {
			if (required && warn & WARN_FONT)
				errprint("%s: no such font", macname(i));
			return(-1);
		}
	return(k);
}


void
caseps(void)
{
}


void
mchbits(void)
{
	chbits = 0;
	setfbits(chbits, font);
	ses = sps = width(' ' | chbits);
}


void
setps(void)
{
	tchar	c;
	register int i, j, k;

	noschr++;
	i = cbits(c = getch());
	if (noschr) noschr--;
	if (ismot(c) && xflag)
		return;
	if (ischar(i) && isdigit(i)) {		/* \sd or \sdd */
		i -= '0';
		if (i == 0)		/* \s0 */
			;
		else if (i <= 3 && ischar(j = cbits(ch = getch())) &&
		    isdigit(j)) {	/* \sdd */
			ch = 0;
		}
	} else if (i == '(') {		/* \s(dd */
		getch();
		getch();
	} else if (i == '+' || i == '-') {	/* \s+, \s- */
		j = cbits(c = getch());
		if (ischar(j) && isdigit(j)) {		/* \s+d, \s-d */
			;
		} else if (j == '(') {		/* \s+(dd, \s-(dd */
			getch();
			getch();
		} else if (xflag) {	/* \s+[dd], */
			k = j == '[' ? ']' : j;			/* \s-'dd' */
			setcbits(c, k);
			hatoi();
			if (nonumb)
				return;
			if (!issame(getch(), c))
				nodelim(k);
		}
	} else if (xflag) {  /* \s'+dd', \s[dd] */
		if (i == '[') {
			i = ']';
			setcbits(c, i);
		}
		j = inumb(&apts);
		if (nonumb)
			return;
		if (!issame(getch(), c))
			nodelim(i);
	}
}


tchar 
setht (void)		/* set character height from \H'...' */
{
	getch();
	inumb(&apts);
	getch();
	return(0);
}


tchar 
setslant (void)		/* set slant from \S'...' */
{
	int	n;

	getch();
	n = 0;
	n = inumb(&n);
	getch();
	return(0);
}


void
caseft(void)
{
	skip(0);
	setfont(1);
}


void
setfont(int a)
{
	register int i, j;

	if (a)
		i = getrq(3);
	else 
		i = getsn(0);
	if (!i || i == 'P') {
		j = font1;
		goto s0;
	}
	if (/* i == 'S' || */ i == '0')
		return;
	if ((j = findft(i, 0)) == -1) {
		if (xflag) {
			font1 = font;
		}
		return;
	}
s0:
	font1 = font;
	font = j;
	mchbits();
}


void
setwd(void)
{
	register int base, wid;
	register tchar i;
	tchar	delim;
	int	emsz, k;
	int	savhp, savapts, savapts1, savfont, savfont1, savpts, savpts1;
	int	savlgf;

	base = numtab[ST].val = wid = numtab[CT].val = 0;
	if (ismot(i = getch()))
		return;
	delim = i;
	savhp = numtab[HP].val;
	numtab[HP].val = 0;
	savapts = apts;
	savapts1 = apts1;
	savfont = font;
	savfont1 = font1;
	savpts = pts;
	savpts1 = pts1;
	savlgf = lgf;
	lgf = 0;
	setwdf++;
	while (i = getch(), !issame(i, delim) && !nlflg) {
		k = width(i);
		wid += k;
		numtab[HP].val += k;
		if (!ismot(i)) {
			emsz = (INCH * pts + 36) / 72;
		} else if (isvmot(i)) {
			k = absmot(i);
			if (isnmot(i))
				k = -k;
			base -= k;
			emsz = 0;
		} else 
			continue;
		if (base < numtab[SB].val)
			numtab[SB].val = base;
		if ((k = base + emsz) > numtab[ST].val)
			numtab[ST].val = k;
	}
	if (!issame(i, delim))
		nodelim(delim);
	setn1(wid, 0, (tchar) 0);
	setnr("rst", 0, 0);
	setnr("rsb", 0, 0);
	numtab[HP].val = savhp;
	apts = savapts;
	apts1 = savapts1;
	font = savfont;
	font1 = savfont1;
	pts = savpts;
	pts1 = savpts1;
	lgf = savlgf;
	mchbits();
	setwdf = 0;
}


tchar 
vmot(void)
{
	dfact = lss;
	vflag++;
	return(mot());
}


tchar 
hmot(void)
{
	dfact = EM;
	return(mot());
}


tchar 
mot(void)
{
	register int j, n;
	register tchar i;
	tchar	c, delim;

	j = HOR;
	noschr++;
	delim = getch(); /*eat delim*/
	if (noschr) noschr--;
	if ((n = hatoi())) {
		if (vflag)
			j = VERT;
		i = makem(quant(n, j));
	} else
		i = 0;
	noschr++;
	c = getch();
	if (noschr) noschr--;
	if (!issame(c, delim))
		nodelim(delim);
	vflag = 0;
	dfact = 1;
	return(i);
}


tchar 
sethl(int k)
{
	register int j;
	tchar i;

	j = t.Halfline;
	if (k == 'u')
		j = -j;
	else if (k == 'r')
		j = -2 * j;
	vflag++;
	i = makem(j);
	vflag = 0;
	return(i);
}


tchar 
makem(int i)
{
	register tchar j;

	if ((j = i) < 0)
		j = -j;
	j = sabsmot(j) | MOT;
	if (i < 0)
		j |= NMOT;
	if (vflag)
		j |= VMOT;
	return(j);
}


tchar 
getlg(tchar i)
{
	return(i);
}


void
caselg(void)
{
}

void
caseflig(void)
{
}

void
casefp(void)
{
	register int i, j;

	skip(1);
	if ((i = cbits(getch()) - '0') < 0 || i > nfonts)
		return;
	if (skip(1) || !(j = getrq(3)))
		return;
	fontlab[i] = j;
}

void
casefps(void)
{
	skip(1);
	getname();
	casefp();
}


void
casecs(void)
{
}


void
casebd(void)
{
	register int i, j = 0, k;

	k = 0;
bd0:
	if (skip(1) || !(i = getrq(0)) || (j = findft(i, 1)) == -1) {
		if (k)
			goto bd1;
		else 
			return;
	}
	if (j == smnt) {
		k = smnt;
		goto bd0;
	}
	if (k) {
		sbold = j;
		j = k;
	}
bd1:
	skip(0);
	noscale++;
	bdtab[j] = hatoi();
	noscale = 0;
}


void
casevs(void)
{
	register int i;

	skip(0);
	vflag++;
	dfact = INCH; /*default scaling is points!*/
	dfactd = 72;
	res = VERT;
	i = inumb(&lss);
	if (nonumb)
		i = lss1;
	if (xflag && i < 0) {
		if (warn & WARN_RANGE)
			errprint("negative vertical spacing ignored");
		i = lss1;
	}
	if (i < VERT)
		i = VERT;	/* was VERT */
	lss1 = lss;
	lss = i;
}




void
casess(void)
{
}


tchar 
xlss(void)
{
	/* stores \x'...' into
	 * two successive tchars.
	 * the first contains HX, the second the value,
	 * encoded as a vertical motion.
	 * decoding is done in n2.c by pchar().
	 */
	int	i;

	getch();
	dfact = lss;
	i = quant(hatoi(), VERT);
	dfact = 1;
	getch();
	if (i >= 0)
		pbbuf[pbp++] = MOT | VMOT | sabsmot(i);
	else
		pbbuf[pbp++] = MOT | VMOT | NMOT | sabsmot(-i);
	return(HX);
}

tchar
setuc0(int n)
{
	if (n & ~0177) {
#ifdef	EUC
		int	k;
		k = (n + nchtab + _SPECCHAR_ST) | chbits;
		if (k >= NCHARS)
			morechars(k);
		return k;
#else
		return 0;
#endif
	} else
		return n | chbits;
}

static void
discard(void)
{
	int	c, delim;

	if ((delim = getach()) != 0)
		do {
			if ((c = getach()) == 0) {
				if (cbits(ch) == ' ')
					ch = 0;
				else
					break;
			}
		} while (c != delim);
}

tchar
setanchor(void)
{
	discard();
	return 0;
}

tchar
setlink(void)
{
	if ((linkin = !linkin))
		discard();
	return 0;
}

tchar
setulink(void)
{
	if ((linkin = !linkin))
		discard();
	return 0;
}

void
casedummy(void){;}
