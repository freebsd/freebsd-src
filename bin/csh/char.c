/*-
 * Copyright (c) 1980, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)char.c	8.1 (Berkeley) 5/31/93";
#else
static const char rcsid[] =
	"$Id: char.c,v 1.4 1997/02/22 14:01:37 peter Exp $";
#endif
#endif /* not lint */

#include "char.h"

unsigned short _cmap[256] = {
/*	nul		soh		stx		etx	*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	eot		enq		ack		bel	*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	bs		ht		nl		vt	*/
	_CTR,		_CTR|_SP|_META,	_CTR|_NL|_META,	_CTR,

/*	np		cr		so		si	*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	dle		dc1		dc2		dc3	*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	dc4		nak		syn		etb	*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	can		em		sub		esc	*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	fs		gs		rs		us	*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	sp		!		"		#	*/
	_SP|_META,	0,		_QF,		_META,

/*	$		%		&		'	*/
	_DOL,		0,		_META|_CMD,	_QF,

/*	(		)		*		+	*/
	_META|_CMD,	_META,		_GLOB,		0,

/*	,		-		.		/	*/
	0,		0,		0,		0,

/*	0		1		2		3	*/
	_DIG|_XD,	_DIG|_XD,	_DIG|_XD,	_DIG|_XD,

/*	4		5		6		7	*/
	_DIG|_XD,	_DIG|_XD,	_DIG|_XD,	_DIG|_XD,

/*	8		9		:		;	*/
	_DIG|_XD,	_DIG|_XD,	0,		_META|_CMD,

/*	<		=		>		?	*/
	_META,		0,		_META,		_GLOB,

/*	@		A		B		C	*/
	0,		_LET|_UP|_XD,	_LET|_UP|_XD,	_LET|_UP|_XD,

/*	D		E		F		G	*/
	_LET|_UP|_XD,	_LET|_UP|_XD,	_LET|_UP|_XD,	_LET|_UP,

/*	H		I		J		K	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	L		M		N		O	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	P		Q		R		S	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	T		U		V		W	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	X		Y		Z		[	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_GLOB,

/*	\		]		^		_	*/
	_ESC,		0,		0,		0,

/*	`		a		b		c	*/
  _QB|_GLOB|_META,	_LET|_LOW|_XD,	_LET|_LOW|_XD,	_LET|_LOW|_XD,

/*	d		e		f		g	*/
	_LET|_LOW|_XD,	_LET|_LOW|_XD,	_LET|_LOW|_XD,	_LET|_LOW,

/*	h		i		j		k	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	l		m		n		o	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	p		q		r		s	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	t		u		v		w	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	x		y		z		{	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_GLOB,

/*	|		}		~		del	*/
	_META|_CMD,	0,		0,		_CTR,

#if defined(SHORT_STRINGS) && !defined(KANJI)
/****************************************************************/
/* 128 - 255 The below is supposedly ISO 8859/1			*/
/****************************************************************/
/*	(undef)		(undef)		(undef)		(undef)		*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	(undef)		(undef)		(undef)		(undef)		*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	(undef)		(undef)		(undef)		(undef)		*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	(undef)		(undef)		(undef)		(undef)		*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	(undef)		(undef)		(undef)		(undef)		*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	(undef)		(undef)		(undef)		(undef)		*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	(undef)		(undef)		(undef)		(undef)		*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	(undef)		(undef)		(undef)		(undef)		*/
	_CTR,		_CTR,		_CTR,		_CTR,

/*	nobreakspace	exclamdown	cent		sterling	*/
	_SP,		0,		0,		0,

/*	currency	yen		brokenbar	section		*/
	0,		0,		0,		0,

/*	diaeresis	copyright	ordfeminine	guillemotleft	*/
	0,		0,		0,		0,

/*	notsign		hyphen		registered	macron		*/
	0,		0,		0,		0,

/*	degree		plusminus	twosuperior	threesuperior	*/
	0,		0,		0,		0,

/*	acute		mu		paragraph	periodcentered	*/
	0,		0,		0,		0,

/*	cedilla		onesuperior	masculine	guillemotright	*/
	0,		0,		0,		0,

/*	onequarter	onehalf		threequarters	questiondown	*/
	0,		0,		0,		0,

/*	Agrave		Aacute		Acircumflex	Atilde		*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	Adiaeresis	Aring		AE		Ccedilla	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	Egrave		Eacute		Ecircumflex	Ediaeresis	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	Igrave		Iacute		Icircumflex	Idiaeresis	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	ETH		Ntilde		Ograve		Oacute		*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	Ocircumflex	Otilde		Odiaeresis	multiply	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	0,

/*	Ooblique	Ugrave		Uacute		Ucircumflex	*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_UP,

/*	Udiaeresis	Yacute		THORN		ssharp		*/
	_LET|_UP,	_LET|_UP,	_LET|_UP,	_LET|_LOW,

/*	agrave		aacute		acircumflex	atilde		*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	adiaeresis	aring		ae		ccedilla	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	egrave		eacute		ecircumflex	ediaeresis	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	igrave		iacute		icircumflex	idiaeresis	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	eth		ntilde		ograve		oacute		*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	ocircumflex	otilde		odiaeresis	division	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	0,

/*	oslash		ugrave		uacute		ucircumflex	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,

/*	udiaeresis	yacute		thorn		ydiaeresis	*/
	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,	_LET|_LOW,
#endif /* SHORT_STRINGS && !KANJI */
};

