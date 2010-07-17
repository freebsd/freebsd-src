/*
 * $FreeBSD: src/lib/libkse/thread/thr_vfork.c,v 1.7.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
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
