/*
 * ng_h4.c
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
 * $Id: ng_h4.c,v 1.25 2002/11/03 02:17:31 max Exp $
 * $FreeBSD$
 * 
 * Based on:
 * ---------
 *
 * FreeBSD: src/sys/netgraph/ng_tty.c
 * Author: Archie Cobbs <archie@freebsd.org>
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/tty.h>
#include <sys/ttycom.h>
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <ng_bluetooth.h>
#include <ng_hci.h>
#include "ng_h4.h"
#include "ng_h4_var.h"
#include "ng_h4_prse.h"

/*****************************************************************************
 *****************************************************************************
 ** This node implements a Bluetooth HCI UART transport layer as per chapter
 ** H4 of the Bluetooth Specification Book v1.1. It is a terminal line 
 ** discipline that is also a netgraph node. Installing this line discipline 
 ** on a terminal device instantiates a new netgraph node of this type, which 
 ** allows access to the device via the "hook" hook of the node.
 **
 ** Once the line discipline is installed, you can find out the name of the 
 ** corresponding netgraph node via a NGIOCGINFO ioctl().
 *****************************************************************************
 *****************************************************************************/

/* MALLOC define */
#ifndef NG_SEPARATE_MALLOC
MALLOC_DEFINE(M_NETGRAPH_H4, "netgraph_h4", "Netgraph Bluetooth H4 node");
#else
#define M_NETGRAPH_H4 M_NETGRAPH
#endif /* NG_SEPARATE_MALLOC */

/* Line discipline methods */
static int	ng_h4_open	(dev_t, struct tty *);
static int	ng_h4_close	(struct tty *, int);
static int	ng_h4_read	(struct tty *, struct uio *, int);
static int	ng_h4_write	(struct tty *, struct uio *, int);
static int	ng_h4_input	(int, struct tty *);
static int	ng_h4_start	(struct tty *);
static void	ng_h4_start2	(node_p, hook_p, void *, int);
static int	ng_h4_ioctl	(struct tty *, u_long, caddr_t, 
					int, struct thread *);

/* Line discipline descriptor */
static struct linesw		ng_h4_disc = {
	ng_h4_open,		/* open */
	ng_h4_close,		/* close */
	ng_h4_read,		/* read */
	ng_h4_write,		/* write */
	ng_h4_ioctl,		/* ioctl */
	ng_h4_input,		/* input */
	ng_h4_start,		/* start */
	ttymodem,		/* modem */
	0 			/* hotchar (don't really care which one) */
};

/* Netgraph methods */
static ng_constructor_t		ng_h4_constructor;
static ng_rcvmsg_t		ng_h4_rcvmsg;
static ng_shutdown_t		ng_h4_shutdown;
static ng_newhook_t		ng_h4_newhook;
static ng_connect_t		ng_h4_connect;
static ng_rcvdata_t		ng_h4_rcvdata;
static ng_disconnect_t		ng_h4_disconnect;

/* Other stuff */
static void	ng_h4_timeout		(node_p);
static void	ng_h4_untimeout		(node_p);
static void	ng_h4_queue_timeout	(void *);
static void	ng_h4_process_timeout	(node_p, hook_p, void *, int);
static int	ng_h4_mod_event		(module_t, int, void *);

/* Netgraph node type descriptor */
static struct ng_type		typestruct = {
	NG_ABI_VERSION,
	NG_H4_NODE_TYPE,	/* typename */
	ng_h4_mod_event,	/* modevent */
	ng_h4_constructor,	/* constructor */
	ng_h4_rcvmsg,		/* control message */
	ng_h4_shutdown,		/* destructor */
	ng_h4_newhook,		/* new hook */
	NULL,			/* find hook */
	ng_h4_connect,		/* connect hook */
	ng_h4_rcvdata,		/* data */
	ng_h4_disconnect,	/* disconnect hook */
	ng_h4_cmdlist		/* node command list */
};
NETGRAPH_INIT(h4, &typestruct);
MODULE_VERSION(ng_h4, NG_BLUETOOTH_VERSION);

