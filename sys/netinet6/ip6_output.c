/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$KAME: ip6_output.c,v 1.279 2002/01/26 06:12:30 jinmei Exp $
 */

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
 * 3. Neither the name of the University nor the names of its contributors
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

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_kern_tls.h"
#include "opt_ratelimit.h"
#include "opt_route.h"
#include "opt_rss.h"
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktls.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/ucred.h>

#include <machine/in_cksum.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>
#include <net/if_llatbl.h>
#include <net/ethernet.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/pfil.h>
#include <net/rss_config.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet6/in6_fib.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6_var.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_rss.h>

#include <netipsec/ipsec_support.h>
#if defined(SCTP) || defined(SCTP_SUPPORT)
#include <netinet/sctp.h>
#include <netinet/sctp_crc32.h>
#endif

#include <netinet6/scope6_var.h>

extern int in6_mcast_loop;

struct ip6_exthdrs {
	struct mbuf *ip6e_ip6;
	struct mbuf *ip6e_hbh;
	struct mbuf *ip6e_dest1;
	struct mbuf *ip6e_rthdr;
	struct mbuf *ip6e_dest2;
};

static MALLOC_DEFINE(M_IP6OPT, "ip6opt", "IPv6 options");

static int ip6_pcbopt(int, u_char *, int, struct ip6_pktopts **,
			   struct ucred *, int);
static int ip6_pcbopts(struct ip6_pktopts **, struct mbuf *,
	struct socket *, struct sockopt *);
static int ip6_getpcbopt(struct inpcb *, int, struct sockopt *);
static int ip6_setpktopt(int, u_char *, int, struct ip6_pktopts *,
	struct ucred *, int, int, int);

static int ip6_copyexthdr(struct mbuf **, caddr_t, int);
static int ip6_insertfraghdr(struct mbuf *, struct mbuf *, int,
	struct ip6_frag **);
static int ip6_insert_jumboopt(struct ip6_exthdrs *, u_int32_t);
static int ip6_splithdr(struct mbuf *, struct ip6_exthdrs *);
static int ip6_getpmtu(struct route_in6 *, int,
	struct ifnet *, const struct in6_addr *, u_long *, int *, u_int,
	u_int);
static int ip6_calcmtu(struct ifnet *, const struct in6_addr *, u_long,
	u_long *, int *, u_int);
static int ip6_getpmtu_ctl(u_int, const struct in6_addr *, u_long *);
static int copypktopts(struct ip6_pktopts *, struct ip6_pktopts *, int);

/*
 * Make an extension header from option data.  hp is the source,
 * mp is the destination, and _ol is the optlen.
 */
#define	MAKE_EXTHDR(hp, mp, _ol)					\
    do {								\
	if (hp) {							\
		struct ip6_ext *eh = (struct ip6_ext *)(hp);		\
		error = ip6_copyexthdr((mp), (caddr_t)(hp),		\
		    ((eh)->ip6e_len + 1) << 3);				\
		if (error)						\
			goto freehdrs;					\
		(_ol) += (*(mp))->m_len;				\
	}								\
    } while (/*CONSTCOND*/ 0)

/*
 * Form a chain of extension headers.
 * m is the extension header mbuf
 * mp is the previous mbuf in the chain
 * p is the next header
 * i is the type of option.
 */
#define MAKE_CHAIN(m, mp, p, i)\
    do {\
	if (m) {\
		if (!hdrsplit) \
			panic("%s:%d: assumption failed: "\
			    "hdr not split: hdrsplit %d exthdrs %p",\
			    __func__, __LINE__, hdrsplit, &exthdrs);\
		*mtod((m), u_char *) = *(p);\
		*(p) = (i);\
		p = mtod((m), u_char *);\
		(m)->m_next = (mp)->m_next;\
		(mp)->m_next = (m);\
		(mp) = (m);\
	}\
    } while (/*CONSTCOND*/ 0)

void
in6_delayed_cksum(struct mbuf *m, uint32_t plen, u_short offset)
{
	u_short csum;

	csum = in_cksum_skip(m, offset + plen, offset);
	if (m->m_pkthdr.csum_flags & CSUM_UDP_IPV6 && csum == 0)
		csum = 0xffff;
	offset += m->m_pkthdr.csum_data;	/* checksum offset */

	if (offset + sizeof(csum) > m->m_len)
		m_copyback(m, offset, sizeof(csum), (caddr_t)&csum);
	else
		*(u_short *)mtodo(m, offset) = csum;
}

static void
ip6_output_delayed_csum(struct mbuf *m, struct ifnet *ifp, int csum_flags,
    int plen, int optlen)
{

	KASSERT((plen >= optlen), ("%s:%d: plen %d < optlen %d, m %p, ifp %p "
	    "csum_flags %#x",
	    __func__, __LINE__, plen, optlen, m, ifp, csum_flags));

	if (csum_flags & CSUM_DELAY_DATA_IPV6) {
		in6_delayed_cksum(m, plen - optlen,
		    sizeof(struct ip6_hdr) + optlen);
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA_IPV6;
	}
#if defined(SCTP) || defined(SCTP_SUPPORT)
	if (csum_flags & CSUM_SCTP_IPV6) {
		sctp_delayed_cksum(m, sizeof(struct ip6_hdr) + optlen);
		m->m_pkthdr.csum_flags &= ~CSUM_SCTP_IPV6;
	}
#endif
}

int
ip6_fragment(struct ifnet *ifp, struct mbuf *m0, int hlen, u_char nextproto,
    int fraglen , uint32_t id)
{
	struct mbuf *m, **mnext, *m_frgpart;
	struct ip6_hdr *ip6, *mhip6;
	struct ip6_frag *ip6f;
	int off;
	int error;
	int tlen = m0->m_pkthdr.len;

	KASSERT((fraglen % 8 == 0), ("Fragment length must be a multiple of 8"));

	m = m0;
	ip6 = mtod(m, struct ip6_hdr *);
	mnext = &m->m_nextpkt;

	for (off = hlen; off < tlen; off += fraglen) {
		m = m_gethdr(M_NOWAIT, MT_DATA);
		if (!m) {
			IP6STAT_INC(ip6s_odropped);
			return (ENOBUFS);
		}

		/*
		 * Make sure the complete packet header gets copied
		 * from the originating mbuf to the newly created
		 * mbuf. This also ensures that existing firewall
		 * classification(s), VLAN tags and so on get copied
		 * to the resulting fragmented packet(s):
		 */
		if (m_dup_pkthdr(m, m0, M_NOWAIT) == 0) {
			m_free(m);
			IP6STAT_INC(ip6s_odropped);
			return (ENOBUFS);
		}

		*mnext = m;
		mnext = &m->m_nextpkt;
		m->m_data += max_linkhdr;
		mhip6 = mtod(m, struct ip6_hdr *);
		*mhip6 = *ip6;
		m->m_len = sizeof(*mhip6);
		error = ip6_insertfraghdr(m0, m, hlen, &ip6f);
		if (error) {
			IP6STAT_INC(ip6s_odropped);
			return (error);
		}
		ip6f->ip6f_offlg = htons((u_short)((off - hlen) & ~7));
		if (off + fraglen >= tlen)
			fraglen = tlen - off;
		else
			ip6f->ip6f_offlg |= IP6F_MORE_FRAG;
		mhip6->ip6_plen = htons((u_short)(fraglen + hlen +
		    sizeof(*ip6f) - sizeof(struct ip6_hdr)));
		if ((m_frgpart = m_copym(m0, off, fraglen, M_NOWAIT)) == NULL) {
			IP6STAT_INC(ip6s_odropped);
			return (ENOBUFS);
		}
		m_cat(m, m_frgpart);
		m->m_pkthdr.len = fraglen + hlen + sizeof(*ip6f);
		ip6f->ip6f_reserved = 0;
		ip6f->ip6f_ident = id;
		ip6f->ip6f_nxt = nextproto;
		IP6STAT_INC(ip6s_ofragments);
		in6_ifstat_inc(ifp, ifs6_out_fragcreat);
	}

	return (0);
}

static int
ip6_output_send(struct inpcb *inp, struct ifnet *ifp, struct ifnet *origifp,
    struct mbuf *m, struct sockaddr_in6 *dst, struct route_in6 *ro,
    bool stamp_tag)
{
#ifdef KERN_TLS
	struct ktls_session *tls = NULL;
#endif
	struct m_snd_tag *mst;
	int error;

	MPASS((m->m_pkthdr.csum_flags & CSUM_SND_TAG) == 0);
	mst = NULL;

#ifdef KERN_TLS
	/*
	 * If this is an unencrypted TLS record, save a reference to
	 * the record.  This local reference is used to call
	 * ktls_output_eagain after the mbuf has been freed (thus
	 * dropping the mbuf's reference) in if_output.
	 */
	if (m->m_next != NULL && mbuf_has_tls_session(m->m_next)) {
		tls = ktls_hold(m->m_next->m_epg_tls);
		mst = tls->snd_tag;

		/*
		 * If a TLS session doesn't have a valid tag, it must
		 * have had an earlier ifp mismatch, so drop this
		 * packet.
		 */
		if (mst == NULL) {
			m_freem(m);
			error = EAGAIN;
			goto done;
		}
		/*
		 * Always stamp tags that include NIC ktls.
		 */
		stamp_tag = true;
	}
#endif
#ifdef RATELIMIT
	if (inp != NULL && mst == NULL) {
		if ((inp->inp_flags2 & INP_RATE_LIMIT_CHANGED) != 0 ||
		    (inp->inp_snd_tag != NULL &&
		    inp->inp_snd_tag->ifp != ifp))
			in_pcboutput_txrtlmt(inp, ifp, m);

		if (inp->inp_snd_tag != NULL)
			mst = inp->inp_snd_tag;
	}
#endif
	if (stamp_tag && mst != NULL) {
		KASSERT(m->m_pkthdr.rcvif == NULL,
		    ("trying to add a send tag to a forwarded packet"));
		if (mst->ifp != ifp) {
			m_freem(m);
			error = EAGAIN;
			goto done;
		}

		/* stamp send tag on mbuf */
		m->m_pkthdr.snd_tag = m_snd_tag_ref(mst);
		m->m_pkthdr.csum_flags |= CSUM_SND_TAG;
	}

	error = nd6_output_ifp(ifp, origifp, m, dst, (struct route *)ro);

done:
	/* Check for route change invalidating send tags. */
#ifdef KERN_TLS
	if (tls != NULL) {
		if (error == EAGAIN)
			error = ktls_output_eagain(inp, tls);
		ktls_free(tls);
	}
#endif
#ifdef RATELIMIT
	if (error == EAGAIN)
		in_pcboutput_eagain(inp);
#endif
	return (error);
}

/*
 * IP6 output.
 * The packet in mbuf chain m contains a skeletal IP6 header (with pri, len,
 * nxt, hlim, src, dst).
 * This function may modify ver and hlim only.
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 * If route_in6 ro is present and has ro_nh initialized, route lookup would be
 * skipped and ro->ro_nh would be used. If ro is present but ro->ro_nh is NULL,
 * then result of route lookup is stored in ro->ro_nh.
 *
 * Type of "mtu": rt_mtu is u_long, ifnet.ifr_mtu is int, and nd_ifinfo.linkmtu
 * is uint32_t.  So we use u_long to hold largest one, which is rt_mtu.
 *
 * ifpp - XXX: just for statistics
 */
int
ip6_output(struct mbuf *m0, struct ip6_pktopts *opt,
    struct route_in6 *ro, int flags, struct ip6_moptions *im6o,
    struct ifnet **ifpp, struct inpcb *inp)
{
	struct ip6_hdr *ip6;
	struct ifnet *ifp, *origifp;
	struct mbuf *m = m0;
	struct mbuf *mprev;
	struct route_in6 *ro_pmtu;
	struct nhop_object *nh;
	struct sockaddr_in6 *dst, sin6, src_sa, dst_sa;
	struct in6_addr odst;
	u_char *nexthdrp;
	int tlen, len;
	int error = 0;
	int vlan_pcp = -1;
	struct in6_ifaddr *ia = NULL;
	u_long mtu;
	int alwaysfrag, dontfrag;
	u_int32_t optlen, plen = 0, unfragpartlen;
	struct ip6_exthdrs exthdrs;
	struct in6_addr src0, dst0;
	u_int32_t zone;
	bool hdrsplit;
	int sw_csum, tso;
	int needfiblookup;
	uint32_t fibnum;
	struct m_tag *fwd_tag = NULL;
	uint32_t id;

	NET_EPOCH_ASSERT();

	if (inp != NULL) {
		INP_LOCK_ASSERT(inp);
		M_SETFIB(m, inp->inp_inc.inc_fibnum);
		if ((flags & IP_NODEFAULTFLOWID) == 0) {
			/* Unconditionally set flowid. */
			m->m_pkthdr.flowid = inp->inp_flowid;
			M_HASHTYPE_SET(m, inp->inp_flowtype);
		}
		if ((inp->inp_flags2 & INP_2PCP_SET) != 0)
			vlan_pcp = (inp->inp_flags2 & INP_2PCP_MASK) >>
			    INP_2PCP_SHIFT;
#ifdef NUMA
		m->m_pkthdr.numa_domain = inp->inp_numa_domain;
#endif
	}

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	/*
	 * IPSec checking which handles several cases.
	 * FAST IPSEC: We re-injected the packet.
	 * XXX: need scope argument.
	 */
	if (IPSEC_ENABLED(ipv6)) {
		if ((error = IPSEC_OUTPUT(ipv6, m, inp)) != 0) {
			if (error == EINPROGRESS)
				error = 0;
			goto done;
		}
	}
#endif /* IPSEC */

