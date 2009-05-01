#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/locale/nomacros.c,v 1.5.34.1 2009/04/15 03:14:26 kensmith Exp $");

/*
 * Tell <ctype.h> to generate extern versions of all its inline
 * functions.  The extern versions get called if the system doesn't
 * support inlines or the user defines _DONT_USE_CTYPE_INLINE_
 * before including <ctype.h>.
 */
#define _EXTERNALIZE_CTYPE_INLINES_

#include <ctype.h>
