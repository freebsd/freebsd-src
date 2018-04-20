/*-
 * Copyright (c) 2015-2016 Yandex LLC
 * Copyright (c) 2015 Alexander V. Chernikov <melifaro@FreeBSD.org>
 * Copyright (c) 2016 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_pflog.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip_fw_nat64.h>

#include <netpfil/ipfw/ip_fw_private.h>
#include <netpfil/ipfw/nat64/ip_fw_nat64.h>
#include <netpfil/ipfw/nat64/nat64lsn.h>
#include <netpfil/ipfw/nat64/nat64_translate.h>
#include <netpfil/pf/pf.h>

MALLOC_DEFINE(M_NAT64LSN, "NAT64LSN", "NAT64LSN");

static void nat64lsn_periodic(void *data);
#define	PERIODIC_DELAY	4
static uint8_t nat64lsn_proto_map[256];
uint8_t nat64lsn_rproto_map[NAT_MAX_PROTO];

#define	NAT64_FLAG_FIN		0x01	/* FIN was seen */
#define	NAT64_FLAG_SYN		0x02	/* First syn in->out */
#define	NAT64_FLAG_ESTAB	0x04	/* Packet with Ack */
#define	NAT64_FLAGS_TCP	(NAT64_FLAG_SYN|NAT64_FLAG_ESTAB|NAT64_FLAG_FIN)

#define	NAT64_FLAG_RDR		0x80	/* Port redirect */
#define	NAT64_LOOKUP(chain, cmd)	\
	(struct nat64lsn_cfg *)SRV_OBJECT((chain), (cmd)->arg1)
/*
 * Delayed job queue, used to create new hosts
 * and new portgroups
 */
enum nat64lsn_jtype {
	JTYPE_NEWHOST = 1,
	JTYPE_NEWPORTGROUP,
	JTYPE_DELPORTGROUP,
};

struct nat64lsn_job_item {
	TAILQ_ENTRY(nat64lsn_job_item)	next;
	enum nat64lsn_jtype	jtype;
	struct nat64lsn_host	*nh;
	struct nat64lsn_portgroup	*pg;
	void			*spare_idx;
	struct in6_addr		haddr;
	uint8_t			nat_proto;
	uint8_t			done;
	int			needs_idx;
	int			delcount;
	unsigned int		fhash;	/* Flow hash */
	uint32_t		aaddr;	/* Last used address (net) */
	struct mbuf		*m;
	struct ipfw_flow_id	f_id;
	uint64_t		delmask[NAT64LSN_PGPTRNMASK];
};

static struct mtx jmtx;
#define	JQUEUE_LOCK_INIT()	mtx_init(&jmtx, "qlock", NULL, MTX_DEF)
#define	JQUEUE_LOCK_DESTROY()	mtx_destroy(&jmtx)
#define	JQUEUE_LOCK()		mtx_lock(&jmtx)
#define	JQUEUE_UNLOCK()		mtx_unlock(&jmtx)

static void nat64lsn_enqueue_job(struct nat64lsn_cfg *cfg,
    struct nat64lsn_job_item *ji);
static void nat64lsn_enqueue_jobs(struct nat64lsn_cfg *cfg,
    struct nat64lsn_job_head *jhead, int jlen);

static struct nat64lsn_job_item *nat64lsn_create_job(struct nat64lsn_cfg *cfg,
    const struct ipfw_flow_id *f_id, int jtype);
static int nat64lsn_request_portgroup(struct nat64lsn_cfg *cfg,
    const struct ipfw_flow_id *f_id, struct mbuf **pm, uint32_t aaddr,
    int needs_idx);
static int nat64lsn_request_host(struct nat64lsn_cfg *cfg,
    const struct ipfw_flow_id *f_id, struct mbuf **pm);
static int nat64lsn_translate4(struct nat64lsn_cfg *cfg,
    const struct ipfw_flow_id *f_id, struct mbuf **pm);
static int nat64lsn_translate6(struct nat64lsn_cfg *cfg,
    struct ipfw_flow_id *f_id, struct mbuf **pm);

static int alloc_portgroup(struct nat64lsn_job_item *ji);
static void destroy_portgroup(struct nat64lsn_portgroup *pg);
static void destroy_host6(struct nat64lsn_host *nh);
static int alloc_host6(struct nat64lsn_cfg *cfg, struct nat64lsn_job_item *ji);

static int attach_portgroup(struct nat64lsn_cfg *cfg,
    struct nat64lsn_job_item *ji);
static int attach_host6(struct nat64lsn_cfg *cfg, struct nat64lsn_job_item *ji);


/* XXX tmp */
static uma_zone_t nat64lsn_host_zone;
static uma_zone_t nat64lsn_pg_zone;
static uma_zone_t nat64lsn_pgidx_zone;

static unsigned int nat64lsn_periodic_chkstates(struct nat64lsn_cfg *cfg,
    struct nat64lsn_host *nh);

#define	I6_hash(x)		(djb_hash((const unsigned char *)(x), 16))
#define	I6_first(_ph, h)	(_ph)[h]
#define	I6_next(x)		(x)->next
#define	I6_val(x)		(&(x)->addr)
#define	I6_cmp(a, b)		IN6_ARE_ADDR_EQUAL(a, b)
#define	I6_lock(a, b)
#define	I6_unlock(a, b)

#define	I6HASH_FIND(_cfg, _res, _a) \
	CHT_FIND(_cfg->ih, _cfg->ihsize, I6_, _res, _a)
#define	I6HASH_INSERT(_cfg, _i)	\
	CHT_INSERT_HEAD(_cfg->ih, _cfg->ihsize, I6_, _i)
#define	I6HASH_REMOVE(_cfg, _res, _tmp, _a)	\
	CHT_REMOVE(_cfg->ih, _cfg->ihsize, I6_, _res, _tmp, _a)

#define	I6HASH_FOREACH_SAFE(_cfg, _x, _tmp, _cb, _arg)	\
	CHT_FOREACH_SAFE(_cfg->ih, _cfg->ihsize, I6_, _x, _tmp, _cb, _arg)

#define	HASH_IN4(x)	djb_hash((const unsigned char *)(x), 8)

static unsigned
djb_hash(const unsigned char *h, const int len)
{
	unsigned int result = 0;
	int i;

	for (i = 0; i < len; i++)
		result = 33 * result ^ h[i];

	return (result);
}

/*
static size_t 
bitmask_size(size_t num, int *level)
{
	size_t x;
	int c;

	for (c = 0, x = num; num > 1; num /= 64, c++)
		;

	return (x);
}

static void
bitmask_prepare(uint64_t *pmask, size_t bufsize, int level)
{
	size_t x, z;

	memset(pmask, 0xFF, bufsize);
	for (x = 0, z = 1; level > 1; x += z, z *= 64, level--)
		;
	pmask[x] ~= 0x01;
}
*/

static void
nat64lsn_log(struct pfloghdr *plog, struct mbuf *m, sa_family_t family,
    uint32_t n, uint32_t sn)
{

	memset(plog, 0, sizeof(*plog));
	plog->length = PFLOG_REAL_HDRLEN;
	plog->af = family;
	plog->action = PF_NAT;
	plog->dir = PF_IN;
	plog->rulenr = htonl(n);
	plog->subrulenr = htonl(sn);
	plog->ruleset[0] = '\0';
	strlcpy(plog->ifname, "NAT64LSN", sizeof(plog->ifname));
	ipfw_bpf_mtap2(plog, PFLOG_HDRLEN, m);
}
/*
 * Inspects icmp packets to see if the message contains different
 * packet header so we need to alter @addr and @port.
 */
