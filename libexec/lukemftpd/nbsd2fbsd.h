/* $FreeBSD$ */

/* XXX: Depend on our system headers protecting against multiple includes. */
#include <paths.h>
#undef _PATH_FTPUSERS

#include <pwd.h>

#define _DIAGASSERT(x)

long long strsuftollx(const char *, const char *,
    long long, long long, char *, size_t);
