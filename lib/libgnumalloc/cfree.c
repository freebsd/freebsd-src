/*
 * cfree.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

void
cfree(void *foo)
{
	free(foo);
}
