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
 * Copyright 1989 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "n9.c	1.11	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)n9.c	1.78 (gritter) 10/23/09
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef EUC
#include <locale.h>
#include <wctype.h>
#include <langinfo.h>
#endif	/* EUC */
#include "tdef.h"
#ifdef NROFF
#include "tw.h"
#endif
#include "pt.h"
#include "ext.h"

#ifdef EUC
#define	ISO646	"646"

int	multi_locale;
int	(*wdbdg)(wchar_t, wchar_t, int);
wchar_t	*(*wddlm)(wchar_t, wchar_t, int);

int	csi_width[4] = {
	1,
	1,
	2,
	3,
};
#endif /* EUC */

/*
 * troff9.c
 * 
 * misc functions
 */

tchar 
setz(void)
{
	tchar i;

	if (!ismot(i = getch()) && cbits(i) != ohc)
		i |= ZBIT;
	return(i);
}

static int
connectchar(tchar i)
{
	int	*cp, c;

	c = cbits(i);
	if (*connectch) {
		for (cp = connectch; *cp; cp++)
			if (c == *cp)
				return 1;
		return 0;
	}
	return c == RULE || c == UNDERLINE || c == ROOTEN;
}

void
setline(void)
{
	register tchar *i;
	tchar c, delim;
	int	length;
	int	w, cnt, rem, temp;
	tchar linebuf[NC];

	if (ismot(c = getch()))
		return;
	delim = c;
	vflag = 0;
	dfact = EM;
	length = quant(hatoi(), HOR);
	dfact = 1;
	if (!length) {
		eat(delim);
		return;
	}
s0:
	if (c = getch(), issame(c, delim)) {
		ch = c;
		c = RULE | chbits;
	} else if (cbits(c) == FILLER)
		goto s0;
	w = width(c);
	i = linebuf;
	if (length < 0) {
		*i++ = makem(length);
		length = -length;
	}
	if (!(cnt = length / w)) {
		*i++ = makem(-(temp = ((w - length) / 2)));
		*i++ = c;
		*i++ = makem(-(w - length - temp));
		goto s1;
	}
	if ((rem = length % w)) {
		if (connectchar(c))
			*i++ = c | ZBIT;
		*i++ = makem(rem);
	}
	if (cnt) {
		*i++ = RPT;
		*i++ = cnt;
		*i++ = c;
	}
s1:
	*i++ = 0;
	eat(delim);
	pushback(linebuf);
}


tchar 
eat(tchar c)
{
	register tchar i;

	while (i = getch(), !issame(i, c) &&  (cbits(i) != '\n'))
		;
	if (cbits(c) != ' ' && !issame(i, c))
		nodelim(c);
	return(i);
}


void
setov(void)
{
	register int j = 0, k;
	tchar i, delim, o[NOV];
	int w[NOV];

	if (ismot(i = getch()))
		return;
	delim = i;
	for (k = 0; (k < NOV) && (j = cbits(i = getch()), !issame(i, delim)) &&  (j != '\n'); k++) {
		o[k] = i;
		w[k] = width(i);
	}
	if (!issame(j, delim))
		nodelim(delim);
	o[k] = w[k] = 0;
	if (o[0])
		for (j = 1; j; ) {
			j = 0;
			for (k = 1; o[k] ; k++) {
				if (w[k-1] < w[k]) {
					j++;
					i = w[k];
					w[k] = w[k-1];
					w[k-1] = i;
					i = o[k];
					o[k] = o[k-1];
					o[k-1] = i;
				}
			}
		}
	else 
		return;
	pbbuf[pbp++] = makem(w[0] / 2);
	for (k = 0; o[k]; k++)
		;
	while (k>0) {
		k--;
		if (pbp >= pbsize-4)
			if (growpbbuf() == NULL) {
				errprint("no space for .ov");
				done(2);
			}
		pbbuf[pbp++] = makem(-((w[k] + w[k+1]) / 2));
		pbbuf[pbp++] = o[k];
	}
}


void
setbra(void)
{
	register int k;
	tchar i, *j, dwn, delim;
	int	cnt;
	tchar brabuf[NC];

	if (ismot(i = getch()))
		return;
	delim = i;
	j = brabuf + 1;
	cnt = 0;
#ifdef NROFF
	dwn = sabsmot(2 * t.Halfline) | MOT | VMOT;
#endif
#ifndef NROFF
	dwn = sabsmot((int)EM) | MOT | VMOT;
#endif
	while ((k = cbits(i = getch()), !issame(delim, i)) && (k != '\n') &&  (j <= (brabuf + NC - 4))) {
		*j++ = i | ZBIT;
		*j++ = dwn;
		cnt++;
	}
	if (!issame(i, delim))
		nodelim(delim);
	if (--cnt < 0)
		return;
	else if (!cnt) {
		ch = *(j - 2);
		return;
	}
	*j = 0;
#ifdef NROFF
	*--j = *brabuf = sabsmot(cnt * t.Halfline) | MOT | NMOT | VMOT;
#endif
#ifndef NROFF
	*--j = *brabuf = sabsmot((cnt * (int)EM) / 2) | MOT | NMOT | VMOT;
#endif
	*--j &= ~ZBIT;
	pushback(brabuf);
}


