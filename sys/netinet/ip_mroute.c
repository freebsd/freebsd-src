/*
 * Copyright (c) 1989 Stephen Deering
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
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
 *	@(#)ip_mroute.c	7.4 (Berkeley) 11/19/92
 */

/*
 * Procedures for the kernel part of DVMRP,
 * a Distance-Vector Multicast Routing Protocol.
 * (See RFC-1075.)
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 *
 * MROUTING 1.1
 */

#ifndef MROUTING
int	ip_mrtproto;				/* for netstat only */
#else

#include "param.h"
#include "errno.h"
#include "ioctl.h"
#include "malloc.h"
#include "mbuf.h"
#include "protosw.h"
#include "socket.h"
#include "socketvar.h"
#include "time.h"

#ifndef __FreeBSD__
#include "net/af.h"
#else
#include "systm.h"
#endif
#include "net/if.h"
#include "net/route.h"
#include "net/raw_cb.h"

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "in_var.h"
#include "ip_var.h"

#include "igmp.h"
#include "igmp_var.h"
#include "ip_mroute.h"

/* Static forwards */
static	int ip_mrouter_init __P((struct socket *));
static	int add_vif __P((struct vifctl *));
static	int del_vif __P((vifi_t *vifip));
static	int add_lgrp __P((struct lgrplctl *));
static	int del_lgrp __P((struct lgrplctl *));
static	int grplst_member __P((struct vif *, struct in_addr));
static	u_long nethash __P((u_long in));
static	int add_mrt __P((struct mrtctl *));
static	int del_mrt __P((struct in_addr *));
static	struct mrt *mrtfind __P((u_long));
static	void phyint_send __P((struct ip *, struct vif *, struct mbuf *));
static	void srcrt_send __P((struct ip *, struct vif *, struct mbuf *));
static	void encap_send __P((struct ip *, struct vif *, struct mbuf *));
static	void multiencap_decap __P((struct mbuf *, int hlen));

#define INSIZ sizeof(struct in_addr)
#define	same(a1, a2) (bcmp((caddr_t)(a1), (caddr_t)(a2), INSIZ) == 0)
#define	satosin(sa)	((struct sockaddr_in *)(sa))

/*
 * Globals.  All but ip_mrouter and ip_mrtproto could be static,
 * except for netstat or debugging purposes.
 */
struct	socket *ip_mrouter = NULL;
int	ip_mrtproto = IGMP_DVMRP;		/* for netstat only */

struct	mrt *mrttable[MRTHASHSIZ];
struct	vif viftable[MAXVIFS];
struct	mrtstat	mrtstat;

/*
 * 'Interfaces' associated with decapsulator (so we can tell
 * packets that went through it from ones that get reflected
 * by a broken gateway).  These interfaces are never linked into
 * the system ifnet list & no routes point to them.  I.e., packets
 * can't be sent this way.  They only exist as a placeholder for
 * multicast source verification.
 */
struct ifnet multicast_decap_if[MAXVIFS];

#define ENCAP_TTL 64
#define ENCAP_PROTO 4

/* prototype IP hdr for encapsulated packets */
struct ip multicast_encap_iphdr = {
#if defined(ultrix) || defined(i386)
	sizeof(struct ip) >> 2, IPVERSION,
#else
	IPVERSION, sizeof(struct ip) >> 2,
#endif
	0,				/* tos */
	sizeof(struct ip),		/* total length */
	0,				/* id */
	0,				/* frag offset */
	ENCAP_TTL, ENCAP_PROTO,	
	0,				/* checksum */
};

/*
 * Private variables.
 */
static	vifi_t numvifs = 0;
static	struct mrt *cached_mrt = NULL;
static	u_long cached_origin;
static	u_long cached_originmask;

static void (*encap_oldrawip)();

/*
 * one-back cache used by multiencap_decap to locate a tunnel's vif
 * given a datagram's src ip address.
 */
static u_long last_encap_src;
static struct vif *last_encap_vif;

/*
 * A simple hash function: returns MRTHASHMOD of the low-order octet of
 * the argument's network or subnet number.
 */
