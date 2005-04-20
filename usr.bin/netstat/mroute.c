/*
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
#include <netinet/ip_mroute.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include "netstat.h"

static void print_bw_meter(struct bw_meter *bw_meter, int *banner_printed);

void
mroutepr(u_long mfcaddr, u_long vifaddr)
{
	struct mfc *mfctable[MFCTBLSIZ];
	struct vif viftable[MAXVIFS];
	struct mfc mfc, *m;
	struct vif *v;
	vifi_t vifi;
	int i;
	int banner_printed;
	int saved_numeric_addr;
	vifi_t maxvif = 0;
	size_t len;

	len = sizeof(mfctable);
	if (sysctlbyname("net.inet.ip.mfctable", mfctable, &len, NULL, 0) < 0) {
		warn("sysctl: net.inet.ip.mfctable");
		/* Compatability with older kernels - candidate for removal */
		if (mfcaddr == 0) {
			printf("No IPv4 multicast routing compiled into this system.\n");
			return;
		}

		kread(mfcaddr, (char *)mfctable, sizeof(mfctable));
	}

	len = sizeof(viftable);
	if (sysctlbyname("net.inet.ip.viftable", viftable, &len, NULL, 0) < 0) {
		warn("sysctl: net.inet.ip.viftable");
		/* Compatability with older kernels - candidate for removal */
		if (vifaddr == 0) {
			printf("No IPv4 multicast routing compiled into this system.\n");
			return;
		}

		kread(vifaddr, (char *)viftable, sizeof(viftable));
	}

	saved_numeric_addr = numeric_addr;
	numeric_addr = 1;

	banner_printed = 0;
	for (vifi = 0, v = viftable; vifi < MAXVIFS; ++vifi, ++v) {
		if (v->v_lcl_addr.s_addr == 0)
			continue;

		maxvif = vifi;
		if (!banner_printed) {
			printf("\nVirtual Interface Table\n"
			       " Vif   Thresh   Rate   Local-Address   "
			       "Remote-Address    Pkts-In   Pkts-Out\n");
			banner_printed = 1;
		}

		printf(" %2u    %6u   %4d   %-15.15s",
					/* opposite math of add_vif() */
		    vifi, v->v_threshold, v->v_rate_limit * 1000 / 1024, 
		    routename(v->v_lcl_addr.s_addr));
		printf(" %-15.15s", (v->v_flags & VIFF_TUNNEL) ?
		    routename(v->v_rmt_addr.s_addr) : "");

		printf(" %9lu  %9lu\n", v->v_pkt_in, v->v_pkt_out);
	}
	if (!banner_printed)
		printf("\nVirtual Interface Table is empty\n");

	banner_printed = 0;
	for (i = 0; i < MFCTBLSIZ; ++i) {
		m = mfctable[i];
		while(m) {
			kread((u_long)m, (char *)&mfc, sizeof mfc);

			if (!banner_printed) {
				printf("\nIPv4 Multicast Forwarding Cache\n"
				       " Origin          Group            "
				       " Packets In-Vif  Out-Vifs:Ttls\n");
				banner_printed = 1;
			}

			printf(" %-15.15s", routename(mfc.mfc_origin.s_addr));
			printf(" %-15.15s", routename(mfc.mfc_mcastgrp.s_addr));
			printf(" %9lu", mfc.mfc_pkt_cnt);
			printf("  %3d   ", mfc.mfc_parent);
			for (vifi = 0; vifi <= maxvif; vifi++) {
				if (mfc.mfc_ttls[vifi] > 0)
					printf(" %u:%u", vifi, 
					       mfc.mfc_ttls[vifi]);
			}
			printf("\n");

			/* Print the bw meter information */
			{
				struct bw_meter bw_meter, *bwm;
				int banner_printed2 = 0;
				
				bwm = mfc.mfc_bw_meter;
				while (bwm) {
				    kread((u_long)bwm, (char *)&bw_meter,
						sizeof bw_meter);
				    print_bw_meter(&bw_meter,
						&banner_printed2);
				    bwm = bw_meter.bm_mfc_next;
				}
#if 0	/* Don't ever print it? */
				if (! banner_printed2)
				    printf("\n  No Bandwidth Meters\n");
#endif
			}

			m = mfc.mfc_next;
		}
	}
	if (!banner_printed)
		printf("\nMulticast Routing Table is empty\n");

	printf("\n");
	numeric_addr = saved_numeric_addr;
}

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
		sprintf(s1, "%llu", bw_meter->bm_measured.b_packets);
	else
		sprintf(s1, "?");
	if (bw_meter->bm_flags & BW_METER_UNIT_BYTES)
		sprintf(s2, "%llu", bw_meter->bm_measured.b_bytes);
	else
		sprintf(s2, "?");
	sprintf(s0, "%lu.%lu|%s|%s",
		bw_meter->bm_start_time.tv_sec,
		bw_meter->bm_start_time.tv_usec,
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
		sprintf(s1, "%llu", bw_meter->bm_threshold.b_packets);
	else
		sprintf(s1, "?");
	if (bw_meter->bm_flags & BW_METER_UNIT_BYTES)
		sprintf(s2, "%llu", bw_meter->bm_threshold.b_bytes);
	else
		sprintf(s2, "?");
	sprintf(s0, "%lu.%lu|%s|%s",
		bw_meter->bm_threshold.b_time.tv_sec,
		bw_meter->bm_threshold.b_time.tv_usec,
		s1, s2);
	printf("  %-30s", s0);

	/* Remaining time */
	timeradd(&bw_meter->bm_start_time,
		 &bw_meter->bm_threshold.b_time, &end);
	if (timercmp(&now, &end, <=)) {
		timersub(&end, &now, &delta);
		sprintf(s3, "%lu.%lu", delta.tv_sec, delta.tv_usec);
	} else {
		/* Negative time */
		timersub(&now, &end, &delta);
		sprintf(s3, "-%lu.%lu", delta.tv_sec, delta.tv_usec);
	}
	printf(" %s", s3);

	printf("\n");
}