void
setvline(void)
{
	register int i;
	tchar c, d, delim, rem, ver, neg;
	int	cnt, v;
	tchar vlbuf[NC];
	register tchar *vlp;

	if (ismot(c = getch()))
		return;
	delim = c;
	dfact = lss;
	vflag++;
	i = quant(hatoi(), VERT);
	dfact = 1;
	if (!i) {
		eat(delim);
		vflag = 0;
		return;
	}
	if (c = getch(), issame(c, delim)) {
		c = BOXRULE | chbits;	/*default box rule*/
	} else {
		d = getch();
		if (!issame(d, delim))
			nodelim(delim);
	}
	c |= ZBIT;
	neg = 0;
	if (i < 0) {
		i = -i;
		neg = NMOT;
	}
#ifdef NROFF
	v = 2 * t.Halfline;
#endif
#ifndef NROFF
	v = EM;
#endif
	cnt = i / v;
	rem = makem(i % v) | neg;
	ver = makem(v) | neg;
	vlp = vlbuf;
	if (!neg)
		*vlp++ = ver;
	if (absmot(rem) != 0) {
		*vlp++ = c;
		*vlp++ = rem;
	}
	while ((vlp < (vlbuf + NC - 3)) && cnt--) {
		*vlp++ = c;
		*vlp++ = ver;
	}
	*(vlp - 2) &= ~ZBIT;
	if (!neg)
		vlp--;
	*vlp++ = 0;
	pushback(vlbuf);
	vflag = 0;
}

#define	NPAIR	(NC/2-6)	/* max pairs in spline, etc. */

void
setdraw (void)	/* generate internal cookies for a drawing function */
{
	int i, dx[NPAIR], dy[NPAIR], type;
	tchar c, delim;
#ifndef	NROFF
	int	hpos, vpos;
	int j, k;
	tchar drawbuf[NC];
#else
	extern int tlp, utf8;
	char drawbuf[NC];
#endif	/* NROFF */

	/* input is \D'f dx dy dx dy ... c' (or at least it had better be) */
	/* this does drawing function f with character c and the */
	/* specified dx,dy pairs interpreted as appropriate */
	/* pairs are deltas from last point, except for radii */

	/* l dx dy:	line from here by dx,dy */
	/* c x:		circle of diameter x, left side here */
	/* e x y:	ellipse of diameters x,y, left side here */
	/* a dx1 dy1 dx2 dy2:
			ccw arc: ctr at dx1,dy1, then end at dx2,dy2 from there */
	/* ~ dx1 dy1 dx2 dy2...:
			spline to dx1,dy1 to dx2,dy2 ... */
	/* f dx dy ...:	f is any other char:  like spline */

	if (ismot(c = getch()))
		return;
	delim = c;
	type = cbits(getch());
	for (i = 0; i < NPAIR ; i++) {
		c = getch();
		if (issame(c, delim))
			break;
	/* ought to pick up optional drawing character */
		if (cbits(c) != ' ')
			ch = c;
		vflag = 0;
		dfact = type == DRAWTHICKNESS ? 1 : EM;
		dx[i] = quant(hatoi(), HOR);
		if (dx[i] > MAXMOT)
			dx[i] = MAXMOT;
		else if (dx[i] < -MAXMOT)
			dx[i] = -MAXMOT;
		if (c = getch(), issame(c, delim)) {	/* spacer */
			dy[i++] = 0;
			break;
		}
		vflag = 1;
		dfact = lss;
		dy[i] = quant(hatoi(), VERT);
		if (type == DRAWTHICKNESS)
			dy[i] = 0;
		else if (dy[i] > MAXMOT)
			dy[i] = MAXMOT;
		else if (dy[i] < -MAXMOT)
			dy[i] = -MAXMOT;
	}
	dfact = 1;
	vflag = 0;
#ifndef NROFF
	drawbuf[0] = DRAWFCN | chbits | ZBIT;
	drawbuf[1] = type | chbits | ZBIT;
	drawbuf[2] = '.' | chbits | ZBIT;	/* use default drawing character */
	hpos = vpos = 0;
	for (k = 0, j = 3; k < i; k++) {
		drawbuf[j++] = MOT | ((dx[k] >= 0) ?
				sabsmot(dx[k]) : (NMOT | sabsmot(-dx[k])));
		drawbuf[j++] = MOT | VMOT | ((dy[k] >= 0) ?
				sabsmot(dy[k]) : (NMOT | sabsmot(-dy[k])));
		hpos += dx[k];
		vpos += dy[k];
	}
	if (type == DRAWELLIPSE || type == DRAWELLIPSEFI) {
		drawbuf[5] = drawbuf[4] | NMOT;	/* so the net vertical is zero */
		j = 6;
	}
	if (gflag && (type == DRAWPOLYGON || type == DRAWPOLYGONFI) &&
			(hpos || vpos)) {
		drawbuf[j++] = MOT | ((hpos < 0) ?
				sabsmot(-hpos) : (NMOT | sabsmot(hpos)));
		drawbuf[j++] = MOT | VMOT | ((vpos < 0) ?
				sabsmot(-vpos) : (NMOT | sabsmot(vpos)));
	}
	drawbuf[j++] = DRAWFCN | chbits | ZBIT;	/* marks end for ptout */
	drawbuf[j] = 0;
	pushback(drawbuf);
#else
	switch (type) {
	case 'l':
		if (dx[0] && !dy[0]) {
			if (dx[0] < 0) {
				snprintf(drawbuf, sizeof(drawbuf), "\\h'%du'",
				    dx[0]);
				cpushback(drawbuf);
			}
			snprintf(drawbuf, sizeof(drawbuf), "\\l'%du%s'",
			    dx[0], tlp ? "\\&-" : utf8 ? "\\U'2500'" : "");
			cpushback(drawbuf);
		} else if (dy[0] && !dx[0]) {
			snprintf(drawbuf, sizeof(drawbuf), "\\L'%du%s'",
			    dy[0], tlp ? "|" : utf8 ? "\\U'2502'" : "");
			cpushback(drawbuf);
		}
	}
#endif
}


