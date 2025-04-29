/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2019 Yandex LLC
 * Copyright (c) 2015 Alexander V. Chernikov <melifaro@FreeBSD.org>
 * Copyright (c) 2015-2019 Andrey V. Elsukov <ae@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/ck.h>
#include <sys/epoch.h>
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

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet6/ip_fw_nat64.h>

#include <netpfil/ipfw/ip_fw_private.h>

#include "nat64lsn.h"

VNET_DEFINE(uint32_t, nat64lsn_eid) = 0;

static struct nat64lsn_instance *
nat64lsn_find(struct namedobj_instance *ni, const char *name, uint8_t set)
{
	struct named_object *no;

	no = ipfw_objhash_lookup_name_type(ni, set,
	    IPFW_TLV_NAT64LSN_NAME, name);
	if (no == NULL)
		return (NULL);
	return (__containerof(no, struct nat64lsn_instance, no));
}

static void
nat64lsn_default_config(ipfw_nat64lsn_cfg *uc)
{

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

	if (uc->states_chunks == 0)
		uc->states_chunks = 1;
	else if (uc->states_chunks >= 128)
		uc->states_chunks = 128;
	else if (!powerof2(uc->states_chunks))
		uc->states_chunks = 1 << fls(uc->states_chunks);
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
	struct nat64lsn_instance *i;
	struct nat64lsn_cfg *cfg;
	struct namedobj_instance *ni;
	uint32_t addr4, mask4;

	if (sd->valsize != sizeof(*olh) + sizeof(*uc))
		return (EINVAL);

	olh = (ipfw_obj_lheader *)sd->kbuf;
	uc = (ipfw_nat64lsn_cfg *)(olh + 1);

	if (ipfw_check_object_name_generic(uc->name) != 0)
		return (EINVAL);

	if (uc->set >= IPFW_MAX_SETS)
		return (EINVAL);

	if (uc->plen4 > 32)
		return (EINVAL);

	/*
	 * Unspecified address has special meaning. But it must
	 * have valid prefix length. This length will be used to
	 * correctly extract and embedd IPv4 address into IPv6.
	 */
	if (nat64_check_prefix6(&uc->prefix6, uc->plen6) != 0 &&
	    IN6_IS_ADDR_UNSPECIFIED(&uc->prefix6) &&
	    nat64_check_prefixlen(uc->plen6) != 0)
		return (EINVAL);

	/* XXX: Check prefix4 to be global */
	addr4 = ntohl(uc->prefix4.s_addr);
	mask4 = ~((1 << (32 - uc->plen4)) - 1);
	if ((addr4 & mask4) != addr4)
		return (EINVAL);

	nat64lsn_default_config(uc);

	ni = CHAIN_TO_SRV(ch);
	IPFW_UH_RLOCK(ch);
	if (nat64lsn_find(ni, uc->name, uc->set) != NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (EEXIST);
	}
	IPFW_UH_RUNLOCK(ch);

	i = malloc(sizeof(struct nat64lsn_instance), M_NAT64LSN,
	    M_WAITOK | M_ZERO);
	strlcpy(i->name, uc->name, sizeof(i->name));
	i->no.name = i->name;
	i->no.etlv = IPFW_TLV_NAT64LSN_NAME;
	i->no.set = uc->set;

	cfg = nat64lsn_init_config(ch, addr4, uc->plen4);
	cfg->base.plat_prefix = uc->prefix6;
	cfg->base.plat_plen = uc->plen6;
	cfg->base.flags = (uc->flags & NAT64LSN_FLAGSMASK) | NAT64_PLATPFX;
	if (IN6_IS_ADDR_WKPFX(&cfg->base.plat_prefix))
		cfg->base.flags |= NAT64_WKPFX;
	else if (IN6_IS_ADDR_UNSPECIFIED(&cfg->base.plat_prefix))
		cfg->base.flags |= NAT64LSN_ANYPREFIX;

	cfg->states_chunks = uc->states_chunks;
	cfg->jmaxlen = uc->jmaxlen;
	cfg->host_delete_delay = uc->nh_delete_delay;
	cfg->pg_delete_delay = uc->pg_delete_delay;
	cfg->st_syn_ttl = uc->st_syn_ttl;
	cfg->st_close_ttl = uc->st_close_ttl;
	cfg->st_estab_ttl = uc->st_estab_ttl;
	cfg->st_udp_ttl = uc->st_udp_ttl;
	cfg->st_icmp_ttl = uc->st_icmp_ttl;
	cfg->nomatch_verdict = IP_FW_DENY;

	IPFW_UH_WLOCK(ch);

	if (nat64lsn_find(ni, uc->name, uc->set) != NULL) {
		IPFW_UH_WUNLOCK(ch);
		nat64lsn_destroy_config(cfg);
		free(i, M_NAT64LSN);
		return (EEXIST);
	}

	if (ipfw_objhash_alloc_idx(ni, &i->no.kidx) != 0) {
		IPFW_UH_WUNLOCK(ch);
		nat64lsn_destroy_config(cfg);
		free(i, M_NAT64LSN);
		return (ENOSPC);
	}
	ipfw_objhash_add(ni, &i->no);

	/* Okay, let's link data */
	i->cfg = cfg;
	SRV_OBJECT(ch, i->no.kidx) = i;
	nat64lsn_start_instance(cfg);

	IPFW_UH_WUNLOCK(ch);
	return (0);
}

