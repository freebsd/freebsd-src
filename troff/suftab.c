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
 * Copyright 1989 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "suftab.c	1.7	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)suftab.c	1.4 (gritter) 8/16/05
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

/*
 * Suffix table
 */

static	const unsigned char sufa[] = {
	02,0200+'t',	/* -TA */
	02,0200+'s',	/* -SA */
	03,0200+'t','r',	/* -TRA */
	03,0200+'d','r',	/* -DRA */
	03,0200+'b','r',	/* -BRA */
	02,0200+'p',	/* -PA */
	02,0200+'n',	/* -NA */
	02,0200+'m',	/* -MA */
	03,0200+'p','l',	/* -PLA */
	02,0200+'l',	/* -LA */
	02,0200+'k',	/* -KA */
	03,0200+'t','h',	/* -THA */
	03,0200+'s','h',	/* -SHA */
	02,0200+'g',	/* -GA */
	02,0200+'d',	/* -DA */
	02,0200+'c',	/* -CA */
	02,0200+'b',	/* -BA */
	00
};

static	const unsigned char sufc[] = {
	04,'e','t',0200+'i',	/* ET-IC */
	07,'a','l',0200+'i','s',0200+'t','i',	/* AL-IS-TIC */
	04,'s',0200+'t','i',	/* S-TIC */
	04,'p',0200+'t','i',	/* P-TIC */
	05,0200+'l','y','t',0200+'i',	/* -LYT-IC */
	04,'o','t',0200+'i',	/* OT-IC */
	05,'a','n',0200+'t','i',	/* AN-TIC */
	04,'n',0200+'t','i',	/* N-TIC */
	04,'c',0200+'t','i',	/* C-TIC */
	04,'a','t',0200+'i',	/* AT-IC */
	04,'h',0200+'n','i',	/* H-NIC */
	03,'n',0200+'i',	/* N-IC */
	03,'m',0200+'i',	/* M-IC */
	04,'l',0200+'l','i',	/* L-LIC */
	04,'b',0200+'l','i',	/* B-LIC */
	04,0200+'c','l','i',	/* -CLIC */
	03,'l',0200+'i',	/* L-IC */
	03,'h',0200+'i',	/* H-IC */
	03,'f',0200+'i',	/* F-IC */
	03,'d',0200+'i',	/* D-IC */
	03,0200+'b','i',	/* -BIC */
	03,'a',0200+'i',	/* A-IC */
	03,0200+'m','a',	/* -MAC */
	03,'i',0200+'a',	/* I-AC */
	00
};

static	const unsigned char sufd[] = {
	04,0200+'w','o','r',	/* -WORD */
	04,0200+'l','o','r',	/* -LORD */
	04,0200+'f','o','r',	/* -FORD */
	04,0200+'y','a','r',	/* -YARD */
	04,0200+'w','a','r',	/* -WARD */
	05,0200+'g','u','a','r',	/* -GUARD */
	04,0200+'t','a','r',	/* -TARD */
	05,0200+'b','o','a','r',	/* -BOARD */
	04,0200+'n','a','r',	/* -NARD */
	05,0200+'l','i','a','r',	/* -LIARD */
	04,0200+'i','a','r',	/* -IARD */
	04,0200+'g','a','r',	/* -GARD */
	04,0200+'b','a','r',	/* -BARD */
	03,0200+'r','o',	/* -ROD */
	04,0200+'w','o','o',	/* -WOOD */
	04,0200+'h','o','o',	/* -HOOD */
	04,0200+'m','o','n',	/* -MOND */
	04,0200+'t','e','n',	/* -TEND */
	05,0200+'s','t','a','n',	/* -STAND */
	04,0200+'l','a','n',	/* -LAND */
	04,0200+'h','a','n',	/* -HAND */
	04,0200+'h','o','l',	/* -HOLD */
	04,0200+'f','o','l',	/* -FOLD */
	05,0200+'f','i','e','l',	/* -FIELD */
	03,0200+'v','i',	/* -VID */
	03,0200+'c','i',	/* -CID */
	04,0200+'s','a','i',	/* -SAID */
	04,0200+'m','a','i',	/* -MAID */
	04,'t',0200+'t','e',	/* T-TED */
	03,'t',0200+'e',	/* T-ED */
	04,0200+'d','r','e',	/* -DRED */
	04,0200+'c','r','e',	/* -CRED */
	04,0200+'b','r','e',	/* -BRED */
	05,'v',0200+'e','l','e',	/* V-ELED */
	0100+04,'a','l',0200+'e',	/* AL/ED */
	0140+03,0200+'e','e',	/* /EED */
	040+05,'e','d',0200+'d','e',	/* ED-DED */
	04,'d',0200+'d','e',	/* D-DED */
	040+04,'e','d',0200+'e',	/* ED-ED */
	03,'d',0200+'e',	/* D-ED */
	05,0200+'d','u','c','e',	/* -DUCED */
	0300+02,'e',	/* E/D */
	05,0200+'s','t','e','a',	/* -STEAD */
	04,0200+'h','e','a',	/* -HEAD */
	00
};

