/*-
 * Copyright (c) 2004 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1990, 1994 Regents of The University of Michigan.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 *
 * $FreeBSD: src/sys/netatalk/ddp_input.c,v 1.32.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <net/if.h>
#include <net/route.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp.h>
#include <netatalk/ddp_var.h>
#include <netatalk/ddp_pcb.h>
#include <netatalk/at_extern.h>

#include <security/mac/mac_framework.h>

static volatile int	ddp_forward = 1;
static volatile int	ddp_firewall = 0;
static struct ddpstat	ddpstat;

static struct route	forwro;

static void     ddp_input(struct mbuf *, struct ifnet *, struct elaphdr *, int);

/*
 * Could probably merge these two code segments a little better...
 */
void
at2intr(struct mbuf *m)
{

	/*
	 * Phase 2 packet handling .
	 */
	ddp_input(m, m->m_pkthdr.rcvif, NULL, 2);
}

void
at1intr(struct mbuf *m)
{
	struct elaphdr *elhp, elh;

	/*
	 * Phase 1 packet handling 
	 */
	if (m->m_len < SZ_ELAPHDR && ((m = m_pullup(m, SZ_ELAPHDR)) ==
	    NULL)) {
		ddpstat.ddps_tooshort++;
		return;
	}

	/*
	 * This seems a little dubious, but I don't know phase 1 so leave it.
	 */
	elhp = mtod(m, struct elaphdr *);
	m_adj(m, SZ_ELAPHDR);

	if (elhp->el_type != ELAP_DDPEXTEND) {
		bcopy((caddr_t)elhp, (caddr_t)&elh, SZ_ELAPHDR);
		ddp_input(m, m->m_pkthdr.rcvif, &elh, 1);
	} else
		ddp_input(m, m->m_pkthdr.rcvif, NULL, 1);
}

