/*
 * The contents of this file are in the public domain.
 * Written by Garrett A. Wollman, 2000-10-07.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/mman.h>

int
(posix_madvise)(void *address, size_t size, int how)
{
	return posix_madvise(address, size, how);
}
