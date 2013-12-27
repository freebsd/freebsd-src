/*	$NetBSD: externs1.h,v 1.13 2002/01/18 21:01:39 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * main.c
 */
extern	int	aflag;
extern	int	bflag;
extern	int	cflag;
extern	int	dflag;
extern	int	eflag;
extern	int	Fflag;
extern	int	gflag;
extern	int	hflag;
extern	int	rflag;
extern	int	sflag;
extern	int	tflag;
extern	int	uflag;
extern	int	vflag;
extern	int	yflag;
extern	int	wflag;
extern	int	zflag;

extern	void	norecover(void);

/*
 * cgram.y
 */
extern	int	blklev;
extern	int	mblklev;
extern	int	yydebug;

extern	int	yyerror(char *);
extern	int	yyparse(void);

/*
 * scan.l
 */
extern	pos_t	curr_pos;
extern	pos_t	csrc_pos;
extern	symt_t	symtyp;
extern	FILE	*yyin;
extern	uint64_t qbmasks[], qlmasks[], qumasks[];

extern	void	initscan(void);
extern	int	sign(int64_t, tspec_t, int);
extern	int	msb(int64_t, tspec_t, int);
extern	int64_t	xsign(int64_t, tspec_t, int);
extern	void	clrwflgs(void);
extern	sym_t	*getsym(sbuf_t *);
extern	void	cleanup(void);
extern	sym_t	*pushdown(sym_t *);
extern	void	rmsym(sym_t *);
extern	void	rmsyms(sym_t *);
extern	void	inssym(int, sym_t *);
extern	void	freeyyv(void *, int);
extern	int	yylex(void);

/*
 * mem1.c
 */
extern	const	char *fnalloc(const char *);
extern	const	char *fnnalloc(const char *, size_t);
extern	int	getfnid(const char *);

extern	void	initmem(void);

extern	void	*getblk(size_t);
extern	void	*getlblk(int, size_t);
extern	void	freeblk(void);
extern	void	freelblk(int);

extern	void	*tgetblk(size_t);
extern	tnode_t	*getnode(void);
extern	void	tfreeblk(void);
extern	struct	mbl *tsave(void);
extern	void	trestor(struct mbl *);

/*
 * err.c
 */
extern	int	nerr;
extern	int	sytxerr;
extern	const	char *msgs[];

extern	void	msglist(void);
extern	void	error(int, ...);
extern	void	warning(int, ...);
extern	void	message(int, ...);
extern	int	gnuism(int, ...);
extern	void	lerror(const char *, ...)
     __attribute__((__noreturn__,__format__(__printf__, 1, 2)));

/*
 * decl.c
 */
extern	dinfo_t	*dcs;
extern	const	char *unnamed;
extern	int	enumval;

extern	void	initdecl(void);
extern	type_t	*gettyp(tspec_t);
extern	type_t	*duptyp(const type_t *);
extern	type_t	*tduptyp(const type_t *);
extern	int	incompl(type_t *);
extern	void	setcompl(type_t *, int);
extern	void	addscl(scl_t);
extern	void	addtype(type_t *);
extern	void	addqual(tqual_t);
extern	void	pushdecl(scl_t);
extern	void	popdecl(void);
extern	void	setasm(void);
extern	void	clrtyp(void);
extern	void	deftyp(void);
extern	int	length(type_t *, const char *);
extern	int	getbound(type_t *);
extern	sym_t	*lnklst(sym_t *, sym_t *);
extern	void	chktyp(sym_t *);
extern	sym_t	*decl1str(sym_t *);
extern	sym_t	*bitfield(sym_t *, int);
extern	pqinf_t	*mergepq(pqinf_t *, pqinf_t *);
extern	sym_t	*addptr(sym_t *, pqinf_t *);
extern	sym_t	*addarray(sym_t *, int, int);
extern	sym_t	*addfunc(sym_t *, sym_t *);
extern	void	chkfdef(sym_t *, int);
extern	sym_t	*dname(sym_t *);
extern	sym_t	*iname(sym_t *);
extern	type_t	*mktag(sym_t *, tspec_t, int, int);
extern	const	char *scltoa(scl_t);
extern	type_t	*compltag(type_t *, sym_t *);
extern	sym_t	*ename(sym_t *, int, int);
extern	void	decl1ext(sym_t *, int);
extern	void	cpuinfo(sym_t *, sym_t *);
extern	int	isredec(sym_t *, int *);
extern	int	eqtype(type_t *, type_t *, int, int, int *);
extern	void	compltyp(sym_t *, sym_t *);
extern	sym_t	*decl1arg(sym_t *, int);
extern	void	cluparg(void);
extern	void	decl1loc(sym_t *, int);
extern	sym_t	*aname(void);
extern	void	globclup(void);
extern	sym_t	*decl1abs(sym_t *);
extern	void	chksz(sym_t *);
extern	void	setsflg(sym_t *);
extern	void	setuflg(sym_t *, int, int);
extern	void	chkusage(dinfo_t *);
extern	void	chkusg1(int, sym_t *);
extern	void	chkglsyms(void);
extern	void	prevdecl(int, sym_t *);

