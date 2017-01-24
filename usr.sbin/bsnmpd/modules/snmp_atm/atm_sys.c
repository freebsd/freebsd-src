/*
 * Copyright (c) 2001-2002
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 *	All rights reserved.
 * Copyright (c) 2003-2004
 *	Hartmut Brandt.
 *	All rights reserved.
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 * SNMP module for ATM hardware interfaces - FreeBSD/Ng specific part.
 */

#include "atm.h"
#include "atm_tree.h"
#include "atm_oid.h"

#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#include <net/if_atm.h>

#include <bsnmp/snmp_netgraph.h>
#include <netgraph/ng_message.h>
#include <netgraph/atm/ng_atm.h>

static const struct hwinfo {
	const char *device;
	const char *vendor;
} hwinfo[] = {
	ATM_DEVICE_NAMES
};

struct atmif_sys {
	ng_ID_t		atm_node;
	void		*regc;	/* cookie registration */
};

/*
 * Find the interface for a given node
 */
struct atmif *
atm_node2if(u_int node)
{
	struct atmif_priv *aif;

	if (node != 0)
		TAILQ_FOREACH(aif, &atmif_list, link)
			if (aif->sys->atm_node == node)
				return (&aif->pub);
	return (NULL);
}

u_int
atm_if2node(struct atmif *pub)
{
	struct atmif_priv *aif = (struct atmif_priv *)pub;

	return (aif->sys->atm_node);
}

/*
 * Destroy system dependend stuff.
 */
void
atmif_sys_destroy(struct atmif_priv *aif)
{

	ng_unregister_cookie(aif->sys->regc);
	free(aif->sys);
	free(aif->pub.mib);
}

/*
 * Handle a message from the ATM node
 */
static void
handle_atm_message(const struct ng_mesg *mesg, const char *path __unused,
    ng_ID_t node, void *uarg)
{
	struct atmif_priv *aif = uarg;
	enum atmif_carrier_state ost;

	switch (mesg->header.cmd) {

	  case NGM_ATM_IF_CHANGE:
	    {
		const struct ngm_atm_if_change *arg;

		ost = aif->pub.carrier;
		if (mesg->header.arglen != sizeof(*arg)) {
			syslog(LOG_ERR, "ATM_IF_CHANGE: wrong size");
			atmif_check_carrier(aif);
			return;
		}
		arg = (const struct ngm_atm_if_change *)
		    (const void *)mesg->data;

		if (arg->carrier)
			aif->pub.carrier = ATMIF_CARRIER_ON;
		else
			aif->pub.carrier = ATMIF_CARRIER_OFF;

		if (ost != aif->pub.carrier)
			atmif_send_notification(aif, ATMIF_NOTIFY_CARRIER,
			    (uintptr_t)ost);
		return;
	    }

	  case NGM_ATM_VCC_CHANGE:
	    {
		const struct ngm_atm_vcc_change *arg;

		if (mesg->header.arglen != sizeof(*arg)) {
			syslog(LOG_ERR, "ATM_VCC_CHANGE: wrong size");
			return;
		}
		arg = (const struct ngm_atm_vcc_change *)
		    (const void *)mesg->data;
		atmif_send_notification(aif, ATMIF_NOTIFY_VCC,
		    (uintptr_t)(((arg->vpi & 0xff) << 24) |
		    ((arg->vci & 0xffff) << 8) | (arg->state & 1)));
		return;
	    }
	}
	syslog(LOG_WARNING, "spurious message %u from node [%x]",
	    mesg->header.cmd, node);
}

/*
 * Attach to an ATM interface
 */
