/*
 * ng_l2cap_misc.c
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
 * $Id: ng_l2cap_misc.c,v 1.16 2002/09/04 21:38:38 max Exp $
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
#include "ng_l2cap.h"
#include "ng_l2cap_var.h"
#include "ng_l2cap_cmds.h"
#include "ng_l2cap_evnt.h"
#include "ng_l2cap_llpi.h"
#include "ng_l2cap_ulpi.h"
#include "ng_l2cap_misc.h"

static u_int16_t	ng_l2cap_get_cid		(ng_l2cap_p);
static void		ng_l2cap_queue_lp_timeout	(void *);
static void		ng_l2cap_queue_command_timeout	(void *);

/******************************************************************************
 ******************************************************************************
 **                              Utility routines
 ******************************************************************************
 ******************************************************************************/

/*
 * Send hook information to the upper layer
 */

void
ng_l2cap_send_hook_info(node_p node, hook_p hook, void *arg1, int arg2)
{
	ng_l2cap_p	 l2cap = NULL;
	struct ng_mesg	*msg = NULL;
	int		 error = 0;

	if (node == NULL || NG_NODE_NOT_VALID(node) ||
	    hook == NULL || NG_HOOK_NOT_VALID(hook))
		return;

	l2cap = (ng_l2cap_p) NG_NODE_PRIVATE(node);
	if (l2cap->hci == NULL || NG_HOOK_NOT_VALID(l2cap->hci) ||
	    bcmp(&l2cap->bdaddr, NG_HCI_BDADDR_ANY, sizeof(l2cap->bdaddr)) == 0)
		return;

	NG_MKMESSAGE(msg, NGM_L2CAP_COOKIE, NGM_L2CAP_NODE_HOOK_INFO,
		sizeof(bdaddr_t), M_NOWAIT);
	if (msg != NULL) {
		bcopy(&l2cap->bdaddr, msg->data, sizeof(bdaddr_t));
		NG_SEND_MSG_HOOK(error, node, msg, hook, NULL);
	} else
		error = ENOMEM;

	if (error != 0)
		NG_L2CAP_INFO(
"%s: %s - failed to send HOOK_INFO message to hook \"%s\", error=%d\n",
			__func__, NG_NODE_NAME(l2cap->node), NG_HOOK_NAME(hook),
			error);
} /* ng_l2cap_send_hook_info */

/*
 * Create new connection descriptor for the "remote" unit. Will create new
 * connection descriptor and signal channel. Will link both connection and
 * channel to the l2cap node.
 */

ng_l2cap_con_p
ng_l2cap_new_con(ng_l2cap_p l2cap, bdaddr_p bdaddr)
{
	ng_l2cap_con_p	con = NULL;

	/* Create new connection descriptor */
	MALLOC(con, ng_l2cap_con_p, sizeof(*con), M_NETGRAPH_L2CAP,
		M_NOWAIT|M_ZERO);
	if (con == NULL)
		return (NULL);

	con->l2cap = l2cap;
	con->state = NG_L2CAP_CON_CLOSED;

	bcopy(bdaddr, &con->remote, sizeof(con->remote));
	callout_handle_init(&con->con_timo);

	con->ident = NG_L2CAP_FIRST_IDENT - 1;
	TAILQ_INIT(&con->cmd_list);

	/* Link connection */
	LIST_INSERT_HEAD(&l2cap->con_list, con, next); 

	return (con);
} /* ng_l2cap_new_con */

/*
 * Free connection descriptor. Will unlink connection and free everything.
 */

