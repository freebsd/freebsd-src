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


/*	from OpenSolaris "tw.h	1.6	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)tw.h	1.6 (gritter) 4/25/06
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

#define	nextfp()	0

/* typewriter driving table structure */

#define	NROFFCHARS	NCHARS	/* ought to be dynamic */
#define	_SPECCHAR_ST	256

extern struct t {
	int	bset;		/* these bits have to be on */
	int	breset;		/* these bits have to be off */
	int	Hor;		/* #units in minimum horiz motion */
	int	Vert;		/* #units in minimum vert motion */
	int	Newline;	/* #units in single line space */
	int	Char;		/* #units in character width */
	int	Em;		/* ditto */
	int	Halfline;	/* half line units */
	int	Adj;		/* minimum units for horizontal adjustment */
	char	*twinit;	/* initialize terminal */
	char	*twrest;	/* reinitialize terminal */
	char	*twnl;		/* terminal sequence for newline */
	char	*hlr;		/* half-line reverse */
	char	*hlf;		/* half-line forward */
	char	*flr;		/* full-line reverse */
	char	*bdon;		/* turn bold mode on */
	char	*bdoff;		/* turn bold mode off */
	char	*iton;		/* turn italic mode on */
	char	*itoff;		/* turn italic mode off */
	char	*ploton;	/* turn plot mode on */
	char	*plotoff;	/* turn plot mode off */
	char	*up;		/* sequence to move up in plot mode */
	char	*down;		/* ditto */
	char	*right;		/* ditto */
	char	*left;		/* ditto */

	char	**codetab;
	char	*width;
} t;