static void
ddp_input(struct mbuf *m, struct ifnet *ifp, struct elaphdr *elh, int phase)
{
	struct sockaddr_at from, to;
	struct ddpshdr *dsh, ddps;
	struct at_ifaddr *aa;
	struct ddpehdr *deh = NULL, ddpe;
	struct ddpcb *ddp;
	int dlen, mlen;
	u_short cksum = 0;

	bzero((caddr_t)&from, sizeof(struct sockaddr_at));
	bzero((caddr_t)&to, sizeof(struct sockaddr_at));
	if (elh != NULL) {
		/*
		 * Extract the information in the short header.  Network
		 * information is defaulted to ATADDR_ANYNET and node
		 * information comes from the elh info.  We must be phase 1.
		 */
		ddpstat.ddps_short++;

		if (m->m_len < sizeof(struct ddpshdr) &&
		    ((m = m_pullup(m, sizeof(struct ddpshdr))) == NULL)) {
			ddpstat.ddps_tooshort++;
			return;
		}

		dsh = mtod(m, struct ddpshdr *);
		bcopy((caddr_t)dsh, (caddr_t)&ddps, sizeof(struct ddpshdr));
		ddps.dsh_bytes = ntohl(ddps.dsh_bytes);
		dlen = ddps.dsh_len;

		to.sat_addr.s_net = ATADDR_ANYNET;
		to.sat_addr.s_node = elh->el_dnode;
		to.sat_port = ddps.dsh_dport;
		from.sat_addr.s_net = ATADDR_ANYNET;
		from.sat_addr.s_node = elh->el_snode;
		from.sat_port = ddps.dsh_sport;

		/* 
		 * Make sure that we point to the phase1 ifaddr info and that
		 * it's valid for this packet.
		 */
		for (aa = at_ifaddr_list; aa != NULL; aa = aa->aa_next) {
			if ((aa->aa_ifp == ifp)
			    && ((aa->aa_flags & AFA_PHASE2) == 0)
			    && ((to.sat_addr.s_node ==
			    AA_SAT(aa)->sat_addr.s_node) ||
			    (to.sat_addr.s_node == ATADDR_BCAST)))
				break;
		}
		/* 
		 * maybe we got a broadcast not meant for us.. ditch it.
		 */
		if (aa == NULL) {
			m_freem(m);
			return;
		}
	} else {
		/*
		 * There was no 'elh' passed on. This could still be either
		 * phase1 or phase2.  We have a long header, but we may be
		 * running on a phase 1 net.  Extract out all the info
		 * regarding this packet's src & dst.
		 */
		ddpstat.ddps_long++;

		if (m->m_len < sizeof(struct ddpehdr) &&
		    ((m = m_pullup(m, sizeof(struct ddpehdr))) == NULL)) {
			ddpstat.ddps_tooshort++;
			return;
		}

		deh = mtod(m, struct ddpehdr *);
		bcopy((caddr_t)deh, (caddr_t)&ddpe, sizeof(struct ddpehdr));
		ddpe.deh_bytes = ntohl(ddpe.deh_bytes);
		dlen = ddpe.deh_len;

		if ((cksum = ddpe.deh_sum) == 0)
			ddpstat.ddps_nosum++;

		from.sat_addr.s_net = ddpe.deh_snet;
		from.sat_addr.s_node = ddpe.deh_snode;
		from.sat_port = ddpe.deh_sport;
		to.sat_addr.s_net = ddpe.deh_dnet;
		to.sat_addr.s_node = ddpe.deh_dnode;
		to.sat_port = ddpe.deh_dport;

		if (to.sat_addr.s_net == ATADDR_ANYNET) {
			/*
			 * The TO address doesn't specify a net, so by
			 * definition it's for this net.  Try find ifaddr
			 * info with the right phase, the right interface,
			 * and either to our node, a broadcast, or looped
			 * back (though that SHOULD be covered in the other
			 * cases).
			 *
			 * XXX If we have multiple interfaces, then the first
			 * with this node number will match (which may NOT be
			 * what we want, but it's probably safe in 99.999% of
			 * cases.
			 */
			for (aa = at_ifaddr_list; aa != NULL;
			    aa = aa->aa_next) {
				if (phase == 1 && (aa->aa_flags &
				    AFA_PHASE2))
					continue;
				if (phase == 2 && (aa->aa_flags &
				    AFA_PHASE2) == 0)
					continue;
				if ((aa->aa_ifp == ifp) &&
				    ((to.sat_addr.s_node ==
				    AA_SAT(aa)->sat_addr.s_node) ||
				    (to.sat_addr.s_node == ATADDR_BCAST) ||
				    (ifp->if_flags & IFF_LOOPBACK)))
					break;
			}
		} else {
			/* 
			 * A destination network was given.  We just try to
			 * find which ifaddr info matches it.
	    		 */
			for (aa = at_ifaddr_list; aa != NULL;
			    aa = aa->aa_next) {
				/*
				 * This is a kludge. Accept packets that are
				 * for any router on a local netrange.
				 */
				if (to.sat_addr.s_net == aa->aa_firstnet &&
				    to.sat_addr.s_node == 0)
					break;
				/*
				 * Don't use ifaddr info for which we are
				 * totally outside the netrange, and it's not
				 * a startup packet.  Startup packets are
				 * always implicitly allowed on to the next
				 * test.
				 */
				if (((ntohs(to.sat_addr.s_net) <
				    ntohs(aa->aa_firstnet)) ||
				    (ntohs(to.sat_addr.s_net) >
				    ntohs(aa->aa_lastnet))) &&
				    ((ntohs(to.sat_addr.s_net) < 0xff00) ||
				    (ntohs(to.sat_addr.s_net) > 0xfffe)))
					continue;

				/*
				 * Don't record a match either if we just
				 * don't have a match in the node address.
				 * This can have if the interface is in
				 * promiscuous mode for example.
				 */
				if ((to.sat_addr.s_node !=
				    AA_SAT(aa)->sat_addr.s_node) &&
				    (to.sat_addr.s_node != ATADDR_BCAST))
					continue;
				break;
			}
		}
	}

	/*
	 * Adjust the length, removing any padding that may have been added
	 * at a link layer.  We do this before we attempt to forward a
	 * packet, possibly on a different media.
	 */
	mlen = m->m_pkthdr.len;
	if (mlen < dlen) {
		ddpstat.ddps_toosmall++;
		m_freem(m);
		return;
	}
	if (mlen > dlen)
		m_adj(m, dlen - mlen);

	/*
	 * If it isn't for a net on any of our interfaces, or it IS for a net
	 * on a different interface than it came in on, (and it is not looped
	 * back) then consider if we should forward it.  As we are not really
	 * a router this is a bit cheeky, but it may be useful some day.
	 */
	if ((aa == NULL) || ((to.sat_addr.s_node == ATADDR_BCAST) &&
	    (aa->aa_ifp != ifp) && ((ifp->if_flags & IFF_LOOPBACK) == 0))) {
		/* 
		 * If we've explicitly disabled it, don't route anything.
		 */
		if (ddp_forward == 0) {
			m_freem(m);
			return;
		}

		/* 
		 * If the cached forwarding route is still valid, use it.
		 *
		 * XXXRW: Access to the cached route may not be properly
		 * synchronized for parallel input handling.
		 */
		if (forwro.ro_rt &&
		    (satosat(&forwro.ro_dst)->sat_addr.s_net !=
		    to.sat_addr.s_net ||
		    satosat(&forwro.ro_dst)->sat_addr.s_node !=
		    to.sat_addr.s_node)) {
			RTFREE(forwro.ro_rt);
			forwro.ro_rt = NULL;
		}

		/*
		 * If we don't have a cached one (any more) or it's useless,
		 * then get a new route.
		 *
		 * XXX this could cause a 'route leak'.  Check this!
		 */
		if (forwro.ro_rt == NULL || forwro.ro_rt->rt_ifp == NULL) {
			forwro.ro_dst.sa_len = sizeof(struct sockaddr_at);
			forwro.ro_dst.sa_family = AF_APPLETALK;
			satosat(&forwro.ro_dst)->sat_addr.s_net =
			    to.sat_addr.s_net;
			satosat(&forwro.ro_dst)->sat_addr.s_node =
			    to.sat_addr.s_node;
			rtalloc(&forwro);
		}

		/* 
		 * If it's not going to get there on this hop, and it's
		 * already done too many hops, then throw it away.
		 */
		if ((to.sat_addr.s_net !=
		    satosat(&forwro.ro_dst)->sat_addr.s_net) &&
		    (ddpe.deh_hops == DDP_MAXHOPS)) {
			m_freem(m);
			return;
		}

		/*
		 * A ddp router might use the same interface to forward the
		 * packet, which this would not effect.  Don't allow packets
		 * to cross from one interface to another however.
		 */
		if (ddp_firewall && ((forwro.ro_rt == NULL) ||
		    (forwro.ro_rt->rt_ifp != ifp))) {
			m_freem(m);
			return;
		}

		/*
		 * Adjust the header.  If it was a short header then it would
		 * have not gotten here, so we can assume there is room to
		 * drop the header in.
		 *
		 * XXX what about promiscuous mode, etc...
		 */
		ddpe.deh_hops++;
		ddpe.deh_bytes = htonl(ddpe.deh_bytes);
		/* XXX deh? */
		bcopy((caddr_t)&ddpe, (caddr_t)deh, sizeof(u_short));
		if (ddp_route(m, &forwro))
			ddpstat.ddps_cantforward++;
		else
			ddpstat.ddps_forward++;
		return;
	}

	/*
	 * It was for us, and we have an ifaddr to use with it.
	 */
	from.sat_len = sizeof(struct sockaddr_at);
	from.sat_family = AF_APPLETALK;

	/* 
	 * We are no longer interested in the link layer so cut it off.
	 */
	if (elh == NULL) {
		if (ddp_cksum && cksum && cksum !=
		    at_cksum(m, sizeof(int))) {
			ddpstat.ddps_badsum++;
			m_freem(m);
			return;
		}
		m_adj(m, sizeof(struct ddpehdr));
	} else
		m_adj(m, sizeof(struct ddpshdr));

	/* 
	 * Search for ddp protocol control blocks that match these addresses. 
	 */
	DDP_LIST_SLOCK();
	if ((ddp = ddp_search(&from, &to, aa)) == NULL)
		goto out;

#ifdef MAC
	SOCK_LOCK(ddp->ddp_socket);
	if (mac_check_socket_deliver(ddp->ddp_socket, m) != 0) {
		SOCK_UNLOCK(ddp->ddp_socket);
		goto out;
	}
	SOCK_UNLOCK(ddp->ddp_socket);
#endif

	/* 
	 * If we found one, deliver the packet to the socket
	 */
	SOCKBUF_LOCK(&ddp->ddp_socket->so_rcv);
	if (sbappendaddr_locked(&ddp->ddp_socket->so_rcv,
	    (struct sockaddr *)&from, m, NULL) == 0) {
    		SOCKBUF_UNLOCK(&ddp->ddp_socket->so_rcv);
		/* 
		 * If the socket is full (or similar error) dump the packet.
		 */
		ddpstat.ddps_nosockspace++;
		goto out;
	}

	/*
	 * And wake up whatever might be waiting for it
	 */
	sorwakeup_locked(ddp->ddp_socket);
	m = NULL;
out:
	DDP_LIST_SUNLOCK();
	if (m != NULL)
		m_freem(m);
}
