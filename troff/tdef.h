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


/*	from OpenSolaris "tdef.h	1.11	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)tdef.h	1.165 (gritter) 10/23/09
 *
 * Portions Copyright (c) 2014 Carsten Kunze <carsten.kunze@arcor.de>
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

#include "global.h"

/* starting values for typesetting parameters: */

#define	PS	10	/* default point size */
#define	FT	1	/* default font position */
#define ULFONT	2	/* default underline font */
#define	BDFONT	3	/* default emboldening font */
#define	BIFONT	4	/* default bold italic font */
#define	LL	(unsigned) 65*INCH/10	/* line length; 39picas=6.5in */
#define	VS	((12*INCH)/72)	/* initial vert space */

#ifdef	NROFF
#	define	EM	t.Em
#	define	HOR	t.Adj
#	define	VERT	t.Vert
#	define	INCH	240	/* increments per inch */
#	define	SPS	INCH/10	/* space size */
#	define	SS	INCH/10	/* " */
#	define	SES	SPS	/* sentence space size */
#	define	SSS	SS	/* " */
#	define	TRAILER	0
#	define	PO	0 /* page offset */
#	define	ASCII	1
#	define	PTID	1
#	define	LG	0
#	define	DTAB	0	/* set at 8 Ems at init time */
#	define	ICS	2*SPS
#endif
#ifndef NROFF	/* TROFF */
	/* Inch is set by ptinit() when troff started. */
	/* all derived values set then too
	*/
#	define	INCH	Inch	/* troff resolution -- number of goobies/inch  */
#	define	POINT	(INCH/72)	/* goobies per point (1/72 inch) */
#	define	HOR	Hor	/* horizontal resolution in goobies */
#	define	VERT	Vert	/* vertical resolution in goobies */
#	define	SPS	(EM/3)	/* space size  */
#	define	SES	SPS	/* sentence space size */
#	define	SS	12	/* space size in 36ths of an em */
#	define	SSS	SS	/* sentence space size in 36ths of an em */
#	define	PO	(INCH)	/* page offset 1 inch */
/* #	define	EM	(POINT * pts) */
#define	EM	(((long) INCH * u2pts(pts) + 36) / 72)	/* don't lose significance */
#define	EMPTS(pts)	(((long) INCH * u2pts(pts) + 36) / 72)
#	define	ASCII	0
#	define	PTID	1
#	define	LG	1
#	define	DTAB	(INCH/2)
#	define	ICS	3*SPS
#endif

/* These "characters" are used to encode various internal functions
 * Some make use of the fact that most ascii characters between
 * 0 and 040 don't have any graphic or other function.
 * The few that do have a purpose (e.g., \n, \b, \t, ...
 * are avoided by the ad hoc choices here.
 * See ifilt[] in n1.c for others -- 1, 2, 3, 5, 6, 7, 010, 011, 012 
 */

#define	LEADER	001
#define	IMP	004	/* impossible char; glues things together */
#define	TAB	011
#define	RPT	014	/* next character is to be repeated many times */
#define	CHARHT	015	/* size field sets character height */
#define	SLANT	016	/* size field sets amount of slant */
#define	DRAWFCN	017	/* next several chars describe arb drawing fcn */
			/* line: 'l' dx dy char */
			/* circle: 'c' r */
			/* ellipse: 'e' rx ry */
			/* arc: 'a' dx dy r */
			/* wiggly line '~' x y x y ... */
#define	DRAWLINE	'l'
#define	DRAWCIRCLE	'c'	/* circle */
#define	DRAWCIRCLEFI	'C'	/* filled circle */
#define	DRAWELLIPSE	'e'
#define	DRAWELLIPSEFI	'E'	/* filled ellipse */
#define	DRAWARC		'a'	/* arbitrary arc */
#define	DRAWSPLINE	'~'	/* quadratic B spline */
#define	DRAWTHICKNESS	't'	/* line thickness */
#define	DRAWPOLYGON	'p'	/* polygon */
#define	DRAWPOLYGONFI	'P'	/* filled polygon */

