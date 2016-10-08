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


/*	from OpenSolaris "n4.c	1.8	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)n4.c	1.102 (gritter) 10/23/09
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
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<locale.h>
#include	<limits.h>
#include	<float.h>
#include "tdef.h"
#ifdef NROFF
#include "tw.h"
#endif
#include "pt.h"
#include "ext.h"
/*
 * troff4.c
 * 
 * number registers, conversion, arithmetic
 */


int	regcnt = NNAMES;
int	falsef	= 0;	/* on if inside false branch of if */
#define	NHASH(i)	((i>>6)^i)&0177
struct	numtab	**nhash;	/* size must be 128 == the 0177 on line above */

static void	nrehash(struct numtab *, int, struct numtab **);
static struct numtab	*_findr(register int i, int, int, int, int *);
static struct acc	_atoi(int);
static struct acc	_atoi0(int);
static struct acc	ckph(int);
static struct acc	atoi1(tchar, int);
static struct acc	_inumb(int *, float *, int, int *);
static void	print_tab_setting(char *, size_t);

static void *
_grownumtab(struct numtab **numtp, int *NNp, struct numtab ***hashp)
{
	int	i, inc = 20;
	ptrdiff_t	j;
	struct numtab	*onc;

	onc = *numtp;
	if ((*numtp = realloc(*numtp, (*NNp+inc) * sizeof *numtab)) == NULL)
		return NULL;
	memset(&(*numtp)[*NNp], 0, inc * sizeof *numtab);
	if (*NNp == 0) {
		if (numtp == &numtab)
			for (i = 0; initnumtab[i].r; i++)
				(*numtp)[i] = initnumtab[i];
		if (*hashp == NULL)
			*hashp = calloc(128, sizeof *hashp);
		nrehash(*numtp, *NNp + inc, *hashp);
	} else {
		j = (char *)(*numtp) - (char *)onc;
		for (i = 0; i < 128; i++)
			if ((*hashp)[i])
				(*hashp)[i] = (struct numtab *)
					((char *)((*hashp)[i]) + j);
		for (i = 0; i < *NNp; i++)
			if ((*numtp)[i].link)
				(*numtp)[i].link = (struct numtab *)
					((char *)((*numtp)[i].link) + j);
	}
	*NNp += inc;
	return *numtp;
}

void *
grownumtab(void)
{
	return _grownumtab(&numtab, &NN, &nhash);
}

static void
print_tab_setting(char *tb, size_t siz) {
	char *cp;
	int i;
	for (i = 0; tabtab[i]; i++);
	while (--i >= 0) {
		cp = tb;
		if (i)
			*cp++ = ' ';
		cp = roff_sprintf(cp, siz - (cp - tb), "%d", tabtab[i]&TABMASK);
		*cp++ = 'u';
		switch (tabtab[i] & ~TABMASK) {
		case RTAB:
			*cp++ = 'R';
			break;
		case CTAB:
			*cp++ = 'C';
			break;
		}
		*cp = 0;
		cpushback(tb);
	}
}

#define	TMYES	if (tm) return(1)
#define	TMNO	if (tm) return(0)

