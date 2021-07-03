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

ATF_TC_WITHOUT_HEAD(1_simplemasq);
ATF_TC_BODY(1_simplemasq, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip *pip;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, 0, ~0);

	pip = ip_packet(254, 64);
	NAT_CHECK(pip, prv1, ext, masq);
	NAT_CHECK(pip, prv2, ext, masq);
	NAT_CHECK(pip, prv3, ext, masq);
	NAT_CHECK(pip, cgn,  ext, masq);
	NAT_CHECK(pip, pub,  ext, masq);

	free(pip);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(2_unregistered);
ATF_TC_BODY(2_unregistered, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip *pip;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_UNREGISTERED_ONLY, ~0);

	pip = ip_packet(254, 64);
	NAT_CHECK(pip, prv1, ext, masq);
	NAT_CHECK(pip, prv2, ext, masq);
	NAT_CHECK(pip, prv3, ext, masq);
	NAT_CHECK(pip, cgn,  ext, cgn);
	NAT_CHECK(pip, pub,  ext, pub);

	/*
	 * State is only for new connections
	 * Because they are now active,
	 * the mode setting should be ignored
	 */
	LibAliasSetMode(la, 0, PKT_ALIAS_UNREGISTERED_ONLY);
	NAT_CHECK(pip, prv1, ext, masq);
	NAT_CHECK(pip, prv2, ext, masq);
	NAT_CHECK(pip, prv3, ext, masq);
	NAT_CHECK(pip, cgn,  ext, cgn);
	NAT_CHECK(pip, pub,  ext, pub);

	free(pip);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(3_cgn);
ATF_TC_BODY(3_cgn, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip *pip;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_UNREGISTERED_CGN, ~0);

	pip = ip_packet(254, 64);
	NAT_CHECK(pip, prv1, ext, masq);
	NAT_CHECK(pip, prv2, ext, masq);
	NAT_CHECK(pip, prv3, ext, masq);
	NAT_CHECK(pip, cgn,  ext, masq);
	NAT_CHECK(pip, pub,  ext, pub);

	/*
	 * State is only for new connections
	 * Because they are now active,
	 * the mode setting should be ignored
	 */
	LibAliasSetMode(la, 0, PKT_ALIAS_UNREGISTERED_CGN);
	NAT_CHECK(pip, prv1, ext, masq);
	NAT_CHECK(pip, prv2, ext, masq);
	NAT_CHECK(pip, prv3, ext, masq);
	NAT_CHECK(pip, cgn,  ext, masq);
	NAT_CHECK(pip, pub,  ext, pub);

	free(pip);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(4_udp);
ATF_TC_BODY(4_udp, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip  *po, *pi;
	struct udphdr *ui, *uo;
	uint16_t sport = 0x1234;
	uint16_t dport = 0x5678;
	uint16_t aport;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, 0, ~0);

	/* Query from prv1 */
	po = ip_packet(0, 64);
	UDP_NAT_CHECK(po, uo, prv1, sport, ext, dport, masq);
	aport = ntohs(uo->uh_sport);
	/* should use a different external port */
	ATF_CHECK(aport != sport);

	/* Response */
	pi = ip_packet(0, 64);
	UDP_UNNAT_CHECK(pi, ui, ext, dport, masq, aport, prv1, sport);

	/* Query from different source with same ports */
	UDP_NAT_CHECK(po, uo, prv2, sport, ext, dport, masq);
	/* should use a different external port */
	ATF_CHECK(uo->uh_sport != htons(aport));

	/* Response to prv2 */
	ui->uh_dport = uo->uh_sport;
	UDP_UNNAT_CHECK(pi, ui, ext, dport, masq, htons(uo->uh_sport), prv2, sport);

	/* Response to prv1 again */
	UDP_UNNAT_CHECK(pi, ui, ext, dport, masq, aport, prv1, sport);

	free(pi);
	free(po);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(5_sameport);
ATF_TC_BODY(5_sameport, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip  *p;
	struct udphdr *u;
	uint16_t sport = 0x1234;
	uint16_t dport = 0x5678;
	uint16_t aport;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_SAME_PORTS, ~0);

	/* Query from prv1 */
	p = ip_packet(0, 64);
	UDP_NAT_CHECK(p, u, prv1, sport, ext, dport, masq);
	aport = ntohs(u->uh_sport);
	/* should use the same external port */
	ATF_CHECK(aport == sport);

	/* Query from different source with same ports */
	UDP_NAT_CHECK(p, u, prv2, sport, ext, dport, masq);
	/* should use a different external port */
	ATF_CHECK(u->uh_sport != htons(aport));

	free(p);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(6_cleartable);
ATF_TC_BODY(6_cleartable, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip  *po, *pi;
	struct udphdr *ui, *uo;
	uint16_t sport = 0x1234;
	uint16_t dport = 0x5678;
	uint16_t aport;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_RESET_ON_ADDR_CHANGE, ~0);
	LibAliasSetMode(la, PKT_ALIAS_SAME_PORTS, PKT_ALIAS_SAME_PORTS);
	LibAliasSetMode(la, PKT_ALIAS_DENY_INCOMING, PKT_ALIAS_DENY_INCOMING);

	/* Query from prv1 */
	po = ip_packet(0, 64);
	UDP_NAT_CHECK(po, uo, prv1, sport, ext, dport, masq);
	aport = ntohs(uo->uh_sport);
	/* should use the same external port */
	ATF_CHECK(aport == sport);

	/* Response */
	pi = ip_packet(0, 64);
	UDP_UNNAT_CHECK(po, uo, ext, dport, masq, aport, prv1, sport);

	/* clear table by keeping the address */
	LibAliasSetAddress(la, ext);
	LibAliasSetAddress(la, masq);

	/* Response to prv1 again -> DENY_INCOMING */
	UDP_UNNAT_FAIL(pi, ui, ext, dport, masq, aport);

	/* Query from different source with same ports */
	UDP_NAT_CHECK(po, uo, prv2, sport, ext, dport, masq);
	/* should use the same external port, because it's free */
	ATF_CHECK(uo->uh_sport == htons(aport));

	/* Response to prv2 */
	UDP_UNNAT_CHECK(po, uo, ext, dport, masq, htons(uo->uh_sport), prv2, sport);

	free(pi);
	free(po);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(7_stress);