#define	LEFT	020	/* \{ */
#define	RIGHT	021	/* \} */
#define	FILLER	022	/* \& and similar purposes */
#define	XON	023	/* \X'...' starts here */
#define	OHC	024	/* optional hyphenation character \% */
#define	CONT	025	/* \c character */
#define	PRESC	026	/* printable escape */
#define	UNPAD	027	/* unpaddable blank */
#define	STRETCH	037	/* stretchable but unbreakable blank */
#define	XPAR	030	/* transparent mode indicator */
#define	FLSS	031
#define	WORDSP	032	/* paddable word space */
#define	ESC	033
#define	XOFF	034	/* \X'...' ends here */

#define	iscontrol(n)	(n==035 || n==036)	/* used to test the next two */
#define	HX	035	/* next character is value of \x'...' */
#define	FONTPOS	036	/* position of font \f(XX encoded in top */

#define	XFUNC	013	/* extended function codes, type in fbits: */
#define	ANCHOR	0001	/* anchor definition */
#define	LINKON	0002	/* link start */
#define	LINKOFF	0003	/* link end */
#define	LETSP	0004	/* positive letter spacing */
#define	NLETSP	0005	/* negative letter spacing */
#define	FLDMARK	0006	/* field marker */
#define	LETSH	0007	/* expanded letter shapes */
#define	NLETSH	0010	/* condensed letter shapes */
#define	HYPHED	0011	/* previous character is an automatic hyphen */
#define	OLT	0012	/* output line trap */
#define	YON	0013	/* indirect copy through */
#define	CC	0014	/* unchangeable control character */
#define	RQ	0015	/* request number */
#define	CHAR	0022	/* formatted result of a .char execution */
#define	INDENT	0023	/* current indent (informational only, no move) */
#define	PENALTY	0024	/* line breaking penalty */
#define	DPENAL	0025	/* default line breaking penalty change */
#define	ULINKON	0026	/* ulink start */
#define	ULINKOFF 0027	/* ulink end */

#define	isxfunc(c, x)	(cbits(c) == XFUNC && fbits(c) == (x))

#define	LAFACT	1000	/* letter adjustment float-to-int conversion factor */

#define	HYPHEN	c_hyphen
#define	EMDASH	c_emdash	/* \(em */
#define	RULE	c_rule		/* \(ru */
#define	MINUS	c_minus		/* minus sign on current font */
#define	LIG_FI	c_fi		/* \(ff */
#define	LIG_FL	c_fl		/* \(fl */
#define	LIG_FF	c_ff		/* \(ff */
#define	LIG_FFI	c_ffi		/* \(Fi */
#define	LIG_FFL	c_ffl		/* \(Fl */
#define	ACUTE	c_acute		/* acute accent \(aa */
#define	GRAVE	c_grave		/* grave accent \(ga */
#define	UNDERLINE c_under	/* \(ul */
#define	ROOTEN	c_rooten	/* root en \(rn */
#define	BOXRULE	c_boxrule	/* box rule \(br */
#define	LEFTHAND c_lefthand	/* left hand for word overflow */
#define	DAGGER	c_dagger	/* dagger for footnotes */

/* array sizes, and similar limits: */

#define	NFONT	10	/* maximum number of fonts (including specials, !afm) */
#define	EXTRAFONT 500	/* extra space for swapping a font */
extern	int	NN;	/* number registers */
#define	NNAMES	15	 /* predefined reg names */
extern	int	NS;	/* name buffer */
#define	NTM	4096	/* tm buffer */
#define	NEV	3	/* environments */
#define	EVLSZ	10	/* size of ev stack */
#define	DSIZE	512	/* disk sector size in chars */

