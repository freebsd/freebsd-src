/*-
 * Copyright (c) 2015 Yandex LLC
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
#include <sys/sockopt.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>

#include <netpfil/ipfw/ip_fw_private.h>
#include <netpfil/ipfw/nat64/ip_fw_nat64.h>
#include <netpfil/ipfw/nat64/nat64lsn.h>
#include <netinet6/ip_fw_nat64.h>

VNET_DEFINE(uint16_t, nat64lsn_eid) = 0;

static struct nat64lsn_cfg *
nat64lsn_find(struct namedobj_instance *ni, const char *name, uint8_t set)
{
	struct nat64lsn_cfg *cfg;

	cfg = (struct nat64lsn_cfg *)ipfw_objhash_lookup_name_type(ni, set,
	    IPFW_TLV_NAT64LSN_NAME, name);

	return (cfg);
}

static void
nat64lsn_default_config(ipfw_nat64lsn_cfg *uc)
{

	if (uc->max_ports == 0)
		uc->max_ports = NAT64LSN_MAX_PORTS;
	else
		uc->max_ports = roundup(uc->max_ports, NAT64_CHUNK_SIZE);
	if (uc->max_ports > NAT64_CHUNK_SIZE * NAT64LSN_MAXPGPTR)
		uc->max_ports = NAT64_CHUNK_SIZE * NAT64LSN_MAXPGPTR;
	if (uc->jmaxlen == 0)
		uc->jmaxlen = NAT64LSN_JMAXLEN;
	if (uc->jmaxlen > 65536)
		uc->jmaxlen = 65536;
	if (uc->nh_delete_delay == 0)
		uc->nh_delete_delay = NAT64LSN_HOST_AGE;
	if (uc->pg_delete_delay == 0)
		uc->pg_delete_delay = NAT64LSN_PG_AGE;
	if (uc->st_syn_ttl == 0)
		uc->st_syn_ttl = NAT64LSN_TCP_SYN_AGE;
	if (uc->st_close_ttl == 0)
		uc->st_close_ttl = NAT64LSN_TCP_FIN_AGE;
	if (uc->st_estab_ttl == 0)
		uc->st_estab_ttl = NAT64LSN_TCP_EST_AGE;
	if (uc->st_udp_ttl == 0)
		uc->st_udp_ttl = NAT64LSN_UDP_AGE;
	if (uc->st_icmp_ttl == 0)
		uc->st_icmp_ttl = NAT64LSN_ICMP_AGE;
}

/*
 * Creates new nat64lsn instance.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_lheader ipfw_nat64lsn_cfg ]
 *
 * Returns 0 on success
 */
