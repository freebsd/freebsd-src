/* $FreeBSD$ */

/* XXX: Depend on our system headers protecting against multiple includes. */
#include <paths.h>
#undef _PATH_FTPUSERS

#include <pwd.h>

#define	LOGIN_NAME_MAX MAXLOGNAME	/* <sys/param.h> */

#define _DIAGASSERT(x)

#include <sys/_types.h>
#ifndef _SIZE_T_DECLARED
typedef	__size_t	size_t;
#define	_SIZE_T_DECLARED
#endif
long long strsuftollx(const char *, const char *,
    long long, long long, char *, size_t);