static	const unsigned char sufe[] = {
	05,'a','r',0200+'i','z',	/* AR-IZE */
	05,'a','n',0200+'i','z',	/* AN-IZE */
	05,'a','l',0200+'i','z',	/* AL-IZE */
	06,0200+'a','r','d',0200+'i','z',	/* -ARD-IZE */
	05,0200+'s','e','l','v',	/* -SELVE */
	05,0200+'k','n','i','v',	/* -KNIVE */
	05,0200+'l','i','e','v',	/* -LIEVE */
	0100+03,0200+'q','u',	/* /QUE */
	07,'o','n',0200+'t','i','n',0200+'u',	/* ON-TIN-UE */
	03,0200+'n','u',	/* -NUE */
	03,0200+'d','u',	/* -DUE */
	0300+02,'u',	/* U/E */
	0300+05,'q','u','a','t',	/*  QUAT/E */
	04,'u',0200+'a','t',	/* U-ATE */
	05,0200+'s','t','a','t',	/* -STATE */
	04,0200+'t','a','t',	/* -TATE */
	06,0200+'t','o','r',0200+'a','t',	/* -TOR-ATE */
	05,'e','n',0200+'a','t',	/* EN-ATE */
	04,0200+'m','a','t',	/* -MATE */
	05,0200+'h','o','u','s',	/* -HOUSE */
	05,0200+'c','l','o','s',	/* -CLOSE */
	04,'i',0200+'o','s',	/* I-OSE */
	04,0200+'w','i','s',	/* -WISE */
	05,'a','s',0200+'u','r',	/* AS-URE */
	040+04,0200+'s','u','r',	/* -SURE */
	06,0200+'f','i','g',0200+'u','r',	/* -FIG-URE */
	040+03,0200+'t','r',	/* -TRE */
	05,0200+'s','t','o','r',	/* -STORE */
	04,0200+'f','o','r',	/* -FORE */
	05,0200+'w','h','e','r',	/* -WHERE */
	06,0200+'s','p','h','e','r',	/* -SPHERE */
	03,0200+'d','r',	/* -DRE */
	03,0200+'c','r',	/* -CRE */
	03,0200+'b','r',	/* -BRE */
	05,0200+'s','c','o','p',	/* -SCOPE */
	04,'y',0200+'o','n',	/* Y-ONE */
	05,0200+'s','t','o','n',	/* -STONE */
	05,0200+'p','h','o','n',	/* -PHONE */
	04,0200+'g','o','n',	/* -GONE */
	04,'e',0200+'o','n',	/* E-ONE */
	040+04,0200+'e','n','n',	/* -ENNE */
	040+05,'a',0200+'r','i','n',	/* A-RINE */
	05,0200+'c','l','i','n',	/* -CLINE */
	04,0200+'l','i','n',	/* -LINE */
	007,00200+'r','o','u',00200+'t','i','n',	/*-ROU-TINE */
	04,0200+'s','o','m',	/* -SOME */
	04,0200+'c','o','m',	/* -COME */
	04,0200+'t','i','m',	/* -TIME */
	03,0200+'z','l',	/* -ZLE */
	03,0200+'t','l',	/* -TLE */
	03,0200+'s','l',	/* -SLE */
	03,0200+'p','l',	/* -PLE */
	05,0200+'v','i','l','l',	/* -VILLE */
	04,'c','k',0200+'l',	/* CK-LE */
	03,0200+'k','l',	/* -KLE */
	03,0200+'g','l',	/* -GLE */
	03,0200+'f','l',	/* -FLE */
	03,0200+'d','l',	/* -DLE */
	03,0200+'c','l',	/* -CLE */
	05,0200+'p','a',0200+'b','l',	/* -PA-BLE */
	05,'f','a',0200+'b','l',	/* FA-BLE */
	05,0200+'c','a',0200+'b','l',	/* -CA-BLE */
	06,0200+'s','t','a','b','l',	/* -STABLE */
	04,0200+'a','b','l',	/* -ABLE */
	03,0200+'b','l',	/* -BLE */
	04,0200+'d','a','l',	/* -DALE */
	04,0200+'m','a','l',	/* -MALE */
	04,0200+'s','a','l',	/* -SALE */
	04,0200+'l','i','k',	/* -LIKE */
	0340+05,'g',0200+'u','a','g',	/* -G/UAGE */
	05,0200+'r','i','a','g',	/* -RIAGE */
	05,'e','r',0200+'a','g',	/* ER-AGE */
	04,'m',0200+'a','g',	/* M-AGE */
	04,'k',0200+'a','g',	/* K-AGE */
	04,'d',0200+'a','g',	/* D-AGE */
	04,0200+'w','i','f',	/* -WIFE */
	05,0200+'k','n','i','f',	/* -KNYFE */
	03,0200+'s','e',	/* -SEE */
	04,0200+'f','r','e',	/* -FREE */
	0340+02,'e',	/* EE */
	04,0200+'w','i','d',	/* -WIDE */
	04,0200+'t','i','d',	/* -TIDE */
	04,0200+'s','i','d',	/* -SIDE */
	06,0200+'q','u','e','n','c',	/* -QUENCE */
	07,0200+'f','l','u',0200+'e','n','c',	/* -FLU-ENCE */
	040+06,'e','s',0200+'e','n','c',	/* ES-ENCE */
	06,'e','r',0200+'e','n','c',	/* ER-ENCE */
	05,'i',0200+'e','n','c',	/* I-ENCE */
	040+05,0200+'s','a','n','c',	/* -SANCE */
	06,'e','r',0200+'a','n','c',	/* ER-ANCE */
	06,'a','r',0200+'a','n','c',	/* AR-ANCE */
	05,0200+'n','a','n','c',	/* -NANCE */
	07,0200+'b','a','l',0200+'a','n','c',	/* -BAL-ANCE */
	05,'i',0200+'a','n','c',	/* I-ANCE */
	07,0200+'j','u','s',0200+'t','i','c',	/* -JUS-TICE */
	05,0200+'s','t','i','c',	/* -STICE */
	05,0200+'p','i','e','c',	/* -PIECE */
	05,0200+'p','l','a','c',	/* -PLACE */
	0340+01,	/* /E */
	00
};

