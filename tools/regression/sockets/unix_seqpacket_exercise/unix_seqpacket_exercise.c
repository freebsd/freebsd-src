/*-
 * Copyright (c) 2009 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define	min(x, y)	(x < y ? x : y)

#define	BUFLEN	32768

#define	SEQPACKET_RCVBUF	(131072-16)
#define	SEQPACKET_SNDBUF	(131072-16)

#define	FAILERR(str)		err(-1, "%s: %s", __func__, str)
#define	FAILNERR(str, n)	err(-1, "%s %zd: %s", __func__, n, str)
#define	FAILNMERR(str, n, m)	err(-1, "%s %zd %d: %s", __func__, n, m, str)
#define	FAILERRX(str)		errx(-1, "%s: %s", __func__, str)
#define	FAILNERRX(str, n)	errx(-1, "%s %zd: %s", __func__, n, str)
#define	FAILNMERRX(str, n, m)	errx(-1, "%s %zd %d: %s", __func__, n, m, str)

static int ann = 0;

#define	ANN()		(ann ? warnx("%s: start", __func__) : 0)
#define	ANNN(n)		(ann ? warnx("%s %zd: start", __func__, (n)) : 0)
#define	ANNNM(n, m)	(ann ? warnx("%s %zd %d: start", __func__, (n), (m)):0)

#define	OK()		warnx("%s: ok", __func__)
#define	OKN(n)		warnx("%s %zd: ok", __func__, (n))
#define	OKNM(n, m)	warnx("%s %zd %d: ok", __func__, (n), (m))

#ifdef SO_NOSIGPIPE
#define	NEW_SOCKET(s) do {						\
	int i;								\
									\
	(s) = socket(PF_LOCAL, SOCK_SEQPACKET, 0);			\
	if ((s) < 0)							\
		FAILERR("socket");					\
									\
	i = 1;								\
	if (setsockopt((s), SOL_SOCKET, SO_NOSIGPIPE, &i, sizeof(i)) < 0) \
		FAILERR("setsockopt SO_NOSIGPIPE");			\
									\
	i = SEQPACKET_RCVBUF;						\
	if (setsockopt((s), SOL_SOCKET, SO_RCVBUF, &i, sizeof(i)) < 0)	\
		FAILERR("setsockopt SO_RCVBUF");			\
									\
	i = SEQPACKET_SNDBUF;						\
	if (setsockopt((s), SOL_SOCKET, SO_SNDBUF, &i, sizeof(i)) < 0)	\
		FAILERR("setsockopt SO_SNDBUF");			\
} while (0)
#else
#define	NEW_SOCKET(s) do {						\
	int i;								\
									\
	(s) = socket(PF_LOCAL, SOCK_SEQPACKET, 0);			\
	if ((s) < 0)							\
		FAILERR("socket");					\
									\
	i = SEQPACKET_RCVBUF;						\
	if (setsockopt((s), SOL_SOCKET, SO_RCVBUF, &i, sizeof(i)) < 0)	\
		FAILERR("setsockopt SO_RCVBUF");			\
									\
	i = SEQPACKET_SNDBUF;						\
	if (setsockopt((s), SOL_SOCKET, SO_SNDBUF, &i, sizeof(i)) < 0)	\
		FAILERR("setsockopt SO_SNDBUF");			\
} while (0)
#endif

static void
server(int s_listen)
{
	char buffer[BUFLEN];
	ssize_t ssize_recv, ssize_send;
	socklen_t socklen;
	int i, s_accept;

	while (1) {
		s_accept = accept(s_listen, NULL, 0);
		if (s_accept >= 0) {
			i = SEQPACKET_RCVBUF;
			if (setsockopt(s_accept, SOL_SOCKET, SO_RCVBUF, &i,
			    sizeof(i)) < 0) {
				warn("server: setsockopt SO_RCVBUF");
				close(s_accept);
				continue;
			}

			if (getsockopt(s_accept, SOL_SOCKET, SO_RCVBUF, &i,
			    &socklen) < 0) {
				warn("server: getsockopt SO_RCVBUF");
				close(s_accept);
				continue;
			}
			if (i != SEQPACKET_RCVBUF) {
				warnx("server: getsockopt SO_RCVBUF wrong %d",
				    i);
				close(s_accept);
				continue;
			}

			socklen = sizeof(i);
			if (getsockopt(s_accept, SOL_SOCKET, SO_SNDBUF, &i,
			    &socklen) < 0) {
				warn("server: getsockopt SO_SNDBUF");
				close(s_accept);
				continue;
			}
			if (i != SEQPACKET_SNDBUF) {
				warnx("server: getsockopt SO_SNDBUF wrong %d",
				    i);
				close(s_accept);
				continue;
			}

			do {
				ssize_recv = recv(s_accept, buffer,
				    sizeof(buffer), 0);
				if (ssize_recv == 0)
					break;
				if (ssize_recv < 0) {
					warn("server: recv");
					break;
				}
				ssize_send = send(s_accept, buffer,
				    ssize_recv, 0);
				if (ssize_send == 0)
					break;
				if (ssize_send < 0) {
					warn("server: send");
					break;
				}
				if (ssize_send != ssize_recv)
					warnx("server: recv %zd sent %zd",
					    ssize_recv, ssize_send);
			} while (1);
			close(s_accept);
		} else
			warn("server: accept");
	}
}

static void
test_connect(struct sockaddr_un *sun)
{
	int s;

	ANN();
	NEW_SOCKET(s);
	if (connect(s, (struct sockaddr *)sun, sizeof(*sun)) < 0)
		FAILERR("connect");
	(void)close(s);
	OK();
}

static void
test_connect_send(struct sockaddr_un *sun)
{
	ssize_t ssize;
	char ch;
	int s;

	ANN();
	NEW_SOCKET(s);
	if (connect(s, (struct sockaddr *)sun, sizeof(*sun)) < 0)
		FAILERR("connect");
	ssize = send(s, &ch, sizeof(ch), 0);
	if (ssize < 0)
		FAILERR("send");
	if (ssize != sizeof(ch))
		FAILERRX("send wrong size");
	(void)close(s);
	OK();
}

static void
test_connect_shutdown_send(struct sockaddr_un *sun)
{
	ssize_t ssize;
	char ch;
	int s;

	ANN();
	NEW_SOCKET(s);
	if (connect(s, (struct sockaddr *)sun, sizeof(*sun)) < 0)
		FAILERR("connect");
	if (shutdown(s, SHUT_RDWR) < 0)
		FAILERR("shutdown SHUT_RDWR");
	ssize = send(s, &ch, sizeof(ch), 0);
	if (ssize >= 0)
		FAILERRX("send");
	if (errno != EPIPE)
		FAILERR("send unexpected error");
	(void)close(s);
	OK();
}

static void
test_connect_send_recv(struct sockaddr_un *sun, size_t size)
{
	char buf[size + 4];	/* Detect extra bytes. */
	size_t truncsize;
	ssize_t ssize;
	int s;

	ANNN(size);
	NEW_SOCKET(s);
	if (connect(s, (struct sockaddr *)sun, sizeof(*sun)) < 0)
		FAILNERR("connect", size);
	ssize = send(s, buf, size, 0);
	if (ssize < 0 && size >= SEQPACKET_RCVBUF)
		goto out;
	if (ssize < 0)
		FAILNERR("send", size);
	if (ssize == 0)
		FAILNERR("send eof", size);
	if (ssize != size)
		FAILNERRX("send size", size);

	truncsize = min(size, BUFLEN);
	ssize = recv(s, buf, sizeof(buf), 0);
	if (ssize < 0)
		FAILNERR("recv", size);
	if (ssize == 0)
		FAILNERRX("recv eof", size);
	if (ssize < truncsize)
		FAILNERRX("recv too few bytes", size);
	if (ssize > truncsize)
		FAILNERRX("recv too many bytes", size);
