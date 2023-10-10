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

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

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
};

#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_state, _field)
static const struct nlattr_parser nla_p_state[] = {
	{ .type = PF_ST_ID, .off = _OUT(id), .cb = nlattr_get_uint32 },
	{ .type = PF_ST_CREATORID, .off = _OUT(creatorid), .cb = nlattr_get_uint32 },
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
	nlattr_add_u32(nw, PF_ST_CREATION, time_uptime - s->creation);
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

	for (int i = 0; i <= pf_hashmask; i++) {
		struct pf_idhash *ih = &V_pf_idhash[i];
		struct pf_kstate *s;

		if (LIST_EMPTY(&ih->states))
			continue;

		PF_HASHROW_LOCK(ih);
		LIST_FOREACH(s, &ih->states, entry) {
			if (s->timeout != PFTM_UNLINKED) {
				error = dump_state(nlp, hdr, s, npt);
				if (error != 0)
					break;
			}
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
	struct pf_kstate *s = pf_find_state_byid(attrs->id, attrs->creatorid);
	if (s == NULL)
		return (ENOENT);
	return (dump_state(nlp, hdr, s, npt));
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

	for (int i = 0; i < pf_hashmask; i++) {
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

static const struct nlhdr_parser *all_parsers[] = { &state_parser };

static int family_id;

static const struct genl_cmd pf_cmds[] = {
	{
		.cmd_num = PFNL_CMD_GETSTATES,
		.cmd_name = "GETSTATES",
		.cmd_cb = pf_handle_getstates,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
	},
	{
		.cmd_num = PFNL_CMD_GETCREATORS,
		.cmd_name = "GETCREATORS",
		.cmd_cb = pf_handle_getcreators,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
	},
	{
		.cmd_num = PFNL_CMD_START,
		.cmd_name = "START",
		.cmd_cb = pf_handle_start,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
	},
	{
		.cmd_num = PFNL_CMD_STOP,
		.cmd_name = "STOP",
		.cmd_cb = pf_handle_stop,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
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
