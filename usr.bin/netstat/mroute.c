/*-
 * Copyright (c) 1989 Stephen Deering
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mroute.c	8.2 (Berkeley) 4/28/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Print multicast routing structures and statistics.
 *
 * MROUTING 1.0
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/protosw.h>
#include <sys/mbuf.h>
#include <sys/time.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/igmp.h>
#include <net/route.h>

#define _KERNEL 1
#include <netinet/ip_mroute.h>
#undef _KERNEL

#include <err.h>
#include <nlist.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "netstat.h"

/*
 * kvm(3) bindings for every needed symbol
 */
static struct nlist mrl[] = {
#define	N_MRTSTAT	0
	{ .n_name = "_mrtstat" },
#define	N_MFCHASHTBL	1
	{ .n_name = "_mfchashtbl" },
#define	N_VIFTABLE	2
	{ .n_name = "_viftable" },
#define	N_MFCTABLESIZE	3
	{ .n_name = "_mfctablesize" },
	{ .n_name = NULL },
};

static void	print_bw_meter(struct bw_meter *, int *);
static void	print_mfc(struct mfc *, int, int *);

static void
print_bw_meter(struct bw_meter *bw_meter, int *banner_printed)
{
	char s0[256], s1[256], s2[256], s3[256];
	struct timeval now, end, delta;

	gettimeofday(&now, NULL);

	if (! *banner_printed) {
		printf(" Bandwidth Meters\n");
		printf("  %-30s", "Measured(Start|Packets|Bytes)");
		printf(" %s", "Type");
		printf("  %-30s", "Thresh(Interval|Packets|Bytes)");
		printf(" Remain");
		printf("\n");
		*banner_printed = 1;
	}

	/* The measured values */
	if (bw_meter->bm_flags & BW_METER_UNIT_PACKETS)
		sprintf(s1, "%ju", (uintmax_t)bw_meter->bm_measured.b_packets);
	else
		sprintf(s1, "?");
	if (bw_meter->bm_flags & BW_METER_UNIT_BYTES)
		sprintf(s2, "%ju", (uintmax_t)bw_meter->bm_measured.b_bytes);
	else
		sprintf(s2, "?");
	sprintf(s0, "%lu.%lu|%s|%s",
		(u_long)bw_meter->bm_start_time.tv_sec,
		(u_long)bw_meter->bm_start_time.tv_usec,
		s1, s2);
	printf("  %-30s", s0);

	/* The type of entry */
	sprintf(s0, "%s", "?");
	if (bw_meter->bm_flags & BW_METER_GEQ)
		sprintf(s0, "%s", ">=");
	else if (bw_meter->bm_flags & BW_METER_LEQ)
		sprintf(s0, "%s", "<=");
	printf("  %-3s", s0);

	/* The threshold values */
	if (bw_meter->bm_flags & BW_METER_UNIT_PACKETS)
		sprintf(s1, "%ju", (uintmax_t)bw_meter->bm_threshold.b_packets);
	else
		sprintf(s1, "?");
	if (bw_meter->bm_flags & BW_METER_UNIT_BYTES)
		sprintf(s2, "%ju", (uintmax_t)bw_meter->bm_threshold.b_bytes);
	else
		sprintf(s2, "?");
	sprintf(s0, "%lu.%lu|%s|%s",
		(u_long)bw_meter->bm_threshold.b_time.tv_sec,
		(u_long)bw_meter->bm_threshold.b_time.tv_usec,
		s1, s2);
	printf("  %-30s", s0);

	/* Remaining time */
	timeradd(&bw_meter->bm_start_time,
		 &bw_meter->bm_threshold.b_time, &end);
	if (timercmp(&now, &end, <=)) {
		timersub(&end, &now, &delta);
		sprintf(s3, "%lu.%lu",
			(u_long)delta.tv_sec,
			(u_long)delta.tv_usec);
	} else {
		/* Negative time */
		timersub(&now, &end, &delta);
		sprintf(s3, "-%lu.%lu",
			(u_long)delta.tv_sec,
			(u_long)delta.tv_usec);
	}
	printf(" %s", s3);

	printf("\n");
}

