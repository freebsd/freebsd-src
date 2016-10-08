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
 * Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)afm.c	1.65 (gritter) 1/14/10
 */

#include <stdlib.h>
#include <string.h>
#include "dev.h"
#include "afm.h"

extern	char		*chname;
extern	short		*chtab;
extern	int		nchtab;

extern	void	errprint(const char *, ...);

#if !defined (DPOST) && !defined (DUMP)
static	void	addkernpair(struct afmtab *, char *_line);
#endif

/*
 * This table maps troff special characters to PostScript names.
 */
const struct names {
	char	*trname;
	char	*psname;
} names[] = {
	{ "hy",	"hyphen" },
	{ "ct",	"cent" },
	{ "fi",	"fi" },
	{ "fi",	"f_i" },
	{ "fl",	"fl" },
	{ "fl",	"f_l" },
	{ "ff", "ff" },
	{ "ff", "f_f" },
	{ "Fi", "ffi" },
	{ "Fi", "f_f_i" },
	{ "Fl", "ffl" },
	{ "Fl", "f_f_l" },
	{ "dg",	"dagger" },
	{ "dd",	"daggerdbl" },
	{ "bu",	"bullet" },
	{ "de",	"ring" },
	{ "em",	"emdash" },
	{ "en",	"endash" },
	{ "sc",	"section" },
	{ "``",	"quotedblleft" },
	{ "''",	"quotedblright" },
	{ "12",	"onehalf" },
	{ "14",	"onequarter" },
	{ "34",	"threequarters" },
	{ "aq", "quotesingle" },
	{ "oq", "quoteleft" },
	{ "cq", "quoteright" },
	{ 0,	0 }
};

/*
 * Names for Symbol fonts only.
 */
const struct names greeknames[] = {
	{ "*A",	"Alpha" },
	{ "*B",	"Beta" },
	{ "*C",	"Xi" },
	{ "*D",	"Delta" },
	{ "*E",	"Epsilon" },
	{ "*F",	"Phi" },
	{ "*G",	"Gamma" },
	{ "*H",	"Theta" },
	{ "*I",	"Iota" },
	{ "*K",	"Kappa" },
	{ "*L",	"Lambda" },
	{ "*M",	"Mu" },
	{ "*N",	"Nu" },
	{ "*O",	"Omicron" },
	{ "*P",	"Pi" },
	{ "*Q",	"Psi" },
	{ "*R",	"Rho" },
	{ "*S",	"Sigma" },
	{ "*T",	"Tau" },
	{ "*U",	"Upsilon" },
	{ "*W",	"Omega" },
	{ "*X",	"Chi" },
	{ "*Y",	"Eta" },
	{ "*Z",	"Zeta" },
	{ "*a",	"alpha" },
	{ "*b",	"beta" },
	{ "*c",	"xi" },
	{ "*d",	"delta" },
	{ "*e",	"epsilon" },
	{ "*f",	"phi" },
	{ "*g",	"gamma" },
	{ "*h",	"theta" },
	{ "*i",	"iota" },
	{ "*k",	"kappa" },
	{ "*l",	"lambda" },
	{ "*m",	"mu" },
	{ "*n",	"nu" },
	{ "*o",	"omicron" },
	{ "*p",	"pi" },
	{ "*q",	"psi" },
	{ "*r",	"rho" },
	{ "*s",	"sigma" },
	{ "*t",	"tau" },
	{ "*u",	"upsilon" },
	{ "*w",	"omega" },
	{ "*x",	"chi" },
	{ "*y",	"eta" },
	{ "*z",	"zeta" },
	{ 0,	0 }
};

const struct names mathnames[] = {
	{ "!=",	"notequal" },
	{ "**",	"asteriskmath" },
	{ "+-",	"plusminus" },
	{ "->",	"arrowright" },
	{ "<",	"less" },
	{ "<-",	"arrowleft" },
	{ "<=",	"lessequal" },
	{ "==",	"equivalence" },
	{ ">",	"greater" },
	{ ">=",	"greaterequal" },
	{ "O+",	"circleplus" },
	{ "Ox",	"circlemultiply" },
	{ "^",	"logicaland" },
	{ "al",	"aleph" },
	{ "ap",	"similar" },
	{ "bu",	"bullet" },
	{ "ca",	"intersection" },
	{ "co",	"copyrightserif" },
	{ "cu",	"union" },
	{ "da",	"arrowdown" },
	{ "de",	"degree" },
	{ "di",	"divide" },
	{ "eq",	"equal" },
	{ "es",	"emptyset" },
	{ "fa",	"universal" },
	{ "fm",	"minute" },
	{ "gr",	"gradient" },
	{ "ib",	"reflexsubset" },
	{ "if",	"infinity" },
	{ "ip",	"reflexsuperset" },
	{ "is",	"integral" },
	{ "mi",	"minus" },
	{ "mo",	"element" },
	{ "mu",	"multiply" },
	{ "no",	"logicalnot" },
	{ "or",	"bar" },
	{ "or",	"logicalor" },
	{ "pd",	"partialdiff" },
	{ "pl",	"plus" },
	{ "pt",	"proportional" },
	{ "rg",	"registerserif" },
	{ "sb",	"propersubset" },
	{ "sl",	"fraction" },
	{ "sp",	"propersuperset" },
	{ "sr",	"radical" },
	{ "te",	"existential" },
	{ "tm",	"trademarkserif" },
	{ "ts",	"sigma1" },
	{ "ua",	"arrowup" },
	{ "~~",	"approxequal" },
	{ 0,	0 },
};

