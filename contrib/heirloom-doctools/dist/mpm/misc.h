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

#ifdef	__GLIBC__
#ifdef	_IO_getc_unlocked
#undef	getc
#define	getc(f)		_IO_getc_unlocked(f)
#endif
#ifdef	_IO_putc_unlocked
#undef	putc
#undef	putchar
#define	putc(c, f)	_IO_putc_unlocked(c, f)
#define	putchar(c)	_IO_putc_unlocked(c, stdout)
#endif
#endif	/* __GLIBC__ */
  
extern char	*progname;
extern int	linenum;
extern int	wantwarn;

extern void	FATAL(const char *, ...);
extern void	WARNING(const char *, ...);

#define	eq(s,t)	(strcmp(s,t) == 0)

inline int	max(int x, int y)	{ return x > y ? x : y; }
inline int	min(int x, int y)	{ return x > y ? y : x; }
// already defined in stdlib.h:
//inline int	abs(int x)		{ return (x >= 0) ? x : -x; }

extern int	dbg;

extern int	pn, userpn;		// actual and user-defined page numbers
extern int	pagetop, pagebot;	// printing margins
extern int	physbot;		// physical bottom of the page