	/* Source address validation. */
	ip6 = mtod(m, struct ip6_hdr *);
	if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src) &&
	    (flags & IPV6_UNSPECSRC) == 0) {
		error = EOPNOTSUPP;
		IP6STAT_INC(ip6s_badscope);
		goto bad;
	}
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_src)) {
		error = EOPNOTSUPP;
		IP6STAT_INC(ip6s_badscope);
		goto bad;
	}

	/*
	 * If we are given packet options to add extension headers prepare them.
	 * Calculate the total length of the extension header chain.
	 * Keep the length of the unfragmentable part for fragmentation.
	 */
	bzero(&exthdrs, sizeof(exthdrs));
	optlen = 0;
	unfragpartlen = sizeof(struct ip6_hdr);
	if (opt) {
		/* Hop-by-Hop options header. */
		MAKE_EXTHDR(opt->ip6po_hbh, &exthdrs.ip6e_hbh, optlen);

		/* Destination options header (1st part). */
		if (opt->ip6po_rthdr) {
#ifndef RTHDR_SUPPORT_IMPLEMENTED
			/*
			 * If there is a routing header, discard the packet
			 * right away here. RH0/1 are obsolete and we do not
			 * currently support RH2/3/4.
			 * People trying to use RH253/254 may want to disable
			 * this check.
			 * The moment we do support any routing header (again)
			 * this block should check the routing type more
			 * selectively.
			 */
			error = EINVAL;
			goto bad;
#endif

			/*
			 * Destination options header (1st part).
			 * This only makes sense with a routing header.
			 * See Section 9.2 of RFC 3542.
			 * Disabling this part just for MIP6 convenience is
			 * a bad idea.  We need to think carefully about a
			 * way to make the advanced API coexist with MIP6
			 * options, which might automatically be inserted in
			 * the kernel.
			 */
			MAKE_EXTHDR(opt->ip6po_dest1, &exthdrs.ip6e_dest1,
			    optlen);
		}
		/* Routing header. */
		MAKE_EXTHDR(opt->ip6po_rthdr, &exthdrs.ip6e_rthdr, optlen);

		unfragpartlen += optlen;

		/*
		 * NOTE: we don't add AH/ESP length here (done in
		 * ip6_ipsec_output()).
		 */

		/* Destination options header (2nd part). */
		MAKE_EXTHDR(opt->ip6po_dest2, &exthdrs.ip6e_dest2, optlen);
	}

	/*
	 * If there is at least one extension header,
	 * separate IP6 header from the payload.
	 */
	hdrsplit = false;
	if (optlen) {
		if ((error = ip6_splithdr(m, &exthdrs)) != 0) {
			m = NULL;
			goto freehdrs;
		}
		m = exthdrs.ip6e_ip6;
		ip6 = mtod(m, struct ip6_hdr *);
		hdrsplit = true;
	}

	/* Adjust mbuf packet header length. */
	m->m_pkthdr.len += optlen;
	plen = m->m_pkthdr.len - sizeof(*ip6);

	/* If this is a jumbo payload, insert a jumbo payload option. */
	if (plen > IPV6_MAXPACKET) {
		if (!hdrsplit) {
			if ((error = ip6_splithdr(m, &exthdrs)) != 0) {
				m = NULL;
				goto freehdrs;
			}
			m = exthdrs.ip6e_ip6;
			ip6 = mtod(m, struct ip6_hdr *);
			hdrsplit = true;
		}
		if ((error = ip6_insert_jumboopt(&exthdrs, plen)) != 0)
			goto freehdrs;
		ip6->ip6_plen = 0;
	} else
		ip6->ip6_plen = htons(plen);
	nexthdrp = &ip6->ip6_nxt;

	if (optlen) {
		/*
		 * Concatenate headers and fill in next header fields.
		 * Here we have, on "m"
		 *	IPv6 payload
		 * and we insert headers accordingly.
		 * Finally, we should be getting:
		 *	IPv6 hbh dest1 rthdr ah* [esp* dest2 payload].
		 *
		 * During the header composing process "m" points to IPv6
		 * header.  "mprev" points to an extension header prior to esp.
		 */
		mprev = m;

		/*
		 * We treat dest2 specially.  This makes IPsec processing
		 * much easier.  The goal here is to make mprev point the
		 * mbuf prior to dest2.
		 *
		 * Result: IPv6 dest2 payload.
		 * m and mprev will point to IPv6 header.
		 */
		if (exthdrs.ip6e_dest2) {
			if (!hdrsplit)
				panic("%s:%d: assumption failed: "
				    "hdr not split: hdrsplit %d exthdrs %p",
				    __func__, __LINE__, hdrsplit, &exthdrs);
			exthdrs.ip6e_dest2->m_next = m->m_next;
			m->m_next = exthdrs.ip6e_dest2;
			*mtod(exthdrs.ip6e_dest2, u_char *) = ip6->ip6_nxt;
			ip6->ip6_nxt = IPPROTO_DSTOPTS;
		}

		/*
		 * Result: IPv6 hbh dest1 rthdr dest2 payload.
		 * m will point to IPv6 header.  mprev will point to the
		 * extension header prior to dest2 (rthdr in the above case).
		 */
		MAKE_CHAIN(exthdrs.ip6e_hbh, mprev, nexthdrp, IPPROTO_HOPOPTS);
		MAKE_CHAIN(exthdrs.ip6e_dest1, mprev, nexthdrp,
			   IPPROTO_DSTOPTS);
		MAKE_CHAIN(exthdrs.ip6e_rthdr, mprev, nexthdrp,
			   IPPROTO_ROUTING);
	}

	IP6STAT_INC(ip6s_localout);

	/* Route packet. */
	ro_pmtu = ro;
	if (opt && opt->ip6po_rthdr)
		ro = &opt->ip6po_route;
	if (ro != NULL)
		dst = (struct sockaddr_in6 *)&ro->ro_dst;
	else
		dst = &sin6;
	fibnum = (inp != NULL) ? inp->inp_inc.inc_fibnum : M_GETFIB(m);

again:
	/*
	 * If specified, try to fill in the traffic class field.
	 * Do not override if a non-zero value is already set.
	 * We check the diffserv field and the ECN field separately.
	 */
	if (opt && opt->ip6po_tclass >= 0) {
		int mask = 0;

		if (IPV6_DSCP(ip6) == 0)
			mask |= 0xfc;
		if (IPV6_ECN(ip6) == 0)
			mask |= 0x03;
		if (mask != 0)
			ip6->ip6_flow |= htonl((opt->ip6po_tclass & mask) << 20);
	}

	/* Fill in or override the hop limit field, if necessary. */
	if (opt && opt->ip6po_hlim != -1)
		ip6->ip6_hlim = opt->ip6po_hlim & 0xff;
	else if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		if (im6o != NULL)
			ip6->ip6_hlim = im6o->im6o_multicast_hlim;
		else
			ip6->ip6_hlim = V_ip6_defmcasthlim;
	}

	if (ro == NULL || ro->ro_nh == NULL) {
		bzero(dst, sizeof(*dst));
		dst->sin6_family = AF_INET6;
		dst->sin6_len = sizeof(*dst);
		dst->sin6_addr = ip6->ip6_dst;
	} 
	/*
	 * Validate route against routing table changes.
	 * Make sure that the address family is set in route.
	 */
	nh = NULL;
	ifp = NULL;
	mtu = 0;
	if (ro != NULL) {
		if (ro->ro_nh != NULL && inp != NULL) {
			ro->ro_dst.sin6_family = AF_INET6; /* XXX KASSERT? */
			NH_VALIDATE((struct route *)ro, &inp->inp_rt_cookie,
			    fibnum);
		}
		if (ro->ro_nh != NULL && fwd_tag == NULL &&
		    (!NH_IS_VALID(ro->ro_nh) ||
		    ro->ro_dst.sin6_family != AF_INET6 ||
		    !IN6_ARE_ADDR_EQUAL(&ro->ro_dst.sin6_addr, &ip6->ip6_dst)))
			RO_INVALIDATE_CACHE(ro);

		if (ro->ro_nh != NULL && fwd_tag == NULL &&
		    ro->ro_dst.sin6_family == AF_INET6 &&
		    IN6_ARE_ADDR_EQUAL(&ro->ro_dst.sin6_addr, &ip6->ip6_dst)) {
			/* Nexthop is valid and contains valid ifp */
			nh = ro->ro_nh;
		} else {
			if (ro->ro_lle)
				LLE_FREE(ro->ro_lle);	/* zeros ro_lle */
			ro->ro_lle = NULL;
			if (fwd_tag == NULL) {
				bzero(&dst_sa, sizeof(dst_sa));
				dst_sa.sin6_family = AF_INET6;
				dst_sa.sin6_len = sizeof(dst_sa);
				dst_sa.sin6_addr = ip6->ip6_dst;
			}
			error = in6_selectroute(&dst_sa, opt, im6o, ro, &ifp,
			    &nh, fibnum, m->m_pkthdr.flowid);
			if (error != 0) {
				IP6STAT_INC(ip6s_noroute);
				if (ifp != NULL)
					in6_ifstat_inc(ifp, ifs6_out_discard);
				goto bad;
			}
			/*
			 * At this point at least @ifp is not NULL
			 * Can be the case when dst is multicast, link-local or
			 * interface is explicitly specificed by the caller.
			 */
		}
		if (nh == NULL) {
			/*
			 * If in6_selectroute() does not return a nexthop
			 * dst may not have been updated.
			 */
			*dst = dst_sa;	/* XXX */
			origifp = ifp;
			mtu = ifp->if_mtu;
		} else {
			ifp = nh->nh_ifp;
			origifp = nh->nh_aifp;
			ia = (struct in6_ifaddr *)(nh->nh_ifa);
			counter_u64_add(nh->nh_pksent, 1);
		}
	} else {
		struct nhop_object *nh;
		struct in6_addr kdst;
		uint32_t scopeid;

		if (fwd_tag == NULL) {
			bzero(&dst_sa, sizeof(dst_sa));
			dst_sa.sin6_family = AF_INET6;
			dst_sa.sin6_len = sizeof(dst_sa);
			dst_sa.sin6_addr = ip6->ip6_dst;
		}

		if (IN6_IS_ADDR_MULTICAST(&dst_sa.sin6_addr) &&
		    im6o != NULL &&
		    (ifp = im6o->im6o_multicast_ifp) != NULL) {
			/* We do not need a route lookup. */
			*dst = dst_sa;	/* XXX */
			origifp = ifp;
			goto nonh6lookup;
		}

		in6_splitscope(&dst_sa.sin6_addr, &kdst, &scopeid);

		if (IN6_IS_ADDR_MC_LINKLOCAL(&dst_sa.sin6_addr) ||
		    IN6_IS_ADDR_MC_NODELOCAL(&dst_sa.sin6_addr)) {
			if (scopeid > 0) {
				ifp = in6_getlinkifnet(scopeid);
				if (ifp == NULL) {
					error = EHOSTUNREACH;
					goto bad;
				}
				*dst = dst_sa;	/* XXX */
				origifp = ifp;
				goto nonh6lookup;
			}
		}

		nh = fib6_lookup(fibnum, &kdst, scopeid, NHR_NONE,
		    m->m_pkthdr.flowid);
		if (nh == NULL) {
			IP6STAT_INC(ip6s_noroute);
			/* No ifp in6_ifstat_inc(ifp, ifs6_out_discard); */
			error = EHOSTUNREACH;;
			goto bad;
		}

		ifp = nh->nh_ifp;
		origifp = nh->nh_aifp;
		ia = ifatoia6(nh->nh_ifa);
		if (nh->nh_flags & NHF_GATEWAY)
			dst->sin6_addr = nh->gw6_sa.sin6_addr;
		else if (fwd_tag != NULL)
			dst->sin6_addr = dst_sa.sin6_addr;
nonh6lookup:
		;
	}
	/*
	 * At this point ifp MUST be pointing to the valid transmit ifp.
	 * origifp MUST be valid and pointing to either the same ifp or,
	 * in case of loopback output, to the interface which ip6_src
	 * belongs to.
	 * Examples:
	 *  fe80::1%em0 -> fe80::2%em0 -> ifp=em0, origifp=em0
	 *  fe80::1%em0 -> fe80::1%em0 -> ifp=lo0, origifp=em0
	 *  ::1 -> ::1 -> ifp=lo0, origifp=lo0
	 *
	 * mtu can be 0 and will be refined later.
	 */
	KASSERT((ifp != NULL), ("output interface must not be NULL"));
	KASSERT((origifp != NULL), ("output address interface must not be NULL"));

	if ((flags & IPV6_FORWARDING) == 0) {
		/* XXX: the FORWARDING flag can be set for mrouting. */
		in6_ifstat_inc(ifp, ifs6_out_request);
	}

	/* Setup data structures for scope ID checks. */
	src0 = ip6->ip6_src;
	bzero(&src_sa, sizeof(src_sa));
	src_sa.sin6_family = AF_INET6;
	src_sa.sin6_len = sizeof(src_sa);
	src_sa.sin6_addr = ip6->ip6_src;

	dst0 = ip6->ip6_dst;
	/* Re-initialize to be sure. */
	bzero(&dst_sa, sizeof(dst_sa));
	dst_sa.sin6_family = AF_INET6;
	dst_sa.sin6_len = sizeof(dst_sa);
	dst_sa.sin6_addr = ip6->ip6_dst;

	/* Check for valid scope ID. */
	if (in6_setscope(&src0, origifp, &zone) == 0 &&
	    sa6_recoverscope(&src_sa) == 0 && zone == src_sa.sin6_scope_id &&
	    in6_setscope(&dst0, origifp, &zone) == 0 &&
	    sa6_recoverscope(&dst_sa) == 0 && zone == dst_sa.sin6_scope_id) {
		/*
		 * The outgoing interface is in the zone of the source
		 * and destination addresses.
		 *
		 */
	} else if ((origifp->if_flags & IFF_LOOPBACK) == 0 ||
	    sa6_recoverscope(&src_sa) != 0 ||
	    sa6_recoverscope(&dst_sa) != 0 ||
	    dst_sa.sin6_scope_id == 0 ||
	    (src_sa.sin6_scope_id != 0 &&
	    src_sa.sin6_scope_id != dst_sa.sin6_scope_id) ||
	    ifnet_byindex(dst_sa.sin6_scope_id) == NULL) {
		/*
		 * If the destination network interface is not a
		 * loopback interface, or the destination network
		 * address has no scope ID, or the source address has
		 * a scope ID set which is different from the
		 * destination address one, or there is no network
		 * interface representing this scope ID, the address
		 * pair is considered invalid.
		 */
		IP6STAT_INC(ip6s_badscope);
		in6_ifstat_inc(origifp, ifs6_out_discard);
		if (error == 0)
			error = EHOSTUNREACH; /* XXX */
		goto bad;
	}
	/* All scope ID checks are successful. */

	if (nh && !IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		if (opt && opt->ip6po_nextroute.ro_nh) {
			/*
			 * The nexthop is explicitly specified by the
			 * application.  We assume the next hop is an IPv6
			 * address.
			 */
			dst = (struct sockaddr_in6 *)opt->ip6po_nexthop;
		}
		else if ((nh->nh_flags & NHF_GATEWAY))
			dst = &nh->gw6_sa;
	}

	if (!IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		m->m_flags &= ~(M_BCAST | M_MCAST); /* Just in case. */
	} else {
		m->m_flags = (m->m_flags & ~M_BCAST) | M_MCAST;
		in6_ifstat_inc(ifp, ifs6_out_mcast);

		/* Confirm that the outgoing interface supports multicast. */
		if (!(ifp->if_flags & IFF_MULTICAST)) {
			IP6STAT_INC(ip6s_noroute);
			in6_ifstat_inc(ifp, ifs6_out_discard);
			error = ENETUNREACH;
			goto bad;
		}
		if ((im6o == NULL && in6_mcast_loop) ||
		    (im6o && im6o->im6o_multicast_loop)) {
			/*
			 * Loop back multicast datagram if not expressly
			 * forbidden to do so, even if we have not joined
			 * the address; protocols will filter it later,
			 * thus deferring a hash lookup and lock acquisition
			 * at the expense of an m_copym().
			 */
			ip6_mloopback(ifp, m);
		} else {
			/*
			 * If we are acting as a multicast router, perform
			 * multicast forwarding as if the packet had just
			 * arrived on the interface to which we are about
			 * to send.  The multicast forwarding function
			 * recursively calls this function, using the
			 * IPV6_FORWARDING flag to prevent infinite recursion.
			 *
			 * Multicasts that are looped back by ip6_mloopback(),
			 * above, will be forwarded by the ip6_input() routine,
			 * if necessary.
			 */
			if (V_ip6_mrouter && (flags & IPV6_FORWARDING) == 0) {
				/*
				 * XXX: ip6_mforward expects that rcvif is NULL
				 * when it is called from the originating path.
				 * However, it may not always be the case.
				 */
				m->m_pkthdr.rcvif = NULL;
				if (ip6_mforward(ip6, ifp, m) != 0) {
					m_freem(m);
					goto done;
				}
			}
		}
		/*
		 * Multicasts with a hoplimit of zero may be looped back,
		 * above, but must not be transmitted on a network.
		 * Also, multicasts addressed to the loopback interface
		 * are not sent -- the above call to ip6_mloopback() will
		 * loop back a copy if this host actually belongs to the
		 * destination group on the loopback interface.
		 */
		if (ip6->ip6_hlim == 0 || (ifp->if_flags & IFF_LOOPBACK) ||
		    IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst)) {
			m_freem(m);
			goto done;
		}
	}

	/*
	 * Fill the outgoing inteface to tell the upper layer
	 * to increment per-interface statistics.
	 */
	if (ifpp)
		*ifpp = ifp;

	/* Determine path MTU. */
	if ((error = ip6_getpmtu(ro_pmtu, ro != ro_pmtu, ifp, &ip6->ip6_dst,
		    &mtu, &alwaysfrag, fibnum, *nexthdrp)) != 0)
		goto bad;
	KASSERT(mtu > 0, ("%s:%d: mtu %ld, ro_pmtu %p ro %p ifp %p "
	    "alwaysfrag %d fibnum %u\n", __func__, __LINE__, mtu, ro_pmtu, ro,
	    ifp, alwaysfrag, fibnum));

	/*
	 * The caller of this function may specify to use the minimum MTU
	 * in some cases.
	 * An advanced API option (IPV6_USE_MIN_MTU) can also override MTU
	 * setting.  The logic is a bit complicated; by default, unicast
	 * packets will follow path MTU while multicast packets will be sent at
	 * the minimum MTU.  If IP6PO_MINMTU_ALL is specified, all packets
	 * including unicast ones will be sent at the minimum MTU.  Multicast
	 * packets will always be sent at the minimum MTU unless
	 * IP6PO_MINMTU_DISABLE is explicitly specified.
	 * See RFC 3542 for more details.
	 */
	if (mtu > IPV6_MMTU) {
		if ((flags & IPV6_MINMTU))
			mtu = IPV6_MMTU;
		else if (opt && opt->ip6po_minmtu == IP6PO_MINMTU_ALL)
			mtu = IPV6_MMTU;
		else if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) &&
			 (opt == NULL ||
			  opt->ip6po_minmtu != IP6PO_MINMTU_DISABLE)) {
			mtu = IPV6_MMTU;
		}
	}

	/*
	 * Clear embedded scope identifiers if necessary.
	 * in6_clearscope() will touch the addresses only when necessary.
	 */
	in6_clearscope(&ip6->ip6_src);
	in6_clearscope(&ip6->ip6_dst);

	/*
	 * If the outgoing packet contains a hop-by-hop options header,
	 * it must be examined and processed even by the source node.
	 * (RFC 2460, section 4.)
	 */
	if (exthdrs.ip6e_hbh) {
		struct ip6_hbh *hbh = mtod(exthdrs.ip6e_hbh, struct ip6_hbh *);
		u_int32_t dummy; /* XXX unused */
		u_int32_t plen = 0; /* XXX: ip6_process will check the value */

#ifdef DIAGNOSTIC
		if ((hbh->ip6h_len + 1) << 3 > exthdrs.ip6e_hbh->m_len)
			panic("ip6e_hbh is not contiguous");
#endif
		/*
		 *  XXX: if we have to send an ICMPv6 error to the sender,
		 *       we need the M_LOOP flag since icmp6_error() expects
		 *       the IPv6 and the hop-by-hop options header are
		 *       contiguous unless the flag is set.
		 */
		m->m_flags |= M_LOOP;
		m->m_pkthdr.rcvif = ifp;
		if (ip6_process_hopopts(m, (u_int8_t *)(hbh + 1),
		    ((hbh->ip6h_len + 1) << 3) - sizeof(struct ip6_hbh),
		    &dummy, &plen) < 0) {
			/* m was already freed at this point. */
			error = EINVAL;/* better error? */
			goto done;
		}
		m->m_flags &= ~M_LOOP; /* XXX */
		m->m_pkthdr.rcvif = NULL;
	}

	/* Jump over all PFIL processing if hooks are not active. */
	if (!PFIL_HOOKED_OUT(V_inet6_pfil_head))
		goto passout;

	odst = ip6->ip6_dst;
	/* Run through list of hooks for output packets. */
	switch (pfil_mbuf_out(V_inet6_pfil_head, &m, ifp, inp)) {
	case PFIL_PASS:
		ip6 = mtod(m, struct ip6_hdr *);
		break;
	case PFIL_DROPPED:
		error = EACCES;
		/* FALLTHROUGH */
	case PFIL_CONSUMED:
		goto done;
	}

	needfiblookup = 0;
	/* See if destination IP address was changed by packet filter. */
	if (!IN6_ARE_ADDR_EQUAL(&odst, &ip6->ip6_dst)) {
		m->m_flags |= M_SKIP_FIREWALL;
		/* If destination is now ourself drop to ip6_input(). */
		if (in6_localip(&ip6->ip6_dst)) {
			m->m_flags |= M_FASTFWD_OURS;
			if (m->m_pkthdr.rcvif == NULL)
				m->m_pkthdr.rcvif = V_loif;
			if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA_IPV6) {
				m->m_pkthdr.csum_flags |=
				    CSUM_DATA_VALID_IPV6 | CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			}
#if defined(SCTP) || defined(SCTP_SUPPORT)
			if (m->m_pkthdr.csum_flags & CSUM_SCTP_IPV6)
				m->m_pkthdr.csum_flags |= CSUM_SCTP_VALID;
#endif
			error = netisr_queue(NETISR_IPV6, m);
			goto done;
		} else {
			if (ro != NULL)
				RO_INVALIDATE_CACHE(ro);
			needfiblookup = 1; /* Redo the routing table lookup. */
		}
	}
	/* See if fib was changed by packet filter. */
	if (fibnum != M_GETFIB(m)) {
		m->m_flags |= M_SKIP_FIREWALL;
		fibnum = M_GETFIB(m);
		if (ro != NULL)
			RO_INVALIDATE_CACHE(ro);
		needfiblookup = 1;
	}
	if (needfiblookup)
		goto again;

	/* See if local, if yes, send it to netisr. */
	if (m->m_flags & M_FASTFWD_OURS) {
		if (m->m_pkthdr.rcvif == NULL)
			m->m_pkthdr.rcvif = V_loif;
		if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA_IPV6) {
			m->m_pkthdr.csum_flags |=
			    CSUM_DATA_VALID_IPV6 | CSUM_PSEUDO_HDR;
			m->m_pkthdr.csum_data = 0xffff;
		}
