/*-
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
__FBSDID("$FreeBSD$");

#include <sys/capability.h>

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

#include <libcapsicum.h>
#include <libcapsicum_dns.h>
#include <libcapsicum_service.h>

static int ntest = 1;

#define CHECK(expr)     do {						\
	if ((expr))							\
		printf("ok %d %s:%u\n", ntest, __FILE__, __LINE__);	\
	else								\
		printf("not ok %d %s:%u\n", ntest, __FILE__, __LINE__);	\
	ntest++;							\
} while (0)
#define CHECKX(expr)     do {						\
	if ((expr)) {							\
		printf("ok %d %s:%u\n", ntest, __FILE__, __LINE__);	\
	} else {							\
		printf("not ok %d %s:%u\n", ntest, __FILE__, __LINE__);	\
		exit(1);						\
	}								\
	ntest++;							\
} while (0)

#define	GETHOSTBYNAME			0x01
#define	GETHOSTBYNAME2_AF_INET		0x02
#define	GETHOSTBYNAME2_AF_INET6		0x04
#define	GETHOSTBYADDR_AF_INET		0x08
#define	GETHOSTBYADDR_AF_INET6		0x10

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

static unsigned int
runtest(cap_channel_t *capdns)
{
	unsigned int result;
	struct hostent *hps, *hpc;
	struct in_addr ip4;
	struct in6_addr ip6;

	result = 0;

	hps = gethostbyname("example.com");
	if (hps == NULL)
		fprintf(stderr, "Unable to resolve %s IPv4.\n", "example.com");
	hpc = cap_gethostbyname(capdns, "example.com");
	if (hostent_compare(hps, hpc))
		result |= GETHOSTBYNAME;

	hps = gethostbyname2("example.com", AF_INET);
	if (hps == NULL)
		fprintf(stderr, "Unable to resolve %s IPv4.\n", "example.com");
	hpc = cap_gethostbyname2(capdns, "example.com", AF_INET);
	if (hostent_compare(hps, hpc))
		result |= GETHOSTBYNAME2_AF_INET;

	hps = gethostbyname2("example.com", AF_INET6);
	if (hps == NULL)
		fprintf(stderr, "Unable to resolve %s IPv6.\n", "example.com");
	hpc = cap_gethostbyname2(capdns, "example.com", AF_INET6);
	if (hostent_compare(hps, hpc))
		result |= GETHOSTBYNAME2_AF_INET6;

	/*
	 * 8.8.178.135 is IPv4 address of freefall.freebsd.org
	 * as of 27 October 2013.
	 */
	inet_pton(AF_INET, "8.8.178.135", &ip4);
	hps = gethostbyaddr(&ip4, sizeof(ip4), AF_INET);
	if (hps == NULL)
		fprintf(stderr, "Unable to resolve %s.\n", "8.8.178.135");
	hpc = cap_gethostbyaddr(capdns, &ip4, sizeof(ip4), AF_INET);
	if (hostent_compare(hps, hpc))
		result |= GETHOSTBYADDR_AF_INET;

	/*
	 * 2001:1900:2254:206c::16:87 is IPv6 address of freefall.freebsd.org
	 * as of 27 October 2013.
	 */
	inet_pton(AF_INET6, "2001:1900:2254:206c::16:87", &ip6);
	hps = gethostbyaddr(&ip6, sizeof(ip6), AF_INET6);
	if (hps == NULL) {
		fprintf(stderr, "Unable to resolve %s.\n",
		    "2001:1900:2254:206c::16:87");
	}
	hpc = cap_gethostbyaddr(capdns, &ip6, sizeof(ip6), AF_INET6);
	if (hostent_compare(hps, hpc))
		result |= GETHOSTBYADDR_AF_INET6;

	return (result);
}