static int	ng_h4_node = 0;

/*****************************************************************************
 *****************************************************************************
 **			    Line discipline methods
 *****************************************************************************
 *****************************************************************************/

/*
 * Set our line discipline on the tty.
 */

static int
ng_h4_open(dev_t dev, struct tty *tp)
{
	char		 name[NG_NODELEN + 1];
	ng_h4_info_p	 sc = NULL;
	int		 s, error;

	/* Super-user only */
	error = suser(curthread); /* XXX */
	if (error != 0)
		return (error);

	s = splnet(); /* XXX */
	spltty(); /* XXX */

	/* Already installed? */
	if (tp->t_line == H4DISC) {
		sc = (ng_h4_info_p) tp->t_sc;
		if (sc != NULL && sc->tp == tp)
			goto out;
	}

	/* Initialize private struct */
	MALLOC(sc, ng_h4_info_p, sizeof(*sc), M_NETGRAPH_H4, M_WAITOK | M_ZERO);
	if (sc == NULL) {
		error = ENOMEM;
		goto out;
	}

	sc->tp = tp;
	sc->debug = NG_H4_WARN_LEVEL;

	sc->state = NG_H4_W4_PKT_IND;
	sc->want = 1;
	sc->got = 0;

	NG_BT_MBUFQ_INIT(&sc->outq, NG_H4_DEFAULTQLEN);
	callout_handle_init(&sc->timo);

	/* Setup netgraph node */
	error = ng_make_node_common(&typestruct, &sc->node);
	if (error != 0) {
		bzero(sc, sizeof(*sc));
		FREE(sc, M_NETGRAPH_H4);
		goto out;
	}

	/* Assign node its name */
	snprintf(name, sizeof(name), "%s%d", typestruct.name, ng_h4_node ++);

	error = ng_name_node(sc->node, name);
	if (error != 0) {
		NG_H4_ALERT("%s: %s - node name exists?\n", __func__, name);
		NG_NODE_UNREF(sc->node);
		bzero(sc, sizeof(*sc));
		FREE(sc, M_NETGRAPH_H4);
		goto out;
	}

	/* Set back pointers */
	NG_NODE_SET_PRIVATE(sc->node, sc);
	tp->t_sc = (caddr_t) sc;

	/* The node has to be a WRITER because data can change node status */
	NG_NODE_FORCE_WRITER(sc->node);

	/*
	 * Pre-allocate cblocks to the an appropriate amount.
	 * I'm not sure what is appropriate.
	 */

	ttyflush(tp, FREAD | FWRITE);
	clist_alloc_cblocks(&tp->t_canq, 0, 0);
	clist_alloc_cblocks(&tp->t_rawq, 0, 0);
	clist_alloc_cblocks(&tp->t_outq,
		MLEN + NG_H4_HIWATER, MLEN + NG_H4_HIWATER);
out:
	splx(s); /* XXX */

	return (error);
} /* ng_h4_open */

/*
 * Line specific close routine, called from device close routine
 * and from ttioctl. This causes the node to be destroyed as well.
 */

static int
ng_h4_close(struct tty *tp, int flag)
{
	ng_h4_info_p	sc = (ng_h4_info_p) tp->t_sc;
	int		s;

	s = spltty(); /* XXX */

	ttyflush(tp, FREAD | FWRITE);
	clist_free_cblocks(&tp->t_outq);
	tp->t_line = 0;
	if (sc != NULL) {
		tp->t_sc = NULL;

		if (sc->node != NULL) {
			if (sc->flags & NG_H4_TIMEOUT)
				ng_h4_untimeout(sc->node);

			NG_NODE_SET_PRIVATE(sc->node, NULL);
			ng_rmnode_self(sc->node);
			sc->node = NULL;
		}

		NG_BT_MBUFQ_DESTROY(&sc->outq);
		bzero(sc, sizeof(*sc));
		FREE(sc, M_NETGRAPH_H4);
	}

	splx(s); /* XXX */

	return (0);
} /* ng_h4_close */