static int
inspect_icmp_mbuf(struct mbuf **m, uint8_t *nat_proto, uint32_t *addr,
    uint16_t *port)
{
	struct ip *ip;
	struct tcphdr *tcp;
	struct udphdr *udp;
	struct icmphdr *icmp;
	int off;
	uint8_t proto;

	ip = mtod(*m, struct ip *); /* Outer IP header */
	off = (ip->ip_hl << 2) + ICMP_MINLEN;
	if ((*m)->m_len < off)
		*m = m_pullup(*m, off);
	if (*m == NULL)
		return (ENOMEM);

	ip = mtod(*m, struct ip *); /* Outer IP header */
	icmp = L3HDR(ip, struct icmphdr *);
	switch (icmp->icmp_type) {
	case ICMP_ECHO:
	case ICMP_ECHOREPLY:
		/* Use icmp ID as distinguisher */
		*port = ntohs(*((uint16_t *)(icmp + 1)));
		return (0);
	case ICMP_UNREACH:
	case ICMP_TIMXCEED:
		break;
	default:
		return (EOPNOTSUPP);
	}
	/*
	 * ICMP_UNREACH and ICMP_TIMXCEED contains IP header + 64 bits
	 * of ULP header.
	 */
	if ((*m)->m_pkthdr.len < off + sizeof(struct ip) + ICMP_MINLEN)
		return (EINVAL);
	if ((*m)->m_len < off + sizeof(struct ip) + ICMP_MINLEN)
		*m = m_pullup(*m, off + sizeof(struct ip) + ICMP_MINLEN);
	if (*m == NULL)
		return (ENOMEM);
	ip = mtodo(*m, off); /* Inner IP header */
	proto = ip->ip_p;
	off += ip->ip_hl << 2; /* Skip inner IP header */
	*addr = ntohl(ip->ip_src.s_addr);
	if ((*m)->m_len < off + ICMP_MINLEN)
		*m = m_pullup(*m, off + ICMP_MINLEN);
	if (*m == NULL)
		return (ENOMEM);
	switch (proto) {
	case IPPROTO_TCP:
		tcp = mtodo(*m, off);
		*nat_proto = NAT_PROTO_TCP;
		*port = ntohs(tcp->th_sport);
		return (0);
	case IPPROTO_UDP:
		udp = mtodo(*m, off);
		*nat_proto = NAT_PROTO_UDP;
		*port = ntohs(udp->uh_sport);
		return (0);
	case IPPROTO_ICMP:
		/*
		 * We will translate only ICMP errors for our ICMP
		 * echo requests.
		 */
		icmp = mtodo(*m, off);
		if (icmp->icmp_type != ICMP_ECHO)
			return (EOPNOTSUPP);
		*port = ntohs(*((uint16_t *)(icmp + 1)));
		return (0);
	};
	return (EOPNOTSUPP);
}

static inline uint8_t
convert_tcp_flags(uint8_t flags)
{
	uint8_t result;

	result = flags & (TH_FIN|TH_SYN);
	result |= (flags & TH_RST) >> 2; /* Treat RST as FIN */
	result |= (flags & TH_ACK) >> 2; /* Treat ACK as estab */

	return (result);
}

static NAT64NOINLINE int
nat64lsn_translate4(struct nat64lsn_cfg *cfg, const struct ipfw_flow_id *f_id,
    struct mbuf **pm)
{
	struct pfloghdr loghdr, *logdata;
	struct in6_addr src6;
	struct nat64lsn_portgroup *pg;
	struct nat64lsn_host *nh;
	struct nat64lsn_state *st;
	struct ip *ip;
	uint32_t addr;
	uint16_t state_flags, state_ts;
	uint16_t port, lport;
	uint8_t nat_proto;
	int ret;

	addr = f_id->dst_ip;
	port = f_id->dst_port;
	if (addr < cfg->prefix4 || addr > cfg->pmask4) {
		NAT64STAT_INC(&cfg->stats, nomatch4);
		return (cfg->nomatch_verdict);
	}

	/* Check if protocol is supported and get its short id */
	nat_proto = nat64lsn_proto_map[f_id->proto];
	if (nat_proto == 0) {
		NAT64STAT_INC(&cfg->stats, noproto);
		return (cfg->nomatch_verdict);
	}

	/* We might need to handle icmp differently */
	if (nat_proto == NAT_PROTO_ICMP) {
		ret = inspect_icmp_mbuf(pm, &nat_proto, &addr, &port);
		if (ret != 0) {
			if (ret == ENOMEM) {
				NAT64STAT_INC(&cfg->stats, nomem);
				return (IP_FW_DENY);
			}
			NAT64STAT_INC(&cfg->stats, noproto);
			return (cfg->nomatch_verdict);
		}
		/* XXX: Check addr for validity */
		if (addr < cfg->prefix4 || addr > cfg->pmask4) {
			NAT64STAT_INC(&cfg->stats, nomatch4);
			return (cfg->nomatch_verdict);
		}
	}

	/* Calc portgroup offset w.r.t protocol */
	pg = GET_PORTGROUP(cfg, addr, nat_proto, port);

	/* Check if this port is occupied by any portgroup */
	if (pg == NULL) {
		NAT64STAT_INC(&cfg->stats, nomatch4);
#if 0
		DPRINTF(DP_STATE, "NOMATCH %u %d %d (%d)", addr, nat_proto, port,
		    _GET_PORTGROUP_IDX(cfg, addr, nat_proto, port));
#endif
		return (cfg->nomatch_verdict);
	}

	/* TODO: Check flags to see if we need to do some static mapping */
	nh = pg->host;

	/* Prepare some fields we might need to update */
	SET_AGE(state_ts);
	ip = mtod(*pm, struct ip *);
	if (ip->ip_p == IPPROTO_TCP)
		state_flags = convert_tcp_flags(
		    L3HDR(ip, struct tcphdr *)->th_flags);
	else
		state_flags = 0;

	/* Lock host and get port mapping */
	NAT64_LOCK(nh);

	st = &pg->states[port & (NAT64_CHUNK_SIZE - 1)];
	if (st->timestamp != state_ts)
		st->timestamp = state_ts;
	if ((st->flags & state_flags) != state_flags)
		st->flags |= state_flags;
	lport = htons(st->u.s.lport);

	NAT64_UNLOCK(nh);

	if (cfg->flags & NAT64_LOG) {
		logdata = &loghdr;
		nat64lsn_log(logdata, *pm, AF_INET, pg->idx, st->cur.off);
	} else
		logdata = NULL;

	src6.s6_addr32[0] = cfg->prefix6.s6_addr32[0];
	src6.s6_addr32[1] = cfg->prefix6.s6_addr32[1];
	src6.s6_addr32[2] = cfg->prefix6.s6_addr32[2];
	src6.s6_addr32[3] = htonl(f_id->src_ip);

	ret = nat64_do_handle_ip4(*pm, &src6, &nh->addr, lport,
	    &cfg->stats, logdata);

	if (ret == NAT64SKIP)
		return (cfg->nomatch_verdict);
	if (ret == NAT64MFREE)
		m_freem(*pm);
	*pm = NULL;

	return (IP_FW_DENY);
}

void
nat64lsn_dump_state(const struct nat64lsn_cfg *cfg,
   const struct nat64lsn_portgroup *pg, const struct nat64lsn_state *st,
   const char *px, int off)
{
	char s[INET6_ADDRSTRLEN], a[INET_ADDRSTRLEN], d[INET_ADDRSTRLEN];

	if ((nat64_debug & DP_STATE) == 0)
		return;
	inet_ntop(AF_INET6, &pg->host->addr, s, sizeof(s));
	inet_ntop(AF_INET, &pg->aaddr, a, sizeof(a));
	inet_ntop(AF_INET, &st->u.s.faddr, d, sizeof(d));

	DPRINTF(DP_STATE, "%s: PG %d ST [%p|%d]: %s:%d/%d <%s:%d> "
	    "%s:%d AGE %d", px, pg->idx, st, off,
	    s, st->u.s.lport, pg->nat_proto, a, pg->aport + off,
	    d, st->u.s.fport, GET_AGE(st->timestamp));
}