const struct names largenames[] = {
	{ "bv",	"braceex" },
	{ "lb",	"braceleftbt" },
	{ "lc",	"bracketlefttp" },
	{ "lf",	"bracketleftbt" },
	{ "lk",	"braceleftmid" },
	{ "lt",	"bracelefttp" },
	{ "rb",	"bracerightbt" },
	{ "rc",	"bracketrighttp" },
	{ "rf",	"bracketrightbt" },
	{ "rk",	"bracerightmid" },
	{ "rn",	"radicalex" },
	{ "rt",	"bracerighttp" },
	{ 0,	0 }
};

const struct names punctnames[] = {
	{ "or",	"bar" },
	{ "\\-","endash" },
	{ "aa","acute" },
	{ "ga","grave" },
	{ "rs","backslash" },
	{ "dq","quotedbl" },
	{ 0,	0 }
};

/*
 * These names are only used with the S font.
 */
const struct names Snames[] = {
	{ "br",	"parenleftex" },
	{ "ul",	"underscore" },
	{ "vr",	"bracketleftex" },
	{ 0,	0 }
};

/*
 * These names are only used with the S1 font.
 */
const struct names S1names[] = {
	{ "ru",	"underscore" },
	{ 0,	0 }
};

/*
 * Figures from charlib.
 */
#define	NCHARLIB	16
static const struct charlib {
	const char	*name;
	int		width;
	int		kern;
	int		code;
	int		symbol;
	enum {
		NEEDS_F = 01,
		NEEDS_I = 02,
		NEEDS_L = 04
	}		needs;
} charlib[] = {
	{ "bx",	 500, 2, 1, 0, 0 },
	{ "ci",	 750, 0, 1, 0, 0 },
	{ "sq",	 500, 2, 1, 0, 0 },
	{ "ff",	 600, 2, 1, 0, 1 },	/* widths of the ligatures */
	{ "Fi",	 840, 2, 1, 0, 3 },	/* are likely wrong, but */
	{ "Fl",	 840, 2, 1, 0, 5 },	/* they are normally not used */
	{ "~=",	 550, 0, 1, 1, 0 },
	{ "L1", 1100, 1, 2, 1, 0 },
	{ "LA",	1100, 1, 2, 1, 0 },
	{ "LV",	1100, 3, 2, 1, 0 },
	{ "LH",	2100, 1, 2, 1, 0 },
	{ "Lb",	2100, 1, 2, 1, 0 },
	{ "lh",	1000, 0, 2, 1, 0 },
	{ "rh",	1000, 0, 2, 1, 0 },
	{ "Sl",	 500, 2, 1, 1, 0 },
	{ "ob",	 380, 0, 1, 1, 0 },
	{    0,	   0, 0, 0, 0, 0 }
};

/*
 * The values in this table determine if a character that is found on an
 * ASCII position in a PostScript font actually is that ASCII character.
 * If not, the position in fitab remains empty, and the fallback sequence
 * is used to find it in another font.
 */
