/*
 * ng_btsocket.c
 */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * $Id: ng_btsocket.c,v 1.4 2003/09/14 23:29:06 max Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/errno.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <net/vnet.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket.h>
#include <netgraph/bluetooth/include/ng_btsocket_hci_raw.h>
#include <netgraph/bluetooth/include/ng_btsocket_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket_rfcomm.h>
#include <netgraph/bluetooth/include/ng_btsocket_sco.h>

static int			ng_btsocket_modevent (module_t, int, void *);

/* 
 * Definitions of protocols supported in the BLUETOOTH domain 
 */

/* Bluetooth raw HCI sockets */
static struct protosw ng_btsocket_hci_raw_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_protocol =		BLUETOOTH_PROTO_HCI,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_ctloutput =		ng_btsocket_hci_raw_ctloutput,
	.pr_abort =		ng_btsocket_hci_raw_abort,
	.pr_attach =		ng_btsocket_hci_raw_attach,
	.pr_bind =		ng_btsocket_hci_raw_bind,
	.pr_connect =		ng_btsocket_hci_raw_connect,
	.pr_control =		ng_btsocket_hci_raw_control,
	.pr_detach =		ng_btsocket_hci_raw_detach,
	.pr_disconnect =	ng_btsocket_hci_raw_disconnect,
	.pr_peeraddr =		ng_btsocket_hci_raw_sockaddr,
	.pr_send =		ng_btsocket_hci_raw_send,
	.pr_sockaddr =		ng_btsocket_hci_raw_sockaddr,
	.pr_close =		ng_btsocket_hci_raw_close,
};

/* Bluetooth raw L2CAP sockets */
static struct protosw ng_btsocket_l2cap_raw_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_protocol =		BLUETOOTH_PROTO_L2CAP,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_abort =		ng_btsocket_l2cap_raw_abort,
	.pr_attach =		ng_btsocket_l2cap_raw_attach,
	.pr_bind =		ng_btsocket_l2cap_raw_bind,
	.pr_connect =		ng_btsocket_l2cap_raw_connect,
	.pr_control =		ng_btsocket_l2cap_raw_control,
	.pr_detach =		ng_btsocket_l2cap_raw_detach,
	.pr_disconnect =	ng_btsocket_l2cap_raw_disconnect,
	.pr_peeraddr =		ng_btsocket_l2cap_raw_peeraddr,
	.pr_send =		ng_btsocket_l2cap_raw_send,
	.pr_sockaddr =		ng_btsocket_l2cap_raw_sockaddr,
	.pr_close =		ng_btsocket_l2cap_raw_close,
};

/* Bluetooth SEQPACKET L2CAP sockets */
static struct protosw ng_btsocket_l2cap_protosw = {
	.pr_type =		SOCK_SEQPACKET,
	.pr_protocol =		BLUETOOTH_PROTO_L2CAP,
	.pr_flags =		PR_ATOMIC|PR_CONNREQUIRED,
	.pr_ctloutput =		ng_btsocket_l2cap_ctloutput,
	.pr_abort =		ng_btsocket_l2cap_abort,
	.pr_accept =		ng_btsocket_l2cap_peeraddr,
	.pr_attach =		ng_btsocket_l2cap_attach,
	.pr_bind =		ng_btsocket_l2cap_bind,
	.pr_connect =		ng_btsocket_l2cap_connect,
	.pr_control =		ng_btsocket_l2cap_control,
	.pr_detach =		ng_btsocket_l2cap_detach,
	.pr_disconnect =	ng_btsocket_l2cap_disconnect,
        .pr_listen =		ng_btsocket_l2cap_listen,
	.pr_peeraddr =		ng_btsocket_l2cap_peeraddr,
	.pr_send =		ng_btsocket_l2cap_send,
	.pr_sockaddr =		ng_btsocket_l2cap_sockaddr,
	.pr_close =		ng_btsocket_l2cap_close,
};

