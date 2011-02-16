/*
 * $FreeBSD: src/lib/libc_r/uthread/uthread_vfork.c,v 1.3.40.1 2010/12/21 17:10:29 kensmith Exp $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
