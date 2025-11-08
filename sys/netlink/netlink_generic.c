/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Alexander V. Chernikov <melifaro@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ck.h>
#include <sys/epoch.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/sx.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_var.h>

#define	DEBUG_MOD_NAME	nl_generic
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);

static int nlctrl_handle_getfamily(struct nlmsghdr *, struct nl_pstate *);

static struct genl_cmd nlctrl_cmds[] = {
	[CTRL_CMD_GETFAMILY] = {
		.cmd_num = CTRL_CMD_GETFAMILY,
		.cmd_name = "GETFAMILY",
		.cmd_cb = nlctrl_handle_getfamily,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP |
		    GENL_CMD_CAP_HASPOL,
	},
};

static struct genl_family {
	const char	*family_name;
	uint16_t	family_hdrsize;
	uint16_t	family_version;
	uint16_t	family_attr_max;
	uint16_t	family_cmd_size;
	uint16_t	family_num_groups;
	struct genl_cmd	*family_cmds;
} families[MAX_FAMILIES] = {
	[CTRL_FAMILY_ID] = {
		.family_name = CTRL_FAMILY_NAME,
		.family_hdrsize = 0,
		.family_version = 2,
		.family_attr_max = CTRL_ATTR_MAX,
		.family_cmd_size = CTRL_CMD_GETFAMILY + 1,
		.family_cmds = nlctrl_cmds,
		.family_num_groups = 1,
	},
};

static struct genl_group {
	struct genl_family	*group_family;
	const char		*group_name;
} groups[MAX_GROUPS] = {
	[CTRL_GROUP_ID] = {
		.group_family = &families[CTRL_FAMILY_ID],
		.group_name = CTRL_GROUP_NAME,
	},
};

static inline struct genl_family *
genl_family(uint16_t family_id)
{
	struct genl_family *gf;

	gf = &families[family_id - GENL_MIN_ID];
	KASSERT(family_id - GENL_MIN_ID < MAX_FAMILIES &&
	    gf->family_name != NULL, ("family %u does not exist", family_id));
	return (gf);
}

static inline uint16_t
genl_family_id(const struct genl_family *gf)
{
	MPASS(gf >= &families[0] && gf < &families[MAX_FAMILIES]);
	return ((uint16_t)(gf - &families[0]) + GENL_MIN_ID);
}

/*
 * Handler called by netlink subsystem when matching netlink message is received
 */
static int
genl_handle_message(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct nlpcb *nlp = npt->nlp;
	struct genl_family *gf;
	uint16_t family_id;
	int error = 0;

	if (__predict_false(hdr->nlmsg_len < sizeof(struct nlmsghdr) +
	    GENL_HDRLEN)) {
		NLP_LOG(LOG_DEBUG, nlp, "invalid message size: %d",
		    hdr->nlmsg_len);
		return (EINVAL);
	}

	family_id = hdr->nlmsg_type - GENL_MIN_ID;
	gf = &families[family_id];
	if (__predict_false(family_id >= MAX_FAMILIES ||
	    gf->family_name == NULL)) {
		NLP_LOG(LOG_DEBUG, nlp, "invalid message type: %d",
		    hdr->nlmsg_type);
		return (ENOTSUP);
	}

	struct genlmsghdr *ghdr = (struct genlmsghdr *)(hdr + 1);

	if (ghdr->cmd >= gf->family_cmd_size || gf->family_cmds[ghdr->cmd].cmd_cb == NULL) {
		NLP_LOG(LOG_DEBUG, nlp, "family %s: invalid cmd %d",
		    gf->family_name, ghdr->cmd);
		return (ENOTSUP);
	}

	struct genl_cmd *cmd = &gf->family_cmds[ghdr->cmd];

	if (cmd->cmd_priv != 0 && !nlp_has_priv(nlp, cmd->cmd_priv)) {
		NLP_LOG(LOG_DEBUG, nlp, "family %s: cmd %d priv_check() failed",
		    gf->family_name, ghdr->cmd);
		return (EPERM);
	}

	NLP_LOG(LOG_DEBUG2, nlp, "received family %s cmd %s(%d) len %d",
	    gf->family_name, cmd->cmd_name, ghdr->cmd, hdr->nlmsg_len);

	error = cmd->cmd_cb(hdr, npt);

	return (error);
}