/*
 * Check if particular TCP state is stale and should be deleted.
 * Return 1 if true, 0 otherwise.
 */
static int
nat64lsn_periodic_check_tcp(const struct nat64lsn_cfg *cfg,
    const struct nat64lsn_state *st, int age)
{
	int ttl;

	if (st->flags & NAT64_FLAG_FIN)
		ttl = cfg->st_close_ttl;
	else if (st->flags & NAT64_FLAG_ESTAB)
		ttl = cfg->st_estab_ttl;
	else if (st->flags & NAT64_FLAG_SYN)
		ttl = cfg->st_syn_ttl;
	else
		ttl = cfg->st_syn_ttl;

	if (age > ttl)
		return (1);
	return (0);
}

/*
 * Check if nat state @st is stale and should be deleted.
 * Return 1 if true, 0 otherwise.
 */
static NAT64NOINLINE int
nat64lsn_periodic_chkstate(const struct nat64lsn_cfg *cfg,
    const struct nat64lsn_portgroup *pg, const struct nat64lsn_state *st)
{
	int age, delete;

	age = GET_AGE(st->timestamp);
	delete = 0;

	/* Skip immutable records */
	if (st->flags & NAT64_FLAG_RDR)
		return (0);

	switch (pg->nat_proto) {
		case NAT_PROTO_TCP:
			delete = nat64lsn_periodic_check_tcp(cfg, st, age);
			break;
		case NAT_PROTO_UDP:
			if (age > cfg->st_udp_ttl)
				delete = 1;
			break;
		case NAT_PROTO_ICMP:
			if (age > cfg->st_icmp_ttl)
				delete = 1;
			break;
	}

	return (delete);
}


/*
 * The following structures and functions
 * are used to perform SLIST_FOREACH_SAFE()
 * analog for states identified by struct st_ptr.
 */

struct st_idx {
	struct nat64lsn_portgroup *pg;
	struct nat64lsn_state *st;
	struct st_ptr sidx_next;
};

static struct st_idx *
st_first(const struct nat64lsn_cfg *cfg, const struct nat64lsn_host *nh,
    struct st_ptr *sidx, struct st_idx *si)
{
	struct nat64lsn_portgroup *pg;
	struct nat64lsn_state *st;

	if (sidx->idx == 0) {
		memset(si, 0, sizeof(*si));
		return (si);
	}

	pg = PORTGROUP_BYSIDX(cfg, nh, sidx->idx);
	st = &pg->states[sidx->off];

	si->pg = pg;
	si->st = st;
	si->sidx_next = st->next;

	return (si);
}

static struct st_idx *
st_next(const struct nat64lsn_cfg *cfg, const struct nat64lsn_host *nh,
    struct st_idx *si)
{
	struct st_ptr sidx;
	struct nat64lsn_portgroup *pg;
	struct nat64lsn_state *st;

	sidx = si->sidx_next;
	if (sidx.idx == 0) {
		memset(si, 0, sizeof(*si));
		si->st = NULL;
		si->pg = NULL;
		return (si);
	}

	pg = PORTGROUP_BYSIDX(cfg, nh, sidx.idx);
	st = &pg->states[sidx.off];

	si->pg = pg;
	si->st = st;
	si->sidx_next = st->next;

	return (si);
}

static struct st_idx *
st_save_cond(struct st_idx *si_dst, struct st_idx *si)
{
	if (si->st != NULL)
		*si_dst = *si;

	return (si_dst);
}

unsigned int
nat64lsn_periodic_chkstates(struct nat64lsn_cfg *cfg, struct nat64lsn_host *nh)
{
	struct st_idx si, si_prev;
	int i;
	unsigned int delcount;

	delcount = 0;
	for (i = 0; i < nh->hsize; i++) {
		memset(&si_prev, 0, sizeof(si_prev));
		for (st_first(cfg, nh, &nh->phash[i], &si);
		    si.st != NULL;
		    st_save_cond(&si_prev, &si), st_next(cfg, nh, &si)) {
			if (nat64lsn_periodic_chkstate(cfg, si.pg, si.st) == 0)
				continue;
			nat64lsn_dump_state(cfg, si.pg, si.st, "DELETE STATE",
			    si.st->cur.off);
			/* Unlink from hash */
			if (si_prev.st != NULL)
				si_prev.st->next = si.st->next;
			else
				nh->phash[i] = si.st->next;
			/* Delete state and free its data */
			PG_MARK_FREE_IDX(si.pg, si.st->cur.off);
			memset(si.st, 0, sizeof(struct nat64lsn_state));
			si.st = NULL;
			delcount++;

			/* Update portgroup timestamp */
			SET_AGE(si.pg->timestamp);
		}
	}
	NAT64STAT_ADD(&cfg->stats, sdeleted, delcount);
	return (delcount);
}

/*
 * Checks if portgroup is not used and can be deleted,
 * Returns 1 if stale, 0 otherwise
 */
static int
stale_pg(const struct nat64lsn_cfg *cfg, const struct nat64lsn_portgroup *pg)
{

	if (!PG_IS_EMPTY(pg))
		return (0);
	if (GET_AGE(pg->timestamp) < cfg->pg_delete_delay)
		return (0);
	return (1);
}

/*
 * Checks if host record is not used and can be deleted,
 * Returns 1 if stale, 0 otherwise
 */
static int
stale_nh(const struct nat64lsn_cfg *cfg, const struct nat64lsn_host *nh)
{

	if (nh->pg_used != 0)
		return (0);
	if (GET_AGE(nh->timestamp) < cfg->nh_delete_delay)
		return (0);
	return (1);
}

struct nat64lsn_periodic_data {
	struct nat64lsn_cfg *cfg;
	struct nat64lsn_job_head jhead;
	int jlen;
};

static NAT64NOINLINE int
nat64lsn_periodic_chkhost(struct nat64lsn_host *nh,
    struct nat64lsn_periodic_data *d)
{
	char a[INET6_ADDRSTRLEN];
	struct nat64lsn_portgroup *pg;
	struct nat64lsn_job_item *ji;
	uint64_t delmask[NAT64LSN_PGPTRNMASK];
	int delcount, i;

	delcount = 0;
	memset(delmask, 0, sizeof(delmask));

	inet_ntop(AF_INET6, &nh->addr, a, sizeof(a));
	DPRINTF(DP_JQUEUE, "Checking %s host %s on cpu %d",
	    stale_nh(d->cfg, nh) ? "stale" : "non-stale", a, curcpu);
	if (!stale_nh(d->cfg, nh)) {
		/* Non-stale host. Inspect internals */
		NAT64_LOCK(nh);

		/* Stage 1: Check&expire states */
		if (nat64lsn_periodic_chkstates(d->cfg, nh) != 0)
			SET_AGE(nh->timestamp);

		/* Stage 2: Check if we need to expire */
		for (i = 0; i < nh->pg_used; i++) {
			pg = PORTGROUP_BYSIDX(d->cfg, nh, i + 1);
			if (pg == NULL)
				continue;

			/* Check if we can delete portgroup */
			if (stale_pg(d->cfg, pg) == 0)
				continue;

			DPRINTF(DP_JQUEUE, "Check PG %d", i);
			delmask[i / 64] |= ((uint64_t)1 << (i % 64));
			delcount++;
		}

		NAT64_UNLOCK(nh);
		if (delcount == 0)
			return (0);
	}

