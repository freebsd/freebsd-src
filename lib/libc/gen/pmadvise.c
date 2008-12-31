/*
 * The contents of this file are in the public domain.
 * Written by Garrett A. Wollman, 2000-10-07.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/pmadvise.c,v 1.3.28.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/mman.h>

int
posix_madvise(void *address, size_t size, int how)
{
	return madvise(address, size, how);
}