static uint32_t
get_cmd_flags(const struct genl_cmd *cmd)
{
	uint32_t flags = cmd->cmd_flags;
	if (cmd->cmd_priv != 0)
		flags |= GENL_ADMIN_PERM;
	return (flags);
}

static int
dump_family(struct nlmsghdr *hdr, struct genlmsghdr *ghdr,
    const struct genl_family *gf, struct nl_writer *nw)
{
	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		goto enomem;

	struct genlmsghdr *ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = ghdr->cmd;
	ghdr_new->version = gf->family_version;
	ghdr_new->reserved = 0;

        nlattr_add_string(nw, CTRL_ATTR_FAMILY_NAME, gf->family_name);
        nlattr_add_u16(nw, CTRL_ATTR_FAMILY_ID, genl_family_id(gf));
        nlattr_add_u32(nw, CTRL_ATTR_VERSION, gf->family_version);
        nlattr_add_u32(nw, CTRL_ATTR_HDRSIZE, gf->family_hdrsize);
        nlattr_add_u32(nw, CTRL_ATTR_MAXATTR, gf->family_attr_max);

	if (gf->family_cmd_size > 0) {
		int off = nlattr_add_nested(nw, CTRL_ATTR_OPS);
		if (off == 0)
			goto enomem;
		for (int i = 0, cnt=0; i < gf->family_cmd_size; i++) {
			struct genl_cmd *cmd = &gf->family_cmds[i];
			if (cmd->cmd_cb == NULL)
				continue;
			int cmd_off = nlattr_add_nested(nw, ++cnt);
			if (cmd_off == 0)
				goto enomem;

			nlattr_add_u32(nw, CTRL_ATTR_OP_ID, cmd->cmd_num);
			nlattr_add_u32(nw, CTRL_ATTR_OP_FLAGS, get_cmd_flags(cmd));
			nlattr_set_len(nw, cmd_off);
		}
		nlattr_set_len(nw, off);
	}
	if (gf->family_num_groups > 0) {
		int off = nlattr_add_nested(nw, CTRL_ATTR_MCAST_GROUPS);
		if (off == 0)
			goto enomem;
		for (u_int i = 0, cnt = 0; i < MAX_GROUPS; i++) {
			struct genl_group *gg = &groups[i];

			if (gg->group_family != gf)
				continue;

			int cmd_off = nlattr_add_nested(nw, ++cnt);
			if (cmd_off == 0)
				goto enomem;
			nlattr_add_u32(nw, CTRL_ATTR_MCAST_GRP_ID, i + MIN_GROUP_NUM);
			nlattr_add_string(nw, CTRL_ATTR_MCAST_GRP_NAME, gg->group_name);
			nlattr_set_len(nw, cmd_off);
		}
		nlattr_set_len(nw, off);
	}
	if (nlmsg_end(nw))
		return (0);
enomem:
        NL_LOG(LOG_DEBUG, "unable to dump family %s state (ENOMEM)", gf->family_name);
        nlmsg_abort(nw);
	return (ENOMEM);
}

struct nl_parsed_family {
	char		*family_name;
	uint16_t	family_id;
	uint8_t		version;
};

#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_family, _field)
static const struct nlfield_parser nlf_p_generic[] = {
	{ .off_in = _IN(version), .off_out = _OUT(version), .cb = nlf_get_u8 },
};

static struct nlattr_parser nla_p_generic[] = {
	{ .type = CTRL_ATTR_FAMILY_ID , .off = _OUT(family_id), .cb = nlattr_get_uint16 },
	{ .type = CTRL_ATTR_FAMILY_NAME , .off = _OUT(family_name), .cb = nlattr_get_string },
};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(genl_parser, struct genlmsghdr, nlf_p_generic, nla_p_generic);