	DPRINTF(DP_JQUEUE, "Queueing %d portgroups for deleting", delcount);
	/* We have something to delete - add it to queue */
	ji = nat64lsn_create_job(d->cfg, NULL, JTYPE_DELPORTGROUP);
	if (ji == NULL)
		return (0);

	ji->haddr = nh->addr;
	ji->delcount = delcount;
	memcpy(ji->delmask, delmask, sizeof(ji->delmask));

	TAILQ_INSERT_TAIL(&d->jhead, ji, next);
	d->jlen++;
	return (0);
}

/*
 * This procedure is used to perform various maintance
 * on dynamic hash list. Currently it is called every second.
 */
static void
nat64lsn_periodic(void *data)
{
	struct ip_fw_chain *ch;
	IPFW_RLOCK_TRACKER;
	struct nat64lsn_cfg *cfg;
	struct nat64lsn_periodic_data d;
	struct nat64lsn_host *nh, *tmp;

	cfg = (struct nat64lsn_cfg *) data;
	ch = cfg->ch;
	CURVNET_SET(cfg->vp);

	memset(&d, 0, sizeof(d));
	d.cfg = cfg;
	TAILQ_INIT(&d.jhead);

	IPFW_RLOCK(ch);

	/* Stage 1: foreach host, check all its portgroups */
	I6HASH_FOREACH_SAFE(cfg, nh, tmp, nat64lsn_periodic_chkhost, &d);

	/* Enqueue everything we have requested */
	nat64lsn_enqueue_jobs(cfg, &d.jhead, d.jlen);

	callout_schedule(&cfg->periodic, hz * PERIODIC_DELAY);

	IPFW_RUNLOCK(ch);

	CURVNET_RESTORE();
}

static NAT64NOINLINE void
reinject_mbuf(struct nat64lsn_cfg *cfg, struct nat64lsn_job_item *ji)
{

	if (ji->m == NULL)
		return;

	/* Request has failed or packet type is wrong */
	if (ji->f_id.addr_type != 6 || ji->done == 0) {
		m_freem(ji->m);
		ji->m = NULL;
		NAT64STAT_INC(&cfg->stats, dropped);
		DPRINTF(DP_DROPS, "mbuf dropped: type %d, done %d",
		    ji->jtype, ji->done);
		return;
	}

	/*
	 * XXX: Limit recursion level
	 */

	NAT64STAT_INC(&cfg->stats, jreinjected);
	DPRINTF(DP_JQUEUE, "Reinject mbuf");
	nat64lsn_translate6(cfg, &ji->f_id, &ji->m);
}

static void
destroy_portgroup(struct nat64lsn_portgroup *pg)
{

	DPRINTF(DP_OBJ, "DESTROY PORTGROUP %d %p", pg->idx, pg);
	uma_zfree(nat64lsn_pg_zone, pg);
}

static NAT64NOINLINE int
alloc_portgroup(struct nat64lsn_job_item *ji)
{
	struct nat64lsn_portgroup *pg;

	pg = uma_zalloc(nat64lsn_pg_zone, M_NOWAIT);
	if (pg == NULL)
		return (1);

	if (ji->needs_idx != 0) {
		ji->spare_idx = uma_zalloc(nat64lsn_pgidx_zone, M_NOWAIT);
		/* Failed alloc isn't always fatal, so don't check */
	}
	memset(&pg->freemask, 0xFF, sizeof(pg->freemask));
	pg->nat_proto = ji->nat_proto;
	ji->pg = pg;
	return (0);

}

static void
destroy_host6(struct nat64lsn_host *nh)
{
	char a[INET6_ADDRSTRLEN];
	int i;

	inet_ntop(AF_INET6, &nh->addr, a, sizeof(a));
	DPRINTF(DP_OBJ, "DESTROY HOST %s %p (pg used %d)", a, nh,
	    nh->pg_used);
	NAT64_LOCK_DESTROY(nh);
	for (i = 0; i < nh->pg_allocated / NAT64LSN_PGIDX_CHUNK; i++)
		uma_zfree(nat64lsn_pgidx_zone, PORTGROUP_CHUNK(nh, i));
	uma_zfree(nat64lsn_host_zone, nh);
}

static NAT64NOINLINE int
alloc_host6(struct nat64lsn_cfg *cfg, struct nat64lsn_job_item *ji)
{
	struct nat64lsn_host *nh;
	char a[INET6_ADDRSTRLEN];

	nh = uma_zalloc(nat64lsn_host_zone, M_NOWAIT);
	if (nh == NULL)
		return (1);
	PORTGROUP_CHUNK(nh, 0) = uma_zalloc(nat64lsn_pgidx_zone, M_NOWAIT);
	if (PORTGROUP_CHUNK(nh, 0) == NULL) {
		uma_zfree(nat64lsn_host_zone, nh);
		return (2);
	}
	if (alloc_portgroup(ji) != 0) {
		NAT64STAT_INC(&cfg->stats, jportfails);
		uma_zfree(nat64lsn_pgidx_zone, PORTGROUP_CHUNK(nh, 0));
		uma_zfree(nat64lsn_host_zone, nh);
		return (3);
	}

	NAT64_LOCK_INIT(nh);
	nh->addr = ji->haddr;
	nh->hsize = NAT64LSN_HSIZE; /* XXX: hardcoded size */
	nh->pg_allocated = NAT64LSN_PGIDX_CHUNK;
	nh->pg_used = 0;
	ji->nh = nh;

	inet_ntop(AF_INET6, &nh->addr, a, sizeof(a));
	DPRINTF(DP_OBJ, "ALLOC HOST %s %p", a, ji->nh);
	return (0);
}

/*
 * Finds free @pg index inside @nh
 */
static NAT64NOINLINE int
find_nh_pg_idx(struct nat64lsn_cfg *cfg, struct nat64lsn_host *nh, int *idx)
{
	int i;

	for (i = 0; i < nh->pg_allocated; i++) {
		if (PORTGROUP_BYSIDX(cfg, nh, i + 1) == NULL) {
			*idx = i;
			return (0);
		}
	}
	return (1);
}

static NAT64NOINLINE int
attach_host6(struct nat64lsn_cfg *cfg, struct nat64lsn_job_item *ji)
{
	char a[INET6_ADDRSTRLEN];
	struct nat64lsn_host *nh;

	I6HASH_FIND(cfg, nh, &ji->haddr);
	if (nh == NULL) {
		/* Add new host to list */
		nh = ji->nh;
		I6HASH_INSERT(cfg, nh);
		cfg->ihcount++;
		ji->nh = NULL;

		inet_ntop(AF_INET6, &nh->addr, a, sizeof(a));
		DPRINTF(DP_OBJ, "ATTACH HOST %s %p", a, nh);
		/*
		 * Try to add portgroup.
		 * Note it will automatically set
		 * 'done' on ji if successful.
		 */
		if (attach_portgroup(cfg, ji) != 0) {
			DPRINTF(DP_DROPS, "%s %p failed to attach PG",
			    a, nh);
			NAT64STAT_INC(&cfg->stats, jportfails);
			return (1);
		}
		return (0);
	}

	/*
	 * nh isn't NULL. This probably means we had several simultaneous
	 * host requests. The previous one request has already attached
	 * this host. Requeue attached mbuf and mark job as done, but
	 * leave nh and pg pointers not changed, so nat64lsn_do_request()
	 * will release all allocated resources.
	 */
	inet_ntop(AF_INET6, &nh->addr, a, sizeof(a));
	DPRINTF(DP_OBJ, "%s %p is already attached as %p",
	    a, ji->nh, nh);
	ji->done = 1;
	return (0);
}

