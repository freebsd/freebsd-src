/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2023 Baptiste Daroussin <bapt@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions~
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <stdio.h>
#include <poll.h>

#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_generic.h>
#include <netlink/netlink_sysevent.h>

#include "genl.h"

static int monitor_mcast(int argc, char **argv);
static int list_families(int argc, char **argv);

static struct commands {
	const char *name;
	const char *usage;
	int (*cmd)(int argc, char **argv);
} cmds[] = {
	{ "monitor", "monitor <family> [multicast group]", monitor_mcast },
	{ "list", "list", list_families },
};

static monitor_parser_t parser_nlctrl_notify;
static monitor_parser_t parser_nlsysevent;

static struct mcast_parsers {
	const char *family;
	monitor_parser_t *parser;
} mcast_parsers [] = {
	{ "nlctrl", parser_nlctrl_notify },
	{ "nlsysevent", parser_nlsysevent },
	{ "rpc", parser_rpc },
};

struct nlevent {
	const char *name;
	const char *subsystem;
	const char *type;
	const char *data;
};
#define _OUT(_field) offsetof(struct nlevent, _field)
static struct snl_attr_parser ap_nlevent_get[] = {
	{ .type = NLSE_ATTR_SYSTEM, .off = _OUT(name), .cb = snl_attr_get_string },
	{ .type = NLSE_ATTR_SUBSYSTEM, .off = _OUT(subsystem), .cb = snl_attr_get_string },
	{ .type = NLSE_ATTR_TYPE, .off = _OUT(type), .cb = snl_attr_get_string },
	{ .type = NLSE_ATTR_DATA, .off = _OUT(data), .cb = snl_attr_get_string },
};
#undef _OUT
SNL_DECLARE_GENL_PARSER(nlevent_get_parser, ap_nlevent_get);

/*
 * We run our own parser(s) for CTRL_CMD_GETFAMILY instead of interfaces
 * provided by <netlink_snl_generic.h>.  One reason is that we want the parsed
 * string attributes to point into long living memory, instead of inside of
 * just received message, hence we use snl_attr_get_stringn() instead of the
 * snl_attr_get_string().  The shared library uses the latter to avoid creating
 * ambiguous memory leaks.  Second reason is that genl(1) usage of the data is
 * more extended than typical application that cares only of its own family and
 * usually one group.  We are going to cycle around all available.
 */
struct genl_ctrl_op {
	uint32_t id;
	uint32_t flags;
};
struct genl_ctrl_ops {
	uint32_t num_ops;
	struct genl_ctrl_op **ops;
};
static struct snl_attr_parser nla_p_getops[] = {
#define _OUT(_field)	offsetof(struct genl_ctrl_op, _field)
	{
		.type = CTRL_ATTR_OP_ID,
		.off = _OUT(id),
		.cb = snl_attr_get_uint32
	},
	{
		.type = CTRL_ATTR_OP_FLAGS,
		.off = _OUT(flags),
		.cb = snl_attr_get_uint32
	},
#undef _OUT
};
SNL_DECLARE_ATTR_PARSER_EXT(genl_ctrl_op_parser, sizeof(struct genl_ctrl_op),
    nla_p_getops, NULL);

struct genl_mcast_group {
	uint32_t id;
	const char *name;
};
struct genl_mcast_groups {
	uint32_t num_groups;
	struct genl_mcast_group **groups;
};
static struct snl_attr_parser nla_p_getmc[] = {
#define	_OUT(_field)	offsetof(struct genl_mcast_group, _field)
	{
		.type = CTRL_ATTR_MCAST_GRP_NAME,
		.off = _OUT(name),
		.cb = snl_attr_get_stringn,
	},
	{
		.type = CTRL_ATTR_MCAST_GRP_ID,
		.off = _OUT(id),
		.cb = snl_attr_get_uint32,
	},
#undef _OUT
};
SNL_DECLARE_ATTR_PARSER_EXT(genl_mc_parser, sizeof(struct genl_mcast_group),
    nla_p_getmc, NULL);

struct genl_family {
	uint16_t id;
	const char *name;
	uint32_t version;
	uint32_t hdrsize;
	uint32_t max_attr;
	struct genl_mcast_groups mcast_groups;
	struct genl_ctrl_ops ops;
};