static void
nat64lsn_detach_instance(struct ip_fw_chain *ch,
    struct nat64lsn_instance *i)
{

	IPFW_UH_WLOCK_ASSERT(ch);
	SRV_OBJECT(ch, i->no.kidx) = NULL;
	ipfw_objhash_del(CHAIN_TO_SRV(ch), &i->no);
	ipfw_objhash_free_idx(CHAIN_TO_SRV(ch), i->no.kidx);
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
	struct nat64lsn_instance *i;
	ipfw_obj_header *oh;

	if (sd->valsize != sizeof(*oh))
		return (EINVAL);

	oh = (ipfw_obj_header *)op3;

	IPFW_UH_WLOCK(ch);
	i = nat64lsn_find(CHAIN_TO_SRV(ch), oh->ntlv.name, oh->ntlv.set);
	if (i == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (ENOENT);
	}

	if (i->no.refcnt > 0) {
		IPFW_UH_WUNLOCK(ch);
		return (EBUSY);
	}

	ipfw_reset_eaction_instance(ch, V_nat64lsn_eid, i->no.kidx);
	nat64lsn_detach_instance(ch, i);
	IPFW_UH_WUNLOCK(ch);

	nat64lsn_destroy_config(i->cfg);
	free(i, M_NAT64LSN);
	return (0);
}

#define	__COPY_STAT_FIELD(_cfg, _stats, _field)	\
	(_stats)->_field = NAT64STAT_FETCH(&(_cfg)->base.stats, _field)
static void
export_stats(struct ip_fw_chain *ch, struct nat64lsn_cfg *cfg,
    struct ipfw_nat64lsn_stats *stats)
{
	struct nat64lsn_alias *alias;
	int i;

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

	stats->hostcount = cfg->hosts_count;
	for (i = 0; i < (1 << (32 - cfg->plen4)); i++) {
		alias = &cfg->aliases[i];
		stats->tcpchunks += alias->tcp_pgcount;
		stats->udpchunks += alias->udp_pgcount;
		stats->icmpchunks += alias->icmp_pgcount;
	}
}
#undef	__COPY_STAT_FIELD

static void
nat64lsn_export_config(struct ip_fw_chain *ch, struct nat64lsn_instance *i,
    ipfw_nat64lsn_cfg *uc)
{
	struct nat64lsn_cfg *cfg;

	strlcpy(uc->name, i->no.name, sizeof(uc->name));
	uc->set = i->no.set;
	cfg = i->cfg;

	uc->flags = cfg->base.flags & NAT64LSN_FLAGSMASK;
	uc->states_chunks = cfg->states_chunks;
	uc->jmaxlen = cfg->jmaxlen;
	uc->nh_delete_delay = cfg->host_delete_delay;
	uc->pg_delete_delay = cfg->pg_delete_delay;
	uc->st_syn_ttl = cfg->st_syn_ttl;
	uc->st_close_ttl = cfg->st_close_ttl;
	uc->st_estab_ttl = cfg->st_estab_ttl;
	uc->st_udp_ttl = cfg->st_udp_ttl;
	uc->st_icmp_ttl = cfg->st_icmp_ttl;
	uc->prefix4.s_addr = htonl(cfg->prefix4);
	uc->prefix6 = cfg->base.plat_prefix;
	uc->plen4 = cfg->plen4;
	uc->plen6 = cfg->base.plat_plen;
}

