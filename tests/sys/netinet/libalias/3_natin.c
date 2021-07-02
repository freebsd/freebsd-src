/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 Lutz Donnerhacke
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <atf-c.h>
#include <alias.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

ATF_TC_WITHOUT_HEAD(1_portforward);
ATF_TC_BODY(1_portforward, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct alias_link *pf1, *pf2, *pf3, *pf4;
	struct ip *p;
	struct udphdr *u;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_RESET_ON_ADDR_CHANGE, ~0);
	LibAliasSetMode(la, PKT_ALIAS_DENY_INCOMING, PKT_ALIAS_DENY_INCOMING);

	/*
	 * Fully specified
	 */
	pf1 = LibAliasRedirectPort(la, prv1, ntohs(0x1234), ext, ntohs(0x5678), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf1 != NULL);

	p = ip_packet(0, 64);
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv1, 0x1234);
	/* try again */
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv1, 0x1234);
	/* different source */
	UDP_UNNAT_FAIL(p, u, pub, 0x5678, masq, 0xabcd);
	UDP_UNNAT_FAIL(p, u, ext, 0xdead, masq, 0xabcd);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/* delete and try again */
	LibAliasRedirectDelete(la, pf1);
	UDP_UNNAT_FAIL(p, u, ext, 0x5678, masq, 0xabcd);

	/*
	 * Any external port
	 */
	pf2 = LibAliasRedirectPort(la, prv2, ntohs(0x1234), ext, ntohs(0), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf2 != NULL);

	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv2, 0x1234);
	/* try again */
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv2, 0x1234);
	/* different source */
	UDP_UNNAT_FAIL(p, u, pub, 0x5678, masq, 0xabcd);
	UDP_UNNAT_CHECK(p, u, ext, 0xdead, masq, 0xabcd, prv2, 0x1234);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/* delete and try again */
	LibAliasRedirectDelete(la, pf2);
	UDP_UNNAT_FAIL(p, u, ext, 0x5678, masq, 0xabcd);

	/*
	 * Any external host
	 */
	pf3 = LibAliasRedirectPort(la, prv3, ntohs(0x1234), ANY_ADDR, ntohs(0x5678), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf3 != NULL);

	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv3, 0x1234);
	/* try again */
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv3, 0x1234);
	/* different source */
	UDP_UNNAT_CHECK(p, u, pub, 0x5678, masq, 0xabcd, prv3, 0x1234);
	UDP_UNNAT_FAIL(p, u, ext, 0xdead, masq, 0xabcd);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/* delete and try again */
	LibAliasRedirectDelete(la, pf3);
	UDP_UNNAT_FAIL(p, u, ext, 0x5678, masq, 0xabcd);

	/*
	 * Any external host, any port
	 */
	pf4 = LibAliasRedirectPort(la, cgn, ntohs(0x1234), ANY_ADDR, ntohs(0), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf4 != NULL);

	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, cgn, 0x1234);
	/* try again */
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, cgn, 0x1234);
	/* different source */
	UDP_UNNAT_CHECK(p, u, pub, 0x5678, masq, 0xabcd, cgn, 0x1234);
	UDP_UNNAT_CHECK(p, u, ext, 0xdead, masq, 0xabcd, cgn, 0x1234);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/* delete and try again */
	LibAliasRedirectDelete(la, pf4);
	UDP_UNNAT_FAIL(p, u, ext, 0x5678, masq, 0xabcd);

	free(p);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(2_portoverlap);
