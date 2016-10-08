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
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

/*	from OpenSolaris "dev.h	1.6	05/06/08 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)dev.h	1.3 (gritter) 8/9/05
 */

/*
	dev.h: characteristics of a typesetter
*/

struct dev {
	unsigned short	filesize;	/* number of bytes in file, */
				/* excluding dev part */
	short	res;		/* basic resolution in goobies/inch */
	short	hor;		/* goobies horizontally */
	short	vert;
	short	unitwidth;	/* size at which widths are given, in effect */
	short	nfonts;		/* number of fonts physically available */
	short	nsizes;		/* number of sizes it has */
	short	sizescale;	/* scaling for fractional point sizes */
	short	paperwidth;	/* max line length in units */
	short	paperlength;	/* max paper length in units */
	short	nchtab;		/* number of funny names in chtab */
	short	lchname;	/* length of chname table */
	short	biggestfont;	/* #chars in largest ever font */
	short	spare2;		/* in case of expansion */
};

struct Font {		/* characteristics of a font */
	char	nwfont;		/* number of width entries for this font */
	char	specfont;	/* 1 == special font */
	char	ligfont;	/* 1 == ligatures exist on this font */
	char	spare1;		/* unused for now */
	char	namefont[10];	/* name of this font (e.g., "R" */
	char	intname[10];	/* internal name (=number) on device, in ascii */
};

/* ligatures, ORed into ligfont */

#define	LFF	01
#define	LFI	02
#define	LFL	04
#define	LFFI	010
#define	LFFL	020
