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


/*	from OpenSolaris "n7.c	1.10	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)n7.c	1.181 (gritter) 6/19/11
 *
 * Portions Copyright (c) 2014, 2015 Carsten Kunze
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

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include "tdef.h"
#ifdef NROFF
#include "tw.h"
#include "draw.h"
#endif
#include "pt.h"
#ifdef NROFF
#define GETCH gettch
tchar	gettch(void);
#endif
#ifndef NROFF
#define GETCH getch
#endif

/*
 * troff7.c
 * 
 * text
 */

#include <math.h>
#include <string.h>
#include <ctype.h>
#include "ext.h"
#if defined (EUC) && defined (NROFF) && defined (ZWDELIMS)
wchar_t	cwc, owc, wceoll;
#endif /* EUC && NROFF && ZWDELIMS */
int	brflg;

#undef	iswascii
#define	iswascii(c)	(((c) & ~(wchar_t)0177) == 0)

static int	_findt(struct d *, int, int);
static tchar	adjbit(tchar);
static void	sethtdp(void);
static void	leftend(tchar, int, int);
static void	parword(void);
static void	parfmt(void);
#ifndef	NROFF
#define	nroff		0
extern int	lastrst;
extern int	lastrsb;
static void	setlhang(tchar);
static void	setrhang(void);
static void	letshrink(void);
static int	letgrow(void);
static int	lspcomp(int);
#else	/* NROFF */
#define	nroff		1
#define	lastrst		0
#define	lastrsb		0
#define	setlhang(a)
#define	setrhang()
#define	getlsp(c)	0
#define	storelsp(a, b)
#define	getlsh(c, w)	0
#define	storelsh(a, b)
#define	letshrink()
#define	letgrow()	0
#define	lspcomp(a)	0
#endif	/* NROFF */

void
tbreak(void)
{
	register int pad, k;
	register tchar	*i, j, c;
	register int resol = 0;
	int _minflg;

restart:
	trap = 0;
	if (nb)
		return;
	if (dip == d && numtab[NL].val == -1) {
		newline(1);
		return;
	}
	if (!nc && !pgchars) {
		setnel();
		if (!wch)
			return;
		if (pendw)
			getword(1);
		movword();
	} else if (pendw && !brflg) {
		getword(1);
		movword();
	} else if (!brflg && adflg & 1) {
		adflg |= 2;
		text();
		return;
	}
	if ((pa || padj) && pglines == 0 && pgchars) {
		parfmt();
		goto restart;
	}
	if (minspsz && !brflg && ad && !admod)
		ne += adspc;
	*linep = dip->nls = 0;
#ifdef NROFF
	if (dip == d)
		horiz(po);
#endif
	if (lnmod)
		donum();
	lastl = ne;
	if (brflg != 1) {
		totout = 0;
		hlc = 0;
	} else if (ad) {
		if ((lastl = ll - un + rhang + lsplast) < ne)
			lastl = ne;
	}
	if (admod && ad && (brflg != 2)) {
		lastl = ne;
		adsp = adrem = 0;
		if (admod == 1)
			un +=  quant(nel / 2, HOR);
		else if (admod == 2)
			un += nel;
	}
	_minflg = minspsz && admod == 0 && ad && brflg == 1 && adflg & 4;
	totout++;
	brflg = 0;
	if (lastl + un > dip->maxl)
		dip->maxl = lastl + un;
	horiz(un);
	if (un != 0)
		pchar(mkxfunc(INDENT, un));
#ifdef NROFF
	if (adrem % t.Adj)
		resol = t.Hor; 
	else 
		resol = t.Adj;
#else
	resol = HOR;
#endif
	adrem = (adrem / resol) * resol;
	for (i = line; nc > 0; ) {
		if ((c = cbits(j = *i++)) == ' ' || c == STRETCH) {
			if ((xflag && !fi && dilev) || iszbit(j) || isadjspc(j))
				goto std;
			pad = 0;
			if (i > &line[1])
				pad += kernadjust(i[-2], i[-1]);
			do {
				if (xflag)
					pchar(adjbit(j));
				minflg = _minflg;
				pad += width(j);
				nc--;
			} while ((c = cbits(j = *i++)) == ' ' || c == STRETCH);
			pad += kernadjust(i[-2], i[-1]);
			i--;
			pad += adsp;
			--nwd;
			if (adrem) {
				if (adrem < 0) {
					pad -= resol;
					adrem += resol;
				} else if ((totout & 01) || adrem / resol >= nwd) {
					pad += resol;
					adrem -= resol;
				}
			}
			pchar((tchar) WORDSP);
			horiz(pad);
		} else {
		std:
			if (!ismot(j) && isxfunc(j, FLDMARK))
				fldcnt--;
			else if (fldcnt == 0 && nc && !ismot(j) &&
					!iszbit(j)) {
				if (lspcur && (cbits(j) > ' ' ||
							isxfunc(j, CHAR))) {
					for (c = j; isxfunc(c, CHAR);
						c = charout[sbits(c)].ch);
					k = (int)sbits(c) / 2 * lspcur / LAFACT;
					if (k >= 0)
						c = mkxfunc(LETSP, k);
					else
						c = mkxfunc(NLETSP, -k);
					pchar(c);
				}
				if (lshcur && cbits(j) > ' ') {
					k = lshcur;
					if (k >= 0)
						c = mkxfunc(LETSH, k);
					else
						c = mkxfunc(NLETSH, -k);
					pchar(c);
				}
			}
			pchar(j);
			nc--;
		}
	}
	if (hlc)
		pchar(mkxfunc(HYPHED, 0));
	if (ic) {
		if ((k = ll - un - lastl + ics) > 0)
			horiz(k);
		pchar(ic);
	}
	if (icf)
		icf++;
	else 
		ic = 0;
	ne = nwd = 0;
	un = in;
	setnel();
	newline(0);
	if (dip != d) {
		if (dip->dnl > dip->hnl)
			dip->hnl = dip->dnl;
	} else {
		if (numtab[NL].val > dip->hnl)
			dip->hnl = numtab[NL].val;
	}
	for (k = ls - 1; k > 0 && !trap; k--)
		newline(0);
	spread = 0;
	spbits = 0;
}

void
donum(void)
{
	register int i, nw;

	nrbits = nmbits;
	nw = width('1' | nrbits);
	if (nn) {
		nn--;
		goto d1;
	}
	if (numtab[LN].val % ndf) {
		numtab[LN].val++;
d1:
		un += nw * (3 + nms + ni);
		return;
	}
	i = 0;
	if (numtab[LN].val < 100)
		i++;
	if (numtab[LN].val < 10)
		i++;
	horiz(nw * (ni + i));
	nform = 0;
	fnumb(numtab[LN].val, pchar);
	un += nw * nms;
	numtab[LN].val++;
}

