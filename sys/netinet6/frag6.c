/*
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
 * $FreeBSD: src/sys/netinet6/frag6.c,v 1.2 1999/12/09 08:56:49 shin Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/icmp6.h>

#include <net/net_osdep.h>

/*
 * Define it to get a correct behavior on per-interface statistics.
 * You will need to perform an extra routing table lookup, per fragment,
 * to do it.  This may, or may not be, a performance hit.
 */
#define	IN6_IFSTAT_STRICT

static void	frag6_enq __P((struct ip6asfrag *, struct ip6asfrag *));
static void	frag6_deq __P((struct ip6asfrag *));
static void	frag6_insque __P((struct ip6q *, struct ip6q *));
static void	frag6_remque __P((struct ip6q *));
static void	frag6_freef __P((struct ip6q *));

int	frag6_doing_reass;
u_int	frag6_nfragpackets;
struct	ip6q	ip6q;	/* ip6 reassemble queue */

#if !defined(M_FTABLE)
MALLOC_DEFINE(M_FTABLE, "fragment", "fragment reassembly header");
#endif

/*
 * Initialise reassembly queue and fragment identifier.
 */
void
frag6_init()
{
	struct timeval tv;

	/*
	 * in many cases, random() here does NOT return random number
	 * as initialization during bootstrap time occur in fixed order.
	 */
	microtime(&tv);
	ip6q.ip6q_next = ip6q.ip6q_prev = &ip6q;
	ip6_id = random() ^ tv.tv_usec;
}

/*
 * Fragment input
 */
int
frag6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp, *t;
	struct ip6_hdr *ip6;
	struct ip6_frag *ip6f;
	struct ip6q *q6;
	struct ip6asfrag *af6, *ip6af;
	int offset = *offp, nxt, i, next;
	int first_frag = 0;
	u_short fragoff, frgpartlen;
	struct ifnet *dstifp;
#ifdef IN6_IFSTAT_STRICT
	static struct route_in6 ro;
	struct sockaddr_in6 *dst;
#endif

	IP6_EXTHDR_CHECK(m, offset, sizeof(struct ip6_frag), IPPROTO_DONE);

	ip6 = mtod(m, struct ip6_hdr *);
	ip6f = (struct ip6_frag *)((caddr_t)ip6 + offset);

	dstifp = NULL;
#ifdef IN6_IFSTAT_STRICT
	/* find the destination interface of the packet. */
	dst = (struct sockaddr_in6 *)&ro.ro_dst;
	if (ro.ro_rt
	 && ((ro.ro_rt->rt_flags & RTF_UP) == 0
	  || !IN6_ARE_ADDR_EQUAL(&dst->sin6_addr, &ip6->ip6_dst))) {
		RTFREE(ro.ro_rt);
		ro.ro_rt = (struct rtentry *)0;
	}
	if (ro.ro_rt == NULL) {
		bzero(dst, sizeof(*dst));
		dst->sin6_family = AF_INET6;
		dst->sin6_len = sizeof(struct sockaddr_in6);
		dst->sin6_addr = ip6->ip6_dst;
	}
	rtalloc((struct route *)&ro);
	if (ro.ro_rt != NULL && ro.ro_rt->rt_ifa != NULL)
		dstifp = ((struct in6_ifaddr *)ro.ro_rt->rt_ifa)->ia_ifp;
#else
	/* we are violating the spec, this is not the destination interface */
	if ((m->m_flags & M_PKTHDR) != 0)
		dstifp = m->m_pkthdr.rcvif;