static const struct asciimap {
	int		code;
	const char	*psc;
} asciimap[] = {
	{ 0x0020,	"space" },
	{ 0x0021,	"exclam" },
	{ 0x0024,	"dollar" },
	{ 0x0024,	"dollaralt" },		/* FournierMT-RegularAlt */
	{ 0x0025,	"percent" },
	{ 0x0026,	"ampersand" },
	{ 0x0026,	"ampersandalt" },	/* AGaramondAlt-Italic */
	{ 0x0027,	"quoteright" },
	{ 0x0028,	"parenleft" },
	{ 0x0029,	"parenright" },
	{ 0x002A,	"asterisk" },
	{ 0x002B,	"plus" },
	{ 0x002C,	"comma" },
	{ 0x002D,	"hyphen" },
	{ 0x002E,	"period" },
	{ 0x002F,	"slash" },
	{ 0x0030,	"zero" },
	{ 0x0030,	"zerooldstyle" },
	{ 0x0030,	"zeroalt" },		/* BulmerMT-RegularAlt */
	{ 0x0031,	"one" },
	{ 0x0031,	"oneoldstyle" },
	{ 0x0031,	"onefitted" },
	{ 0x0031,	"onealtfitted" },	/* BulmerMT-ItalicAlt */
	{ 0x0032,	"two" },
	{ 0x0032,	"twooldstyle" },
	{ 0x0032,	"twoalt" },		/* BulmerMT-RegularAlt */
	{ 0x0033,	"three" },
	{ 0x0033,	"threeoldstyle" },
	{ 0x0033,	"threealt" },		/* BulmerMT-RegularAlt */
	{ 0x0034,	"four" },
	{ 0x0034,	"fouroldstyle" },
	{ 0x0034,	"fouralt" },		/* BulmerMT-RegularAlt */
	{ 0x0035,	"five" },
	{ 0x0035,	"fiveoldstyle" },
	{ 0x0035,	"fivealt" },		/* BulmerMT-RegularAlt */
	{ 0x0036,	"six" },
	{ 0x0036,	"sixoldstyle" },
	{ 0x0036,	"sixalt" },		/* BulmerMT-RegularAlt */
	{ 0x0037,	"seven" },
	{ 0x0037,	"sevenoldstyle" },
	{ 0x0037,	"sevenalt" },		/* BulmerMT-RegularAlt */
	{ 0x0038,	"eight" },
	{ 0x0038,	"eightoldstyle" },
	{ 0x0038,	"eightalt" },		/* BulmerMT-RegularAlt */
	{ 0x0039,	"nine" },
	{ 0x0039,	"nineoldstyle" },
	{ 0x0039,	"ninealt" },		/* BulmerMT-RegularAlt */
	{ 0x003A,	"colon" },
	{ 0x003B,	"semicolon" },
	{ 0x003D,	"equal" },
	{ 0x003F,	"question" },
	{ 0x0041,	"A" },
	{ 0x0041,	"Aswash" },		/* AGaramondAlt-Italic */
	{ 0x0042,	"B" },
	{ 0x0042,	"Bswash" },		/* AGaramondAlt-Italic */
	{ 0x0043,	"C" },
	{ 0x0043,	"Cswash" },		/* AGaramondAlt-Italic */
	{ 0x0044,	"D" },
	{ 0x0044,	"Dswash" },		/* AGaramondAlt-Italic */
	{ 0x0045,	"E" },
	{ 0x0045,	"Eswash" },		/* AGaramondAlt-Italic */
	{ 0x0046,	"F" },
	{ 0x0046,	"Fswash" },		/* AGaramondAlt-Italic */
	{ 0x0047,	"G" },
	{ 0x0047,	"Gswash" },		/* AGaramondAlt-Italic */
	{ 0x0048,	"H" },
	{ 0x0048,	"Hswash" },		/* AGaramondAlt-Italic */
	{ 0x0049,	"I" },
	{ 0x0049,	"Iswash" },		/* AGaramondAlt-Italic */
	{ 0x004A,	"J" },
	{ 0x004A,	"Jalt" },		/* FournierMT-RegularAlt */
	{ 0x004A,	"Jalttwo" },		/* BulmerMT-ItalicAlt */
	{ 0x004A,	"Jswash" },		/* AGaramondAlt-Italic */
	{ 0x004A,	"JTallCapalt" },	/* FournierMT-RegularAlt */
	{ 0x004B,	"K" },
	{ 0x004B,	"Kalt" },		/* BulmerMT-ItalicAlt */
	{ 0x004B,	"Kswash" },		/* AGaramondAlt-Italic */
	{ 0x004C,	"L" },
	{ 0x004C,	"Lswash" },		/* AGaramondAlt-Italic */
	{ 0x004D,	"M" },
	{ 0x004D,	"Mswash" },		/* AGaramondAlt-Italic */
	{ 0x004E,	"N" },
	{ 0x004E,	"Nalt" },		/* BulmerMT-ItalicAlt */
	{ 0x004E,	"Nswash" },		/* AGaramondAlt-Italic */
	{ 0x004F,	"O" },
	{ 0x004F,	"Oalt" },		/* BulmerMT-ItalicAlt */
	{ 0x004F,	"Oswash" },		/* AGaramondAlt-Italic */
	{ 0x0050,	"P" },
	{ 0x0050,	"Pswash" },		/* AGaramondAlt-Italic */
	{ 0x0051,	"Q" },
	{ 0x0051,	"Qalt" },		/* FournierMT-RegularAlt */
	{ 0x0051,	"Qalttitling" },	/* AGaramondAlt-Regular */
	{ 0x0051,	"Qswash" },		/* AGaramondAlt-Italic */
	{ 0x0051,	"QTallCapalt" },	/* FournierMT-RegularAlt */
	{ 0x0052,	"R" },
	{ 0x0052,	"Ralternate" },		/* Bembo-Alt */
	{ 0x0052,	"Rswash" },		/* AGaramondAlt-Italic */
	{ 0x0053,	"S" },
	{ 0x0053,	"Sswash" },		/* AGaramondAlt-Italic */
	{ 0x0054,	"T" },
	{ 0x0054,	"Talt" },		/* BulmerMT-ItalicAlt */
	{ 0x0054,	"Tswash" },		/* AGaramondAlt-Italic */
	{ 0x0055,	"U" },
	{ 0x0055,	"Uswash" },		/* AGaramondAlt-Italic */
	{ 0x0056,	"V" },
	{ 0x0056,	"Vswash" },		/* AGaramondAlt-Italic */
	{ 0x0057,	"W" },
	{ 0x0057,	"Wswash" },		/* AGaramondAlt-Italic */
	{ 0x0058,	"X" },
	{ 0x0058,	"Xswash" },		/* AGaramondAlt-Italic */
	{ 0x0059,	"Y" },
	{ 0x0059,	"Yalt" },		/* BulmerMT-ItalicAlt */
	{ 0x0059,	"Yswash" },		/* AGaramondAlt-Italic */
	{ 0x005A,	"Z" },
	{ 0x005A,	"Zswash" },		/* AGaramondAlt-Italic */
	{ 0x005B,	"bracketleft" },
	{ 0x005D,	"bracketright" },
	{ 0x005F,	"underscore" },
	{ 0x0060,	"quoteleft" },
	{ 0x0060,	"quotealtleft" },	/* BulmerMT-RegularAlt */
	{ 0x0061,	"a" },
	{ 0x0061,	"Asmall" },
	{ 0x0061,	"aswash" },		/* AGaramondAlt-Regular */
	{ 0x0062,	"b" },
	{ 0x0062,	"Bsmall" },
	{ 0x0063,	"c" },
	{ 0x0063,	"Csmall" },
	{ 0x0064,	"d" },
	{ 0x0064,	"Dsmall" },
	{ 0x0065,	"e" },
	{ 0x0065,	"Esmall" },
	{ 0x0065,	"eswash" },		/* AGaramondAlt-Regular */
	{ 0x0066,	"f" },
	{ 0x0066,	"Fsmall" },
	{ 0x0067,	"g" },
	{ 0x0067,	"Gsmall" },
	{ 0x0068,	"h" },
	{ 0x0068,	"Hsmall" },
	{ 0x0069,	"i" },
	{ 0x0069,	"Ismall" },
	{ 0x006A,	"j" },
	{ 0x006A,	"Jsmall" },
	{ 0x006A,	"Jsmallalt" },		/* FournierMT-RegularAlt */
	{ 0x006B,	"k" },
	{ 0x006B,	"Ksmall" },
	{ 0x006C,	"l" },
	{ 0x006C,	"Lsmall" },
	{ 0x006D,	"m" },
	{ 0x006D,	"Msmall" },
	{ 0x006E,	"n" },
	{ 0x006E,	"Nsmall" },
	{ 0x006E,	"nswash" },		/* AGaramondAlt-Regular */
	{ 0x006F,	"o" },
	{ 0x006F,	"Osmall" },
	{ 0x0070,	"p" },
	{ 0x0070,	"Psmall" },
	{ 0x0071,	"q" },
	{ 0x0071,	"Qsmall" },
	{ 0x0072,	"r" },
	{ 0x0072,	"Rsmall" },
	{ 0x0072,	"rswash" },		/* AGaramondAlt-Regular */
	{ 0x0073,	"s" },
	{ 0x0073,	"Ssmall" },
	{ 0x0074,	"t" },
	{ 0x0074,	"Tsmall" },
	{ 0x0074,	"tswash" },		/* AGaramondAlt-Regular */
	{ 0x0074,	"tswashalt" },		/* AGaramondAlt-Regular */
	{ 0x0075,	"u" },
	{ 0x0075,	"Usmall" },
	{ 0x0076,	"v" },
	{ 0x0076,	"Vsmall" },
	{ 0x0076,	"vswash" },		/* AGaramondAlt-Italic */
	{ 0x0077,	"w" },
	{ 0x0077,	"Wsmall" },
	{ 0x0077,	"walt" },		/* FournierMT-RegularAlt */
	{ 0x0078,	"x" },
	{ 0x0078,	"Xsmall" },
	{ 0x0079,	"y" },
	{ 0x0079,	"Ysmall" },
	{ 0x007A,	"z" },
	{ 0x007A,	"Zsmall" },
	{ 0x007A,	"zalt" },		/* FournierMT-ItalicAlt */
	{ 0x007A,	"zswash" },		/* AGaramondAlt-Regular */
	{ 0x007B,	"braceleft" },
 	{ 0x007C,	"bar" },
	{ 0x007D,	"braceright" },
	{ 0,		0 }
};