out:
	(void)close(s);
	OKN(size);
}

static void
test_connect_send_recv_count(struct sockaddr_un *sun, int count, size_t size)
{
	char buf[size + 4];	/* Detect extra bytes and coalescing. */
	size_t truncsize;
	ssize_t ssize;
	int i, s;

	ANNNM(size, count);
	NEW_SOCKET(s);
	if (connect(s, (struct sockaddr *)sun, sizeof(*sun)) < 0)
		FAILNMERR("connect", size, count);
	for (i = 0; i < count; i++) {
		usleep(5000);
		ssize = send(s, buf, size, 0);
		if (ssize < 0 && size >= SEQPACKET_RCVBUF)
			goto out;
		if (ssize < 0)
			FAILNMERR("send", size, count);
		if (ssize == 0)
			FAILNMERRX("send eof", size, count);
		if (ssize != size)
			FAILNMERRX("send size", size, count);
	}

	truncsize = min(size, BUFLEN);
	for (i = 0; i < count; i++) {
		ssize = recv(s, buf, sizeof(buf), 0);
		if (ssize < 0)
			FAILNMERR("recv", size, count);
		if (ssize == 0)
			FAILNMERRX("recv eof", size, count);
		if (ssize < truncsize)
			FAILNMERRX("recv too few bytes", size, count);
		if (ssize > truncsize)
			FAILNMERRX("recv too many bytes", size, count);
	}
out:
	(void)close(s);
	OKNM(size, count);
}

