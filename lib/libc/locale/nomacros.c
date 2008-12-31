#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/locale/nomacros.c,v 1.5.32.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * Tell <ctype.h> to generate extern versions of all its inline
 * functions.  The extern versions get called if the system doesn't
 * support inlines or the user defines _DONT_USE_CTYPE_INLINE_
 * before including <ctype.h>.
 */
#define _EXTERNALIZE_CTYPE_INLINES_

#include <ctype.h>