int
atmif_sys_attach_if(struct atmif_priv *aif)
{
	struct ng_mesg *resp, *resp1;
	struct namelist *list;
	u_int i;

	if ((aif->sys = malloc(sizeof(*aif->sys))) == NULL) {
		syslog(LOG_CRIT, "out of memory");
		return (-1);
	}
	memset(aif->sys, 0, sizeof(*aif->sys));

	if ((aif->pub.mib = malloc(sizeof(*aif->pub.mib))) == NULL) {
		free(aif->sys);
		syslog(LOG_CRIT, "out of memory");
		return (-1);
	}

	atmif_sys_fill_mib(aif);

	/*
	 * Get ATM node Id. Must do it the hard way by scanning all nodes
	 * because the name may be wrong.
	 */
	if ((resp = ng_dialog_id(snmp_node, NGM_GENERIC_COOKIE, NGM_LISTNODES,
	    NULL, 0)) == NULL) {
		syslog(LOG_ERR, "cannot fetch node list: %m");
		free(aif->sys);
		return (-1);
	}
	list = (struct namelist *)(void *)resp->data;

	for (i = 0; i < list->numnames; i++) {
		if (strcmp(list->nodeinfo[i].type, NG_ATM_NODE_TYPE) != 0)
			continue;
		if ((resp1 = ng_dialog_id(list->nodeinfo[i].id,
		    NGM_ATM_COOKIE, NGM_ATM_GET_IFNAME, NULL, 0)) == NULL)
			continue;
		if (strcmp(resp1->data, aif->pub.ifp->name) == 0) {
			free(resp1);
			break;
		}
		free(resp1);
	}
	if (i == list->numnames)
		aif->sys->atm_node = 0;
	else
		aif->sys->atm_node = list->nodeinfo[i].id;

	free(resp);

	if ((aif->sys->regc = ng_register_cookie(module, NGM_ATM_COOKIE,
	    aif->sys->atm_node, handle_atm_message, aif)) == NULL) {
		syslog(LOG_ERR, "cannot register cookie: %m");
		free(aif->sys);
		return (-1);
	}
	return (0);
}

/*
 * Table of all ATM interfaces - Ng part
 */
int
op_atmif_ng(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int vindex __unused, enum snmp_op op)
{
	struct atmif_priv *aif;
	int err;

	if ((err = atmif_get_aif(value, sub, op, &aif)) != SNMP_ERR_NOERROR)
		return (err);

	if (op == SNMP_OP_SET) {
		switch (value->var.subs[sub - 1]) {

		  default:
			return (SNMP_ERR_NOT_WRITEABLE);
		}
	}

	switch (value->var.subs[sub - 1]) {

	  case LEAF_begemotAtmIfNodeId:
		value->v.uint32 = aif->sys->atm_node;
		return (SNMP_ERR_NOERROR);
	}
	abort();
}

/*
 * Get vendor string
 */
int
atm_sys_get_hw_vendor(struct atmif_priv *aif, struct snmp_value *value)
{

	if (aif->pub.mib->device >= sizeof(hwinfo) / sizeof(hwinfo[0]))
		return (string_get(value, "unknown", -1));
	return (string_get(value, hwinfo[aif->pub.mib->device].vendor, -1));
}

/*
 * Get device string
 */
int
atm_sys_get_hw_device(struct atmif_priv *aif, struct snmp_value *value)
{

	if (aif->pub.mib->device >= sizeof(hwinfo) / sizeof(hwinfo[0]))
		return (string_get(value, "unknown", -1));
	return (string_get(value, hwinfo[aif->pub.mib->device].device, -1));
}

/*
 * Extract the ATM MIB from the interface's private MIB
 */
void
atmif_sys_fill_mib(struct atmif_priv *aif)
{
	struct ifatm_mib *mib;

	if (aif->pub.ifp->specmiblen != sizeof(struct ifatm_mib)) {
		syslog(LOG_ERR, "atmif MIB has wrong size %zu",
		    aif->pub.ifp->specmiblen);
		memset(aif->pub.mib, 0, sizeof(*aif->pub.mib));
		aif->pub.mib->version = 0;
		return;
	}
	mib = (struct ifatm_mib *)aif->pub.ifp->specmib;

	aif->pub.mib->device = mib->device;
	aif->pub.mib->serial = mib->serial;
	aif->pub.mib->hw_version = mib->hw_version;
	aif->pub.mib->sw_version = mib->sw_version;
	aif->pub.mib->media = mib->media;

	memcpy(aif->pub.mib->esi, mib->esi, 6);
	aif->pub.mib->pcr = mib->pcr;
	aif->pub.mib->vpi_bits = mib->vpi_bits;
	aif->pub.mib->vci_bits = mib->vci_bits;
	aif->pub.mib->max_vpcs = mib->max_vpcs;
	aif->pub.mib->max_vccs = mib->max_vccs;
}
