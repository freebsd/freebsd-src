/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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

/*
 * Idea from a test case by Andrew "RhodiumToad" Gierth in Bugzilla PR 271766.
 */

#include <sys/param.h>
#include <sys/disk.h>
#include <sys/mman.h>

#include <crypto/cryptodev.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	const char *disk;
	char *buf1, *buf2;
	off_t disksz;
	size_t bufsz, iosz;
	ssize_t n;
	unsigned int offsets, secsz;
	int fd;

	if (argc != 2)
		errx(1, "Usage: %s <disk>", argv[0]);
	disk = argv[1];

	fd = open(disk, O_RDWR);
	if (fd < 0)
		err(1, "open(%s)", disk);

	if (ioctl(fd, DIOCGSECTORSIZE, &secsz) != 0)
		err(1, "ioctl(DIOCGSECTORSIZE)");
	if (secsz == 0)
		errx(1, "ioctl(DIOCGSECTORSIZE) returned 0");
	if (ioctl(fd, DIOCGMEDIASIZE, &disksz) != 0)
		err(1, "ioctl(DIOCGMEDIASIZE)");
	if (disksz / secsz < 2)
		errx(1, "disk needs to be at least 2 sectors in size");
	iosz = 2 * secsz;

	bufsz = iosz + secsz;
	buf1 = mmap(NULL, bufsz, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
	    -1, 0);
	if (buf1 == MAP_FAILED)
		err(1, "mmap");
	buf2 = mmap(NULL, bufsz, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
	    -1, 0);
	if (buf2 == MAP_FAILED)
		err(1, "mmap");

	arc4random_buf(buf1, bufsz);
	n = pwrite(fd, buf1, bufsz, 0);
	if (n < 0 || (size_t)n != bufsz)
		err(1, "pwrite");

	/*
	 * Limit the number of offsets we test with, to avoid spending too much
	 * time when the sector size is large.
	 */
	offsets = MAX(EALG_MAX_BLOCK_LEN, HMAC_MAX_BLOCK_LEN) + 1;

	/*
	 * Read test: read the first 2 sectors into buf1, then do the same with
	 * buf2, except at varying offsets into buf2.  After each read, compare
	 * the buffers and make sure they're identical.  This exercises corner
	 * cases in the crypto layer's buffer handling.
	 */
	n = pread(fd, buf1, iosz, 0);
	if (n < 0 || (size_t)n != iosz)
		err(1, "pread");
	for (unsigned int i = 0; i < offsets; i++) {
		n = pread(fd, buf2 + i, iosz, 0);
		if (n < 0 || (size_t)n != iosz)
			err(1, "pread");
		if (memcmp(buf1, buf2 + i, iosz) != 0)
			errx(1, "read mismatch at offset %u/%u", i, secsz);
	}

	/*
	 * Write test.  Try writing buffers at various alignments, and verify
	 * that we read back what we wrote.
	 */
	arc4random_buf(buf1, bufsz);
	for (unsigned int i = 0; i < offsets; i++) {
		n = pwrite(fd, buf1 + i, iosz, 0);
		if (n < 0 || (size_t)n != iosz)
			err(1, "pwrite");
		n = pread(fd, buf2, iosz, 0);
		if (n < 0 || (size_t)n != iosz)
			err(1, "pread");
		if (memcmp(buf1 + i, buf2, iosz) != 0)
			errx(1, "write mismatch at offset %u/%u", i, secsz);
	}

	return (0);
}
