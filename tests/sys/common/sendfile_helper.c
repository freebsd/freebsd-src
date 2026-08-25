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
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char buf[1024*1024];
static ssize_t readlen;
static volatile bool read_done = false;

static int
tcp_socketpair(int *sv)
{
	struct sockaddr_in sin = {
		.sin_len = sizeof(struct sockaddr_in),
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	int flags;
	int ls;

	ls = socket(PF_INET, SOCK_STREAM, 0);
	if (ls < 0)
		err(1, "socket ls");

	if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &(socklen_t){1},
	    sizeof(int)) < 0)
		err(1, "SO_REUSEADDR");

	if (bind(ls, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		err(1, "bind ls");

	if (getsockname(ls, (struct sockaddr *)&sin,
	    &(socklen_t){ sizeof(sin) }) < 0)
		err(1, "getsockname");

	if (listen(ls, 5) < 0)
		err(1, "listen ls");

	sv[0] = socket(PF_INET, SOCK_STREAM, 0);
	if (sv[0] < 0)
		err(1, "socket cs");

	flags = fcntl(sv[0], F_GETFL);
	flags |= O_NONBLOCK;
	if (fcntl(sv[0], F_SETFL, flags) == -1)
		err(1, "fcntl +O_NONBLOCK");

	if (connect(sv[0], (void *)&sin, sizeof(sin)) == -1 &&
	    errno != EINPROGRESS)
		err(1, "connect cs");

	sv[1] = accept(ls, NULL, 0);
	if (sv[1] < 0)
		err(1, "accept ls");

	flags &= ~O_NONBLOCK;
	if (fcntl(sv[0], F_SETFL, flags) == -1)
		err(1, "fcntl -O_NONBLOCK");

	close(ls);

	return (0);
}

static int
tcp_client_socket(const char *host, const char *port)
{
	struct addrinfo hints, *res, *res0;
	int error;
	int s;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((error = getaddrinfo(host, port, &hints, &res0)) != 0)
		errx(1, "host %s port %s: %s.",
		    host, port, gai_strerror(error));
	s = -1;
	for (res = res0; res != NULL; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype,
			res->ai_protocol);
		if (s < 0) {
			warn("socket(pf:%d, type:%d, proto:%d)",
			    res->ai_family, res->ai_socktype,
			    res->ai_protocol);
			continue;
		}
		if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
			warn("connect(%s, %s)", host, port);
			close(s);
			s = -1;
			continue;
		} else
			break;
	}
	if (s < 0)
		exit(1);
	freeaddrinfo(res0);

	return s;
}

static void *
receiver(void *arg)
{
	int s = *(int *)arg;
	ssize_t rv;

	do {
		rv = read(s, buf, sizeof(buf));
		if (rv == -1)
			err(2, "read receiver");
		if (rv == 0)
			break;
		readlen -= rv;
	} while (readlen != 0);

	read_done = true;

	return NULL;
}

static void
usage(void)
{
	errx(1, "usage: %s [-u] [-c host] [-p port] "
	    "<file> <start> <len> <flags>", getprogname());
}

int
main(int argc, char **argv)
{
	pthread_t pt;
	off_t start;
	int ch, fd, ss[2], flags, error;
	bool pf_unix = false;
	bool tcp_client = false;
	const char *host, *port;

	while ((ch = getopt(argc, argv, "c:p:u")) != -1)
		switch (ch) {
		case 'c':
			host = optarg;
			tcp_client = true;
			break;
		case 'p':
			port = optarg;
			tcp_client = true;
			break;
		case 'u':
			pf_unix = true;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 4)
		usage();
	if (tcp_client && (host == NULL || port == NULL))
		errx(1, "Need to specify host and port.");

	start = strtoull(argv[1], NULL, 0);
	readlen = strtoull(argv[2], NULL, 0);
	flags = strtoul(argv[3], NULL, 0);

	fd = open(argv[0], O_RDONLY);
	if (fd < 0)
		err(1, "open");

	if (pf_unix) {
		if (socketpair(PF_LOCAL, SOCK_STREAM, 0, ss) != 0)
			err(1, "socketpair");
	} else if (tcp_client)
		ss[0] = tcp_client_socket(host, port);
	else
		tcp_socketpair(ss);

	if (tcp_client)
		read_done = true;	/* The receiver is another process. */
	else {
		error = pthread_create(&pt, NULL, receiver, &ss[1]);
		if (error)
			errc(1, error, "pthread_create");
	}

	if (sendfile(fd, ss[0], start, readlen, NULL, NULL, flags) < 0)
		err(3, "sendfile");

	while (!read_done)
		usleep(1000);

	exit(0);
}