static u_long
nethash(n)
	u_long n;
{
	struct in_addr in;

	in.s_addr = n;
	n = in_netof(in);
	while ((n & 0xff) == 0)
		n >>= 8;
	return (MRTHASHMOD(n));
}

/*
 * this is a direct-mapped cache used to speed the mapping from a
 * datagram source address to the associated multicast route.  Note
 * that unlike mrttable, the hash is on IP address, not IP net number.
 */
#define MSRCHASHSIZ 1024
#define MSRCHASH(a) ((((a) >> 20) ^ ((a) >> 10) ^ (a)) & (MSRCHASHSIZ - 1))
struct mrt *mrtsrchash[MSRCHASHSIZ];

/*
 * Find a route for a given origin IP address.
 */
#define MRTFIND(o, rt) { \
	register u_int _mrhash = o; \
	_mrhash = MSRCHASH(_mrhash); \
	++mrtstat.mrts_mrt_lookups; \
	rt = mrtsrchash[_mrhash]; \
	if (rt == NULL || \
	    (o & rt->mrt_originmask.s_addr) != rt->mrt_origin.s_addr) \
		if ((rt = mrtfind(o)) != NULL) \
		    mrtsrchash[_mrhash] = rt; \
}

static struct mrt *
mrtfind(origin)
    u_long origin;
{
	register struct mrt *rt;
	register u_int hash;

	mrtstat.mrts_mrt_misses++;

	hash = nethash(origin);
	for (rt = mrttable[hash]; rt; rt = rt->mrt_next) {
		if ((origin & rt->mrt_originmask.s_addr) ==
		    rt->mrt_origin.s_addr)
			return (rt);
	}
	return (NULL);
}

/*
 * Handle DVMRP setsockopt commands to modify the multicast routing tables.
 */
