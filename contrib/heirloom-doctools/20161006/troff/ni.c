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
 * Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "ni.c	1.11	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)ni.c	1.47 (gritter) 12/17/06
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

#include "tdef.h"
#include "ext.h"

/* You may want to change these names */

#ifdef NROFF

char	*termtab = TABDIR "/tab.";  /* term type added in ptinit() */
char	*fontfile = "";		/* not used */
char	devname[20] = "37";

#else

char	*termtab = FNTDIR;              /* rest added in ptinit() */
char	*fontfile = FNTDIR;             /* rest added in casefp() */
char	devname[20]	 = "ps";	/* default typesetter */
int	html;

#endif
char	obuf[OBUFSZ];	/* characters collected here for typesetter output */
char	*obufp = obuf;
int	NN;
struct numtab *numtab;
const struct numtab initnumtab[] = {
	{ PAIR('%', 0) },
	{ PAIR('n', 'l') },
	{ PAIR('y', 'r') },
	{ PAIR('h', 'p') },
	{ PAIR('c', 't') },
	{ PAIR('d', 'n') },
	{ PAIR('m', 'o') },
	{ PAIR('d', 'y') },
	{ PAIR('d', 'w') },
	{ PAIR('l', 'n') },
	{ PAIR('d', 'l') },
	{ PAIR('s', 't') },
	{ PAIR('s', 'b') },
	{ PAIR('c', '.') },
	{ PAIR('$', '$') },
	{ 0 }
};


int	pto = 10000;
int	pfrom = 1;
int	print = 1;
char	*nextf;
int	NS;
char	**mfiles;
int	nmfi = 0;
int	NMF;
#ifndef NROFF
int	oldbits = -1;
#endif
int	init = 1;
int	fc = IMP;	/* field character */
int	eschar = '\\';
int	ecs = '\\';
#ifdef	NROFF
int	pl = 11*INCH;
int	po = PO;
#else
int	pl;
int	po;
#endif
int	dfact = 1;
int	dfactd = 1;
int	res = 1;
int	smnt = 0;	/* beginning of special fonts */
int	ascii = ASCII;
int	ptid = PTID;
int	lg = LG;
int	pnlist[NPN] = { -1 };
int	vpt = 1;


int	*pnp = pnlist;
int	npn = 1;
int	npnflg = 1;
int	dpn = -1;
int	totout = 1;
int	ulfont = ULFONT;
int	tabch = TAB;
int	ldrch = LEADER;

extern void	caseft(void), caseps(void), casevs(void), casefp(void),
       		casess(void), casecs(void), casebd(void), caselg(void);

enum warn	warn = WARN_FONT;

int	NM;
struct contab *contab;
#define	C(a,b)	{a, 0, (void(*)(int))b, 0}
const struct contab initcontab[] = {
	C(PAIR('d', 's'), caseds),
	C(PAIR('a', 's'), caseas),
	C(PAIR('s', 'p'), casesp),
	C(PAIR('f', 't'), caseft),
	C(PAIR('p', 's'), caseps),
	C(PAIR('v', 's'), casevs),
	C(PAIR('n', 'r'), casenr),
	C(PAIR('i', 'f'), caseif),
	C(PAIR('i', 'e'), caseie),
	C(PAIR('e', 'l'), caseel),
	C(PAIR('p', 'o'), casepo),
	C(PAIR('t', 'l'), casetl),
	C(PAIR('t', 'm'), casetm),
	C(PAIR('b', 'p'), casebp),
	C(PAIR('c', 'h'), casech),
	C(PAIR('p', 'n'), casepn),
	C(PAIR('b', 'r'), tbreak),
	C(PAIR('t', 'i'), caseti),
	C(PAIR('n', 'e'), casene),
	C(PAIR('n', 'f'), casenf),
	C(PAIR('c', 'e'), casece),
	C(PAIR('r', 'j'), caserj),
	C(PAIR('f', 'i'), casefi),
	C(PAIR('i', 'n'), casein),
	C(PAIR('l', 'l'), casell),
	C(PAIR('n', 's'), casens),
	C(PAIR('m', 'k'), casemk),
	C(PAIR('r', 't'), casert),
	C(PAIR('a', 'm'), caseam),
	C(PAIR('d', 'e'), casede),
	C(PAIR('d', 'i'), casedi),
	C(PAIR('d', 'a'), caseda),
	C(PAIR('w', 'h'), casewh),
	C(PAIR('d', 't'), casedt),
	C(PAIR('i', 't'), caseit),
	C(PAIR('r', 'm'), caserm),
	C(PAIR('r', 'r'), caserr),
	C(PAIR('r', 'n'), casern),
	C(PAIR('a', 'd'), casead),
	C(PAIR('r', 's'), casers),
	C(PAIR('n', 'a'), casena),
	C(PAIR('p', 'l'), casepl),
	C(PAIR('t', 'a'), caseta),
	C(PAIR('t', 'r'), casetr),
	C(PAIR('u', 'l'), caseul),
	C(PAIR('c', 'u'), casecu),
	C(PAIR('l', 't'), caselt),
	C(PAIR('n', 'x'), casenx),
	C(PAIR('s', 'o'), caseso),
	C(PAIR('i', 'g'), caseig),
	C(PAIR('t', 'c'), casetc),
	C(PAIR('f', 'c'), casefc),
	C(PAIR('e', 'c'), caseec),
	C(PAIR('e', 'o'), caseeo),
	C(PAIR('l', 'c'), caselc),
	C(PAIR('e', 'v'), caseev),
	C(PAIR('r', 'd'), caserd),
	C(PAIR('a', 'b'), caseab),
	C(PAIR('f', 'l'), casefl),
	C(PAIR('e', 'x'), done),
	C(PAIR('s', 's'), casess),
	C(PAIR('f', 'p'), casefp),
	C(PAIR('c', 's'), casecs),
	C(PAIR('b', 'd'), casebd),
	C(PAIR('l', 'g'), caselg),
	C(PAIR('h', 'c'), casehc),
	C(PAIR('h', 'y'), casehy),
	C(PAIR('n', 'h'), casenh),
	C(PAIR('n', 'm'), casenm),
	C(PAIR('n', 'n'), casenn),
	C(PAIR('s', 'v'), casesv),
	C(PAIR('o', 's'), caseos),
	C(PAIR('l', 's'), casels),
	C(PAIR('c', 'c'), casecc),
	C(PAIR('c', '2'), casec2),
	C(PAIR('e', 'm'), caseem),
	C(PAIR('a', 'f'), caseaf),
	C(PAIR('h', 'w'), casehw),
	C(PAIR('m', 'c'), casemc),
	C(PAIR('p', 'm'), casepm),
	C(PAIR('p', 'i'), casepi),
	C(PAIR('u', 'f'), caseuf),
	C(PAIR('p', 'c'), casepc),
	C(PAIR('h', 't'), caseht),
	C(PAIR('c', 'f'), casecf),
	C(PAIR('s', 'y'), casesy),
	C(PAIR('l', 'f'), caself),
	C(PAIR('d', 'b'), casedb),
/*	C(PAIR('!', 0), casesy), */	/* synonym for .sy */
	C(PAIR(XFUNC, 0), caseif),	/* while loop execution */
	C(PAIR('c', 'p'), casecp),
	C(0,              0)
};