static int
_setn(int tm)	/* tm: test for presence of readonly register */
{
	extern const char	revision[];
	struct numtab	*numtp;
	char	tb[30], *cp;
	const char	*name;
	struct s	*s;
	register int i;
	register tchar ii;
	int	f, j;
	float	fl;

	f = nform = 0;
	if (tm) {
		i = tm;
		goto sl;
	}
	noschr++;
	if ((i = cbits(ii = getach())) == '+')
		f = 1;
	else if (i == '-')
		f = -1;
	else 
		ch = ii;
	if (noschr) noschr--;
	if (falsef)
		f = 0;
	if ((i = getsn(1)) == 0)
		return(0);
sl:
	name = macname(i);
	if (i < 65536 && (i & 0177) == '.')
		switch (i >> BYTE) {
		case 's': 
			i = fl = u2pts(pts);
			if (i != fl)
				goto flt;
			break;
		case 'v': 
			i = lss;		
			break;
		case 'f': 
			i = font;	
			break;
		case 'p': 
			i = pl;		
			break;
		case 't':  
			i = findt1();	
			break;
		case 'o': 
			i = po;		
			break;
		case 'l': 
			i = ll;		
			break;
		case 'i': 
			i = in;		
			break;
		case '$': 
			for (s = frame; s->loopf && s != stk; s = s->pframe);
			i = s->nargs;
			break;
		case 'A': 
			i = ascii;		
			break;
		case 'c': 
			i = numtab[CD].val;		
			break;
		case 'n': 
			i = lastl;		
			break;
		case 'a': 
			i = ralss;		
			break;
		case 'h': 
			i = dip->hnl;	
			break;
		case 'd':
			if (dip != d)
				i = dip->dnl; 
			else 
				i = numtab[NL].val;
			break;
		case 'u': 
			i = fi;		
			break;
		case 'j': 
			i = ad + 2 * admod + pa * 70;
			break;
		case 'w': 
			i = widthp;
			break;
		case 'x': 
			if (gflag)
				goto s0;
			i = nel - adspc;
			break;
		case 'y': 
			if (gflag)
				goto s0;
			i = un;		
			break;
		case 'T': 
			i = dotT;		
			break; /*-Tterm used in nroff*/
		case 'V': 
			i = VERT;		
			break;
		case 'H': 
			i = HOR;		
			break;
		case 'k': 
			if ((pa || padj) && pglines == 0) {
				/* fake a value to make -mm work */
				i = pgne % (ll - in);
				if (i == 0 && pgne != 0)
					i = 1;
			} else
				i = ne + adspc;		
			if (gflag) {
				if (ce || rj || !fi ? pendnf : pendw != NULL)
					i += wne - wsp;
				else if (nwd || pgchars) {
					i += sps;
					if (seflg || spflg)
						i += ses;
				}
			}
			break;
		case 'P': 
			i = print;		
			break;
		case 'L': 
			i = ls;		
			break;
		case 'R': 
			i = NN - regcnt;	
			break;
		case 'z': 
			TMYES;
			cpushback(macname(dip->curd));
			return(0);
		case 'b': 
			i = bdtab[font];
			break;
		case 'F':
			TMYES;
			cpushback(cfname[ifi] ? cfname[ifi] : "");
			return(0);
		case 'S':
			TMYES;
			print_tab_setting(tb, sizeof(tb));
			return(0);
		case 'X':
			if (xflag) {
				i = xflag;
				break;
			}
			/*FALLTHRU*/
		case 'Y':
			if (xflag) {
				TMYES;
				cpushback((char *)revision);
				return(0);
			}
			/*FALLTHRU*/

		default:
			goto s0;
		}
	else if (name[0] == '.') {
		if (strcmp(&name[1], "warn") == 0)
			i = warn;
		else if (strcmp(&name[1], "vpt") == 0)
			i = vpt;
		else if (strcmp(&name[1], "ascender") == 0)
			i = getascender();
		else if (strcmp(&name[1], "descender") == 0)
			i = getdescender();
		else if (strcmp(&name[1], "fp") == 0)
			i = nextfp();
		else if (strcmp(&name[1], "ss") == 0)
			i = spacesz;
		else if (strcmp(&name[1], "sss") == 0)
			i = sesspsz;
		else if (strcmp(&name[1], "minss") == 0)
			i = minspsz ? minspsz : spacesz;
		else if (strcmp(&name[1], "lshmin") == 0) {
			i = fl = 100 - lshmin / (LAFACT/100.0);
			if (i != fl)
				goto flt;
		} else if (strcmp(&name[1], "lshmax") == 0) {
			i = fl = 100 + lshmax / (LAFACT/100.0);
			if (i != fl)
				goto flt;
		} else if (strcmp(&name[1], "lspmin") == 0) {
			i = fl = 100 - lspmin / (LAFACT/100.0);
			if (i != fl)
				goto flt;
		} else if (strcmp(&name[1], "lspmax") == 0) {
			i = fl = 100 + lspmax / (LAFACT/100.0);
			if (i != fl)
				goto flt;
		} else if (strcmp(&name[1], "letss") == 0)
			i = letspsz;
		else if (strcmp(&name[1], "hlm") == 0)
			i = hlm;
		else if (strcmp(&name[1], "hlc") == 0)
			i = hlc;
		else if (strcmp(&name[1], "lc_ctype") == 0) {
			if ((cp = setlocale(LC_CTYPE, NULL)) == NULL)
				cp = "C";
			TMYES;
			cpushback(cp);
			return(0);
		} else if (strcmp(&name[1], "hylang") == 0) {
			TMYES;
			if (hylang)
				cpushback(hylang);
			return(0);
		} else if (strcmp(&name[1], "fzoom") == 0) {
			i = fl = getfzoom();
			if (i != fl)
				goto flt;
		} else if (strcmp(&name[1], "sentchar") == 0) {
			TMYES;
			if (sentch[0] == IMP)
				/*EMPTY*/;
			else if (sentch[0] == 0)
				cpushback(".?!:");
			else {
				tchar	tc[NSENT+1];
				for (i = 0; sentch[i] && i < NSENT; i++)
					tc[i] = sentch[i];
				tc[i] = 0;
				pushback(tc);
			}
			return(0);
		} else if (strcmp(&name[1], "transchar") == 0) {
			tchar	tc[NSENT+1];
			TMYES;
			if (transch[0] == IMP)
				/*EMPTY*/;
			else if (transch[0] == 0) {
				cpushback("\"')]*");
				tc[0] = DAGGER;
				tc[1] = 0;
				pushback(tc);
			} else {
				for (i = 0; transch[i] && i < NSENT; i++)
					tc[i] = transch[i];
				tc[i] = 0;
				pushback(tc);
			}
			return(0);
		} else if (strcmp(&name[1], "breakchar") == 0) {
			tchar	tc[NSENT+1];
			TMYES;
			if (breakch[0] == IMP)
				/*EMPTY*/;
			else if (breakch[0] == 0) {
				tc[0] = EMDASH;
				tc[1] = '-';
				tc[2] = 0;
				pushback(tc);
			} else {
				for (i = 0; breakch[i] && i < NSENT; i++)
					tc[i] = breakch[i];
				tc[i] = 0;
				pushback(tc);
			}
			return(0);
		} else if (strcmp(&name[1], "nhychar") == 0) {
			tchar	tc[NSENT+1];
			TMYES;
			if (nhych[0] == IMP)
				/*EMPTY*/;
			else if (nhych[0] == 0) {
				if (!hyext) {
					tc[0] = EMDASH;
					tc[1] = '-';
					tc[2] = 0;
					pushback(tc);
				}
			} else {
				for (i = 0; nhych[i] && i < NSENT; i++)
					tc[i] = nhych[i];
				tc[i] = 0;
				pushback(tc);
			}
			return(0);
		} else if (strcmp(&name[1], "connectchar") == 0) {
			tchar	tc[NSENT+1];
			TMYES;
			if (connectch[0] == IMP)
				/*EMPTY*/;
			else if (connectch[0] == 0) {
				cpushback("\"\\(ru\\(ul\\(rn");
			} else {
				for (i = 0; connectch[i] && i < NSENT; i++)
					tc[i] = connectch[i];
				tc[i] = 0;
				pushback(tc);
			}
			return(0);
		} else if (strcmp(&name[1], "shc") == 0) {
			tchar	tc[2];
			TMYES;
			tc[0] = shc ? shc : HYPHEN;
			tc[1] = 0;
			pushback(tc);
			return(0);
		} else if (strcmp(&name[1], "hylen") == 0) {
			i = hylen;
		} else if (strcmp(&name[1], "hypp") == 0) {
			i = hypp;
		} else if (strcmp(&name[1], "hypp2") == 0) {
			i = hypp2;
		} else if (strcmp(&name[1], "hypp3") == 0) {
			i = hypp3;
		} else if (strcmp(&name[1], "padj") == 0) {
			i = padj;
		} else if (strcmp(&name[1], "ev") == 0) {
			TMYES;
			cpushback(evname ? evname : "0");
			return(0);
		} else if (strcmp(&name[1], "ps") == 0) {
			i = pts;
#ifdef	NROFF
			i *= INCH / 72;
#endif	/* NROFF */
		} else if (strcmp(&name[1], "tabs") == 0) {
			TMYES;
			print_tab_setting(tb, sizeof(tb));
			return(0);
		} else if (strcmp(&name[1], "lpfx") == 0) {
			TMYES;
			if (lpfx)
				pushback(lpfx);
			return(0);
		} else if (strcmp(&name[1], "ce") == 0)
			i = ce;
		else if (strcmp(&name[1], "rj") == 0)
			i = rj;
		else if (strcmp(&name[1], "brnl") == 0)
			i = brnl;
		else if (strcmp(&name[1], "brpnl") == 0)
			i = brpnl;
		else if (strcmp(&name[1], "cht") == 0)
			i = cht;
		else if (strcmp(&name[1], "cdp") == 0)
			i = cdp;
		else if (strcmp(&name[1], "in") == 0)
			i = un;
		else if (strcmp(&name[1], "hy") == 0)
			i = hyf;
		else if (strcmp(&name[1], "int") == 0)
			i = ce || rj || !fi ? pendnf : pendw != NULL;
		else if (strcmp(&name[1], "lt") == 0)
			i = lt;
		else if (strcmp(&name[1], "pn") == 0)
			i = npnflg ? npn : numtab[PN].val + 1;
		else if (strcmp(&name[1], "psr") == 0) {
			i = apts;
#ifdef	NROFF
			i *= INCH / 72;
#endif	/* NROFF */
		} else if (strcmp(&name[1], "sr") == 0) {
			i = fl = u2pts(apts);
			if (i != fl)
				goto flt;
		} else if (strcmp(&name[1], "kc") == 0)
			i = wne - wsp;
		else if (strcmp(&name[1], "dilev") == 0)
			i = dilev;
		else if (strcmp(&name[1], "defpenalty") == 0)
			i = dpenal ? dpenal - INFPENALTY0 - 1 : 0;
		else if (strcmp(&name[1], "ns") == 0)
			i = dip->nls;
		else
			goto s0;
	} else if (strcmp(name, "lss") == 0)
		i = glss;
	else if (strcmp(name, "lsn") == 0)
		i = lsn;
	else {
s0:
		TMNO;
		if ((numtp = _findr(i, 1, 2, 0, &j)) == NULL) {
			if (j < 0) {
				i = -j;
				goto sl;
			}
			i = 0;
		} else if (numtp->fmt == -1) {
			fl = numtp->fval = numtp->fval + numtp->finc*f;
			if (numtp->finc)
				prwatchn(numtp);
			goto flt;
		} else {
			i = numtp->val = numtp->val + numtp->inc*f;
			if (numtp->inc)
				prwatchn(numtp);
			nform = numtp->fmt;
		}
	}
	TMYES;
	setn1(i, nform, (tchar) 0);
	return(0);
flt:
	TMYES;
	roff_sprintf(tb, sizeof(tb), "%g", fl);
	cpushback(tb);
	return(0);
}

