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


/*	from OpenSolaris "n2.c	1.9	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)n2.c	1.47 (gritter) 5/25/08
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

/*
 * n2.c
 *
 * output, cleanup
 */

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#ifdef EUC
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#endif /* EUC */
#include "tdef.h"
#ifdef NROFF
#include "tw.h"
#endif
#include "pt.h"
#include "ext.h"

extern	jmp_buf	sjbuf;
int	toolate;
int	error;

static void	outtp(tchar);

int
pchar(register tchar i)
{
	register int j;
	static int hx = 0;	/* records if have seen HX */
	static int xon = 0;	/* records if have seen XON */
	static int drawfcn = 0;	/* records if have seen DRAWFCN */

	if (hx) {
		hx = 0;
		j = absmot(i);
		if (isnmot(i)) {
			if (j > dip->blss)
				dip->blss = j;
		} else {
			if (j > dip->alss)
				dip->alss = j;
			ralss = dip->alss;
		}
		return 1;
	}
	if (ismot(i)) {
		pchar1(i); 
		return 1;
	}
	switch (j = cbits(i)) {
	case 0:
	case IMP:
	case RIGHT:
	case LEFT:
		if (xflag) {
			i = j = FILLER;	/* avoid kerning in output routine */
			goto dfl;
		}
		return 1;
	case HX:
		hx = 1;
		if (xflag) {
			i = j = FILLER;	/* avoid kerning in output routine */
			goto dfl;
		}
		return 1;
	case XON:
		xon = 1;
		goto dfl;
	case XOFF:
		xon = 0;
		goto dfl;
	case DRAWFCN:
		drawfcn = !drawfcn;
		goto dfl;
	case PRESC:
		if (dip == &d[0])
			j = eschar;	/* fall through */
	default:
	dfl:
#ifndef NROFF
		if (html) {
			if (!xflag || !isdi(i)) {
				setcbits(i, j >= NCHARS ? j :
				    tflg ? trnttab[j] : trtab[j]);
				if (xon == 0 && drawfcn == 0 && i < NCHARS)
					setcbits(i, ftrans(fbits(i),
					    cbits(i)));
			}
		} else
#endif
		if (!xflag || !isdi(i)) {
			setcbits(i, tflg ? trnttab[j] : trtab[j]);
			if (xon == 0 && drawfcn == 0)
				setcbits(i, ftrans(fbits(i), cbits(i)));
		}
	}
#ifdef	NROFF
	if (xon && xflag)
		return 1;
#endif	/* NROFF */
	pchar1(i);
	return 1;
}


void
pchar1(register tchar i)
{
	static int	_olt;
	tchar	_olp[1];
	register int j;
	filep	savip;
	extern void ptout(tchar);

	j = cbits(i);
	if (dip != &d[0]) {
		if (i == FLSS)
			dip->flss++;
		else if (dip->flss > 0)
			dip->flss--;
		else if (!ismot(i) && (cbits(i) > 32 || cbits(i) == XFUNC) &&
				!tflg)
			i |= DIBIT;
		wbf(i);
		dip->op = offset;
		return;
	}
	if (!tflg && !print) {
		if (j == '\n')
			dip->alss = dip->blss = 0;
		return;
	}
	if (no_out)
		return;
	if (tflg) {	/* transparent mode, undiverted */
		outtp(i);
		return;
	}
	if (cbits(i) == XFUNC) {
		switch (fbits(i)) {
		case OLT:
			olt = realloc(olt, (nolt + 1) * sizeof *olt);
			_olt = 1;
			return;
		case CHAR:
#ifndef	NROFF
			if (!ascii)
				break;
#endif	/* !NROFF */
			savip = ip;
			ip = charout[sbits(i)].op;
			app++;
			fmtchar++;
			while ((i = rbf()) != 0 && cbits(i) != '\n' &&
					cbits(i) != FLSS)
				pchar(i);
			fmtchar--;
			app--;
			ip = savip;
			return;
		}
	}
	if (cbits(i) == 'x')
		fmtchar = fmtchar;
	if (_olt) {
		_olp[0] = i;
		olt[nolt++] = fetchrq(_olp);
		_olt = 0;
	}
#ifndef NROFF
	if (ascii)
		outascii(i);
	else
#endif
		ptout(i);
}

static void
outtp(tchar i)
{
#ifndef NROFF
	int	j = cbits(i);

#ifdef	EUC
	if (iscopy(i))
		fdprintf(ptid, "%lc", j);
	else
#endif	/* EUC */
		fdprintf(ptid, "%c", j);
#endif
}

