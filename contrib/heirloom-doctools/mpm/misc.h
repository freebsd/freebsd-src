/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 source code published at the 9fans list by Rob Pike,
 * <http://lists.cse.psu.edu/archives/9fans/2002-February/015773.html>
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)misc.h	1.3 (gritter) 10/30/05	*/
#include	<stdio.h>
#include	<stdlib.h>
#include	<math.h>
#include	<ctype.h>
#include	<string.h>
#include	"global.h"

extern char	*progname;
extern int	linenum;
extern int	wantwarn;

extern void	FATAL(const char *, ...);
extern void	WARNING(const char *, ...);

#define	eq(s,t)	(strcmp(s,t) == 0)

extern int	dbg;

extern int	pn, userpn;		// actual and user-defined page numbers
extern int	pagetop, pagebot;	// printing margins
extern int	physbot;		// physical bottom of the page
