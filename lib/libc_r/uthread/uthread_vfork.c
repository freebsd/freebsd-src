/*
 * $FreeBSD: src/lib/libc_r/uthread/uthread_vfork.c,v 1.3.34.1 2009/04/15 03:14:26 kensmith Exp $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