static int
nat64lsn_create(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	ipfw_obj_lheader *olh;
	ipfw_nat64lsn_cfg *uc;
	struct nat64lsn_cfg *cfg;
	struct namedobj_instance *ni;
	uint32_t addr4, mask4;

	if (sd->valsize != sizeof(*olh) + sizeof(*uc))
		return (EINVAL);

	olh = (ipfw_obj_lheader *)sd->kbuf;
	uc = (ipfw_nat64lsn_cfg *)(olh + 1);

	if (ipfw_check_object_name_generic(uc->name) != 0)
		return (EINVAL);

	if (uc->agg_prefix_len > 127 || uc->set >= IPFW_MAX_SETS)
		return (EINVAL);

	if (uc->plen4 > 32)
		return (EINVAL);
	if (uc->plen6 > 128 || ((uc->plen6 % 8) != 0))
		return (EINVAL);

	/* XXX: Check prefix4 to be global */
	addr4 = ntohl(uc->prefix4.s_addr);
	mask4 = ~((1 << (32 - uc->plen4)) - 1);
	if ((addr4 & mask4) != addr4)
		return (EINVAL);

	/* XXX: Check prefix6 */
	if (uc->min_port == 0)
		uc->min_port = NAT64_MIN_PORT;
	if (uc->max_port == 0)
		uc->max_port = 65535;
	if (uc->min_port > uc->max_port)
		return (EINVAL);
	uc->min_port = roundup(uc->min_port, NAT64_CHUNK_SIZE);
	uc->max_port = roundup(uc->max_port, NAT64_CHUNK_SIZE);

	nat64lsn_default_config(uc);

	ni = CHAIN_TO_SRV(ch);
	IPFW_UH_RLOCK(ch);
	if (nat64lsn_find(ni, uc->name, uc->set) != NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (EEXIST);
	}
	IPFW_UH_RUNLOCK(ch);

	cfg = nat64lsn_init_instance(ch, 1 << (32 - uc->plen4));
	strlcpy(cfg->name, uc->name, sizeof(cfg->name));
	cfg->no.name = cfg->name;
	cfg->no.etlv = IPFW_TLV_NAT64LSN_NAME;
	cfg->no.set = uc->set;

	cfg->prefix4 = addr4;
	cfg->pmask4 = addr4 | ~mask4;
	/* XXX: Copy 96 bits */
	cfg->plen6 = 96;
	memcpy(&cfg->prefix6, &uc->prefix6, cfg->plen6 / 8);
	cfg->plen4 = uc->plen4;
	cfg->flags = uc->flags & NAT64LSN_FLAGSMASK;
	cfg->max_chunks = uc->max_ports / NAT64_CHUNK_SIZE;
	cfg->agg_prefix_len = uc->agg_prefix_len;
	cfg->agg_prefix_max = uc->agg_prefix_max;

	cfg->min_chunk = uc->min_port / NAT64_CHUNK_SIZE;
	cfg->max_chunk = uc->max_port / NAT64_CHUNK_SIZE;

	cfg->jmaxlen = uc->jmaxlen;
	cfg->nh_delete_delay = uc->nh_delete_delay;
	cfg->pg_delete_delay = uc->pg_delete_delay;
	cfg->st_syn_ttl = uc->st_syn_ttl;
	cfg->st_close_ttl = uc->st_close_ttl;
	cfg->st_estab_ttl = uc->st_estab_ttl;
	cfg->st_udp_ttl = uc->st_udp_ttl;
	cfg->st_icmp_ttl = uc->st_icmp_ttl;

	cfg->nomatch_verdict = IP_FW_DENY;
	cfg->nomatch_final = 1;	/* Exit outer loop by default */

	IPFW_UH_WLOCK(ch);

	if (nat64lsn_find(ni, uc->name, uc->set) != NULL) {
		IPFW_UH_WUNLOCK(ch);
		nat64lsn_destroy_instance(cfg);
		return (EEXIST);
	}

	if (ipfw_objhash_alloc_idx(CHAIN_TO_SRV(ch), &cfg->no.kidx) != 0) {
		IPFW_UH_WUNLOCK(ch);
		nat64lsn_destroy_instance(cfg);
		return (ENOSPC);
	}
	ipfw_objhash_add(CHAIN_TO_SRV(ch), &cfg->no);

	/* Okay, let's link data */
	IPFW_WLOCK(ch);
	SRV_OBJECT(ch, cfg->no.kidx) = cfg;
	IPFW_WUNLOCK(ch);

	nat64lsn_start_instance(cfg);

	IPFW_UH_WUNLOCK(ch);
	return (0);
}

static void
nat64lsn_detach_config(struct ip_fw_chain *ch, struct nat64lsn_cfg *cfg)
{

	IPFW_UH_WLOCK_ASSERT(ch);

	ipfw_objhash_del(CHAIN_TO_SRV(ch), &cfg->no);
	ipfw_objhash_free_idx(CHAIN_TO_SRV(ch), cfg->no.kidx);
}

/*
 * Destroys nat64 instance.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_header ]
 *
 * Returns 0 on success
 */
static int
nat64lsn_destroy(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	struct nat64lsn_cfg *cfg;
	ipfw_obj_header *oh;

	if (sd->valsize != sizeof(*oh))
		return (EINVAL);

	oh = (ipfw_obj_header *)op3;

	IPFW_UH_WLOCK(ch);
	cfg = nat64lsn_find(CHAIN_TO_SRV(ch), oh->ntlv.name, oh->ntlv.set);
	if (cfg == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (ESRCH);
	}

	if (cfg->no.refcnt > 0) {
		IPFW_UH_WUNLOCK(ch);
		return (EBUSY);
	}

	IPFW_WLOCK(ch);
	SRV_OBJECT(ch, cfg->no.kidx) = NULL;
	IPFW_WUNLOCK(ch);

	nat64lsn_detach_config(ch, cfg);
	IPFW_UH_WUNLOCK(ch);

	nat64lsn_destroy_instance(cfg);
	return (0);
}