void
casefc(void)
{
	register int i;
	tchar j;

	gchtab[fc] &= ~FCBIT;
	fc = IMP;
	padc = ' ';
	if (skip(0) || ismot(j = getch()) || (i = cbits(j)) == '\n')
		return;
	fc = i;
	gchtab[fc] |= FCBIT;
	if (skip(0) || ismot(ch) || (ch = cbits(ch)) == fc)
		return;
	padc = ch;
}


tchar 
setfield(int x)
{
	register tchar ii, jj, *fp;
	register int i, j, k;
	int length, ws, npad, temp, type;
	tchar **pp, *padptr[NPP];
	tchar fbuf[FBUFSZ];
	int savfc, savtc, savlc;
	tchar rchar = 0, nexti = 0;
	int savepos;
	int oev;

	prdblesc = 1;
	if (x == tabch) 
		rchar = tabc | chbits;
	else if (x ==  ldrch) 
		rchar = dotc | chbits;
	if (chartab[trtab[cbits(rchar)]] != 0)
		rchar = setchar(rchar);
	temp = npad = ws = 0;
	savfc = fc;
	savtc = tabch;
	savlc = ldrch;
	tabch = ldrch = fc = IMP;
	savepos = numtab[HP].val;
	gchtab[tabch] &= ~TABBIT;
	gchtab[ldrch] &= ~LDRBIT;
	gchtab[fc] &= ~FCBIT;
	gchtab[IMP] |= TABBIT|LDRBIT|FCBIT;
	for (j = 0; ; j++) {
		if ((tabtab[j] & TABMASK) == 0) {
			if (x == savfc)
				errprint("zero field width.");
			jj = 0;
			goto rtn;
		}
		if ((length = ((tabtab[j] & TABMASK) - numtab[HP].val)) > 0 )
			break;
	}
	type = tabtab[j] & (~TABMASK);
	fp = fbuf;
	pp = padptr;
	if (x == savfc) {
		*fp++ = mkxfunc(FLDMARK, 0);
		nexti = getch();
		while (1) {
			j = cbits(ii = nexti);
			jj = width(ii);
			oev = ev;
			if (j != savfc && j != '\n' &&
					pp < (padptr + NPP - 1) &&
					fp < (fbuf + FBUFSZ - 3))
				nexti = getch();
			else
				nexti = 0;
			if (ev == oev)
				jj += kernadjust(ii, nexti);
			widthp = jj;
			numtab[HP].val += jj;
			if (j == padc) {
				npad++;
				*pp++ = fp;
				if (pp > (padptr + NPP - 1))
					break;
				goto s1;
			} else if (j == savfc) 
				break;
			else if (j == '\n') {
				temp = j;
				nlflg = 0;
				break;
			}
			ws += jj;
s1:
			*fp++ = ii;
			if (fp > (fbuf + FBUFSZ - 3))
				break;
		}
		if (!npad) {
			npad++;
			*pp++ = fp;
			*fp++ = 0;
		}
		*fp++ = temp;
		*fp++ = 0;
		temp = i = (j = length - ws) / npad;
		i = (i / HOR) * HOR;
		if ((j -= i * npad) < 0)
			j = -j;
		ii = makem(i);
		if (temp < 0)
			ii |= NMOT;
		for (; npad > 0; npad--) {
			*(*--pp) = ii;
			if (j) {
				j -= HOR;
				(*(*pp)) += HOR;
			}
		}
		pushback(fbuf);
		jj = 0;
	} else if (type == 0) {
		/*plain tab or leader*/
		if (pbp >= pbsize-4)
			growpbbuf();
		pbbuf[pbp++] = mkxfunc(FLDMARK, 0);
		if ((j = width(rchar)) > 0) {
			int nchar;
			k = kernadjust(rchar, rchar);
			if (length < j)
				nchar = 0;
			else {
				nchar = 1;
				length -= j;
				nchar += length / (k+j);
				length %= k+j;
			}
			pbbuf[pbp++] = FILLER;
			while (nchar-->0) {
				if (pbp >= pbsize-5)
					if (growpbbuf() == NULL)
						break;
				numtab[HP].val += j;
				widthp = j;
				if (nchar > 0) {
					numtab[HP].val += k;
					widthp += k;
				}
				pbbuf[pbp++] = rchar;
			}
			pbbuf[pbp++] = FILLER;
		}
		if (length)
			jj = sabsmot(length) | MOT;
		else 
			jj = 0;
	} else {
		/*center tab*/
		/*right tab*/
		*fp++ = mkxfunc(FLDMARK, 0);
		nexti = getch();
		while (((j = cbits(ii = nexti)) != savtc) &&  (j != '\n') && (j != savlc)) {
			jj = width(ii);
			oev = ev;
			if (fp < (fbuf + FBUFSZ - 3)) {
				nexti = getch();
				if (ev == oev)
					jj += kernadjust(ii, nexti);
			}
			ws += jj;
			numtab[HP].val += jj;
			widthp = jj;
			*fp++ = ii;
			if (fp > (fbuf + FBUFSZ - 3)) 
				break;
		}
		*fp++ = ii;
		*fp++ = 0;
		if (type == RTAB)
			length -= ws;
		else 
			length -= ws / 2; /*CTAB*/
		pushback(fbuf);
		if ((j = width(rchar)) != 0 && length > 0) {
			int nchar;
			k = kernadjust(rchar, rchar);
			if (length < j)
				nchar = 0;
			else {
				nchar = 1;
				length -= j;
				nchar += length / (k+j);
				length %= k+j;
			}
			if (pbp >= pbsize-3)
				growpbbuf();
			pbbuf[pbp++] = FILLER;
			while (nchar-- > 0) {
				if (pbp >= pbsize-3)
					if (growpbbuf() == NULL)
						break;
				pbbuf[pbp++] = rchar;
			}
		}
		length = (length / HOR) * HOR;
		jj = makem(length);
		nlflg = 0;
	}
rtn:
	gchtab[fc] &= ~FCBIT;
	gchtab[tabch] &= ~TABBIT;
	gchtab[ldrch] &= ~LDRBIT;
	fc = savfc;
	tabch = savtc;
	ldrch = savlc;
	gchtab[fc] |= FCBIT;
	gchtab[tabch] = TABBIT;
	gchtab[ldrch] |= LDRBIT;
	numtab[HP].val = savepos;
	if (pbp < pbsize-3 || growpbbuf())
		pbbuf[pbp++] = mkxfunc(FLDMARK, x);
	prdblesc = 0;
	return(jj | ADJBIT);
}


