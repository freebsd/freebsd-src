/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Klara, Inc.
 */

/*
 * A simple TCP proxy.  Listens on a local address until a connection appears,
 * then opens a TCP connection to the target address and shuttles data between
 * the two until one side closes its connection.
 *
 * For example:
 *
 *   $ proxy -l 127.0.0.1:8080 www.example.com:80
 *
 * The -m flag selects the mode of the transfer.  Specify "-m copy" to enable
 * copying through userspace, and "-m splice" to use SO_SPLICE.
 *
 * The -L flag enables a loopback mode, wherein all data is additionally proxied
 * through a loopback TCP connection.  This exists mostly to help test a
 * specific use-case in custom proxy software where we would like to use
 * SO_SPLICE.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct proxy_softc {
	struct sockaddr_storage	lss;
	struct sockaddr_storage	tss;
	size_t bufsz;
	enum proxy_mode { PROXY_MODE_COPY, PROXY_MODE_SPLICE } mode;
	bool loopback;
};

static void
usage(void)
{
	fprintf(stderr,
"usage: proxy [-m copy|splice] [-s <buf-size>] [-L] -l <listen addr> <target addr>\n");
	exit(1);
}

static void
proxy_copy(struct proxy_softc *sc, int cs, int ts)
{
	struct kevent kev[2];
	uint8_t *buf;
	int kq;

	kq = kqueue();
	if (kq == -1)
		err(1, "kqueue");

	EV_SET(&kev[0], cs, EVFILT_READ, EV_ADD, 0, 0, (void *)(uintptr_t)ts);
	EV_SET(&kev[1], ts, EVFILT_READ, EV_ADD, 0, 0, (void *)(uintptr_t)cs);
	if (kevent(kq, kev, 2, NULL, 0, NULL) == -1)
		err(1, "kevent");

	buf = malloc(sc->bufsz);
	if (buf == NULL)
		err(1, "malloc");

	for (;;) {
		uint8_t *data;
		ssize_t n, resid;
		int rs, ws;

		if (kevent(kq, NULL, 0, kev, 2, NULL) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "kevent");
		}

		rs = (int)kev[0].ident;
		ws = (int)(uintptr_t)kev[0].udata;

		n = read(rs, buf, sc->bufsz);
		if (n == -1) {
			if (errno == ECONNRESET)
				break;
			err(1, "read");
		}
		if (n == 0)
			break;

		data = buf;
		resid = n;
		do {
			n = write(ws, data, resid);
			if (n == -1) {
				if (errno == EINTR)
					continue;
				if (errno == ECONNRESET || errno == EPIPE)
					break;
				err(1, "write");
			}
			assert(n > 0);
			data += n;
			resid -= n;
		} while (resid > 0);
	}

	free(buf);
	close(kq);
}

static void
splice(int s1, int s2)
{
	struct splice sp;

	memset(&sp, 0, sizeof(sp));
	sp.sp_fd = s2;
	if (setsockopt(s1, SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp)) == -1)
		err(1, "setsockopt");
}

static void
proxy_splice(struct proxy_softc *sc __unused, int cs, int ts)
{
	struct kevent kev[2];
	int error, kq;

	/* Set up our splices. */
	splice(cs, ts);
	splice(ts, cs);

	/* Block until the connection is terminated. */
	kq = kqueue();
	if (kq == -1)
		err(1, "kqueue");
	EV_SET(&kev[0], cs, EVFILT_READ, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[1], ts, EVFILT_READ, EV_ADD, 0, 0, NULL);
	do {
		error = kevent(kq, kev, 2, kev, 2, NULL);
		if (error == -1 && errno != EINTR)
			err(1, "kevent");
	} while (error <= 0);

	close(kq);
}

static void
nodelay(int s)
{
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int)) ==
	    -1)
		err(1, "setsockopt");
}

/*
 * Like socketpair(2), but for TCP sockets on the  loopback address.
 */
static void
tcp_socketpair(int out[2], int af)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr *sa;
	int sd[2];

	sd[0] = socket(af, SOCK_STREAM, 0);
	if (sd[0] == -1)
		err(1, "socket");
	sd[1] = socket(af, SOCK_STREAM, 0);
	if (sd[1] == -1)
		err(1, "socket");

	nodelay(sd[0]);
	nodelay(sd[1]);

	if (af == AF_INET) {
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		sin.sin_port = 0;
		sin.sin_len = sizeof(sin);
		sa = (struct sockaddr *)&sin;
	} else if (af == AF_INET6) {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = in6addr_loopback;
		sin6.sin6_port = 0;
		sin6.sin6_len = sizeof(sin6);
		sa = (struct sockaddr *)&sin6;
	} else {
		errx(1, "unsupported address family %d", af);
	}

	if (bind(sd[0], sa, sa->sa_len) == -1)
		err(1, "bind");
	if (listen(sd[0], 1) == -1)
		err(1, "listen");

	if (getsockname(sd[0], sa, &(socklen_t){sa->sa_len}) == -1)
		err(1, "getsockname");
	if (connect(sd[1], sa, sa->sa_len) == -1)
		err(1, "connect");

	out[0] = sd[1];
	out[1] = accept(sd[0], NULL, NULL);
	if (out[1] == -1)
		err(1, "accept");
	close(sd[0]);
}