extern	int	NM;	/* requests + macros */
#define	NHYP	40	/* max hyphens per word */
#define	NTAB	40	/* tab stops */
#define	NSO	5	/* "so" depth */
extern	int	NMF;	/* size of space for -m flags */
#define	WDSIZE	540	/* default word buffer size */
#define	LNSIZE	680	/* default line buffer size */
extern	int	NDI;	/* number of diversions */
extern	int	NCHARS;	/* maximum size of troff character set */
#define	NTRTAB	NCHARS	/* number of items in trtab[] */
#define	NWIDCACHE NCHARS	/* number of items in widcache */
#define	NTRAP	20	/* number of traps */
#define	NPN	20	/* numbers in "-o" */
#define	FBUFSZ	512	/* field buf size words */
#define	OBUFSZ	4096	/* bytes */
#define	IBUFSZ	4096L	/* bytes */
#define	NC	1024	/* cbuf size words */
#define	NOV	10	/* number of overstrike chars */
#define	NPP	10	/* pads per field */
#define	NSENT	40	/* number of sentence end characters */

/*
	Internal character representation:
	Internally, every character is carried around as
	a 64 bit cookie, called a "tchar" (typedef int64_t).
	Bits are numbered 63..0 from left to right.
	If bit 21 is 1, the character is motion, with
		if bit 22 it's vertical motion
		if bit 23 it's negative motion
	If bit 21 is 0, the character is a real character.
		if bit 63	zero motion
		bits 62..40	size
		bits 39..32	font
*/

/* in the following, "LL" should really be a tchar, but ... */

#define	MOT	(01LL<<21)	/* motion character indicator */
#define	VMOT	(01LL<<22)	/* vert motion bit */
#define	NMOT	(01LL<<23)	/* negative motion indicator*/
#define	BMBITS	0x001FFFFFLL	/* basic absolute motion bits */
#define	XMBITS	0x03000000LL	/* extended absolute motion bits */
#define	XMSHIFT	3		/* extended absolute motion shift */
#define	MAXMOT	0x007FFFFFLL	/* bad way to write this!!! */

#define	ismot(n)	((n) & MOT)
#define	isvmot(n)	((n) & VMOT)	/* must have tested MOT previously */
#define	isnmot(n)	((n) & NMOT)	/* ditto */
#define	absmot(n)	(unsigned long)((BMBITS&(n)) | ((n)&XMBITS)>>XMSHIFT)
#define	sabsmot(n)	(!xflag || (n) <= MAXMOT ? ((n)&BMBITS) | ((n)&~BMBITS)<<XMSHIFT : moflo(n))

#define	ZBIT		(01ULL << 63) 	/* zero width char */
#define	iszbit(n)	((n) & ZBIT)
#define	BLBIT		(01ULL << 31)	/* optional break-line char */
#define	isblbit(n)	((n) & BLBIT)
#define	COPYBIT		(01ULL << 30)	/* wide character in copy mode */
#define	iscopy(n)	((n) & COPYBIT && !ismot(n) && cbits(n) & ~0177)
#define	ADJBIT		(01ULL << 30)	/* adjusted space */
#define	isadjspc(n)	((n) & ADJBIT && !ismot(n) && (cbits(n) & ~0177) == 0 \
				&& cbits(n) != FILLER)
#define	isadjmot(n)	((n) & ADJBIT && ismot(n))
#define	TRANBIT		(01ULL << 30)	/* transparent filler */
#define	istrans(n)	((n) & TRANBIT && cbits(n) == FILLER)
#define	AUTOLIG		(01ULL << 29)	/* ligature substituted automatically */
#define	islig(n)	((n) & AUTOLIG)
#define	TAILBIT		(01ULL << 29)	/* tail recursion */
#define	istail(n)	(((n) & (TAILBIT|MOT|'\n')) == (TAILBIT|'\n'))
#define	SENTSP		(01ULL << 29)	/* sentence space */
#define	issentsp(n)	((n) & SENTSP)
#define	DIBIT		(01ULL << 28)	/* written in a diversion */
#define	isdi(n)		((n) & DIBIT)

