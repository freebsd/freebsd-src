/* $FreeBSD: src/cddl/compat/opensolaris/include/libintl.h,v 1.2.2.2.4.1 2009/04/15 03:14:26 kensmith Exp $ */

#ifndef	_LIBINTL_H_
#define	_LIBINTL_H_

#include <sys/cdefs.h>
#include <stdio.h>

#define	textdomain(domain)	0
#define	gettext(...)		(__VA_ARGS__)
#define	dgettext(domain, ...)	(__VA_ARGS__)

#endif	/* !_SOLARIS_H_ */
