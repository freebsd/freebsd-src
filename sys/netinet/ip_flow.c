/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the 3am Software Foundry ("3am").  It was developed by Matt Thomas.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_flow.h>

#define	IPFLOW_TIMER		(5 * PR_SLOWHZ)
#define IPFLOW_HASHBITS		6	/* should not be a multiple of 8 */
#define	IPFLOW_HASHSIZE		(1 << IPFLOW_HASHBITS)
#if IPFLOW_HASHSIZE > 255
#error "make ipf_hash larger"
#endif
static struct ipflow_head ipflows[IPFLOW_HASHSIZE];
static int ipflow_inuse;
#define	IPFLOW_MAX		256

/*
 * Each flow list has a lock that guards updates to the list and to
 * all entries on the list.  Flow entries hold the hash index for
 * finding the head of the list so the lock can be found quickly.
 *
 * ipflow_inuse holds a count of the number of flow entries present.
 * This is used to bound the size of the table.  When IPFLOW_MAX entries
 * are present and an additional entry is needed one is chosen for
 * replacement.  We could use atomic ops for this counter but having it
 * inconsistent doesn't appear to be a problem.
 */
#define	IPFLOW_HEAD_LOCK(_ipfh)		mtx_lock(&(_ipfh)->ipfh_mtx)
#define	IPFLOW_HEAD_UNLOCK(_ipfh)	mtx_unlock(&(_ipfh)->ipfh_mtx)
#define	IPFLOW_LOCK(_ipf) \
	IPFLOW_HEAD_LOCK(&ipflows[(_ipf)->ipf_hash])
#define	IPFLOW_UNLOCK(_ipf) \
	IPFLOW_HEAD_UNLOCK(&ipflows[(_ipf)->ipf_hash])

static int ipflow_active = 0;
SYSCTL_INT(_net_inet_ip, IPCTL_FASTFORWARDING, fastforwarding, CTLFLAG_RW,
    &ipflow_active, 0, "Enable flow-based IP forwarding");

static MALLOC_DEFINE(M_IPFLOW, "ip_flow", "IP flow");

static unsigned
ipflow_hash(struct in_addr dst, struct in_addr src, unsigned tos)
{
	unsigned hash = tos;
	int idx;
	for (idx = 0; idx < 32; idx += IPFLOW_HASHBITS)
		hash += (dst.s_addr >> (32 - idx)) + (src.s_addr >> idx);
	return hash & (IPFLOW_HASHSIZE-1);
}

static struct ipflow *
ipflow_lookup(const struct ip *ip)
{
	unsigned hash;
	struct ipflow_head *head;
	struct ipflow *ipf;

	hash = ipflow_hash(ip->ip_dst, ip->ip_src, ip->ip_tos);
	head = &ipflows[hash];

	IPFLOW_HEAD_LOCK(head);
	LIST_FOREACH(ipf, &head->ipfh_head, ipf_next) {
		if (ip->ip_dst.s_addr == ipf->ipf_dst.s_addr
		    && ip->ip_src.s_addr == ipf->ipf_src.s_addr
		    && ip->ip_tos == ipf->ipf_tos) {
			/* NB: return head locked */
			return ipf;
		}
	}
	IPFLOW_HEAD_UNLOCK(head);
	return NULL;
}

