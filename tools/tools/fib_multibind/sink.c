/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Klara, Inc.
 */

/*
 * A program to demonstrate the effects of the net.inet.tcp.bind_all_fibs and
 * net.inet.udp.bind_all_fibs sysctls when they are set to 0.
 *
 * The program accepts TCP connections (default) or UDP datagrams (-u flag) and
 * prints the FIB on which they were received, then discards them.  If -a is
 * specific, the program accepts data from all FIBs, otherwise it only accepts
 * data from the FIB specified by the -f option.
 */

#include <sys/event.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#include <err.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct sink_softc {
	struct sockaddr_storage ss;
	enum { SINK_TCP, SINK_UDP } type;
	int nfibs;
	int kq;
	int *fds;
};

static void _Noreturn
usage(void)
{
	fprintf(stderr,
	    "usage: sink [-au] [-f <fib>] [<listen addr>] <listen port>\n");
	exit(1);
}

static void
check_multibind(struct sink_softc *sc)
{
	const char *sysctl;
	size_t len;
	int error, val;

	sysctl = sc->type == SINK_TCP ? "net.inet.tcp.bind_all_fibs" :
	    "net.inet.udp.bind_all_fibs";
	len = sizeof(val);
	error = sysctlbyname(sysctl, &val, &len, NULL, 0);
	if (error != 0)
		err(1, "sysctlbyname(%s)", sysctl);
	if (val != 0)
		errx(1, "multibind is disabled, set %s=0 to enable", sysctl);
}

static void
addrinfo(struct sink_softc *sc, const char *addr, int port)
{
	struct addrinfo hints, *res, *res1;
	char portstr[8];
	int error;

	memset(&sc->ss, 0, sizeof(sc->ss));

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = sc->type == SINK_TCP ? SOCK_STREAM : SOCK_DGRAM;
	snprintf(portstr, sizeof(portstr), "%d", port);
	error = getaddrinfo(addr, portstr, &hints, &res);
	if (error != 0)
		errx(1, "%s", gai_strerror(error));
	for (res1 = res; res != NULL; res = res->ai_next) {
		if ((res->ai_protocol == IPPROTO_TCP && sc->type == SINK_TCP) ||
		    (res->ai_protocol == IPPROTO_UDP && sc->type == SINK_UDP)) {
			memcpy(&sc->ss, res->ai_addr, res->ai_addrlen);
			break;
		}
	}
	if (res == NULL) {
		errx(1, "no %s address found for '%s'",
		    sc->type == SINK_TCP ? "TCP" : "UDP", addr);
	}
	freeaddrinfo(res1);
}

int
main(int argc, char **argv)
{
	struct sink_softc sc;
	const char *laddr;
	int ch, error, fib, lport;
	bool all;

	all = false;
	sc.type = SINK_TCP;
	fib = -1;
	while ((ch = getopt(argc, argv, "af:u")) != -1) {
		switch (ch) {
		case 'a':
			all = true;
			break;
		case 'f':
			fib = atoi(optarg);
			break;
		case 'u':
			sc.type = SINK_UDP;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (all && fib != -1)
		errx(1, "-a and -f are mutually exclusive");
	if (fib == -1) {
		size_t len;

		error = sysctlbyname("net.my_fibnum", &fib, &len, NULL, 0);
		if (error != 0)
			err(1, "sysctlbyname(net.my_fibnum)");
	}

	if (argc == 2) {
		laddr = argv[0];
		lport = atoi(argv[1]);
	} else if (argc == 1) {
		laddr = NULL;
		lport = atoi(argv[0]);
	} else {
		usage();
	}
	addrinfo(&sc, laddr, lport);

	check_multibind(&sc);

	sc.kq = kqueue();
	if (sc.kq == -1)
		err(1, "kqueue");

	if (all) {
		size_t len;

		len = sizeof(sc.nfibs);
		error = sysctlbyname("net.fibs", &sc.nfibs, &len, NULL, 0);
		if (error != 0)
			err(1, "sysctlbyname(net.fibs)");
	} else {
		sc.nfibs = 1;
	}

	sc.fds = calloc(sc.nfibs, sizeof(int));
	if (sc.fds == NULL)
		err(1, "calloc");
	for (int i = 0; i < sc.nfibs; i++) {
		struct kevent kev;
		int s;

		if (sc.type == SINK_TCP)
			s = socket(sc.ss.ss_family, SOCK_STREAM, 0);
		else
			s = socket(sc.ss.ss_family, SOCK_DGRAM, 0);
		if (s == -1)
			err(1, "socket");
		error = setsockopt(s, SOL_SOCKET, SO_SETFIB,
		    all ? &i : &fib, sizeof(int));
		if (error != 0)
			err(1, "setsockopt(SO_SETFIB)");

		error = bind(s, (struct sockaddr *)&sc.ss, sc.ss.ss_len);
		if (error != 0)
			err(1, "bind");

		if (sc.type == SINK_TCP) {
			error = listen(s, 5);
			if (error != 0)
				err(1, "listen");
		}

		EV_SET(&kev, s, EVFILT_READ, EV_ADD, 0, 0, NULL);
		error = kevent(sc.kq, &kev, 1, NULL, 0, NULL);
		if (error != 0)
			err(1, "kevent");

		sc.fds[i] = s;
	}

	for (;;) {
		struct kevent kev;
		socklen_t optlen;
		int n;

		n = kevent(sc.kq, NULL, 0, &kev, 1, NULL);
		if (n == -1)
			err(1, "kevent");
		if (n == 0)
			continue;

		optlen = sizeof(fib);
		error = getsockopt((int)kev.ident, SOL_SOCKET, SO_FIB,
		    &fib, &optlen);
		if (error == -1)
			err(1, "getsockopt(SO_FIB)");

		if (sc.type == SINK_TCP) {
			int cs;

			printf("accepting connection from FIB %d\n", fib);

			cs = accept((int)kev.ident, NULL, NULL);
			if (cs == -1)
				err(1, "accept");
			close(cs);
		} else {
			char buf[1024];
			ssize_t nb;

			printf("receiving datagram from FIB %d\n", fib);

			nb = recvfrom((int)kev.ident, buf, sizeof(buf), 0,
			    NULL, NULL);
			if (nb == -1)
				err(1, "recvfrom");
		}
	}

	return (0);
}
