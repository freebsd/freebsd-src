/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Alexander V. Chernikov <melifaro@FreeBSD.org>
 * Copyright (c) 2023 Rubicon Communications, LLC (Netgate)
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
 */
#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/ucred.h>

#include <net/pfvar.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_message_writer.h>

#include <netpfil/pf/pf_nl.h>

#define	DEBUG_MOD_NAME	nl_pf
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_DEBUG);

struct nl_parsed_state {
	uint8_t		version;
	uint32_t	id;
	uint32_t	creatorid;
	char		ifname[IFNAMSIZ];
	uint16_t	proto;
	sa_family_t	af;
	struct pf_addr	addr;
	struct pf_addr	mask;
};

#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_state, _field)
static const struct nlattr_parser nla_p_state[] = {
	{ .type = PF_ST_ID, .off = _OUT(id), .cb = nlattr_get_uint32 },
	{ .type = PF_ST_CREATORID, .off = _OUT(creatorid), .cb = nlattr_get_uint32 },
	{ .type = PF_ST_IFNAME, .arg = (const void *)IFNAMSIZ, .off = _OUT(ifname), .cb = nlattr_get_chara },
	{ .type = PF_ST_AF, .off = _OUT(af), .cb = nlattr_get_uint8 },
	{ .type = PF_ST_PROTO, .off = _OUT(proto), .cb = nlattr_get_uint16 },
	{ .type = PF_ST_FILTER_ADDR, .off = _OUT(addr), .cb = nlattr_get_in6_addr },
	{ .type = PF_ST_FILTER_MASK, .off = _OUT(mask), .cb = nlattr_get_in6_addr },
};
static const struct nlfield_parser nlf_p_generic[] = {
	{ .off_in = _IN(version), .off_out = _OUT(version), .cb = nlf_get_u8 },
};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(state_parser, struct genlmsghdr, nlf_p_generic, nla_p_state);

static void
dump_addr(struct nl_writer *nw, int attr, const struct pf_addr *addr, int af)
{
	switch (af) {
	case AF_INET:
		nlattr_add(nw, attr, 4, &addr->v4);
		break;
	case AF_INET6:
		nlattr_add(nw, attr, 16, &addr->v6);
		break;
	};
}

static bool
dump_state_peer(struct nl_writer *nw, int attr, const struct pf_state_peer *peer)
{
	int off = nlattr_add_nested(nw, attr);
	if (off == 0)
		return (false);

	nlattr_add_u32(nw, PF_STP_SEQLO, peer->seqlo);
	nlattr_add_u32(nw, PF_STP_SEQHI, peer->seqhi);
	nlattr_add_u32(nw, PF_STP_SEQDIFF, peer->seqdiff);
	nlattr_add_u16(nw, PF_STP_MAX_WIN, peer->max_win);
	nlattr_add_u16(nw, PF_STP_MSS, peer->mss);
	nlattr_add_u8(nw, PF_STP_STATE, peer->state);
	nlattr_add_u8(nw, PF_STP_WSCALE, peer->wscale);

	if (peer->scrub != NULL) {
		struct pf_state_scrub *sc = peer->scrub;
		uint16_t pfss_flags = sc->pfss_flags & PFSS_TIMESTAMP;

		nlattr_add_u16(nw, PF_STP_PFSS_FLAGS, pfss_flags);
		nlattr_add_u32(nw, PF_STP_PFSS_TS_MOD, sc->pfss_ts_mod);
		nlattr_add_u8(nw, PF_STP_PFSS_TTL, sc->pfss_ttl);
		nlattr_add_u8(nw, PF_STP_SCRUB_FLAG, PFSYNC_SCRUB_FLAG_VALID);
	}
	nlattr_set_len(nw, off);

	return (true);
}

static bool
dump_state_key(struct nl_writer *nw, int attr, const struct pf_state_key *key)
{
	int off = nlattr_add_nested(nw, attr);
	if (off == 0)
		return (false);

	dump_addr(nw, PF_STK_ADDR0, &key->addr[0], key->af);
	dump_addr(nw, PF_STK_ADDR1, &key->addr[1], key->af);
	nlattr_add_u16(nw, PF_STK_PORT0, key->port[0]);
	nlattr_add_u16(nw, PF_STK_PORT1, key->port[1]);

	nlattr_set_len(nw, off);

	return (true);
}

static int
dump_state(struct nlpcb *nlp, const struct nlmsghdr *hdr, struct pf_kstate *s,
    struct nl_pstate *npt)
{
	struct nl_writer *nw = npt->nw;
	int error = 0;
	int af;
	struct pf_state_key *key;

	PF_STATE_LOCK_ASSERT(s);

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		goto enomem;

	struct genlmsghdr *ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_GETSTATES;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	nlattr_add_u64(nw, PF_ST_VERSION, PF_STATE_VERSION);

	key = s->key[PF_SK_WIRE];
	if (!dump_state_key(nw, PF_ST_KEY_WIRE, key))
		goto enomem;
	key = s->key[PF_SK_STACK];
	if (!dump_state_key(nw, PF_ST_KEY_STACK, key))
		goto enomem;

	af = s->key[PF_SK_WIRE]->af;
	nlattr_add_u8(nw, PF_ST_PROTO, s->key[PF_SK_WIRE]->proto);
	nlattr_add_u8(nw, PF_ST_AF, af);

	nlattr_add_string(nw, PF_ST_IFNAME, s->kif->pfik_name);
	nlattr_add_string(nw, PF_ST_ORIG_IFNAME, s->orig_kif->pfik_name);
	dump_addr(nw, PF_ST_RT_ADDR, &s->rt_addr, af);
	nlattr_add_u32(nw, PF_ST_CREATION, time_uptime - (s->creation / 1000));
	uint32_t expire = pf_state_expires(s);
	if (expire > time_uptime)
		expire = expire - time_uptime;
	nlattr_add_u32(nw, PF_ST_EXPIRE, expire);
	nlattr_add_u8(nw, PF_ST_DIRECTION, s->direction);
	nlattr_add_u8(nw, PF_ST_LOG, s->act.log);
	nlattr_add_u8(nw, PF_ST_TIMEOUT, s->timeout);
	nlattr_add_u16(nw, PF_ST_STATE_FLAGS, s->state_flags);
	uint8_t sync_flags = 0;
	if (s->src_node)
		sync_flags |= PFSYNC_FLAG_SRCNODE;
	if (s->nat_src_node)
		sync_flags |= PFSYNC_FLAG_NATSRCNODE;
	nlattr_add_u8(nw, PF_ST_SYNC_FLAGS, sync_flags);
	nlattr_add_u64(nw, PF_ST_ID, s->id);
	nlattr_add_u32(nw, PF_ST_CREATORID, htonl(s->creatorid));

	nlattr_add_u32(nw, PF_ST_RULE, s->rule.ptr ? s->rule.ptr->nr : -1);
	nlattr_add_u32(nw, PF_ST_ANCHOR, s->anchor.ptr ? s->anchor.ptr->nr : -1);
	nlattr_add_u32(nw, PF_ST_NAT_RULE, s->nat_rule.ptr ? s->nat_rule.ptr->nr : -1);

	nlattr_add_u64(nw, PF_ST_PACKETS0, s->packets[0]);
	nlattr_add_u64(nw, PF_ST_PACKETS1, s->packets[1]);
	nlattr_add_u64(nw, PF_ST_BYTES0, s->bytes[0]);
	nlattr_add_u64(nw, PF_ST_BYTES1, s->bytes[1]);
	nlattr_add_u32(nw, PF_ST_RTABLEID, s->act.rtableid);
	nlattr_add_u8(nw, PF_ST_MIN_TTL, s->act.min_ttl);
	nlattr_add_u16(nw, PF_ST_MAX_MSS, s->act.max_mss);
	nlattr_add_u16(nw, PF_ST_DNPIPE, s->act.dnpipe);
	nlattr_add_u16(nw, PF_ST_DNRPIPE, s->act.dnrpipe);
	nlattr_add_u8(nw, PF_ST_RT, s->rt);
	if (s->rt_kif != NULL)
		nlattr_add_string(nw, PF_ST_RT_IFNAME, s->rt_kif->pfik_name);

	if (!dump_state_peer(nw, PF_ST_PEER_SRC, &s->src))
		goto enomem;
	if (!dump_state_peer(nw, PF_ST_PEER_DST, &s->dst))
		goto enomem;

	if (nlmsg_end(nw))
		return (0);

enomem:
	error = ENOMEM;
	nlmsg_abort(nw);
	return (error);
}

