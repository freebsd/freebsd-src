/*
 * $FreeBSD: src/lib/libkse/thread/thr_vfork.c,v 1.7.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $
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
