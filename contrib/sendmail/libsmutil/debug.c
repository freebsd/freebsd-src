/*
 * Copyright (c) 1999 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: debug.c,v 8.2 1999/07/26 04:04:09 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>

u_char	tTdvect[100];	/* trace vector */

#if _FFR_DPRINTF_
void
/*VARARGS1*/
#ifdef __STDC__
dprintf(const char *fmt, ...)
#else /* __STDC__ */
dprintf(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif /* __STDC__ */
{
	VA_LOCAL_DECL;

	(void) vfprintf(stdout, fmt, ap);
}

int
dflush()
{
	return fflush(stdout);
}
#endif /* _FFR_DPRINTF_ */
