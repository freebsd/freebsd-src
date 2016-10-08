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


/*	from OpenSolaris "n5.c	1.10	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)n5.c	1.130 (gritter) 10/23/09
 */

/*
 * Changes Copyright (c) 2014, 2015 Carsten Kunze <carsten.kunze at arcor.de>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#if defined (EUC)
#include <stddef.h>
#include <wchar.h>
#endif	/* EUC */
#include <string.h>
#include <unistd.h>
#include "tdef.h"
#include "ext.h"
#ifdef	NROFF
#include "tw.h"
#endif
#include "pt.h"

/*
 * troff5.c
 * 
 * misc processing requests
 */

void
casead(void)
{
	register int i;

	ad = 1;
	/*leave admod alone*/
	if (skip(0))
		return;
	pa = 0;
loop:
	switch (i = cbits(getch())) {
	case 'r':	/*right adj, left ragged*/
		admod = 2;
		break;
	case 'l':	/*left adj, right ragged*/
		admod = ad = 0;	/*same as casena*/
		break;
	case 'c':	/*centered adj*/
		admod = 1;
		break;
	case 'b': 
	case 'n':
		admod = 0;
		break;
	case '0': 
	case '2': 
	case '4':
		ad = 0;
	case '1': 
	case '3': 
	case '5':
		admod = (i - '0') / 2;
		break;
	case 'p':
	case '7':
		if (xflag) {
			pa = 1;
			admod = 0;
			goto loop;
		}
	}
}


void
casena(void)
{
	ad = 0;
}


void
casefi(void)
{
	tbreak();
	fi++;
	pendnf = 0;
}


void
casenf(void)
{
	tbreak();
	fi = 0;
}


void
casepadj(void)
{
	int	n;

	if (skip(0))
		padj = 1;
	else {
		n = hatoi();
		if (!nonumb)
			padj = n;
	}
}


void
casers(void)
{
	dip->nls = 0;
}


void
casens(void)
{
	dip->nls++;
}


void
casespreadwarn(void)
{
	if (skip(0))
		spreadwarn = !spreadwarn;
	else {
		dfact = EM;
		spreadlimit = inumb(&spreadlimit);
		spreadwarn = 1;
	}
}


int 
chget(int c)
{
	tchar i = 0;

	charf++;
	if (skip(0) || ismot(i = getch()) || cbits(i) == ' ' || cbits(i) == '\n') {
		ch = i;
		return(c);
	} else 
		return(cbits(i));
}


void
casecc(void)
{
	cc = chget('.');
}


void
casec2(void)
{
	c2 = chget('\'');
}


void
casehc(void)
{
	ohc = chget(OHC);
}


void
casetc(void)
{
	tabc = chget(0);
}


void
caselc(void)
{
	dotc = chget(0);
}


void
casehy(void)
{
	register int i;

	hyf = 1;
	if (skip(0))
		return;
	noscale++;
	i = hatoi();
	noscale = 0;
	if (nonumb)
		return;
	hyf = max(i, 0);
}


void
casenh(void)
{
	hyf = 0;
}


void
casehlm(void)
{
	int	i;

	if (!skip(0)) {
		noscale++;
		i = hatoi();
		noscale = 0;
		if (!nonumb)
			hlm = i;
	} else
		hlm = -1;
}

void
casehcode(void)
{
	tchar	c, d;
	int	k;

	lgf++;
	if (skip(1))
		return;
	do {
		c = getch();
		if (skip(1))
			break;
		d = getch();
		if (c && d && !ismot(c) && !ismot(d)) {
			if ((k = cbits(c)) >= nhcode) {
				hcode = realloc(hcode, (k+1) * sizeof *hcode);
				memset(&hcode[nhcode], 0,
					(k+1-nhcode) * sizeof *hcode);
				nhcode = k+1;
			}
			hcode[k] = cbits(d);
		}
	} while (!skip(0));
}

void
caseshc(void)
{
	shc = skip(0) ? 0 : getch();
}

void
casehylen(void)
{
	int	n;

	if (skip(0))
		hylen = 5;
	else {
		n = hatoi();
		if (!nonumb)
			hylen = n;
	}
}

void
casehypp(void)
{
	float	t;

	if (skip(0))
		hypp = hypp2 = hypp3 = 0;
	else {
		t = atop();
		if (!nonumb)
			hypp = t;
		if (skip(0))
			hypp2 = hypp3 = 0;
		else {
			t = atop();
			if (!nonumb)
				hypp2 = t;
			if (skip(0))
				hypp3 = 0;
			else {
				t = atop();
				if (!nonumb)
					hypp3 = t;
			}
		}
	}
}

static void
chkin(int indent, int linelength, const char *note)
{
	if (indent > linelength - INCH / 10) {
		if (warn & WARN_RANGE)
			errprint("excess of %sindent", note);
	}
}

void
casepshape(void)
{
	int	i, l;
	int	lastin = in, lastll = ll;

	pshapes = 0;
	if (skip(0)) {
		pshapes = 0;
		return;
	}
	do {
		i = max(hnumb(&lastin), 0);
		if (nonumb)
			break;
		if (skip(0))
			l = ll;
		else {
			l = max(hnumb(&lastll), INCH / 10);
			if (nonumb)
				break;
		}
		if (pshapes >= pgsize)
			growpgsize();
		chkin(i, l, "");
		pgin[pshapes] = i;
		pgll[pshapes] = l;
		pshapes++;
		lastin = i;
		lastll = l;
	} while (!skip(0));
}

void
caselpfx(void)
{
	int	n;
	tchar	c;

	if (skip(0)) {
		free(lpfx);
		lpfx = NULL;
		nlpfx = 0;
	} else {
		for (n = 0; ; n++) {
			if (n+1 >= nlpfx) {
				nlpfx += 10;
				lpfx = realloc(lpfx, nlpfx * sizeof *lpfx);
			}
			c = getch();
			if (nlflg)
				break;
			if (n == 0 && cbits(c) == '"')
				continue;
			lpfx[n] = c;
		}
		lpfx[n] = 0;
	}
}

int 
max(int aa, int bb)
{
	if (aa > bb)
		return(aa);
	else 
		return(bb);
}

int 
min(int aa, int bb)
{
	if (aa < bb)
		return(aa);
	else 
		return(bb);
}


static void
cerj(int dorj)
{
	register int i;

	noscale++;
	skip(0);
	i = max(hatoi(), 0);
	if (nonumb)
		i = 1;
	tbreak();
	if (dorj) {
		rj = i;
		ce = 0;
	} else {
		ce = i;
		rj = 0;
	}
	noscale = 0;
}


void
casece(void)
{
	cerj(0);
}


void
caserj(void)
{
	if (xflag)
		cerj(1);
}


