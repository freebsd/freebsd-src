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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/ck.h>
#include <sys/epoch.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/route.h>
#include <net/route/route_ctl.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#define	DEBUG_MOD_NAME	nl_route_core
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);

#define	HANDLER_MAX_NUM	(NL_RTM_MAX + 10)
static const struct rtnl_cmd_handler *rtnl_handler[HANDLER_MAX_NUM] = {};

bool
rtnl_register_messages(const struct rtnl_cmd_handler *handlers, int count)
{
	for (int i = 0; i < count; i++) {
		if (handlers[i].cmd >= HANDLER_MAX_NUM)
			return (false);
		MPASS(rtnl_handler[handlers[i].cmd] == NULL);
	}
	for (int i = 0; i < count; i++)
		rtnl_handler[handlers[i].cmd] = &handlers[i];
	return (true);
}

/*
 * Handler called by netlink subsystem when matching netlink message is received
 */
static int
rtnl_handle_message(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	const struct rtnl_cmd_handler *cmd;
	struct epoch_tracker et;
	struct nlpcb *nlp = npt->nlp;
	int error = 0;

	if (__predict_false(hdr->nlmsg_type >= HANDLER_MAX_NUM)) {
		NLMSG_REPORT_ERR_MSG(npt, "unknown message type: %d", hdr->nlmsg_type);
		return (ENOTSUP);
	}

	cmd = rtnl_handler[hdr->nlmsg_type];
	if (__predict_false(cmd == NULL)) {
		NLMSG_REPORT_ERR_MSG(npt, "unknown message type: %d", hdr->nlmsg_type);
		return (ENOTSUP);
	}

	NLP_LOG(LOG_DEBUG2, nlp, "received msg %s(%d) len %d", cmd->name,
	    hdr->nlmsg_type, hdr->nlmsg_len);

	if (cmd->priv != 0 && !nlp_has_priv(nlp, cmd->priv)) {
		NLP_LOG(LOG_DEBUG2, nlp, "priv %d check failed for msg %s", cmd->priv, cmd->name);
		return (EPERM);
	} else if (cmd->priv != 0)
		NLP_LOG(LOG_DEBUG3, nlp, "priv %d check passed for msg %s", cmd->priv, cmd->name);

	if (!nlp_unconstrained_vnet(nlp) && (cmd->flags & RTNL_F_ALLOW_NONVNET_JAIL) == 0) {
		NLP_LOG(LOG_DEBUG2, nlp, "jail check failed for msg %s", cmd->name);
		return (EPERM);
	}

	bool need_epoch = !(cmd->flags & RTNL_F_NOEPOCH);

	if (need_epoch)
		NET_EPOCH_ENTER(et);
	error = cmd->cb(hdr, nlp, npt);
	if (need_epoch)
		NET_EPOCH_EXIT(et);

	NLP_LOG(LOG_DEBUG3, nlp, "message %s -> error %d", cmd->name, error);

	return (error);
}

static struct rtbridge nlbridge = {
	.route_f = rtnl_handle_route_event,
	.ifmsg_f = rtnl_handle_ifnet_event,
};
static struct rtbridge *nlbridge_orig_p;

static void
rtnl_load(void *u __unused)
{
	NL_LOG(LOG_DEBUG2, "rtnl loading");
	nlbridge_orig_p = netlink_callback_p;
	netlink_callback_p = &nlbridge;
	rtnl_neighs_init();
	rtnl_ifaces_init();
	rtnl_nexthops_init();
	rtnl_routes_init();
	netlink_register_proto(NETLINK_ROUTE, "NETLINK_ROUTE", rtnl_handle_message);
}
SYSINIT(rtnl_load, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, rtnl_load, NULL);

static void
rtnl_unload(void *u __unused)
{
	netlink_callback_p = nlbridge_orig_p;
	rtnl_ifaces_destroy();
	rtnl_neighs_destroy();

	/* Wait till all consumers read nlbridge data */
	NET_EPOCH_WAIT();
}
SYSUNINIT(rtnl_unload, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, rtnl_unload, NULL);
