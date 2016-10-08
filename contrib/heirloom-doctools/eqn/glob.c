/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
     
/*
 * Copyright (c) 1983, 1984 1985, 1986, 1987, 1988, Sun Microsystems, Inc.
 * All Rights Reserved.
 */
  
/*	from OpenSolaris "glob.c	1.4	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)glob.c	1.8 (gritter) 10/19/06
 */

#include "e.h"

int	dbg;	/* debugging print if non-zero */
int	lp[512];	/* stack for things like piles and matrices */
int	ct;	/* pointer to lp */
int	used[100];	/* available registers */
float	ps;	/* default init point size */
/*int	resolution = 72;	 * was: resolution of ditroff */
float	deltaps	= 3;	/* default change in ps */
float	gsize	= 10;	/* default initial point size */
int	gfont	= ITAL;	/* italic */
int	ft;	/* default font */
FILE	*curfile;	/* current input file */
int	ifile;
int	linect;	/* line number in file */
int	eqline;	/* line where eqn started */
int	svargc;
char	**svargv;
#ifndef	NEQN
float	eht[100];
float	ebase[100];
#else	/* NEQN */
int	eht[100];
int	ebase[100];
#endif	/* NEQN */
int	lfont[100];
int	rfont[100];
int	eqnreg;	/* register where final string appears */
int	eqnht;	/* inal height of equation */
int	lefteq	= '\0';	/* left in-line delimiter */
int	righteq	= '\0';	/* right in-line delimiter */
int	lastchar;	/* last character read by lex */
int	markline	= 0;	/* 1 if this EQ/EN contains mark or lineup */
char	*progname;