static int
handle_dumpstates(struct nlpcb *nlp, struct nl_parsed_state *attrs,
    struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	int error = 0;

	hdr->nlmsg_flags |= NLM_F_MULTI;

	for (int i = 0; i <= V_pf_hashmask; i++) {
		struct pf_idhash *ih = &V_pf_idhash[i];
		struct pf_kstate *s;

		if (LIST_EMPTY(&ih->states))
			continue;

		PF_HASHROW_LOCK(ih);
		LIST_FOREACH(s, &ih->states, entry) {
			sa_family_t af = s->key[PF_SK_WIRE]->af;

			if (s->timeout == PFTM_UNLINKED)
				continue;

			/* Filter */
			if (attrs->creatorid != 0 && s->creatorid != attrs->creatorid)
				continue;
			if (attrs->ifname[0] != 0 &&
			    strncmp(attrs->ifname, s->kif->pfik_name, IFNAMSIZ) != 0)
				continue;
			if (attrs->proto != 0 && s->key[PF_SK_WIRE]->proto != attrs->proto)
				continue;
			if (attrs->af != 0 && af != attrs->af)
				continue;
			if (pf_match_addr(1, &s->key[PF_SK_WIRE]->addr[0],
			    &attrs->mask, &attrs->addr, af) &&
			    pf_match_addr(1, &s->key[PF_SK_WIRE]->addr[1],
			    &attrs->mask, &attrs->addr, af) &&
			    pf_match_addr(1, &s->key[PF_SK_STACK]->addr[0],
			    &attrs->mask, &attrs->addr, af) &&
			    pf_match_addr(1, &s->key[PF_SK_STACK]->addr[1],
			    &attrs->mask, &attrs->addr, af))
				continue;

			error = dump_state(nlp, hdr, s, npt);
			if (error != 0)
				break;
		}
		PF_HASHROW_UNLOCK(ih);
	}

	if (!nlmsg_end_dump(npt->nw, error, hdr)) {
		NL_LOG(LOG_DEBUG, "Unable to finalize the dump");
		return (ENOMEM);
	}

	return (error);
}

static int
handle_getstate(struct nlpcb *nlp, struct nl_parsed_state *attrs,
    struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pf_kstate *s;
	int ret;

	s = pf_find_state_byid(attrs->id, attrs->creatorid);
	if (s == NULL)
		return (ENOENT);
	ret = dump_state(nlp, hdr, s, npt);
	PF_STATE_UNLOCK(s);

	return (ret);
}

static int
dump_creatorid(struct nlpcb *nlp, const struct nlmsghdr *hdr, uint32_t creator,
    struct nl_pstate *npt)
{
	struct nl_writer *nw = npt->nw;

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		goto enomem;

	struct genlmsghdr *ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_GETCREATORS;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	nlattr_add_u32(nw, PF_ST_CREATORID, htonl(creator));

	if (nlmsg_end(nw))
		return (0);

enomem:
	nlmsg_abort(nw);
	return (ENOMEM);
}