static void
_brnl(int p)
{
	int	n;

	noscale++;
	if (skip(0))
		n = INT_MAX;
	else {
		n = hatoi();
		if (nonumb || n < 0)
			n = p ? brpnl : brpnl;
	}
	noscale--;
	tbreak();
	if (p) {
		brpnl = n;
		brnl = 0;
	} else {
		brnl = n;
		brpnl = 0;
	}
}


void
casebrnl(void)
{
	_brnl(0);
}


void
casebrpnl(void)
{
	_brnl(1);
}


void
casein(void)
{
	register int i;

	if ((pa || padj) && pglines == 0 && pgchars)
		tbreak();
	if (skip(0))
		i = in1;
	else 
		i = max(hnumb(&in), 0);
	tbreak();
	in1 = in;
	in = i;
	chkin(in, ll, "");
	if (!nc && !pgwords) {
		un = in;
		setnel();
	} else if (pgwords) {
		pgflags[pgwords] |= PG_NEWIN;
		pgwdin[pgwords] = in;
	}
}


void
casell(void)
{
	register int i;

	if (skip(0))
		i = ll1;
	else 
		i = max(hnumb(&ll), INCH / 10);
	ll1 = ll;
	ll = i;
	chkin(in, ll, "");
	setnel();
	if (pgwords) {
		pgflags[pgwords] |= PG_NEWLL;
		pgwdll[pgwords] = ll;
	}
}


void
caselt(void)
{
	register int i;

	if (skip(0))
		i = lt1;
	else 
		i = max(hnumb(&lt), 0);
	lt1 = lt;
	lt = i;
}


void
caseti(void)
{
	register int i;

	if (skip(1))
		return;
	if ((pa || padj) && pglines == 0 && pgchars)
		tbreak();
	i = max(hnumb(&in), 0);
	tbreak();
	un1 = i;
	chkin(i, ll, "temporary ");
	setnel();
}


void
casels(void)
{
	register int i;

	noscale++;
	if (skip(0))
		i = ls1;
	else 
		i = max(inumb(&ls), 1);
	ls1 = ls;
	ls = i;
	noscale = 0;
}


void
casepo(void)
{
	register int i;

	if (skip(0))
		i = po1;
	else 
		i = max(hnumb(&po), 0);
	po1 = po;
	po = i;
#ifndef NROFF
	if (!ascii)
		esc += po - po1;
#endif
}


void
casepl(void)
{
	register int i;

	skip(0);
	if ((i = vnumb(&pl)) == 0)
		pl = defaultpl ? defaultpl : 11 * INCH; /*11in*/
	else 
		pl = i;
	if (numtab[NL].val > pl) {
		numtab[NL].val = pl;
		prwatchn(&numtab[NL]);
	}
}


static void
chkt(struct d *dp, int n)
{
	if (n <= 0 && dp != d)
		if (warn & WARN_RANGE)
			errprint("trap at %d not effective in diversion", n);
}


static void
_casewh(struct d *dp)
{
	register int i, j, k;

	lgf++;
	skip(1);
	i = vnumb((int *)0);
	if (nonumb)
		return;
	skip(0);
	j = getrq(1);
	if ((k = findn(dp, i)) != NTRAP) {
		dp->mlist[k] = j;
		return;
	}
	for (k = 0; k < NTRAP; k++)
		if (dp->mlist[k] == 0)
			break;
	if (k == NTRAP) {
		flusho();
		errprint("cannot plant trap.");
		return;
	}
	dp->mlist[k] = j;
	dp->nlist[k] = i;
	chkt(dp, i);
}


void
casewh(void)
{
	_casewh(d);
}


void
casedwh(void)
{
	_casewh(dip);
}


static void
_casech(struct d *dp)
{
	register int i, j, k;

	lgf++;
	skip(1);
	if (!(j = getrq(0)))
		return;
	else  {
		for (k = 0; k < NTRAP; k++)
			if (dp->mlist[k] == j)
				break;
	}
	if (k == NTRAP)
		return;
	skip(0);
	i = vnumb((int *)0);
	if (nonumb)
		dp->mlist[k] = 0;
	dp->nlist[k] = i;
	chkt(dp, i);
}


void
casech(void)
{
	_casech(d);
}


void
casedch(void)
{
	_casech(dip);
}


void
casevpt(void)
{
	if (skip(1))
		return;
	vpt = hatoi() != 0;
}


tchar
setolt(void)
{
	storerq(getsn(1));
	return mkxfunc(OLT, 0);
}


int 
findn(struct d *dp, int i)
{
	register int k;

	for (k = 0; k < NTRAP; k++)
		if ((dp->nlist[k] == i) && (dp->mlist[k] != 0))
			break;
	return(k);
}


void
casepn(void)
{
	register int i;

	skip(1);
	noscale++;
	i = max(inumb(&numtab[PN].val), 0);
	prwatchn(&numtab[PN]);
	noscale = 0;
	if (!nonumb) {
		npn = i;
		npnflg++;
	}
}


void
casebp(void)
{
	register int i;
	register struct s *savframe;

	if (dip != d)
		return;
	savframe = frame;
	if (skip(0))
		i = -1;
	else {
		if ((i = inumb(&numtab[PN].val)) < 0)
			i = 0;
		if (nonumb)
			i = -1;
	}
	tbreak();
	if (i >= 0) {
		npn = i;
		npnflg++;
	} else if (dip->nls && donef < 1)
		return;
	eject(savframe);
}


static void
tmtmcwr(int ab, int tmc, int wr, int ep, int tmm)
{
	const char tmtab[] = {
		'a',000,000,000,000,000,000,000,
		000,000,000,000,000,000,000,000,
		'{','}','&',000,'%','c','e',' ',
		'!',000,000,000,000,000,000,'~',
		000
	};
	struct contab	*cp;
	register int i, j;
	tchar	c;
	char	tmbuf[NTM];
	filep	savip = ip;
	int	discard = 0;

	lgf++;
	if (tmm) {
		if (skip(1) || (i = getrq(0)) == 0)
			return;
		if ((cp = findmn(i)) == NULL || !cp->mx) {
			nosuch(i);
			return;
		}
		savip = ip;
		ip = (filep)cp->mx;
		app++;
		copyf++;
	} else {
		copyf++;
		if (skip(0) && ab)
			errprint("User Abort");
	}
loop:	for (i = 0; i < NTM - 5 - mb_cur_max; ) {
		if (tmm) {
			if ((c = rbf()) == 0) {
				ip = savip;
				tmm = 0;
				app--;
				break;
			}
		} else
			c = getch();
		if (discard) {
			discard--;
			continue;
		}
		if (c == '\n') {
			tmbuf[i++] = '\n';
			break;
		}
	c:	j = cbits(c);
		if (iscopy(c)) {
			int	n;
			if ((n = wctomb(&tmbuf[i], j)) > 0) {
				i += n;
				continue;
			}
		}
		if (xflag == 0) {
			tmbuf[i++] = c;
			continue;
		}
		if (ismot(c))
			continue;
		tmbuf[i++] = '\\';
		if (c == (OHC|BLBIT))
			j = ':';
		else if (istrans(c))
			j = ')';
		else if (j >= 0 && j < sizeof tmtab && tmtab[j])
			j = tmtab[j];
		else if (j == ACUTE)
			j = '\'';
		else if (j == GRAVE)
			j = '`';
		else if (j == UNDERLINE)
			j = '_';
		else if (j == MINUS)
			j = '-';
		else {
			i--;
			if (c == WORDSP)
				j = ' ';
			else if (j == WORDSP)
				continue;
			else if (j == FLSS) {
				discard++;
				continue;
			}
		}
		if (j == XFUNC)
			switch (fbits(c)) {
			case CHAR:
				c = charout[sbits(c)].ch;
				goto c;
			default:
				continue;
			}
		tmbuf[i++] = j;
	}
	if (i == NTM - 2)
		tmbuf[i++] = '\n';
	if (tmc)
		i--;
	tmbuf[i] = 0;
	if (ab)	/* truncate output */
		obufp = obuf;	/* should be a function in n2.c */
	if (ep) {
		flusho();
		errprint("%s", tmbuf);
	} else if (wr < 0) {
		flusho();
		fprintf(stderr, "%s", tmbuf);
	} else if (i)
		write(wr, tmbuf, i);
	if (tmm)
		goto loop;
	copyf--;
	lgf--;
}

