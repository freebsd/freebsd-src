
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

# include <stdarg.h>
#include <stdio.h>

#include "ipf.h"
#include "opts.h"

int	debuglevel = 0;


void
debug(int level, char *fmt, ...)
{
	va_list pvar;

	va_start(pvar, fmt);

	if ((debuglevel > 0) && (level <= debuglevel))
		vfprintf(stderr, fmt, pvar);
	va_end(pvar);
}


void
ipfkdebug(char *fmt, ...)
{
	va_list pvar;

	va_start(pvar, fmt);

	if (opts & OPT_DEBUG)
		debug(0x1fffffff, fmt, pvar);
	va_end(pvar);
}
