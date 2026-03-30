/*
 * Copyright (c) 2026 Stormshield
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * A dead-simple IPv6 multicast routing daemon.  It registers itself with the
 * multicast routing code and then waits for messages from the kernel.  Received
 * messages are handled by installing multicast routes.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet6/ip6_mroute.h>
#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct mif {
	const char *name;
	int mifi;
	int pifi;
	STAILQ_ENTRY(mif) next;
};
static STAILQ_HEAD(, mif) miflist = STAILQ_HEAD_INITIALIZER(miflist);

static void *
xmalloc(size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (ptr == NULL)
		err(1, "malloc");
	return (ptr);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-i <iface>] [-m <srcaddr>/<groupaddr>/<iface>]\n",
	    getprogname());
	exit(1);
}

static void
add_route(int sd, const struct in6_addr *src, const struct in6_addr *group,
    mifi_t mifi)
{
	struct mf6cctl mfcc;
	struct mif *mif;
	int error;

	memset(&mfcc, 0, sizeof(mfcc));
	mfcc.mf6cc_parent = mifi;
	mfcc.mf6cc_origin.sin6_family = AF_INET6;
	mfcc.mf6cc_origin.sin6_len = sizeof(struct sockaddr_in6);
	mfcc.mf6cc_origin.sin6_addr = *src;
	mfcc.mf6cc_mcastgrp.sin6_family = AF_INET6;
	mfcc.mf6cc_mcastgrp.sin6_len = sizeof(struct sockaddr_in6);
	mfcc.mf6cc_mcastgrp.sin6_addr = *group;

	STAILQ_FOREACH(mif, &miflist, next) {
		if (mif->mifi != mifi)
			IF_SET(mif->mifi, &mfcc.mf6cc_ifset);
	}

	error = setsockopt(sd, IPPROTO_IPV6, MRT6_ADD_MFC,
	    &mfcc, sizeof(mfcc));
	if (error != 0)
		err(1, "setsockopt(MRT6_ADD_MFC)");
}

static void
handle_upcalls(int sd)
{
	struct kevent ev;
	int kq;

	kq = kqueue();
	if (kq < 0)
		err(1, "kqueue");
	EV_SET(&ev, sd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
	if (kevent(kq, &ev, 1, NULL, 0, NULL) < 0)
		err(1, "kevent");

	for (;;) {
		char buf1[INET6_ADDRSTRLEN], buf2[INET6_ADDRSTRLEN];
		struct mrt6msg msg;
		ssize_t len;
		int n;

		n = kevent(kq, NULL, 0, &ev, 1, NULL);
		if (n < 0) {
			if (errno == EINTR)
				break;
			err(1, "kevent");
		}
		if (n == 0)
			continue;
		assert(n == 1);
		assert(ev.filter == EVFILT_READ);

		len = recv(sd, &msg, sizeof(msg), 0);
		if (len < 0)
			err(1, "recv");
		if ((size_t)len < sizeof(msg)) {
			warnx("short read on upcall, %zd bytes", len);
			continue;
		}

		printf("upcall received:\n");
		printf("msgtype=%d mif=%d src=%s dst=%s\n",
		    msg.im6_msgtype, msg.im6_mif,
		    inet_ntop(AF_INET6, &msg.im6_src, buf1, sizeof(buf1)),
		    inet_ntop(AF_INET6, &msg.im6_dst, buf2, sizeof(buf2)));

		add_route(sd, &msg.im6_src, &msg.im6_dst, msg.im6_mif);
	}

	close(kq);
}

int
main(int argc, char **argv)
{
	struct mif *mif;
	int ch, error, mifi, sd, v;

	mifi = 0;
	while ((ch = getopt(argc, argv, "i:m:")) != -1) {
		switch (ch) {
		case 'i':
			mif = xmalloc(sizeof(*mif));
			mif->name = strdup(optarg);
			mif->mifi = mifi++;
			mif->pifi = if_nametoindex(optarg);
			if (mif->pifi == 0)
				errx(1, "unknown interface %s", optarg);
			STAILQ_INSERT_TAIL(&miflist, mif, next);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	sd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (sd < 0)
		err(1, "socket");

	v = 1;
	error = setsockopt(sd, IPPROTO_IPV6, MRT6_INIT, &v, sizeof(v));
	if (error != 0)
		err(1, "setsockopt(MRT6_INIT)");

	STAILQ_FOREACH(mif, &miflist, next) {
		struct mif6ctl mifc;

		mifc.mif6c_mifi = mif->mifi;
		mifc.mif6c_pifi = mif->pifi;
		mifc.mif6c_flags = 0;
		error = setsockopt(sd, IPPROTO_IPV6, MRT6_ADD_MIF,
		    &mifc, sizeof(mifc));
		if (error != 0)
			err(1, "setsockopt(MRT6_ADD_MIF) on %s", mif->name);
	}

	handle_upcalls(sd);

	error = setsockopt(sd, IPPROTO_IPV6, MRT6_DONE, NULL, 0);
	if (error != 0)
		err(1, "setsockopt(MRT6_DONE)");

	return (0);
}