static void
test_sendto(struct sockaddr_un *sun)
{
	ssize_t ssize;
	char ch;
	int s;

	ANN();
	NEW_SOCKET(s);
	ssize = sendto(s, &ch, sizeof(ch), 0, (struct sockaddr *)sun,
	    sizeof(*sun));
	if (ssize < 0)
		FAILERR("sendto");
	(void)close(s);
	OK();
}

static void
client(struct sockaddr_un *sun)
{
	size_t sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048,
	    4096, 8192, 16384, 32768, 65536 /*, 131072 */};
	int c, i;

	test_connect(sun);
	test_connect_send(sun);
	test_connect_shutdown_send(sun);

	/*
	 * Try a range of sizes and packet counts.
	 */
	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
		test_connect_send_recv(sun, sizes[i]);
	for (c = 1; c <= 8; c++) {
		for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
			test_connect_send_recv_count(sun, c, sizes[i]);
	}
	test_sendto(sun);
	printf("client done\n");
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un sun;
	char path[PATH_MAX];
	pid_t pid_client, pid_server;
	int i, s_listen;

	snprintf(path, sizeof(path), "/tmp/lds_exercise.XXXXXXXXX");
	if (mktemp(path) == NULL)
		FAILERR("mktemp");

	s_listen = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	if (s_listen < 0) {
		(void)unlink(path);
		FAILERR("socket");
	}

	i = SEQPACKET_RCVBUF;
	if (setsockopt(s_listen, SOL_SOCKET, SO_RCVBUF, &i, sizeof(i)) < 0) {
		(void)unlink(path);
		FAILERR("setsockopt SO_RCVBUF");
	}

	i = SEQPACKET_SNDBUF;
	if (setsockopt(s_listen, SOL_SOCKET, SO_SNDBUF, &i, sizeof(i)) < 0) {
		(void)unlink(path);
		FAILERR("setsockopt SO_SNDBUF");
	}

	i = 1;
	if (setsockopt(s_listen, SOL_SOCKET, SO_NOSIGPIPE, &i, sizeof(i))
	    < 0) {
		(void)unlink(path);
		FAILERR("setsockopt SO_NOSIGPIPE");
	}

	bzero(&sun, sizeof(sun));
	sun.sun_len = sizeof(sun);
	sun.sun_family = AF_LOCAL;
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));

	if (bind(s_listen, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
		(void)unlink(path);
		FAILERR("bind");
	}

	if (listen(s_listen, -1) < 0) {
		(void)unlink(path);
		FAILERR("listen");
	}

	pid_server = fork();
	if (pid_server < 0) {
		(void)unlink(path);
		FAILERR("fork");
	}
	if (pid_server == 0) {
		server(s_listen);
		return (0);
	}

	pid_client = fork();
	if (pid_client < 0) {
		(void)kill(pid_server, SIGKILL);
		(void)unlink(path);
		FAILERR("fork");
	}
	if (pid_client == 0) {
		client(&sun);
		return (0);
	}

	/*
	 * When the client is done, kill the server and clean up.
	 */
	(void)waitpid(pid_client, NULL, 0);
	(void)kill(pid_server, SIGKILL);
	(void)unlink(path);
	return (0);
}