struct nat64_dump_arg {
	struct ip_fw_chain *ch;
	struct sockopt_data *sd;
};

static int
export_config_cb(struct namedobj_instance *ni, struct named_object *no,
    void *arg)
{
	struct nat64_dump_arg *da;
	ipfw_nat64lsn_cfg *uc;

	da = (struct nat64_dump_arg *)arg;
	uc = (struct _ipfw_nat64lsn_cfg *)ipfw_get_sopt_space(da->sd,
	    sizeof(*uc));
	nat64lsn_export_config(da->ch,
	    __containerof(no, struct nat64lsn_instance, no), uc);
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
	struct nat64lsn_instance *i;
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
		i = nat64lsn_find(ni, oh->ntlv.name, oh->ntlv.set);
		if (i == NULL) {
			IPFW_UH_RUNLOCK(ch);
			return (ENOENT);
		}
		nat64lsn_export_config(ch, i, uc);
		IPFW_UH_RUNLOCK(ch);
		return (0);
	}

	nat64lsn_default_config(uc);

	IPFW_UH_WLOCK(ch);
	i = nat64lsn_find(ni, oh->ntlv.name, oh->ntlv.set);
	if (i == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (ENOENT);
	}

	/*
	 * For now allow to change only following values:
	 *  jmaxlen, nh_del_age, pg_del_age, tcp_syn_age, tcp_close_age,
	 *  tcp_est_age, udp_age, icmp_age, flags, states_chunks.
	 */
	cfg = i->cfg;
	cfg->states_chunks = uc->states_chunks;
	cfg->jmaxlen = uc->jmaxlen;
	cfg->host_delete_delay = uc->nh_delete_delay;
	cfg->pg_delete_delay = uc->pg_delete_delay;
	cfg->st_syn_ttl = uc->st_syn_ttl;
	cfg->st_close_ttl = uc->st_close_ttl;
	cfg->st_estab_ttl = uc->st_estab_ttl;
	cfg->st_udp_ttl = uc->st_udp_ttl;
	cfg->st_icmp_ttl = uc->st_icmp_ttl;
	cfg->base.flags &= ~NAT64LSN_FLAGSMASK;
	cfg->base.flags |= uc->flags & NAT64LSN_FLAGSMASK;

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
	struct nat64lsn_instance *i;
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
	i = nat64lsn_find(CHAIN_TO_SRV(ch), oh->ntlv.name, oh->ntlv.set);
	if (i == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (ENOENT);
	}

	export_stats(ch, i->cfg, &stats);
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
	struct nat64lsn_instance *i;
	ipfw_obj_header *oh;

	if (sd->valsize != sizeof(*oh))
		return (EINVAL);
	oh = (ipfw_obj_header *)sd->kbuf;
	if (ipfw_check_object_name_generic(oh->ntlv.name) != 0 ||
	    oh->ntlv.set >= IPFW_MAX_SETS)
		return (EINVAL);

	IPFW_UH_WLOCK(ch);
	i = nat64lsn_find(CHAIN_TO_SRV(ch), oh->ntlv.name, oh->ntlv.set);
	if (i == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (ENOENT);
	}
	COUNTER_ARRAY_ZERO(i->cfg->base.stats.cnt, NAT64STATS);
	IPFW_UH_WUNLOCK(ch);
	return (0);
}

#ifdef __LP64__
#define	FREEMASK_COPY(pg, n, out)	(out) = *FREEMASK_CHUNK((pg), (n))
#else
#define	FREEMASK_COPY(pg, n, out)	(out) = *FREEMASK_CHUNK((pg), (n)) | \
    ((uint64_t)*(FREEMASK_CHUNK((pg), (n)) + 1) << 32)
#endif
/*
 * Reply: [ ipfw_obj_header ipfw_obj_data [ ipfw_nat64lsn_stg
 *	ipfw_nat64lsn_state x count, ... ] ]
 */
static int
nat64lsn_export_states(struct nat64lsn_cfg *cfg, union nat64lsn_pgidx *idx,
    struct nat64lsn_pg *pg, struct sockopt_data *sd, uint32_t *ret_count)
{
	ipfw_nat64lsn_state_v1 *s;
	struct nat64lsn_state *state;
	uint64_t freemask;
	uint32_t i, count;

	/* validate user input */
	if (idx->chunk > pg->chunks_count - 1)
		return (EINVAL);

