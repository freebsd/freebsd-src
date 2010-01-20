/*
 * $FreeBSD$
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
