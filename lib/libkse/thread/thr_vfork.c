/*
 * $FreeBSD: src/lib/libkse/thread/thr_vfork.c,v 1.5.2.1.2.1 2010/02/10 00:26:20 kensmith Exp $
 */

#include <unistd.h>
#include "thr_private.h"

LT10_COMPAT_PRIVATE(_vfork);
LT10_COMPAT_DEFAULT(vfork);

int _vfork(void);

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