void
setn(void)
{
	_setn(0);
}

tchar	numbuf[17];
tchar	*numbufp;

int 
wrc(tchar i)
{
	if (numbufp >= &numbuf[16])
		return(0);
	*numbufp++ = i;
	return(1);
}



/* insert into input number i, in format form, with size-font bits bits */
void
setn1(int i, int form, tchar bits)
{
	numbufp = numbuf;
	nrbits = bits;
	nform = form;
	fnumb(i, wrc);
	*numbufp = 0;
	pushback(numbuf);
}


static void
nrehash(struct numtab *numtp, int n, struct numtab **hashp)
{
	register struct numtab *p;
	register int i;

	for (i=0; i<128; i++)
		hashp[i] = 0;
	for (p=numtp; p < &numtp[n]; p++)
		p->link = 0;
	for (p=numtp; p < &numtp[n]; p++) {
		if (p->r == 0)
			continue;
		i = NHASH(p->r);
		p->link = hashp[i];
		hashp[i] = p;
	}
}

void
nunhash(register struct numtab *rp)
{	
	register struct numtab *p;
	register struct numtab **lp;
	struct numtab	**hashp;
	struct s	*sp;

	if (rp->r == 0)
		return;
	if (rp >= numtab && rp < &numtab[NN])
		hashp = nhash;
	else {
		sp = macframe();
		if (rp >= sp->numtab && rp < &sp->numtab[sp->NN])
			hashp = sp->nhash;
		else
			return;
	}
	lp = &hashp[NHASH(rp->r)];
	p = *lp;
	while (p) {
		if (p == rp) {
			*lp = p->link;
			p->link = 0;
			return;
		}
		lp = &p->link;
		p = p->link;
	}
}


/*
 * Note: Pointers returned by findr(), usedr(), etc. may
 * become invalid after a following call to getch() or another
 * call to findr() since these may result in a number register
 * creation and following grownumtab().
 */
struct numtab *
findr(int i)
{
	return _findr(i, 0, 1, 0, NULL);
}

