/*	$Id: term_ascii.c,v 1.43 2015/02/16 14:11:41 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#if HAVE_WCHAR
#include <locale.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#if HAVE_WCHAR
#include <wchar.h>
#endif

#include "mandoc.h"
#include "mandoc_aux.h"
#include "out.h"
#include "term.h"
#include "main.h"

static	struct termp	 *ascii_init(enum termenc,
				const struct mchars *, char *);
static	double		  ascii_hspan(const struct termp *,
				const struct roffsu *);
static	size_t		  ascii_width(const struct termp *, int);
static	void		  ascii_advance(struct termp *, size_t);
static	void		  ascii_begin(struct termp *);
static	void		  ascii_end(struct termp *);
static	void		  ascii_endline(struct termp *);
static	void		  ascii_letter(struct termp *, int);
static	void		  ascii_setwidth(struct termp *, int, size_t);

#if HAVE_WCHAR
static	void		  locale_advance(struct termp *, size_t);
static	void		  locale_endline(struct termp *);
static	void		  locale_letter(struct termp *, int);
static	size_t		  locale_width(const struct termp *, int);
#endif


static struct termp *
ascii_init(enum termenc enc, const struct mchars *mchars, char *outopts)
{
	const char	*toks[5];
	char		*v;
	struct termp	*p;
	const char	*errstr;
	int		num;

	p = mandoc_calloc(1, sizeof(struct termp));

	p->symtab = mchars;
	p->tabwidth = 5;
	p->defrmargin = p->lastrmargin = 78;
	p->fontq = mandoc_reallocarray(NULL,
	     (p->fontsz = 8), sizeof(enum termfont));
	p->fontq[0] = p->fontl = TERMFONT_NONE;

	p->begin = ascii_begin;
	p->end = ascii_end;
	p->hspan = ascii_hspan;
	p->type = TERMTYPE_CHAR;

	p->enc = TERMENC_ASCII;
	p->advance = ascii_advance;
	p->endline = ascii_endline;
	p->letter = ascii_letter;
	p->setwidth = ascii_setwidth;
	p->width = ascii_width;

#if HAVE_WCHAR
	if (TERMENC_ASCII != enc) {
		v = TERMENC_LOCALE == enc ?
		    setlocale(LC_ALL, "") :
		    setlocale(LC_CTYPE, "en_US.UTF-8");
		if (NULL != v && MB_CUR_MAX > 1) {
			p->enc = enc;
			p->advance = locale_advance;
			p->endline = locale_endline;
			p->letter = locale_letter;
			p->width = locale_width;
		}
	}
#endif

	toks[0] = "indent";
	toks[1] = "width";
	toks[2] = "mdoc";
	toks[3] = "synopsis";
	toks[4] = NULL;

	while (outopts && *outopts)
		switch (getsubopt(&outopts, UNCONST(toks), &v)) {
		case 0:
			num = strtonum(v, 0, 1000, &errstr);
			if (!errstr)
				p->defindent = num;
			break;
		case 1:
			num = strtonum(v, 0, 1000, &errstr);
			if (!errstr)
				p->defrmargin = num;
			break;
		case 2:
			/*
			 * Temporary, undocumented mode
			 * to imitate mdoc(7) output style.
			 */
			p->mdocstyle = 1;
			p->defindent = 5;
			break;
		case 3:
			p->synopsisonly = 1;
			break;
		default:
			break;
		}

	/* Enforce a lower boundary. */
	if (p->defrmargin < 58)
		p->defrmargin = 58;

	return(p);
}

void *
ascii_alloc(const struct mchars *mchars, char *outopts)
{

	return(ascii_init(TERMENC_ASCII, mchars, outopts));
}

void *
utf8_alloc(const struct mchars *mchars, char *outopts)
{

	return(ascii_init(TERMENC_UTF8, mchars, outopts));
}

void *
locale_alloc(const struct mchars *mchars, char *outopts)
{

	return(ascii_init(TERMENC_LOCALE, mchars, outopts));
}

static void
ascii_setwidth(struct termp *p, int iop, size_t width)
{

	p->rmargin = p->defrmargin;
	if (iop > 0)
		p->defrmargin += width;
	else if (iop == 0)
		p->defrmargin = width ? width : p->lastrmargin;
	else if (p->defrmargin > width)
		p->defrmargin -= width;
	else
		p->defrmargin = 0;
	p->lastrmargin = p->rmargin;
	p->rmargin = p->maxrmargin = p->defrmargin;
}

void
ascii_sepline(void *arg)
{
	struct termp	*p;
	size_t		 i;

	p = (struct termp *)arg;
	putchar('\n');
	for (i = 0; i < p->defrmargin; i++)
		putchar('-');
	putchar('\n');
	putchar('\n');
}

