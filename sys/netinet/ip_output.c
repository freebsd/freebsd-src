/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)ip_output.c	8.3 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ipfw.h"
#include "opt_ipsec.h"
#include "opt_mac.h"
#include "opt_mbuf_stress_test.h"
#include "opt_mpath.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>
#include <sys/vimage.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/pfil.h>
#include <net/route.h>
#ifdef RADIX_MPATH
#include <net/radix_mpath.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>

#ifdef IPSEC
#include <netinet/ip_ipsec.h>
#include <netipsec/ipsec.h>
#endif /* IPSEC*/

#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

#define print_ip(x, a, y)	 printf("%s %d.%d.%d.%d%s",\
				x, (ntohl(a.s_addr)>>24)&0xFF,\
				  (ntohl(a.s_addr)>>16)&0xFF,\
				  (ntohl(a.s_addr)>>8)&0xFF,\
				  (ntohl(a.s_addr))&0xFF, y);

u_short ip_id;

#ifdef MBUF_STRESS_TEST
int mbuf_frag_size = 0;
SYSCTL_INT(_net_inet_ip, OID_AUTO, mbuf_frag_size, CTLFLAG_RW,
	&mbuf_frag_size, 0, "Fragment outgoing mbufs to this size");
#endif

static void	ip_mloopback
	(struct ifnet *, struct mbuf *, struct sockaddr_in *, int);


extern	struct protosw inetsw[];

/*
 * IP output.  The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 * In the IP forwarding case, the packet will arrive with options already
 * inserted, so must have a NULL opt pointer.
 */
int
ip_output(struct mbuf *m, struct mbuf *opt, struct route *ro, int flags,
    struct ip_moptions *imo, struct inpcb *inp)
{
	INIT_VNET_NET(curvnet);
	INIT_VNET_INET(curvnet);
	struct ip *ip;
	struct ifnet *ifp = NULL;	/* keep compiler happy */
	struct mbuf *m0;
	int hlen = sizeof (struct ip);
	int mtu;
	int len, error = 0;
	struct sockaddr_in *dst = NULL;	/* keep compiler happy */
	struct in_ifaddr *ia = NULL;
	int isbroadcast, sw_csum;
	struct route iproute;
	struct in_addr odst;
#ifdef IPFIREWALL_FORWARD
	struct m_tag *fwd_tag = NULL;
#endif
	M_ASSERTPKTHDR(m);

	if (ro == NULL) {
		ro = &iproute;
		bzero(ro, sizeof (*ro));
	}

	if (inp != NULL)
		INP_LOCK_ASSERT(inp);

	if (opt) {
		len = 0;
		m = ip_insertoptions(m, opt, &len);
		if (len != 0)
			hlen = len;
	}
	ip = mtod(m, struct ip *);

	/*
	 * Fill in IP header.  If we are not allowing fragmentation,
	 * then the ip_id field is meaningless, but we don't set it
	 * to zero.  Doing so causes various problems when devices along
	 * the path (routers, load balancers, firewalls, etc.) illegally
	 * disable DF on our packet.  Note that a 16-bit counter
	 * will wrap around in less than 10 seconds at 100 Mbit/s on a
	 * medium with MTU 1500.  See Steven M. Bellovin, "A Technique
	 * for Counting NATted Hosts", Proc. IMW'02, available at
	 * <http://www.cs.columbia.edu/~smb/papers/fnat.pdf>.
	 */
	if ((flags & (IP_FORWARDING|IP_RAWOUTPUT)) == 0) {
		ip->ip_v = IPVERSION;
		ip->ip_hl = hlen >> 2;
		ip->ip_id = ip_newid();
		V_ipstat.ips_localout++;
	} else {
		hlen = ip->ip_hl << 2;
	}

	dst = (struct sockaddr_in *)&ro->ro_dst;
again:
	/*
	 * If there is a cached route,
	 * check that it is to the same destination
	 * and is still up.  If not, free it and try again.
	 * The address family should also be checked in case of sharing the
	 * cache with IPv6.
	 */
	if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
			  dst->sin_family != AF_INET ||
			  dst->sin_addr.s_addr != ip->ip_dst.s_addr)) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = (struct rtentry *)NULL;
	}
