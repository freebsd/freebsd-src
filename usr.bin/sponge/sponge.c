/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Eitan Adler
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/uio.h>

#define DEFAULT_BUF_SIZE 16384
#define DEFAULT_BUF_CNT 12

static int flag_append = 0;

static void usage(void);
static void *safe_malloc(size_t size);
static void *safe_calloc(size_t count, size_t size);
static void *safe_reallocf(void *ptr, size_t size);

static void *
safe_malloc(size_t size)
{
	void *ret;

	ret = malloc(size);
	if (ret == NULL) {
		err(1, "malloc failed");
	}
	return (ret);
}

static void *
safe_calloc(size_t count, size_t size)
{
	void *ret;

	ret = calloc(count, size);
	if (ret == NULL) {
		err(1, "calloc failed");
	}
	return (ret);
}

static void *
safe_reallocf(void *ptr, size_t size)
{
	void *ret;

	ret = reallocf(ptr, size);
	if (ret == NULL) {
		err(1, "reallocf failed");
	}
	return (ret);
}

static void
usage(void)
{
	fprintf(stderr, "usage: sponge [-a] filename\n");
}

int
main(int argc, char* argv[])
{
	struct iovec *iov;
	char *buf;
	char *outfile;
	ssize_t i;
	size_t bufcnt;
	size_t whichbuf;
	size_t bufremain;
	long maxiovec;
	int fd;
	int openflags = O_WRONLY;
	int opt;

	while ((opt = getopt(argc, argv, "ah")) != -1) {
		switch (opt) {
		case 'a':
			flag_append = 1;
			break;
		case 'h':
			usage();
			exit(0);
		case '?':
		default:
			usage();
			exit(1);
		}
	}

	if (optind < argc) {
                outfile = argv[optind];
	}


	bufcnt = DEFAULT_BUF_CNT;
	whichbuf = 0;
	iov = safe_calloc(bufcnt, sizeof(*iov));

	for (;;) {
		buf = safe_malloc(DEFAULT_BUF_SIZE);
		i = read(STDIN_FILENO, buf, DEFAULT_BUF_SIZE);
		if (whichbuf == bufcnt) {
			bufcnt *= 2;
			iov = safe_reallocf(iov, bufcnt * sizeof(*iov));
		}
		if (i < 0) {
			err(1, "read failed");
		}
		if (i == 0) {
			free(buf);
			break;
		}
		iov[whichbuf].iov_base = buf;
		iov[whichbuf].iov_len = i;
		whichbuf++;
	}

	if (outfile) {
		if (flag_append) {
			openflags |= O_APPEND;
		} else {
			openflags |= O_TRUNC;
		}
		fd = open(outfile, openflags);
	}
	else {
		fd = STDOUT_FILENO;
	}

	if (fd < 0) {
		err(1, "failed to open");
	}

	maxiovec = sysconf(_SC_IOV_MAX);
	if (maxiovec == -1) {
		maxiovec =  _XOPEN_IOV_MAX;
	}
	bufcnt = whichbuf;
	bufremain = bufcnt;

	while (bufremain > 0) {
		whichbuf = bufremain < (unsigned long) maxiovec
			? bufremain : maxiovec;
		bufremain -= whichbuf;

		i = writev(fd, iov, whichbuf);
		if (i < 0) {
			err(1, "failed to write");
		}
	}

	if (outfile) {
		i = close(fd);
		if (i < 0) {
			err(1, "failed to close");
		}
	}
}