#define	SMASK		(037777777LL << 40)
#define	FMASK		(0377LL << 32)
#define	SFMASK		(SMASK|FMASK)	/* size and font in a tchar */
#define	sbits(n)	(unsigned)(((n) >> 40) & 037777777)
#define	fbits(n)	(((n) >> 32) & 0377)
#define	sfbits(n)	(unsigned)(037777777777UL & (((n) & SFMASK) >> 32))
#define	cbits(n)	(unsigned)(0x003FFFFFLL & (n))	/* isolate bottom 22 bits  */

#define	setsbits(n,s)	n = (n & ~SMASK) | (tchar)(s) << 40
#define	setfbits(n,f)	n = (n & ~FMASK) | (tchar)(f) << 32
#define	setsfbits(n,sf)	n = (n & ~SFMASK) | (tchar)(sf) << 32
#define	setcbits(n,c)	n = ((n & ~0x001FFFFFLL) | (c))	/* set character bits */

#define	BYTEMASK	0377
#define	BYTE	8

#define	ischar(n)	(((n) & ~BYTEMASK) == 0)

#if defined (NROFF) && defined (EUC) && defined (ZWDELIMS)
#define ZWDELIM1	ZBIT | FIRSTOFMB | ' '	/* non-ASCII word delimiter 1 */
#define ZWDELIM2	ZBIT | MIDDLEOFMB | ' '	/* non-ASCII word delimiter 2 */
#define ZWDELIM(c)	((c) == 0) ? ' ' : ((c) == 1) ? ZWDELIM1 : ZWDELIM2
#endif	/* NROFF && EUC && ZWDELIMS */

#define	ZONE	5	/* 5 hrs for EST */
#define	TABMASK	0x3FFFFFFF
#define	RTAB	(unsigned) 0x80000000
#define	CTAB	0x40000000

#define	TABBIT	02		/* bits in gchtab */
#define	LDRBIT	04
#define	FCBIT	010
#define	LGBIT	020
#define	CHBIT	040

#define	INFPENALTY0	(1e6)
#define	INFPENALTY	(1e9)
#define	MAXPENALTY	(1e6)
#define	PENALSCALE	(1.0/50)

#define	PAIR(A,B)	(A|(B<<BYTE))
#define	LOOP		(-4)

#ifndef EUC
#define	oput(c)	if ((*obufp++ = (c)), obufp >= &obuf[OBUFSZ]) flusho(); else
#else
#define	oput(c)	if ((*obufp++ = cbits(c) & BYTEMASK), obufp >= &obuf[OBUFSZ]) flusho(); else
#endif /* EUC */

#ifdef	NROFF
#define	ftrans(f, c)	(c)
#else
#define	ftrans(f, c)	(f >= 0 && f <= nfonts ? ftrtab[f][ftrtab[f][c]] : (c))
#endif

#define hex2nibble(x) ((x)>='0' && (x)<='9' ? (x)-'0'    : \
                       (x)>='A' && (x)<='F' ? (x)-'A'+10 : \
                                              (x)-'a'+10 )

/*
 * "temp file" parameters.  macros and strings
 * are stored in an array of linked blocks,
 * which may be in memory and an array called
 * corebuf[].

 * The numerology is delicate if filep is 16 bits:
	#define BLK 128
	#define NBLIST 512
 * i.e., the product is 16 bits long.

 * If filep is a long then NBLIST
 * can be a lot bigger.  Of course that makes
 * the file or core image a lot bigger too,
 * and means you don't detect missing diversion
 * terminations as quickly... .
 * It also means that you may have trouble running
 * on non-swapping systems, since the core image
 * will be over 1Mb.

 * Note: As of 8/14/05, NBLIST has gone, and corebuf is
 * grown dynamically as needed.

 * filep must be a signed integer since the value -1
 * is special (it indicates that input is read from tty).

 * filep must be an int for LP64 platforms since it is
 * casted to unsigned at some places (or changed there)

 * BLK must be a power of 2
 */

typedef int filep;

#define	BLK	128	/* alloc block in tchars */

/* Other things are stored in the temp file or corebuf:
 *	a single block for .rd input, at offset RD_OFFSET
 *	NEV copies of the environment block, at offset ENV_OFFSET

 * The existing implementation assumes very strongly that
 * these are respectively NBLIST*BLK and 0.

 */