static NAT64NOINLINE int
find_pg_place_addr(const struct nat64lsn_cfg *cfg, int addr_off,
    int nat_proto, uint16_t *aport, int *ppg_idx)
{
	int j, pg_idx;

	pg_idx = addr_off * _ADDR_PG_COUNT +
	    (nat_proto - 1) * _ADDR_PG_PROTO_COUNT;

	for (j = NAT64_MIN_CHUNK; j < _ADDR_PG_PROTO_COUNT; j++) {
		if (cfg->pg[pg_idx + j] != NULL)
			continue;

		*aport = j * NAT64_CHUNK_SIZE;
		*ppg_idx = pg_idx + j;
		return (1);
	}

	return (0);
}

/*
 * XXX: This function needs to be rewritten to
 * use free bitmask for faster pg finding,
 * additionally, it should take into consideration
 * a) randomization and
 * b) previous addresses allocated to given nat instance
 *
 */
static NAT64NOINLINE int
find_portgroup_place(struct nat64lsn_cfg *cfg, struct nat64lsn_job_item *ji,
    uint32_t *aaddr, uint16_t *aport, int *ppg_idx)
{
	int i, nat_proto;

	/*
	 * XXX: Use bitmask index to be able to find/check if IP address
	 * has some spare pg's
	 */
	nat_proto = ji->nat_proto;

	/* First, try to use same address */
	if (ji->aaddr != 0) {
		i = ntohl(ji->aaddr) - cfg->prefix4;
		if (find_pg_place_addr(cfg, i, nat_proto, aport,
		    ppg_idx) != 0){
			/* Found! */
			*aaddr = htonl(cfg->prefix4 + i);
			return (0);
		}
	}

	/* Next, try to use random address based on flow hash */
	i = ji->fhash % (1 << (32 - cfg->plen4));
	if (find_pg_place_addr(cfg, i, nat_proto, aport, ppg_idx) != 0) {
		/* Found! */
		*aaddr = htonl(cfg->prefix4 + i);
		return (0);
	}


	/* Last one: simply find ANY available */
	for (i = 0; i < (1 << (32 - cfg->plen4)); i++) {
		if (find_pg_place_addr(cfg, i, nat_proto, aport,
		    ppg_idx) != 0){
			/* Found! */
			*aaddr = htonl(cfg->prefix4 + i);
			return (0);
		}
	}

	return (1);
}

static NAT64NOINLINE int
attach_portgroup(struct nat64lsn_cfg *cfg, struct nat64lsn_job_item *ji)
{
	char a[INET6_ADDRSTRLEN];
	struct nat64lsn_portgroup *pg;
	struct nat64lsn_host *nh;
	uint32_t aaddr;
	uint16_t aport;
	int nh_pg_idx, pg_idx;

	pg = ji->pg;

	/*
	 * Find source host and bind: we can't rely on
	 * pg->host
	 */
	I6HASH_FIND(cfg, nh, &ji->haddr);
	if (nh == NULL)
		return (1);

	/* Find spare port chunk */
	if (find_portgroup_place(cfg, ji, &aaddr, &aport, &pg_idx) != 0) {
		inet_ntop(AF_INET6, &nh->addr, a, sizeof(a));
		DPRINTF(DP_OBJ | DP_DROPS, "empty PG not found for %s", a);
		return (2);
	}

	/* Expand PG indexes if needed */
	if (nh->pg_allocated < cfg->max_chunks && ji->spare_idx != NULL) {
		PORTGROUP_CHUNK(nh, nh->pg_allocated / NAT64LSN_PGIDX_CHUNK) =
		    ji->spare_idx;
		nh->pg_allocated += NAT64LSN_PGIDX_CHUNK;
		ji->spare_idx = NULL;
	}

	/* Find empty index to store PG in the @nh */
	if (find_nh_pg_idx(cfg, nh, &nh_pg_idx) != 0) {
		inet_ntop(AF_INET6, &nh->addr, a, sizeof(a));
		DPRINTF(DP_OBJ | DP_DROPS, "free PG index not found for %s",
		    a);
		return (3);
	}

	cfg->pg[pg_idx] = pg;
	cfg->protochunks[pg->nat_proto]++;
	NAT64STAT_INC(&cfg->stats, spgcreated);

	pg->aaddr = aaddr;
	pg->aport = aport;
	pg->host = nh;
	pg->idx = pg_idx;
	SET_AGE(pg->timestamp);

	PORTGROUP_BYSIDX(cfg, nh, nh_pg_idx + 1) = pg;
	if (nh->pg_used == nh_pg_idx)
		nh->pg_used++;
	SET_AGE(nh->timestamp);

	ji->pg = NULL;
	ji->done = 1;

	return (0);
}

static NAT64NOINLINE void
consider_del_portgroup(struct nat64lsn_cfg *cfg, struct nat64lsn_job_item *ji)
{
	struct nat64lsn_host *nh, *nh_tmp;
	struct nat64lsn_portgroup *pg, *pg_list[256];
	int i, pg_lidx, idx;

	/* Find source host */
	I6HASH_FIND(cfg, nh, &ji->haddr);
	if (nh == NULL || nh->pg_used == 0)
		return;

	memset(pg_list, 0, sizeof(pg_list));
	pg_lidx = 0;

	NAT64_LOCK(nh);

	for (i = nh->pg_used - 1; i >= 0; i--) {
		if ((ji->delmask[i / 64] & ((uint64_t)1 << (i % 64))) == 0)
			continue;
		pg = PORTGROUP_BYSIDX(cfg, nh, i + 1);

		/* Check that PG isn't busy. */
		if (stale_pg(cfg, pg) == 0)
			continue;

		/* DO delete */
		pg_list[pg_lidx++] = pg;
		PORTGROUP_BYSIDX(cfg, nh, i + 1) = NULL;

		idx = _GET_PORTGROUP_IDX(cfg, ntohl(pg->aaddr), pg->nat_proto,
		    pg->aport);
		KASSERT(cfg->pg[idx] == pg, ("Non matched pg"));
		cfg->pg[idx] = NULL;
		cfg->protochunks[pg->nat_proto]--;
		NAT64STAT_INC(&cfg->stats, spgdeleted);

		/* Decrease pg_used */
		while (nh->pg_used > 0 &&
		    PORTGROUP_BYSIDX(cfg, nh, nh->pg_used) == NULL)
			nh->pg_used--;

		/* Check if on-stack buffer has ended */
		if (pg_lidx == nitems(pg_list))
			break;
	}

	NAT64_UNLOCK(nh);

	if (stale_nh(cfg, nh)) {
		I6HASH_REMOVE(cfg, nh, nh_tmp, &ji->haddr);
		KASSERT(nh != NULL, ("Unable to find address"));
		cfg->ihcount--;
		ji->nh = nh;
		I6HASH_FIND(cfg, nh, &ji->haddr);
		KASSERT(nh == NULL, ("Failed to delete address"));
	}

	/* TODO: Delay freeing portgroups */
	while (pg_lidx > 0) {
		pg_lidx--;
		NAT64STAT_INC(&cfg->stats, spgdeleted);
		destroy_portgroup(pg_list[pg_lidx]);
	}
}

/*
 * Main request handler.
 * Responsible for handling jqueue, e.g.
 * creating new hosts, addind/deleting portgroups.
 */
