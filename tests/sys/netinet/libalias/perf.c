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

static void usage(void) __dead2;

#define	timevalcmp(tv, uv, cmp)			\
	(((tv).tv_sec == (uv).tv_sec)		\
	 ? ((tv).tv_usec cmp (uv).tv_usec)	\
	 : ((tv).tv_sec cmp (uv).tv_sec))

#define timevaldiff(n, o) (float)		\
	(((n).tv_sec - (o).tv_sec)*1000000l +	\
	 ((n).tv_usec - (o).tv_usec))

#define check_timeout()	do {				\
	if (check_timeout_cnt++ > 1000) {		\
		check_timeout_cnt = 0;			\
		gettimeofday(&now, NULL);		\
		if (timevalcmp(now, timeout, >=))	\
		    goto out;				\
	} } while(0)

static void
usage(void) {
	printf("Usage: perf [max_seconds [batch_size [random_size [attack_size [redir_size]]]]]\n");
	exit(1);
}

int main(int argc, char ** argv)
{
	struct libalias *la;
	struct timeval timeout, now, start;
	struct ip *p;
	struct udphdr *u;
	struct {
		struct in_addr src, dst;
		uint16_t sport, dport, aport;
	} *batch;
	struct {
		unsigned long ok, fail;
	} nat, usenat, unnat, random, attack;
	int i, round, check_timeout_cnt = 0;
	int max_seconds = 90, batch_size = 2000,
	    random_size = 1000, attack_size = 1000,
	    redir_size = 2000;

	if (argc >= 2) {
		char * end;

		max_seconds = strtol(argv[1], &end, 10);
		if (max_seconds < 2 || end[0] != '\0')
			usage();
	}
	if (argc > 2 && (batch_size  = atoi(argv[2])) < 0)	usage();
	if (argc > 3 && (random_size = atoi(argv[3])) < 0)	usage();
	if (argc > 4 && (attack_size = atoi(argv[4])) < 0)	usage();
	if (argc > 5 && (redir_size  = atoi(argv[5])) < 0)	usage();

	printf("Running perfomance test with parameters:\n");
	printf("  Maximum Runtime (max_seconds) = %d\n", max_seconds);
	printf("  Amount of valid connections (batch_size) = %d\n", batch_size);
	printf("  Amount of random, incoming packets (batch_size) = %d\n", random_size);
	printf("  Repeat count of a random, incoming packet (attack_size) = %d\n", attack_size);
	printf("  Amount of open port forwardings (redir_size) = %d\n", redir_size);
	printf("\n");

	if (NULL == (la = LibAliasInit(NULL))) {
		perror("LibAliasInit");
		return -1;
	}

	bzero(&nat, sizeof(nat));
	bzero(&usenat, sizeof(usenat));
	bzero(&unnat, sizeof(unnat));
	bzero(&random, sizeof(random));
	bzero(&attack, sizeof(attack));

	LibAliasSetAddress(la, masq);
	LibAliasSetMode(la, PKT_ALIAS_SAME_PORTS | PKT_ALIAS_DENY_INCOMING, ~0);

	prv1.s_addr &= htonl(0xffff0000);
	ext.s_addr &= htonl(0xffff0000);

	for (i = 0; i < redir_size; i++) {
		int aport = htons(rand_range(1000, 2000));
		int sport = htons(rand_range(1000, 2000));

		prv2.s_addr &= htonl(0xffff0000);
		prv2.s_addr |= rand_range(0, 0xffff);
		LibAliasRedirectPort(la, prv2, sport, ANY_ADDR, 0, masq, aport, IPPROTO_UDP);
	}

	p = ip_packet(0, 64);
	u = set_udp(p, 0, 0);

	if (NULL == (batch = calloc(batch_size, sizeof(*batch)))) {
		perror("calloc(batch)");
		return -1;
	}

	gettimeofday(&timeout, NULL);
	timeout.tv_sec += max_seconds;

	printf("RND SECOND newNAT RANDOM ATTACK useNAT\n");
	for (round = 0; ; round++) {
		int res, cnt;

		printf("%3d ", round+1);

		gettimeofday(&start, NULL);
		printf("%6.1f ", max_seconds - timevaldiff(timeout, start)/1000000.0f);
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

			check_timeout();
		}
		gettimeofday(&now, NULL);
		if (cnt > 0)
			printf("%6.2f ", timevaldiff(now, start) / cnt);
		else
			printf("------ ");

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

			check_timeout();
		}
		gettimeofday(&now, NULL);
		if (cnt > 0)
			printf("%6.2f ", timevaldiff(now, start) / cnt);
		else
			printf("------ ");

		start = now;
		p->ip_src.s_addr = ext.s_addr & htonl(0xfff00000);
		p->ip_src.s_addr |= htonl(rand_range(0, 0xffff));
		p->ip_dst = masq;
		u = set_udp(p, rand_range(1, 0xffff), rand_range(1, 0xffff));
		for (cnt = i = 0; i < attack_size; i++, cnt++) {
			res = LibAliasIn(la, p, 64);

			if (res == PKT_ALIAS_OK)
				attack.ok++;
			else
				attack.fail++;

			check_timeout();
		}
		gettimeofday(&now, NULL);
		if (cnt > 0)
			printf("%6.2f ", timevaldiff(now, start) / cnt);
		else
			printf("------ ");

		qsort(batch, batch_size, sizeof(*batch), randcmp);

		gettimeofday(&start, NULL);
		for (cnt = i = 0; i < batch_size; i++) {
			int j;

			/* random communication length */
			for(j = rand_range(1, 150); j-- > 0; cnt++) {
				int k;

				/* a random flow out of rolling window */
				k = rand_range(i, i + 25);
				if (k >= batch_size)
					k = i;

				/* 10% outgoing, 90% incoming */
				if (rand_range(0, 100) > 10) {
					p->ip_src = batch[k].dst;
					p->ip_dst = masq;
					u = set_udp(p, batch[k].dport, batch[k].aport);

					res = LibAliasIn(la, p, 64);
					if (res == PKT_ALIAS_OK &&
					    u->uh_sport == htons(batch[k].dport) &&
					    u->uh_dport == htons(batch[k].sport) &&
					    addr_eq(p->ip_dst, batch[k].src) &&
					    addr_eq(p->ip_src, batch[k].dst))
						unnat.ok++;
					else
						unnat.fail++;
				} else {
					p->ip_src = batch[k].src;
					p->ip_dst = batch[k].dst;
					u = set_udp(p, batch[k].sport, batch[k].dport);

					res = LibAliasOut(la, p, 64);
					if (res == PKT_ALIAS_OK &&
					    u->uh_sport == htons(batch[k].aport) &&
					    u->uh_dport == htons(batch[k].dport) &&
					    addr_eq(p->ip_dst, batch[k].dst) &&
					    addr_eq(p->ip_src, masq))
						usenat.ok++;
					else
						usenat.fail++;
				}
				check_timeout();
			}
		}
		gettimeofday(&now, NULL);
		if (cnt > 0)
			printf("%6.2f ", timevaldiff(now, start) / cnt);
		else
			printf("------ ");

		printf("\n");
	}
