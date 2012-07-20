/*-
 * Copyright (c) 2012, Adrian Chadd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */

#include "opt_ah.h"

#include <sys/types.h>
#include <sys/file.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_var.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "ah.h"
#include "ah_desc.h"
#include "net80211/ieee80211_ioctl.h"
#include "net80211/ieee80211_radiotap.h"
#include "if_athioctl.h"
#include "if_athrate.h"

#include "ath_rate/sample/sample.h"

struct ath_ratestats {
	int s;
	struct ath_rateioctl re;
};

static inline int
dot11rate(struct ath_rateioctl_rt *rt, int rix)
{

	if (rt->ratecode[rix] & IEEE80211_RATE_MCS)
		return rt->ratecode[rix] & ~(IEEE80211_RATE_MCS);
	else
		return (rt->ratecode[rix] / 2);
}

static const char *
dot11str(struct ath_rateioctl_rt *rt, int rix)
{
	if (rix == -1)
		return "";
	else if (rt->ratecode[rix] & IEEE80211_RATE_MCS)
		return "MCS";
	else
		return " Mb";
}

static void
ath_sample_stats(struct ath_ratestats *r, struct ath_rateioctl_rt *rt,
    struct sample_node *sn)
{
	uint32_t mask;
	int rix, y;

	printf("static_rix (%d) ratemask 0x%x\n",
	    sn->static_rix,
	    sn->ratemask);

	for (y = 0; y < NUM_PACKET_SIZE_BINS; y++) {
		printf("[%4u] cur rate %d %s since switch: "
		    "packets %d ticks %u\n",
		    bin_to_size(y),
		    dot11rate(rt, sn->current_rix[y]),
		    dot11str(rt, sn->current_rix[y]),
		    sn->packets_since_switch[y],
		    sn->ticks_since_switch[y]);

		printf("[%4u] last sample (%d %s) cur sample (%d %s) "
		    "packets sent %d\n",
		    bin_to_size(y),
		    dot11rate(rt, sn->last_sample_rix[y]),
		    dot11str(rt, sn->last_sample_rix[y]),
		    dot11rate(rt, sn->current_sample_rix[y]),
		    dot11str(rt, sn->current_sample_rix[y]),
		    sn->packets_sent[y]);

		printf("[%4u] packets since sample %d sample tt %u\n",
		    bin_to_size(y),
		    sn->packets_since_sample[y],
		    sn->sample_tt[y]);
	}
	for (mask = sn->ratemask, rix = 0; mask != 0; mask >>= 1, rix++) {
		if ((mask & 1) == 0)
				continue;
		for (y = 0; y < NUM_PACKET_SIZE_BINS; y++) {
			if (sn->stats[y][rix].total_packets == 0)
				continue;
			printf("[%2u %s:%4u] %8ju:%-8ju (%3d%%) "
			    "(EWMA %3d.%1d%%) T %8ju F %4d avg %5u last %u\n",
			    dot11rate(rt, rix),
			    dot11str(rt, rix),
			    bin_to_size(y),
			    (uintmax_t) sn->stats[y][rix].total_packets,
			    (uintmax_t) sn->stats[y][rix].packets_acked,
			    (int) ((sn->stats[y][rix].packets_acked * 100ULL) /
			     sn->stats[y][rix].total_packets),
			    sn->stats[y][rix].ewma_pct / 10,
			    sn->stats[y][rix].ewma_pct % 10,
			    (uintmax_t) sn->stats[y][rix].tries,
			    sn->stats[y][rix].successive_failures,
			    sn->stats[y][rix].average_tx_time,
			    sn->stats[y][rix].last_tx);
		}
	}
}

static void
ath_setifname(struct ath_ratestats *r, const char *ifname)
{

	strncpy(r->re.if_name, ifname, sizeof (r->re.if_name));
}

static void
ath_setsta(struct ath_ratestats *r, const char *mac)
{

	memcpy(&r->re.is_u.macaddr, mac, sizeof(r->re.is_u.macaddr));
}

static void
ath_rate_ioctl(struct ath_ratestats *r)
{
	printf("ether: %x:%x:%x:%x:%x:%x\n",
	    r->re.is_u.macaddr[0],
	    r->re.is_u.macaddr[1],
	    r->re.is_u.macaddr[2],
	    r->re.is_u.macaddr[3],
	    r->re.is_u.macaddr[4],
	    r->re.is_u.macaddr[5]);
	if (ioctl(r->s, SIOCGATHNODERATESTATS, &r->re) < 0)
		err(1, "ioctl");
}

#define	STATS_BUF_SIZE	8192
int
main(int argc, const char *argv[])
{
	struct ath_ratestats r;
	struct ether_addr *e;
	uint8_t *buf;
	struct ath_rateioctl_tlv *av;
	struct sample_node *sn = NULL;
	struct ath_rateioctl_rt *rt = NULL;

	buf = calloc(1, STATS_BUF_SIZE);
	if (buf == NULL)
		err(1, "calloc");

	bzero(&r, sizeof(r));
	r.s = socket(AF_INET, SOCK_DGRAM, 0);
	if (r.s < 0) {
		err(1, "socket");
	}
	/* XXX error check */
	ath_setifname(&r, "ath0");

	e = ether_aton(argv[1]);

	/* caddr_t ? */
	r.re.buf = buf;
	r.re.len = STATS_BUF_SIZE;

	ath_setsta(&r, e->octet);
	ath_rate_ioctl(&r);

	/*
	 * For now, hard-code the TLV order and contents.  Ew!
	 */
	av = (struct ath_rateioctl_tlv *) buf;
	if (av->tlv_id != ATH_RATE_TLV_RATETABLE) {
		fprintf(stderr, "unexpected rate control TLV (got 0x%x, "
		    "expected 0x%x\n",
		    av->tlv_id,
		    ATH_RATE_TLV_RATETABLE);
		exit(127);
	}
	if (av->tlv_len != sizeof(struct ath_rateioctl_rt)) {
		fprintf(stderr, "unexpected TLV len (got %d bytes, "
		    "expected %d bytes\n",
		    av->tlv_len,
		    sizeof(struct ath_rateioctl_rt));
		exit(127);
	}
	rt = (void *) (buf + sizeof(struct ath_rateioctl_tlv));

	/* Next */
	av = (void *) (buf + sizeof(struct ath_rateioctl_tlv) +
	    sizeof(struct ath_rateioctl_rt));
	if (av->tlv_id != ATH_RATE_TLV_SAMPLENODE) {
		fprintf(stderr, "unexpected rate control TLV (got 0x%x, "
		    "expected 0x%x\n",
		    av->tlv_id,
		    ATH_RATE_TLV_SAMPLENODE);
		exit(127);
	}
	if (av->tlv_len != sizeof(struct sample_node)) {
		fprintf(stderr, "unexpected TLV len (got %d bytes, "
		    "expected %d bytes\n",
		    av->tlv_len,
		    sizeof(struct sample_node));
		exit(127);
	}
	sn = (void *) (buf + sizeof(struct ath_rateioctl_tlv) +
	    sizeof(struct ath_rateioctl_rt) +
	    sizeof(struct ath_rateioctl_tlv));

	ath_sample_stats(&r, rt, sn);
}