#ifndef	NROFF
static void
outmb(tchar i)
{
	extern int nchtab;
	int j = cbits(i);
#ifdef	EUC
	wchar_t	wc;
	char	mb[MB_LEN_MAX+1];
	int	n;
	int	f;
#endif	/* EUC */

	if (j < 0177) {
		oput(j);
		return;
	}
#ifdef	EUC
	if (iscopy(i))
		wc = cbits(i);
	else {
		if ((f = fbits(i)) == 0)
			f = font;
		wc = tr2un(j, f);
	}
	if (wc != -1 && (n = wctomb(mb, wc)) > 0) {
		mb[n] = 0;
		oputs(mb);
	} else
#endif	/* EUC */
	if (j < 128 + nchtab) {
		oput('\\');
		oput('(');
		oput(chname[chtab[j-128]]);
		oput(chname[chtab[j-128]+1]);
	}
}

void
outascii (	/* print i in best-guess ascii */
    tchar i
)
{
	int j = cbits(i);
	int f = fbits(i);
	int k;

	if (j == FILLER)
		return;
	if (isadjspc(i))
		return;
	if (ismot(i)) {
		oput(' ');
		return;
	}
	if ((j < 0177 && j >= ' ') || j == '\n') {
		oput(j);
		return;
	}
	if (f == 0)
		f = xfont;
	if (j == DRAWFCN)
		oputs("\\D");
	else if (j == HYPHEN || j == MINUS)
		oput('-');
	else if (j == XON)
		oputs("\\X");
	else if (islig(i) && lgrevtab && lgrevtab[f] && lgrevtab[f][j]) {
		for (k = 0; lgrevtab[f][j][k]; k++)
			outmb(sfmask(i) | lgrevtab[f][j][k]);
	} else if (j == WORDSP)
		;	/* nothing at all */
	else if (j > 0177)
		outmb(i);
}
#endif


/*
 * now a macro
oput(i)
	register int	i;
{
	*obufp++ = i;
	if (obufp >= &obuf[OBUFSZ])
		flusho();
}
*/

void
oputs(register char *i)
{
	while (*i != 0)
		oput(*i++&0377);
}


void
flusho(void)
{
	if (obufp == obuf)
		return;
	if (no_out == 0) {
		if (!toolate) {
			toolate++;
#ifdef NROFF
			set_tty();
			{
				char	*p = t.twinit;
				while (*p++)
					;
				if (p - t.twinit > 1)
					write(ptid, t.twinit, p - t.twinit - 1);
			}
#endif
		}
		toolate += write(ptid, obuf, obufp - obuf);
	}
	obufp = obuf;
}

void
caseoutput(void)
{
	tchar	i;

	copyf++;
	if (!skip(0)) {
		if (cbits(i = getch()) == '"')
			i = getch();
		while (i != 0) {
			outtp(i);
			if (cbits(i) == '\n')
				break;
			i = getch();
		}
	}
	copyf--;
}


void
done(int x)
{
	register int i;

	error |= x;
	dl = app = ds = lgf = 0;
	if (pgchars && !pglines) {
		donep = 1;
		tbreak();
		donep = 0;
	}
	if ((i = em)) {
		donef = -1;
		em = 0;
		if (control(i, 0))
			longjmp(sjbuf, 1);
	}
	if (!nfo)
		done3(0);
	mflg = 0;
	dip = &d[0];
	if (woff)
		wbt((tchar)0);
	if (pendw)
		getword(1);
	pendnf = 0;
	if (donef == 1)
		done1(0);
	donef = 1;
	ip = 0;
	frame = stk;
	nxf = calloc(1, sizeof *nxf);
	if (!ejf)
		tbreak();
	nflush++;
	eject((struct s *)0);
	longjmp(sjbuf, 1);
}


void
done1(int x) 
{
	error |= x;
	if (numtab[NL].val) {
		trap = 0;
		eject((struct s *)0);
		longjmp(sjbuf, 1);
	}
	if (nofeed) {
		ptlead();
		flusho();
		done3(0);
	} else {
		pttrailer();
		done2(0);
	}
}


void
done2(int x) 
{
	ptlead();
#ifndef NROFF
	if (!ascii)
		ptstop();
#endif
	flusho();
	done3(x);
}

void
done3(int x)
{
	error |= x;
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
#ifdef NROFF
	twdone();
#endif
	if (ascii)
		mesg(1);
	exit(error);
}


void
edone(int x)
{
	frame = stk;
	nxf = calloc(1, sizeof *nxf);
	ip = 0;
	done(x);
}



void
casepi(void)
{
	register pid_t i;
	int	id[2];

	if (skip(1))
		return;
	if (toolate || !getname() || pipe(id) == -1 || (i = fork()) == -1) {
		errprint("Pipe not created.");
		return;
	}
	ptid = id[1];
	if (i > 0) {
		close(id[0]);
		toolate++;
		pipeflg = i;
		return;
	}
	close(0);
	dup(id[0]);
	close(id[1]);
	execl(nextf, nextf, NULL);
	errprint("Cannot exec %s", nextf);
	exit(-4);
}