#if defined(SCTP) || defined(SCTP_SUPPORT)
		if (m->m_pkthdr.csum_flags & CSUM_SCTP_IPV6)
			m->m_pkthdr.csum_flags |= CSUM_SCTP_VALID;
#endif
		error = netisr_queue(NETISR_IPV6, m);
		goto done;
	}
	/* Or forward to some other address? */
	if ((m->m_flags & M_IP6_NEXTHOP) &&
	    (fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL)) != NULL) {
		if (ro != NULL)
			dst = (struct sockaddr_in6 *)&ro->ro_dst;
		else
			dst = &sin6;
		bcopy((fwd_tag+1), &dst_sa, sizeof(struct sockaddr_in6));
		m->m_flags |= M_SKIP_FIREWALL;
		m->m_flags &= ~M_IP6_NEXTHOP;
		m_tag_delete(m, fwd_tag);
		goto again;
	}

passout:
	if (vlan_pcp > -1)
		EVL_APPLY_PRI(m, vlan_pcp);

	/* Ensure the packet data is mapped if the interface requires it. */
	if ((ifp->if_capenable & IFCAP_MEXTPG) == 0) {
		m = mb_unmapped_to_ext(m);
		if (m == NULL) {
			IP6STAT_INC(ip6s_odropped);
			return (ENOBUFS);
		}
	}

	/*
	 * Send the packet to the outgoing interface.
	 * If necessary, do IPv6 fragmentation before sending.
	 *
	 * The logic here is rather complex:
	 * 1: normal case (dontfrag == 0, alwaysfrag == 0)
	 * 1-a:	send as is if tlen <= path mtu
	 * 1-b:	fragment if tlen > path mtu
	 *
	 * 2: if user asks us not to fragment (dontfrag == 1)
	 * 2-a:	send as is if tlen <= interface mtu
	 * 2-b:	error if tlen > interface mtu
	 *
	 * 3: if we always need to attach fragment header (alwaysfrag == 1)
	 *	always fragment
	 *
	 * 4: if dontfrag == 1 && alwaysfrag == 1
	 *	error, as we cannot handle this conflicting request.
	 */
	sw_csum = m->m_pkthdr.csum_flags;
	if (!hdrsplit) {
		tso = ((sw_csum & ifp->if_hwassist &
		    (CSUM_TSO | CSUM_INNER_TSO)) != 0) ? 1 : 0;
		sw_csum &= ~ifp->if_hwassist;
	} else
		tso = 0;
	/*
	 * If we added extension headers, we will not do TSO and calculate the
	 * checksums ourselves for now.
	 * XXX-BZ  Need a framework to know when the NIC can handle it, even
	 * with ext. hdrs.
	 */
	ip6_output_delayed_csum(m, ifp, sw_csum, plen, optlen);
	/* XXX-BZ m->m_pkthdr.csum_flags &= ~ifp->if_hwassist; */
	tlen = m->m_pkthdr.len;

	if ((opt && (opt->ip6po_flags & IP6PO_DONTFRAG)) || tso)
		dontfrag = 1;
	else
		dontfrag = 0;
	if (dontfrag && alwaysfrag) {	/* Case 4. */
		/* Conflicting request - can't transmit. */
		error = EMSGSIZE;
		goto bad;
	}
	if (dontfrag && tlen > IN6_LINKMTU(ifp) && !tso) {	/* Case 2-b. */
		/*
		 * Even if the DONTFRAG option is specified, we cannot send the
		 * packet when the data length is larger than the MTU of the
		 * outgoing interface.
		 * Notify the error by sending IPV6_PATHMTU ancillary data if
		 * application wanted to know the MTU value. Also return an
		 * error code (this is not described in the API spec).
		 */
		if (inp != NULL)
			ip6_notify_pmtu(inp, &dst_sa, (u_int32_t)mtu);
		error = EMSGSIZE;
		goto bad;
	}

	/* Transmit packet without fragmentation. */
	if (dontfrag || (!alwaysfrag && tlen <= mtu)) {	/* Cases 1-a and 2-a. */
		struct in6_ifaddr *ia6;

		ip6 = mtod(m, struct ip6_hdr *);
		ia6 = in6_ifawithifp(ifp, &ip6->ip6_src);
		if (ia6) {
			/* Record statistics for this interface address. */
			counter_u64_add(ia6->ia_ifa.ifa_opackets, 1);
			counter_u64_add(ia6->ia_ifa.ifa_obytes,
			    m->m_pkthdr.len);
		}
		error = ip6_output_send(inp, ifp, origifp, m, dst, ro,
		    (flags & IP_NO_SND_TAG_RL) ? false : true);
		goto done;
	}

	/* Try to fragment the packet.  Cases 1-b and 3. */
	if (mtu < IPV6_MMTU) {
		/* Path MTU cannot be less than IPV6_MMTU. */
		error = EMSGSIZE;
		in6_ifstat_inc(ifp, ifs6_out_fragfail);
		goto bad;
	} else if (ip6->ip6_plen == 0) {
		/* Jumbo payload cannot be fragmented. */
		error = EMSGSIZE;
		in6_ifstat_inc(ifp, ifs6_out_fragfail);
		goto bad;
	} else {
		u_char nextproto;

		/*
		 * Too large for the destination or interface;
		 * fragment if possible.
		 * Must be able to put at least 8 bytes per fragment.
		 */
		if (mtu > IPV6_MAXPACKET)
			mtu = IPV6_MAXPACKET;

		len = (mtu - unfragpartlen - sizeof(struct ip6_frag)) & ~7;
		if (len < 8) {
			error = EMSGSIZE;
			in6_ifstat_inc(ifp, ifs6_out_fragfail);
			goto bad;
		}

		/*
		 * If the interface will not calculate checksums on
		 * fragmented packets, then do it here.
		 * XXX-BZ handle the hw offloading case.  Need flags.
		 */
		ip6_output_delayed_csum(m, ifp, m->m_pkthdr.csum_flags, plen,
		    optlen);

		/*
		 * Change the next header field of the last header in the
		 * unfragmentable part.
		 */
		if (exthdrs.ip6e_rthdr) {
			nextproto = *mtod(exthdrs.ip6e_rthdr, u_char *);
			*mtod(exthdrs.ip6e_rthdr, u_char *) = IPPROTO_FRAGMENT;
		} else if (exthdrs.ip6e_dest1) {
			nextproto = *mtod(exthdrs.ip6e_dest1, u_char *);
			*mtod(exthdrs.ip6e_dest1, u_char *) = IPPROTO_FRAGMENT;
		} else if (exthdrs.ip6e_hbh) {
			nextproto = *mtod(exthdrs.ip6e_hbh, u_char *);
			*mtod(exthdrs.ip6e_hbh, u_char *) = IPPROTO_FRAGMENT;
		} else {
			ip6 = mtod(m, struct ip6_hdr *);
			nextproto = ip6->ip6_nxt;
			ip6->ip6_nxt = IPPROTO_FRAGMENT;
		}

		/*
		 * Loop through length of segment after first fragment,
		 * make new header and copy data of each part and link onto
		 * chain.
		 */
		m0 = m;
		id = htonl(ip6_randomid());
		error = ip6_fragment(ifp, m, unfragpartlen, nextproto,len, id);
		if (error != 0)
			goto sendorfree;

		in6_ifstat_inc(ifp, ifs6_out_fragok);
	}

	/* Remove leading garbage. */