static	const unsigned char suff[] = {
	03,0200+'o','f',	/* -OFF */
	05,0200+'p','r','o','o',	/* -PROOF */
	04,0200+'s','e','l',	/* -SELF */
	03,0200+'r','i',	/* -RIF */
	040+04,0200+'l','i','e',	/* -LIEF */
	00
};

static	const unsigned char sufg[] = {
	03,0200+'l','o',	/* -LOG */
	04,0200+'l','o','n',	/* -LONG */
	05,'t',0200+'t','i','n',	/* T-TING */
	06,0200+'s','t','r','i','n',	/*  -STRING */
	05,'r',0200+'r','i','n',	/* R-RING */
	05,'p',0200+'p','i','n',	/* P-PING */
	05,'n',0200+'n','i','n',	/* N-NING */
	05,'m',0200+'m','i','n',	/* M-MING */
	05,'l',0200+'l','i','n',	/*  L-LING */
	05,0200+'z','l','i','n',	/* -ZLING */
	05,0200+'t','l','i','n',	/* -TLING */
	040+05,'s',0200+'l','i','n',	/* S-LING */
	05,'r',0200+'l','i','n',	/* R-LING */
	05,0200+'p','l','i','n',	/* -PLING */
	06,'n',0200+'k','l','i','n',	/* N-KLING */
	05,'k',0200+'l','i','n',	/* K-LING */
	05,0200+'g','l','i','n',	/* -GLING */
	05,0200+'f','l','i','n',	/* -FLING */
	05,0200+'d','l','i','n',	/* -DLING */
	05,0200+'c','l','i','n',	/* -CLING */
	05,0200+'b','l','i','n',	/* -BLING */
	06,'y',0200+'t','h','i','n',	/* Y-THING */
	07,'e','e','t','h',0200+'i','n',	/* EETH-ING */
	06,'e',0200+'t','h','i','n',	/* E-THING */
	05,'g',0200+'g','i','n',	/* G-GING */
	05,'d',0200+'d','i','n',	/* D-DING */
	05,'b',0200+'b','i','n',	/* B-BING */
	03,0200+'i','n',	/* -ING */
	00
};