void
casetm(int ab)
{
	tmtmcwr(ab, 0, -1, 0, 0);
}

void
casetmc(void)
{
	tmtmcwr(0, 1, -1, 0, 0);
}

void
caseerrprint(void)
{
	tmtmcwr(0, 1, -1, 1, 0);
}

static struct stream {
	char	*name;
	int	fd;
} *streams;
static int	nstreams;

static void
open1(int flags)
{
	int	ns = nstreams;

	lgf++;
	if (skip(1) || !getname() || skip(1))
		return;
	streams = realloc(streams, sizeof *streams * ++nstreams);
	streams[ns].name = malloc(NS);
	n_strcpy(streams[ns].name, nextf, NS);
	getname();
	if ((streams[ns].fd = open(nextf, flags, 0666)) < 0) {
		errprint("can't open file %s", nextf);
		done(02);
	}
}

void
caseopen(void)
{
	open1(O_WRONLY|O_CREAT|O_TRUNC);
}

void
caseopena(void)
{
	open1(O_WRONLY|O_CREAT|O_APPEND);
}

static int
getstream(const char *name)
{
	int	i;

	for (i = 0; i < nstreams; i++)
		if (strcmp(streams[i].name, name) == 0)
			return i;
	errprint("no such stream %s", name);
	return -1;
}

static void
write1(int writec, int writem)
{
	int	i;

	lgf++;
	if (skip(1) || !getname())
		return;
	if ((i = getstream(nextf)) < 0)
		return;
	tmtmcwr(0, writec, streams[i].fd, 0, writem);
}

void
casewrite(void)
{
	write1(0, 0);
}

void
casewritec(void)
{
	write1(1, 0);
}

void
casewritem(void)
{
	write1(0, 1);
}

void
caseclose(void)
{
	int	i;

	lgf++;
	if (skip(1) || !getname())
		return;
	if ((i = getstream(nextf)) < 0)
		return;
	free(streams[i].name);
	memmove(&streams[i], &streams[i+1], (nstreams-i-1) * sizeof *streams);
	nstreams--;
}


void
casesp(int a)
{
	register int i, j, savlss;

	tbreak();
	if (dip->nls || trap)
		return;
	i = findt1();
	if (!a) {
		skip(0);
		j = vnumb((int *)0);
		if (nonumb)
			j = lss;
	} else 
		j = a;
	if (j == 0)
		return;
	if (i < j)
		j = i;
	savlss = lss;
	if (dip != d)
		i = dip->dnl; 
	else 
		i = numtab[NL].val;
	if ((i + j) < 0)
		j = -i;
	lss = j;
	newline(0);
	lss = savlss;
}


void
casebrp(void)
{
	if (nc || pgchars) {
		spread = 2;
		flushi();
		if (pgchars)
			tbreak();
		else {
			pendt++;
			text();
		}
	} else
		tbreak();
}


void
caseblm(void)
{
	if (!skip(0))
		blmac = getrq(1);
	else
		blmac = 0;
}

void
caselsm(void)
{
	if (!skip(0))
		lsmac = getrq(1);
	else
		lsmac = 0;
}

void
casert(void)
{
	register int a, *p;

	skip(0);
	if (dip != d)
		p = &dip->dnl; 
	else 
		p = &numtab[NL].val;
	a = vnumb(p);
	if (nonumb)
		a = dip->mkline;
	if ((a < 0) || (a >= *p))
		return;
	nb++;
	casesp(a - *p);
}


void
caseem(void)
{
	lgf++;
	skip(1);
	em = getrq(1);
}


void
casefl(void)
{
	tbreak();
	flusho();
}


static struct evnames {
	int	number;
	char	*name;
} *evnames;
static struct env	*evp;
static int	*evlist;
static int	evi;
static int	evlsz;
static int	Nev = NEV;

static struct env *
findev(int *number, char *name)
{
	int	i;

	if (*number < 0)
		return &evp[-1 - (*number)];
	else if (name) {
		for (i = 0; i < Nev-NEV; i++)
			if (evnames[i].name != NULL &&
					strcmp(evnames[i].name, name) == 0) {
				*number = -1 - i;
				return &evp[i];
			}
		*number = -1 - i;
		return NULL;
	} else if (*number >= NEV) {
		for (i = 0; i < Nev-NEV; i++)
			if (evnames[i].name == NULL &&
					evnames[i].number == *number)
				return &evp[i];
		*number = -1 - i;
		return NULL;
	} else {
		extern tchar *corebuf;
		return &((struct env *)corebuf)[*number];
	}
}

static int
getev(int *nxevp, char **namep)
{
	char	*name = NULL;
	int nxev = 0;
	int	c;
	int	i = 0, sz = 0, valid = 1;

	*namep = NULL;
	*nxevp = 0;
	if (skip(0))
		return 0;
	c = cbits(ch);
	if (xflag == 0 || isdigit(c) || c == '(') {
		noscale++;
		nxev = hatoi();
		noscale = 0;
		if (nonumb) {
			flushi();
			return 0;
		}
	} else {
		do {
			c = rgetach();
			if (i >= sz)
				name = realloc(name, (sz += 8) * sizeof *name);
			name[i++] = c;
		} while (c);
		if (*name == 0) {
			free(name);
			name = NULL;
			valid = 0;
		}
	}
	flushi();
	*namep = name;
	*nxevp = nxev;
	return valid;
}