/*
 * Proxy data between two connected TCP sockets.  Returns the PID of the process
 * forked off to handle the data transfer.
 */
static pid_t
proxy(struct proxy_softc *sc, int s1, int s2)
{
	pid_t child;

	child = fork();
	if (child == -1)
		err(1, "fork");
	if (child != 0) {
		close(s1);
		close(s2);
		return (child);
	}

	if (sc->mode == PROXY_MODE_COPY)
		proxy_copy(sc, s1, s2);
	else
		proxy_splice(sc, s1, s2);
	_exit(0);
}

/*
 * The proxy event loop accepts connections and forks off child processes to
 * handle them.  We also handle events generated when child processes exit
 * (triggered by one side closing its connection).
 */
static void
eventloop(struct proxy_softc *sc)
{
	struct kevent kev;
	int kq, lsd;
	pid_t child;

	lsd = socket(sc->lss.ss_family, SOCK_STREAM, 0);
	if (lsd == -1)
		err(1, "socket");
	if (setsockopt(lsd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) ==
	    -1)
		err(1, "setsockopt");
	if (bind(lsd, (struct sockaddr *)&sc->lss, sc->lss.ss_len) == -1)
		err(1, "bind");
	if (listen(lsd, 5) == -1)
		err(1, "listen");

	kq = kqueue();
	if (kq == -1)
		err(1, "kqueue");
	EV_SET(&kev, lsd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1)
		err(1, "kevent");

	for (;;) {
		if (kevent(kq, NULL, 0, &kev, 1, NULL) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "kevent");
		}

		switch (kev.filter) {
		case EVFILT_READ: {
			int s, ts;

			if ((int)kev.ident != lsd)
				errx(1, "unexpected event ident %d",
				    (int)kev.ident);

			s = accept(lsd, NULL, NULL);
			if (s == -1)
				err(1, "accept");
			nodelay(s);

			ts = socket(sc->tss.ss_family, SOCK_STREAM, 0);
			if (ts == -1)
				err(1, "socket");
			nodelay(ts);
			if (connect(ts, (struct sockaddr *)&sc->tss,
			    sc->tss.ss_len) == -1)
				err(1, "connect");

			if (sc->loopback) {
				int ls[2];

				tcp_socketpair(ls, sc->tss.ss_family);
				child = proxy(sc, ls[0], ts);
				EV_SET(&kev, child, EVFILT_PROC, EV_ADD,
				    NOTE_EXIT, 0, NULL);
				if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1)
					err(1, "kevent");
				child = proxy(sc, s, ls[1]);
				EV_SET(&kev, child, EVFILT_PROC, EV_ADD,
				    NOTE_EXIT, 0, NULL);
				if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1)
					err(1, "kevent");
			} else {
				child = proxy(sc, s, ts);
				EV_SET(&kev, child, EVFILT_PROC, EV_ADD,
				    NOTE_EXIT, 0, NULL);
				if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1)
					err(1, "kevent");
			}

			break;
			}
		case EVFILT_PROC: {
			int status;

			child = kev.ident;
			status = (int)kev.data;
			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					errx(1, "child exited with status %d",
					    WEXITSTATUS(status));
				}
			} else if (WIFSIGNALED(status)) {
				warnx("child %d terminated by signal %d",
				    (pid_t)kev.ident, WTERMSIG(status));
			}
			if (waitpid(child, NULL, 0) == -1)
				err(1, "waitpid");
			break;
			}
		}
	}
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

static void
proxy_init(struct proxy_softc *sc, const char *laddr, const char *taddr,
    size_t bufsz, enum proxy_mode mode, bool loopback)
{
	addrinfo(&sc->lss, laddr);
	addrinfo(&sc->tss, taddr);

	sc->bufsz = bufsz;
	sc->mode = mode;
	sc->loopback = loopback;
}

int
main(int argc, char **argv)
{
	struct proxy_softc sc;
	char *laddr, *taddr;
	size_t bufsz;
	enum proxy_mode mode;
	int ch;
	bool loopback;

	(void)signal(SIGPIPE, SIG_IGN);

	loopback = false;
	mode = PROXY_MODE_COPY;
	bufsz = 2 * 1024 * 1024ul;
	laddr = taddr = NULL;
	while ((ch = getopt(argc, argv, "Ll:m:s:")) != -1) {
		switch (ch) {
		case 'l':
			laddr = optarg;
			break;
		case 'L':
			loopback = true;
			break;
		case 'm':
			if (strcmp(optarg, "copy") == 0)
				mode = PROXY_MODE_COPY;
			else if (strcmp(optarg, "splice") == 0)
				mode = PROXY_MODE_SPLICE;
			else
				usage();
			break;
		case 's':
			bufsz = atoi(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (laddr == NULL || argc != 1)
		usage();
	taddr = argv[0];

	/* Marshal command-line parameters into a neat structure. */
	proxy_init(&sc, laddr, taddr, bufsz, mode, loopback);

	/* Start handling connections. */
	eventloop(&sc);

	return (0);
}
