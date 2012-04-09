/*
 * $FreeBSD: src/lib/libc_r/uthread/uthread_vfork.c,v 1.3.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
