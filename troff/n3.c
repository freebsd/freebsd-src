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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "n3.c	1.11	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)n3.c	1.181 (gritter) 10/23/09
 */

/*
 * Changes Copyright (c) 2014 Steffen Nurpmeso
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
 * troff3.c
 * 
 * macro and string routines, storage allocation
 */


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "tdef.h"
#ifdef NROFF
#include "tw.h"
#endif
#include "pt.h"
#include "ext.h"
#include <unistd.h>

#define	MHASH(x)	((x>>6)^x)&0177
struct	contab **mhash;	/* size must be 128 == the 0177 on line above */
#define	blisti(i)	(((i)-ENV_BLK*BLK) / BLK)
filep	*blist;
int	nblist;
int	pagech = '%';
int	strflg;

tchar *wbuf;
tchar *corebuf;

struct contab	*oldmn;
struct contab	*newmn;

static void	mrehash(struct contab *, int, struct contab **);
static void	_collect(int);
static struct contab	*_findmn(int, int, int);
static void	clrmn(struct contab *);
static void	caselds(void);
static void	casewatchlength(void);
static void	caseshift(void);
static void	casesubstring(void);
static void	caselength(void);
static void	caseindex(void);
static void	caseasciify(void);
static void	caseunformat(int);
static int	getls(int, int *, int);
static void	addcon(int, char *, void(*)(int));

static const struct {
	char	*n;
	void	(*f)(int);
} longrequests[] = {
	{ "aln",		(void(*)(int))casealn },
	{ "als",		(void(*)(int))caseals },
	{ "asciify",		(void(*)(int))caseasciify },
	{ "bleedat",		(void(*)(int))casebleedat },
	{ "blm",		(void(*)(int))caseblm },
	{ "brnl",		(void(*)(int))casebrnl },
	{ "brpnl",		(void(*)(int))casebrpnl },
	{ "box",		(void(*)(int))casebox},
	{ "boxa",		(void(*)(int))caseboxa},
	{ "break",		(void(*)(int))casebreak},
	{ "breakchar",		(void(*)(int))casebreakchar },
	{ "brp",		(void(*)(int))casebrp },
	{ "char",		(void(*)(int))casechar },
	{ "chomp",		(void(*)(int))casechomp },
	{ "chop",		(void(*)(int))casechop },
	{ "close",		(void(*)(int))caseclose },
	{ "connectchar",	(void(*)(int))caseconnectchar },
	{ "continue",		(void(*)(int))casecontinue },
	{ "cropat",		(void(*)(int))casecropat },
	{ "dch",		(void(*)(int))casedch },
	{ "dwh",		(void(*)(int))casedwh },
	{ "ecs",		(void(*)(int))caseecs },
	{ "ecr",		(void(*)(int))caseecr },
	{ "errprint",		(void(*)(int))caseerrprint },
	{ "escoff",		(void(*)(int))caseescoff },
	{ "escon",		(void(*)(int))caseescon },
	{ "evc",		(void(*)(int))caseevc },
	{ "fallback",		(void(*)(int))casefallback },
	{ "fchar",		(void(*)(int))casefchar },
	{ "fdeferlig",		(void(*)(int))casefdeferlig },
	{ "feature",		(void(*)(int))casefeature },
	{ "fkern",		(void(*)(int))casefkern },
	{ "flig",		(void(*)(int))caseflig },
	{ "fps",		(void(*)(int))casefps },
	{ "fspacewidth",	(void(*)(int))casefspacewidth },
	{ "ftr",		(void(*)(int))caseftr },
	{ "fzoom",		(void(*)(int))casefzoom },
	{ "hcode",		(void(*)(int))casehcode },
	{ "hidechar",		(void(*)(int))casehidechar },
	{ "hlm",		(void(*)(int))casehlm },
	{ "hylang",		(void(*)(int))casehylang },
	{ "hylen",		(void(*)(int))casehylen },
	{ "hypp",		(void(*)(int))casehypp },
	{ "index",		(void(*)(int))caseindex },
	{ "itc",		(void(*)(int))caseitc },
	{ "kern",		(void(*)(int))casekern },
	{ "kernafter",		(void(*)(int))casekernafter },
	{ "kernbefore",		(void(*)(int))casekernbefore },
	{ "kernpair",		(void(*)(int))casekernpair },
	{ "lc_ctype",		(void(*)(int))caselc_ctype },
	{ "lds",		(void(*)(int))caselds },
	{ "length",		(void(*)(int))caselength },
	{ "letadj",		(void(*)(int))caseletadj },
	{ "lhang",		(void(*)(int))caselhang },
	{ "lnr",		(void(*)(int))caselnr },
	{ "lnrf",		(void(*)(int))caselnrf },
	{ "lpfx",		(void(*)(int))caselpfx },
	{ "lsm",		(void(*)(int))caselsm },
	{ "mediasize",		(void(*)(int))casemediasize },
	{ "minss",		(void(*)(int))caseminss },
	{ "nhychar",		(void(*)(int))casenhychar },
	{ "nop",		(void(*)(int))casenop },
	{ "nrf",		(void(*)(int))casenrf },
	{ "open",		(void(*)(int))caseopen },
	{ "opena",		(void(*)(int))caseopena },
	{ "output",		(void(*)(int))caseoutput },
	{ "padj",		(void(*)(int))casepadj },
	{ "papersize",		(void(*)(int))casepapersize },
	{ "psbb",		(void(*)(int))casepsbb },
	{ "pshape",		(void(*)(int))casepshape },
	{ "pso",		(void(*)(int))casepso },
	{ "rchar",		(void(*)(int))caserchar },
	{ "recursionlimit",	(void(*)(int))caserecursionlimit },
	{ "return",		(void(*)(int))casereturn },
	{ "rhang",		(void(*)(int))caserhang },
	{ "rnn",		(void(*)(int))casernn },
	{ "sentchar",		(void(*)(int))casesentchar },
	{ "shc",		(void(*)(int))caseshc },
	{ "shift",		(void(*)(int))caseshift },
	{ "spacewidth",		(void(*)(int))casespacewidth },
	{ "spreadwarn",		(void(*)(int))casespreadwarn },
	{ "substring",		(void(*)(int))casesubstring },
	{ "tmc",		(void(*)(int))casetmc },
	{ "track",		(void(*)(int))casetrack },
	{ "transchar",		(void(*)(int))casetranschar },
	{ "trimat",		(void(*)(int))casetrimat },
	{ "trin",		(void(*)(int))casetrin },
	{ "trnt",		(void(*)(int))casetrnt },
	{ "unformat",		(void(*)(int))caseunformat },
	{ "unwatch",		(void(*)(int))caseunwatch },
	{ "unwatchn",		(void(*)(int))caseunwatchn },
#ifdef NROFF
	{ "utf8conv",		(void(*)(int))caseutf8conv },
#endif
	{ "vpt",		(void(*)(int))casevpt },
	{ "warn",		(void(*)(int))casewarn },
	{ "watch",		(void(*)(int))casewatch },
	{ "watchlength",	(void(*)(int))casewatchlength },
	{ "watchn",		(void(*)(int))casewatchn },
	{ "while",		(void(*)(int))casewhile },
	{ "write",		(void(*)(int))casewrite },
	{ "writec",		(void(*)(int))casewritec },
	{ "writem",		(void(*)(int))casewritem },
	{ "xflag",		(void(*)(int))casexflag },
	{ NULL,			NULL }
};

