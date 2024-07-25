/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/event.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <libnvmf.h>
#include <libutil.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

bool data_digests = false;
bool header_digests = false;
bool flow_control_disable = false;
bool kernel_io = false;

static const char *subnqn;
static volatile bool quit = false;

static void
usage(void)
{
	fprintf(stderr, "nvmfd -K [-dFGg] [-P port] [-p port] [-t transport] [-n subnqn]\n"
	    "nvmfd [-dFGg] [-P port] [-p port] [-t transport] [-n subnqn]\n"
	    "\tdevice [device [...]]\n"
	    "\n"
	    "Devices use one of the following syntaxes:\n"
	    "\tpathame      - file or disk device\n"
	    "\tramdisk:size - memory disk of given size\n");
	exit(1);
}

static void
handle_sig(int sig __unused)
{
	quit = true;
}

static void
register_listen_socket(int kqfd, int s, void *udata)
{
	struct kevent kev;

	if (listen(s, -1) != 0)
		err(1, "listen");

	EV_SET(&kev, s, EVFILT_READ, EV_ADD, 0, 0, udata);
	if (kevent(kqfd, &kev, 1, NULL, 0, NULL) == -1)
		err(1, "kevent: failed to add listen socket");
}

static void
create_passive_sockets(int kqfd, const char *port, bool discovery)
{
	struct addrinfo hints, *ai, *list;
	bool created;
	int error, s;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo(NULL, port, &hints, &list);
	if (error != 0)
		errx(1, "%s", gai_strerror(error));
	created = false;

	for (ai = list; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s == -1)
			continue;

		if (bind(s, ai->ai_addr, ai->ai_addrlen) != 0) {
			close(s);
			continue;
		}

		if (discovery) {
			register_listen_socket(kqfd, s, (void *)1);
		} else {
			register_listen_socket(kqfd, s, (void *)2);
			discovery_add_io_controller(s, subnqn);
		}
		created = true;
	}

	freeaddrinfo(list);
	if (!created)
		err(1, "Failed to create any listen sockets");
}

static void
handle_connections(int kqfd)
{
	struct kevent ev;
	int s;

	signal(SIGHUP, handle_sig);
	signal(SIGINT, handle_sig);
	signal(SIGQUIT, handle_sig);
	signal(SIGTERM, handle_sig);

	while (!quit) {
		if (kevent(kqfd, NULL, 0, &ev, 1, NULL) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "kevent");
		}

		assert(ev.filter == EVFILT_READ);

		s = accept(ev.ident, NULL, NULL);
		if (s == -1) {
			warn("accept");
			continue;
		}

		switch ((uintptr_t)ev.udata) {
		case 1:
			handle_discovery_socket(s);
			break;
		case 2:
			handle_io_socket(s);
			break;
		default:
			__builtin_unreachable();
		}
	}
}

int
main(int ac, char **av)
{
	struct pidfh *pfh;
	const char *dport, *ioport, *transport;
	pid_t pid;
	int ch, error, kqfd;
	bool daemonize;
	static char nqn[NVMF_NQN_MAX_LEN];

	/* 7.4.9.3 Default port for discovery */
	dport = "8009";

	pfh = NULL;
	daemonize = true;
	ioport = "0";
	subnqn = NULL;
	transport = "tcp";
	while ((ch = getopt(ac, av, "dFgGKn:P:p:t:")) != -1) {
		switch (ch) {
		case 'd':
			daemonize = false;
			break;
		case 'F':
			flow_control_disable = true;
			break;
		case 'G':
			data_digests = true;
			break;
		case 'g':
			header_digests = true;
			break;
		case 'K':
			kernel_io = true;
			break;
		case 'n':
			subnqn = optarg;
			break;
		case 'P':
			dport = optarg;
			break;
		case 'p':
			ioport = optarg;
			break;
		case 't':
			transport = optarg;
			break;
		default:
			usage();
		}
	}

	av += optind;
	ac -= optind;

	if (kernel_io) {
		if (ac > 0)
			usage();
		if (modfind("nvmft") == -1 && kldload("nvmft") == -1)
			warn("couldn't load nvmft");
	} else {
		if (ac < 1)
			usage();
	}

	if (strcasecmp(transport, "tcp") == 0) {
	} else
		errx(1, "Invalid transport %s", transport);

	if (subnqn == NULL) {
		error = nvmf_nqn_from_hostuuid(nqn);
		if (error != 0)
			errc(1, error, "Failed to generate NQN");
		subnqn = nqn;
	}

	if (!kernel_io)
		register_devices(ac, av);

	init_discovery();
	init_io(subnqn);

	if (daemonize) {
		pfh = pidfile_open(NULL, 0600, &pid);
		if (pfh == NULL) {
			if (errno == EEXIST)
				errx(1, "Daemon already running, pid: %jd",
				    (intmax_t)pid);
			warn("Cannot open or create pidfile");
		}

		if (daemon(0, 0) != 0) {
			pidfile_remove(pfh);
			err(1, "Failed to fork into the background");
		}

		pidfile_write(pfh);
	}

	kqfd = kqueue();
	if (kqfd == -1) {
		pidfile_remove(pfh);
		err(1, "kqueue");
	}

	create_passive_sockets(kqfd, dport, true);
	create_passive_sockets(kqfd, ioport, false);

	handle_connections(kqfd);
	shutdown_io();
	if (pfh != NULL)
		pidfile_remove(pfh);
	return (0);
}