void
ng_l2cap_free_con(ng_l2cap_con_p con)
{
	ng_l2cap_chan_p f = NULL, n = NULL;

	if (con->state == NG_L2CAP_W4_LP_CON_CFM)
		ng_l2cap_lp_untimeout(con);

	if (con->tx_pkt != NULL) {
		while (con->tx_pkt != NULL) {
			struct mbuf	*m = con->tx_pkt->m_nextpkt;

			m_freem(con->tx_pkt);
			con->tx_pkt = m;
		}
	}

	NG_FREE_M(con->rx_pkt);

	for (f = LIST_FIRST(&con->l2cap->chan_list); f != NULL; ) {
		n = LIST_NEXT(f, next);

		if (f->con == con)
			ng_l2cap_free_chan(f);

		f = n;
	}

	while (!TAILQ_EMPTY(&con->cmd_list)) {
		ng_l2cap_cmd_p	cmd = TAILQ_FIRST(&con->cmd_list);

		ng_l2cap_unlink_cmd(cmd);
		ng_l2cap_free_cmd(cmd);
	}

	LIST_REMOVE(con, next);
	bzero(con, sizeof(*con));
	FREE(con, M_NETGRAPH_L2CAP);
} /* ng_l2cap_free_con */

/*
 * Get connection by "remote" address
 */

ng_l2cap_con_p
ng_l2cap_con_by_addr(ng_l2cap_p l2cap, bdaddr_p bdaddr)
{
	ng_l2cap_con_p	con = NULL;

	LIST_FOREACH(con, &l2cap->con_list, next)
		if (bcmp(bdaddr, &con->remote, sizeof(con->remote)) == 0)
			break;

	return (con);
} /* ng_l2cap_con_by_addr */

/*
 * Get connection by "handle" 
 */

ng_l2cap_con_p
ng_l2cap_con_by_handle(ng_l2cap_p l2cap, u_int16_t con_handle)
{
	ng_l2cap_con_p	con = NULL;

	LIST_FOREACH(con, &l2cap->con_list, next)
		if (con->con_handle == con_handle)
			break;

	return (con);
} /* ng_l2cap_con_by_handle */

/*
 * Allocate new L2CAP channel descriptor on "con" conection with "psm".
 * Will link the channel to the l2cap node
 */

ng_l2cap_chan_p
ng_l2cap_new_chan(ng_l2cap_p l2cap, ng_l2cap_con_p con, u_int16_t psm)
{
	ng_l2cap_chan_p	ch = NULL;

	MALLOC(ch, ng_l2cap_chan_p, sizeof(*ch), M_NETGRAPH_L2CAP,
		M_NOWAIT|M_ZERO);
	if (ch == NULL)
		return (NULL);

	ch->scid = ng_l2cap_get_cid(l2cap);

	if (ch->scid != NG_L2CAP_NULL_CID) {
		/* Initialize channel */
		ch->psm = psm;
		ch->con = con;
		ch->state = NG_L2CAP_CLOSED;

		/* Set MTU and flow control settings to defaults */
		ch->imtu = NG_L2CAP_MTU_DEFAULT;
		bcopy(ng_l2cap_default_flow(), &ch->iflow, sizeof(ch->iflow));

		ch->omtu = NG_L2CAP_MTU_DEFAULT;
		bcopy(ng_l2cap_default_flow(), &ch->oflow, sizeof(ch->oflow));

		ch->flush_timo = NG_L2CAP_FLUSH_TIMO_DEFAULT;
		ch->link_timo = NG_L2CAP_LINK_TIMO_DEFAULT;

		LIST_INSERT_HEAD(&l2cap->chan_list, ch, next);
	} else {
		bzero(ch, sizeof(*ch));
		FREE(ch, M_NETGRAPH_L2CAP);
		ch = NULL;
	}

	return (ch);
} /* ng_l2cap_new_chan */

/*
 * Get channel by source (local) channel ID
 */

ng_l2cap_chan_p
ng_l2cap_chan_by_scid(ng_l2cap_p l2cap, u_int16_t scid)
{
	ng_l2cap_chan_p	ch = NULL;

	LIST_FOREACH(ch, &l2cap->chan_list, next)
		if (ch->scid == scid)
			break;

	return (ch);
} /* ng_l2cap_chan_by_scid */

/*
 * Free channel descriptor.
 */