static int
nlctrl_handle_getfamily(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	int error = 0;

	struct nl_parsed_family attrs = {};
	error = nl_parse_nlmsg(hdr, &genl_parser, npt, &attrs);
	if (error != 0)
		return (error);

	struct genlmsghdr ghdr = {
		.cmd = CTRL_CMD_NEWFAMILY,
	};

	if (attrs.family_id != 0 || attrs.family_name != NULL) {
		for (u_int i = 0; i < MAX_FAMILIES; i++) {
			struct genl_family *gf = &families[i];

			if (gf->family_name == NULL)
				continue;
			if (attrs.family_id != 0 &&
			    attrs.family_id != genl_family_id(gf))
				continue;
			if (attrs.family_name != NULL &&
			    strcmp(attrs.family_name, gf->family_name) != 0)
				continue;
			return (dump_family(hdr, &ghdr, gf, npt->nw));
		}
		return (ENOENT);
	}

	hdr->nlmsg_flags = hdr->nlmsg_flags | NLM_F_MULTI;
	for (u_int i = 0; i < MAX_FAMILIES; i++) {
		struct genl_family *gf = &families[i];

		if (gf->family_name != NULL) {
			error = dump_family(hdr, &ghdr, gf, npt->nw);
			if (error != 0)
				break;
		}
	}

	if (!nlmsg_end_dump(npt->nw, error, hdr)) {
                NL_LOG(LOG_DEBUG, "Unable to finalize the dump");
                return (ENOMEM);
        }

	return (error);
}

static void
nlctrl_notify(void *arg __unused, const char *family_name __unused,
    uint16_t family_id, u_int cmd)
{
	struct nlmsghdr hdr = {.nlmsg_type = NETLINK_GENERIC };
	struct genlmsghdr ghdr = { .cmd = cmd };
	struct genl_family *gf;
	struct nl_writer nw;

	gf = genl_family(family_id);
	if (!nl_writer_group(&nw, NLMSG_SMALL, NETLINK_GENERIC, CTRL_GROUP_ID,
	    0, false)) {
		NL_LOG(LOG_DEBUG, "error allocating group writer");
		return;
	}

	dump_family(&hdr, &ghdr, gf, &nw);
	nlmsg_flush(&nw);
}

static const struct nlhdr_parser *all_parsers[] = { &genl_parser };
static eventhandler_tag family_event_tag;

static void
genl_load_all(void *u __unused)
{
	NL_VERIFY_PARSERS(all_parsers);
	family_event_tag = EVENTHANDLER_REGISTER(genl_family_event,
	    nlctrl_notify, NULL, EVENTHANDLER_PRI_ANY);
	netlink_register_proto(NETLINK_GENERIC, "NETLINK_GENERIC",
	    genl_handle_message);
}
SYSINIT(genl_load_all, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, genl_load_all, NULL);

static void
genl_unload(void *u __unused)
{
	netlink_unregister_proto(NETLINK_GENERIC);
	EVENTHANDLER_DEREGISTER(genl_family_event, family_event_tag);
	NET_EPOCH_WAIT();
}
SYSUNINIT(genl_unload, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, genl_unload, NULL);

/*
 * Public KPI for NETLINK_GENERIC families/groups registration logic below.
 */

static struct sx sx_lock;
SX_SYSINIT(genl_lock, &sx_lock, "genetlink lock");
#define	GENL_LOCK()		sx_xlock(&sx_lock)
#define	GENL_UNLOCK()		sx_xunlock(&sx_lock)
#define	GENL_ASSERT_LOCKED()	sx_assert(&sx_lock, SA_LOCKED)
#define	GENL_ASSERT_XLOCKED()	sx_assert(&sx_lock, SA_XLOCKED)

uint16_t
genl_register_family(const char *family_name, size_t hdrsize,
    uint16_t family_version, uint16_t max_attr_idx)
{
	struct genl_family *gf;
	uint16_t family_id;

	MPASS(family_name != NULL);

	GENL_LOCK();
	for (u_int i = 0; i < MAX_FAMILIES; i++)
		if (families[i].family_name != NULL &&
		    strcmp(families[i].family_name, family_name) == 0) {
			GENL_UNLOCK();
			return (0);
		}

	/* Microoptimization: index 0 is reserved for the control family. */
	gf = NULL;
	for (u_int i = 1; i < MAX_FAMILIES; i++)
		if (families[i].family_name == NULL) {
			gf = &families[i];
			break;
		}
	KASSERT(gf, ("%s: maximum of %u generic netlink families allocated",
	    __func__, MAX_FAMILIES));

	*gf = (struct genl_family) {
	    .family_name = family_name,
	    .family_version = family_version,
	    .family_hdrsize = hdrsize,
	    .family_attr_max = max_attr_idx,
	};
	family_id = genl_family_id(gf);
	GENL_UNLOCK();

	NL_LOG(LOG_DEBUG2, "Registered family %s id %d", gf->family_name,
	    family_id);
	EVENTHANDLER_INVOKE(genl_family_event, gf->family_name, family_id,
	    CTRL_CMD_NEWFAMILY);

	return (family_id);
}