sendorfree:
	m = m0->m_nextpkt;
	m0->m_nextpkt = 0;
	m_freem(m0);
	for (; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = 0;
		if (error == 0) {
			/* Record statistics for this interface address. */
			if (ia) {
				counter_u64_add(ia->ia_ifa.ifa_opackets, 1);
				counter_u64_add(ia->ia_ifa.ifa_obytes,
				    m->m_pkthdr.len);
			}
			if (vlan_pcp > -1)
				EVL_APPLY_PRI(m, vlan_pcp);
			error = ip6_output_send(inp, ifp, origifp, m, dst, ro,
			    true);
		} else
			m_freem(m);
	}

	if (error == 0)
		IP6STAT_INC(ip6s_fragmented);

done:
	return (error);

freehdrs:
	m_freem(exthdrs.ip6e_hbh);	/* m_freem() checks if mbuf is NULL. */
	m_freem(exthdrs.ip6e_dest1);
	m_freem(exthdrs.ip6e_rthdr);
	m_freem(exthdrs.ip6e_dest2);
	/* FALLTHROUGH */
bad:
	if (m)
		m_freem(m);
	goto done;
}

static int
ip6_copyexthdr(struct mbuf **mp, caddr_t hdr, int hlen)
{
	struct mbuf *m;

	if (hlen > MCLBYTES)
		return (ENOBUFS); /* XXX */

	if (hlen > MLEN)
		m = m_getcl(M_NOWAIT, MT_DATA, 0);
	else
		m = m_get(M_NOWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = hlen;
	if (hdr)
		bcopy(hdr, mtod(m, caddr_t), hlen);

	*mp = m;
	return (0);
}

/*
 * Insert jumbo payload option.
 */
static int
ip6_insert_jumboopt(struct ip6_exthdrs *exthdrs, u_int32_t plen)
{
	struct mbuf *mopt;
	u_char *optbuf;
	u_int32_t v;

#define JUMBOOPTLEN	8	/* length of jumbo payload option and padding */

	/*
	 * If there is no hop-by-hop options header, allocate new one.
	 * If there is one but it doesn't have enough space to store the
	 * jumbo payload option, allocate a cluster to store the whole options.
	 * Otherwise, use it to store the options.
	 */
	if (exthdrs->ip6e_hbh == NULL) {
		mopt = m_get(M_NOWAIT, MT_DATA);
		if (mopt == NULL)
			return (ENOBUFS);
		mopt->m_len = JUMBOOPTLEN;
		optbuf = mtod(mopt, u_char *);
		optbuf[1] = 0;	/* = ((JUMBOOPTLEN) >> 3) - 1 */
		exthdrs->ip6e_hbh = mopt;
	} else {
		struct ip6_hbh *hbh;

		mopt = exthdrs->ip6e_hbh;
		if (M_TRAILINGSPACE(mopt) < JUMBOOPTLEN) {
			/*
			 * XXX assumption:
			 * - exthdrs->ip6e_hbh is not referenced from places
			 *   other than exthdrs.
			 * - exthdrs->ip6e_hbh is not an mbuf chain.
			 */
			int oldoptlen = mopt->m_len;
			struct mbuf *n;

			/*
			 * XXX: give up if the whole (new) hbh header does
			 * not fit even in an mbuf cluster.
			 */
			if (oldoptlen + JUMBOOPTLEN > MCLBYTES)
				return (ENOBUFS);

			/*
			 * As a consequence, we must always prepare a cluster
			 * at this point.
			 */
			n = m_getcl(M_NOWAIT, MT_DATA, 0);
			if (n == NULL)
				return (ENOBUFS);
			n->m_len = oldoptlen + JUMBOOPTLEN;
			bcopy(mtod(mopt, caddr_t), mtod(n, caddr_t),
			    oldoptlen);
			optbuf = mtod(n, caddr_t) + oldoptlen;
			m_freem(mopt);
			mopt = exthdrs->ip6e_hbh = n;
		} else {
			optbuf = mtod(mopt, u_char *) + mopt->m_len;
			mopt->m_len += JUMBOOPTLEN;
		}
		optbuf[0] = IP6OPT_PADN;
		optbuf[1] = 1;

		/*
		 * Adjust the header length according to the pad and
		 * the jumbo payload option.
		 */
		hbh = mtod(mopt, struct ip6_hbh *);
		hbh->ip6h_len += (JUMBOOPTLEN >> 3);
	}

	/* fill in the option. */
	optbuf[2] = IP6OPT_JUMBO;
	optbuf[3] = 4;
	v = (u_int32_t)htonl(plen + JUMBOOPTLEN);
	bcopy(&v, &optbuf[4], sizeof(u_int32_t));

	/* finally, adjust the packet header length */
	exthdrs->ip6e_ip6->m_pkthdr.len += JUMBOOPTLEN;

	return (0);
#undef JUMBOOPTLEN
}

/*
 * Insert fragment header and copy unfragmentable header portions.
 */
static int
ip6_insertfraghdr(struct mbuf *m0, struct mbuf *m, int hlen,
    struct ip6_frag **frghdrp)
{
	struct mbuf *n, *mlast;

	if (hlen > sizeof(struct ip6_hdr)) {
		n = m_copym(m0, sizeof(struct ip6_hdr),
		    hlen - sizeof(struct ip6_hdr), M_NOWAIT);
		if (n == NULL)
			return (ENOBUFS);
		m->m_next = n;
	} else
		n = m;

	/* Search for the last mbuf of unfragmentable part. */
	for (mlast = n; mlast->m_next; mlast = mlast->m_next)
		;

	if (M_WRITABLE(mlast) &&
	    M_TRAILINGSPACE(mlast) >= sizeof(struct ip6_frag)) {
		/* use the trailing space of the last mbuf for the fragment hdr */
		*frghdrp = (struct ip6_frag *)(mtod(mlast, caddr_t) +
		    mlast->m_len);
		mlast->m_len += sizeof(struct ip6_frag);
		m->m_pkthdr.len += sizeof(struct ip6_frag);
	} else {
		/* allocate a new mbuf for the fragment header */
		struct mbuf *mfrg;

		mfrg = m_get(M_NOWAIT, MT_DATA);
		if (mfrg == NULL)
			return (ENOBUFS);
		mfrg->m_len = sizeof(struct ip6_frag);
		*frghdrp = mtod(mfrg, struct ip6_frag *);
		mlast->m_next = mfrg;
	}

	return (0);
}

/*
 * Calculates IPv6 path mtu for destination @dst.
 * Resulting MTU is stored in @mtup.
 *
 * Returns 0 on success.
 */
static int
ip6_getpmtu_ctl(u_int fibnum, const struct in6_addr *dst, u_long *mtup)
{
	struct epoch_tracker et;
	struct nhop_object *nh;
	struct in6_addr kdst;
	uint32_t scopeid;
	int error;

	in6_splitscope(dst, &kdst, &scopeid);

	NET_EPOCH_ENTER(et);
	nh = fib6_lookup(fibnum, &kdst, scopeid, NHR_NONE, 0);
	if (nh != NULL)
		error = ip6_calcmtu(nh->nh_ifp, dst, nh->nh_mtu, mtup, NULL, 0);
	else
		error = EHOSTUNREACH;
	NET_EPOCH_EXIT(et);

	return (error);
}

/*
 * Calculates IPv6 path MTU for @dst based on transmit @ifp,
 * and cached data in @ro_pmtu.
 * MTU from (successful) route lookup is saved (along with dst)
 * inside @ro_pmtu to avoid subsequent route lookups after packet
 * filter processing.
 *
 * Stores mtu and always-frag value into @mtup and @alwaysfragp.
 * Returns 0 on success.
 */
static int
ip6_getpmtu(struct route_in6 *ro_pmtu, int do_lookup,
    struct ifnet *ifp, const struct in6_addr *dst, u_long *mtup,
    int *alwaysfragp, u_int fibnum, u_int proto)
{
	struct nhop_object *nh;
	struct in6_addr kdst;
	uint32_t scopeid;
	struct sockaddr_in6 *sa6_dst, sin6;
	u_long mtu;

	NET_EPOCH_ASSERT();

	mtu = 0;
	if (ro_pmtu == NULL || do_lookup) {
		/*
		 * Here ro_pmtu has final destination address, while
		 * ro might represent immediate destination.
		 * Use ro_pmtu destination since mtu might differ.
		 */
		if (ro_pmtu != NULL) {
			sa6_dst = (struct sockaddr_in6 *)&ro_pmtu->ro_dst;
			if (!IN6_ARE_ADDR_EQUAL(&sa6_dst->sin6_addr, dst))
				ro_pmtu->ro_mtu = 0;
		} else
			sa6_dst = &sin6;

		if (ro_pmtu == NULL || ro_pmtu->ro_mtu == 0) {
			bzero(sa6_dst, sizeof(*sa6_dst));
			sa6_dst->sin6_family = AF_INET6;
			sa6_dst->sin6_len = sizeof(struct sockaddr_in6);
			sa6_dst->sin6_addr = *dst;

			in6_splitscope(dst, &kdst, &scopeid);
			nh = fib6_lookup(fibnum, &kdst, scopeid, NHR_NONE, 0);
			if (nh != NULL) {
				mtu = nh->nh_mtu;
				if (ro_pmtu != NULL)
					ro_pmtu->ro_mtu = mtu;
			}
		} else
			mtu = ro_pmtu->ro_mtu;
	}

	if (ro_pmtu != NULL && ro_pmtu->ro_nh != NULL)
		mtu = ro_pmtu->ro_nh->nh_mtu;

	return (ip6_calcmtu(ifp, dst, mtu, mtup, alwaysfragp, proto));
}

/*
 * Calculate MTU based on transmit @ifp, route mtu @rt_mtu and
 * hostcache data for @dst.
 * Stores mtu and always-frag value into @mtup and @alwaysfragp.
 *
 * Returns 0 on success.
 */
static int
ip6_calcmtu(struct ifnet *ifp, const struct in6_addr *dst, u_long rt_mtu,
    u_long *mtup, int *alwaysfragp, u_int proto)
{
	u_long mtu = 0;
	int alwaysfrag = 0;
	int error = 0;

	if (rt_mtu > 0) {
		u_int32_t ifmtu;
		struct in_conninfo inc;

		bzero(&inc, sizeof(inc));
		inc.inc_flags |= INC_ISIPV6;
		inc.inc6_faddr = *dst;

		ifmtu = IN6_LINKMTU(ifp);

		/* TCP is known to react to pmtu changes so skip hc */
		if (proto != IPPROTO_TCP)
			mtu = tcp_hc_getmtu(&inc);

		if (mtu)
			mtu = min(mtu, rt_mtu);
		else
			mtu = rt_mtu;
		if (mtu == 0)
			mtu = ifmtu;
		else if (mtu < IPV6_MMTU) {
			/*
			 * RFC2460 section 5, last paragraph:
			 * if we record ICMPv6 too big message with
			 * mtu < IPV6_MMTU, transmit packets sized IPV6_MMTU
			 * or smaller, with framgent header attached.
			 * (fragment header is needed regardless from the
			 * packet size, for translators to identify packets)
			 */
			alwaysfrag = 1;
			mtu = IPV6_MMTU;
		}
	} else if (ifp) {
		mtu = IN6_LINKMTU(ifp);
	} else
		error = EHOSTUNREACH; /* XXX */

	*mtup = mtu;
	if (alwaysfragp)
		*alwaysfragp = alwaysfrag;
	return (error);
}

/*
 * IP6 socket option processing.
 */
int
ip6_ctloutput(struct socket *so, struct sockopt *sopt)
{
	int optdatalen, uproto;
	void *optdata;
	struct inpcb *inp = sotoinpcb(so);
	int error, optval;
	int level, op, optname;
	int optlen;
	struct thread *td;
#ifdef	RSS
	uint32_t rss_bucket;
	int retval;
#endif

/*
 * Don't use more than a quarter of mbuf clusters.  N.B.:
 * nmbclusters is an int, but nmbclusters * MCLBYTES may overflow
 * on LP64 architectures, so cast to u_long to avoid undefined
 * behavior.  ILP32 architectures cannot have nmbclusters
 * large enough to overflow for other reasons.
 */
#define IPV6_PKTOPTIONS_MBUF_LIMIT	((u_long)nmbclusters * MCLBYTES / 4)

	level = sopt->sopt_level;
	op = sopt->sopt_dir;
	optname = sopt->sopt_name;
	optlen = sopt->sopt_valsize;
	td = sopt->sopt_td;
	error = 0;
	optval = 0;
	uproto = (int)so->so_proto->pr_protocol;

	if (level != IPPROTO_IPV6) {
		error = EINVAL;

		if (sopt->sopt_level == SOL_SOCKET &&
		    sopt->sopt_dir == SOPT_SET) {
			switch (sopt->sopt_name) {
			case SO_REUSEADDR:
				INP_WLOCK(inp);
				if ((so->so_options & SO_REUSEADDR) != 0)
					inp->inp_flags2 |= INP_REUSEADDR;
				else
					inp->inp_flags2 &= ~INP_REUSEADDR;
				INP_WUNLOCK(inp);
				error = 0;
				break;
			case SO_REUSEPORT:
				INP_WLOCK(inp);
				if ((so->so_options & SO_REUSEPORT) != 0)
					inp->inp_flags2 |= INP_REUSEPORT;
				else
					inp->inp_flags2 &= ~INP_REUSEPORT;
				INP_WUNLOCK(inp);
				error = 0;
				break;
			case SO_REUSEPORT_LB:
				INP_WLOCK(inp);
				if ((so->so_options & SO_REUSEPORT_LB) != 0)
					inp->inp_flags2 |= INP_REUSEPORT_LB;
				else
					inp->inp_flags2 &= ~INP_REUSEPORT_LB;
				INP_WUNLOCK(inp);
				error = 0;
				break;
			case SO_SETFIB:
				INP_WLOCK(inp);
				inp->inp_inc.inc_fibnum = so->so_fibnum;
				INP_WUNLOCK(inp);
				error = 0;
				break;
			case SO_MAX_PACING_RATE:
#ifdef RATELIMIT
				INP_WLOCK(inp);
				inp->inp_flags2 |= INP_RATE_LIMIT_CHANGED;
				INP_WUNLOCK(inp);
				error = 0;
#else
				error = EOPNOTSUPP;
#endif
				break;
			default:
				break;
			}
		}
	} else {		/* level == IPPROTO_IPV6 */
		switch (op) {
		case SOPT_SET:
			switch (optname) {
			case IPV6_2292PKTOPTIONS:
#ifdef IPV6_PKTOPTIONS
			case IPV6_PKTOPTIONS:
#endif
			{
				struct mbuf *m;

				if (optlen > IPV6_PKTOPTIONS_MBUF_LIMIT) {
					printf("ip6_ctloutput: mbuf limit hit\n");
					error = ENOBUFS;
					break;
				}

				error = soopt_getm(sopt, &m); /* XXX */
				if (error != 0)
					break;
				error = soopt_mcopyin(sopt, m); /* XXX */
				if (error != 0)
					break;
				INP_WLOCK(inp);
				error = ip6_pcbopts(&inp->in6p_outputopts, m,
				    so, sopt);
				INP_WUNLOCK(inp);
				m_freem(m); /* XXX */
				break;
			}

			/*
			 * Use of some Hop-by-Hop options or some
			 * Destination options, might require special
			 * privilege.  That is, normal applications
			 * (without special privilege) might be forbidden
			 * from setting certain options in outgoing packets,
			 * and might never see certain options in received
			 * packets. [RFC 2292 Section 6]
			 * KAME specific note:
			 *  KAME prevents non-privileged users from sending or
			 *  receiving ANY hbh/dst options in order to avoid
			 *  overhead of parsing options in the kernel.
			 */
			case IPV6_RECVHOPOPTS:
			case IPV6_RECVDSTOPTS:
			case IPV6_RECVRTHDRDSTOPTS:
				if (td != NULL) {
					error = priv_check(td,
					    PRIV_NETINET_SETHDROPTS);
					if (error)
						break;
				}
				/* FALLTHROUGH */
			case IPV6_UNICAST_HOPS:
			case IPV6_HOPLIMIT:

			case IPV6_RECVPKTINFO:
			case IPV6_RECVHOPLIMIT:
			case IPV6_RECVRTHDR:
			case IPV6_RECVPATHMTU:
			case IPV6_RECVTCLASS:
			case IPV6_RECVFLOWID:
#ifdef	RSS
			case IPV6_RECVRSSBUCKETID:
#endif
			case IPV6_V6ONLY:
			case IPV6_AUTOFLOWLABEL:
			case IPV6_ORIGDSTADDR:
			case IPV6_BINDANY:
			case IPV6_BINDMULTI:
#ifdef	RSS
			case IPV6_RSS_LISTEN_BUCKET:
#endif
			case IPV6_VLAN_PCP:
				if (optname == IPV6_BINDANY && td != NULL) {
					error = priv_check(td,
					    PRIV_NETINET_BINDANY);
					if (error)
						break;
				}

				if (optlen != sizeof(int)) {
					error = EINVAL;
					break;
				}
				error = sooptcopyin(sopt, &optval,
					sizeof optval, sizeof optval);
				if (error)
					break;
				switch (optname) {
				case IPV6_UNICAST_HOPS:
					if (optval < -1 || optval >= 256)
						error = EINVAL;
					else {
						/* -1 = kernel default */
						inp->in6p_hops = optval;
						if ((inp->inp_vflag &
						     INP_IPV4) != 0)
							inp->inp_ip_ttl = optval;
					}
					break;
#define OPTSET(bit) \
do { \
	INP_WLOCK(inp); \
	if (optval) \
		inp->inp_flags |= (bit); \
	else \
		inp->inp_flags &= ~(bit); \
	INP_WUNLOCK(inp); \
} while (/*CONSTCOND*/ 0)
#define OPTSET2292(bit) \
do { \
	INP_WLOCK(inp); \
	inp->inp_flags |= IN6P_RFC2292; \
	if (optval) \
		inp->inp_flags |= (bit); \
	else \
		inp->inp_flags &= ~(bit); \
	INP_WUNLOCK(inp); \
} while (/*CONSTCOND*/ 0)
#define OPTBIT(bit) (inp->inp_flags & (bit) ? 1 : 0)

#define OPTSET2_N(bit, val) do {					\
	if (val)							\
		inp->inp_flags2 |= bit;					\
	else								\
		inp->inp_flags2 &= ~bit;				\
} while (0)
#define OPTSET2(bit, val) do {						\
	INP_WLOCK(inp);							\
	OPTSET2_N(bit, val);						\
	INP_WUNLOCK(inp);						\
} while (0)
#define OPTBIT2(bit) (inp->inp_flags2 & (bit) ? 1 : 0)
#define OPTSET2292_EXCLUSIVE(bit)					\
do {									\
	INP_WLOCK(inp);							\
	if (OPTBIT(IN6P_RFC2292)) {					\
		error = EINVAL;						\
	} else {							\
		if (optval)						\
			inp->inp_flags |= (bit);			\
		else							\
			inp->inp_flags &= ~(bit);			\
	}								\
	INP_WUNLOCK(inp);						\
} while (/*CONSTCOND*/ 0)

				case IPV6_RECVPKTINFO:
					OPTSET2292_EXCLUSIVE(IN6P_PKTINFO);
					break;

				case IPV6_HOPLIMIT:
				{
					struct ip6_pktopts **optp;

					/* cannot mix with RFC2292 */
					if (OPTBIT(IN6P_RFC2292)) {
						error = EINVAL;
						break;
					}
					INP_WLOCK(inp);
					if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
						INP_WUNLOCK(inp);
						return (ECONNRESET);
					}
					optp = &inp->in6p_outputopts;
					error = ip6_pcbopt(IPV6_HOPLIMIT,
					    (u_char *)&optval, sizeof(optval),
					    optp, (td != NULL) ? td->td_ucred :
					    NULL, uproto);
					INP_WUNLOCK(inp);
					break;
				}

				case IPV6_RECVHOPLIMIT:
					OPTSET2292_EXCLUSIVE(IN6P_HOPLIMIT);
					break;

				case IPV6_RECVHOPOPTS:
					OPTSET2292_EXCLUSIVE(IN6P_HOPOPTS);
					break;

				case IPV6_RECVDSTOPTS:
					OPTSET2292_EXCLUSIVE(IN6P_DSTOPTS);
					break;

				case IPV6_RECVRTHDRDSTOPTS:
					OPTSET2292_EXCLUSIVE(IN6P_RTHDRDSTOPTS);
					break;

				case IPV6_RECVRTHDR:
					OPTSET2292_EXCLUSIVE(IN6P_RTHDR);
					break;

				case IPV6_RECVPATHMTU:
					/*
					 * We ignore this option for TCP
					 * sockets.
					 * (RFC3542 leaves this case
					 * unspecified.)
					 */
					if (uproto != IPPROTO_TCP)
						OPTSET(IN6P_MTU);
					break;

				case IPV6_RECVFLOWID:
					OPTSET2(INP_RECVFLOWID, optval);
					break;

#ifdef	RSS
				case IPV6_RECVRSSBUCKETID:
					OPTSET2(INP_RECVRSSBUCKETID, optval);
					break;
#endif

				case IPV6_V6ONLY:
					INP_WLOCK(inp);
					if (inp->inp_lport ||
					    !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
						/*
						 * The socket is already bound.
						 */
						INP_WUNLOCK(inp);
						error = EINVAL;
						break;
					}
					if (optval) {
						inp->inp_flags |= IN6P_IPV6_V6ONLY;
						inp->inp_vflag &= ~INP_IPV4;
					} else {
						inp->inp_flags &= ~IN6P_IPV6_V6ONLY;
						inp->inp_vflag |= INP_IPV4;
					}
					INP_WUNLOCK(inp);
					break;
				case IPV6_RECVTCLASS:
					/* cannot mix with RFC2292 XXX */
					OPTSET2292_EXCLUSIVE(IN6P_TCLASS);
					break;
				case IPV6_AUTOFLOWLABEL:
					OPTSET(IN6P_AUTOFLOWLABEL);
					break;

				case IPV6_ORIGDSTADDR:
					OPTSET2(INP_ORIGDSTADDR, optval);
					break;
				case IPV6_BINDANY:
					OPTSET(INP_BINDANY);
					break;

				case IPV6_BINDMULTI:
					OPTSET2(INP_BINDMULTI, optval);
					break;
#ifdef	RSS
				case IPV6_RSS_LISTEN_BUCKET:
					if ((optval >= 0) &&
					    (optval < rss_getnumbuckets())) {
						INP_WLOCK(inp);
						inp->inp_rss_listen_bucket = optval;
						OPTSET2_N(INP_RSS_BUCKET_SET, 1);
						INP_WUNLOCK(inp);
					} else {
						error = EINVAL;
					}
					break;
#endif
				case IPV6_VLAN_PCP:
					if ((optval >= -1) && (optval <=
					    (INP_2PCP_MASK >> INP_2PCP_SHIFT))) {
						if (optval == -1) {
							INP_WLOCK(inp);
							inp->inp_flags2 &=
							    ~(INP_2PCP_SET |
							    INP_2PCP_MASK);
							INP_WUNLOCK(inp);
						} else {
							INP_WLOCK(inp);
							inp->inp_flags2 |=
							    INP_2PCP_SET;
							inp->inp_flags2 &=
							    ~INP_2PCP_MASK;
							inp->inp_flags2 |=
							    optval <<
							    INP_2PCP_SHIFT;
							INP_WUNLOCK(inp);
						}
					} else
						error = EINVAL;
					break;
				}
				break;

			case IPV6_TCLASS:
			case IPV6_DONTFRAG:
			case IPV6_USE_MIN_MTU:
			case IPV6_PREFER_TEMPADDR:
				if (optlen != sizeof(optval)) {
					error = EINVAL;
					break;
				}
				error = sooptcopyin(sopt, &optval,
					sizeof optval, sizeof optval);
				if (error)
					break;
				{
					struct ip6_pktopts **optp;
					INP_WLOCK(inp);
					if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
						INP_WUNLOCK(inp);
						return (ECONNRESET);
					}
					optp = &inp->in6p_outputopts;
					error = ip6_pcbopt(optname,
					    (u_char *)&optval, sizeof(optval),
					    optp, (td != NULL) ? td->td_ucred :
					    NULL, uproto);
					INP_WUNLOCK(inp);
					break;
				}

			case IPV6_2292PKTINFO:
			case IPV6_2292HOPLIMIT:
			case IPV6_2292HOPOPTS:
			case IPV6_2292DSTOPTS:
			case IPV6_2292RTHDR:
				/* RFC 2292 */
				if (optlen != sizeof(int)) {
					error = EINVAL;
					break;
				}
				error = sooptcopyin(sopt, &optval,
					sizeof optval, sizeof optval);
				if (error)
					break;
				switch (optname) {
				case IPV6_2292PKTINFO:
					OPTSET2292(IN6P_PKTINFO);
					break;
				case IPV6_2292HOPLIMIT:
					OPTSET2292(IN6P_HOPLIMIT);
					break;
				case IPV6_2292HOPOPTS:
					/*
					 * Check super-user privilege.
					 * See comments for IPV6_RECVHOPOPTS.
					 */
					if (td != NULL) {
						error = priv_check(td,
						    PRIV_NETINET_SETHDROPTS);
						if (error)
							return (error);
					}
					OPTSET2292(IN6P_HOPOPTS);
					break;
				case IPV6_2292DSTOPTS:
					if (td != NULL) {
						error = priv_check(td,
						    PRIV_NETINET_SETHDROPTS);
						if (error)
							return (error);
					}
					OPTSET2292(IN6P_DSTOPTS|IN6P_RTHDRDSTOPTS); /* XXX */
					break;
				case IPV6_2292RTHDR:
					OPTSET2292(IN6P_RTHDR);
					break;
				}
				break;
			case IPV6_PKTINFO:
			case IPV6_HOPOPTS:
			case IPV6_RTHDR:
			case IPV6_DSTOPTS:
			case IPV6_RTHDRDSTOPTS:
			case IPV6_NEXTHOP:
			{
				/* new advanced API (RFC3542) */
				u_char *optbuf;
				u_char optbuf_storage[MCLBYTES];
				int optlen;
				struct ip6_pktopts **optp;

				/* cannot mix with RFC2292 */
				if (OPTBIT(IN6P_RFC2292)) {
					error = EINVAL;
					break;
				}

				/*
				 * We only ensure valsize is not too large
				 * here.  Further validation will be done
				 * later.
				 */
				error = sooptcopyin(sopt, optbuf_storage,
				    sizeof(optbuf_storage), 0);
				if (error)
					break;
				optlen = sopt->sopt_valsize;
				optbuf = optbuf_storage;
				INP_WLOCK(inp);
				if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
					INP_WUNLOCK(inp);
					return (ECONNRESET);
				}
				optp = &inp->in6p_outputopts;
				error = ip6_pcbopt(optname, optbuf, optlen,
				    optp, (td != NULL) ? td->td_ucred : NULL,
				    uproto);
				INP_WUNLOCK(inp);
				break;
			}
