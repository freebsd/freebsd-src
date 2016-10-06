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


/*	from OpenSolaris "ext.h	1.10	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)ext.h	1.111 (gritter) 10/23/09
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

#include <sys/types.h>

extern	char	**argp;
extern	char	*chname;
extern	char	*eibuf;
extern	char	*ibufp;
extern	char	*obufp;
extern	char	*xbufp;
extern	char	*xeibuf;
extern	char	*cfname[NSO+1];
extern	char	devname[20];
extern	char	ibuf[IBUFSZ];
extern	char	**mfiles;
extern	char	*nextf;
extern	char	obuf[],	*obufp;
extern	char	*termtab,	*fontfile;
extern	char	xbuf[IBUFSZ];
extern	filep	apptr;
extern	filep	ip;
extern	filep	nextb;
extern	filep	offset;
extern	filep	roff;
extern	filep	woff;
#ifdef NROFF
extern	long	lvmot;
extern	size_t	*chtab;
#else
extern	short	*chtab;
extern	int	html;
#endif
extern	int	*pnp;
extern	int	*pstab;
extern	int	app;
extern	int	ascii;
extern	int	bd;
extern	int	*bdtab;
extern	int	blmac;
extern	int	lsmac;
extern	int	glss;
extern	int	lsn;
extern	int	ccs;
extern	int	charf;
extern	tchar	**chartab;
extern	struct charout	*charout;
extern	int	charoutsz;
extern	int	clonef;
extern	int	copyf;
extern	int	cs;
extern	int	defaultpl;
extern	int	defcf;
extern	int	dfact;
extern	int	dfactd;
extern	int	diflg;
extern	int	dilev;
extern	int	donef;
extern	int	donep;
extern	int	dotT;
extern	int	dpn;
extern	int	dl;
extern	int	ds;
extern	int	ecs;
extern	int	ejf;
extern	int	em;
extern	int	eqflg;
extern	int	error;
extern	int	esc;
extern	int	eschar;
extern	int	ev;
extern	int	fc;
extern	char	*fchartab;
extern	int	flss;
extern	int	fmtchar;
extern	int	*fontlab;
extern	int	gflag;
extern	int	hflg;
extern	int	ifi;
extern	int	ifile;
extern	int	ifl[NSO];
extern	int	iflg;
extern	int	init;
extern	int	lastkern;
extern	int	lasttrack;
extern	int	lead;
extern	int	lg;
extern	int	lgf;
extern	int	macerr;
extern	int	mb_cur_max;
extern	int	mflg;
extern	int	mfont;
extern	int	minflg;
extern	int	minspc;
extern	int	mpts;
extern	int	ndone;
extern	struct contab	*newmn;
extern	int	nflush;
extern	int	nfo;
extern	int	nfonts;
extern	int	nform;
extern	int	nhyp;
extern	int	nlflg;
extern	int	nmfi;
extern	int	no_out;
extern	int	nofeed;
extern	int	nolt;
extern	int	nonumb;
extern	int	noscale;
extern	int	npn;
extern	int	npnflg;
extern	int	nx;
extern	int	oldbits;
extern	struct contab	*oldmn;
extern	int	*olt;
extern	int	over;
extern	int	padc;
extern	int	padj;
extern	int	pfont;
extern	int	pfrom;
extern	pid_t	pipeflg;
extern	int	pl;
extern	int	pnlist[];
extern	int	po1;
extern	int	po;
extern	int	ppts;
extern	int	print;
extern	int	ptid;
extern	int	pto;
extern	int	quiet;
extern	int	ralss;
extern	int	rargc;
extern	int	raw;
extern	int	rawwidth;
extern	long	realpage;
extern	int	res;
extern	int	setwdf;
extern	int	sfont;
extern	int	smnt;
extern	int	stdi;
extern	int	stop;
extern	int	sv;
extern	int	tabch,	ldrch;
extern	int	tailflg;
extern	int	tflg;
extern	int	totout;
extern	int	trap;
extern	int	*trtab;
extern	int	*trintab;
extern	int	*trnttab;
extern	int	tryglf;
extern	int	tty;
extern	int	ttyod;
extern	int	Tflg;
extern	int	ulfont;
extern	int	vflag;
extern	int	vpt;
extern	int	wbfi;
extern	int	widthp;
extern	int	xflag;
extern	int	xfont;
extern	int	xpts;
extern	int	no_out;
extern	int	ejl;
extern	struct	s	*frame,	*stk,	*nxf;
extern	tchar	**hyp;
extern	tchar	*olinep;
extern	tchar	*pbbuf;
extern	int	pbsize;
extern	int	pbp;
extern	int	lastpbp;
extern	tchar	ch;
extern	tchar	nrbits;
extern	tchar	*oline;
extern	size_t	olinesz;
extern	struct widcache {	/* width cache, indexed by character */
	int	fontpts;
	int	rst;
	int	rsb;
	int	width;
	int	track;
	char	*evid;
} *widcache;
extern	char *gchtab;
extern	struct	d	*d;
extern	struct	d	*dip;
extern	int	initbdtab[];

