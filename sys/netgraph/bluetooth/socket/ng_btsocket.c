/*
 * ng_btsocket.c
 */

/*-
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
 * $FreeBSD$
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
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket.h>
#include <netgraph/bluetooth/include/ng_btsocket_hci_raw.h>
#include <netgraph/bluetooth/include/ng_btsocket_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket_rfcomm.h>

static int			ng_btsocket_modevent (module_t, int, void *);
extern struct domain		ng_btsocket_domain;

/*
 * Bluetooth raw HCI sockets
 */

static struct pr_usrreqs	ng_btsocket_hci_raw_usrreqs = {
	ng_btsocket_hci_raw_abort,	/* abort */
	pru_accept_notsupp,		/* accept */
	ng_btsocket_hci_raw_attach,	/* attach */
	ng_btsocket_hci_raw_bind,	/* bind */
	ng_btsocket_hci_raw_connect,	/* connect */
	pru_connect2_notsupp,		/* connect2 */
	ng_btsocket_hci_raw_control,	/* control */
	ng_btsocket_hci_raw_detach,	/* detach */
	ng_btsocket_hci_raw_disconnect,	/* disconnect */
	pru_listen_notsupp,		/* listen */
	ng_btsocket_hci_raw_peeraddr,	/* peeraddr */
	pru_rcvd_notsupp,		/* rcvd */
	pru_rcvoob_notsupp,		/* rcvoob */
	ng_btsocket_hci_raw_send,	/* send */
	pru_sense_null,			/* send */
	NULL,				/* shutdown */
	ng_btsocket_hci_raw_sockaddr,	/* sockaddr */
	sosend,
	soreceive,
	sopoll,
	pru_sosetlabel_null
};

/*
 * Bluetooth raw L2CAP sockets
 */

static struct pr_usrreqs	ng_btsocket_l2cap_raw_usrreqs = {
	ng_btsocket_l2cap_raw_abort,	/* abort */
	pru_accept_notsupp,		/* accept */
	ng_btsocket_l2cap_raw_attach,	/* attach */
	ng_btsocket_l2cap_raw_bind,	/* bind */
	ng_btsocket_l2cap_raw_connect,	/* connect */
	pru_connect2_notsupp,		/* connect2 */
	ng_btsocket_l2cap_raw_control,	/* control */
	ng_btsocket_l2cap_raw_detach,	/* detach */
	ng_btsocket_l2cap_raw_disconnect, /* disconnect */
        pru_listen_notsupp,		/* listen */
	ng_btsocket_l2cap_raw_peeraddr,	/* peeraddr */
	pru_rcvd_notsupp,		/* rcvd */
	pru_rcvoob_notsupp,		/* rcvoob */
	ng_btsocket_l2cap_raw_send,	/* send */
	pru_sense_null,			/* send */
	NULL,				/* shutdown */
	ng_btsocket_l2cap_raw_sockaddr,	/* sockaddr */
	sosend,
	soreceive,
	sopoll,
	pru_sosetlabel_null
};

/*
 * Bluetooth SEQPACKET L2CAP sockets
 */

static struct pr_usrreqs	ng_btsocket_l2cap_usrreqs = {
	ng_btsocket_l2cap_abort,	/* abort */
	ng_btsocket_l2cap_accept,	/* accept */
	ng_btsocket_l2cap_attach,	/* attach */
	ng_btsocket_l2cap_bind,		/* bind */
	ng_btsocket_l2cap_connect,	/* connect */
	pru_connect2_notsupp,		/* connect2 */
	ng_btsocket_l2cap_control,	/* control */
	ng_btsocket_l2cap_detach,	/* detach */
	ng_btsocket_l2cap_disconnect,	/* disconnect */
        ng_btsocket_l2cap_listen,	/* listen */
	ng_btsocket_l2cap_peeraddr,	/* peeraddr */
	pru_rcvd_notsupp,		/* rcvd */
	pru_rcvoob_notsupp,		/* rcvoob */
	ng_btsocket_l2cap_send,		/* send */
	pru_sense_null,			/* send */
	NULL,				/* shutdown */
	ng_btsocket_l2cap_sockaddr,	/* sockaddr */
	sosend,
	soreceive,
	sopoll,
	pru_sosetlabel_null
};

/*
 * Bluetooth STREAM RFCOMM sockets
 */

static struct pr_usrreqs	ng_btsocket_rfcomm_usrreqs = {
	ng_btsocket_rfcomm_abort,	/* abort */
	ng_btsocket_rfcomm_accept,	/* accept */
	ng_btsocket_rfcomm_attach,	/* attach */
	ng_btsocket_rfcomm_bind,	/* bind */
	ng_btsocket_rfcomm_connect,	/* connect */
	pru_connect2_notsupp,		/* connect2 */
	ng_btsocket_rfcomm_control,	/* control */
	ng_btsocket_rfcomm_detach,	/* detach */
	ng_btsocket_rfcomm_disconnect,	/* disconnect */
        ng_btsocket_rfcomm_listen,	/* listen */
	ng_btsocket_rfcomm_peeraddr,	/* peeraddr */
	pru_rcvd_notsupp,		/* rcvd */
	pru_rcvoob_notsupp,		/* rcvoob */
	ng_btsocket_rfcomm_send,	/* send */
	pru_sense_null,			/* send */
	NULL,				/* shutdown */
	ng_btsocket_rfcomm_sockaddr,	/* sockaddr */
	sosend,
	soreceive,
	sopoll,
	pru_sosetlabel_null
};

