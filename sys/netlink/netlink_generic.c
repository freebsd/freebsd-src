/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/ck.h>
#include <sys/epoch.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/sx.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_generic.h>

#define	DEBUG_MOD_NAME	nl_generic
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_DEBUG3);

#define	MAX_FAMILIES	20
#define	MAX_GROUPS	64

#define	MIN_GROUP_NUM	48

static struct sx sx_lock;

#define	GENL_LOCK_INIT()	sx_init(&sx_lock, "genetlink lock")
#define	GENL_LOCK_DESTROY()	sx_destroy(&sx_lock)
#define	GENL_LOCK()		sx_xlock(&sx_lock)
#define	GENL_UNLOCK()		sx_xunlock(&sx_lock)

struct genl_family {
	const char	*family_name;
	uint16_t	family_hdrsize;
	uint16_t	family_id;
	uint16_t	family_version;
	uint16_t	family_attr_max;
	uint16_t	family_cmd_size;
	uint16_t	family_num_groups;
	struct genl_cmd	*family_cmds;
};

static struct genl_family	families[MAX_FAMILIES];


struct genl_group {
	struct genl_family	*group_family;
	const char		*group_name;
};
static struct genl_group	groups[MAX_GROUPS];


static int dump_family(struct nlmsghdr *hdr, struct genlmsghdr *ghdr,
    const struct genl_family *gf, struct nl_writer *nw);
static void nlctrl_notify(const struct genl_family *gf, int action);

static struct genl_family *
find_family(const char *family_name)
{
	for (int i = 0; i < MAX_FAMILIES; i++) {
		struct genl_family *gf = &families[i];
		if (gf->family_name != NULL && !strcmp(gf->family_name, family_name))
			return (gf);
	}

	return (NULL);
}

uint32_t
genl_register_family(const char *family_name, size_t hdrsize, int family_version,
    int max_attr_idx)
{
	uint32_t family_id = 0;

	MPASS(family_name != NULL);
	if (find_family(family_name) != NULL)
		return (0);

	GENL_LOCK();
	for (int i = 0; i < MAX_FAMILIES; i++) {
		struct genl_family *gf = &families[i];
		if (gf->family_name == NULL) {
			gf->family_name = family_name;
			gf->family_version = family_version;
			gf->family_hdrsize = hdrsize;
			gf->family_attr_max = max_attr_idx;
			gf->family_id = i + GENL_MIN_ID;
			NL_LOG(LOG_DEBUG2, "Registered family %s id %d",
			    gf->family_name, gf->family_id);
			family_id = gf->family_id;
			nlctrl_notify(gf, CTRL_CMD_NEWFAMILY);
			break;
		}
	}
	GENL_UNLOCK();

	return (family_id);
}

static void
free_family(struct genl_family *gf)
{
	if (gf->family_cmds != NULL)
		free(gf->family_cmds, M_NETLINK);
}

/*
 * unregister groups of a given family
 */
static void
unregister_groups(const struct genl_family *gf)
{

	for (int i = 0; i < MAX_GROUPS; i++) {
		struct genl_group *gg = &groups[i];
		if (gg->group_family == gf && gg->group_name != NULL) {
			gg->group_family = NULL;
			gg->group_name = NULL;
		}
	}
}

/*
 * Can sleep, I guess
 */
bool
genl_unregister_family(const char *family_name)
{
	bool found = false;

	GENL_LOCK();
	struct genl_family *gf = find_family(family_name);

	if (gf != NULL) {
		nlctrl_notify(gf, CTRL_CMD_DELFAMILY);
		found = true;
		unregister_groups(gf);
		/* TODO: zero pointer first */
		free_family(gf);
		bzero(gf, sizeof(*gf));
	}
	GENL_UNLOCK();

	return (found);
}

bool
genl_register_cmds(const char *family_name, const struct genl_cmd *cmds, int count)
{
	GENL_LOCK();
	struct genl_family *gf = find_family(family_name);
	if (gf == NULL) {
		GENL_UNLOCK();
		return (false);
	}

	int cmd_size = gf->family_cmd_size;

	for (int i = 0; i < count; i++) {
		MPASS(cmds[i].cmd_cb != NULL);
		if (cmds[i].cmd_num >= cmd_size)
			cmd_size = cmds[i].cmd_num + 1;
	}

	if (cmd_size > gf->family_cmd_size) {
		/* need to realloc */
		size_t sz = cmd_size * sizeof(struct genl_cmd);
		void *data = malloc(sz, M_NETLINK, M_WAITOK | M_ZERO);

		memcpy(data, gf->family_cmds, gf->family_cmd_size * sizeof(struct genl_cmd));
		void *old_data = gf->family_cmds;
		gf->family_cmds = data;
		gf->family_cmd_size = cmd_size;
		free(old_data, M_NETLINK);
	}

	for (int i = 0; i < count; i++) {
		const struct genl_cmd *cmd = &cmds[i];
		MPASS(gf->family_cmds[cmd->cmd_num].cmd_cb == NULL);
		gf->family_cmds[cmd->cmd_num] = cmds[i];
		NL_LOG(LOG_DEBUG2, "Adding cmd %s(%d) to family %s",
		    cmd->cmd_name, cmd->cmd_num, gf->family_name);
	}
	GENL_UNLOCK();
	return (true);
}

