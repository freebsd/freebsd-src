/*
 * The contents of this file are in the public domain.
 * Written by Garrett A. Wollman, 2000-10-07.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/pmadvise.c,v 1.2 2002/02/01 00:57:29 obrien Exp $");

#include <sys/mman.h>

int
(posix_madvise)(void *address, size_t size, int how)
{
	return posix_madvise(address, size, how);
}