/* 
 * Definitions of protocols supported in the BLUETOOTH domain 
 */

static struct protosw		ng_btsocket_protosw[] = {
{
	SOCK_RAW,			/* protocol type */
	&ng_btsocket_domain,		/* backpointer to domain */
	BLUETOOTH_PROTO_HCI,		/* protocol */
	PR_ATOMIC | PR_ADDR,		/* flags */
	NULL, NULL, NULL,		/* input, output, ctlinput */
	ng_btsocket_hci_raw_ctloutput,	/* ctloutput */
	NULL,				/* ousrreq() */
	ng_btsocket_hci_raw_init,	/* init */ 
	NULL, NULL, NULL,		/* fasttimeo, slowtimo, drain */
	&ng_btsocket_hci_raw_usrreqs,	/* usrreq table (above) */
	/* { NULL } */			/* pfh (protocol filter head?) */
},
{
	SOCK_RAW,			/* protocol type */
	&ng_btsocket_domain,		/* backpointer to domain */
	BLUETOOTH_PROTO_L2CAP,		/* protocol */
	PR_ATOMIC | PR_ADDR,		/* flags */
	NULL, NULL, NULL,		/* input, output, ctlinput */
	NULL,				/* ctloutput */
	NULL,				/* ousrreq() */
	ng_btsocket_l2cap_raw_init,	/* init */
	NULL, NULL, NULL,		/* fasttimeo, slowtimo, drain */
	&ng_btsocket_l2cap_raw_usrreqs,	/* usrreq table (above) */
	/* { NULL } */			/* pfh (protocol filter head?) */
},
{
	SOCK_SEQPACKET,			/* protocol type */
	&ng_btsocket_domain,		/* backpointer to domain */
	BLUETOOTH_PROTO_L2CAP,		/* protocol */
	PR_ATOMIC | PR_CONNREQUIRED,	/* flags */
	NULL, NULL, NULL,		/* input, output, ctlinput */
	ng_btsocket_l2cap_ctloutput,	/* ctloutput */
	NULL,				/* ousrreq() */
	ng_btsocket_l2cap_init,		/* init */
	NULL, NULL, NULL,		/* fasttimeo, slowtimo, drain */
	&ng_btsocket_l2cap_usrreqs,	/* usrreq table (above) */
	/* { NULL } */			/* pfh (protocol filter head?) */
},
{
	SOCK_STREAM,			/* protocol type */
	&ng_btsocket_domain,		/* backpointer to domain */
	BLUETOOTH_PROTO_RFCOMM,		/* protocol */
	PR_ATOMIC | PR_CONNREQUIRED,	/* flags */
	NULL, NULL, NULL,		/* input, output, ctlinput */
	ng_btsocket_rfcomm_ctloutput,	/* ctloutput */
	NULL,				/* ousrreq() */
	ng_btsocket_rfcomm_init,	/* init */
	NULL, NULL, NULL,		/* fasttimeo, slowtimo, drain */
	&ng_btsocket_rfcomm_usrreqs,	/* usrreq table (above) */
	/* { NULL } */			/* pfh (protocol filter head?) */
}
};
#define ng_btsocket_protosw_size \
	(sizeof(ng_btsocket_protosw)/sizeof(ng_btsocket_protosw[0]))
#define ng_btsocket_protosw_end \
	&ng_btsocket_protosw[ng_btsocket_protosw_size]

/*
 * BLUETOOTH domain
 */

struct domain			ng_btsocket_domain = {
	AF_BLUETOOTH,			/* family */
	"bluetooth",			/* domain name */
	NULL,				/* init() */
	NULL,				/* externalize() */
	NULL,				/* dispose() */
	ng_btsocket_protosw,		/* protosw entry */
	ng_btsocket_protosw_end,	/* end of protosw entries */
	NULL,				/* next domain in list */
	NULL,				/* rtattach() */
	0,				/* arg to rtattach in bits */
	0				/* maxrtkey */
};

/* 
 * Socket sysctl tree 
 */

SYSCTL_NODE(_net_bluetooth_hci, OID_AUTO, sockets, CTLFLAG_RW,
	0, "Bluetooth HCI sockets family");
SYSCTL_NODE(_net_bluetooth_l2cap, OID_AUTO, sockets, CTLFLAG_RW,
	0, "Bluetooth L2CAP sockets family");
SYSCTL_NODE(_net_bluetooth_rfcomm, OID_AUTO, sockets, CTLFLAG_RW,
	0, "Bluetooth RFCOMM sockets family");

/* 
 * Module 
 */

static moduledata_t	ng_btsocket_mod = {
	"ng_btsocket",
	ng_btsocket_modevent,
	NULL
};

DECLARE_MODULE(ng_btsocket, ng_btsocket_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
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
		net_add_domain(&ng_btsocket_domain);
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

