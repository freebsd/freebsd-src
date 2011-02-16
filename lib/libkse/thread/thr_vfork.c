/*
 * $FreeBSD: src/lib/libkse/thread/thr_vfork.c,v 1.5.2.1.4.1 2010/12/21 17:10:29 kensmith Exp $
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
