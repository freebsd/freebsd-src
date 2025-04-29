/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Ng Peng Nam Sean
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/ck.h>
#include <sys/syslog.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_var.h>
#include <netlink/route/route_var.h>

#include <machine/atomic.h>

FEATURE(netlink, "Netlink support");

#define	DEBUG_MOD_NAME	nl_mod
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);


#define NL_MAX_HANDLERS	20
struct nl_proto_handler _nl_handlers[NL_MAX_HANDLERS];
struct nl_proto_handler *nl_handlers = _nl_handlers;

CK_LIST_HEAD(nl_control_head, nl_control);
static struct nl_control_head vnets_head = CK_LIST_HEAD_INITIALIZER();

VNET_DEFINE(struct nl_control, nl_ctl) = {
	.ctl_port_head = CK_LIST_HEAD_INITIALIZER(),
	.ctl_pcb_head = CK_LIST_HEAD_INITIALIZER(),
};

struct mtx nl_global_mtx;
MTX_SYSINIT(nl_global_mtx, &nl_global_mtx, "global netlink lock", MTX_DEF);

#define NL_GLOBAL_LOCK()	mtx_lock(&nl_global_mtx)
#define NL_GLOBAL_UNLOCK()	mtx_unlock(&nl_global_mtx)

int netlink_unloading = 0;

static void
vnet_nl_init(const void *unused __unused)
{
	rm_init(&V_nl_ctl.ctl_lock, "netlink lock");

	NL_GLOBAL_LOCK();
	CK_LIST_INSERT_HEAD(&vnets_head, &V_nl_ctl, ctl_next);
	NL_LOG(LOG_DEBUG2, "VNET %p init done, inserted %p into global list",
	    curvnet, &V_nl_ctl);
	NL_GLOBAL_UNLOCK();
}
VNET_SYSINIT(vnet_nl_init, SI_SUB_INIT_IF, SI_ORDER_FIRST, vnet_nl_init, NULL);

static void
vnet_nl_uninit(const void *unused __unused)
{
	/* Assume at the time all of the processes / sockets are dead */
	NL_GLOBAL_LOCK();
	NL_LOG(LOG_DEBUG2, "Removing %p from global list", &V_nl_ctl);
	CK_LIST_REMOVE(&V_nl_ctl, ctl_next);
	NL_GLOBAL_UNLOCK();

	rm_destroy(&V_nl_ctl.ctl_lock);
}
VNET_SYSUNINIT(vnet_nl_uninit, SI_SUB_INIT_IF, SI_ORDER_FIRST, vnet_nl_uninit,
    NULL);

int
nl_verify_proto(int proto)
{
	if (proto < 0 || proto >= NL_MAX_HANDLERS) {
		return (EINVAL);
	}
	int handler_defined = nl_handlers[proto].cb != NULL;
	return (handler_defined ? 0 : EPROTONOSUPPORT);
}

const char *
nl_get_proto_name(int proto)
{
	return (nl_handlers[proto].proto_name);
}

bool
netlink_register_proto(int proto, const char *proto_name, nl_handler_f handler)
{
	if ((proto < 0) || (proto >= NL_MAX_HANDLERS))
		return (false);
	NL_GLOBAL_LOCK();
	KASSERT((nl_handlers[proto].cb == NULL), ("netlink handler %d is already set", proto));
	nl_handlers[proto].cb = handler;
	nl_handlers[proto].proto_name = proto_name;
	NL_GLOBAL_UNLOCK();
	NL_LOG(LOG_DEBUG2, "Registered netlink %s(%d) handler", proto_name, proto);
	return (true);
}

bool
netlink_unregister_proto(int proto)
{
	if ((proto < 0) || (proto >= NL_MAX_HANDLERS))
		return (false);
	NL_GLOBAL_LOCK();
	KASSERT((nl_handlers[proto].cb != NULL), ("netlink handler %d is not set", proto));
	nl_handlers[proto].cb = NULL;
	nl_handlers[proto].proto_name = NULL;
	NL_GLOBAL_UNLOCK();
	NL_LOG(LOG_DEBUG2, "Unregistered netlink proto %d handler", proto);
	return (true);
}

#if !defined(NETLINK) && defined(NETLINK_MODULE)
/* Non-stub function provider */
const static struct nl_function_wrapper nl_module = {
	.nlmsg_add = _nlmsg_add,
	.nlmsg_refill_buffer = _nlmsg_refill_buffer,
	.nlmsg_flush = _nlmsg_flush,
	.nlmsg_end = _nlmsg_end,
	.nlmsg_abort = _nlmsg_abort,
	.nl_writer_unicast = _nl_writer_unicast,
	.nl_writer_group = _nl_writer_group,
	.nlmsg_end_dump = _nlmsg_end_dump,
	.nl_modify_ifp_generic = _nl_modify_ifp_generic,
	.nl_store_ifp_cookie = _nl_store_ifp_cookie,
	.nl_get_thread_nlp = _nl_get_thread_nlp,
};
#endif

static bool
can_unload(void)
{
	struct nl_control *ctl;
	bool result = true;

	NL_GLOBAL_LOCK();

	CK_LIST_FOREACH(ctl, &vnets_head, ctl_next) {
		NL_LOG(LOG_DEBUG2, "Iterating VNET head %p", ctl);
		if (!CK_LIST_EMPTY(&ctl->ctl_pcb_head)) {
			NL_LOG(LOG_NOTICE, "non-empty socket list in ctl %p", ctl);
			result = false;
			break;
		}
	}

	NL_GLOBAL_UNLOCK();

	return (result);
}

static int
netlink_modevent(module_t mod __unused, int what, void *priv __unused)
{
	int ret = 0;

	switch (what) {
	case MOD_LOAD:
		NL_LOG(LOG_DEBUG2, "Loading");
		nl_osd_register();
#if !defined(NETLINK) && defined(NETLINK_MODULE)
		nl_set_functions(&nl_module);
#endif
		break;

	case MOD_UNLOAD:
		NL_LOG(LOG_DEBUG2, "Unload called");
		if (can_unload()) {
			NL_LOG(LOG_WARNING, "unloading");
			netlink_unloading = 1;
#if !defined(NETLINK) && defined(NETLINK_MODULE)
			nl_set_functions(NULL);
#endif
			nl_osd_unregister();
		} else
			ret = EBUSY;
		break;

	default:
		ret = EOPNOTSUPP;
		break;
	}

	return (ret);
}
static moduledata_t netlink_mod = { "netlink", netlink_modevent, NULL };

DECLARE_MODULE(netlink, netlink_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(netlink, 1);