void
text(void)
{
	register tchar i, c, lasti = 0;
	int	k = 0;
	static int	spcnt;
	int	recadj = 0;

	if (adflg & 2) {
		adflg &= ~3;
		recadj = 1;
		goto adj;
	}
	adflg = 0;
	nflush++;
	numtab[HP].val = 0;
	if ((dip == d) && (numtab[NL].val == -1)) {
		newline(1); 
		goto r;
	}
	setnel();
	if (ce || rj || !fi) {
		nofill();
		goto r;
	}
	if (pendw)
		goto t4;
	if (pendt) {
		if (spcnt)
			goto t2; 
		else 
			goto t3;
	}
	pendt++;
	if (spcnt)
		goto t2;
	while ((c = cbits(i = GETCH())) == ' ' || c == STRETCH) {
		if (iszbit(i))
			break;
		if (isadjspc(i))
			continue;
		spcnt++;
		widthp = xflag ? width(i) : sps;
		sethtdp();
		numtab[HP].val += widthp;
		lasti = i;
	}
	if (lasti) {
		k = kernadjust(lasti, i);
		numtab[HP].val += k;
		widthp += k;
	}
	if (nlflg) {
t1:
		nflush = pendt = ch = spcnt = 0;
		callsp();
		goto r;
	}
	ch = i;
	if (spcnt) {
t2:
		lsn = spcnt;
		glss = spcnt * sps + k;
		if (lsmac) {
			spcnt = 0;
			control(lsmac, 0);
			goto rtn;
		} else {
		tbreak();
		if (nc || wch)
			goto rtn;
		un += glss;
		spcnt = 0;
		setnel();
		if (trap)
			goto rtn;
		if (nlflg)
			goto t1;
		}
	}
t3:
	if (spread && !brpnl)
		goto t5;
	if (pendw || !wch)
t4:
		if (getword(0)) {
			if (!pendw) {
				if (brnl || (brpnl && spread))
					goto tb;
				if (brpnl)
					goto t5;
			}
			goto t6;
		}
	if (!movword())
		goto t3;
t5:
	if (nlflg)
		pendt = 0;
adj:
	adsp = adrem = 0;
	if (ad) {
		setrhang();
		if (admod == 0 && lspcur == 0 && lshcur == 0 &&
				nel < 0 && lspnc)
			letshrink();
	jst:	if (nwd == 1)
			adsp = nel; 
		else 
			adsp = nel / (nwd - 1);
		adsp = (adsp / HOR) * HOR;
		if (admod == 0 && lspcur == 0 && lshcur == 0 &&
				adsp > letsps - sps)
			if (letgrow())
				goto jst;
		adrem = nel - adsp*(nwd-1);
		if (admod == 0 && nwd == 1 && warn & WARN_BREAK)
			errprint("can't break line");
		else if (admod == 0 && spreadwarn && adsp >= spreadlimit)
			errprint("spreadlimit exceeded, %gm", (double)adsp/EM);
	}
	brflg = 1;
tb:
	tbreak();
	spread = 0;
	if (!trap && !recadj && !brnl && !brpnl)
		goto t3;
	if (brnl > 0 && brnl < INT_MAX)
		brnl--;
	if (brpnl > 0 && brpnl < INT_MAX)
		brpnl--;
	if (!nlflg)
		goto rtn;
t6:
	pendt = 0;
	ckul();
rtn:
	nflush = 0;
r:
	if (chomp) {
		chomp = 0;
		chompend = 1;
	}
}


void
nofill(void)
{
	register int j;
	register tchar i, nexti;
	int k, oev;

	if (chompend) {
		chompend = 0;
	} else if (!pendnf && !chomp) {
		over = 0;
		tbreak();
		if (trap)
			goto rtn;
		if (nlflg) {
			ch = nflush = 0;
			callsp();
			return;
		}
		adsp = adrem = 0;
		nwd = 10000;
	}
	nexti = GETCH();
	leftend(nexti, !ce && !rj && !pendnf, !isdi(nexti));
	while ((j = (cbits(i = nexti))) != '\n') {
		if (stopch && issame(i, stopch))
			break;
		if (j == ohc) {
			nexti = GETCH();
			continue;
		}
		if (j == CONT) {
			pendnf++;
			nflush = 0;
			flushi();
			ckul();
			return;
		}
		j = width(i);
		widthp = j;
		sethtdp();
		numtab[HP].val += j;
		storeline(i, j);
		oev = ev;
		nexti = GETCH();
		if (ev == oev) {
			k = kernadjust(i, nexti);
			ne += k;
			nel -= k;
			numtab[HP].val += k;
		}
	}
	if (chomp) {
		return;
	}
	if (ce) {
		ce--;
		if ((i = quant(nel / 2, HOR)) > 0)
			un += i;
	}
	if (rj) {
		rj--;
		setrhang();
		if (nel > 0)
			un += nel;
	}
	if (!nc)
		storeline((tchar)FILLER, 0);
	brflg = 2;
	tbreak();
	ckul();
rtn:
	pendnf = nflush = 0;
}


void
callsp(void)
{
	register int i;

	if (flss)
		i = flss; 
	else 
		i = lss;
	flss = 0;
	if (blmac && (fi || (frame->flags & FLAG_DIVERSION) == 0))
		control(blmac, 0);
	else
		casesp(i);
}


void
ckul(void)
{
	if (ul && (--ul == 0)) {
		cu = 0;
		font = sfont;
		mchbits();
	}
	if (it && !pglines && (!itc || (!pendw && !pendnf)) &&
			(--it == 0) && itmac)
		control(itmac, 0);
}


int
storeline(register tchar c, int w)
{
#ifdef NROFF
	if (ismot(c)) {
		if (isvmot(c)) {
			if (isnmot(c))
				lvmot -= c & BMBITS;
			else
				lvmot += c & BMBITS;
		}
	} else if (ndraw && !(cbits(c) & ~0xffff)) {
		storechar(c, numtab[NL].val+lvmot, po + in + ne);
	}
#endif
	if (linep == NULL || linep >= line + lnsize - 4) {
		tchar	*k;
		if (over)
			return 0;
		lnsize += lnsize ? 100 : LNSIZE;
		if ((k = realloc(line, lnsize * sizeof *line)) == NULL) {
			flusho();
			errprint("Line overflow.");
			over++;
			c = LEFTHAND;
			w = -1;
			goto s1;
		}
		linep = (tchar *)((char *)linep + ((char *)k - (char *)line));
		line = k;
	}
s1:
	if (w == -1) {
		minflg = minspsz && ad && !admod;
		w = width(c);
	}
	ne += w;
	nel -= w;
	*linep++ = c;
	nc++;
	return 1;
}


#ifndef	NROFF
static int
getlsp(tchar c)
{
	int	s;

	if (!ad || admod || ismot(c))
		return 0;
	if (iszbit(c) || (cbits(c) <= ' ' && !isxfunc(c, CHAR)))
		return 0;
	while (isxfunc(c, CHAR))
		c = charout[sbits(c)].ch;
	s = sbits(c) / 2;
	return s;
}

static void
storelsp(tchar c, int neg)
{
	int	s;

	if (isxfunc(c, FLDMARK)) {
		lsplow = lsphigh = lspnc = 0;
		fldcnt += neg ? -1 : 1;
		return;
	}
	if ((s = getlsp(c)) != 0) {
		if (neg)
			s = -s;
		lsplow += s * lspmin / LAFACT;
		lsphigh += s * lspmax / LAFACT;
		lspnc += neg ? -1 : 1;
	}
}

static int
getlsh(tchar c, int w)
{
	if (!ad || admod || ismot(c))
		return 0;
	if (iszbit(c) || cbits(c) <= ' ')
		return 0;
	return w;
}

static void
storelsh(tchar c, int w)
{
	int	s;

	if (isxfunc(c, FLDMARK)) {
		lshlow = lshhigh = lshwid = 0;
		return;
	}
	if ((s = getlsh(c, w)) != 0) {
		lshwid += s;
		lshlow = lshwid * lshmin / LAFACT;
		lshhigh = lshwid * lshmax / LAFACT;
	}
}
#endif	/* !NROFF */

void
newline(int a)
{
	register int i, j, nlss = 0, nl;
	int	opn;

#ifdef NROFF
	if (lvmot) {
		tchar c;
		c = MOT | VMOT | (lvmot < 0 ? -lvmot : lvmot | NMOT);
		pchar1(c);
		lvmot = 0;
	}
#endif
	if (a)
		goto nl1;
	if (dip != d) {
		j = lss;
		pchar1((tchar)FLSS);
		if (flss)
			lss = flss;
		i = nlss = lss + dip->blss;
		dip->dnl += i;
		pchar1((tchar)i);
		pchar1((tchar)'\n');
		lss = j;
		dip->blss = flss = 0;
		if (dip->alss) {
			pchar1((tchar)FLSS);
			pchar1((tchar)dip->alss);
			pchar1((tchar)'\n');
			dip->dnl += dip->alss;
			nlss += dip->alss;
			dip->alss = 0;
		}
		if (vpt > 0 && dip->ditrap && !dip->ditf && dip->dnl >= dip->ditrap && dip->dimac)
			if (control(dip->dimac, 0)) {
				trap++; 
				dip->ditf++;
			}
		goto nlt;
	}
	j = lss;
	if (flss)
		lss = flss;
	nlss = dip->alss + dip->blss + lss;
	numtab[NL].val += nlss;
#ifndef NROFF
	if (ascii) {
		dip->alss = dip->blss = 0;
	}
#endif
	pchar1((tchar)'\n');
	flss = 0;
	lss = j;
	if (vpt == 0 || numtab[NL].val < pl)
		goto nl2;
nl1:
	ejf = dip->hnl = numtab[NL].val = 0;
	ejl = frame->tail_cnt;
	if (donef || (ndone && pgchars && !pglines)) {
		if ((!nc && !wch && !pglines) || ndone)
			done1(0);
		ndone++;
		donef = 0;
		if (frame == stk)
			nflush++;
	}
	opn = numtab[PN].val;
	numtab[PN].val++;
	if (npnflg) {
		numtab[PN].val = npn;
		npn = npnflg = 0;
	}
	prwatchn(&numtab[PN]);
nlpn:
	if (numtab[PN].val == pfrom) {
		print++;
		pfrom = -1;
	} else if (opn == pto) {
		print = 0;
		opn = -1;
		chkpn();
		goto nlpn;
	}
	if (print)
		newpage(numtab[PN].val);	/* supposedly in a clean state so can pause */
	if (stop && print) {
		dpn++;
		if (dpn >= stop) {
			dpn = 0;
			dostop();
		}
	}
nl2:
	trap = 0;
nlt:
	if (dip != d)
		nl = dip->dnl;
	else
		nl = numtab[NL].val;
	if (vpt <= 0)
		/*EMPTY*/;
	else if (nl == 0) {
		if ((j = findn(dip, 0)) != NTRAP)
			trap |= control(dip->mlist[j], 0);
	} else if ((i = _findt(dip, nl - nlss, 0)) <= nlss) {
		if ((j = findn1(dip, nl - nlss + i)) == NTRAP) {
			flusho();
			errprint("Trap botch.");
			done2(-5);
		}
		trap |= control(dip->mlist[j], 0);
	}
	if (nolt && dip == d) {
		for (i = nolt - 1; i >= 0; i--)
			trap |= control(olt[i], 0);
		nolt = 0;
		free(olt);
		olt = NULL;
	}
}


