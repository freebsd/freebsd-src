/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Klara, Inc.
 */

/*
 * A utility which implements a simple ping-pong over TCP, and prints the amount
 * of time elapsed for each round trip.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr,
"usage: pingpong [-c <target addr> [-l <msgsz>] [-n <count]]|[-s <listen addr>]\n");
	exit(1);
}

static void
addrinfo(struct sockaddr_storage *ss, const char *addr)
{
	struct addrinfo hints, *res, *res1;
	char *host, *port;
	int error;

	host = strdup(addr);
	if (host == NULL)
		err(1, "strdup");
	port = strchr(host, ':');
	if (port == NULL)
		errx(1, "invalid address '%s', should be <addr>:<port>", host);
	*port++ = '\0';

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, port, &hints, &res);
	if (error != 0)
		errx(1, "%s", gai_strerror(error));
	for (res1 = res; res != NULL; res = res->ai_next) {
		if (res->ai_protocol == IPPROTO_TCP) {
			memcpy(ss, res->ai_addr, res->ai_addrlen);
			break;
		}
	}
	if (res == NULL)
		errx(1, "no TCP address found for '%s'", host);
	free(host);
	freeaddrinfo(res1);
}

int
main(int argc, char **argv)
{
	char buf[1024];
	struct sockaddr_storage ss;
	char *addr;
	size_t len;
	int ch, count, s;
	enum { NONE, CLIENT, SERVER } mode;

	count = 0;
	len = 0;
	mode = NONE;
	while ((ch = getopt(argc, argv, "c:l:n:s:")) != -1) {
		switch (ch) {
		case 'c':
			addr = optarg;
			mode = CLIENT;
			break;
		case 'l':
			len = strtol(optarg, NULL, 10);
			if (len > sizeof(buf))
				errx(1, "message size too large");
			if (len == 0)
				errx(1, "message size must be non-zero");
			break;
		case 'n':
			count = strtol(optarg, NULL, 10);
			if (count <= 0)
				errx(1, "count must be positive");
			break;
		case 's':
			addr = optarg;
			mode = SERVER;
			break;
		default:
			usage();
			break;
		}
	}

	if (mode == NONE)
		usage();
	if (mode == SERVER && len != 0)
		usage();

	if (mode == CLIENT) {
		if (len == 0)
			len = 1;
		if (count == 0)
			count = 1;
	}

	addrinfo(&ss, addr);

	s = socket(ss.ss_family, SOCK_STREAM, 0);
	if (s == -1)
		err(1, "socket");
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int)) ==
	    -1)
		err(1, "setsockopt");

	if (mode == CLIENT) {
		struct timespec ts1, ts2;
		ssize_t n;

		memset(buf, 'a', len);
		if (connect(s, (struct sockaddr *)&ss, ss.ss_len) == -1)
			err(1, "connect");

		for (int i = 0; i < count; i++) {
			int64_t ns;

			clock_gettime(CLOCK_MONOTONIC, &ts1);
			n = write(s, buf, len);
			if (n < 0)
				err(1, "write");
			n = read(s, buf, len);
			if (n < 0)
				err(1, "read");
			if ((size_t)n < len)
				errx(1, "unexpected short read");
			clock_gettime(CLOCK_MONOTONIC, &ts2);

			ns = (ts2.tv_sec - ts1.tv_sec) * 1000000000 +
			    (ts2.tv_nsec - ts1.tv_nsec);
			printf("round trip time: %ld.%02ld us\n", ns / 1000,
			    (ns % 1000) / 10);
			for (size_t j = 0; j < len; j++) {
				if (buf[j] != 'b')
					errx(1, "unexpected data");
			}
		}
	} else /* if (mode == SERVER) */ {
		int cs;

		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &(int){1},
		    sizeof(int)) == -1)
			err(1, "setsockopt");
		if (bind(s, (struct sockaddr *)&ss, ss.ss_len) == -1)
			err(1, "bind");
		if (listen(s, 1) == -1)
			err(1, "listen");

		for (;;) {
			ssize_t n;

			cs = accept(s, NULL, NULL);
			if (cs == -1)
				err(1, "accept");

			for (;;) {
				n = read(cs, buf, sizeof(buf));
				if (n < 0) {
					if (errno == ECONNRESET)
						break;
					err(1, "read");
				}
				if (n == 0)
					break;
				memset(buf, 'b', n);
				n = write(cs, buf, n);
				if (n < 0)
					err(1, "write");
			}
			(void)close(cs);
		}
	}

	return (0);
}
