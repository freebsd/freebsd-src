/*-
 * Copyright (c) 2004-2005 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 2001-2003 Roman V. Palagin <romanp@unshadow.net>
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
 * $SourceForge: netflow.c,v 1.41 2004/09/05 11:41:10 glebius Exp $
 */

static const char rcs_id[] =
    "@(#) $FreeBSD$";

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <machine/atomic.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>

#include <netgraph/netflow/netflow.h>
#include <netgraph/netflow/ng_netflow.h>

#define	NBUCKETS	(65536)		/* must be power of 2 */

/* This hash is for TCP or UDP packets */
#define FULL_HASH(addr1,addr2,port1,port2)\
	(((addr1 >> 16) ^		\
	  (addr2 & 0x00FF) ^		\
	  ((port1 ^ port2) << 8) )&	\
	 (NBUCKETS - 1))

/* This hash is for all other IP packets */
#define ADDR_HASH(addr1,addr2)\
	(((addr1 >> 16) ^		\
	  (addr2 & 0x00FF) )&		\
	 (NBUCKETS - 1))

/* Macros to shorten logical constructions */
/* XXX: priv must exist in namespace */
#define	INACTIVE(fle)	(time_uptime - fle->f.last > priv->info.nfinfo_inact_t)
#define	AGED(fle)	(time_uptime - fle->f.first > priv->info.nfinfo_act_t)
#define	ISFREE(fle)	(fle->f.packets == 0)

/*
 * 4 is a magical number: statistically number of 4-packet flows is
 * bigger than 5,6,7...-packet flows by an order of magnitude. Most UDP/ICMP
 * scans are 1 packet (~ 90% of flow cache). TCP scans are 2-packet in case
 * of reachable host and 4-packet otherwise.
 */
#define	SMALL(fle)	(fle->f.packets <= 4)

/*
 * Cisco uses milliseconds for uptime. Bad idea, since it overflows
 * every 48+ days. But we will do same to keep compatibility. This macro
 * does overflowable multiplication to 1000.
 */
#define	MILLIUPTIME(t)	(((t) << 9) +	/* 512 */	\
			 ((t) << 8) +	/* 256 */	\
			 ((t) << 7) +	/* 128 */	\
			 ((t) << 6) +	/* 64  */	\
			 ((t) << 5) +	/* 32  */	\
			 ((t) << 3))	/* 8   */

MALLOC_DECLARE(M_NETFLOW_HASH);
MALLOC_DEFINE(M_NETFLOW_HASH, "NetFlow hash", "NetFlow hash");

static int export_add(item_p, struct flow_entry *);
static int export_send(priv_p, item_p, int flags);

/* Generate hash for a given flow record. */
static __inline uint32_t
ip_hash(struct flow_rec *r)
{
	switch (r->r_ip_p) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
		return FULL_HASH(r->r_src.s_addr, r->r_dst.s_addr,
		    r->r_sport, r->r_dport);
	default:
		return ADDR_HASH(r->r_src.s_addr, r->r_dst.s_addr);
	}
}

/* This is callback from uma(9), called on alloc. */
static int
uma_ctor_flow(void *mem, int size, void *arg, int how)
{
	priv_p priv = (priv_p )arg;

	if (atomic_load_acq_32(&priv->info.nfinfo_used) >= CACHESIZE)
		return (ENOMEM);

	atomic_add_32(&priv->info.nfinfo_used, 1);

	return (0);
}

/* This is callback from uma(9), called on free. */
static void
uma_dtor_flow(void *mem, int size, void *arg)
{
	priv_p priv = (priv_p )arg;

	atomic_subtract_32(&priv->info.nfinfo_used, 1);
}

/*
 * Detach export datagram from priv, if there is any.
 * If there is no, allocate a new one.
 */