static	const unsigned char sufh[] = {
	05,0200+'m','o','u','t',	/* -MOUTH */
	05,0200+'w','o','r','t',	/* -WORTH */
	04,0200+'w','i','t',	/* -WITH */
	05,'t',0200+'t','i','s',	/* T-TISH */
	05,'e',0200+'t','i','s',	/* E-TISH */
	05,'p',0200+'p','i','s',	/* P-PISH */
	05,'r',0200+'n','i','s',	/* R-NISH */
	05,'n',0200+'n','i','s',	/* N-NISH */
	05,0200+'p','l','i','s',	/* -PLISH */
	05,0200+'g','u','i','s',	/*  -GUISH */
	05,0200+'g','l','i','s',	/*  -GLISH */
	05,'b',0200+'l','i','s',	/*  B-LISH */
	05,'g',0200+'g','i','s',	/* G-GISH */
	05,'d',0200+'d','i','s',	/* D-DISH */
	03,0200+'i','s',	/* -ISH */
	05,0200+'g','r','a','p',	/* -GRAPH */
	07,0200+'b','o','r',0200+'o','u','g',	/* -BOR-OUGH */
	05,0200+'b','u','r','g',	/* -BURGH */
	04,0200+'v','i','c',	/* -VICH */
	03,0200+'n','a',	/* -NAH */
	03,0200+'l','a',	/* -LAH */
	04,0200+'m','i',0200+'a',	/* -MI-AH */
	00
};

static	const unsigned char sufi[] = {
	03,0200+'t','r',	/* -TRI */
	03,0200+'c','h',	/* -CHI */
	0200+03,'i','f',	/* IF-I */
	0200+03,'e','d',	/* ED-I */
	05,0200+'a','s','c','i',	/* -ASCII */
	04,0200+'s','e','m',	/* -SEMI */
	00
};

static	const unsigned char sufk[] = {
	04,0200+'w','o','r',	/* -WORK */
	04,0200+'m','a','r',	/* -MARK */
	04,0200+'b','o','o',	/* -BOOK */
	04,0200+'w','a','l',	/* -WALK */
	05,0200+'c','r','a','c',	/* -CRACK */
	04,0200+'b','a','c',	/* -BACK */
	00
};

