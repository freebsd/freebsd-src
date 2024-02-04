/*-
 * Copyright (c) 2024 Lexi Winter
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <inttypes.h>
#include <paths.h>
#include <sysexits.h>
#include <netdb.h>
#include <capsicum_helpers.h>

#include "traceroute.h"

// make sure the stdio fds are open
static void
check_fds(void) {
	for (int i = 0; i < 3; ++i) {
		int fd = open(_PATH_DEVNULL, O_RDWR);
		if (fd == -1)
			exit(1); // report error?
		if (fd > 2) {
			close(fd);
			return;
		}
		// leak the fd, this is fine
	}
}

// add source-routing options

static void
set_sourceroute(struct context *ctx, struct sockaddr_storage *gateways,
		size_t ngateways) {
	uint8_t *optbuf;
	// type +  length + pointer + padding + data
	size_t optlen = 4 + (ngateways * sizeof(struct in_addr));
	optbuf = calloc(1, optlen);
	if (optbuf == NULL)
		err(EX_OSERR, "calloc");

	if (optlen > UINT8_MAX)
		errx(EX_USAGE, "too many gateways");

	optbuf[0] = IPOPT_NOP;
	optbuf[1] = IPOPT_LSRR; // option type
	optbuf[2] = optlen - 1;	// option length
	optbuf[3] = 4;		// pointer; must be >= 4 (RFC 791)

	for (size_t i = 0; i < ngateways; ++i) {
		if (gateways[i].ss_family != AF_INET)
			// should be impossible
			errx(EX_SOFTWARE, "set_sourcewroute: wrong AF?!");

		struct sockaddr_in sin;
		memcpy(&sin, &gateways[i], sizeof(sin));
		memcpy(optbuf + 4 + (4*i), &sin.sin_addr.s_addr,
		       sizeof(sin.sin_addr.s_addr));
	}

	if (setsockopt(ctx->sendsock, IPPROTO_IP, IP_OPTIONS,
		       optbuf, optlen) == -1)
		err(EX_OSERR, "setsockopt(IP_OPTIONS)");
}

static void
set_rthdr(struct context *ctx) {
	// if no -g option, nothing to do
	if (ctx->options->ngateways == 0)
		return;

	// resolve all the gateways
	struct sockaddr_storage *gateways;
	gateways = calloc(ctx->options->ngateways,
			  sizeof(struct sockaddr_storage));
	if (gateways == NULL)
		err(EX_OSERR, "calloc");

	for (unsigned i = 0; i < ctx->options->ngateways; ++i) {
		struct addrinfo hints, *addrs;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = ctx->destination->sa_family;
		hints.ai_flags = AI_ADDRCONFIG;

		int ai_error = cap_getaddrinfo(
			ctx->capnet, ctx->options->gateways[i], NULL,
			&hints, &addrs);
		if (ai_error != 0)
			errx(EX_NOHOST, "cannot resolve gateway %s: %s",
			     ctx->options->gateways[i],
			     gai_strerror(ai_error));

		memcpy(&gateways[i], addrs->ai_addr, addrs->ai_addrlen);
		freeaddrinfo(addrs);
	}

	switch (ctx->destination->sa_family) {
	case AF_INET6:
		errx(EX_USAGE, "source routing is not supported for IPv6");
		break;
	case AF_INET:
		set_sourceroute(ctx, gateways, ctx->options->ngateways);
		break;
	default:
		abort();
	}

	free(gateways);
}

int main(int argc, char **argv) {
	check_fds();

	// must be called before setuid(), because it creates our sockets
	protocol_init();

	// receiving socket for ICMP errors.  we create both the IPv4 and IPv6
	// sockets here, then later close whichever one we don't want.
	int rcvsock4 = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (rcvsock4 == -1)
		rcvsock4 = -errno;

	int rcvsock6 = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (rcvsock6 == -1)
		rcvsock6 = -errno;

	// drop privileges

	if (setuid(getuid()) != 0)
		err(EX_OSERR, "setuid");

	// set progname
	if (argv[0] == NULL)
		setprogname("traceroute");
	else
		setprogname(argv[0]);

	struct context *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		err(EX_OSERR, "calloc");

	// enter capability mode
	setup_cap(ctx);

	// parse command line options
	ctx->options = parse_options(&argc, argv);

	// set our ident, used as source port with some protocols.  using 7FFF
	// as the mask ensures we have some room to increment the port if
	// needed.
	ctx->ident = htons(getpid() & 0x7fffU);

	// set source and destination addresses in context
	set_destination(ctx, ctx->options->destination_hostname,
			ctx->options->family);
	set_source(ctx, ctx->options->source_hostname,
		   ctx->options->interface);

	// Special case: if protocol is IPPROTO_ICMP, use IPPROTO_ICMPV6 in the
	// IPv6 case.
	if (ctx->options->protocol == IPPROTO_ICMP
	    && ctx->destination->sa_family == AF_INET6)
		ctx->options->protocol = IPPROTO_ICMPV6;

	// fetch the protocol definition
	ctx->protocol = protocol_find(ctx->options->protocol);
	if (ctx->protocol == NULL)
		errx(EX_OSERR, "unknown protocol %d", ctx->options->protocol);

	// check minimum packet length
	if (ctx->options->packetlen < ctx->protocol->min_pktlen)
		ctx->options->packetlen = ctx->protocol->min_pktlen;

	ctx->sendsock = protocol_get_socket(ctx->protocol,
					    ctx->destination->sa_family);
	if (ctx->sendsock == -1)
		err(EX_OSERR, "socket");

	// set source-routing headers
	set_rthdr(ctx);

	// set our receiving socket for ICMP errors.
	if (ctx->destination->sa_family == AF_INET) {
		ctx->rcvsock = rcvsock4;
		if (rcvsock6 >= 0)
			close(rcvsock6);
	} else {
		ctx->rcvsock = rcvsock6;
		if (rcvsock4 >= 0)
			close(rcvsock4);
	}

	if (ctx->rcvsock < 0)
		errx(EX_OSERR, "socket(rcvsock): %s", strerror(-ctx->rcvsock));

	// Bind and connect the sending socket.

	if (bind(ctx->sendsock, ctx->source, ctx->source->sa_len) == -1)
		err(EX_OSERR, "bind");

	if (connect(ctx->sendsock, ctx->destination,
		    ctx->destination->sa_len) != 0)
		err(EX_OSERR, "connect");

	// XXX: move this to setup_cap()
	cap_rights_t rights;

	cap_rights_init(&rights, CAP_SEND, CAP_SETSOCKOPT);
	if (cap_rights_limit(ctx->sendsock, &rights) < 0)
		err(EX_OSERR, "cap_rights_limit sndsock");

	cap_rights_init(&rights, CAP_RECV, CAP_SETSOCKOPT, CAP_EVENT);
	if (cap_rights_limit(ctx->rcvsock, &rights) < 0)
		err(EX_OSERR, "cap_rights_limit s");
	// Set socket options
	if (ctx->options->socket_debug) {
		int one = 1;
		if (setsockopt(ctx->rcvsock, SOL_SOCKET, SO_DEBUG,
			       &one, sizeof(one)) == -1)
			warn("setsockopt(SO_DEBUG)");
	}

	if (ctx->options->send_direct) {
		int one = 1;
		if (setsockopt(ctx->rcvsock, SOL_SOCKET, SO_DONTROUTE,
			       &one, sizeof(one)) == -1)
			warn("setsockopt(SO_DONTROUTE)");
	}

	//
	// Set max_ttl based on the destination protocol.
	//
	if (ctx->options->max_ttl == 0) {
		int mib[4] = { CTL_NET, 0 };
		switch (ctx->destination->sa_family) {
		case AF_INET:
			mib[1] = PF_INET;
			mib[2] = IPPROTO_IP;
			mib[3] = IPCTL_DEFTTL;
			break;
		case AF_INET6:
			mib[1] = PF_INET6;
			mib[2] = IPPROTO_IPV6;
			mib[3] = IPV6CTL_DEFHLIM;
			break;
		default:
			abort();
		}

		size_t sz = sizeof(ctx->options->max_ttl);

		if (sysctl(mib, 4, &ctx->options->max_ttl, &sz, NULL, 0) == -1)
			err(EX_OSERR, "sysctl()");
	}

	//
	// Initialise the ASN lookup code if requested.
	//
	if (ctx->options->asn_lookups) {
		ctx->asndb = as_setup(ctx->options->asn_lookup_server);
		if (ctx->asndb == NULL)
			warnx("as_setup failed, AS# lookups disabled");
	}

	// Print the header
	(void)fprintf(stderr, "traceroute to %s (%s)",
	    ctx->options->destination_hostname, str_ss(ctx->destination));
	(void)fprintf(stderr, " from %s", str_ss(ctx->source));
	(void)fprintf(stderr, ", %d hops max, %"PRIu16" byte packets\n",
		      ctx->options->max_ttl,
		      ctx->options->packetlen);
	(void)fflush(stderr);

	//
	// Run the appropriate traceroute.
	//
	switch (ctx->destination->sa_family) {
	case AF_INET:
		return traceroute4(ctx);
	case AF_INET6:
		return traceroute6(ctx);
	default:
		errx(EX_SOFTWARE, "unknown address family");
	}
}