static int
pf_handle_getstates(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	int error;

	struct nl_parsed_state attrs = {};
	error = nl_parse_nlmsg(hdr, &state_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (attrs.id != 0)
		error = handle_getstate(npt->nlp, &attrs, hdr, npt);
	else
		error = handle_dumpstates(npt->nlp, &attrs, hdr, npt);

	return (error);
}

static int
pf_handle_getcreators(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	uint32_t creators[16];
	int error = 0;

	bzero(creators, sizeof(creators));

	for (int i = 0; i < V_pf_hashmask; i++) {
		struct pf_idhash *ih = &V_pf_idhash[i];
		struct pf_kstate *s;

		if (LIST_EMPTY(&ih->states))
			continue;

		PF_HASHROW_LOCK(ih);
		LIST_FOREACH(s, &ih->states, entry) {
			int j;
			if (s->timeout == PFTM_UNLINKED)
				continue;

			for (j = 0; j < nitems(creators); j++) {
				if (creators[j] == s->creatorid)
					break;
				if (creators[j] == 0) {
					creators[j] = s->creatorid;
					break;
				}
			}
			if (j == nitems(creators))
				printf("Warning: too many creators!\n");
		}
		PF_HASHROW_UNLOCK(ih);
	}

	hdr->nlmsg_flags |= NLM_F_MULTI;
	for (int i = 0; i < nitems(creators); i++) {
		if (creators[i] == 0)
			break;
		error = dump_creatorid(npt->nlp, hdr, creators[i], npt);
	}

	if (!nlmsg_end_dump(npt->nw, error, hdr)) {
		NL_LOG(LOG_DEBUG, "Unable to finalize the dump");
		return (ENOMEM);
	}

	return (error);
}

static int
pf_handle_start(struct nlmsghdr *hdr __unused, struct nl_pstate *npt __unused)
{
	return (pf_start());
}

static int
pf_handle_stop(struct nlmsghdr *hdr __unused, struct nl_pstate *npt __unused)
{
	return (pf_stop());
}

#define _OUT(_field)	offsetof(struct pf_addr_wrap, _field)
static const struct nlattr_parser nla_p_addr_wrap[] = {
	{ .type = PF_AT_ADDR, .off = _OUT(v.a.addr), .cb = nlattr_get_in6_addr },
	{ .type = PF_AT_MASK, .off = _OUT(v.a.mask), .cb = nlattr_get_in6_addr },
	{ .type = PF_AT_IFNAME, .off = _OUT(v.ifname), .arg = (void *)IFNAMSIZ,.cb = nlattr_get_chara },
	{ .type = PF_AT_TABLENAME, .off = _OUT(v.tblname), .arg = (void *)PF_TABLE_NAME_SIZE, .cb = nlattr_get_chara },
	{ .type = PF_AT_TYPE, .off = _OUT(type), .cb = nlattr_get_uint8 },
	{ .type = PF_AT_IFLAGS, .off = _OUT(iflags), .cb = nlattr_get_uint8 },
};
NL_DECLARE_ATTR_PARSER(addr_wrap_parser, nla_p_addr_wrap);
#undef _OUT

static bool
nlattr_add_addr_wrap(struct nl_writer *nw, int attrtype, struct pf_addr_wrap *a)
{
	int off = nlattr_add_nested(nw, attrtype);

	nlattr_add_in6_addr(nw, PF_AT_ADDR, &a->v.a.addr.v6);
	nlattr_add_in6_addr(nw, PF_AT_MASK, &a->v.a.mask.v6);
	nlattr_add_u8(nw, PF_AT_TYPE, a->type);
	nlattr_add_u8(nw, PF_AT_IFLAGS, a->iflags);

	if (a->type == PF_ADDR_DYNIFTL) {
		nlattr_add_string(nw, PF_AT_IFNAME, a->v.ifname);
		nlattr_add_u32(nw, PF_AT_DYNCNT, a->p.dyncnt);
	} else if (a->type == PF_ADDR_TABLE) {
		nlattr_add_string(nw, PF_AT_TABLENAME, a->v.tblname);
		nlattr_add_u32(nw, PF_AT_TBLCNT, a->p.tblcnt);
	}

	nlattr_set_len(nw, off);

	return (true);
}

#define _OUT(_field)	offsetof(struct pf_rule_addr, _field)
static const struct nlattr_parser nla_p_ruleaddr[] = {
	{ .type = PF_RAT_ADDR, .off = _OUT(addr), .arg = &addr_wrap_parser, .cb = nlattr_get_nested },
	{ .type = PF_RAT_SRC_PORT, .off = _OUT(port[0]), .cb = nlattr_get_uint16 },
	{ .type = PF_RAT_DST_PORT, .off = _OUT(port[1]), .cb = nlattr_get_uint16 },
	{ .type = PF_RAT_NEG, .off = _OUT(neg), .cb = nlattr_get_uint8 },
	{ .type = PF_RAT_OP, .off = _OUT(port_op), .cb = nlattr_get_uint8 },
};
NL_DECLARE_ATTR_PARSER(rule_addr_parser, nla_p_ruleaddr);
#undef _OUT

static bool
nlattr_add_rule_addr(struct nl_writer *nw, int attrtype, struct pf_rule_addr *r)
{
	struct pf_addr_wrap aw = {0};
	int off = nlattr_add_nested(nw, attrtype);

	bcopy(&(r->addr), &aw, sizeof(struct pf_addr_wrap));
	pf_addr_copyout(&aw);

	nlattr_add_addr_wrap(nw, PF_RAT_ADDR, &aw);
	nlattr_add_u16(nw, PF_RAT_SRC_PORT, r->port[0]);
	nlattr_add_u16(nw, PF_RAT_DST_PORT, r->port[1]);
	nlattr_add_u8(nw, PF_RAT_NEG, r->neg);
	nlattr_add_u8(nw, PF_RAT_OP, r->port_op);

	nlattr_set_len(nw, off);

	return (true);
}

#define _OUT(_field)	offsetof(struct pf_mape_portset, _field)
static const struct nlattr_parser nla_p_mape_portset[] = {
	{ .type = PF_MET_OFFSET, .off = _OUT(offset), .cb = nlattr_get_uint8 },
	{ .type = PF_MET_PSID_LEN, .off = _OUT(psidlen), .cb = nlattr_get_uint8 },
	{. type = PF_MET_PSID, .off = _OUT(psid), .cb = nlattr_get_uint16 },
};
NL_DECLARE_ATTR_PARSER(mape_portset_parser, nla_p_mape_portset);
#undef _OUT

static bool
nlattr_add_mape_portset(struct nl_writer *nw, int attrtype, const struct pf_mape_portset *m)
{
	int off = nlattr_add_nested(nw, attrtype);

	nlattr_add_u8(nw, PF_MET_OFFSET, m->offset);
	nlattr_add_u8(nw, PF_MET_PSID_LEN, m->psidlen);
	nlattr_add_u16(nw, PF_MET_PSID, m->psid);

	nlattr_set_len(nw, off);

	return (true);
}

struct nl_parsed_labels
{
	char		labels[PF_RULE_MAX_LABEL_COUNT][PF_RULE_LABEL_SIZE];
	uint32_t	i;
};

static int
nlattr_get_pf_rule_labels(struct nlattr *nla, struct nl_pstate *npt,
    const void *arg, void *target)
{
	struct nl_parsed_labels *l = (struct nl_parsed_labels *)target;
	int ret;

	if (l->i >= PF_RULE_MAX_LABEL_COUNT)
		return (E2BIG);

	ret = nlattr_get_chara(nla, npt, (void *)PF_RULE_LABEL_SIZE,
	    l->labels[l->i]);
	if (ret == 0)
		l->i++;

	return (ret);
}

#define _OUT(_field)	offsetof(struct nl_parsed_labels, _field)
static const struct nlattr_parser nla_p_labels[] = {
	{ .type = PF_LT_LABEL, .off = 0, .cb = nlattr_get_pf_rule_labels },
};
NL_DECLARE_ATTR_PARSER(rule_labels_parser, nla_p_labels);
#undef _OUT

static int
nlattr_get_nested_pf_rule_labels(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	struct nl_parsed_labels parsed_labels = { };
	int error;

	/* Assumes target points to the beginning of the structure */
	error = nl_parse_header(NLA_DATA(nla), NLA_DATA_LEN(nla), &rule_labels_parser, npt, &parsed_labels);
	if (error != 0)
		return (error);

	memcpy(target, parsed_labels.labels, sizeof(parsed_labels.labels));

	return (0);
}

static bool
nlattr_add_labels(struct nl_writer *nw, int attrtype, const struct pf_krule *r)
{
	int off = nlattr_add_nested(nw, attrtype);
	int i = 0;

	while (r->label[i][0] != 0
	    && i < PF_RULE_MAX_LABEL_COUNT) {
		nlattr_add_string(nw, PF_LT_LABEL, r->label[i]);
		i++;
	}

	nlattr_set_len(nw, off);

	return (true);
}

#define _OUT(_field)	offsetof(struct pf_kpool, _field)
static const struct nlattr_parser nla_p_pool[] = {
	{ .type = PF_PT_KEY, .off = _OUT(key), .arg = (void *)sizeof(struct pf_poolhashkey), .cb = nlattr_get_bytes },
	{ .type = PF_PT_COUNTER, .off = _OUT(counter), .cb = nlattr_get_in6_addr },
	{ .type = PF_PT_TBLIDX, .off = _OUT(tblidx), .cb = nlattr_get_uint32 },
	{ .type = PF_PT_PROXY_SRC_PORT, .off = _OUT(proxy_port[0]), .cb = nlattr_get_uint16 },
	{ .type = PF_PT_PROXY_DST_PORT, .off = _OUT(proxy_port[1]), .cb = nlattr_get_uint16 },
	{ .type = PF_PT_OPTS, .off = _OUT(opts), .cb = nlattr_get_uint8 },
	{ .type = PF_PT_MAPE, .off = _OUT(mape), .arg = &mape_portset_parser, .cb = nlattr_get_nested },
};
NL_DECLARE_ATTR_PARSER(pool_parser, nla_p_pool);
#undef _OUT

static bool
nlattr_add_pool(struct nl_writer *nw, int attrtype, const struct pf_kpool *pool)
{
	int off = nlattr_add_nested(nw, attrtype);

	nlattr_add(nw, PF_PT_KEY, sizeof(struct pf_poolhashkey), &pool->key);
	nlattr_add_in6_addr(nw, PF_PT_COUNTER, (const struct in6_addr *)&pool->counter);
	nlattr_add_u32(nw, PF_PT_TBLIDX, pool->tblidx);
	nlattr_add_u16(nw, PF_PT_PROXY_SRC_PORT, pool->proxy_port[0]);
	nlattr_add_u16(nw, PF_PT_PROXY_DST_PORT, pool->proxy_port[1]);
	nlattr_add_u8(nw, PF_PT_OPTS, pool->opts);
	nlattr_add_mape_portset(nw, PF_PT_MAPE, &pool->mape);

	nlattr_set_len(nw, off);

	return (true);
}

#define _OUT(_field)	offsetof(struct pf_rule_uid, _field)
static const struct nlattr_parser nla_p_rule_uid[] = {
	{ .type = PF_RUT_UID_LOW, .off = _OUT(uid[0]), .cb = nlattr_get_uint32 },
	{ .type = PF_RUT_UID_HIGH, .off = _OUT(uid[1]), .cb = nlattr_get_uint32 },
	{ .type = PF_RUT_OP, .off = _OUT(op), .cb = nlattr_get_uint8 },
};
NL_DECLARE_ATTR_PARSER(rule_uid_parser, nla_p_rule_uid);
#undef _OUT

static bool
nlattr_add_rule_uid(struct nl_writer *nw, int attrtype, const struct pf_rule_uid *u)
{
	int off = nlattr_add_nested(nw, attrtype);

	nlattr_add_u32(nw, PF_RUT_UID_LOW, u->uid[0]);
	nlattr_add_u32(nw, PF_RUT_UID_HIGH, u->uid[1]);
	nlattr_add_u8(nw, PF_RUT_OP, u->op);

	nlattr_set_len(nw, off);

	return (true);
}

struct nl_parsed_timeouts
{
	uint32_t	timeouts[PFTM_MAX];
	uint32_t	i;
};

static int
nlattr_get_pf_timeout(struct nlattr *nla, struct nl_pstate *npt,
    const void *arg, void *target)
{
	struct nl_parsed_timeouts *t = (struct nl_parsed_timeouts *)target;
	int ret;

	if (t->i >= PFTM_MAX)
		return (E2BIG);

	ret = nlattr_get_uint32(nla, npt, NULL, &t->timeouts[t->i]);
	if (ret == 0)
		t->i++;

	return (ret);
}

#define _OUT(_field)	offsetof(struct nl_parsed_timeout, _field)
static const struct nlattr_parser nla_p_timeouts[] = {
	{ .type = PF_TT_TIMEOUT, .off = 0, .cb = nlattr_get_pf_timeout },
};
NL_DECLARE_ATTR_PARSER(timeout_parser, nla_p_timeouts);
#undef _OUT

static int
nlattr_get_nested_timeouts(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	struct nl_parsed_timeouts parsed_timeouts = { };
	int error;

	/* Assumes target points to the beginning of the structure */
	error = nl_parse_header(NLA_DATA(nla), NLA_DATA_LEN(nla), &timeout_parser, npt, &parsed_timeouts);
	if (error != 0)
		return (error);

	memcpy(target, parsed_timeouts.timeouts, sizeof(parsed_timeouts.timeouts));

	return (0);
}

static bool
nlattr_add_timeout(struct nl_writer *nw, int attrtype, uint32_t *timeout)
{
	int off = nlattr_add_nested(nw, attrtype);

	for (int i = 0; i < PFTM_MAX; i++)
		nlattr_add_u32(nw, PF_RT_TIMEOUT, timeout[i]);

	nlattr_set_len(nw, off);

	return (true);
}

#define _OUT(_field)	offsetof(struct pf_krule, _field)
static const struct nlattr_parser nla_p_rule[] = {
	{ .type = PF_RT_SRC, .off = _OUT(src), .arg = &rule_addr_parser,.cb = nlattr_get_nested },
	{ .type = PF_RT_DST, .off = _OUT(dst), .arg = &rule_addr_parser,.cb = nlattr_get_nested },
	{ .type = PF_RT_RIDENTIFIER, .off = _OUT(ridentifier), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_LABELS, .off = _OUT(label), .arg = &rule_labels_parser,.cb = nlattr_get_nested_pf_rule_labels },
	{ .type = PF_RT_IFNAME, .off = _OUT(ifname), .arg = (void *)IFNAMSIZ, .cb = nlattr_get_chara },
	{ .type = PF_RT_QNAME, .off = _OUT(qname), .arg = (void *)PF_QNAME_SIZE, .cb = nlattr_get_chara },
	{ .type = PF_RT_PQNAME, .off = _OUT(pqname), .arg = (void *)PF_QNAME_SIZE, .cb = nlattr_get_chara },
	{ .type = PF_RT_TAGNAME, .off = _OUT(tagname), .arg = (void *)PF_TAG_NAME_SIZE, .cb = nlattr_get_chara },
	{ .type = PF_RT_MATCH_TAGNAME, .off = _OUT(match_tagname), .arg = (void *)PF_TAG_NAME_SIZE, .cb = nlattr_get_chara },
	{ .type = PF_RT_OVERLOAD_TBLNAME, .off = _OUT(overload_tblname), .arg = (void *)PF_TABLE_NAME_SIZE, .cb = nlattr_get_chara },
	{ .type = PF_RT_RPOOL, .off = _OUT(rpool), .arg = &pool_parser, .cb = nlattr_get_nested },
	{ .type = PF_RT_OS_FINGERPRINT, .off = _OUT(os_fingerprint), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_RTABLEID, .off = _OUT(rtableid), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_TIMEOUT, .off = _OUT(timeout), .arg = &timeout_parser, .cb = nlattr_get_nested_timeouts },
	{ .type = PF_RT_MAX_STATES, .off = _OUT(max_states), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_MAX_SRC_NODES, .off = _OUT(max_src_nodes), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_MAX_SRC_STATES, .off = _OUT(max_src_states), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_MAX_SRC_CONN_RATE_LIMIT, .off = _OUT(max_src_conn_rate.limit), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_MAX_SRC_CONN_RATE_SECS, .off = _OUT(max_src_conn_rate.seconds), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_DNPIPE, .off = _OUT(dnpipe), .cb = nlattr_get_uint16 },
	{ .type = PF_RT_DNRPIPE, .off = _OUT(dnrpipe), .cb = nlattr_get_uint16 },
	{ .type = PF_RT_DNFLAGS, .off = _OUT(free_flags), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_NR, .off = _OUT(nr), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_PROB, .off = _OUT(prob), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_CUID, .off = _OUT(cuid), .cb = nlattr_get_uint32 },
	{. type = PF_RT_CPID, .off = _OUT(cpid), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_RETURN_ICMP, .off = _OUT(return_icmp), .cb = nlattr_get_uint16 },
	{ .type = PF_RT_RETURN_ICMP6, .off = _OUT(return_icmp6), .cb = nlattr_get_uint16 },
	{ .type = PF_RT_MAX_MSS, .off = _OUT(max_mss), .cb = nlattr_get_uint16 },
	{ .type = PF_RT_SCRUB_FLAGS, .off = _OUT(scrub_flags), .cb = nlattr_get_uint16 },
	{ .type = PF_RT_UID, .off = _OUT(uid), .arg = &rule_uid_parser, .cb = nlattr_get_nested },
	{ .type = PF_RT_GID, .off = _OUT(gid), .arg = &rule_uid_parser, .cb = nlattr_get_nested },
	{ .type = PF_RT_RULE_FLAG, .off = _OUT(rule_flag), .cb = nlattr_get_uint32 },
	{ .type = PF_RT_ACTION, .off = _OUT(action), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_DIRECTION, .off = _OUT(direction), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_LOG, .off = _OUT(log), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_LOGIF, .off = _OUT(logif), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_QUICK, .off = _OUT(quick), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_IF_NOT, .off = _OUT(ifnot), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_MATCH_TAG_NOT, .off = _OUT(match_tag_not), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_NATPASS, .off = _OUT(natpass), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_KEEP_STATE, .off = _OUT(keep_state), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_AF, .off = _OUT(af), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_PROTO, .off = _OUT(proto), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_TYPE, .off = _OUT(type), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_CODE, .off = _OUT(code), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_FLAGS, .off = _OUT(flags), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_FLAGSET, .off = _OUT(flagset), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_MIN_TTL, .off = _OUT(min_ttl), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_ALLOW_OPTS, .off = _OUT(allow_opts), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_RT, .off = _OUT(rt), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_RETURN_TTL, .off = _OUT(return_ttl), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_TOS, .off = _OUT(tos), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_SET_TOS, .off = _OUT(set_tos), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_ANCHOR_RELATIVE, .off = _OUT(anchor_relative), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_ANCHOR_WILDCARD, .off = _OUT(anchor_wildcard), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_FLUSH, .off = _OUT(flush), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_PRIO, .off = _OUT(prio), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_SET_PRIO, .off = _OUT(set_prio[0]), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_SET_PRIO_REPLY, .off = _OUT(set_prio[1]), .cb = nlattr_get_uint8 },
	{ .type = PF_RT_DIVERT_ADDRESS, .off = _OUT(divert.addr), .cb = nlattr_get_in6_addr },
	{ .type = PF_RT_DIVERT_PORT, .off = _OUT(divert.port), .cb = nlattr_get_uint16 },
	{ .type = PF_RT_RCV_IFNAME, .off = _OUT(rcv_ifname), .arg = (void *)IFNAMSIZ, .cb = nlattr_get_chara },
	{ .type = PF_RT_MAX_SRC_CONN, .off = _OUT(max_src_conn), .cb = nlattr_get_uint32 },
};
NL_DECLARE_ATTR_PARSER(rule_parser, nla_p_rule);
#undef _OUT
struct nl_parsed_addrule {
	struct pf_krule	*rule;
	uint32_t	 ticket;
	uint32_t	 pool_ticket;
	char		*anchor;
	char		*anchor_call;
};
#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_addrule, _field)
static const struct nlattr_parser nla_p_addrule[] = {
	{ .type = PF_ART_TICKET, .off = _OUT(ticket), .cb = nlattr_get_uint32 },
	{ .type = PF_ART_POOL_TICKET, .off = _OUT(pool_ticket), .cb = nlattr_get_uint32 },
	{ .type = PF_ART_ANCHOR, .off = _OUT(anchor), .cb = nlattr_get_string },
	{ .type = PF_ART_ANCHOR_CALL, .off = _OUT(anchor_call), .cb = nlattr_get_string },
	{ .type = PF_ART_RULE, .off = _OUT(rule), .arg = &rule_parser, .cb = nlattr_get_nested_ptr }
};
static const struct nlfield_parser nlf_p_addrule[] = {
};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(addrule_parser, struct genlmsghdr, nlf_p_addrule, nla_p_addrule);

static int
pf_handle_addrule(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	int error;
	struct nl_parsed_addrule attrs = {};

	attrs.rule = pf_krule_alloc();

	error = nl_parse_nlmsg(hdr, &addrule_parser, npt, &attrs);
	if (error != 0) {
		pf_free_rule(attrs.rule);
		return (error);
	}

	error = pf_ioctl_addrule(attrs.rule, attrs.ticket, attrs.pool_ticket,
	    attrs.anchor, attrs.anchor_call, nlp_get_cred(npt->nlp)->cr_uid,
	    hdr->nlmsg_pid);

	return (error);
}

#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pfioc_rule, _field)
static const struct nlattr_parser nla_p_getrules[] = {
	{ .type = PF_GR_ANCHOR, .off = _OUT(anchor), .arg = (void *)MAXPATHLEN, .cb = nlattr_get_chara },
	{ .type = PF_GR_ACTION, .off = _OUT(rule.action), .cb = nlattr_get_uint8 },
};
static const struct nlfield_parser nlf_p_getrules[] = {
};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(getrules_parser, struct genlmsghdr, nlf_p_getrules, nla_p_getrules);

static int
pf_handle_getrules(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pfioc_rule attrs = {};
	int error;
	struct nl_writer *nw = npt->nw;
	struct genlmsghdr *ghdr_new;

	error = nl_parse_nlmsg(hdr, &getrules_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (ENOMEM);

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_GETRULES;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	error = pf_ioctl_getrules(&attrs);
	if (error != 0)
		goto out;

	nlattr_add_u32(nw, PF_GR_NR, attrs.nr);
	nlattr_add_u32(nw, PF_GR_TICKET, attrs.ticket);

	if (!nlmsg_end(nw)) {
		error = ENOMEM;
		goto out;
	}

	return (0);

out:
	nlmsg_abort(nw);
	return (error);
}

struct nl_parsed_get_rule {
	char anchor[MAXPATHLEN];
	uint8_t action;
	uint32_t nr;
	uint32_t ticket;
	uint8_t clear;
};
#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_get_rule, _field)
static const struct nlattr_parser nla_p_getrule[] = {
	{ .type = PF_GR_ANCHOR, .off = _OUT(anchor), .arg = (void *)MAXPATHLEN, .cb = nlattr_get_chara },
	{ .type = PF_GR_ACTION, .off = _OUT(action), .cb = nlattr_get_uint8 },
	{ .type = PF_GR_NR, .off = _OUT(nr), .cb = nlattr_get_uint32 },
	{ .type = PF_GR_TICKET, .off = _OUT(ticket), .cb = nlattr_get_uint32 },
	{ .type = PF_GR_CLEAR, .off = _OUT(clear), .cb = nlattr_get_uint8 },
};
static const struct nlfield_parser nlf_p_getrule[] = {
};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(getrule_parser, struct genlmsghdr, nlf_p_getrule, nla_p_getrule);

static int
pf_handle_getrule(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	char				 anchor_call[MAXPATHLEN];
	struct nl_parsed_get_rule	 attrs = {};
	struct nl_writer		*nw = npt->nw;
	struct genlmsghdr		*ghdr_new;
	struct pf_kruleset		*ruleset;
	struct pf_krule			*rule;
	int				 rs_num;
	int				 error;

	error = nl_parse_nlmsg(hdr, &getrule_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (ENOMEM);

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_GETRULE;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	PF_RULES_WLOCK();
	ruleset = pf_find_kruleset(attrs.anchor);
	if (ruleset == NULL) {
		PF_RULES_WUNLOCK();
		error = ENOENT;
		goto out;
	}

	rs_num = pf_get_ruleset_number(attrs.action);
	if (rs_num >= PF_RULESET_MAX) {
		PF_RULES_WUNLOCK();
		error = EINVAL;
		goto out;
	}

	if (attrs.ticket != ruleset->rules[rs_num].active.ticket) {
		PF_RULES_WUNLOCK();
		error = EBUSY;
		goto out;
	}

	rule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
	while ((rule != NULL) && (rule->nr != attrs.nr))
		rule = TAILQ_NEXT(rule, entries);
	if (rule == NULL) {
		PF_RULES_WUNLOCK();
		error = EBUSY;
		goto out;
	}

	nlattr_add_rule_addr(nw, PF_RT_SRC, &rule->src);
	nlattr_add_rule_addr(nw, PF_RT_DST, &rule->dst);
	nlattr_add_u32(nw, PF_RT_RIDENTIFIER, rule->ridentifier);
	nlattr_add_labels(nw, PF_RT_LABELS, rule);
	nlattr_add_string(nw, PF_RT_IFNAME, rule->ifname);
	nlattr_add_string(nw, PF_RT_QNAME, rule->qname);
	nlattr_add_string(nw, PF_RT_PQNAME, rule->pqname);
	nlattr_add_string(nw, PF_RT_TAGNAME, rule->tagname);
	nlattr_add_string(nw, PF_RT_MATCH_TAGNAME, rule->match_tagname);
	nlattr_add_string(nw, PF_RT_OVERLOAD_TBLNAME, rule->overload_tblname);
	nlattr_add_pool(nw, PF_RT_RPOOL, &rule->rpool);
	nlattr_add_u32(nw, PF_RT_OS_FINGERPRINT, rule->os_fingerprint);
	nlattr_add_u32(nw, PF_RT_RTABLEID, rule->rtableid);
	nlattr_add_timeout(nw, PF_RT_TIMEOUT, rule->timeout);
	nlattr_add_u32(nw, PF_RT_MAX_STATES, rule->max_states);
	nlattr_add_u32(nw, PF_RT_MAX_SRC_NODES, rule->max_src_nodes);
	nlattr_add_u32(nw, PF_RT_MAX_SRC_STATES, rule->max_src_states);
	nlattr_add_u32(nw, PF_RT_MAX_SRC_CONN, rule->max_src_conn);
	nlattr_add_u32(nw, PF_RT_MAX_SRC_CONN_RATE_LIMIT, rule->max_src_conn_rate.limit);
	nlattr_add_u32(nw, PF_RT_MAX_SRC_CONN_RATE_SECS, rule->max_src_conn_rate.seconds);

	nlattr_add_u16(nw, PF_RT_DNPIPE, rule->dnpipe);
	nlattr_add_u16(nw, PF_RT_DNRPIPE, rule->dnrpipe);
	nlattr_add_u32(nw, PF_RT_DNFLAGS, rule->free_flags);

	nlattr_add_u32(nw, PF_RT_NR, rule->nr);
	nlattr_add_u32(nw, PF_RT_PROB, rule->prob);
	nlattr_add_u32(nw, PF_RT_CUID, rule->cuid);
	nlattr_add_u32(nw, PF_RT_CPID, rule->cpid);

	nlattr_add_u16(nw, PF_RT_RETURN_ICMP, rule->return_icmp);
	nlattr_add_u16(nw, PF_RT_RETURN_ICMP6, rule->return_icmp6);
	nlattr_add_u16(nw, PF_RT_RETURN_ICMP6, rule->return_icmp6);
	nlattr_add_u16(nw, PF_RT_MAX_MSS, rule->max_mss);
	nlattr_add_u16(nw, PF_RT_SCRUB_FLAGS, rule->scrub_flags);

	nlattr_add_rule_uid(nw, PF_RT_UID, &rule->uid);
	nlattr_add_rule_uid(nw, PF_RT_GID, (const struct pf_rule_uid *)&rule->gid);

	nlattr_add_string(nw, PF_RT_RCV_IFNAME, rule->rcv_ifname);

	nlattr_add_u32(nw, PF_RT_RULE_FLAG, rule->rule_flag);
	nlattr_add_u8(nw, PF_RT_ACTION, rule->action);
	nlattr_add_u8(nw, PF_RT_DIRECTION, rule->direction);
	nlattr_add_u8(nw, PF_RT_LOG, rule->log);
	nlattr_add_u8(nw, PF_RT_LOGIF, rule->logif);
	nlattr_add_u8(nw, PF_RT_QUICK, rule->quick);
	nlattr_add_u8(nw, PF_RT_IF_NOT, rule->ifnot);
	nlattr_add_u8(nw, PF_RT_MATCH_TAG_NOT, rule->match_tag_not);
	nlattr_add_u8(nw, PF_RT_NATPASS, rule->natpass);
	nlattr_add_u8(nw, PF_RT_KEEP_STATE, rule->keep_state);

	nlattr_add_u8(nw, PF_RT_AF, rule->af);
	nlattr_add_u8(nw, PF_RT_PROTO, rule->proto);
	nlattr_add_u8(nw, PF_RT_TYPE, rule->type);
	nlattr_add_u8(nw, PF_RT_CODE, rule->code);
	nlattr_add_u8(nw, PF_RT_FLAGS, rule->flags);
	nlattr_add_u8(nw, PF_RT_FLAGSET, rule->flagset);
	nlattr_add_u8(nw, PF_RT_MIN_TTL, rule->min_ttl);
	nlattr_add_u8(nw, PF_RT_ALLOW_OPTS, rule->allow_opts);
	nlattr_add_u8(nw, PF_RT_RT, rule->rt);
	nlattr_add_u8(nw, PF_RT_RETURN_TTL, rule->return_ttl);
	nlattr_add_u8(nw, PF_RT_TOS, rule->tos);
	nlattr_add_u8(nw, PF_RT_SET_TOS, rule->set_tos);
	nlattr_add_u8(nw, PF_RT_ANCHOR_RELATIVE, rule->anchor_relative);
	nlattr_add_u8(nw, PF_RT_ANCHOR_WILDCARD, rule->anchor_wildcard);
	nlattr_add_u8(nw, PF_RT_FLUSH, rule->flush);
	nlattr_add_u8(nw, PF_RT_PRIO, rule->prio);
	nlattr_add_u8(nw, PF_RT_SET_PRIO, rule->set_prio[0]);
	nlattr_add_u8(nw, PF_RT_SET_PRIO_REPLY, rule->set_prio[1]);

	nlattr_add_in6_addr(nw, PF_RT_DIVERT_ADDRESS, &rule->divert.addr.v6);
	nlattr_add_u16(nw, PF_RT_DIVERT_PORT, rule->divert.port);

	nlattr_add_u64(nw, PF_RT_PACKETS_IN, pf_counter_u64_fetch(&rule->packets[0]));
	nlattr_add_u64(nw, PF_RT_PACKETS_OUT, pf_counter_u64_fetch(&rule->packets[1]));
	nlattr_add_u64(nw, PF_RT_BYTES_IN, pf_counter_u64_fetch(&rule->bytes[0]));
	nlattr_add_u64(nw, PF_RT_BYTES_OUT, pf_counter_u64_fetch(&rule->bytes[1]));
	nlattr_add_u64(nw, PF_RT_EVALUATIONS, pf_counter_u64_fetch(&rule->evaluations));
	nlattr_add_u64(nw, PF_RT_TIMESTAMP, pf_get_timestamp(rule));
	nlattr_add_u64(nw, PF_RT_STATES_CUR, counter_u64_fetch(rule->states_cur));
	nlattr_add_u64(nw, PF_RT_STATES_TOTAL, counter_u64_fetch(rule->states_tot));
	nlattr_add_u64(nw, PF_RT_SRC_NODES, counter_u64_fetch(rule->src_nodes));

	error = pf_kanchor_copyout(ruleset, rule, anchor_call, sizeof(anchor_call));
	MPASS(error == 0);

	nlattr_add_string(nw, PF_RT_ANCHOR_CALL, anchor_call);

	if (attrs.clear)
		pf_krule_clear_counters(rule);

	PF_RULES_WUNLOCK();

	if (!nlmsg_end(nw)) {
		error = ENOMEM;
		goto out;
	}

	return (0);
out:
	nlmsg_abort(nw);
	return (error);
}

#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pf_kstate_kill, _field)
static const struct nlattr_parser nla_p_clear_states[] = {
	{ .type = PF_CS_CMP_ID, .off = _OUT(psk_pfcmp.id), .cb = nlattr_get_uint64 },
	{ .type = PF_CS_CMP_CREATORID, .off = _OUT(psk_pfcmp.creatorid), .cb = nlattr_get_uint32 },
	{ .type = PF_CS_CMP_DIR, .off = _OUT(psk_pfcmp.direction), .cb = nlattr_get_uint8 },
	{ .type = PF_CS_AF, .off = _OUT(psk_af), .cb = nlattr_get_uint8 },
	{ .type = PF_CS_PROTO, .off = _OUT(psk_proto), .cb = nlattr_get_uint8 },
	{ .type = PF_CS_SRC, .off = _OUT(psk_src), .arg = &rule_addr_parser, .cb = nlattr_get_nested },
	{ .type = PF_CS_DST, .off = _OUT(psk_dst), .arg = &rule_addr_parser, .cb = nlattr_get_nested },
	{ .type = PF_CS_RT_ADDR, .off = _OUT(psk_rt_addr), .arg = &rule_addr_parser, .cb = nlattr_get_nested },
	{ .type = PF_CS_IFNAME, .off = _OUT(psk_ifname), .arg = (void *)IFNAMSIZ, .cb = nlattr_get_chara },
	{ .type = PF_CS_LABEL, .off = _OUT(psk_label), .arg = (void *)PF_RULE_LABEL_SIZE, .cb = nlattr_get_chara },
	{ .type = PF_CS_KILL_MATCH, .off = _OUT(psk_kill_match), .cb = nlattr_get_bool },
	{ .type = PF_CS_NAT, .off = _OUT(psk_nat), .cb = nlattr_get_bool },
};
static const struct nlfield_parser nlf_p_clear_states[] = {};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(clear_states_parser, struct genlmsghdr, nlf_p_clear_states, nla_p_clear_states);

static int
pf_handle_killclear_states(struct nlmsghdr *hdr, struct nl_pstate *npt, int cmd)
{
	struct pf_kstate_kill		 kill = {};
	struct epoch_tracker		 et;
	struct nl_writer		*nw = npt->nw;
	struct genlmsghdr		*ghdr_new;
	int				 error;
	unsigned int			 killed = 0;

	error = nl_parse_nlmsg(hdr, &clear_states_parser, npt, &kill);
	if (error != 0)
		return (error);

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (ENOMEM);

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = cmd;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	NET_EPOCH_ENTER(et);
	if (cmd == PFNL_CMD_KILLSTATES)
		pf_killstates(&kill, &killed);
	else
		killed = pf_clear_states(&kill);
	NET_EPOCH_EXIT(et);

	nlattr_add_u32(nw, PF_CS_KILLED, killed);

	if (! nlmsg_end(nw)) {
		error = ENOMEM;
		goto out;
	}

	return (0);

out:
	nlmsg_abort(nw);
	return (error);
}

static int
pf_handle_clear_states(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	return (pf_handle_killclear_states(hdr, npt, PFNL_CMD_CLRSTATES));
}

static int
pf_handle_kill_states(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	return (pf_handle_killclear_states(hdr, npt, PFNL_CMD_KILLSTATES));
}

struct nl_parsed_set_statusif {
	char ifname[IFNAMSIZ];
};
#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_set_statusif, _field)
static const struct nlattr_parser nla_p_set_statusif[] = {
	{ .type = PF_SS_IFNAME, .off = _OUT(ifname), .arg = (const void *)IFNAMSIZ, .cb = nlattr_get_chara },
};
static const struct nlfield_parser nlf_p_set_statusif[] = {};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(set_statusif_parser, struct genlmsghdr, nlf_p_set_statusif, nla_p_set_statusif);

static int
pf_handle_set_statusif(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	int error;
	struct nl_parsed_set_statusif attrs = {};

	error = nl_parse_nlmsg(hdr, &set_statusif_parser, npt, &attrs);
	if (error != 0)
		return (error);

	PF_RULES_WLOCK();
	strlcpy(V_pf_status.ifname, attrs.ifname, IFNAMSIZ);
	PF_RULES_WUNLOCK();

	return (0);
}

static bool
nlattr_add_counters(struct nl_writer *nw, int attr, size_t number, char **names,
    counter_u64_t *counters)
{
	for (int i = 0; i < number; i++) {
		int off = nlattr_add_nested(nw, attr);
		nlattr_add_u32(nw, PF_C_ID, i);
		nlattr_add_string(nw, PF_C_NAME, names[i]);
		nlattr_add_u64(nw, PF_C_COUNTER, counter_u64_fetch(counters[i]));
		nlattr_set_len(nw, off);
	}

	return (true);
}

static bool
nlattr_add_fcounters(struct nl_writer *nw, int attr, size_t number, char **names,
    struct pf_counter_u64 *counters)
{
	for (int i = 0; i < number; i++) {
		int off = nlattr_add_nested(nw, attr);
		nlattr_add_u32(nw, PF_C_ID, i);
		nlattr_add_string(nw, PF_C_NAME, names[i]);
		nlattr_add_u64(nw, PF_C_COUNTER, pf_counter_u64_fetch(&counters[i]));
		nlattr_set_len(nw, off);
	}

	return (true);
}

static bool
nlattr_add_u64_array(struct nl_writer *nw, int attr, size_t number, uint64_t *array)
{
	int off = nlattr_add_nested(nw, attr);

	for (size_t i = 0; i < number; i++)
		nlattr_add_u64(nw, 0, array[i]);

	nlattr_set_len(nw, off);

	return (true);
}

static int
pf_handle_get_status(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pf_status s;
	struct nl_writer *nw = npt->nw;
	struct genlmsghdr *ghdr_new;
	char *pf_reasons[PFRES_MAX+1] = PFRES_NAMES;
	char *pf_lcounter[KLCNT_MAX+1] = KLCNT_NAMES;
	char *pf_fcounter[FCNT_MAX+1] = FCNT_NAMES;
	int error;

	PF_RULES_RLOCK_TRACKER;

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (ENOMEM);

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_GET_STATUS;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	PF_RULES_RLOCK();

	nlattr_add_string(nw, PF_GS_IFNAME, V_pf_status.ifname);
	nlattr_add_bool(nw, PF_GS_RUNNING, V_pf_status.running);
	nlattr_add_u32(nw, PF_GS_SINCE, V_pf_status.since);
	nlattr_add_u32(nw, PF_GS_DEBUG, V_pf_status.debug);
	nlattr_add_u32(nw, PF_GS_HOSTID, ntohl(V_pf_status.hostid));
	nlattr_add_u32(nw, PF_GS_STATES, V_pf_status.states);
	nlattr_add_u32(nw, PF_GS_SRC_NODES, V_pf_status.src_nodes);
	nlattr_add_u32(nw, PF_GS_REASSEMBLE, V_pf_status.reass);
	nlattr_add_u32(nw, PF_GS_SYNCOOKIES_ACTIVE, V_pf_status.syncookies_active);

	nlattr_add_counters(nw, PF_GS_COUNTERS, PFRES_MAX, pf_reasons,
	    V_pf_status.counters);
	nlattr_add_counters(nw, PF_GS_LCOUNTERS, KLCNT_MAX, pf_lcounter,
	    V_pf_status.lcounters);
	nlattr_add_fcounters(nw, PF_GS_FCOUNTERS, FCNT_MAX, pf_fcounter,
	    V_pf_status.fcounters);
	nlattr_add_counters(nw, PF_GS_SCOUNTERS, SCNT_MAX, pf_fcounter,
	    V_pf_status.scounters);

	pfi_update_status(V_pf_status.ifname, &s);
	nlattr_add_u64_array(nw, PF_GS_BCOUNTERS, 2 * 2, (uint64_t *)s.bcounters);
	nlattr_add_u64_array(nw, PF_GS_PCOUNTERS, 2 * 2 * 2, (uint64_t *)s.pcounters);

	nlattr_add(nw, PF_GS_CHKSUM, PF_MD5_DIGEST_LENGTH, V_pf_status.pf_chksum);

	PF_RULES_RUNLOCK();

	if (!nlmsg_end(nw)) {
		error = ENOMEM;
		goto out;
	}

	return (0);

out:
	nlmsg_abort(nw);
	return (error);
}

static int
pf_handle_clear_status(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	pf_ioctl_clear_status();

	return (0);
}

struct pf_nl_natlook {
	sa_family_t af;
	uint8_t direction;
	uint8_t proto;
	struct pf_addr src;
	struct pf_addr dst;
	uint16_t sport;
	uint16_t dport;
};

#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pf_nl_natlook, _field)
static const struct nlattr_parser nla_p_natlook[] = {
	{ .type = PF_NL_AF, .off = _OUT(af), .cb = nlattr_get_uint8 },
	{ .type = PF_NL_DIRECTION, .off = _OUT(direction), .cb = nlattr_get_uint8 },
	{ .type = PF_NL_PROTO, .off = _OUT(proto), .cb = nlattr_get_uint8 },
	{ .type = PF_NL_SRC_ADDR, .off = _OUT(src), .cb = nlattr_get_in6_addr },
	{ .type = PF_NL_DST_ADDR, .off = _OUT(dst), .cb = nlattr_get_in6_addr },
	{ .type = PF_NL_SRC_PORT, .off = _OUT(sport), .cb = nlattr_get_uint16 },
	{ .type = PF_NL_DST_PORT, .off = _OUT(dport), .cb = nlattr_get_uint16 },
};
static const struct nlfield_parser nlf_p_natlook[] = {};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(natlook_parser, struct genlmsghdr, nlf_p_natlook, nla_p_natlook);

static int
pf_handle_natlook(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pf_nl_natlook	 attrs = {};
	struct pf_state_key_cmp	 key = {};
	struct nl_writer	*nw = npt->nw;
	struct pf_state_key	*sk;
	struct pf_kstate	*state;
	struct genlmsghdr	*ghdr_new;
	int			 error, m;
	int			 sidx, didx;

	error = nl_parse_nlmsg(hdr, &natlook_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (attrs.proto == 0 ||
	    PF_AZERO(&attrs.src, attrs.af) ||
	    PF_AZERO(&attrs.dst, attrs.af) ||
	    ((attrs.proto == IPPROTO_TCP || attrs.proto == IPPROTO_UDP) &&
	     (attrs.sport == 0 || attrs.dport == 0)))
		return (EINVAL);

	/* NATLOOK src and dst are reversed, so reverse sidx/didx */
	sidx = (attrs.direction == PF_IN) ? 1 : 0;
	didx = (attrs.direction == PF_IN) ? 0 : 1;

	key.af = attrs.af;
	key.proto = attrs.proto;
	PF_ACPY(&key.addr[sidx], &attrs.src, attrs.af);
	key.port[sidx] = attrs.sport;
	PF_ACPY(&key.addr[didx], &attrs.dst, attrs.af);
	key.port[didx] = attrs.dport;

	state = pf_find_state_all(&key, attrs.direction, &m);
	if (state == NULL)
		return (ENOENT);
	if (m > 1) {
		PF_STATE_UNLOCK(state);
		return (E2BIG);
	}

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr))) {
		PF_STATE_UNLOCK(state);
		return (ENOMEM);
	}

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_NATLOOK;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	sk = state->key[sidx];

	nlattr_add_in6_addr(nw, PF_NL_SRC_ADDR, &sk->addr[sidx].v6);
	nlattr_add_in6_addr(nw, PF_NL_DST_ADDR, &sk->addr[didx].v6);
	nlattr_add_u16(nw, PF_NL_SRC_PORT, sk->port[sidx]);
	nlattr_add_u16(nw, PF_NL_DST_PORT, sk->port[didx]);

	PF_STATE_UNLOCK(state);

	if (!nlmsg_end(nw)) {
		nlmsg_abort(nw);
		return (ENOMEM);
	}

	return (0);
}

struct pf_nl_set_debug
{
	uint32_t level;
};
#define	_OUT(_field)	offsetof(struct pf_nl_set_debug, _field)
static const struct nlattr_parser nla_p_set_debug[] = {
	{ .type = PF_SD_LEVEL, .off = _OUT(level), .cb = nlattr_get_uint32 },
};
static const struct nlfield_parser nlf_p_set_debug[] = {};
#undef _OUT
NL_DECLARE_PARSER(set_debug_parser, struct genlmsghdr, nlf_p_set_debug, nla_p_set_debug);

static int
pf_handle_set_debug(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pf_nl_set_debug attrs = {};
	int error;

	error = nl_parse_nlmsg(hdr, &set_debug_parser, npt, &attrs);
	if (error != 0)
		return (error);

	PF_RULES_WLOCK();
	V_pf_status.debug = attrs.level;
	PF_RULES_WUNLOCK();

	return (0);
}

struct pf_nl_set_timeout
{
	uint32_t timeout;
	uint32_t seconds;
};
#define	_OUT(_field)	offsetof(struct pf_nl_set_timeout, _field)
static const struct nlattr_parser nla_p_set_timeout[] = {
	{ .type = PF_TO_TIMEOUT, .off = _OUT(timeout), .cb = nlattr_get_uint32 },
	{ .type = PF_TO_SECONDS, .off = _OUT(seconds), .cb = nlattr_get_uint32 },
};
static const struct nlfield_parser nlf_p_set_timeout[] = {};
#undef _OUT
NL_DECLARE_PARSER(set_timeout_parser, struct genlmsghdr, nlf_p_set_timeout, nla_p_set_timeout);

static int
pf_handle_set_timeout(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pf_nl_set_timeout attrs = {};
	int error;

	error = nl_parse_nlmsg(hdr, &set_timeout_parser, npt, &attrs);
	if (error != 0)
		return (error);

	return (pf_ioctl_set_timeout(attrs.timeout, attrs.seconds, NULL));
}

static int
pf_handle_get_timeout(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pf_nl_set_timeout attrs = {};
	struct nl_writer *nw = npt->nw;
	struct genlmsghdr *ghdr_new;
	int error;

	error = nl_parse_nlmsg(hdr, &set_timeout_parser, npt, &attrs);
	if (error != 0)
		return (error);

	error = pf_ioctl_get_timeout(attrs.timeout, &attrs.seconds);
	if (error != 0)
		return (error);

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (ENOMEM);

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_GET_TIMEOUT;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	nlattr_add_u32(nw, PF_TO_SECONDS, attrs.seconds);

	if (!nlmsg_end(nw)) {
		nlmsg_abort(nw);
		return (ENOMEM);
	}

	return (0);
}

struct pf_nl_set_limit
{
	uint32_t index;
	uint32_t limit;
};
#define	_OUT(_field)	offsetof(struct pf_nl_set_limit, _field)
static const struct nlattr_parser nla_p_set_limit[] = {
	{ .type = PF_LI_INDEX, .off = _OUT(index), .cb = nlattr_get_uint32 },
	{ .type = PF_LI_LIMIT, .off = _OUT(limit), .cb = nlattr_get_uint32 },
};
static const struct nlfield_parser nlf_p_set_limit[] = {};
#undef _OUT
NL_DECLARE_PARSER(set_limit_parser, struct genlmsghdr, nlf_p_set_limit, nla_p_set_limit);

static int
pf_handle_set_limit(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pf_nl_set_limit attrs = {};
	int error;

	error = nl_parse_nlmsg(hdr, &set_limit_parser, npt, &attrs);
	if (error != 0)
		return (error);

	return (pf_ioctl_set_limit(attrs.index, attrs.limit, NULL));
}

static int
pf_handle_get_limit(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pf_nl_set_limit attrs = {};
	struct nl_writer *nw = npt->nw;
	struct genlmsghdr *ghdr_new;
	int error;

	error = nl_parse_nlmsg(hdr, &set_limit_parser, npt, &attrs);
	if (error != 0)
		return (error);

	error = pf_ioctl_get_limit(attrs.index, &attrs.limit);
	if (error != 0)
		return (error);

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (ENOMEM);

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_GET_LIMIT;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	nlattr_add_u32(nw, PF_LI_LIMIT, attrs.limit);

	if (!nlmsg_end(nw)) {
		nlmsg_abort(nw);
		return (ENOMEM);
	}

	return (0);
}

static int
pf_handle_begin_addrs(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct nl_writer *nw = npt->nw;
	struct genlmsghdr *ghdr_new;
	uint32_t ticket;
	int error;

	error = pf_ioctl_begin_addrs(&ticket);
	if (error != 0)
		return (error);

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (ENOMEM);

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_BEGIN_ADDRS;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	nlattr_add_u32(nw, PF_BA_TICKET, ticket);

	if (!nlmsg_end(nw)) {
		nlmsg_abort(nw);
		return (ENOMEM);
	}

	return (0);
}

static bool
nlattr_add_pool_addr(struct nl_writer *nw, int attrtype, struct pf_pooladdr *a)
{
	int off;

	off = nlattr_add_nested(nw, attrtype);

	nlattr_add_addr_wrap(nw, PF_PA_ADDR, &a->addr);
	nlattr_add_string(nw, PF_PA_IFNAME, a->ifname);

	nlattr_set_len(nw, off);

	return (true);
}

#define _OUT(_field)	offsetof(struct pf_pooladdr, _field)
static const struct nlattr_parser nla_p_pool_addr[] = {
	{ .type = PF_PA_ADDR, .off = _OUT(addr), .arg = &addr_wrap_parser, .cb = nlattr_get_nested },
	{ .type = PF_PA_IFNAME, .off = _OUT(ifname), .arg = (void *)IFNAMSIZ, .cb = nlattr_get_chara },
};
NL_DECLARE_ATTR_PARSER(pool_addr_parser, nla_p_pool_addr);
#undef _OUT

#define	_OUT(_field)	offsetof(struct pfioc_pooladdr, _field)
static const struct nlattr_parser nla_p_add_addr[] = {
	{ .type = PF_AA_ACTION, .off = _OUT(action), .cb = nlattr_get_uint32 },
	{ .type = PF_AA_TICKET, .off = _OUT(ticket), .cb = nlattr_get_uint32 },
	{ .type = PF_AA_NR, .off = _OUT(nr), .cb = nlattr_get_uint32 },
	{ .type = PF_AA_R_NUM, .off = _OUT(r_num), .cb = nlattr_get_uint32 },
	{ .type = PF_AA_R_ACTION, .off = _OUT(r_action), .cb = nlattr_get_uint8 },
	{ .type = PF_AA_R_LAST, .off = _OUT(r_last), .cb = nlattr_get_uint8 },
	{ .type = PF_AA_AF, .off = _OUT(af), .cb = nlattr_get_uint8 },
	{ .type = PF_AA_ANCHOR, .off = _OUT(anchor), .arg = (void *)MAXPATHLEN, .cb = nlattr_get_chara },
	{ .type = PF_AA_ADDR, .off = _OUT(addr), .arg = &pool_addr_parser, .cb = nlattr_get_nested },
};
static const struct nlfield_parser nlf_p_add_addr[] = {};
#undef _OUT
NL_DECLARE_PARSER(add_addr_parser, struct genlmsghdr, nlf_p_add_addr, nla_p_add_addr);

static int
pf_handle_add_addr(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pfioc_pooladdr attrs = { 0 };
	int error;

	error = nl_parse_nlmsg(hdr, &add_addr_parser, npt, &attrs);
	if (error != 0)
		return (error);

	error = pf_ioctl_add_addr(&attrs);

	return (error);
}

static int
pf_handle_get_addrs(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pfioc_pooladdr attrs = { 0 };
	struct nl_writer *nw = npt->nw;
	struct genlmsghdr *ghdr_new;
	int error;

	error = nl_parse_nlmsg(hdr, &add_addr_parser, npt, &attrs);
	if (error != 0)
		return (error);

	error = pf_ioctl_get_addrs(&attrs);
	if (error != 0)
		return (error);

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (ENOMEM);

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_GET_ADDRS;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	nlattr_add_u32(nw, PF_AA_NR, attrs.nr);

	if (!nlmsg_end(nw)) {
		nlmsg_abort(nw);
		return (ENOMEM);
	}

	return (error);
}

static int
pf_handle_get_addr(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pfioc_pooladdr attrs = { 0 };
	struct nl_writer *nw = npt->nw;
	struct genlmsghdr *ghdr_new;
	int error;

	error = nl_parse_nlmsg(hdr, &add_addr_parser, npt, &attrs);
	if (error != 0)
		return (error);

	error = pf_ioctl_get_addr(&attrs);
	if (error != 0)
		return (error);

	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (ENOMEM);

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFNL_CMD_GET_ADDRS;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	nlattr_add_u32(nw, PF_AA_ACTION, attrs.action);
	nlattr_add_u32(nw, PF_AA_TICKET, attrs.ticket);
	nlattr_add_u32(nw, PF_AA_NR, attrs.nr);
	nlattr_add_u32(nw, PF_AA_R_NUM, attrs.r_num);
	nlattr_add_u8(nw, PF_AA_R_ACTION, attrs.r_action);
	nlattr_add_u8(nw, PF_AA_R_LAST, attrs.r_last);
	nlattr_add_u8(nw, PF_AA_AF, attrs.af);
	nlattr_add_string(nw, PF_AA_ANCHOR, attrs.anchor);
	nlattr_add_pool_addr(nw, PF_AA_ADDR, &attrs.addr);

	if (!nlmsg_end(nw)) {
		nlmsg_abort(nw);
		return (ENOMEM);
	}

	return (0);
}

static const struct nlhdr_parser *all_parsers[] = {
	&state_parser,
	&addrule_parser,
	&getrules_parser,
	&clear_states_parser,
	&set_statusif_parser,
	&natlook_parser,
	&set_debug_parser,
	&set_timeout_parser,
	&set_limit_parser,
	&pool_addr_parser,
	&add_addr_parser,
};

static int family_id;

static const struct genl_cmd pf_cmds[] = {
	{
		.cmd_num = PFNL_CMD_GETSTATES,
		.cmd_name = "GETSTATES",
		.cmd_cb = pf_handle_getstates,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_GETCREATORS,
		.cmd_name = "GETCREATORS",
		.cmd_cb = pf_handle_getcreators,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_START,
		.cmd_name = "START",
		.cmd_cb = pf_handle_start,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_STOP,
		.cmd_name = "STOP",
		.cmd_cb = pf_handle_stop,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_ADDRULE,
		.cmd_name = "ADDRULE",
		.cmd_cb = pf_handle_addrule,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_GETRULES,
		.cmd_name = "GETRULES",
		.cmd_cb = pf_handle_getrules,
		.cmd_flags = GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_GETRULE,
		.cmd_name = "GETRULE",
		.cmd_cb = pf_handle_getrule,
		.cmd_flags = GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_CLRSTATES,
		.cmd_name = "CLRSTATES",
		.cmd_cb = pf_handle_clear_states,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_KILLSTATES,
		.cmd_name = "KILLSTATES",
		.cmd_cb = pf_handle_kill_states,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_SET_STATUSIF,
		.cmd_name = "SETSTATUSIF",
		.cmd_cb = pf_handle_set_statusif,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_GET_STATUS,
		.cmd_name = "GETSTATUS",
		.cmd_cb = pf_handle_get_status,
		.cmd_flags = GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_CLEAR_STATUS,
		.cmd_name = "CLEARSTATUS",
		.cmd_cb = pf_handle_clear_status,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_NATLOOK,
		.cmd_name = "NATLOOK",
		.cmd_cb = pf_handle_natlook,
		.cmd_flags = GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_SET_DEBUG,
		.cmd_name = "SET_DEBUG",
		.cmd_cb = pf_handle_set_debug,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_SET_TIMEOUT,
		.cmd_name = "SET_TIMEOUT",
		.cmd_cb = pf_handle_set_timeout,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_GET_TIMEOUT,
		.cmd_name = "GET_TIMEOUT",
		.cmd_cb = pf_handle_get_timeout,
		.cmd_flags = GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_SET_LIMIT,
		.cmd_name = "SET_LIMIT",
		.cmd_cb = pf_handle_set_limit,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_GET_LIMIT,
		.cmd_name = "GET_LIMIT",
		.cmd_cb = pf_handle_get_limit,
		.cmd_flags = GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_BEGIN_ADDRS,
		.cmd_name = "BEGIN_ADDRS",
		.cmd_cb = pf_handle_begin_addrs,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_ADD_ADDR,
		.cmd_name = "ADD_ADDR",
		.cmd_cb = pf_handle_add_addr,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_GET_ADDRS,
		.cmd_name = "GET_ADDRS",
		.cmd_cb = pf_handle_get_addrs,
		.cmd_flags = GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFNL_CMD_GET_ADDR,
		.cmd_name = "GET_ADDRS",
		.cmd_cb = pf_handle_get_addr,
		.cmd_flags = GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
};

void
pf_nl_register(void)
{
	NL_VERIFY_PARSERS(all_parsers);

	family_id = genl_register_family(PFNL_FAMILY_NAME, 0, 2, PFNL_CMD_MAX);
	genl_register_cmds(PFNL_FAMILY_NAME, pf_cmds, NL_ARRAY_LEN(pf_cmds));
}

void
pf_nl_unregister(void)
{
	genl_unregister_family(PFNL_FAMILY_NAME);
}