/*
 * tree.c
 */
extern	void	initmtab(void);
extern	type_t	*incref(type_t *, tspec_t);
extern	type_t	*tincref(type_t *, tspec_t);
extern	tnode_t	*getcnode(type_t *, val_t *);
extern	tnode_t	*getnnode(sym_t *, int);
extern	tnode_t	*getsnode(strg_t *);
extern	sym_t	*strmemb(tnode_t *, op_t, sym_t *);
extern	tnode_t	*build(op_t, tnode_t *, tnode_t *);
extern	tnode_t	*cconv(tnode_t *);
extern	int	typeok(op_t, int, tnode_t *, tnode_t *);
extern	tnode_t	*promote(op_t, int, tnode_t *);
extern	tnode_t	*convert(op_t, int, type_t *, tnode_t *);
extern	void	cvtcon(op_t, int, type_t *, val_t *, val_t *);
extern	const	char *tyname(type_t *);
extern	tnode_t	*bldszof(type_t *);
extern	tnode_t	*cast(tnode_t *, type_t *);
extern	tnode_t	*funcarg(tnode_t *, tnode_t *);
extern	tnode_t	*funccall(tnode_t *, tnode_t *);
extern	val_t	*constant(tnode_t *);
extern	void	expr(tnode_t *, int, int);
extern	void	chkmisc(tnode_t *, int, int, int, int, int, int);
extern	int	conaddr(tnode_t *, sym_t **, ptrdiff_t *);
extern	strg_t	*catstrg(strg_t *, strg_t *);

/*
 * func.c
 */
extern	sym_t	*funcsym;
extern	int	reached;
extern	int	rchflg;
extern	int	ftflg;
extern	int	nargusg;
extern	pos_t	aupos;
extern	int	nvararg;
extern	pos_t	vapos;
extern	int	prflstrg;
extern	pos_t	prflpos;
extern	int	scflstrg;
extern	pos_t	scflpos;
extern	int	ccflg;
extern	int	llibflg;
extern	int	nowarn;
extern	int	bitfieldtype_ok;
extern	int	plibflg;
extern	int	quadflg;

extern	void	pushctrl(int);
extern	void	popctrl(int);
extern	void	chkreach(void);
extern	void	funcdef(sym_t *);
extern	void	funcend(void);
extern	void	label(int, sym_t *, tnode_t *);
extern	void	if1(tnode_t *);
extern	void	if2(void);
extern	void	if3(int);
extern	void	switch1(tnode_t *);
extern	void	switch2(void);
extern	void	while1(tnode_t *);
extern	void	while2(void);
extern	void	do1(void);
extern	void	do2(tnode_t *);
extern	void	for1(tnode_t *, tnode_t *, tnode_t *);
extern	void	for2(void);
extern	void	dogoto(sym_t *);
extern	void	docont(void);
extern	void	dobreak(void);
extern	void	doreturn(tnode_t *);
extern	void	glclup(int);
extern	void	argsused(int);
extern	void	constcond(int);
extern	void	fallthru(int);
extern	void	notreach(int);
extern	void	lintlib(int);
extern	void	linted(int);
extern	void	varargs(int);
extern	void	printflike(int);
extern	void	scanflike(int);
extern	void	protolib(int);
extern	void	longlong(int);
extern	void	bitfieldtype(int);

/*
 * init.c
 */
extern	int	initerr;
extern	sym_t	*initsym;
extern	int	startinit;

extern	void	prepinit(void);
extern	void	initrbr(void);
extern	void	initlbr(void);
extern	void	mkinit(tnode_t *);

/*
 * emit.c
 */
extern	void	outtype(type_t *);
extern	const	char *ttos(type_t *);
extern	void	outsym(sym_t *, scl_t, def_t);
extern	void	outfdef(sym_t *, pos_t *, int, int, sym_t *);
extern	void	outcall(tnode_t *, int, int);
extern	void	outusg(sym_t *);