static	const unsigned char sufl[] = {
	03,0200+'f','u',	/* -FUL */
	05,'s',0200+'w','e','l',	/* S-WELL */
	04,0200+'t','e','l',	/* -TELL */
	05,0200+'s','h','e','l',	/* -SHELL */
	05,0200+'s','t','a','l',	/* -STALL */
	04,'s',0200+'t','a',	/* S-TAL */
	04,0200+'b','a','l',	/* -BALL */
	04,0200+'c','a','l',	/* -CALL */
	03,'v',0200+'e',	/* V-EL */
	03,'u',0200+'e',	/* U-EL */
	03,'k',0200+'e',	/* K-EL */
	04,'t','h',0200+'e',	/* TH-EL */
	05,'t','c','h',0200+'e',	/* TCH-EL */
	03,'a',0200+'e',	/* A-EL */
	0140+04,0200+'q','u','a',	/* /QUAL */
	040+03,'u',0200+'a',	/* U-AL */
	03,0200+'t','a',	/* -TAL */
	04,'u','r',0200+'a',	/* UR-AL */
	040+05,'g',0200+'o',0200+'n','a',	/* G-O-NAL */
	04,'o','n',0200+'a',	/* ON-AL */
	03,0200+'n','a',	/* -NAL */
	04,0200+'t','i','a',	/* -TIAL */
	04,0200+'s','i','a',	/* -SIAL */
	040+05,0200+'t','r','i',0200+'a',	/* -TRI-AL */
	04,'r','i',0200+'a',	/* RI-AL */
	04,0200+'n','i',0200+'a',	/* -NI-AL */
	04,0200+'d','i',0200+'a',	/* -DI-AL */
	04,0200+'c','i','a',	/* -CIAL */
	03,0200+'g','a',	/* -GAL */
	04,0200+'m','e','a',	/* -MEAL */
/*	040+04,0200+'r','e',0200+'a', */	/* -RE-AL */
	040+04,0200+'r','e','a',	/* -REAL */
	06,'c',0200+'t','i',0200+'c','a',	/* C-TI-CAL */
	05,0200+'s','i',0200+'c','a',	/* -SI-CAL */
	04,0200+'i',0200+'c','a',	/* -I-CAL */
	03,0200+'c','a',	/* -CAL */
	03,0200+'b','a',	/* -BAL */
	06,0200+'n','o',0200+'m','i',0200+'a',	/* -NO-MI-AL */
	00
};

static	const unsigned char sufm[] = {
	03,0200+'n','u',	/* -NUM */
	05,'o',0200+'r','i',0200+'u',	/* O-RI-UM */
	040+03,'i',0200+'u',	/* I-UM */
	040+03,'e',0200+'u',	/* E-UM */
	05,'i','v',0200+'i','s',	/* IV-ISM */
	04,0200+'t','i','s',	/* -TISM */
	05,'i',0200+'m','i','s',	/* I-MISM */
	05,'a','l',0200+'i','s',	/* AL-ISM */
	040+04,'e',0200+'i','s',	/* E-ISM */
	040+04,'a',0200+'i','s',	/* A-ISM */
	04,0200+'r','o','o',	/* -ROOM */
	03,0200+'d','o',	/* -DOM */
	03,0200+'h','a',	/* -HAM */
	06,0200+'a',0200+'r','i','t','h',	/* -A-RITHM */
	05,0200+'r','i','t','h',	/* -RITHM */
	00
};