void
caseev(void)
{
	char	*name;
	int nxev;
	struct env	*np, *op;

	if (getev(&nxev, &name) == 0) {
		if (evi == 0)
			return;
		nxev =  evlist[--evi];
		goto e1;
	}
	if (xflag == 0 && ((nxev >= NEV) || (nxev < 0) || (evi >= EVLSZ)))
		goto cannot;
	if (evi >= evlsz) {
		evlsz = evi + 1;
		if ((evlist = realloc(evlist, evlsz * sizeof *evlist)) == NULL)
			goto cannot;
	}
	if ((name && findev(&nxev, name) == NULL) || nxev >= Nev) {
		if ((evp = realloc(evp, (Nev-NEV+1) * sizeof *evp)) == NULL ||
				(evnames = realloc(evnames,
				   (Nev-NEV+1) * sizeof *evnames)) == NULL)
			goto cannot;
		evnames[Nev-NEV].number = nxev;
		evnames[Nev-NEV].name = name;
		evp[Nev-NEV] = initenv;
		Nev++;
	}
	if (name == NULL && nxev < 0) {
		flusho();
	cannot:	errprint("cannot do ev.");
		if (error)
			done2(040);
		else 
			edone(040);
		return;
	}
	evlist[evi++] = ev;
e1:
	if (ev == nxev)
		return;
	if ((np = findev(&nxev, name)) == NULL ||
			(op = findev(&ev, NULL)) == NULL)
		goto cannot;
	*op = env;
	env = *np;
	ev = nxev;
	if (evname == NULL) {
		if (name)
			evname = name;
		else {
			size_t l = 20;
			evname = malloc(l);
			roff_sprintf(evname, l, "%d", ev);
		}
	}
}

void
caseevc(void)
{
	char	*name;
	int	nxev;
	struct env	*ep;

	if (getev(&nxev, &name) == 0 || (ep = findev(&nxev, name)) == NULL)
		return;
	relsev(&env);
	evc(&env, ep);
}

void
evc(struct env *dp, struct env *sp)
{
	if (dp != sp) {
		char *name;
		name = dp->_evname;
		memcpy(dp, sp, sizeof *dp);
		dp->_evname = name;
	}
	if (sp->_hcode) {
		dp->_hcode = malloc(dp->_nhcode * sizeof *dp->_hcode);
		memcpy(dp->_hcode, sp->_hcode, dp->_nhcode *
		    sizeof *dp->_hcode);
	}
	if (sp->_lpfx) {
		dp->_lpfx = malloc(dp->_nlpfx * sizeof *dp->_lpfx);
		memcpy(dp->_lpfx, sp->_lpfx, dp->_nlpfx * sizeof *dp->_lpfx);
	}
	dp->_pendnf = 0;
	dp->_pendw = 0;
	dp->_pendt = 0;
	dp->_wch = 0;
	dp->_wne = 0;
	dp->_wsp = 0;
	dp->_wdstart = 0;
	dp->_wdend = 0;
	dp->_lnsize = 0;
	dp->_line = NULL;
	dp->_linep = NULL;
	dp->_wdsize = 0;
	dp->_word = 0;
	dp->_wdpenal = 0;
	dp->_wordp = 0;
	dp->_spflg = 0;
	dp->_seflg = 0;
	dp->_ce = 0;
	dp->_rj = 0;
	dp->_pgsize = 0;
	dp->_pgcsize = 0;
	dp->_pgssize = 0;
	dp->_pglines = 0;
	dp->_pgwords = 0;
	dp->_pgchars = 0;
	dp->_pgspacs = 0;
	dp->_para = NULL;
	dp->_parsp = NULL;
	dp->_pgwordp = NULL;
	dp->_pgspacp = NULL;
	dp->_pgwordw = NULL;
	dp->_pghyphw = NULL;
	dp->_pgadspc = NULL;
	dp->_pglsphc = NULL;
	dp->_pgopt = NULL;
	dp->_pgspacw = NULL;
	dp->_pglgsc = NULL;
	dp->_pglgec = NULL;
	dp->_pglgsw = NULL;
	dp->_pglgew = NULL;
	dp->_pglgsh = NULL;
	dp->_pglgeh = NULL;
	dp->_pgin = NULL;
	dp->_pgll = NULL;
	dp->_pgwdin = NULL;
	dp->_pgwdll = NULL;
	dp->_pgflags = NULL;
	dp->_pglno = NULL;
	dp->_pgpenal = NULL;
	dp->_inlevp = NULL;
	if (dp->_brnl < INT_MAX)
		dp->_brnl = 0;
	if (dp->_brpnl < INT_MAX)
		dp->_brpnl = 0;
	dp->_nn = 0;
	dp->_ndf = 0;
	dp->_nms = 0;
	dp->_ni = 0;
	dp->_ul = 0;
	dp->_cu = 0;
	dp->_it = 0;
	dp->_itc = 0;
	dp->_itmac = 0;
	dp->_nc = 0;
	dp->_un = 0;
	dp->_un1 = -1;
	dp->_nwd = 0;
	dp->_hyoff = 0;
	dp->_nb = 0;
	dp->_spread = 0;
	dp->_lnmod = 0;
	dp->_hlc = 0;
	dp->_cht = 0;
	dp->_cdp = 0;
	dp->_maxcht = 0;
	dp->_maxcdp = 0;
	setnel();
}

