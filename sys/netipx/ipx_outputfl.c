/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ipx_outputfl.c
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/route.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_var.h>

#ifdef vax
#include <machine/mtpr.h>
#endif

static int ipx_copy_output = 0;

int
ipx_outputfl(m0, ro, flags)
	struct mbuf *m0;
	struct route *ro;
	int flags;
{
	register struct ipx *ipx = mtod(m0, struct ipx *);
	register struct ifnet *ifp = NULL;
	int error = 0;
	struct sockaddr_ipx *dst;
	struct route ipxroute;

	/*
	 * Route packet.
	 */
	if (ro == NULL) {
		ro = &ipxroute;
		bzero((caddr_t)ro, sizeof(*ro));
	}
	dst = (struct sockaddr_ipx *)&ro->ro_dst;
	if (ro->ro_rt == NULL) {
		dst->sipx_family = AF_IPX;
		dst->sipx_len = sizeof(*dst);
		dst->sipx_addr = ipx->ipx_dna;
		dst->sipx_addr.x_port = 0;
		/*
		 * If routing to interface only,
		 * short circuit routing lookup.
		 */
		if (flags & IPX_ROUTETOIF) {
			struct ipx_ifaddr *ia = ipx_iaonnetof(&ipx->ipx_dna);

			if (ia == NULL) {
				ipxstat.ipxs_noroute++;
				error = ENETUNREACH;
				goto bad;
			}
			ifp = ia->ia_ifp;
			goto gotif;
		}
		rtalloc(ro);
	} else if ((ro->ro_rt->rt_flags & RTF_UP) == 0) {
		/*
		 * The old route has gone away; try for a new one.
		 */
		rtfree(ro->ro_rt);
		ro->ro_rt = NULL;
		rtalloc(ro);
	}
	if (ro->ro_rt == NULL || (ifp = ro->ro_rt->rt_ifp) == NULL) {
		ipxstat.ipxs_noroute++;
		error = ENETUNREACH;
		goto bad;
	}
	ro->ro_rt->rt_use++;
	if (ro->ro_rt->rt_flags & (RTF_GATEWAY|RTF_HOST))
		dst = (struct sockaddr_ipx *)ro->ro_rt->rt_gateway;
gotif:
	/*
	 * Look for multicast addresses and
	 * and verify user is allowed to send
	 * such a packet.
	 */
	if (dst->sipx_addr.x_host.c_host[0]&1) {
		if ((ifp->if_flags & (IFF_BROADCAST | IFF_LOOPBACK)) == 0) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if ((flags & IPX_ALLOWBROADCAST) == 0) {
			error = EACCES;
			goto bad;
		}
		m0->m_flags |= M_BCAST;
	}

	if (htons(ipx->ipx_len) <= ifp->if_mtu) {
		ipxstat.ipxs_localout++;
		if (ipx_copy_output) {
			ipx_watch_output(m0, ifp);
		}
		error = (*ifp->if_output)(ifp, m0,
					(struct sockaddr *)dst, ro->ro_rt);
		goto done;
	} else {
		ipxstat.ipxs_mtutoosmall++;
		error = EMSGSIZE;
	}
bad:
	if (ipx_copy_output) {
		ipx_watch_output(m0, ifp);
	}
	m_freem(m0);
done:
	if (ro == &ipxroute && (flags & IPX_ROUTETOIF) == 0 &&
	    ro->ro_rt != NULL) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = NULL;
	}
	return (error);
}

/*
 * This will broadcast the type 20 (Netbios) packet to all the interfaces
 * that have ipx configured and isn't in the list yet.
 */
int
ipx_output_type20(m)
	struct mbuf *m;
{
	register struct ipx *ipx;
	union ipx_net *nbnet;
	struct ipx_ifaddr *ia, *tia = NULL;
	int error = 0;
	struct mbuf *m1;
	int i;
	struct ifnet *ifp;
	struct sockaddr_ipx dst;

	/*
	 * We have to get to the 32 bytes after the ipx header also, so
	 * that we can fill in the network address of the receiving
	 * interface.
	 */
	if ((m->m_flags & M_EXT || m->m_len < (sizeof(struct ipx) + 32)) &&
	    (m = m_pullup(m, sizeof(struct ipx) + 32)) == NULL) {
		ipxstat.ipxs_toosmall++;
		return (0);
	}
	ipx = mtod(m, struct ipx *);
	nbnet = (union ipx_net *)(ipx + 1);

	if (ipx->ipx_tc >= 8)
		goto bad;
	/*
	 * Now see if we have already seen this.
	 */
	for (ia = ipx_ifaddr; ia != NULL; ia = ia->ia_next)
		if(ia->ia_ifa.ifa_ifp == m->m_pkthdr.rcvif) {
			if(tia == NULL)
				tia = ia;

			for (i=0;i<ipx->ipx_tc;i++,nbnet++)
				if(ipx_neteqnn(ia->ia_addr.sipx_addr.x_net,
							*nbnet))
					goto bad;
		}
	/*
	 * Don't route the packet if the interface where it come from
	 * does not have an IPX address.
	 */
	if(tia == NULL)
		goto bad;

	/*
	 * Add our receiving interface to the list.
	 */
        nbnet = (union ipx_net *)(ipx + 1);
	nbnet += ipx->ipx_tc;
	*nbnet = tia->ia_addr.sipx_addr.x_net;

	/*
	 * Increment the hop count.
	 */
	ipx->ipx_tc++;
	ipxstat.ipxs_forward++;

	/*
	 * Send to all directly connected ifaces not in list and
	 * not to the one it came from.
	 */
	m->m_flags &= ~M_BCAST;
	bzero(&dst, sizeof(dst));
	dst.sipx_family = AF_IPX;
	dst.sipx_len = 12;
	dst.sipx_addr.x_host = ipx_broadhost;

	for (ia = ipx_ifaddr; ia != NULL; ia = ia->ia_next)
		if(ia->ia_ifa.ifa_ifp != m->m_pkthdr.rcvif) {
        		nbnet = (union ipx_net *)(ipx + 1);
			for (i=0;i<ipx->ipx_tc;i++,nbnet++)
				if(ipx_neteqnn(ia->ia_addr.sipx_addr.x_net,
							*nbnet))
					goto skip_this;

			/*
			 * Insert the net address of the dest net and
			 * calculate the new checksum if needed.
			 */
			ifp = ia->ia_ifa.ifa_ifp;
			dst.sipx_addr.x_net = ia->ia_addr.sipx_addr.x_net;
			ipx->ipx_dna.x_net = dst.sipx_addr.x_net;
			if(ipx->ipx_sum != 0xffff)
				ipx->ipx_sum = ipx_cksum(m, ntohs(ipx->ipx_len));

			m1 = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
			if(m1) {
				error = (*ifp->if_output)(ifp, m1,
					(struct sockaddr *)&dst, NULL);
				/* XXX ipxstat.ipxs_localout++; */
			}
skip_this: ;
		}

bad:
	m_freem(m);
	return (error);
}
