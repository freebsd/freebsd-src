/*
 * ng_hci_misc.c
 *
 * Copyright (c) Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
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
 *
 * $Id: ng_hci_misc.c,v 1.18 2002/10/30 00:18:19 max Exp $
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include "ng_bluetooth.h"
#include "ng_hci.h"
#include "ng_hci_var.h"
#include "ng_hci_cmds.h"
#include "ng_hci_evnt.h"
#include "ng_hci_ulpi.h"
#include "ng_hci_misc.h"

/******************************************************************************
 ******************************************************************************
 **                              Utility routines
 ******************************************************************************
 ******************************************************************************/

static void	ng_hci_command_queue_timeout		(void *);
static void	ng_hci_con_queue_timeout		(void *);
static void	ng_hci_con_queue_watchdog_timeout	(void *);

/*
 * Give packet to RAW hook
 * Assumes input mbuf is read only.
 */

void
ng_hci_mtap(ng_hci_unit_p unit, struct mbuf *m0)
{
	struct mbuf	*m = NULL;
	int		 error = 0;

	if (unit->raw != NULL && NG_HOOK_IS_VALID(unit->raw)) {
		m = m_dup(m0, M_DONTWAIT);
		if (m != NULL)
			NG_SEND_DATA_ONLY(error, unit->raw, m);

		if (error != 0)
			NG_HCI_INFO(
"%s: %s - Could not forward packet, error=%d\n",
				__func__, NG_NODE_NAME(unit->node), error);
	}
} /* ng_hci_mtap */

/*
 * Send notification to the upper layer's
 */

void
ng_hci_node_is_up(node_p node, hook_p hook, void *arg1, int arg2)
{
	ng_hci_unit_p		 unit = NULL;
	struct ng_mesg		*msg = NULL;
	ng_hci_node_up_ep	*ep = NULL;
	int			 error;

	if (node == NULL || NG_NODE_NOT_VALID(node) ||
	    hook == NULL || NG_HOOK_NOT_VALID(hook))
		return;

	unit = (ng_hci_unit_p) NG_NODE_PRIVATE(node);
	if ((unit->state & NG_HCI_UNIT_READY) != NG_HCI_UNIT_READY)
		return;

	if (hook != unit->acl && hook != unit->sco)
		return;

	NG_MKMESSAGE(msg,NGM_HCI_COOKIE,NGM_HCI_NODE_UP,sizeof(*ep),M_NOWAIT);
	if (msg != NULL) {
		ep = (ng_hci_node_up_ep *)(msg->data);

		if (hook == unit->acl) {
			NG_HCI_BUFF_ACL_SIZE(unit->buffer, ep->pkt_size);
			NG_HCI_BUFF_ACL_TOTAL(unit->buffer, ep->num_pkts);
		} else {
			NG_HCI_BUFF_SCO_SIZE(unit->buffer, ep->pkt_size);
			NG_HCI_BUFF_SCO_TOTAL(unit->buffer, ep->num_pkts);
		} 

		bcopy(&unit->bdaddr, &ep->bdaddr, sizeof(ep->bdaddr));

		NG_SEND_MSG_HOOK(error, node, msg, hook, NULL);
	} else
		error = ENOMEM;

	if (error != 0)
		NG_HCI_INFO(
"%s: %s - failed to send NODE_UP message to hook \"%s\", error=%d\n",
			__func__, NG_NODE_NAME(unit->node), 
			NG_HOOK_NAME(hook), error);
} /* ng_hci_node_is_up */

/*
 * Clean unit (helper)
 */

void
ng_hci_unit_clean(ng_hci_unit_p unit, int reason)
{
	int	size;

	/* Drain command queue */
	if (unit->state & NG_HCI_UNIT_COMMAND_PENDING)
		ng_hci_command_untimeout(unit);

	NG_BT_MBUFQ_DRAIN(&unit->cmdq);
	NG_HCI_BUFF_CMD_SET(unit->buffer, 1);

	/* Clean up connection list */
	while (!LIST_EMPTY(&unit->con_list)) {
		ng_hci_unit_con_p	con = LIST_FIRST(&unit->con_list);

		/*
		 * Notify upper layer protocol and destroy connection 
		 * descriptor. Do not really care about the result.
		 */

		ng_hci_lp_discon_ind(con, reason);
		ng_hci_free_con(con);
	}

	NG_HCI_BUFF_ACL_TOTAL(unit->buffer, size);
	NG_HCI_BUFF_ACL_FREE(unit->buffer, size);

	NG_HCI_BUFF_SCO_TOTAL(unit->buffer, size);
	NG_HCI_BUFF_SCO_FREE(unit->buffer, size);

	/* Clean up neighbors list */
	ng_hci_flush_neighbor_cache(unit);
} /* ng_hci_unit_clean */