int 
findn1(struct d *dp, int a)
{
	register int i, j;

	for (i = 0; i < NTRAP; i++) {
		if (dp->mlist[i]) {
			if ((j = dp->nlist[i]) < 0 && dp == d)
				j += pl;
			if (j == a)
				break;
		}
	}
	return(i);
}


void
chkpn(void)
{
	pto = *(pnp++);
	pfrom = pto>=0 ? pto : -pto;
	if (pto == -32767) {
		flusho();
		done1(0);
	}
	if (pto < 0) {
		pto = -pto;
		print++;
		pfrom = 0;
	}
}


int 
findt(struct d *dp, int a)
{
	return _findt(dp, a, 1);
}

static int
_findt(struct d *dp, int a, int maydi)
{
	register int i, j, k;

	k = INT_MAX;
	if (dip != d && maydi) {
		if (dip->dimac && (i = dip->ditrap - a) > 0)
			k = i;
	}
	for (i = 0; i < NTRAP; i++) {
		if (dp->mlist[i]) {
			if ((j = dp->nlist[i]) < 0 && dp == d)
				j += pl;
			if ((j -= a) <= 0)
				continue;
			if (j < k)
				k = j;
		}
	}
	if (dp == d) {
		i = pl - a;
		if (k > i)
			k = i;
	}
	return(k);
}


int 
findt1(void)
{
	register int i;

	if (dip != d)
		i = dip->dnl;
	else 
		i = numtab[NL].val;
	return(findt(dip, i));
}


void
eject(struct s *a)
{
	register int savlss;

#ifdef NROFF
	if (ndraw) npic(0);
#endif
	if (dip != d)
		return;
	if (vpt == 0) {
		if (donef == 0) {
			errprint("page not ejected because traps are disabled");
			return;
		}
		errprint("page forcefully ejected although traps are disabled");
		vpt = -1;
	}
	ejf++;
	if (a)
		ejl = a->tail_cnt;
	else 
		ejl = frame->tail_cnt;
	if (trap)
		return;
e1:
	savlss = lss;
	lss = findt(d, numtab[NL].val);
	newline(0);
	lss = savlss;
	if (numtab[NL].val && !trap)
		goto e1;
}


static int
maybreak(tchar c, int dv)
{
	int	i, k = cbits(c);

	if (c & BLBIT)
		return 1;
	if (iszbit(c))
		return 0;
	switch (breakch[0]) {
	case IMP:
		return 0;
	case 0:
		return (!gemu || dv) && (k == '-' || k == EMDASH);
	default:
		for (i = 0; breakch[i] && i < NSENT; i++)
			if (breakch[i] == k)
				return 1;
		return 0;
	}
}

static int
nhychar(tchar c)
{
	int	i, k = cbits(c);

	switch (nhych[0]) {
	case IMP:
		return 0;
	case 0:
		if (hyext)
			return 0;
		return k == '-' || k == EMDASH;
	default:
		for (i = 0; nhych[i] && i < NSENT; i++)
			if (nhych[i] == k)
				return 1;
		return 0;
	}
}

static int
ishyp(tchar *wp)
{
	tchar	*tp;
	int	yes = 0;

	tp = (tchar *)((intptr_t)*hyp & ~(intptr_t)03);
	if (hyoff != 1 && tp == wp && !iszbit(*wp)) {
		if (!wdstart || (wp > wdstart + 1 && wp < wdend &&
		   (!(wdhyf & 04) || wp < wdend - 1) &&		/* 04 => last 2 */
		   (!(wdhyf & 010) || wp > wdstart + 2)) ||	/* 010 => 1st 2 */
		   (wdhyf & 020 && wp == wdend) ||		/* 020 = allow last */
		   (wdhyf & 040 && wp == wdstart + 1))		/* 040 = allow first */
			yes = 1;
		hyp++;
	}
	return yes;
}


