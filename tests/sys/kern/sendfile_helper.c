/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Netflix, Inc.
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int ls;
static char buf[1024*1024];
static volatile bool accept_done = false;
static volatile bool read_done = false;

static void *
server(void *arg)
{
	struct sockaddr_in sin;
	ssize_t rv;
	socklen_t slen;
	int ss;
	ssize_t readlen = (uintptr_t)arg;

	slen = sizeof(sin);
	ss = accept(ls, (void *)&sin, &slen);
	if (ss < 0)
		err(1, "accept ls");

	accept_done = true;

	do {
		rv = read(ss, buf, sizeof(buf));
		if (rv == -1)
			err(2, "read receiver");
		if (rv == 0)
			break;
		readlen -= rv;
	} while (readlen != 0);

	read_done = true;

	return NULL;
}

int
main(int argc, char **argv)
{
	pthread_t pt;
	struct sockaddr_in sin;
	off_t start, len;
	socklen_t slen;
	int fd, cs, on, flags, error;

	if (argc != 5)
		errx(1, "usage: %s <file> <start> <len> <flags>",
		    getprogname());

	start = strtoull(argv[2], NULL, 0);
	len = strtoull(argv[3], NULL, 0);
	flags = strtoul(argv[4], NULL, 0);

	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		err(1, "open");		

	ls = socket(PF_INET, SOCK_STREAM, 0);
	if (ls < 0)
		err(1, "socket ls");

	on = 1;
	if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (void *)&on,
	    (socklen_t)sizeof(on)) < 0)
		err(1, "SO_REUSEADDR");

	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = 0;
	if (bind(ls, (void *)&sin, sizeof(sin)) < 0)
		err(1, "bind ls");

	slen = sizeof(sin);
	if (getsockname(ls, (void *)&sin, &slen) < 0)
		err(1, "getsockname");

	if (listen(ls, 5) < 0)
		err(1, "listen ls");

	error = pthread_create(&pt, NULL, server, (void *)(uintptr_t)len);
	if (error)
		errc(1, error, "pthread_create");

	cs = socket(PF_INET, SOCK_STREAM, 0);
	if (cs < 0)
		err(1, "socket cs");

	if (connect(cs, (void *)&sin, sizeof(sin)) < 0)
		err(1, "connect cs");

	while (!accept_done)
		usleep(1000);

	if (sendfile(fd, cs, start, len, NULL, NULL, flags) < 0)
		err(3, "sendfile");

	while (!read_done)
		usleep(1000);

	exit(0);
}