static item_p
get_export_dgram(priv_p priv)
{
	item_p	item = NULL;

	mtx_lock(&priv->export_mtx);
	if (priv->export_item != NULL) {
		item = priv->export_item;
		priv->export_item = NULL;
	}
	mtx_unlock(&priv->export_mtx);

	if (item == NULL) {
		struct netflow_v5_export_dgram *dgram;
		struct mbuf *m;

		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL)
			return (NULL);
		item = ng_package_data(m, NG_NOFLAGS);
		if (item == NULL)
			return (NULL);
		dgram = mtod(m, struct netflow_v5_export_dgram *);
		dgram->header.count = 0;
		dgram->header.version = htons(NETFLOW_V5);

	}

	return (item);
}

/*
 * Re-attach incomplete datagram back to priv.
 * If there is already another one, then send incomplete. */
static void
return_export_dgram(priv_p priv, item_p item, int flags)
{
	/*
	 * It may happen on SMP, that some thread has already
	 * put its item there, in this case we bail out and
	 * send what we have to collector.
	 */
	mtx_lock(&priv->export_mtx);
	if (priv->export_item == NULL) {
		priv->export_item = item;
		mtx_unlock(&priv->export_mtx);
	} else {
		mtx_unlock(&priv->export_mtx);
		export_send(priv, item, flags);
	}
}

/*
 * The flow is over. Call export_add() and free it. If datagram is
 * full, then call export_send().
 */
static __inline void
expire_flow(priv_p priv, item_p *item, struct flow_entry *fle, int flags)
{
	if (*item == NULL)
		*item = get_export_dgram(priv);
	if (*item == NULL) {
		atomic_add_32(&priv->info.nfinfo_export_failed, 1);
		uma_zfree_arg(priv->zone, fle, priv);
		return;
	}
	if (export_add(*item, fle) > 0) {
		export_send(priv, *item, flags);
		*item = NULL;
	}
	uma_zfree_arg(priv->zone, fle, priv);
}

/* Get a snapshot of node statistics */
void
ng_netflow_copyinfo(priv_p priv, struct ng_netflow_info *i)
{
	/* XXX: atomic */
	memcpy((void *)i, (void *)&priv->info, sizeof(priv->info));
}

/* Calculate number of bits in netmask */
#define	g21	0x55555555ul	/* = 0101_0101_0101_0101_0101_0101_0101_0101 */
#define	g22	0x33333333ul	/* = 0011_0011_0011_0011_0011_0011_0011_0011 */
#define	g23	0x0f0f0f0ful	/* = 0000_1111_0000_1111_0000_1111_0000_1111 */
static __inline u_char
bit_count(uint32_t v)
{
	v = (v & g21) + ((v >> 1) & g21);
	v = (v & g22) + ((v >> 2) & g22);
	v = (v + (v >> 4)) & g23;
	return (v + (v >> 8) + (v >> 16) + (v >> 24)) & 0x3f;
}

/*
 * Insert a record into defined slot.
 *
 * First we get for us a free flow entry, then fill in all
 * possible fields in it.
 *
 * TODO: consider dropping hash mutex while filling in datagram,
 * as this was done in previous version. Need to test & profile
 * to be sure.
 */
