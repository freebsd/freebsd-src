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


/*	from OpenSolaris "ext.h	1.5	05/06/08 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)ext.h	1.7 (gritter) 3/27/07
 */

/*
 *
 * External varible declarations - many are defined in glob.c.
 *
 */


extern char	**argv;			/* global so everyone can use them */
extern int	argc;

extern int	x_stat;			/* program exit status */
extern int	debug;			/* debug flag */
extern int	ignore;			/* what we do with FATAL errors */

extern long	lineno;			/* line number */
extern long	position;		/* byte position */
extern int	printed;		/* pages printed so far */
extern char	*prog_name;		/* and program name - for errors */
extern char	*temp_file;		/* temporary file - for some programs */

extern int	LanguageLevel;		/* language level of generated PS */