#define	__COPY_STAT_FIELD(_cfg, _stats, _field)	\
	(_stats)->_field = NAT64STAT_FETCH(&(_cfg)->stats, _field)
static void
export_stats(struct ip_fw_chain *ch, struct nat64lsn_cfg *cfg,
    struct ipfw_nat64lsn_stats *stats)
{

	__COPY_STAT_FIELD(cfg, stats, opcnt64);
	__COPY_STAT_FIELD(cfg, stats, opcnt46);
	__COPY_STAT_FIELD(cfg, stats, ofrags);
	__COPY_STAT_FIELD(cfg, stats, ifrags);
	__COPY_STAT_FIELD(cfg, stats, oerrors);
	__COPY_STAT_FIELD(cfg, stats, noroute4);
	__COPY_STAT_FIELD(cfg, stats, noroute6);
	__COPY_STAT_FIELD(cfg, stats, nomatch4);
	__COPY_STAT_FIELD(cfg, stats, noproto);
	__COPY_STAT_FIELD(cfg, stats, nomem);
	__COPY_STAT_FIELD(cfg, stats, dropped);

	__COPY_STAT_FIELD(cfg, stats, jcalls);
	__COPY_STAT_FIELD(cfg, stats, jrequests);
	__COPY_STAT_FIELD(cfg, stats, jhostsreq);
	__COPY_STAT_FIELD(cfg, stats, jportreq);
	__COPY_STAT_FIELD(cfg, stats, jhostfails);
	__COPY_STAT_FIELD(cfg, stats, jportfails);
	__COPY_STAT_FIELD(cfg, stats, jmaxlen);
	__COPY_STAT_FIELD(cfg, stats, jnomem);
	__COPY_STAT_FIELD(cfg, stats, jreinjected);
	__COPY_STAT_FIELD(cfg, stats, screated);
	__COPY_STAT_FIELD(cfg, stats, sdeleted);
	__COPY_STAT_FIELD(cfg, stats, spgcreated);
	__COPY_STAT_FIELD(cfg, stats, spgdeleted);

	stats->hostcount = cfg->ihcount;
	stats->tcpchunks = cfg->protochunks[NAT_PROTO_TCP];
	stats->udpchunks = cfg->protochunks[NAT_PROTO_UDP];
	stats->icmpchunks = cfg->protochunks[NAT_PROTO_ICMP];
}
#undef	__COPY_STAT_FIELD

static void
nat64lsn_export_config(struct ip_fw_chain *ch, struct nat64lsn_cfg *cfg,
    ipfw_nat64lsn_cfg *uc)
{

	uc->flags = cfg->flags & NAT64LSN_FLAGSMASK;
	uc->max_ports = cfg->max_chunks * NAT64_CHUNK_SIZE;
	uc->agg_prefix_len = cfg->agg_prefix_len;
	uc->agg_prefix_max = cfg->agg_prefix_max;

	uc->jmaxlen = cfg->jmaxlen;
	uc->nh_delete_delay = cfg->nh_delete_delay;
	uc->pg_delete_delay = cfg->pg_delete_delay;
	uc->st_syn_ttl = cfg->st_syn_ttl;
	uc->st_close_ttl = cfg->st_close_ttl;
	uc->st_estab_ttl = cfg->st_estab_ttl;
	uc->st_udp_ttl = cfg->st_udp_ttl;
	uc->st_icmp_ttl = cfg->st_icmp_ttl;
	uc->prefix4.s_addr = htonl(cfg->prefix4);
	uc->prefix6 = cfg->prefix6;
	uc->plen4 = cfg->plen4;
	uc->plen6 = cfg->plen6;
	uc->set = cfg->no.set;
	strlcpy(uc->name, cfg->no.name, sizeof(uc->name));
}

struct nat64_dump_arg {
	struct ip_fw_chain *ch;
	struct sockopt_data *sd;
};