int
movword(void)
{
	register int w;
	register tchar i, *wp, c, *lp, *lastlp, lasti = 0;
	int	savwch, hys, stretches = 0, wholewd = 0, mnel, hyphenated = 0;
	int	hc;
#ifndef	NROFF
	tchar	lgs = 0, lge = 0, optlgs = 0, optlge = 0;
	int	*ip, s, lgw = 0, optlgw = 0, lgr = 0, optlgr = 0;
	tchar	*optlinep = NULL, *optwp = NULL;
	int	optnc = 0, optnel = 0, optne = 0, optadspc = 0, optwne = 0,
		optwch = 0, optwholewd = 0,
		optlsplow = 0, optlsphigh = 0, optlspnc = 0, optfldcnt = 0,
		optlshwid = 0, optlshlow = 0, optlshhigh = 0;
#else	/* NROFF */
#define	lgw	0
#define	optlinep	0
#endif	/* NROFF */

	if (pa || padj) {
		parword();
		return(0);
	}
	over = 0;
	wp = wordp;
	if (!nwd) {
		while ((c = cbits(i = *wp++)) == ' ') {
			if (iszbit(i))
				break;
			wch--;
			wne -= xflag ? width(i) : sps;
			if (xflag && linep > line)
				storeline(adjbit(i), 0);
		}
		wp--;
		if (wp > wordp)
			wne -= kernadjust(wp[-1], wp[0]);
		leftend(*wp, admod != 1 && admod != 2, 1);
	}
	if (wdhyf == -1)
		wdhyf = hyf;
	wsp = 0;
	if (wne > nel - adspc && !hyoff && wdhyf && (hlm < 0 || hlc < hlm) &&
	   (!nwd || nel + lsplow + lshlow - adspc >
	    3 * (minsps && ad && !admod ? minsps : sps)) &&
	   (!(wdhyf & 02) || (findt1() > lss)))
		hyphen(wp);
	savwch = wch;
	hyp = hyptr;
	nhyp = 0;
	while (*hyp && *hyp <= wp)
		hyp++;
	while (wch) {
		if (ishyp(wp)) {
			i = IMP;
			setsbits(i, (intptr_t)(hyp[-1]) & 03);
			if (storeline(i, 0))
				nhyp++;
		}
		i = *wp++;
		minflg = minspsz && ad && !admod;
		w = width(i);
		storelsh(i, rawwidth);
		adspc += minspc;
		w += kernadjust(i, *wp);
		wne -= w;
		wch--;
		if (cbits(i) == STRETCH && cbits(lasti) != STRETCH)
			stretches++;
		lasti = i;
		storeline(i, w);
		if (letsps)
			storelsp(i, 0);
	}
	*linep = *wp;
	lastlp = linep;
	mnel = ad && !admod ? (sps - minsps) * nwd : 0;
	if (nel >= 0 || (nel + lsplow + lshlow >= 0 &&
			lspnc - (nwd ? nwd : 1) > 0)) {
		if ((nel >= 0 && nwd && nel - adspc < 0 && nel / nwd < sps) ||
				(nel < 0 && nel + lsplow + lshlow >= 0)) {
			wholewd = 1;
			goto m0;
		}
		nwd += stretches + 1;
		if (nel - adspc < 0 && nwd > 1)
			adflg |= 5;
		w = kernadjust(lasti,  ' ' | (spbits?spbits:sfmask(lasti)));
		ne += w;
		nel -= w;
		return(0);	/* line didn't fill up */
	}
m0:
	hc = shc ? shc : HYPHEN;
#ifndef NROFF
	xbits((tchar)hc, 1);
#endif
	hys = width((tchar)hc);
	if (wholewd)
		goto m1a;
m1:
	if (!nhyp) {
		if (!nwd)
			goto m3;
		if (wch == savwch) {
			if (optlinep)
				goto m2;
			goto m4;
		}
	}
	if ((*--linep & ~SMASK) != IMP)
		goto m5;
#ifndef	NROFF
	i = *(linep + 1);
	if ((s = sbits(*linep)) != 0 &&
			(ip = lgrevtab[fbits(i)][cbits(i)]) != NULL) {
		lgs = strlg(fbits(i), ip, s) | sfmask(i) | AUTOLIG;
		for (w = 0; ip[s+w]; w++);
		lge = strlg(fbits(i), &ip[s], w) | sfmask(i) | AUTOLIG;
		lgw = width(lgs);
		lgr = rawwidth;
		if (linep - 1 >= wordp) {
			lgw += kernadjust(i, *(linep - 1));
			lgw -= kernadjust(*(linep + 1), *(linep - 1));
		}
	} else {
		lgs = 0;
		lge = 0;
		lgw = 0;
	}
#endif	/* !NROFF */
	if (!(--nhyp))
		if (!nwd)
			goto m2;
	if (nel + lsplow + lshlow < hys + lgw) {
		nc--;
		goto m1;
	}
	if (nel >= mnel + hys + lgw)
		goto m2;
	wholewd = 0;
m1a:
#ifndef	NROFF
	if ((minspsz && ad && !admod &&
			wch < savwch && nwd && nel / nwd > 0 && nel < mnel) ||
			(nel + lsplow + lshlow >= (wholewd ? 0 : hys + lgw) &&
			nel < (wholewd ? 0 : hys + lgw))) {
		optlgs = lgs, optlge = lge, optlgw = lgw, optlgr = lgr;
		optlinep = linep, optwp = wp, optnc = nc, optnel = nel,
		optne = ne, optadspc = adspc, optwne = wne, optwch = wch;
		optlsplow = lsplow, optlsphigh = lsphigh, optlspnc = lspnc;
		optfldcnt = fldcnt;
		optlshwid = lshwid, optlshlow = lshlow, optlshhigh = lshhigh;
		optwholewd = wholewd;
		nc -= !wholewd;
		goto m1;
	} else if (wholewd) {
		wholewd = 0;
		goto m1;
	}
#endif
m2:
#ifndef	NROFF
	if (optlinep && 3*abs(optnel - mnel) < 5*abs(nel - mnel)) {
		lgs = optlgs, lge = optlge, lgw = optlgw, lgr = optlgr;
		linep = optlinep, wp = optwp, nc = optnc, nel = optnel,
		ne = optne, adspc = optadspc, wne = optwne, wch = optwch;
		lsplow = optlsplow, lsphigh = optlsphigh, lspnc = optlspnc;
		fldcnt = optfldcnt;
		lshwid = optlshwid, lshlow = optlshlow, lshhigh = optlshhigh;
		if ((wholewd = optwholewd))
			goto m3;
	} else if (optlinep && wch == savwch && !nhyp)
		goto m4;
	if (lgs != 0) {
		*wp = lge;
		storeline(lgs, lgw);
		storelsh(lgs, lgr);
	}
#endif	/* !NROFF */
	if (!maybreak(*(linep - 1), 1)) {
		*linep = sfmask(*(linep - 1)) | hc;
		w = -kernadjust(*(linep - 1), *(linep + 1));
		w += kernadjust(*(linep - 1), *linep);
		w += width(*linep);
		storelsh(*linep, rawwidth);
		w += kernadjust(*linep, ' ' | sfmask(*linep));
		nel -= w;
		ne += w;
		if (letsps)
			storelsp(*linep, 0);
		linep++;
		hyphenated++;
	}
m3:
	nwd++;
m4:
	if (letsps && linep > line)
		storelsp(linep[-1], 1);
	adflg &= ~1;
	adflg |= 4;
	wordp = wp;
	if (hyphenated)
		hlc++;
	else
		hlc = 0;
	return(1);	/* line filled up */
m5:
	nc--;
	minflg = minspsz && ad && !admod;
	w = width(*linep);
	storelsh(*linep, -rawwidth);
	adspc -= minspc;
	for (lp = &linep[1]; lp < lastlp && cbits(*lp) == IMP; lp++);
	w += kernadjust(*linep, *lp ? *lp : ' ' | sfmask(*linep));
	ne -= w;
	nel += w;
	wne += w;
	wch++;
	wp--;
	if (letsps)
		storelsp(*linep, 1);
	goto m1;
}


void
horiz(int i)
{
	vflag = 0;
	if (i)
		pchar(makem(i));
}


void
setnel(void)
{
	if (!nc) {
		linep = line;
		if (un1 >= 0 && (!pgwords || pglines)) {
			un = un1;
			un1 = -1;
		}
		nel = ll - un;
		rhang = ne = adsp = adrem = adspc = 0;
#ifndef	NROFF
		lsplow = lsphigh = lspcur = lsplast = lspnc = fldcnt = 0;
		lshwid = lshhigh = lshlow = lshcur = 0;
#endif	/* !NROFF */
		cht = cdp = 0;
	}
}


