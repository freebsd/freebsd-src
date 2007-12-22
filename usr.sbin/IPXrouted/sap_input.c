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
 * $FreeBSD$
 */

/*
 * IPX Routing Table Management Daemon
 */
#include "defs.h"

int dognreply = 1;

/*
 * Process a newly received packet.
 */
void
sap_input(from, size)
	struct sockaddr *from;
	int size;
{
	int newsize;
	int sapchanged = 0;
	struct sap_entry *sap;
	struct sap_info *n;
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

	if (ftrace) 
		dumpsappacket(ftrace, "received", from, (char *)sap_msg , size);

	if (from->sa_family >= AF_MAX)
		return;
	afp = &afswitch[from->sa_family];
	
	size -= sizeof (u_short)	/* command */;
	n = sap_msg->sap;

	switch (ntohs(sap_msg->sap_cmd)) {

	case SAP_REQ_NEAR:
		if (ftrace)
			fprintf(ftrace, "Received a sap REQ_NEAR packet.\n");
		if (!dognreply)
			return;
		sap = sap_nearestserver(n->ServType, ifp);
		if (sap == NULL)
			return;
		sap_msg->sap_cmd = htons(SAP_RESP_NEAR);
		*n = sap->sap;
		n->hops = htons(ntohs(n->hops) + 1);
		if (ntohs(n->hops) >= HOPCNT_INFINITY)
			return;

		newsize = sizeof(struct sap_info) + sizeof(struct sap_packet);
		(*afp->af_output)(sapsock, 0, from, newsize);
		if (ftrace) {
			fprintf(ftrace, "sap_nearestserver %X %s returned:\n",
				ntohs(n->ServType),
				ifp->int_name);
			fprintf(ftrace, "  service %04X %-20.20s "
					"addr %s.%04X metric %d\n",
					ntohs(sap->sap.ServType),
					sap->sap.ServName,
					ipxdp_ntoa(&sap->sap.ipx),
					ntohs(sap->sap.ipx.x_port),
					ntohs(sap->sap.hops));
		}
		return;

	case SAP_REQ:
		if (ftrace)
			fprintf(ftrace, "Received a sap REQ packet.\n");

		sap_supply(from, 0, ifp, n->ServType, 0);
		return;

	case SAP_RESP_NEAR:
		/* XXX We do nothing here, for the moment.
		 * Maybe we should check if the service is in our table?
		 *
		 */
		if (ftrace)
			fprintf(ftrace, "Received a sap RESP_NEAR packet.\n");

		return;

	case SAP_RESP:
		if (ftrace)
			fprintf(ftrace, "Received a sap RESP packet.\n");

		(*afp->af_canon)(from);

		for (; size > 0; size -= sizeof (struct sap_info), n++) {
			if (size < sizeof (struct netinfo))
				break;
			/*
			 * The idea here is that if the hop count is more
			 * than INFINITY it is bogus and should be discarded.
			 * If it is equal to INFINITY it is a message to say
			 * that a service went down. If we don't allready
			 * have it in our tables discard it. Otherwise
			 * update our table and set the timer to EXPIRE_TIME
			 * so that it is removed next time we go through the
			 * tables.
			 */
			if (ntohs(n->hops) > HOPCNT_INFINITY)
				continue;
			sap = sap_lookup(n->ServType, n->ServName);
			if (sap == 0) {
				if (ntohs(n->hops) == HOPCNT_INFINITY)
					continue;
				sap_add(n, from);
				sapchanged = 1;
				continue;
			}

			/*
			 * A clone is a different route to the same service
			 * with exactly the same cost (metric).
			 * They must all be recorded because those interfaces
			 * must be handled in the same way as the first route
			 * to that service. ie When using the split horizon
			 * algorithm we must look at these interfaces also.
			 *
			 * Update if from gateway and different,
			 * from anywhere and less hops or
			 * getting stale and equivalent.
			 */
			if (((ifp != sap->ifp) ||
			     !equal(&sap->source, from)) &&
			    (n->hops == sap->sap.hops) &&
			    (ntohs(n->hops) != HOPCNT_INFINITY)) {
				register struct sap_entry *tsap = sap->clone;

				while (tsap) {
					if ((ifp == tsap->ifp) &&
					    equal(&tsap->source, from)) {
						tsap->timer = 0;
						break;
					}
					tsap = tsap->clone;
				}
				if (tsap == NULL) {
					sap_add_clone(sap, n, from);
				}
				continue;
			}
			if ((ifp == sap->ifp) &&
			    equal(&sap->source, from) &&
			    (ntohs(n->hops) == ntohs(sap->sap.hops)))
				sap->timer = 0;
			else if (((ifp == sap->ifp) &&
				  equal(&sap->source, from) &&
				  (n->hops != sap->sap.hops)) ||
				 (ntohs(n->hops) < ntohs(sap->sap.hops)) ||
				 (sap->timer > (EXPIRE_TIME*2/3) &&
				  ntohs(sap->sap.hops) == ntohs(n->hops) &&
				  ntohs(n->hops) != HOPCNT_INFINITY)) {
				sap_change(sap, n, from);
				sapchanged = 1;
			}
		}
		if (sapchanged) {
			register struct sap_entry *sap;
			register struct sap_hash *sh;
			sap_supply_toall(1);

			for (sh = sap_head; sh < &sap_head[SAPHASHSIZ]; sh++)
				for (sap = sh->forw;
				    sap != (struct sap_entry *)sh;
				    sap = sap->forw)
					sap->state &= ~RTS_CHANGED;
		}
		return;
	}
}
