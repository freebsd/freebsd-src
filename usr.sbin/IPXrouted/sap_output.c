/*
 * Copyright (c) 1995 John Hay.  All rights reserved.
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
 *	This product includes software developed by John Hay.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY John Hay AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL John Hay OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: sap_output.c,v 1.1 1995/10/26 21:28:24 julian Exp $
 */

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
void
sap_supply_toall(void)
{
	register struct interface *ifp;
	struct sockaddr dst;
	register struct sockaddr_ipx *ipx_dst;
	register int flags;
	extern struct interface *ifnet;

	ipx_dst = (struct sockaddr_ipx *)&dst;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (ifp->int_flags & IFF_PASSIVE)
			continue;

		dst = ifp->int_flags & IFF_BROADCAST ? ifp->int_broadaddr :
		      ifp->int_flags & IFF_POINTOPOINT ? ifp->int_dstaddr :
		      ifp->int_addr;

		ipx_dst->sipx_addr.x_port = htons(IPXPORT_SAP);

		flags = ifp->int_flags & IFF_INTERFACE ? MSG_DONTROUTE : 0;
		sap_supply(&dst, flags, ifp, SAP_WILDCARD);
	}
}

void 
sapsndmsg(dst, flags, ifp)
	struct sockaddr *dst;
	int flags;
	struct interface *ifp;
{
	struct sockaddr t_dst;
	struct sockaddr_ipx *ipx_dst;

	t_dst = *dst;
	ipx_dst = (struct sockaddr_ipx *)&t_dst;

	if (ipx_dst->sipx_addr.x_port == 0)
		ipx_dst->sipx_addr.x_port = htons(IPXPORT_SAP);

        (*afswitch[dst->sa_family].af_output)
		(sapsock, flags, &t_dst, 
		sizeof (struct sap_packet) + sizeof(u_short));
	TRACE_SAP_OUTPUT(ifp, &t_dst, 
			 sizeof (struct sap_packet) + sizeof(u_short));
}

/*
 * Supply dst with the contents of the SAP tables. If the ServType ==
 * SAP_WILDCARD (0xFFFF) supply the whole table, otherwise only the
 * services that are of ServType. If this won't fit in one packet, chop 
 * it up into several.
 *
 * This must be done using the split horizon algorithm.
 * 1. Don't send SAP info to the interface from where it was received.
 * 2. If a service is received from more than one interface and the cost is
 *    the same, don't publish it on either interface. I am calling this
 *    clones.
 */
void
sap_supply(dst, flags, ifp, ServType)
	struct sockaddr *dst;
	int flags;
	struct interface *ifp;
	int ServType;
{
	register struct sap_entry *sap;
	register struct sap_entry *csap; /* Clone route */
	register struct sap_hash *sh;
	register struct sap_info *n = sap_msg->sap;
	struct sap_hash *base = sap_head;
	struct sockaddr_ipx *sipx =  (struct sockaddr_ipx *) dst;
	af_output_t *output = afswitch[dst->sa_family].af_output;
	int size, metric;

	if (sipx->sipx_port == 0)
		sipx->sipx_port = htons(IPXPORT_SAP);

	sap_msg->sap_cmd = ntohs(SAP_RESP);

	for (sh = base; sh < &base[SAPHASHSIZ]; sh++)
	for (sap = sh->forw; sap != (struct sap_entry *)sh; sap = sap->forw) {
		size = (char *)n - (char *)sap_msg;
		if (size > MAXPACKETSIZE - sizeof (struct sap_info)) {
			(*output)(sapsock, flags, dst, size);
			TRACE_SAP_OUTPUT(ifp, dst, size);
			n = sap_msg->sap;
		}

		/*
		 * Check for the servicetype except if the ServType is
		 * a wildcard (0xFFFF).
		 */
		if ((ServType != SAP_WILDCARD) &&
		    (ServType != sap->sap.ServType))
			continue;

		/*
		 * This should do rule one and two of the split horizon
		 * algorithm.
		 */
		if (sap->ifp == ifp)
			continue;

		/*
		 * Rule 2.
		 * Look if we have clones (different routes to the same
		 * place with exactly the same cost).
		 *
		 * We should not publish on any of the clone interfaces.
		 */
		csap = sap->clone;
		while (csap) {
			if (csap->ifp == ifp)
				continue;
			csap = csap->clone;
		}

		/*
		 * Don't advertise services with more than 15 hops. It
		 * will be confused with a service that has gone down.
		 */
		if (ntohs(sap->sap.hops) == (HOPCNT_INFINITY - 1))
			continue;
		metric = min(ntohs(sap->sap.hops) + 1, HOPCNT_INFINITY);

		*n = sap->sap;
		n->hops = htons(metric);
		n++;
	}
	if (n != sap_msg->sap) {
		size = (char *)n - (char *)sap_msg;
		(*output)(sapsock, flags, dst, size);
		TRACE_SAP_OUTPUT(ifp, dst, size);
	}
}