tchar *oline;

/*
 * troff environment block
 */

struct	env env = {
/* int	ics	 */	0,
/* int	sps	 */	0,
/* int	ses	 */	0,
/* int	spacesz	 */	0,
/* int	sesspsz  */	0,
#ifndef	NROFF
/* int	minsps	 */	0,
/* int	minspsz  */	0,
/* int	letspsz	 */	0,
/* int	letsps	 */	0,
/* int	lspmin	 */	0,
/* int	lspmax	 */	0,
/* int	lspnc	 */	0,
/* int	lsplow	 */	0,
/* int	lsphigh	 */	0,
/* int	lspcur	 */	0,
/* int	lsplast	 */	0,
/* int	lshmin	 */	0,
/* int	lshmax	 */	0,
/* int	lshwid	 */	0,
/* int	lshlow	 */	0,
/* int	lshhigh	 */	0,
/* int	lshcur	 */	0,
#endif	/* !NROFF */
/* int	fldcnt	 */	0,
/* int	lss	 */	0,
/* int	lss1	 */	0,
/* int	ll	 */	0,
/* int	ll1	 */	0,
/* int	lt	 */	0,
/* int	lt1	 */	0,
/* tchar i	*/	0, 	/* insertion character */
/* int	icf	 */	0,
/* tchar	chbits	 */	0,	/* size+font bits for current character */
/* tchar	spbits	 */	0,
/* tchar	nmbits	 */	0,
/* int	apts	 */	PS,	/* actual point size -- as requested by user */
/* int	apts1	 */	PS,	/* need not match an existent size */
/* int	pts	 */	PS,	/* hence, this is the size that really exists */
/* int	pts1	 */	PS,
/* int	font	 */	FT,
/* int	font1	 */	FT,
/* int	ls	 */	1,
/* int	ls1	 */	1,
/* int	ad	 */	1,
/* int	nms	 */	1,
/* int	ndf	 */	1,
/* int	fi	 */	1,
/* int	cc	 */	'.',
/* int	c2	 */	'\'',
/* int	ohc	 */	OHC,
/* int	tdelim	 */	IMP,
/* int	hyf	 */	1,
/* int	hyoff	 */	0,
/* int	hlm	 */	-1,
/* int	hlc	 */	0,
/* int	hylen	 */	5,
/* float hypp	 */	0,
/* float hypp2	 */	0,
/* float hypp3	 */	0,
/* int	un1	 */	-1,
/* int	tabc	 */	0,
/* int	dotc	 */	'.',
/* int	adsp	 */	0,
/* int	adrem	 */	0,
/* int	lastl	 */	0,
/* int	nel	 */	0,
/* int	admod	 */	0,
/* int	adflg	 */	0,
/* int	adspc	 */	0,
/* int	pa	 */	0,
/* tchar	*wordp	 */	0,
/* int	spflg	 */	0,	/* probably to indicate space after punctuation needed */
/* int	seflg	 */	0,
/* tchar	*linep	 */	0,
/* tchar	*wdend	 */	0,
/* tchar	*wdstart	 */	0,
/* int	wne	 */	0,
/* int	wsp	 */	0,
/* int	ne	 */	0,
/* int	nc	 */	0,
/* int	nb	 */	0,
/* int	lnmod	 */	0,
/* int	nwd	 */	0,
/* int	nn	 */	0,
/* int	ni	 */	0,
/* int	ul	 */	0,
/* int	cu	 */	0,
/* int	ce	 */	0,
/* int	rj	 */	0,
/* int	brnl	 */	0,
/* int	brpnl	 */	0,
/* int	in	 */	0,
/* int	in1	 */	0,
/* int	un	 */	0,
/* int	wch	 */	0,
/* int	rhang	 */	0,
/* int	pendt	 */	0,
/* tchar	*pendw	 */	(tchar *)0,
/* int	pendnf	 */	0,
/* int	spread	 */	0,
/* int	dpenal	 */	0,
/* int	it	 */	0,
/* int	itc	 */	0,
/* int	itmac	 */	0,
/* int	lnsize	 */	0,
/* int	wdsize	 */	0,
};
