/* $FreeBSD$ */

/* XXX: Depend on our system headers protecting against multiple includes. */
#include <paths.h>
#undef _PATH_FTPUSERS

#include <pwd.h>

#define _DIAGASSERT(x)

#include <sys/_types.h>
#ifndef _SIZE_T_DECLARED
typedef	__size_t	size_t;
#define	_SIZE_T_DECLARED
#endif
long long strsuftollx(const char *, const char *,
    long long, long long, char *, size_t);

/*
 * IEEE Std 1003.1c-95, adopted in X/Open CAE Specification Issue 5 Version 2
 */
#if __POSIX_VISIBLE >= 199506 || __XSI_VISIBLE >= 500
#define	LOGIN_NAME_MAX	MAXLOGNAME	/* max login name length (incl. NUL) */
#endif