	FREEMASK_COPY(pg, idx->chunk, freemask);
	count = 64 - bitcount64(freemask);
	if (count == 0)
		return (0);	/* Try next PG/chunk */

	DPRINTF(DP_STATE, "EXPORT PG 0x%16jx, count %d",
	    (uintmax_t)idx->index, count);

	s = (ipfw_nat64lsn_state_v1 *)ipfw_get_sopt_space(sd,
	    count * sizeof(ipfw_nat64lsn_state_v1));
	if (s == NULL)
		return (ENOMEM);

	for (i = 0; i < 64; i++) {
		if (ISSET64(freemask, i))
			continue;
		state = pg->chunks_count == 1 ? &pg->states->state[i] :
		    &pg->states_chunk[idx->chunk]->state[i];

		s->host6 = state->host->addr;
		s->daddr.s_addr = htonl(state->ip_dst);
		s->dport = state->dport;
		s->sport = state->sport;
		s->aport = state->aport;
		s->flags = (uint8_t)(state->flags & 7);
		s->proto = state->proto;
		s->idle = GET_AGE(state->timestamp);
		s++;
	}
	*ret_count = count;
	return (0);
}

#define	LAST_IDX	0xFF
static int
nat64lsn_next_pgidx(struct nat64lsn_cfg *cfg, struct nat64lsn_pg *pg,
    union nat64lsn_pgidx *idx)
{

	/* First iterate over chunks */
	if (pg != NULL) {
		if (idx->chunk < pg->chunks_count - 1) {
			idx->chunk++;
			return (0);
		}
	}
	idx->chunk = 0;
	/* Then over PGs */
	if (idx->port < UINT16_MAX - 64) {
		idx->port += 64;
		return (0);
	}
	idx->port = NAT64_MIN_PORT;
	/* Then over supported protocols */
	switch (idx->proto) {
	case IPPROTO_ICMP:
		idx->proto = IPPROTO_TCP;
		return (0);
	case IPPROTO_TCP:
		idx->proto = IPPROTO_UDP;
		return (0);
	default:
		idx->proto = IPPROTO_ICMP;
	}
	/* And then over IPv4 alias addresses */
	if (idx->addr < cfg->pmask4) {
		idx->addr++;
		return (1);	/* New states group is needed */
	}
	idx->index = LAST_IDX;
	return (-1);		/* No more states */
}

static struct nat64lsn_pg*
nat64lsn_get_pg_byidx(struct nat64lsn_cfg *cfg, union nat64lsn_pgidx *idx)
{
	struct nat64lsn_alias *alias;
	int pg_idx;

	alias = &cfg->aliases[idx->addr & ((1 << (32 - cfg->plen4)) - 1)];
	MPASS(alias->addr == idx->addr);

	pg_idx = (idx->port - NAT64_MIN_PORT) / 64;
	switch (idx->proto) {
	case IPPROTO_ICMP:
		if (ISSET32(alias->icmp_pgmask[pg_idx / 32], pg_idx % 32))
			return (alias->icmp[pg_idx / 32]->pgptr[pg_idx % 32]);
		break;
	case IPPROTO_TCP:
		if (ISSET32(alias->tcp_pgmask[pg_idx / 32], pg_idx % 32))
			return (alias->tcp[pg_idx / 32]->pgptr[pg_idx % 32]);
		break;
	case IPPROTO_UDP:
		if (ISSET32(alias->udp_pgmask[pg_idx / 32], pg_idx % 32))
			return (alias->udp[pg_idx / 32]->pgptr[pg_idx % 32]);
		break;
	}
	return (NULL);
}

/*
 * Lists nat64lsn states.
 * Data layout (v1)(current):
 * Request: [ ipfw_obj_header ipfw_obj_data [ uint64_t ]]
 * Reply: [ ipfw_obj_header ipfw_obj_data [
 *		ipfw_nat64lsn_stg_v1 ipfw_nat64lsn_state_v1 x N] ]
 *
 * Returns 0 on success
 */