out:
	printf("\n\n");
	free(batch);
	free(p);

	printf("Results\n");
	printf("   Rounds  : %9u\n", round);
	printf("newNAT ok  : %9lu\n", nat.ok);
	printf("newNAT fail: %9lu\n", nat.fail);
	printf("useNAT ok  : %9lu (out)\n", usenat.ok);
	printf("useNAT fail: %9lu (out)\n", usenat.fail);
	printf("useNAT ok  : %9lu (in)\n", unnat.ok);
	printf("useNAT fail: %9lu (in)\n", unnat.fail);
	printf("RANDOM ok  : %9lu\n", random.ok);
	printf("RANDOM fail: %9lu\n", random.fail);
	printf("ATTACK ok  : %9lu\n", attack.ok);
	printf("ATTACK fail: %9lu\n", attack.fail);
	printf("             ---------\n");
	printf("      Total: %9lu\n",
	       nat.ok + nat.fail +
	       unnat.ok + unnat.fail +
	       usenat.ok + usenat.fail +
	       random.ok + random.fail +
	       attack.ok + attack.fail);

	gettimeofday(&start, NULL);
	printf("\n  Cleanup  : ");
	LibAliasUninit(la);
	gettimeofday(&now, NULL);
	printf("%.2fs\n", timevaldiff(now, start)/1000000l);
	return (0);
}