#ifdef IPFIREWALL_FORWARD
	if (ro->ro_rt == NULL && fwd_tag == NULL) {
#else
	if (ro->ro_rt == NULL) {
#endif
		bzero(dst, sizeof(*dst));
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = ip->ip_dst;
	}
	/*
	 * If routing to interface only, short circuit routing lookup.
	 * The use of an all-ones broadcast address implies this; an
	 * interface is specified by the broadcast address of an interface,
	 * or the destination address of a ptp interface.
	 */
	if (flags & IP_SENDONES) {
		if ((ia = ifatoia(ifa_ifwithbroadaddr(sintosa(dst)))) == NULL &&
		    (ia = ifatoia(ifa_ifwithdstaddr(sintosa(dst)))) == NULL) {
			V_ipstat.ips_noroute++;
			error = ENETUNREACH;
			goto bad;
		}
		ip->ip_dst.s_addr = INADDR_BROADCAST;
		dst->sin_addr = ip->ip_dst;
		ifp = ia->ia_ifp;
		ip->ip_ttl = 1;
		isbroadcast = 1;
	} else if (flags & IP_ROUTETOIF) {
		if ((ia = ifatoia(ifa_ifwithdstaddr(sintosa(dst)))) == NULL &&
		    (ia = ifatoia(ifa_ifwithnet(sintosa(dst)))) == NULL) {
			V_ipstat.ips_noroute++;
			error = ENETUNREACH;
			goto bad;
		}
		ifp = ia->ia_ifp;
		ip->ip_ttl = 1;
		isbroadcast = in_broadcast(dst->sin_addr, ifp);
	} else if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) &&
	    imo != NULL && imo->imo_multicast_ifp != NULL) {
		/*
		 * Bypass the normal routing lookup for multicast
		 * packets if the interface is specified.
		 */
		ifp = imo->imo_multicast_ifp;
		IFP_TO_IA(ifp, ia);
		isbroadcast = 0;	/* fool gcc */
	} else {
		/*
		 * We want to do any cloning requested by the link layer,
		 * as this is probably required in all cases for correct
		 * operation (as it is for ARP).
		 */
		if (ro->ro_rt == NULL)
#ifdef RADIX_MPATH
			rtalloc_mpath_fib(ro,
			    ntohl(ip->ip_src.s_addr ^ ip->ip_dst.s_addr),
			    inp ? inp->inp_inc.inc_fibnum : M_GETFIB(m));
#else
			in_rtalloc_ign(ro, 0,
			    inp ? inp->inp_inc.inc_fibnum : M_GETFIB(m));
#endif
		if (ro->ro_rt == NULL) {
			V_ipstat.ips_noroute++;
			error = EHOSTUNREACH;
			goto bad;
		}
		ia = ifatoia(ro->ro_rt->rt_ifa);
		ifp = ro->ro_rt->rt_ifp;
		ro->ro_rt->rt_rmx.rmx_pksent++;
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = (struct sockaddr_in *)ro->ro_rt->rt_gateway;
		if (ro->ro_rt->rt_flags & RTF_HOST)
			isbroadcast = (ro->ro_rt->rt_flags & RTF_BROADCAST);
		else
			isbroadcast = in_broadcast(dst->sin_addr, ifp);
	}
	/*
	 * Calculate MTU.  If we have a route that is up, use that,
	 * otherwise use the interface's MTU.
	 */
	if (ro->ro_rt != NULL && (ro->ro_rt->rt_flags & (RTF_UP|RTF_HOST))) {
		/*
		 * This case can happen if the user changed the MTU
		 * of an interface after enabling IP on it.  Because
		 * most netifs don't keep track of routes pointing to
		 * them, there is no way for one to update all its
		 * routes when the MTU is changed.
		 */
		if (ro->ro_rt->rt_rmx.rmx_mtu > ifp->if_mtu)
			ro->ro_rt->rt_rmx.rmx_mtu = ifp->if_mtu;
		mtu = ro->ro_rt->rt_rmx.rmx_mtu;
	} else {
		mtu = ifp->if_mtu;
	}
	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
		struct in_multi *inm;

		m->m_flags |= M_MCAST;
		/*
		 * IP destination address is multicast.  Make sure "dst"
		 * still points to the address in "ro".  (It may have been
		 * changed to point to a gateway address, above.)
		 */
		dst = (struct sockaddr_in *)&ro->ro_dst;
		/*
		 * See if the caller provided any multicast options
		 */
		if (imo != NULL) {
			ip->ip_ttl = imo->imo_multicast_ttl;
			if (imo->imo_multicast_vif != -1)
				ip->ip_src.s_addr =
				    ip_mcast_src ?
				    ip_mcast_src(imo->imo_multicast_vif) :
				    INADDR_ANY;
		} else
			ip->ip_ttl = IP_DEFAULT_MULTICAST_TTL;
		/*
		 * Confirm that the outgoing interface supports multicast.
		 */
		if ((imo == NULL) || (imo->imo_multicast_vif == -1)) {
			if ((ifp->if_flags & IFF_MULTICAST) == 0) {
				V_ipstat.ips_noroute++;
				error = ENETUNREACH;
				goto bad;
			}
		}
		/*
		 * If source address not specified yet, use address
		 * of outgoing interface.
		 */
		if (ip->ip_src.s_addr == INADDR_ANY) {
			/* Interface may have no addresses. */
			if (ia != NULL)
				ip->ip_src = IA_SIN(ia)->sin_addr;
		}

		IN_MULTI_LOCK();
		IN_LOOKUP_MULTI(ip->ip_dst, ifp, inm);
		if (inm != NULL &&
		   (imo == NULL || imo->imo_multicast_loop)) {
			IN_MULTI_UNLOCK();
			/*
			 * If we belong to the destination multicast group
			 * on the outgoing interface, and the caller did not
			 * forbid loopback, loop back a copy.
			 */
			ip_mloopback(ifp, m, dst, hlen);
		}
		else {
			IN_MULTI_UNLOCK();
			/*
			 * If we are acting as a multicast router, perform
			 * multicast forwarding as if the packet had just
			 * arrived on the interface to which we are about
			 * to send.  The multicast forwarding function
			 * recursively calls this function, using the
			 * IP_FORWARDING flag to prevent infinite recursion.
			 *
			 * Multicasts that are looped back by ip_mloopback(),
			 * above, will be forwarded by the ip_input() routine,
			 * if necessary.
			 */
			if (V_ip_mrouter && (flags & IP_FORWARDING) == 0) {
				/*
				 * If rsvp daemon is not running, do not
				 * set ip_moptions. This ensures that the packet
				 * is multicast and not just sent down one link
				 * as prescribed by rsvpd.
				 */
				if (!V_rsvp_on)
					imo = NULL;
				if (ip_mforward &&
				    ip_mforward(ip, ifp, m, imo) != 0) {
					m_freem(m);
					goto done;
				}
			}
		}

		/*
		 * Multicasts with a time-to-live of zero may be looped-
		 * back, above, but must not be transmitted on a network.
		 * Also, multicasts addressed to the loopback interface
		 * are not sent -- the above call to ip_mloopback() will
		 * loop back a copy if this host actually belongs to the
		 * destination group on the loopback interface.
		 */
		if (ip->ip_ttl == 0 || ifp->if_flags & IFF_LOOPBACK) {
			m_freem(m);
			goto done;
		}

		goto sendit;
	}

	/*
	 * If the source address is not specified yet, use the address
	 * of the outoing interface.
	 */
	if (ip->ip_src.s_addr == INADDR_ANY) {
		/* Interface may have no addresses. */
		if (ia != NULL) {
			ip->ip_src = IA_SIN(ia)->sin_addr;
		}
	}

	/*
	 * Verify that we have any chance at all of being able to queue the
	 * packet or packet fragments, unless ALTQ is enabled on the given
	 * interface in which case packetdrop should be done by queueing.
	 */
