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

/*
 * Print DVMRP multicast routing structures and statistics.
 *
 * MROUTING 1.0
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/igmp.h>
#include <net/route.h>
#define KERNEL 1		/* XXX bogus! */
#include <netinet/ip_mroute.h>
#undef KERNEL

#include <stdio.h>
#include <stdlib.h>
#include "netstat.h"

void
mroutepr(mrpaddr, mfcaddr, vifaddr)
	u_long mrpaddr, mfcaddr, vifaddr;
{
	u_int mrtproto;
	struct mbuf *mfctable[MFCTBLSIZ];
	struct vif viftable[MAXVIFS];
	struct mbuf mb, *m;
	struct mfc smfc;
	register struct vif *v;
	register vifi_t vifi;
	register struct in_addr *grp;
	register int i, n;
	register int banner_printed;
	register int saved_nflag;
	vifi_t maxvif;

	if (mrpaddr == 0) {
		printf("ip_mrtproto: symbol not in namelist\n");
		return;
	}

	kread(mrpaddr, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {

	case 0:
		printf("no multicast routing compiled into this system\n");
		return;

	case IGMP_DVMRP:
		break;

	default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	if (mfcaddr == 0) {
		printf("mfctable: symbol not in namelist\n");
		return;
	}
	if (vifaddr == 0) {
		printf("viftable: symbol not in namelist\n");
		return;
	}

	saved_nflag = nflag;
	nflag = 1;

	kread(vifaddr, (char *)&viftable, sizeof(viftable));
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
		    vifi, v->v_threshold, v->v_rate_limit, 
		    routename(v->v_lcl_addr.s_addr));
		printf(" %-15.15s", (v->v_flags & VIFF_TUNNEL) ?
		    routename(v->v_rmt_addr.s_addr) : "");

		printf(" %9lu  %9lu\n", v->v_pkt_in, v->v_pkt_out);
	}
	if (!banner_printed)
		printf("\nVirtual Interface Table is empty\n");

	kread(mfcaddr, (char *)&mfctable, sizeof(mfctable));
	banner_printed = 0;
	for (i = 0; i < MFCTBLSIZ; ++i) {
		m = mfctable[i];
		while(m) {
			kread((u_long)m, (char *)&mb, sizeof mb);
			m = &mb;

			if (!banner_printed) {
				printf("\nMulticast Forwarding Cache\n"
				       " Origin          Group            "
				       " Packets In-Vif  Out-Vifs:Ttls\n");
				banner_printed = 1;
			}

			kread((u_long)mtod(m, char *), 
			      (char *)&smfc, sizeof smfc);
			printf(" %-15.15s", routename(smfc.mfc_origin.s_addr));
			printf(" %-15.15s", routename(smfc.mfc_mcastgrp.s_addr));
			printf(" %9lu", smfc.mfc_pkt_cnt);
			printf("  %3d   ", smfc.mfc_parent);
			for (vifi = 0; vifi <= maxvif; vifi++) {
				if (smfc.mfc_ttls[vifi] > 0)
					printf(" %u:%u", vifi, 
					       smfc.mfc_ttls[vifi]);
			}
			printf("\n");
			m = m->m_act;
		}
	}
	if (!banner_printed)
		printf("\nMulticast Routing Table is empty\n");

	printf("\n");
	nflag = saved_nflag;
}


void
mrt_stats(mrpaddr, mstaddr)
	u_long mrpaddr, mstaddr;
{
	u_int mrtproto;
	struct mrtstat mrtstat;

	if(mrpaddr == 0) {
		printf("ip_mrtproto: symbol not in namelist\n");
		return;
	}

	kread(mrpaddr, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {
	    case 0:
		printf("no multicast routing compiled into this system\n");
		return;

	    case IGMP_DVMRP:
		break;

	    default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	if (mstaddr == 0) {
		printf("mrtstat: symbol not in namelist\n");
		return;
	}

	kread(mstaddr, (char *)&mrtstat, sizeof(mrtstat));
	printf("multicast forwarding:\n");
	printf(" %10u multicast forwarding cache lookup%s\n",
	  mrtstat.mrts_mfc_lookups, plural(mrtstat.mrts_mfc_lookups));
	printf(" %10u multicast forwarding cache miss%s\n",
	  mrtstat.mrts_mfc_misses, plurales(mrtstat.mrts_mfc_misses));
	printf(" %10u upcall%s to mrouted\n",
	  mrtstat.mrts_upcalls, plural(mrtstat.mrts_upcalls));
	printf(" %10u upcall queue overflow%s\n",
	  mrtstat.mrts_upq_ovflw, plural(mrtstat.mrts_upq_ovflw));
	printf(" %10u upcall%s dropped due to full socket buffer\n",
	  mrtstat.mrts_upq_sockfull, plural(mrtstat.mrts_upq_sockfull));
	printf(" %10u cache cleanup%s\n",
	  mrtstat.mrts_cache_cleanups, plural(mrtstat.mrts_cache_cleanups));
	printf(" %10u datagram%s with no route for origin\n",
	  mrtstat.mrts_no_route, plural(mrtstat.mrts_no_route));
	printf(" %10u datagram%s arrived with bad tunneling\n",
	  mrtstat.mrts_bad_tunnel, plural(mrtstat.mrts_bad_tunnel));
	printf(" %10u datagram%s could not be tunneled\n",
	  mrtstat.mrts_cant_tunnel, plural(mrtstat.mrts_cant_tunnel));
	printf(" %10u datagram%s arrived on wrong interface\n",
	  mrtstat.mrts_wrong_if, plural(mrtstat.mrts_wrong_if));
	printf(" %10u datagram%s selectively dropped\n",
	  mrtstat.mrts_drop_sel, plural(mrtstat.mrts_drop_sel));
	printf(" %10u datagram%s dropped due to queue overflow\n",
	  mrtstat.mrts_q_overflow, plural(mrtstat.mrts_q_overflow));
	printf(" %10u datagram%s dropped for being too large\n",
	  mrtstat.mrts_pkt2large, plural(mrtstat.mrts_pkt2large));
}
