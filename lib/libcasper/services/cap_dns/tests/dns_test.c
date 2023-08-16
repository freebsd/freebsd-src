/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/capsicum.h>
#include <sys/nv.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcasper.h>
#include <casper/cap_dns.h>

#include <atf-c.h>

#define	GETHOSTBYNAME			0x01
#define	GETHOSTBYNAME2_AF_INET		0x02
#define	GETHOSTBYNAME2_AF_INET6		0x04
#define	GETHOSTBYADDR_AF_INET		0x08
#define	GETHOSTBYADDR_AF_INET6		0x10
#define	GETADDRINFO_AF_UNSPEC		0x20
#define	GETADDRINFO_AF_INET		0x40
#define	GETADDRINFO_AF_INET6		0x80

static bool
addrinfo_compare(struct addrinfo *ai0, struct addrinfo *ai1)
{
	struct addrinfo *at0, *at1;

	if (ai0 == NULL && ai1 == NULL)
		return (true);
	if (ai0 == NULL || ai1 == NULL)
		return (false);

	at0 = ai0;
	at1 = ai1;
	while (true) {
		if ((at0->ai_flags == at1->ai_flags) &&
		    (at0->ai_family == at1->ai_family) &&
		    (at0->ai_socktype == at1->ai_socktype) &&
		    (at0->ai_protocol == at1->ai_protocol) &&
		    (at0->ai_addrlen == at1->ai_addrlen) &&
		    (memcmp(at0->ai_addr, at1->ai_addr,
			at0->ai_addrlen) == 0)) {
			if (at0->ai_canonname != NULL &&
			    at1->ai_canonname != NULL) {
				if (strcmp(at0->ai_canonname,
				    at1->ai_canonname) != 0) {
					return (false);
				}
			}

			if (at0->ai_canonname == NULL &&
			    at1->ai_canonname != NULL) {
				return (false);
			}
			if (at0->ai_canonname != NULL &&
			    at1->ai_canonname == NULL) {
				return (false);
			}

			if (at0->ai_next == NULL && at1->ai_next == NULL)
				return (true);
			if (at0->ai_next == NULL || at1->ai_next == NULL)
				return (false);

			at0 = at0->ai_next;
			at1 = at1->ai_next;
		} else {
			return (false);
		}
	}

	/* NOTREACHED */
	fprintf(stderr, "Dead code reached in addrinfo_compare()\n");
	exit(1);
}

static bool
hostent_aliases_compare(char **aliases0, char **aliases1)
{
	int i0, i1;

	if (aliases0 == NULL && aliases1 == NULL)
		return (true);
	if (aliases0 == NULL || aliases1 == NULL)
		return (false);

	for (i0 = 0; aliases0[i0] != NULL; i0++) {
		for (i1 = 0; aliases1[i1] != NULL; i1++) {
			if (strcmp(aliases0[i0], aliases1[i1]) == 0)
				break;
		}
		if (aliases1[i1] == NULL)
			return (false);
	}

	return (true);
}

static bool
hostent_addr_list_compare(char **addr_list0, char **addr_list1, int length)
{
	int i0, i1;

	if (addr_list0 == NULL && addr_list1 == NULL)
		return (true);
	if (addr_list0 == NULL || addr_list1 == NULL)
		return (false);

	for (i0 = 0; addr_list0[i0] != NULL; i0++) {
		for (i1 = 0; addr_list1[i1] != NULL; i1++) {
			if (memcmp(addr_list0[i0], addr_list1[i1], length) == 0)
				break;
		}
		if (addr_list1[i1] == NULL)
			return (false);
	}

	return (true);
}

static bool
hostent_compare(const struct hostent *hp0, const struct hostent *hp1)
{

	if (hp0 == NULL && hp1 != NULL)
		return (true);

	if (hp0 == NULL || hp1 == NULL)
		return (false);

	if (hp0->h_name != NULL || hp1->h_name != NULL) {
		if (hp0->h_name == NULL || hp1->h_name == NULL)
			return (false);
		if (strcmp(hp0->h_name, hp1->h_name) != 0)
			return (false);
	}

	if (!hostent_aliases_compare(hp0->h_aliases, hp1->h_aliases))
		return (false);
	if (!hostent_aliases_compare(hp1->h_aliases, hp0->h_aliases))
		return (false);

	if (hp0->h_addrtype != hp1->h_addrtype)
		return (false);

	if (hp0->h_length != hp1->h_length)
		return (false);

	if (!hostent_addr_list_compare(hp0->h_addr_list, hp1->h_addr_list,
	    hp0->h_length)) {
		return (false);
	}
	if (!hostent_addr_list_compare(hp1->h_addr_list, hp0->h_addr_list,
	    hp0->h_length)) {
		return (false);
	}

	return (true);
}