int
getword(int x)
{
	register int j, k = 0, w;
	register tchar i = 0, *wp, nexti, gotspc = 0, t;
	int noword, n, inword = 0;
	int lastsp = ' ';
	static int dv;
#if defined (EUC) && defined (NROFF) && defined (ZWDELIMS)
	wchar_t *wddelim;
	char mbbuf3[MB_LEN_MAX + 1];
	char *mbbuf3p;
	int wbf;
	tchar m;
#endif /* EUC && NROFF && ZWDELIMS */

	dv = 0;
	noword = 0;
	if (x)
		if (pendw) {
			*pendw = 0;
			goto rtn;
		}
	if ((wordp = pendw))
		goto g1;
	hyp = hyptr;
	wordp = word;
	over = wne = wch = 0;
	hyoff = 0;
	wdhyf = -1;
	memset(wdpenal, 0, wdsize * sizeof *wdpenal);
	n = 0;
	while (1) {	/* picks up 1st char of word */
		j = cbits(i = GETCH());
#if defined (EUC) && defined (NROFF) && defined (ZWDELIMS)
		if (multi_locale)
			collectmb(i);
#endif /* EUC && NROFF && ZWDELIMS */
		if (j == '\n') {
			wne = wch = 0;
			noword = 1;
			goto rtn;
		}
		if (j == ohc) {
			hyoff = 1;	/* 1 => don't hyphenate */
			continue;
		}
		if ((j == ' ' || (padj && j == STRETCH)) && !iszbit(i)) {
			lastsp = j;
			n++;
			if (isadjspc(i))
				w = 0;
			else if (xflag && seflg && sesspsz == 0) {
				i |= ZBIT;
				w = 0;
			} else if (xflag && seflg && sesspsz && n == 1) {
				if (spbits) {
					i = lastsp | SENTSP | spbits;
					w = width(i);
				} else
					w = ses;
			} else if (spbits && xflag) {
				i = lastsp | spbits;
				w = width(i);
			} else
				w = sps;
			cht = cdp = 0;
			storeword(i, w);
			numtab[HP].val += w;
			if (!isadjspc(j)) {
				widthp = w;
				gotspc = i;
				spbits = sfmask(i);
			}
			continue;
		}
		if (gotspc) {
			k = kernadjust(gotspc, i);
			numtab[HP].val += k;
			wne += k;
			widthp += k;
		}
		break;
	}
	seflg = 0;
#if defined (EUC) && defined (NROFF) && defined (ZWDELIMS)
	if (!multi_locale)
		goto a0;
	if (wddlm && iswprint(wceoll) && iswprint(cwc) &&
	    (!iswascii(wceoll) || !iswascii(cwc)) &&
	    !iswspace(wceoll) && !iswspace(cwc)) {
		wddelim = (*wddlm)(wceoll, cwc, 1);
		wceoll = 0;
		if (*wddelim != ' ') {
			if (!*wddelim) {
				storeword(((*wdbdg)(wceoll, cwc, 1) < 3) ?
					  ZWDELIM(1) : ZWDELIM(2), 0);
			} else {
				while (*wddelim) {
					if ((n = wctomb(mbbuf3, *wddelim++))
					    > 0) {
						m = setuc0(wddelim[-1]);
						storeword(m, -1);
					} else {
						storeword(' ' | chbits, sps);
						break;
					}
				}
			}
			spflg = 0;
			goto g0;
		}
	}
a0:
#endif /* EUC && NROFF && ZWDELIMS */
	if (spbits && xflag) {
		t = lastsp | spbits;
		w = width(t);
	} else {
		t = lastsp | chbits;
		w = sps;
	}
	cdp = cht = 0;
	if (chompend) {
		chompend = 0;
	} else {
		storeword(t, w + k);
	}
	if (spflg) {
		if (xflag == 0 || ses != 0)
			storeword(t | SENTSP, ses);
		spflg = 0;
	}
	if (!nwd)
		wsp = wne;
g0:
	if (ninlev)
		wdhyf = hyf;
	if (j == CONT) {
		pendw = wordp;
		nflush = 0;
		flushi();
		return(1);
	}
	if (hyoff != 1) {
		if (j == ohc) {
			if (!inword && xflag) {
				hyoff = 1;
				goto g1;
			}
			hyoff = 2;
			*hyp++ = wordp;
			if (hyp > (hyptr + NHYP - 1))
				hyp = hyptr + NHYP - 1;
			if (isblbit(i) && wordp > word)
				wordp[-1] |= BLBIT;
			goto g1;
		}
		if (maybreak(j, dv)) {
			if (wordp > word + 1) {
				int i;
				if (!xflag)
					hyoff = 2;
				if (gemu && hyp > hyptr && wordp > word
				    && hyp[-1] == wordp && (
				    (i = cbits(wordp[-1])) == '-'
				    || i == EMDASH))
					hyp--;
				*hyp++ = wordp + 1;
				if (hyp > (hyptr + NHYP - 1))
					hyp = hyptr + NHYP - 1;
			}
		} else {
			int i = cbits(j);
			dv = alph(j) || (i >= '0' && i <= '9');
		}
		if (xflag && nhychar(j))
			hyoff = 2;
	}
	j = width(i);
	numtab[HP].val += j;
	storeword(i, j);
	if (1) {
		int	oev = ev;
		nexti = GETCH();
		if (ev == oev) {
			if (cbits(nexti) == '\n')
				t = ' ' | chbits;
			else
				t = nexti;
			k = kernadjust(i, t);
			wne += k;
			widthp += k;
			numtab[HP].val += k;
		}
	} else
g1:		nexti = GETCH();
	j = cbits(i = nexti);
	if (gemu && (j == FILLER || j == UNPAD))
		inword = 0;
	else
	if (!ismot(i) && j != ohc)
		inword = 1;
#if defined (EUC) && defined (NROFF) && defined (ZWDELIMS)
	if (multi_locale)
		collectmb(i);
#endif /* EUC && NROFF && ZWDELIMS */
	{
		static int sentchar[] =
			{ '.', '?', '!', ':', 0 }; /* sentence terminators */
		int	*sp, *tp;
		static int transchar[] =
			{ '"', '\'', ')', ']', '*', 0, 0 };
		transchar[5] = DAGGER;
		if ((j != '\n' && j != ' ' && (!padj || j != STRETCH)) ||
				ismot(i) || iszbit(i) ||
				isadjspc(i))
#if defined (EUC) && defined (NROFF) && defined (ZWDELIMS)
			if (!multi_locale)
#endif /* EUC && NROFF && ZWDELIMS */
			goto g0;
#if defined (EUC) && defined (NROFF) && defined (ZWDELIMS)
			else {
				if (!wdbdg || (iswascii(cwc) && iswascii(owc)))
					goto g0;
				if ((wbf = (*wdbdg)(owc, cwc, 1)) < 5) {
					storeword((wbf < 3) ? ZWDELIM(1) :
						  ZWDELIM(2), 0);
					*wordp = 0;
					goto rtn;
				} else goto g0;
			}
#endif /* EUC && NROFF && ZWDELIMS */
		if (j == STRETCH && padj)
			storeword(mkxfunc(PENALTY, INFPENALTY), 0);
		wp = wordp-1;	/* handle extra space at end of sentence */
		sp = *sentch ? sentch : sentchar;
		tp = *transch ? transch : transchar;
		while (sp[0] != IMP && wp >= word) {
			j = cbits(*wp--);
			if (istrans(wp[1]))
				goto cont;
			for (k = 0; tp[0] != IMP && tp[k] && k < NSENT; k++)
				if (j == tp[k])
					goto cont;
			for (k = 0; sp[k] && k < NSENT; k++)
				if (j == sp[k]) {
					if (nlflg)
						spflg++;
					else
						seflg++;
					break;
				}
			break;
		cont:;
		}
	}
#if defined (EUC) && defined (NROFF) && defined (ZWDELIMS)
	wceoll = owc;
#endif /* EUC && NROFF && ZWDELIMS */
	*wordp = 0;
	numtab[HP].val += xflag ? width(i) : sps;
rtn:
	if (i & SFMASK)
		spbits = sfmask(i);
	else if (i == '\n')
		spbits = chbits;
	for (wp = word; *wp; wp++) {
		j = cbits(*wp);
		if ((j == ' ' || j == STRETCH) && !iszbit(j) && !isadjspc(j))
			continue;
		if (!ischar(j) || (!isdigit(j) && j != '-'))
			break;
	}
	if (*wp == 0)	/* all numbers, so don't hyphenate */
		hyoff = 1;
	wdstart = 0;
	wordp = word;
	pendw = 0;
	*hyp++ = 0;
	setnel();
	return(noword);
}


void
storeword(register tchar c, register int w)
{

	if (wordp == NULL || wordp >= &word[wdsize - 3]) {
		tchar	*k, **h;
		ptrdiff_t	j;
		int	*pp, owdsize;
		if (over)
			return;
		owdsize = wdsize;
		wdsize += wdsize ? 100 : WDSIZE;
		if ((k = realloc(word, wdsize * sizeof *word)) == NULL ||
				(pp = realloc(wdpenal,
				    wdsize * sizeof *wdpenal)) == NULL) {
			flusho();
			errprint("Word overflow.");
			over++;
			c = LEFTHAND;
			w = -1;
			wdsize = owdsize;
			goto s1;
		}
		j = (char *)k - (char *)word;
		wordp = (tchar *)((char *)wordp + j);
		for (h = hyptr; h < hyp; h++)
			if (*h)
				*h = (tchar *)((char *)*h + j);
		word = k;
		wdpenal = pp;
		memset(&wdpenal[owdsize], 0,
				(wdsize - owdsize) * sizeof *wdpenal);
	}
s1:
	if (isxfunc(c, PENALTY)) {
		wdpenal[max(0, wordp - word - 1)] = sbits(c) | 0x80000000;
		return;
	}
	if (isxfunc(c, DPENAL)) {
		if ((wdpenal[max(0, wordp - word - 1)]&0x80000000) == 0)
			wdpenal[max(0, wordp - word - 1)] = sbits(c);
		return;
	}
	if (w == -1)
		w = width(c);
	widthp = w;
	sethtdp();
	wne += w;
	*wordp++ = c;
	wch++;
	if (dpenal)
		wdpenal[max(0, wordp - word - 1)] = dpenal;
}


#ifdef NROFF
tchar gettch(void)
{
	extern int c_isalnum;
	tchar i;
	int j;

	i = getch();
	j = cbits(i);
	if (ismot(i) || fbits(i) != ulfont)
		return(i);
	if (cu) {
		if (trtab[j] == ' ') {
			setcbits(i, '_');
			setfbits(i, FT);	/* default */
		}
		return(i);
	}
	/* should test here for characters that ought to be underlined */
	/* in the old nroff, that was the 200 bit on the width! */
	/* for now, just do letters, digits and certain special chars */
	if (j <= 127) {
		if (!isalnum(j))
			setfbits(i, FT);
	} else {
		if (j < c_isalnum)
			setfbits(i, FT);
	}
	return(i);
}


#endif
#if defined (EUC) && defined (NROFF) && defined (ZWDELIMS)
int
collectmb(tchar i)
{
	owc = cwc;
	cwc = tr2un(cbits(i), fbits(i));
	return(0);
}