static void
print_mfc(struct mfc *m, int maxvif, int *banner_printed)
{
	struct bw_meter bw_meter, *bwm;
	int bw_banner_printed;
	int error;
	vifi_t vifi;

	bw_banner_printed = 0;

	if (! *banner_printed) {
		printf("\nIPv4 Multicast Forwarding Table\n"
		       " Origin          Group            "
		       " Packets In-Vif  Out-Vifs:Ttls\n");
		*banner_printed = 1;
	}

	printf(" %-15.15s", routename(m->mfc_origin.s_addr));
	printf(" %-15.15s", routename(m->mfc_mcastgrp.s_addr));
	printf(" %9lu", m->mfc_pkt_cnt);
	printf("  %3d   ", m->mfc_parent);
	for (vifi = 0; vifi <= maxvif; vifi++) {
		if (m->mfc_ttls[vifi] > 0)
			printf(" %u:%u", vifi, m->mfc_ttls[vifi]);
	}
	printf("\n");

	/*
	 * XXX We break the rules and try to use KVM to read the
	 * bandwidth meters, they are not retrievable via sysctl yet.
	 */
	bwm = m->mfc_bw_meter;
	while (bwm != NULL) {
		error = kread((u_long)bwm, (char *)&bw_meter,
		    sizeof(bw_meter));
		if (error)
			break;
		print_bw_meter(&bw_meter, &bw_banner_printed);
		bwm = bw_meter.bm_mfc_next;
	}
}

void
mroutepr()
{
	struct vif viftable[MAXVIFS];
	struct vif *v;
	struct mfc *m;
	u_long pmfchashtbl, pmfctablesize, pviftbl;
	int banner_printed;
	int saved_numeric_addr;
	size_t len;
	vifi_t vifi, maxvif;

	saved_numeric_addr = numeric_addr;
	numeric_addr = 1;

	/*
	 * TODO:
	 * The VIF table will move to hanging off the struct if_info for
	 * each IPv4 configured interface. Currently it is statically
	 * allocated, and retrieved either using KVM or an opaque SYSCTL.
	 *
	 * This can't happen until the API documented in multicast(4)
	 * is itself refactored. The historical reason why VIFs use
	 * a separate ifindex space is entirely due to the legacy
	 * capability of the MROUTING code to create IPIP tunnels on
	 * the fly to support DVMRP. When gif(4) became available, this
	 * functionality was deprecated, as PIM does not use it.
	 */
	maxvif = 0;
	pmfchashtbl = pmfctablesize = pviftbl = 0;

	len = sizeof(viftable);
	if (live) {
		if (sysctlbyname("net.inet.ip.viftable", viftable, &len, NULL,
		    0) < 0) {
			warn("sysctl: net.inet.ip.viftable");
			return;
		}
	} else {
		kresolve_list(mrl);
		pmfchashtbl = mrl[N_MFCHASHTBL].n_value;
		pmfctablesize = mrl[N_MFCTABLESIZE].n_value;
		pviftbl = mrl[N_VIFTABLE].n_value;

		if (pmfchashtbl == 0 || pmfctablesize == 0 || pviftbl == 0) {
			fprintf(stderr, "No IPv4 MROUTING kernel support.\n");
			return;
		}

		kread(pviftbl, (char *)viftable, sizeof(viftable));
	}

	banner_printed = 0;
	for (vifi = 0, v = viftable; vifi < MAXVIFS; ++vifi, ++v) {
		if (v->v_lcl_addr.s_addr == 0)
			continue;

		maxvif = vifi;
		if (!banner_printed) {
			printf("\nIPv4 Virtual Interface Table\n"
			       " Vif   Thresh   Local-Address   "
			       "Remote-Address    Pkts-In   Pkts-Out\n");
			banner_printed = 1;
		}

		printf(" %2u    %6u   %-15.15s",
					/* opposite math of add_vif() */
		    vifi, v->v_threshold,
		    routename(v->v_lcl_addr.s_addr));
		printf(" %-15.15s", (v->v_flags & VIFF_TUNNEL) ?
		    routename(v->v_rmt_addr.s_addr) : "");

		printf(" %9lu  %9lu\n", v->v_pkt_in, v->v_pkt_out);
	}
	if (!banner_printed)
		printf("\nIPv4 Virtual Interface Table is empty\n");

	banner_printed = 0;

	/*
	 * TODO:
	 * The MFC table will move into the AF_INET radix trie in future.
	 * In 8.x, it becomes a dynamically allocated structure referenced
	 * by a hashed LIST, allowing more than 256 entries w/o kernel tuning.
	 *
	 * If retrieved via opaque SYSCTL, the kernel will coalesce it into
	 * a static table for us.
	 * If retrieved via KVM, the hash list pointers must be followed.
	 */
	if (live) {
		struct mfc *mfctable;

		len = 0;
		if (sysctlbyname("net.inet.ip.mfctable", NULL, &len, NULL,
		    0) < 0) {
			warn("sysctl: net.inet.ip.mfctable");
			return;
		}

		mfctable = malloc(len);
		if (mfctable == NULL) {
			warnx("malloc %lu bytes", (u_long)len);
			return;
		}
		if (sysctlbyname("net.inet.ip.mfctable", mfctable, &len, NULL,
		    0) < 0) {
			free(mfctable);
			warn("sysctl: net.inet.ip.mfctable");
			return;
		}

		m = mfctable;
		while (len >= sizeof(*m)) {
			print_mfc(m++, maxvif, &banner_printed);
			len -= sizeof(*m);
		}
		if (len != 0)
			warnx("print_mfc: %lu trailing bytes", (u_long)len);

		free(mfctable);
	} else {
		LIST_HEAD(, mfc) *mfchashtbl;
		u_long i, mfctablesize;
		struct mfc mfc;
		int error;

		error = kread(pmfctablesize, (char *)&mfctablesize,
		    sizeof(u_long));
		if (error) {
			warn("kread: mfctablesize");
			return;
		}

		len = sizeof(*mfchashtbl) * mfctablesize;
		mfchashtbl = malloc(len);
		if (mfchashtbl == NULL) {
			warnx("malloc %lu bytes", (u_long)len);
			return;
		}
		kread(pmfchashtbl, (char *)&mfchashtbl, len);

		for (i = 0; i < mfctablesize; i++) {
			LIST_FOREACH(m, &mfchashtbl[i], mfc_hash) {
				kread((u_long)m, (char *)&mfc, sizeof(mfc));
				print_mfc(m, maxvif, &banner_printed);
			}
		}

		free(mfchashtbl);
	}

	if (!banner_printed)
		printf("\nIPv4 Multicast Forwarding Table is empty\n");

	printf("\n");
	numeric_addr = saved_numeric_addr;
}