/*
 * Once the device has been turned into a node, we don't allow reading.
 */

static int
ng_h4_read(struct tty *tp, struct uio *uio, int flag)
{
	return (EIO);
} /* ng_h4_read */

/*
 * Once the device has been turned into a node, we don't allow writing.
 */

static int
ng_h4_write(struct tty *tp, struct uio *uio, int flag)
{
	return (EIO);
} /* ng_h4_write */

/*
 * We implement the NGIOCGINFO ioctl() defined in ng_message.h.
 */

static int
ng_h4_ioctl(struct tty *tp, u_long cmd, caddr_t data, int flag,
		struct thread *td)
{
	ng_h4_info_p	sc = (ng_h4_info_p) tp->t_sc;
	int		s, error = 0;

	s = spltty(); /* XXX */

	switch (cmd) {
	case NGIOCGINFO:
#undef	NI
#define NI(x)	((struct nodeinfo *)(x))

		bzero(data, sizeof(*NI(data)));

		if (NG_NODE_HAS_NAME(sc->node))
			strncpy(NI(data)->name, NG_NODE_NAME(sc->node), 
				sizeof(NI(data)->name) - 1);

		strncpy(NI(data)->type, sc->node->nd_type->name, 
			sizeof(NI(data)->type) - 1);

		NI(data)->id = (u_int32_t) ng_node2ID(sc->node);
		NI(data)->hooks = NG_NODE_NUMHOOKS(sc->node);
		break;

	default:
		error = ENOIOCTL;
		break;
	}

	splx(s); /* XXX */

	return (error);
} /* ng_h4_ioctl */

/*
 * Receive data coming from the device. We get one character at a time, which 
 * is kindof silly.
 */