#endif /* EUC && NROFF && ZWDELIMS */

static tchar
adjbit(tchar c)
{
	if (cbits(c) == ' ')
		setcbits(c, WORDSP);
	return(c | ADJBIT);
}

static void
sethtdp(void)
{
	if ((cht = lastrst) > maxcht)
		maxcht = cht;
	if ((cdp = -lastrsb) > maxcdp)
		maxcdp = cdp;
}

static void
leftend(tchar c, int hang, int dolpfx)
{
	int	k, w;

	if (dolpfx && lpfx) {
		if (hang)
			setlhang(lpfx[0]);
		for (k = 0; lpfx[k]; k++) {
			w = width(lpfx[k]);
			w += k ? kernadjust(lpfx[k-1], lpfx[k]) : 0;
			storeline(lpfx[k], w);
		}
		if (k) {
			w = kernadjust(lpfx[k-1], c);
			nel -= w;
			ne += w;
		}
	} else if (hang)
		setlhang(c);
}

#ifndef	NROFF
static void
setlhang(tchar c)
{
	int	k;

	if (lhangtab != NULL && !ismot(c) && cbits(c) != SLANT &&
			cbits(c) != XFUNC &&
			lhangtab[fbits(c)] != NULL &&
			(k = lhangtab[fbits(c)][cbits(c)]) != 0) {
		width(c);	/* set xpts */
		k = (k * u2pts(xpts) + (Unitwidth / 2)) / Unitwidth;
		nel -= k;
		storeline(makem(k), 0);
	}
}

static void
setrhang(void)
{
	int	j, k;
	tchar	c;

	if (nc > 0) {
		c = 0;
		for (j = nc - 1; j >= 0; j--)
			if ((c = line[j]) != IMP)
				break;
		width(c);
		j = lasttrack;
		j += kernadjust(c, ' ' | sfmask(c));
		if (admod != 1 && rhangtab != NULL && !ismot(c) &&
				rhangtab[xfont] != NULL &&
				(k = rhangtab[xfont][cbits(c)]) != 0) {
			rhang = (k * u2pts(xpts) + (Unitwidth / 2)) / Unitwidth;
			j += rhang;
		}
		ne -= j;
		nel += j;
	}
}

static void
letshrink(void)
{
	int	diff, nsp, lshdiff;

	nsp = nwd == 1 ? nwd : nwd - 1;
	diff = nel;
	if (lspnc && lsplow)
		diff += nel * nsp / lspnc;
	if (lshwid) {
		if (lshlow < -diff / 2)
			lshcur = -lshmin;
		else if (lsplow < -diff / 2)
			lshcur = (double)(diff - lsplow) / lshwid * LAFACT;
		else
			lshcur = (double)diff / 2 / lshwid * LAFACT;
		lshdiff = (double)lshcur / LAFACT * lshwid;
		nel -= lshdiff;
		ne += lshdiff;
	} else
		lshdiff = 0;
	diff -= lshdiff;
	if (lsplow)
		lspcur = lspmin * diff / lsplow;
	else
		lspcur = 0;
	ne += lspcomp(lshdiff);
}

static int
letgrow(void)
{
	int	diff, n, nsp, lshdiff;

	nsp = nwd == 1 ? nwd : nwd - 1;
	if ((lspnc - nsp <= 0 || lsphigh <= 0) && lshhigh <= 0)
		return 0;
	n = (letsps - (minsps && ad && !admod ? minsps : sps)) * nsp;
	diff = nel;
	if (lspnc && lsphigh)
		diff += nel * nsp / lspnc - n;
	if (lshwid) {
		if (lshhigh < diff / 2)
			lshcur = lshmax;
		else if (lsphigh < diff / 2)
			lshcur = (double)(diff - lsphigh) / lshwid * LAFACT;
		else
			lshcur = (double)diff / 2 / lshwid * LAFACT;
		lshdiff = (double)lshcur / LAFACT * lshwid;
		nel -= lshdiff;
		ne += lshdiff;
	} else
		lshdiff = 0;
	diff -= lshdiff;
	if (diff > lsphigh)
		diff = lsphigh;
	if (lsphigh)
		lspcur = lspmax * diff / lsphigh;
	else
		lspcur = 0;
	return lspcomp(lshdiff);
}

static int
lspcomp(int idiff)
{
	int	diff = 0, i;
	tchar	c;

	diff = 0;
	for (i = 0; i < nc; i++)
		if (!ismot(c = line[i])) {
			if (isxfunc(c, FLDMARK))
				diff = lsplast = 0;
			else if (cbits(c) > ' ' || isxfunc(c, CHAR)) {
				while (isxfunc(c, CHAR))
					c = charout[sbits(c)].ch;
				lsplast = (int)sbits(c) / 2 * lspcur / LAFACT;
				diff += lsplast;
			}
		}
	diff -= lsplast;
	nel -= diff;
	ne += lsplast;
	return idiff + diff;
}
#endif	/* !NROFF */

/*
 * A dynamic programming approach to line breaking over
 * a paragraph as introduced by D. E. Knuth & M. F. Plass,
 * "Breaking paragraphs into lines", Software - Practice
 * and Experience, Vol. 11, Issue 12 (1981), pp. 1119-1184.
 */

static double
penalty(int k, int s, int h, int h2, int h3)
{
	double	t, d;

	t = nel - k;
	t = t >= 0 ? t * 5 / 3 : -t;
	if (ad && !admod) {
		d = s;
		if (k - s && (letsps || lshmin || lshmax))
			d += (double)(k - s) / 100;
		if (d)
			t /= d;
	} else
		t /= nel / 10;
	if (h && hypp)
		t += hypp;
	if (h2 && hypp2)
		t += hypp2;
	if (h3 && hypp3)
		t += hypp3;
	t = t * t * t;
	if (t > MAXPENALTY)
		t = MAXPENALTY;
	return t;
}

static void
parcomp(int start)
{
	double	*cost, *_cost;
	long double	t;
	int	*prevbreak, *hypc, *_hypc, *brcnt, *_brcnt;
	int	i, j, k, m, h, v, s;

	_cost = malloc((pgsize + 1) * sizeof *_cost);
	cost = &_cost[1];
	_hypc = calloc(pgsize + 1, sizeof *_hypc);
	hypc = &_hypc[1];
	_brcnt = calloc(pgsize + 1, sizeof *_brcnt);
	brcnt = &_brcnt[1];
	prevbreak = calloc(pgsize, sizeof *prevbreak);
	for (i = -1; i < start; i++)
		cost[i] = 0;
	for (i = start; i < pgwords; i++)
		cost[i] = HUGE_VAL;
	for (i = start; i < pgwords; i++) {
		if (pshapes) {
			j = brcnt[i-1];
			if (j < pshapes)
				nel = pgll[j] - pgin[j];
			else
				nel = pgll[pshapes-1] - pgin[pshapes-1];
		} else if (un != in) {
			nel = ll;
			nel -= i > start ? in : un;
		}
		k = pgwordw[i] + pglgsw[i];
		m = pglsphc[i] + pglgsh[i];
		s = 0;
		for (j = i; j < pgwords; j++) {
			if (j > i) {
				k += pgspacw[j] + pgwordw[j];
				m += pgadspc[j] + pglsphc[j];
				s += pgspacw[j];
			}
			v = k + pghyphw[j] + pglgew[j];
			if (v - m - pglgeh[j] <= nel) {
				if (!spread && j == pgwords - 1 &&
						pgpenal[j] == 0)
					t = 0;
				else
					t = penalty(v, s, pghyphw[j],
						pghyphw[j] && hypc[i-1],
						pghyphw[j] && j >= pglastw);
				t += pgpenal[j];
				t += cost[i-1];
				/*fprintf(stderr, "%c%c%c%c to %c%c%c%c "
				                 "t=%g cost[%d]=%g "
						 "brcnt=%d oldbrcnt=%d\n",
						(char)para[pgwordp[i]],
						(char)para[pgwordp[i]+1],
						(char)para[pgwordp[i]+2],
						(char)para[pgwordp[i]+3],
						(char)para[pgwordp[j+1]-4],
						(char)para[pgwordp[j+1]-3],
						(char)para[pgwordp[j+1]-2],
						(char)para[pgwordp[j+1]-1],
						t, j, cost[j],
						1 + brcnt[i-1],
						brcnt[j]
					);*/
				if ((double)t <= cost[j]) {
					if (pghyphw[j])
						h = hypc[i-1] + 1;
					else
						h = 0;
					/*
					 * This is not completely
					 * correct: It might be
					 * preferable to disallow
					 * an earlier hyphenation
					 * point. But it seems
					 * good enough.
					 */
					if (hlm < 0 || h <= hlm) {
						hypc[j] = h;
						cost[j] = t;
						prevbreak[j] = i;
						brcnt[j] = 1 + brcnt[i-1];
					}
				}
			} else {
				if (j == i) {
					t = 1 + cost[i-1];
					cost[j] = t;
					prevbreak[j] = i;
					brcnt[j] = 1 + brcnt[i-1];
				}
				break;
			}
		}
	}
	/*for (i = 0; i < pgwords; i++)
		fprintf(stderr, "cost[%d] = %g %c%c%c%c to %c%c%c%c\n",
				i, cost[i],
				(char)para[pgwordp[prevbreak[i]]],
				(char)para[pgwordp[prevbreak[i]]+1],
				(char)para[pgwordp[prevbreak[i]]+2],
				(char)para[pgwordp[prevbreak[i]]+3],
				(char)para[pgwordp[i]],
				(char)para[pgwordp[i]+1],
				(char)para[pgwordp[i]+2],
				(char)para[pgwordp[i]+3]
			);*/
	pglines = 0;
	memset(&pgopt[pglnout], 0, (pgsize - pglnout) * sizeof *pgopt);
	i = j = pgwords - 1;
	do {
		pglines++;
		j = prevbreak[j];
		pgopt[i--] = j--;
	} while (j >= start && i >= pglnout);
	memmove(&pgopt[pglnout+1], &pgopt[i+2], pglines * sizeof *pgopt);
	pgopt[pglnout] = start;
	free(_cost);
	free(_hypc);
	free(_brcnt);
	free(prevbreak);
}