int
ipflow_fastforward(struct mbuf *m)
{
	struct ip *ip;
	struct ipflow *ipf;
	struct rtentry *rt;
	struct sockaddr *dst;
	int error;

	/*
	 * Are we forwarding packets?  Big enough for an IP packet?
	 */
	if (!ipforwarding || !ipflow_active || m->m_len < sizeof(struct ip))
		return 0;
	/*
	 * IP header with no option and valid version and length
	 */
	ip = mtod(m, struct ip *);
	if (ip->ip_v != IPVERSION || ip->ip_hl != (sizeof(struct ip) >> 2)
	    || ntohs(ip->ip_len) > m->m_pkthdr.len)
		return 0;
	/*
	 * Find a flow.
	 */
	if ((ipf = ipflow_lookup(ip)) == NULL)
		return 0;

	/*
	 * Route and interface still up?
	 */
	rt = ipf->ipf_ro.ro_rt;
	if ((rt->rt_flags & RTF_UP) == 0 || (rt->rt_ifp->if_flags & IFF_UP) == 0) {
		IPFLOW_UNLOCK(ipf);
		return 0;
	}

	/*
	 * Packet size OK?  TTL?
	 */
	if (m->m_pkthdr.len > rt->rt_ifp->if_mtu || ip->ip_ttl <= IPTTLDEC) {
		IPFLOW_UNLOCK(ipf);
		return 0;
	}

	/*
	 * Everything checks out and so we can forward this packet.
	 * Modify the TTL and incrementally change the checksum.
	 */
	ip->ip_ttl -= IPTTLDEC;
	if (ip->ip_sum >= htons(0xffff - (IPTTLDEC << 8))) {
		ip->ip_sum += htons(IPTTLDEC << 8) + 1;
	} else {
		ip->ip_sum += htons(IPTTLDEC << 8);
	}

	/*
	 * Send the packet on its way.  All we can get back is ENOBUFS
	 */
	ipf->ipf_uses++;
	ipf->ipf_timer = IPFLOW_TIMER;

	if (rt->rt_flags & RTF_GATEWAY)
		dst = rt->rt_gateway;
	else
		dst = &ipf->ipf_ro.ro_dst;
	if ((error = (*rt->rt_ifp->if_output)(rt->rt_ifp, m, dst, rt)) != 0) {
		if (error == ENOBUFS)
			ipf->ipf_dropped++;
		else
			ipf->ipf_errors++;
	}
	IPFLOW_UNLOCK(ipf);
	return 1;
}

static void
ipflow_addstats(struct ipflow *ipf)
{
	ipf->ipf_ro.ro_rt->rt_use += ipf->ipf_uses;
	ipstat.ips_cantforward += ipf->ipf_errors + ipf->ipf_dropped;
	ipstat.ips_forward += ipf->ipf_uses;
	ipstat.ips_fastforward += ipf->ipf_uses;
}

/*
 * XXX the locking here makes reaping an entry very expensive...
 */
static struct ipflow *
ipflow_reap(void)
{
	struct ipflow *victim = NULL;
	struct ipflow *ipf;
	int idx;

	for (idx = 0; idx < IPFLOW_HASHSIZE; idx++) {
		struct ipflow_head *head = &ipflows[idx];

		IPFLOW_HEAD_LOCK(head);
		LIST_FOREACH(ipf, &head->ipfh_head, ipf_next) {
			/*
			 * If this no longer points to a valid route
			 * reclaim it.
			 */
			if ((ipf->ipf_ro.ro_rt->rt_flags & RTF_UP) == 0)
				goto done;
			/*
			 * choose the one that's been least recently used
			 * or has had the least uses in the last 1.5 
			 * intervals.
			 */
			if (victim == NULL)
				victim = ipf;
			else if (ipf->ipf_timer < victim->ipf_timer
			    || (ipf->ipf_timer == victim->ipf_timer
				&& ipf->ipf_last_uses + ipf->ipf_uses <
				    victim->ipf_last_uses + victim->ipf_uses)) {
				if (victim->ipf_hash != ipf->ipf_hash)
					IPFLOW_UNLOCK(victim);
				victim = ipf;
			}
		}
		if (victim && victim->ipf_hash != idx)
			IPFLOW_HEAD_UNLOCK(head);
	}
	ipf = victim;
    done:
	/*
	 * Remove the entry from the flow table.
	 */
	LIST_REMOVE(ipf, ipf_next);
	IPFLOW_UNLOCK(ipf);

	ipflow_addstats(ipf);
	RTFREE(ipf->ipf_ro.ro_rt);
	return ipf;
}

