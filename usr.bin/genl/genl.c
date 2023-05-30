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

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <stdio.h>

#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_generic.h>

struct genl_ctrl_op {
	uint32_t id;
	uint32_t flags;
};

struct genl_ctrl_ops {
	uint32_t num_ops;
	struct genl_ctrl_op **ops;
};

#define _OUT(_field)	offsetof(struct genl_ctrl_op, _field)
static struct snl_attr_parser _nla_p_getops[] = {
	{ .type = CTRL_ATTR_OP_ID, .off = _OUT(id), .cb = snl_attr_get_uint32},
	{ .type = CTRL_ATTR_OP_FLAGS, .off = _OUT(flags), .cb = snl_attr_get_uint32 },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER_EXT(genl_ctrl_op_parser,
		sizeof(struct genl_ctrl_op),
		_nla_p_getops, NULL);

struct genl_family {
	uint16_t id;
	char *name;
	uint32_t version;
	uint32_t hdrsize;
	uint32_t max_attr;
	struct snl_genl_ctrl_mcast_groups mcast_groups;
	struct genl_ctrl_ops ops;
};

#define	_OUT(_field)	offsetof(struct genl_family, _field)
static struct snl_attr_parser _nla_p_getfamily[] = {
	{ .type = CTRL_ATTR_FAMILY_ID , .off = _OUT(id), .cb = snl_attr_get_uint16 },
	{ .type = CTRL_ATTR_FAMILY_NAME, .off = _OUT(name), .cb = snl_attr_get_string },
	{ .type = CTRL_ATTR_VERSION, .off = _OUT(version), .cb = snl_attr_get_uint32 },
	{ .type = CTRL_ATTR_VERSION, .off = _OUT(hdrsize), .cb = snl_attr_get_uint32 },
	{ .type = CTRL_ATTR_MAXATTR, .off = _OUT(max_attr), .cb = snl_attr_get_uint32 },
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
		.arg = &_genl_ctrl_mc_parser,
	},
};
#undef _OUT
SNL_DECLARE_GENL_PARSER(genl_family_parser, _nla_p_getfamily);

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
		printf("\t  - ID: %#02x, Capabilities: %#02x (",
		    ops->ops[i]->id,
		    ops->ops[i]->flags);
		for (size_t j = 0; j < nitems(op_caps); j++)
			if ((ops->ops[i]->flags & op_caps[j].flag) == op_caps[j].flag)
				printf("%s; ", op_caps[j].str);
		printf("\b\b)\n");
	}
}

static void
dump_mcast_groups( struct snl_genl_ctrl_mcast_groups *mcast_groups)
{
	if (mcast_groups->num_groups == 0)
		return;
	printf("\tmulticast groups: \n");
	for (uint32_t i = 0; i < mcast_groups->num_groups; i++)
		printf("\t  - ID: %#02x, Name: %s\n",
		    mcast_groups->groups[i]->mcast_grp_id,
		    mcast_groups->groups[i]->mcast_grp_name);
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

int
main(int argc, char **argv __unused)
{
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct snl_errmsg_data e = {};
	uint32_t seq_id;

	if (argc > 1)
		errx(EXIT_FAILURE, "usage: genl does not accept any argument");
	if (modfind("netlink") == -1)
		err(EXIT_FAILURE, "require netlink module to be loaded");

	if (!snl_init(&ss, NETLINK_GENERIC))
		err(EXIT_FAILURE, "snl_init()");

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, GENL_ID_CTRL, CTRL_CMD_GETFAMILY);
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