static NAT64NOINLINE void
nat64lsn_do_request(void *data) 
{
	IPFW_RLOCK_TRACKER;
	struct nat64lsn_job_head jhead;
	struct nat64lsn_job_item *ji;
	int jcount, nhsize;
	struct nat64lsn_cfg *cfg = (struct nat64lsn_cfg *) data;
	struct ip_fw_chain *ch;
	int delcount;

	CURVNET_SET(cfg->vp);

	TAILQ_INIT(&jhead);

	/* XXX: We're running unlocked here */

	ch = cfg->ch;
	delcount = 0;
	IPFW_RLOCK(ch);

	/* Grab queue */
	JQUEUE_LOCK();
	TAILQ_SWAP(&jhead, &cfg->jhead, nat64lsn_job_item, next);
	jcount = cfg->jlen;
	cfg->jlen = 0;
	JQUEUE_UNLOCK();

	/* check if we need to resize hash */
	nhsize = 0;
	if (cfg->ihcount > cfg->ihsize && cfg->ihsize < 65536) {
		nhsize = cfg->ihsize;
		for ( ; cfg->ihcount > nhsize && nhsize < 65536; nhsize *= 2)
			;
	} else if (cfg->ihcount < cfg->ihsize * 4) {
		nhsize = cfg->ihsize;
		for ( ; cfg->ihcount < nhsize * 4 && nhsize > 32; nhsize /= 2)
			;
	}

	IPFW_RUNLOCK(ch);

	if (TAILQ_EMPTY(&jhead)) {
		CURVNET_RESTORE();
		return;
	}

	NAT64STAT_INC(&cfg->stats, jcalls);
	DPRINTF(DP_JQUEUE, "count=%d", jcount);

	/*
	 * TODO:
	 * What we should do here is to build a hash
	 * to ensure we don't have lots of duplicate requests.
	 * Skip this for now.
	 *
	 * TODO: Limit per-call number of items
	 */

	/* Pre-allocate everything for entire chain */
	TAILQ_FOREACH(ji, &jhead,  next) {
		switch (ji->jtype) {
			case JTYPE_NEWHOST:
				if (alloc_host6(cfg, ji) != 0)
					NAT64STAT_INC(&cfg->stats, jhostfails);
				break;
			case JTYPE_NEWPORTGROUP:
				if (alloc_portgroup(ji) != 0)
					NAT64STAT_INC(&cfg->stats, jportfails);
				break;
			case JTYPE_DELPORTGROUP:
				delcount += ji->delcount;
				break;
			default:
				break;
		}
	}

	/*
	 * TODO: Alloc hew hash
	 */
	nhsize = 0;
	if (nhsize > 0) {
		/* XXX: */
	}

	/* Apply all changes in batch */
	IPFW_UH_WLOCK(ch);
	IPFW_WLOCK(ch);

	TAILQ_FOREACH(ji, &jhead,  next) {
		switch (ji->jtype) {
			case JTYPE_NEWHOST:
				if (ji->nh != NULL)
					attach_host6(cfg, ji);
				break;
			case JTYPE_NEWPORTGROUP:
				if (ji->pg != NULL &&
				    attach_portgroup(cfg, ji) != 0)
					NAT64STAT_INC(&cfg->stats, jportfails);
				break;
			case JTYPE_DELPORTGROUP:
				consider_del_portgroup(cfg, ji);
				break;
		}
	}

	if (nhsize > 0) {
		/* XXX: Move everything to new hash */
	}

	IPFW_WUNLOCK(ch);
	IPFW_UH_WUNLOCK(ch);

	/* Flush unused entries */
	while (!TAILQ_EMPTY(&jhead)) {
		ji = TAILQ_FIRST(&jhead);
		TAILQ_REMOVE(&jhead, ji, next);
		if (ji->nh != NULL)
			destroy_host6(ji->nh);
		if (ji->pg != NULL)
			destroy_portgroup(ji->pg);
		if (ji->m != NULL)
			reinject_mbuf(cfg, ji);
		if (ji->spare_idx != NULL)
			uma_zfree(nat64lsn_pgidx_zone, ji->spare_idx);
		free(ji, M_IPFW);
	}
	CURVNET_RESTORE();
}

static NAT64NOINLINE struct nat64lsn_job_item *
nat64lsn_create_job(struct nat64lsn_cfg *cfg, const struct ipfw_flow_id *f_id,
    int jtype)
{
	struct nat64lsn_job_item *ji;
	struct in6_addr haddr;
	uint8_t nat_proto;

	/*
	 * Do not try to lock possibly contested mutex if we're near the limit.
	 * Drop packet instead.
	 */
	if (cfg->jlen >= cfg->jmaxlen) {
		NAT64STAT_INC(&cfg->stats, jmaxlen);
		return (NULL);
	}

	memset(&haddr, 0, sizeof(haddr));
	nat_proto = 0;
	if (f_id != NULL) {
		haddr = f_id->src_ip6;
		nat_proto = nat64lsn_proto_map[f_id->proto];

		DPRINTF(DP_JQUEUE, "REQUEST pg nat_proto %d on proto %d",
		    nat_proto, f_id->proto);

		if (nat_proto == 0)
			return (NULL);
	}

	ji = malloc(sizeof(struct nat64lsn_job_item), M_IPFW,
	    M_NOWAIT | M_ZERO);

	if (ji == NULL) {
		NAT64STAT_INC(&cfg->stats, jnomem);
		return (NULL);
	}

	ji->jtype = jtype;

	if (f_id != NULL) {
		ji->f_id = *f_id;
		ji->haddr = haddr;
		ji->nat_proto = nat_proto;
	}

	return (ji);
}

static NAT64NOINLINE void
nat64lsn_enqueue_job(struct nat64lsn_cfg *cfg, struct nat64lsn_job_item *ji)
{

	if (ji == NULL)
		return;

	JQUEUE_LOCK();
	TAILQ_INSERT_TAIL(&cfg->jhead, ji, next);
	cfg->jlen++;
	NAT64STAT_INC(&cfg->stats, jrequests);

	if (callout_pending(&cfg->jcallout) == 0)
		callout_reset(&cfg->jcallout, 1, nat64lsn_do_request, cfg);
	JQUEUE_UNLOCK();
}

static NAT64NOINLINE void
nat64lsn_enqueue_jobs(struct nat64lsn_cfg *cfg,
    struct nat64lsn_job_head *jhead, int jlen)
{

	if (TAILQ_EMPTY(jhead))
		return;

	/* Attach current queue to execution one */
	JQUEUE_LOCK();
	TAILQ_CONCAT(&cfg->jhead, jhead, next);
	cfg->jlen += jlen;
	NAT64STAT_ADD(&cfg->stats, jrequests, jlen);

	if (callout_pending(&cfg->jcallout) == 0)
		callout_reset(&cfg->jcallout, 1, nat64lsn_do_request, cfg);
	JQUEUE_UNLOCK();
}

static unsigned int
flow6_hash(const struct ipfw_flow_id *f_id)
{
	unsigned char hbuf[36];

	memcpy(hbuf, &f_id->dst_ip6, 16);
	memcpy(&hbuf[16], &f_id->src_ip6, 16);
	memcpy(&hbuf[32], &f_id->dst_port, 2);
	memcpy(&hbuf[32], &f_id->src_port, 2);

	return (djb_hash(hbuf, sizeof(hbuf)));
}

