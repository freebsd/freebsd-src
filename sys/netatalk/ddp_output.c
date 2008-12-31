/*-
 * Copyright (c) 1990, 1991 Regents of The University of Michigan.
 * All Rights Reserved.
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
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 */

/* $FreeBSD: src/sys/netatalk/ddp_output.c,v 1.30.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $ */

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>

#undef s_net

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

#include <security/mac/mac_framework.h>

int	ddp_cksum = 1;

int
ddp_output(struct mbuf *m, struct socket *so)
{
	struct ddpehdr	*deh;
	struct ddpcb *ddp = sotoddpcb(so);

#ifdef MAC
	SOCK_LOCK(so);
	mac_create_mbuf_from_socket(so, m);
	SOCK_UNLOCK(so);
#endif

	M_PREPEND(m, sizeof(struct ddpehdr), M_DONTWAIT);
	if (m == NULL)
	return (ENOBUFS);

	deh = mtod(m, struct ddpehdr *);
	deh->deh_pad = 0;
	deh->deh_hops = 0;

	deh->deh_len = m->m_pkthdr.len;

	deh->deh_dnet = ddp->ddp_fsat.sat_addr.s_net;
	deh->deh_dnode = ddp->ddp_fsat.sat_addr.s_node;
	deh->deh_dport = ddp->ddp_fsat.sat_port;
	deh->deh_snet = ddp->ddp_lsat.sat_addr.s_net;
	deh->deh_snode = ddp->ddp_lsat.sat_addr.s_node;
	deh->deh_sport = ddp->ddp_lsat.sat_port;

	/*
	 * The checksum calculation is done after all of the other bytes have
	 * been filled in.
	 */
	if (ddp_cksum)
		deh->deh_sum = at_cksum(m, sizeof(int));
	else
		deh->deh_sum = 0;
	deh->deh_bytes = htonl(deh->deh_bytes);

#ifdef NETATALK_DEBUG
	printf ("ddp_output: from %d.%d:%d to %d.%d:%d\n",
	ntohs(deh->deh_snet), deh->deh_snode, deh->deh_sport,
	ntohs(deh->deh_dnet), deh->deh_dnode, deh->deh_dport);
#endif
	return (ddp_route(m, &ddp->ddp_route));
}

u_short
at_cksum(struct mbuf *m, int skip)
{
	u_char	*data, *end;
	u_long	cksum = 0;

	for (; m; m = m->m_next) {
		for (data = mtod(m, u_char *), end = data + m->m_len;
		    data < end; data++) {
			if (skip) {
				skip--;
				continue;
			}
			cksum = (cksum + *data) << 1;
			if (cksum & 0x00010000)
				cksum++;
			cksum &= 0x0000ffff;
		}
	}

	if (cksum == 0)
		cksum = 0x0000ffff;
	return ((u_short)cksum);
}