static	const unsigned char sufn[] = {
	05,0200+'k','n','o','w', /* -KNOWN */
	04,0200+'t','o','w',	/* -TOWN */
	04,0200+'d','o','w',	/* -DOWN */
	04,0200+'t','u','r',	/* -TURN */
	05,0200+'s','p','o','o',	/* -SPOON */
	04,0200+'n','o','o',	/* -NOON */
	04,0200+'m','o','o',	/* -MOON */
	011,'a','l',0200+'i',0200+'z','a',0200+'t','i','o',	/* AL-I-ZA-TION */
	07,0200+'i',0200+'z','a',0200+'t','i','o',	/* -I-ZA-TION */
	07,'l',0200+'i',0200+'a',0200+'t','i','o',	/* L-I-A-TION */
	04,0200+'t','i','o',	/* -TION */
	040+05,'s',0200+'s','i','o',	/* S-SION */
	04,0200+'s','i','o',	/* -SION */
	04,'n',0200+'i','o',	/* N-ION */
	04,0200+'g','i','o',	/* -GION */
	04,0200+'c','i','o',	/* -CION */
	03,0200+'c','o',	/* -CON */
	05,0200+'c','o','l','o',	/* -COLON */
	03,0200+'t','o',	/* -TON */
	04,'i','s',0200+'o',		/* IS-ON */
	03,0200+'s','o',	/* -SON */
	03,0200+'r','i',	/* -RIN */
	03,0200+'p','i',	/* -PIN */
	03,0200+'n','i',	/* -NIN */
	03,0200+'m','i',	/* -MIN */
	03,0200+'l','i',	/* -LIN */
	03,0200+'k','i',	/* -KIN */
	05,0200+'s','t','e','i',	/* -STEIN */
	04,0200+'t','a','i',	/* -TAIN */
	05,'g','h','t',0200+'e',	/* GHT-EN */
	05,0200+'w','o','m',0200+'e',	/* -WOM-EN */
	03,0200+'m','e',	/* -MEN */
	04,'o',0200+'k','e',	/* O-KEN */
	03,'k',0200+'e',	/* K-EN */
	04,0200+'t','e','e',	/* -TEEN */
	04,0200+'s','e','e',	/* -SEEN */
	040+03,0200+'s','a',	/* -SAN */
	05,0200+'w','o','m',0200+'a',	/* -WOM-AN */
	03,0200+'m','a',	/* -MAN */
	04,0200+'t','i','a',	/* -TIAN */
	04,0200+'s','i','a',	/* -SIAN */
	040+04,'e',0200+'i','a',	/* E-IAN */
	04,0200+'c','i','a',	/* -CIAN */
	0300+03,'i','a',	/* IA/N */
	05,0200+'c','l','e','a',	/* -CLEAN */
	04,0200+'m','e','a',	/* -MEAN */
	040+03,'e',0200+'a',	/* E-AN */
	00
};

static	const unsigned char sufo[] = {
	05,0200+'m','a','c',0200+'r',	/* -MAC-RO */
	00
};

static	const unsigned char sufp[] = {
	05,0200+'g','r','o','u',	/* -GROUP */
	02,0200+'u',	/* -UP */
	04,0200+'s','h','i',	/* -SHIP */
	04,0200+'k','e','e',	/* -KEEP */
	00
};

static	const unsigned char sufr[] = {
	04,0200+'z','a','r',	/* -ZARR */
	0300+02,'r',	/* R/R */
	03,0200+'t','o',	/* -TOR */
	040+03,0200+'s','o',	/* -SOR */
	040+04,0200+'r','i',0200+'o',	/* -RI-OR */
	04,'i','z',0200+'e',	/* IZ-ER */
	05,0200+'c','o','v',0200+'e',	/* -COV-ER */
	04,0200+'o','v','e',	/* -OVER */
	04,0200+'e','v',0200+'e',	/* -EV-ER */
	8,0200+'c','o','m',0200+'p','u','t',0200+'e',	/* -COM-PUT-ER */
	040+05,'u','s',0200+'t','e',	/* US-TER */
	05,'o','s','t',0200+'e',	/* OST-ER */
	040+05,0200+'a','c',0200+'t','e',	/* -AC-TER */
	06,0200+'w','r','i','t',0200+'e',	/* -WRIT-ER */
	040+05,'i','s',0200+'t','e',	/* IS-TER */
	040+05,'e','s',0200+'t','e',	/* ES-TER */
	040+05,'a','s',0200+'t','e',	/* AS-TER */
	04,0200+'s','t','e',	/* -STER */
	05,'a','r',0200+'t','e',	/* AR-TER */
	04,'r','t',0200+'e',	/* RT-ER */
	040+05,'m',0200+'e',0200+'t','e',	/* M-E-TER */
	05,0200+'w','a',0200+'t','e',	/* -WA-TER */
	03,'r',0200+'e',	/* R-ER */
	04,'o','p',0200+'e',	/* OP-ER */
	05,0200+'p','a',0200+'p','e',	/* -PA-PER */
	04,'w','n',0200+'e',	/* WN-ER */
	040+04,'s',0200+'n','e',	/* S-NER */
	04,'o','n',0200+'e',	/* ON-ER */
	04,'r','m',0200+'e',	/* RM-ER */
	03,0200+'m','e',	/* -MER */
	04,'l','l',0200+'e',	/* LL-ER */
	05,'d',0200+'d','l','e',	/* D-DLER */
	04,0200+'b','l','e',	/* -BLER */
	03,'k',0200+'e',	/* K-ER */
	05,'n',0200+'t','h','e',	/* N-THER */
	06,0200+'f','a',0200+'t','h','e',	/* -FA-THER */
	06,'e','i',0200+'t','h','e',	/* EI-THER */
	04,'t','h',0200+'e',	/* TH-ER */
	04,'s','h',0200+'e',	/* SH-ER */
	04,0200+'p','h','e',	/* -PHER */
	04,'c','h',0200+'e',	/* CH-ER */
	04,'d','g',0200+'e',	/* DG-ER */
	04,'r','d',0200+'e',	/* RD-ER */
	06,'o','u','n','d',0200+'e',	/* OUND-ER */
	04,'l','d',0200+'e',	/* LD-ER */
	04,'i','d',0200+'e',	/* ID-ER */
	05,0200+'d','u','c',0200+'e',	/* -DUC-ER */
	04,'n','c',0200+'e',	/* NC-ER */
	0100+02, 0200+'e',	/*  /ER */
	03,0200+'s','a',	/* -SAR */
	040+06,'a','c',0200+'u',0200+'l','a',	/* AC-U-LAR */
	040+06,'e','c',0200+'u',0200+'l','a',	/* EC-U-LAR */
	040+06,'i','c',0200+'u',0200+'l','a',	/* IC-U-LAR */
	040+06,'e','g',0200+'u',0200+'l','a',	/* EG-U-LAR */
	00
};