static __inline int
hash_insert(priv_p priv, struct flow_hash_entry  *hsh, struct flow_rec *r,
	int plen, uint8_t tcp_flags)
{
	struct flow_entry	*fle;
	struct route ro;
	struct sockaddr_in *sin;

	mtx_assert(&hsh->mtx, MA_OWNED);

	fle = uma_zalloc_arg(priv->zone, priv, M_NOWAIT);
	if (fle == NULL) {
		atomic_add_32(&priv->info.nfinfo_alloc_failed, 1);
		return (ENOMEM);
	}

	/*
	 * Now fle is totally ours. It is detached from all lists,
	 * we can safely edit it.
	 */

	bcopy(r, &fle->f.r, sizeof(struct flow_rec));
	fle->f.bytes = plen;
	fle->f.packets = 1;
	fle->f.tcp_flags = tcp_flags;

	fle->f.first = fle->f.last = time_uptime;

	/*
	 * First we do route table lookup on destination address. So we can
	 * fill in out_ifx, dst_mask, nexthop, and dst_as in future releases.
	 */
	bzero((caddr_t)&ro, sizeof(ro));
	sin = (struct sockaddr_in *)&ro.ro_dst;
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = fle->f.r.r_dst;
	rtalloc_ign(&ro, RTF_CLONING);
	if (ro.ro_rt != NULL) {
		struct rtentry *rt = ro.ro_rt;

		fle->f.fle_o_ifx = rt->rt_ifp->if_index;

		if (rt->rt_flags & RTF_GATEWAY &&
		    rt->rt_gateway->sa_family == AF_INET)
			fle->f.next_hop =
			    ((struct sockaddr_in *)(rt->rt_gateway))->sin_addr;

		if (rt_mask(rt))
			fle->f.dst_mask =
			    bit_count(((struct sockaddr_in *)rt_mask(rt))->sin_addr.s_addr);
		else if (rt->rt_flags & RTF_HOST)
			/* Give up. We can't determine mask :( */
			fle->f.dst_mask = 32;

		RTFREE(ro.ro_rt);
	}

	/* Do route lookup on source address, to fill in src_mask. */

	bzero((caddr_t)&ro, sizeof(ro));
	sin = (struct sockaddr_in *)&ro.ro_dst;
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = fle->f.r.r_src;
	rtalloc_ign(&ro, RTF_CLONING);
	if (ro.ro_rt != NULL) {
		struct rtentry *rt = ro.ro_rt;

		if (rt_mask(rt))
			fle->f.src_mask =
			    bit_count(((struct sockaddr_in *)rt_mask(rt))->sin_addr.s_addr);
		else if (rt->rt_flags & RTF_HOST)
			/* Give up. We can't determine mask :( */
			fle->f.src_mask = 32;

		RTFREE(ro.ro_rt);
	}

	/* Push new flow at the and of hash. */
	TAILQ_INSERT_TAIL(&hsh->head, fle, fle_hash);

	return (0);
}


/*
 * Non-static functions called from ng_netflow.c
 */

/* Allocate memory and set up flow cache */
int
ng_netflow_cache_init(priv_p priv)
{
	struct flow_hash_entry	*hsh;
	int i;

	/* Initialize cache UMA zone. */
	priv->zone = uma_zcreate("NetFlow cache", sizeof(struct flow_entry),
	    uma_ctor_flow, uma_dtor_flow, NULL, NULL, UMA_ALIGN_CACHE, 0);
	uma_zone_set_max(priv->zone, CACHESIZE);

	/* Allocate hash. */
	MALLOC(priv->hash, struct flow_hash_entry *,
	    NBUCKETS * sizeof(struct flow_hash_entry),
	    M_NETFLOW_HASH, M_WAITOK | M_ZERO);

	if (priv->hash == NULL) {
		uma_zdestroy(priv->zone);
		return (ENOMEM);
	}

	/* Initialize hash. */
	for (i = 0, hsh = priv->hash; i < NBUCKETS; i++, hsh++) {
		mtx_init(&hsh->mtx, "hash mutex", NULL, MTX_DEF);
		TAILQ_INIT(&hsh->head);
	}

	mtx_init(&priv->export_mtx, "export dgram lock", NULL, MTX_DEF);

	return (0);
}

/* Free all flow cache memory. Called from node close method. */
void
ng_netflow_cache_flush(priv_p priv)
{
	struct flow_entry	*fle, *fle1;
	struct flow_hash_entry	*hsh;
	item_p			item = NULL;
	int i;

	/*
	 * We are going to free probably billable data.
	 * Expire everything before freeing it.
	 * No locking is required since callout is already drained.
	 */
	for (hsh = priv->hash, i = 0; i < NBUCKETS; hsh++, i++)
		TAILQ_FOREACH_SAFE(fle, &hsh->head, fle_hash, fle1) {
			TAILQ_REMOVE(&hsh->head, fle, fle_hash);
			expire_flow(priv, &item, fle, NG_QUEUE);
		}

	if (item != NULL)
		export_send(priv, item, NG_QUEUE);

	uma_zdestroy(priv->zone);

	/* Destroy hash mutexes. */
	for (i = 0, hsh = priv->hash; i < NBUCKETS; i++, hsh++)
		mtx_destroy(&hsh->mtx);

	/* Free hash memory. */
	if (priv->hash)
		FREE(priv->hash, M_NETFLOW_HASH);

	mtx_destroy(&priv->export_mtx);
}