static struct genl_group *
find_group(const struct genl_family *gf, const char *group_name)
{
	for (int i = 0; i < MAX_GROUPS; i++) {
		struct genl_group *gg = &groups[i];
		if (gg->group_family == gf && !strcmp(gg->group_name, group_name))
			return (gg);
	}
	return (NULL);
}

uint32_t
genl_register_group(const char *family_name, const char *group_name)
{
	uint32_t group_id = 0;

	MPASS(family_name != NULL);
	MPASS(group_name != NULL);

	GENL_LOCK();
	struct genl_family *gf = find_family(family_name);

	if (gf == NULL || find_group(gf, group_name) != NULL) {
		GENL_UNLOCK();
		return (0);
	}

	for (int i = 0; i < MAX_GROUPS; i++) {
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

/*
 * Handler called by netlink subsystem when matching netlink message is received
 */
static int
genl_handle_message(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct nlpcb *nlp = npt->nlp;
	int error = 0;

	int family_id = (int)hdr->nlmsg_type - GENL_MIN_ID;

	if (__predict_false(family_id < 0 || family_id >= MAX_FAMILIES)) {
		NLP_LOG(LOG_DEBUG, nlp, "invalid message type: %d", hdr->nlmsg_type);
		return (ENOTSUP);
	}

	if (__predict_false(hdr->nlmsg_len < sizeof(hdr) + GENL_HDRLEN)) {
		NLP_LOG(LOG_DEBUG, nlp, "invalid message size: %d", hdr->nlmsg_len);
		return (EINVAL);
	}

	struct genl_family *gf = &families[family_id];

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
        nlattr_add_u16(nw, CTRL_ATTR_FAMILY_ID, gf->family_id);
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
		for (int i = 0, cnt = 0; i < MAX_GROUPS; i++) {
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


/* Declare ourself as a user */
#define	CTRL_FAMILY_NAME	"nlctrl"

static uint32_t ctrl_family_id;
static uint32_t ctrl_group_id;

struct nl_parsed_family {
	uint32_t	family_id;
	char		*family_name;
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

static bool
match_family(const struct genl_family *gf, const struct nl_parsed_family *attrs)
{
	if (gf->family_name == NULL)
		return (false);
	if (attrs->family_id != 0 && attrs->family_id != gf->family_id)
		return (false);
	if (attrs->family_name != NULL && strcmp(attrs->family_name, gf->family_name))
		return (false);
	return (true);
}

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
		/* Resolve request */
		for (int i = 0; i < MAX_FAMILIES; i++) {
			struct genl_family *gf = &families[i];
			if (match_family(gf, &attrs)) {
				error = dump_family(hdr, &ghdr, gf, npt->nw);
				return (error);
			}
		}
		return (ENOENT);
	}

	hdr->nlmsg_flags = hdr->nlmsg_flags | NLM_F_MULTI;
	for (int i = 0; i < MAX_FAMILIES; i++) {
		struct genl_family *gf = &families[i];
		if (match_family(gf, &attrs)) {
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
nlctrl_notify(const struct genl_family *gf, int cmd)
{
	struct nlmsghdr hdr = {.nlmsg_type = NETLINK_GENERIC };
	struct genlmsghdr ghdr = { .cmd = cmd };
	struct nl_writer nw = {};

	if (nlmsg_get_group_writer(&nw, NLMSG_SMALL, NETLINK_GENERIC, ctrl_group_id)) {
		dump_family(&hdr, &ghdr, gf, &nw);
		nlmsg_flush(&nw);
		return;
	}
	NL_LOG(LOG_DEBUG, "error allocating group writer");
}

static const struct genl_cmd nlctrl_cmds[] = {
	{
		.cmd_num = CTRL_CMD_GETFAMILY,
		.cmd_name = "GETFAMILY",
		.cmd_cb = nlctrl_handle_getfamily,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
	},
};

static void
genl_nlctrl_init(void)
{
	ctrl_family_id = genl_register_family(CTRL_FAMILY_NAME, 0, 2, CTRL_ATTR_MAX);
	genl_register_cmds(CTRL_FAMILY_NAME, nlctrl_cmds, NL_ARRAY_LEN(nlctrl_cmds));
	ctrl_group_id = genl_register_group(CTRL_FAMILY_NAME, "notify");
}

static void
genl_nlctrl_destroy(void)
{
	genl_unregister_family(CTRL_FAMILY_NAME);
}

static const struct nlhdr_parser *all_parsers[] = { &genl_parser };

static void
genl_load(void *u __unused)
{
	GENL_LOCK_INIT();
	NL_VERIFY_PARSERS(all_parsers);
	netlink_register_proto(NETLINK_GENERIC, "NETLINK_GENERIC", genl_handle_message);
	genl_nlctrl_init();
}
SYSINIT(genl_load, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, genl_load, NULL);

static void
genl_unload(void *u __unused)
{
	genl_nlctrl_destroy();
	GENL_LOCK_DESTROY();
	NET_EPOCH_WAIT();
}
SYSUNINIT(genl_unload, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, genl_unload, NULL);