static struct snl_attr_parser nla_p_getfamily[] = {
#define	_OUT(_field)	offsetof(struct genl_family, _field)
	{
		.type = CTRL_ATTR_FAMILY_ID,
		.off = _OUT(id),
		.cb = snl_attr_get_uint16,
	},
	{
		.type = CTRL_ATTR_FAMILY_NAME,
		.off = _OUT(name),
		.cb = snl_attr_get_stringn,
	},
	{
		.type = CTRL_ATTR_VERSION,
		.off = _OUT(version),
		.cb = snl_attr_get_uint32,
	},
	{
		.type = CTRL_ATTR_VERSION,
		.off = _OUT(hdrsize),
		.cb = snl_attr_get_uint32,
	},
	{
		.type = CTRL_ATTR_MAXATTR,
		.off = _OUT(max_attr),
		.cb = snl_attr_get_uint32,
	},
	{
		.type = CTRL_ATTR_OPS,
		.off = _OUT(ops),
		.cb = snl_attr_get_parray,
		.arg = &genl_ctrl_op_parser,
	},
	{
		.type = CTRL_ATTR_MCAST_GROUPS,
		.off = _OUT(mcast_groups),
		.cb = snl_attr_get_parray,
		.arg = &genl_mc_parser,
	},
#undef _OUT
};
SNL_DECLARE_GENL_PARSER(genl_family_parser, nla_p_getfamily);

static struct op_capability {
	uint32_t flag;
	const char *str;
} op_caps[] = {
	{ GENL_ADMIN_PERM, "requires admin permission" },
	{ GENL_CMD_CAP_DO, "can modify" },
	{ GENL_CMD_CAP_DUMP, "can get/dump" },
	{ GENL_CMD_CAP_HASPOL, "has policy" },
};

static void
dump_operations(struct genl_ctrl_ops *ops)
{
	if (ops->num_ops == 0)
		return;
	printf("\tsupported operations: \n");
	for (uint32_t i = 0; i < ops->num_ops; i++) {
		bool p = true;

		printf("\t  - ID: %#02x, Capabilities: %#02x",
		    ops->ops[i]->id,
		    ops->ops[i]->flags);
		for (size_t j = 0; j < nitems(op_caps); j++)
			if ((ops->ops[i]->flags & op_caps[j].flag) ==
			    op_caps[j].flag) {
				printf("%s%s", p ? " (" : "; ",
				    op_caps[j].str);
				p = false;
			}
		printf("%s\n", p ? "" : ")");
	}
}

static void
dump_mcast_groups(struct genl_mcast_groups *mcast_groups)
{
	if (mcast_groups->num_groups == 0)
		return;
	printf("\tmulticast groups: \n");
	for (uint32_t i = 0; i < mcast_groups->num_groups; i++)
		printf("\t  - ID: %#02x, Name: %s\n",
		    mcast_groups->groups[i]->id,
		    mcast_groups->groups[i]->name);
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s\n", getprogname());
	for (size_t i = 0; i < nitems(cmds); i++)
		fprintf(stderr, "       %s %s\n", getprogname(), cmds[i].usage);
}

static void
dump_family(struct genl_family *family)
{
	printf("Name: %s\n\tID: %#02hx, Version: %#02x, "
	    "header size: %d, max attributes: %d\n",
	    family->name, family->id, family->version,
	    family->hdrsize, family->max_attr);
	dump_operations(&family->ops);
	dump_mcast_groups(&family->mcast_groups);
}

static void
parser_nlctrl_notify(struct snl_state *ss, struct nlmsghdr *hdr)
{
	struct genl_family family = {};

	if (snl_parse_nlmsg(ss, hdr, &genl_family_parser,
				&family))
		dump_family(&family);
}

static void
parser_nlsysevent(struct snl_state *ss, struct nlmsghdr *hdr)
{
	struct nlevent ne = {};
	if (snl_parse_nlmsg(ss, hdr, &nlevent_get_parser, &ne)) {
		printf("system=%s subsystem=%s type=%s", ne.name, ne.subsystem, ne.type);
		if (ne.data) {
			printf(" %s", ne.data);
			if (ne.data[strlen(ne.data) -1] != '\n')
				printf("\n");
		}
	}
}

static void
parser_fallback(struct snl_state *ss __unused, struct nlmsghdr *hdr)
{
	printf("Unknown message: type 0x%x, length %u\n",
	    hdr->nlmsg_type, hdr->nlmsg_len);
}

/* Populated by monitor_mcast() and may be used by protocol parser callbacks. */
static struct genl_family attrs;

const char *
group_name(uint32_t id)
{
	for (u_int i = 0; i < attrs.mcast_groups.num_groups; i++)
		if (attrs.mcast_groups.groups[i]->id == id)
			return (attrs.mcast_groups.groups[i]->name);
	return ("???");
}