ATF_TC_BODY(2_portoverlap, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct alias_link *pf1, *pf2, *pf3, *pf4;
	struct ip *p;
	struct udphdr *u;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_RESET_ON_ADDR_CHANGE, ~0);
	LibAliasSetMode(la, PKT_ALIAS_DENY_INCOMING, PKT_ALIAS_DENY_INCOMING);

	/*
	 * Fully specified
	 */
	pf1 = LibAliasRedirectPort(la, prv2, ntohs(0x1234), ext, ntohs(0x5678), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf1 != NULL);

	p = ip_packet(0, 64);
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv2, 0x1234);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/*
	 * Fully specified (override)
	 */
	pf1 = LibAliasRedirectPort(la, prv1, ntohs(0x1234), ext, ntohs(0x5678), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf1 != NULL);

	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv1, 0x1234);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/*
	 * Any external port
	 */
	pf2 = LibAliasRedirectPort(la, prv2, ntohs(0x1234), ext, ntohs(0), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf2 != NULL);

	UDP_UNNAT_CHECK(p, u, ext, 0x5679, masq, 0xabcd, prv2, 0x1234);
	/* more specific rule wins */
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv1, 0x1234);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/*
	 * Any external host
	 */
	pf3 = LibAliasRedirectPort(la, prv3, ntohs(0x1234), ANY_ADDR, ntohs(0x5678), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf3 != NULL);

	UDP_UNNAT_CHECK(p, u, pub, 0x5678, masq, 0xabcd, prv3, 0x1234);
	/* more specific rule wins */
	UDP_UNNAT_CHECK(p, u, ext, 0x5679, masq, 0xabcd, prv2, 0x1234);
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv1, 0x1234);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/*
	 * Any external host, any port
	 */
	pf4 = LibAliasRedirectPort(la, cgn, ntohs(0x1234), ANY_ADDR, ntohs(0), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf4 != NULL);

	UDP_UNNAT_CHECK(p, u, prv1, 0x5679, masq, 0xabcd, cgn, 0x1234);
	/* more specific rule wins */
	UDP_UNNAT_CHECK(p, u, pub, 0x5678, masq, 0xabcd, prv3, 0x1234);
	UDP_UNNAT_CHECK(p, u, ext, 0x5679, masq, 0xabcd, prv2, 0x1234);
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv1, 0x1234);

	free(p);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(3_redirectany);
ATF_TC_BODY(3_redirectany, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct alias_link *pf;
	struct ip *p;
	struct udphdr *u;

	ATF_REQUIRE(la != NULL);
	LibAliasSetMode(la, PKT_ALIAS_DENY_INCOMING, ~0);
	p = ip_packet(0, 64);

	pf = LibAliasRedirectPort(la, prv1, ntohs(0x1234), ANY_ADDR, 0, ANY_ADDR, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf != NULL);

	LibAliasSetAddress(la, masq);
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv1, 0x1234);
	UDP_UNNAT_FAIL(p, u, pub, 0x5678, pub, 0xabcd);

	LibAliasSetAddress(la, pub);
	UDP_UNNAT_CHECK(p, u, pub, 0x5679, pub, 0xabcd, prv1, 0x1234);
	UDP_UNNAT_FAIL(p, u, ext, 0x5679, masq, 0xabcd);

	free(p);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(4_redirectaddr);
