/*
 * $FreeBSD: src/lib/libc_r/uthread/uthread_vfork.c,v 1.3.38.1 2010/02/10 00:26:20 kensmith Exp $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