#ifdef ALTQ
	if ((!ALTQ_IS_ENABLED(&ifp->if_snd)) &&
	    ((ifp->if_snd.ifq_len + ip->ip_len / mtu + 1) >=
	    ifp->if_snd.ifq_maxlen))
#else
	if ((ifp->if_snd.ifq_len + ip->ip_len / mtu + 1) >=
	    ifp->if_snd.ifq_maxlen)
#endif /* ALTQ */
	{
		error = ENOBUFS;
		V_ipstat.ips_odropped++;
		ifp->if_snd.ifq_drops += (ip->ip_len / ifp->if_mtu + 1);
		goto bad;
	}

	/*
	 * Look for broadcast address and
	 * verify user is allowed to send
	 * such a packet.
	 */
	if (isbroadcast) {
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if ((flags & IP_ALLOWBROADCAST) == 0) {
			error = EACCES;
			goto bad;
		}
		/* don't allow broadcast messages to be fragmented */
		if (ip->ip_len > mtu) {
			error = EMSGSIZE;
			goto bad;
		}
		m->m_flags |= M_BCAST;
	} else {
		m->m_flags &= ~M_BCAST;
	}

sendit:
#ifdef IPSEC
	switch(ip_ipsec_output(&m, inp, &flags, &error, &ro, &iproute, &dst, &ia, &ifp)) {
	case 1:
		goto bad;
	case -1:
		goto done;
	case 0:
	default:
		break;	/* Continue with packet processing. */
	}
	/* Update variables that are affected by ipsec4_output(). */
	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;
