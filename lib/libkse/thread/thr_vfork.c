/*
 * $FreeBSD: src/lib/libkse/thread/thr_vfork.c,v 1.7.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $
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