ATF_TC_BODY(4_redirectaddr, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct alias_link *pf1, *pf2;
	struct ip *p;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	pf1 = LibAliasRedirectAddr(la, prv1, pub);
	ATF_REQUIRE(pf1 != NULL);

	p = ip_packet(254, 64);
	UNNAT_CHECK(p, ext, pub, prv1);
	UNNAT_CHECK(p, ext, masq, masq);

	pf2 = LibAliasRedirectAddr(la, prv2, pub);
	ATF_REQUIRE(pf2 != NULL);
	UNNAT_CHECK(p, ext, pub, prv1);
	p->ip_p = 253;		       /* new flows */
	UNNAT_CHECK(p, ext, pub, prv2);
	UNNAT_CHECK(p, ext, masq, masq);

	p->ip_p = 252;		       /* new flows */
	NAT_CHECK(p, prv1, ext, pub);
	NAT_CHECK(p, prv2, ext, pub);
	NAT_CHECK(p, prv3, ext, masq);

	LibAliasSetMode(la, PKT_ALIAS_DENY_INCOMING, ~0);
	p->ip_p = 251;		       /* new flows */
	UNNAT_FAIL(p, ext, pub);
	UNNAT_FAIL(p, ext, masq);

	/* unhide older version */
	LibAliasRedirectDelete(la, pf2);
	LibAliasSetMode(la, 0, ~0);
	p->ip_p = 250;		       /* new flows */
	UNNAT_CHECK(p, ext, pub, prv1);

	p->ip_p = 249;		       /* new flows */
	NAT_CHECK(p, prv1, ext, pub);
	NAT_CHECK(p, prv2, ext, masq);
	NAT_CHECK(p, prv3, ext, masq);

	free(p);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(5_lsnat);
ATF_TC_BODY(5_lsnat, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct alias_link *pf;
	struct ip *p;
	struct udphdr *u;

	ATF_REQUIRE(la != NULL);
	LibAliasSetMode(la, 0, ~0);
	p = ip_packet(0, 64);

	pf = LibAliasRedirectPort(la, cgn, ntohs(0xdead), ANY_ADDR, 0, masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf != NULL);

	ATF_REQUIRE(0 == LibAliasAddServer(la, pf, prv1, ntohs(0x1234)));
	ATF_REQUIRE(0 == LibAliasAddServer(la, pf, prv2, ntohs(0x2345)));
	ATF_REQUIRE(0 == LibAliasAddServer(la, pf, prv3, ntohs(0x3456)));

	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv3, 0x3456);
	UDP_UNNAT_CHECK(p, u, ext, 0x5679, masq, 0xabcd, prv2, 0x2345);
	UDP_UNNAT_CHECK(p, u, ext, 0x567a, masq, 0xabcd, prv1, 0x1234);
	UDP_UNNAT_CHECK(p, u, ext, 0x567b, masq, 0xabcd, prv3, 0x3456);
	UDP_UNNAT_CHECK(p, u, ext, 0x567c, masq, 0xabcd, prv2, 0x2345);
	UDP_UNNAT_CHECK(p, u, ext, 0x567d, masq, 0xabcd, prv1, 0x1234);

	free(p);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(6_oneshot);
ATF_TC_BODY(6_oneshot, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct alias_link *pf;
	struct ip *p;
	struct udphdr *u;

	ATF_REQUIRE(la != NULL);
	LibAliasSetMode(la, 0, ~0);
	LibAliasSetMode(la, PKT_ALIAS_RESET_ON_ADDR_CHANGE, ~0);
	LibAliasSetMode(la, PKT_ALIAS_DENY_INCOMING, PKT_ALIAS_DENY_INCOMING);

	pf = LibAliasRedirectPort(la, prv1, ntohs(0x1234), ANY_ADDR, 0, masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf != NULL);
	/* only for fully specified links */
	ATF_CHECK(-1 == LibAliasRedirectDynamic(la, pf));
	LibAliasRedirectDelete(la, pf);

	pf = LibAliasRedirectPort(la, prv1, ntohs(0x1234), ext, ntohs(0x5678), masq, ntohs(0xabcd), IPPROTO_UDP);
	ATF_REQUIRE(pf != NULL);
	ATF_CHECK(0 == LibAliasRedirectDynamic(la, pf));

	p = ip_packet(0, 64);
	UDP_UNNAT_CHECK(p, u, ext, 0x5678, masq, 0xabcd, prv1, 0x1234);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/* does not work anymore */
	UDP_UNNAT_FAIL(p, u, ext, 0x5678, masq, 0xabcd);

	free(p);
	LibAliasUninit(la);
}

ATF_TP_ADD_TCS(natin)
{
	/* Use "dd if=/dev/random bs=2 count=1 | od -x" to reproduce */
	srand(0xe859);

	ATF_TP_ADD_TC(natin, 1_portforward);
	ATF_TP_ADD_TC(natin, 2_portoverlap);
	ATF_TP_ADD_TC(natin, 3_redirectany);
	ATF_TP_ADD_TC(natin, 4_redirectaddr);
	ATF_TP_ADD_TC(natin, 5_lsnat);
	ATF_TP_ADD_TC(natin, 6_oneshot);

	return atf_no_error();
}