static int
ng_h4_input(int c, struct tty *tp)
{
	ng_h4_info_p	sc = (ng_h4_info_p) tp->t_sc;

	if (sc == NULL || tp != sc->tp ||
	    sc->node == NULL || NG_NODE_NOT_VALID(sc->node))
		return (0);

	/* Check for error conditions */
	if ((sc->tp->t_state & TS_CONNECTED) == 0) {
		NG_H4_INFO("%s: %s - no carrier\n", __func__,
			NG_NODE_NAME(sc->node));

		sc->state = NG_H4_W4_PKT_IND;
		sc->want = 1;
		sc->got = 0;

		return (0); /* XXX Loss of synchronization here! */
	}

	/* Check for framing error or overrun on this char */
	if (c & TTY_ERRORMASK) {
		NG_H4_ERR("%s: %s - line error %#x, c=%#x\n", __func__, 
			NG_NODE_NAME(sc->node), c & TTY_ERRORMASK,
			c & TTY_CHARMASK);

		NG_H4_STAT_IERROR(sc->stat);

		sc->state = NG_H4_W4_PKT_IND;
		sc->want = 1;
		sc->got = 0;

		return (0); /* XXX Loss of synchronization here! */
	}

	NG_H4_STAT_BYTES_RECV(sc->stat, 1);

	/* Append char to mbuf */
	if (sc->got >= sizeof(sc->ibuf)) {
		NG_H4_ALERT("%s: %s - input buffer overflow, c=%#x, got=%d\n",
			__func__, NG_NODE_NAME(sc->node), c & TTY_CHARMASK,
			sc->got);

		NG_H4_STAT_IERROR(sc->stat);

		sc->state = NG_H4_W4_PKT_IND;
		sc->want = 1;
		sc->got = 0;

		return (0); /* XXX Loss of synchronization here! */
	}

	sc->ibuf[sc->got ++] = (c & TTY_CHARMASK);

	NG_H4_INFO("%s: %s - got char %#x, want=%d, got=%d\n", __func__,
		NG_NODE_NAME(sc->node), c, sc->want, sc->got);

	if (sc->got < sc->want)
		return (0); /* Wait for more */

	switch (sc->state) {
	/* Got packet indicator */
	case NG_H4_W4_PKT_IND:
		NG_H4_INFO("%s: %s - got packet indicator %#x\n", __func__,
			NG_NODE_NAME(sc->node), sc->ibuf[0]);

		sc->state = NG_H4_W4_PKT_HDR;

		/*
		 * Since packet indicator included in the packet header
		 * just set sc->want to sizeof(packet header).
		 */

		switch (sc->ibuf[0]) {
		case NG_HCI_ACL_DATA_PKT:
			sc->want = sizeof(ng_hci_acldata_pkt_t);
			break;

		case NG_HCI_SCO_DATA_PKT:
			sc->want = sizeof(ng_hci_scodata_pkt_t);
			break;

		case NG_HCI_EVENT_PKT:
			sc->want = sizeof(ng_hci_event_pkt_t);
			break;

		default:
			NG_H4_WARN("%s: %s - ignoring unknown packet " \
				"type=%#x\n", __func__, NG_NODE_NAME(sc->node),
				sc->ibuf[0]);

			NG_H4_STAT_IERROR(sc->stat);

			sc->state = NG_H4_W4_PKT_IND;
			sc->want = 1;
			sc->got = 0;
			break;
		}
		break;

	/* Got packet header */
	case NG_H4_W4_PKT_HDR:
		sc->state = NG_H4_W4_PKT_DATA;

		switch (sc->ibuf[0]) {
		case NG_HCI_ACL_DATA_PKT:
			c = le16toh(((ng_hci_acldata_pkt_t *)
				(sc->ibuf))->length);
			break;

		case NG_HCI_SCO_DATA_PKT:
			c = ((ng_hci_scodata_pkt_t *)(sc->ibuf))->length;
			break;

		case NG_HCI_EVENT_PKT:
			c = ((ng_hci_event_pkt_t *)(sc->ibuf))->length;
			break;

		default:
			KASSERT((0), ("Invalid packet type=%#x\n",
				sc->ibuf[0]));
			break;
		}

		NG_H4_INFO("%s: %s - got packet header, packet type=%#x, " \
			"packet size=%d, payload size=%d\n", __func__, 
			NG_NODE_NAME(sc->node), sc->ibuf[0], sc->got, c);

		if (c > 0) {
			sc->want += c;

			/* 
			 * Try to prevent possible buffer overrun
			 *
			 * XXX I'm *really* confused here. It turns out
			 * that Xircom card sends us packets with length
			 * greater then 512 bytes! This is greater then
			 * our old receive buffer (ibuf) size. In the same
			 * time the card demands from us *not* to send 
			 * packets greater then 192 bytes. Weird! How the 
			 * hell i should know how big *receive* buffer 
			 * should be? For now increase receiving buffer 
			 * size to 1K and add the following check.
			 */

			if (sc->want >= sizeof(sc->ibuf)) {
				int	b;

				NG_H4_ALERT("%s: %s - packet too big for " \
					"buffer, type=%#x, got=%d, want=%d, " \
					"length=%d\n", __func__, 
					NG_NODE_NAME(sc->node), sc->ibuf[0],
					sc->got, sc->want, c);

				NG_H4_ALERT("Packet header:\n");
				for (b = 0; b < sc->got; b++)
					NG_H4_ALERT("%#x ", sc->ibuf[b]);
				NG_H4_ALERT("\n");

				/* Reset state */
				NG_H4_STAT_IERROR(sc->stat);

				sc->state = NG_H4_W4_PKT_IND;
				sc->want = 1;
				sc->got = 0;
			}

			break;
		}

		/* else FALLTHROUGH and deliver frame */
		/* XXX Is this true? Should we deliver empty frame? */

	/* Got packet data */
	case NG_H4_W4_PKT_DATA:
		NG_H4_INFO("%s: %s - got full packet, packet type=%#x, " \
			"packet size=%d\n", __func__,
			NG_NODE_NAME(sc->node), sc->ibuf[0], sc->got);

		if (sc->hook != NULL && NG_HOOK_IS_VALID(sc->hook)) {
			struct mbuf	*m = NULL;

			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m != NULL) {
				m->m_pkthdr.len = 0;

				/* XXX m_copyback() is stupid */
				m->m_len = min(MHLEN, sc->got);

				m_copyback(m, 0, sc->got, sc->ibuf);
				NG_SEND_DATA_ONLY(c, sc->hook, m);
			} else {
				NG_H4_ERR("%s: %s - could not get mbuf\n",
					__func__, NG_NODE_NAME(sc->node));

				NG_H4_STAT_IERROR(sc->stat);
			}
		}

		sc->state = NG_H4_W4_PKT_IND;
		sc->want = 1;
		sc->got = 0;

		NG_H4_STAT_PCKTS_RECV(sc->stat);
		break;

	default:
		KASSERT((0), ("Invalid H4 node state=%d", sc->state));
		break;
	}

	return (0);
} /* ng_h4_input */