static int
export_config_cb(struct namedobj_instance *ni, struct named_object *no,
    void *arg)
{
	struct nat64_dump_arg *da = (struct nat64_dump_arg *)arg;
	ipfw_nat64lsn_cfg *uc;

	uc = (struct _ipfw_nat64lsn_cfg *)ipfw_get_sopt_space(da->sd,
	    sizeof(*uc));
	nat64lsn_export_config(da->ch, (struct nat64lsn_cfg *)no, uc);
	return (0);
}

/*
 * Lists all nat64 lsn instances currently available in kernel.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_lheader ]
 * Reply: [ ipfw_obj_lheader ipfw_nat64lsn_cfg x N ]
 *
 * Returns 0 on success
 */
static int
nat64lsn_list(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	ipfw_obj_lheader *olh;
	struct nat64_dump_arg da;

	/* Check minimum header size */
	if (sd->valsize < sizeof(ipfw_obj_lheader))
		return (EINVAL);

	olh = (ipfw_obj_lheader *)ipfw_get_sopt_header(sd, sizeof(*olh));

	IPFW_UH_RLOCK(ch);
	olh->count = ipfw_objhash_count_type(CHAIN_TO_SRV(ch),
	    IPFW_TLV_NAT64LSN_NAME);
	olh->objsize = sizeof(ipfw_nat64lsn_cfg);
	olh->size = sizeof(*olh) + olh->count * olh->objsize;

	if (sd->valsize < olh->size) {
		IPFW_UH_RUNLOCK(ch);
		return (ENOMEM);
	}
	memset(&da, 0, sizeof(da));
	da.ch = ch;
	da.sd = sd;
	ipfw_objhash_foreach_type(CHAIN_TO_SRV(ch), export_config_cb, &da,
	    IPFW_TLV_NAT64LSN_NAME);
	IPFW_UH_RUNLOCK(ch);

	return (0);
}

/*
 * Change existing nat64lsn instance configuration.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_header ipfw_nat64lsn_cfg ]
 * Reply: [ ipfw_obj_header ipfw_nat64lsn_cfg ]
 *
 * Returns 0 on success
 */
static int
nat64lsn_config(struct ip_fw_chain *ch, ip_fw3_opheader *op,
    struct sockopt_data *sd)
{
	ipfw_obj_header *oh;
	ipfw_nat64lsn_cfg *uc;
	struct nat64lsn_cfg *cfg;
	struct namedobj_instance *ni;

	if (sd->valsize != sizeof(*oh) + sizeof(*uc))
		return (EINVAL);

	oh = (ipfw_obj_header *)ipfw_get_sopt_space(sd,
	    sizeof(*oh) + sizeof(*uc));
	uc = (ipfw_nat64lsn_cfg *)(oh + 1);

	if (ipfw_check_object_name_generic(oh->ntlv.name) != 0 ||
	    oh->ntlv.set >= IPFW_MAX_SETS)
		return (EINVAL);

	ni = CHAIN_TO_SRV(ch);
	if (sd->sopt->sopt_dir == SOPT_GET) {
		IPFW_UH_RLOCK(ch);
		cfg = nat64lsn_find(ni, oh->ntlv.name, oh->ntlv.set);
		if (cfg == NULL) {
			IPFW_UH_RUNLOCK(ch);
			return (EEXIST);
		}
		nat64lsn_export_config(ch, cfg, uc);
		IPFW_UH_RUNLOCK(ch);
		return (0);
	}

	nat64lsn_default_config(uc);

	IPFW_UH_WLOCK(ch);
	cfg = nat64lsn_find(ni, oh->ntlv.name, oh->ntlv.set);
	if (cfg == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (EEXIST);
	}

	/*
	 * For now allow to change only following values:
	 *  jmaxlen, nh_del_age, pg_del_age, tcp_syn_age, tcp_close_age,
	 *  tcp_est_age, udp_age, icmp_age, flags, max_ports.
	 */

	cfg->max_chunks = uc->max_ports / NAT64_CHUNK_SIZE;
	cfg->jmaxlen = uc->jmaxlen;
	cfg->nh_delete_delay = uc->nh_delete_delay;
	cfg->pg_delete_delay = uc->pg_delete_delay;
	cfg->st_syn_ttl = uc->st_syn_ttl;
	cfg->st_close_ttl = uc->st_close_ttl;
	cfg->st_estab_ttl = uc->st_estab_ttl;
	cfg->st_udp_ttl = uc->st_udp_ttl;
	cfg->st_icmp_ttl = uc->st_icmp_ttl;
	cfg->flags = uc->flags & NAT64LSN_FLAGSMASK;

