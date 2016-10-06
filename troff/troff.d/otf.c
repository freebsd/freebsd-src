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
 * Sccsid @(#)otf.c	1.68 (gritter) 3/17/10
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include <setjmp.h>
#include <limits.h>
#include "dev.h"
#include "afm.h"

extern	struct dev	dev;
extern	char		*chname;
extern	short		*chtab;
extern	int		nchtab;

extern	void	errprint(const char *, ...);
extern	void	verrprint(const char *, va_list);

static jmp_buf	breakpoint;
static char	*contents;
static size_t	size;
static unsigned short	numTables;
static int	ttf;
static const char	*filename;
unsigned short	unitsPerEm;
short	xMin, yMin, xMax, yMax;
short	indexToLocFormat;
static struct afmtab	*a;
static int	nc;
static int	fsType;
static int	WeightClass;
static int	isFixedPitch;
static int	minMemType42;
static int	maxMemType42;
static int	numGlyphs;
static char	*PostScript_name;
static char	*Copyright;
static char	*Notice;

static struct table_directory {
	char	tag[4];
	unsigned long	checkSum;
	unsigned long	offset;
	unsigned long	length;
} *table_directories;

static int	pos_CFF;
static int	pos_head;
static int	pos_hhea;
static int	pos_loca;
static int	pos_prep;
static int	pos_fpgm;
static int	pos_vhea;
static int	pos_glyf;
static int	pos_cvt;
static int	pos_maxp;
static int	pos_vmtx;
static int	pos_hmtx;
static int	pos_OS_2;
static int	pos_GSUB;
static int	pos_GPOS;
static int	pos_post;
static int	pos_kern;
static int	pos_name;
static int	pos_cmap;

static struct table {
	const char	*name;
	int	*pos;
	int	in_sfnts;
	uint32_t	checksum;
} tables[] = {
	{ "CFF ",	&pos_CFF,	0 },
	{ "cmap",	&pos_cmap,	0 },
	{ "cvt ",	&pos_cvt,	1 },
	{ "fpgm",	&pos_fpgm,	1 },
	{ "GPOS",	&pos_GPOS,	0 },
	{ "GSUB",	&pos_GSUB,	0 },
	{ "head",	&pos_head,	2 },
	{ "hhea",	&pos_hhea,	1 },
	{ "hmtx",	&pos_hmtx,	1 },
	{ "kern",	&pos_kern,	0 },
	{ "loca",	&pos_loca,	1 },
	{ "maxp",	&pos_maxp,	1 },
	{ "name",	&pos_name,	0 },
	{ "OS/2",	&pos_OS_2,	0 },
	{ "post",	&pos_post,	0 },
	{ "prep",	&pos_prep,	1 },
	{ "vhea",	&pos_vhea,	1 },
	{ "vmtx",	&pos_vmtx,	1 },
	{ "glyf",	&pos_glyf,	3 },	/* holds glyph data */
	{ NULL,		NULL,		0 }
};

static unsigned short	*gid2sid;

struct INDEX {
	unsigned short	count;
	int	offSize;
	int	*offset;
	char	*data;
};

static struct CFF {
	struct INDEX	*Name;
	struct INDEX	*Top_DICT;
	struct INDEX	*String;
	struct INDEX	*Global_Subr;
	struct INDEX	*CharStrings;
	int	Charset;
	int	baseoffset;
} CFF;

static const int ExpertCharset[] = {
	0, 1, 229, 230, 231, 232, 233, 234, 235,
	236, 237, 238, 13, 14, 15, 99, 239, 240,
	241, 242, 243, 244, 245, 246, 247, 248, 27,
	28, 249, 250, 251, 252, 253, 254, 255, 256,
	257, 258, 259, 260, 261, 262, 263, 264, 265,
	266, 109, 110, 267, 268, 269, 270, 271, 272,
	273, 274, 275, 276, 277, 278, 279, 280, 281,
	282, 283, 284, 285, 286, 287, 288, 289, 290,
	291, 292, 293, 294, 295, 296, 297, 298, 299,
	300, 301, 302, 303, 304, 305, 306, 307, 308,
	309, 310, 311, 312, 313, 314, 315, 316, 317,
	318, 158, 155, 163, 319, 320, 321, 322, 323,
	324, 325, 326, 150, 164, 169, 327, 328, 329,
	330, 331, 332, 333, 334, 335, 336, 337, 338,
	339, 340, 341, 342, 343, 344, 345, 346, 347,
	348, 349, 350, 351, 352, 353, 354, 355, 356,
	357, 358, 359, 360, 361, 362, 363, 364, 365,
	366, 367, 368, 369, 370, 371, 372, 373, 374,
	375, 376, 377, 378
};

static const int ExpertSubsetCharset[] = {
	0, 1, 231, 232, 235, 236, 237, 238, 13,
	14, 15, 99, 239, 240, 241, 242, 243, 244,
	245, 246, 247, 248, 27, 28, 249, 250, 251,
	253, 254, 255, 256, 257, 258, 259, 260, 261,
	262, 263, 264, 265, 266, 109, 110, 267, 268,
	269, 270, 272, 300, 301, 302, 305, 314, 315,
	158, 155, 163, 320, 321, 322, 323, 324, 325,
	326, 150, 164, 169, 327, 328, 329, 330, 331,
	332, 333, 334, 335, 336, 337, 338, 339, 340,
	341, 342, 343, 344, 345, 346
};

static const char *const StandardStrings[] = {
	".notdef",	/* 0 */
	"space",	/* 1 */
	"exclam",	/* 2 */
	"quotedbl",	/* 3 */
	"numbersign",	/* 4 */
	"dollar",	/* 5 */
	"percent",	/* 6 */
	"ampersand",	/* 7 */
	"quoteright",	/* 8 */
	"parenleft",	/* 9 */
	"parenright",	/* 10 */
	"asterisk",	/* 11 */
	"plus",	/* 12 */
	"comma",	/* 13 */
	"hyphen",	/* 14 */
	"period",	/* 15 */
	"slash",	/* 16 */
	"zero",	/* 17 */
	"one",	/* 18 */
	"two",	/* 19 */
	"three",	/* 20 */
	"four",	/* 21 */
	"five",	/* 22 */
	"six",	/* 23 */
	"seven",	/* 24 */
	"eight",	/* 25 */
	"nine",	/* 26 */
	"colon",	/* 27 */
	"semicolon",	/* 28 */
	"less",	/* 29 */
	"equal",	/* 30 */
	"greater",	/* 31 */
	"question",	/* 32 */
	"at",	/* 33 */
	"A",	/* 34 */
	"B",	/* 35 */
	"C",	/* 36 */
	"D",	/* 37 */
	"E",	/* 38 */
	"F",	/* 39 */
	"G",	/* 40 */
	"H",	/* 41 */
	"I",	/* 42 */
	"J",	/* 43 */
	"K",	/* 44 */
	"L",	/* 45 */
	"M",	/* 46 */
	"N",	/* 47 */
	"O",	/* 48 */
	"P",	/* 49 */
	"Q",	/* 50 */
	"R",	/* 51 */
	"S",	/* 52 */
	"T",	/* 53 */
	"U",	/* 54 */
	"V",	/* 55 */
	"W",	/* 56 */
	"X",	/* 57 */
	"Y",	/* 58 */
	"Z",	/* 59 */
	"bracketleft",	/* 60 */
	"backslash",	/* 61 */
	"bracketright",	/* 62 */
	"asciicircum",	/* 63 */
	"underscore",	/* 64 */
	"quoteleft",	/* 65 */
	"a",	/* 66 */
	"b",	/* 67 */
	"c",	/* 68 */
	"d",	/* 69 */
	"e",	/* 70 */
	"f",	/* 71 */
	"g",	/* 72 */
	"h",	/* 73 */
	"i",	/* 74 */
	"j",	/* 75 */
	"k",	/* 76 */
	"l",	/* 77 */
	"m",	/* 78 */
	"n",	/* 79 */
	"o",	/* 80 */
	"p",	/* 81 */
	"q",	/* 82 */
	"r",	/* 83 */
	"s",	/* 84 */
	"t",	/* 85 */
	"u",	/* 86 */
	"v",	/* 87 */
	"w",	/* 88 */
	"x",	/* 89 */
	"y",	/* 90 */
	"z",	/* 91 */
	"braceleft",	/* 92 */
	"bar",	/* 93 */
	"braceright",	/* 94 */
	"asciitilde",	/* 95 */
	"exclamdown",	/* 96 */
	"cent",	/* 97 */
	"sterling",	/* 98 */
	"fraction",	/* 99 */
	"yen",	/* 100 */
	"florin",	/* 101 */
	"section",	/* 102 */
	"currency",	/* 103 */
	"quotesingle",	/* 104 */
	"quotedblleft",	/* 105 */
	"guillemotleft",	/* 106 */
	"guilsinglleft",	/* 107 */
	"guilsinglright",	/* 108 */
	"fi",	/* 109 */
	"fl",	/* 110 */
	"endash",	/* 111 */
	"dagger",	/* 112 */
	"daggerdbl",	/* 113 */
	"periodcentered",	/* 114 */
	"paragraph",	/* 115 */
	"bullet",	/* 116 */
	"quotesinglbase",	/* 117 */
	"quotedblbase",	/* 118 */
	"quotedblright",	/* 119 */
	"guillemotright",	/* 120 */
	"ellipsis",	/* 121 */
	"perthousand",	/* 122 */
	"questiondown",	/* 123 */
	"grave",	/* 124 */
	"acute",	/* 125 */
	"circumflex",	/* 126 */
	"tilde",	/* 127 */
	"macron",	/* 128 */
	"breve",	/* 129 */
	"dotaccent",	/* 130 */
	"dieresis",	/* 131 */
	"ring",	/* 132 */
	"cedilla",	/* 133 */
	"hungarumlaut",	/* 134 */
	"ogonek",	/* 135 */
	"caron",	/* 136 */
	"emdash",	/* 137 */
	"AE",	/* 138 */
	"ordfeminine",	/* 139 */
	"Lslash",	/* 140 */
	"Oslash",	/* 141 */
	"OE",	/* 142 */
	"ordmasculine",	/* 143 */
	"ae",	/* 144 */
	"dotlessi",	/* 145 */
	"lslash",	/* 146 */
	"oslash",	/* 147 */
	"oe",	/* 148 */
	"germandbls",	/* 149 */
	"onesuperior",	/* 150 */
	"logicalnot",	/* 151 */
	"mu",	/* 152 */
	"trademark",	/* 153 */
	"Eth",	/* 154 */
	"onehalf",	/* 155 */
	"plusminus",	/* 156 */
	"Thorn",	/* 157 */
	"onequarter",	/* 158 */
	"divide",	/* 159 */
	"brokenbar",	/* 160 */
	"degree",	/* 161 */
	"thorn",	/* 162 */
	"threequarters",	/* 163 */
	"twosuperior",	/* 164 */
	"registered",	/* 165 */
	"minus",	/* 166 */
	"eth",	/* 167 */
	"multiply",	/* 168 */
	"threesuperior",	/* 169 */
	"copyright",	/* 170 */
	"Aacute",	/* 171 */
	"Acircumflex",	/* 172 */
	"Adieresis",	/* 173 */
	"Agrave",	/* 174 */
	"Aring",	/* 175 */
	"Atilde",	/* 176 */
	"Ccedilla",	/* 177 */
	"Eacute",	/* 178 */
	"Ecircumflex",	/* 179 */
	"Edieresis",	/* 180 */
	"Egrave",	/* 181 */
	"Iacute",	/* 182 */
	"Icircumflex",	/* 183 */
	"Idieresis",	/* 184 */
	"Igrave",	/* 185 */
	"Ntilde",	/* 186 */
	"Oacute",	/* 187 */
	"Ocircumflex",	/* 188 */
	"Odieresis",	/* 189 */
	"Ograve",	/* 190 */
	"Otilde",	/* 191 */
	"Scaron",	/* 192 */
	"Uacute",	/* 193 */
	"Ucircumflex",	/* 194 */
	"Udieresis",	/* 195 */
	"Ugrave",	/* 196 */
	"Yacute",	/* 197 */
	"Ydieresis",	/* 198 */
	"Zcaron",	/* 199 */
	"aacute",	/* 200 */
	"acircumflex",	/* 201 */
	"adieresis",	/* 202 */
	"agrave",	/* 203 */
	"aring",	/* 204 */
	"atilde",	/* 205 */
	"ccedilla",	/* 206 */
	"eacute",	/* 207 */
	"ecircumflex",	/* 208 */
	"edieresis",	/* 209 */
	"egrave",	/* 210 */
	"iacute",	/* 211 */
	"icircumflex",	/* 212 */
	"idieresis",	/* 213 */
	"igrave",	/* 214 */
	"ntilde",	/* 215 */
	"oacute",	/* 216 */
	"ocircumflex",	/* 217 */
	"odieresis",	/* 218 */
	"ograve",	/* 219 */
	"otilde",	/* 220 */
	"scaron",	/* 221 */
	"uacute",	/* 222 */
	"ucircumflex",	/* 223 */
	"udieresis",	/* 224 */
	"ugrave",	/* 225 */
	"yacute",	/* 226 */
	"ydieresis",	/* 227 */
	"zcaron",	/* 228 */
	"exclamsmall",	/* 229 */
	"Hungarumlautsmall",	/* 230 */
	"dollaroldstyle",	/* 231 */
	"dollarsuperior",	/* 232 */
	"ampersandsmall",	/* 233 */
	"Acutesmall",	/* 234 */
	"parenleftsuperior",	/* 235 */
	"parenrightsuperior",	/* 236 */
	"twodotenleader",	/* 237 */
	"onedotenleader",	/* 238 */
	"zerooldstyle",	/* 239 */
	"oneoldstyle",	/* 240 */
	"twooldstyle",	/* 241 */
	"threeoldstyle",	/* 242 */
	"fouroldstyle",	/* 243 */
	"fiveoldstyle",	/* 244 */
	"sixoldstyle",	/* 245 */
	"sevenoldstyle",	/* 246 */
	"eightoldstyle",	/* 247 */
	"nineoldstyle",	/* 248 */
	"commasuperior",	/* 249 */
	"threequartersemdash",	/* 250 */
	"periodsuperior",	/* 251 */
	"questionsmall",	/* 252 */
	"asuperior",	/* 253 */
	"bsuperior",	/* 254 */
	"centsuperior",	/* 255 */
	"dsuperior",	/* 256 */
	"esuperior",	/* 257 */
	"isuperior",	/* 258 */
	"lsuperior",	/* 259 */
	"msuperior",	/* 260 */
	"nsuperior",	/* 261 */
	"osuperior",	/* 262 */
	"rsuperior",	/* 263 */
	"ssuperior",	/* 264 */
	"tsuperior",	/* 265 */
	"ff",	/* 266 */
	"ffi",	/* 267 */
	"ffl",	/* 268 */
	"parenleftinferior",	/* 269 */
	"parenrightinferior",	/* 270 */
	"Circumflexsmall",	/* 271 */
	"hyphensuperior",	/* 272 */
	"Gravesmall",	/* 273 */
	"Asmall",	/* 274 */
	"Bsmall",	/* 275 */
	"Csmall",	/* 276 */
	"Dsmall",	/* 277 */
	"Esmall",	/* 278 */
	"Fsmall",	/* 279 */
	"Gsmall",	/* 280 */
	"Hsmall",	/* 281 */
	"Ismall",	/* 282 */
	"Jsmall",	/* 283 */
	"Ksmall",	/* 284 */
	"Lsmall",	/* 285 */
	"Msmall",	/* 286 */
	"Nsmall",	/* 287 */
	"Osmall",	/* 288 */
	"Psmall",	/* 289 */
	"Qsmall",	/* 290 */
	"Rsmall",	/* 291 */
	"Ssmall",	/* 292 */
	"Tsmall",	/* 293 */
	"Usmall",	/* 294 */
	"Vsmall",	/* 295 */
	"Wsmall",	/* 296 */
	"Xsmall",	/* 297 */
	"Ysmall",	/* 298 */
	"Zsmall",	/* 299 */
	"colonmonetary",	/* 300 */
	"onefitted",	/* 301 */
	"rupiah",	/* 302 */
	"Tildesmall",	/* 303 */
	"exclamdownsmall",	/* 304 */
	"centoldstyle",	/* 305 */
	"Lslashsmall",	/* 306 */
	"Scaronsmall",	/* 307 */
	"Zcaronsmall",	/* 308 */
	"Dieresissmall",	/* 309 */
	"Brevesmall",	/* 310 */
	"Caronsmall",	/* 311 */
	"Dotaccentsmall",	/* 312 */
	"Macronsmall",	/* 313 */
	"figuredash",	/* 314 */
	"hypheninferior",	/* 315 */
	"Ogoneksmall",	/* 316 */
	"Ringsmall",	/* 317 */
	"Cedillasmall",	/* 318 */
	"questiondownsmall",	/* 319 */
	"oneeighth",	/* 320 */
	"threeeighths",	/* 321 */
	"fiveeighths",	/* 322 */
	"seveneighths",	/* 323 */
	"onethird",	/* 324 */
	"twothirds",	/* 325 */
	"zerosuperior",	/* 326 */
	"foursuperior",	/* 327 */
	"fivesuperior",	/* 328 */
	"sixsuperior",	/* 329 */
	"sevensuperior",	/* 330 */
	"eightsuperior",	/* 331 */
	"ninesuperior",	/* 332 */
	"zeroinferior",	/* 333 */
	"oneinferior",	/* 334 */
	"twoinferior",	/* 335 */
	"threeinferior",	/* 336 */
	"fourinferior",	/* 337 */
	"fiveinferior",	/* 338 */
	"sixinferior",	/* 339 */
	"seveninferior",	/* 340 */
	"eightinferior",	/* 341 */
	"nineinferior",	/* 342 */
	"centinferior",	/* 343 */
	"dollarinferior",	/* 344 */
	"periodinferior",	/* 345 */
	"commainferior",	/* 346 */
	"Agravesmall",	/* 347 */
	"Aacutesmall",	/* 348 */
	"Acircumflexsmall",	/* 349 */
	"Atildesmall",	/* 350 */
	"Adieresissmall",	/* 351 */
	"Aringsmall",	/* 352 */
	"AEsmall",	/* 353 */
	"Ccedillasmall",	/* 354 */
	"Egravesmall",	/* 355 */
	"Eacutesmall",	/* 356 */
	"Ecircumflexsmall",	/* 357 */
	"Edieresissmall",	/* 358 */
	"Igravesmall",	/* 359 */
	"Iacutesmall",	/* 360 */
	"Icircumflexsmall",	/* 361 */
	"Idieresissmall",	/* 362 */
	"Ethsmall",	/* 363 */
	"Ntildesmall",	/* 364 */
	"Ogravesmall",	/* 365 */
	"Oacutesmall",	/* 366 */
	"Ocircumflexsmall",	/* 367 */
	"Otildesmall",	/* 368 */
	"Odieresissmall",	/* 369 */
	"OEsmall",	/* 370 */
	"Oslashsmall",	/* 371 */
	"Ugravesmall",	/* 372 */
	"Uacutesmall",	/* 373 */
	"Ucircumflexsmall",	/* 374 */
	"Udieresissmall",	/* 375 */
	"Yacutesmall",	/* 376 */
	"Thornsmall",	/* 377 */
	"Ydieresissmall",	/* 378 */
	"001.000",	/* 379 */
	"001.001",	/* 380 */
	"001.002",	/* 381 */
	"001.003",	/* 382 */
	"Black",	/* 383 */
	"Bold",	/* 384 */
	"Book",	/* 385 */
	"Light",	/* 386 */
	"Medium",	/* 387 */
	"Regular",	/* 388 */
	"Roman",	/* 389 */
	"Semibold"	/* 390 */
};

static const int	nStdStrings = 391;

static const char *const MacintoshStrings[] = {
	".notdef",	/* 0 */
	".null",	/* 1 */
	"nonmarkingreturn",	/* 2 */
	"space",	/* 3 */
	"exclam",	/* 4 */
	"quotedbl",	/* 5 */
	"numbersign",	/* 6 */
	"dollar",	/* 7 */
	"percent",	/* 8 */
	"ampersand",	/* 9 */
	"quotesingle",	/* 10 */
	"parenleft",	/* 11 */
	"parenright",	/* 12 */
	"asterisk",	/* 13 */
	"plus",	/* 14 */
	"comma",	/* 15 */
	"hyphen",	/* 16 */
	"period",	/* 17 */
	"slash",	/* 18 */
	"zero",	/* 19 */
	"one",	/* 20 */
	"two",	/* 21 */
	"three",	/* 22 */
	"four",	/* 23 */
	"five",	/* 24 */
	"six",	/* 25 */
	"seven",	/* 26 */
	"eight",	/* 27 */
	"nine",	/* 28 */
	"colon",	/* 29 */
	"semicolon",	/* 30 */
	"less",	/* 31 */
	"equal",	/* 32 */
	"greater",	/* 33 */
	"question",	/* 34 */
	"at",	/* 35 */
	"A",	/* 36 */
	"B",	/* 37 */
	"C",	/* 38 */
	"D",	/* 39 */
	"E",	/* 40 */
	"F",	/* 41 */
	"G",	/* 42 */
	"H",	/* 43 */
	"I",	/* 44 */
	"J",	/* 45 */
	"K",	/* 46 */
	"L",	/* 47 */
	"M",	/* 48 */
	"N",	/* 49 */
	"O",	/* 50 */
	"P",	/* 51 */
	"Q",	/* 52 */
	"R",	/* 53 */
	"S",	/* 54 */
	"T",	/* 55 */
	"U",	/* 56 */
	"V",	/* 57 */
	"W",	/* 58 */
	"X",	/* 59 */
	"Y",	/* 60 */
	"Z",	/* 61 */
	"bracketleft",	/* 62 */
	"backslash",	/* 63 */
	"bracketright",	/* 64 */
	"asciicircum",	/* 65 */
	"underscore",	/* 66 */
	"grave",	/* 67 */
	"a",	/* 68 */
	"b",	/* 69 */
	"c",	/* 70 */
	"d",	/* 71 */
	"e",	/* 72 */
	"f",	/* 73 */
	"g",	/* 74 */
	"h",	/* 75 */
	"i",	/* 76 */
	"j",	/* 77 */
	"k",	/* 78 */
	"l",	/* 79 */
	"m",	/* 80 */
	"n",	/* 81 */
	"o",	/* 82 */
	"p",	/* 83 */
	"q",	/* 84 */
	"r",	/* 85 */
	"s",	/* 86 */
	"t",	/* 87 */
	"u",	/* 88 */
	"v",	/* 89 */
	"w",	/* 90 */
	"x",	/* 91 */
	"y",	/* 92 */
	"z",	/* 93 */
	"braceleft",	/* 94 */
	"bar",	/* 95 */
	"braceright",	/* 96 */
	"asciitilde",	/* 97 */
	"Adieresis",	/* 98 */
	"Aring",	/* 99 */
	"Ccedilla",	/* 100 */
	"Eacute",	/* 101 */
	"Ntilde",	/* 102 */
	"Odieresis",	/* 103 */
	"Udieresis",	/* 104 */
	"aacute",	/* 105 */
	"agrave",	/* 106 */
	"acircumflex",	/* 107 */
	"adieresis",	/* 108 */
	"atilde",	/* 109 */
	"aring",	/* 110 */
	"ccedilla",	/* 111 */
	"eacute",	/* 112 */
	"egrave",	/* 113 */
	"ecircumflex",	/* 114 */
	"edieresis",	/* 115 */
	"iacute",	/* 116 */
	"igrave",	/* 117 */
	"icircumflex",	/* 118 */
	"idieresis",	/* 119 */
	"ntilde",	/* 120 */
	"oacute",	/* 121 */
	"ograve",	/* 122 */
	"ocircumflex",	/* 123 */
	"odieresis",	/* 124 */
	"otilde",	/* 125 */
	"uacute",	/* 126 */
	"ugrave",	/* 127 */
	"ucircumflex",	/* 128 */
	"udieresis",	/* 129 */
	"dagger",	/* 130 */
	"degree",	/* 131 */
	"cent",	/* 132 */
	"sterling",	/* 133 */
	"section",	/* 134 */
	"bullet",	/* 135 */
	"paragraph",	/* 136 */
	"germandbls",	/* 137 */
	"registered",	/* 138 */
	"copyright",	/* 139 */
	"trademark",	/* 140 */
	"acute",	/* 141 */
	"dieresis",	/* 142 */
	"notequal",	/* 143 */
	"AE",	/* 144 */
	"Oslash",	/* 145 */
	"infinity",	/* 146 */
	"plusminus",	/* 147 */
	"lessequal",	/* 148 */
	"greaterequal",	/* 149 */
	"yen",	/* 150 */
	"mu",	/* 151 */
	"partialdiff",	/* 152 */
	"summation",	/* 153 */
	"product",	/* 154 */
	"pi",	/* 155 */
	"integral",	/* 156 */
	"ordfeminine",	/* 157 */
	"ordmasculine",	/* 158 */
	"Omega",	/* 159 */
	"ae",	/* 160 */
	"oslash",	/* 161 */
	"questiondown",	/* 162 */
	"exclamdown",	/* 163 */
	"logicalnot",	/* 164 */
	"radical",	/* 165 */
	"florin",	/* 166 */
	"approxequal",	/* 167 */
	"Delta",	/* 168 */
	"guillemotleft",	/* 169 */
	"guillemotright",	/* 170 */
	"ellipsis",	/* 171 */
	"nonbreakingspace",	/* 172 */
	"Agrave",	/* 173 */
	"Atilde",	/* 174 */
	"Otilde",	/* 175 */
	"OE",	/* 176 */
	"oe",	/* 177 */
	"endash",	/* 178 */
	"emdash",	/* 179 */
	"quotedblleft",	/* 180 */
	"quotedblright",	/* 181 */
	"quoteleft",	/* 182 */
	"quoteright",	/* 183 */
	"divide",	/* 184 */
	"lozenge",	/* 185 */
	"ydieresis",	/* 186 */
	"Ydieresis",	/* 187 */
	"fraction",	/* 188 */
	"currency",	/* 189 */
	"guilsinglleft",	/* 190 */
	"guilsinglright",	/* 191 */
	"fi",	/* 192 */
	"fl",	/* 193 */
	"daggerdbl",	/* 194 */
	"periodcentered",	/* 195 */
	"quotesinglbase",	/* 196 */
	"quotedblbase",	/* 197 */
	"perthousand",	/* 198 */
	"Acircumflex",	/* 199 */
	"Ecircumflex",	/* 200 */
	"Aacute",	/* 201 */
	"Edieresis",	/* 202 */
	"Egrave",	/* 203 */
	"Iacute",	/* 204 */
	"Icircumflex",	/* 205 */
	"Idieresis",	/* 206 */
	"Igrave",	/* 207 */
	"Oacute",	/* 208 */
	"Ocircumflex",	/* 209 */
	"apple",	/* 210 */
	"Ograve",	/* 211 */
	"Uacute",	/* 212 */
	"Ucircumflex",	/* 213 */
	"Ugrave",	/* 214 */
	"dotlessi",	/* 215 */
	"circumflex",	/* 216 */
	"tilde",	/* 217 */
	"macron",	/* 218 */
	"breve",	/* 219 */
	"dotaccent",	/* 220 */
	"ring",	/* 221 */
	"cedilla",	/* 222 */
	"hungarumlaut",	/* 223 */
	"ogonek",	/* 224 */
	"caron",	/* 225 */
	"Lslash",	/* 226 */
	"lslash",	/* 227 */
	"Scaron",	/* 228 */
	"scaron",	/* 229 */
	"Zcaron",	/* 230 */
	"zcaron",	/* 231 */
	"brokenbar",	/* 232 */
	"Eth",	/* 233 */
	"eth",	/* 234 */
	"Yacute",	/* 235 */
	"yacute",	/* 236 */
	"Thorn",	/* 237 */
	"thorn",	/* 238 */
	"minus",	/* 239 */
	"multiply",	/* 240 */
	"onesuperior",	/* 241 */
	"twosuperior",	/* 242 */
	"threesuperior",	/* 243 */
	"onehalf",	/* 244 */
	"onequarter",	/* 245 */
	"threequarters",	/* 246 */
	"franc",	/* 247 */
	"Gbreve",	/* 248 */
	"gbreve",	/* 249 */
	"Idotaccent",	/* 250 */
	"Scedilla",	/* 251 */
	"scedilla",	/* 252 */
	"Cacute",	/* 253 */
	"cacute",	/* 254 */
	"Ccaron",	/* 255 */
	"ccaron",	/* 256 */
	"dcroat"	/* 257 */
};

static const int	nMacintoshStrings = 258;

static const struct WGL {
	int	u;
	const char	*s;
} WGL[] = {		/* WGL4 */
	{ 0x0000,	".notdef" },
	{ 0x0020,	"space" },
	{ 0x0021,	"exclam" },
	{ 0x0022,	"quotedbl" },
	{ 0x0023,	"numbersign" },
	{ 0x0024,	"dollar" },
	{ 0x0025,	"percent" },
	{ 0x0026,	"ampersand" },
	{ 0x0027,	"quotesingle" },
	{ 0x0028,	"parenleft" },
	{ 0x0029,	"parenright" },
	{ 0x002a,	"asterisk" },
	{ 0x002b,	"plus" },
	{ 0x002c,	"comma" },
	{ 0x002d,	"hyphen" },
	{ 0x002e,	"period" },
	{ 0x002f,	"slash" },
	{ 0x0030,	"zero" },
	{ 0x0031,	"one" },
	{ 0x0032,	"two" },
	{ 0x0033,	"three" },
	{ 0x0034,	"four" },
	{ 0x0035,	"five" },
	{ 0x0036,	"six" },
	{ 0x0037,	"seven" },
	{ 0x0038,	"eight" },
	{ 0x0039,	"nine" },
	{ 0x003a,	"colon" },
	{ 0x003b,	"semicolon" },
	{ 0x003c,	"less" },
	{ 0x003d,	"equal" },
	{ 0x003e,	"greater" },
	{ 0x003f,	"question" },
	{ 0x0040,	"at" },
	{ 0x0041,	"A" },
	{ 0x0042,	"B" },
	{ 0x0043,	"C" },
	{ 0x0044,	"D" },
	{ 0x0045,	"E" },
	{ 0x0046,	"F" },
	{ 0x0047,	"G" },
	{ 0x0048,	"H" },
	{ 0x0049,	"I" },
	{ 0x004a,	"J" },
	{ 0x004b,	"K" },
	{ 0x004c,	"L" },
	{ 0x004d,	"M" },
	{ 0x004e,	"N" },
	{ 0x004f,	"O" },
	{ 0x0050,	"P" },
	{ 0x0051,	"Q" },
	{ 0x0052,	"R" },
	{ 0x0053,	"S" },
	{ 0x0054,	"T" },
	{ 0x0055,	"U" },
	{ 0x0056,	"V" },
	{ 0x0057,	"W" },
	{ 0x0058,	"X" },
	{ 0x0059,	"Y" },
	{ 0x005a,	"Z" },
	{ 0x005b,	"bracketleft" },
	{ 0x005c,	"backslash" },
	{ 0x005d,	"bracketright" },
	{ 0x005e,	"asciicircum" },
	{ 0x005f,	"underscore" },
	{ 0x0060,	"grave" },
	{ 0x0061,	"a" },
	{ 0x0062,	"b" },
	{ 0x0063,	"c" },
	{ 0x0064,	"d" },
	{ 0x0065,	"e" },
	{ 0x0066,	"f" },
	{ 0x0067,	"g" },
	{ 0x0068,	"h" },
	{ 0x0069,	"i" },
	{ 0x006a,	"j" },
	{ 0x006b,	"k" },
	{ 0x006c,	"l" },
	{ 0x006d,	"m" },
	{ 0x006e,	"n" },
	{ 0x006f,	"o" },
	{ 0x0070,	"p" },
	{ 0x0071,	"q" },
	{ 0x0072,	"r" },
	{ 0x0073,	"s" },
	{ 0x0074,	"t" },
	{ 0x0075,	"u" },
	{ 0x0076,	"v" },
	{ 0x0077,	"w" },
	{ 0x0078,	"x" },
	{ 0x0079,	"y" },
	{ 0x007a,	"z" },
	{ 0x007b,	"braceleft" },
	{ 0x007c,	"bar" },
	{ 0x007d,	"braceright" },
	{ 0x007e,	"asciitilde" },
	{ 0x00a1,	"exclamdown" },
	{ 0x00a2,	"cent" },
	{ 0x00a3,	"sterling" },
	{ 0x00a4,	"currency" },
	{ 0x00a5,	"yen" },
	{ 0x00a6,	"brokenbar" },
	{ 0x00a7,	"section" },
	{ 0x00a8,	"dieresis" },
	{ 0x00a9,	"copyright" },
	{ 0x00aa,	"ordfeminine" },
	{ 0x00ab,	"guillemotleft" },
	{ 0x00ac,	"logicalnot" },
	{ 0x00ae,	"registered" },
	{ 0x00af,	"macron" },
	{ 0x00b0,	"degree" },
	{ 0x00b1,	"plusminus" },
	{ 0x00b2,	"twosuperior" },
	{ 0x00b3,	"threesuperior" },
	{ 0x00b4,	"acute" },
	{ 0x00b5,	"mu" },
	{ 0x00b6,	"paragraph" },
	{ 0x00b7,	"periodcentered" },
	{ 0x00b8,	"cedilla" },
	{ 0x00b9,	"onesuperior" },
	{ 0x00ba,	"ordmasculine" },
	{ 0x00bb,	"guillemotright" },
	{ 0x00bc,	"onequarter" },
	{ 0x00bd,	"onehalf" },
	{ 0x00be,	"threequarters" },
	{ 0x00bf,	"questiondown" },
	{ 0x00c0,	"Agrave" },
	{ 0x00c1,	"Aacute" },
	{ 0x00c2,	"Acircumflex" },
	{ 0x00c3,	"Atilde" },
	{ 0x00c4,	"Adieresis" },
	{ 0x00c5,	"Aring" },
	{ 0x00c6,	"AE" },
	{ 0x00c7,	"Ccedilla" },
	{ 0x00c8,	"Egrave" },
	{ 0x00c9,	"Eacute" },
	{ 0x00ca,	"Ecircumflex" },
	{ 0x00cb,	"Edieresis" },
	{ 0x00cc,	"Igrave" },
	{ 0x00cd,	"Iacute" },
	{ 0x00ce,	"Icircumflex" },
	{ 0x00cf,	"Idieresis" },
	{ 0x00d0,	"Eth" },
	{ 0x00d1,	"Ntilde" },
	{ 0x00d2,	"Ograve" },
	{ 0x00d3,	"Oacute" },
	{ 0x00d4,	"Ocircumflex" },
	{ 0x00d5,	"Otilde" },
	{ 0x00d6,	"Odieresis" },
	{ 0x00d7,	"multiply" },
	{ 0x00d8,	"Oslash" },
	{ 0x00d9,	"Ugrave" },
	{ 0x00da,	"Uacute" },
	{ 0x00db,	"Ucircumflex" },
	{ 0x00dc,	"Udieresis" },
	{ 0x00dd,	"Yacute" },
	{ 0x00de,	"Thorn" },
	{ 0x00df,	"germandbls" },
	{ 0x00e0,	"agrave" },
	{ 0x00e1,	"aacute" },
	{ 0x00e2,	"acircumflex" },
	{ 0x00e3,	"atilde" },
	{ 0x00e4,	"adieresis" },
	{ 0x00e5,	"aring" },
	{ 0x00e6,	"ae" },
	{ 0x00e7,	"ccedilla" },
	{ 0x00e8,	"egrave" },
	{ 0x00e9,	"eacute" },
	{ 0x00ea,	"ecircumflex" },
	{ 0x00eb,	"edieresis" },
	{ 0x00ec,	"igrave" },
	{ 0x00ed,	"iacute" },
	{ 0x00ee,	"icircumflex" },
	{ 0x00ef,	"idieresis" },
	{ 0x00f0,	"eth" },
	{ 0x00f1,	"ntilde" },
	{ 0x00f2,	"ograve" },
	{ 0x00f3,	"oacute" },
	{ 0x00f4,	"ocircumflex" },
	{ 0x00f5,	"otilde" },
	{ 0x00f6,	"odieresis" },
	{ 0x00f7,	"divide" },
	{ 0x00f8,	"oslash" },
	{ 0x00f9,	"ugrave" },
	{ 0x00fa,	"uacute" },
	{ 0x00fb,	"ucircumflex" },
	{ 0x00fc,	"udieresis" },
	{ 0x00fd,	"yacute" },
	{ 0x00fe,	"thorn" },
	{ 0x00ff,	"ydieresis" },
	{ 0x0100,	"Amacron" },
	{ 0x0101,	"amacron" },
	{ 0x0102,	"Abreve" },
	{ 0x0103,	"abreve" },
	{ 0x0104,	"Aogonek" },
	{ 0x0105,	"aogonek" },
	{ 0x0106,	"Cacute" },
	{ 0x0107,	"cacute" },
	{ 0x0108,	"Ccircumflex" },
	{ 0x0109,	"ccircumflex" },
	{ 0x010a,	"Cdotaccent" },
	{ 0x010b,	"cdotaccent" },
	{ 0x010c,	"Ccaron" },
	{ 0x010d,	"ccaron" },
	{ 0x010e,	"Dcaron" },
	{ 0x010f,	"dcaron" },
	{ 0x0110,	"Dcroat" },
	{ 0x0111,	"dcroat" },
	{ 0x0112,	"Emacron" },
	{ 0x0113,	"emacron" },
	{ 0x0114,	"Ebreve" },
	{ 0x0115,	"ebreve" },
	{ 0x0116,	"Edotaccent" },
	{ 0x0117,	"edotaccent" },
	{ 0x0118,	"Eogonek" },
	{ 0x0119,	"eogonek" },
	{ 0x011a,	"Ecaron" },
	{ 0x011b,	"ecaron" },
	{ 0x011c,	"Gcircumflex" },
	{ 0x011d,	"gcircumflex" },
	{ 0x011e,	"Gbreve" },
	{ 0x011f,	"gbreve" },
	{ 0x0120,	"Gdotaccent" },
	{ 0x0121,	"gdotaccent" },
	{ 0x0122,	"Gcommaaccent" },
	{ 0x0123,	"gcommaaccent" },
	{ 0x0124,	"Hcircumflex" },
	{ 0x0125,	"hcircumflex" },
	{ 0x0126,	"Hbar" },
	{ 0x0127,	"hbar" },
	{ 0x0128,	"Itilde" },
	{ 0x0129,	"itilde" },
	{ 0x012a,	"Imacron" },
	{ 0x012b,	"imacron" },
	{ 0x012c,	"Ibreve" },
	{ 0x012d,	"ibreve" },
	{ 0x012e,	"Iogonek" },
	{ 0x012f,	"iogonek" },
	{ 0x0130,	"Idotaccent" },
	{ 0x0131,	"dotlessi" },
	{ 0x0132,	"IJ" },
	{ 0x0133,	"ij" },
	{ 0x0134,	"Jcircumflex" },
	{ 0x0135,	"jcircumflex" },
	{ 0x0136,	"Kcommaaccent" },
	{ 0x0137,	"kcommaaccent" },
	{ 0x0138,	"kgreenlandic" },
	{ 0x0139,	"Lacute" },
	{ 0x013a,	"lacute" },
	{ 0x013b,	"Lcommaaccent" },
	{ 0x013c,	"lcommaaccent" },
	{ 0x013d,	"Lcaron" },
	{ 0x013e,	"lcaron" },
	{ 0x013f,	"Ldot" },
	{ 0x0140,	"ldot" },
	{ 0x0141,	"Lslash" },
	{ 0x0142,	"lslash" },
	{ 0x0143,	"Nacute" },
	{ 0x0144,	"nacute" },
	{ 0x0145,	"Ncommaaccent" },
	{ 0x0146,	"ncommaaccent" },
	{ 0x0147,	"Ncaron" },
	{ 0x0148,	"ncaron" },
	{ 0x0149,	"napostrophe" },
	{ 0x014a,	"Eng" },
	{ 0x014b,	"eng" },
	{ 0x014c,	"Omacron" },
	{ 0x014d,	"omacron" },
	{ 0x014e,	"Obreve" },
	{ 0x014f,	"obreve" },
	{ 0x0150,	"Ohungarumlaut" },
	{ 0x0151,	"ohungarumlaut" },
	{ 0x0152,	"OE" },
	{ 0x0153,	"oe" },
	{ 0x0154,	"Racute" },
	{ 0x0155,	"racute" },
	{ 0x0156,	"Rcommaaccent" },
	{ 0x0157,	"rcommaaccent" },
	{ 0x0158,	"Rcaron" },
	{ 0x0159,	"rcaron" },
	{ 0x015a,	"Sacute" },
	{ 0x015b,	"sacute" },
	{ 0x015c,	"Scircumflex" },
	{ 0x015d,	"scircumflex" },
	{ 0x015e,	"Scedilla" },
	{ 0x015f,	"scedilla" },
	{ 0x0160,	"Scaron" },
	{ 0x0161,	"scaron" },
	{ 0x0164,	"Tcaron" },
	{ 0x0165,	"tcaron" },
	{ 0x0166,	"Tbar" },
	{ 0x0167,	"tbar" },
	{ 0x0168,	"Utilde" },
	{ 0x0169,	"utilde" },
	{ 0x016a,	"Umacron" },
	{ 0x016b,	"umacron" },
	{ 0x016c,	"Ubreve" },
	{ 0x016d,	"ubreve" },
	{ 0x016e,	"Uring" },
	{ 0x016f,	"uring" },
	{ 0x0170,	"Uhungarumlaut" },
	{ 0x0171,	"uhungarumlaut" },
	{ 0x0172,	"Uogonek" },
	{ 0x0173,	"uogonek" },
	{ 0x0174,	"Wcircumflex" },
	{ 0x0175,	"wcircumflex" },
	{ 0x0176,	"Ycircumflex" },
	{ 0x0177,	"ycircumflex" },
	{ 0x0178,	"Ydieresis" },
	{ 0x0179,	"Zacute" },
	{ 0x017a,	"zacute" },
	{ 0x017b,	"Zdotaccent" },
	{ 0x017c,	"zdotaccent" },
	{ 0x017d,	"Zcaron" },
	{ 0x017e,	"zcaron" },
	{ 0x017f,	"longs" },
	{ 0x0192,	"florin" },
	{ 0x01fa,	"Aringacute" },
	{ 0x01fb,	"aringacute" },
	{ 0x01fc,	"AEacute" },
	{ 0x01fd,	"aeacute" },
	{ 0x01fe,	"Oslashacute" },
	{ 0x01ff,	"oslashacute" },
	{ 0x02c6,	"circumflex" },
	{ 0x02c7,	"caron" },
	{ 0x02d8,	"breve" },
	{ 0x02d9,	"dotaccent" },
	{ 0x02da,	"ring" },
	{ 0x02db,	"ogonek" },
	{ 0x02dc,	"tilde" },
	{ 0x02dd,	"hungarumlaut" },
	{ 0x0384,	"tonos" },
	{ 0x0385,	"dieresistonos" },
	{ 0x0386,	"Alphatonos" },
	{ 0x0387,	"anoteleia" },
	{ 0x0388,	"Epsilontonos" },
	{ 0x0389,	"Etatonos" },
	{ 0x038a,	"Iotatonos" },
	{ 0x038c,	"Omicrontonos" },
	{ 0x038e,	"Upsilontonos" },
	{ 0x038f,	"Omegatonos" },
	{ 0x0390,	"iotadieresistonos" },
	{ 0x0391,	"Alpha" },
	{ 0x0392,	"Beta" },
	{ 0x0393,	"Gamma" },
	{ 0x0395,	"Epsilon" },
	{ 0x0396,	"Zeta" },
	{ 0x0397,	"Eta" },
	{ 0x0398,	"Theta" },
	{ 0x0399,	"Iota" },
	{ 0x039a,	"Kappa" },
	{ 0x039b,	"Lambda" },
	{ 0x039c,	"Mu" },
	{ 0x039d,	"Nu" },
	{ 0x039e,	"Xi" },
	{ 0x039f,	"Omicron" },
	{ 0x03a0,	"Pi" },
	{ 0x03a1,	"Rho" },
	{ 0x03a3,	"Sigma" },
	{ 0x03a4,	"Tau" },
	{ 0x03a5,	"Upsilon" },
	{ 0x03a6,	"Phi" },
	{ 0x03a7,	"Chi" },
	{ 0x03a8,	"Psi" },
	{ 0x03aa,	"Iotadieresis" },
	{ 0x03ab,	"Upsilondieresis" },
	{ 0x03ac,	"alphatonos" },
	{ 0x03ad,	"epsilontonos" },
	{ 0x03ae,	"etatonos" },
	{ 0x03af,	"iotatonos" },
	{ 0x03b0,	"upsilondieresistonos" },
	{ 0x03b1,	"alpha" },
	{ 0x03b2,	"beta" },
	{ 0x03b3,	"gamma" },
	{ 0x03b4,	"delta" },
	{ 0x03b5,	"epsilon" },
	{ 0x03b6,	"zeta" },
	{ 0x03b7,	"eta" },
	{ 0x03b8,	"theta" },
	{ 0x03b9,	"iota" },
	{ 0x03ba,	"kappa" },
	{ 0x03bb,	"lambda" },
	{ 0x03bd,	"nu" },
	{ 0x03be,	"xi" },
	{ 0x03bf,	"omicron" },
	{ 0x03c0,	"pi" },
	{ 0x03c1,	"rho" },
	{ 0x03c2,	"sigma1" },
	{ 0x03c3,	"sigma" },
	{ 0x03c4,	"tau" },
	{ 0x03c5,	"upsilon" },
	{ 0x03c6,	"phi" },
	{ 0x03c7,	"chi" },
	{ 0x03c8,	"psi" },
	{ 0x03c9,	"omega" },
	{ 0x03ca,	"iotadieresis" },
	{ 0x03cb,	"upsilondieresis" },
	{ 0x03cc,	"omicrontonos" },
	{ 0x03cd,	"upsilontonos" },
	{ 0x03ce,	"omegatonos" },
	{ 0x0401,	"afii10023" },
	{ 0x0402,	"afii10051" },
	{ 0x0403,	"afii10052" },
	{ 0x0404,	"afii10053" },
	{ 0x0405,	"afii10054" },
	{ 0x0406,	"afii10055" },
	{ 0x0407,	"afii10056" },
	{ 0x0408,	"afii10057" },
	{ 0x0409,	"afii10058" },
	{ 0x040a,	"afii10059" },
	{ 0x040b,	"afii10060" },
	{ 0x040c,	"afii10061" },
	{ 0x040e,	"afii10062" },
	{ 0x040f,	"afii10145" },
	{ 0x0410,	"afii10017" },
	{ 0x0411,	"afii10018" },
	{ 0x0412,	"afii10019" },
	{ 0x0413,	"afii10020" },
	{ 0x0414,	"afii10021" },
	{ 0x0415,	"afii10022" },
	{ 0x0416,	"afii10024" },
	{ 0x0417,	"afii10025" },
	{ 0x0418,	"afii10026" },
	{ 0x0419,	"afii10027" },
	{ 0x041a,	"afii10028" },
	{ 0x041b,	"afii10029" },
	{ 0x041c,	"afii10030" },
	{ 0x041d,	"afii10031" },
	{ 0x041e,	"afii10032" },
	{ 0x041f,	"afii10033" },
	{ 0x0420,	"afii10034" },
	{ 0x0421,	"afii10035" },
	{ 0x0422,	"afii10036" },
	{ 0x0423,	"afii10037" },
	{ 0x0424,	"afii10038" },
	{ 0x0425,	"afii10039" },
	{ 0x0426,	"afii10040" },
	{ 0x0427,	"afii10041" },
	{ 0x0428,	"afii10042" },
	{ 0x0429,	"afii10043" },
	{ 0x042a,	"afii10044" },
	{ 0x042b,	"afii10045" },
	{ 0x042c,	"afii10046" },
	{ 0x042d,	"afii10047" },
	{ 0x042e,	"afii10048" },
	{ 0x042f,	"afii10049" },
	{ 0x0430,	"afii10065" },
	{ 0x0431,	"afii10066" },
	{ 0x0432,	"afii10067" },
	{ 0x0433,	"afii10068" },
	{ 0x0434,	"afii10069" },
	{ 0x0435,	"afii10070" },
	{ 0x0436,	"afii10072" },
	{ 0x0437,	"afii10073" },
	{ 0x0438,	"afii10074" },
	{ 0x0439,	"afii10075" },
	{ 0x043a,	"afii10076" },
	{ 0x043b,	"afii10077" },
	{ 0x043c,	"afii10078" },
	{ 0x043d,	"afii10079" },
	{ 0x043e,	"afii10080" },
	{ 0x043f,	"afii10081" },
	{ 0x0440,	"afii10082" },
	{ 0x0441,	"afii10083" },
	{ 0x0442,	"afii10084" },
	{ 0x0443,	"afii10085" },
	{ 0x0444,	"afii10086" },
	{ 0x0445,	"afii10087" },
	{ 0x0446,	"afii10088" },
	{ 0x0447,	"afii10089" },
	{ 0x0448,	"afii10090" },
	{ 0x0449,	"afii10091" },
	{ 0x044a,	"afii10092" },
	{ 0x044b,	"afii10093" },
	{ 0x044c,	"afii10094" },
	{ 0x044d,	"afii10095" },
	{ 0x044e,	"afii10096" },
	{ 0x044f,	"afii10097" },
	{ 0x0451,	"afii10071" },
	{ 0x0452,	"afii10099" },
	{ 0x0453,	"afii10100" },
	{ 0x0454,	"afii10101" },
	{ 0x0455,	"afii10102" },
	{ 0x0456,	"afii10103" },
	{ 0x0457,	"afii10104" },
	{ 0x0458,	"afii10105" },
	{ 0x0459,	"afii10106" },
	{ 0x045a,	"afii10107" },
	{ 0x045b,	"afii10108" },
	{ 0x045c,	"afii10109" },
	{ 0x045e,	"afii10110" },
	{ 0x045f,	"afii10193" },
	{ 0x0490,	"afii10050" },
	{ 0x0491,	"afii10098" },
	{ 0x1e80,	"Wgrave" },
	{ 0x1e81,	"wgrave" },
	{ 0x1e82,	"Wacute" },
	{ 0x1e83,	"wacute" },
	{ 0x1e84,	"Wdieresis" },
	{ 0x1e85,	"wdieresis" },
	{ 0x1ef2,	"Ygrave" },
	{ 0x1ef3,	"ygrave" },
	{ 0x2013,	"endash" },
	{ 0x2014,	"emdash" },
	{ 0x2015,	"afii00208" },
	{ 0x2017,	"underscoredbl" },
	{ 0x2018,	"quoteleft" },
	{ 0x2019,	"quoteright" },
	{ 0x201a,	"quotesinglbase" },
	{ 0x201b,	"quotereversed" },
	{ 0x201c,	"quotedblleft" },
	{ 0x201d,	"quotedblright" },
	{ 0x201e,	"quotedblbase" },
	{ 0x2020,	"dagger" },
	{ 0x2021,	"daggerdbl" },
	{ 0x2022,	"bullet" },
	{ 0x2026,	"ellipsis" },
	{ 0x2030,	"perthousand" },
	{ 0x2032,	"minute" },
	{ 0x2033,	"second" },
	{ 0x2039,	"guilsinglleft" },
	{ 0x203a,	"guilsinglright" },
	{ 0x203c,	"exclamdbl" },
	{ 0x203e,	"uni203E" },
	{ 0x2044,	"fraction" },
	{ 0x207f,	"nsuperior" },
	{ 0x20a3,	"franc" },
	{ 0x20a4,	"lira" },
	{ 0x20a7,	"peseta" },
	{ 0x20ac,	"Euro" },
	{ 0x2105,	"afii61248" },
	{ 0x2113,	"afii61289" },
	{ 0x2116,	"afii61352" },
	{ 0x2122,	"trademark" },
	{ 0x2126,	"Omega" },
	{ 0x212e,	"estimated" },
	{ 0x215b,	"oneeighth" },
	{ 0x215c,	"threeeighths" },
	{ 0x215d,	"fiveeighths" },
	{ 0x215e,	"seveneighths" },
	{ 0x2190,	"arrowleft" },
	{ 0x2191,	"arrowup" },
	{ 0x2192,	"arrowright" },
	{ 0x2193,	"arrowdown" },
	{ 0x2194,	"arrowboth" },
	{ 0x2195,	"arrowupdn" },
	{ 0x21a8,	"arrowupdnbse" },
	{ 0x2202,	"partialdiff" },
	{ 0x2206,	"Delta" },
	{ 0x220f,	"product" },
	{ 0x2211,	"summation" },
	{ 0x2212,	"minus" },
	{ 0x221a,	"radical" },
	{ 0x221e,	"infinity" },
	{ 0x221f,	"orthogonal" },
	{ 0x2229,	"intersection" },
	{ 0x222b,	"integral" },
	{ 0x2248,	"approxequal" },
	{ 0x2260,	"notequal" },
	{ 0x2261,	"equivalence" },
	{ 0x2264,	"lessequal" },
	{ 0x2265,	"greaterequal" },
	{ 0x2302,	"house" },
	{ 0x2310,	"revlogicalnot" },
	{ 0x2320,	"integraltp" },
	{ 0x2321,	"integralbt" },
	{ 0x2500,	"SF100000" },
	{ 0x2502,	"SF110000" },
	{ 0x250c,	"SF010000" },
	{ 0x2510,	"SF030000" },
	{ 0x2514,	"SF020000" },
	{ 0x2518,	"SF040000" },
	{ 0x251c,	"SF080000" },
	{ 0x2524,	"SF090000" },
	{ 0x252c,	"SF060000" },
	{ 0x2534,	"SF070000" },
	{ 0x253c,	"SF050000" },
	{ 0x2550,	"SF430000" },
	{ 0x2551,	"SF240000" },
	{ 0x2552,	"SF510000" },
	{ 0x2553,	"SF520000" },
	{ 0x2554,	"SF390000" },
	{ 0x2555,	"SF220000" },
	{ 0x2556,	"SF210000" },
	{ 0x2557,	"SF250000" },
	{ 0x2558,	"SF500000" },
	{ 0x2559,	"SF490000" },
	{ 0x255a,	"SF380000" },
	{ 0x255b,	"SF280000" },
	{ 0x255c,	"SF270000" },
	{ 0x255d,	"SF260000" },
	{ 0x255e,	"SF360000" },
	{ 0x255f,	"SF370000" },
	{ 0x2560,	"SF420000" },
	{ 0x2561,	"SF190000" },
	{ 0x2562,	"SF200000" },
	{ 0x2563,	"SF230000" },
	{ 0x2564,	"SF470000" },
	{ 0x2565,	"SF480000" },
	{ 0x2566,	"SF410000" },
	{ 0x2567,	"SF450000" },
	{ 0x2568,	"SF460000" },
	{ 0x2569,	"SF400000" },
	{ 0x256a,	"SF540000" },
	{ 0x256b,	"SF530000" },
	{ 0x256c,	"SF440000" },
	{ 0x2580,	"upblock" },
	{ 0x2584,	"dnblock" },
	{ 0x2588,	"block" },
	{ 0x258c,	"lfblock" },
	{ 0x2590,	"rtblock" },
	{ 0x2591,	"ltshade" },
	{ 0x2592,	"shade" },
	{ 0x2593,	"dkshade" },
	{ 0x25a0,	"filledbox" },
	{ 0x25a1,	"H22073" },
	{ 0x25aa,	"H18543" },
	{ 0x25ab,	"H18551" },
	{ 0x25ac,	"filledrect" },
	{ 0x25b2,	"triagup" },
	{ 0x25ba,	"triagrt" },
	{ 0x25bc,	"triagdn" },
	{ 0x25c4,	"triaglf" },
	{ 0x25ca,	"lozenge" },
	{ 0x25cb,	"circle" },
	{ 0x25cf,	"H18533" },
	{ 0x25d8,	"invbullet" },
	{ 0x25d9,	"invcircle" },
	{ 0x25e6,	"openbullet" },
	{ 0x263a,	"smileface" },
	{ 0x263b,	"invsmileface" },
	{ 0x263c,	"sun" },
	{ 0x2640,	"female" },
	{ 0x2642,	"male" },
	{ 0x2660,	"spade" },
	{ 0x2663,	"club" },
	{ 0x2665,	"heart" },
	{ 0x2666,	"diamond" },
	{ 0x266a,	"musicalnote" },
	{ 0x266b,	"musicalnotedbl" },
	{ 0xfb01,	"fi" },
	{ 0xfb02,	"fl" },
	{ -1,		NULL }
};

static int	nWGL = 642;

static char	**ExtraStrings;
static char	*ExtraStringSpace;
static int	ExtraStringSpacePos;
static int	nExtraStrings;

static char *
getSID(int n)
{
	if (ttf == 3) {
		/*EMPTY*/;
	} else if (ttf == 2) {
		if (n >= 0 && n < nWGL)
			return (char *)WGL[n].s;
		n -= nWGL;
	} else if (ttf == 1) {
		if (n >= 0 && n < nMacintoshStrings)
			return (char *)MacintoshStrings[n];
		n -= nMacintoshStrings;
	} else {
		if (n >= 0 && n < nStdStrings)
			return (char *)StandardStrings[n];
		n -= nStdStrings;
	}
	if (n < nExtraStrings)
		return ExtraStrings[n];
	return NULL;
}

static void
error(const char *fmt, ...)
{
	char	buf[4096];
	va_list	ap;
	int	n;

	n = snprintf(buf, sizeof buf, "%s: ", filename);
	va_start(ap, fmt);
	vsnprintf(&buf[n], sizeof buf - n, fmt, ap);
	errprint("%s", buf);
	va_end(ap);
	longjmp(breakpoint, 1);
}

#define _pbe16(cp) ((uint16_t)((cp)[1]&0377) + ((uint16_t)((cp)[0]&0377) << 8))

static uint32_t
pbe16(const char *cp)
{
	return (uint16_t)(cp[1]&0377) +
		((uint16_t)(cp[0]&0377) << 8);
}

static uint32_t
pbe24(const char *cp)
{
	return (uint32_t)(cp[3]&0377) +
		((uint32_t)(cp[2]&0377) << 8) +
		((uint32_t)(cp[1]&0377) << 16);
}

static uint32_t
pbe32(const char *cp)
{
	return (uint32_t)(cp[3]&0377) +
		((uint32_t)(cp[2]&0377) << 8) +
		((uint32_t)(cp[1]&0377) << 16) +
		((uint32_t)(cp[0]&0377) << 24);
}

static uint32_t
pbeXX(const char *cp, int n)
{
	switch (n) {
	default:
		error("invalid number size %d", n);
	case 1:
		return *cp&0377;
	case 2:
		return _pbe16(cp);
	case 3:
		return pbe24(cp);
	case 4:
		return pbe32(cp);
	}
}

static double
cffoper(long *op)
{
	int	b0;
	int	n = 0;
	double	v = 0;

	b0 = contents[*op]&0377;
	if (b0 >= 32 && b0 <= 246) {
		n = 1;
		v = b0 - 139;
	} else if (b0 >= 247 && b0 <= 250) {
		n = 2;
		v = (b0 - 247) * 256 + (contents[*op+1]&0377) + 108;
	} else if (b0 >= 251 && b0 <= 254) {
		n = 2;
		v = -(b0 - 251) * 256 - (contents[*op+1]&0377) - 108;
	} else if (b0 == 28) {
		n = 3;
		v = (int16_t)((contents[*op+1]&0377)<<8 |
				(contents[*op+2]&0377));
	} else if (b0 == 29) {
		n = 5;
		v = (int32_t)pbe32(&contents[*op+1]);
	} else if (b0 == 30) {
		char	buf[100], *xp;
		int	c, i = 0, s = 0;
		n = 1;
		for (;;) {
			if (i == sizeof buf - 2)
				error("floating point operand too long");
			c = (contents[*op+n]&0377) >> (s ? 8 : 0) & 0xf;
			if (c >= 0 && c <= 9)
				buf[i++] = c + '0';
			else if (c == 0xa)
				buf[i++] = '.';
			else if (c == 0xb)
				buf[i++] = 'E';
			else if (c == 0xc) {
				buf[i++] = 'E';
				buf[i++] = '-';
			} else if (c == 0xd)
				error("reserved nibble d in floating point "
						"operand");
			else if (c == 0xe)
				buf[i++] = '-';
			else if (c == 0xf) {
				buf[i++] = 0;
				if (s == 0)
					n++;
				break;
			}
			if ((s = !s) == 0)
				n++;
		}
		v = strtod(buf, &xp);
		if (*xp != 0)
			error("invalid floating point operand <%s>", buf);
	} else
		error("invalid operand b0 range %d", b0);
	*op += n;
	return v;
}

static void
get_offset_table(void)
{
	char	buf[12];

	if (size < 12)
		error("no offset table");
	memcpy(buf, contents, 12);
	if (pbe32(buf) == 0x00010000 || memcmp(buf, "true", 4) == 0) {
		ttf = 1;
	} else if (memcmp(buf, "OTTO", 4) == 0) {
		ttf = 0;
	} else
		error("unknown font type");
	numTables = pbe16(&buf[4]);
}

static void
get_table_directories(void)
{
	int	i, j, o;
	char	buf[16];

	free(table_directories);
	table_directories = calloc(numTables, sizeof *table_directories);
	o = 12;
	for (i = 0; tables[i].pos; i++)
		*tables[i].pos = -1;
	for (i = 0; i < numTables; i++) {
		if (o + 16 >= size)
			error("cannot get %dth table directory", i);
		memcpy(buf, &contents[o], 16);
		for (j = 0; tables[j].name; j++)
			if (memcmp(buf, tables[j].name, 4) == 0) {
				*tables[j].pos = i;
				break;
			}
		o += 16;
		memcpy(table_directories[i].tag, buf, 4);
		table_directories[i].checkSum = pbe32(&buf[4]);
		table_directories[i].offset = pbe32(&buf[8]);
		table_directories[i].length = pbe32(&buf[12]);
		if (table_directories[i].offset +
				table_directories[i].length > size)
			error("invalid table directory, "
					"size for entry %4.4s too large",
					table_directories[i].tag);
	}
}

static void
free_INDEX(struct INDEX *ip)
{
	if (ip) {
		free(ip->offset);
		free(ip);
	}
}

static struct INDEX *
get_INDEX(long *op)
{
	struct INDEX	*ip;
	int	i;

	if (*op + 3 >= size)
		error("no index at position %ld", *op);
	ip = calloc(1, sizeof *ip);
	ip->count = pbe16(&contents[*op]);
	*op += 2;
	if (ip->count != 0) {
		ip->offSize = contents[(*op)++] & 0377;
		ip->offset = calloc(ip->count+1, sizeof *ip->offset);
		for (i = 0; i < ip->count+1; i++) {
			if (*op + ip->offSize >= size) {
				free_INDEX(ip);
				error("no index offset at position %ld", *op);
			}
			ip->offset[i] = pbeXX(&contents[*op], ip->offSize);
			*op += ip->offSize;
		}
		ip->data = &contents[*op];
		for (i = 0; i < ip->count+1; i++)
			ip->offset[i] += *op - 1;
		*op = ip->offset[ip->count];
	}
	return ip;
}

static void
get_bb(int gid, int B[4])
{
	int	k, o;

	if (pos_loca < 0 || pos_glyf < 0)
		return;
	o = table_directories[pos_loca].offset;
	k = indexToLocFormat ? pbe32(&contents[o+4*gid]) :
		pbe16(&contents[o+2*gid]) * 2;
	o = table_directories[pos_glyf].offset;
	B[0] = (int16_t)pbe16(&contents[o+k+2]);
	B[1] = (int16_t)pbe16(&contents[o+k+4]);
	B[2] = (int16_t)pbe16(&contents[o+k+6]);
	B[3] = (int16_t)pbe16(&contents[o+k+8]);
}

static void
onechar(int gid, int sid)
{
	long	o;
	int	w, tp;
	char	*N;
	int	*b = NULL, B[4] = { 0, 0, 0, 0};

	if ((gid == 0 && sid != 0) || (sid == 0 && gid != 0))
		return;		/* require .notdef to be GID 0 */
	if (gid >= nc)
		return;
	if (pos_hmtx < 0)
		error("no hmtx table");
	if (table_directories[pos_hmtx].length < 4)
		error("empty hmtx table");
	o = table_directories[pos_hmtx].offset;
	if (isFixedPitch)
		w = pbe16(&contents[o]);
	else {
		if (table_directories[pos_hmtx].length < 4 * (gid+1))
			return;	/* just ignore this glyph */
		w = pbe16(&contents[o + 4 * gid]);
	}
	if (sid != 0 && gid2sid[gid] != 0)
		return;
	if (a) {
		if ((N = getSID(sid)) != NULL) {
			a->nspace += strlen(N) + 1;
			tp = afmmapname(N, a->spec);
		} else
			tp = 0;
		if (ttf)
			get_bb(gid, b = B);
		afmaddchar(a, gid, tp, 0, w, b, N, a->spec, gid);
	}
	gid2sid[gid] = sid;
}

static int
get_CFF_Top_DICT_Entry(int e)
{
	long	o;
	int	d = 0;

	if (CFF.Top_DICT == NULL || CFF.Top_DICT->offset == NULL)
		error("no Top DICT INDEX");
	o = CFF.Top_DICT->offset[0];
	while (o < CFF.Top_DICT->offset[1] && contents[o] != e) {
		if (contents[o] < 0 || contents[o] > 27)
			d = cffoper(&o);
		else {
			d = 0;
			if (contents[o] == 12)
				o++;
			o++;
		}
	}
	return d;
}

static void
get_CFF_Charset(void)
{
	int	d = 0;
	int	gid, i, j, first, nLeft;

	d = get_CFF_Top_DICT_Entry(15);
	if (d == 0) {
		for (i = 0; i < nc && i <= 228; i++)
			onechar(i, i);
	} else if (d == 1) {
		for (i = 0; i < nc && i <= 166; i++)
			onechar(i, ExpertCharset[i]);
	} else if (d == 2) {
		for (i = 0; i < nc && i <= 87; i++)
			onechar(i, ExpertSubsetCharset[i]);
	} else if (d > 2) {
		d = CFF.Charset = d + CFF.baseoffset;
		onechar(0, 0);
		gid = 1;
		switch (contents[d++]) {
		case 0:
			for (i = 1; i < nc; i++) {
				j = pbe16(&contents[d]);
				d += 2;
				onechar(gid++, j);
			}
			break;
		case 1:
			i = nc - 1;
			while (i > 0) {
				first = pbe16(&contents[d]);
				d += 2;
				nLeft = contents[d++] & 0377;
				for (j = 0; j <= nLeft && gid < nc; j++)
					onechar(gid++, first + j);
				i -= nLeft + 1;
			}
			break;
		default:
			error("unknown Charset table format %d", contents[d-1]);
		case 2:
			i = nc - 1;
			while (i > 0) {
				first = pbe16(&contents[d]);
				d += 2;
				nLeft = pbe16(&contents[d]);
				d += 2;
				for (j = 0; j <= nLeft && gid < nc; j++)
					onechar(gid++, first + j);
				i -= nLeft + 1;
			}
		}
	} else
		error("invalid Charset offset");
}

static void
build_ExtraStrings(void)
{
	int	c, i;
	char	*sp;

	if (CFF.String == NULL || CFF.String->count == 0)
		return;
	ExtraStrings = calloc(CFF.String->count, sizeof *ExtraStrings);
	sp = ExtraStringSpace = malloc(CFF.String->count +
			CFF.String->offset[CFF.String->count]);
	for (c = 0; c < CFF.String->count; c++) {
		ExtraStrings[c] = sp;
		for (i = CFF.String->offset[c];
				i < CFF.String->offset[c+1]; i++)
			*sp++ = contents[i];
		*sp++ = 0;
	}
	nExtraStrings = c;
}

static void
otfalloc(int _nc)
{
	nc = _nc;
	gid2sid = calloc(nc, sizeof *gid2sid);
	if (a) {
		afmalloc(a, nc);
		a->gid2tr = calloc(nc, sizeof *a->gid2tr);
	}
}

static void
get_CFF(void)
{
	long	o;
	char	buf[4];

	if (pos_CFF < 0)
		error("no CFF table");
	CFF.baseoffset = o = table_directories[pos_CFF].offset;
	if (o + 4 >= size)
		error("no CFF header");
	memcpy(buf, &contents[o], 4);
	o += 4;
	if (buf[0] != 1)
		error("can only handle CFF major version 1");
	CFF.Name = get_INDEX(&o);
	CFF.Top_DICT = get_INDEX(&o);
	CFF.String = get_INDEX(&o);
	build_ExtraStrings();
	CFF.Global_Subr = get_INDEX(&o);
	o = get_CFF_Top_DICT_Entry(17);
	o += CFF.baseoffset;
	CFF.CharStrings = get_INDEX(&o);
	if (CFF.Name->count != 1)
		error("cannot handle CFF data with more than one font");
	a->fontname = malloc(CFF.Name->offset[1] - CFF.Name->offset[0] + 1);
	memcpy(a->fontname, &contents[CFF.Name->offset[0]],
			CFF.Name->offset[1] - CFF.Name->offset[0]);
	a->fontname[CFF.Name->offset[1] - CFF.Name->offset[0]] = 0;
#ifdef	DUMP
	print(SHOW_NAME, "name %s", a->fontname);
#endif
	if (CFF.CharStrings == NULL || CFF.CharStrings->count == 0)
		error("no characters in font");
	otfalloc(CFF.CharStrings->count);
	get_CFF_Charset();
	afmremap(a);
}

/*ARGSUSED*/
static void
get_ttf_post_1_0(int o)
{
	int	i;

	otfalloc(numGlyphs);
	for (i = 0; i < numGlyphs; i++)
		onechar(i, i);
}

static void
get_ttf_post_2_0(int o)
{
	int	numberOfGlyphs;
	int	numberNewGlyphs;
	int	i, j, n;
	char	*cp, *sp;

	numberOfGlyphs = pbe16(&contents[o+32]);
	if (34+2*numberOfGlyphs > table_directories[pos_post].length)
		error("numberOfGlyphs value in post table too large");
	otfalloc(numberOfGlyphs);
	numberNewGlyphs = 0;
	for (i = 0; i < numberOfGlyphs; i++) {
		n = pbe16(&contents[o+34+2*i]);
		if (n >= 258) {
			n -= 258;
			if (n >= numberNewGlyphs)
				numberNewGlyphs = n + 1;
		}
	}
	ExtraStrings = calloc(numberNewGlyphs, sizeof *ExtraStrings);
	sp = ExtraStringSpace = malloc(table_directories[pos_post].length -
			34 - 2*numberOfGlyphs);
	cp = &contents[o+34+2*numberOfGlyphs];
	for (i = 0; i < numberNewGlyphs; i++) {
		if (cp >= &contents[o + table_directories[pos_post].length])
			break;
		ExtraStrings[i] = sp;
		n = *cp++ & 0377;
		if (&cp[n] > &contents[o + table_directories[pos_post].length])
			break;
		for (j = 0; j < n; j++)
			*sp++ = *cp++;
		*sp++ = 0;
	}
	nExtraStrings = i;
	for (i = 0; i < numberOfGlyphs; i++) {
		n = pbe16(&contents[o+34+2*i]);
		onechar(i, n);
	}
}

static void
get_ttf_post_2_5(int o)
{
	int	numberOfGlyphs;
	int	i, offset;

	numberOfGlyphs = pbe16(&contents[o+32]);
	if (34+numberOfGlyphs > table_directories[pos_post].length)
		error("numberOfGlyphs value in post table too large");
	otfalloc(numberOfGlyphs);
	for (i = 0; i < numberOfGlyphs; i++) {
		offset = ((signed char *)contents)[o+34+i];
		onechar(i, i + offset);
	}
}

static void
unichar(int gid, int c)
{
	int	i;
	char	*sp;

	for (i = 0; WGL[i].s; i++)
		if (WGL[i].u == c) {
			onechar(gid, i);
			return;
		}
	if (ExtraStrings == NULL)
		ExtraStrings = calloc(nc, sizeof *ExtraStrings);
	if (ExtraStringSpace == NULL)
		ExtraStringSpace = malloc(nc * 12);
	sp = &ExtraStringSpace[ExtraStringSpacePos];
	ExtraStrings[nExtraStrings] = sp;
	ExtraStringSpacePos += snprintf(sp, 10, "uni%04X", c) + 1;
	onechar(gid, nWGL + nExtraStrings++);
}

#if !defined (DPOST) && !defined (DUMP)

#include "unimap.h"

static void
addunimap(int gid, int c)
{
	struct unimap	***up, *u, *ut;
	int	x, y;

	if (c != 0 && (c&~0xffff) == 0) {
		if (a->unimap == NULL)
			a->unimap = calloc(256, sizeof *up);
		up = a->unimap;
		x = c >> 8;
		y = c & 0377;
		if (up[x] == NULL)
			up[x] = calloc(256, sizeof **up);
		u = calloc(1, sizeof *u);
		u->u.code = gid;
		if (up[x][y] != NULL) {
			for (ut = up[x][y]; ut->next;
					ut = ut->next);
			ut->next = u;
		} else
			up[x][y] = u;
	}
}
#endif	/* !DPOST && !DUMP */

static void
addunitab(int c, int u)
{
#if !defined (DPOST) && !defined (DUMP)
	if (c >= a->nunitab) {
		a->unitab = realloc(a->unitab, (c+1) * sizeof *a->unitab);
		memset(&a->unitab[a->nunitab], 0,
				(c+1-a->nunitab) * sizeof *a->unitab);
		a->nunitab = c+1;
	}
	a->unitab[c] = u;

	addunimap(c, u);
#endif
}

static char	*got_gid;

static void
got_mapping(int c, int gid, int addchar)
{
	if (gid < nc) {
		if (addchar) {
			if (!got_gid[gid]) {
				unichar(gid, c);
				got_gid[gid] = 1;
			}
		} else {
			addunitab(a->gid2tr[gid].ch1, c);
			addunitab(a->gid2tr[gid].ch2, c);
		}
	}
}

static int
get_ms_unicode_cmap4(int o, int addchar)
{
	/* int	length; */
	int	segCount;
	int	endCount;
	int	startCount;
	int	idDelta;
	int	idRangeOffset;
	/* int	glyphIdArray; */
	int	c, e, i, d, r, s, gid, x;

	/* length = */ pbe16(&contents[o+2]);
	segCount = pbe16(&contents[o+6]) / 2;
	endCount = o + 14;
	startCount = endCount + 2*segCount + 2;
	idDelta = startCount + 2*segCount;
	idRangeOffset = idDelta + 2*segCount;
	/* glyphIdArray = idRangeOffset + 2*segCount; */
	for (i = 0; i < segCount; i++) {
		s = pbe16(&contents[startCount+2*i]);
		e = pbe16(&contents[endCount+2*i]);
		d = pbe16(&contents[idDelta+2*i]);
		r = pbe16(&contents[idRangeOffset+2*i]);
		for (c = s; c <= e; c++) {
			if (r) {
				x = r + 2*(c - s) + idRangeOffset+2*i;
				if (x+1 >=
					    table_directories[pos_cmap].offset +
					    table_directories[pos_cmap].length)
					continue;
				gid = pbe16(&contents[x]);
				if (gid != 0)
					gid += d;
			} else
				gid = c + d;
			gid &= 0xffff;
			if (gid != 0)
				got_mapping(c, gid, addchar);
		}
	}
	return 1;
}

static int
get_ms_unicode_cmap12(int o, int addchar)
{
	/* int	length; */
	int	nGroups;
	int	startCharCode;
	int	endCharCode;
	int	startGlyphID;
	int	c, i, gid;

	/* length = */ pbe32(&contents[o+4]);
	nGroups = pbe32(&contents[o+12]);
	o += 16;
	for (i = 0; i < nGroups; i++) {
		startCharCode = pbe32(&contents[o]);
		endCharCode = pbe32(&contents[o+4]);
		startGlyphID = pbe32(&contents[o+8]);
		for (c = startCharCode, gid = startGlyphID; c <= endCharCode; c++, gid++)
			got_mapping(c, gid, addchar);
		o += 12;
	}
	return 1;
}

static int
get_ms_unicode_cmap(int o, int addchar)
{
	int	format;

	format = pbe16(&contents[o]);
	switch (format) {
	case 4:
		return get_ms_unicode_cmap4(o, addchar);
	case 12:
		return get_ms_unicode_cmap12(o, addchar);
	default:
		return 0;
	}
}

static int
get_cmap(int addchar)
{
	int	numTables;
	int	platformID;
	int	encodingID;
	int	offset;
	int	i, o;
	int	want_tbl;
	int	gotit = 0;

	if (pos_cmap < 0) {
		if (addchar)
			error("no cmap table");
		return gotit;
	}
	o = table_directories[pos_cmap].offset;
	if (pbe16(&contents[o]) != 0) {
		if (addchar)
			error("can only handle version 0 cmap tables");
		return gotit;
	}
	numTables = pbe16(&contents[o+2]);
	if (4 + 8*numTables > table_directories[pos_cmap].length) {
		if (addchar)
			error("cmap table too small for values inside");
		return gotit;
	}
	if (addchar)
		otfalloc(numGlyphs);
	want_tbl = -1;
	for (i = 0; i < numTables; i++) {
		platformID = pbe16(&contents[o+4+8*i]);
		encodingID = pbe16(&contents[o+4+8*i+2]);
		if ((platformID == 3 && encodingID == 10) ||
				(want_tbl < 0 &&
				((platformID == 3 && (encodingID == 0 || encodingID == 1)) ||
				platformID == 0)))
			want_tbl = i;
	}
	if (want_tbl >= 0) {
		offset = pbe32(&contents[o+4+8*want_tbl+4]);
		gotit |= get_ms_unicode_cmap(o + offset, addchar);
	}
	return gotit;
}

static void
get_ttf_post_3_0(int o)
{
	int	i, n;
	int	gotit;
	char	*sp;
	size_t	l;

	ttf = 2;
	got_gid = calloc(numGlyphs, sizeof *got_gid);
	gotit = get_cmap(1);
	if (gotit <= 0) {
		ttf = 3;
		ExtraStrings = calloc(numGlyphs, sizeof *ExtraStrings);
		l = n = 12 * numGlyphs;
		sp = ExtraStringSpace = malloc(l);
		n_strcpy(sp, ".notdef", l);
		ExtraStrings[0] = sp;
		sp += 8;
		nExtraStrings = 1;
		onechar(0, 0);
		for (i = 1; i < numGlyphs; i++) {
			ExtraStrings[i] = sp;
			sp += snprintf(sp, n - (sp - ExtraStringSpace),
					"index0x%02X", i) + 1;
			if (sp >= &ExtraStringSpace[n])
				sp = &ExtraStringSpace[n];
			nExtraStrings++;
			onechar(i, i);
		}
	} else {
		n = numGlyphs * 12;
		if (ExtraStrings == NULL)
			ExtraStrings = calloc(numGlyphs, sizeof *ExtraStrings);
		if (ExtraStringSpace == NULL)
			ExtraStringSpace = malloc(n);
		sp = &ExtraStringSpace[ExtraStringSpacePos];
		for (i = 0; i < numGlyphs; i++)
			if (got_gid[i] == 0) {
				ExtraStrings[nExtraStrings] = sp;
				sp += snprintf(sp, n - (sp - ExtraStringSpace),
						"index0x%02X", i) + 1;
				if (sp >= &ExtraStringSpace[n])
					sp = &ExtraStringSpace[n];
				onechar(i, nWGL + nExtraStrings++);
			}
	}
	free(got_gid);
	got_gid = NULL;
}

static void
ttfname(void)
{
	if (a) {
		if (PostScript_name && strchr(PostScript_name, ' ') == NULL)
			a->fontname = strdup(PostScript_name);
		else {
			const char *base = a->Font.namefont[0] ?
				a->Font.namefont :
				a->base;
			size_t l = strlen(base) + 5;
			a->fontname = malloc(l);
			n_strcpy(a->fontname, base, l);
			n_strcat(a->fontname, ".TTF", l);
		}
#ifdef	DUMP
		print(SHOW_NAME, "name %s", a->fontname);
#endif
	}
}

static void
get_ttf(void)
{
	long	o;
	int	Version;

	if (pos_post < 0)
		error("no post table");
	o = table_directories[pos_post].offset;
	switch (Version = pbe32(&contents[o])) {
	case 0x00010000:
		ttfname();
		get_ttf_post_1_0(o);
		break;
	case 0x00020000:
		ttfname();
		get_ttf_post_2_0(o);
		break;
	case 0x00025000:
		ttfname();
		get_ttf_post_2_5(o);
		break;
	case 0x00030000:
		ttfname();
		get_ttf_post_3_0(o);
		break;
	default:
		error("cannot handle TrueType fonts with "
				"version %d.%d post table",
				Version>>16, (Version&0xffff) >> 12);
	}
	if (a)
		afmremap(a);
}

static void
get_head(void)
{
	long	o;

	if (pos_head < 0)
		error("no head table");
	o = table_directories[pos_head].offset;
	if (pbe32(&contents[o]) != 0x00010000)
		error("can only handle version 1.0 head tables");
	unitsPerEm = pbe16(&contents[o + 18]);
	xMin = (int16_t)pbe16(&contents[o + 36]);
	yMin = (int16_t)pbe16(&contents[o + 38]);
	xMax = (int16_t)pbe16(&contents[o + 40]);
	yMax = (int16_t)pbe16(&contents[o + 42]);
	indexToLocFormat = pbe16(&contents[o + 50]);
}

static void
get_post(void)
{
	long	o;

	isFixedPitch = 0;
	minMemType42 = maxMemType42 = -1;
	if (pos_post < 0)
		return;
	o = table_directories[pos_post].offset;
	if (pbe32(&contents[o]) > 0x00030000)
		return;
	if (table_directories[pos_post].length >= 16)
		isFixedPitch = pbe32(&contents[o+12]);
	if (a)
		a->isFixedPitch = isFixedPitch;
	if (table_directories[pos_post].length >= 20)
		minMemType42 = pbe32(&contents[o+16]);
	if (table_directories[pos_post].length >= 24)
		maxMemType42 = pbe32(&contents[o+20]);
}

static void
get_maxp(void)
{
	if (pos_maxp < 0)
		error("no maxp table");
	numGlyphs = pbe16(&contents[table_directories[pos_maxp].offset+4]);
}

static char *
build_string(int o, int length, int ucs)
{
	char	*string, *sp;
	int	i;
	int	ch;

	sp = string = malloc(3*length + 1);
	for (i = 0; i < length; i++) {
		if (ucs) {
			ch = pbe16(&contents[o+i]);
			i++;
		} else
			ch = contents[o+i]&0377;
		if ((ch & 0200) == 0) {
			switch (ch) {
			case '\\':
			case '(':
			case ')':
				*sp++ = '\\';
				/*FALLTHRU*/
			default:
				*sp++ = ch;
			}
		} else if (ch == 169) {
			/*
			 * 169 happens to be COPYRIGHT SIGN in both MacRoman and
			 * Unicode.
			 */
			*sp++ = '(';
			*sp++ = 'c';
			*sp++ = ')';
		}
	}
	*sp = 0;
	return string;
}

static void
get_name(void)
{
	char	**sp;
	long	o;
	int	count;
	int	stringOffset;
	int	i;
	int	platformID;
	int	encodingID;
	int	languageID;
	int	nameID;
	int	length;
	int	offset;

	if (pos_name < 0)
		return;
	o = table_directories[pos_name].offset;
	if (pbe16(&contents[o]) != 0)
		return;
	count = pbe16(&contents[o+2]);
	stringOffset = o + pbe16(&contents[o+4]);
	for (i = 0; i < count; i++) {
		platformID = pbe16(&contents[o+6+12*i]);
		encodingID = pbe16(&contents[o+6+12*i+2]);
		languageID = pbe16(&contents[o+6+12*i+4]);
		nameID = pbe16(&contents[o+6+12*i+6]);
		length = pbe16(&contents[o+6+12*i+8]);
		offset = pbe16(&contents[o+6+12*i+10]);
		switch (nameID) {
		case 0:
			sp = &Copyright;
			break;
		case 6:
			sp = &PostScript_name;
			break;
		case 7:
			sp = &Notice;
			break;
		default:
			sp = NULL;
		}
		if (sp != NULL && *sp == NULL) {
			if (platformID == 1 && encodingID == 0 && languageID == 0)
				*sp = build_string(stringOffset+offset, length, 0);
			else if (platformID == 3 && languageID == 0x409)
				*sp = build_string(stringOffset+offset, length, 1);
		}
	}
}

static void
get_OS_2(void)
{
	long	o;

	if (pos_OS_2 < 0)
		goto dfl;
	o = table_directories[pos_OS_2].offset;
	if (pbe16(&contents[o]) > 0x0003)
		goto dfl;
	if (table_directories[pos_OS_2].length >= 6)
		WeightClass = pbe16(&contents[o+4]);
	else
		WeightClass = -1;
	if (table_directories[pos_OS_2].length >= 10)
		fsType = pbe16(&contents[o+8]);
	else
		fsType = -1;
	if (table_directories[pos_OS_2].length >= 72) {
		if (a) {
			a->ascender =
				_unitconv((int16_t)pbe16(&contents[o + 68]));
			a->descender =
				_unitconv((int16_t)pbe16(&contents[o + 70]));
		}
	}
	if (table_directories[pos_OS_2].length >= 92) {
		if (a) {
			a->xheight = _unitconv(pbe16(&contents[o + 88]));
			a->capheight = _unitconv(pbe16(&contents[o + 90]));
		}
	} else {
	dfl:	if (a) {
			a->xheight = 500;
			a->capheight = 700;
		}
	}
}

static char *
GID2SID(int gid)
{
	if (gid < 0 || gid >= nc)
		return NULL;
	return getSID(gid2sid[gid]);
}

int
fprintenc(FILE *fd, const char *enc)
{
	const char *cp;
	for (cp = enc; *cp && !isspace(*cp); cp++);
	if (*cp) {
		return fprintf(fd, "(%s) cvn", enc);
	} else {
		return fprintf(fd, "/%s", enc);
	}
}

#ifndef	DPOST
static int	ScriptList;
static int	FeatureList;
static int	LookupList;

struct cov {
	int	offset;
	int	CoverageFormat;
	int	RangeCount;
	int	GlyphCount;
	int	cnt;
	int	gid;
};

static struct cov *
open_cov(int o)
{
	struct cov	*cp;

	cp = calloc(1, sizeof *cp);
	cp->offset = o;
	switch (cp->CoverageFormat = pbe16(&contents[o])) {
	default:
		free(cp);
		return NULL;
	case 1:
		cp->GlyphCount = pbe16(&contents[o+2]);
		return cp;
	case 2:
		cp->RangeCount = pbe16(&contents[o+2]);
		cp->gid = -1;
		return cp;
	}
}

static int
get_cov(struct cov *cp)
{
	int	Start, End;

	switch (cp->CoverageFormat) {
	default:
		return -1;
	case 1:
		if (cp->cnt < cp->GlyphCount)
			return pbe16(&contents[cp->offset+4+2*cp->cnt++]);
		return -1;
	case 2:
		while (cp->cnt < cp->RangeCount) {
			Start = pbe16(&contents[cp->offset+4+6*cp->cnt]);
			End = pbe16(&contents[cp->offset+4+6*cp->cnt+2]);
			if (cp->gid > End) {
				cp->gid = -1;
				cp->cnt++;
				continue;
			}
			if (cp->gid < Start)
				cp->gid = Start;
			return cp->gid++;
		}
		return -1;
	}
}

static void
free_cov(struct cov *cp)
{
	free(cp);
}

struct class {
	int	offset;
	int	ClassFormat;
	int	StartGlyph;
	int	GlyphCount;
	int	ClassRangeCount;
	int	cnt;
	int	gid;
};

static struct class *
open_class(int o)
{
	struct class	*cp;

	cp = calloc(1, sizeof *cp);
	cp->offset = o;
	switch (cp->ClassFormat = pbe16(&contents[o])) {
	default:
		free(cp);
		return NULL;
	case 1:
		cp->StartGlyph = pbe16(&contents[o+2]);
		cp->GlyphCount = pbe16(&contents[o+4]);
		return cp;
	case 2:
		cp->ClassRangeCount = pbe16(&contents[o+2]);
		cp->gid = -1;
		return cp;
	}
}

static inline void
get_class(struct class *cp, int *gp, int *vp)
{
	int	Start, End;

	switch (cp->ClassFormat) {
	case 1:
		if (cp->cnt < cp->GlyphCount) {
			*gp = cp->StartGlyph + cp->cnt;
			*vp = _pbe16(&contents[cp->offset+6+2*cp->cnt]);
			cp->cnt++;
			return;
		}
		goto dfl;
	case 2:
		while (cp->cnt < cp->ClassRangeCount) {
			Start = _pbe16(&contents[cp->offset+4+6*cp->cnt]);
			End = _pbe16(&contents[cp->offset+4+6*cp->cnt+2]);
			if (cp->gid > End) {
				cp->gid = -1;
				cp->cnt++;
				continue;
			}
			if (cp->gid < Start)
				cp->gid = Start;
			*gp = cp->gid++;
			*vp = _pbe16(&contents[cp->offset+4+6*cp->cnt+4]);
			return;
		}
		/*FALLTHRU*/
	default:
	dfl:	*gp = -1;
		*vp = -1;
		return;
	}
}

static void
free_class(struct class *cp)
{
	free(cp);
}

static int
get_value_size(int ValueFormat1, int ValueFormat2)
{
	int	i, sz = 0;

	for (i = 0; i < 16; i++)
		if (ValueFormat1 & (1<<i))
			sz += 2;
	for (i = 0; i < 16; i++)
		if (ValueFormat2 & (1<<i))
			sz += 2;
	return sz;
}

static inline int
get_x_adj(int ValueFormat1, int o)
{
	int	x = 0;
	int	z = 0;

	if (ValueFormat1 & 0x0001) {
		x += (int16_t)_pbe16(&contents[o+z]);
		z += 2;
	}
	if (ValueFormat1 & 0x0002)
		z += 2;
	if (ValueFormat1 & 0x0004) {
		x += (int16_t)_pbe16(&contents[o+z]);
		z += 2;
	}
	return x;
}

static void	kerninit(void);
static void	kernfinish(void);

static int	got_kern;

#ifdef	DUMP
static void	kernpair(int, int, int);
#else	/* !DUMP */

static struct namecache	**nametable;

static void
kerninit(void)
{
	char	*cp;
	int	i;

	got_kern = 0;
	nametable = calloc(nc, sizeof *nametable);
	for (i = 0; i < nc; i++)
		if ((cp = GID2SID(i)) != NULL)
			nametable[i] = afmnamelook(a, cp);
}

#define	GID2name(gid)	((gid) < 0 || (gid) >= nc ? NULL : nametable[gid])

static inline void
kernpair(int first, int second, int x)
{
	struct namecache	*np1, *np2;

	if (x == 0 || (x = _unitconv(x)) == 0)
		return;
	np1 = GID2name(first);
	np2 = GID2name(second);
	if (np1 == NULL || np2 == NULL)
		return;
	if (np1->fival[0] != NOCODE && np2->fival[0] != NOCODE)
		afmaddkernpair(a, np1->fival[0], np2->fival[0], x);
	if (np1->fival[0] != NOCODE && np2->fival[1] != NOCODE)
		afmaddkernpair(a, np1->fival[0], np2->fival[1], x);
	if (np1->fival[1] != NOCODE && np2->fival[0] != NOCODE)
		afmaddkernpair(a, np1->fival[1], np2->fival[0], x);
	if (np1->fival[1] != NOCODE && np2->fival[1] != NOCODE)
		afmaddkernpair(a, np1->fival[1], np2->fival[1], x);
}

static void
kernfinish(void)
{
	free(nametable);
}
#endif	/* !DUMP */

static void
get_PairValueRecord(int first, int ValueFormat1, int ValueFormat2, int o)
{
	int	second;
	int	x;

	second = _pbe16(&contents[o]);
	x = get_x_adj(ValueFormat1, o+2);
	kernpair(first, second, x);
}

static void
get_PairSet(int first, int ValueFormat1, int ValueFormat2, int o)
{
	int	PairValueCount;
	int	i;
	int	sz;

	PairValueCount = _pbe16(&contents[o]);
	sz = get_value_size(ValueFormat1, ValueFormat2);
	for (i = 0; i < PairValueCount; i++)
		get_PairValueRecord(first, ValueFormat1, ValueFormat2,
				o+2+(2+sz)*i);
}

static void
get_PairPosFormat1(int o)
{
	struct cov	*cp;
	int	Coverage;
	int	ValueFormat1, ValueFormat2;
	int	PairSetCount;
	int	first;
	int	i;

	Coverage = o + pbe16(&contents[o+2]);
	if ((cp = open_cov(Coverage)) == NULL)
		return;
	ValueFormat1 = pbe16(&contents[o+4]);
	ValueFormat2 = pbe16(&contents[o+6]);
	PairSetCount = pbe16(&contents[o+8]);
	for (i = 0; i < PairSetCount && (first = get_cov(cp)) >= 0; i++)
		get_PairSet(first, ValueFormat1, ValueFormat2,
				o + pbe16(&contents[o+10+2*i]));
	free_cov(cp);
}

static void
get_PairPosFormat2(int o)
{
	struct class	*c1, *c2;
	int	ValueFormat1, ValueFormat2;
	int	ClassDef1, ClassDef2;
	int	Class1Count, Class2Count;
	int	g, *g2 = NULL;
	int	v, *v2 = NULL;
	int	sz;
	int	i, n, a;
	int	x;

	ValueFormat1 = pbe16(&contents[o+4]);
	ValueFormat2 = pbe16(&contents[o+6]);
	ClassDef1 = o + pbe16(&contents[o+8]);
	ClassDef2 = o + pbe16(&contents[o+10]);
	Class1Count = pbe16(&contents[o+12]);
	Class2Count = pbe16(&contents[o+14]);
	sz = get_value_size(ValueFormat1, ValueFormat2);
	if ((c1 = open_class(ClassDef1)) != NULL) {
		if ((c2 = open_class(ClassDef2)) != NULL) {
			n = a = 0;
			while (get_class(c2, &g, &v), g >= 0) {
				if (v < 0 || v >= Class2Count)
					continue;
				if (n >= a) {
					a = a ? 2*a : 128;
					g2 = realloc(g2, a * sizeof *g2);
					v2 = realloc(v2, a * sizeof *v2);
				}
				g2[n] = g;
				v2[n] = v;
				n++;
			}
			while (get_class(c1, &g, &v), g >= 0) {
				if (v < 0 || v >= Class1Count)
					continue;
				for (i = 0; i < n; i++) {
					x = get_x_adj(ValueFormat1,
						o + 16 +
						v*Class2Count*sz +
						v2[i]*sz);
					kernpair(g, g2[i], x);
				}
			}
			free_class(c2);
		}
		free_class(c1);
	}
	free(g2);
	free(v2);
}

static void
get_GPOS_kern1(int _t, int o, const char *_name)
{
	int	PosFormat;

	got_kern = 1;
	switch (PosFormat = pbe16(&contents[o])) {
	case 1:
		get_PairPosFormat1(o);
		break;
	}
}

static void
get_GPOS_kern2(int _t, int o, const char *_name)
{
	int	PosFormat;

	got_kern = 1;
	switch (PosFormat = pbe16(&contents[o])) {
	case 2:
		get_PairPosFormat2(o);
		break;
	}
}

static void
get_Ligature(int first, int o)
{
	int	LigGlyph;
	int	CompCount;
	int	Component[16];
	int	i;
	char	*gn;

	LigGlyph = pbe16(&contents[o]);
	CompCount = pbe16(&contents[o+2]);
	for (i = 0; i < CompCount - 1 &&
			i < sizeof Component / sizeof *Component - 1; i++) {
		Component[i] = pbe16(&contents[o+4+2*i]);
	}
	Component[i] = -1;
	gn = GID2SID(first);
	if (gn && gn[0] == 'f' && gn[1] == 0 && CompCount > 1) {
		gn = GID2SID(Component[0]);
		if (gn && gn[0] && gn[1] == 0) switch (gn[0]) {
		case 'f':
			if (CompCount == 2) {
				gn = GID2SID(LigGlyph);
				if (gn && (strcmp(gn, "ff") == 0 ||
						strcmp(gn, "f_f") == 0))
					a->Font.ligfont |= LFF;
			} else if (CompCount == 3) {
				gn = GID2SID(Component[1]);
				if (gn[0] && gn[1] == 0) switch (gn[0]) {
				case 'i':
					gn = GID2SID(LigGlyph);
					if (gn && (strcmp(gn, "ffi") == 0 ||
						    strcmp(gn, "f_f_i") == 0))
						a->Font.ligfont |= LFFI;
					break;
				case 'l':
					gn = GID2SID(LigGlyph);
					if (gn && (strcmp(gn, "ffl") == 0 ||
						    strcmp(gn, "f_f_l") == 0))
						a->Font.ligfont |= LFFL;
					break;
				}
			}
			break;
		case 'i':
			if (CompCount == 2) {
				gn = GID2SID(LigGlyph);
				if (gn && (strcmp(gn, "fi") == 0 ||
						strcmp(gn, "f_i") == 0))
					a->Font.ligfont |= LFI;
			}
			break;
		case 'l':
			if (CompCount == 2) {
				gn = GID2SID(LigGlyph);
				if (gn && (strcmp(gn, "fl") == 0 ||
						strcmp(gn, "f_l") == 0))
					a->Font.ligfont |= LFL;
			}
			break;
		}
	}
}

static void
get_LigatureSet(int first, int o)
{
	int	LigatureCount;
	int	i;

	LigatureCount = pbe16(&contents[o]);
	for (i = 0; i < LigatureCount; i++)
		get_Ligature(first, o + pbe16(&contents[o+2+2*i]));
}

static void
get_LigatureSubstFormat1(int _t, int o, const char *_name)
{
	struct cov	*cp;
	int	Coverage;
	int	LigSetCount;
	int	i;
	int	first;

	if (pbe16(&contents[o]) != 1)
		return;
	Coverage = o + pbe16(&contents[o+2]);
	if ((cp = open_cov(Coverage)) == NULL)
		return;
	LigSetCount = pbe16(&contents[o+4]);
	for (i = 0; i < LigSetCount && (first = get_cov(cp)) >= 0; i++)
		get_LigatureSet(first, o + pbe16(&contents[o+6+2*i]));
	free_cov(cp);
}

static struct feature *
add_feature(const char *name)
{
	int	i;
	char	*np;

	if (a->features == NULL)
		a->features = calloc(1, sizeof *a->features);
	for (i = 0; a->features[i]; i++)
		if (strcmp(a->features[i]->name, name) == 0)
			return a->features[i];
	a->features = realloc(a->features, (i+2) * sizeof *a->features);
	a->features[i] = calloc(1, sizeof **a->features);
	a->features[i]->name = strdup(name);
	for (np = a->features[i]->name; *np; np++)
		if (*np == ' ') {
			*np = 0;
			break;
		}
	a->features[i+1] = NULL;
	return a->features[i];
}

static void
add_substitution_pair1(struct feature *fp, int ch1, int ch2)
{
	fp->pairs = realloc(fp->pairs, (fp->npairs+1) * sizeof *fp->pairs);
	fp->pairs[fp->npairs].ch1 = ch1;
	fp->pairs[fp->npairs].ch2 = ch2;
	fp->npairs++;
}

static void
add_substitution_pair(struct feature *fp, int ch1, int ch2)
{
	if (ch1 && ch2) {
#ifdef	DUMP
		print(SHOW_SUBSTITUTIONS, "feature %s substitution %s %s",
				fp->name, GID2SID(ch1), GID2SID(ch2));
#endif
		if (a->gid2tr[ch1].ch1) {
			if (a->gid2tr[ch2].ch1)
				add_substitution_pair1(fp,
						a->gid2tr[ch1].ch1,
						a->gid2tr[ch2].ch1);
			if (a->gid2tr[ch2].ch2)
				add_substitution_pair1(fp,
						a->gid2tr[ch1].ch1,
						a->gid2tr[ch2].ch2);
		}
		if (a->gid2tr[ch1].ch2) {
			if (a->gid2tr[ch2].ch1)
				add_substitution_pair1(fp,
						a->gid2tr[ch1].ch2,
						a->gid2tr[ch2].ch1);
			if (a->gid2tr[ch2].ch2)
				add_substitution_pair1(fp,
						a->gid2tr[ch1].ch2,
						a->gid2tr[ch2].ch2);
		}
	}
}

static void
get_SingleSubstitutionFormat1(int o, const char *name)
{
	struct feature	*fp;
	struct cov	*cp;
	int	c, d;
	int	Coverage;
	int	DeltaGlyphID;

	if (pbe16(&contents[o]) != 1)
		return;
	Coverage = o + pbe16(&contents[o+2]);
	if ((cp = open_cov(Coverage)) == NULL)
		return;
	DeltaGlyphID = pbe16(&contents[o+4]);
	fp = add_feature(name);
	while ((c = get_cov(cp)) >= 0)
		if ((d = c + DeltaGlyphID) < nc)
			add_substitution_pair(fp, c, d);
	free_cov(cp);
}

static void
get_SingleSubstitutionFormat2(int o, const char *name)
{
	struct feature	*fp;
	struct cov	*cp;
	int	Coverage;
	int	GlyphCount;
	int	c, i;

	if (pbe16(&contents[o]) != 2)
		return;
	Coverage = o + pbe16(&contents[o+2]);
	if ((cp = open_cov(Coverage)) == NULL)
		return;
	GlyphCount = pbe16(&contents[o+4]);
	fp = add_feature(name);
	for (i = 0; i < GlyphCount && (c = get_cov(cp)) >= 0; i++)
		add_substitution_pair(fp, c, pbe16(&contents[o+6+2*i]));
	free_cov(cp);
}

static void
get_substitutions(int type, int o, const char *name)
{
	int	format;

	format = pbe16(&contents[o]);
	switch (type) {
	case 1:
		switch (format) {
		case 1:
			get_SingleSubstitutionFormat1(o, name);
			break;
		case 2:
			get_SingleSubstitutionFormat2(o, name);
			break;
		}
	}
}

static void
get_lookup(int o, int type, const char *name,
		void (*func)(int, int, const char *))
{
	int	i, j, t, x, y;
	int	LookupCount;
	int	SubTableCount;

	LookupCount = pbe16(&contents[o+2]);
	for (i = 0; i < LookupCount; i++) {
		x = pbe16(&contents[o+4+2*i]);
		y = pbe16(&contents[LookupList+2+2*x]);
		if ((t = pbe16(&contents[LookupList+y])) == type || type < 0) {
			SubTableCount = pbe16(&contents[LookupList+y+4]);
			for (j = 0; j < SubTableCount; j++)
				func(t, LookupList+y +
					pbe16(&contents[LookupList+y+6+2*j]),
					name);
		}
	}
}

static void
get_LangSys(int o, const char *name, int type,
		void (*func)(int, int, const char *))
{
	char	nb[5];
	int	i, x;
	int	FeatureCount;
	int	ReqFeatureIndex;

	ReqFeatureIndex = pbe16(&contents[o+2]);
	FeatureCount = pbe16(&contents[o+4]);
	if (ReqFeatureIndex != 0xFFFF)
		FeatureCount += ReqFeatureIndex;
	for (i = 0; i < FeatureCount; i++) {
		x = pbe16(&contents[o+6+2*i]);
		if (name == NULL ||
			   memcmp(&contents[FeatureList+2+6*x], name, 4) == 0) {
			memcpy(nb, &contents[FeatureList+2+6*x], 4);
			nb[4] = 0;
			get_lookup(FeatureList +
				pbe16(&contents[FeatureList+2+6*x+4]),
				type, nb, func);
		}
	}
}

static void
get_feature(int table, const char *name, int type,
		void (*func)(int, int, const char *))
{
	long	o;
	int	i;
	int	DefaultLangSys;
	int	ScriptCount;
	int	Script;

	if (table < 0)
		return;
	o = table_directories[table].offset;
	if (pbe32(&contents[o]) != 0x00010000)
		return;
	ScriptList = o + pbe16(&contents[o+4]);
	FeatureList = o + pbe16(&contents[o+6]);
	LookupList = o + pbe16(&contents[o+8]);
	ScriptCount = pbe16(&contents[ScriptList]);
	for (i = 0; i < ScriptCount; i++)
		if (memcmp(&contents[ScriptList+2+6*i], "DFLT", 4) == 0 ||
				memcmp(&contents[ScriptList+2+6*i],
					"latn", 4) == 0) {
			Script = ScriptList +
				pbe16(&contents[ScriptList+2+6*i+4]);
			DefaultLangSys = Script + pbe16(&contents[Script]);
			get_LangSys(DefaultLangSys, name, type, func);
		}
}

static void
get_kern_subtable(int o)
{
	int	length;
	int	coverage;
	int	nPairs;
	int	i;
	int	left, right, value;

	if (pbe16(&contents[o]) != 0)
		return;
	length = pbe16(&contents[o+2]);
	coverage = pbe16(&contents[o+4]);
	if ((coverage&1) != 1 ||		/* check: horizontal data */
			(coverage&2) != 0 ||	/* . . . kerning values */
			(coverage&4) != 0 ||	/* . . . not perpendicular */
			((coverage&0xff00) != 0))	/* . . . format 0 */
		return;
	got_kern = 1;
	nPairs = pbe16(&contents[o+6]);
	for (i = 0; i < nPairs; i++) {
		if (o + 14 + 6 * (i+1) > o + length)
			break;
		left = pbe16(&contents[o+14+6*i]);
		right = pbe16(&contents[o+14+6*i+2]);
		value = (int16_t)pbe16(&contents[o+14+6*i+4]);
		kernpair(left, right, value);
	}
}

static void
get_kern(void)
{
	long	o;
	int	nTables;
	int	i, length;

	if (pos_kern < 0)
		return;
	o = table_directories[pos_kern].offset;
	if (pbe16(&contents[o]) != 0)
		return;
	nTables = pbe16(&contents[o+2]);
	o += 4;
	for (i = 0; i < nTables; i++) {
		if (o + 6 > table_directories[pos_kern].offset +
				table_directories[pos_kern].length)
			return;
		length = pbe16(&contents[o+2]);
		if (o + length > table_directories[pos_kern].offset +
				table_directories[pos_kern].length)
			return;
		get_kern_subtable(o);
		o += length;
	}
}

#endif	/* !DPOST */

#ifdef	DPOST
static void
checkembed(void)
{
	/*
	 * Do not check the embedding bits under the assumption that the
	 * resulting PostScript file is sent to a printer. This follows
	 * Adobe's "Font Embedding Guidelines for Adobe Third-party
	 * Developers", 5/16/05, p. 8.
	 *
	 * It is the responsibility of a following distiller command or
	 * the like to check the fsType bit then.
	 *
	if (fsType != -1 && (fsType&0x030e) == 0x0002 || fsType & 0x0200)
		error("embedding not allowed");
	*/
}

int
otfcff(const char *path,
		char *_contents, size_t _size, size_t *offset, size_t *length)
{
	int	ok = 0;

	(void) &ok;
	a = NULL;
	filename = path;
	contents = _contents;
	size = _size;
	if (setjmp(breakpoint) == 0) {
		get_offset_table();
		get_table_directories();
		get_OS_2();
		if (pos_CFF < 0)
			error("no CFF table");
		checkembed();
		*offset = table_directories[pos_CFF].offset;
		*length = table_directories[pos_CFF].length;
	} else
		ok = -1;
	return ok;
}

static uint32_t
CalcTableChecksum(uint32_t sum, const char *cp, int length)
{
	while (length > 0) {
		sum += pbe32(cp);
		cp += 4;
		length -= 4;
	}
	return sum;
}

static void
sfnts1(struct table *tp, int *offset, uint32_t *ccs, FILE *fp)
{
	int	o, length;

	o = table_directories[*tp->pos].offset;
	length = table_directories[*tp->pos].length;
	if (tp->in_sfnts == 2)	/* head table */
		memset(&contents[o+8], 0, 4);	/* checkSumAdjustment */
	tp->checksum = CalcTableChecksum(0, &contents[o], length);
	*ccs = CalcTableChecksum(*ccs, tp->name, 4);
	*ccs += tp->checksum;
	*ccs += *offset;
	*ccs += length;
	*offset += length;
}

static void
sfnts1a(struct table *tp, int *offset, uint32_t *ccs, FILE *fp)
{
	int	o, length, m;

	o = table_directories[*tp->pos].offset;
	length = table_directories[*tp->pos].length;
	if (tp->in_sfnts == 2) {
		*ccs -= 0xB1B0AFBA;
		contents[o+8] = (*ccs&0xff000000) >> 24;
		contents[o+9] = (*ccs&0x00ff0000) >> 16;
		contents[o+10] = (*ccs&0x0000ff00) >> 8;
		contents[o+11] = (*ccs&0x000000ff);
	}
	fprintf(fp, "%08X%08X%08X%08X",
			pbe32(tp->name),
			(unsigned int)tp->checksum,
			*offset, length);
	if ((m = length % 4) != 0)
		length += 4 - m;
	*offset += length;
}

static int
start_of_next_glyph(int *start, int offset)
{
	int	i = *start;
	int	last = INT_MAX;
	int	cur;
	int	o;
	int	tms = 0;

	if (pos_loca < 0)
		error("no loca table");
	o = table_directories[pos_loca].offset;
	for (;;) {
		if (i >= numGlyphs) {
			i = 0;
			if (tms++ == 4)
				return -1;
		}
		cur = indexToLocFormat ? pbe32(&contents[o + 4*i]) :
			pbe16(&contents[o + 2*i]) * 2;
		if (offset > last && offset < cur) {
			*start = i;
			return cur;
		}
		if (cur < last)
			last = cur;
		i++;
	}
}

static void
sfnts2(struct table *tp, FILE *fp)
{
	const char	hex[] = "0123456789ABCDEF";
	int	i, o, length, next = -1;
	int	start = 0;

	o = table_directories[*tp->pos].offset;
	length = table_directories[*tp->pos].length;
	putc('<', fp);
	for (i = 0; i < length; i++) {
		if (i && i % 36 == 0)
			putc('\n', fp);
		if (i && i % 60000 == 0 && tp->in_sfnts == 3) {
			/* split string at start of next glyph */
			next = start_of_next_glyph(&start, i);
		}
		if (i == next)
			fprintf(fp, "00><");
		if (i && i % 65534 == 0 && tp->in_sfnts != 3)
			fprintf(fp, "00>\n<");
		putc(hex[(contents[o+i]&0360)>>4], fp);
		putc(hex[contents[o+i]&017], fp);
	}
	while (i++ % 4)
		fprintf(fp, "00");
	fprintf(fp, "00>\n");
}

static void
build_sfnts(FILE *fp)
{
	int	i, o, n;
	unsigned short	numTables;
	unsigned short	searchRange;
	unsigned short	entrySelector;
	unsigned short	rangeShift;
	uint32_t	ccs;

	numTables = 0;
	for (i = 0; tables[i].name; i++)
		if (tables[i].in_sfnts && *tables[i].pos >= 0)
			numTables++;
	entrySelector = 0;
	for (searchRange = 1; searchRange*2 < numTables; searchRange *= 2)
		entrySelector++;
	searchRange *= 16;
	rangeShift = numTables * 16 - searchRange;
	fprintf(fp, "<%08X%04hX%04hX%04hX%04hX\n", 0x00010000,
			numTables, searchRange, entrySelector, rangeShift);
	ccs = 0x00010000 + (numTables<<16) + searchRange +
		(entrySelector<<16) + rangeShift;
	o = 12 + numTables * 16;
	for (i = 0; tables[i].name; i++)
		if (tables[i].in_sfnts && *tables[i].pos >= 0)
			sfnts1(&tables[i], &o, &ccs, fp);
	o = 12 + numTables * 16;
	n = 0;
	for (i = 0; tables[i].name; i++) {
		if (tables[i].in_sfnts && *tables[i].pos >= 0) {
			if (n++)
				putc('\n', fp);
			sfnts1a(&tables[i], &o, &ccs, fp);
		}
	}
	fprintf(fp, "00>\n");
	for (i = 0; tables[i].name; i++)
		if (tables[i].in_sfnts && *tables[i].pos >= 0)
			sfnts2(&tables[i], fp);
}

int
otft42(char *font, char *path, char *_contents, size_t _size, FILE *fp)
{
	char	*cp;
	int	ok = 0;
	int	i;

	(void) &ok;
	a = NULL;
	filename = path;
	contents = _contents;
	size = _size;
	if (setjmp(breakpoint) == 0) {
		get_offset_table();
		get_table_directories();
		get_head();
		get_OS_2();
		get_post();
		get_maxp();
		get_name();
		if (ttf == 0)
			error("not a TrueType font file");
		checkembed();
		get_ttf();
		if (minMemType42 >= 0 && maxMemType42 >= 0 &&
				(minMemType42 || maxMemType42))
			fprintf(fp, "%%%%VMUsage: %d %d\n",
					minMemType42, maxMemType42);
		fprintf(fp, "11 dict begin\n");
		fprintf(fp, "/FontType 42 def\n");
		fprintf(fp, "/FontMatrix [1 0 0 1 0 0] def\n");
		fprintf(fp, "/FontName /%s def\n", font);
		fprintf(fp, "/FontBBox [%d %d %d %d] def\n",
				xMin * 1000 / unitsPerEm,
				yMin * 1000 / unitsPerEm,
				xMax * 1000 / unitsPerEm,
				yMax * 1000 / unitsPerEm);
		fprintf(fp, "/PaintType 0 def\n");
		fprintf(fp, "/Encoding StandardEncoding def\n");
		if (fsType != -1 || Notice || Copyright || WeightClass) {
			fprintf(fp, "/FontInfo 4 dict dup begin\n");
			if (fsType != -1)
				fprintf(fp, "/FSType %d def\n", fsType);
			if (Notice)
				fprintf(fp, "/Notice (%s) readonly def\n",
						Notice);
			if (Copyright)
				fprintf(fp, "/Copyright (%s) readonly def\n",
						Copyright);
			if (WeightClass) {
				if (WeightClass <= 350)
					cp = "Light";
				else if (WeightClass <= 550)
					cp = "Medium";
				else if (WeightClass <= 750)
					cp = "Bold";
				else if (WeightClass <= 850)
					cp = "Ultra";
				else
					cp = "Heavy";
				fprintf(fp, "/Weight (%s) readonly def\n", cp);
			}
			fprintf(fp, "end readonly def\n");
		}
		fprintf(fp, "/CharStrings %d dict dup begin\n", nc);
		for (i = 0; i < nc; i++) {
			if ((cp = GID2SID(i)) != NULL &&
					(i == 0 || strcmp(cp, ".notdef"))) {
				fprintenc(fp, cp);
				fprintf(fp, " %d def\n", i);
			} else
				fprintf(fp, "/index0x%02X %d def\n", i, i);
		}
		fprintf(fp, "end readonly def\n");
		fprintf(fp, "/sfnts[");
		build_sfnts(fp);
		fprintf(fp, "]def\n");
		fprintf(fp, "FontName currentdict end definefont pop\n");
	} else
		ok = -1;
	free(PostScript_name);
	PostScript_name = 0;
	free(Copyright);
	Copyright = 0;
	free(Notice);
	Notice = 0;
	free(ExtraStringSpace);
	ExtraStringSpace = NULL;
	ExtraStringSpacePos = 0;
	free(ExtraStrings);
	ExtraStrings = NULL;
	nExtraStrings = 0;
	return ok;
}
#endif	/* DPOST */

int
otfget(struct afmtab *_a, char *_contents, size_t _size)
{
	int	ok = 0;

	(void) &ok;
	a = _a;
	filename = a->path;
	contents = _contents;
	size = _size;
	if (setjmp(breakpoint) == 0) {
		get_offset_table();
		get_table_directories();
		get_head();
		get_OS_2();
		get_post();
		if (ttf == 0) {
			a->type = TYPE_OTF;
			get_CFF();
		} else {
			a->type = TYPE_TTF;
			get_maxp();
			get_name();
			get_ttf();
		}
#ifndef	DPOST
		kerninit();
		get_feature(pos_GSUB, "liga", 4, get_LigatureSubstFormat1);
		get_feature(pos_GPOS, "kern", 2, get_GPOS_kern1);
		get_feature(pos_GPOS, "kern", 2, get_GPOS_kern2);
		get_feature(pos_GSUB, NULL, -1, get_substitutions);
		if (ttf && got_kern == 0)
			get_kern();
		kernfinish();
		get_cmap(0);
#endif	/* !DPOST */
		a->Font.nwfont = a->nchars > 255 ? 255 : a->nchars;
	} else
		ok = -1;
	free(PostScript_name);
	PostScript_name = 0;
	free(Copyright);
	Copyright = 0;
	free(Notice);
	Notice = 0;
	free_INDEX(CFF.Name);
	CFF.Name = 0;
	free_INDEX(CFF.Top_DICT);
	CFF.Top_DICT = 0;
	free_INDEX(CFF.String);
	CFF.String = 0;
	free_INDEX(CFF.Global_Subr);
	CFF.Global_Subr = 0;
	free_INDEX(CFF.CharStrings);
	CFF.CharStrings = 0;
	free(ExtraStringSpace);
	ExtraStringSpace = NULL;
	ExtraStringSpacePos = 0;
	free(ExtraStrings);
	ExtraStrings = NULL;
	nExtraStrings = 0;
	free(a->gid2tr);
	a->gid2tr = NULL;
	return ok;
}