#undef OPTSET

			case IPV6_MULTICAST_IF:
			case IPV6_MULTICAST_HOPS:
			case IPV6_MULTICAST_LOOP:
			case IPV6_JOIN_GROUP:
			case IPV6_LEAVE_GROUP:
			case IPV6_MSFILTER:
			case MCAST_BLOCK_SOURCE:
			case MCAST_UNBLOCK_SOURCE:
			case MCAST_JOIN_GROUP:
			case MCAST_LEAVE_GROUP:
			case MCAST_JOIN_SOURCE_GROUP:
			case MCAST_LEAVE_SOURCE_GROUP:
				error = ip6_setmoptions(inp, sopt);
				break;

			case IPV6_PORTRANGE:
				error = sooptcopyin(sopt, &optval,
				    sizeof optval, sizeof optval);
				if (error)
					break;

				INP_WLOCK(inp);
				switch (optval) {
				case IPV6_PORTRANGE_DEFAULT:
					inp->inp_flags &= ~(INP_LOWPORT);
					inp->inp_flags &= ~(INP_HIGHPORT);
					break;

				case IPV6_PORTRANGE_HIGH:
					inp->inp_flags &= ~(INP_LOWPORT);
					inp->inp_flags |= INP_HIGHPORT;
					break;

				case IPV6_PORTRANGE_LOW:
					inp->inp_flags &= ~(INP_HIGHPORT);
					inp->inp_flags |= INP_LOWPORT;
					break;

				default:
					error = EINVAL;
					break;
				}
				INP_WUNLOCK(inp);
				break;

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
			case IPV6_IPSEC_POLICY:
				if (IPSEC_ENABLED(ipv6)) {
					error = IPSEC_PCBCTL(ipv6, inp, sopt);
					break;
				}
				/* FALLTHROUGH */