static void *
_growcontab(struct contab **contp, int *NMp, struct contab ***hashp)
{
	int	i, j, inc = 256;
	ptrdiff_t	sft;
	struct contab	*onc;
	struct s	*s;

	onc = *contp;
	if ((*contp = realloc(*contp, (*NMp+inc) * sizeof **contp)) == NULL)
		return NULL;
	memset(&(*contp)[*NMp], 0, inc * sizeof **contp);
	if (*NMp == 0) {
		if (contp == &contab) {
			for (i = 0; initcontab[i].f; i++)
				(*contp)[i] = initcontab[i];
			for (j = 0; longrequests[j].f; j++)
				addcon(i++, longrequests[j].n,
						longrequests[j].f);
		}
		*hashp = calloc(128, sizeof **hashp);
		mrehash(*contp, inc, *hashp);
	} else {
		sft = (char *)*contp - (char *)onc;
		for (i = 0; i < 128; i++)
			if ((*hashp)[i])
				(*hashp)[i] = (struct contab *)
					((char *)((*hashp)[i]) + sft);
		for (i = 0; i < *NMp; i++)
			if ((*contp)[i].link)
				(*contp)[i].link = (struct contab *)
					((char *)((*contp)[i].link) + sft);
		for (s = frame; s != stk; s = s->pframe)
			if (s->contp >= onc && s->contp < &onc[*NMp])
				s->contp = (struct contab *)
					((char *)(s->contp) + sft);
		for (i = 0; i <= dilev; i++)
			if (d[i].soff >= onc && d[i].soff < &onc[*NMp])
				d[i].soff = (struct contab *)
					((char *)(d[i].soff) + sft);
	}
	*NMp += inc;
	return *contp;
}

void *
growcontab(void)
{
	return _growcontab(&contab, &NM, &mhash);
}

void *
growblist(void)
{
	static tchar	*_corebuf;
	int	inc = 512;
	tchar	*ocb;

	if ((blist = realloc(blist, (nblist+inc) * sizeof *blist)) == NULL)
		return NULL;
	memset(&blist[nblist], 0, inc * sizeof *blist);
	ocb = _corebuf;
	if ((_corebuf = realloc(_corebuf,
	    ((ENV_BLK+nblist+inc+1) * BLK + 1) * sizeof *_corebuf)) == NULL)
		return NULL;
	if (ocb == NULL)
		memset(_corebuf, 0, ((ENV_BLK+1) * BLK + 1) * sizeof *_corebuf);
	corebuf = &_corebuf[1];
	memset(&corebuf[(ENV_BLK+nblist+1) * BLK], 0,
			inc * BLK * sizeof *corebuf);
	if (wbuf)
		wbuf = (tchar *)((char *)wbuf + ((char *)corebuf-(char *)ocb));
	nblist += inc;
	return blist;
}

void
caseig(void)
{
	register int i;
	register filep oldoff;

	oldoff = offset;
	offset = 0;
	i = copyb();
	offset = oldoff;
	if (i != '.')
		control(i, 1);
}


void
casern(void)
{
	register int i, j;

	lgf++;
	skip(1);
	if ((i = getrq(0)) == 0)
		return;
	if ((oldmn = _findmn(i, 0, 0)) == NULL) {
		nosuch(i);
		return;
	}
	skip(1);
	j = getrq(1);
	clrmn(_findmn(j, 0, oldmn->flags & FLAG_LOCAL));
	if (j) {
		munhash(oldmn);
		oldmn->rq = j;
		maddhash(oldmn);
		if (oldmn->flags & FLAG_WATCH)
			errprint("%s: %s%s renamed to %s", macname(lastrq),
					oldmn->flags & FLAG_LOCAL ?
						"local " : "",
					macname(i), macname(j));
	}
}

static struct contab **
gethash(struct contab *mp)
{
	struct s	*sp;
	struct contab	**mh;

	if (mp >= contab && mp < &contab[NM])
		mh = mhash;
	else {
		sp = macframe();
		if (mp >= sp->contab && mp < &sp->contab[sp->NM])
			mh = sp->mhash;
		else
			mh = NULL;
	}
	return mh;
}

void
maddhash(register struct contab *rp)
{
	register struct contab **hp;
	struct contab	**mh;

	if (rp->rq == 0)
		return;
	if ((mh = gethash(rp)) == NULL)
		return;
	hp = &mh[MHASH(rp->rq)];
	rp->link = *hp;
	*hp = rp;
}

void
munhash(register struct contab *mp)
{	
	register struct contab *p;
	register struct contab **lp;
	struct contab	**mh;

	if (mp->rq == 0)
		return;
	if ((mh = gethash(mp)) == NULL)
		return;
	lp = &mh[MHASH(mp->rq)];
	p = *lp;
	while (p) {
		if (p == mp) {
			*lp = p->link;
			p->link = 0;
			return;
		}
		lp = &p->link;
		p = p->link;
	}
}

static void
mrehash(struct contab *contp, int n, struct contab **hashp)
{
	register struct contab *p;
	register int i;

	for (i=0; i<128; i++)
		hashp[i] = 0;
	for (p=contp; p < &contp[n]; p++)
		p->link = 0;
	for (p=contp; p < &contp[n]; p++) {
		if (p->rq == 0)
			continue;
		i = MHASH(p->rq);
		p->link = hashp[i];
		hashp[i] = p;
	}
}

void
caserm(void)
{
	struct contab	*contp, *contt;
	int j, cnt = 0;

	lgf++;
	while (!skip(!cnt++)) {
		if ((j = getrq(2)) <= 0)
			continue;
		if ((contp = _findmn(j, 0, 0)) == NULL)
			continue;
		if (contp->als) {
			contt = _findmn(j, 1, contp->flags & FLAG_LOCAL);
			/* bugfix by S.N. */
			if (contt != NULL && --contt->nlink <= 0)
				clrmn(contt);
		}
		if (contp->nlink > 0)
			contp->nlink--;
		if (contp->flags & FLAG_WATCH)
			errprint("%s: %s%s removed", macname(lastrq),
				contp->flags & FLAG_LOCAL ? "local " : "",
				macname(j));
		if (contp->nlink <= 0)
			clrmn(contp);
	}
	lgf--;
}


void
caseas(void)
{
	app++;
	caseds();
}


void
caseds(void)
{
	ds++;
	casede();
}


void
caseam(void)
{
	app++;
	casede();
}

static void
caselds(void)
{
	dl += macframe() != stk;
	caseds();
}

void
casede(void)
{
	register int i, req;
	register filep savoff;
	int	k, nlink;

	if (dip != d)
		wbfl();
	req = '.';
	lgf++;
	skip(1);
	if ((i = getrq(1)) == 0)
		goto de1;
	if ((offset = finds(i, 1, !ds)) == 0)
		goto de1;
	if (ds)
		copys();
	else 
		req = copyb();
	wbfl();
	if (oldmn != NULL && (nlink = oldmn->nlink) > 0)
		k = oldmn->rq;
	else {
		k = i;
		nlink = 0;
	}
	clrmn(oldmn);
	if (newmn != NULL) {
		if (newmn->rq)
			munhash(newmn);
		newmn->rq = k;
		newmn->nlink = nlink;
		newmn->flags &= ~FLAG_DIVERSION;
		if (ds)
			newmn->flags |= FLAG_STRING;
		else
			newmn->flags &= ~FLAG_STRING;
		maddhash(newmn);
		prwatch(newmn, i, 1);
	} else if (apptr)
		prwatch(findmn(i), i, 1);
	if (apptr) {
		savoff = offset;
		offset = apptr;
		wbt((tchar) IMP);
		offset = savoff;
	}
	offset = dip->op;
	if (req != '.')
		control(req, 1);
de1:
	ds = app = 0;
	return;
}