static int
readpenalty(int *valp)
{
	int	n, t;

	t = dpenal ? dpenal - INFPENALTY0 - 1 : 0;
	noscale++;
	n = inumb(&t);
	noscale--;
	if (nonumb)
		return 0;
	if (n > INFPENALTY0)
		n = INFPENALTY0;
	else if (n < -INFPENALTY0)
		n = -INFPENALTY0;
	n += INFPENALTY0 + 1;
	*valp = n;
	return 1;
}

static int
getpenalty(int *valp)
{
	tchar	c, delim;

	if (ismot(delim = getch()))
		return 0;
	if (readpenalty(valp) == 0)
		return 0;
	c = getch();
	if (!issame(c, delim)) {
		nodelim(delim);
		return 0;
	}
	return 1;
}

tchar
setpenalty(void)
{
	int	n;

	if (getpenalty(&n))
		return mkxfunc(PENALTY, n);
	return 0;
}

tchar
setdpenal(void)
{
	if (getpenalty(&dpenal))
		return mkxfunc(DPENAL, dpenal);
	return 0;
}


tchar
mkxfunc(int f, int s)
{
	tchar	t = XFUNC;
	setfbits(t, f);
	setsbits(t, s);
	return t;
}

void
pushinlev(void)
{
	if (ninlev >= ainlev) {
		ainlev += 4;
		inlevp = realloc(inlevp, ainlev * sizeof *inlevp);
	}
	inlevp[ninlev]._apts = apts;
	inlevp[ninlev]._apts1 = apts1;
	inlevp[ninlev]._pts = pts;
	inlevp[ninlev]._pts1 = pts1;
	inlevp[ninlev]._font = font;
	inlevp[ninlev]._font1 = font1;
	inlevp[ninlev]._cc = cc;
	inlevp[ninlev]._c2 = c2;
	inlevp[ninlev]._ohc = ohc;
	inlevp[ninlev]._hyf = hyf;
	inlevp[ninlev]._tabc = tabc;
	inlevp[ninlev]._dotc = dotc;
	inlevp[ninlev]._dpenal = dpenal;
	ninlev++;
}

