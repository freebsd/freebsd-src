/* $FreeBSD$ */

/* XXX: Depend on our system headers protecting against multiple includes. */
#include <paths.h>
#undef _PATH_FTPUSERS

#include <pwd.h>

#define	LOGIN_NAME_MAX MAXLOGNAME	/* <sys/param.h> */

#define _DIAGASSERT(x)
