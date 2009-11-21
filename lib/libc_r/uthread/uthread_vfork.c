/*
 * $FreeBSD: src/lib/libc_r/uthread/uthread_vfork.c,v 1.3.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
