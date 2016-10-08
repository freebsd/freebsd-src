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


/*	from OpenSolaris "dev.h	1.5	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)dev.h	1.15 (gritter) 9/24/06
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
	dev.h: characteristics of a typesetter
*/

#ifndef	TROFF_DEV_H
#define	TROFF_DEV_H

#include "global.h"

struct dev {
	unsigned	filesize;	/* number of bytes in file, */
				/* excluding dev part */
	int	res;		/* basic resolution in goobies/inch */
	int	hor;		/* goobies horizontally */
	int	vert;
	int	unitwidth;	/* size at which widths are given, in effect */
	int	nfonts;		/* number of fonts physically available */
	int	nsizes;		/* number of sizes it has */
	int	sizescale;	/* scaling for fractional point sizes */
	int	anysize;	/* device can print any size */
	int	allpunct;	/* all fonts contain punctuation characters */
	int	afmfonts;	/* device uses AFM fonts by default */
	int	paperwidth;	/* max line length in units */
	int	paperlength;	/* max paper length in units */
	int	nchtab;		/* number of funny names in chtab */
	int	lchname;	/* length of chname table */
	int	biggestfont;	/* #chars in largest ever font */
	int	spare2;		/* in case of expansion */
	int	lc_ctype;	/* understands x X LC_CTYPE */
	int	encoding;	/* default output encoding */
};

struct Font {		/* characteristics of a font */
	char	nwfont;		/* number of width entries for this font */
	char	specfont;	/* 1 == special font */
	char	ligfont;	/* 1 == ligatures exist on this font */
	char	kernfont;	/* minimum kerning, 0 == no limit, -1 == off */
	char	namefont[10];	/* name of this font (e.g., "R" */
	char	intname[10];	/* internal name (=number) on device, in ascii */
	int	afmpos;		/* afmpos-1 = position in afmtab */
	int	spacewidth;	/* width of space character */
	int	cspacewidth;	/* custom space width */
};

/* ligatures, ORed into ligfont */

#define	LFF	01
#define	LFI	02
#define	LFL	04
#define	LFFI	010
#define	LFFL	020

extern	void		*readdesc(const char *);
extern	void		*readfont(const char *, struct dev *, int);

#endif	/* !TROFF_DEV_H */