#endif

	/* jumbo payload can't contain a fragment header */
	if (ip6->ip6_plen == 0) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER, offset);
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		return IPPROTO_DONE;
	}

	/*
	 * check whether fragment packet's fragment length is
	 * multiple of 8 octets.
	 * sizeof(struct ip6_frag) == 8
	 * sizeof(struct ip6_hdr) = 40
	 */
	if ((ip6f->ip6f_offlg & IP6F_MORE_FRAG) &&
	    (((ntohs(ip6->ip6_plen) - offset) & 0x7) != 0)) {
		icmp6_error(m, ICMP6_PARAM_PROB,
			    ICMP6_PARAMPROB_HEADER,
			    (caddr_t)&ip6->ip6_plen - (caddr_t)ip6);
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		return IPPROTO_DONE;
	}

	ip6stat.ip6s_fragments++;
	in6_ifstat_inc(dstifp, ifs6_reass_reqd);
	
	/*
	 * Presence of header sizes in mbufs
	 * would confuse code below.
	 */
	
	offset += sizeof(struct ip6_frag);
	m->m_data += offset;
	m->m_len -= offset;

	for (q6 = ip6q.ip6q_next; q6 != &ip6q; q6 = q6->ip6q_next)
		if (ip6f->ip6f_ident == q6->ip6q_ident &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &q6->ip6q_src) &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &q6->ip6q_dst))
			break;

	if (q6 == &ip6q) {
		/*
		 * the first fragment to arrive, create a reassembly queue.
		 */
		first_frag = 1;
		frag6_nfragpackets++;

		/*
		 * Enforce upper bound on number of fragmented packets
		 * for which we attempt reassembly;
		 * If maxfrag is 0, never accept fragments.
		 * If maxfrag is -1, accept all fragments without limitation.
		 */
		if (frag6_nfragpackets >= (u_int)ip6_maxfragpackets) {
			ip6stat.ip6s_fragoverflow++;
			in6_ifstat_inc(dstifp, ifs6_reass_fail);
			frag6_freef(ip6q.ip6q_prev);
		}
		q6 = (struct ip6q *)malloc(sizeof(struct ip6q), M_FTABLE,
			M_DONTWAIT);
		if (q6 == NULL)
			goto dropfrag;

		frag6_insque(q6, &ip6q);

		q6->ip6q_down	= q6->ip6q_up = (struct ip6asfrag *)q6;
		q6->ip6q_ident	= ip6f->ip6f_ident;
		q6->ip6q_arrive = 0; /* Is it used anywhere? */
		q6->ip6q_ttl 	= IPV6_FRAGTTL;
		q6->ip6q_src	= ip6->ip6_src;
		q6->ip6q_dst	= ip6->ip6_dst;
		q6->ip6q_unfrglen = -1;	/* The 1st fragment has not arrived. */
	}

	/*
	 * If it's the 1st fragment, record the length of the
	 * unfragmentable part and the next header of the fragment header.
	 */
	fragoff = ntohs(ip6f->ip6f_offlg & IP6F_OFF_MASK);
	if (fragoff == 0) {
		q6->ip6q_unfrglen = offset - sizeof(struct ip6_hdr)
			- sizeof(struct ip6_frag);
		q6->ip6q_nxt = ip6f->ip6f_nxt;
	}

	/*
	 * Check that the reassembled packet would not exceed 65535 bytes
	 * in size.
	 * If it would exceed, discard the fragment and return an ICMP error.
	 */
	frgpartlen =  sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen) - offset;
	if (q6->ip6q_unfrglen >= 0) {
		/* The 1st fragment has already arrived. */
		if (q6->ip6q_unfrglen + fragoff + frgpartlen > IPV6_MAXPACKET) {
			m->m_data -= offset;
			m->m_len += offset;
			icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
				    offset - sizeof(struct ip6_frag) + 2);
			return(IPPROTO_DONE);
		}
	}
	else if (fragoff + frgpartlen > IPV6_MAXPACKET) {
		m->m_data -= offset;
		m->m_len += offset;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    offset - sizeof(struct ip6_frag) + 2);
		return(IPPROTO_DONE);
	}
	/*
	 * If it's the first fragment, do the above check for each
	 * fragment already stored in the reassembly queue.
	 */
	if (fragoff == 0) {
		struct ip6asfrag *af6dwn;
		
		for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
		     af6 = af6dwn) {
			af6dwn = af6->ip6af_down;

			if (q6->ip6q_unfrglen + af6->ip6af_off + af6->ip6af_frglen >
			    IPV6_MAXPACKET) {
				struct mbuf *merr = IP6_REASS_MBUF(af6);
				struct ip6_hdr *ip6err;
				int erroff = af6->ip6af_offset;

				/* dequeue the fragment. */
				frag6_deq(af6);

				/* adjust pointer. */
				merr->m_data -= af6->ip6af_offset;
				merr->m_len += af6->ip6af_offset;
				ip6err = mtod(merr, struct ip6_hdr *);

				/*
				 * Restore source and destination addresses
				 * in the erroneous IPv6 header.
				 */
				ip6err->ip6_src = q6->ip6q_src;
				ip6err->ip6_dst = q6->ip6q_dst;

				icmp6_error(merr, ICMP6_PARAM_PROB,
					    ICMP6_PARAMPROB_HEADER,
					    erroff - sizeof(struct ip6_frag) + 2);
			}
		}
	}

	/* Override the IPv6 header */
	ip6af = (struct ip6asfrag *)ip6;
	ip6af->ip6af_mff = ip6f->ip6f_offlg & IP6F_MORE_FRAG;
	ip6af->ip6af_off = fragoff;
	ip6af->ip6af_frglen = frgpartlen;
	ip6af->ip6af_offset = offset;
	IP6_REASS_MBUF(ip6af) = m;

	if (first_frag) {
		af6 = (struct ip6asfrag *)q6;
		goto insert;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	     af6 = af6->ip6af_down)
		if (af6->ip6af_off > ip6af->ip6af_off)
			break;

	/*
	 * If the incoming framgent overlaps some existing fragments in
	 * the reassembly queue, drop it, since it is dangerous to override
	 * existing fragments from a security point of view.
	 */
	if (af6->ip6af_up != (struct ip6asfrag *)q6) {
		i = af6->ip6af_up->ip6af_off + af6->ip6af_up->ip6af_frglen
			- ip6af->ip6af_off;
		if (i > 0) {
			log(LOG_ERR, "%d bytes of a fragment from %s "
			    "overlaps the previous fragment\n",
			    i, ip6_sprintf(&q6->ip6q_src));
			goto dropfrag;
		}
	}
	if (af6 != (struct ip6asfrag *)q6) {
		i = (ip6af->ip6af_off + ip6af->ip6af_frglen) - af6->ip6af_off;
		if (i > 0) {
			log(LOG_ERR, "%d bytes of a fragment from %s "
			    "overlaps the succeeding fragment",
			    i, ip6_sprintf(&q6->ip6q_src));
			goto dropfrag;
		}
	}