	IPFW_UH_WUNLOCK(ch);

	return (0);
}

/*
 * Get nat64lsn statistics.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_header ]
 * Reply: [ ipfw_obj_header ipfw_counter_tlv ]
 *
 * Returns 0 on success
 */
static int
nat64lsn_stats(struct ip_fw_chain *ch, ip_fw3_opheader *op,
    struct sockopt_data *sd)
{
	struct ipfw_nat64lsn_stats stats;
	struct nat64lsn_cfg *cfg;
	ipfw_obj_header *oh;
	ipfw_obj_ctlv *ctlv;
	size_t sz;

	sz = sizeof(ipfw_obj_header) + sizeof(ipfw_obj_ctlv) + sizeof(stats);
	if (sd->valsize % sizeof(uint64_t))
		return (EINVAL);
	if (sd->valsize < sz)
		return (ENOMEM);
	oh = (ipfw_obj_header *)ipfw_get_sopt_header(sd, sz);
	if (oh == NULL)
		return (EINVAL);
	memset(&stats, 0, sizeof(stats));

	IPFW_UH_RLOCK(ch);
	cfg = nat64lsn_find(CHAIN_TO_SRV(ch), oh->ntlv.name, oh->ntlv.set);
	if (cfg == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (ESRCH);
	}

	export_stats(ch, cfg, &stats);
	IPFW_UH_RUNLOCK(ch);

	ctlv = (ipfw_obj_ctlv *)(oh + 1);
	memset(ctlv, 0, sizeof(*ctlv));
	ctlv->head.type = IPFW_TLV_COUNTERS;
	ctlv->head.length = sz - sizeof(ipfw_obj_header);
	ctlv->count = sizeof(stats) / sizeof(uint64_t);
	ctlv->objsize = sizeof(uint64_t);
	ctlv->version = IPFW_NAT64_VERSION;
	memcpy(ctlv + 1, &stats, sizeof(stats));
	return (0);
}

/*
 * Reset nat64lsn statistics.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_header ]
 *
 * Returns 0 on success
 */
static int
nat64lsn_reset_stats(struct ip_fw_chain *ch, ip_fw3_opheader *op,
    struct sockopt_data *sd)
{
	struct nat64lsn_cfg *cfg;
	ipfw_obj_header *oh;

	if (sd->valsize != sizeof(*oh))
		return (EINVAL);
	oh = (ipfw_obj_header *)sd->kbuf;
	if (ipfw_check_object_name_generic(oh->ntlv.name) != 0 ||
	    oh->ntlv.set >= IPFW_MAX_SETS)
		return (EINVAL);

	IPFW_UH_WLOCK(ch);
	cfg = nat64lsn_find(CHAIN_TO_SRV(ch), oh->ntlv.name, oh->ntlv.set);
	if (cfg == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (ESRCH);
	}
	COUNTER_ARRAY_ZERO(cfg->stats.stats, NAT64STATS);
	IPFW_UH_WUNLOCK(ch);
	return (0);
}

/*
 * Reply: [ ipfw_obj_header ipfw_obj_data [ ipfw_nat64lsn_stg
 *	ipfw_nat64lsn_state x count, ... ] ]
 */