static size_t
ascii_width(const struct termp *p, int c)
{

	return(1);
}

void
ascii_free(void *arg)
{

	term_free((struct termp *)arg);
}

static void
ascii_letter(struct termp *p, int c)
{

	putchar(c);
}

static void
ascii_begin(struct termp *p)
{

	(*p->headf)(p, p->argf);
}

static void
ascii_end(struct termp *p)
{

	(*p->footf)(p, p->argf);
}

static void
ascii_endline(struct termp *p)
{

	putchar('\n');
}

static void
ascii_advance(struct termp *p, size_t len)
{
	size_t		i;

	for (i = 0; i < len; i++)
		putchar(' ');
}

static double
ascii_hspan(const struct termp *p, const struct roffsu *su)
{
	double		 r;

	/*
	 * Approximate based on character width.
	 * None of these will be actually correct given that an inch on
	 * the screen depends on character size, terminal, etc., etc.
	 */
	switch (su->unit) {
	case SCALE_BU:
		r = su->scale * 10.0 / 240.0;
		break;
	case SCALE_CM:
		r = su->scale * 10.0 / 2.54;
		break;
	case SCALE_FS:
		r = su->scale * 2730.666;
		break;
	case SCALE_IN:
		r = su->scale * 10.0;
		break;
	case SCALE_MM:
		r = su->scale / 100.0;
		break;
	case SCALE_PC:
		r = su->scale * 10.0 / 6.0;
		break;
	case SCALE_PT:
		r = su->scale * 10.0 / 72.0;
		break;
	case SCALE_VS:
		r = su->scale * 2.0 - 1.0;
		break;
	case SCALE_EN:
		/* FALLTHROUGH */
	case SCALE_EM:
		r = su->scale;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(r);
}

const char *
ascii_uc2str(int uc)
{
	static const char nbrsp[2] = { ASCII_NBRSP, '\0' };
	static const char *tab[] = {
	"<NUL>","<SOH>","<STX>","<ETX>","<EOT>","<ENQ>","<ACK>","<BEL>",
	"<BS>",	"\t",	"<LF>",	"<VT>",	"<FF>",	"<CR>",	"<SO>",	"<SI>",
	"<DLE>","<DC1>","<DC2>","<DC3>","<DC4>","<NAK>","<SYN>","<ETB>",
	"<CAN>","<EM>",	"<SUB>","<ESC>","<FS>",	"<GS>",	"<RS>",	"<US>",
	" ",	"!",	"\"",	"#",	"$",	"%",	"&",	"'",
	"(",	")",	"*",	"+",	",",	"-",	".",	"/",
	"0",	"1",	"2",	"3",	"4",	"5",	"6",	"7",
	"8",	"9",	":",	";",	"<",	"=",	">",	"?",
	"@",	"A",	"B",	"C",	"D",	"E",	"F",	"G",
	"H",	"I",	"J",	"K",	"L",	"M",	"N",	"O",
	"P",	"Q",	"R",	"S",	"T",	"U",	"V",	"W",
	"X",	"Y",	"Z",	"[",	"\\",	"]",	"^",	"_",
	"`",	"a",	"b",	"c",	"d",	"e",	"f",	"g",
	"h",	"i",	"j",	"k",	"l",	"m",	"n",	"o",
	"p",	"q",	"r",	"s",	"t",	"u",	"v",	"w",
	"x",	"y",	"z",	"{",	"|",	"}",	"~",	"<DEL>",
	"<80>",	"<81>",	"<82>",	"<83>",	"<84>",	"<85>",	"<86>",	"<87>",
	"<88>",	"<89>",	"<8A>",	"<8B>",	"<8C>",	"<8D>",	"<8E>",	"<8F>",
	"<90>",	"<91>",	"<92>",	"<93>",	"<94>",	"<95>",	"<96>",	"<97>",
	"<99>",	"<99>",	"<9A>",	"<9B>",	"<9C>",	"<9D>",	"<9E>",	"<9F>",
	nbrsp,	"!",	"/\bc",	"GBP",	"o\bx",	"=\bY",	"|",	"<sec>",
	"\"",	"(C)",	"_\ba",	"<<",	"~",	"",	"(R)",	"-",
	"<deg>","+-",	"2",	"3",	"'",	",\bu",	"<par>",".",
	",",	"1",	"_\bo",	">>",	"1/4",	"1/2",	"3/4",	"?",
	"`\bA",	"'\bA",	"^\bA",	"~\bA",	"\"\bA","o\bA",	"AE",	",\bC",
	"`\bE",	"'\bE",	"^\bE",	"\"\bE","`\bI",	"'\bI",	"^\bI",	"\"\bI",
	"-\bD",	"~\bN",	"`\bO",	"'\bO",	"^\bO",	"~\bO",	"\"\bO","x",
	"/\bO",	"`\bU",	"'\bU",	"^\bU",	"\"\bU","'\bY",	"Th",	"ss",
	"`\ba",	"'\ba",	"^\ba",	"~\ba",	"\"\ba","o\ba",	"ae",	",\bc",
	"`\be",	"'\be",	"^\be",	"\"\be","`\bi",	"'\bi",	"^\bi",	"\"\bi",
	"d",	"~\bn",	"`\bo",	"'\bo",	"^\bo",	"~\bo",	"\"\bo","-:-",
	"/\bo",	"`\bu",	"'\bu",	"^\bu",	"\"\bu","'\by",	"th",	"\"\by",
	"A",	"a",	"A",	"a",	"A",	"a",	"'\bC",	"'\bc",
	"^\bC",	"^\bc",	"C",	"c",	"C",	"c",	"D",	"d",
	"/\bD",	"/\bd",	"E",	"e",	"E",	"e",	"E",	"e",
	"E",	"e",	"E",	"e",	"^\bG",	"^\bg",	"G",	"g",
	"G",	"g",	",\bG",	",\bg",	"^\bH",	"^\bh",	"/\bH",	"/\bh",
	"~\bI",	"~\bi",	"I",	"i",	"I",	"i",	"I",	"i",
	"I",	"i",	"IJ",	"ij",	"^\bJ",	"^\bj",	",\bK",	",\bk",
	"q",	"'\bL",	"'\bl",	",\bL",	",\bl",	"L",	"l",	"L",
	"l",	"/\bL",	"/\bl",	"'\bN",	"'\bn",	",\bN",	",\bn",	"N",
	"n",	"'n",	"Ng",	"ng",	"O",	"o",	"O",	"o",
	"O",	"o",	"OE",	"oe",	"'\bR",	"'\br",	",\bR",	",\br",
	"R",	"r",	"'\bS",	"'\bs",	"^\bS",	"^\bs",	",\bS",	",\bs",
	"S",	"s",	",\bT",	",\bt",	"T",	"t",	"/\bT",	"/\bt",
	"~\bU",	"~\bu",	"U",	"u",	"U",	"u",	"U",	"u",
	"U",	"u",	"U",	"u",	"^\bW",	"^\bw",	"^\bY",	"^\by",
	"\"\bY","'\bZ",	"'\bz",	"Z",	"z",	"Z",	"z",	"s",
	"b",	"B",	"B",	"b",	"6",	"6",	"O",	"C",
	"c",	"D",	"D",	"D",	"d",	"d",	"3",	"@",
	"E",	"F",	",\bf",	"G",	"G",	"hv",	"I",	"/\bI",
	"K",	"k",	"/\bl",	"l",	"W",	"N",	"n",	"~\bO",
	"O",	"o",	"OI",	"oi",	"P",	"p",	"YR",	"2",
	"2",	"SH",	"sh",	"t",	"T",	"t",	"T",	"U",
	"u",	"Y",	"V",	"Y",	"y",	"/\bZ",	"/\bz",	"ZH",
	"ZH",	"zh",	"zh",	"/\b2",	"5",	"5",	"ts",	"w",
	"|",	"||",	"|=",	"!",	"DZ",	"Dz",	"dz",	"LJ",
	"Lj",	"lj",	"NJ",	"Nj",	"nj",	"A",	"a",	"I",
	"i",	"O",	"o",	"U",	"u",	"U",	"u",	"U",
	"u",	"U",	"u",	"U",	"u",	"@",	"A",	"a",
	"A",	"a",	"AE",	"ae",	"/\bG",	"/\bg",	"G",	"g",
	"K",	"k",	"O",	"o",	"O",	"o",	"ZH",	"zh",
	"j",	"DZ",	"Dz",	"dz",	"'\bG",	"'\bg",	"HV",	"W",
	"`\bN",	"`\bn",	"A",	"a",	"'\bAE","'\bae","O",	"o"};

	assert(uc >= 0);
	if ((size_t)uc < sizeof(tab)/sizeof(tab[0]))
		return(tab[uc]);
	return(mchars_uc2str(uc));
}

#if HAVE_WCHAR
static size_t
locale_width(const struct termp *p, int c)
{
	int		rc;

	if (c == ASCII_NBRSP)
		c = ' ';
	rc = wcwidth(c);
	if (rc < 0)
		rc = 0;
	return(rc);
}

static void
locale_advance(struct termp *p, size_t len)
{
	size_t		i;

	for (i = 0; i < len; i++)
		putwchar(L' ');
}

static void
locale_endline(struct termp *p)
{

	putwchar(L'\n');
}

static void
locale_letter(struct termp *p, int c)
{

	putwchar(c);
}
#endif