#endif /* IPSEC */

	/* Jump over all PFIL processing if hooks are not active. */
	if (!PFIL_HOOKED(&inet_pfil_hook))
		goto passout;

	/* Run through list of hooks for output packets. */
	odst.s_addr = ip->ip_dst.s_addr;
	error = pfil_run_hooks(&inet_pfil_hook, &m, ifp, PFIL_OUT, inp);
	if (error != 0 || m == NULL)
		goto done;

	ip = mtod(m, struct ip *);

	/* See if destination IP address was changed by packet filter. */
	if (odst.s_addr != ip->ip_dst.s_addr) {
		m->m_flags |= M_SKIP_FIREWALL;
		/* If destination is now ourself drop to ip_input(). */
		if (in_localip(ip->ip_dst)) {
			m->m_flags |= M_FASTFWD_OURS;
			if (m->m_pkthdr.rcvif == NULL)
				m->m_pkthdr.rcvif = V_loif;
			if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
				m->m_pkthdr.csum_flags |=
				    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			}
			m->m_pkthdr.csum_flags |=
			    CSUM_IP_CHECKED | CSUM_IP_VALID;

			error = netisr_queue(NETISR_IP, m);
			goto done;
		} else
			goto again;	/* Redo the routing table lookup. */
	}

#ifdef IPFIREWALL_FORWARD
	/* See if local, if yes, send it to netisr with IP_FASTFWD_OURS. */
	if (m->m_flags & M_FASTFWD_OURS) {
		if (m->m_pkthdr.rcvif == NULL)
			m->m_pkthdr.rcvif = V_loif;
		if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
			m->m_pkthdr.csum_flags |=
			    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			m->m_pkthdr.csum_data = 0xffff;
		}
		m->m_pkthdr.csum_flags |=
			    CSUM_IP_CHECKED | CSUM_IP_VALID;

		error = netisr_queue(NETISR_IP, m);
		goto done;
	}
	/* Or forward to some other address? */
	fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL);
	if (fwd_tag) {
		dst = (struct sockaddr_in *)&ro->ro_dst;
		bcopy((fwd_tag+1), dst, sizeof(struct sockaddr_in));
		m->m_flags |= M_SKIP_FIREWALL;
		m_tag_delete(m, fwd_tag);
		goto again;
	}
#endif /* IPFIREWALL_FORWARD */