/*
 * ASCII characters that are always taken from the S (math) font.
 */
static const struct asciimap	mathascii[] = {
	{ 0x002D,	"minus" },
	{ 0x007E,	"similar" },
	{ 0,		0 }
};

/*
 * ASCII characters that are always taken from the S1 (punct) font.
 */
static const struct asciimap	punctascii[] = {
 	{ 0x0022,	"quotedbl" },
 	{ 0x0023,	"numbersign" },
 	{ 0x003C,	"less" },
 	{ 0x003E,	"greater" },
 	{ 0x0040,	"at" },
 	{ 0x005C,	"backslash" },
 	{ 0x005E,	"circumflex" },
 	{ 0x007E,	"tilde" },
	{ 0,		NULL }
};

int
nextprime(int n)
{
	const int	primes[] = {
		509, 1021, 2039, 4093, 8191, 16381, 32749, 65521,
		131071, 262139, 524287, 1048573, 2097143, 4194301,
		8388593, 16777213, 33554393, 67108859, 134217689,
		268435399, 536870909, 1073741789, 2147483647
	};
	int	mprime = 7;
	int	i;

	for (i = 0; i < sizeof primes / sizeof *primes; i++)
		if ((mprime = primes[i]) >= (n < 65536 ? n*4 :
					n < 262144 ? n*2 : n))
			break;
	if (i == sizeof primes / sizeof *primes)
		mprime = n;     /* not so prime, but better than failure */
	return mprime;
}

unsigned
pjw(const char *cp)
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

struct namecache *
afmnamelook(struct afmtab *a, const char *name)
{
	struct namecache	*np;
	unsigned	h, c, n = 0;

	h = pjw(name) % a->nameprime;
	np = &a->namecache[c = h];
	while (np->afpos != 0) {
		if (a->nametab[np->afpos] == 0 ||
				strcmp(a->nametab[np->afpos], name) == 0)
			break;
		h = (n + 1) / 2;
		h *= h;
		if (n & 1)
			c -= h;
		else
			c += h;
		n++;
		while (c >= a->nameprime)
			c -= a->nameprime;
		np = &a->namecache[c];
	}
	return np;
}

static int
mapname1(const char *psname, const struct names *np)
{
	int	i, j;

	for (i = 0; np[i].trname; i++)
		if (strcmp(np[i].psname, psname) == 0)
			break;
	if (np[i].trname)
		for (j = 0; j < nchtab; j++)
			if (strcmp(np[i].trname, &chname[chtab[j]]) == 0)
				return j + 128;
	return 0;
}