static struct numtab *
findr1(struct numtab **numtp, int *NNp, struct numtab ***hashp,
		register int i, int rd, int aln, int create, int *rop)
{
	register struct numtab *p;
	register int h = NHASH(i);

	if (rop)
		*rop = 0;
	if (i == 0 || i == -2)
		return(NULL);
	if (*hashp != NULL) {
		for (p = (*hashp)[h]; p; p = p->link)
			if (i == p->r) {
				if (p->aln < 0) {
					if (aln > 1) {
						if (rop)
							*rop = p->aln;
						return(NULL);
					} else if (aln)
						continue;
				}
				if (aln && p->aln > 0)
					return(&(*numtp)[p->aln - 1]);
				return(p);
			}
	}
	if (create) {
		if (rd && warn & WARN_REG)
			errprint("no such register %s", macname(i));
		do {
			for (p = *numtp; p < &(*numtp)[*NNp]; p++) {
				if (p->r == 0) {
					p->r = i;
					p->link = (*hashp)[h];
					(*hashp)[h] = p;
					regcnt++;
					if (*numtp != numtab)
						p->flags |= FLAG_LOCAL;
					return(p);
				}
			}
		} while (p == &(*numtp)[*NNp] &&
				_grownumtab(numtp, NNp, hashp) != NULL);
		errprint("too many number registers (%d).", *NNp);
		done2(04); 
	}
	return(NULL);
}

static struct numtab *
_findr(register int i, int rd, int aln, int forcecreate, int *rop)
{
	struct s	*sp;
	struct numtab	*numtp;

	if ((sp = macframe()) != stk) {
		if ((numtp = findr1(&sp->numtab, &sp->NN, &sp->nhash,
				i, rd, aln, forcecreate == 1, rop)) != NULL)
			return numtp;
	}
	return findr1(&numtab, &NN, &nhash, i, rd, aln, forcecreate >= 0, rop);
}


static struct numtab *
_usedr (	/* returns NULL if nr i has never been used */
    register int i, int aln, int *rop
)
{
	return _findr(i, 0, aln, -1, rop);
}


struct numtab *
usedr(int i)
{
	return _usedr(i, 1, NULL);
}


int 
fnumb(register int i, register int (*f)(tchar))
{
	register int j;

	j = 0;
	if (i < 0) {
		j = (*f)('-' | nrbits);
		i = -i;
	}
	switch (nform) {
	default:
	case '1':
	case 0: 
		return decml(i, f) + j;
		break;
	case 'i':
	case 'I': 
		return roman(i, f) + j;
		break;
	case 'a':
	case 'A': 
		return abc(i, f) + j;
		break;
	}
}


int 
decml(register int i, register int (*f)(tchar))
{
	register int j, k;

	k = 0;
	nform--;
	if ((j = i / 10) || (nform > 0))
		k = decml(j, f);
	return(k + (*f)((i % 10 + '0') | nrbits));
}


int 
roman(int i, int (*f)(tchar))
{

	if (!i)
		return((*f)('0' | nrbits));
	if (nform == 'i')
		return(roman0(i, f, "ixcmz", "vldw"));
	else 
		return(roman0(i, f, "IXCMZ", "VLDW"));
}


int 
roman0(int i, int (*f)(tchar), char *onesp, char *fivesp)
{
	register int q, rem, k;

	k = 0;
	if (!i)
		return(0);
	k = roman0(i / 10, f, onesp + 1, fivesp + 1);
	q = (i = i % 10) / 5;
	rem = i % 5;
	if (rem == 4) {
		k += (*f)(*onesp | nrbits);
		if (q)
			i = *(onesp + 1);
		else 
			i = *fivesp;
		return(k += (*f)(i | nrbits));
	}
	if (q)
		k += (*f)(*fivesp | nrbits);
	while (--rem >= 0)
		k += (*f)(*onesp | nrbits);
	return(k);
}


int 
abc(int i, int (*f)(tchar))
{
	if (!i)
		return((*f)('0' | nrbits));
	else 
		return(abc0(i - 1, f));
}


int 
abc0(int i, int (*f)(tchar))
{
	register int j, k;

	k = 0;
	if ((j = i / 26))
		k = abc0(j - 1, f);
	return(k + (*f)((i % 26 + nform) | nrbits));
}

static int	illscale;
static int	parlevel;
static int	whitexpr;
static int	empty;

static tchar
agetch(void)
{
	tchar	c;

	for (;;) {
		c = getch();
		if (xflag == 0 || parlevel == 0 || cbits(c) != ' ')
			break;
		whitexpr++;
	}
	return c;
}

int
hatoi(void)
{
	struct acc	a;

	lgf++;
	a = _atoi(0);
	lgf--;
	return a.n;
}

float
atof(void)
{
	struct acc	a;

	lgf++;
	a = _atoi(1);
	lgf--;
	return a.f;
}

static struct acc
_atoi(int flt)
{
	struct acc	n;
	int	c;

	illscale = 0;
	whitexpr = parlevel = 0;
	empty = 1;
	n = _atoi0(flt);
	c = cbits(ch);
	if (c == RIGHT) {
		if (!empty && (nonumb || parlevel) && warn & WARN_RIGHT_BRACE)
			errprint("\\} terminates numerical expression");
	} else if (nonumb && c && c != ' ' && c != '\n' &&
			warn & WARN_NUMBER && illscale == 0) {
		if (c == 'T' && Tflg)
			/*EMPTY*/;
		else if ((c & ~0177) == 0 && isprint(c))
			errprint("illegal number, char %c", c);
		else
			errprint("illegal number");
	} else if (warn & WARN_SYNTAX) {
		if (parlevel > 0)
			errprint("missing ')'");
		if (parlevel < 0)
			errprint("excess ')'");
		if (xflag && whitexpr && parlevel)
			nonumb = 1;
	}
	if (flt) {
		if (!nonumb && ((n.f<0 && n.f<-FLT_MAX) || (n.f>0 && n.f>FLT_MAX))) {
			if (warn & WARN_NUMBER)
				errprint("floating-point arithmetic overflow");
			if (xflag)
				nonumb = 1;
		}
	} else {
		if (!nonumb && ((n.n<0 && n.n <INT_MIN) || (n.n>0 && n.n>INT_MAX))) {
			if (warn & WARN_NUMBER)
				errprint("arithmetic overflow");
			if (xflag)
				nonumb = 1;
		}
	}
	return n;
}