static void
runtest(cap_channel_t *capdns, unsigned int expected)
{
	unsigned int result, failure;
	struct addrinfo *ais, *aic, hints, *hintsp;
	struct hostent *hps, *hpc;
	struct in_addr ip4;
	struct in6_addr ip6;
	int caperr, syserr;

	failure = result = 0;

	hps = gethostbyname("example.com");
	if (hps == NULL) {
		failure |= GETHOSTBYNAME;
		fprintf(stderr, "Unable to resolve %s IPv4.\n", "example.com");
	} else {
		hpc = cap_gethostbyname(capdns, "example.com");
		if (hostent_compare(hps, hpc))
			result |= GETHOSTBYNAME;
	}

	hps = gethostbyname2("example.com", AF_INET);
	if (hps == NULL) {
		failure |= GETHOSTBYNAME2_AF_INET;
		fprintf(stderr, "Unable to resolve %s IPv4.\n", "example.com");
	} else {
		hpc = cap_gethostbyname2(capdns, "example.com", AF_INET);
		if (hostent_compare(hps, hpc))
			result |= GETHOSTBYNAME2_AF_INET;
	}

	hps = gethostbyname2("example.com", AF_INET6);
	if (hps == NULL) {
		failure |= GETHOSTBYNAME2_AF_INET6;
		fprintf(stderr, "Unable to resolve %s IPv6.\n", "example.com");
	} else {
		hpc = cap_gethostbyname2(capdns, "example.com", AF_INET6);
		if (hostent_compare(hps, hpc))
			result |= GETHOSTBYNAME2_AF_INET6;
	}

	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = 0;
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;

	hintsp = &hints;

	syserr = getaddrinfo("freebsd.org", "25", hintsp, &ais);
	if (syserr != 0) {
		failure |= GETADDRINFO_AF_UNSPEC;
		fprintf(stderr,
		    "Unable to issue [system] getaddrinfo() for AF_UNSPEC: %s\n",
		    gai_strerror(syserr));
	} else {
		caperr = cap_getaddrinfo(capdns, "freebsd.org", "25", hintsp,
		    &aic);
		if (caperr == 0) {
			if (addrinfo_compare(ais, aic))
				result |= GETADDRINFO_AF_UNSPEC;
			freeaddrinfo(ais);
			freeaddrinfo(aic);
		}
	}

	hints.ai_family = AF_INET;
	syserr = getaddrinfo("freebsd.org", "25", hintsp, &ais);
	if (syserr != 0) {
		failure |= GETADDRINFO_AF_INET;
		fprintf(stderr,
		    "Unable to issue [system] getaddrinfo() for AF_UNSPEC: %s\n",
		    gai_strerror(syserr));
	} else {
		caperr = cap_getaddrinfo(capdns, "freebsd.org", "25", hintsp,
		    &aic);
		if (caperr == 0) {
			if (addrinfo_compare(ais, aic))
				result |= GETADDRINFO_AF_INET;
			freeaddrinfo(ais);
			freeaddrinfo(aic);
		}
	}

	hints.ai_family = AF_INET6;
	syserr = getaddrinfo("freebsd.org", "25", hintsp, &ais);
	if (syserr != 0) {
		failure |= GETADDRINFO_AF_INET6;
		fprintf(stderr,
		    "Unable to issue [system] getaddrinfo() for AF_UNSPEC: %s\n",
		    gai_strerror(syserr));
	} else {
		caperr = cap_getaddrinfo(capdns, "freebsd.org", "25", hintsp,
		    &aic);
		if (caperr == 0) {
			if (addrinfo_compare(ais, aic))
				result |= GETADDRINFO_AF_INET6;
			freeaddrinfo(ais);
			freeaddrinfo(aic);
		}
	}

	/* XXX: hardcoded addresses for "google-public-dns-a.google.com". */
#define	GOOGLE_DNS_IPV4	"8.8.8.8"
#define	GOOGLE_DNS_IPV6	"2001:4860:4860::8888"

	inet_pton(AF_INET, GOOGLE_DNS_IPV4, &ip4);
	hps = gethostbyaddr(&ip4, sizeof(ip4), AF_INET);
	if (hps == NULL) {
		failure |= GETHOSTBYADDR_AF_INET;
		fprintf(stderr, "Unable to resolve %s.\n", GOOGLE_DNS_IPV4);
	} else {
		hpc = cap_gethostbyaddr(capdns, &ip4, sizeof(ip4), AF_INET);
		if (hostent_compare(hps, hpc))
			result |= GETHOSTBYADDR_AF_INET;
	}

	inet_pton(AF_INET6, GOOGLE_DNS_IPV6, &ip6);
	hps = gethostbyaddr(&ip6, sizeof(ip6), AF_INET6);
	if (hps == NULL) {
		failure |= GETHOSTBYADDR_AF_INET6;
		fprintf(stderr, "Unable to resolve %s.\n", GOOGLE_DNS_IPV6);
	} else {
		hpc = cap_gethostbyaddr(capdns, &ip6, sizeof(ip6), AF_INET6);
		if (hostent_compare(hps, hpc)) {
			caperr = h_errno;
			result |= GETHOSTBYADDR_AF_INET6;
		}
	}

	/*
	 * If we had any failures, make sure that all lookups failed.  If some
	 * succeeded and some failed, there's a problem with the test or the DNS
	 * and we should not fail silently.
	 */
	if (failure != 0) {
		ATF_REQUIRE_MSG(failure == (GETHOSTBYNAME |
		    GETHOSTBYNAME2_AF_INET | GETHOSTBYNAME2_AF_INET6 |
		    GETADDRINFO_AF_UNSPEC | GETADDRINFO_AF_INET |
		    GETADDRINFO_AF_INET6 |
		    GETHOSTBYADDR_AF_INET | GETHOSTBYADDR_AF_INET6),
		    "expected all tests to fail, got 0x%x", failure);
		atf_tc_skip(
		    "no name lookups succeeded, tests require Internet access");
	}
	ATF_REQUIRE_MSG(result == expected,
	    "expected 0x%x, got 0x%x", expected, result);
}

