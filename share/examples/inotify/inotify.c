/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Klara, Inc.
 */

/*
 * A simple program to demonstrate inotify.  Given one or more paths, it watches
 * all events on those paths and prints them to standard output.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/inotify.h>

#include <assert.h>
#include <err.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libxo/xo.h>

static void
usage(void)
{
	xo_errx(1, "usage: inotify <path1> [<path2> ...]");
}

static const char *
ev2str(uint32_t event)
{
	switch (event & IN_ALL_EVENTS) {
	case IN_ACCESS:
		return ("IN_ACCESS");
	case IN_ATTRIB:
		return ("IN_ATTRIB");
	case IN_CLOSE_WRITE:
		return ("IN_CLOSE_WRITE");
	case IN_CLOSE_NOWRITE:
		return ("IN_CLOSE_NOWRITE");
	case IN_CREATE:
		return ("IN_CREATE");
	case IN_DELETE:
		return ("IN_DELETE");
	case IN_DELETE_SELF:
		return ("IN_DELETE_SELF");
	case IN_MODIFY:
		return ("IN_MODIFY");
	case IN_MOVE_SELF:
		return ("IN_MOVE_SELF");
	case IN_MOVED_FROM:
		return ("IN_MOVED_FROM");
	case IN_MOVED_TO:
		return ("IN_MOVED_TO");
	case IN_OPEN:
		return ("IN_OPEN");
	default:
		switch (event) {
		case IN_IGNORED:
			return ("IN_IGNORED");
		case IN_Q_OVERFLOW:
			return ("IN_Q_OVERFLOW");
		case IN_UNMOUNT:
			return ("IN_UNMOUNT");
		}
		warnx("unknown event %#x", event);
		assert(0);
	}
}

static void
set_handler(int kq, int sig)
{
	struct kevent kev;

	(void)signal(sig, SIG_IGN);
	EV_SET(&kev, sig, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &kev, 1, NULL, 0, NULL) < 0)
		xo_err(1, "kevent");
}

int
main(int argc, char **argv)
{
	struct inotify_event *iev, *iev1;
	struct kevent kev;
	size_t ievsz;
	int ifd, kq;

	argc = xo_parse_args(argc, argv);
	if (argc < 2)
		usage();
	argc--;
	argv++;

	ifd = inotify_init1(IN_NONBLOCK);
	if (ifd < 0)
		xo_err(1, "inotify");
	for (int i = 0; i < argc; i++) {
		int wd;

		wd = inotify_add_watch(ifd, argv[i], IN_ALL_EVENTS);
		if (wd < 0)
			xo_err(1, "inotify_add_watch(%s)", argv[i]);
	}

	xo_set_version("1");
	xo_open_list("events");

	kq = kqueue();
	if (kq < 0)
		xo_err(1, "kqueue");

	/*
	 * Handle signals in the event loop so that we can close the xo list.
	 */
	set_handler(kq, SIGINT);
	set_handler(kq, SIGTERM);
	set_handler(kq, SIGHUP);
	set_handler(kq, SIGQUIT);

	/*
	 * Monitor the inotify descriptor for events.
	 */
	EV_SET(&kev, ifd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &kev, 1, NULL, 0, NULL) < 0)
		xo_err(1, "kevent");

	ievsz = sizeof(*iev) + NAME_MAX + 1;
	iev = malloc(ievsz);
	if (iev == NULL)
		err(1, "malloc");

	for (;;) {
		ssize_t n;
		const char *ev;

		if (kevent(kq, NULL, 0, &kev, 1, NULL) < 0)
			xo_err(1, "kevent");
		if (kev.filter == EVFILT_SIGNAL)
			break;

		n = read(ifd, iev, ievsz);
		if (n < 0)
			xo_err(1, "read");
		assert(n >= (ssize_t)sizeof(*iev));

		for (iev1 = iev; n > 0;) {
			assert(n >= (ssize_t)sizeof(*iev1));

			ev = ev2str(iev1->mask);
			xo_open_instance("event");
			xo_emit("{:wd/%3d} {:event/%16s} {:name/%s}\n",
			    iev1->wd, ev, iev1->len > 0 ? iev1->name : "");
			xo_close_instance("event");

			n -= sizeof(*iev1) + iev1->len;
			iev1 = (struct inotify_event *)(void *)
			    ((char *)iev1 + sizeof(*iev1) + iev1->len);
		}
		(void)xo_flush();
	}

	xo_close_list("events");

	if (xo_finish() < 0)
		xo_err(1, "stdout");
	exit(0);
}