#ifdef	EUC
#include <stddef.h>
extern	int	multi_locale;
extern  int	csi_width[];
extern	char	mbbuf1[];
extern	char	*mbbuf1p;
extern	wchar_t	twc;
extern	int	(*wdbdg)(wchar_t, wchar_t, int);
extern	wchar_t	*(*wddlm)(wchar_t, wchar_t, int);
#endif	/* EUC */
extern	int	**lhangtab;
extern	int	**rhangtab;
extern	int	**kernafter;
extern	int	**kernbefore;
extern	int	**ftrtab;
extern	char	*lgmark;
extern	struct lgtab	**lgtab;
extern	int	***lgrevtab;
extern	int	spreadwarn;
extern	int	spreadlimit;
extern	int	lastrq;
extern	int	noschr;
extern	int	argdelim;
extern	int	bol;
extern	int	prdblesc;
extern	int	gemu;
extern	int	chomp;
extern	int	chompend;

/* n1.c */
extern	void	mainloop(void);
extern	int	tryfile(char *, char *, int);
extern	void	catch(int);
extern	void	kcatch(int);
extern	void	init0(void);
extern	void	init1(char);
extern	void	init2(void);
extern	void	cvtime(void);
extern	int	ctoi(register char *);
extern	void	mesg(int);
extern	void	errprint(const char *, ...);
#define	fdprintf	xxfdprintf
extern	void	fdprintf(int, char *, ...);
extern	char	*roff_sprintf(char *, size_t, char *, ...);
extern	int	control(register int, register int);
extern	int	getrq2(void);
extern	int	getrq(int);
extern	tchar	getch(void);
extern	void	setxon(void);
extern	tchar	getch0(void);
extern	void	pushback(register tchar *);
extern	void	cpushback(register char *);
extern	tchar	*growpbbuf(void);
extern	int	nextfile(void);
extern	int	popf(void);
extern	void	flushi(void);
extern	int	getach(void);
extern	int	rgetach(void);
extern	void	casenx(void);
extern	int	getname(void);
extern	void	caseso(void);
extern	void	casepso(void);
extern	void	caself(void);
extern	void	casecf(void);
extern	void	casesy(void);
extern	void	getpn(register char *);
extern	void	setrpt(void);
extern	void	casedb(void);
extern	void	casexflag(void);
extern	void	casecp(void);
extern	void	caserecursionlimit(void);
extern	void	casechar(int);
extern	void	casefchar(void);
extern	void	caserchar(void);
extern	tchar	setchar(tchar);
extern	tchar	sfmask(tchar);
extern	int	issame(tchar, tchar);
/* n2.c */
extern	int	pchar(register tchar);
extern	void	pchar1(register tchar);
extern	void	outascii(tchar);
extern	void	oputs(register char *);
extern	void	flusho(void);
extern	void	caseoutput(void);
extern	void	done(int);
extern	void	done1(int);
extern	void	done2(int);
extern	void	done3(int);
extern	void	edone(int);
extern	void	casepi(void);
/* n3.c */
extern	void	*growcontab(void);
extern	void	*growblist(void);
extern	void	caseig(void);
extern	void	casern(void);
extern	void	maddhash(register struct contab *);
extern	void	munhash(register struct contab *);
extern	filep	finds(register int, int, int);
extern	void	caserm(void);
extern	void	caseas(void);
extern	void	caseds(void);
extern	void	caseam(void);
extern	void	casede(void);
extern	struct contab	*findmn(register int);
extern	struct contab	*findmx(register int);
extern	int	skip(int);
extern	int	copyb(void);
extern	void	copys(void);
extern	filep	alloc(void);
extern	void	ffree(filep);
extern	void	wbt(tchar);
extern	void	wbf(register tchar);
extern	void	wbfl(void);
extern	tchar	rbf(void);
extern	tchar	rbf0(register filep);
extern	filep	incoff(register filep);
extern	tchar	popi(void);
extern	int	pushi(filep, int, enum flags);
extern	void	sfree(struct s *);
extern 	struct s	*macframe(void);
extern	int	getsn(int);
extern	int	setstr(void);
extern	void	collect(void);
extern	void	seta(void);
extern	void	casebox(void);
extern	void	caseboxa(void);
extern	void	caseda(int);
extern	void	casedi(int);
extern	void	casedt(void);
extern	void	caseals(void);
extern	void	casewatch(int);
extern	void	caseunwatch(void);
extern	void	prwatch(struct contab *, int, int);
extern	void	casetl(void);
extern	void	casepc(void);
extern	void	casechop(void);
extern	void	casepm(void);
extern	void	stackdump(void);
extern	char	*macname(int);
extern	int	maybemore(int, int);
extern	tchar	setuc(void);
extern	int	makerq(const char *);
/* n4.c */
extern	void	*grownumtab(void);
extern	void	setn(void);
extern	int	wrc(tchar);
extern	void	setn1(int, int, tchar);
extern	void	nunhash(register struct numtab *);
extern	struct numtab	*findr(register int);
extern	struct numtab	*usedr(register int);
extern	int	fnumb(register int, register int (*)(tchar));
extern	int	decml(register int, register int (*)(tchar));
extern	int	roman(int, int (*)(tchar));
extern	int	roman0(int, int (*)(tchar), char *, char *);
extern	int	abc(int, int (*)(tchar));
extern	int	abc0(int, int (*)(tchar));
extern	int	hatoi(void);
#undef	atof
#define	atof	xxatof
extern	float	atof(void);
extern	long long	atoi0(void);
extern	double	atof0(void);
extern	void	setnr(const char *, int, int);
extern	void	setnrf(const char *, float, float);
extern	void	caserr(void);
extern	void	casernn(void);
extern	void	casenr(void);
extern	void	casenrf(void);
extern	void	caselnr(void);
extern	void	caselnrf(void);
extern	void	setr(void);
extern	void	caseaf(void);
extern	void	setaf(void);
extern	void	casealn(void);
extern	void	casewatchn(int);
extern	void	caseunwatchn(void);
extern	void	prwatchn(struct numtab *);
extern	int	vnumb(int *);
extern	int	hnumb(int *);
extern	int	inumb(int *);
extern	int	inumb2(int *, int *);
extern	float	atop(void);
extern	int	quant(int, int);
extern	tchar	moflo(int);
/* n5.c */
extern	void	save_tty(void);
extern	void	casead(void);
extern	void	casena(void);
extern	void	casefi(void);
extern	void	casenf(void);
extern	void	casepadj(void);
extern	void	casers(void);
extern	void	casens(void);
extern	void	casespreadwarn(void);
extern	int	chget(int);
extern	void	casecc(void);
extern	void	casec2(void);
extern	void	casehc(void);
extern	void	casetc(void);
extern	void	caselc(void);
extern	void	casehy(void);
extern	void	casenh(void);
extern	void	casehlm(void);
extern	void	casehcode(void);
extern	void	caseshc(void);
extern	void	casehylen(void);
extern	void	casehypp(void);
extern	void	casepshape(void);
extern	void	caselpfx(void);
extern	int	max(int, int);
extern	int	min(int, int);
extern	void	casece(void);
extern	void	caserj(void);
extern	void	casebrnl(void);
extern	void	casebrpnl(void);
extern	void	casein(void);
extern	void	casell(void);
extern	void	caselt(void);
extern	void	caseti(void);
extern	void	casels(void);
extern	void	casepo(void);
extern	void	casepl(void);
extern	void	casewh(void);
extern	void	casedwh(void);
extern	void	casech(void);
extern	void	casedch(void);
extern	void	casevpt(void);
extern	tchar	setolt(void);
extern	int	findn(struct d *, int);
extern	void	casepn(void);
extern	void	casebp(void);
extern	void	casetm(int);
extern	void	casetmc(void);
extern	void	caseerrprint(void);
extern	void	caseopen(void);
extern	void	caseopena(void);
extern	void	casewrite(void);
extern	void	casewritec(void);
extern	void	casewritem(void);
extern	void	caseclose(void);
extern	void	casesp(int);
extern	void	casebrp(void);
extern	void	caseblm(void);
extern	void	caselsm(void);
extern	void	casert(void);
extern	void	caseem(void);
extern	void	casefl(void);
extern	void	caseev(void);
extern	void	caseevc(void);
extern	void	evc(struct env *, struct env *);
extern	void	evcline(struct env *, struct env *);
extern	void	relsev(struct env *);
extern	void	caseel(void);
extern	void	caseie(void);
extern	void	caseif(int);
extern	void	casenop(void);
extern	void	casechomp(void);
extern	void	casereturn(void);
extern	void	casewhile(void);
extern	void	casebreak(void);
extern	void	casecontinue(int);
extern	void	eatblk(int);
extern	int	cmpstr(tchar);
extern	void	caserd(void);
extern	int	rdtty(void);
extern	void	caseec(void);
extern	void	caseeo(void);
extern	void	caseecs(void);
extern	void	caseecr(void);
extern	void	caseescoff(void);
extern	void	caseescon(void);
extern	void	caseta(void);
extern	void	casene(void);
extern	void	casetr(int);
extern	void	casetrin(void);
extern	void	casetrnt(void);
extern	void	casecu(void);
extern	void	caseul(void);
extern	void	caseuf(void);
extern	void	caseit(int);
extern	void	caseitc(void);
extern	void	casemc(void);
extern	void	casesentchar(void);
extern	void	casetranschar(void);
extern	void	casebreakchar(void);
extern	void	casenhychar(void);
extern	void	caseconnectchar(void);
extern	void	casemk(void);
extern	void	casesv(void);
extern	void	caseos(void);
extern	void	casenm(void);
extern	void	getnm(int *, int);
extern	void	casenn(void);
extern	void	caseab(void);
extern	void	restore_tty(void);
extern	void	set_tty(void);
extern	void	echo_off(void);
extern	void	echo_on(void);
/* n7.c */
extern	int	collectmb(tchar);
extern	void	tbreak(void);
extern	void	donum(void);
extern	void	text(void);
extern	void	nofill(void);
extern	void	callsp(void);
extern	void	ckul(void);
extern	int	storeline(register tchar, int);
extern	void	newline(int);
extern	int	findn1(struct d *, int);
extern	void	chkpn(void);
extern	int	findt(struct d *, int);
extern	int	findt1(void);
extern	void	eject(struct s *);
extern	int	movword(void);
extern	void	horiz(int);
extern	void	setnel(void);
extern	int	getword(int);
extern	void	storeword(register tchar, register int);
extern	void	growpgsize(void);
/* n8.c */
extern	void	hyphen(tchar *);
extern	int	punct(tchar);
extern	int	alph(tchar);
extern	void	caseht(void);
extern	void	casehw(void);
extern	int	exword(void);
extern	int	suffix(void);
extern	int	maplow(tchar);
extern	int	vowel(tchar);
extern	tchar	*chkvow(tchar *);
extern	void	digram(void);
extern	int	dilook(tchar, tchar, const char [26][13]);
extern	void	casehylang(void);
/* n9.c */
extern	tchar	setz(void);
extern	void	setline(void);
extern	tchar	eat(tchar);
extern	void	setov(void);
extern	void	setbra(void);
extern	void	setvline(void);
extern	void	setdraw(void);
extern	void	casefc(void);
extern	tchar	setfield(int);
extern	tchar	setpenalty(void);
extern	tchar	setdpenal(void);
extern	tchar	mkxfunc(int, int);
extern	void	pushinlev(void);
extern	tchar	popinlev(void);
extern	void	localize(void);
extern	void	caselc_ctype(void);
extern	void	casepsbb(void);
extern	void	casewarn(void);
extern	void	nosuch(int);
extern	void	illseq(int, const char *, int);
extern	void	missing(void);
extern	void	nodelim(int);
extern	void	storerq(int);
extern	int	fetchrq(tchar *);
extern	void	morechars(int);
#ifdef NROFF
extern	void	caseutf8conv(void);
extern	int	addch(char *);
#endif