/*
 * This is called when the device driver is ready for more output. Called from 
 * tty system. 
 */

static int
ng_h4_start(struct tty *tp)
{
	ng_h4_info_p	sc = (ng_h4_info_p) tp->t_sc;

	if (sc == NULL || tp != sc->tp || 
	    sc->node == NULL || NG_NODE_NOT_VALID(sc->node))
		return (0);

	return (ng_send_fn(sc->node, NULL, ng_h4_start2, NULL, 0));
} /* ng_h4_start */

/*
 * Device driver is ready for more output. Part 2. Called (via ng_send_fn) 
 * ng_h4_start() and from ng_h4_rcvdata() when a new mbuf is available for 
 * output.
 */

static void
ng_h4_start2(node_p node, hook_p hook, void *arg1, int arg2)
{
	ng_h4_info_p	 sc = (ng_h4_info_p) NG_NODE_PRIVATE(node);
	struct mbuf	*m = NULL;
	int		 s, size;

	s = spltty(); /* XXX */

#if 0
	while (sc->tp->t_outq.c_cc < NG_H4_HIWATER) { /* XXX 2.2 specific ? */
#else
	while (1) {
#endif
		/* Remove first mbuf from queue */
		NG_BT_MBUFQ_DEQUEUE(&sc->outq, m);
		if (m == NULL)
			break;

		/* Send as much of it as possible */
		while (m != NULL) {
			size = m->m_len - b_to_q(mtod(m, u_char *),
					m->m_len, &sc->tp->t_outq);

			NG_H4_STAT_BYTES_SENT(sc->stat, size);

			m->m_data += size;
			m->m_len -= size;
			if (m->m_len > 0)
				break;	/* device can't take no more */

			m = m_free(m);
		}

		/* Put remainder of mbuf chain (if any) back on queue */
		if (m != NULL) {
			NG_BT_MBUFQ_PREPEND(&sc->outq, m);
			break;
		}

		/* Full packet has been sent */
		NG_H4_STAT_PCKTS_SENT(sc->stat);
	}

	/* 
	 * Call output process whether or not there is any output. We are
	 * being called in lieu of ttstart and must do what it would.
	 */

	if (sc->tp->t_oproc != NULL)
		(*sc->tp->t_oproc)(sc->tp);

	/*
	 * This timeout is needed for operation on a pseudo-tty, because the
	 * pty code doesn't call pppstart after it has drained the t_outq.
	 */

	if (NG_BT_MBUFQ_LEN(&sc->outq) > 0 && (sc->flags & NG_H4_TIMEOUT) == 0)
		ng_h4_timeout(node);

	splx(s); /* XXX */
} /* ng_h4_start2 */

/*****************************************************************************
 *****************************************************************************
 **			    Netgraph node methods
 *****************************************************************************
 *****************************************************************************/

/*
 * Initialize a new node of this type. We only allow nodes to be created as 
 * a result of setting the line discipline on a tty, so always return an error
 * if not.
 */

static int
ng_h4_constructor(node_p node)
{
	return (EOPNOTSUPP);
} /* ng_h4_constructor */

/*
 * Add a new hook. There can only be one.
 */

static int
ng_h4_newhook(node_p node, hook_p hook, const char *name)
{
	ng_h4_info_p	sc = (ng_h4_info_p) NG_NODE_PRIVATE(node);

	if (strcmp(name, NG_H4_HOOK) != 0)
		return (EINVAL);

	if (sc->hook != NULL)
		return (EISCONN);

	sc->hook = hook;

	return (0);
} /* ng_h4_newhook */

/*
 * Connect hook. Just say yes.
 */

static int
ng_h4_connect(hook_p hook)
{
	ng_h4_info_p	sc = (ng_h4_info_p) NG_NODE_PRIVATE(NG_HOOK_NODE(hook));

	if (hook != sc->hook) {
		sc->hook = NULL;
		return (EINVAL);
	}

	NG_HOOK_FORCE_QUEUE(NG_HOOK_PEER(hook));

	return (0);
} /* ng_h4_connect */

/*
 * Disconnect the hook
 */

static int
ng_h4_disconnect(hook_p hook)
{
	ng_h4_info_p	sc = (ng_h4_info_p) NG_NODE_PRIVATE(NG_HOOK_NODE(hook));

	/*
	 * We need to check for sc != NULL because we can be called from
	 * ng_h4_clsoe() via ng_rmnode_self()
	 */

	if (sc != NULL) {
		if (hook != sc->hook)
			return (EINVAL);

		/* XXX do we have to untimeout and drain out queue? */
		if (sc->flags & NG_H4_TIMEOUT)
			ng_h4_untimeout(NG_HOOK_NODE(hook));

		NG_BT_MBUFQ_DRAIN(&sc->outq); 
		sc->state = NG_H4_W4_PKT_IND;
		sc->want = 1;
		sc->got = 0;

		sc->hook = NULL;
	}

	return (0);
} /* ng_h4_disconnect */

/*
 * Remove this node. The does the netgraph portion of the shutdown.
 * This should only be called indirectly from ng_h4_close().
 */

static int
ng_h4_shutdown(node_p node)
{
	ng_h4_info_p	sc = (ng_h4_info_p) NG_NODE_PRIVATE(node);
	char		name[NG_NODELEN + 1];

	/* Let old node go */
	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);

	/* Check if device was closed */
	if (sc == NULL)
		goto out;

	/* Setup new netgraph node */
	if (ng_make_node_common(&typestruct, &sc->node) != 0) {
		printf("%s: Unable to create new node!\n", __func__);
		sc->node = NULL;
		goto out;
	}

	/* Assign node its name */
	snprintf(name, sizeof(name), "%s%d", typestruct.name, ng_h4_node ++);

	if (ng_name_node(sc->node, name) != 0) {
		printf("%s: %s - node name exists?\n", __func__, name);
		NG_NODE_UNREF(sc->node);
		sc->node = NULL;
		goto out;
	}

	/* The node has to be a WRITER because data can change node status */
	NG_NODE_FORCE_WRITER(sc->node);
	NG_NODE_SET_PRIVATE(sc->node, sc);
out:
	return (0);
} /* ng_h4_shutdown */

