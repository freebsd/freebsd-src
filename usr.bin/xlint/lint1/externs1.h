/*	$NetBSD: externs1.h,v 1.7 1995/10/02 17:31:39 jpo Exp $	*/

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
extern	int	pflag;
extern	int	rflag;
extern	int	sflag;
extern	int	tflag;
extern	int	uflag;
extern	int	vflag;
extern	int	yflag;
extern	int	zflag;

extern	void	norecover __P((void));

/*
 * cgram.y
 */
extern	int	blklev;
extern	int	mblklev;
extern	int	yydebug;

extern	int	yyerror __P((char *));
extern	int	yyparse __P((void));

/*
 * scan.l
 */
extern	pos_t	curr_pos;
extern	pos_t	csrc_pos;
extern	symt_t	symtyp;
extern	FILE	*yyin;
extern	u_quad_t qbmasks[], qlmasks[], qumasks[];

extern	void	initscan __P((void));
extern	int	sign __P((quad_t, tspec_t, int));
extern	int	msb __P((quad_t, tspec_t, int));
extern	quad_t	xsign __P((quad_t, tspec_t, int));
extern	void	clrwflgs __P((void));
extern	sym_t	*getsym __P((sbuf_t *));
extern	void	cleanup __P((void));
extern	sym_t	*pushdown __P((sym_t *));
extern	void	rmsym __P((sym_t *));
extern	void	rmsyms __P((sym_t *));
extern	void	inssym __P((int, sym_t *));
extern	void	freeyyv __P((void *, int));
extern	int	yylex __P((void));

/*
 * mem1.c
 */
extern	const	char *fnalloc __P((const char *));
extern	const	char *fnnalloc __P((const char *, size_t));
extern	int	getfnid __P((const char *));

extern	void	initmem __P((void));

extern	void	*getblk __P((size_t));
extern	void	*getlblk __P((int, size_t));
extern	void	freeblk __P((void));
extern	void	freelblk __P((int));

extern	void	*tgetblk __P((size_t));
extern	tnode_t	*getnode __P((void));
extern	void	tfreeblk __P((void));
extern	struct	mbl *tsave __P((void));
extern	void	trestor __P((struct mbl *));

/*
 * err.c
 */
extern	int	nerr;
extern	int	sytxerr;
extern	const	char *msgs[];

extern	void	error __P((int, ...));
extern	void	warning __P((int, ...));
extern	void	message __P((int, ...));
extern	int	gnuism __P((int, ...));
extern	void	lerror __P((const char *, ...));

/*
 * decl.c
 */
extern	dinfo_t	*dcs;
extern	const	char *unnamed;
extern	int	enumval;

extern	void	initdecl __P((void));
extern	type_t	*gettyp __P((tspec_t));
extern	type_t	*duptyp __P((const type_t *));
extern	type_t	*tduptyp __P((const type_t *));
extern	int	incompl __P((type_t *));
extern	void	setcompl __P((type_t *, int));
extern	void	addscl __P((scl_t));
extern	void	addtype __P((type_t *));
extern	void	addqual	__P((tqual_t));
extern	void	pushdecl __P((scl_t));
extern	void	popdecl __P((void));
extern	void	setasm __P((void));
extern	void	clrtyp __P((void));
extern	void	deftyp __P((void));
extern	int	length __P((type_t *, const char *));
extern	int	getbound __P((type_t *));
extern	sym_t	*lnklst __P((sym_t *, sym_t *));
extern	void	chktyp __P((sym_t *));
extern	sym_t	*decl1str __P((sym_t *));
extern	sym_t	*bitfield __P((sym_t *, int));
extern	pqinf_t	*mergepq __P((pqinf_t *, pqinf_t *));
extern	sym_t	*addptr __P((sym_t *, pqinf_t *));
extern	sym_t	*addarray __P((sym_t *, int, int));
extern	sym_t	*addfunc __P((sym_t *, sym_t *));
extern	void	chkfdef __P((sym_t *, int));
extern	sym_t	*dname __P((sym_t *));
extern	sym_t	*iname __P((sym_t *));
extern	type_t	*mktag __P((sym_t *, tspec_t, int, int));
extern	const	char *scltoa __P((scl_t));
extern	type_t	*compltag __P((type_t *, sym_t *));
extern	sym_t	*ename __P((sym_t *, int, int));
extern	void	decl1ext __P((sym_t *, int));
extern	void	cpuinfo __P((sym_t *, sym_t *));
extern	int	isredec __P((sym_t *, int *));
extern	int	eqtype __P((type_t *, type_t *, int, int, int *));
extern	void	compltyp __P((sym_t *, sym_t *));
extern	sym_t	*decl1arg __P((sym_t *, int));
extern	void	cluparg __P((void));
extern	void	decl1loc __P((sym_t *, int));
extern	sym_t	*aname __P((void));
extern	void	globclup __P((void));
extern	sym_t	*decl1abs __P((sym_t *));
extern	void	chksz __P((sym_t *));
extern	void	setsflg __P((sym_t *));
extern	void	setuflg __P((sym_t *, int, int));
extern	void	chkusage __P((dinfo_t *));
extern	void	chkusg1 __P((int, sym_t *));
extern	void	chkglsyms __P((void));
extern	void	prevdecl __P((int, sym_t *));