passout:
	/* 127/8 must not appear on wire - RFC1122. */
	if ((ntohl(ip->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    (ntohl(ip->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) {
		if ((ifp->if_flags & IFF_LOOPBACK) == 0) {
			V_ipstat.ips_badaddr++;
			error = EADDRNOTAVAIL;
			goto bad;
		}
	}

	m->m_pkthdr.csum_flags |= CSUM_IP;
	sw_csum = m->m_pkthdr.csum_flags & ~ifp->if_hwassist;
	if (sw_csum & CSUM_DELAY_DATA) {
		in_delayed_cksum(m);
		sw_csum &= ~CSUM_DELAY_DATA;
	}
	m->m_pkthdr.csum_flags &= ifp->if_hwassist;

	/*
	 * If small enough for interface, or the interface will take
	 * care of the fragmentation for us, we can just send directly.
	 */
	if (ip->ip_len <= mtu ||
	    (m->m_pkthdr.csum_flags & ifp->if_hwassist & CSUM_TSO) != 0 ||
	    ((ip->ip_off & IP_DF) == 0 && (ifp->if_hwassist & CSUM_FRAGMENT))) {
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
		ip->ip_sum = 0;
		if (sw_csum & CSUM_DELAY_IP)
			ip->ip_sum = in_cksum(m, hlen);

		/*
		 * Record statistics for this interface address.
		 * With CSUM_TSO the byte/packet count will be slightly
		 * incorrect because we count the IP+TCP headers only
		 * once instead of for every generated packet.
		 */
		if (!(flags & IP_FORWARDING) && ia) {
			if (m->m_pkthdr.csum_flags & CSUM_TSO)
				ia->ia_ifa.if_opackets +=
				    m->m_pkthdr.len / m->m_pkthdr.tso_segsz;
			else
				ia->ia_ifa.if_opackets++;
			ia->ia_ifa.if_obytes += m->m_pkthdr.len;
		}
#ifdef MBUF_STRESS_TEST
		if (mbuf_frag_size && m->m_pkthdr.len > mbuf_frag_size)
			m = m_fragment(m, M_DONTWAIT, mbuf_frag_size);
#endif
		/*
		 * Reset layer specific mbuf flags
		 * to avoid confusing lower layers.
		 */
		m->m_flags &= ~(M_PROTOFLAGS);

		error = (*ifp->if_output)(ifp, m,
				(struct sockaddr *)dst, ro->ro_rt);
		goto done;
	}

	/* Balk when DF bit is set or the interface didn't support TSO. */
	if ((ip->ip_off & IP_DF) || (m->m_pkthdr.csum_flags & CSUM_TSO)) {
		error = EMSGSIZE;
		V_ipstat.ips_cantfrag++;
		goto bad;
	}

	/*
	 * Too large for interface; fragment if possible. If successful,
	 * on return, m will point to a list of packets to be sent.
	 */
	error = ip_fragment(ip, &m, mtu, ifp->if_hwassist, sw_csum);
	if (error)
		goto bad;
	for (; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = 0;
		if (error == 0) {
			/* Record statistics for this interface address. */
			if (ia != NULL) {
				ia->ia_ifa.if_opackets++;
				ia->ia_ifa.if_obytes += m->m_pkthdr.len;
			}
			/*
			 * Reset layer specific mbuf flags
			 * to avoid confusing upper layers.
			 */
			m->m_flags &= ~(M_PROTOFLAGS);

			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst, ro->ro_rt);
		} else
			m_freem(m);
	}

	if (error == 0)
		V_ipstat.ips_fragmented++;

done:
	if (ro == &iproute && ro->ro_rt) {
		RTFREE(ro->ro_rt);
	}
	return (error);
bad:
	m_freem(m);
	goto done;
}

/*
 * Create a chain of fragments which fit the given mtu. m_frag points to the
 * mbuf to be fragmented; on return it points to the chain with the fragments.
 * Return 0 if no error. If error, m_frag may contain a partially built
 * chain of fragments that should be freed by the caller.
 *
 * if_hwassist_flags is the hw offload capabilities (see if_data.ifi_hwassist)
 * sw_csum contains the delayed checksums flags (e.g., CSUM_DELAY_IP).
 */
int
ip_fragment(struct ip *ip, struct mbuf **m_frag, int mtu,
    u_long if_hwassist_flags, int sw_csum)
{
	INIT_VNET_INET(curvnet);
	int error = 0;
	int hlen = ip->ip_hl << 2;
	int len = (mtu - hlen) & ~7;	/* size of payload in each fragment */
	int off;
	struct mbuf *m0 = *m_frag;	/* the original packet		*/
	int firstlen;
	struct mbuf **mnext;
	int nfrags;

	if (ip->ip_off & IP_DF) {	/* Fragmentation not allowed */
		V_ipstat.ips_cantfrag++;
		return EMSGSIZE;
	}

	/*
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (len < 8)
		return EMSGSIZE;

	/*
	 * If the interface will not calculate checksums on
	 * fragmented packets, then do it here.
	 */
	if (m0->m_pkthdr.csum_flags & CSUM_DELAY_DATA &&
	    (if_hwassist_flags & CSUM_IP_FRAGS) == 0) {
		in_delayed_cksum(m0);
		m0->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}

	if (len > PAGE_SIZE) {
		/* 
		 * Fragment large datagrams such that each segment 
		 * contains a multiple of PAGE_SIZE amount of data, 
		 * plus headers. This enables a receiver to perform 
		 * page-flipping zero-copy optimizations.
		 *
		 * XXX When does this help given that sender and receiver
		 * could have different page sizes, and also mtu could
		 * be less than the receiver's page size ?
		 */
		int newlen;
		struct mbuf *m;

		for (m = m0, off = 0; m && (off+m->m_len) <= mtu; m = m->m_next)
			off += m->m_len;

		/*
		 * firstlen (off - hlen) must be aligned on an 
		 * 8-byte boundary
		 */
		if (off < hlen)
			goto smart_frag_failure;
		off = ((off - hlen) & ~7) + hlen;
		newlen = (~PAGE_MASK) & mtu;
		if ((newlen + sizeof (struct ip)) > mtu) {
			/* we failed, go back the default */
smart_frag_failure:
			newlen = len;
			off = hlen + len;
		}
		len = newlen;

	} else {
		off = hlen + len;
	}

	firstlen = off - hlen;
	mnext = &m0->m_nextpkt;		/* pointer to next packet */

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 * Here, m0 is the original packet, m is the fragment being created.
	 * The fragments are linked off the m_nextpkt of the original
	 * packet, which after processing serves as the first fragment.
	 */
	for (nfrags = 1; off < ip->ip_len; off += len, nfrags++) {
		struct ip *mhip;	/* ip header on the fragment */
		struct mbuf *m;
		int mhlen = sizeof (struct ip);

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			error = ENOBUFS;
			V_ipstat.ips_odropped++;
			goto done;
		}
		m->m_flags |= (m0->m_flags & M_MCAST) | M_FRAG;
		/*
		 * In the first mbuf, leave room for the link header, then
		 * copy the original IP header including options. The payload
		 * goes into an additional mbuf chain returned by m_copy().
		 */
		m->m_data += max_linkhdr;
		mhip = mtod(m, struct ip *);
		*mhip = *ip;
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			mhip->ip_v = IPVERSION;
			mhip->ip_hl = mhlen >> 2;
		}
		m->m_len = mhlen;
		/* XXX do we need to add ip->ip_off below ? */
		mhip->ip_off = ((off - hlen) >> 3) + ip->ip_off;
		if (off + len >= ip->ip_len) {	/* last fragment */
			len = ip->ip_len - off;
			m->m_flags |= M_LASTFRAG;
		} else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_short)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == NULL) {	/* copy failed */
			m_free(m);
			error = ENOBUFS;	/* ??? */
			V_ipstat.ips_odropped++;
			goto done;
		}
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = NULL;
#ifdef MAC
		mac_netinet_fragment(m0, m);