/* Bluetooth STREAM RFCOMM sockets */
static struct protosw ng_btsocket_rfcomm_protosw = {
	.pr_type =		SOCK_STREAM,
	.pr_protocol =		BLUETOOTH_PROTO_RFCOMM,
	.pr_flags =		PR_CONNREQUIRED,
	.pr_ctloutput =		ng_btsocket_rfcomm_ctloutput,
	.pr_abort =		ng_btsocket_rfcomm_abort,
	.pr_accept =		ng_btsocket_rfcomm_peeraddr,
	.pr_attach =		ng_btsocket_rfcomm_attach,
	.pr_bind =		ng_btsocket_rfcomm_bind,
	.pr_connect =		ng_btsocket_rfcomm_connect,
	.pr_control =		ng_btsocket_rfcomm_control,
	.pr_detach =		ng_btsocket_rfcomm_detach,
	.pr_disconnect =	ng_btsocket_rfcomm_disconnect,
        .pr_listen =		ng_btsocket_rfcomm_listen,
	.pr_peeraddr =		ng_btsocket_rfcomm_peeraddr,
	.pr_send =		ng_btsocket_rfcomm_send,
	.pr_sockaddr =		ng_btsocket_rfcomm_sockaddr,
	.pr_close =		ng_btsocket_rfcomm_close,
};

/* Bluetooth SEQPACKET SCO sockets */
static struct protosw ng_btsocket_sco_protosw = {
	.pr_type =		SOCK_SEQPACKET,
	.pr_protocol =		BLUETOOTH_PROTO_SCO,
	.pr_flags =		PR_ATOMIC|PR_CONNREQUIRED,
	.pr_ctloutput =		ng_btsocket_sco_ctloutput,
	.pr_abort =		ng_btsocket_sco_abort,
	.pr_accept =		ng_btsocket_sco_peeraddr,
	.pr_attach =		ng_btsocket_sco_attach,
	.pr_bind =		ng_btsocket_sco_bind,
	.pr_connect =		ng_btsocket_sco_connect,
	.pr_control =		ng_btsocket_sco_control,
	.pr_detach =		ng_btsocket_sco_detach,
	.pr_disconnect =	ng_btsocket_sco_disconnect,
	.pr_listen =		ng_btsocket_sco_listen,
	.pr_peeraddr =		ng_btsocket_sco_peeraddr,
	.pr_send =		ng_btsocket_sco_send,
	.pr_sockaddr =		ng_btsocket_sco_sockaddr,
	.pr_close =		ng_btsocket_sco_close,
};

/*
 * BLUETOOTH domain
 */

static struct domain ng_btsocket_domain = {
	.dom_family =		AF_BLUETOOTH,
	.dom_name =		"bluetooth",
	.dom_nprotosw =		5,
	.dom_protosw = {
		&ng_btsocket_hci_raw_protosw,
		&ng_btsocket_l2cap_raw_protosw,
		&ng_btsocket_l2cap_protosw,
		&ng_btsocket_rfcomm_protosw,
		&ng_btsocket_sco_protosw,
	},
};

/* 
 * Socket sysctl tree 
 */

SYSCTL_NODE(_net_bluetooth_hci, OID_AUTO, sockets,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Bluetooth HCI sockets family");
SYSCTL_NODE(_net_bluetooth_l2cap, OID_AUTO, sockets,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Bluetooth L2CAP sockets family");
SYSCTL_NODE(_net_bluetooth_rfcomm, OID_AUTO, sockets,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Bluetooth RFCOMM sockets family");
SYSCTL_NODE(_net_bluetooth_sco, OID_AUTO, sockets,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Bluetooth SCO sockets family");

/* 
 * Module 
 */

static moduledata_t	ng_btsocket_mod = {
	"ng_btsocket",
	ng_btsocket_modevent,
	NULL
};

DECLARE_MODULE(ng_btsocket, ng_btsocket_mod, SI_SUB_PROTO_DOMAIN,
	SI_ORDER_ANY);
MODULE_VERSION(ng_btsocket, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_btsocket, ng_bluetooth, NG_BLUETOOTH_VERSION,
	NG_BLUETOOTH_VERSION, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_btsocket, netgraph, NG_ABI_VERSION,
	NG_ABI_VERSION, NG_ABI_VERSION);

/*
 * Handle loading and unloading for this node type.
 * This is to handle auxiliary linkages (e.g protocol domain addition).
 */

static int  
ng_btsocket_modevent(module_t mod, int event, void *data)
{
	int	error = 0;
        
	switch (event) {
	case MOD_LOAD:
		break;

	case MOD_UNLOAD:
		/* XXX can't unload protocol domain yet */
		error = EBUSY;
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
} /* ng_btsocket_modevent */

DOMAIN_SET(ng_btsocket_);
