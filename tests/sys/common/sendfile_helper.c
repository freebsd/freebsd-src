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
#include <sys/ioctl.h>
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

/*
 * Queue a byte with SCM_RIGHTS on a unix(4) socket right behind the sendfile
 * data, which isn't ready yet if the disk I/O is slow, then receive everything
 * and check that the socket is empty and reports so.
 */
static void
unix_rights(int fd, off_t start, size_t len, int flags)
{
	char cbuf[CMSG_SPACE(sizeof(int))], rcbuf[CMSG_SPACE(sizeof(int))];
	struct cmsghdr *cmsg;
	struct iovec iov;
	struct msghdr msg;
	off_t sbytes;
	size_t total;
	ssize_t rv;
	int n, nfd, rights, ss[2];

	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, ss) != 0)
		err(1, "socketpair");
	nfd = open("/dev/null", O_RDONLY);
	if (nfd < 0)
		err(1, "open /dev/null");

	if (sendfile(fd, ss[0], start, len, NULL, &sbytes, flags) < 0)
		err(3, "sendfile");

	memset(cbuf, 0, sizeof(cbuf));
	iov.iov_base = "x";
	iov.iov_len = 1;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cmsg), &nfd, sizeof(int));
	if (sendmsg(ss[0], &msg, 0) != 1)
		err(1, "sendmsg");

	rights = 0;
	for (total = 0; total < (size_t)sbytes + 1; total += rv) {
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = rcbuf;
		msg.msg_controllen = sizeof(rcbuf);
		rv = recvmsg(ss[1], &msg, 0);
		if (rv == -1)
			err(2, "recvmsg");
		if (rv == 0)
			errx(4, "unexpected EOF after %zu of %jd bytes", total,
			    (intmax_t)sbytes + 1);
		cmsg = CMSG_FIRSTHDR(&msg);
		if (cmsg != NULL && cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			memcpy(&n, CMSG_DATA(cmsg), sizeof(int));
			close(n);
			rights++;
		}
	}
	if (rights != 1)
		errx(4, "received %d SCM_RIGHTS messages", rights);

	/* Check the count before reading from the empty socket. */
	if (ioctl(ss[1], FIONREAD, &n) == -1)
		err(1, "FIONREAD");
	if (n != 0)
		errx(4, "FIONREAD %d after receiving everything", n);
	rv = recv(ss[1], buf, sizeof(buf), MSG_DONTWAIT);
	if (rv != -1 || errno != EAGAIN)
		errx(4, "recv on the empty socket returned %zd", rv);

	exit(0);
}

static void
usage(void)
{
	errx(1, "usage: %s [-u [-r]] [-c host] [-p port] "
	    "<file> <start> <len> <flags>", getprogname());
}

int
main(int argc, char **argv)
{
	pthread_t pt;
	off_t start;
	int ch, fd, ss[2], flags, error;
	bool pf_unix = false;
	bool rights = false;
	bool tcp_client = false;
	const char *host, *port;

	while ((ch = getopt(argc, argv, "c:p:ru")) != -1)
		switch (ch) {
		case 'c':
			host = optarg;
			tcp_client = true;
			break;
		case 'p':
			port = optarg;
			tcp_client = true;
			break;
		case 'r':
			rights = true;
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
	if (rights && !pf_unix)
		usage();

	start = strtoull(argv[1], NULL, 0);
	readlen = strtoull(argv[2], NULL, 0);
	flags = strtoul(argv[3], NULL, 0);

	fd = open(argv[0], O_RDONLY);
	if (fd < 0)
		err(1, "open");

	if (rights)
		unix_rights(fd, start, readlen, flags);

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