void
growpgsize(void)
{
	pgsize += 20;
	pgwordp = realloc(pgwordp, pgsize * sizeof *pgwordp);
	pgwordw = realloc(pgwordw, pgsize * sizeof *pgwordw);
	pghyphw = realloc(pghyphw, pgsize * sizeof *pghyphw);
	pgadspc = realloc(pgadspc, pgsize * sizeof *pgadspc);
	pglsphc = realloc(pglsphc, pgsize * sizeof *pglsphc);
	pgopt = realloc(pgopt, pgsize * sizeof *pgopt);
	pgspacp = realloc(pgspacp, pgsize * sizeof *pgspacp);
	pgspacw = realloc(pgspacw, pgsize * sizeof *pgspacw);
	pglgsc = realloc(pglgsc, pgsize * sizeof *pglgsc);
	pglgec = realloc(pglgec, pgsize * sizeof *pglgec);
	pglgsw = realloc(pglgsw, pgsize * sizeof *pglgsw);
	pglgew = realloc(pglgew, pgsize * sizeof *pglgew);
	pglgsh = realloc(pglgsh, pgsize * sizeof *pglgsh);
	pglgeh = realloc(pglgeh, pgsize * sizeof *pglgeh);
	pgin = realloc(pgin, pgsize * sizeof *pgin);
	pgll = realloc(pgll, pgsize * sizeof *pgll);
	pgwdin = realloc(pgwdin, pgsize * sizeof *pgwdin);
	pgwdll = realloc(pgwdll, pgsize * sizeof *pgwdll);
	pgflags = realloc(pgflags, pgsize * sizeof *pgflags);
	pglno = realloc(pglno, pgsize * sizeof *pglno);
	pgpenal = realloc(pgpenal, pgsize * sizeof *pgpenal);
	if (pgwordp == NULL || pgwordw == NULL || pghyphw == NULL ||
			pgopt == NULL || pgspacw == NULL ||
			pgadspc == NULL || pglsphc == NULL ||
			pglgsc == NULL || pglgec == NULL ||
			pglgsw == NULL || pglgew == NULL ||
			pglgsh == NULL || pglgeh == NULL ||
			pgin == NULL || pgll == NULL ||
			pgwdin == NULL || pgwdll == NULL ||
			pgflags == NULL || pglno == NULL ||
			pgpenal == NULL || pgwdin == NULL || pgwdin == NULL) {
		errprint("out of memory justifying paragraphs");
		done(02);
	}
}

static void
parlgzero(int i)
{
	pglgsc[i] = 0;
	pglgec[i] = 0;
	pglgsw[i] = 0;
	pglgew[i] = 0;
	pglgsh[i] = 0;
	pglgeh[i] = 0;
}

static float
makepgpenal(int p)
{
	p &= ~0x80000000;
	p -= INFPENALTY0 + 1;
	if (p >= INFPENALTY0)
		return INFPENALTY;
	else if (p <= -INFPENALTY0)
		return -INFPENALTY;
	else
		return p * PENALSCALE;
}

static void
parword(void)
{
	int	a, c, w, hc;
	tchar	i, *wp;

	if (pgwords + 1 >= pgsize)
		growpgsize();
	hc = shc ? shc : HYPHEN;
	pglastw = pgwords;
	wp = wordp;
	a = w = 0;
	pglno[pgwords] = numtab[CD].val;
	pgspacp[pgwords] = pgspacs;
	pgpenal[pgwords] = 0;
	pgwdin[pgwords] = in;
	pgwdll[pgwords] = ll;
	if (pgwords == 0)
		pgflags[pgwords] = 0;
	un1 = -1;
	while ((c = cbits(i = *wp++)) == ' ' || c == STRETCH) {
		if (iszbit(i))
			break;
		wch--;
		minflg = minspsz && ad && !admod;
		w += width(i);
		a += minspc;
		w += kernadjust(wp[-1], wp[0]);
		if (pgspacs >= pgssize) {
			pgssize += 60;
			parsp = realloc(parsp, pgssize * sizeof *parsp);
			if (parsp == NULL) {
				errprint("no memory for spaces in paragraph");
				done(02);
			}
		}
		parsp[pgspacs++] = i;
		if (c == STRETCH)
			pgpenal[pgwords] = INFPENALTY;
		else if (wdpenal[wp-word-1])
			pgpenal[pgwords] = makepgpenal(wdpenal[wp-word-1]);
	}
	if (wch == 0)
		return;
	pgspacp[pgwords+1] = pgspacs;
	if (--wp > wordp && pgchars > 0)
		w += kernadjust(para[pgchars-1], wordp[0]);
	wne -= w;
	pgspacw[pgwords] = pgwords ? w + a : 0;
	pghyphw[pgwords] = 0;
	pgadspc[pgwords] = pgwords ? a : 0;
	pglsphc[pgwords] = 0;
	pgwordw[pgwords] = 0;
	pgwordp[pgwords] = pgchars;
	pgne += pgspacw[pgwords];
	parlgzero(pgwords);
	parlgzero(pgwords+1);
	if (wdhyf == -1)
		wdhyf = hyf;
	if (!hyoff && wdhyf && hlm)
		hyphen(wp);
	hyp = hyptr;
	nhyp = 0;
	while (*hyp && *hyp <= wp)
		hyp++;
	while (wch) {
		if (ishyp(wp) && !maybreak(wp[-1], 1)) {
			i = sfmask(wp[-1]) | hc;
			w = width(i);
			w += kernadjust(wp[-1], i);
			pghyphw[pgwords] = w;
#ifndef	NROFF
			{
			int		*ip;
			intptr_t	n;
			tchar		e, s;

			n = (intptr_t)hyp[-1] & 03;
			ip = n ? lgrevtab[fbits(*wp)][cbits(*wp)] : NULL;
			if (n != 0 && ip != NULL) {
				pglgec[pgwords] = e =
					strlg(fbits(*wp), ip, n) |
						sfmask(*wp) | AUTOLIG;
				for (w = 0; ip[n+w]; w++);
				pglgsc[pgwords+1] = s =
					strlg(fbits(*wp), &ip[n], w) |
						sfmask(*wp) | AUTOLIG;
				pglgew[pgwords] = width(e);
				pglgeh[pgwords] = getlsh(e, rawwidth) *
					lshmin / LAFACT;
				pglgew[pgwords] += kernadjust(wp[-1], e);
				pghyphw[pgwords] += kernadjust(e, i);
				pghyphw[pgwords] -= kernadjust(wp[-1], i);
				pglgsw[pgwords+1] = width(s);
				pglgsh[pgwords+1] = getlsh(s, rawwidth) *
					lshmin / LAFACT;
				pglgsw[pgwords+1] -= width(*wp);
				pglgsh[pgwords+1] -= getlsh(*wp, rawwidth) *
					lshmin / LAFACT;
				pglgsw[pgwords+1] += kernadjust(s, wp[1]);
				pglgsw[pgwords+1] -= kernadjust(wp[0], wp[1]);
			} }
#endif	/* !NROFF */
		}
		if (pghyphw[pgwords] || (wp > word && maybreak(wp[-1], 1))) {
			if (pghyphw[pgwords])
				pghyphw[pgwords] -= kernadjust(wp[-1], wp[0]);
			pgne += pgwordw[pgwords];
			pgwordp[++pgwords] = pgchars;
			if (pgwords + 1 >= pgsize)
				growpgsize();
			pglno[pgwords] = numtab[CD].val;
			pgspacp[pgwords] = pgspacs;
			pgspacp[pgwords+1] = pgspacs;
			pgspacw[pgwords] = 0;
			pgwordw[pgwords] = 0;
			pghyphw[pgwords] = 0;
			pgadspc[pgwords] = 0;
			pglsphc[pgwords] = 0;
			pgpenal[pgwords] = 0;
			pgwdin[pgwords] = in;
			pgwdll[pgwords] = ll;
			pgflags[pgwords] = 0;
			parlgzero(pgwords+1);
		}
		i = *wp++;
		w = width(i);
		pglsphc[pgwords] += getlsh(i, rawwidth) * lshmin / LAFACT;
		w += kernadjust(i, *wp);
		wne -= w;
		wch--;
		pgwordw[pgwords] += w;
		if (letsps)
			pglsphc[pgwords] += getlsp(i) * lspmin / LAFACT;
		if (pgchars + 1 >= pgcsize) {
			pgcsize += 600;
			para = realloc(para, pgcsize * sizeof *para);
			if (para == NULL) {
				errprint("no memory for characters "
				         "in paragraph");
				done(02);
			}
		}
		para[pgchars++] = i;
		if (wdpenal[wp-word-1])
			pgpenal[pgwords] = makepgpenal(wdpenal[wp-word-1]);
	}
	pgne += pgwordw[pgwords];
	pgwordp[++pgwords] = pgchars;
	pgspacw[pgwords] = 0;
	pgwordw[pgwords] = 0;
	pghyphw[pgwords] = 0;
	pgadspc[pgwords] = 0;
	pglsphc[pgwords] = 0;
	pgpenal[pgwords] = 0;
	pgwdin[pgwords] = in;
	pgwdll[pgwords] = ll;
	pgflags[pgwords] = 0;
	parlgzero(pgwords);
	if (spread)
		tbreak();
}