#endif
		m->m_pkthdr.csum_flags = m0->m_pkthdr.csum_flags;
		mhip->ip_off = htons(mhip->ip_off);
		mhip->ip_sum = 0;
		if (sw_csum & CSUM_DELAY_IP)
			mhip->ip_sum = in_cksum(m, mhlen);
		*mnext = m;
		mnext = &m->m_nextpkt;
	}
	V_ipstat.ips_ofragments += nfrags;

	/* set first marker for fragment chain */
	m0->m_flags |= M_FIRSTFRAG | M_FRAG;
	m0->m_pkthdr.csum_data = nfrags;

	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header.
	 */
	m_adj(m0, hlen + firstlen - ip->ip_len);
	m0->m_pkthdr.len = hlen + firstlen;
	ip->ip_len = htons((u_short)m0->m_pkthdr.len);
	ip->ip_off |= IP_MF;
	ip->ip_off = htons(ip->ip_off);
	ip->ip_sum = 0;
	if (sw_csum & CSUM_DELAY_IP)
		ip->ip_sum = in_cksum(m0, hlen);

done:
	*m_frag = m0;
	return error;
}

void
in_delayed_cksum(struct mbuf *m)
{
	struct ip *ip;
	u_short csum, offset;

	ip = mtod(m, struct ip *);
	offset = ip->ip_hl << 2 ;
	csum = in_cksum_skip(m, ip->ip_len, offset);
	if (m->m_pkthdr.csum_flags & CSUM_UDP && csum == 0)
		csum = 0xffff;
	offset += m->m_pkthdr.csum_data;	/* checksum offset */

	if (offset + sizeof(u_short) > m->m_len) {
		printf("delayed m_pullup, m->len: %d  off: %d  p: %d\n",
		    m->m_len, offset, ip->ip_p);
		/*
		 * XXX
		 * this shouldn't happen, but if it does, the
		 * correct behavior may be to insert the checksum
		 * in the appropriate next mbuf in the chain.
		 */
		return;
	}
	*(u_short *)(m->m_data + offset) = csum;
}

/*
 * IP socket option processing.
 */
