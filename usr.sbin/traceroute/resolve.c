/*-
 * Copyright (c) 2024 Lexi Winter
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>

#include "traceroute.h"

//
// set the destination address and hostname.
//
void
set_destination(struct context *ctx, char const *hostname, int af_hint) {
	struct addrinfo hints, *addrs;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af_hint;
	hints.ai_flags = AI_ADDRCONFIG;
	// these values are not actually used, we just set them to avoid
	// getting duplicate addresses.
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	int ai_error = cap_getaddrinfo(capnet, hostname, NULL, &hints, &addrs);
	if (ai_error != 0)
		errx(EX_NOHOST, "%s: %s", hostname, gai_strerror(ai_error));

	/* If we asked for AF_UNSPEC, skip any unsupported address families. */
	struct addrinfo *addr;
	for (addr = addrs; addr; addr = addr->ai_next)
		if (addr->ai_family == AF_INET || addr->ai_family == AF_INET6)
			break;

	if (!addr)
		errx(EX_NOHOST, "%s: no addresses", hostname);

	ctx->destination = calloc(1, addr->ai_addrlen);
	if (!ctx->destination)
		err(EX_OSERR, "calloc");

	memcpy(ctx->destination, addr->ai_addr, addr->ai_addrlen);
	if (addr->ai_canonname) {
		free(ctx->options->destination_hostname);
		ctx->options->destination_hostname = strdup(addr->ai_canonname);
		if (!ctx->options->destination_hostname)
			err(EX_OSERR, "strdup");
	}

	if (addrs->ai_next)
		warnx("%s has multiple addresses; using %s",
		      hostname, str_ss(ctx->destination));

	freeaddrinfo(addrs);
}

//
// print a hop's hostname and IP address.
//
void print_hop(struct context *ctx, struct sockaddr const *_addr) {
	// copy the address in case we want to modify it
	struct sockaddr_storage addr;
	memcpy(&addr, _addr, _addr->sa_len);

	// if this is an IPv6-compat address (::1.2.3.4) or IPv6-mapped address
	// (::ffff:1.2.3.4), convert it into an IPv4 address.  this is easier
	// for the user to understand, and allows DNS lookups to work for
	// packets going through a NAT64 gateway.
	if (addr.ss_family == AF_INET6) {
		struct sockaddr_in6 addr6;
		memcpy(&addr6, &addr, sizeof(addr6));

		if (IN6_IS_ADDR_V4COMPAT(&addr6.sin6_addr)
		    || IN6_IS_ADDR_V4MAPPED(&addr6.sin6_addr)) {
			struct sockaddr_in addr4;
			memset(&addr4, 0, sizeof(addr4));
			addr4.sin_family = AF_INET;
			addr4.sin_len = sizeof(addr4);
			addr4.sin_addr.s_addr = 
				((uint32_t)addr6.sin6_addr.s6_addr[16 - 1] << 24U)
				| ((uint32_t)addr6.sin6_addr.s6_addr[16 - 2] << 16U)
				| ((uint32_t)addr6.sin6_addr.s6_addr[16 - 3] << 8U)
				| ((uint32_t)addr6.sin6_addr.s6_addr[16 - 4] << 0U);
			memcpy(&addr, &addr4, sizeof(addr4));
		}
	}

	// format the IP address first
	char ipaddr[NI_MAXHOST];

	int ai_error = cap_getnameinfo(ctx->capnet, (struct sockaddr *)&addr,
				       addr.ss_len, ipaddr, sizeof(ipaddr),
				       NULL, 0, NI_NUMERICHOST);
	if (ai_error)
		// this cannot fail
		errx(EX_OSERR, "cap_getnameinfo: %s", gai_strerror(ai_error));

	if (ctx->asndb)
		printf(" [AS%u]", as_lookup(ctx->asndb, ipaddr,
					    addr.ss_family));

	// if -n, just print the address and stop
	if (ctx->options->no_dns) {
		(void)printf(" %s", ipaddr);
		return;
	}

	// resolve the hostname
	char hostname[NI_MAXHOST];

	ai_error = cap_getnameinfo(ctx->capnet, (struct sockaddr *)&addr,
				   addr.ss_len, hostname, sizeof(hostname),
				   NULL, 0, NI_NAMEREQD | NI_NOFQDN);
	if (ai_error) {
		(void)printf(" %s", ipaddr);
		return;
	}

	(void)printf(" %s (%s)", hostname, ipaddr);
}
