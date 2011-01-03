/*-
 * Copyright (c) 2010 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $FreeBSD$ */
/*
 * Compile with 'cc -I. -I../../cddl/boot/zfs zfstest.c -o zfstest'
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>

#define NBBY 8

void
pager_output(const char *line)
{
	printf("%s", line);
}

#include "zfsimpl.c"

static int
vdev_read(vdev_t *vdev, void *priv, off_t off, void *buf, size_t bytes)
{
	int fd = *(int *) priv;

	if (pread(fd, buf, bytes, off) != bytes)
		return -1;
	return 0;
}

static int
zfs_read(spa_t *spa, dnode_phys_t *dn, void *buf, size_t size, off_t off)
{
	const znode_phys_t *zp = (const znode_phys_t *) dn->dn_bonus;
	size_t n;
	int rc;

	n = size;
	if (off + n > zp->zp_size)
		n = zp->zp_size - off;
	
	rc = dnode_read(spa, dn, off, buf, n);
	if (rc)
		return (rc);

	return (n);
}

int
main(int argc, char** argv)
{
	int i, n, off;
	int fd[99];
	spa_t *spa;
	dnode_phys_t dn;
	char buf[512];

	zfs_init();
	if (argc == 1) {
		static char *av[] = {
			"zfstest", "/dev/da0p2", "/dev/da1p2", "/dev/da2p2",
			NULL,
		};
		argc = 4;
		argv = av;
	}
	for (i = 1; i < argc; i++) {
		fd[i] = open(argv[i], O_RDONLY);
		if (fd[i] < 0)
			continue;
		if (vdev_probe(vdev_read, &fd[i], NULL) != 0)
			close(fd[i]);
	}
	spa_all_status();

	spa = STAILQ_FIRST(&zfs_pools);
	if (!spa || zfs_mount_pool(spa))
		exit(1);

	if (zfs_lookup(spa, "zfs.c", &dn))
		exit(1);

	off = 0;
	do {
		n = zfs_read(spa, &dn, buf, 512, off);
		write(1, buf, n);
		off += n;
	} while (n == 512);
}