ATF_TC_BODY(7_stress, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip *p;
	struct udphdr *u;
	struct {
		struct in_addr src, dst;
		uint16_t sport, dport, aport;
	} *batch;
	size_t const batch_size = 1200;
	size_t const rounds = 25;
	size_t i, j;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);

	p = ip_packet(0, 64);

	batch = calloc(batch_size, sizeof(*batch));
	ATF_REQUIRE(batch != NULL);
	for (j = 0; j < rounds; j++) {
		for (i = 0; i < batch_size; i++) {
			struct in_addr s, d;
			switch (i&3) {
			case 0: s = prv1; d = ext; break;
			case 1: s = prv2; d = pub; break;
			case 2: s = prv3; d = ext; break;
			case 3: s = cgn;  d = pub; break;
			}
			s.s_addr &= htonl(0xffff0000);
			d.s_addr &= htonl(0xffff0000);
			batch[i].src.s_addr = s.s_addr | htonl(rand_range(0, 0xffff));
			batch[i].dst.s_addr = d.s_addr | htonl(rand_range(0, 0xffff));
			batch[i].sport = rand_range(1000, 60000);
			batch[i].dport = rand_range(1000, 60000);
		}

		for (i = 0; i < batch_size; i++) {
			UDP_NAT_CHECK(p, u,
			    batch[i].src, batch[i].sport,
			    batch[i].dst, batch[i].dport,
			    masq);
			batch[i].aport = htons(u->uh_sport);
		}

		qsort(batch, batch_size, sizeof(*batch), randcmp);

		for (i = 0; i < batch_size; i++) {
			UDP_UNNAT_CHECK(p, u,
			    batch[i].dst,  batch[i].dport,
			    masq, batch[i].aport,
			    batch[i].src, batch[i].sport);
		}
	}

	free(batch);
	free(p);
	LibAliasUninit(la);
}

ATF_TC_WITHOUT_HEAD(8_portrange);
ATF_TC_BODY(8_portrange, dummy)
{
	struct libalias *la = LibAliasInit(NULL);
	struct ip  *po;
	struct udphdr *uo;
	uint16_t sport = 0x1234;
	uint16_t dport = 0x5678;
	uint16_t aport;

	ATF_REQUIRE(la != NULL);
	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, 0, ~0);
	po = ip_packet(0, 64);

	LibAliasSetAliasPortRange(la, 0, 0); /* reinit like ipfw */
	UDP_NAT_CHECK(po, uo, prv1, sport, ext, dport, masq);
	aport = ntohs(uo->uh_sport);
	ATF_CHECK(aport >= 0x8000);

	/* Different larger range */
	LibAliasSetAliasPortRange(la, 2000, 3000);
	dport++;
	UDP_NAT_CHECK(po, uo, prv1, sport, ext, dport, masq);
	aport = ntohs(uo->uh_sport);
	ATF_CHECK(aport >= 2000 && aport < 3000);

	/* Different small range (contains two ports) */
	LibAliasSetAliasPortRange(la, 4000, 4001);
	dport++;
	UDP_NAT_CHECK(po, uo, prv1, sport, ext, dport, masq);
	aport = ntohs(uo->uh_sport);
	ATF_CHECK(aport >= 4000 && aport <= 4001);

	sport++;
	UDP_NAT_CHECK(po, uo, prv1, sport, ext, dport, masq);
	aport = ntohs(uo->uh_sport);
	ATF_CHECK(aport >= 4000 && aport <= 4001);

	/* Third port not available in the range */
	sport++;
	UDP_NAT_FAIL(po, uo, prv1, sport, ext, dport);

	/* Back to normal */
	LibAliasSetAliasPortRange(la, 0, 0);
	dport++;
	UDP_NAT_CHECK(po, uo, prv1, sport, ext, dport, masq);
	aport = ntohs(uo->uh_sport);
	ATF_CHECK(aport >= 0x8000);

	free(po);
	LibAliasUninit(la);
}

ATF_TP_ADD_TCS(natout)
{
	/* Use "dd if=/dev/random bs=2 count=1 | od -x" to reproduce */
	srand(0x0b61);

	ATF_TP_ADD_TC(natout, 1_simplemasq);
	ATF_TP_ADD_TC(natout, 2_unregistered);
	ATF_TP_ADD_TC(natout, 3_cgn);
	ATF_TP_ADD_TC(natout, 4_udp);
	ATF_TP_ADD_TC(natout, 5_sameport);
	ATF_TP_ADD_TC(natout, 6_cleartable);
	ATF_TP_ADD_TC(natout, 7_stress);
	ATF_TP_ADD_TC(natout, 8_portrange);

	return atf_no_error();
}