/*
 * Allocate and link new unit neighbor cache entry
 */

ng_hci_neighbor_p
ng_hci_new_neighbor(ng_hci_unit_p unit)
{
	ng_hci_neighbor_p	n = NULL;

	MALLOC(n, ng_hci_neighbor_p, sizeof(*n), M_NETGRAPH_HCI,
		M_NOWAIT | M_ZERO); 
	if (n != NULL) {
		getmicrotime(&n->updated);
		LIST_INSERT_HEAD(&unit->neighbors, n, next);
	}

	return (n);
} /* ng_hci_new_neighbor */

/*
 * Free unit neighbor cache entry
 */

void
ng_hci_free_neighbor(ng_hci_neighbor_p n)
{
	LIST_REMOVE(n, next);
	bzero(n, sizeof(*n));
	FREE(n, M_NETGRAPH_HCI);
} /* ng_hci_free_neighbor */

/*
 * Flush neighbor cache 
 */

void
ng_hci_flush_neighbor_cache(ng_hci_unit_p unit)
{
	while (!LIST_EMPTY(&unit->neighbors))
		ng_hci_free_neighbor(LIST_FIRST(&unit->neighbors));
} /* ng_hci_flush_neighbor_cache */

/*
 * Lookup unit in neighbor cache
 */

ng_hci_neighbor_p
ng_hci_get_neighbor(ng_hci_unit_p unit, bdaddr_p bdaddr)
{
	ng_hci_neighbor_p	n = NULL;

	for (n = LIST_FIRST(&unit->neighbors); n != NULL; ) {
		ng_hci_neighbor_p	nn = LIST_NEXT(n, next);

		if (!ng_hci_neighbor_stale(n)) {
			if (bcmp(&n->bdaddr, bdaddr, sizeof(*bdaddr)) == 0)
				break;
		} else 
			ng_hci_free_neighbor(n); /* remove old entry */

		n = nn;
	}
	
	return (n);
} /* ng_hci_get_neighbor */

/*
 * Check if neighbor entry is stale
 */

int
ng_hci_neighbor_stale(ng_hci_neighbor_p n)
{
	struct timeval	now;

	getmicrotime(&now);

	return (now.tv_sec - n->updated.tv_sec > bluetooth_hci_max_neighbor_age());
} /* ng_hci_neighbor_stale */

/*
 * Allocate and link new connection descriptor
 */

ng_hci_unit_con_p
ng_hci_new_con(ng_hci_unit_p unit, int link_type)
{
	ng_hci_unit_con_p	con = NULL;
	int			num_pkts;

	MALLOC(con, ng_hci_unit_con_p, sizeof(*con), M_NETGRAPH_HCI,
		M_NOWAIT | M_ZERO);
	if (con != NULL) {
		con->unit = unit;
		con->state = NG_HCI_CON_CLOSED;
		con->link_type = link_type;

		if (con->link_type == NG_HCI_LINK_ACL)
			NG_HCI_BUFF_ACL_TOTAL(unit->buffer, num_pkts);
		else
			NG_HCI_BUFF_SCO_TOTAL(unit->buffer, num_pkts);

		NG_BT_ITEMQ_INIT(&con->conq, num_pkts);

		callout_handle_init(&con->con_timo);
		callout_handle_init(&con->watchdog_timo);

		LIST_INSERT_HEAD(&unit->con_list, con, next);
	}

	return (con);
} /* ng_hci_new_con */

/*
 * Free connection descriptor
 */

void
ng_hci_free_con(ng_hci_unit_con_p con)
{ 
	LIST_REMOVE(con, next);

	/* Remove all timeouts (if any) */
	if (con->flags & NG_HCI_CON_TIMEOUT_PENDING)
		ng_hci_con_untimeout(con);

	if (con->flags & NG_HCI_CON_WATCHDOG_TIMEOUT_PENDING)
		ng_hci_con_watchdog_untimeout(con);

	/*
	 * If we have pending packets then assume that Host Controller has 
	 * flushed these packets and we can free them too
	 */

	if (con->link_type == NG_HCI_LINK_ACL)
		NG_HCI_BUFF_ACL_FREE(con->unit->buffer, con->pending);
	else
		NG_HCI_BUFF_SCO_FREE(con->unit->buffer, con->pending);

	NG_BT_ITEMQ_DESTROY(&con->conq);

	bzero(con, sizeof(*con));
	FREE(con, M_NETGRAPH_HCI);
} /* ng_hci_free_con */