/*
 * Receive incoming data from Netgraph system. Put it on our
 * output queue and start output if necessary.
 */

static int
ng_h4_rcvdata(hook_p hook, item_p item)
{
	ng_h4_info_p	 sc = (ng_h4_info_p)NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	int		 error = 0;
	struct mbuf	*m = NULL;

	if (sc == NULL) {
		error = EHOSTDOWN;
		goto out;
	}

	if (hook != sc->hook) {
		error = EINVAL;
		goto out;
	}

	NGI_GET_M(item, m);

	if (NG_BT_MBUFQ_FULL(&sc->outq)) {
		NG_H4_ERR("%s: %s - dropping mbuf, len=%d\n", __func__,
			NG_NODE_NAME(sc->node), m->m_pkthdr.len);

		NG_BT_MBUFQ_DROP(&sc->outq);
		NG_H4_STAT_OERROR(sc->stat);

		NG_FREE_M(m);
		error = ENOBUFS;
	} else {
		NG_H4_INFO("%s: %s - queue mbuf, len=%d\n", __func__,
			NG_NODE_NAME(sc->node), m->m_pkthdr.len);

		NG_BT_MBUFQ_ENQUEUE(&sc->outq, m);

		/*
		 * We have lock on the node, so we can call ng_h4_start2()
		 * directly
		 */

		ng_h4_start2(sc->node, NULL, NULL, 0);
	}
out:
	NG_FREE_ITEM(item);

	return (error);
} /* ng_h4_rcvdata */