void
genl_unregister_family(uint16_t family_id)
{
	struct genl_family *gf;

	GENL_LOCK();
	gf = genl_family(family_id);

	EVENTHANDLER_INVOKE(genl_family_event, gf->family_name,
	    family_id, CTRL_CMD_DELFAMILY);
	for (u_int i = 0; i < MAX_GROUPS; i++) {
		struct genl_group *gg = &groups[i];
		if (gg->group_family == gf && gg->group_name != NULL) {
			gg->group_family = NULL;
			gg->group_name = NULL;
		}
	}
	if (gf->family_cmds != NULL)
		free(gf->family_cmds, M_NETLINK);
	bzero(gf, sizeof(*gf));
	GENL_UNLOCK();
}

bool
genl_register_cmds(uint16_t family_id, const struct genl_cmd *cmds,
    u_int count)
{
	struct genl_family *gf;
	uint16_t cmd_size;

	GENL_LOCK();
	gf = genl_family(family_id);

	cmd_size = gf->family_cmd_size;

	for (u_int i = 0; i < count; i++) {
		MPASS(cmds[i].cmd_cb != NULL);
		if (cmds[i].cmd_num >= cmd_size)
			cmd_size = cmds[i].cmd_num + 1;
	}

	if (cmd_size > gf->family_cmd_size) {
		void *old_data;

		/* need to realloc */
		size_t sz = cmd_size * sizeof(struct genl_cmd);
		void *data = malloc(sz, M_NETLINK, M_WAITOK | M_ZERO);

		memcpy(data, gf->family_cmds,
		    gf->family_cmd_size * sizeof(struct genl_cmd));
		old_data = gf->family_cmds;
		gf->family_cmds = data;
		gf->family_cmd_size = cmd_size;
		free(old_data, M_NETLINK);
	}

	for (u_int i = 0; i < count; i++) {
		const struct genl_cmd *cmd = &cmds[i];

		MPASS(gf->family_cmds[cmd->cmd_num].cmd_cb == NULL);
		gf->family_cmds[cmd->cmd_num] = cmds[i];
		NL_LOG(LOG_DEBUG2, "Adding cmd %s(%d) to family %s",
		    cmd->cmd_name, cmd->cmd_num, gf->family_name);
	}
	GENL_UNLOCK();
	return (true);
}

uint32_t
genl_register_group(uint16_t family_id, const char *group_name)
{
	struct genl_family *gf;
	uint32_t group_id = 0;

	MPASS(group_name != NULL);

	GENL_LOCK();
	gf = genl_family(family_id);

	for (u_int i = 0; i < MAX_GROUPS; i++)
		if (groups[i].group_family == gf &&
		    strcmp(groups[i].group_name, group_name) == 0) {
			GENL_UNLOCK();
			return (0);
		}

	/* Microoptimization: index 0 is reserved for the control family */
	for (u_int i = 1; i < MAX_GROUPS; i++) {
		struct genl_group *gg = &groups[i];
		if (gg->group_family == NULL) {
			gf->family_num_groups++;
			gg->group_family = gf;
			gg->group_name = group_name;
			group_id = i + MIN_GROUP_NUM;
			break;
		}
	}
	GENL_UNLOCK();

	return (group_id);
}

void
genl_unregister_group(uint16_t family_id, uint32_t group_id)
{
	struct genl_family *gf;
	struct genl_group *gg;

	MPASS(group_id > MIN_GROUP_NUM &&
	    group_id < MIN_GROUP_NUM + MAX_GROUPS);

	nl_clear_group(group_id);

	group_id -= MIN_GROUP_NUM;

	GENL_LOCK();
	gf = genl_family(family_id);
	gg = &groups[group_id];

	MPASS(gg->group_family == gf);
	MPASS(gf->family_num_groups > 0);

	gf->family_num_groups--;
	gg->group_family = NULL;
	gg->group_name = NULL;
	GENL_UNLOCK();
}