#define	RD_OFFSET	(NBLIST * BLK)
#define ENV_OFFSET	0
#define	ENV_BLK		((NEV * sizeof(env) / sizeof(tchar) + BLK-1) / BLK)
				/* rounded up to even BLK */

#include <setjmp.h>
#include <inttypes.h>
typedef	int64_t		tchar;

extern	int	Inch, Hor, Vert, Unitwidth;

/* BSD systems have a function devname(); avoid a collision */
#define	devname	troff_devname

/* these characters are used as various signals or values
 * in miscellaneous places.
 * values are set in specnames in t10.c
 */

extern int	c_hyphen;
extern int	c_emdash;
extern int	c_rule;
extern int	c_minus;
extern int	c_fi;
extern int	c_fl;
extern int	c_ff;
extern int	c_ffi;
extern int	c_ffl;
extern int	c_acute;
extern int	c_grave;
extern int	c_under;
extern int	c_rooten;
extern int	c_boxrule;
extern int	c_lefthand;
extern int	c_dagger;

struct lgtab {
	struct lgtab	*next;
	struct lgtab	*link;
	int	from;
	int	to;
};

#ifdef	DEBUG
extern	int	debug;	/*debug flag*/
#define	DB_MAC	01	/*print out macro calls*/
#define	DB_ALLC	02	/*print out message from alloc()*/
#define	DB_GETC	04	/*print out message from getch()*/
#define	DB_LOOP	010	/*print out message before "Eileen's loop" fix*/
#define	DB_ABRT	020	/*abort on errprint()*/
#endif	/* DEBUG */

extern enum warn {
	WARN_NONE	= 0,
	WARN_CHAR	= 1,
	WARN_NUMBER	= 2,
	WARN_BREAK	= 4,
	WARN_DELIM	= 8,
	WARN_EL		= 16,
	WARN_SCALE	= 32,
	WARN_RANGE	= 64,
	WARN_SYNTAX	= 128,
	WARN_DI		= 256,
	WARN_MAC	= 512,
	WARN_REG	= 1024,
	WARN_RIGHT_BRACE= 4096,
	WARN_MISSING	= 8192,
	WARN_INPUT	= 16384,
	WARN_ESCAPE	= 32768,
	WARN_SPACE	= 65536,
	WARN_FONT	= 131072,
	WARN_ALL	= 2147481855,	/* all except di, mac, reg */
	WARN_W		= 2147483647
} warn;

enum flags {
	FLAG_WATCH	= 01,
	FLAG_STRING	= 02,
	FLAG_USED	= 04,
	FLAG_DIVERSION	= 010,
	FLAG_LOCAL	= 020,
	FLAG_PARAGRAPH	= 040
};

enum {
	PG_NONE		= 00,
	PG_NEWIN	= 01,
	PG_NEWLL	= 02
};

struct	d {	/* diversion */
	filep	op;
	int	dnl;
	int	dimac;
	int	ditrap;
	int	ditf;
	int	alss;
	int	blss;
	int	nls;
	int	mkline;
	int	maxl;
	int	hnl;
	int	curd;
	int	flss;
	struct contab	*soff;
	int	mlist[NTRAP];
	int	nlist[NTRAP];
	struct env	*boxenv;
};

struct	charout {	/* formatted result of .char */
	filep	op;
	int	width;
	int	height;
	int	depth;
	tchar	ch;
};

struct	s {	/* stack frame */
	int	nargs;
	struct s *pframe;
	filep	pip;
	filep	newip;
	int	*argt;
	tchar	*argsp;
	int	ppendt;
	tchar	pch;
	int	lastpbp;
	int	mname;
	int	frame_cnt;
	int	tail_cnt;
	struct contab	*contp;
	struct numtab	*numtab;
	struct numtab	**nhash;
	struct contab	*contab;
	struct contab	**mhash;
	int	NN;
	int	NM;
	enum {
		LOOP_FREE = 01,
		LOOP_NEXT = 02,
		LOOP_EVAL = 04
	} loopf;
	enum flags	flags;
	jmp_buf	*jmp;
};

