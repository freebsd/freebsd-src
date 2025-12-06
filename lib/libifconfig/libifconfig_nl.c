/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025, Muhammad Saheed <saheed@FreeBSD.org>
 */

#include <netlink/netlink.h>
#include <netlink/netlink_snl.h>
#include <netlink/route/common.h>
#include <netlink/route/interface.h>

#include "libifconfig.h"
#include "libifconfig_internal.h"

static int ifconfig_modify_flags(ifconfig_handle_t *h, const char *ifname,
    int ifi_flags, int ifi_change);

static int
ifconfig_modify_flags(ifconfig_handle_t *h, const char *ifname, int ifi_flags,
    int ifi_change)
{
	int ret = 0;
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct ifinfomsg *ifi;
	struct snl_errmsg_data e = { 0 };

	if (!snl_init(&ss, NETLINK_ROUTE)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_NEWLINK);
	ifi = snl_reserve_msg_object(&nw, struct ifinfomsg);
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, ifname);

	ifi->ifi_flags = ifi_flags;
	ifi->ifi_change = ifi_change;

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL) {
		ifconfig_error(h, NETLINK, ENOMEM);
		ret = -1;
		goto out;
	}

	if (!snl_send_message(&ss, hdr)) {
		ifconfig_error(h, NETLINK, EIO);
		ret = -1;
		goto out;
	}

	if (!snl_read_reply_code(&ss, hdr->nlmsg_seq, &e)) {
		ifconfig_error(h, NETLINK, e.error);
		ret = -1;
		goto out;
	}

out:
	snl_free(&ss);
	return (ret);
}

int
ifconfig_set_up(ifconfig_handle_t *h, const char *ifname, bool up)
{
	int flag = up ? IFF_UP : ~IFF_UP;

	return (ifconfig_modify_flags(h, ifname, flag, IFF_UP));
}