void
mrt_stats()
{
	struct mrtstat mrtstat;
	u_long mstaddr;

	kresolve_list(mrl);
	mstaddr = mrl[N_MRTSTAT].n_value;

	if (mstaddr == 0) {
		fprintf(stderr, "No IPv4 MROUTING kernel support.\n");
		return;
	}

	if (fetch_stats("net.inet.ip.mrtstat", mstaddr, &mrtstat,
	    sizeof(mrtstat), kread_counters) != 0)
		return;

	printf("IPv4 multicast forwarding:\n");

#define	p(f, m) if (mrtstat.f || sflag <= 1) \
	printf(m, (uintmax_t)mrtstat.f, plural(mrtstat.f))
#define	p2(f, m) if (mrtstat.f || sflag <= 1) \
	printf(m, (uintmax_t)mrtstat.f, plurales(mrtstat.f))

	p(mrts_mfc_lookups, "\t%ju multicast forwarding cache lookup%s\n");
	p2(mrts_mfc_misses, "\t%ju multicast forwarding cache miss%s\n");
	p(mrts_upcalls, "\t%ju upcall%s to multicast routing daemon\n");
	p(mrts_upq_ovflw, "\t%ju upcall queue overflow%s\n");
	p(mrts_upq_sockfull,
	    "\t%ju upcall%s dropped due to full socket buffer\n");
	p(mrts_cache_cleanups, "\t%ju cache cleanup%s\n");
	p(mrts_no_route, "\t%ju datagram%s with no route for origin\n");
	p(mrts_bad_tunnel, "\t%ju datagram%s arrived with bad tunneling\n");
	p(mrts_cant_tunnel, "\t%ju datagram%s could not be tunneled\n");
	p(mrts_wrong_if, "\t%ju datagram%s arrived on wrong interface\n");
	p(mrts_drop_sel, "\t%ju datagram%s selectively dropped\n");
	p(mrts_q_overflow, "\t%ju datagram%s dropped due to queue overflow\n");
	p(mrts_pkt2large, "\t%ju datagram%s dropped for being too large\n");

#undef	p2
#undef	p
}