int
ip_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct	inpcb *inp = sotoinpcb(so);
	int	error, optval;

	error = optval = 0;
	if (sopt->sopt_level != IPPROTO_IP) {
		return (EINVAL);
	}

	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (sopt->sopt_name) {
		case IP_OPTIONS:
#ifdef notyet
		case IP_RETOPTS:
#endif
		{
			struct mbuf *m;
			if (sopt->sopt_valsize > MLEN) {
				error = EMSGSIZE;
				break;
			}
			MGET(m, sopt->sopt_td ? M_WAIT : M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				error = ENOBUFS;
				break;
			}
			m->m_len = sopt->sopt_valsize;
			error = sooptcopyin(sopt, mtod(m, char *), m->m_len,
					    m->m_len);
			if (error) {
				m_free(m);
				break;
			}
			INP_WLOCK(inp);
			error = ip_pcbopts(inp, sopt->sopt_name, m);
			INP_WUNLOCK(inp);
			return (error);
		}

		case IP_TOS:
		case IP_TTL:
		case IP_MINTTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IP_RECVTTL:
		case IP_RECVIF:
		case IP_FAITH:
		case IP_ONESBCAST:
		case IP_DONTFRAG:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			switch (sopt->sopt_name) {
			case IP_TOS:
				inp->inp_ip_tos = optval;
				break;

			case IP_TTL:
				inp->inp_ip_ttl = optval;
				break;

			case IP_MINTTL:
				if (optval > 0 && optval <= MAXTTL)
					inp->inp_ip_minttl = optval;
				else
					error = EINVAL;
				break;

#define	OPTSET(bit) do {						\
	INP_WLOCK(inp);							\
	if (optval)							\
		inp->inp_flags |= bit;					\
	else								\
		inp->inp_flags &= ~bit;					\
	INP_WUNLOCK(inp);						\
} while (0)

			case IP_RECVOPTS:
				OPTSET(INP_RECVOPTS);
				break;

			case IP_RECVRETOPTS:
				OPTSET(INP_RECVRETOPTS);
				break;

			case IP_RECVDSTADDR:
				OPTSET(INP_RECVDSTADDR);
				break;

			case IP_RECVTTL:
				OPTSET(INP_RECVTTL);
				break;

			case IP_RECVIF:
				OPTSET(INP_RECVIF);
				break;

			case IP_FAITH:
				OPTSET(INP_FAITH);
				break;

			case IP_ONESBCAST:
				OPTSET(INP_ONESBCAST);
				break;
			case IP_DONTFRAG:
				OPTSET(INP_DONTFRAG);
				break;
			}
			break;
#undef OPTSET

		/*
		 * Multicast socket options are processed by the in_mcast
		 * module.
		 */
		case IP_MULTICAST_IF:
		case IP_MULTICAST_VIF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
		case IP_ADD_SOURCE_MEMBERSHIP:
		case IP_DROP_SOURCE_MEMBERSHIP:
		case IP_BLOCK_SOURCE:
		case IP_UNBLOCK_SOURCE:
		case IP_MSFILTER:
		case MCAST_JOIN_GROUP:
		case MCAST_LEAVE_GROUP:
		case MCAST_JOIN_SOURCE_GROUP:
		case MCAST_LEAVE_SOURCE_GROUP:
		case MCAST_BLOCK_SOURCE:
		case MCAST_UNBLOCK_SOURCE:
			error = inp_setmoptions(inp, sopt);
			break;

		case IP_PORTRANGE:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			INP_WLOCK(inp);
			switch (optval) {
			case IP_PORTRANGE_DEFAULT:
				inp->inp_flags &= ~(INP_LOWPORT);
				inp->inp_flags &= ~(INP_HIGHPORT);
				break;

			case IP_PORTRANGE_HIGH:
				inp->inp_flags &= ~(INP_LOWPORT);
				inp->inp_flags |= INP_HIGHPORT;
				break;

			case IP_PORTRANGE_LOW:
				inp->inp_flags &= ~(INP_HIGHPORT);
				inp->inp_flags |= INP_LOWPORT;
				break;

			default:
				error = EINVAL;
				break;
			}
			INP_WUNLOCK(inp);
			break;

#ifdef IPSEC
		case IP_IPSEC_POLICY:
		{
			caddr_t req;
			struct mbuf *m;

			if ((error = soopt_getm(sopt, &m)) != 0) /* XXX */
				break;
			if ((error = soopt_mcopyin(sopt, m)) != 0) /* XXX */
				break;
			req = mtod(m, caddr_t);
			error = ipsec4_set_policy(inp, sopt->sopt_name, req,
			    m->m_len, (sopt->sopt_td != NULL) ?
			    sopt->sopt_td->td_ucred : NULL);
			m_freem(m);
			break;
		}