insert:

	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 * Move to front of packet queue, as we are
	 * the most recently active fragmented packet.
	 */
	frag6_enq(ip6af, af6->ip6af_up);
	next = 0;
	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	     af6 = af6->ip6af_down) {
		if (af6->ip6af_off != next) {
			frag6_doing_reass = 0;
			return IPPROTO_DONE;
		}
		next += af6->ip6af_frglen;
	}
	if (af6->ip6af_up->ip6af_mff) {
		frag6_doing_reass = 0;
		return IPPROTO_DONE;
	}

	/*
	 * Reassembly is complete; concatenate fragments.
	 */

	ip6af = q6->ip6q_down;
	t = m = IP6_REASS_MBUF(ip6af);
	af6 = ip6af->ip6af_down;
	while (af6 != (struct ip6asfrag *)q6) {
		while (t->m_next)
			t = t->m_next;
		t->m_next = IP6_REASS_MBUF(af6);
		af6 = af6->ip6af_down;
	}

	/* adjust offset to point where the original next header starts */
	offset = ip6af->ip6af_offset - sizeof(struct ip6_frag);
	ip6 = (struct ip6_hdr *)ip6af;
	ip6->ip6_plen = htons((u_short)next + offset - sizeof(struct ip6_hdr));
	ip6->ip6_src = q6->ip6q_src;
	ip6->ip6_dst = q6->ip6q_dst;
	nxt = q6->ip6q_nxt;

	/*
	 * Delete frag6 header with as a few cost as possible.
	 */

	if (offset < m->m_len)
		ovbcopy((caddr_t)ip6, (caddr_t)ip6 + sizeof(struct ip6_frag),
			offset);
	else {
		ovbcopy(mtod(m, caddr_t), (caddr_t)ip6 + offset, m->m_len);
		m->m_data -= sizeof(struct ip6_frag);
	}
	m->m_data -= offset;
	m->m_len += offset;

	/*
	 * Store NXT to the original.
	 */
	{
		char *prvnxtp = ip6_get_prevhdr(m, offset); /* XXX */
		*prvnxtp = nxt;
	}

	frag6_remque(q6);
	free(q6, M_FTABLE);
	frag6_nfragpackets--;

	if (m->m_flags & M_PKTHDR) { /* Isn't it always true? */
		int plen = 0;
		for (t = m; t; t = t->m_next)
			plen += t->m_len;
		m->m_pkthdr.len = plen;
	}
	
	ip6stat.ip6s_reassembled++;
	in6_ifstat_inc(dstifp, ifs6_reass_ok);

	/*
	 * Tell launch routine the next header
	 */

	*mp = m;
	*offp = offset;

	frag6_doing_reass = 0;
	return nxt;

 dropfrag:
	in6_ifstat_inc(dstifp, ifs6_reass_fail);
	ip6stat.ip6s_fragdropped++;
	m_freem(m);
	return IPPROTO_DONE;
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
void
frag6_freef(q6)
	struct ip6q *q6;
{
	struct ip6asfrag *af6, *down6;

	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	     af6 = down6) {
		struct mbuf *m = IP6_REASS_MBUF(af6);

		down6 = af6->ip6af_down;
		frag6_deq(af6);

		/*
		 * Return ICMP time exceeded error for the 1st fragment.
		 * Just free other fragments.
		 */
		if (af6->ip6af_off == 0) {
			struct ip6_hdr *ip6;

			/* adjust pointer */
			m->m_data -= af6->ip6af_offset;
			m->m_len += af6->ip6af_offset;
			ip6 = mtod(m, struct ip6_hdr *);

			/* restoure source and destination addresses */
			ip6->ip6_src = q6->ip6q_src;
			ip6->ip6_dst = q6->ip6q_dst;

			icmp6_error(m, ICMP6_TIME_EXCEEDED,
				    ICMP6_TIME_EXCEED_REASSEMBLY, 0);
		}
		else
			m_freem(m);
	}
	frag6_remque(q6);
	free(q6, M_FTABLE);
	frag6_nfragpackets--;
}