static cap_channel_t *
cap_dns_init(void)
{
	cap_channel_t *capcas, *capdns;

	capcas = cap_init();
	ATF_REQUIRE(capcas != NULL);

	capdns = cap_service_open(capcas, "system.dns");
	ATF_REQUIRE(capdns != NULL);

	cap_close(capcas);

	return (capdns);
}

ATF_TC(dns_no_limits);
ATF_TC_HEAD(dns_no_limits, tc)
{
}
ATF_TC_BODY(dns_no_limits, tc)
{
	cap_channel_t *capdns;

	capdns = cap_dns_init();

	runtest(capdns,
	    (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET | GETHOSTBYNAME2_AF_INET6 |
	     GETHOSTBYADDR_AF_INET | GETHOSTBYADDR_AF_INET6 |
	     GETADDRINFO_AF_UNSPEC | GETADDRINFO_AF_INET |
	     GETADDRINFO_AF_INET6));

	cap_close(capdns);
}

ATF_TC(dns_all_limits);
ATF_TC_HEAD(dns_all_limits, tc)
{
}
ATF_TC_BODY(dns_all_limits, tc)
{
	cap_channel_t *capdns;
	const char *types[2];
	int families[2];

	capdns = cap_dns_init();

	types[0] = "NAME2ADDR";
	types[1] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 2) == 0);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, NULL, 0) == -1);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, NULL, 0) == -1);

	runtest(capdns,
	    (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET | GETHOSTBYNAME2_AF_INET6 |
	     GETHOSTBYADDR_AF_INET | GETHOSTBYADDR_AF_INET6 |
	     GETADDRINFO_AF_INET | GETADDRINFO_AF_INET6));

	cap_close(capdns);
}

ATF_TC(dns_name_limit);
ATF_TC_HEAD(dns_name_limit, tc)
{
}
ATF_TC_BODY(dns_name_limit, tc)
{
	cap_channel_t *capdns;
	const char *types[2];
	int families[2];

	capdns = cap_dns_init();

	types[0] = "NAME2ADDR";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "ADDR2NAME";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 2) == -1);
	types[0] = "ADDR2NAME";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 1) == -1);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 2) == 0);

	runtest(capdns,
	    (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET | GETHOSTBYNAME2_AF_INET6 |
	    GETADDRINFO_AF_INET | GETADDRINFO_AF_INET6));

	cap_close(capdns);
}

ATF_TC(dns_addr_limit);
ATF_TC_HEAD(dns_addr_limit, tc)
{
}
ATF_TC_BODY(dns_addr_limit, tc)
{
	cap_channel_t *capdns;
	const char *types[2];
	int families[2];

	capdns = cap_dns_init();

	types[0] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "NAME2ADDR";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 2) == -1);
	types[0] = "NAME2ADDR";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 1) == -1);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 2) == 0);

	runtest(capdns,
	    (GETHOSTBYADDR_AF_INET | GETHOSTBYADDR_AF_INET6));

	cap_close(capdns);
}

ATF_TC(dns_inet_limit);
ATF_TC_HEAD(dns_inet_limit, tc)
{
}
ATF_TC_BODY(dns_inet_limit, tc)
{
	cap_channel_t *capdns;
	const char *types[2];
	int families[2];

	capdns = cap_dns_init();

	types[0] = "NAME2ADDR";
	types[1] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET6;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 2) == -1);
	families[0] = AF_INET6;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 1) == -1);

	runtest(capdns,
	    (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET | GETHOSTBYADDR_AF_INET |
	    GETADDRINFO_AF_INET));

	cap_close(capdns);
}

