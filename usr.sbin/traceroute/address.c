/*-
 * Copyright (c) 2024 Lexi Winter
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/socket.h>

#include <netinet/in.h>
#include <libcasper.h>
#include <casper/cap_net.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sysexits.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "traceroute.h"

/* system interface list */
static struct ifaddrs *ifaddrs;

//
// create a source address given a generic address.  this sets the source port
// to 0, which is required or else bind() will fail later.
//
static struct sockaddr *
make_source(struct sockaddr *addr) {

	switch (addr->sa_family) {

	case AF_INET: {
		struct sockaddr_in *sin;
		if ((sin = calloc(1, sizeof(*sin))) == NULL)
			err(EX_OSERR, "calloc");
		memcpy(sin, addr, sizeof(*sin));
		sin->sin_port = 0;
		return (struct sockaddr *)sin;
	}

	case AF_INET6: {
		struct sockaddr_in6 *sin6;
		if ((sin6 = calloc(1, sizeof(*sin6))) == NULL)
			err(EX_OSERR, "calloc");
		memcpy(sin6, addr, sizeof(*sin6));
		sin6->sin6_port = 0;
		return (struct sockaddr *)sin6;
	}

	default:
		abort();
	}
}

/*
 * given a destination address, find the default source address.
 */
static struct sockaddr *
get_default_source(struct sockaddr *daddr) {
	struct sockaddr_storage dest;

	memcpy(&dest, daddr, daddr->sa_len);

	int sock = socket(dest.ss_family, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1)
		err(EX_OSERR, "set_default_source: socket()");

	switch (daddr->sa_family) {
	case AF_INET:
		((struct sockaddr_in *)&dest)->sin_port = htons(65535);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)&dest)->sin6_port = htons(65535);
		break;
	default:
		abort();
	}

	if (connect(sock, (struct sockaddr *)&dest, dest.ss_len) == -1)
		err(EX_OSERR, "set_default_source: connect()");

	struct sockaddr_storage sockname;
	socklen_t socklen;
	if (getsockname(sock, (struct sockaddr *)&sockname, &socklen) == -1)
		err(EX_OSERR, "set_default_source: getsockname()");

	(void)close(sock);
	return make_source((struct sockaddr *)&sockname);
}

static struct sockaddr *
get_hostname_source(char const *hostname, int family) {
	struct addrinfo hints, *addrs;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_family = family;

	int ai_error = cap_getaddrinfo(capnet, hostname, NULL, &hints, &addrs);
	if (ai_error != 0)
		errx(EX_NOHOST, "cannot resolve source address %s: %s",
		     hostname, gai_strerror(ai_error));

	struct sockaddr *ret = calloc(1, addrs->ai_addrlen);
	memcpy(ret, addrs->ai_addr, addrs->ai_addrlen);
	freeaddrinfo(addrs);
	return ret;
}

static struct sockaddr *
get_interface_address(char const *device_name,
		      int family,
		      char const *hostname) {
	struct sockaddr *addr = NULL;
	struct addrinfo *addrs = NULL;

	if (hostname) {
		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = family;
		hints.ai_flags = AI_ADDRCONFIG;

		int ai_error = cap_getaddrinfo(capnet, hostname, NULL,
					       &hints, &addrs);
		if (ai_error != 0)
			errx(EX_NOHOST, "cannot resolve source address %s: %s",
			     hostname, gai_strerror(ai_error));
		addr = addrs->ai_addr;
	}

	for (struct ifaddrs *ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != family)
			continue;

		if (strcmp(device_name, ifa->ifa_name) != 0)
			continue;

		if (addr && (memcmp(addr, ifa->ifa_addr, addr->sa_len) != 0))
			continue;

		if (addrs)
			freeaddrinfo(addrs);

		return make_source(ifa->ifa_addr);
	}

	errx(EX_USAGE, "no appropriate address found for interface %s",
	     device_name);
}

/*
 * set the source address.
 */
void
set_source(struct context *ctx, char const *hostname, char const *device) {
	if (getifaddrs(&ifaddrs) == -1)
		err(EX_OSERR, "getifaddrs");

	if (!hostname && !device) {
		// if no source was specified, use the system default.
		ctx->source = get_default_source(ctx->destination);

	} else if (hostname && !device) {
		// if a hostname but not a device was specified, resolve the
		// hostname and use that.
		ctx->source = get_hostname_source(hostname,
						  ctx->destination->sa_family);

	} else if (device) {
		// if a device was specified, use the device's first configured
		// address or the specified hostname.
		ctx->source = get_interface_address(device,
					    ctx->destination->sa_family,
					    hostname);
	}
}