static void
ipflow_free(struct ipflow *ipf)
{
	/*
	 * Remove the flow from the hash table.
	 */
	LIST_REMOVE(ipf, ipf_next);

	ipflow_addstats(ipf);
	RTFREE(ipf->ipf_ro.ro_rt);
	ipflow_inuse--;
	free(ipf, M_IPFLOW);
}

void
ipflow_slowtimo(void)
{
	struct ipflow *ipf;
	int idx;

	for (idx = 0; idx < IPFLOW_HASHSIZE; idx++) {
		struct ipflow_head *head = &ipflows[idx];

		IPFLOW_HEAD_LOCK(head);
		ipf = LIST_FIRST(&head->ipfh_head);
		while (ipf != NULL) {
			struct ipflow *next_ipf = LIST_NEXT(ipf, ipf_next);
			if (--ipf->ipf_timer == 0) {
				ipflow_free(ipf);
			} else {
				ipf->ipf_last_uses = ipf->ipf_uses;
				ipf->ipf_ro.ro_rt->rt_use += ipf->ipf_uses;
				ipstat.ips_forward += ipf->ipf_uses;
				ipstat.ips_fastforward += ipf->ipf_uses;
				ipf->ipf_uses = 0;
			}
			ipf = next_ipf;
		}
		IPFLOW_HEAD_UNLOCK(head);
	}
}

void
ipflow_create(const struct route *ro, struct mbuf *m)
{
	const struct ip *const ip = mtod(m, struct ip *);
	struct ipflow *ipf;

	/*
	 * Don't create cache entries for ICMP messages.
	 */
	if (!ipflow_active || ip->ip_p == IPPROTO_ICMP)
		return;
	/*
	 * See if an existing flow struct exists.  If so remove it from it's
	 * list and free the old route.  If not, try to malloc a new one
	 * (if we aren't at our limit).
	 */
	ipf = ipflow_lookup(ip);
	if (ipf == NULL) {
		if (ipflow_inuse == IPFLOW_MAX) {
			ipf = ipflow_reap();
		} else {
			ipf = (struct ipflow *) malloc(sizeof(*ipf), M_IPFLOW,
						       M_NOWAIT);
			if (ipf == NULL)
				return;
			ipflow_inuse++;
		}
		bzero((caddr_t) ipf, sizeof(*ipf));

		ipf->ipf_hash = ipflow_hash(ip->ip_dst, ip->ip_src, ip->ip_tos);
		ipf->ipf_dst = ip->ip_dst;
		ipf->ipf_src = ip->ip_src;
		ipf->ipf_tos = ip->ip_tos;

		IPFLOW_LOCK(ipf);
	} else {
		LIST_REMOVE(ipf, ipf_next);

		ipflow_addstats(ipf);		/* add stats to old route */
		RTFREE(ipf->ipf_ro.ro_rt);	/* clear reference */
		ipf->ipf_uses = ipf->ipf_last_uses = 0;
		ipf->ipf_errors = ipf->ipf_dropped = 0;
	}

	/*
	 * Fill in the updated information.
	 */
	ipf->ipf_ro = *ro;
	ro->ro_rt->rt_refcnt++;
	ipf->ipf_timer = IPFLOW_TIMER;
	/*
	 * Insert into the approriate bucket of the flow table.
	 */
	LIST_INSERT_HEAD(&ipflows[ipf->ipf_hash].ipfh_head, ipf, ipf_next);
	IPFLOW_UNLOCK(ipf);
}

static void
ipflow_init(void)
{
	int idx;

	for (idx = 0; idx < IPFLOW_HASHSIZE; idx++) {
		struct ipflow_head *head = &ipflows[idx];
		LIST_INIT(&head->ipfh_head);
		mtx_init(&head->ipfh_mtx, "ipflow list head", NULL, MTX_DEF);
	}
}
SYSINIT(ipflow, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY, ipflow_init, 0);
