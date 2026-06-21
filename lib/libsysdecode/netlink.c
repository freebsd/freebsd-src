/*
 * Copyright (c) 2026 Ishan Agrawal
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <netlink/netlink.h>

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include "support.h"
#include "sysdecode.h"

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

/*
 * Decodes a buffer as a Netlink message stream.
 *
 * Returns true if the data was successfully decoded as Netlink.
 * Returns false if the data is malformed, allowing the caller
 * to fallback to a standard hex/string dump.
 */
bool
sysdecode_netlink(FILE *fp, const void *buf, size_t len)
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

		fprintf(fp, "len=%u,type=", nl->nlmsg_len);

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
		default:
			fprintf(fp, "%u", nl->nlmsg_type);
			break;
		}

		fprintf(fp, ",flags=");
		print_netlink_flags(fp, nl->nlmsg_flags);

		fprintf(fp, ",seq=%u,pid=%u", nl->nlmsg_seq, nl->nlmsg_pid);

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