int
afmmapname(const char *psname, enum spec s)
{
	int	k;

	if (s & SPEC_S && (k = mapname1(psname, Snames)) > 0)
		return k;
	if (s & SPEC_S1 && (k = mapname1(psname, S1names)) > 0)
		return k;
	if (s & (SPEC_MATH|SPEC_S) && (k = mapname1(psname, mathnames)) > 0)
		return k;
	if (s & (SPEC_GREEK|SPEC_S) && (k = mapname1(psname, greeknames)) > 0)
		return k;
	if (s & (SPEC_LARGE|SPEC_S) && (k = mapname1(psname, largenames)) > 0)
		return k;
	if (s & (SPEC_PUNCT|SPEC_S1) && (k = mapname1(psname, punctnames)) > 0)
		return k;
	return mapname1(psname, names);
}

/*
 * After all characters have been read, construct a font-specific
 * encoding for the rest. Also move the name table to permanent space.
 */
void
afmremap(struct afmtab *a)
{
	int	i, j = 128 - 32 + nchtab;
	char	*space, *tp;
	struct namecache	*np;

	for (i = 1; i < a->nchars; i++) {
		if (a->codetab[i] == NOCODE && a->nametab[i] != NULL) {
#if defined (DPOST) || defined (DUMP)
			while (a->fitab[j] != 0)
				j++;
#else	/* TROFF */
			extern int	ps2cc(const char *);
			j = ps2cc(a->nametab[i]) + 128 - 32 + nchtab;
#endif	/* TROFF */
			a->fitab[j] = i;
			np = afmnamelook(a, a->nametab[i]);
			np->afpos = i;
			np->fival[0] = j;
			if (np->gid != 0 && a->gid2tr)
				a->gid2tr[np->gid].ch1 = j + 32 + nchtab + 128;
			if (strcmp(a->nametab[i], "space") == 0) {
				np->fival[1] = 0;
				if (np->gid != 0 && a->gid2tr)
					a->gid2tr[np->gid].ch2 = 32;
			}
		}
	}
	space = malloc(a->nspace);
	for (i = 0; i < a->nchars; i++) {
		if (a->nametab[i]) {
			tp = a->nametab[i];
			a->nametab[i] = space;
			while ((*space++ = *tp++));
		}
	}
}

#ifndef	DUMP
static int
asciiequiv(int code, const char *psc, enum spec s)
{
	int	i;

	if (psc != NULL) {
		for (i = 0; asciimap[i].psc; i++)
			if (strcmp(asciimap[i].psc, psc) == 0)
				return asciimap[i].code;
		if (s & (SPEC_MATH|SPEC_S)) {
			for (i = 0; mathascii[i].psc; i++)
				if (strcmp(mathascii[i].psc, psc) == 0)
					return mathascii[i].code;
		}
		if (s & (SPEC_PUNCT|SPEC_S1)) {
			for (i = 0; punctascii[i].psc; i++)
				if (strcmp(punctascii[i].psc, psc) == 0)
					return punctascii[i].code;
		}
	}
	return 0;
}

static char *
thisword(const char *text, const char *wrd)
{
	while (*text == *wrd)
		text++, wrd++;
	if (*wrd != 0)
		return 0;
	if (*text == 0 || *text == ' ' || *text == '\t' || *text == '\n' ||
			*text == '\r') {
		while (*text != 0 && (*text == ' ' || *text == '\t'))
			text++;
		return (char *)text;
	}
	return NULL;
}
#endif	/* !DUMP */

int
unitconv(int i)
{
	if (unitsPerEm * 72 != dev.unitwidth * dev.res)
		i = i * dev.unitwidth * dev.res / 72 / unitsPerEm;
	return i;
}

#ifndef	DUMP
void
afmaddchar(struct afmtab *a, int C, int tp, int cl, int WX, int B[4], char *N,
		enum spec s, int gid)
{
	struct namecache	*np = NULL;

	if (N != NULL) {
		np = afmnamelook(a, N);
		np->afpos = a->nchars;
		np->gid = gid;
		if (a->isFixedPitch && strcmp(N, "space") == 0)
			a->fontab[0] = _unitconv(WX);
	}
	a->fontab[a->nchars] = _unitconv(WX);
	if (B) {
		a->bbtab[a->nchars] = malloc(4 * sizeof **a->bbtab);
		a->bbtab[a->nchars][0] = _unitconv(B[0]);
		a->bbtab[a->nchars][1] = _unitconv(B[1]);
		a->bbtab[a->nchars][2] = _unitconv(B[2]);
		a->bbtab[a->nchars][3] = _unitconv(B[3]);
	/*
	 * Crude heuristics mainly based on observations with the existing
	 * fonts for -Tpost and on tests with eqn.
	 */
		if (B[1] <= -10)
			a->kerntab[a->nchars] |= 1;
		if (B[3] > (a->xheight + a->capheight) / 2)
			a->kerntab[a->nchars] |= 2;
	}
	/*
	 * Only map a character directly if it maps to an ASCII
	 * equivalent or to a troff special character.
	 */
	C = asciiequiv(C, N, s);
	if (cl)
		a->codetab[a->nchars] = cl;
	else if (tp)
		a->codetab[a->nchars] = tp;
	else if (C > 32 && C < 127 && a->fitab[C - 32] == 0)
		a->codetab[a->nchars] = C;
	else
		a->codetab[a->nchars] = NOCODE;
	if (C > 32 && C < 127 && a->fitab[C - 32] == 0) {
		a->fitab[C - 32] = a->nchars;
		if (gid && a->gid2tr)
			a->gid2tr[gid].ch1 = C;
		if (np)
			np->fival[0] = C - 32;
	} else if (C == 32 && np)
		np->fival[0] = 0;
	if (tp) {
		a->fitab[tp - 32] = a->nchars;
		if (gid && a->gid2tr)
			a->gid2tr[gid].ch2 = tp;
		if (np)
			np->fival[1] = tp - 32;
	}
	a->nametab[a->nchars] = N;
	a->nchars++;
}