/*
 * Lookup connection for given unit and connection handle.
 */

ng_hci_unit_con_p
ng_hci_con_by_handle(ng_hci_unit_p unit, int con_handle)
{
	ng_hci_unit_con_p	con = NULL;

	LIST_FOREACH(con, &unit->con_list, next)
		if (con->con_handle == con_handle)
			break;

	return (con);
} /* ng_hci_con_by_handle */

/*
 * Lookup connection for given unit, link type and remove unit address
 */

ng_hci_unit_con_p
ng_hci_con_by_bdaddr(ng_hci_unit_p unit, bdaddr_p bdaddr, int link_type)
{
	ng_hci_unit_con_p	con = NULL;

	LIST_FOREACH(con, &unit->con_list, next)
		if (con->link_type == link_type &&
		    bcmp(&con->bdaddr, bdaddr, sizeof(bdaddr_t)) == 0)
			break;

	return (con);
} /* ng_hci_con_by_bdaddr */

/*
 * Set HCI command timeout
 */

void
ng_hci_command_timeout(ng_hci_unit_p unit)
{
	if (!(unit->state & NG_HCI_UNIT_COMMAND_PENDING)) {
		NG_NODE_REF(unit->node);
		unit->state |= NG_HCI_UNIT_COMMAND_PENDING;
		unit->cmd_timo = timeout(ng_hci_command_queue_timeout, unit, 
					bluetooth_hci_command_timeout());
	} else
		KASSERT(0,
("%s: %s - Duplicated command timeout!\n", __func__, NG_NODE_NAME(unit->node)));
} /* ng_hci_command_timeout */

/*
 * Unset HCI command timeout
 */

void
ng_hci_command_untimeout(ng_hci_unit_p unit)
{
	if (unit->state & NG_HCI_UNIT_COMMAND_PENDING) {
		unit->state &= ~NG_HCI_UNIT_COMMAND_PENDING;
		untimeout(ng_hci_command_queue_timeout, unit, unit->cmd_timo);
		NG_NODE_UNREF(unit->node);
	} else
		KASSERT(0,
("%s: %s - No command timeout!\n", __func__, NG_NODE_NAME(unit->node)));
} /* ng_hci_command_untimeout */

/*
 * OK timeout has happend, so queue timeout processing function
 */

static void
ng_hci_command_queue_timeout(void *context)
{
	ng_hci_unit_p	unit = (ng_hci_unit_p) context;
	node_p		node = unit->node;

	if (NG_NODE_IS_VALID(node))
		ng_send_fn(node,NULL,&ng_hci_process_command_timeout,unit,0);

	NG_NODE_UNREF(node);
} /* ng_hci_command_queue_timeout */

/*
 * Set HCI connection timeout
 */

void
ng_hci_con_timeout(ng_hci_unit_con_p con)
{
	if (!(con->flags & NG_HCI_CON_TIMEOUT_PENDING)) {
		NG_NODE_REF(con->unit->node);
		con->flags |= NG_HCI_CON_TIMEOUT_PENDING;
		con->con_timo = timeout(ng_hci_con_queue_timeout, con, 
					bluetooth_hci_connect_timeout());
	} else
		KASSERT(0,
("%s: %s - Duplicated connection timeout!\n",
			__func__, NG_NODE_NAME(con->unit->node)));
} /* ng_hci_con_timeout */

/*
 * Unset HCI connection timeout
 */

void
ng_hci_con_untimeout(ng_hci_unit_con_p con)
{
	if (con->flags & NG_HCI_CON_TIMEOUT_PENDING) {
		con->flags &= ~NG_HCI_CON_TIMEOUT_PENDING;
		untimeout(ng_hci_con_queue_timeout, con, con->con_timo);
		NG_NODE_UNREF(con->unit->node);
	} else
		KASSERT(0,
("%s: %s - No connection timeout!\n", __func__, NG_NODE_NAME(con->unit->node)));
} /* ng_hci_con_untimeout */

/*
 * OK timeout has happend, so queue timeout processing function
 */

static void
ng_hci_con_queue_timeout(void *context)
{
	ng_hci_unit_con_p	con = (ng_hci_unit_con_p) context;
	node_p			node = con->unit->node;

	if (NG_NODE_IS_VALID(node))
		ng_send_fn(node, NULL, &ng_hci_process_con_timeout, con, 0);

	NG_NODE_UNREF(node);
} /* ng_hci_con_queue_timeout */

/*
 * Set HCI connection watchdog timeout
 */