int
ip_mrouter_cmd(cmd, so, m)
	register int cmd;
	register struct socket *so;
	register struct mbuf *m;
{
	register int error = 0;

	if (cmd != DVMRP_INIT && so != ip_mrouter)
		error = EACCES;
	else switch (cmd) {

	case DVMRP_INIT:
		error = ip_mrouter_init(so);
		break;

	case DVMRP_DONE:
		error = ip_mrouter_done();
		break;

	case DVMRP_ADD_VIF:
		if (m == NULL || m->m_len < sizeof(struct vifctl))
			error = EINVAL;
		else
			error = add_vif(mtod(m, struct vifctl *));
		break;

	case DVMRP_DEL_VIF:
		if (m == NULL || m->m_len < sizeof(short))
			error = EINVAL;
		else
			error = del_vif(mtod(m, vifi_t *));
		break;

	case DVMRP_ADD_LGRP:
		if (m == NULL || m->m_len < sizeof(struct lgrplctl))
			error = EINVAL;
		else
			error = add_lgrp(mtod(m, struct lgrplctl *));
		break;

	case DVMRP_DEL_LGRP:
		if (m == NULL || m->m_len < sizeof(struct lgrplctl))
			error = EINVAL;
		else
			error = del_lgrp(mtod(m, struct lgrplctl *));
		break;

	case DVMRP_ADD_MRT:
		if (m == NULL || m->m_len < sizeof(struct mrtctl))
			error = EINVAL;
		else
			error = add_mrt(mtod(m, struct mrtctl *));
		break;

	case DVMRP_DEL_MRT:
		if (m == NULL || m->m_len < sizeof(struct in_addr))
			error = EINVAL;
		else
			error = del_mrt(mtod(m, struct in_addr *));
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

/*
 * Enable multicast routing
 */
static int
ip_mrouter_init(so)
	register struct socket *so;
{
	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_IGMP)
		return (EOPNOTSUPP);

	if (ip_mrouter != NULL)
		return (EADDRINUSE);

	ip_mrouter = so;

	return (0);
}

/*
 * Disable multicast routing
 */
int
ip_mrouter_done()
{
	register vifi_t vifi;
	register int i;
	register struct ifnet *ifp;
	register int s;
	struct ifreq ifr;

	s = splnet();

	/*
	 * For each phyint in use, free its local group list and
	 * disable promiscuous reception of all IP multicasts.
	 */
	for (vifi = 0; vifi < numvifs; vifi++) {
		if (viftable[vifi].v_lcl_addr.s_addr != 0 &&
		    !(viftable[vifi].v_flags & VIFF_TUNNEL)) {
			if (viftable[vifi].v_lcl_grps)
				free(viftable[vifi].v_lcl_grps, M_MRTABLE);
			satosin(&ifr.ifr_addr)->sin_family = AF_INET;
			satosin(&ifr.ifr_addr)->sin_addr.s_addr = INADDR_ANY;
			ifp = viftable[vifi].v_ifp;
			(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
		}
	}
	bzero((caddr_t)viftable, sizeof(viftable));
	numvifs = 0;

	/*
	 * Free any multicast route entries.
	 */
	for (i = 0; i < MRTHASHSIZ; i++)
		if (mrttable[i])
			free(mrttable[i], M_MRTABLE);
	bzero((caddr_t)mrttable, sizeof(mrttable));
	bzero((caddr_t)mrtsrchash, sizeof(mrtsrchash));

	ip_mrouter = NULL;

	splx(s);
	return (0);
}

/*
 * Add a vif to the vif table
 */
static int
add_vif(vifcp)
	register struct vifctl *vifcp;
{
	register struct vif *vifp = viftable + vifcp->vifc_vifi;
	register struct ifaddr *ifa;
	register struct ifnet *ifp;
	struct ifreq ifr;
	register int error, s;
	static struct sockaddr_in sin = { sizeof(sin), AF_INET };

	if (vifcp->vifc_vifi >= MAXVIFS)
		return (EINVAL);
	if (vifp->v_lcl_addr.s_addr != 0)
		return (EADDRINUSE);

	/* Find the interface with an address in AF_INET family */
	sin.sin_addr = vifcp->vifc_lcl_addr;
	ifa = ifa_ifwithaddr((struct sockaddr *)&sin);
	if (ifa == 0)
		return (EADDRNOTAVAIL);
	ifp = ifa->ifa_ifp;

	if (vifcp->vifc_flags & VIFF_TUNNEL) {
		if ((vifcp->vifc_flags & VIFF_SRCRT) == 0) {
			/*
			 * An encapsulating tunnel is wanted.  If we
			 * haven't done so already, put our decap routine
			 * in front of raw_input so we have a chance to
			 * decapsulate incoming packets.  Then set the
			 * arrival 'interface' to be the decapsulator.
			 */
			if (encap_oldrawip == 0) {
				register int pr = ip_protox[ENCAP_PROTO];

				encap_oldrawip = inetsw[pr].pr_input;
				inetsw[pr].pr_input = multiencap_decap;
				for (s = 0; s < MAXVIFS; ++s) {
					multicast_decap_if[s].if_name =
						"mdecap";
					multicast_decap_if[s].if_unit = s;
				}
			}
			ifp = &multicast_decap_if[vifcp->vifc_vifi];
		} else {
			ifp = 0;
		}
	} else {
		/* Make sure the interface supports multicast */
		if ((ifp->if_flags & IFF_MULTICAST) == 0)
			return EOPNOTSUPP;

		/*
		 * Enable promiscuous reception of all
		 * IP multicasts from the if
		 */
		((struct sockaddr_in *)&ifr.ifr_addr)->sin_family = AF_INET;
		((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr = 
			INADDR_ANY;
		s = splnet();
		error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr);
		splx(s);
		if (error)
			return error;
	}

	s = splnet();
	vifp->v_flags = vifcp->vifc_flags;
	vifp->v_threshold = vifcp->vifc_threshold;
	vifp->v_lcl_addr = vifcp->vifc_lcl_addr;
	vifp->v_ifp = ifa->ifa_ifp;
	vifp->v_rmt_addr  = vifcp->vifc_rmt_addr;
	splx(s);

	/* Adjust numvifs up if the vifi is higher than numvifs */
	if (numvifs <= vifcp->vifc_vifi)
		numvifs = vifcp->vifc_vifi + 1;

	splx(s);
	return (0);
}

/*
 * Delete a vif from the vif table
 */
static int
del_vif(vifip)
	register vifi_t *vifip;
{
	register struct vif *vifp = viftable + *vifip;
	register struct ifnet *ifp;
	register int i, s;
	struct ifreq ifr;

	if (*vifip >= numvifs)
		return (EINVAL);
	if (vifp->v_lcl_addr.s_addr == 0)
		return (EADDRNOTAVAIL);

	s = splnet();

	if (!(vifp->v_flags & VIFF_TUNNEL)) {
		if (vifp->v_lcl_grps)
			free(vifp->v_lcl_grps, M_MRTABLE);
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr.s_addr = INADDR_ANY;
		ifp = vifp->v_ifp;
		(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
	}
	if (vifp == last_encap_vif) {
		last_encap_vif = 0;
		last_encap_src = 0;
	}
	bzero((caddr_t)vifp, sizeof (*vifp));

	/* Adjust numvifs down */
	for (i = numvifs - 1; i >= 0; i--)
		if (viftable[i].v_lcl_addr.s_addr != 0)
			break;
	numvifs = i + 1;

	splx(s);
	return (0);
}

/*
 * Add the multicast group in the lgrpctl to the list of local multicast
 * group memberships associated with the vif indexed by gcp->lgc_vifi.
 */
static int
add_lgrp(gcp)
	register struct lgrplctl *gcp;
{
	register struct vif *vifp;
	register int s;

	if (gcp->lgc_vifi >= numvifs)
		return (EINVAL);

	vifp = viftable + gcp->lgc_vifi;
	if (vifp->v_lcl_addr.s_addr == 0 || (vifp->v_flags & VIFF_TUNNEL))
		return (EADDRNOTAVAIL);

	/* If not enough space in existing list, allocate a larger one */
	s = splnet();
	if (vifp->v_lcl_grps_n + 1 >= vifp->v_lcl_grps_max) {
		register int num;
		register struct in_addr *ip;

		num = vifp->v_lcl_grps_max;
		if (num <= 0)
			num = 32;	/* initial number */
		else
			num += num;	/* double last number */
		ip = (struct in_addr *)malloc(num * sizeof(*ip),
		    M_MRTABLE, M_NOWAIT);
		if (ip == NULL) {
			splx(s);
			return (ENOBUFS);
		}

		bzero((caddr_t)ip, num * sizeof(*ip));	/* XXX paranoid */
		bcopy((caddr_t)vifp->v_lcl_grps, (caddr_t)ip,
		    vifp->v_lcl_grps_n * sizeof(*ip));

		vifp->v_lcl_grps_max = num;
		if (vifp->v_lcl_grps)
			free(vifp->v_lcl_grps, M_MRTABLE);
		vifp->v_lcl_grps = ip;
	}

	vifp->v_lcl_grps[vifp->v_lcl_grps_n++] = gcp->lgc_gaddr;

	if (gcp->lgc_gaddr.s_addr == vifp->v_cached_group)
		vifp->v_cached_result = 1;

	splx(s);
	return (0);
}

/*
 * Delete the the local multicast group associated with the vif
 * indexed by gcp->lgc_vifi.
 */

static int
del_lgrp(gcp)
	register struct lgrplctl *gcp;
{
	register struct vif *vifp;
	register int i, error, s;

	if (gcp->lgc_vifi >= numvifs)
		return (EINVAL);
	vifp = viftable + gcp->lgc_vifi;
	if (vifp->v_lcl_addr.s_addr == 0 || (vifp->v_flags & VIFF_TUNNEL))
		return (EADDRNOTAVAIL);

	s = splnet();

	if (gcp->lgc_gaddr.s_addr == vifp->v_cached_group)
		vifp->v_cached_result = 0;

	error = EADDRNOTAVAIL;
	for (i = 0; i < vifp->v_lcl_grps_n; ++i)
		if (same(&gcp->lgc_gaddr, &vifp->v_lcl_grps[i])) {
			error = 0;
			--vifp->v_lcl_grps_n;
			for (; i < vifp->v_lcl_grps_n; ++i)
				vifp->v_lcl_grps[i] = vifp->v_lcl_grps[i + 1];
			error = 0;
			break;
		}

	splx(s);
	return (error);
}

/*
 * Return 1 if gaddr is a member of the local group list for vifp.
 */
static int
grplst_member(vifp, gaddr)
	register struct vif *vifp;
	struct in_addr gaddr;
{
	register int i, s;
	register u_long addr;

	mrtstat.mrts_grp_lookups++;

	addr = gaddr.s_addr;
	if (addr == vifp->v_cached_group)
		return (vifp->v_cached_result);

	mrtstat.mrts_grp_misses++;

	for (i = 0; i < vifp->v_lcl_grps_n; ++i)
		if (addr == vifp->v_lcl_grps[i].s_addr) {
			s = splnet();
			vifp->v_cached_group = addr;
			vifp->v_cached_result = 1;
			splx(s);
			return (1);
		}
	s = splnet();
	vifp->v_cached_group = addr;
	vifp->v_cached_result = 0;
	splx(s);
	return (0);
}

/*
 * Add an mrt entry
 */
static int
add_mrt(mrtcp)
	register struct mrtctl *mrtcp;
{
	struct mrt *rt;
	u_long hash;
	int s;

	if (rt = mrtfind(mrtcp->mrtc_origin.s_addr)) {
		/* Just update the route */
		s = splnet();
		rt->mrt_parent = mrtcp->mrtc_parent;
		VIFM_COPY(mrtcp->mrtc_children, rt->mrt_children);
		VIFM_COPY(mrtcp->mrtc_leaves, rt->mrt_leaves);
		splx(s);
		return (0);
	}

	s = splnet();

	rt = (struct mrt *)malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
	if (rt == NULL) {
		splx(s);
		return (ENOBUFS);
	}

	/*
	 * insert new entry at head of hash chain
	 */
	rt->mrt_origin = mrtcp->mrtc_origin;
	rt->mrt_originmask = mrtcp->mrtc_originmask;
	rt->mrt_parent = mrtcp->mrtc_parent;
	VIFM_COPY(mrtcp->mrtc_children, rt->mrt_children);
	VIFM_COPY(mrtcp->mrtc_leaves, rt->mrt_leaves);
	/* link into table */
	hash = nethash(mrtcp->mrtc_origin.s_addr);
	rt->mrt_next = mrttable[hash];
	mrttable[hash] = rt;

	splx(s);
	return (0);
}

/*
 * Delete an mrt entry
 */
static int
del_mrt(origin)
	register struct in_addr *origin;
{
	register struct mrt *rt, *prev_rt;
	register u_long hash = nethash(origin->s_addr);
	register struct mrt **cmrt, **cmrtend;
	register int s;

	for (prev_rt = rt = mrttable[hash]; rt; prev_rt = rt, rt = rt->mrt_next)
		if (origin->s_addr == rt->mrt_origin.s_addr)
			break;
	if (!rt)
		return (ESRCH);

	s = splnet();

	cmrt = mrtsrchash;
	cmrtend = cmrt + MSRCHASHSIZ;
	for ( ; cmrt < cmrtend; ++cmrt)
		if (*cmrt == rt)
			*cmrt = 0;

	if (prev_rt == rt)
		mrttable[hash] = rt->mrt_next;
	else
		prev_rt->mrt_next = rt->mrt_next;
	free(rt, M_MRTABLE);

	splx(s);
	return (0);
}

/*
 * IP multicast forwarding function. This function assumes that the packet
 * pointed to by "ip" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IP multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is tunneled
 * or erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */

#define IP_HDR_LEN  20	/* # bytes of fixed IP header (excluding options) */
#define TUNNEL_LEN  12  /* # bytes of IP option for tunnel encapsulation  */

int
ip_mforward(ip, ifp, m)
	register struct ip *ip;
	register struct ifnet *ifp;
	register struct mbuf *m;
{
	register struct mrt *rt;
	register struct vif *vifp;
	register int vifi;
	register u_char *ipoptions;
	u_long tunnel_src;

	if (ip->ip_hl < (IP_HDR_LEN + TUNNEL_LEN) >> 2 ||
	    (ipoptions = (u_char *)(ip + 1))[1] != IPOPT_LSRR ) {
		/*
		 * Packet arrived via a physical interface.
		 */
		tunnel_src = 0;
	} else {
		/*
		 * Packet arrived through a tunnel.
		 *
		 * A tunneled packet has a single NOP option and a
		 * two-element loose-source-and-record-route (LSRR)
		 * option immediately following the fixed-size part of
		 * the IP header.  At this point in processing, the IP
		 * header should contain the following IP addresses:
		 *
		 * original source          - in the source address field
		 * destination group        - in the destination address field
		 * remote tunnel end-point  - in the first  element of LSRR
		 * one of this host's addrs - in the second element of LSRR
		 *
		 * NOTE: RFC-1075 would have the original source and
		 * remote tunnel end-point addresses swapped.  However,
		 * that could cause delivery of ICMP error messages to
		 * innocent applications on intermediate routing
		 * hosts!  Therefore, we hereby change the spec.
		 */

		/*
		 * Verify that the tunnel options are well-formed.
		 */
		if (ipoptions[0] != IPOPT_NOP ||
		    ipoptions[2] != 11 ||	/* LSRR option length   */
		    ipoptions[3] != 12 ||	/* LSRR address pointer */
		    (tunnel_src = *(u_long *)(&ipoptions[4])) == 0) {
			mrtstat.mrts_bad_tunnel++;
			return (1);
		}

		/*
		 * Delete the tunnel options from the packet.
		 */
		ovbcopy((caddr_t)(ipoptions + TUNNEL_LEN), (caddr_t)ipoptions,
		    (unsigned)(m->m_len - (IP_HDR_LEN + TUNNEL_LEN)));
		m->m_len -= TUNNEL_LEN;
		ip->ip_len -= TUNNEL_LEN;
		ip->ip_hl -= TUNNEL_LEN >> 2;
	}

	/*
	 * Don't forward a packet with time-to-live of zero or one,
	 * or a packet destined to a local-only group.
	 */
	if (ip->ip_ttl <= 1 ||
	    ntohl(ip->ip_dst.s_addr) <= INADDR_MAX_LOCAL_GROUP)
		return ((int)tunnel_src);

	/*
	 * Don't forward if we don't have a route for the packet's origin.
	 */
	MRTFIND(ip->ip_src.s_addr, rt)
	if (rt == NULL) {
		mrtstat.mrts_no_route++;
		return ((int)tunnel_src);
	}

	/*
	 * Don't forward if it didn't arrive from the
	 * parent vif for its origin.
	 *
	 * Notes: v_ifp is zero for src route tunnels, multicast_decap_if
	 * for encapsulated tunnels and a real ifnet for non-tunnels so
	 * the first part of the if catches wrong physical interface or
	 * tunnel type; v_rmt_addr is zero for non-tunneled packets so
	 * the 2nd part catches both packets that arrive via a tunnel
	 * that shouldn't and packets that arrive via the wrong tunnel.
	 */
	vifi = rt->mrt_parent;
	if (viftable[vifi].v_ifp != ifp ||
	    (ifp == 0 && viftable[vifi].v_rmt_addr.s_addr != tunnel_src)) {
		/* came in the wrong interface */
		++mrtstat.mrts_wrong_if;
		return (int)tunnel_src;
	}

	/*
	 * For each vif, decide if a copy of the packet should be forwarded.
	 * Forward if:
	 *		- the ttl exceeds the vif's threshold AND
	 *		- the vif is a child in the origin's route AND
	 *		- ( the vif is not a leaf in the origin's route OR
	 *		    the destination group has members on the vif )
	 *
	 * (This might be speeded up with some sort of cache -- someday.)
	 */
	for (vifp = viftable, vifi = 0; vifi < numvifs; vifp++, vifi++) {
		if (ip->ip_ttl > vifp->v_threshold &&
		    VIFM_ISSET(vifi, rt->mrt_children) &&
		    (!VIFM_ISSET(vifi, rt->mrt_leaves) ||
		    grplst_member(vifp, ip->ip_dst))) {
			if (vifp->v_flags & VIFF_SRCRT)
				srcrt_send(ip, vifp, m);
			else if (vifp->v_flags & VIFF_TUNNEL)
				encap_send(ip, vifp, m);
			else
				phyint_send(ip, vifp, m);
		}
	}
	return ((int)tunnel_src);
}

static void
phyint_send(ip, vifp, m)
	register struct ip *ip;
	register struct vif *vifp;
	register struct mbuf *m;
{
	register struct mbuf *mb_copy;
	register struct ip_moptions *imo;
	register int error;
	struct ip_moptions simo;

	mb_copy = m_copy(m, 0, M_COPYALL);
	if (mb_copy == NULL)
		return;

	imo = &simo;
	imo->imo_multicast_ifp = vifp->v_ifp;
	imo->imo_multicast_ttl = ip->ip_ttl - 1;
	imo->imo_multicast_loop = 1;

	error = ip_output(mb_copy, NULL, NULL,
			  IP_FORWARDING|IP_MULTICASTOPTS, imo);
}

static void
srcrt_send(ip, vifp, m)
	register struct ip *ip;
	register struct vif *vifp;
	register struct mbuf *m;
{
	register struct mbuf *mb_copy, *mb_opts;
	register struct ip *ip_copy;
	register int error;
	register u_char *cp;

	/*
	 * Make sure that adding the tunnel options won't exceed the
	 * maximum allowed number of option bytes.
	 */
	if (ip->ip_hl > (60 - TUNNEL_LEN) >> 2) {
		mrtstat.mrts_cant_tunnel++;
		return;
	}

	mb_copy = m_copy(m, 0, M_COPYALL);
	if (mb_copy == NULL)
		return;
	ip_copy = mtod(mb_copy, struct ip *);
	ip_copy->ip_ttl--;
	ip_copy->ip_dst = vifp->v_rmt_addr;	/* remote tunnel end-point */
	/*
	 * Adjust the ip header length to account for the tunnel options.
	 */
	ip_copy->ip_hl += TUNNEL_LEN >> 2;
	ip_copy->ip_len += TUNNEL_LEN;
	MGETHDR(mb_opts, M_DONTWAIT, MT_HEADER);
	if (mb_opts == NULL) {
		m_freem(mb_copy);
		return;
	}
	/*
	 * 'Delete' the base ip header from the mb_copy chain
	 */
	mb_copy->m_len -= IP_HDR_LEN;
	mb_copy->m_data += IP_HDR_LEN;
	/*
	 * Make mb_opts be the new head of the packet chain.
	 * Any options of the packet were left in the old packet chain head
	 */
	mb_opts->m_next = mb_copy;
	mb_opts->m_len = IP_HDR_LEN + TUNNEL_LEN;
	mb_opts->m_data += MSIZE - mb_opts->m_len;
	/*
	 * Copy the base ip header from the mb_copy chain to the new head mbuf
	 */
	bcopy((caddr_t)ip_copy, mtod(mb_opts, caddr_t), IP_HDR_LEN);
	/*
	 * Add the NOP and LSRR after the base ip header
	 */
	cp = mtod(mb_opts, u_char *) + IP_HDR_LEN;
	*cp++ = IPOPT_NOP;
	*cp++ = IPOPT_LSRR;
	*cp++ = 11;		/* LSRR option length */
	*cp++ = 8;		/* LSSR pointer to second element */
	*(u_long*)cp = vifp->v_lcl_addr.s_addr;	/* local tunnel end-point */
	cp += 4;
	*(u_long*)cp = ip->ip_dst.s_addr;		/* destination group */

	error = ip_output(mb_opts, NULL, NULL, IP_FORWARDING, NULL);
}

static void
encap_send(ip, vifp, m)
	register struct ip *ip;
	register struct vif *vifp;
	register struct mbuf *m;
{
	register struct mbuf *mb_copy;
	register struct ip *ip_copy;
	register int i, len = ip->ip_len;

	/*
	 * copy the old packet & pullup it's IP header into the
	 * new mbuf so we can modify it.  Try to fill the new
	 * mbuf since if we don't the ethernet driver will.
	 */
	MGETHDR(mb_copy, M_DONTWAIT, MT_HEADER);
	if (mb_copy == NULL)
		return;
	mb_copy->m_data += 16;
	mb_copy->m_len = sizeof(multicast_encap_iphdr);
	if ((mb_copy->m_next = m_copy(m, 0, M_COPYALL)) == NULL) {
		m_freem(mb_copy);
		return;
	}
	i = MHLEN - 16;
	if (i > len)
		i = len;
	mb_copy = m_pullup(mb_copy, i);
	if (mb_copy == NULL)
		return;

	/*
	 * fill in the encapsulating IP header.
	 */
	ip_copy = mtod(mb_copy, struct ip *);
	*ip_copy = multicast_encap_iphdr;
	ip_copy->ip_id = htons(ip_id++);
	ip_copy->ip_len += len;
	ip_copy->ip_src = vifp->v_lcl_addr;
	ip_copy->ip_dst = vifp->v_rmt_addr;

	/*
	 * turn the encapsulated IP header back into a valid one.
	 */
	ip = (struct ip *)((caddr_t)ip_copy + sizeof(multicast_encap_iphdr));
	--ip->ip_ttl;
	HTONS(ip->ip_len);
	HTONS(ip->ip_off);
	ip->ip_sum = 0;
#if defined(LBL) && !defined(ultrix) && !defined(i386)
	ip->ip_sum = ~oc_cksum((caddr_t)ip, ip->ip_hl << 2, 0);
#else
	mb_copy->m_data += sizeof(multicast_encap_iphdr);
	ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
	mb_copy->m_data -= sizeof(multicast_encap_iphdr);
#endif
	ip_output(mb_copy, (struct mbuf *)0, (struct route *)0,
		  IP_FORWARDING, (struct mbuf *)0);
}

/*
 * De-encapsulate a packet and feed it back through ip input (this
 * routine is called whenever IP gets a packet with proto type
 * ENCAP_PROTO and a local destination address).
 */
static void
multiencap_decap(m, hlen)
	register struct mbuf *m;
	int hlen;
{
	struct ifnet *ifp;
	register struct ip *ip = mtod(m, struct ip *);
	register int s;
	register struct ifqueue *ifq;
	register struct vif *vifp;

	if (ip->ip_p != ENCAP_PROTO) {
		(*encap_oldrawip)(m, hlen);
		return;
	}
	/*
	 * dump the packet if it's not to a multicast destination or if
	 * we don't have an encapsulating tunnel with the source.
	 * Note:  This code assumes that the remote site IP address
	 * uniquely identifies the tunnel (i.e., that this site has
	 * at most one tunnel with the remote site).
	 */
	if (! IN_MULTICAST(ntohl(((struct ip *)((char *)ip + hlen))->ip_dst.s_addr))) {
		++mrtstat.mrts_bad_tunnel;
		m_freem(m);
		return;
	}
	if (ip->ip_src.s_addr != last_encap_src) {
		register struct vif *vife;

		vifp = viftable;
		vife = vifp + numvifs;
		last_encap_src = ip->ip_src.s_addr;
		last_encap_vif = 0;
		for ( ; vifp < vife; ++vifp)
			if (vifp->v_rmt_addr.s_addr == ip->ip_src.s_addr) {
				if ((vifp->v_flags & (VIFF_TUNNEL|VIFF_SRCRT))
				    == VIFF_TUNNEL)
					last_encap_vif = vifp;
				break;
			}
	}
	if ((vifp = last_encap_vif) == 0) {
		mrtstat.mrts_cant_tunnel++; /*XXX*/
		m_freem(m);
		return;
	}
	ifp = vifp->v_ifp;
	m->m_data += hlen;
	m->m_len -= hlen;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len -= hlen;
	ifq = &ipintrq;
	s = splimp();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
	} else {
		IF_ENQUEUE(ifq, m);
		/*
		 * normally we would need a "schednetisr(NETISR_IP)"
		 * here but we were called by ip_input and it is going
		 * to loop back & try to dequeue the packet we just
		 * queued as soon as we return so we avoid the
		 * unnecessary software interrrupt.
		 */
	}
	splx(s);
}
#endif
