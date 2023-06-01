/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Baptiste Daroussin <bapt@FreeBSD.org>
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
#include <sys/types.h>
#include <sys/devctl.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <net/vnet.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_generic.h>

#include "netlink_sysevent.h"

#define DEBUG_MOD_NAME  nl_sysevent
#define DEBUG_MAX_LEVEL LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);

MALLOC_DEFINE(M_NLSE, "nlsysevent", "Memory used for Netlink sysevent");
#define	NLSE_FAMILY_NAME	"nlsysevent"
static uint32_t ctrl_family_id;

#define MAX_SYSEVENTS	64
static struct sysevent {
	char *name;
	uint32_t id;
} sysevents[MAX_SYSEVENTS] = {};

static void
sysevent_write(struct sysevent *se, const char *subsystem, const char *type,
    const char *data)
{
	struct nl_writer nw = {};

	if (!nlmsg_get_group_writer(&nw, NLMSG_LARGE, NETLINK_GENERIC, se->id)) {
		NL_LOG(LOG_DEBUG, "error allocating group writer");
		return;
	}
	struct nlmsghdr hdr = { .nlmsg_type = ctrl_family_id };
	if (!nlmsg_reply(&nw, &hdr, sizeof(struct genlmsghdr))) {
		return;
	}

	struct genlmsghdr *ghdr = nlmsg_reserve_object(&nw, struct genlmsghdr);
	if (ghdr == NULL) {
		NL_LOG(LOG_DEBUG, "unable to allocate memory");
		return;
	}
	ghdr->version = 0;
	ghdr->cmd = 0;
	ghdr->reserved = 0;
	nlattr_add_string(&nw, NLSE_ATTR_SYSTEM, se->name);
	nlattr_add_string(&nw, NLSE_ATTR_SUBSYSTEM, subsystem);
	nlattr_add_string(&nw, NLSE_ATTR_TYPE, type);
	if (data != NULL)
		nlattr_add_string(&nw, NLSE_ATTR_DATA, data);
	nlmsg_end(&nw);
	nlmsg_flush(&nw);
}

static void
sysevent_send(const char *system, const char *subsystem, const char *type,
    const char *data)
{
	struct sysevent *se = NULL;

	for (size_t i = 0; i < MAX_SYSEVENTS; i++) {
		if (sysevents[i].name == NULL) {
			sysevents[i].name = strdup(system, M_NLSE);
			sysevents[i].id = genl_register_group(NLSE_FAMILY_NAME,
			    system);
			se = &sysevents[i];
			break;
		}
		if (strcmp(sysevents[i].name, system) == 0) {
			se = &sysevents[i];
			break;
		}
	}
	if (se == NULL) {
		NL_LOG(LOG_WARNING, "impossible to add the event %s, "
		    "too many events\n", system);
		return;
	}

	CURVNET_SET(vnet0);
	sysevent_write(se, subsystem, type, data);
	CURVNET_RESTORE();
}

static void
nlsysevent_load(void)
{
	devctl_set_notify_hook(sysevent_send);
	ctrl_family_id = genl_register_family(NLSE_FAMILY_NAME, 0, 2, NLSE_ATTR_MAX);
	for (size_t i = 0; i < nitems(devctl_systems); i++) {
		if (i >= MAX_SYSEVENTS) {
			NL_LOG(LOG_WARNING, "impossible to add the event %s, too many events\n", devctl_systems[i]);
			continue;
		}
		sysevents[i].name = strdup(devctl_systems[i], M_NLSE);
		sysevents[i].id = genl_register_group(NLSE_FAMILY_NAME, devctl_systems[i]);
	}
}

static void
nlsysevent_unload(void)
{
	devctl_unset_notify_hook();
	genl_unregister_family(NLSE_FAMILY_NAME);
	for (size_t i = 0; i < MAX_SYSEVENTS; i++) {
		if (sysevents[i].name == NULL)
			break;
		free(sysevents[i].name, M_NLSE);
	}
}

static int
nlsysevent_loader(module_t mod __unused, int what, void *priv __unused)
{
	int err = 0;

	switch (what) {
	case MOD_LOAD:
		nlsysevent_load();
		break;
	case MOD_UNLOAD:
		nlsysevent_unload();
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}
	return (err);
}
static moduledata_t nlsysevent_mod = { "nlsysevent", nlsysevent_loader, NULL};

DECLARE_MODULE(nlsysevent, nlsysevent_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_DEPEND(nlsysevent, netlink, 1, 1, 1);
MODULE_VERSION(nlsysevent, 1);