static void
pbreak(int sprd, int lastf, struct s *s)
{
	int	j;

	if (sprd)
		adflg |= 5;
	if (pshapes) {
		j = pglnout < pshapes ? pglnout : pshapes - 1;
		un = pgin[j];
	}
	nlflg = 1;
	tbreak();
	pglnout++;
	if (trap) {
		extern jmp_buf	sjbuf;
		jmp_buf	savsjbuf;
		if (setjmp(*s->jmp) == 0) {
			nlflg = 1;
			memcpy(&savsjbuf, &sjbuf, sizeof sjbuf);
			if (donep && lastf)
				donef = -1;
			mainloop();
		}
		memcpy(&sjbuf, &savsjbuf, sizeof sjbuf);
	}
}

void
parpr(struct s *s)
{
	int	i, j, k = 0, nw = 0, w, stretches, _spread = spread, hc;
	int	savll, savin, savcd, lastin, lastll, curin, curll;
	tchar	c, e, lastc, lgs;

	savll = ll;
	savin = in;
	curin = 0;
	lastin = 0;
	curll = -1;
	lastll = 0;
	savcd = numtab[CD].val;
	hc = shc ? shc : HYPHEN;
	nw = 0;
	for (i = 0; i < pgwords; i++) {
		lgs = 0;
		numtab[CD].val = pglno[i];
		if (i == 0 || pgflags[i] & PG_NEWIN)
			lastin = pgwdin[i];
		if (i == 0 || pgflags[i] & PG_NEWLL)
			lastll = pgwdll[i];
		if (k == 0 || pgopt[k] == i) {
			if (k++ > 0) {
				if (pghyphw[i-1]) {
#ifndef	NROFF
					if ((e = pglgec[i-1]) != 0) {
						w = width(e);
						storelsh(e, rawwidth);
						w += kernadjust(para[pgwordp[i]-1], e);
						storeline(e, w);
						if (letsps)
							storelsp(e, 0);
						lgs = pglgsc[i];
					} else
#endif
						e = para[pgwordp[i]-1];
					c = sfmask(e) | hc;
					w = width(c);
					storelsh(c, rawwidth);
					w += kernadjust(e, c);
					storeline(c, w);
					if (letsps)
						storelsp(c, 0);
				}
				pbreak(1, i >= pgwords, s);
				if (i >= pgwords)
					break;
			}
			if (pshapes) {
				if (k == 1)
					parcomp(0);
				j = k-1 < pshapes ? k-1 : pshapes - 1;
				ll = pgll[j];
				un = pgin[j];
				nel = ll - un;
			} else if (k > 1 && (in != curin || ll != curll)) {
				savin = curin = lastin = in;
				savll = curll = lastll = ll;
				un = in;
				nel = ll - un;
				parcomp(i);
			} else if (lastin != curin || lastll != curll) {
				savin = in = curin = lastin;
				savll = ll = curll = lastll;
				if (k > 1)
					un = in;
				nel = ll - un;
				parcomp(i);
			}
			nel = ll - un;
			nw = nwd = 1;
			leftend(para[pgwordp[i]], admod != 1 && admod != 2, 1);
		} else {
			for (j = pgspacp[i]; j < pgspacp[i+1]; j++) {
				c = parsp[j];
				minflg = minspsz && ad && !admod;
				w = width(c);
				adspc += minspc;
				if (j == pgspacp[i] && i > 0)
					w += kernadjust(para[pgwordp[i]-1], c);
				if (j == pgspacp[i+1]-1)
					w += kernadjust(c, para[pgwordp[i]]);
				storeline(c, w);
				spbits = sfmask(c);
			}
			nwd += pgspacp[i] != pgspacp[i+1];
		}
		stretches = 0;
		lastc = 0;
		for (j = pgwordp[i]; j < pgwordp[i+1]; j++) {
			c = lgs ? lgs : para[j];
			lgs = 0;
			w = width(c);
			storelsh(c, rawwidth);
			if (j == pgwordp[i] && i > 0 && nw > 1 &&
					pgspacp[i] == pgspacp[i+1])
				w += kernadjust(para[j-1], c);
			if (j < pgwordp[i+1]-1)
				w += kernadjust(c, para[j+1]);
			storeline(c, w);
			if (cbits(c) == STRETCH && cbits(lastc) != STRETCH)
				stretches++;
			lastc = c;
			if (letsps)
				storelsp(c, 0);
		}
		nwd += stretches;
		nw++;
	}
	pbreak((nel - adspc < 0 && nwd > 1) || _spread, 1, s);
	if (pgflags[pgwords] & PG_NEWIN)
		savin = pgwdin[pgwords];
	if (pgflags[pgwords] & PG_NEWLL)
		savll = pgwdll[pgwords];
	pgwords = pgchars = pgspacs = pglines = pgne = pglastw = 0;
	ll = savll;
	in = un = savin;
	numtab[CD].val = savcd;
}

static void
parfmt(void)
{
	int	_nlflg = nlflg;
	int	_spread = spread;
	struct s	*s;

	if (pgchars == 0)
		return;
	setnel();
	pglnout = 0;
	s = frame;
	nxf->jmp = malloc(sizeof *nxf->jmp);
	pushi(-2, 0, FLAG_PARAGRAPH);
	parpr(frame);
	while (frame != s)
		ch = popi();
	nlflg = _nlflg;
	if (_spread == 1 && pshapes > pglnout) {
		memmove(&pgin[0], &pgin[pglnout],
			(pshapes - pglnout) * sizeof *pgin);
		memmove(&pgll[0], &pgll[pglnout],
			(pshapes - pglnout) * sizeof *pgll);
		pshapes -= pglnout;
	} else
		pshapes = 0;
}