ATF_TC(dns_inet6_limit);
ATF_TC_HEAD(dns_inet6_limit, tc)
{
}
ATF_TC_BODY(dns_inet6_limit, tc)
{
	cap_channel_t *capdns;
	const char *types[2];
	int families[2];

	capdns = cap_dns_init();

	types[0] = "NAME2ADDR";
	types[1] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET6;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 2) == -1);
	families[0] = AF_INET;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 1) == -1);

	runtest(capdns,
	    (GETHOSTBYNAME2_AF_INET6 | GETHOSTBYADDR_AF_INET6 |
	    GETADDRINFO_AF_INET6));

	cap_close(capdns);
}

ATF_TC(dns_name_inet_limit);
ATF_TC_HEAD(dns_name_inet_limit, tc)
{
}
ATF_TC_BODY(dns_name_inet_limit, tc)
{
	cap_channel_t *capdns;
	const char *types[2];
	int families[2];

	capdns = cap_dns_init();

	types[0] = "NAME2ADDR";
	types[1] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 2) == 0);
	types[0] = "NAME2ADDR";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "ADDR2NAME";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 2) == -1);
	types[0] = "ADDR2NAME";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 1) == -1);
	families[0] = AF_INET;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET6;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 2) == -1);
	families[0] = AF_INET6;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 1) == -1);

	runtest(capdns,
	    (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET | GETADDRINFO_AF_INET));

	cap_close(capdns);
}

ATF_TC(dns_name_inet6_limit);
ATF_TC_HEAD(dns_name_inet6_limit, tc)
{
}
ATF_TC_BODY(dns_name_inet6_limit, tc)
{
	cap_channel_t *capdns;
	const char *types[2];
	int families[2];

	capdns = cap_dns_init();

	types[0] = "NAME2ADDR";
	types[1] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET6;
	families[1] = AF_INET;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 2) == 0);
	types[0] = "NAME2ADDR";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "ADDR2NAME";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 2) == -1);
	types[0] = "ADDR2NAME";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 1) == -1);
	families[0] = AF_INET6;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 2) == -1);
	families[0] = AF_INET;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 1) == -1);

	runtest(capdns,
	    (GETHOSTBYNAME2_AF_INET6 | GETADDRINFO_AF_INET6));

	cap_close(capdns);
}

ATF_TC(dns_addr_inet_limit);
ATF_TC_HEAD(dns_addr_inet_limit, tc)
{
}
ATF_TC_BODY(dns_addr_inet_limit, tc)
{
	cap_channel_t *capdns;
	const char *types[2];
	int families[2];

	capdns = cap_dns_init();

	types[0] = "NAME2ADDR";
	types[1] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 2) == 0);
	types[0] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "NAME2ADDR";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 2) == -1);
	types[0] = "NAME2ADDR";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 1) == -1);
	families[0] = AF_INET;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET6;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 2) == -1);
	families[0] = AF_INET6;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 1) == -1);

	runtest(capdns, GETHOSTBYADDR_AF_INET);

	cap_close(capdns);
}

ATF_TC(dns_addr_inet6_limit);
ATF_TC_HEAD(dns_addr_inet6_limit, tc)
{
}
ATF_TC_BODY(dns_addr_inet6_limit, tc)
{
	cap_channel_t *capdns;
	const char *types[2];
	int families[2];

	capdns = cap_dns_init();

	types[0] = "NAME2ADDR";
	types[1] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET6;
	families[1] = AF_INET;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 2) == 0);
	types[0] = "ADDR2NAME";
	ATF_REQUIRE(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "NAME2ADDR";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 2) == -1);
	types[0] = "NAME2ADDR";
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_type_limit(capdns, types, 1) == -1);
	families[0] = AF_INET6;
	ATF_REQUIRE(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 2) == -1);
	families[0] = AF_INET;
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    cap_dns_family_limit(capdns, families, 1) == -1);

	runtest(capdns, GETHOSTBYADDR_AF_INET6);

	cap_close(capdns);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dns_no_limits);
	ATF_TP_ADD_TC(tp, dns_all_limits);
	ATF_TP_ADD_TC(tp, dns_name_limit);
	ATF_TP_ADD_TC(tp, dns_addr_limit);
	ATF_TP_ADD_TC(tp, dns_inet_limit);
	ATF_TP_ADD_TC(tp, dns_inet6_limit);
	ATF_TP_ADD_TC(tp, dns_name_inet_limit);
	ATF_TP_ADD_TC(tp, dns_name_inet6_limit);
	ATF_TP_ADD_TC(tp, dns_addr_inet_limit);
	ATF_TP_ADD_TC(tp, dns_addr_inet6_limit);

	return atf_no_error();
}