static struct contab *
findmn1(struct contab **hashp, register int i, int als)
{
	register struct contab *p;

	for (p = hashp[MHASH(i)]; p; p = p->link)
		if (i == p->rq) {
			if (als && p->als)
				return(findmn1(hashp, p->als, als));
			return(p);
		}
	return(NULL);
}


static struct contab *
_findmn(register int i, int als, int forcelocal)
{
	struct s	*s;
	struct contab	*contp;

	s = macframe();
	if (forcelocal || (s != stk && s->mhash)) {
		if (s->mhash == NULL)
			return NULL;
		if ((contp = findmn1(s->mhash, i, als)) != NULL)
			return contp;
		if (forcelocal)
			return NULL;
	}
	return findmn1(mhash, i, als);
}


struct contab *
findmn(int i)
{
	return _findmn(i, 1, 0);
}


struct contab *
findmx(int i)
{
	return findmn1(mhash, i, 1);
}


void
clrmn(struct contab *contp)
{
	struct s	*s;

	if (contp != NULL) {
		if (contp->flags & FLAG_USED) {
			if (warn & WARN_MAC)
				errprint("Macro %s removed while in use",
						macname(contp->rq));
			for (s = frame; s != stk; s = s->pframe)
				if (s->contp == contp)
					s->contp = NULL;
		} else if (contp->mx)
			ffree((filep)contp->mx);
		munhash(contp);
		memset(contp, 0, sizeof *contp);
		contp->rq = 0;
		contp->mx = 0;
		contp->f = 0;
		contp->als = 0;
		contp->nlink = 0;
	}
}


/*
 * Note: finds() may invalidate the result of a previous findmn()
 * for another macro since it may call growcontab().
 */
filep 
finds(register int mn, int als, int globonly)
{
	register tchar i;
	register filep savip;
	enum flags	flags = 0;
	struct s	*s;
	struct contab	**contp, ***hashp;
	int	*NMp;

	oldmn = _findmn(mn, als, dl);
	newmn = NULL;
	apptr = (filep)0;
	if (oldmn != NULL)
		flags = oldmn->flags;
	if (globonly && (dl || (oldmn && oldmn->flags & FLAG_LOCAL))) {
		errprint("refusing to create local %s %s",
			diflg || (oldmn && oldmn->flags & FLAG_DIVERSION) ?
				"diversion" : "macro",
			macname(mn));
		app = 0;
		return(0);
	}
	if (app && oldmn != NULL && oldmn->mx) {
		savip = ip;
		ip = (filep)oldmn->mx;
		oldmn = NULL;
		while ((i = rbf()) != 0) {
			if (!diflg && istail(i))
				corebuf[ip - 1] &= ~(tchar)TAILBIT;
		}
		apptr = ip;
		if (!diflg)
			ip = incoff(ip);
		nextb = ip;
		ip = savip;
	} else {
		if (oldmn && oldmn->flags & FLAG_LOCAL)
			dl++;
		if (dl && (s = macframe()) != stk) {
			contp = &s->contab;
			NMp = &s->NM;
			hashp = &s->mhash;
		} else {
			dl = 0;
			contp = &contab;
			NMp = &NM;
			hashp = &mhash;
		}
		for (i = 0; i < *NMp; i++) {
			if ((*contp)[i].rq == 0)
				break;
		}
		nextb = 0;
		if ((i == *NMp && _growcontab(contp, NMp, hashp) == NULL) ||
				(als && (nextb = alloc()) == 0)) {
			app = 0;
			if (macerr++ > 1)
				done2(02);
			errprint("Too many (%d) string/macro names", NM);
			edone(04);
			return(als ? offset = 0 : 0);
		}
		oldmn = _findmn(mn, als, dl);
		(*contp)[i].mx = (unsigned) nextb;
		newmn = &(*contp)[i];
		if (!diflg) {
			if (oldmn == NULL)
				newmn->rq = -1;
		} else {
			newmn->rq = mn;
			maddhash(newmn);
		}
		newmn->flags = flags&(FLAG_WATCH|FLAG_STRING|FLAG_DIVERSION);
		if (dl)
			newmn->flags |= FLAG_LOCAL;
	}
	dl = app = 0;
	return(als ? offset = nextb : 1);
}


int 
skip (int required)		/*skip over blanks; return nlflg*/
{
	register tchar i;

	while (cbits(i = getch()) == ' ')
		;
	ch = i;
	if (nlflg && required)
		missing();
	return(nlflg);
}


int 
copyb(void)
{
	register int i, j, state;
	register tchar ii;
	int	req;
	filep savoff = 0, tailoff = 0;
	tchar	tailc = 0;
	char	*contp, *mn;
	size_t l;

	if (skip(0) || !(j = getrq(1)))
		j = '.';
	req = j;
	contp = macname(req);
	l = strlen(contp) + 1;
	mn = malloc(l);
	n_strcpy(mn, contp, l);
	copyf++;
	flushi();
	nlflg = 0;
	state = 1;

/* state 0	eat up
 * state 1	look for .
 * state 2	look for chars of end macro
 */

	while (1) {
		i = cbits(ii = getch());
		if (state == 2 && mn[j] == 0) {
			ch = ii;
			if (!getach())
				break;
			state = 0;
			goto c0;
		}
		if (i == '\n') {
			state = 1;
			nlflg = 0;
			tailoff = offset;
			tailc = ii;
			ii &= ~(tchar)TAILBIT;
			goto c0;
		}
		if (state == 1 && i == '.') {
			state++;
			savoff = offset;
			j = 0;
			goto c0;
		}
		if (state == 2) {
			if (i == mn[j]) {
				j++;
				goto c0;
			} else if (i == ' ' || i == '\t') {
				goto c0;
			}
		}
		state = 0;
c0:
		if (offset)
			wbf(ii);
	}
	if (offset) {
		wbfl();
		offset = savoff;
		wbt((tchar)0);
		if (tailoff) {
			offset = tailoff;
			wbt(tailc | TAILBIT);
		}
	}
	copyf--;
	free(mn);
	return(req);
}


void
copys(void)
{
	register tchar i;

	copyf++;
	if (skip(0))
		goto c0;
	if (cbits(i = getch()) != '"')
		wbf(i);
	while (cbits(i = getch()) != '\n')
		wbf(i);
c0:
	wbt((tchar)0);
	copyf--;
}


filep 
alloc (void)		/*return free blist[] block in nextb*/
{
	register int i;
	register filep j;

	do {
		for (i = 0; i < nblist; i++) {
			if (blist[i] == 0)
				break;
		}
	} while (i == nblist && growblist() != NULL);
	if (i == nblist) {
		j = 0;
	} else {
		blist[i] = -1;
		j = (filep)i * BLK + ENV_BLK * BLK;
	}
#ifdef	DEBUG
	if (debug & DB_ALLC) {
		char cc1, cc2;
		fprintf(stderr, "alloc: ");
		if (oldmn != NULL) {
			cc1 = oldmn->rq & 0177;
			if ((cc2 = (oldmn->rq >> BYTE) & 0177) == 0)
				cc2 = ' ';
			fprintf(stderr, "oldmn %p %c%c, ", oldmn, cc1, cc2);
		}
		fprintf(stderr, "newmn %p; nextb was %lx, will be %lx\n",
			newmn, (long)nextb, (long)j);
	}
#endif	/* DEBUG */
	return(nextb = j);
}