void
ng_hci_con_watchdog_timeout(ng_hci_unit_con_p con)
{
	if (!(con->flags & NG_HCI_CON_WATCHDOG_TIMEOUT_PENDING)) {
		NG_NODE_REF(con->unit->node);
		con->flags |= NG_HCI_CON_WATCHDOG_TIMEOUT_PENDING;
		con->watchdog_timo = timeout(ng_hci_con_queue_watchdog_timeout,
					con, bluetooth_hci_watchdog_timeout());
	} else
		KASSERT(0,
("%s: %s - Duplicated connection watchdog timeout!\n",
			__func__, NG_NODE_NAME(con->unit->node)));
} /* ng_hci_con_watchdog_timeout */

/*
 * Unset HCI connection watchdog timeout
 */

void
ng_hci_con_watchdog_untimeout(ng_hci_unit_con_p con)
{
	if (con->flags & NG_HCI_CON_WATCHDOG_TIMEOUT_PENDING) {
		con->flags &= ~NG_HCI_CON_WATCHDOG_TIMEOUT_PENDING;
		untimeout(ng_hci_con_queue_watchdog_timeout, con, 
			con->watchdog_timo);
		NG_NODE_UNREF(con->unit->node);
	} else
		KASSERT(0,
("%s: %s - No connection watchdog timeout!\n",
			 __func__, NG_NODE_NAME(con->unit->node)));
} /* ng_hci_con_watchdog_untimeout */

/*
 * OK timeout has happend, so queue timeout processing function
 */

static void
ng_hci_con_queue_watchdog_timeout(void *context)
{
	ng_hci_unit_con_p	con = (ng_hci_unit_con_p) context;
	node_p			node = con->unit->node;

	if (NG_NODE_IS_VALID(node))
		ng_send_fn(node, NULL, &ng_hci_process_con_watchdog_timeout, 
			con, 0);

	NG_NODE_UNREF(node);
} /* ng_hci_con_queue_watchdog_timeout */

#if 0
/*
 * Convert numeric error code/reason to a string
 */

char const * const
ng_hci_str_error(u_int16_t code)
{
#define	LAST_ERROR_CODE			((sizeof(s)/sizeof(s[0]))-1)
	static char const * const	s[] = {
	/* 0x00 */ "No error",
	/* 0x01 */ "Unknown HCI command",
	/* 0x02 */ "No connection",
	/* 0x03 */ "Hardware failure",
	/* 0x04 */ "Page timeout",
	/* 0x05 */ "Authentication failure",
	/* 0x06 */ "Key missing",
	/* 0x07 */ "Memory full",
	/* 0x08 */ "Connection timeout",
	/* 0x09 */ "Max number of connections",
	/* 0x0a */ "Max number of SCO connections to a unit",
	/* 0x0b */ "ACL connection already exists",
	/* 0x0c */ "Command disallowed",
	/* 0x0d */ "Host rejected due to limited resources",
	/* 0x0e */ "Host rejected due to securiity reasons",
	/* 0x0f */ "Host rejected due to remote unit is a personal unit",
	/* 0x10 */ "Host timeout",
	/* 0x11 */ "Unsupported feature or parameter value",
	/* 0x12 */ "Invalid HCI command parameter",
	/* 0x13 */ "Other end terminated connection: User ended connection",
	/* 0x14 */ "Other end terminated connection: Low resources",
	/* 0x15 */ "Other end terminated connection: About to power off",
	/* 0x16 */ "Connection terminated by local host",
	/* 0x17 */ "Repeated attempts",
	/* 0x18 */ "Pairing not allowed",
	/* 0x19 */ "Unknown LMP PDU",
	/* 0x1a */ "Unsupported remote feature",
	/* 0x1b */ "SCO offset rejected",
	/* 0x1c */ "SCO interval rejected",
	/* 0x1d */ "SCO air mode rejected",
	/* 0x1e */ "Invalid LMP parameters",
	/* 0x1f */ "Unspecified error",
	/* 0x20 */ "Unsupported LMP parameter value",
	/* 0x21 */ "Role change not allowed",
	/* 0x22 */ "LMP response timeout",
	/* 0x23 */ "LMP error transaction collision",
	/* 0x24 */ "LMP PSU not allowed",
	/* 0x25 */ "Encryption mode not acceptable",
	/* 0x26 */ "Unit key used",
	/* 0x27 */ "QoS is not supported",
	/* 0x28 */ "Instant passed",
	/* 0x29 */ "Paring with unit key not supported",
	/* SHOULD ALWAYS BE LAST */ "Unknown error"
	};

	return ((code >= LAST_ERROR_CODE)? s[LAST_ERROR_CODE] : s[code]);
} /* ng_hci_str_error */
#endif

