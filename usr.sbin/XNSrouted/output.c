/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
static char sccsid[] = "@(#)output.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"

/*
 * Apply the function "f" to all non-passive
 * interfaces.  If the interface supports the
 * use of broadcasting use it, otherwise address
 * the output to the known router.
 */
toall(f)
	int (*f)();
{
	register struct interface *ifp;
	register struct sockaddr *dst;
	register int flags;
	extern struct interface *ifnet;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (ifp->int_flags & IFF_PASSIVE)
			continue;
		dst = ifp->int_flags & IFF_BROADCAST ? &ifp->int_broadaddr :
		      ifp->int_flags & IFF_POINTOPOINT ? &ifp->int_dstaddr :
		      &ifp->int_addr;
		flags = ifp->int_flags & IFF_INTERFACE ? MSG_DONTROUTE : 0;
		(*f)(dst, flags, ifp);
	}
}

/*
 * Output a preformed packet.
 */
/*ARGSUSED*/
sndmsg(dst, flags, ifp)
	struct sockaddr *dst;
	int flags;
	struct interface *ifp;
{

	(*afswitch[dst->sa_family].af_output)
		(flags, dst, sizeof (struct rip));
	TRACE_OUTPUT(ifp, dst, sizeof (struct rip));
}

/*
 * Supply dst with the contents of the routing tables.
 * If this won't fit in one packet, chop it up into several.
 */
supply(dst, flags, ifp)
	struct sockaddr *dst;
	int flags;
	struct interface *ifp;
{
	register struct rt_entry *rt;
	register struct rthash *rh;
	register struct netinfo *nn;
	register struct netinfo *n = msg->rip_nets;
	struct rthash *base = hosthash;
	struct sockaddr_ns *sns =  (struct sockaddr_ns *) dst;
	int (*output)() = afswitch[dst->sa_family].af_output;
	int doinghost = 1, size, metric;
	union ns_net net;

	msg->rip_cmd = ntohs(RIPCMD_RESPONSE);
again:
	for (rh = base; rh < &base[ROUTEHASHSIZ]; rh++)
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		size = (char *)n - (char *)msg;
		if (size > MAXPACKETSIZE - sizeof (struct netinfo)) {
			(*output)(flags, dst, size);
			TRACE_OUTPUT(ifp, dst, size);
			n = msg->rip_nets;
		}
		sns = (struct sockaddr_ns *)&rt->rt_dst;
	        if ((rt->rt_flags & (RTF_HOST|RTF_GATEWAY)) == RTF_HOST)
			sns = (struct sockaddr_ns *)&rt->rt_router;
		metric = min(rt->rt_metric + 1, HOPCNT_INFINITY);
		net = sns->sns_addr.x_net;
		/*
		 * Make sure that we don't put out a two net entries
		 * for a pt to pt link (one for the G route, one for the if)
		 * This is a kludge, and won't work if there are lots of nets.
		 */
		for (nn = msg->rip_nets; nn < n; nn++) {
			if (ns_neteqnn(net, nn->rip_dst)) {
				if (metric < ntohs(nn->rip_metric))
					nn->rip_metric = htons(metric);
				goto next;
			}
		}
		n->rip_dst = net;
		n->rip_metric = htons(metric);
		n++;
	next:;
	}
	if (doinghost) {
		doinghost = 0;
		base = nethash;
		goto again;
	}
	if (n != msg->rip_nets) {
		size = (char *)n - (char *)msg;
		(*output)(flags, dst, size);
		TRACE_OUTPUT(ifp, dst, size);
	}
}