void
ffree (		/*free blist[i] and blocks pointed to*/
    filep i
)
{
	register int j;

	while (blist[j = blisti(i)] != (unsigned) ~0) {
		i = (filep) blist[j];
		blist[j] = 0;
	}
	blist[j] = 0;
}

void
wbt(tchar i)
{
	wbf(i);
	wbfl();
}


void
wbf (			/*store i into blist[offset] (?) */
    register tchar i
)
{
	register int j;

	if (!offset)
		return;
	if (!woff) {
		woff = offset;
		wbuf = &corebuf[woff];
		wbfi = 0;
	}
	wbuf[wbfi++] = i;
	if (!((++offset) & (BLK - 1))) {
		wbfl();
		j = blisti(--offset);
		if (j < 0 || (j >= nblist && growblist() == NULL)) {
			errprint("Out of temp file space");
			done2(01);
		}
		if (blist[j] == (unsigned) ~0) {
			if (alloc() == 0) {
				errprint("Out of temp file space");
				done2(01);
			}
			blist[j] = (unsigned)(nextb);
		}
		offset = ((filep)blist[j]);
	}
	if (wbfi >= BLK)
		wbfl();
}


void
wbfl (void)			/*flush current blist[] block*/
{
	if (woff == 0)
		return;
	if ((woff & (~(BLK - 1))) == (roff & (~(BLK - 1))))
		roff = -1;
	woff = 0;
}


tchar 
rbf (void)		/*return next char from blist[] block*/
{
	register tchar i;
	register filep j, p;

	if (ip == -1) {		/* for rdtty */
		if ((j = rdtty()))
			return(j);
		else
			return(popi());
	}
	if (ip == -2) {
		errprint("Bad storage while processing paragraph");
		ip = 0;
		done2(-5);
	}
	/* this is an inline expansion of rbf0: dirty! */
	i = corebuf[ip];
	/* end of rbf0 */
	if (i == 0) {
		if (!app)
			i = popi();
		return(i);
	}
	/* this is an inline expansion of incoff: also dirty */
	p = ++ip;
	if ((p & (BLK - 1)) == 0) {
		if ((ip = blist[blisti(p-1)]) == (unsigned) ~0) {
			errprint("Bad storage allocation");
			ip = 0;
			done2(-5);
		}
		/* this was meant to protect against people removing
		 * the macro they were standing on, but it's too
		 * sensitive to block boundaries.
		 * if (ip == 0) {
		 *	errprint("Block removed while in use");
		 *	done2(-6);
		 * }
		 */
	}
	return(i);
}


tchar 
rbf0(register filep p)
{
	return(corebuf[p]);
}


filep 
incoff (		/*get next blist[] block*/
    register filep p
)
{
	p++;
	if ((p & (BLK - 1)) == 0) {
		if ((p = blist[blisti(p-1)]) == (unsigned) ~0) {
			errprint("Bad storage allocation");
			done2(-5);
		}
	}
	return(p);
}


tchar 
popi(void)
{
	register struct s *p;
	tchar	c, d;

	if (frame == stk)
		return(0);
	if (strflg)
		strflg--;
	p = frame;
	sfree(p);
	if (p->contp != NULL)
		p->contp->flags &= ~FLAG_USED;
	frame = p->pframe;
	ip = p->pip;
	pendt = p->ppendt;
	lastpbp = p->lastpbp;
	c = p->pch;
	if (p->loopf & LOOP_NEXT) {
		d = ch;
		ch = c;
		pushi(p->newip, p->mname, p->flags);
		c = 0;
		ch = d;
	} else
		if (p->loopf & LOOP_FREE)
			ffree(p->newip);
	free(p);
	if (frame->flags & FLAG_PARAGRAPH)
		longjmp(*frame->jmp, 1);
	return(c);
}


int 
pushi(filep newip, int mname, enum flags flags)
{
	register struct s *p;

	p = nxf;
	p->pframe = frame;
	p->pip = ip;
	p->ppendt = pendt;
	p->pch = ch;
	p->lastpbp = lastpbp;
	p->mname = mname;
	p->flags = flags;
	if (mname != LOOP) {
		p->frame_cnt = frame->frame_cnt + 1;
		p->tail_cnt = frame->tail_cnt + 1;
	} else {
		p->frame_cnt = frame->frame_cnt;
		p->tail_cnt = frame->tail_cnt;
		p->loopf = LOOP_EVAL;
	}
	p->newip = newip;
	lastpbp = pbp;
	pendt = ch = 0;
	frame = nxf;
	nxf = calloc(1, sizeof *nxf);
	return(ip = newip);
}


void
sfree(struct s *p)
{
	int	i;

	if (p->nargs > 0) {
		free(p->argt);
		free(p->argsp);
	}
	free(p->numtab);
	free(p->nhash);
	if (p->contab) {
		for (i = 0; i < p->NM; i++)
			if (p->contab[i].mx > 0)
				ffree((filep)p->contab[i].mx);
		free(p->contab);
		free(p->mhash);
	}
}


struct s *
macframe(void)
{
	struct s	*p;

	for (p = frame; p != stk &&
			(p->flags & (FLAG_STRING|FLAG_DIVERSION) || p->loopf);
			p = p->pframe);
	return(p);
}

static int
_getsn(int *strp, int create)
{
	register int i;

	if ((i = getach()) == 0)
		return(0);
	if (i == '(')
		return(getrq2());
	else if (i == '[' && xflag > 1)
		return(getls(']', strp, create));
	else 
		return(i);
}

int
getsn(int create)
{
	return _getsn(0, create);
}


int 
setstr(void)
{
	struct contab	*contp;
	register int i, k;
	int	space = 0;
	tchar	c;

	lgf++;
	if ((i = _getsn(&space, 0)) == 0 || (contp = findmn(i)) == NULL ||
			!contp->mx) {
		if (space) {
			do {
				if (cbits(c = getch()) == ']')
					break;
			} while (!nlflg);
			if (nlflg)
				nodelim(']');
		}
		nosuch(i);
		lgf--;
		return(0);
	} else {
		if (space)
			_collect(']');
		else
			nxf->nargs = 0;
		strflg++;
		lgf--;
		contp->flags |= FLAG_USED;
		k = pushi((filep)contp->mx, i, contp->flags);
		frame->contp = contp;
		return(k);
	}
}

void
collect(void)
{
	_collect(0);
}

static void
_collect(int termc)
{
	register tchar i = 0;
	int	at = 0, asp = 0;
	int	nt = 0, nsp = 0, nsp0;
	int	quote, right;
	struct s *savnxf;

	copyf++;
	nxf->nargs = 0;
	nxf->argt = NULL;
	nxf->argsp = NULL;
	savnxf = nxf;
	nxf = calloc(1, sizeof *nxf);
	if (skip(0))
		goto rtn;

	strflg = 0;
	while (!skip(0)) {
		if (nt >= at)
			savnxf->argt = realloc(savnxf->argt,
				(at += 10) * sizeof *savnxf->argt);
		savnxf->argt[nt] = nsp0 = nsp; /* CK: Bugfix: \} counts \n(.$ */
		quote = right = 0;
		if (cbits(i = getch()) == '"')
			quote++;
		else 
			ch = i;
		while (1) {
			i = getch();
			if (termc && !quote && i == termc) {
				if (nsp >= asp)
					savnxf->argsp = realloc(savnxf->argsp,
						++asp * sizeof *savnxf->argsp);
				nt++;
				savnxf->argsp[nsp++] = 0;
				goto rtn;
			}
			if (nlflg || (!quote && cbits(i) == ' '))
				break;
			if (   quote
			    && (cbits(i) == '"')
			    && (cbits(i = getch()) != '"')) {
				ch = i;
				break;
			}
			if (nsp >= asp)
				savnxf->argsp = realloc(savnxf->argsp,
					(asp += 200) * sizeof *savnxf->argsp);
			if (cbits(i) == RIGHT) /* CK: Bugfix: \} counts \n(.$ */
				right = 1;
			else
				savnxf->argsp[nsp++] = i;
		}
		if (nsp >= asp)
			savnxf->argsp = realloc(savnxf->argsp,
				++asp * sizeof *savnxf->argsp);
		if (!right || nsp != nsp0) { /* CK: Bugfix: \} counts \n(.$ */
			nt++;
			savnxf->argsp[nsp++] = 0;
		}
	}
rtn:
	if (termc && i != termc)
		nodelim(termc);
	free(nxf);
	nxf = savnxf;
	nxf->nargs = nt;
	copyf--;
}


