/*
 * $FreeBSD: src/lib/libkse/thread/thr_vfork.c,v 1.7.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
 */

#include <unistd.h>
#include "thr_private.h"

int _vfork(void);

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