void
evcline(struct env *dp, struct env *sp)
{
	if (dp == sp)
		return;
#ifndef	NROFF
	dp->_lspnc = sp->_lspnc;
	dp->_lsplow = sp->_lsplow;
	dp->_lsphigh = sp->_lsphigh;
	dp->_lspcur = sp->_lspcur;
	dp->_lsplast = sp->_lsplast;
	dp->_lshwid = sp->_lshwid;
	dp->_lshlow = sp->_lshlow;
	dp->_lshhigh = sp->_lshhigh;
	dp->_lshcur = sp->_lshcur;
#endif
	dp->_fldcnt = sp->_fldcnt;
	dp->_hyoff = sp->_hyoff;
	dp->_hlc = sp->_hlc;
	dp->_nel = sp->_nel;
	dp->_adflg = sp->_adflg;
	dp->_adspc = sp->_adspc;
	dp->_wne = sp->_wne;
	dp->_wsp = sp->_wsp;
	dp->_ne = sp->_ne;
	dp->_nc = sp->_nc;
	dp->_nwd = sp->_nwd;
	dp->_un = sp->_un;
	dp->_wch = sp->_wch;
	dp->_rhang = sp->_rhang;
	dp->_cht = sp->_cht;
	dp->_cdp = sp->_cdp;
	dp->_maxcht = sp->_maxcht;
	dp->_maxcdp = sp->_maxcdp;
	if (icf == 0)
		dp->_ic = sp->_ic;
	memcpy(dp->_hyptr, sp->_hyptr, NHYP * sizeof *sp->_hyptr);
	dp->_line = malloc((dp->_lnsize = sp->_lnsize) * sizeof *dp->_line);
	memcpy(dp->_line, sp->_line, sp->_lnsize * sizeof *sp->_line);
	dp->_word = malloc((dp->_wdsize = sp->_wdsize) * sizeof *dp->_word);
	memcpy(dp->_word, sp->_word, sp->_wdsize * sizeof *sp->_word);
	dp->_wdpenal = malloc((dp->_wdsize = sp->_wdsize) *
			sizeof *dp->_wdpenal);
	memcpy(dp->_wdpenal, sp->_wdpenal, sp->_wdsize * sizeof *sp->_wdpenal);
	dp->_linep = sp->_linep + (dp->_line - sp->_line);
	dp->_wordp = sp->_wordp + (dp->_word - sp->_word);
	dp->_wdend = sp->_wdend + (dp->_word - sp->_word);
	dp->_wdstart = sp->_wdstart + (dp->_word - sp->_word);
	dp->_para = malloc((dp->_pgcsize = sp->_pgcsize) * sizeof *dp->_para);
	memcpy(dp->_para, sp->_para, dp->_pgcsize * sizeof *sp->_para);
	dp->_parsp = malloc((dp->_pgssize = sp->_pgssize) * sizeof *dp->_parsp);
	memcpy(dp->_parsp, sp->_parsp, dp->_pgssize * sizeof *sp->_parsp);
	dp->_pgsize = sp->_pgsize;
	dp->_pgwordp = malloc(dp->_pgsize * sizeof *dp->_pgwordp);
	memcpy(dp->_pgwordp, sp->_pgwordp, dp->_pgsize * sizeof *dp->_pgwordp);
	dp->_pgwordw = malloc(dp->_pgsize * sizeof *dp->_pgwordw);
	memcpy(dp->_pgwordw, sp->_pgwordw, dp->_pgsize * sizeof *dp->_pgwordw);
	dp->_pghyphw = malloc(dp->_pgsize * sizeof *dp->_pghyphw);
	memcpy(dp->_pghyphw, sp->_pghyphw, dp->_pgsize * sizeof *dp->_pghyphw);
	dp->_pgadspc = malloc(dp->_pgsize * sizeof *dp->_pgadspc);
	memcpy(dp->_pgadspc, sp->_pgadspc, dp->_pgsize * sizeof *dp->_pgadspc);
	dp->_pglsphc = malloc(dp->_pgsize * sizeof *dp->_pglsphc);
	memcpy(dp->_pglsphc, sp->_pglsphc, dp->_pgsize * sizeof *dp->_pglsphc);
	dp->_pgopt = malloc(dp->_pgsize * sizeof *dp->_pgopt);
	memcpy(dp->_pgopt, sp->_pgopt, dp->_pgsize * sizeof *dp->_pgopt);
	dp->_pgspacw = malloc(dp->_pgsize * sizeof *dp->_pgspacw);
	memcpy(dp->_pgspacw, sp->_pgspacw, dp->_pgsize * sizeof *dp->_pgspacw);
	dp->_pgspacp = malloc(dp->_pgsize * sizeof *dp->_pgspacp);
	memcpy(dp->_pgspacp, sp->_pgspacp, dp->_pgsize * sizeof *dp->_pgspacp);
	dp->_pglgsc = malloc(dp->_pgsize * sizeof *dp->_pglgsc);
	memcpy(dp->_pglgsc, sp->_pglgsc, dp->_pgsize * sizeof *dp->_pglgsc);
	dp->_pglgec = malloc(dp->_pgsize * sizeof *dp->_pglgec);
	memcpy(dp->_pglgec, sp->_pglgec, dp->_pgsize * sizeof *dp->_pglgec);
	dp->_pglgsw = malloc(dp->_pgsize * sizeof *dp->_pglgsw);
	memcpy(dp->_pglgsw, sp->_pglgsw, dp->_pgsize * sizeof *dp->_pglgsw);
	dp->_pglgew = malloc(dp->_pgsize * sizeof *dp->_pglgew);
	memcpy(dp->_pglgew, sp->_pglgew, dp->_pgsize * sizeof *dp->_pglgew);
	dp->_pglgsh = malloc(dp->_pgsize * sizeof *dp->_pglgsh);
	memcpy(dp->_pglgsh, sp->_pglgsh, dp->_pgsize * sizeof *dp->_pglgsh);
	dp->_pglgeh = malloc(dp->_pgsize * sizeof *dp->_pglgeh);
	memcpy(dp->_pglgeh, sp->_pglgeh, dp->_pgsize * sizeof *dp->_pglgeh);
	dp->_pgin = malloc(dp->_pgsize * sizeof *dp->_pgin);
	memcpy(dp->_pgin, sp->_pgin, dp->_pgsize * sizeof *dp->_pgin);
	dp->_pgll = malloc(dp->_pgsize * sizeof *dp->_pgll);
	memcpy(dp->_pgll, sp->_pgll, dp->_pgsize * sizeof *dp->_pgll);
	dp->_pgwdin = malloc(dp->_pgsize * sizeof *dp->_pgwdin);
	memcpy(dp->_pgwdin, sp->_pgwdin, dp->_pgsize * sizeof *dp->_pgwdin);
	dp->_pgwdll = malloc(dp->_pgsize * sizeof *dp->_pgwdll);
	memcpy(dp->_pgwdll, sp->_pgwdll, dp->_pgsize * sizeof *dp->_pgwdll);
	dp->_pgflags = malloc(dp->_pgsize * sizeof *dp->_pgflags);
	memcpy(dp->_pgflags, sp->_pgflags, dp->_pgsize * sizeof *dp->_pgflags);
	dp->_pglno = malloc(dp->_pgsize * sizeof *dp->_pglno);
	memcpy(dp->_pglno, sp->_pglno, dp->_pgsize * sizeof *dp->_pglno);
	dp->_pgpenal = malloc(dp->_pgsize * sizeof *dp->_pgpenal);
	memcpy(dp->_pgpenal, sp->_pgpenal, dp->_pgsize * sizeof *dp->_pgpenal);
	dp->_inlevp = malloc(dp->_ainlev * sizeof *dp->_inlevp);
	memcpy(dp->_inlevp, sp->_inlevp, dp->_ninlev * sizeof *dp->_inlevp);
	dp->_pgwords = sp->_pgwords;
	dp->_pgchars = sp->_pgchars;
	dp->_pgspacs = sp->_pgspacs;
	dp->_pglines = sp->_pglines;
}