void
seta(void)
{
	register int c, i;
	char q[] = { 0, 0 };
	struct s	*s;

	for (s = frame; s != stk; s = s->pframe) {
		if (s->loopf)
			continue;
		if (gflag && s->contp && s->contp->flags & FLAG_STRING
				&& s->nargs == 0)
			continue;
		break;
	}
	switch (c = cbits(getch())) {
	case '@':
		q[0] = '"';
		/*FALLTHRU*/
	case '*':
		if (xflag == 0)
			goto dfl;
		for (i = s->nargs; i >= 1; i--) {
			if (q[0])
				cpushback(q);
			pushback(&s->argsp[s->argt[i - 1]]);
			if (q[0])
				cpushback(q);
			if (i > 1)
				cpushback(" ");
		}
		break;
	case '(':
		if (xflag == 0)
			goto dfl;
		c = cbits(getch());
		i = 10 * (c - '0');
		c = cbits(getch());
		i += c - '0';
		goto assign;
	case '[':
		if (xflag == 0)
			goto dfl;
		i = 0;
		while ((c = cbits(getch())) != ']' && c != '\n' && c != 0)
			i = 10 * i + (c - '0');
		goto assign;
	default:
	dfl:	i = c - '0';
	assign:	if (i > 0 && i <= s->nargs)
			pushback(&s->argsp[s->argt[i - 1]]);
		else if (i == 0)
			cpushback(macname(s->mname));
	}
}

static void
caseshift(void)
{
	int	i, j;
	struct s	*s;

	for (s = frame; s->loopf && s != stk; s = s->pframe);
	if (skip(0))
		i = 1;
	else {
		noscale++;
		i = hatoi();
		noscale--;
		if (nonumb)
			return;
	}
	if (i > 0 && i <= s->nargs) {
		s->nargs -= i;
		for (j = 1; j <= s->nargs; j++)
			s->argt[j - 1] = s->argt[j + i - 1];
	}
}


void
casebox(void)
{
	casedi(1);
}

void
caseboxa(void)
{
	caseda(1);
}

void
caseda(int box)
{
	app++;
	casedi(box);
}


void
casedi(int box)
{
	register int i, j;
	register int *k;
	int	nlink;

	lgf++;
	if (skip(0) || (i = getrq(1)) == 0) {
		if (dip != d)
			wbt((tchar)0);
		if (dilev > 0) {
#ifdef	DEBUG
			if (debug & DB_MAC)
				fprintf(stderr, "ending diversion %s\n",
						macname(dip->curd));
#endif	/* DEBUG */
			numtab[DN].val = dip->dnl;
			numtab[DL].val = dip->maxl;
			prwatchn(&numtab[DN]);
			prwatchn(&numtab[DL]);
			if (dip->boxenv) {
				relsev(&env);
				env = *dip->boxenv;
				free(dip->boxenv);
			}
			prwatch(dip->soff, dip->curd, 1);
			dip = &d[--dilev];
			offset = dip->op;
		} else if (warn & WARN_DI)
			errprint(".di outside active diversion");
		goto rtn;
	}
#ifdef	DEBUG
	if (debug & DB_MAC)
		fprintf(stderr, "starting diversion %s\n", macname(i));
#endif	/* DEBUG */
	if (++dilev == NDI) {
		struct d	*nd;
		const int	inc = 5;
		if ((nd = realloc(d, (NDI+inc) * sizeof *d)) == NULL) {
			--dilev;
			errprint("Diversions nested too deep");
			edone(02);
		}
		d = nd;
		memset(&d[NDI], 0, inc * sizeof *d);
		NDI += inc;
	}
	if (dip != d)
		wbt((tchar)0);
	diflg++;
	dip = &d[dilev];
	if ((dip->op = finds(i, 1, 1)) == 0) {
		dip = &d[--dilev];
		goto rtn;
	}
	dip->curd = i;
	if (newmn && oldmn != NULL && (nlink = oldmn->nlink) > 0) {
		munhash(newmn);
		j = oldmn->rq;
	} else {
		j = i;
		nlink = 0;
	}
	clrmn(oldmn);
	if (newmn) {
		newmn->rq = j;
		newmn->nlink = nlink;
		newmn->flags &= ~FLAG_STRING;
		newmn->flags |= FLAG_DIVERSION;
		if (i != j)
			maddhash(newmn);
		prwatch(newmn, i, 0);
	}
	dip->soff = newmn;
	k = &dip->dnl;
	dip->flss = 0;
	for (j = 0; j < 10; j++)
		k[j] = 0;	/*not op and curd*/
	memset(dip->mlist, 0, sizeof dip->mlist);
	memset(dip->nlist, 0, sizeof dip->nlist);
	if (box) {
		dip->boxenv = malloc(sizeof *dip->boxenv);
		*dip->boxenv = env;
		evc(&env, &env);
	} else
		dip->boxenv = 0;
rtn:
	app = 0;
	diflg = 0;
}


void
casedt(void)
{
	lgf++;
	dip->dimac = dip->ditrap = dip->ditf = 0;
	skip(0);
	dip->ditrap = vnumb((int *)0);
	if (nonumb)
		return;
	skip(0);
	dip->dimac = getrq(1);
}


void
caseals(void)
{
	struct contab	*contp;
	int	i, j, t;
	int	flags = 0;

	if (skip(1))
		return;
	i = getrq(1);
	if (skip(1))
		return;
	j = getrq(1);
	if ((contp = findmn(j)) == NULL) {
		nosuch(j);
		return;
	}
	if (contp->nlink == 0) {
		munhash(contp);
		t = makerq(NULL);
		contp->rq = t;
		maddhash(contp);
		if (contp->flags & FLAG_LOCAL)
			dl++;
		if (finds(j, 0, 0) != 0 && newmn) {
			newmn->als = t;
			newmn->rq = j;
			maddhash(newmn);
			contp->nlink = 1;
		}
	} else
		t = j;
	if (contp->flags & FLAG_LOCAL)
		dl++;
	if (finds(i, 0, !dl) != 0) {
		if (oldmn != NULL && newmn != NULL)
			flags = oldmn->flags | newmn->flags;
		flags &= FLAG_WATCH|FLAG_STRING|FLAG_DIVERSION;
		clrmn(oldmn);
		if (newmn) {
			if (newmn->rq)
				munhash(newmn);
			newmn->als = t;
			newmn->rq = i;
			newmn->flags |= flags;
			maddhash(newmn);
			contp = findmn(j);
			contp->nlink++;
			if (flags & FLAG_WATCH)
				errprint("%s: creating alias %s to %s%s %s",
					macname(lastrq),
					contp->flags & FLAG_LOCAL ?
						"local " : "",
					contp->flags & FLAG_STRING ? "string" :
						contp->flags & FLAG_DIVERSION ?
							"diversion" : "macro",
					macname(i), macname(j));
		}
	}
}


