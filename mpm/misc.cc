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

/*	Sccsid @(#)misc.cc	1.3 (gritter) 10/30/05	*/
#include	"misc.h"
#include	<stdarg.h>

char	*progname;
int	wantwarn = 0;

int	dbg	= 0;
// dbg = 1 : dump slugs
// dbg = 2 : dump ranges
// dbg = 4 : report function entry
// dbg = 8 : follow queue progress
// dbg = 16: follow page fill progress

static void
msg(void)
{
	fprintf(stdout, "\n#MESSAGE TO USER:  ");
}

void
FATAL(const char *fmt, ...)
{
	char	buf[4096];
	va_list	ap;

	msg();
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	fputs(buf, stdout);
	fprintf(stderr, "%s: ", progname);
	fputs(buf, stderr);
	fflush(stdout);
	exit(1);
}

void
WARNING(const char *fmt, ...)
{
	char	buf[4096];
	va_list	ap;

	msg();
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	fputs(buf, stdout);
	if (wantwarn) {
		fprintf(stderr, "%s: ", progname);
		fputs(buf, stderr);
	}
	fflush(stdout);
}