tchar
popinlev(void)
{
	tchar	c = 0;

	if (--ninlev < 0) {
		ninlev = 0;
		return c;
	}
	if (dpenal != inlevp[ninlev]._dpenal)
		c = mkxfunc(DPENAL, inlevp[ninlev]._dpenal);
	apts = inlevp[ninlev]._apts;
	apts1 = inlevp[ninlev]._apts1;
	pts = inlevp[ninlev]._pts;
	pts1 = inlevp[ninlev]._pts1;
	font = inlevp[ninlev]._font;
	font1 = inlevp[ninlev]._font1;
	cc = inlevp[ninlev]._cc;
	c2 = inlevp[ninlev]._c2;
	ohc = inlevp[ninlev]._ohc;
	hyf = inlevp[ninlev]._hyf;
	tabc = inlevp[ninlev]._tabc;
	dotc = inlevp[ninlev]._dotc;
	dpenal = inlevp[ninlev]._dpenal;
	mchbits();
	if (ninlev == 0) {
		free(inlevp);
		inlevp = NULL;
		ainlev = 0;
	}
	return c;
}

#ifdef EUC
/* locale specific initialization */
void
localize(void)
{
	extern int	wdbindf(wchar_t, wchar_t, int);
	extern wchar_t	*wddelim(wchar_t, wchar_t, int);
	char	*codeset;

	codeset = nl_langinfo(CODESET);

	if (mb_cur_max > 1)
		multi_locale = 1;
	else {
		if (*codeset == '\0' ||
			(strcmp(codeset, ISO646) == 0)) {
			/*
			 * if codeset is an empty string
			 * assumes this is C locale (7-bit) locale.
			 * This happens in 2.5, 2.5.1, and 2.6 system
			 * Or, if codeset is "646"
			 * this is 7-bit locale.
			 */
			multi_locale = 0;
		} else {
			/* 8-bit locale */
			multi_locale = 1;
		}

	}
	wdbdg = wdbindf;
	wddlm = wddelim;
}

#ifndef	__sun
int
wdbindf(wchar_t wc1, wchar_t wc2, int type)
{
	return 6;
}

wchar_t *
wddelim(wchar_t wc1, wchar_t wc2, int type)
{
	return L" ";
}
#endif	/* !__sun */
#endif /* EUC */

void
caselc_ctype(void)
{
#ifdef	EUC
	char	c, *buf = NULL;
	int	i = 0, sz = 0;

	skip(1);
	do {
		c = getach()&0377;
		if (i >= sz)
			buf = realloc(buf, (sz += 8) * sizeof *buf);
		buf[i++] = c;
	} while (c && c != ' ' && c != '\n');
	buf[i-1] = 0;
	setlocale(LC_CTYPE, buf);
	mb_cur_max = MB_CUR_MAX;
	localize();
#ifndef	NROFF
	ptlocale(buf);
#endif
	free(buf);
#endif
}

#ifndef	NROFF
struct fg {
	char	buf[512];
	char	*bp;
	char	*ep;
	int	fd;
	int	eof;
};

static int
psskip(struct fg *fp, size_t n)
{
	size_t	i;

	if (fp->eof)
		return -1;
	if (fp->bp < fp->ep) {
		i = fp->ep - fp->bp;
		if (i > n) {
			fp->bp += n;
			return 0;
		}
		fp->bp = fp->buf;
		n -= i;
	}
	if (lseek(fp->fd, n, SEEK_CUR) == (off_t)-1)
		return -1;
	return 0;
}

