/*-
 * Copyright (c) 2023 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/mman.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define MAP_32BIT_MAX_ADDR      ((vm_offset_t)1 << 31)

int
main(void)
{
	size_t pagesize;
	void *s32;
	int fd;

	if ((pagesize = getpagesize()) <= 0)
		err(1, "getpagesize");

	fd = open("/dev/zero", O_RDONLY);
	if (fd <= 0)
		err(1, "open failed");

	s32 = mmap(NULL, pagesize, PROT_READ, MAP_32BIT | MAP_PRIVATE, fd, 0);
	if (s32 == MAP_FAILED)
		err(1, "mmap MAP_32BIT | MAP_PRIVATE failed");
	if (((vm_offset_t)s32 + pagesize) > MAP_32BIT_MAX_ADDR)
		errx(1, "mmap invalid result %p", s32);

	close(fd);
	if (munmap(s32, pagesize) != 0)
		err(1, "munmap failed");

	s32 = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
	    MAP_32BIT | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (s32 == MAP_FAILED)
		err(1, "mmap MAP_32BIT | MAP_ANONYMOUS | MAP_PRIVATE failed");
	if (((vm_offset_t)s32 + pagesize) > MAP_32BIT_MAX_ADDR)
		errx(1, "mmap invalid result %p", s32);

	if (munmap(s32, pagesize) != 0)
		err(1, "munmap failed");
	exit(0);
}