static int
export_pg_states(struct nat64lsn_cfg *cfg, struct nat64lsn_portgroup *pg,
    ipfw_nat64lsn_stg *stg, struct sockopt_data *sd)
{
	ipfw_nat64lsn_state *ste;
	struct nat64lsn_state *st;
	int i, count;

	NAT64_LOCK(pg->host);
	count = 0;
	for (i = 0; i < 64; i++) {
		if (PG_IS_BUSY_IDX(pg, i))
			count++;
	}
	DPRINTF(DP_STATE, "EXPORT PG %d, count %d", pg->idx, count);

	if (count == 0) {
		stg->count = 0;
		NAT64_UNLOCK(pg->host);
		return (0);
	}
	ste = (ipfw_nat64lsn_state *)ipfw_get_sopt_space(sd,
	    count * sizeof(ipfw_nat64lsn_state));
	if (ste == NULL) {
		NAT64_UNLOCK(pg->host);
		return (1);
	}

	stg->alias4.s_addr = pg->aaddr;
	stg->proto = nat64lsn_rproto_map[pg->nat_proto];
	stg->flags = 0;
	stg->host6 = pg->host->addr;
	stg->count = count;
	for (i = 0; i < 64; i++) {
		if (PG_IS_FREE_IDX(pg, i))
			continue;
		st = &pg->states[i];
		ste->daddr.s_addr = st->u.s.faddr;
		ste->dport = st->u.s.fport;
		ste->aport = pg->aport + i;
		ste->sport = st->u.s.lport;
		ste->flags = st->flags; /* XXX filter flags */
		ste->idle = GET_AGE(st->timestamp);
		ste++;
	}
	NAT64_UNLOCK(pg->host);

	return (0);
}

static int
get_next_idx(struct nat64lsn_cfg *cfg, uint32_t *addr, uint8_t *nat_proto,
    uint16_t *port)
{

	if (*port < 65536 - NAT64_CHUNK_SIZE) {
		*port += NAT64_CHUNK_SIZE;
		return (0);
	}
	*port = 0;

	if (*nat_proto < NAT_MAX_PROTO - 1) {
		*nat_proto += 1;
		return (0);
	}
	*nat_proto = 1;

	if (*addr < cfg->pmask4) {
		*addr += 1;
		return (0);
	}

	/* End of space. */
	return (1);
}

#define	PACK_IDX(addr, proto, port)	\
	((uint64_t)addr << 32) | ((uint32_t)port << 16) | (proto << 8)
#define	UNPACK_IDX(idx, addr, proto, port)		\
	(addr) = (uint32_t)((idx) >> 32);		\
	(port) = (uint16_t)(((idx) >> 16) & 0xFFFF);	\
	(proto) = (uint8_t)(((idx) >> 8) & 0xFF)

static struct nat64lsn_portgroup *
get_next_pg(struct nat64lsn_cfg *cfg, uint32_t *addr, uint8_t *nat_proto,
  uint16_t *port)
{
	struct nat64lsn_portgroup *pg;
	uint64_t pre_pack, post_pack;

	pg = NULL;
	pre_pack = PACK_IDX(*addr, *nat_proto, *port);
	for (;;) {
		if (get_next_idx(cfg, addr, nat_proto, port) != 0) {
			/* End of states */
			return (pg);
		}

		pg = GET_PORTGROUP(cfg, *addr, *nat_proto, *port);
		if (pg != NULL)
			break;
	}

	post_pack = PACK_IDX(*addr, *nat_proto, *port);
	if (pre_pack == post_pack)
		DPRINTF(DP_STATE, "XXX: PACK_IDX %u %d %d",
		    *addr, *nat_proto, *port);
	return (pg);
}

static NAT64NOINLINE struct nat64lsn_portgroup *
get_first_pg(struct nat64lsn_cfg *cfg, uint32_t *addr, uint8_t *nat_proto,
  uint16_t *port)
{
	struct nat64lsn_portgroup *pg;

	pg = GET_PORTGROUP(cfg, *addr, *nat_proto, *port);
	if (pg == NULL)
		pg = get_next_pg(cfg, addr, nat_proto, port);

	return (pg);
}

/*
 * Lists nat64lsn states.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_header ipfw_obj_data [ uint64_t ]]
 * Reply: [ ipfw_obj_header ipfw_obj_data [
 *		ipfw_nat64lsn_stg ipfw_nat64lsn_state x N] ]
 *
 * Returns 0 on success
 */
