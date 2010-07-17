/* $FreeBSD: src/cddl/compat/opensolaris/include/libintl.h,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */

#ifndef	_LIBINTL_H_
#define	_LIBINTL_H_

#include <sys/cdefs.h>
#include <stdio.h>

#define	textdomain(domain)	0
#define	gettext(...)		(__VA_ARGS__)
#define	dgettext(domain, ...)	(__VA_ARGS__)

#endif	/* !_SOLARIS_H_ */