void
casewatch(int unwatch)
{
	struct contab	*contp;
	int	j;

	lgf++;
	if (skip(1))
		return;
	do {
		if (!(j = getrq(1)))
			break;
		if ((contp = findmn(j)) == NULL) {
			if (finds(j, 0, 0) == 0 || newmn == NULL)
				continue;
			if (newmn->rq)
				munhash(newmn);
			newmn->rq = j;
			maddhash(newmn);
			contp = newmn;
		}
		if (unwatch)
			contp->flags &= ~FLAG_WATCH;
		else
			contp->flags |= FLAG_WATCH;
	} while (!skip(0));
}


void
caseunwatch(void)
{
	casewatch(1);
}


static int	watchlength = 30;


static void
casewatchlength(void)
{
	int	i;

	if (!skip(1)) {
		noscale++;
		i = hatoi();
		noscale--;
		if (!nonumb)
			watchlength = i;
		if (watchlength < 0)
			watchlength = 0;
	}
}


void
prwatch(struct contab *contp, int rq, int prc)
{
	const char prtab[] = {
		'a',000,000,000,000,000,000,000,
		'b','t','n',000,000,000,000,000,
		'{','}','&',000,'%','c','e',' ',
		'!',000,000,000,000,000,000,'~',
		000
	};
	char	*buf = NULL;
	char	*local;
	filep	savip;
	tchar	c;
	int	j, k;

	if (contp == NULL)
		return;
	if (rq == 0)
		rq = contp->rq;
	local = contp->flags & FLAG_LOCAL ? "local " : "";
	if (contp->flags & FLAG_WATCH) {
		if (watchlength <= 10 || !prc) {
			errprint("%s: %s%s %s redefined", macname(lastrq),
				local,
				contp->flags & FLAG_STRING ? "string" :
					contp->flags & FLAG_DIVERSION ?
						"diversion" : "macro",
				macname(rq));
			return;
		}
		savip = ip;
		ip = (filep)contp->mx;
		app++;
		j = 0;
		buf = malloc(watchlength);
		while ((c = rbf()) != 0) {
			while (isxfunc(c, CHAR))
				c = charout[sbits(c)].ch;
			if (iscopy(c) && (k = wctomb(&buf[j], cbits(c))) > 0)
				j += k;
			else if (ismot(c))
				buf[j++] = '?';
			else if ((k = cbits(c)) < 0177) {
				if (isprint(k))
					buf[j++] = k;
				else if (istrans(c)) {
					buf[j++] = '\\';
					buf[j++] = ')';
				} else if (k < ' ' && prtab[k]) {
					buf[j++] = '\\';
					buf[j++] = prtab[k];
				} else if (k < ' ') {
					buf[j++] = '^';
					buf[j++] = k + 0100;
				} else
					buf[j++] = '?';
			} else if (k == ACUTE)
				buf[j++] = '\'';
			else if (k == GRAVE)
				buf[j++] = '`';
			else if (j == UNDERLINE)
				buf[j++] = '_';
			else if (j == MINUS)
				buf[j++] = '-';
			else
				buf[j++] = '?';
			if (j >= watchlength - 5 - mb_cur_max) {
				buf[j++] = '.';
				buf[j++] = '.';
				buf[j++] = '.';
				break;
			}
		}
		buf[j] = 0;
		ip = savip;
		app--;
		errprint("%s: %s%s %s redefined to \"%s\"", macname(lastrq),
				local,
				contp->flags & FLAG_STRING ? "string" :
					contp->flags & FLAG_DIVERSION ?
						"diversion" : "macro",
				macname(rq), buf);
		free(buf);
	}
}


void
casetl(void)
{
	register int j;
	int w[3];
	tchar *buf = NULL;
	int	bufsz = 0;
	register tchar *tp;
	tchar i, delim, nexti;
	int oev;

	dip->nls = 0;
	skip(1);
	if (ismot(delim = getch())) {
		ch = delim;
		delim = '\'';
	} else 
		delim = cbits(delim);
	noschr = 0;
	argdelim = delim;
	bufsz = LNSIZE;
	buf = malloc(bufsz * sizeof *buf);
	tp = buf;
	numtab[HP].val = 0;
	w[0] = w[1] = w[2] = 0;
	j = 0;
	nexti = getch();
	while (cbits(i = nexti) != '\n') {
		if (cbits(i) == cbits(delim)) {
			if (j < 3)
				w[j] = numtab[HP].val;
			numtab[HP].val = 0;
			j++;
			*tp++ = 0;
			nexti = getch();
		} else {
			if (cbits(i) == pagech) {
				setn1(numtab[PN].val, findr('%')->fmt,
				      sfmask(i));
				nexti = getch();
				continue;
			}
			numtab[HP].val += width(i);
			oev = ev;
			nexti = getch();
			if (ev == oev)
				numtab[HP].val += kernadjust(i, nexti);
			if (tp >= &buf[bufsz-10]) {
				tchar	*k;
				bufsz += 100;
				k = realloc(buf, bufsz * sizeof *buf);
				tp = (tchar *)
				    ((char *)tp + ((char *)k - (char *)buf));
				buf = k;
			}
			*tp++ = i;
		}
	}
	argdelim = 0;
	if (j<3)
		w[j] = numtab[HP].val;
	*tp++ = 0;
	*tp++ = 0;
	*tp++ = 0;
	tp = buf;
#ifdef NROFF
	horiz(po);
#endif
	while ((i = *tp++))
		pchar(i);
	if (w[1] || w[2])
	{
#ifdef NROFF
		if (gemu)
			horiz(j = quant((lt + HOR - w[1]) / 2 - w[0], HOR));
		else
#endif
			horiz(j = quant((lt - w[1]) / 2 - w[0], HOR));
	}
	while ((i = *tp++))
		pchar(i);
	if (w[2]) {
		horiz(lt - w[0] - w[1] - w[2] - j);
		while ((i = *tp++))
			pchar(i);
	}
	newline(0);
	if (dip != d) {
		if (dip->dnl > dip->hnl)
			dip->hnl = dip->dnl;
	} else {
		if (numtab[NL].val > dip->hnl)
			dip->hnl = numtab[NL].val;
	}
	free(buf);
}

void
casepc(void)
{
	pagech = chget(IMP);
}

void
casechop(void)
{
	int	i;
	struct contab	*contp;
	filep	savip;

	if (dip != d)
		wbfl();
	lgf++;
	skip(1);
	if ((i = getrq(0)) == 0)
		return;
	if ((contp = findmn(i)) == NULL || !contp->mx) {
		nosuch(i);
		return;
	}
	savip = ip;
	ip = (filep)contp->mx;
	app = 1;
	while (rbf() != 0);
	app = 0;
	if (ip > (filep)contp->mx) {
		offset = ip - 1;
		wbt(0);
	}
	ip = savip;
	offset = dip->op;
	prwatch(contp, i, 1);
}