void
relsev(struct env *ep)
{
	free(ep->_hcode);
	ep->_hcode = NULL;
	ep->_nhcode = 0;
	free(ep->_line);
	ep->_line = NULL;
	ep->_lnsize = 0;
	free(ep->_word);
	ep->_word = NULL;
	free(ep->_wdpenal);
	ep->_wdpenal = NULL;
	ep->_wdsize = 0;
	free(ep->_para);
	ep->_para = NULL;
	ep->_pgcsize = 0;
	free(ep->_pgwordp);
	ep->_pgwordp = NULL;
	free(ep->_pgwordw);
	ep->_pgwordw = NULL;
	free(ep->_pghyphw);
	ep->_pghyphw = NULL;
	free(ep->_pgadspc);
	ep->_pgadspc = NULL;
	free(ep->_pglsphc);
	ep->_pglsphc = NULL;
	free(ep->_pgopt);
	ep->_pgopt = NULL;
	free(ep->_pgspacw);
	ep->_pgspacw = NULL;
	free(ep->_pgspacp);
	ep->_pgspacp = NULL;
	free(ep->_pglgsc);
	ep->_pglgsc = NULL;
	free(ep->_pglgec);
	ep->_pglgec = NULL;
	free(ep->_pglgsw);
	ep->_pglgsw = NULL;
	free(ep->_pglgew);
	ep->_pglgew = NULL;
	free(ep->_pglgsh);
	ep->_pglgsh = NULL;
	free(ep->_pglgeh);
	ep->_pglgeh = NULL;
	free(ep->_pgin);
	ep->_pgin = NULL;
	free(ep->_pgll);
	ep->_pgll = NULL;
	free(ep->_pgwdin);
	ep->_pgwdin = NULL;
	free(ep->_pgwdll);
	ep->_pgwdll = NULL;
	free(ep->_pgflags);
	ep->_pgflags = NULL;
	free(ep->_pglno);
	ep->_pglno = NULL;
	free(ep->_pgpenal);
	ep->_pgpenal = NULL;
	ep->_pgsize = 0;
	free(ep->_inlevp);
	ep->_inlevp = NULL;
	ep->_ninlev = 0;
	ep->_ainlev = 0;
}

void
caseel(void)
{
	caseif(2);
}

void
caseie(void)
{
	caseif(1);
}

int	tryglf;

void
caseif(int x)
{
	extern int falsef;
	register int notflag, true;
	tchar i, j;
	enum warn w = warn;
	int	flt = 0;
	static int el;

	if (x == 3)
		goto i2;
	if (x == 2) {
		notflag = 0;
		true = el;
		el = 0;
		goto i1;
	}
	true = 0;
	skip(1);
	if ((cbits(i = getch())) == '!') {
		notflag = 1;
		if (xflag == 0)
			/*EMPTY*/;
		else if ((cbits(i = getch())) == 'f')
			flt = 1;
		else
			ch = i;
	} else if (xflag && cbits(i) == 'f') {
		flt = 1;
		notflag = 0;
	} else {
		notflag = 0;
		ch = i;
	}
	if (flt)
		i = atof0() > 0;
	else
		i = (int)atoi0();
	if (!nonumb) {
		if (i > 0)
			true++;
		goto i1;
	}
	i = getch();
	switch (cbits(i)) {
	case 'e':
		if (!(numtab[PN].val & 01))
			true++;
		break;
	case 'o':
		if (numtab[PN].val & 01)
			true++;
		break;
#ifdef NROFF
	case 'n':
		true++;
	case 't':
#endif
#ifndef NROFF
	case 't':
		true++;
	case 'n':
#endif
		break;
	case 'c':
		if (xflag == 0)
			goto dfl;
		warn &= ~WARN_CHAR;
		tryglf++;
		if (!skip(1)) {
			j = getch();
			true = !ismot(j) && cbits(j) && cbits(j) != ' ';
		}
		tryglf--;
		warn = w;
		break;
	case 'r':
	case 'd':
		if (xflag == 0)
			goto dfl;
		warn &= ~(WARN_MAC|WARN_SPACE|WARN_REG);
		if (!skip(1)) {
			j = getrq(2);
			true = (cbits(i) == 'r' ?
					usedr(j) != NULL : findmn(j) != NULL);
		}
		warn = w;
		break;
	case 'F':
		if (xflag == 0)
			goto dfl;
		if (!skip(1)) {
			j = getrq(3);
			true = findft(j, 0) != -1;
		}
		break;
	case 'v':
		/* break; */
	case ' ':
		break;
	default:
	dfl:	true = cmpstr(i);
	}
i1:
	true ^= notflag;
	if (x == 1) {
		el = !true;
	}
	if (true) {
		if (frame->loopf & LOOP_EVAL) {
			if (nonumb)
				goto i3;
			frame->loopf &= ~LOOP_EVAL;
			frame->loopf |= LOOP_NEXT;
		}
i2:
		noschr = 0;
		bol = 1;
		while ((cbits(i = getch())) == ' ')
			;
		bol = 0;
		if (cbits(i) == LEFT)
			goto i2;
		ch = i;
		nflush++;
	} else {
i3:
		if (frame->loopf & LOOP_EVAL)
			frame->loopf = LOOP_FREE;
		copyf++;
		falsef++;
		eatblk(0);
		copyf--;
		falsef--;
	}
}

void
casenop(void)
{
	caseif(3);
}

void
casechomp(void) {
	chomp = 1;
	caseif(3);
}

void
casereturn(void)
{
	flushi();
	nflush++;
	while (frame->loopf) {
		frame->loopf = LOOP_FREE;
		popi();
	}
	popi();
}

void
casewhile(void)
{
	tchar	c;
	int	k, level;
	filep	newip;

	if (dip != d)
		wbfl();
	if ((nextb = alloc()) == 0) {
		errprint("out of space");
		edone(04);
		return;
	}
	newip = offset = nextb;
	wbf(mkxfunc(CC, 0));
	wbf(XFUNC);	/* caseif */
	wbf(' ');
	copyf++, clonef++;
	level = 0;
	do {
		nlflg = 0;
		k = cbits(c = getch());
		switch (k) {
		case LEFT:
			level++;
			break;
		case RIGHT:
			level--;
			break;
		}
		wbf(c);
	} while (!nlflg || level > 0);
	if (level < 0 && warn & WARN_DELIM)
		errprint("%d excess delimiter(s)", -level);
	wbt(0);
	copyf--, clonef--;
	pushi(newip, LOOP, 0);
	offset = dip->op;
}

void
casebreak(void)
{
	casecontinue(1);
}

void
casecontinue(int _break)
{
	int	i, j;
	struct s	*s;

	noscale++;
	if (skip(0) || (i = hatoi()) <= 0 || nonumb)
		i = 1;
	noscale--;
	j = 0;
	for (s = frame; s != stk; s = s->pframe)
		if (s->loopf && ++j >= i)
			break;
	if (j != i) {
		if (i == 1) {
			if (warn & WARN_RANGE)
				errprint("%s outside loop", macname(lastrq));
			return;
		}
		if (warn & WARN_RANGE)
			errprint("%s: breaking out of %d current loop "
					"levels but %d requested",
				macname(lastrq), j, i);
		_break = 1;
		i = j;
	}
	flushi();
	nflush++;
	while (i > 1 || (_break && i > 0)) {
		if (frame->loopf) {
			frame->loopf = LOOP_FREE;
			i--;
		}
		popi();
	}
	if (i == 1) {
		while (frame->loopf == 0)
			popi();
		popi();
	}
}

