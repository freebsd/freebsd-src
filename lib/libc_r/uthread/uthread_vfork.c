/*
 * $FreeBSD$
 */
#include <unistd.h>

#pragma weak	vfork=_vfork

int
_vfork(void)
{
	return (fork());
}