long long
atoi0(void)
{
	struct acc	a;

	whitexpr = parlevel = 0;
	lgf++;
	a = _atoi0(0);
	lgf--;
	return a.n;
}

double
atof0(void)
{
	struct acc	a;

	whitexpr = parlevel = 0;
	lgf++;
	a = _atoi0(0);
	lgf--;
	return a.f;
}

static struct acc
_atoi0(int flt)
{
	register int c, k, cnt;
	register tchar ii;
	struct acc	i, acc;

	noschr++;
	i.f = i.n = 0; 
	acc.f = acc.n = 0;
	nonumb = 0;
	cnt = -1;
a0:
	cnt++;
	ii = agetch();
	c = cbits(ii);
	switch (c) {
	default:
		ch = ii;
		if (cnt)
			break;
	case '+':
		i = ckph(flt);
		if (nonumb)
			break;
		acc.n += i.n;
		if (flt) acc.f += i.f;
		goto a0;
	case '-':
		i = ckph(flt);
		if (nonumb)
			break;
		acc.n -= i.n;
		if (flt) acc.f -= i.f;
		goto a0;
	case '*':
		i = ckph(flt);
		if (nonumb)
			break;
		if (!flt) acc.n *= i.n;
		if (flt) acc.f *= i.f;
		goto a0;
	case '/':
		i = ckph(flt);
		if (nonumb)
			break;
		if ((!flt && i.n == 0) || (flt && i.f == 0)) {
			flusho();
			errprint("divide by zero.");
			acc.n = 0;
			if (flt) acc.f = 0;
		} else  {
			if (!flt) acc.n /= i.n;
			if (flt) acc.f /= i.f;
		}
		goto a0;
	case '%':
		i = ckph(flt);
		if (nonumb)
			break;
		if (flt) acc.n = acc.f, i.n = i.f;
		if (i.n != 0)
			acc.n %= i.n;
		else if (warn & WARN_RANGE)
			errprint("modulo by zero.");
		if (flt) acc.f = acc.n;
		goto a0;
	case '&':	/*and*/
		i = ckph(flt);
		if (nonumb)
			break;
		if ((acc.n > 0) && (i.n > 0))
			acc.n = 1; 
		else 
			acc.n = 0;
		if (flt) acc.f = acc.n;
		goto a0;
	case ':':	/*or*/
		i = ckph(flt);
		if (nonumb)
			break;
		if ((acc.n > 0) || (i.n > 0))
			acc.n = 1; 
		else 
			acc.n = 0;
		if (flt) acc.f = acc.n;
		goto a0;
	case '=':
		if (cbits(ii = getch()) != '=')
			ch = ii;
		i = ckph(flt);
		if (nonumb) {
			acc.n = 0; 
			if (flt) acc.f = 0;
			break;
		}
		acc.n = i.n == acc.n;
		if (flt) acc.f = i.f == acc.f;
		goto a0;
	case '>':
		k = 0;
		if (cbits(ii = getch()) == '=')
			k++; 
		else if (xflag && cbits(ii) == '?')
			goto maximum;
		else 
			ch = ii;
		i = ckph(flt);
		if (nonumb) {
			acc.n = 0; 
			if (flt) acc.f = 0;
			break;
		}
		if (!flt) {
			if (acc.n > (i.n - k))
				acc.n = 1; 
			else 
				acc.n = 0;
		} else
			acc.f = k ? acc.f >= i.f : acc.f > i.f;
		goto a0;
	maximum:
		i = ckph(flt);
		if (nonumb) {
			acc.n = 0; 
			if (flt) acc.f = 0;
			break;
		}
		if (i.n > acc.n)
			acc.n = i.n;
		if (flt && i.f > acc.f)
			acc.f = i.f;
		goto a0;
	case '<':
		k = 0;
		if (cbits(ii = getch()) == '=')
			k++; 
		else if (xflag && cbits(ii) == '?')
			goto minimum;
		else if (xflag && cbits(ii) == '>')
			goto notequal;
		else 
			ch = ii;
		i = ckph(flt);
		if (nonumb) {
			acc.n = 0; 
			if (flt) acc.f = 0;
			break;
		}
		if (!flt) {
			if (acc.n < (i.n + k))
				acc.n = 1; 
			else 
				acc.n = 0;
		} else
			acc.f = k ? acc.f <= i.f : acc.f < i.f;
		goto a0;
	minimum:
		i = ckph(flt);
		if (nonumb) {
			acc.n = 0; 
			if (flt) acc.f = 0;
			break;
		}
		if (i.n < acc.n)
			acc.n = i.n;
		if (flt && i.f < acc.f)
			acc.f = i.f;
		goto a0;
	notequal:
		i = ckph(flt);
		if (nonumb) {
			acc.n = 0; 
			if (flt) acc.f = 0;
			break;
		}
		acc.n = i.n != acc.n;
		if (flt) acc.f = i.f != acc.f;
		goto a0;
	case ')': 
		parlevel--;
		break;
	case '(':
		parlevel++;
		acc = _atoi0(flt);
		goto a0;
	}
	if (noschr) noschr--;
	return(acc);
}


static struct acc
ckph(int flt)
{
	register tchar i;
	struct acc	j;

	if (cbits(i = agetch()) == '(') {
		parlevel++;
		j = _atoi0(flt);
	} else {
		j = atoi1(i, flt);
	}
	return(j);
}


