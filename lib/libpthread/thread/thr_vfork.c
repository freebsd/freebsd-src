/*
 * $FreeBSD: src/lib/libpthread/thread/thr_vfork.c,v 1.3 2001/04/10 04:19:20 deischen Exp $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