/*
 * Add charlib figues.
 */
static void
addcharlib(struct afmtab *a, int symbol)
{
	int	i, j;
	int	B[4] = { 0, 0, 0, 0 };

	for (j = 0; j < nchtab; j++)
		for (i = 0; charlib[i].name; i++) {
			if (charlib[i].symbol && !symbol)
				continue;
			if (charlib[i].needs & NEEDS_F && a->fitab['f'-32] == 0)
				continue;
			if (charlib[i].needs & NEEDS_I && a->fitab['i'-32] == 0)
				continue;
			if (charlib[i].needs & NEEDS_L && a->fitab['l'-32] == 0)
				continue;
			if (strcmp(charlib[i].name, &chname[chtab[j]]) == 0) {
				B[1] = charlib[i].kern & 1 ? -11 : 0;
				B[3] = charlib[i].kern & 2 ?
					a->capheight + 1 : 0;
				afmaddchar(a, NOCODE, j+128, charlib[i].code,
					charlib[i].width * unitsPerEm / 1024,
					B, NULL, SPEC_NONE, 0);
			}
		}
}

static void
addmetrics(struct afmtab *a, char *_line, enum spec s)
{
	char	*lp = _line, c, *xp;
	int	C = NOCODE, WX = 0, tp;
	char	*N = NULL;
	int	B[4] = { -1, -1, -1, -1 };

	while (*lp && *lp != '\n' && *lp != '\r') {
		switch (*lp) {
		case 'C':
			C = strtol(&lp[1], NULL, 10);
			break;
		case 'W':
			if (lp[1] == 'X')
				WX = strtol(&lp[2], NULL, 10);
			break;
		case 'N':
			for (N = &lp[1]; *N == ' ' || *N == '\t'; N++);
			for (lp = N; *lp && *lp != '\n' && *lp != '\r' &&
					*lp != ' ' && *lp != '\t' &&
					*lp != ';'; lp++);
			c = *lp;
			*lp++ = 0;
			a->nspace += lp - N;
			if (c == ';')
				continue;
			break;
		case 'B':
			xp = &lp[1];
			B[0] = strtol(xp, &xp, 10);
			B[1] = strtol(xp, &xp, 10);
			B[2] = strtol(xp, &xp, 10);
			B[3] = strtol(xp, &xp, 10);
			break;
		case 'L':
			if (C == 'f') {
				xp = &lp[1];
				while (*xp == ' ' || *xp == '\t')
					xp++;
				switch (*xp) {
				case 'i':
					a->Font.ligfont |= LFI;
					break;
				case 'l':
					a->Font.ligfont |= LFL;
					break;
				}
			}
			break;
		default:
			lp++;
		}
		while (*lp && *lp != '\n' && *lp != '\r' && *lp != ';')
			lp++;
		if (*lp == ';') {
			while (*lp && *lp != '\n' && *lp != '\r' &&
					(*lp == ' ' || *lp == '\t' ||
					 *lp == ';'))
				lp++;
		}
	}
	if (N == NULL)
		return;
	tp = afmmapname(N, s);
	afmaddchar(a, C, tp, 0, WX, B, N, s, 0);
}

void
afmalloc(struct afmtab *a, int n)
{
	int	i;

#if defined (DPOST) || defined (DUMP)
	a->fichars = n+NCHARLIB+1 + 128 - 32 + nchtab;
#else	/* TROFF */
	extern int	psmaxcode;
	a->fichars = n+NCHARLIB+1 + 128 - 32 + nchtab + psmaxcode+1;
#endif	/* TROFF */
	a->fitab = calloc(a->fichars, sizeof *a->fitab);
	a->fontab = malloc((n+NCHARLIB+1)*sizeof *a->fontab);
	a->fontab[0] = dev.res * dev.unitwidth / 72 / 3;
	a->bbtab = calloc(n+NCHARLIB+1, sizeof *a->bbtab);
	a->kerntab = calloc(n+NCHARLIB+1, sizeof *a->kerntab);
	a->codetab = malloc((n+NCHARLIB+1)*sizeof *a->codetab);
	a->codetab[0] = 0;
	for (i = 1; i < n+NCHARLIB+1; i++)
		a->codetab[i] = NOCODE;
	a->nametab = malloc((n+NCHARLIB+1)*sizeof *a->nametab);
	a->nametab[0] = 0;
	a->nchars = 1;
	addcharlib(a, (a->base[0]=='S' && a->base[1]==0) || a->spec&SPEC_S);
	a->nameprime = nextprime(n+NCHARLIB+1);
	a->namecache = calloc(a->nameprime, sizeof *a->namecache);
	for (i = 0; i < a->nameprime; i++) {
		a->namecache[i].fival[0] = NOCODE;
		a->namecache[i].fival[1] = NOCODE;
	}
}