/* Insert packet from into flow cache. */
int
ng_netflow_flow_add(priv_p priv, struct ip *ip, iface_p iface,
	struct ifnet *ifp)
{
	register struct flow_entry	*fle, *fle1;
	struct flow_hash_entry		*hsh;
	struct flow_rec		r;
	item_p			item = NULL;
	int			hlen, plen;
	int			error = 0;
	uint8_t			tcp_flags = 0;

	/* Try to fill flow_rec r */
	bzero(&r, sizeof(r));
	/* check version */
	if (ip->ip_v != IPVERSION)
		return (EINVAL);

	/* verify min header length */
	hlen = ip->ip_hl << 2;

	if (hlen < sizeof(struct ip))
		return (EINVAL);

	r.r_src = ip->ip_src;
	r.r_dst = ip->ip_dst;

	/* save packet length */
	plen = ntohs(ip->ip_len);

	r.r_ip_p = ip->ip_p;
	r.r_tos = ip->ip_tos;

	/* Configured in_ifx overrides mbuf's */
	if (iface->info.ifinfo_index == 0) {
		if (ifp != NULL)
			r.r_i_ifx = ifp->if_index;
	} else
		r.r_i_ifx = iface->info.ifinfo_index;

	/*
	 * XXX NOTE: only first fragment of fragmented TCP, UDP and
	 * ICMP packet will be recorded with proper s_port and d_port.
	 * Following fragments will be recorded simply as IP packet with
	 * ip_proto = ip->ip_p and s_port, d_port set to zero.
	 * I know, it looks like bug. But I don't want to re-implement
	 * ip packet assebmling here. Anyway, (in)famous trafd works this way -
	 * and nobody complains yet :)
	 */
	if ((ip->ip_off & htons(IP_OFFMASK)) == 0)
		switch(r.r_ip_p) {
		case IPPROTO_TCP:
		{
			register struct tcphdr *tcp;

			tcp = (struct tcphdr *)((caddr_t )ip + hlen);
			r.r_sport = tcp->th_sport;
			r.r_dport = tcp->th_dport;
			tcp_flags = tcp->th_flags;
			break;
		}
			case IPPROTO_UDP:
			r.r_ports = *(uint32_t *)((caddr_t )ip + hlen);
			break;
		}

	/* Update node statistics. XXX: race... */
	priv->info.nfinfo_packets ++;
	priv->info.nfinfo_bytes += plen;

	/* Find hash slot. */
	hsh = &priv->hash[ip_hash(&r)];

	mtx_lock(&hsh->mtx);

	/*
	 * Go through hash and find our entry. If we encounter an
	 * entry, that should be expired, purge it. We do a reverse
	 * search since most active entries are first, and most
	 * searches are done on most active entries.
	 */
	TAILQ_FOREACH_REVERSE_SAFE(fle, &hsh->head, fhead, fle_hash, fle1) {
		if (bcmp(&r, &fle->f.r, sizeof(struct flow_rec)) == 0)
			break;
		if ((INACTIVE(fle) && SMALL(fle)) || AGED(fle)) {
			TAILQ_REMOVE(&hsh->head, fle, fle_hash);
			expire_flow(priv, &item, fle, NG_QUEUE);
			atomic_add_32(&priv->info.nfinfo_act_exp, 1);
		}
	}