static int
nat64lsn_states(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	ipfw_obj_header *oh;
	ipfw_obj_data *od;
	ipfw_nat64lsn_stg *stg;
	struct nat64lsn_cfg *cfg;
	struct nat64lsn_portgroup *pg, *pg_next;
	uint64_t next_idx;
	size_t sz;
	uint32_t addr, states;
	uint16_t port;
	uint8_t nat_proto;

	sz = sizeof(ipfw_obj_header) + sizeof(ipfw_obj_data) +
	    sizeof(uint64_t);
	/* Check minimum header size */
	if (sd->valsize < sz)
		return (EINVAL);

	oh = (ipfw_obj_header *)sd->kbuf;
	od = (ipfw_obj_data *)(oh + 1);
	if (od->head.type != IPFW_TLV_OBJDATA ||
	    od->head.length != sz - sizeof(ipfw_obj_header))
		return (EINVAL);

	next_idx = *(uint64_t *)(od + 1);
	/* Translate index to the request position to start from */
	UNPACK_IDX(next_idx, addr, nat_proto, port);
	if (nat_proto >= NAT_MAX_PROTO)
		return (EINVAL);
	if (nat_proto == 0 && addr != 0)
		return (EINVAL);

	IPFW_UH_RLOCK(ch);
	cfg = nat64lsn_find(CHAIN_TO_SRV(ch), oh->ntlv.name, oh->ntlv.set);
	if (cfg == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (ESRCH);
	}
	/* Fill in starting point */
	if (addr == 0) {
		addr = cfg->prefix4;
		nat_proto = 1;
		port = 0;
	}
	if (addr < cfg->prefix4 || addr > cfg->pmask4) {
		IPFW_UH_RUNLOCK(ch);
		DPRINTF(DP_GENERIC | DP_STATE, "XXX: %lu %u %u",
		    next_idx, addr, cfg->pmask4);
		return (EINVAL);
	}

	sz = sizeof(ipfw_obj_header) + sizeof(ipfw_obj_data) +
	    sizeof(ipfw_nat64lsn_stg);
	if (sd->valsize < sz)
		return (ENOMEM);
	oh = (ipfw_obj_header *)ipfw_get_sopt_space(sd, sz);
	od = (ipfw_obj_data *)(oh + 1);
	od->head.type = IPFW_TLV_OBJDATA;
	od->head.length = sz - sizeof(ipfw_obj_header);
	stg = (ipfw_nat64lsn_stg *)(od + 1);

	pg = get_first_pg(cfg, &addr, &nat_proto, &port);
	if (pg == NULL) {
		/* No states */
		stg->next_idx = 0xFF;
		stg->count = 0;
		IPFW_UH_RUNLOCK(ch);
		return (0);
	}
	states = 0;
	pg_next = NULL;
	while (pg != NULL) {
		pg_next = get_next_pg(cfg, &addr, &nat_proto, &port);
		if (pg_next == NULL)
			stg->next_idx = 0xFF;
		else
			stg->next_idx = PACK_IDX(addr, nat_proto, port);

		if (export_pg_states(cfg, pg, stg, sd) != 0) {
			IPFW_UH_RUNLOCK(ch);
			return (states == 0 ? ENOMEM: 0);
		}
		states += stg->count;
		od->head.length += stg->count * sizeof(ipfw_nat64lsn_state);
		sz += stg->count * sizeof(ipfw_nat64lsn_state);
		if (pg_next != NULL) {
			sz += sizeof(ipfw_nat64lsn_stg);
			if (sd->valsize < sz)
				break;
			stg = (ipfw_nat64lsn_stg *)ipfw_get_sopt_space(sd,
			    sizeof(ipfw_nat64lsn_stg));
		}
		pg = pg_next;
	}
	IPFW_UH_RUNLOCK(ch);
	return (0);
}

static struct ipfw_sopt_handler	scodes[] = {
	{ IP_FW_NAT64LSN_CREATE, 0,	HDIR_BOTH,	nat64lsn_create },
	{ IP_FW_NAT64LSN_DESTROY,0,	HDIR_SET,	nat64lsn_destroy },
	{ IP_FW_NAT64LSN_CONFIG, 0,	HDIR_BOTH,	nat64lsn_config },
	{ IP_FW_NAT64LSN_LIST,	 0,	HDIR_GET,	nat64lsn_list },
	{ IP_FW_NAT64LSN_STATS,	 0,	HDIR_GET,	nat64lsn_stats },
	{ IP_FW_NAT64LSN_RESET_STATS,0,	HDIR_SET,	nat64lsn_reset_stats },
	{ IP_FW_NAT64LSN_LIST_STATES,0,	HDIR_GET,	nat64lsn_states },
};