/*
 * Receive control message
 */

static int
ng_h4_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	ng_h4_info_p	 sc = (ng_h4_info_p) NG_NODE_PRIVATE(node);
	struct ng_mesg	*msg = NULL, *resp = NULL;
	int		 error = 0;

	if (sc == NULL) {
		error = EHOSTDOWN;
		goto out;
	}

	NGI_GET_MSG(item, msg);

	switch (msg->header.typecookie) {
	case NGM_GENERIC_COOKIE:
		switch (msg->header.cmd) {
		case NGM_TEXT_STATUS:
			NG_MKRESPONSE(resp, msg, NG_TEXTRESPONSE, M_NOWAIT);
			if (resp == NULL)
				error = ENOMEM;
			else
				snprintf(resp->data, NG_TEXTRESPONSE,
					"Hook: %s\n"   \
					"Flags: %#x\n" \
					"Debug: %d\n"  \
					"State: %d\n"  \
					"Queue: [have:%d,max:%d]\n" \
					"Input: [got:%d,want:%d]",
					(sc->hook != NULL)? NG_H4_HOOK : "",
					sc->flags,
					sc->debug,
					sc->state,
					NG_BT_MBUFQ_LEN(&sc->outq),
					sc->outq.maxlen,
					sc->got,
					sc->want);
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	case NGM_H4_COOKIE:
		switch (msg->header.cmd) {
		case NGM_H4_NODE_RESET:
			NG_BT_MBUFQ_DRAIN(&sc->outq); 
			sc->state = NG_H4_W4_PKT_IND;
			sc->want = 1;
			sc->got = 0;
			break;

		case NGM_H4_NODE_GET_STATE:
			NG_MKRESPONSE(resp, msg, sizeof(ng_h4_node_state_ep),
				M_NOWAIT);
			if (resp == NULL)
				error = ENOMEM;
			else
				*((ng_h4_node_state_ep *)(resp->data)) = 
					sc->state;
			break;

		case NGM_H4_NODE_GET_DEBUG:
			NG_MKRESPONSE(resp, msg, sizeof(ng_h4_node_debug_ep),
				M_NOWAIT);
			if (resp == NULL)
				error = ENOMEM;
			else
				*((ng_h4_node_debug_ep *)(resp->data)) = 
					sc->debug;
			break;

		case NGM_H4_NODE_SET_DEBUG:
			if (msg->header.arglen != sizeof(ng_h4_node_debug_ep))
				error = EMSGSIZE;
			else
				sc->debug =
					*((ng_h4_node_debug_ep *)(msg->data));
			break;

		case NGM_H4_NODE_GET_QLEN:
			NG_MKRESPONSE(resp, msg, sizeof(ng_h4_node_qlen_ep),
				M_NOWAIT);
			if (resp == NULL)
				error = ENOMEM;
			else
				*((ng_h4_node_qlen_ep *)(resp->data)) = 
					sc->outq.maxlen;
			break;

		case NGM_H4_NODE_SET_QLEN:
			if (msg->header.arglen != sizeof(ng_h4_node_qlen_ep))
				error = EMSGSIZE;
			else if (*((ng_h4_node_qlen_ep *)(msg->data)) <= 0)
				error = EINVAL;
			else
				sc->outq.maxlen =
					*((ng_h4_node_qlen_ep *)(msg->data));
			break;

		case NGM_H4_NODE_GET_STAT:
			NG_MKRESPONSE(resp, msg, sizeof(ng_h4_node_stat_ep),
				M_NOWAIT);
			if (resp == NULL)
				error = ENOMEM;
			else
				bcopy(&sc->stat, resp->data,
					sizeof(ng_h4_node_stat_ep));
			break;

		case NGM_H4_NODE_RESET_STAT:
			NG_H4_STAT_RESET(sc->stat);
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	default:
		error = EINVAL;
		break;
	}
out:
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);

	return (error);
} /* ng_h4_rcvmsg */

