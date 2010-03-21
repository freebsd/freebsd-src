/*	$FreeBSD: src/contrib/ipfilter/lib/debug.c,v 1.4.12.1 2010/02/10 00:26:20 kensmith Exp $	*/

/*
 * Copyright (C) 2000-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: debug.c,v 1.6.4.1 2006/06/16 17:20:57 darrenr Exp $
 */

#if defined(__STDC__)
# include <stdarg.h>
#else
# include <varargs.h>
#endif
#include <stdio.h>

#include "ipt.h"
#include "opts.h"


#ifdef	__STDC__
void	debug(char *fmt, ...)
#else
void	debug(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
	va_list pvar;

	va_start(pvar, fmt);

	if (opts & OPT_DEBUG)
		vprintf(fmt, pvar);
	va_end(pvar);
}