extern struct contab {
	unsigned int	rq;
	struct	contab *link;
	void	(*f)(int);
	unsigned mx;
	unsigned int	als;
	int	nlink;
	enum flags	flags;
} *contab;
extern const struct contab initcontab[];

extern struct numtab {
	int	r;		/* name */
	short	fmt;
	int	inc;
	int	val;
	struct	numtab *link;
	int	aln;
	int	nlink;
	float	fval;
	float	finc;
	enum flags	flags;
} *numtab;
extern const struct numtab initnumtab[];

#define	PN	0
#define	NL	1
#define	YR	2
#define	HP	3
#define	CT	4
#define	DN	5
#define	MO	6
#define	DY	7
#define	DW	8
#define	LN	9
#define	DL	10
#define	ST	11
#define	SB	12
#define	CD	13
#define	PID	14

struct acc {
	long long	n;
	double	f;
};

/* inline environment */
struct inlev {
	int	_apts;
	int	_apts1;
	int	_pts;
	int	_pts1;
	int	_font;
	int	_font1;
	int	_cc;
	int	_c2;
	int	_ohc;
	int	_hyf;
	int	_tabc;
	int	_dotc;
	int	_dpenal;
};

/* the infamous environment block */

#define	ics	env._ics
#define	sps	env._sps
#define	ses	env._ses
#define	spacesz	env._spacesz
#define	sesspsz	env._sesspsz
#ifndef	NROFF
#define	minsps	env._minsps
#define	minspsz	env._minspsz
#define	letspsz	env._letspsz
#define	letsps	env._letsps
#define	lspmin	env._lspmin
#define	lspmax	env._lspmax
#define	lspnc	env._lspnc
#define	lsplow	env._lsplow
#define	lsphigh	env._lsphigh
#define	lspcur	env._lspcur
#define	lsplast	env._lsplast
#define	lshmin	env._lshmin
#define	lshmax	env._lshmax
#define	lshwid	env._lshwid
#define	lshlow	env._lshlow
#define	lshhigh	env._lshhigh
#define	lshcur	env._lshcur
#else	/* NROFF */
#define	minsps	0
#define	minspsz	0
#define	letspsz	0
#define	letsps	0
#define	lspmin	0
#define	lspmax	0
#define	lspnc	0
#define	lsplow	0
#define	lsphigh	0
#define	lspcur	0
#define	lsplast	0
#define	lshmin	0
#define	lshmax	0
#define	lshwid	0
#define	lshlow	0
#define	lshhigh	0
#define	lshcur	0
#endif	/* NROFF */
#define	fldcnt	env._fldcnt
#define	lss	env._lss
#define	lss1	env._lss1
#define	ll	env._ll
#define	ll1	env._ll1
#define	lt	env._lt
#define	lt1	env._lt1
#define	ic	env._ic
#define	icf	env._icf
#define	chbits	env._chbits
#define	spbits	env._spbits
#define	nmbits	env._nmbits
#define	apts	env._apts
#define	apts1	env._apts1
#define	pts	env._pts
#define	pts1	env._pts1
#define	font	env._font
#define	font1	env._font1
#define	ls	env._ls
#define	ls1	env._ls1
#define	ad	env._ad
#define	nms	env._nms
#define	ndf	env._ndf
#define	fi	env._fi
#define	cc	env._cc
#define	c2	env._c2
#define	ohc	env._ohc
#define	tdelim	env._tdelim
#define	hyf	env._hyf
#define	hyoff	env._hyoff
#define	hlm	env._hlm
#define	hlc	env._hlc
#define	hylen	env._hylen
#define	hypp	env._hypp
#define	hypp2	env._hypp2
#define	hypp3	env._hypp3
#define	un1	env._un1
#define	tabc	env._tabc
#define	dotc	env._dotc
#define	adsp	env._adsp
#define	adrem	env._adrem
#define	lastl	env._lastl
#define	nel	env._nel
#define	admod	env._admod
#define	adflg	env._adflg
#define	adspc	env._adspc
#define	pa	env._pa
#define	wordp	env._wordp
#define	spflg	env._spflg
#define	seflg	env._seflg
#define	linep	env._linep
#define	wdend	env._wdend
#define	wdstart	env._wdstart
#define	wne	env._wne
#define	wsp	env._wsp
#define	ne	env._ne
#define	nc	env._nc
#define	nb	env._nb
#define	lnmod	env._lnmod
#define	nwd	env._nwd
#define	nn	env._nn
#define	ni	env._ni
#define	ul	env._ul
#define	cu	env._cu
#define	ce	env._ce
#define	rj	env._rj
#define	brnl	env._brnl
#define	brpnl	env._brpnl
#define	in	env._in
#define	in1	env._in1
#define	un	env._un
#define	wch	env._wch
#define	rhang	env._rhang
#define	pendt	env._pendt
#define	pendw	env._pendw
#define	pendnf	env._pendnf
#define	spread	env._spread
#define	dpenal	env._dpenal
#define	it	env._it
#define	itc	env._itc
#define	itmac	env._itmac
#define	lnsize	env._lnsize
#define	wdsize	env._wdsize
#define	pgsize	env._pgsize
#define	pgcsize	env._pgcsize
#define	pgssize	env._pgssize
#define	pgchars	env._pgchars
#define	pgspacs	env._pgspacs
#define	pgwords	env._pgwords
#define	pglines	env._pglines
#define	pglnout	env._pglnout
#define	pshapes	env._pshapes
#define	pgne	env._pgne
#define	pglastw	env._pglastw
#define	linkin	env._linkin
#define	linkout	env._linkout
#define	linkhp	env._linkhp
#define	hylang	env._hylang
#define	dicthnj	env._dicthnj
#define	hyext	env._hyext
#define	hcode	env._hcode
#define	nhcode	env._nhcode
#define	shc	env._shc
#define	lpfx	env._lpfx
#define	nlpfx	env._nlpfx
#define	stopch	env._stopch
#define	cht	env._cht
#define	cdp	env._cdp
#define	maxcht	env._maxcht
#define	maxcdp	env._maxcdp
#define	wdhyf	env._wdhyf
#define	hyptr	env._hyptr
#define	tabtab	env._tabtab
#define	line	env._line
#define	word	env._word
#define	wdpenal	env._wdpenal
#define	sentch	env._sentch
#define	transch	env._transch
#define	breakch	env._breakch
#define	nhych	env._nhych
#define	connectch	env._connectch
#define	para	env._para
#define	parsp	env._parsp
#define	pgwordp	env._pgwordp
#define	pgspacp	env._pgspacp
#define	pgwordw	env._pgwordw
#define	pghyphw	env._pghyphw
#define	pgadspc	env._pgadspc
#define	pglsphc	env._pglsphc
#define	pgopt	env._pgopt
#define	pgspacw	env._pgspacw
#define	pglgsc	env._pglgsc
#define	pglgec	env._pglgec
#define	pglgsw	env._pglgsw
#define	pglgew	env._pglgew
#define	pglgsh	env._pglgsh
#define	pglgeh	env._pglgeh
#define	pgin	env._pgin
#define	pgll	env._pgll
#define	pgwdin	env._pgwdin
#define	pgwdll	env._pgwdll
#define	pgflags	env._pgflags
#define	pglno	env._pglno
#define	pgpenal	env._pgpenal
#define	ninlev	env._ninlev
#define	ainlev	env._ainlev
#define	inlevp	env._inlevp
#define	evname	env._evname

