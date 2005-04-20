/*-
 * Copyright (c) 2004 Gleb Smirnoff <glebius@FreeBSD.org>
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

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
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

#define	NBUCKETS	(4096)	/* must be power of 2 */

/* This hash is for TCP or UDP packets */
#define FULL_HASH(addr1,addr2,port1,port2)\
	(((addr1 >> 16) ^		\
	  (addr2 & 0x00FF) ^		\
	  ((port1 ^ port2) << 8) )&	\
	 (NBUCKETS - 1))

/* This hash for all other IP packets */
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

MALLOC_DECLARE(M_NETFLOW);
MALLOC_DEFINE(M_NETFLOW, "NetFlow", "flow cache");

static int export_add(priv_p , struct flow_entry *);
static int export_send(priv_p );

/* Generate hash for a given flow record */
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

/* Lookup for record in given slot */
static __inline struct flow_entry *
hash_lookup(struct flow_hash_entry *h, int slot, struct flow_rec *r)
{
	struct flow_entry *fle;

	LIST_FOREACH(fle, &(h[slot].head), fle_hash)
		if (bcmp(r, &fle->f.r, sizeof(struct flow_rec)) == 0)
			return (fle);

	return (NULL);
}

/* Get a flow entry from free list */
static __inline struct flow_entry *
alloc_flow(priv_p priv, int *flows)
{
	register struct flow_entry	*fle;

	mtx_lock(&priv->free_mtx);

	if (SLIST_EMPTY(&priv->free_list)) {
		mtx_unlock(&priv->free_mtx);
		return(NULL);
	}

	fle = SLIST_FIRST(&priv->free_list);
	SLIST_REMOVE_HEAD(&priv->free_list, fle_free);

	priv->info.nfinfo_used++;
	priv->info.nfinfo_free--;

	if (flows != NULL)
		*flows = priv->info.nfinfo_used;

	mtx_unlock(&priv->free_mtx);

	return (fle);
}

/* Insert flow entry into a free list. */
static __inline int
free_flow(priv_p priv, struct flow_entry *fle)
{
	int flows;

	mtx_lock(&priv->free_mtx);
	fle->f.packets = 0;
	SLIST_INSERT_HEAD(&priv->free_list, fle, fle_free);
	flows = priv->info.nfinfo_used--;
	priv->info.nfinfo_free++;
	mtx_unlock(&priv->free_mtx);

	return flows;
}

#define	NGNF_GETUSED(priv, rval)	do {	\
	mtx_lock(&priv->free_mtx);		\
	rval = priv->info.nfinfo_used;		\
	mtx_unlock(&priv->free_mtx);		\
	} while (0)

/* Insert flow entry into expire list. */
/* XXX: Flow must be detached from work queue, but not from cache */
static __inline void
expire_flow(priv_p priv, struct flow_entry *fle)
{
	mtx_assert(&priv->work_mtx, MA_OWNED);
	LIST_REMOVE(fle, fle_hash);

	mtx_lock(&priv->expire_mtx);
	SLIST_INSERT_HEAD(&priv->expire_list, fle, fle_free);
	mtx_unlock(&priv->expire_mtx);
}