void
ng_l2cap_free_chan(ng_l2cap_chan_p ch)
{
	ng_l2cap_cmd_p	f = NULL, n = NULL;

	f = TAILQ_FIRST(&ch->con->cmd_list);
	while (f != NULL) {
		n = TAILQ_NEXT(f, next);

		if (f->ch == ch) {
			ng_l2cap_unlink_cmd(f);
			ng_l2cap_free_cmd(f);
		}

		f = n;
	}

	LIST_REMOVE(ch, next);
	bzero(ch, sizeof(*ch));
	FREE(ch, M_NETGRAPH_L2CAP);
} /* ng_l2cap_free_chan */

/*
 * Create new L2CAP command descriptor. WILL NOT add command to the queue.
 */

ng_l2cap_cmd_p
ng_l2cap_new_cmd(ng_l2cap_con_p con, ng_l2cap_chan_p ch, u_int8_t ident,
		u_int8_t code, u_int32_t token)
{
	ng_l2cap_cmd_p	cmd = NULL;

	KASSERT((ch == NULL || ch->con == con),
("%s: %s - invalid channel pointer!\n",
		__func__, NG_NODE_NAME(con->l2cap->node)));

	MALLOC(cmd, ng_l2cap_cmd_p, sizeof(*cmd), M_NETGRAPH_L2CAP,
		M_NOWAIT|M_ZERO);
	if (cmd == NULL)
		return (NULL);

	cmd->con = con;
	cmd->ch = ch;
	cmd->ident = ident;
	cmd->code = code;
	cmd->token = token;
	callout_handle_init(&cmd->timo);

	return (cmd);
} /* ng_l2cap_new_cmd */
 
/*
 * Get L2CAP command descriptor by ident
 */

ng_l2cap_cmd_p
ng_l2cap_cmd_by_ident(ng_l2cap_con_p con, u_int8_t ident)
{
	ng_l2cap_cmd_p	cmd = NULL;

	TAILQ_FOREACH(cmd, &con->cmd_list, next)
		if (cmd->ident == ident)
			break;

	return (cmd);
} /* ng_l2cap_cmd_by_ident */

/*
 * Set LP timeout
 */

void
ng_l2cap_lp_timeout(ng_l2cap_con_p con)
{
	NG_NODE_REF(con->l2cap->node);
	con->con_timo = timeout(ng_l2cap_queue_lp_timeout, con,
				bluetooth_hci_connect_timeout());
} /* ng_l2cap_lp_timeout */

/*
 * Unset LP timeout
 */

void
ng_l2cap_lp_untimeout(ng_l2cap_con_p con)
{
	untimeout(ng_l2cap_queue_lp_timeout, con, con->con_timo);
	NG_NODE_UNREF(con->l2cap->node);
} /* ng_l2cap_lp_untimeout */

/*
 * OK, timeout has happend so queue LP timeout processing function
 */

static void
ng_l2cap_queue_lp_timeout(void *context)
{
	ng_l2cap_con_p	con = (ng_l2cap_con_p) context;
	node_p		node = con->l2cap->node;

	/*
	 * We need to save node pointer here, because ng_send_fn()
	 * can execute ng_l2cap_process_lp_timeout() without putting
	 * item into node's queue (if node can be locked). Once 
	 * ng_l2cap_process_lp_timeout() executed the con pointer 
	 * is no longer valid.
	 */

	if (NG_NODE_IS_VALID(node))
		ng_send_fn(node, NULL, &ng_l2cap_process_lp_timeout, con, 0);

	NG_NODE_UNREF(node);
} /* ng_l2cap_queue_lp_timeout */

/*
 * Set L2CAP command timeout
 */

void
ng_l2cap_command_timeout(ng_l2cap_cmd_p cmd, int timo)
{
	NG_NODE_REF(cmd->con->l2cap->node);
	cmd->flags |= NG_L2CAP_CMD_PENDING;
	cmd->timo = timeout(ng_l2cap_queue_command_timeout, cmd, timo);
} /* ng_l2cap_command_timeout */