void
mrt_stats(u_long mstaddr)
{
	struct mrtstat mrtstat;
	size_t len = sizeof mrtstat;

	if (sysctlbyname("net.inet.ip.mrtstat", &mrtstat, &len,
				NULL, 0) < 0) {
		warn("sysctl: net.inet.ip.mrtstat");
		/* Compatability with older kernels - candidate for removal */
		if (mstaddr == 0) {
			printf("No IPv4 multicast routing compiled into this system.\n");
			return;
		}

		kread(mstaddr, (char *)&mrtstat, sizeof(mrtstat));
	}
	printf("IPv4 multicast forwarding:\n");

#define	p(f, m) if (mrtstat.f || sflag <= 1) \
	printf(m, mrtstat.f, plural(mrtstat.f))
#define	p2(f, m) if (mrtstat.f || sflag <= 1) \
	printf(m, mrtstat.f, plurales(mrtstat.f))

	p(mrts_mfc_lookups, "\t%lu multicast forwarding cache lookup%s\n");
	p2(mrts_mfc_misses, "\t%lu multicast forwarding cache miss%s\n");
	p(mrts_upcalls, "\t%lu upcall%s to mrouted\n");
	p(mrts_upq_ovflw, "\t%lu upcall queue overflow%s\n");
	p(mrts_upq_sockfull,
	    "\t%lu upcall%s dropped due to full socket buffer\n");
	p(mrts_cache_cleanups, "\t%lu cache cleanup%s\n");
	p(mrts_no_route, "\t%lu datagram%s with no route for origin\n");
	p(mrts_bad_tunnel, "\t%lu datagram%s arrived with bad tunneling\n");
	p(mrts_cant_tunnel, "\t%lu datagram%s could not be tunneled\n");
	p(mrts_wrong_if, "\t%lu datagram%s arrived on wrong interface\n");
	p(mrts_drop_sel, "\t%lu datagram%s selectively dropped\n");
	p(mrts_q_overflow, "\t%lu datagram%s dropped due to queue overflow\n");
	p(mrts_pkt2large, "\t%lu datagram%s dropped for being too large\n");

#undef	p2
#undef	p
}