void
eatblk(int inblk)
{	register int cnt, i;
	tchar	ii;

	cnt = 0;
	do {
		if (ch)	{
			i = cbits(ii = ch);
			ch = 0;
		} else
			i = cbits(ii = getch0());
		if (i == ESC)
			cnt++;
		else {
			if (cnt == 1)
				switch (i) {
				case '{':  i = LEFT; break;
				case '}':  i = RIGHT; break;
				case '\n': i = 'x'; break;
				}
			cnt = 0;
		}
		if (i == LEFT) eatblk(1);
	} while ((!inblk && (i != '\n')) || (inblk && (i != RIGHT)));
	if (i == '\n') {
		nlflg++;
		tailflg = istail(ii);
	}
}


int 
cmpstr(tchar c)
{
	register int j, delim;
	register tchar i;
	register int val;
	int savapts, savapts1, savfont, savfont1, savpts, savpts1;
	tchar string[1280];
	register tchar *sp;

	if (ismot(c))
		return(0);
	argdelim = delim = cbits(c);
	savapts = apts;
	savapts1 = apts1;
	savfont = font;
	savfont1 = font1;
	savpts = pts;
	savpts1 = pts1;
	sp = string;
	while ((j = cbits(i = getch()))!=delim && j!='\n' && sp<&string[1280-1])
		*sp++ = i;
	if (j != delim)
		nodelim(delim);
	if (sp >= string + 1280) {
		errprint("too-long string compare.");
		edone(0100);
	}
	if (nlflg) {
		val = sp==string;
		goto rtn;
	}
	*sp++ = 0;
	apts = savapts;
	apts1 = savapts1;
	font = savfont;
	font1 = savfont1;
	pts = savpts;
	pts1 = savpts1;
	mchbits();
	val = 1;
	sp = string;
	while ((j = cbits(i = getch())) != delim && j != '\n') {
		if (*sp != i) {
			eat(delim);
			val = 0;
			goto rtn;
		}
		sp++;
	}
	if (j != delim)
		nodelim(delim);
	if (*sp)
		val = 0;
rtn:
	apts = savapts;
	apts1 = savapts1;
	font = savfont;
	font1 = savfont1;
	pts = savpts;
	pts1 = savpts1;
	mchbits();
	argdelim = 0;
	return(val);
}


void
caserd(void)
{

	lgf++;
	skip(0);
	getname();
	if (!iflg) {
		if (quiet) {
#ifdef	NROFF
			echo_off();
			flusho();
#endif	/* NROFF */
			fprintf(stderr, "\007"); /*bell*/
		} else {
			if (nextf[0]) {
				fprintf(stderr, "%s:", nextf);
			} else {
				fprintf(stderr, "\007"); /*bell*/
			}
		}
	}
	collect();
	tty++;
	pushi(-1, PAIR('r','d'), 0);
}


int 
rdtty(void)
{
	char	onechar;
#if defined (EUC)
	int	i, n;

loop:
#endif /* EUC */

	onechar = 0;
	if (read(0, &onechar, 1) == 1) {
		if (onechar == '\n')
			tty++;
		else 
			tty = 1;
#if !defined (EUC)
		if (tty != 3)
			return(onechar);
#else	/* EUC */
		if (tty != 3) {
			if (!multi_locale)
				return(onechar);
			i = onechar & 0377;
			*mbbuf1p++ = i;
			*mbbuf1p = 0;
			if ((*mbbuf1&~(wchar_t)0177) == 0) {
				twc = 0;
				mbbuf1p = mbbuf1;
			}
			else if ((n = mbtowc(&twc, mbbuf1, mb_cur_max)) <= 0) {
				if (mbbuf1p >= mbbuf1 + mb_cur_max) {
					illseq(-1, mbbuf1, mbbuf1p-mbbuf1);
					twc = 0;
					mbbuf1p = mbbuf1;
					*mbbuf1p = 0;
					i &= 0177;
				} else {
					goto loop;
				}
			} else {
				i = twc | COPYBIT;
				twc = 0;
				mbbuf1p = mbbuf1;
			}
			return(i);
		}
#endif /* EUC */
	}
	popi();
	tty = 0;
#ifdef	NROFF
	if (quiet)
		echo_on();
#endif	/* NROFF */
	return(0);
}


void
caseec(void)
{
	eschar = chget('\\');
}


void
caseeo(void)
{
	eschar = 0;
}


void
caseecs(void)
{
	ecs = eschar;
}


void
caseecr(void)
{
	eschar = ecs;
}


void
caseta(void)
{
	int	T[NTAB];
	register int i, j, n = 0;

	tabtab[0] = nonumb = 0;
	Tflg = 1;
	for (i = 0; ((i < (NTAB - 1)) && !nonumb); i++) {
		if (skip(0))
			break;
		tabtab[i] = max(hnumb(&tabtab[max(i-1,0)]), 0) & TABMASK;
		if (nonumb && cbits(ch) == 'T') {
			ch = 0;
			nonumb = 0;
			Tflg = 0;
			goto T;
		}
		if (!nonumb) 
			switch (cbits(ch)) {
			case 'C':
				tabtab[i] |= CTAB;
				break;
			case 'R':
				tabtab[i] |= RTAB;
				break;
			default: /*includes L*/
				break;
			}
		nonumb = ch = 0;
	}
	Tflg = 0;
	tabtab[i] = 0;
	return;
T:
	for (j = 0; j < NTAB - 1 && !nonumb; j++) {
		if (skip(0))
			break;
		T[j] = hatoi() & TABMASK;
		if (!nonumb)
			switch (cbits(ch)) {
			case 'C':
				T[j] |= CTAB;
				break;
			case 'R':
				T[j] |= RTAB;
				break;
			default:
				break;
			}
		nonumb = ch = 0;
	}
	T[j] = 0;
	while (i < NTAB - 1) {
		if (T[j] == 0) {
			j = 0;
			n = (i ? tabtab[i-1] : 0) & TABMASK;
		}
		tabtab[i++] = (n + (T[j] & TABMASK)) | (T[j] & ~TABMASK);
		j++;
	}
	tabtab[i] = 0;
}


void
casene(void)
{
	register int i, j;

	skip(0);
	i = vnumb((int *)0);
	if (nonumb)
		i = lss;
	if (i > (j = findt1())) {
		i = lss;
		lss = j;
		dip->nls = 0;
		newline(0);
		lss = i;
	}
}


void
casetr(int flag)
{
	register int i, j;
	tchar k;

	lgf++;
	tryglf++;
	skip(1);
	if (!ch && cbits(getch()) == '\n')
		goto r;
	while ((i = cbits(k=getch())) != '\n') {
		if (ismot(k))
			goto r;
		if (ismot(k = getch()))
			goto r;
		if ((j = cbits(k)) == '\n')
			j = ' ';
		trtab[i] = j;
		if (flag & 1)
			trintab[j] = i;
		else
			trintab[j] = 0;
		if (flag & 2)
			trnttab[i] = i;
		else
			trnttab[i] = j;
	}
r:
	tryglf--;
}