/* Get a snapshot of node statistics */
void
ng_netflow_copyinfo(priv_p priv, struct ng_netflow_info *i)
{
	mtx_lock(&priv->free_mtx);
	memcpy((void *)i, (void *)&priv->info, sizeof(priv->info));
	mtx_unlock(&priv->free_mtx);
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
 * possible fields in it. Then obtain lock on flow cache
 * and insert flow entry.
 */
static __inline int
hash_insert(priv_p priv, int slot, struct flow_rec *r, int plen,
	uint8_t tcp_flags)
{
	struct flow_hash_entry	*h = priv->hash;
	struct flow_entry	*fle;
	struct route ro;
	struct sockaddr_in *sin;

	fle = alloc_flow(priv, NULL);
	if (fle == NULL)
		return (ENOMEM);

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

	/* Push new flow entry into flow cache */
	mtx_lock(&priv->work_mtx);
	LIST_INSERT_HEAD(&(h[slot].head), fle, fle_hash);
	TAILQ_INSERT_TAIL(&priv->work_queue, fle, fle_work);
	mtx_unlock(&priv->work_mtx);

	return (0);
}

static __inline int
make_flow_rec(struct mbuf **m, int *plen, struct flow_rec *r,
	uint8_t *tcp_flags, u_int16_t i_ifx)
{
	register struct ip *ip;
	int hlen;
	int error = 0;

	ip = mtod(*m, struct ip*);

	/* check version */
	if (ip->ip_v != IPVERSION)
		return (EINVAL);

	/* verify min header length */
	hlen = ip->ip_hl << 2;

	if (hlen < sizeof(struct ip))
		return (EINVAL);

	r->r_src = ip->ip_src;
	r->r_dst = ip->ip_dst;

	/* save packet length */
	*plen = ntohs(ip->ip_len);

	r->r_ip_p = ip->ip_p;
	r->r_tos = ip->ip_tos;

	/* Configured in_ifx overrides mbuf's */
	if (i_ifx == 0) {
		if ((*m)->m_pkthdr.rcvif)
			r->r_i_ifx = (*m)->m_pkthdr.rcvif->if_index;
	} else
		r->r_i_ifx = i_ifx;

	/*
	 * XXX NOTE: only first fragment of fragmented TCP, UDP and
	 * ICMP packet will be recorded with proper s_port and d_port.
	 * Following fragments will be recorded simply as IP packet with
	 * ip_proto = ip->ip_p and s_port, d_port set to zero.
	 * I know, it looks like bug. But I don't want to re-implement
	 * ip packet assebmling here. Anyway, (in)famous trafd works this way -
	 * and nobody complains yet :)
	 */
	if(ip->ip_off & htons(IP_OFFMASK))
		return (0);

	/* skip IP header */
	m_adj(*m, hlen);

	switch(r->r_ip_p) {
	case IPPROTO_TCP:
	{
		register struct tcphdr *tcp;

		/* verify that packet is not truncated */
		if (CHECK_MLEN(*m, sizeof(struct tcphdr)))
			ERROUT(EINVAL);

		if (CHECK_PULLUP(*m, sizeof(struct tcphdr)))
			ERROUT(ENOBUFS);

		tcp = mtod(*m, struct tcphdr*);
		r->r_sport = tcp->th_sport;
		r->r_dport = tcp->th_dport;
		*tcp_flags = tcp->th_flags;
		break;
	}
	case IPPROTO_UDP:
		/* verify that packet is not truncated */
		if (CHECK_MLEN(*m, sizeof(struct udphdr)))
			ERROUT(EINVAL);

		if (CHECK_PULLUP(*m, sizeof(struct udphdr)))
			ERROUT(ENOBUFS);

		r->r_ports = *(mtod(*m, uint32_t *));
		break;
	}

done:
	return (error);
}

/*
 * Non-static functions called from ng_netflow.c
 */

/* Allocate memory and set up flow cache */
int
ng_netflow_cache_init(priv_p priv)
{
	struct flow_entry *fle;
	int i;

	/* allocate cache */
	MALLOC(priv->cache, struct flow_entry *,
	    CACHESIZE * sizeof(struct flow_entry),
	    M_NETFLOW, M_WAITOK | M_ZERO);

	if (priv->cache == NULL)
		return (ENOMEM);

	/* allocate hash */
	MALLOC(priv->hash, struct flow_hash_entry *,
	    NBUCKETS * sizeof(struct flow_hash_entry),
	    M_NETFLOW, M_WAITOK | M_ZERO);

	if (priv->hash == NULL) {
		FREE(priv->cache, M_NETFLOW);
		return (ENOMEM);
	}

	TAILQ_INIT(&priv->work_queue);
	SLIST_INIT(&priv->free_list);
	SLIST_INIT(&priv->expire_list);

	mtx_init(&priv->work_mtx, "ng_netflow cache mutex", NULL, MTX_DEF);
	mtx_init(&priv->free_mtx, "ng_netflow free mutex", NULL, MTX_DEF);
	mtx_init(&priv->expire_mtx, "ng_netflow expire mutex", NULL, MTX_DEF);

	/* build free list */
	for (i = 0, fle = priv->cache; i < CACHESIZE; i++, fle++)
		SLIST_INSERT_HEAD(&priv->free_list, fle, fle_free);

	priv->info.nfinfo_free = CACHESIZE;

	return (0);
}

/* Free all flow cache memory. Called from node close method. */
void
ng_netflow_cache_flush(priv_p priv)
{
	register struct flow_entry	*fle;
	int i;

	/*
	 * We are going to free probably billable data.
	 * Expire everything before freeing it.
	 * No locking is required since callout is already drained.
	 */

	for (i = 0, fle = priv->cache; i < CACHESIZE; i++, fle++)
		if (!ISFREE(fle))
			/* ignore errors now */
			(void )export_add(priv, fle);

	mtx_destroy(&priv->work_mtx);
	mtx_destroy(&priv->free_mtx);
	mtx_destroy(&priv->expire_mtx);

	/* free hash memory */
	if (priv->hash)
		FREE(priv->hash, M_NETFLOW);

	/* free flow cache */
	if (priv->cache)
		FREE(priv->cache, M_NETFLOW);

}

/* Insert packet from &m into flow cache. */
int
ng_netflow_flow_add(priv_p priv, struct mbuf **m, iface_p iface)
{
	struct flow_hash_entry		*h = priv->hash;
	register struct flow_entry	*fle;
	struct flow_rec		r;
	int			plen;
	int			error = 0;
	uint32_t		slot;
	uint8_t			tcp_flags = 0;

	/* Try to fill *rec */
	bzero(&r, sizeof(r));
	if ((error = make_flow_rec(m, &plen, &r, &tcp_flags,
	    iface->info.ifinfo_index)))
		return (error);

	slot = ip_hash(&r);

	mtx_lock(&priv->work_mtx);

	/* Update node statistics. */
	priv->info.nfinfo_packets ++;
	priv->info.nfinfo_bytes += plen;

	fle = hash_lookup(h, slot, &r); /* New flow entry or existent? */

	if (fle) {	/* an existent entry */

		TAILQ_REMOVE(&priv->work_queue, fle, fle_work);

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
		    (fle->f.bytes >= (UINT_MAX - IF_MAXMTU)) )
			expire_flow(priv, fle);
		else
			TAILQ_INSERT_TAIL(&priv->work_queue, fle, fle_work);

		mtx_unlock(&priv->work_mtx);

	} else {	/* a new flow entry */

		mtx_unlock(&priv->work_mtx);
		return hash_insert(priv, slot, &r, plen, tcp_flags);

	}

	mtx_assert(&priv->work_mtx, MA_NOTOWNED);
	mtx_assert(&priv->expire_mtx, MA_NOTOWNED);
	mtx_assert(&priv->free_mtx, MA_NOTOWNED);

	return (0);
}