	if (fle) {			/* An existent entry. */

		fle->f.bytes += plen;
		fle->f.packets ++;
		fle->f.tcp_flags |= tcp_flags;
		fle->f.last = time_uptime;

		/*
		 * We have the following reasons to expire flow in active way:
		 * - it hit active timeout
		 * - a TCP connection closed
		 * - it is going to overflow counter
		 */
		if (tcp_flags & TH_FIN || tcp_flags & TH_RST || AGED(fle) ||
		    (fle->f.bytes >= (UINT_MAX - IF_MAXMTU)) ) {
			TAILQ_REMOVE(&hsh->head, fle, fle_hash);
			expire_flow(priv, &item, fle, NG_QUEUE);
			atomic_add_32(&priv->info.nfinfo_act_exp, 1);
		} else {
			/*
			 * It is the newest, move it to the tail,
			 * if it isn't there already. Next search will
			 * locate it quicker.
			 */
			if (fle != TAILQ_LAST(&hsh->head, fhead)) {
				TAILQ_REMOVE(&hsh->head, fle, fle_hash);
				TAILQ_INSERT_TAIL(&hsh->head, fle, fle_hash);
			}
		}
	} else				/* A new flow entry. */
		error = hash_insert(priv, hsh, &r, plen, tcp_flags);

	mtx_unlock(&hsh->mtx);

	if (item != NULL)
		return_export_dgram(priv, item, NG_QUEUE);

	return (error);
}

/*
 * Return records from cache to userland.
 *
 * TODO: matching particular IP should be done in kernel, here.
 */
int
ng_netflow_flow_show(priv_p priv, uint32_t last, struct ng_mesg *resp)
{
	struct flow_hash_entry *hsh;
	struct flow_entry *fle;
	struct ngnf_flows *data;
	int i;

	data = (struct ngnf_flows *)resp->data;
	data->last = 0;
	data->nentries = 0;

	/* Check if this is a first run */
	if (last == 0) {
		hsh = priv->hash;
		i = 0;
	} else {
		if (last > NBUCKETS-1)
			return (EINVAL);
		hsh = priv->hash + last;
		i = last;
	}

	/*
	 * We will transfer not more than NREC_AT_ONCE. More data
	 * will come in next message.
	 * We send current hash index to userland, and userland should
	 * return it back to us. Then, we will restart with new entry.
	 *
	 * The resulting cache snapshot is inaccurate for the
	 * following reasons:
	 *  - we skip locked hash entries
	 *  - we bail out, if someone wants our entry
	 *  - we skip rest of entry, when hit NREC_AT_ONCE
	 */
	for (; i < NBUCKETS; hsh++, i++) {
		if (mtx_trylock(&hsh->mtx) == 0)
			continue;

		TAILQ_FOREACH(fle, &hsh->head, fle_hash) {
			if (hsh->mtx.mtx_lock & MTX_CONTESTED)
				break;

			bcopy(&fle->f, &(data->entries[data->nentries]),
			    sizeof(fle->f));
			data->nentries++;
			if (data->nentries == NREC_AT_ONCE) {
				mtx_unlock(&hsh->mtx);
				if (++i < NBUCKETS)
					data->last = i;
				return (0);
			}
		}
		mtx_unlock(&hsh->mtx);
	}

	return (0);
}

/* We have full datagram in privdata. Send it to export hook. */
static int
export_send(priv_p priv, item_p item, int flags)
{
	struct mbuf *m = NGI_M(item);
	struct netflow_v5_export_dgram *dgram = mtod(m,
					struct netflow_v5_export_dgram *);
	struct netflow_v5_header *header = &dgram->header;
	struct timespec ts;
	int error = 0;

	/* Fill mbuf header. */
	m->m_len = m->m_pkthdr.len = sizeof(struct netflow_v5_record) *
	   header->count + sizeof(struct netflow_v5_header);

	/* Fill export header. */
	header->sys_uptime = htonl(MILLIUPTIME(time_uptime));
	getnanotime(&ts);
	header->unix_secs  = htonl(ts.tv_sec);
	header->unix_nsecs = htonl(ts.tv_nsec);
	header->flow_seq = htonl(atomic_fetchadd_32(&priv->flow_seq,
	    header->count));
	header->count = htons(header->count);

	if (priv->export != NULL)
		/* Should also NET_LOCK_GIANT(). */
		NG_FWD_ITEM_HOOK_FLAGS(error, item, priv->export, flags);

	return (error);
}