int
afmget(struct afmtab *a, char *contents, size_t size)
{
	enum {
		NONE,
		FONTMETRICS,
		CHARMETRICS,
		KERNDATA,
		KERNPAIRS
	} state = NONE;
	char	*cp, *th, *tp;
	int	n = 0;
	enum spec	s;
	size_t	l;

	if ((cp = strrchr(a->file, '/')) == NULL)
		cp = a->file;
	else
		cp++;
	l = strlen(cp) + 1;
	a->base = malloc(l);
	n_strcpy(a->base, cp, l);
	if ((cp = strrchr(a->base, '.')) != NULL)
		*cp = '\0';
	if (dev.allpunct)
		a->spec |= SPEC_PUNCT;
	if (a->base[0]=='S' && a->base[1]==0)
		a->spec |= SPEC_S;
	if (a->base[0]=='S' && a->base[1]=='1' && a->base[2]==0)
		a->spec |= SPEC_S1;
	s = a->spec;
	a->xheight = 500;
	a->capheight = 700;
	unitsPerEm = 1000;
	if (memcmp(contents, "OTTO", 4) == 0 ||
			memcmp(contents, "\0\1\0\0", 4) == 0 ||
			memcmp(contents, "true", 4) == 0)
		return otfget(a, contents, size);
	a->lineno = 1;
	for (cp = contents; cp < &contents[size]; a->lineno++, cp++) {
		while (*cp == ' ' || *cp == '\t' || *cp == '\r')
			cp++;
		if (*cp == '\n')
			continue;
		if (thisword(cp, "Comment"))
			/*EMPTY*/;
		else if (state == NONE && thisword(cp, "StartFontMetrics"))
			state = FONTMETRICS;
		else if (state == FONTMETRICS && thisword(cp, "EndFontMetrics"))
			state = NONE;
		else if (state == FONTMETRICS &&
				(th = thisword(cp, "FontName")) != NULL) {
			for (tp = th; *tp && *tp != ' ' && *tp != '\t' &&
					*tp != '\n' && *tp != '\r'; tp++);
			a->fontname = malloc(tp - th + 1);
			memcpy(a->fontname, th, tp - th);
			a->fontname[tp - th] = 0;
		} else if (state == FONTMETRICS &&
				(th = thisword(cp, "IsFixedPitch")) != NULL) {
			a->isFixedPitch = strncmp(th, "true", 4) == 0;
		} else if (state == FONTMETRICS &&
				(th = thisword(cp, "XHeight")) != NULL) {
			a->xheight = strtol(th, NULL, 10);
		} else if (state == FONTMETRICS &&
				(th = thisword(cp, "CapHeight")) != NULL) {
			a->capheight = strtol(th, NULL, 10);
		} else if (state == FONTMETRICS &&
				(th = thisword(cp, "Ascender")) != NULL) {
			a->ascender = strtol(th, NULL, 10);
		} else if (state == FONTMETRICS &&
				(th = thisword(cp, "Descender")) != NULL) {
			a->descender = strtol(th, NULL, 10);
		} else if (state == FONTMETRICS &&
				(th = thisword(cp, "StartCharMetrics")) != 0) {
			n = strtol(th, NULL, 10);
			state = CHARMETRICS;
			afmalloc(a, n);
		} else if (state == CHARMETRICS &&
				thisword(cp, "EndCharMetrics")) {
			state = FONTMETRICS;
			afmremap(a);
		} else if (state == CHARMETRICS && n-- > 0) {
			addmetrics(a, cp, s);
#ifndef	DPOST
		} else if (state == FONTMETRICS &&
				thisword(cp, "StartKernData") != 0) {
			state = KERNDATA;
		} else if (state == KERNDATA &&
				(th = thisword(cp, "StartKernPairs")) != 0) {
			n = strtol(th, NULL, 10);
			state = KERNPAIRS;
		} else if (state == KERNPAIRS &&
				thisword(cp, "EndKernPairs")) {
			state = KERNDATA;
		} else if (state == KERNPAIRS && n-- > 0) {
			addkernpair(a, cp);
		} else if (state == KERNDATA &&
				thisword(cp, "EndKernData")) {
			state = FONTMETRICS;
#endif	/* !DPOST */
		}
		while (cp < &contents[size] && *cp != '\n')
			cp++;
	}
	if (a->fontname == NULL) {
		errprint("Missing \"FontName\" in %s", a->path);
		return -1;
	}
	a->Font.nwfont = a->nchars > 255 ? 255 : a->nchars;
	return 0;
}

/*
 * This is for legacy font support. It exists at this place because both
 * troff and dpost need it in combination with AFM support.
 */
void
makefont(int nf, char *devfontab, char *devkerntab, char *devcodetab,
		char *devfitab, int nw)
{
	int	i;

	free(fontab[nf]);
	free(kerntab[nf]);
	free(codetab[nf]);
	free(fitab[nf]);
	fontab[nf] = calloc(nw, sizeof *fontab);
	kerntab[nf] = calloc(nw, sizeof *kerntab);
	codetab[nf] = calloc(nw, sizeof *codetab);
	fitab[nf] = calloc(NCHARS ? NCHARS : 128 - 32 + nchtab, sizeof *fitab);
	if (devfontab) for (i = 0; i < nw; i++)
		fontab[nf][i] = devfontab[i]&0377;
	if (devkerntab) for (i = 0; i < nw; i++)
		kerntab[nf][i] = devkerntab[i]&0377;
	if (devcodetab) for (i = 0; i < nw; i++)
		codetab[nf][i] = devcodetab[i]&0377;
	if (devfitab) for (i = 0; i < 128 - 32 + nchtab; i++)
		fitab[nf][i] = devfitab[i]&0377;
	fontbase[nf]->spacewidth = fontab[nf][0];
	fontbase[nf]->cspacewidth = -1;
}