/*
 * Return records from cache. netgraph(4) guarantees us that we
 * are locked against ng_netflow_rcvdata(). However we can
 * work with ng_netflow_expire() in parrallel. XXX: Is it dangerous?
 *
 * TODO: matching particular IP should be done in kernel, here.
 */
int
ng_netflow_flow_show(priv_p priv, uint32_t last, struct ng_mesg *resp)
{
	struct flow_entry *fle;
	struct ngnf_flows *data;

	data = (struct ngnf_flows *)resp->data;
	data->last = 0;
	data->nentries = 0;

	/* Check if this is a first run */
	if (last == 0)
		fle = priv->cache;
	else {
		if (last > CACHESIZE-1)
			return (EINVAL);
		fle = priv->cache + last;
	}

	/*
	 * We will transfer not more than NREC_AT_ONCE. More data
	 * will come in next message.
	 * We send current stop point to userland, and userland should return
	 * it back to us.
	 */
	for (; last < CACHESIZE; fle++, last++) {
		if (ISFREE(fle))
			continue;
		bcopy(&fle->f, &(data->entries[data->nentries]),
		    sizeof(fle->f));
		data->nentries ++;
		if (data->nentries == NREC_AT_ONCE) {
			if (++last < CACHESIZE)
				data->last = (++fle - priv->cache);
			return (0);
		}
     	}

	return (0);
}

