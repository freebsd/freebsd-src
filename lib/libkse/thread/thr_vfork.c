/*
 * $FreeBSD: src/lib/libkse/thread/thr_vfork.c,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $
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