static int
nat64lsn_states(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	ipfw_obj_header *oh;
	ipfw_obj_data *od;
	ipfw_nat64lsn_stg_v1 *stg;
	struct nat64lsn_instance *i;
	struct nat64lsn_cfg *cfg;
	struct nat64lsn_pg *pg;
	union nat64lsn_pgidx idx;
	size_t sz;
	uint32_t count, total;
	int ret;

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

	idx.index = *(uint64_t *)(od + 1);
	if (idx.index != 0 && idx.proto != IPPROTO_ICMP &&
	    idx.proto != IPPROTO_TCP && idx.proto != IPPROTO_UDP)
		return (EINVAL);
	if (idx.index == LAST_IDX)
		return (EINVAL);

	IPFW_UH_RLOCK(ch);
	i = nat64lsn_find(CHAIN_TO_SRV(ch), oh->ntlv.name, oh->ntlv.set);
	if (i == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (ENOENT);
	}
	cfg = i->cfg;
	if (idx.index == 0) {	/* Fill in starting point */
		idx.addr = cfg->prefix4;
		idx.proto = IPPROTO_ICMP;
		idx.port = NAT64_MIN_PORT;
	}
	if (idx.addr < cfg->prefix4 || idx.addr > cfg->pmask4 ||
	    idx.port < NAT64_MIN_PORT) {
		IPFW_UH_RUNLOCK(ch);
		return (EINVAL);
	}
	sz = sizeof(ipfw_obj_header) + sizeof(ipfw_obj_data) +
	    sizeof(ipfw_nat64lsn_stg_v1);
	if (sd->valsize < sz) {
		IPFW_UH_RUNLOCK(ch);
		return (ENOMEM);
	}
	oh = (ipfw_obj_header *)ipfw_get_sopt_space(sd, sz);
	od = (ipfw_obj_data *)(oh + 1);
	od->head.type = IPFW_TLV_OBJDATA;
	od->head.length = sz - sizeof(ipfw_obj_header);
	stg = (ipfw_nat64lsn_stg_v1 *)(od + 1);
	stg->count = total = 0;
	stg->next.index = idx.index;
	/*
	 * Acquire CALLOUT_LOCK to avoid races with expiration code.
	 * Thus states, hosts and PGs will not expire while we hold it.
	 */
	CALLOUT_LOCK(cfg);
	ret = 0;
	do {
		pg = nat64lsn_get_pg_byidx(cfg, &idx);
		if (pg != NULL) {
			count = 0;
			ret = nat64lsn_export_states(cfg, &idx, pg,
			    sd, &count);
			if (ret != 0)
				break;
			if (count > 0) {
				stg->count += count;
				total += count;
				/* Update total size of reply */
				od->head.length +=
				    count * sizeof(ipfw_nat64lsn_state_v1);
				sz += count * sizeof(ipfw_nat64lsn_state_v1);
			}
			stg->alias4.s_addr = htonl(idx.addr);
		}
		/* Determine new index */
		switch (nat64lsn_next_pgidx(cfg, pg, &idx)) {
		case -1:
			ret = ENOENT; /* End of search */
			break;
		case 1: /*
			 * Next alias address, new group may be needed.
			 * If states count is zero, use this group.
			 */
			if (stg->count == 0)
				continue;
			/* Otherwise try to create new group */
			sz += sizeof(ipfw_nat64lsn_stg_v1);
			if (sd->valsize < sz) {
				ret = ENOMEM;
				break;
			}
			/* Save next index in current group */
			stg->next.index = idx.index;
			stg = (ipfw_nat64lsn_stg_v1 *)ipfw_get_sopt_space(sd,
			    sizeof(ipfw_nat64lsn_stg_v1));
			od->head.length += sizeof(ipfw_nat64lsn_stg_v1);
			stg->count = 0;
			break;
		}
		stg->next.index = idx.index;
	} while (ret == 0);
	CALLOUT_UNLOCK(cfg);
	IPFW_UH_RUNLOCK(ch);
	return ((total > 0 || idx.index == LAST_IDX) ? 0: ret);
}

static struct ipfw_sopt_handler	scodes[] = {
    { IP_FW_NAT64LSN_CREATE,	IP_FW3_OPVER,	HDIR_BOTH, nat64lsn_create },
    { IP_FW_NAT64LSN_DESTROY,	IP_FW3_OPVER,	HDIR_SET,  nat64lsn_destroy },
    { IP_FW_NAT64LSN_CONFIG,	IP_FW3_OPVER,	HDIR_BOTH, nat64lsn_config },
    { IP_FW_NAT64LSN_LIST,	IP_FW3_OPVER,	HDIR_GET,  nat64lsn_list },
    { IP_FW_NAT64LSN_STATS,	IP_FW3_OPVER,	HDIR_GET,  nat64lsn_stats },
    { IP_FW_NAT64LSN_RESET_STATS, IP_FW3_OPVER,	HDIR_SET,  nat64lsn_reset_stats },
    { IP_FW_NAT64LSN_LIST_STATES, IP_FW3_OPVER,	HDIR_GET,  nat64lsn_states },
};