void
casetrin(void)
{
	casetr(1);
}


void
casetrnt(void)
{
	casetr(2);
}


void
casecu(void)
{
	cu++;
	caseul();
}


void
caseul(void)
{
	register int i;

	noscale++;
	if (skip(0))
		i = 1;
	else 
		i = hatoi();
	if (ul && (i == 0)) {
		font = sfont;
		ul = cu = 0;
	}
	if (i) {
		if (!ul) {
			sfont = font;
			font = ulfont;
		}
		ul = i;
	}
	noscale = 0;
	mchbits();
}


void
caseuf(void)
{
	register int i, j;
	extern int findft(int, int);

	if (skip(0) || !(i = getrq(2)) || i == 'S' || (j = findft(i, 1))  == -1)
		ulfont = ULFONT; /*default underline position*/
	else 
		ulfont = j;
#ifdef NROFF
	if (ulfont == FT)
		ulfont = ULFONT;
#endif
}


void
caseit(int cflag)
{
	register int i;

	lgf++;
	it = itc = itmac = 0;
	noscale++;
	skip(0);
	i = hatoi();
	skip(0);
	if (!nonumb && (itmac = getrq(1))) {
		it = i;
		itc = cflag;
	}
	noscale = 0;
}


void
caseitc(void)
{
	caseit(1);
}


void
casemc(void)
{
	register int i;

	if (icf > 1)
		ic = 0;
	icf = 0;
	if (skip(0))
		return;
	ic = getch();
	icf = 1;
	skip(0);
	i = max(hnumb((int *)0), 0);
	if (!nonumb)
		ics = i;
}


static void
propchar(int *tp)
{
	int	c, *tpp;
	tchar	i;

	if (skip(0)) {
		*tp = IMP;
		return;
	}
	tpp = tp;
	do {
		while (!ismot(c = cbits(i = getch())) &&
				c != ' ' && c != '\n')
			if (tpp < &tp[NSENT])
				*tpp++ = c;
	} while (!skip(0));
}

void
casesentchar(void)
{
	propchar(sentch);
}

void
casetranschar(void)
{
	propchar(transch);
}

void
casebreakchar(void)
{
	propchar(breakch);
}

void
casenhychar(void)
{
	propchar(nhych);
}

void
caseconnectchar(void)
{
	propchar(connectch);
}

void
casemk(void)
{
	register int i, j;
	struct numtab	*np;

	if (dip != d)
		j = dip->dnl; 
	else 
		j = numtab[NL].val;
	if (skip(0)) {
		dip->mkline = j;
		return;
	}
	if ((i = getrq(1)) == 0)
		return;
	np = findr(i);
	np->val = j;
	prwatchn(np);
}


void
casesv(void)
{
	register int i;

	skip(0);
	if ((i = vnumb((int *)0)) < 0)
		return;
	if (nonumb)
		i = 1;
	sv += i;
	caseos();
}


void
caseos(void)
{
	register int savlss;

	if (sv <= findt1()) {
		savlss = lss;
		lss = sv;
		newline(0);
		lss = savlss;
		sv = 0;
	}
}


void
casenm(void)
{
	register int i;

	lnmod = nn = 0;
	if (skip(0))
		return;
	lnmod++;
	noscale++;
	i = inumb(&numtab[LN].val);
	if (!nonumb)
		numtab[LN].val = max(i, 0);
	prwatchn(&numtab[LN]);
	getnm(&ndf, 1);
	getnm(&nms, 0);
	getnm(&ni, 0);
	noscale = 0;
	nmbits = chbits;
}


void
getnm(int *p, int min)
{
	register int i;

	eat(' ');
	if (skip(0))
		return;
	i = atoi0();
	if (nonumb)
		return;
	*p = max(i, min);
}


void
casenn(void)
{
	noscale++;
	skip(0);
	nn = max(hatoi(), 1);
	noscale = 0;
}


void
caseab(void)
{
	casetm(1);
	done3(0);
}


#ifdef	NROFF
/*
 * The following routines are concerned with setting terminal options.
 *	The manner of doing this differs between research/Berkeley systems
 *	and UNIX System V systems (i.e. DOCUMENTER'S WORKBENCH)
 *	The distinction is controlled by the #define'd variable USG,
 *	which must be set by System V users.
 */


#ifdef	USG
#include <termios.h>
#define	ECHO_USG (ECHO | ECHOE | ECHOK | ECHONL)
struct termios	ttys;
#else
#include <sgtty.h>
struct	sgttyb	ttys[2];
#endif	/* USG */

int	ttysave[2] = {-1, -1};

void
save_tty(void)			/*save any tty settings that may be changed*/
{

#ifdef	USG
	if (tcgetattr(0, &ttys) >= 0)
		ttysave[0] = ttys.c_lflag;
#else
	if (gtty(0, &ttys[0]) >= 0)
		ttysave[0] = ttys[0].sg_flags;
	if (gtty(1, &ttys[1]) >= 0)
		ttysave[1] = ttys[1].sg_flags;
#endif	/* USG */

}


void 
restore_tty (void)			/*restore tty settings from beginning*/
{

	if (ttysave[0] != -1) {
#ifdef	USG
		ttys.c_lflag = ttysave[0];
		tcsetattr(0, TCSADRAIN, &ttys);
#else
		ttys[0].sg_flags = ttysave[0];
		stty(0, &ttys[0]);
	}
	if (ttysave[1] != -1) {
		ttys[1].sg_flags = ttysave[1];
		stty(1, &ttys[1]);
#endif	/* USG */
	}
}


void 
set_tty (void)			/*this replaces the use of bset and breset*/
{

#ifndef	USG			/*for research/BSD only, reset CRMOD*/
	if (ttysave[1] == -1)
		save_tty();
	if (ttysave[1] != -1) {
		ttys[1].sg_flags &= ~CRMOD;
		stty(1, &ttys[1]);
	}
#endif	/* USG */

}


void 
echo_off (void)			/*turn off ECHO for .rd in "-q" mode*/
{
	if (ttysave[0] == -1)
		return;

#ifdef	USG
	ttys.c_lflag &= ~ECHO_USG;
	tcsetattr(0, TCSADRAIN, &ttys);
#else
	ttys[0].sg_flags &= ~ECHO;
	stty(0, &ttys[0]);
#endif	/* USG */

}


void 
echo_on (void)			/*restore ECHO after .rd in "-q" mode*/
{
	if (ttysave[0] == -1)
		return;

#ifdef	USG
	ttys.c_lflag |= ECHO_USG;
	tcsetattr(0, TCSADRAIN, &ttys);
#else
	ttys[0].sg_flags |= ECHO;
	stty(0, &ttys[0]);
#endif	/* USG */

}
#endif	/* NROFF */