/*
 * Put an ip fragment on a reassembly chain.
 * Like insque, but pointers in middle of structure.
 */
void
frag6_enq(af6, up6)
	struct ip6asfrag *af6, *up6;
{
	af6->ip6af_up = up6;
	af6->ip6af_down = up6->ip6af_down;
	up6->ip6af_down->ip6af_up = af6;
	up6->ip6af_down = af6;
}

/*
 * To frag6_enq as remque is to insque.
 */
void
frag6_deq(af6)
	struct ip6asfrag *af6;
{
	af6->ip6af_up->ip6af_down = af6->ip6af_down;
	af6->ip6af_down->ip6af_up = af6->ip6af_up;
}

void
frag6_insque(new, old)
	struct ip6q *new, *old;
{
	new->ip6q_prev = old;
	new->ip6q_next = old->ip6q_next;
	old->ip6q_next->ip6q_prev= new;
	old->ip6q_next = new;
}

void
frag6_remque(p6)
	struct ip6q *p6;
{
	p6->ip6q_prev->ip6q_next = p6->ip6q_next;
	p6->ip6q_next->ip6q_prev = p6->ip6q_prev;
}

/*
 * IP timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
frag6_slowtimo()
{
	struct ip6q *q6;
	int s = splnet();

	frag6_doing_reass = 1;
	q6 = ip6q.ip6q_next;
	if (q6)
		while (q6 != &ip6q) {
			--q6->ip6q_ttl;
			q6 = q6->ip6q_next;
			if (q6->ip6q_prev->ip6q_ttl == 0) {
				ip6stat.ip6s_fragtimeout++;
				/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
				frag6_freef(q6->ip6q_prev);
			}
		}
	/*
	 * If we are over the maximum number of fragments
	 * (due to the limit being lowered), drain off
	 * enough to get down to the new limit.
	 */
	while (frag6_nfragpackets > (u_int)ip6_maxfragpackets) {
		ip6stat.ip6s_fragoverflow++;
		/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
		frag6_freef(ip6q.ip6q_prev);
	}
	frag6_doing_reass = 0;
	splx(s);
}

/*
 * Drain off all datagram fragments.
 */
void
frag6_drain()
{
	if (frag6_doing_reass)
		return;
	while (ip6q.ip6q_next != &ip6q) {
		ip6stat.ip6s_fragdropped++;
		/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
		frag6_freef(ip6q.ip6q_next);
	}
}