#define	NAT64LSN_ARE_EQUAL(v)	(cfg0->v == cfg1->v)
static int
nat64lsn_cmp_configs(struct nat64lsn_cfg *cfg0, struct nat64lsn_cfg *cfg1)
{

	if ((cfg0->base.flags & cfg1->base.flags & NAT64LSN_ALLOW_SWAPCONF) &&
	    NAT64LSN_ARE_EQUAL(prefix4) &&
	    NAT64LSN_ARE_EQUAL(pmask4) &&
	    NAT64LSN_ARE_EQUAL(plen4) &&
	    NAT64LSN_ARE_EQUAL(base.plat_plen) &&
	    IN6_ARE_ADDR_EQUAL(&cfg0->base.plat_prefix,
		&cfg1->base.plat_prefix))
		return (0);
	return (1);
}
#undef NAT64LSN_ARE_EQUAL

static void
nat64lsn_swap_configs(struct nat64lsn_instance *i0,
    struct nat64lsn_instance *i1)
{
	struct nat64lsn_cfg *cfg;

	cfg = i0->cfg;
	i0->cfg = i1->cfg;
	i1->cfg = cfg;
}

/*
 * NAT64LSN sets swap handler.
 *
 * When two sets have NAT64LSN instance with the same name, we check
 * most important configuration parameters, and if there are no difference,
 * and both instances have NAT64LSN_ALLOW_SWAPCONF flag, we will exchange
 * configs between instances. This allows to keep NAT64 states when ipfw's
 * rules are reloaded using new set.
 *
 * XXX: since manage_sets caller doesn't hold IPFW_WLOCK(), it is possible
 * that some states will be created during switching, because set of rules
 * is changed a bit earley than named objects.
 */
static int
nat64lsn_swap_sets_cb(struct namedobj_instance *ni, struct named_object *no,
    void *arg)
{
	struct nat64lsn_instance *i0, *i1;
	uint8_t *sets;

	sets = arg;
	if (no->set == sets[0]) {
		/*
		 * Check if we have instance in new set with the same
		 * config that is sets aware and ready to swap configs.
		 */
		i0 = __containerof(no, struct nat64lsn_instance, no);
		if ((i0->cfg->base.flags & NAT64LSN_ALLOW_SWAPCONF) &&
		    (i1 = nat64lsn_find(ni, no->name, sets[1])) != NULL) {
			/* Compare configs */
			if (nat64lsn_cmp_configs(i0->cfg, i1->cfg) == 0) {
				IPFW_UH_WLOCK_ASSERT(&V_layer3_chain);
				IPFW_WLOCK(&V_layer3_chain);
				nat64lsn_swap_configs(i0, i1);
				IPFW_WUNLOCK(&V_layer3_chain);
			}
		}
	}
	return (0);
}

static int
nat64lsn_manage_sets(struct ip_fw_chain *ch, uint32_t set, uint8_t new_set,
    enum ipfw_sets_cmd cmd)
{
	uint8_t sets[2];

	if (cmd == SWAP_ALL) {
		sets[0] = (uint8_t)set;
		sets[1] = new_set;
		ipfw_objhash_foreach_type(CHAIN_TO_SRV(ch),
		    nat64lsn_swap_sets_cb, &sets, IPFW_TLV_NAT64LSN_NAME);
	}
	return (ipfw_obj_manage_sets(CHAIN_TO_SRV(ch), IPFW_TLV_NAT64LSN_NAME,
	    set, new_set, cmd));
}
NAT64_DEFINE_OPCODE_REWRITER(nat64lsn, NAT64LSN, opcodes);

static int
destroy_config_cb(struct namedobj_instance *ni, struct named_object *no,
    void *arg)
{
	struct nat64lsn_instance *i;
	struct ip_fw_chain *ch;

	ch = (struct ip_fw_chain *)arg;
	i = (struct nat64lsn_instance *)SRV_OBJECT(ch, no->kidx);
	nat64lsn_detach_instance(ch, i);
	nat64lsn_destroy_config(i->cfg);
	free(i, M_NAT64LSN);
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