#endif /* IPSEC */

			default:
				error = ENOPROTOOPT;
				break;
			}
			break;

		case SOPT_GET:
			switch (optname) {
			case IPV6_2292PKTOPTIONS:
#ifdef IPV6_PKTOPTIONS
			case IPV6_PKTOPTIONS:
#endif
				/*
				 * RFC3542 (effectively) deprecated the
				 * semantics of the 2292-style pktoptions.
				 * Since it was not reliable in nature (i.e.,
				 * applications had to expect the lack of some
				 * information after all), it would make sense
				 * to simplify this part by always returning
				 * empty data.
				 */
				sopt->sopt_valsize = 0;
				break;

			case IPV6_RECVHOPOPTS:
			case IPV6_RECVDSTOPTS:
			case IPV6_RECVRTHDRDSTOPTS:
			case IPV6_UNICAST_HOPS:
			case IPV6_RECVPKTINFO:
			case IPV6_RECVHOPLIMIT:
			case IPV6_RECVRTHDR:
			case IPV6_RECVPATHMTU:

			case IPV6_V6ONLY:
			case IPV6_PORTRANGE:
			case IPV6_RECVTCLASS:
			case IPV6_AUTOFLOWLABEL:
			case IPV6_BINDANY:
			case IPV6_FLOWID:
			case IPV6_FLOWTYPE:
			case IPV6_RECVFLOWID:
#ifdef	RSS
			case IPV6_RSSBUCKETID:
			case IPV6_RECVRSSBUCKETID:
#endif
			case IPV6_BINDMULTI:
			case IPV6_VLAN_PCP:
				switch (optname) {
				case IPV6_RECVHOPOPTS:
					optval = OPTBIT(IN6P_HOPOPTS);
					break;

				case IPV6_RECVDSTOPTS:
					optval = OPTBIT(IN6P_DSTOPTS);
					break;

				case IPV6_RECVRTHDRDSTOPTS:
					optval = OPTBIT(IN6P_RTHDRDSTOPTS);
					break;

				case IPV6_UNICAST_HOPS:
					optval = inp->in6p_hops;
					break;

				case IPV6_RECVPKTINFO:
					optval = OPTBIT(IN6P_PKTINFO);
					break;

				case IPV6_RECVHOPLIMIT:
					optval = OPTBIT(IN6P_HOPLIMIT);
					break;

				case IPV6_RECVRTHDR:
					optval = OPTBIT(IN6P_RTHDR);
					break;

				case IPV6_RECVPATHMTU:
					optval = OPTBIT(IN6P_MTU);
					break;

				case IPV6_V6ONLY:
					optval = OPTBIT(IN6P_IPV6_V6ONLY);
					break;

				case IPV6_PORTRANGE:
				    {
					int flags;
					flags = inp->inp_flags;
					if (flags & INP_HIGHPORT)
						optval = IPV6_PORTRANGE_HIGH;
					else if (flags & INP_LOWPORT)
						optval = IPV6_PORTRANGE_LOW;
					else
						optval = 0;
					break;
				    }
				case IPV6_RECVTCLASS:
					optval = OPTBIT(IN6P_TCLASS);
					break;

				case IPV6_AUTOFLOWLABEL:
					optval = OPTBIT(IN6P_AUTOFLOWLABEL);
					break;

				case IPV6_ORIGDSTADDR:
					optval = OPTBIT2(INP_ORIGDSTADDR);
					break;

				case IPV6_BINDANY:
					optval = OPTBIT(INP_BINDANY);
					break;

				case IPV6_FLOWID:
					optval = inp->inp_flowid;
					break;

				case IPV6_FLOWTYPE:
					optval = inp->inp_flowtype;
					break;

				case IPV6_RECVFLOWID:
					optval = OPTBIT2(INP_RECVFLOWID);
					break;
#ifdef	RSS
				case IPV6_RSSBUCKETID:
					retval =
					    rss_hash2bucket(inp->inp_flowid,
					    inp->inp_flowtype,
					    &rss_bucket);
					if (retval == 0)
						optval = rss_bucket;
					else
						error = EINVAL;
					break;

				case IPV6_RECVRSSBUCKETID:
					optval = OPTBIT2(INP_RECVRSSBUCKETID);
					break;
#endif

				case IPV6_BINDMULTI:
					optval = OPTBIT2(INP_BINDMULTI);
					break;

				case IPV6_VLAN_PCP:
					if (OPTBIT2(INP_2PCP_SET)) {
						optval = (inp->inp_flags2 &
							    INP_2PCP_MASK) >>
							    INP_2PCP_SHIFT;
					} else {
						optval = -1;
					}
					break;
				}

				if (error)
					break;
				error = sooptcopyout(sopt, &optval,
					sizeof optval);
				break;

			case IPV6_PATHMTU:
			{
				u_long pmtu = 0;
				struct ip6_mtuinfo mtuinfo;
				struct in6_addr addr;

				if (!(so->so_state & SS_ISCONNECTED))
					return (ENOTCONN);
				/*
				 * XXX: we dot not consider the case of source
				 * routing, or optional information to specify
				 * the outgoing interface.
				 * Copy faddr out of inp to avoid holding lock
				 * on inp during route lookup.
				 */
				INP_RLOCK(inp);
				bcopy(&inp->in6p_faddr, &addr, sizeof(addr));
				INP_RUNLOCK(inp);
				error = ip6_getpmtu_ctl(so->so_fibnum,
				    &addr, &pmtu);
				if (error)
					break;
				if (pmtu > IPV6_MAXPACKET)
					pmtu = IPV6_MAXPACKET;

				bzero(&mtuinfo, sizeof(mtuinfo));
				mtuinfo.ip6m_mtu = (u_int32_t)pmtu;
				optdata = (void *)&mtuinfo;
				optdatalen = sizeof(mtuinfo);
				error = sooptcopyout(sopt, optdata,
				    optdatalen);
				break;
			}

			case IPV6_2292PKTINFO:
			case IPV6_2292HOPLIMIT:
			case IPV6_2292HOPOPTS:
			case IPV6_2292RTHDR:
			case IPV6_2292DSTOPTS:
				switch (optname) {
				case IPV6_2292PKTINFO:
					optval = OPTBIT(IN6P_PKTINFO);
					break;
				case IPV6_2292HOPLIMIT:
					optval = OPTBIT(IN6P_HOPLIMIT);
					break;
				case IPV6_2292HOPOPTS:
					optval = OPTBIT(IN6P_HOPOPTS);
					break;
				case IPV6_2292RTHDR:
					optval = OPTBIT(IN6P_RTHDR);
					break;
				case IPV6_2292DSTOPTS:
					optval = OPTBIT(IN6P_DSTOPTS|IN6P_RTHDRDSTOPTS);
					break;
				}
				error = sooptcopyout(sopt, &optval,
				    sizeof optval);
				break;
			case IPV6_PKTINFO:
			case IPV6_HOPOPTS:
			case IPV6_RTHDR:
			case IPV6_DSTOPTS:
			case IPV6_RTHDRDSTOPTS:
			case IPV6_NEXTHOP:
			case IPV6_TCLASS:
			case IPV6_DONTFRAG:
			case IPV6_USE_MIN_MTU:
			case IPV6_PREFER_TEMPADDR:
				error = ip6_getpcbopt(inp, optname, sopt);
				break;

			case IPV6_MULTICAST_IF:
			case IPV6_MULTICAST_HOPS:
			case IPV6_MULTICAST_LOOP:
			case IPV6_MSFILTER:
				error = ip6_getmoptions(inp, sopt);
				break;

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
			case IPV6_IPSEC_POLICY:
				if (IPSEC_ENABLED(ipv6)) {
					error = IPSEC_PCBCTL(ipv6, inp, sopt);
					break;
				}
				/* FALLTHROUGH */
#endif /* IPSEC */
			default:
				error = ENOPROTOOPT;
				break;
			}
			break;
		}
	}
	return (error);
}

int
ip6_raw_ctloutput(struct socket *so, struct sockopt *sopt)
{
	int error = 0, optval, optlen;
	const int icmp6off = offsetof(struct icmp6_hdr, icmp6_cksum);
	struct inpcb *inp = sotoinpcb(so);
	int level, op, optname;

	level = sopt->sopt_level;
	op = sopt->sopt_dir;
	optname = sopt->sopt_name;
	optlen = sopt->sopt_valsize;

	if (level != IPPROTO_IPV6) {
		return (EINVAL);
	}

	switch (optname) {
	case IPV6_CHECKSUM:
		/*
		 * For ICMPv6 sockets, no modification allowed for checksum
		 * offset, permit "no change" values to help existing apps.
		 *
		 * RFC3542 says: "An attempt to set IPV6_CHECKSUM
		 * for an ICMPv6 socket will fail."
		 * The current behavior does not meet RFC3542.
		 */
		switch (op) {
		case SOPT_SET:
			if (optlen != sizeof(int)) {
				error = EINVAL;
				break;
			}
			error = sooptcopyin(sopt, &optval, sizeof(optval),
					    sizeof(optval));
			if (error)
				break;
			if (optval < -1 || (optval % 2) != 0) {
				/*
				 * The API assumes non-negative even offset
				 * values or -1 as a special value.
				 */
				error = EINVAL;
			} else if (inp->inp_ip_p == IPPROTO_ICMPV6) {
				if (optval != icmp6off)
					error = EINVAL;
			} else
				inp->in6p_cksum = optval;
			break;

		case SOPT_GET:
			if (inp->inp_ip_p == IPPROTO_ICMPV6)
				optval = icmp6off;
			else
				optval = inp->in6p_cksum;

			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	default:
		error = ENOPROTOOPT;
		break;
	}

	return (error);
}

/*
 * Set up IP6 options in pcb for insertion in output packets or
 * specifying behavior of outgoing packets.
 */
static int
ip6_pcbopts(struct ip6_pktopts **pktopt, struct mbuf *m,
    struct socket *so, struct sockopt *sopt)
{
	struct ip6_pktopts *opt = *pktopt;
	int error = 0;
	struct thread *td = sopt->sopt_td;
	struct epoch_tracker et;

	/* turn off any old options. */
	if (opt) {
#ifdef DIAGNOSTIC
		if (opt->ip6po_pktinfo || opt->ip6po_nexthop ||
		    opt->ip6po_hbh || opt->ip6po_dest1 || opt->ip6po_dest2 ||
		    opt->ip6po_rhinfo.ip6po_rhi_rthdr)
			printf("ip6_pcbopts: all specified options are cleared.\n");
#endif
		ip6_clearpktopts(opt, -1);
	} else {
		opt = malloc(sizeof(*opt), M_IP6OPT, M_NOWAIT);
		if (opt == NULL)
			return (ENOMEM);
	}
	*pktopt = NULL;

	if (!m || m->m_len == 0) {
		/*
		 * Only turning off any previous options, regardless of
		 * whether the opt is just created or given.
		 */
		free(opt, M_IP6OPT);
		return (0);
	}

	/*  set options specified by user. */
	NET_EPOCH_ENTER(et);
	if ((error = ip6_setpktopts(m, opt, NULL, (td != NULL) ?
	    td->td_ucred : NULL, so->so_proto->pr_protocol)) != 0) {
		ip6_clearpktopts(opt, -1); /* XXX: discard all options */
		free(opt, M_IP6OPT);
		NET_EPOCH_EXIT(et);
		return (error);
	}
	NET_EPOCH_EXIT(et);
	*pktopt = opt;
	return (0);
}

/*
 * initialize ip6_pktopts.  beware that there are non-zero default values in
 * the struct.
 */
void
ip6_initpktopts(struct ip6_pktopts *opt)
{

	bzero(opt, sizeof(*opt));
	opt->ip6po_hlim = -1;	/* -1 means default hop limit */
	opt->ip6po_tclass = -1;	/* -1 means default traffic class */
	opt->ip6po_minmtu = IP6PO_MINMTU_MCASTONLY;
	opt->ip6po_prefer_tempaddr = IP6PO_TEMPADDR_SYSTEM;
}

static int
ip6_pcbopt(int optname, u_char *buf, int len, struct ip6_pktopts **pktopt,
    struct ucred *cred, int uproto)
{
	struct epoch_tracker et;
	struct ip6_pktopts *opt;
	int ret;

	if (*pktopt == NULL) {
		*pktopt = malloc(sizeof(struct ip6_pktopts), M_IP6OPT,
		    M_NOWAIT);
		if (*pktopt == NULL)
			return (ENOBUFS);
		ip6_initpktopts(*pktopt);
	}
	opt = *pktopt;

	NET_EPOCH_ENTER(et);
	ret = ip6_setpktopt(optname, buf, len, opt, cred, 1, 0, uproto);
	NET_EPOCH_EXIT(et);

	return (ret);
}

#define GET_PKTOPT_VAR(field, lenexpr) do {					\
	if (pktopt && pktopt->field) {						\
		INP_RUNLOCK(inp);						\
		optdata = malloc(sopt->sopt_valsize, M_TEMP, M_WAITOK);		\
		malloc_optdata = true;						\
		INP_RLOCK(inp);							\
		if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {		\
			INP_RUNLOCK(inp);					\
			free(optdata, M_TEMP);					\
			return (ECONNRESET);					\
		}								\
		pktopt = inp->in6p_outputopts;					\
		if (pktopt && pktopt->field) {					\
			optdatalen = min(lenexpr, sopt->sopt_valsize);		\
			bcopy(pktopt->field, optdata, optdatalen);		\
		} else {							\
			free(optdata, M_TEMP);					\
			optdata = NULL;						\
			malloc_optdata = false;					\
		}								\
	}									\
} while(0)

