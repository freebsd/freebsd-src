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
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include "util.h"
#include <alias.h>

#define	timevalcmp(tv, uv, cmp)			\
	(((tv).tv_sec == (uv).tv_sec)		\
	 ? ((tv).tv_usec cmp (uv).tv_usec)	\
	 : ((tv).tv_sec cmp (uv).tv_sec))

#define timevaldiff(n, o) (float)		\
	(((n).tv_sec - (o).tv_sec)*1000000l +	\
	 ((n).tv_usec - (o).tv_usec))

int main(int argc, char ** argv)
{
	struct libalias *la;
	struct timeval timeout;
	struct ip *p;
	struct udphdr *u;
	struct {
		struct in_addr src, dst;
		uint16_t sport, dport, aport;
	} *batch;
	struct {
		unsigned long ok, fail;
	} nat, unnat, random, attack;
	int max_seconds, batch_size, random_size, attack_length, round, cnt;

	if(argc != 5 ||
	   0 >  (max_seconds = atoi(argv[1])) ||
	   0 >= (batch_size = atoi(argv[2])) ||
	   0 >= (random_size = atoi(argv[3])) ||
	   0 >= (attack_length = atoi(argv[4]))) {
		printf("Usage: %s max_seconds batch_size random_size attack_length\n", argv[0]);
		return 1;
	}
	if (NULL == (la = LibAliasInit(NULL))) {
		perror("LibAliasInit");
		return -1;
	}

	bzero(&nat, sizeof(nat));
	bzero(&unnat, sizeof(unnat));
	bzero(&random, sizeof(random));
	bzero(&attack, sizeof(attack));

	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_DENY_INCOMING, PKT_ALIAS_DENY_INCOMING);

	prv1.s_addr &= htonl(0xffff0000);
	ext.s_addr &= htonl(0xffff0000);

	p = ip_packet(0, 64);
	u = set_udp(p, 0, 0);

	if (NULL == (batch = calloc(batch_size, sizeof(*batch)))) {
		perror("calloc(batch)");
		return -1;
	}

	gettimeofday(&timeout, NULL);
	timeout.tv_sec += max_seconds;

	printf("RND SECND NAT RND ATT UNA\n");
	for (round = 0; ; round++) {
		int i, res;
		struct timeval now, start;

		printf("%3d ", round+1);

		gettimeofday(&start, NULL);
		printf("%5.1f ", max_seconds - timevaldiff(timeout, start)/1000000.0f);
		for (cnt = i = 0; i < batch_size; i++, cnt++) {
			batch[i].src.s_addr = prv1.s_addr | htonl(rand_range(0, 0xffff));
			batch[i].dst.s_addr = ext.s_addr | htonl(rand_range(0, 0xffff));
			batch[i].sport = rand_range(1000, 60000);
			batch[i].dport = rand_range(1000, 60000);

			p->ip_src = batch[i].src;
			p->ip_dst = batch[i].dst;
			u = set_udp(p, batch[i].sport, batch[i].dport);

			res = LibAliasOut(la, p, 64);
			batch[i].aport = htons(u->uh_sport);

			if (res == PKT_ALIAS_OK &&
			    u->uh_dport == htons(batch[i].dport) &&
			    addr_eq(p->ip_dst, batch[i].dst) &&
			    addr_eq(p->ip_src, masq))
				nat.ok++;
			else
				nat.fail++;

			gettimeofday(&now, NULL);
			if(timevalcmp(now, timeout, >=))
				goto out;
		}
		if (cnt > 0)
			printf("%3.0f ", timevaldiff(now, start) / cnt);

		start = now;
		for (cnt = i = 0; i < random_size; i++, cnt++) {
			p->ip_src.s_addr = ext.s_addr & htonl(0xfff00000);
			p->ip_src.s_addr |= htonl(rand_range(0, 0xffff));
			p->ip_dst = masq;
			u = set_udp(p, rand_range(1, 0xffff), rand_range(1, 0xffff));

			res = LibAliasIn(la, p, 64);

			if (res == PKT_ALIAS_OK)
				random.ok++;
			else
				random.fail++;

			gettimeofday(&now, NULL);
			if(timevalcmp(now, timeout, >=))
				goto out;
		}
		if (cnt > 0)
			printf("%3.0f ", timevaldiff(now, start) / cnt);

		start = now;
		p->ip_src.s_addr = ext.s_addr & htonl(0xfff00000);
		p->ip_src.s_addr |= htonl(rand_range(0, 0xffff));
		p->ip_dst = masq;
		u = set_udp(p, rand_range(1, 0xffff), rand_range(1, 0xffff));
		for (cnt = i = 0; i < attack_length; i++, cnt++) {
			res = LibAliasIn(la, p, 64);

			if (res == PKT_ALIAS_OK)
				attack.ok++;
			else
				attack.fail++;

			gettimeofday(&now, NULL);
			if(timevalcmp(now, timeout, >=))
				goto out;
		}
		if (cnt > 0)
			printf("%3.0f ", timevaldiff(now, start) / cnt);

		qsort(batch, batch_size, sizeof(*batch), randcmp);

		gettimeofday(&start, NULL);
		for (cnt = i = 0; i < batch_size; i++, cnt++) {
			p->ip_src = batch[i].dst;
			p->ip_dst = masq;
			u = set_udp(p, batch[i].dport, batch[i].aport);

			res = LibAliasIn(la, p, 64);
			batch[i].aport = htons(u->uh_sport);

			if (res == PKT_ALIAS_OK &&
			    u->uh_sport == htons(batch[i].dport) &&
			    u->uh_dport == htons(batch[i].sport) &&
			    addr_eq(p->ip_dst, batch[i].src) &&
			    addr_eq(p->ip_src, batch[i].dst))
				unnat.ok++;
			else
				unnat.fail++;

			gettimeofday(&now, NULL);
			if(timevalcmp(now, timeout, >=))
				goto out;
		}
		if (cnt > 0)
			printf("%3.0f\n", timevaldiff(now, start) / cnt);
	}
out:
	printf("\n\n");
	free(batch);
	free(p);
	LibAliasUninit(la);

	printf("Results\n");
	printf("   Rounds  : %7u\n", round);
	printf("   NAT ok  : %7lu\n", nat.ok);
	printf("   NAT fail: %7lu\n", nat.fail);
	printf(" UNNAT ok  : %7lu\n", unnat.ok);
	printf(" UNNAT fail: %7lu\n", unnat.fail);
	printf("RANDOM ok  : %7lu\n", random.ok);
	printf("RANDOM fail: %7lu\n", random.fail);
	printf("ATTACK ok  : %7lu\n", attack.ok);
	printf("ATTACK fail: %7lu\n", attack.fail);
	printf(" -------------------\n");
	printf("      Total: %7lu\n",
	       nat.ok + nat.fail + unnat.ok + unnat.fail +
	       random.ok + random.fail + attack.ok + attack.fail);
	return (0);
}