#endif /* IPSEC */

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	case SOPT_GET:
		switch (sopt->sopt_name) {
		case IP_OPTIONS:
		case IP_RETOPTS:
			if (inp->inp_options)
				error = sooptcopyout(sopt, 
						     mtod(inp->inp_options,
							  char *),
						     inp->inp_options->m_len);
			else
				sopt->sopt_valsize = 0;
			break;

		case IP_TOS:
		case IP_TTL:
		case IP_MINTTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IP_RECVTTL:
		case IP_RECVIF:
		case IP_PORTRANGE:
		case IP_FAITH:
		case IP_ONESBCAST:
		case IP_DONTFRAG:
			switch (sopt->sopt_name) {

			case IP_TOS:
				optval = inp->inp_ip_tos;
				break;

			case IP_TTL:
				optval = inp->inp_ip_ttl;
				break;

			case IP_MINTTL:
				optval = inp->inp_ip_minttl;
				break;

#define	OPTBIT(bit)	(inp->inp_flags & bit ? 1 : 0)

			case IP_RECVOPTS:
				optval = OPTBIT(INP_RECVOPTS);
				break;

			case IP_RECVRETOPTS:
				optval = OPTBIT(INP_RECVRETOPTS);
				break;

			case IP_RECVDSTADDR:
				optval = OPTBIT(INP_RECVDSTADDR);
				break;

			case IP_RECVTTL:
				optval = OPTBIT(INP_RECVTTL);
				break;

			case IP_RECVIF:
				optval = OPTBIT(INP_RECVIF);
				break;

			case IP_PORTRANGE:
				if (inp->inp_flags & INP_HIGHPORT)
					optval = IP_PORTRANGE_HIGH;
				else if (inp->inp_flags & INP_LOWPORT)
					optval = IP_PORTRANGE_LOW;
				else
					optval = 0;
				break;

			case IP_FAITH:
				optval = OPTBIT(INP_FAITH);
				break;

			case IP_ONESBCAST:
				optval = OPTBIT(INP_ONESBCAST);
				break;
			case IP_DONTFRAG:
				optval = OPTBIT(INP_DONTFRAG);
				break;
			}
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;

		/*
		 * Multicast socket options are processed by the in_mcast
		 * module.
		 */
		case IP_MULTICAST_IF:
		case IP_MULTICAST_VIF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_MSFILTER:
			error = inp_getmoptions(inp, sopt);
			break;

#ifdef IPSEC
		case IP_IPSEC_POLICY:
		{
			struct mbuf *m = NULL;
			caddr_t req = NULL;
			size_t len = 0;

			if (m != 0) {
				req = mtod(m, caddr_t);
				len = m->m_len;
			}
			error = ipsec4_get_policy(sotoinpcb(so), req, len, &m);
			if (error == 0)
				error = soopt_mcopyout(sopt, m); /* XXX */
			if (error == 0)
				m_freem(m);
			break;
		}
#endif /* IPSEC */

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}

/*
 * Routine called from ip_output() to loop back a copy of an IP multicast
 * packet to the input queue of a specified interface.  Note that this
 * calls the output routine of the loopback "driver", but with an interface
 * pointer that might NOT be a loopback interface -- evil, but easier than
 * replicating that code here.
 */
static void
ip_mloopback(struct ifnet *ifp, struct mbuf *m, struct sockaddr_in *dst,
    int hlen)
{
	register struct ip *ip;
	struct mbuf *copym;

	/*
	 * Make a deep copy of the packet because we're going to
	 * modify the pack in order to generate checksums.
	 */
	copym = m_dup(m, M_DONTWAIT);
	if (copym != NULL && (copym->m_flags & M_EXT || copym->m_len < hlen))
		copym = m_pullup(copym, hlen);
	if (copym != NULL) {
		/* If needed, compute the checksum and mark it as valid. */
		if (copym->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
			in_delayed_cksum(copym);
			copym->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
			copym->m_pkthdr.csum_flags |=
			    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			copym->m_pkthdr.csum_data = 0xffff;
		}
		/*
		 * We don't bother to fragment if the IP length is greater
		 * than the interface's MTU.  Can this possibly matter?
		 */
		ip = mtod(copym, struct ip *);
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(copym, hlen);
#if 1 /* XXX */
		if (dst->sin_family != AF_INET) {
			printf("ip_mloopback: bad address family %d\n",
						dst->sin_family);
			dst->sin_family = AF_INET;
		}
#endif
		if_simloop(ifp, copym, dst->sin_family, 0);
	}
}