void
casesubstring(void)
{
	struct contab	*contp;
	int	i, j, k, sz = 0, st;
	int	n1, n2 = -1, nlink;
	tchar	*tp = NULL, c;
	filep	savip;

	if (dip != d)
		wbfl();
	lgf++;
	skip(1);
	if ((i = getrq(0)) == 0)
		return;
	if ((contp = findmn(i)) == NULL || !contp->mx) {
		nosuch(i);
		return;
	}
	if (skip(1))
		return;
	noscale++;
	n1 = hatoi();
	if (skip(0) == 0)
		n2 = hatoi();
	noscale--;
	savip = ip;
	ip = (filep)contp->mx;
	k = 0;
	app = 1;
	while ((c = rbf()) != 0) {
		if (k >= sz) {
			sz += 512;
			tp = realloc(tp, sz * sizeof *tp);
		}
		tp[k++] = c;
	}
	app = 0;
	ip = savip;
	if ((offset = finds(i, 1, 0)) != 0) {
		st = 0;
		if (n1 < 0)
			n1 = k + n1;
		if (n2 < 0)
			n2 = k + n2;
		if (n1 >= 0 || n2 >= 0) {
			if (n2 < n1) {
				j = n1;
				n1 = n2;
				n2 = j;
			}
			for (j = 0; j <= k; j++) {
				if (st == 0) {
					if (j >= n1)
						st = 1;
				}
				if (st == 1) {
					if (tp)
						wbf(tp[j]);
					if (j >= n2)
						break;
				}
			}
		}
		wbt(0);
		if (oldmn != NULL && (nlink = oldmn->nlink) > 0)
			k = oldmn->rq;
		else {
			k = i;
			nlink = 0;
		}
		clrmn(oldmn);
		if (newmn) {
			if (newmn->rq)
				munhash(newmn);
			newmn->rq = k;
			newmn->nlink = nlink;
			maddhash(newmn);
			prwatch(newmn, i, 1);
		}
	}
	free(tp);
	offset = dip->op;
	ip = savip;
}

void
caselength(void)
{
	tchar	c;
	int	i, j;
	struct numtab	*numtp;

	lgf++;
	skip(1);
	if ((i = getrq(1)) == 0)
		return;
	j = 0;
	lgf--;
	copyf++;
	if (skip(1) == 0) {
		if (cbits(c = getch()) != '"' || ismot(c))
			ch = c;
		while(cbits(getch()) != '\n')
			j++;
	}
	copyf--;
	numtp = findr(i);
	numtp->val = j;
	prwatchn(numtp);
}

void
caseindex(void)
{
	int	i, j, n, N;
	struct contab	*contp;
	int	*sp = NULL, as = 0, ns = 0, *np;
	tchar	c;
	filep	savip;
	struct numtab	*numtp;

	lgf++;
	skip(1);
	if ((N = getrq(1)) == 0)
		return;
	skip(1);
	if ((i = getrq(1)) == 0)
		return;
	if ((contp = findmn(i)) == NULL || !contp->mx) {
		nosuch(i);
		return;
	}
	copyf++;
	if (!skip(0)) {
		if (cbits(c = getch()) != '"' || ismot(c))
			ch = c;
		while ((c = getch()) != 0 && !ismot(c) &&
				(i = cbits(c)) != '\n') {
			if (ns >= as)
				sp = realloc(sp, (as += 10) * sizeof *sp);
			sp[ns++] = i;
		}
		np = malloc((ns + 1) * sizeof *np);
		i = 0;
		j = -1;
		for (;;) {
			np[i++] = j++;
			if (i >= ns)
				break;
			while (j >= 0 && sp[i] != sp[j])
				j = np[j];
		}
		savip = ip;
		ip = (filep)contp->mx;
		app = 1;
		j = 0;
		n = 0;
		while ((c = rbf()) != 0 && j < ns) {
			while (j >= 0 && cbits(c) != sp[j])
				j = np[j];
			j++;
			n++;
		}
		n = j == ns ? n - ns : -1;
		app = 0;
		ip = savip;
		free(sp);
		free(np);
	} else
		n = -1;
	copyf--;
	numtp = findr(N);
	numtp->val = n;
	prwatchn(numtp);
}

static void
caseasciify(void)
{
	caseunformat(1);
}

static void
caseunformat(int flag)
{
	struct contab	*contp;
	int	i, j, k, nlink;
	int	ns = 0, as = 0;
	tchar	*tp = NULL, c;
	filep	savip;
	int	noout = 0;

	if (dip != d)
		wbfl();
	lgf++;
	skip(1);
	if ((i = getrq(0)) == 0)
		return;
	if ((contp = findmn(i)) == NULL || !contp->mx) {
		nosuch(i);
		return;
	}
	savip = ip;
	ip = (filep)contp->mx;
	ns = 0;
	app = 1;
	while ((c = rbf()) != 0) {
		if (ns >= as) {
			as += 512;
			tp = realloc(tp, as * sizeof *tp);
		}
		tp[ns++] = c;
	}
	app = 0;
	ip = savip;
	if ((offset = finds(i, 1, 0)) != 0) {
		for (j = 0; j < ns; j++) {
			if (!ismot(c) && cbits(c) == '\n')
				noout = 0;
			else if (j+1 < ns && isxfunc(tp[j+1], HYPHED))
				noout = 1;
			c = tp[j];
			while (flag & 1 && isxfunc(c, CHAR))
				c = charout[sbits(c)].ch;
			if (isadjspc(c)) {
				if (cbits(c) == WORDSP)
					setcbits(c, ' ');
				c &= ~ADJBIT;
			} else if (c == WORDSP) {
				j++;
				continue;
			} else if (c == FLSS) {
				j++;
				continue;
			} else if (cbits(c) == XFUNC) {
				switch (fbits(c)) {
				case FLDMARK:
					if ((c = sbits(c)) == 0)
						continue;
					break;
				case LETSP:
				case NLETSP:
				case LETSH:
				case NLETSH:
				case INDENT:
					continue;
				}
			} else if (isadjmot(c))
				continue;
			else if (cbits(c) == PRESC) {
				if (!noout) {
					wbf(eschar);
					wbf('e');
				}
				continue;
			}
			if (flag & 1 && !ismot(c) && cbits(c) != SLANT) {
#ifndef	NROFF
				int	m = cbits(c);
				int	f = fbits(c);
				int	k;
				if (islig(c) && lgrevtab && lgrevtab[f] &&
						lgrevtab[f][m]) {
					for (k = 0; lgrevtab[f][m][k]; k++)
						if (!noout)
							wbf(lgrevtab[f][m][k]);
					continue;
				} else
#endif
					c = cbits(c);
			}
			if (flag & 1 && !ismot(c) && (k = trintab[c]) != 0)
				c = k;
			if (!noout)
				wbf(c);
		}
		wbt(0);
		if (oldmn != NULL && (nlink = oldmn->nlink) > 0)
			k = oldmn->rq;
		else {
			k = i;
			nlink = 0;
		}
		clrmn(oldmn);
		if (newmn) {
			if (newmn->rq)
				munhash(newmn);
			newmn->rq = k;
			newmn->nlink = nlink;
			maddhash(newmn);
			prwatch(newmn, i, 1);
		}
	}
	free(tp);
	offset = dip->op;
}


/*
 * Tables for names with more than two characters. Any number in
 * contab.rq or numtab.rq that is greater or equal to MAXRQ2 refers
 * to a long name.
 */
#define	MAXRQ2	0200000

static struct map {
	struct map	*link;
	int	n;
} *map[128];
static char	**had;
static int	hadn;
static int	alcd;

#define	maphash(cp)	(_pjw(cp) & 0177)

static unsigned
_pjw(const char *cp)
{
	unsigned	h = 0, g;

	cp--;
	while (*++cp) {
		h = (h << 4 & 0xffffffff) + (*cp&0377);
		if ((g = h & 0xf0000000) != 0) {
			h = h ^ g >> 24;
			h = h ^ g;
		}
	}
	return h;
}