static struct acc
atoi1(register tchar ii, int flt)
{
	register int i, j, digits;
	int	digit = 0;
	struct acc	acc;
	int	neg, abs, field;
	int	_noscale = 0, scale;
	double	e, f;

	neg = abs = field = digits = 0;
	acc.f = acc.n = 0;
	for (;;) {
		i = cbits(ii);
		switch (i) {
		default:
			break;
		case '+':
			ii = agetch();
			continue;
		case '-':
			neg = 1;
			ii = agetch();
			continue;
		case '|':
			abs = 1 + neg;
			neg = 0;
			ii = agetch();
			continue;
		}
		break;
	}
	if (xflag && i == '(') {
		parlevel++;
		acc = _atoi0(flt);
		e = 1;
		i = j = 1;
		field = -1;
		goto aa;
	}
a1:
	while (i >= '0' && i <= '9') {
		field++;
		digits++;
		digit = 1;
		if (!flt)
			acc.n = 10 * acc.n + i - '0';
		else if (field == digits)
			acc.f = 10 * acc.f + i - '0';
		else {
			f = i - '0';
			for (j = 0; j < digits; j++)
				f /= 10;
			acc.f += f;
		}
		ii = getch();
		i = cbits(ii);
	}
	if (i == '.') {
		field++;
		digits = 0;
		ii = getch();
		i = cbits(ii);
		goto a1;
	}
	e = 1;
	if (xflag && digits && (i == 'e' || i == 'E')) {
		if ((i = cbits(ii = getch())) == '+')
			j = 1;
		else if (i == '-')
			j = -1;
		else if (i >= '0' && i <= '9') {
			j = 1;
			ch = ii;
		} else {
			ch = ii;
			field = 0;
			goto a2;
		}
		f = 0;
		while ((i = cbits(ii = getch())) >= '0' && i <= '9')
			f = f * 10 + i - '0';
		while (f-- > 0)
			e *= 10;
		if (j < 0)
			e = 1/e;
	}
	if ((!xflag || !parlevel) && !field) {
		ch = ii;
		goto a2;
	}
	switch (scale = i) {
	case 's':
		if (!xflag)
			goto dfl;
		/*FALLTHRU*/
	case 'u':
		i = j = 1;	/* should this be related to HOR?? */
		break;
	case 'v':	/*VSs - vert spacing*/
		j = lss;
		i = 1;
		break;
	case 'm':	/*Ems*/
		j = EM;
		i = 1;
		break;
	case 'n':	/*Ens*/
		j = EM;
#ifndef NROFF
		i = 2;
#endif
#ifdef NROFF
		i = 1;	/*Same as Ems in NROFF*/
#endif
		break;
	case 'z':
		if (!xflag)
			goto dfl;
		/*FALLTHRU*/
	case 'p':	/*Points*/
		j = INCH;
		i = 72;
		break;
	case 'i':	/*Inches*/
		j = INCH;
		i = 1;
		break;
	case 'c':	/*Centimeters*/
		/* if INCH is too big, this will overflow */
		j = INCH * 50;
		i = 127;
		break;
	case 'P':	/*Picas*/
		j = INCH;
		i = 6;
		break;
	case 'D':	/* Didot points */
		if (!xflag)
			goto dfl;
		j = INCH * 24;	/* following H. R. Bosshard, */
		i = 1621;	/* Technische Grundlagen zur */
		break;		/* Satzherstellung, Berne 1980, p. 17 */
	case 'C':	/* Cicero */
		if (!xflag)
			goto dfl;
		j = INCH * 24 * 12;
		i = 1621;
		break;
	case 't':	/* printer's points */
		if (!xflag)
			goto dfl;
		j = INCH * 100;	/* following Knuth */
		i = 7227;
		break;
	case 'T':	/* printer's picas */
		if (!xflag)
			goto dfl;
		j = INCH * 100 * 4;
		i = 2409;
		break;
	case 'M':	/*Ems/100*/
		if (!xflag)
			goto dfl;
		j = EM;
		i = 100;
		break;
	case ';':
		if (!xflag)
			goto dfl;
		i = j = 1;
		_noscale = 1;
		goto newscale;
	default:
	dfl:	if (!field) {
			ch = ii;
			goto a2;
		}
		if (((i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z')) &&
				warn & WARN_SCALE) {
			errprint("undefined scale indicator %c", i);
			illscale = 1;
		} else
			scale = 0;
		j = dfact;
		ch = ii;
		i = dfactd;
	}
	if (!field) {
		tchar	t, tp[2];
		int	f, d, n;
		t = getch();
		if (cbits(t) != ';') {
			tp[0] = t;
			tp[1] = 0;
			pushback(tp);
			ch = ii;
			goto a2;
		}
	newscale:
		/* (c;e) */
		f = dfact;
		d = dfactd;
		n = noscale;
		dfact = j;
		dfactd = i;
		noscale = _noscale;
		acc = _atoi0(flt);
		dfact = f;
		dfactd = d;
		noscale = n;
		return(acc);
	}
	if (noscale && scale && warn & WARN_SYNTAX)
		errprint("ignoring scale indicator %c", scale);
aa:
	if (neg) {
		acc.n = -acc.n;
		if (flt) acc.f = -acc.f;
	}
	if (!noscale) {
		if (!flt) acc.n = (acc.n * j) / i;
		if (flt) acc.f = (acc.f * j) / i;
	}
	if (!flt && (field != digits) && (digits > 0)) {
		if (e != 1) acc.n = e * acc.n;
		while (digits--)
			acc.n /= 10;
	} else if (e != 1) {
		if (!flt) acc.n = (int)(e * acc.n);
		if (flt) acc.f *= e;
	}
	if (abs) {
		if (dip != d)
			j = dip->dnl; 
		else 
			j = numtab[NL].val;
		if (!vflag) {
			j = numtab[HP].val;
		}
		if (abs == 2)
			j = -j;
		acc.n -= j;
		if (flt) acc.f -= j;
	}
a2:
	nonumb = !field || !digit;
	if (empty)
		empty = !field;
	return(acc);
}


void
setnr(const char *name, int val, int inc)
{
	int	j;
	struct numtab	*numtp;

	if ((j = makerq(name)) < 0)
		return;
	if ((numtp = findr(j)) == NULL)
		return;
	numtp->val = val;
	numtp->inc = inc;
	if (numtp->fmt == -1)
		numtp->fmt = 0;
	prwatchn(numtp);
}

void
setnrf(const char *name, float val, float inc)
{
	int	j;
	struct numtab	*numtp;

	if ((j = makerq(name)) < 0)
		return;
	if ((numtp = findr(j)) == NULL)
		return;
	numtp->val = numtp->fval = val;
	numtp->inc = numtp->finc = inc;
	numtp->fmt = -1;
	prwatchn(numtp);
}


static void
clrnr(struct numtab *p)
{
	if (p != NULL) {
		memset(p, 0, sizeof *p);
		regcnt--;
	}
}

void
caserr(void)
{
	register int i;
	struct numtab	*p, *kp;
	int cnt = 0;

	lgf++;
	while (!skip(!cnt++) && (i = getrq(2)) ) {
		p = _usedr(i, 0, NULL);
		if (p == NULL)
			continue;
		nunhash(p);
		if (p->aln && (kp = _usedr(i, 1, NULL)) != NULL) {
			if (--kp->nlink <= 0)
				clrnr(kp);
		}
		if (p->flags & FLAG_WATCH)
			errprint("%s: removing %sregister %s", macname(lastrq),
					p->flags & FLAG_LOCAL ? "local " : "",
					macname(i));
		if (p->nlink > 0)
			p->nlink--;
		if (p->nlink == 0)
			clrnr(p);
		else
			p->r = -1;
	}
}


void
casernn(void)
{
	int	i, j, flags;
	struct numtab	*kp, *numtp;

	lgf++;
	skip(1);
	if ((i = getrq(0)) == 0)
		return;
	skip(1);
	j = getrq(1);
	if ((kp = _usedr(i, 0, NULL)) == NULL) {
		if (warn & WARN_REG)
			errprint("no such register %s", macname(i));
		return;
	}
	flags = kp->flags;
	numtp = _findr(j, 0, 0, flags & FLAG_LOCAL, NULL);
	if (numtp != NULL) {
		if (numtp->nlink) {
			numtp->nlink--;
			numtp->r = -1;
		}
		numtp = _findr(j, 0, 0, flags & FLAG_LOCAL, NULL);
	}
	kp = _usedr(i, 0, NULL);
	if (numtp != NULL) {
		*numtp = *kp;
		numtp->r = j;
	}
	clrnr(kp);
	if (numtp->flags & FLAG_WATCH)
		errprint("%s: renaming %sregister %s to %s", macname(lastrq),
				numtp->flags & FLAG_LOCAL ? "local " : "",
				macname(i), macname(j));
}


void
setr(void)
{
	int	termc, j, rq;
	struct numtab	*numtp;

	lgf++;
	termc = getach();
	rq = getrq(3);
	lgf--;
	if (skip(1) || (numtp = findr(rq)) == NULL)
		return;
	j = inumb(&numtp->val);
	if (nonumb)
		return;
	if (getach() != termc) {
		nodelim(termc);
		return;
	}
	numtp = findr(rq);	/* twice because of getch() before */
	numtp->val = j;
	if (numtp->fmt == -1)
		numtp->fmt = 0;
	prwatchn(numtp);
}

static void
casnr1(int flt, int local)
{
	register int j, rq;
	struct acc	a;
	struct numtab	*numtp;

	lgf++;
	skip(1);
	rq = getrq(3);
	skip(!local);
	if ((numtp = _findr(rq, 0, 1, local, NULL)) == NULL)
		goto rtn;
	a = _inumb(&numtp->val, flt ? &numtp->fval : NULL, flt, NULL);
	if (nonumb)
		goto rtn;
	if (rq == PAIR('.', 'g')) gemu = xflag && a.n == 1 ? 1 : 0;
	numtp->val = a.n;
	if (flt) {
		numtp->fval = a.f;
		numtp->fmt = -1;
	} else if (numtp->fmt == -1)
		numtp->fmt = 0;
	/*
	 * It is common use in pre-processors and macro packages
	 * to append a unit definition to a user-supplied number
	 * in order to achieve a default scale. Catch this case
	 * now to avoid a warning because of an illegal number.
	 */
	j = cbits(ch);
	if (((j >= 'a' && j <= 'z') || (j >= 'A' && j <= 'Z')) &&
			warn & WARN_SCALE)
		goto rtns;
	skip(0);
	a = _atoi(flt);
	if (nonumb)
		goto rtns;
	numtp = _findr(rq, 0, 1, local, NULL);
	numtp->inc = a.n;
	if (flt)
		numtp->finc = a.f;
rtns:
	prwatchn(numtp);
rtn:
	return;
}

void
casenr(void)
{
	casnr1(0, 0);
}

void
casenrf(void)
{
	casnr1(1, 0);
}

void
caselnr(void)
{
	casnr1(0, macframe() != stk);
}

void
caselnrf(void)
{
	casnr1(1, macframe() != stk);
}


void
caseaf(void)
{
	register int i, k;
	register tchar j, jj;
	struct numtab	*numtp;

	lgf++;
	if (skip(1) || !(i = getrq(3)))
		return;
	if (skip(1))
		return;
	k = 0;
	j = getch();
	if (!ischar(jj = cbits(j)) || !isalpha(jj)) {
		ch = j;
		while ((j = cbits(getch())) >= '0' &&  j <= '9')
			k++;
	}
	if (!k)
		k = j;
	numtp = findr(i);
	if (numtp->fmt == -1) {
		if (warn & WARN_RANGE)
			errprint("cannot change format of floating-point register");
		return;
	}
	numtp->fmt = k & BYTEMASK;
	if (numtp->flags & FLAG_WATCH) {
		char	b[40];
		int	x;
		if ((k & BYTEMASK) > ' ') {
			b[0] = k & BYTEMASK;
			b[1] = 0;
		} else {
			for (x = 0; x < k; x++)
				b[x] = '0';
			b[x] = 0;
		}
		errprint("%s: format of %sregister %s set to %s",
				macname(lastrq),
				numtp->flags & FLAG_LOCAL ? "local " : "",
				macname(i), b);
	}
}

void
setaf (void)	/* return format of number register */
{
	register int j;
	struct numtab	*numtp;

	numtp = usedr(getsn(0));
	if (numtp == NULL)
		return;
	if (numtp->fmt > 20)	/* it was probably a, A, i or I */
		pbbuf[pbp++] = numtp->fmt;
	else if (numtp->fmt == -1)
		pbbuf[pbp++] = '0';
	else {
		for (j = (numtp->fmt ? numtp->fmt : 1); j; j--) {
			if (pbp >= pbsize-3)
				if (growpbbuf() == NULL) {
					errprint("no space for .af");
					done(2);
				}
			pbbuf[pbp++] = '0';
		}
	}
}


void
casealn(void)
{
	int	c, i, j, k;
	int	flags, local = 0;
	struct numtab	*op, *rp = NULL;

	if (skip(1))
		return;
	i = getrq(1);
	if (skip(1))
		return;
	j = getrq(1);
	for (c = 0; ; c++) {
		if (((op = _usedr(j, 2, &k)) == NULL)) {
			if (k < -1)
				/*EMPTY*/;
			else if (_setn(j))
				k = -j;
			else {
				if (warn & WARN_REG)
					errprint("no such register %s",
							macname(j));
				return;
			}
		}
		if (c)
			break;
		local = op != NULL && op->flags & FLAG_LOCAL;
		if ((rp = _findr(i, 0, 0, local, NULL)) == NULL)
			return;
	}
	if (op != NULL) {
		rp->aln = op - (local ? macframe()->numtab : numtab) + 1;
		if (op->nlink == 0)
			op->nlink = 1;
		op->nlink++;
	} else
		rp->aln = k;
	flags = rp->flags;
	if (op != NULL)
		flags |= op->flags;
	if (flags & FLAG_WATCH)
		errprint("%s: creating alias %s to %sregister %s",
				macname(lastrq), macname(i),
				op->flags & FLAG_LOCAL ? "local " : "",
				macname(j));
}


void
casewatchn(int unwatch)
{
	int	j;
	struct numtab	*numtp;

	lgf++;
	if (skip(1))
		return;
	do {
		if (!(j = getrq(1)) || (numtp = findr(j)) == NULL)
			break;
		if (unwatch)
			numtp->flags &= ~FLAG_WATCH;
		else
			numtp->flags |= FLAG_WATCH;
	} while (!skip(0));
}


void
caseunwatchn(void)
{
	casewatchn(1);
}


void
prwatchn(struct numtab *numtp)
{
	char	*local;

	if (numtp == NULL)
		return;
	local = numtp->flags & FLAG_LOCAL ? "local " : "";
	if (numtp->flags & FLAG_WATCH) {
		if (numtp->fmt == -1)
			errprint("%s: %sfloating-point register %s set to %g, increment %g",
					macname(lastrq), local,
					macname(numtp->r),
					numtp->fval, numtp->finc);
		else
			errprint("%s: %sregister %s set to %d, increment %d",
					macname(lastrq), local,
					macname(numtp->r),
					numtp->val, numtp->inc);
	}
}


int 
vnumb(int *i)
{
	vflag++;
	dfact = lss;
	res = VERT;
	return(inumb(i));
}


int 
hnumb(int *i)
{
	dfact = EM;
	res = HOR;
	return(inumb(i));
}


int 
inumb(int *n)
{
	struct acc	a;

	a = _inumb(n, NULL, 0, NULL);
	return a.n;
}


int
inumb2(int *n, int *relative)
{
	struct acc	a;

	a = _inumb(n, NULL, 0, relative);
	return a.n;
}

static struct acc
_inumb(int *n, float *fp, int flt, int *relative)
{
	struct acc	i;
	register int j, f;
	register tchar ii;
	int	nv = 0;
	float	fv = 0;

	f = 0;
	lgf++;
	if (n) {
		nv = *n;
		if (fp)
			fv = *fp;
		noschr++;
		if ((j = cbits(ii = getch())) == '+')
			f = 1;
		else if (j == '-')
			f = -1;
		else 
			ch = ii;
		if (noschr) noschr--;
	}
	i = _atoi(flt);
	lgf--;
	if (n && f && !flt)
		i.n = nv + f * i.n;
	if (fp && f && flt)
		i.f = fv + f * i.f;
	if (!flt) i.n = quant(i.n, res);
	vflag = 0;
	res = dfactd = dfact = 1;
	if (nonumb) {
		i.n = 0;
		if (flt) i.f = 0;
	}
	if (relative)
		*relative = f;
	return(i);
}


float
atop(void)
{
	float	t;

	noscale++;
	t = atof();
	noscale--;
	if (t < -INFPENALTY)
		t = -INFPENALTY;
	else if (t > INFPENALTY)
		t = INFPENALTY;
	else
		t *= PENALSCALE;
	return t;
}


int 
quant(int n, int m)
{
	register int i, neg;

	neg = 0;
	if (n < 0) {
		neg++;
		n = -n;
	}
	/* better as i = ((n + (m/2))/m)*m */
	i = n / m;
	if ((n - m * i) > (m / 2))
		i += 1;
	i *= m;
	if (neg)
		i = -i;
	return(i);
}


tchar
moflo(int n)
{
	if (warn & WARN_RANGE)
		errprint("value too large for motion");
	return sabsmot(MAXMOT);
}