/*
 * Set timeout
 */

static void
ng_h4_timeout(node_p node)
{
	ng_h4_info_p	sc = (ng_h4_info_p) NG_NODE_PRIVATE(node);

	NG_NODE_REF(node);
	sc->timo = timeout(ng_h4_queue_timeout, node, 1);
	sc->flags |= NG_H4_TIMEOUT;
} /* ng_h4_timeout */

/*
 * Unset timeout
 */

static void
ng_h4_untimeout(node_p node)
{
	ng_h4_info_p	sc = (ng_h4_info_p) NG_NODE_PRIVATE(node);

	sc->flags &= ~NG_H4_TIMEOUT;
	untimeout(ng_h4_queue_timeout, node, sc->timo);
	NG_NODE_UNREF(node);
} /* ng_h4_untimeout */

/*
 * OK, timeout has happend, so queue function to process it
 */

static void
ng_h4_queue_timeout(void *context)
{
	node_p	node = (node_p) context;

	if (NG_NODE_IS_VALID(node))
		ng_send_fn(node, NULL, &ng_h4_process_timeout, NULL, 0);

	NG_NODE_UNREF(node);
} /* ng_h4_queue_timeout */

/*
 * Timeout processing function.
 * We still have data to output to the device, so try sending more.
 */

static void
ng_h4_process_timeout(node_p node, hook_p hook, void *arg1, int arg2)
{
	ng_h4_info_p	sc = (ng_h4_info_p) NG_NODE_PRIVATE(node);

	sc->flags &= ~NG_H4_TIMEOUT;

	/*
	 * We can call ng_h4_start2() directly here because we have lock
	 * on the node.
	 */

	ng_h4_start2(node, NULL, NULL, 0);
} /* ng_h4_process_timeout */

/*
 * Handle loading and unloading for this node type
 */

static int
ng_h4_mod_event(module_t mod, int event, void *data)
{
	static int	ng_h4_ldisc;
	int		s, error = 0;

	s = spltty(); /* XXX */

	switch (event) {
	case MOD_LOAD:
		/* Register line discipline */
		ng_h4_ldisc = ldisc_register(H4DISC, &ng_h4_disc);
		if (ng_h4_ldisc < 0) {
			printf("%s: can't register H4 line discipline\n",
				__func__);
			error = EIO;
		}
		break;

	case MOD_UNLOAD:
		/* Unregister line discipline */
		ldisc_deregister(ng_h4_ldisc);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	splx(s); /* XXX */

	return (error);
} /* ng_h4_mod_event */

