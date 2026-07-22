/*
 * Copyright (c) 2026 Ishan Agrawal
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "sysdecode.h"
#include "support.h"

/*
 * Decodes a buffer as a Netlink message stream.
 *
 * Returns true if the data was successfully decoded as Netlink.
 * Returns false if the data is malformed, allowing the caller
 * to fallback to a standard hex/string dump.
 */

/*
 * These nlmsg_flags bits have a unique meaning regardless of the
 * message direction or operation type.  Bits in the 0x100-0x800 range
 * are operation-specific and are left as raw hex to avoid ambiguity.
 */
static struct name_table netlink_base_flags[] = {
	{ NLM_F_REQUEST,	"NLM_F_REQUEST" },
	{ NLM_F_MULTI,		"NLM_F_MULTI" },
	{ NLM_F_ACK,		"NLM_F_ACK" },
	{ NLM_F_ECHO,		"NLM_F_ECHO" },
	{ NLM_F_DUMP_INTR,	"NLM_F_DUMP_INTR" },
	{ NLM_F_DUMP_FILTERED,	"NLM_F_DUMP_FILTERED" },
	{ 0,			NULL },
};

static void
print_netlink_flags(FILE *fp, uint16_t flags)
{
	int rem;

	if (!print_mask_int(fp, netlink_base_flags, flags, &rem))
		fprintf(fp, "0x%x", rem);
	else if (rem != 0)
		fprintf(fp, "|0x%x", rem);
}

static struct name_table *family_table = NULL;
static size_t num_family = 0;

static void
sysdecode_netlink_pf(FILE *fp, const struct genlmsghdr *genl)
{
	uint8_t cmd = genl->cmd;
	const char *cmd_name = sysdecode_pfnl_cmd(cmd);

	if (cmd_name != NULL)
		fprintf(fp, "cmd=%s", cmd_name);
	else
		fprintf(fp, "cmd=%u", cmd);
}

bool
sysdecode_netlink(FILE *fp, const void *buf, size_t len, int protocol)
{
	const struct nlmsghdr *nl = buf;
	size_t remaining = len;
	bool first = true;

	/* Basic sanity check: Buffer must be at least one header size. */
	if (remaining < sizeof(struct nlmsghdr))
		return (false);

	/* * Protocol Sanity Check:
	 * The first message length must be valid (>= header) and fit
	 * inside the provided buffer snapshot.
	 */
	if (nl->nlmsg_len < sizeof(struct nlmsghdr) || nl->nlmsg_len > remaining)
		return (false);

	if (family_table == NULL) {
		family_table = malloc((num_family + 1) *
		    sizeof(struct name_table));
		family_table[num_family] = (struct name_table){0, NULL};
	}

	fprintf(fp, "netlink{");

	while (remaining >= sizeof(struct nlmsghdr)) {
		if (!first)
			fprintf(fp, ",");

		/* Safety check for current message. */
		if (nl->nlmsg_len < sizeof(struct nlmsghdr) ||
		    nl->nlmsg_len > remaining) {
			fprintf(fp, "<truncated>");
			break;
		}

		fprintf(fp, "flags=");
		print_netlink_flags(fp, nl->nlmsg_flags);

		fprintf(fp, ",seq=%u,pid=%u", nl->nlmsg_seq, nl->nlmsg_pid);

		fprintf(fp, ",len=%u,type=", nl->nlmsg_len);

		/* Decode Standard Message Types. */
		switch (nl->nlmsg_type) {
		case NLMSG_NOOP:
			fprintf(fp, "NLMSG_NOOP");
			break;
		case NLMSG_ERROR:
			fprintf(fp, "NLMSG_ERROR");
			break;
		case NLMSG_DONE:
			fprintf(fp, "NLMSG_DONE");
			break;
		case NLMSG_OVERRUN:
			fprintf(fp, "NLMSG_OVERRUN");
			break;
		case GENL_ID_CTRL:
			if (protocol != NETLINK_GENERIC)
				break;

			fprintf(fp, "GENL_ID_CTRL");

			const struct genlmsghdr *genl =
			    (const struct genlmsghdr *)(const void *)
			    ((const char *)nl + sizeof(struct nlmsghdr));

			uint16_t family_id = 0;
			const char *family_name = NULL;

			fprintf(fp,
			    ",genl={cmd=%u,"
			    "ver=%u,reserve=%u",
			    genl->cmd,
			    genl->version, genl->reserved);

			size_t cur_len = (sizeof(struct nlmsghdr)
			    + sizeof(struct genlmsghdr));
			size_t nla_len = nl->nlmsg_len - cur_len;

			const struct nlattr *nla;
			const struct nlattr *nla_head = (const struct nlattr *)
			    (const void *)((const char *)nl + cur_len);

			NLA_FOREACH_CONST(nla, nla_head, nla_len) {
				switch (nla->nla_type) {
				case CTRL_ATTR_FAMILY_ID:
					memcpy(&family_id, NLA_DATA_CONST(nla),
					    sizeof(family_id));
					fprintf(fp, ",family_id=%u", family_id);
					break;
				case CTRL_ATTR_FAMILY_NAME:
					family_name =
					    ((const char *)NLA_DATA_CONST(nla));
					fprintf(fp, ",family_name=%s",
					    family_name);
					break;
				default:
					break;
				}
			}

			if (family_name && family_id &&
			    !lookup_value(family_table, family_id)) {
				num_family++;

				family_table = realloc(family_table,
				    (num_family + 1) *
				    sizeof(struct name_table));
				family_table[num_family - 1].val =
				    family_id;
				family_table[num_family - 1].str =
				    strdup(family_name);

				family_table[num_family] =
				    (struct name_table){0, NULL};
			}

			fprintf(fp, "}");
			break;
		default:
			fprintf(fp, "%u", nl->nlmsg_type);
			break;
		}

		const char *family = lookup_value(family_table, nl->nlmsg_type);

		if (family != NULL && protocol == NETLINK_GENERIC) {
			fprintf(fp, ",%s={", family);

			if (strcmp(family, "pfctl") == 0) {
				const struct genlmsghdr *genl =
				    (const struct genlmsghdr *)(const void *)
				    ((const char *)nl +
				    sizeof(struct nlmsghdr));

				sysdecode_netlink_pf(fp, genl);
			}

			fprintf(fp, "}");
		}

		/* Handle Alignment (Netlink messages are 4-byte aligned). */
		size_t aligned_len = NLMSG_ALIGN(nl->nlmsg_len);
		if (aligned_len > remaining)
			remaining = 0;
		else
			remaining -= aligned_len;

		nl = (const struct nlmsghdr *)(const void *)((const char *)nl + aligned_len);
		first = false;
	}

	fprintf(fp, "}");
	return (true);
}