static int
psgetline(struct fg *fp, char **linebp, size_t *linesize)
{
	int	i, n = 0;
	int	nl = 0;

	if (fp->bp == NULL)
		fp->bp = fp->buf;
	for (;;) {
		if (fp->eof == 0 && fp->bp == fp->buf) {
			if ((i = read(fp->fd, fp->buf, sizeof fp->buf)) <= 0)
				fp->eof = 1;
			fp->ep = &fp->buf[i];
		}
		for (;;) {
			if (*linesize < n + 2)
				*linebp = realloc(*linebp, *linesize += 128);
			if (fp->bp >= fp->ep)
				break;
			if (*fp->bp == '\n' || nl) {
				nl = 2;
				break;
			}
			if (*fp->bp == '\r')
				nl = 1;
			(*linebp)[n++] = *fp->bp++;
		}
		if (fp->bp < fp->ep && *fp->bp == '\n') {
			(*linebp)[n++] = *fp->bp++;
			break;
		}
		if (nl == 2 || fp->eof)
			break;
		fp->bp = fp->buf;
	}
	(*linebp)[n] = 0;
	return n;
}

static char *
getcom(const char *cp, const char *tp)
{
	int	n;

	n = strlen(tp);
	if (strncmp(cp, tp, n))
		return NULL;
	if (cp[n] == ' ' || cp[n] == '\t' || cp[n] == '\r' ||
			cp[n] == '\n' || cp[n] == 0)
		return (char *)&cp[n];
	return NULL;
}

static void
getpsbb(const char *name, double bb[4])
{
	struct fg	*fp;
	char	*buf = NULL;
	char	*cp;
	size_t	size = 0;
	int	fd, n, k;
	int	lineno = 0;
	int	found = 0;
	int	atend = 0;
	int	state = 0;
	int	indoc = 0;

	if ((fd = open(name, O_RDONLY)) < 0) {
		errprint("can't open %s", name);
		return;
	}
	fp = calloc(1, sizeof *fp);
	fp->fd = fd;
	for (;;) {
		n = psgetline(fp, &buf, &size);
		if (++lineno == 1 && (n == 0 || strncmp(buf, "%!PS-", 5))) {
			errprint("%s is not a DSC-conforming "
					"PostScript document", name);
			break;
		}
		if (n > 0 && state != 1 &&
				(cp = getcom(buf, "%%BoundingBox:")) != NULL) {
			while (*cp == ' ' || *cp == '\t')
				cp++;
			if (strncmp(cp, "(atend)", 7) == 0) {
				atend++;
				continue;
			}
			bb[0] = strtod(cp, &cp);
			if (*cp)
				bb[1] = strtod(cp, &cp);
			if (*cp)
				bb[2] = strtod(cp, &cp);
			if (*cp) {
				bb[3] = strtod(cp, &cp);
				found = 1;
			} else
				errprint("missing arguments to "
					"%%%%BoundingBox: in %s, line %d\n",
					name, lineno);
			continue;
		}
		if (n > 0 && state != 1 &&
				(cp = getcom(buf, "%%HiResBoundingBox:"))
				!= NULL) {
			while (*cp == ' ' || *cp == '\t')
				cp++;
			if (strncmp(cp, "(atend)", 7) == 0) {
				atend++;
				continue;
			}
			bb[0] = strtod(cp, &cp);
			if (*cp)
				bb[1] = strtod(cp, &cp);
			if (*cp)
				bb[2] = strtod(cp, &cp);
			if (*cp) {
				bb[3] = strtod(cp, &cp);
				break;
			} else {
				errprint("missing arguments to "
					"%%%%HiResBoundingBox: in %s, "
					"line %d\n",
					name, lineno);
				continue;
			}
		}
		if (n == 0 || (state == 0 &&
				(getcom(buf, "%%EndComments") != NULL ||
				 buf[0] != '%' || buf[1] == ' ' ||
				 buf[1] == '\t' || buf[1] == '\r' ||
				 buf[1] == '\n'))) {
		eof:	if (found == 0 && (atend == 0 || n == 0))
				errprint("%s lacks a %%%%BoundingBox: DSC "
					"comment", name);
			if (atend == 0 || n == 0)
				break;
			state = 1;
			continue;
		}
		if (indoc == 0 && getcom(buf, "%%EOF") != NULL) {
			n = 0;
			goto eof;
		}
		if (state == 1 && indoc == 0 &&
				getcom(buf, "%%Trailer") != NULL) {
			state = 2;
			continue;
		}
		if (state == 1 && getcom(buf, "%%BeginDocument:") != NULL) {
			indoc++;
			continue;
		}
		if (state == 1 && indoc > 0 &&
				getcom(buf, "%%EndDocument") != NULL) {
			indoc--;
			continue;
		}
		if (state == 1 &&
				(cp = getcom(buf, "%%BeginBinary:")) != NULL) {
			if ((k = strtol(cp, &cp, 10)) > 0)
				psskip(fp, k);
			continue;
		}
		if (state == 1 && (cp = getcom(buf, "%%BeginData:")) != NULL) {
			if ((k = strtol(cp, &cp, 10)) > 0) {
				while (*cp == ' ' || *cp == '\t')
					cp++;
				while (*cp && *cp != ' ' && *cp != '\t')
					cp++;
				while (*cp == ' ' || *cp == '\t')
					cp++;
				if (strncmp(cp, "Bytes", 5) == 0)
					psskip(fp, k);
				else if (strncmp(cp, "Lines", 5) == 0) {
					while (k--) {
						n = psgetline(fp, &buf, &size);
						if (n == 0)
							goto eof;
					}
				}
			}
			continue;
		}
	}
	free(fp);
	free(buf);
	close(fd);
}
#endif	/* !NROFF */