static	const unsigned char sufs[] = {
	040+04,'u',0200+'o','u',	/* U-OUS */
	05,0200+'t','i','o','u',	/* -TIOUS */
	05,0200+'g','i','o','u',	/* -GIOUS */
	05,0200+'c','i','o','u',	/* -CIOUS */
	040+04,'i',0200+'o','u',	/* I-OUS */
	05,0200+'g','e','o','u',	/* -GEOUS */
	05,0200+'c','e','o','u',	/* -CEOUS */
	04,'e',0200+'o','u',	/* E-OUS */
	0140+02,0200+'u',	/* /US */
	04,0200+'n','e','s',	/* -NESS */
	04,0200+'l','e','s',	/* -LESS */
	0140+02,0200+'s',	/* /SS */
	040+05,'p',0200+'o',0200+'l','i',	/* P-O-LIS */
	0140+02,0200+'i',	/* /IS */
	0100+03,0200+'x','e',	/* X/ES */
	0100+03,0200+'s','e',	/* S/ES */
	0100+04,'s','h',0200+'e',	/* SH/ES */
	0100+04,'c','h',0200+'e',	/* CH/ES */
	0300+01,	/* /S */
	00
};

static	const unsigned char suft[] = {
	06,'i','o','n',0200+'i','s',	/* ION-IST */
	05,'i','n',0200+'i','s',	/* IN-IST */
	05,'a','l',0200+'i','s',	/* AL-IST */
	06,'l',0200+'o',0200+'g','i','s',	/* L-O-GIST */
	05,'h','t',0200+'e','s',	/* HT-EST */
	04,'i',0200+'e','s',	/* I-EST */
	05,'g',0200+'g','e','s',	/* G-GEST */
	04,'g',0200+'e','s',	/* G-EST */
	05,'d',0200+'d','e','s',	/* D-DEST */
	04,'d',0200+'e','s',	/* D-EST */
	04,0200+'c','a','s',	/* -CAST */
	05,0200+'h','e','a','r',	/* -HEART */
	04,0200+'f','o','o',	/* -FOOT */
	03,'i',0200+'o',	/* I-OT */
	05,0200+'f','r','o','n',	/* -FRONT */
	05,0200+'p','r','i','n',	/* -PRINT */
	04,0200+'m','e','n',	/* -MENT */
	05,0200+'c','i','e','n',	/* -CIENT */
	04,'i',0200+'a','n',	/* I-ANT */
	06,0200+'w','r','i','g','h',	/* -WRIGHT */
	06,0200+'b','r','i','g','h',	/* -BRIGHT */
	06,0200+'f','l','i','g','h',	/* -FLIGHT */
	06,0200+'w','e','i','g','h',	/* -WEIGHT */
	05,0200+'s','h','i','f',	/* -SHIFT */
	05,0200+'c','r','a','f',	/* -CRAFT */
	040+04,'d','g',0200+'e',	/* DG-ET */
	04,0200+'g','o','a',	/* -GOAT */
	04,0200+'c','o','a',	/* -COAT */
	04,0200+'b','o','a',	/* -BOAT */
	04,0200+'w','h','a',	/* -WHAT */
	04,0200+'c','u','i',	/* -CUIT */
	00
};

