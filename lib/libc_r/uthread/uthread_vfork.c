/*
 * $FreeBSD$
 */
#include <unistd.h>

int
vfork(void)
{
	return (fork());
}