void
casepsbb(void)
{
#ifndef	NROFF
	char	*buf = NULL;
	int	c;
	int	n = 0, sz = 0;
	double	bb[4] = { 0, 0, 0, 0 };

	lgf++;
	skip(1);
	do {
		c = getach();
		if (n >= sz)
			buf = realloc(buf, (sz += 14) * sizeof *buf);
		buf[n++] = c;
	} while (c);
	getpsbb(buf, bb);
	free(buf);
	setnrf("llx", bb[0], 0);
	setnrf("lly", bb[1], 0);
	setnrf("urx", bb[2], 0);
	setnrf("ury", bb[3], 0);
#endif	/* !NROFF */
}

static const struct {
	enum warn	n;
	const char	*s;
} warnnames[] = {
	{ WARN_NONE,	"none" },
	{ WARN_CHAR,	"char" },
	{ WARN_NUMBER,	"number" },
	{ WARN_BREAK,	"break" },
	{ WARN_DELIM,	"delim" },
	{ WARN_EL,	"el" },
	{ WARN_SCALE,	"scale" },
	{ WARN_RANGE,	"range" },
	{ WARN_SYNTAX,	"syntax" },
	{ WARN_DI,	"di" },
	{ WARN_MAC,	"mac" },
	{ WARN_REG,	"reg" },
	{ WARN_RIGHT_BRACE, "right-brace" },
	{ WARN_MISSING,	"missing" },
	{ WARN_INPUT,	"input" },
	{ WARN_ESCAPE,	"escape" },
	{ WARN_SPACE,	"space" },
	{ WARN_FONT,	"font" },
	{ WARN_ALL,	"all" },
	{ WARN_W,	"w" },
	{ 0,		NULL }
};

static int
warn1(void)
{
	char	name[NC];
	int	i, n, sign;
	tchar	c;

	switch (cbits(c = getch())) {
	case '-':
		c = getch();
		sign = -1;
		break;
	case '+':
		c = getch();
		sign = 1;
		break;
	default:
		sign = 0;
		break;
	case 0:
		return 1;
	}
	ch = c;
	n = atoi0();
	if ((i = cbits(ch)) != 0 && i != ' ' && i != '\n') {
		if (c != ch) {
			while (getach());
			errprint("illegal number, char %c", i);
			return 1;
		}
		for (i = 0; i < sizeof name - 2; i++) {
			if ((c = getach()) == 0)
				break;
			name[i] = c;
		}
		name[i] = 0;
		for (i = 0; warnnames[i].s; i++)
			if (strcmp(name, warnnames[i].s) == 0) {
				n = warnnames[i].n;
				break;
			}
		if (warnnames[i].s == NULL) {
			errprint("unknown warning category %s", name);
			return 1;
		}
	}
	switch (sign) {
	case 1:
		warn |= n;
		break;
	case -1:
		warn &= ~n;
		break;
	default:
		warn = n;
	}
	return 0;
}

void
casewarn(void)
{
	if (skip(0))
		warn = WARN_W;
	else
		while (!warn1() && !skip(0));
}

void
nosuch(int rq)
{
	if (rq && rq != RIGHT && rq != PAIR(RIGHT, RIGHT) && warn & WARN_MAC)
		errprint("%s: no such request", macname(rq));
}

void
missing(void)
{
	if (warn & WARN_MISSING) {
		if (lastrq)
			errprint("%s: missing argument", macname(lastrq));
		else
			errprint("missing argument");
	}
}