#ifndef NLS
/* _cmap_lower, _cmap_upper for ISO 8859/1 */

unsigned char _cmap_lower[256] = {
	0000,	0001,	0002,	0003,	0004,	0005,	0006,	0007,
	0010,	0011,	0012,	0013,	0014,	0015,	0016,	0017,
	0020,	0021,	0022,	0023,	0024,	0025,	0026,	0027,
	0030,	0031,	0032,	0033,	0034,	0035,	0036,	0037,
	0040,	0041,	0042,	0043,	0044,	0045,	0046,	0047,
	0050,	0051,	0052,	0053,	0054,	0055,	0056,	0057,
	0060,	0061,	0062,	0063,	0064,	0065,	0066,	0067,
	0070,	0071,	0072,	0073,	0074,	0075,	0076,	0077,
	0100,	0141,	0142,	0143,	0144,	0145,	0146,	0147,
	0150,	0151,	0152,	0153,	0154,	0155,	0156,	0157,
	0160,	0161,	0162,	0163,	0164,	0165,	0166,	0167,
	0170,	0171,	0172,	0133,	0134,	0135,	0136,	0137,
	0140,	0141,	0142,	0143,	0144,	0145,	0146,	0147,
	0150,	0151,	0152,	0153,	0154,	0155,	0156,	0157,
	0160,	0161,	0162,	0163,	0164,	0165,	0166,	0167,
	0170,	0171,	0172,	0173,	0174,	0175,	0176,	0177,
	0200,	0201,	0202,	0203,	0204,	0205,	0206,	0207,
	0210,	0211,	0212,	0213,	0214,	0215,	0216,	0217,
	0220,	0221,	0222,	0223,	0224,	0225,	0226,	0227,
	0230,	0231,	0232,	0233,	0234,	0235,	0236,	0237,
	0240,	0241,	0242,	0243,	0244,	0245,	0246,	0247,
	0250,	0251,	0252,	0253,	0254,	0255,	0256,	0257,
	0260,	0261,	0262,	0263,	0264,	0265,	0266,	0267,
	0270,	0271,	0272,	0273,	0274,	0275,	0276,	0277,
	0340,	0341,	0342,	0343,	0344,	0345,	0346,	0347,
	0350,	0351,	0352,	0353,	0354,	0355,	0356,	0357,
	0360,	0361,	0362,	0363,	0364,	0365,	0366,	0327,
	0370,	0371,	0372,	0373,	0374,	0375,	0376,	0337,
	0340,	0341,	0342,	0343,	0344,	0345,	0346,	0347,
	0350,	0351,	0352,	0353,	0354,	0355,	0356,	0357,
	0360,	0361,	0362,	0363,	0364,	0365,	0366,	0367,
	0370,	0371,	0372,	0373,	0374,	0375,	0376,	0377,
};

unsigned char _cmap_upper[256] = {
	0000,	0001,	0002,	0003,	0004,	0005,	0006,	0007,
	0010,	0011,	0012,	0013,	0014,	0015,	0016,	0017,
	0020,	0021,	0022,	0023,	0024,	0025,	0026,	0027,
	0030,	0031,	0032,	0033,	0034,	0035,	0036,	0037,
	0040,	0041,	0042,	0043,	0044,	0045,	0046,	0047,
	0050,	0051,	0052,	0053,	0054,	0055,	0056,	0057,
	0060,	0061,	0062,	0063,	0064,	0065,	0066,	0067,
	0070,	0071,	0072,	0073,	0074,	0075,	0076,	0077,
	0100,	0101,	0102,	0103,	0104,	0105,	0106,	0107,
	0110,	0111,	0112,	0113,	0114,	0115,	0116,	0117,
	0120,	0121,	0122,	0123,	0124,	0125,	0126,	0127,
	0130,	0131,	0132,	0133,	0134,	0135,	0136,	0137,
	0140,	0101,	0102,	0103,	0104,	0105,	0106,	0107,
	0110,	0111,	0112,	0113,	0114,	0115,	0116,	0117,
	0120,	0121,	0122,	0123,	0124,	0125,	0126,	0127,
	0130,	0131,	0132,	0173,	0174,	0175,	0176,	0177,
	0200,	0201,	0202,	0203,	0204,	0205,	0206,	0207,
	0210,	0211,	0212,	0213,	0214,	0215,	0216,	0217,
	0220,	0221,	0222,	0223,	0224,	0225,	0226,	0227,
	0230,	0231,	0232,	0233,	0234,	0235,	0236,	0237,
	0240,	0241,	0242,	0243,	0244,	0245,	0246,	0247,
	0250,	0251,	0252,	0253,	0254,	0255,	0256,	0257,
	0260,	0261,	0262,	0263,	0264,	0265,	0266,	0267,
	0270,	0271,	0272,	0273,	0274,	0275,	0276,	0277,
	0300,	0301,	0302,	0303,	0304,	0305,	0306,	0307,
	0310,	0311,	0312,	0313,	0314,	0315,	0316,	0317,
	0320,	0321,	0322,	0323,	0324,	0325,	0326,	0327,
	0330,	0331,	0332,	0333,	0334,	0335,	0336,	0337,
	0300,	0301,	0302,	0303,	0304,	0305,	0306,	0307,
	0310,	0311,	0312,	0313,	0314,	0315,	0316,	0317,
	0320,	0321,	0322,	0323,	0324,	0325,	0326,	0367,
	0330,	0331,	0332,	0333,	0334,	0335,	0336,	0377,
};
#endif /* NLS */