static int
mapget(const char *cp)
{
	int	h = maphash(cp);
	struct map	*mp;

	for (mp = map[h]; mp; mp = mp->link)
		if (strcmp(had[mp->n], cp) == 0)
			return mp->n;
	return hadn;
}

static void
mapadd(const char *cp, int n)
{
	int	h = maphash(cp);
	struct map	*mp;

	mp = calloc(1, sizeof *mp);
	mp->n = n;
	mp->link = map[h];
	map[h] = mp;
}

void
casepm(void)
{
	struct contab	*contp;
	register int i, k;
	int	xx, cnt, tcnt, kk, tot;
	filep j;

	kk = cnt = tcnt = 0;
	tot = !skip(0);
	for (i = 0; i < NM; i++) {
		if ((xx = contab[i].rq) == 0 || contab[i].mx == 0) {
			if (contab[i].als && (contp = findmx(xx)) != NULL) {
				k = contp - contab;
				if (contab[k].rq == 0 || contab[k].mx == 0)
					continue;
			} else
				continue;
		}
		tcnt++;
		if (contab[i].als == 0 && (j = (filep) contab[i].mx) != 0) {
			k = 1;
			while ((j = blist[blisti(j)]) != (unsigned) ~0) {
				k++; 
			}
			cnt++;
		} else
			k = 0;
		kk += k;
		if (!tot && contab[i].nlink == 0)
			fprintf(stderr, "%s %d\n", macname(xx), k);
	}
	fprintf(stderr, "pm: total %d, macros %d, space %d\n", tcnt, cnt, kk);
}

void
stackdump (void)	/* dumps stack of macros in process */
{
	struct s *p;

	if (frame != stk) {
		for (p = frame; p != stk; p = p->pframe)
			if (p->mname != LOOP)
				fprintf(stderr, "%s ", macname(p->mname));
		fprintf(stderr, "\n");
	}
}

static char	laststr[NC+1];

char *
macname(int rq)
{
	static char	buf[4][3];
	static int	i;
	if (rq < 0) {
		return laststr;
	} else if (rq < MAXRQ2) {
		i &= 3;
		buf[i][0] = rq&0177;
		buf[i][1] = (rq>>BYTE)&0177;
		buf[i][2] = 0;
		return buf[i++];
	} else if (rq - MAXRQ2 < hadn)
		return had[rq - MAXRQ2];
	else
		return "???";
}

const char nmctab[] = {
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	001,002,003,000,004,005,000,006,
	000,000,000,000,000,000,000,000,
	000
};

static tchar
mgetach(void)
{
	tchar	i;
	int	j;

	lgf++;
	i = getch();
	while (isxfunc(i, CHAR))
		i = charout[sbits(i)].ch;
	j = cbits(i);
	if (ismot(i) || j == ' ' || j == '\n' || j >= 0200 ||
			(j < sizeof nmctab && nmctab[j])) {
		if (!ismot(i) && j >= 0200)
			illseq(j, NULL, -3);
		ch = i;
		j = 0;
	}
	lgf--;
	return j & 0177;
}

/*
 * To handle requests with more than two characters, an additional
 * table is maintained. On places where more than two characters are
 * allowed, the characters collected are passed in "sofar", and "flags"
 * specifies whether the request is a new one. The routine returns an
 * integer which is above the regular PAIR() values.
 */
int
maybemore(int sofar, int flags)
{
	char	c, buf[NC+1], pb[] = { '\n', 0 };
	int	i = 2, n, _raw = raw, _init = init, _app = app;
	size_t	l;

	if (xflag < 2)
		return sofar;
	if (xflag == 2)
		raw = 1;
	else
		app = 0;
	buf[0] = sofar&BYTEMASK;
	buf[1] = (sofar>>BYTE)&BYTEMASK;
	do {
		c = xflag < 3 ? getch0() : mgetach();
		if (i+1 >= sizeof buf) {
			buf[i] = 0;
			goto retn;
		}
		buf[i++] = c;
	} while (c && c != ' ' && c != '\t' && c != '\n');
	buf[i-1] = 0;
	buf[i] = 0;
	if (i == 3)
		goto retn;
	if ((n = mapget(buf)) >= hadn) {
		if ((flags & 1) == 0) {
			n_strcpy(laststr, buf, sizeof(laststr));
		retn:	buf[i-1] = c;
			if (xflag < 3)
				cpushback(&buf[2]);
			raw = _raw;
			init = _init;
			app = _app;
			if (flags & 2) {
				if (i > 3 && xflag >= 3)
					sofar = -2;
			} else if (i > 3 && xflag >= 3) {
				buf[i-1] = 0;
				if (warn & WARN_MAC)
					errprint("%s: no such request", buf);
				sofar = 0;
			} else if (warn & WARN_SPACE && i > 3 &&
					_findmn(sofar, 0, 0) != NULL) {
				buf[i-1] = 0;
				errprint("%s: missing space", macname(sofar));
			}
			return sofar;
		}
		if (n >= alcd)
			had = realloc(had, (alcd += 20) * sizeof *had);
		l = strlen(buf) + 1;
		had[n] = malloc(l);
		n_strcpy(had[n], buf, l);
		hadn = n+1;
		mapadd(buf, n);
	}
	pb[0] = c;
	if (xflag < 3)
		cpushback(pb);
	raw = _raw;
	init = _init;
	app = _app;
	return MAXRQ2 + n;
}

static int
getls(int termc, int *strp, int create)
{
	char	c, buf[NC+1];
	int	i = 0, j = -1, n = -1;
	size_t	l;

	do {
		c = xflag < 3 ? getach() : mgetach();
		if (i >= sizeof buf)
			return -1;
		buf[i++] = c;
	} while (c && c != termc);
	if (strp)
		*strp = 0;
	if (c != termc) {
		if (strp && !nlflg)
			*strp = 1;
		else
			nodelim(termc);
	}
	buf[--i] = 0;
	if (i == 0 || (c != termc && (!strp || nlflg)))
		j = 0;
	else if (i <= 2) {
		j = PAIR(buf[0], buf[1]);
	} else {
		if ((n = mapget(buf)) >= hadn) {
			if (create) {
				if (hadn++ >= alcd)
					had = realloc(had, (alcd += 20) *
							sizeof *had);
				l = strlen(buf) + 1;
				had[n] = malloc(l);
				n_strcpy(had[n], buf, l);
				hadn = n + 1;
				mapadd(buf, n);
			} else {
				n = -1;
				n_strcpy(laststr, buf, sizeof(laststr));
			}
		}
	}
	return n >= 0 ? MAXRQ2 + n : j;
}

int
makerq(const char *name)
{
	static int	t;
	char	_name[20];
	int	n;
	size_t	l;

	if (name == NULL) {
		roff_sprintf(_name, sizeof(_name), "\13%d", ++t);
		name = _name;
	}
	if (name[0] == 0 || name[1] == 0 || name[2] == 0)
		return PAIR(name[0], name[1]);
	if ((n = mapget(name)) < hadn)
		return MAXRQ2 + n;
	if (hadn++ >= alcd)
		had = realloc(had, (alcd += 20) * sizeof *had);
	l = strlen(name) + 1;
	had[n] = malloc(l);
	n_strcpy(had[n], name, l);
	hadn = n + 1;
	mapadd(name, n);
	return MAXRQ2 + n;
}

static void
addcon(int t, char *rs, void(*f)(int))
{
	int	n = hadn;

	if (hadn++ >= alcd)
		had = realloc(had, (alcd += 20) * sizeof *had);
	had[n] = rs;
	contab[t].rq = MAXRQ2 + n;
	contab[t].f = f;
	mapadd(rs, n);
}