void
nodelim(int delim)
{
	if (warn & WARN_DELIM)
		errprint("%c delimiter missing", (int)delim);
}

void
illseq(int wc, const char *mb, int n)
{
	if ((warn & WARN_INPUT) == 0)
		return;
	if (n == -3)
		errprint("non-ASCII input byte 0x%x terminates name", wc);
	else if (n == 0) {
		if (wc & ~0177)
			errprint("ignoring '%U' in input", wc);
		else
			errprint("ignoring '\\%o' in input", wc);
	} else
		errprint("illegal byte sequence at '\\%o' in input", *mb&0377);
}

void
storerq(int i)
{
	tchar	tp[2];

	tp[0] = mkxfunc(RQ, i);
	tp[1] = 0;
	pushback(tp);
}

int
fetchrq(tchar *tp)
{
	if (ismot(tp[0]) || !isxfunc(tp[0], RQ))
		return 0;
	return sbits(tp[0]);
}

void
morechars(int n)
{
	int	i, nnc;

	if (n <= NCHARS)
		return;
	for (nnc = 1024; nnc <= n; nnc <<= 1);
	widcache = realloc(widcache, nnc * sizeof *widcache);
	memset(&widcache[NCHARS], 0, (nnc-NCHARS) * sizeof *widcache);
	trtab = realloc(trtab, nnc * sizeof *trtab);
	trnttab = realloc(trnttab, nnc * sizeof *trnttab);
	for (i = NCHARS; i < nnc; i++)
		trnttab[i] = trtab[i] = i;
	trintab = realloc(trintab, nnc * sizeof *trintab);
	memset(&trintab[NCHARS], 0, (nnc-NCHARS) * sizeof *trintab);
	gchtab = realloc(gchtab, nnc * sizeof *gchtab);
	memset(&gchtab[NCHARS], 0, (nnc-NCHARS) * sizeof *gchtab);
	chartab = realloc(chartab, nnc * sizeof *chartab);
	memset(&chartab[NCHARS], 0, (nnc-NCHARS) * sizeof *chartab);
#ifndef	NROFF
	fchartab = realloc(fchartab, nnc * sizeof *fchartab);
	memset(&fchartab[NCHARS], 0, (nnc-NCHARS) * sizeof *fchartab);
	for (i = 0; i <= nfonts; i++) {
		extern short	**fitab;
		if (fitab != NULL && fitab[i] != NULL) {
			fitab[i] = realloc(fitab[i], nnc * sizeof **fitab);
			memset(&fitab[i][NCHARS], 0,
					(nnc-NCHARS) * sizeof **fitab);
		}
		if (lhangtab != NULL && lhangtab[i] != NULL) {
			lhangtab[i] = realloc(lhangtab[i],
					nnc * sizeof **lhangtab);
			memset(&lhangtab[i][NCHARS], 0,
					(nnc-NCHARS) * sizeof **lhangtab);
		}
		if (lhangtab != NULL && rhangtab[i] != NULL) {
			rhangtab[i] = realloc(rhangtab[i],
					nnc * sizeof **rhangtab);
			memset(&rhangtab[i][NCHARS], 0,
					(nnc-NCHARS) * sizeof **rhangtab);
		}
		if (kernafter != NULL && kernafter[i] != NULL) {
			kernafter[i] = realloc(kernafter[i],
					nnc * sizeof **kernafter);
			memset(&kernafter[i][NCHARS], 0,
					(nnc-NCHARS) * sizeof **kernafter);
		}
		if (kernbefore != NULL && kernbefore[i] != NULL) {
			kernbefore[i] = realloc(kernbefore[i],
					nnc * sizeof **kernbefore);
			memset(&kernbefore[i][NCHARS], 0,
					(nnc-NCHARS) * sizeof **kernbefore);
		}
		if (ftrtab != NULL && ftrtab[i] != NULL) {
			int	j;
			ftrtab[i] = realloc(ftrtab[i], nnc * sizeof **ftrtab);
			for (j = NCHARS; j < nnc; j++)
				ftrtab[i][j] = j;
		}
		if (lgtab != NULL && lgtab[i] != NULL) {
			lgtab[i] = realloc(lgtab[i], nnc * sizeof **lgtab);
			memset(&lgtab[i][NCHARS], 0,
					(nnc-NCHARS) * sizeof **lgtab);
		}
		if (lgrevtab != NULL && lgrevtab[i] != NULL) {
			lgrevtab[i] = realloc(lgrevtab[i],
					nnc * sizeof **lgrevtab);
			memset(&lgrevtab[i][NCHARS], 0,
					(nnc-NCHARS) * sizeof **lgrevtab);
		}
	}
#endif	/* !NROFF */
	NCHARS = nnc;
}