#define GET_PKTOPT_EXT_HDR(field) GET_PKTOPT_VAR(field,				\
	(((struct ip6_ext *)pktopt->field)->ip6e_len + 1) << 3)

#define GET_PKTOPT_SOCKADDR(field) GET_PKTOPT_VAR(field,			\
	pktopt->field->sa_len)

static int
ip6_getpcbopt(struct inpcb *inp, int optname, struct sockopt *sopt)
{
	void *optdata = NULL;
	bool malloc_optdata = false;
	int optdatalen = 0;
	int error = 0;
	struct in6_pktinfo null_pktinfo;
	int deftclass = 0, on;
	int defminmtu = IP6PO_MINMTU_MCASTONLY;
	int defpreftemp = IP6PO_TEMPADDR_SYSTEM;
	struct ip6_pktopts *pktopt;

	INP_RLOCK(inp);
	pktopt = inp->in6p_outputopts;

	switch (optname) {
	case IPV6_PKTINFO:
		optdata = (void *)&null_pktinfo;
		if (pktopt && pktopt->ip6po_pktinfo) {
			bcopy(pktopt->ip6po_pktinfo, &null_pktinfo,
			    sizeof(null_pktinfo));
			in6_clearscope(&null_pktinfo.ipi6_addr);
		} else {
			/* XXX: we don't have to do this every time... */
			bzero(&null_pktinfo, sizeof(null_pktinfo));
		}
		optdatalen = sizeof(struct in6_pktinfo);
		break;
	case IPV6_TCLASS:
		if (pktopt && pktopt->ip6po_tclass >= 0)
			deftclass = pktopt->ip6po_tclass;
		optdata = (void *)&deftclass;
		optdatalen = sizeof(int);
		break;
	case IPV6_HOPOPTS:
		GET_PKTOPT_EXT_HDR(ip6po_hbh);
		break;
	case IPV6_RTHDR:
		GET_PKTOPT_EXT_HDR(ip6po_rthdr);
		break;
	case IPV6_RTHDRDSTOPTS:
		GET_PKTOPT_EXT_HDR(ip6po_dest1);
		break;
	case IPV6_DSTOPTS:
		GET_PKTOPT_EXT_HDR(ip6po_dest2);
		break;
	case IPV6_NEXTHOP:
		GET_PKTOPT_SOCKADDR(ip6po_nexthop);
		break;
	case IPV6_USE_MIN_MTU:
		if (pktopt)
			defminmtu = pktopt->ip6po_minmtu;
		optdata = (void *)&defminmtu;
		optdatalen = sizeof(int);
		break;
	case IPV6_DONTFRAG:
		if (pktopt && ((pktopt->ip6po_flags) & IP6PO_DONTFRAG))
			on = 1;
		else
			on = 0;
		optdata = (void *)&on;
		optdatalen = sizeof(on);
		break;
	case IPV6_PREFER_TEMPADDR:
		if (pktopt)
			defpreftemp = pktopt->ip6po_prefer_tempaddr;
		optdata = (void *)&defpreftemp;
		optdatalen = sizeof(int);
		break;
	default:		/* should not happen */
#ifdef DIAGNOSTIC
		panic("ip6_getpcbopt: unexpected option\n");
#endif
		INP_RUNLOCK(inp);
		return (ENOPROTOOPT);
	}
	INP_RUNLOCK(inp);

	error = sooptcopyout(sopt, optdata, optdatalen);
	if (malloc_optdata)
		free(optdata, M_TEMP);

	return (error);
}

void
ip6_clearpktopts(struct ip6_pktopts *pktopt, int optname)
{
	if (pktopt == NULL)
		return;

	if (optname == -1 || optname == IPV6_PKTINFO) {
		if (pktopt->ip6po_pktinfo)
			free(pktopt->ip6po_pktinfo, M_IP6OPT);
		pktopt->ip6po_pktinfo = NULL;
	}
	if (optname == -1 || optname == IPV6_HOPLIMIT)
		pktopt->ip6po_hlim = -1;
	if (optname == -1 || optname == IPV6_TCLASS)
		pktopt->ip6po_tclass = -1;
	if (optname == -1 || optname == IPV6_NEXTHOP) {
		if (pktopt->ip6po_nextroute.ro_nh) {
			NH_FREE(pktopt->ip6po_nextroute.ro_nh);
			pktopt->ip6po_nextroute.ro_nh = NULL;
		}
		if (pktopt->ip6po_nexthop)
			free(pktopt->ip6po_nexthop, M_IP6OPT);
		pktopt->ip6po_nexthop = NULL;
	}
	if (optname == -1 || optname == IPV6_HOPOPTS) {
		if (pktopt->ip6po_hbh)
			free(pktopt->ip6po_hbh, M_IP6OPT);
		pktopt->ip6po_hbh = NULL;
	}
	if (optname == -1 || optname == IPV6_RTHDRDSTOPTS) {
		if (pktopt->ip6po_dest1)
			free(pktopt->ip6po_dest1, M_IP6OPT);
		pktopt->ip6po_dest1 = NULL;
	}
	if (optname == -1 || optname == IPV6_RTHDR) {
		if (pktopt->ip6po_rhinfo.ip6po_rhi_rthdr)
			free(pktopt->ip6po_rhinfo.ip6po_rhi_rthdr, M_IP6OPT);
		pktopt->ip6po_rhinfo.ip6po_rhi_rthdr = NULL;
		if (pktopt->ip6po_route.ro_nh) {
			NH_FREE(pktopt->ip6po_route.ro_nh);
			pktopt->ip6po_route.ro_nh = NULL;
		}
	}
	if (optname == -1 || optname == IPV6_DSTOPTS) {
		if (pktopt->ip6po_dest2)
			free(pktopt->ip6po_dest2, M_IP6OPT);
		pktopt->ip6po_dest2 = NULL;
	}
}

#define PKTOPT_EXTHDRCPY(type) \
do {\
	if (src->type) {\
		int hlen = (((struct ip6_ext *)src->type)->ip6e_len + 1) << 3;\
		dst->type = malloc(hlen, M_IP6OPT, canwait);\
		if (dst->type == NULL)\
			goto bad;\
		bcopy(src->type, dst->type, hlen);\
	}\
} while (/*CONSTCOND*/ 0)

static int
copypktopts(struct ip6_pktopts *dst, struct ip6_pktopts *src, int canwait)
{
	if (dst == NULL || src == NULL)  {
		printf("ip6_clearpktopts: invalid argument\n");
		return (EINVAL);
	}

	dst->ip6po_hlim = src->ip6po_hlim;
	dst->ip6po_tclass = src->ip6po_tclass;
	dst->ip6po_flags = src->ip6po_flags;
	dst->ip6po_minmtu = src->ip6po_minmtu;
	dst->ip6po_prefer_tempaddr = src->ip6po_prefer_tempaddr;
	if (src->ip6po_pktinfo) {
		dst->ip6po_pktinfo = malloc(sizeof(*dst->ip6po_pktinfo),
		    M_IP6OPT, canwait);
		if (dst->ip6po_pktinfo == NULL)
			goto bad;
		*dst->ip6po_pktinfo = *src->ip6po_pktinfo;
	}
	if (src->ip6po_nexthop) {
		dst->ip6po_nexthop = malloc(src->ip6po_nexthop->sa_len,
		    M_IP6OPT, canwait);
		if (dst->ip6po_nexthop == NULL)
			goto bad;
		bcopy(src->ip6po_nexthop, dst->ip6po_nexthop,
		    src->ip6po_nexthop->sa_len);
	}
	PKTOPT_EXTHDRCPY(ip6po_hbh);
	PKTOPT_EXTHDRCPY(ip6po_dest1);
	PKTOPT_EXTHDRCPY(ip6po_dest2);
	PKTOPT_EXTHDRCPY(ip6po_rthdr); /* not copy the cached route */
	return (0);

  bad:
	ip6_clearpktopts(dst, -1);
	return (ENOBUFS);
}
#undef PKTOPT_EXTHDRCPY

struct ip6_pktopts *
ip6_copypktopts(struct ip6_pktopts *src, int canwait)
{
	int error;
	struct ip6_pktopts *dst;

	dst = malloc(sizeof(*dst), M_IP6OPT, canwait);
	if (dst == NULL)
		return (NULL);
	ip6_initpktopts(dst);

	if ((error = copypktopts(dst, src, canwait)) != 0) {
		free(dst, M_IP6OPT);
		return (NULL);
	}

	return (dst);
}

void
ip6_freepcbopts(struct ip6_pktopts *pktopt)
{
	if (pktopt == NULL)
		return;

	ip6_clearpktopts(pktopt, -1);

	free(pktopt, M_IP6OPT);
}

/*
 * Set IPv6 outgoing packet options based on advanced API.
 */
int
ip6_setpktopts(struct mbuf *control, struct ip6_pktopts *opt,
    struct ip6_pktopts *stickyopt, struct ucred *cred, int uproto)
{
	struct cmsghdr *cm = NULL;

	if (control == NULL || opt == NULL)
		return (EINVAL);

	/*
	 * ip6_setpktopt can call ifnet_byindex(), so it's imperative that we
	 * are in the network epoch here.
	 */
	NET_EPOCH_ASSERT();

	ip6_initpktopts(opt);
	if (stickyopt) {
		int error;

		/*
		 * If stickyopt is provided, make a local copy of the options
		 * for this particular packet, then override them by ancillary
		 * objects.
		 * XXX: copypktopts() does not copy the cached route to a next
		 * hop (if any).  This is not very good in terms of efficiency,
		 * but we can allow this since this option should be rarely
		 * used.
		 */
		if ((error = copypktopts(opt, stickyopt, M_NOWAIT)) != 0)
			return (error);
	}

	/*
	 * XXX: Currently, we assume all the optional information is stored
	 * in a single mbuf.
	 */
	if (control->m_next)
		return (EINVAL);

	for (; control->m_len > 0; control->m_data += CMSG_ALIGN(cm->cmsg_len),
	    control->m_len -= CMSG_ALIGN(cm->cmsg_len)) {
		int error;

		if (control->m_len < CMSG_LEN(0))
			return (EINVAL);

		cm = mtod(control, struct cmsghdr *);
		if (cm->cmsg_len == 0 || cm->cmsg_len > control->m_len)
			return (EINVAL);
		if (cm->cmsg_level != IPPROTO_IPV6)
			continue;

		error = ip6_setpktopt(cm->cmsg_type, CMSG_DATA(cm),
		    cm->cmsg_len - CMSG_LEN(0), opt, cred, 0, 1, uproto);
		if (error)
			return (error);
	}

	return (0);
}

/*
 * Set a particular packet option, as a sticky option or an ancillary data
 * item.  "len" can be 0 only when it's a sticky option.
 * We have 4 cases of combination of "sticky" and "cmsg":
 * "sticky=0, cmsg=0": impossible
 * "sticky=0, cmsg=1": RFC2292 or RFC3542 ancillary data
 * "sticky=1, cmsg=0": RFC3542 socket option
 * "sticky=1, cmsg=1": RFC2292 socket option
 */
static int
ip6_setpktopt(int optname, u_char *buf, int len, struct ip6_pktopts *opt,
    struct ucred *cred, int sticky, int cmsg, int uproto)
{
	int minmtupolicy, preftemp;
	int error;

	NET_EPOCH_ASSERT();

	if (!sticky && !cmsg) {
#ifdef DIAGNOSTIC
		printf("ip6_setpktopt: impossible case\n");
#endif
		return (EINVAL);
	}

	/*
	 * IPV6_2292xxx is for backward compatibility to RFC2292, and should
	 * not be specified in the context of RFC3542.  Conversely,
	 * RFC3542 types should not be specified in the context of RFC2292.
	 */
	if (!cmsg) {
		switch (optname) {
		case IPV6_2292PKTINFO:
		case IPV6_2292HOPLIMIT:
		case IPV6_2292NEXTHOP:
		case IPV6_2292HOPOPTS:
		case IPV6_2292DSTOPTS:
		case IPV6_2292RTHDR:
		case IPV6_2292PKTOPTIONS:
			return (ENOPROTOOPT);
		}
	}
	if (sticky && cmsg) {
		switch (optname) {
		case IPV6_PKTINFO:
		case IPV6_HOPLIMIT:
		case IPV6_NEXTHOP:
		case IPV6_HOPOPTS:
		case IPV6_DSTOPTS:
		case IPV6_RTHDRDSTOPTS:
		case IPV6_RTHDR:
		case IPV6_USE_MIN_MTU:
		case IPV6_DONTFRAG:
		case IPV6_TCLASS:
		case IPV6_PREFER_TEMPADDR: /* XXX: not an RFC3542 option */
			return (ENOPROTOOPT);
		}
	}