extern struct env {
	int	_ics;
	int	_sps;
	int	_ses;
	int	_spacesz;
	int	_sesspsz;
#ifndef	NROFF
	int	_minsps;
	int	_minspsz;
	int	_letspsz;
	int	_letsps;
	int	_lspmin;
	int	_lspmax;
	int	_lspnc;
	int	_lsplow;
	int	_lsphigh;
	int	_lspcur;
	int	_lsplast;
	int	_lshmin;
	int	_lshmax;
	int	_lshwid;
	int	_lshlow;
	int	_lshhigh;
	int	_lshcur;
#endif	/* !NROFF */
	int	_fldcnt;
	int	_lss;
	int	_lss1;
	int	_ll;
	int	_ll1;
	int	_lt;
	int	_lt1;
	tchar	_ic;
	int	_icf;
	tchar	_chbits;
	tchar	_spbits;
	tchar	_nmbits;
	int	_apts;
	int	_apts1;
	int	_pts;
	int	_pts1;
	int	_font;
	int	_font1;
	int	_ls;
	int	_ls1;
	int	_ad;
	int	_nms;
	int	_ndf;
	int	_fi;
	int	_cc;
	int	_c2;
	int	_ohc;
	int	_tdelim;
	int	_hyf;
	int	_hyoff;
	int	_hlm;
	int	_hlc;
	int	_hylen;
	float	_hypp;
	float	_hypp2;
	float	_hypp3;
	int	_un1;
	int	_tabc;
	int	_dotc;
	int	_adsp;
	int	_adrem;
	int	_lastl;
	int	_nel;
	int	_admod;
	int	_adflg;
	int	_adspc;
	int	_pa;
	tchar	*_wordp;
	int	_spflg;
	int	_seflg;
	tchar	*_linep;
	tchar	*_wdend;
	tchar	*_wdstart;
	int	_wne;
	int	_wsp;
	int	_ne;
	int	_nc;
	int	_nb;
	int	_lnmod;
	int	_nwd;
	int	_nn;
	int	_ni;
	int	_ul;
	int	_cu;
	int	_ce;
	int	_rj;
	int	_brnl;
	int	_brpnl;
	int	_in;
	int	_in1;
	int	_un;
	int	_wch;
	int	_rhang;
	int	_pendt;
	tchar	*_pendw;
	int	_pendnf;
	int	_spread;
	int	_dpenal;
	int	_it;
	int	_itc;
	int	_itmac;
	int	_lnsize;
	int	_wdsize;
	int	_pgsize;
	int	_pgcsize;
	int	_pgssize;
	int	_pgchars;
	int	_pgspacs;
	int	_pgwords;
	int	_pglines;
	int	_pglnout;
	int	_pshapes;
	int	_pgne;
	int	_pglastw;
	int	_linkin;
	int	_linkout;
	int	_linkhp;
	char	*_hylang;
	void	*_dicthnj;
	int	_hyext;
	int	*_hcode;
	int	_nhcode;
	tchar	*_lpfx;
	int	_nlpfx;
	int	_shc;
	tchar	_stopch;
	int	_cht;
	int	_cdp;
	int	_maxcht;
	int	_maxcdp;
	int	_wdhyf;
	tchar	*_hyptr[NHYP];
	int	_tabtab[NTAB];
	int	_sentch[NSENT];
	int	_transch[NSENT];
	int	_breakch[NSENT];
	int	_nhych[NSENT];
	int	_connectch[NSENT];
	tchar	*_line;
	tchar	*_word;
	int	*_wdpenal;
	tchar	*_para;
	tchar	*_parsp;
	int	*_pgwordp;
	int	*_pgspacp;
	int	*_pgwordw;
	int	*_pghyphw;
	int	*_pgadspc;
	int	*_pglsphc;
	int	*_pgopt;
	int	*_pgspacw;
	tchar	*_pglgsc;
	tchar	*_pglgec;
	tchar	*_pglgsw;
	tchar	*_pglgew;
	tchar	*_pglgsh;
	tchar	*_pglgeh;
	int	*_pgin;
	int	*_pgll;
	int	*_pgwdin;
	int	*_pgwdll;
	int	*_pglno;
	int	*_pgflags;
	float	*_pgpenal;
	int	_ninlev;
	int	_ainlev;
	struct inlev	*_inlevp;
	char	*_evname;
} env, initenv;
