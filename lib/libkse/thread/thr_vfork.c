/*
 * $FreeBSD: src/lib/libkse/thread/thr_vfork.c,v 1.5 2007/10/09 13:42:29 obrien Exp $
 */
#include <unistd.h>

#include "thr_private.h"

LT10_COMPAT_PRIVATE(_vfork);
LT10_COMPAT_DEFAULT(vfork);

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