/*
 * tree.c
 */
extern	void	initmtab __P((void));
extern	type_t	*incref __P((type_t *, tspec_t));
extern	type_t	*tincref __P((type_t *, tspec_t));
extern	tnode_t	*getcnode __P((type_t *, val_t *));
extern	tnode_t	*getnnode __P((sym_t *, int));
extern	tnode_t	*getsnode __P((strg_t *));
extern	sym_t	*strmemb __P((tnode_t *, op_t, sym_t *));
extern	tnode_t	*build __P((op_t, tnode_t *, tnode_t *));
extern	tnode_t	*cconv __P((tnode_t *));
extern	int	typeok __P((op_t, int, tnode_t *, tnode_t *));
extern	tnode_t	*promote __P((op_t, int, tnode_t *));
extern	tnode_t	*convert __P((op_t, int, type_t *, tnode_t *));
extern	void	cvtcon __P((op_t, int, type_t *, val_t *, val_t *));
extern	const	char *tyname __P((type_t *));
extern	tnode_t	*bldszof __P((type_t *));
extern	tnode_t	*cast __P((tnode_t *, type_t *));
extern	tnode_t	*funcarg __P((tnode_t *, tnode_t *));
extern	tnode_t	*funccall __P((tnode_t *, tnode_t *));
extern	val_t	*constant __P((tnode_t *));
extern	void	expr __P((tnode_t *, int, int));
extern	void	chkmisc __P((tnode_t *, int, int, int, int, int, int));
extern	int	conaddr __P((tnode_t *, sym_t **, ptrdiff_t *));
extern	strg_t	*catstrg __P((strg_t *, strg_t *));

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
extern	int	plibflg;
extern	int	quadflg;

extern	void	pushctrl __P((int));
extern	void	popctrl __P((int));
extern	void	chkreach __P((void));
extern	void	funcdef __P((sym_t *));
extern	void	funcend __P((void));
extern	void	label __P((int, sym_t *, tnode_t *));
extern	void	if1 __P((tnode_t *));
extern	void	if2 __P((void));
extern	void	if3 __P((int));
extern	void	switch1 __P((tnode_t *));
extern	void	switch2 __P((void));
extern	void	while1 __P((tnode_t *));
extern	void	while2 __P((void));
extern	void	do1 __P((void));
extern	void	do2 __P((tnode_t *));
extern	void	for1 __P((tnode_t *, tnode_t *, tnode_t *));
extern	void	for2 __P((void));
extern	void	dogoto __P((sym_t *));
extern	void	docont __P((void));
extern	void	dobreak __P((void));
extern	void	doreturn __P((tnode_t *));
extern	void	glclup __P((int));
extern	void	argsused __P((int));
extern	void	constcond __P((int));
extern	void	fallthru __P((int));
extern	void	notreach __P((int));
extern	void	lintlib __P((int));
extern	void	linted __P((int));
extern	void	varargs __P((int));
extern	void	printflike __P((int));
extern	void	scanflike __P((int));
extern	void	protolib __P((int));
extern	void	longlong __P((int));

/*
 * init.c
 */
extern	int	initerr;
extern	sym_t	*initsym;
extern	int	startinit;

extern	void	prepinit __P((void));
extern	void	initrbr __P((void));
extern	void	initlbr __P((void));
extern	void	mkinit __P((tnode_t *));

/*
 * emit.c
 */
extern	void	outtype __P((type_t *));
extern	const	char *ttos __P((type_t *));
extern	void	outsym __P((sym_t *, scl_t, def_t));
extern	void	outfdef __P((sym_t *, pos_t *, int, int, sym_t *));
extern	void	outcall __P((tnode_t *, int, int));
extern	void	outusg __P((sym_t *));