int
ddp_route(struct mbuf *m, struct route *ro)
{
	struct sockaddr_at	gate;
	struct elaphdr	*elh;
	struct mbuf		*m0;
	struct at_ifaddr	*aa = NULL;
	struct ifnet	*ifp = NULL;
	u_short		net;

#if 0
	/* Check for net zero, node zero ("myself") */
	if (satosat(&ro->ro_dst)->sat_addr.s_net == ATADDR_ANYNET
	    && satosat(&ro->ro_dst)->sat_addr.s_node == ATADDR_ANYNODE) {
		/* Find the loopback interface */
	}
#endif

	/*
	 * If we have a route, find the ifa that refers to this route.  I.e
	 * the ifa used to get to the gateway.
	 */
	if ((ro->ro_rt == NULL) || (ro->ro_rt->rt_ifa == NULL) ||
	    ((ifp = ro->ro_rt->rt_ifa->ifa_ifp) == NULL))
		rtalloc(ro);
	if ((ro->ro_rt != NULL) && (ro->ro_rt->rt_ifa) &&
	    (ifp = ro->ro_rt->rt_ifa->ifa_ifp)) {
		net = ntohs(satosat(ro->ro_rt->rt_gateway)->sat_addr.s_net);
		for (aa = at_ifaddr_list; aa != NULL; aa = aa->aa_next) {
			if (((net == 0) || (aa->aa_ifp == ifp)) &&
			    net >= ntohs(aa->aa_firstnet) &&
			    net <= ntohs(aa->aa_lastnet))
				break;
		}
	} else {
		m_freem(m);
#ifdef NETATALK_DEBUG
		if (ro->ro_rt == NULL)
			printf ("ddp_route: no ro_rt.\n");
		else if (ro->ro_rt->rt_ifa == NULL)
			printf ("ddp_route: no ro_rt->rt_ifa\n");
		else
			printf ("ddp_route: no ro_rt->rt_ifa->ifa_ifp\n");
#endif
		return (ENETUNREACH);
	}

	if (aa == NULL) {
#ifdef NETATALK_DEBUG
		printf("ddp_route: no atalk address found for %s\n",
		    ifp->if_xname);
#endif
		m_freem(m);
		return (ENETUNREACH);
	}

	/*
	 * If the destination address is on a directly attached node use
	 * that, else use the official gateway.
	 */
	if (ntohs(satosat(&ro->ro_dst)->sat_addr.s_net) >=
	    ntohs(aa->aa_firstnet) &&
	    ntohs(satosat(&ro->ro_dst)->sat_addr.s_net) <=
	    ntohs(aa->aa_lastnet))
		gate = *satosat(&ro->ro_dst);
	else
		gate = *satosat(ro->ro_rt->rt_gateway);

	/*
	 * There are several places in the kernel where data is added to an
	 * mbuf without ensuring that the mbuf pointer is aligned.  This is
	 * bad for transition routing, since phase 1 and phase 2 packets end
	 * up poorly aligned due to the three byte elap header.
	 *
	 * XXXRW: kern/4184 suggests that an m_pullup() of (m) should take
	 * place here to address possible alignment issues.
	 *
	 * XXXRW: This appears not to handle M_PKTHDR properly, as it doesn't
	 * move the existing header from the old packet to the new one.
	 * Posibly should call M_MOVE_PKTHDR()?  This would also allow
	 * removing mac_mbuf_copy().
	 */
	if (!(aa->aa_flags & AFA_PHASE2)) {
		MGET(m0, M_DONTWAIT, MT_DATA);
		if (m0 == NULL) {
			m_freem(m);
			printf("ddp_route: no buffers\n");
			return (ENOBUFS);
		}
#ifdef MAC
		mac_copy_mbuf(m, m0);
#endif
		m0->m_next = m;
		/* XXX perhaps we ought to align the header? */
		m0->m_len = SZ_ELAPHDR;
		m = m0;

		elh = mtod(m, struct elaphdr *);
		elh->el_snode = satosat(&aa->aa_addr)->sat_addr.s_node;
		elh->el_type = ELAP_DDPEXTEND;
		elh->el_dnode = gate.sat_addr.s_node;
	}
	ro->ro_rt->rt_use++;

#ifdef NETATALK_DEBUG
	printf ("ddp_route: from %d.%d to %d.%d, via %d.%d (%s)\n",
	    ntohs(satosat(&aa->aa_addr)->sat_addr.s_net),
	    satosat(&aa->aa_addr)->sat_addr.s_node,
	    ntohs(satosat(&ro->ro_dst)->sat_addr.s_net),
	    satosat(&ro->ro_dst)->sat_addr.s_node,
	    ntohs(gate.sat_addr.s_net), gate.sat_addr.s_node, ifp->if_xname);
#endif

	/* Short-circuit the output if we're sending this to ourself. */
	if ((satosat(&aa->aa_addr)->sat_addr.s_net ==
	    satosat(&ro->ro_dst)->sat_addr.s_net) &&
	    (satosat(&aa->aa_addr)->sat_addr.s_node ==
	    satosat(&ro->ro_dst)->sat_addr.s_node))
		return (if_simloop(ifp, m, gate.sat_family, 0));

	/* XXX */
	return ((*ifp->if_output)(ifp, m, (struct sockaddr *)&gate, NULL));
}