#ifndef	DPOST
/*
 * For short to medium-sized documents, the run time is dominated by
 * the time required to read kerning pairs for fonts with many pairs.
 * Kerning pairs are thus simply dumped in the order they occur at
 * this point. Later when a kerning pair is actually looked up, the
 * structures accessed are sorted (eliding duplicates), leading to
 * better performance with large documents.
 */
void
afmaddkernpair(struct afmtab *a, int ch1, int ch2, int k)
{
	struct kernpairs	*kp;

	if (k == 0)
		return;
	if (a->kernpairs == NULL)
		a->kernpairs = calloc(a->fichars, sizeof *a->kernpairs);
	kp = &a->kernpairs[ch1];
	while (kp->cnt == NKERNPAIRS) {
		if (kp->next == NULL)
			kp->next = calloc(1, sizeof *kp->next);
		kp = kp->next;
	}
	kp->ch2[kp->cnt] = ch2;
	kp->k[kp->cnt] = k;
	kp->cnt++;
}

static void
addkernpair(struct afmtab *a, char *_line)
{
	struct namecache	*np1, *np2;
	char	*lp = _line, c, *cp;
	int	n, i, j;

	if (lp[0] == 'K' && lp[1] == 'P') {
		lp += 2;
		if (*lp == 'X')
			lp++;
		while ((*lp && *lp == ' ') || *lp == '\t')
			lp++;
		cp = lp;
		while (*lp && *lp != '\n' && *lp != '\r' &&
					*lp != ' ' && *lp != '\t')
			lp++;
		if ((c = *lp) == 0)
			return;
		*lp = 0;
		np1 = afmnamelook(a, cp);
		*lp = c;
		while ((*lp && *lp == ' ') || *lp == '\t')
			lp++;
		cp = lp;
		while (*lp && *lp != '\n' && *lp != '\r' &&
				*lp != ' ' && *lp != '\t')
			lp++;
		if ((c = *lp) == 0)
			return;
		*lp = 0;
		np2 = afmnamelook(a, cp);
		*lp = c;
		n = _unitconv(strtol(&lp[1], NULL, 10));
		for (i = 0; i < 2; i++)
			if (np1->fival[i] != NOCODE)
				for (j = 0; j < 2; j++)
					if (np2->fival[j] != NOCODE)
						afmaddkernpair(a,
								np1->fival[i],
								np2->fival[j],
								n);
	}
}

static void
sortkernpairs(struct kernpairs *kp)
{
	int	i, j, s, t;
	do {
		s = 0;
		for (i = 0; i < kp->cnt-1; i++)
			if (kp->ch2[i] > kp->ch2[i+1]) {
				t = kp->ch2[i];
				kp->ch2[i] = kp->ch2[i+1];
				kp->ch2[i+1] = t;
				t = kp->k[i];
				kp->k[i] = kp->k[i+1];
				kp->k[i+1] = t;
				s = 1;
			}
	} while (s);
	for (j = 0; j < kp->cnt-1; j++)
		if (kp->ch2[j] == kp->ch2[j+1]) {
			for (i = j+1; i < kp->cnt-1; i++) {
				kp->ch2[i] = kp->ch2[i+1];
				kp->k[i] = kp->k[i+1];
			}
			kp->cnt--;
			j--;
		}
	kp->sorted = 1;
}

int
afmgetkern(struct afmtab *a, int ch1, int ch2)
{
	struct kernpairs	*kp;
	int	l, m, r;

	if (a->kernpairs) {
		kp = &a->kernpairs[ch1];
		do {
			if (kp->sorted == 0)
				sortkernpairs(kp);
			r = kp->cnt - 1;
			if (ch2 >= kp->ch2[0] && r >= 0 && ch2 <= kp->ch2[r]) {
				l = 0;
				do {
					m = (l+r) / 2;
					if (ch2 < kp->ch2[m])
						r = m-1;
					else
						l = m+1;
				} while (ch2 != kp->ch2[m] && l <= r);
				if (kp->ch2[m] == ch2)
					return kp->k[m];
			}
		} while ((kp = kp->next) != NULL);
	}
	return 0;
}
#endif	/* !DPOST */

char *
afmencodepath(const char *cp)
{
	const char	hex[] = "0123456789ABCDEF";
	char	*enc, *ep;

	ep = enc = malloc(3 * strlen(cp) + 1);
	while (*cp) {
		if (*cp&0200 || *cp <= 040 || *cp == 0177) {
			*ep++ = '%';
			*ep++ = hex[(*cp&0360) >> 4];
			*ep++ = hex[*cp++&017];
		} else
			*ep++ = *cp++;
	}
	*ep = 0;
	return enc;
}

static int
unhex(int c)
{
	if (c >= 'A' && c <= 'F')
		return c - 'A';
	if (c >= 'a' && c <= 'f')
		return c - 'a';
	return c - '0';
}

static int
xdigit(int c)
{
	return (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') ||
		(c >= '0' && c <= '9');
}

char *
afmdecodepath(const char *cp)
{
	char	*dec, *dp;

	dec = dp = malloc(strlen(cp) + 1);
	while (*cp) {
		if (cp[0] == '%' && xdigit(cp[1]&0377) && xdigit(cp[2]&0377)) {
			*dp++ = unhex(cp[1]) << 4 | unhex(cp[2]);
			cp += 3;
		} else
			*dp++ = *cp++;
	}
	*dp = 0;
	return dec;
}
#endif	/* !DUMP */