/*
 * Unset L2CAP command timeout
 */

void
ng_l2cap_command_untimeout(ng_l2cap_cmd_p cmd)
{
	cmd->flags &= ~NG_L2CAP_CMD_PENDING;
	untimeout(ng_l2cap_queue_command_timeout, cmd, cmd->timo);
	NG_NODE_UNREF(cmd->con->l2cap->node);
} /* ng_l2cap_command_untimeout */

/*
 * OK, timeout has happend so queue L2CAP command timeout processing function
 */

static void
ng_l2cap_queue_command_timeout(void *context)
{
	ng_l2cap_cmd_p	cmd = (ng_l2cap_cmd_p) context;
	node_p		node = cmd->con->l2cap->node;

	/*
	 * We need to save node pointer here, because ng_send_fn()
	 * can execute ng_l2cap_process_command_timeout() without
	 * putting item into node's queue (if node can be locked).
	 * Once ng_l2cap_process_command_timeout() executed the
	 * cmd pointer is no longer valid.
	 */

	if (NG_NODE_IS_VALID(node))
		ng_send_fn(node,NULL,&ng_l2cap_process_command_timeout,cmd,0);

	NG_NODE_UNREF(node);
} /* ng_l2cap_queue_command_timeout */

/*
 * Prepend "m"buf with "size" bytes
 */

struct mbuf *
ng_l2cap_prepend(struct mbuf *m, int size)
{
	M_PREPEND(m, size, M_DONTWAIT);
	if (m == NULL || (m->m_len < size && (m = m_pullup(m, size)) == NULL))
		return (NULL);

	return (m);
} /* ng_l2cap_prepend */

/*
 * Default flow settings
 */

ng_l2cap_flow_p
ng_l2cap_default_flow(void)
{
	static ng_l2cap_flow_t	default_flow = {
		/* flags */		0x0,
		/* service_type */	NG_HCI_SERVICE_TYPE_BEST_EFFORT,
		/* token_rate */	0xffffffff, /* maximum */
		/* token_bucket_size */	0xffffffff, /* maximum */
		/* peak_bandwidth */	0x00000000, /* maximum */
		/* latency */		0xffffffff, /* don't care */
		/* delay_variation */	0xffffffff  /* don't care */
	};

	return (&default_flow);
} /* ng_l2cap_default_flow */

/*
 * Get next available channel ID
 * XXX FIXME this is *UGLY* but will do for now
 */

static u_int16_t
ng_l2cap_get_cid(ng_l2cap_p l2cap)
{
	u_int16_t	cid = l2cap->cid + 1;

	if (cid < NG_L2CAP_FIRST_CID)
		cid = NG_L2CAP_FIRST_CID;

	while (cid != l2cap->cid) {
		if (ng_l2cap_chan_by_scid(l2cap, cid) == NULL) {
			l2cap->cid = cid;

			return (cid);
		}

		cid ++;
		if (cid < NG_L2CAP_FIRST_CID)
			cid = NG_L2CAP_FIRST_CID;
	}
		
	return (NG_L2CAP_NULL_CID);
} /* ng_l2cap_get_cid */

/*
 * Get next available command ident
 * XXX FIXME this is *UGLY* but will do for now
 */

u_int8_t
ng_l2cap_get_ident(ng_l2cap_con_p con)
{
	u_int8_t	ident = con->ident + 1;

	if (ident < NG_L2CAP_FIRST_IDENT)
		ident = NG_L2CAP_FIRST_IDENT;

	while (ident != con->ident) {
		if (ng_l2cap_cmd_by_ident(con, ident) == NULL) {
			con->ident = ident;

			return (ident);
		}

		ident ++;
		if (ident < NG_L2CAP_FIRST_IDENT)
			ident = NG_L2CAP_FIRST_IDENT;
	}

	return (NG_L2CAP_NULL_IDENT);
} /* ng_l2cap_get_ident */