/* Add export record to dgram. */
static int
export_add(item_p item, struct flow_entry *fle)
{
	struct netflow_v5_export_dgram *dgram = mtod(NGI_M(item),
					struct netflow_v5_export_dgram *);
	struct netflow_v5_header *header = &dgram->header;
	struct netflow_v5_record *rec;

	if (header->count == 0 ) {	/* first record */
		rec = &dgram->r[0];
		header->count = 1;
	} else {			/* continue filling datagram */
		rec = &dgram->r[header->count];
		header->count ++;
	}

	KASSERT(header->count <= NETFLOW_V5_MAX_RECORDS,
	    ("ng_netflow: export too big"));

	/* Fill in export record. */
	rec->src_addr = fle->f.r.r_src.s_addr;
	rec->dst_addr = fle->f.r.r_dst.s_addr;
	rec->next_hop = fle->f.next_hop.s_addr;
	rec->i_ifx    = htons(fle->f.fle_i_ifx);
	rec->o_ifx    = htons(fle->f.fle_o_ifx);
	rec->packets  = htonl(fle->f.packets);
	rec->octets   = htonl(fle->f.bytes);
	rec->first    = htonl(MILLIUPTIME(fle->f.first));
	rec->last     = htonl(MILLIUPTIME(fle->f.last));
	rec->s_port   = fle->f.r.r_sport;
	rec->d_port   = fle->f.r.r_dport;
	rec->flags    = fle->f.tcp_flags;
	rec->prot     = fle->f.r.r_ip_p;
	rec->tos      = fle->f.r.r_tos;
	rec->dst_mask = fle->f.dst_mask;
	rec->src_mask = fle->f.src_mask;

	/* Not supported fields. */
	rec->src_as = rec->dst_as = 0;

	if (header->count == NETFLOW_V5_MAX_RECORDS)
		return (1); /* end of datagram */
	else
		return (0);	
}

/* Periodic flow expiry run. */
void
ng_netflow_expire(void *arg)
{
	struct flow_entry	*fle, *fle1;
	struct flow_hash_entry	*hsh;
	priv_p			priv = (priv_p )arg;
	item_p			item = NULL;
	uint32_t		used;
	int			i;

	/*
	 * Going through all the cache.
	 */
	for (hsh = priv->hash, i = 0; i < NBUCKETS; hsh++, i++) {
		/*
		 * Skip entries, that are already being worked on.
		 */
		if (mtx_trylock(&hsh->mtx) == 0)
			continue;

		used = atomic_load_acq_32(&priv->info.nfinfo_used);
		TAILQ_FOREACH_SAFE(fle, &hsh->head, fle_hash, fle1) {
			/*
			 * Interrupt thread wants this entry!
			 * Quick! Quick! Bail out!
			 */
			if (hsh->mtx.mtx_lock & MTX_CONTESTED)
				break;

			/*
			 * Don't expire aggressively while hash collision
			 * ratio is predicted small.
			 */
			if (used <= (NBUCKETS*2) && !INACTIVE(fle))
				break;

			if ((INACTIVE(fle) && (SMALL(fle) || (used > (NBUCKETS*2)))) ||
			    AGED(fle)) {
				TAILQ_REMOVE(&hsh->head, fle, fle_hash);
				expire_flow(priv, &item, fle, NG_NOFLAGS);
				used--;
				atomic_add_32(&priv->info.nfinfo_inact_exp, 1);
			}
		}
		mtx_unlock(&hsh->mtx);
	}

	if (item != NULL)
		return_export_dgram(priv, item, NG_NOFLAGS);

	/* Schedule next expire. */
	callout_reset(&priv->exp_callout, (1*hz), &ng_netflow_expire,
	    (void *)priv);
}