static int
nat64lsn_classify(ipfw_insn *cmd, uint16_t *puidx, uint8_t *ptype)
{
	ipfw_insn *icmd;

	icmd = cmd - 1;
	if (icmd->opcode != O_EXTERNAL_ACTION ||
	    icmd->arg1 != V_nat64lsn_eid)
		return (1);

	*puidx = cmd->arg1;
	*ptype = 0;
	return (0);
}

static void
nat64lsn_update_arg1(ipfw_insn *cmd, uint16_t idx)
{

	cmd->arg1 = idx;
}

static int
nat64lsn_findbyname(struct ip_fw_chain *ch, struct tid_info *ti,
    struct named_object **pno)
{
	int err;

	err = ipfw_objhash_find_type(CHAIN_TO_SRV(ch), ti,
	    IPFW_TLV_NAT64LSN_NAME, pno);
	return (err);
}

static struct named_object *
nat64lsn_findbykidx(struct ip_fw_chain *ch, uint16_t idx)
{
	struct namedobj_instance *ni;
	struct named_object *no;

	IPFW_UH_WLOCK_ASSERT(ch);
	ni = CHAIN_TO_SRV(ch);
	no = ipfw_objhash_lookup_kidx(ni, idx);
	KASSERT(no != NULL, ("NAT64LSN with index %d not found", idx));

	return (no);
}

static int
nat64lsn_manage_sets(struct ip_fw_chain *ch, uint16_t set, uint8_t new_set,
    enum ipfw_sets_cmd cmd)
{

	return (ipfw_obj_manage_sets(CHAIN_TO_SRV(ch), IPFW_TLV_NAT64LSN_NAME,
	    set, new_set, cmd));
}

static struct opcode_obj_rewrite opcodes[] = {
	{
		.opcode = O_EXTERNAL_INSTANCE,
		.etlv = IPFW_TLV_EACTION /* just show it isn't table */,
		.classifier = nat64lsn_classify,
		.update = nat64lsn_update_arg1,
		.find_byname = nat64lsn_findbyname,
		.find_bykidx = nat64lsn_findbykidx,
		.manage_sets = nat64lsn_manage_sets,
	},
};

static int
destroy_config_cb(struct namedobj_instance *ni, struct named_object *no,
    void *arg)
{
	struct nat64lsn_cfg *cfg;
	struct ip_fw_chain *ch;

	ch = (struct ip_fw_chain *)arg;
	cfg = (struct nat64lsn_cfg *)SRV_OBJECT(ch, no->kidx);
	SRV_OBJECT(ch, no->kidx) = NULL;
	nat64lsn_detach_config(ch, cfg);
	nat64lsn_destroy_instance(cfg);
	return (0);
}

int
nat64lsn_init(struct ip_fw_chain *ch, int first)
{

	if (first != 0)
		nat64lsn_init_internal();
	V_nat64lsn_eid = ipfw_add_eaction(ch, ipfw_nat64lsn, "nat64lsn");
	if (V_nat64lsn_eid == 0)
		return (ENXIO);
	IPFW_ADD_SOPT_HANDLER(first, scodes);
	IPFW_ADD_OBJ_REWRITER(first, opcodes);
	return (0);
}

void
nat64lsn_uninit(struct ip_fw_chain *ch, int last)
{

	IPFW_DEL_OBJ_REWRITER(last, opcodes);
	IPFW_DEL_SOPT_HANDLER(last, scodes);
	ipfw_del_eaction(ch, V_nat64lsn_eid);
	/*
	 * Since we already have deregistered external action,
	 * our named objects become unaccessible via rules, because
	 * all rules were truncated by ipfw_del_eaction().
	 * So, we can unlink and destroy our named objects without holding
	 * IPFW_WLOCK().
	 */
	IPFW_UH_WLOCK(ch);
	ipfw_objhash_foreach_type(CHAIN_TO_SRV(ch), destroy_config_cb, ch,
	    IPFW_TLV_NAT64LSN_NAME);
	V_nat64lsn_eid = 0;
	IPFW_UH_WUNLOCK(ch);
	if (last != 0)
		nat64lsn_uninit_internal();
}