	switch (optname) {
	case IPV6_2292PKTINFO:
	case IPV6_PKTINFO:
	{
		struct ifnet *ifp = NULL;
		struct in6_pktinfo *pktinfo;

		if (len != sizeof(struct in6_pktinfo))
			return (EINVAL);

		pktinfo = (struct in6_pktinfo *)buf;

		/*
		 * An application can clear any sticky IPV6_PKTINFO option by
		 * doing a "regular" setsockopt with ipi6_addr being
		 * in6addr_any and ipi6_ifindex being zero.
		 * [RFC 3542, Section 6]
		 */
		if (optname == IPV6_PKTINFO && opt->ip6po_pktinfo &&
		    pktinfo->ipi6_ifindex == 0 &&
		    IN6_IS_ADDR_UNSPECIFIED(&pktinfo->ipi6_addr)) {
			ip6_clearpktopts(opt, optname);
			break;
		}

		if (uproto == IPPROTO_TCP && optname == IPV6_PKTINFO &&
		    sticky && !IN6_IS_ADDR_UNSPECIFIED(&pktinfo->ipi6_addr)) {
			return (EINVAL);
		}
		if (IN6_IS_ADDR_MULTICAST(&pktinfo->ipi6_addr))
			return (EINVAL);
		/* validate the interface index if specified. */
		if (pktinfo->ipi6_ifindex) {
			ifp = ifnet_byindex(pktinfo->ipi6_ifindex);
			if (ifp == NULL)
				return (ENXIO);
		}
		if (ifp != NULL && (ifp->if_afdata[AF_INET6] == NULL ||
		    (ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED) != 0))
			return (ENETDOWN);

		if (ifp != NULL &&
		    !IN6_IS_ADDR_UNSPECIFIED(&pktinfo->ipi6_addr)) {
			struct in6_ifaddr *ia;

			in6_setscope(&pktinfo->ipi6_addr, ifp, NULL);
			ia = in6ifa_ifpwithaddr(ifp, &pktinfo->ipi6_addr);
			if (ia == NULL)
				return (EADDRNOTAVAIL);
			ifa_free(&ia->ia_ifa);
		}
		/*
		 * We store the address anyway, and let in6_selectsrc()
		 * validate the specified address.  This is because ipi6_addr
		 * may not have enough information about its scope zone, and
		 * we may need additional information (such as outgoing
		 * interface or the scope zone of a destination address) to
		 * disambiguate the scope.
		 * XXX: the delay of the validation may confuse the
		 * application when it is used as a sticky option.
		 */
		if (opt->ip6po_pktinfo == NULL) {
			opt->ip6po_pktinfo = malloc(sizeof(*pktinfo),
			    M_IP6OPT, M_NOWAIT);
			if (opt->ip6po_pktinfo == NULL)
				return (ENOBUFS);
		}
		bcopy(pktinfo, opt->ip6po_pktinfo, sizeof(*pktinfo));
		break;
	}

	case IPV6_2292HOPLIMIT:
	case IPV6_HOPLIMIT:
	{
		int *hlimp;

		/*
		 * RFC 3542 deprecated the usage of sticky IPV6_HOPLIMIT
		 * to simplify the ordering among hoplimit options.
		 */
		if (optname == IPV6_HOPLIMIT && sticky)
			return (ENOPROTOOPT);

		if (len != sizeof(int))
			return (EINVAL);
		hlimp = (int *)buf;
		if (*hlimp < -1 || *hlimp > 255)
			return (EINVAL);

		opt->ip6po_hlim = *hlimp;
		break;
	}

	case IPV6_TCLASS:
	{
		int tclass;

		if (len != sizeof(int))
			return (EINVAL);
		tclass = *(int *)buf;
		if (tclass < -1 || tclass > 255)
			return (EINVAL);

		opt->ip6po_tclass = tclass;
		break;
	}

	case IPV6_2292NEXTHOP:
	case IPV6_NEXTHOP:
		if (cred != NULL) {
			error = priv_check_cred(cred, PRIV_NETINET_SETHDROPTS);
			if (error)
				return (error);
		}

		if (len == 0) {	/* just remove the option */
			ip6_clearpktopts(opt, IPV6_NEXTHOP);
			break;
		}

		/* check if cmsg_len is large enough for sa_len */
		if (len < sizeof(struct sockaddr) || len < *buf)
			return (EINVAL);

		switch (((struct sockaddr *)buf)->sa_family) {
		case AF_INET6:
		{
			struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)buf;
			int error;

			if (sa6->sin6_len != sizeof(struct sockaddr_in6))
				return (EINVAL);

			if (IN6_IS_ADDR_UNSPECIFIED(&sa6->sin6_addr) ||
			    IN6_IS_ADDR_MULTICAST(&sa6->sin6_addr)) {
				return (EINVAL);
			}
			if ((error = sa6_embedscope(sa6, V_ip6_use_defzone))
			    != 0) {
				return (error);
			}
			break;
		}
		case AF_LINK:	/* should eventually be supported */
		default:
			return (EAFNOSUPPORT);
		}

		/* turn off the previous option, then set the new option. */
		ip6_clearpktopts(opt, IPV6_NEXTHOP);
		opt->ip6po_nexthop = malloc(*buf, M_IP6OPT, M_NOWAIT);
		if (opt->ip6po_nexthop == NULL)
			return (ENOBUFS);
		bcopy(buf, opt->ip6po_nexthop, *buf);
		break;

	case IPV6_2292HOPOPTS:
	case IPV6_HOPOPTS:
	{
		struct ip6_hbh *hbh;
		int hbhlen;

		/*
		 * XXX: We don't allow a non-privileged user to set ANY HbH
		 * options, since per-option restriction has too much
		 * overhead.
		 */
		if (cred != NULL) {
			error = priv_check_cred(cred, PRIV_NETINET_SETHDROPTS);
			if (error)
				return (error);
		}

		if (len == 0) {
			ip6_clearpktopts(opt, IPV6_HOPOPTS);
			break;	/* just remove the option */
		}

		/* message length validation */
		if (len < sizeof(struct ip6_hbh))
			return (EINVAL);
		hbh = (struct ip6_hbh *)buf;
		hbhlen = (hbh->ip6h_len + 1) << 3;
		if (len != hbhlen)
			return (EINVAL);

		/* turn off the previous option, then set the new option. */
		ip6_clearpktopts(opt, IPV6_HOPOPTS);
		opt->ip6po_hbh = malloc(hbhlen, M_IP6OPT, M_NOWAIT);
		if (opt->ip6po_hbh == NULL)
			return (ENOBUFS);
		bcopy(hbh, opt->ip6po_hbh, hbhlen);

		break;
	}

	case IPV6_2292DSTOPTS:
	case IPV6_DSTOPTS:
	case IPV6_RTHDRDSTOPTS:
	{
		struct ip6_dest *dest, **newdest = NULL;
		int destlen;

		if (cred != NULL) { /* XXX: see the comment for IPV6_HOPOPTS */
			error = priv_check_cred(cred, PRIV_NETINET_SETHDROPTS);
			if (error)
				return (error);
		}

		if (len == 0) {
			ip6_clearpktopts(opt, optname);
			break;	/* just remove the option */
		}

		/* message length validation */
		if (len < sizeof(struct ip6_dest))
			return (EINVAL);
		dest = (struct ip6_dest *)buf;
		destlen = (dest->ip6d_len + 1) << 3;
		if (len != destlen)
			return (EINVAL);

		/*
		 * Determine the position that the destination options header
		 * should be inserted; before or after the routing header.
		 */
		switch (optname) {
		case IPV6_2292DSTOPTS:
			/*
			 * The old advacned API is ambiguous on this point.
			 * Our approach is to determine the position based
			 * according to the existence of a routing header.
			 * Note, however, that this depends on the order of the
			 * extension headers in the ancillary data; the 1st
			 * part of the destination options header must appear
			 * before the routing header in the ancillary data,
			 * too.
			 * RFC3542 solved the ambiguity by introducing
			 * separate ancillary data or option types.
			 */
			if (opt->ip6po_rthdr == NULL)
				newdest = &opt->ip6po_dest1;
			else
				newdest = &opt->ip6po_dest2;
			break;
		case IPV6_RTHDRDSTOPTS:
			newdest = &opt->ip6po_dest1;
			break;
		case IPV6_DSTOPTS:
			newdest = &opt->ip6po_dest2;
			break;
		}

		/* turn off the previous option, then set the new option. */
		ip6_clearpktopts(opt, optname);
		*newdest = malloc(destlen, M_IP6OPT, M_NOWAIT);
		if (*newdest == NULL)
			return (ENOBUFS);
		bcopy(dest, *newdest, destlen);

		break;
	}

	case IPV6_2292RTHDR:
	case IPV6_RTHDR:
	{
		struct ip6_rthdr *rth;
		int rthlen;

		if (len == 0) {
			ip6_clearpktopts(opt, IPV6_RTHDR);
			break;	/* just remove the option */
		}

		/* message length validation */
		if (len < sizeof(struct ip6_rthdr))
			return (EINVAL);
		rth = (struct ip6_rthdr *)buf;
		rthlen = (rth->ip6r_len + 1) << 3;
		if (len != rthlen)
			return (EINVAL);

		switch (rth->ip6r_type) {
		case IPV6_RTHDR_TYPE_0:
			if (rth->ip6r_len == 0)	/* must contain one addr */
				return (EINVAL);
			if (rth->ip6r_len % 2) /* length must be even */
				return (EINVAL);
			if (rth->ip6r_len / 2 != rth->ip6r_segleft)
				return (EINVAL);
			break;
		default:
			return (EINVAL);	/* not supported */
		}

		/* turn off the previous option */
		ip6_clearpktopts(opt, IPV6_RTHDR);
		opt->ip6po_rthdr = malloc(rthlen, M_IP6OPT, M_NOWAIT);
		if (opt->ip6po_rthdr == NULL)
			return (ENOBUFS);
		bcopy(rth, opt->ip6po_rthdr, rthlen);

		break;
	}

	case IPV6_USE_MIN_MTU:
		if (len != sizeof(int))
			return (EINVAL);
		minmtupolicy = *(int *)buf;
		if (minmtupolicy != IP6PO_MINMTU_MCASTONLY &&
		    minmtupolicy != IP6PO_MINMTU_DISABLE &&
		    minmtupolicy != IP6PO_MINMTU_ALL) {
			return (EINVAL);
		}
		opt->ip6po_minmtu = minmtupolicy;
		break;

	case IPV6_DONTFRAG:
		if (len != sizeof(int))
			return (EINVAL);

		if (uproto == IPPROTO_TCP || *(int *)buf == 0) {
			/*
			 * we ignore this option for TCP sockets.
			 * (RFC3542 leaves this case unspecified.)
			 */
			opt->ip6po_flags &= ~IP6PO_DONTFRAG;
		} else
			opt->ip6po_flags |= IP6PO_DONTFRAG;
		break;

	case IPV6_PREFER_TEMPADDR:
		if (len != sizeof(int))
			return (EINVAL);
		preftemp = *(int *)buf;
		if (preftemp != IP6PO_TEMPADDR_SYSTEM &&
		    preftemp != IP6PO_TEMPADDR_NOTPREFER &&
		    preftemp != IP6PO_TEMPADDR_PREFER) {
			return (EINVAL);
		}
		opt->ip6po_prefer_tempaddr = preftemp;
		break;

	default:
		return (ENOPROTOOPT);
	} /* end of switch */

	return (0);
}

/*
 * Routine called from ip6_output() to loop back a copy of an IP6 multicast
 * packet to the input queue of a specified interface.  Note that this
 * calls the output routine of the loopback "driver", but with an interface
 * pointer that might NOT be &loif -- easier than replicating that code here.
 */
void
ip6_mloopback(struct ifnet *ifp, struct mbuf *m)
{
	struct mbuf *copym;
	struct ip6_hdr *ip6;

	copym = m_copym(m, 0, M_COPYALL, M_NOWAIT);
	if (copym == NULL)
		return;

	/*
	 * Make sure to deep-copy IPv6 header portion in case the data
	 * is in an mbuf cluster, so that we can safely override the IPv6
	 * header portion later.
	 */
	if (!M_WRITABLE(copym) ||
	    copym->m_len < sizeof(struct ip6_hdr)) {
		copym = m_pullup(copym, sizeof(struct ip6_hdr));
		if (copym == NULL)
			return;
	}
	ip6 = mtod(copym, struct ip6_hdr *);
	/*
	 * clear embedded scope identifiers if necessary.
	 * in6_clearscope will touch the addresses only when necessary.
	 */
	in6_clearscope(&ip6->ip6_src);
	in6_clearscope(&ip6->ip6_dst);
	if (copym->m_pkthdr.csum_flags & CSUM_DELAY_DATA_IPV6) {
		copym->m_pkthdr.csum_flags |= CSUM_DATA_VALID_IPV6 |
		    CSUM_PSEUDO_HDR;
		copym->m_pkthdr.csum_data = 0xffff;
	}
	if_simloop(ifp, copym, AF_INET6, 0);
}

/*
 * Chop IPv6 header off from the payload.
 */
static int
ip6_splithdr(struct mbuf *m, struct ip6_exthdrs *exthdrs)
{
	struct mbuf *mh;
	struct ip6_hdr *ip6;

	ip6 = mtod(m, struct ip6_hdr *);
	if (m->m_len > sizeof(*ip6)) {
		mh = m_gethdr(M_NOWAIT, MT_DATA);
		if (mh == NULL) {
			m_freem(m);
			return ENOBUFS;
		}
		m_move_pkthdr(mh, m);
		M_ALIGN(mh, sizeof(*ip6));
		m->m_len -= sizeof(*ip6);
		m->m_data += sizeof(*ip6);
		mh->m_next = m;
		m = mh;
		m->m_len = sizeof(*ip6);
		bcopy((caddr_t)ip6, mtod(m, caddr_t), sizeof(*ip6));
	}
	exthdrs->ip6e_ip6 = m;
	return 0;
}

/*
 * Compute IPv6 extension header length.
 */
int
ip6_optlen(struct inpcb *inp)
{
	int len;

	if (!inp->in6p_outputopts)
		return 0;

	len = 0;
#define elen(x) \
    (((struct ip6_ext *)(x)) ? (((struct ip6_ext *)(x))->ip6e_len + 1) << 3 : 0)

	len += elen(inp->in6p_outputopts->ip6po_hbh);
	if (inp->in6p_outputopts->ip6po_rthdr)
		/* dest1 is valid with rthdr only */
		len += elen(inp->in6p_outputopts->ip6po_dest1);
	len += elen(inp->in6p_outputopts->ip6po_rthdr);
	len += elen(inp->in6p_outputopts->ip6po_dest2);
	return len;
#undef elen
}
