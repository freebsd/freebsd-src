/*
 * $FreeBSD: src/lib/libc_r/uthread/uthread_vfork.c,v 1.3.32.1 2008/11/25 02:59:29 kensmith Exp $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