static NAT64NOINLINE int
nat64lsn_request_host(struct nat64lsn_cfg *cfg,
    const struct ipfw_flow_id *f_id, struct mbuf **pm)
{
	struct nat64lsn_job_item *ji;
	struct mbuf *m;

	m = *pm;
	*pm = NULL;

	ji = nat64lsn_create_job(cfg, f_id, JTYPE_NEWHOST);
	if (ji == NULL) {
		m_freem(m);
		NAT64STAT_INC(&cfg->stats, dropped);
		DPRINTF(DP_DROPS, "failed to create job");
	} else {
		ji->m = m;
		/* Provide pseudo-random value based on flow */
		ji->fhash = flow6_hash(f_id);
		nat64lsn_enqueue_job(cfg, ji);
		NAT64STAT_INC(&cfg->stats, jhostsreq);
	}

	return (IP_FW_DENY);
}

static NAT64NOINLINE int
nat64lsn_request_portgroup(struct nat64lsn_cfg *cfg,
    const struct ipfw_flow_id *f_id, struct mbuf **pm, uint32_t aaddr,
    int needs_idx)
{
	struct nat64lsn_job_item *ji;
	struct mbuf *m;

	m = *pm;
	*pm = NULL;

	ji = nat64lsn_create_job(cfg, f_id, JTYPE_NEWPORTGROUP);
	if (ji == NULL) {
		m_freem(m);
		NAT64STAT_INC(&cfg->stats, dropped);
		DPRINTF(DP_DROPS, "failed to create job");
	} else {
		ji->m = m;
		/* Provide pseudo-random value based on flow */
		ji->fhash = flow6_hash(f_id);
		ji->aaddr = aaddr;
		ji->needs_idx = needs_idx;
		nat64lsn_enqueue_job(cfg, ji);
		NAT64STAT_INC(&cfg->stats, jportreq);
	}

	return (IP_FW_DENY);
}

static NAT64NOINLINE struct nat64lsn_state * 
nat64lsn_create_state(struct nat64lsn_cfg *cfg, struct nat64lsn_host *nh,
    int nat_proto, struct nat64lsn_state *kst, uint32_t *aaddr)
{
	struct nat64lsn_portgroup *pg;
	struct nat64lsn_state *st;
	int i, hval, off;

	/* XXX: create additional bitmask for selecting proper portgroup */
	for (i = 0; i < nh->pg_used; i++) {
		pg = PORTGROUP_BYSIDX(cfg, nh, i + 1);
		if (pg == NULL)
			continue;
		if (*aaddr == 0)
			*aaddr = pg->aaddr;
		if (pg->nat_proto != nat_proto)
			continue;

		off = PG_GET_FREE_IDX(pg);
		if (off != 0) {
			/* We have found spare state. Use it */
			off--;
			PG_MARK_BUSY_IDX(pg, off);
			st = &pg->states[off];

			/*
			 * Fill in new info. Assume state was zeroed.
			 * Timestamp and flags will be filled by caller.
			 */
			st->u.s = kst->u.s;
			st->cur.idx = i + 1;
			st->cur.off = off;

			/* Insert into host hash table */
			hval = HASH_IN4(&st->u.hkey) & (nh->hsize - 1);
			st->next = nh->phash[hval];
			nh->phash[hval] = st->cur;

			nat64lsn_dump_state(cfg, pg, st, "ALLOC STATE", off);

			NAT64STAT_INC(&cfg->stats, screated);

			return (st);
		}
		/* Saev last used alias affress */
		*aaddr = pg->aaddr;
	}

	return (NULL);
}

static NAT64NOINLINE int
nat64lsn_translate6(struct nat64lsn_cfg *cfg, struct ipfw_flow_id *f_id,
    struct mbuf **pm)
{
	struct pfloghdr loghdr, *logdata;
	char a[INET6_ADDRSTRLEN];
	struct nat64lsn_host *nh;
	struct st_ptr sidx;
	struct nat64lsn_state *st, kst;
	struct nat64lsn_portgroup *pg;
	struct icmp6_hdr *icmp6;
	uint32_t aaddr;
	int action, hval, nat_proto, proto;
	uint16_t aport, state_ts, state_flags;

	/* Check if af/protocol is supported and get it short id */
	nat_proto = nat64lsn_proto_map[f_id->proto];
	if (nat_proto == 0) {
		/*
		 * Since we can be called from jobs handler, we need
		 * to free mbuf by self, do not leave this task to
		 * ipfw_check_packet().
		 */
		NAT64STAT_INC(&cfg->stats, noproto);
		m_freem(*pm);
		*pm = NULL;
		return (IP_FW_DENY);
	}

	/* Try to find host first */
	I6HASH_FIND(cfg, nh, &f_id->src_ip6);

	if (nh == NULL)
		return (nat64lsn_request_host(cfg, f_id, pm));

	/* Fill-in on-stack state structure */
	kst.u.s.faddr = f_id->dst_ip6.s6_addr32[3];
	kst.u.s.fport = f_id->dst_port;
	kst.u.s.lport = f_id->src_port;

	/* Prepare some fields we might need to update */
	hval = 0;
	proto = nat64_getlasthdr(*pm, &hval);
	if (proto < 0) {
		NAT64STAT_INC(&cfg->stats, dropped);
		DPRINTF(DP_DROPS, "dropped due to mbuf isn't contigious");
		m_freem(*pm);
		*pm = NULL;
		return (IP_FW_DENY);
	}

	SET_AGE(state_ts);
	if (proto == IPPROTO_TCP)
		state_flags = convert_tcp_flags(
		    TCP(mtodo(*pm, hval))->th_flags);
	else
		state_flags = 0;
	if (proto == IPPROTO_ICMPV6) {
		/* Alter local port data */
		icmp6 = mtodo(*pm, hval);
		if (icmp6->icmp6_type == ICMP6_ECHO_REQUEST ||
		    icmp6->icmp6_type == ICMP6_ECHO_REPLY)
			kst.u.s.lport = ntohs(icmp6->icmp6_id);
	}

	hval = HASH_IN4(&kst.u.hkey) & (nh->hsize - 1);
	pg = NULL;
	st = NULL;

	/* OK, let's find state in host hash */
	NAT64_LOCK(nh);
	sidx = nh->phash[hval];
	int k = 0;
	while (sidx.idx != 0) {
		pg = PORTGROUP_BYSIDX(cfg, nh, sidx.idx);
		st = &pg->states[sidx.off];
		//DPRINTF("SISX: %d/%d next: %d/%d", sidx.idx, sidx.off,
		//st->next.idx, st->next.off);
		if (st->u.hkey == kst.u.hkey && pg->nat_proto == nat_proto)
			break;
		if (k++ > 1000) {
			DPRINTF(DP_ALL, "XXX: too long %d/%d %d/%d\n",
			    sidx.idx, sidx.off, st->next.idx, st->next.off);
			inet_ntop(AF_INET6, &nh->addr, a, sizeof(a));
			DPRINTF(DP_GENERIC, "TR host %s %p on cpu %d",
			    a, nh, curcpu);
			k = 0;
		}
		sidx = st->next;
	}

	if (sidx.idx == 0) {
		aaddr = 0;
		st = nat64lsn_create_state(cfg, nh, nat_proto, &kst, &aaddr);
		if (st == NULL) {
			/* No free states. Request more if we can */
			if (nh->pg_used >= cfg->max_chunks) {
				/* Limit reached */
				NAT64STAT_INC(&cfg->stats, dropped);
				inet_ntop(AF_INET6, &nh->addr, a, sizeof(a));
				DPRINTF(DP_DROPS, "PG limit reached "
				    " for host %s (used %u, allocated %u, "
				    "limit %u)", a,
				    nh->pg_used * NAT64_CHUNK_SIZE,
				    nh->pg_allocated * NAT64_CHUNK_SIZE,
				    cfg->max_chunks * NAT64_CHUNK_SIZE);
				m_freem(*pm);
				*pm = NULL;
				NAT64_UNLOCK(nh);
				return (IP_FW_DENY);
			}
			if ((nh->pg_allocated <=
			    nh->pg_used + NAT64LSN_REMAININGPG) &&
			    nh->pg_allocated < cfg->max_chunks)
				action = 1; /* Request new indexes */
			else
				action = 0;
			NAT64_UNLOCK(nh);
			//DPRINTF("No state, unlock for %p", nh);
			return (nat64lsn_request_portgroup(cfg, f_id,
			    pm, aaddr, action));
		}

		/* We've got new state. */
		sidx = st->cur;
		pg = PORTGROUP_BYSIDX(cfg, nh, sidx.idx);
	}

