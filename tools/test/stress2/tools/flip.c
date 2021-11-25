/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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

/*
 * Flip one or more bits in a file.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char *optarg;
extern int optind;

static long
random_long(long mi, long ma)
{
        return (arc4random()  % (ma - mi + 1) + mi);
}

static void
flip(void *ap, size_t len)
{
	unsigned char *cp;
	int byte;
	unsigned char bit, buf, mask, old __unused;

	cp = (unsigned char *)ap;
	byte = random_long(0, len);
	bit = random_long(0,7);
	mask = ~(1 << bit);
	buf = cp[byte];
	old = cp[byte];
	buf = (buf & mask) | (~buf & ~mask);
	cp[byte] = buf;
#if defined(DEBUG)
	printf("Change %2x to %2x at %d by flipping bit %d\n",
	    old, buf, byte, bit);
#endif
}

int
main(int argc, char *argv[])
{
	struct stat st;
	off_t pos;
	size_t size;
	int fd, i, times;
	char c;

	times = 1;
	size = 0;
	while ((c = getopt(argc, argv, "n:s:")) != -1) {
		switch (c) {
			case 'n':
				times = atoi(optarg);
				break;
			case 's':
				size = atol(optarg);
				break;
			case '?':
			default:
				fprintf(stderr,
				    "Usage: %s [ -n <num> <file>]\n",
				    argv[0]);
				exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "Missing file name\n");
		exit(1);
	}

	if ((fd = open(argv[0], O_RDWR)) == -1)
		err(1, "open(%s)", argv[0]);

	if (size == 0) {
		if (fstat(fd, &st) == -1)
			err(1, "stat %s", argv[0]);
		size = st.st_size;
	}

	for (i = 0; i < times; i++) {
		pos = arc4random() % size;
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek()");
		if (read(fd, &c, 1) != 1)
			err(1, "read()");
		flip(&c, 1);
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek()");
		if (write(fd, &c, 1) != 1)
			err(1, "write()");
	}

	return (0);
}
