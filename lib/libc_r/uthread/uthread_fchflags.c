/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * $OpenBSD: uthread_fchflags.c,v 1.1 1999/01/08 05:42:18 d Exp $
 * $FreeBSD$
 */

#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "pthread_private.h"

int
_fchflags(int fd, u_long flags)
{
	int             ret;

	if ((ret = _FD_LOCK(fd, FD_WRITE, NULL)) == 0) {
		ret = __sys_fchflags(fd, flags);
		_FD_UNLOCK(fd, FD_WRITE);
	}
	return (ret);
}

__strong_reference(_fchflags, fchflags);