	/* Okay, state found */

	/* Update necessary fileds */
	if (st->timestamp != state_ts)
		st->timestamp = state_ts;
	if ((st->flags & state_flags) != 0)
		st->flags |= state_flags;

	/* Copy needed state data */
	aaddr = pg->aaddr;
	aport = htons(pg->aport + sidx.off);

	NAT64_UNLOCK(nh);

	if (cfg->flags & NAT64_LOG) {
		logdata = &loghdr;
		nat64lsn_log(logdata, *pm, AF_INET6, pg->idx, st->cur.off);
	} else
		logdata = NULL;

	action = nat64_do_handle_ip6(*pm, aaddr, aport, &cfg->stats, logdata);
	if (action == NAT64SKIP)
		return (cfg->nomatch_verdict);
	if (action == NAT64MFREE)
		m_freem(*pm);
	*pm = NULL;	/* mark mbuf as consumed */
	return (IP_FW_DENY);
}

/*
 * Main dataplane entry point.
 */
int
ipfw_nat64lsn(struct ip_fw_chain *ch, struct ip_fw_args *args,
    ipfw_insn *cmd, int *done)
{
	ipfw_insn *icmd;
	struct nat64lsn_cfg *cfg;
	int ret;

	IPFW_RLOCK_ASSERT(ch);

	*done = 1; /* terminate the search */
	icmd = cmd + 1;
	if (cmd->opcode != O_EXTERNAL_ACTION ||
	    cmd->arg1 != V_nat64lsn_eid ||
	    icmd->opcode != O_EXTERNAL_INSTANCE ||
	    (cfg = NAT64_LOOKUP(ch, icmd)) == NULL)
		return (0);

	switch (args->f_id.addr_type) {
	case 4:
		ret = nat64lsn_translate4(cfg, &args->f_id, &args->m);
		break;
	case 6:
		ret = nat64lsn_translate6(cfg, &args->f_id, &args->m);
		break;
	default:
		return (cfg->nomatch_verdict);
	}
	return (ret);
}

static int
nat64lsn_ctor_host(void *mem, int size, void *arg, int flags)
{
	struct nat64lsn_host *nh;

	nh = (struct nat64lsn_host *)mem;
	memset(nh->pg_ptr, 0, sizeof(nh->pg_ptr));
	memset(nh->phash, 0, sizeof(nh->phash));
	return (0);
}

static int
nat64lsn_ctor_pgidx(void *mem, int size, void *arg, int flags)
{

	memset(mem, 0, size);
	return (0);
}

void
nat64lsn_init_internal(void)
{

	memset(nat64lsn_proto_map, 0, sizeof(nat64lsn_proto_map));
	/* Set up supported protocol map */
	nat64lsn_proto_map[IPPROTO_TCP] = NAT_PROTO_TCP;
	nat64lsn_proto_map[IPPROTO_UDP] = NAT_PROTO_UDP;
	nat64lsn_proto_map[IPPROTO_ICMP] = NAT_PROTO_ICMP;
	nat64lsn_proto_map[IPPROTO_ICMPV6] = NAT_PROTO_ICMP;
	/* Fill in reverse proto map */
	memset(nat64lsn_rproto_map, 0, sizeof(nat64lsn_rproto_map));
	nat64lsn_rproto_map[NAT_PROTO_TCP] = IPPROTO_TCP;
	nat64lsn_rproto_map[NAT_PROTO_UDP] = IPPROTO_UDP;
	nat64lsn_rproto_map[NAT_PROTO_ICMP] = IPPROTO_ICMPV6;

	JQUEUE_LOCK_INIT();
	nat64lsn_host_zone = uma_zcreate("NAT64 hosts zone",
	    sizeof(struct nat64lsn_host), nat64lsn_ctor_host, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);
	nat64lsn_pg_zone = uma_zcreate("NAT64 portgroups zone",
	    sizeof(struct nat64lsn_portgroup), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	nat64lsn_pgidx_zone = uma_zcreate("NAT64 portgroup indexes zone",
	    sizeof(struct nat64lsn_portgroup *) * NAT64LSN_PGIDX_CHUNK,
	    nat64lsn_ctor_pgidx, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
}

void
nat64lsn_uninit_internal(void)
{

	JQUEUE_LOCK_DESTROY();
	uma_zdestroy(nat64lsn_host_zone);
	uma_zdestroy(nat64lsn_pg_zone);
	uma_zdestroy(nat64lsn_pgidx_zone);
}

void
nat64lsn_start_instance(struct nat64lsn_cfg *cfg)
{

	callout_reset(&cfg->periodic, hz * PERIODIC_DELAY,
	    nat64lsn_periodic, cfg);
}

struct nat64lsn_cfg *
nat64lsn_init_instance(struct ip_fw_chain *ch, size_t numaddr)
{
	struct nat64lsn_cfg *cfg;

	cfg = malloc(sizeof(struct nat64lsn_cfg), M_IPFW, M_WAITOK | M_ZERO);
	TAILQ_INIT(&cfg->jhead);
	cfg->vp = curvnet;
	cfg->ch = ch;
	COUNTER_ARRAY_ALLOC(cfg->stats.stats, NAT64STATS, M_WAITOK);

	cfg->ihsize = NAT64LSN_HSIZE;
	cfg->ih = malloc(sizeof(void *) * cfg->ihsize, M_IPFW,
	    M_WAITOK | M_ZERO);

	cfg->pg = malloc(sizeof(void *) * numaddr * _ADDR_PG_COUNT, M_IPFW,
	    M_WAITOK | M_ZERO);

        callout_init(&cfg->periodic, CALLOUT_MPSAFE);
        callout_init(&cfg->jcallout, CALLOUT_MPSAFE);

	return (cfg);
}

/*
 * Destroy all hosts callback.
 * Called on module unload when all activity already finished, so
 * can work without any locks.
 */
static NAT64NOINLINE int
nat64lsn_destroy_host(struct nat64lsn_host *nh, struct nat64lsn_cfg *cfg)
{
	struct nat64lsn_portgroup *pg;
	int i;

	for (i = nh->pg_used; i > 0; i--) {
		pg = PORTGROUP_BYSIDX(cfg, nh, i);
		if (pg == NULL)
			continue;
		cfg->pg[pg->idx] = NULL;
		destroy_portgroup(pg);
		nh->pg_used--;
	}
	destroy_host6(nh);
	cfg->ihcount--;
	return (0);
}

void
nat64lsn_destroy_instance(struct nat64lsn_cfg *cfg)
{
	struct nat64lsn_host *nh, *tmp;

	callout_drain(&cfg->jcallout);
	callout_drain(&cfg->periodic);
	I6HASH_FOREACH_SAFE(cfg, nh, tmp, nat64lsn_destroy_host, cfg);
	DPRINTF(DP_OBJ, "instance %s: hosts %d", cfg->name, cfg->ihcount);

	COUNTER_ARRAY_FREE(cfg->stats.stats, NAT64STATS);
	free(cfg->ih, M_IPFW);
	free(cfg->pg, M_IPFW);
	free(cfg, M_IPFW);
}