/* We have full datagram in privdata. Send it to export hook. */
static int
export_send(priv_p priv)
{
	struct netflow_v5_header *header = &priv->dgram.header;
	struct timespec ts;
	struct mbuf *m;
	int error = 0;
	int mlen;

	header->sys_uptime = htonl(MILLIUPTIME(time_uptime));

	getnanotime(&ts);
	header->unix_secs  = htonl(ts.tv_sec);
	header->unix_nsecs = htonl(ts.tv_nsec);

	/* Flow sequence contains number of first record */
	header->flow_seq = htonl(priv->flow_seq - header->count);

	mlen = sizeof(struct netflow_v5_header) +
	    sizeof(struct netflow_v5_record) * header->count;

	header->count = htons(header->count);
	if ((m = m_devget((caddr_t)header, mlen, 0, NULL, NULL)) == NULL) {
		log(LOG_CRIT, "ng_netflow: m_devget() failed, losing export "
		    "dgram\n");
		header->count = 0;
		return(ENOBUFS);
	}

	header->count = 0;

	/* Giant is required in sosend() at this moment. */
	NET_LOCK_GIANT();
	NG_SEND_DATA_ONLY(error, priv->export, m);
	NET_UNLOCK_GIANT();

	if (error)
		NG_FREE_M(m);

	return (error);
}


/* Create export datagram. */
static int
export_add(priv_p priv, struct flow_entry *fle)
{
	struct netflow_v5_header *header = &priv->dgram.header;
	struct netflow_v5_record *rec;

	if (header->count == 0 ) {	/* first record */
		rec = &priv->dgram.r[0];
		header->count = 1;
	} else {			/* continue filling datagram */
		rec = &priv->dgram.r[header->count];
		header->count ++;
	}

	/* Fill in export record */
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

	priv->flow_seq++;

	if (header->count == NETFLOW_V5_MAX_RECORDS) /* end of datagram */
		return export_send(priv);

	return (0);	
}

/* Periodic flow expiry run. */
void
ng_netflow_expire(void *arg)
{
	register struct flow_entry	*fle, *fle1;
	priv_p priv = (priv_p )arg;
	uint32_t used;
	int error = 0;

	/* First pack actively expired entries */
	mtx_lock(&priv->expire_mtx);
	while (!SLIST_EMPTY(&(priv->expire_list))) {
		fle = SLIST_FIRST(&(priv->expire_list));
		SLIST_REMOVE_HEAD(&(priv->expire_list), fle_free);
		mtx_unlock(&priv->expire_mtx);

		/*
		 * While we have dropped the lock, expire_flow() may
		 * insert another flow into top of the list.
		 * This is not harmful for us, since we have already
		 * detached our own.
		 */

		if ((error = export_add(priv, fle)) != 0)
			log(LOG_CRIT, "ng_netflow: export_add() failed: %u\n",
			    error);
		(void )free_flow(priv, fle);

		mtx_lock(&priv->expire_mtx);
	}
	mtx_unlock(&priv->expire_mtx);

	NGNF_GETUSED(priv, used);
	mtx_lock(&priv->work_mtx);
	TAILQ_FOREACH_SAFE(fle, &(priv->work_queue), fle_work, fle1) {
		/*
		 * When cache size has not reached CACHELOWAT yet, we keep
		 * both inactive and active flows in cache. Doing this, we
		 * reduce number of exports, since many inactive flows may
		 * wake up and continue their life. However, we make an
		 * exclusion for scans. It is very rare situation that
		 * inactive 1-packet flow will wake up.
		 * When cache has reached CACHELOWAT, we expire all inactive
		 * flows, until cache gets to a sane size.
		 */
		if (used <= CACHELOWAT && !INACTIVE(fle))
			goto finish;

		if ((INACTIVE(fle) && (SMALL(fle) || (used > CACHELOWAT))) ||
		    AGED(fle)) {

			/* Detach flow entry from cache */
			LIST_REMOVE(fle, fle_hash);
			TAILQ_REMOVE(&priv->work_queue, fle, fle_work);

			/*
			 * While we are sending to collector, unlock cache.
			 * XXX: it can happen, however with a small probability,
			 * that item, we are holding now, can be moved to the
			 * top of flow cache by node thread. In this case our
			 * expire thread stops checking. Since this is not
			 * fatal we will just ignore it now.
			 */
			mtx_unlock(&priv->work_mtx);

			if ((error = export_add(priv, fle)) != 0)
				log(LOG_CRIT, "ng_netflow: export_add() "
				    "failed: %u\n", error);

			used = free_flow(priv, fle);

			mtx_lock(&priv->work_mtx);
		}
     	}

finish:
	mtx_unlock(&priv->work_mtx);

	mtx_assert(&priv->expire_mtx, MA_NOTOWNED);
	mtx_assert(&priv->free_mtx, MA_NOTOWNED);

	/* schedule next expire */
	callout_reset(&priv->exp_callout, (1*hz), &ng_netflow_expire,
	    (void *)priv);

}
