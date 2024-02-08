/*-
 * Copyright (c) 2024 Lexi Winter
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/nv.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/icmp6.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/udplite.h>
#include <netinet/tcp.h>
#include <netinet/sctp.h>

#include <capsicum_helpers.h>
#include <libcasper.h>
#include <casper/cap_net.h>

#include <netdb.h>
#include <err.h>
#include <sysexits.h>
#include <assert.h>

#include "traceroute.h"

// return a formatted string representing the given address.
char const *
str_ss(struct sockaddr const *addr) {
	assert(addr);

	static char buf[NI_MAXHOST];

	int error = cap_getnameinfo(capnet, addr, addr->sa_len,
				    buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);

	if (error != 0)
		errx(EX_NOHOST, "getnameinfo(): %s", gai_strerror(error));

	return buf;
}

void setup_cap(struct context *ctx) {
	cap_channel_t *casper = cap_init();
	if (casper == NULL)
		err(EX_OSERR, "cap_init");

	caph_cache_catpages();

	if (caph_enter_casper() == -1)
		err(EX_OSERR, "caph_enter");

	ctx->capnet = cap_service_open(casper, "system.net");
	if (ctx->capnet == NULL)
		errx(EX_OSERR, "cap_service_open(system.net)");

	cap_close(casper);

	cap_net_limit_t *limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	if (limit == NULL)
		err(EX_OSERR, "cap_net_limit_init");

	int cap_dns_families[] = { AF_INET, AF_INET6 };
	cap_net_limit_name2addr_family(limit, cap_dns_families,
		     sizeof(cap_dns_families) / sizeof(*cap_dns_families));

	if (cap_net_limit(limit) == -1)
		err(EX_OSERR, "cap_net_limit");
}

//
// protocol definitions
//

static protocol_t protocols[] = {
	{
		.protocol_number = IPPROTO_ICMPV6,
		.min_pktlen = sizeof(struct icmp6_hdr),
		.pkt_make = pkt_make_icmp,
		._socket4 = -1, ._socket6 = -1,
	},
	{
		.protocol_number = IPPROTO_ICMP,
		.min_pktlen = sizeof(struct icmp),
		.pkt_make = pkt_make_icmp,
		._socket4 = -1, ._socket6 = -1,
	},
	{
		.protocol_number = IPPROTO_UDP,
		.min_pktlen = sizeof(struct udphdr),
		.pkt_make = pkt_make_udp,
		._socket4 = -1, ._socket6 = -1,
	},
	{
		.protocol_number = IPPROTO_UDPLITE,
		.min_pktlen = sizeof(struct udplitehdr),
		.pkt_make = pkt_make_udplite,
		._socket4 = -1, ._socket6 = -1,
	},
	{
		.protocol_number = IPPROTO_TCP,
		.min_pktlen = sizeof(struct tcphdr),
		.pkt_make = pkt_make_tcp,
		._socket4 = -1, ._socket6 = -1,
	},
	{
		.protocol_number = IPPROTO_SCTP,
		.min_pktlen = sizeof(struct sctphdr),
		.pkt_make = pkt_make_sctp,
		._socket4 = -1, ._socket6 = -1,
	},
};

void
protocol_init(void) {
	// for each protocol, pre-create a socket for each address family; this
	// allows main() to drop privileges earlier.
	
	for (size_t i = 0; i < sizeof(protocols) / sizeof(*protocols); ++i) {
		protocol_t *p = &protocols[i];

		p->_socket6 = socket(AF_INET6, SOCK_RAW | SOCK_CLOEXEC,
				     p->protocol_number);
		if (p->_socket6 == -1)
			p->_socket6 = -errno;

		p->_socket4 = socket(AF_INET, SOCK_RAW | SOCK_CLOEXEC,
				     p->protocol_number);
		if (p->_socket4 == -1)
			p->_socket4 = -errno;
	}
}

protocol_t const *
protocol_find(int protocol_number) {
	for (size_t i = 0; i < sizeof(protocols) / sizeof(*protocols); ++i) {
		protocol_t *p = &protocols[i];

		if (p->protocol_number == protocol_number)
			return p;
	}

	return NULL;
}

int
protocol_get_socket(protocol_t const *p, int family) {
	int ret_socket = -1, ret_errno = 0;

	for (size_t i = 0; i < sizeof(protocols) / sizeof(*protocols); ++i) {
		protocol_t *defn = &protocols[i];

		if (defn != p) {
			if (defn->_socket4 >= 0) {
				(void)close(defn->_socket4);
				defn->_socket4 = -1;
			}

			if (defn->_socket6 >= 0) {
				(void)close(defn->_socket6);
				defn->_socket6 = -1;
			}

			continue;
		}

		switch (family) {
		case AF_INET6:
			if (defn->_socket4 >= 0) {
				(void)close(defn->_socket4);
				defn->_socket4 = -1;
			}

			if (defn->_socket6 >= 0)
				ret_socket = defn->_socket6;
			else
				ret_errno = -defn->_socket6;
			break;

		case AF_INET:
			if (defn->_socket6 >= 0) {
				(void)close(defn->_socket6);
				defn->_socket6 = -1;
			}

			if (defn->_socket4 >= 0)
				ret_socket = defn->_socket4;
			else
				ret_errno = -defn->_socket4;
			break;

		default:
			abort();
		}
	}

	errno = ret_errno;
	return ret_socket;
}

double
deltaT(struct timeval const *a, struct timeval const *b) {
	struct timeval diff;
	timersub(b, a, &diff);

	double ddiff = (double)diff.tv_sec
			+ ((double)diff.tv_usec) / 1000;
	return (ddiff);
}
