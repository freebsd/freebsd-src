/* $FreeBSD: src/libexec/lukemftpd/nbsd2fbsd.h,v 1.3.4.1 2003/08/24 17:46:47 obrien Exp $ */

/* XXX: Depend on our system headers protecting against multiple includes. */
#include <paths.h>
#undef _PATH_FTPUSERS

#include <pwd.h>

#define _DIAGASSERT(x)

long long strsuftollx(const char *, const char *,
    long long, long long, char *, size_t);
