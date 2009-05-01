/*
 * The contents of this file are in the public domain.
 * Written by Garrett A. Wollman, 2000-10-07.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/pmadvise.c,v 1.3.30.1 2009/04/15 03:14:26 kensmith Exp $");

#include <sys/mman.h>

int
posix_madvise(void *address, size_t size, int how)
{
	return madvise(address, size, how);
}
