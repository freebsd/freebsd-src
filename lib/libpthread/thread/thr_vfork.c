/*
 * $FreeBSD: src/lib/libpthread/thread/thr_vfork.c,v 1.3.26.1 2008/10/02 02:57:24 kensmith Exp $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