int
main(void)
{
	cap_channel_t *capcas, *capdns, *origcapdns;
	const char *types[2];
	int families[2];

	printf("1..91\n");

	capcas = cap_init();
	CHECKX(capcas != NULL);

	origcapdns = capdns = cap_service_open(capcas, "system.dns");
	CHECKX(capdns != NULL);

	cap_close(capcas);

	/* No limits set. */

	CHECK(runtest(capdns) ==
	    (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET | GETHOSTBYNAME2_AF_INET6 |
	     GETHOSTBYADDR_AF_INET | GETHOSTBYADDR_AF_INET6));

	/*
	 * Allow:
	 * type: NAME, ADDR
	 * family: AF_INET, AF_INET6
	 */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "NAME";
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == 0);

	CHECK(runtest(capdns) ==
	    (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET | GETHOSTBYNAME2_AF_INET6 |
	     GETHOSTBYADDR_AF_INET | GETHOSTBYADDR_AF_INET6));

	cap_close(capdns);

	/*
	 * Allow:
	 * type: NAME
	 * family: AF_INET, AF_INET6
	 */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == -1 &&
	    errno == ENOTCAPABLE);
	types[0] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 1) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == 0);

	CHECK(runtest(capdns) ==
	    (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET | GETHOSTBYNAME2_AF_INET6));

	cap_close(capdns);

	/*
	 * Allow:
	 * type: ADDR
	 * family: AF_INET, AF_INET6
	 */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 2) == -1 &&
	    errno == ENOTCAPABLE);
	types[0] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 1) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == 0);

	CHECK(runtest(capdns) ==
	    (GETHOSTBYADDR_AF_INET | GETHOSTBYADDR_AF_INET6));

	cap_close(capdns);

	/*
	 * Allow:
	 * type: NAME, ADDR
	 * family: AF_INET
	 */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "NAME";
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest(capdns) ==
	    (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET | GETHOSTBYADDR_AF_INET));

	cap_close(capdns);

	/*
	 * Allow:
	 * type: NAME, ADDR
	 * family: AF_INET6
	 */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "NAME";
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 2) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest(capdns) ==
	    (GETHOSTBYNAME2_AF_INET6 | GETHOSTBYADDR_AF_INET6));

	cap_close(capdns);

	/* Below we also test further limiting capability. */

	/*
	 * Allow:
	 * type: NAME
	 * family: AF_INET
	 */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "NAME";
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == 0);
	types[0] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == -1 &&
	    errno == ENOTCAPABLE);
	types[0] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 1) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest(capdns) == (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET));

	cap_close(capdns);

	/*
	 * Allow:
	 * type: NAME
	 * family: AF_INET6
	 */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "NAME";
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == 0);
	types[0] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == -1 &&
	    errno == ENOTCAPABLE);
	types[0] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 1) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 2) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest(capdns) == GETHOSTBYNAME2_AF_INET6);

	cap_close(capdns);

	/*
	 * Allow:
	 * type: ADDR
	 * family: AF_INET
	 */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "NAME";
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == 0);
	types[0] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 2) == -1 &&
	    errno == ENOTCAPABLE);
	types[0] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 1) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest(capdns) == GETHOSTBYADDR_AF_INET);

	cap_close(capdns);

	/*
	 * Allow:
	 * type: ADDR
	 * family: AF_INET6
	 */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "NAME";
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == 0);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == 0);
	types[0] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 1) == 0);
	types[1] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 2) == -1 &&
	    errno == ENOTCAPABLE);
	types[0] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 1) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 1) == 0);
	families[1] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 2) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest(capdns) == GETHOSTBYADDR_AF_INET6);

	cap_close(capdns);

	/* Trying to rise the limits. */

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 1) == 0);
	families[0] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 1) == 0);

	types[0] = "NAME";
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == -1 &&
	    errno == ENOTCAPABLE);

	types[0] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 1) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(cap_dns_type_limit(capdns, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	CHECK(cap_dns_family_limit(capdns, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);

	/* Do the limits still hold? */
	CHECK(runtest(capdns) == (GETHOSTBYNAME | GETHOSTBYNAME2_AF_INET));

	cap_close(capdns);

	capdns = cap_clone(origcapdns);
	CHECK(capdns != NULL);

	types[0] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 1) == 0);
	families[0] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 1) == 0);

	types[0] = "NAME";
	types[1] = "ADDR";
	CHECK(cap_dns_type_limit(capdns, types, 2) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	families[1] = AF_INET6;
	CHECK(cap_dns_family_limit(capdns, families, 2) == -1 &&
	    errno == ENOTCAPABLE);

	types[0] = "NAME";
	CHECK(cap_dns_type_limit(capdns, types, 1) == -1 &&
	    errno == ENOTCAPABLE);
	families[0] = AF_INET;
	CHECK(cap_dns_family_limit(capdns, families, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(cap_dns_type_limit(capdns, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	CHECK(cap_dns_family_limit(capdns, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);

	/* Do the limits still hold? */
	CHECK(runtest(capdns) == GETHOSTBYADDR_AF_INET6);

	cap_close(capdns);

	cap_close(origcapdns);

	exit(0);
}
