/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
 *
 * This file includes significant work done at Cornell University by
 * Bill Nesheim.  That work included by permission.
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
 *	$Id: input.c,v 1.5 1997/02/22 16:00:56 peter Exp $
 */

#ifndef lint
static char sccsid[] = "@(#)input.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

/*
 * IPX Routing Table Management Daemon
 */
#include "defs.h"

struct sockaddr *
ipx_nettosa(net)
union ipx_net net;
{
	static struct sockaddr_ipx sxn;
	
	bzero(&sxn, sizeof (struct sockaddr_ipx));
	sxn.sipx_family = AF_IPX;
	sxn.sipx_len = sizeof (sxn);
	sxn.sipx_addr.x_net = net;
	return( (struct sockaddr *)&sxn);
	
}

/*
 * Process a newly received packet.
 */
void
rip_input(from, size)
	struct sockaddr *from;
	int size;
{
	int newsize;
	int rtchanged = 0;
	struct rt_entry *rt;
	struct netinfo *n;
	struct interface *ifp = 0;
	struct afswitch *afp;
	struct sockaddr_ipx *ipxp;

	ifp = if_ifwithnet(from);
	ipxp = (struct sockaddr_ipx *)from;
	if (ifp == 0) {
		if(ftrace) {
			fprintf(ftrace, "Received bogus packet from %s\n",
				ipxdp_ntoa(&ipxp->sipx_addr));
		}
		return;
	}

	TRACE_INPUT(ifp, from, size);
	if (from->sa_family >= AF_MAX)
		return;
	afp = &afswitch[from->sa_family];
	
	size -= sizeof (u_short)	/* command */;
	n = msg->rip_nets;

	switch (ntohs(msg->rip_cmd)) {

	case RIPCMD_REQUEST:
		if (ipx_hosteq(satoipx_addr(ifp->int_addr), ipxp->sipx_addr))
			return;
		newsize = 0;
		while (size > 0) {
			if (size < sizeof (struct netinfo))
				break;
			size -= sizeof (struct netinfo);

			/* 
			 * A single entry with rip_dst == DSTNETS_ALL and
			 * metric ``infinity'' means ``all routes''.
			 *
			 * XXX According to the IPX RIP spec the metric
			 * and tick fields can be anything. So maybe we
			 * should not check the metric???
			 */
			if (ipx_neteqnn(n->rip_dst, ipx_anynet) &&
		            ntohs(n->rip_metric) == HOPCNT_INFINITY &&
			    size == 0) {
				supply(from, 0, ifp, 0);
				return;
			}
			/*
			 * request for specific nets
			 */
			rt = rtlookup(ipx_nettosa(n->rip_dst));
			if (ftrace) {
				fprintf(ftrace,
					"specific request for %s",
					ipxdp_nettoa(n->rip_dst));
				fprintf(ftrace,
					" yields route %x\n",
					(u_int)rt);
			}
			/*
			 * XXX We break out on the first net that isn't
			 * found. The specs is a bit vague here. I'm not
			 * sure what we should do.
			 */
			if (rt == 0)
				return;
			/* XXX
			 * According to the spec we should not include
			 * information about networks for which the number
			 * of hops is 16.
			 */
			if (rt->rt_metric == (HOPCNT_INFINITY-1))
				return;
			n->rip_metric = htons( rt == 0 ? HOPCNT_INFINITY :
				min(rt->rt_metric+1, HOPCNT_INFINITY));
			n->rip_ticks = htons(rt->rt_ticks+1);

			/*
			 * We use split horizon with a twist. If the requested
			 * net is the directly connected net we supply an
			 * answer. This is so that the host can learn about
			 * the routers on its net.
			 */
			{
				register struct rt_entry *trt = rt;

				while (trt) {
					if ((trt->rt_ifp == ifp) && 
					    !ipx_neteqnn(n->rip_dst, 
						satoipx_addr(ifp->int_addr).x_net))
						return;
					trt = trt->rt_clone;
				}
				n++;
		        	newsize += sizeof (struct netinfo);
			}
		}
		if (newsize > 0) {
			msg->rip_cmd = htons(RIPCMD_RESPONSE);
			newsize += sizeof (u_short);
			/* should check for if with dstaddr(from) first */
			(*afp->af_output)(ripsock, 0, from, newsize);
			TRACE_OUTPUT(ifp, from, newsize);
			if (ftrace) {
				/* XXX This should not happen anymore. */
				if(ifp == 0)
					fprintf(ftrace, "--- ifp = 0\n");
				else
					fprintf(ftrace,
						"request arrived on interface %s\n",
						ifp->int_name);
			}
		}
		return;

	case RIPCMD_RESPONSE:
		/* verify message came from a router */
		if ((*afp->af_portmatch)(from) == 0)
			return;
		(*afp->af_canon)(from);
		/* are we talking to ourselves? */
		if ((ifp = if_ifwithaddr(from)) != 0) {
			rt = rtfind(from);
			if (rt == 0 || (rt->rt_state & RTS_INTERFACE) == 0) {
				addrouteforif(ifp);
				rtchanged = 1;
			} else
				rt->rt_timer = 0;
			return;
		}
		/* Update timer for interface on which the packet arrived.
		 * If from other end of a point-to-point link that isn't
		 * in the routing tables, (re-)add the route.
		 */
		if ((rt = rtfind(from)) && (rt->rt_state & RTS_INTERFACE)) {
			if(ftrace) fprintf(ftrace, "Got route\n");
			rt->rt_timer = 0;
		} else if ((ifp = if_ifwithdstaddr(from)) != 0) {
			if(ftrace) fprintf(ftrace, "Got partner\n");
			addrouteforif(ifp);
			rtchanged = 1;
		}
		for (; size > 0; size -= sizeof (struct netinfo), n++) {
			struct sockaddr *sa;
			if (size < sizeof (struct netinfo))
				break;
			if ((unsigned) ntohs(n->rip_metric) > HOPCNT_INFINITY)
				continue;
			rt = rtfind(sa = ipx_nettosa(n->rip_dst));
			if (rt == 0) {
				if (ntohs(n->rip_metric) == HOPCNT_INFINITY)
					continue;
				rtadd(sa, from, ntohs(n->rip_metric),
					ntohs(n->rip_ticks), 0);
				rtchanged = 1;
				continue;
			}

			/*
			 * A clone is a different route to the same net
			 * with exactly the same cost (ticks and metric).
			 * They must all be recorded because those interfaces
			 * must be handled in the same way as the first route
			 * to that net. ie When using the split horizon
			 * algorithm we must look at these interfaces also.
			 *
			 * Update if from gateway and different,
			 * from anywhere and less ticks or
			 * if same ticks and shorter,
			 * or getting stale and equivalent.
			 */
			if (!equal(from, &rt->rt_router) &&
			    ntohs(n->rip_ticks) == rt->rt_ticks &&
			    ntohs(n->rip_metric) == rt->rt_metric &&
			    ntohs(n->rip_metric) != HOPCNT_INFINITY) {
				register struct rt_entry *trt = rt->rt_clone;

				while (trt) {
					if (equal(from, &trt->rt_router)) {
						trt->rt_timer = 0;
						break;
					}
					trt = trt->rt_clone;
				}
				if (trt == NULL) {
					rtadd_clone(rt, sa, from, 
						    ntohs(n->rip_metric),
						    ntohs(n->rip_ticks), 0);
				}
				continue;
			}
			if ((equal(from, &rt->rt_router) &&
			    ((ntohs(n->rip_ticks) != rt->rt_ticks) ||
			    (ntohs(n->rip_metric) != rt->rt_metric))) ||
			    (ntohs(n->rip_ticks) < rt->rt_ticks) ||
			    ((ntohs(n->rip_ticks) == rt->rt_ticks) &&
			    (ntohs(n->rip_metric) < rt->rt_metric)) ||
			    (rt->rt_timer > (EXPIRE_TIME*2/3) &&
			    rt->rt_metric == ntohs(n->rip_metric) &&
			    ntohs(n->rip_metric) != HOPCNT_INFINITY)) {
				rtchange(rt, from, ntohs(n->rip_metric),
					ntohs(n->rip_ticks));
				if (ntohs(n->rip_metric) == HOPCNT_INFINITY)
					rt->rt_timer = EXPIRE_TIME;
				else
					rt->rt_timer = 0;
				rtchanged = 1;
			} else if (equal(from, &rt->rt_router) &&
				   (ntohs(n->rip_ticks) == rt->rt_ticks) &&
				   (ntohs(n->rip_metric) == rt->rt_metric) &&
				   (ntohs(n->rip_metric) != HOPCNT_INFINITY)) {
				rt->rt_timer = 0;
			}
		}
		if (rtchanged) {
			register struct rthash *rh;
			register struct rt_entry *rt;

			toall(supply, NULL, 1);
			for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++)
				for (rt = rh->rt_forw;
				    rt != (struct rt_entry *)rh;
				    rt = rt->rt_forw)
					rt->rt_state &= ~RTS_CHANGED;
		}

		return;
	}
}