static	const unsigned char sufy[] = {
	040+04,'e','s',0200+'t',	/* ES-TY */
	040+05,'q','u','i',0200+'t',	/* QUI-TY */
	04,0200+'t','i',0200+'t',	/* -TI-TY */
	040+05,'o','s',0200+'i',0200+'t',	/* OS-I-TY */
	04,0200+'s','i',0200+'t',	/* -SI-TY */
	05,'i','n',0200+'i',0200+'t',	/* IN-I-TY */
	04,'n','i',0200+'t',	/* NI-TY */
	040+010,'f','a',0200+'b','i','l',0200+'i',0200+'t',	/* FA-BIL-I-TY */
	010,0200+'c','a',0200+'b','i','l',0200+'i',0200+'t',	/* -CA-BIL-I-TY */
	010,0200+'p','a',0200+'b','i','l',0200+'i',0200+'t',	/* -PA-BIL-I-TY */
	06,0200+'b','i','l',0200+'i',0200+'t',	/* -BIL-I-TY */
	03,'i',0200+'t',	/* I-TY */
	04,0200+'b','u','r',	/* -BUR-Y */
	04,0200+'t','o',0200+'r',	/* -TO-RY */
	05,0200+'q','u','a','r',	/* -QUAR-Y */
	040+04,'u',0200+'a','r',	/* U-ARY */
	07,0200+'m','e','n',0200+'t','a',0200+'r',	/* -MEN-TA-RY */
	06,'i','o','n',0200+'a','r',	/* ION-ARY */
	04,'i',0200+'a','r',	/* I-ARY */
	04,'n',0200+'o',0200+'m',	/* N-O-MY */
	03,0200+'p','l',	/* -PLY */
	04,'g',0200+'g','l',	/* G-GLY */
	05,0200+'p','a',0200+'b','l',	/* -PA-BLY */
	05,'f','a',0200+'b','l',	/* FA-BLY */
	05,0200+'c','a',0200+'b','l',	/* -CA-BLY */
	04,0200+'a','b','l',	/* -ABLY */
	03,0200+'b','l',	/* -BLY */
	02,0200+'l',	/* -LY */
	03,0200+'s','k',	/* -SKY */
	040+06,'g',0200+'r','a',0200+'p','h',	/* G-RA-PHY */
	04,'l',0200+'o',0200+'g',	/* L-O-GY */
	02,0200+'f',	/* -FY */
	03,0200+'n','e',	/* -NEY */
	03,0200+'l','e',	/* -LEY */
	04,'c','k',0200+'e',	/* CK-EY */
	03,0200+'k','e',	/* -KEY */
	04,0200+'b','o','d',	/* -BODY */
	05,0200+'s','t','u','d',	/* -STUDY */
	0340+04,'e','e','d',	/* EEDY */
	02,0200+'b',	/* -BY */
	03,0200+'w','a',	/* -WAY */
	03,0200+'d','a',	/* -DAY */
	00
};

const unsigned char	*suftab[] = {
	sufa,
	0,
	sufc,
	sufd,
	sufe,
	suff,
	sufg,
	sufh,
	sufi,
	0,
	sufk,
	sufl,
	sufm,
	sufn,
	sufo,
	sufp,
	0,
	sufr,
	sufs,
	suft,
	0,
	0,
	0,
	0,
	sufy,
	0,
};