static int
monitor_mcast(int argc, char **argv)
{
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct pollfd pfd;
	bool found = false;
	bool all = false;
	monitor_parser_t *parser;

	if (argc < 1 || argc > 2) {
		usage();
		return (EXIT_FAILURE);
	}

	if (!snl_init(&ss, NETLINK_GENERIC))
		err(EXIT_FAILURE, "snl_init()");
	snl_init_writer(&ss, &nw);
	snl_create_genl_msg_request(&nw, GENL_ID_CTRL, CTRL_CMD_GETFAMILY);
	snl_add_msg_attr_string(&nw, CTRL_ATTR_FAMILY_NAME, argv[0]);
	if ((hdr = snl_finalize_msg(&nw)) == NULL)
		err(EXIT_FAILURE, "snl_finalize_msg");
	if (!snl_send_message(&ss, hdr))
		err(EXIT_FAILURE, "snl_send_message");
	hdr = snl_read_reply(&ss, hdr->nlmsg_seq);
	if (hdr == NULL)
		err(EXIT_FAILURE, "snl_read_reply");
	if (hdr->nlmsg_type == NLMSG_ERROR)
		err(EXIT_FAILURE, "netlink(4) returned error");
	memset(&attrs, 0, sizeof(attrs));
	if (!snl_parse_nlmsg(&ss, hdr, &genl_family_parser, &attrs))
		err(EXIT_FAILURE, "snl_parse_nlmsg CTRL_CMD_GETFAMILY");

	if (argc == 1)
		all = true;
	for (u_int i = 0; i < attrs.mcast_groups.num_groups; i++) {
		if (all ||
		    strcmp(attrs.mcast_groups.groups[i]->name, argv[1]) == 0) {
			found = true;
			if (setsockopt(ss.fd, SOL_NETLINK,
			    NETLINK_ADD_MEMBERSHIP,
			    &attrs.mcast_groups.groups[i]->id,
			    sizeof(attrs.mcast_groups.groups[i]->id))
			    == -1)
				err(EXIT_FAILURE, "Cannot subscribe to command "
				    "notify");
			if (!all)
				break;
		}
	}
	if (!found)
		errx(EXIT_FAILURE, "No such multicast group '%s'"
		    " in family '%s'", argv[1], argv[0]);
	parser = parser_fallback;
	for (size_t i= 0; i < nitems(mcast_parsers); i++) {
		if (strcmp(mcast_parsers[i].family, argv[0]) == 0) {
			parser = mcast_parsers[i].parser;
			break;
		}
	}
	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = ss.fd;
	pfd.events = POLLIN | POLLERR;
	while (true) {
		pfd.revents = 0;
		if (poll(&pfd, 1, -1) == -1) {
			if (errno == EINTR)
				continue;
			err(EXIT_FAILURE, "poll()");
		}
		hdr = snl_read_message(&ss);
		if (hdr != NULL && hdr->nlmsg_type != NLMSG_ERROR)
			parser(&ss, hdr);

	}

	return (EXIT_SUCCESS);
}

int
list_families(int argc, char **argv __unused)
{
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct snl_errmsg_data e = {};
	uint32_t seq_id;

	if (argc != 0) {
		usage();
		return (EXIT_FAILURE);
	}
	if (!snl_init(&ss, NETLINK_GENERIC))
		err(EXIT_FAILURE, "snl_init()");

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, GENL_ID_CTRL,
	    CTRL_CMD_GETFAMILY);
	if ((hdr = snl_finalize_msg(&nw)) == NULL)
		err(EXIT_FAILURE, "snl_finalize_msg");
	seq_id = hdr->nlmsg_seq;
	if (!snl_send_message(&ss, hdr))
		err(EXIT_FAILURE, "snl_send_message");

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		if (e.error != 0) {
			err(EXIT_FAILURE, "Error reading generic netlink");
		}
		struct genl_family family = {};
		if (snl_parse_nlmsg(&ss, hdr, &genl_family_parser, &family))
			dump_family(&family);
	}

	return (EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	if (modfind("netlink") == -1)
		err(EXIT_FAILURE, "require netlink module to be loaded");

	if (argc == 1)
		return (list_families(0, NULL));

	for (size_t i = 0; i < nitems(cmds); i++) {
		if (strcmp(argv[1], cmds[i].name) == 0)
			return (cmds[i].cmd(argc - 2, argv + 2));
	}
	usage();

	return (EXIT_FAILURE);
}
