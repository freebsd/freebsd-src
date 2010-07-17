/*
 * $FreeBSD: src/lib/libc_r/uthread/uthread_vfork.c,v 1.3.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
