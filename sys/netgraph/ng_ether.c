
/*
 * ng_ether.c
 *
 * Copyright (c) 1996-2000 Whistle Communications, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Authors: Archie Cobbs <archie@freebsd.org>
 *	    Julian Elischer <julian@freebsd.org>
 *
 * $FreeBSD$
 */

/*
 * ng_ether(4) netgraph node type
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/if_var.h>
#include <net/ethernet.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_ether.h>

#define IFP2AC(IFP)  ((struct arpcom *)IFP)
#define IFP2NG(ifp)  ((struct ng_node *)((struct arpcom *)(ifp))->ac_netgraph)

/* Per-node private data */
struct private {
	struct ifnet	*ifp;		/* associated interface */
	hook_p		upper;		/* upper hook connection */
	hook_p		lower;		/* lower OR orphan hook connection */
	u_char		lowerOrphan;	/* whether lower is lower or orphan */
	u_char		autoSrcAddr;	/* always overwrite source address */
	u_char		promisc;	/* promiscuous mode enabled */
};
typedef struct private *priv_p;

/* Functional hooks called from if_ethersubr.c */
static void	ng_ether_input(struct ifnet *ifp,
		    struct mbuf **mp, struct ether_header *eh);
static void	ng_ether_input_orphan(struct ifnet *ifp,
		    struct mbuf *m, struct ether_header *eh);
static int	ng_ether_output(struct ifnet *ifp, struct mbuf **mp);
static void	ng_ether_attach(struct ifnet *ifp);
static void	ng_ether_detach(struct ifnet *ifp); 

/* Other functions */
static void	ng_ether_input2(node_p node,
		    struct mbuf **mp, struct ether_header *eh);
static int	ng_ether_glueback_header(struct mbuf **mp,
			struct ether_header *eh);
static int	ng_ether_rcv_lower(node_p node, struct mbuf *m, meta_p meta);
static int	ng_ether_rcv_upper(node_p node, struct mbuf *m, meta_p meta);

/* Netgraph node methods */
static ng_constructor_t	ng_ether_constructor;
static ng_rcvmsg_t	ng_ether_rcvmsg;
static ng_shutdown_t	ng_ether_rmnode;
static ng_newhook_t	ng_ether_newhook;
static ng_rcvdata_t	ng_ether_rcvdata;
static ng_disconnect_t	ng_ether_disconnect;
static int		ng_ether_mod_event(module_t mod, int event, void *data);

/* Parse type for an Ethernet address */
static ng_parse_t	ng_enaddr_parse;
static ng_unparse_t	ng_enaddr_unparse;
const struct ng_parse_type ng_ether_enaddr_type = {
	NULL,
	NULL,
	NULL,
	ng_enaddr_parse,
	ng_enaddr_unparse,
	NULL,			/* no such thing as a "default" EN address */
	0
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_ether_cmdlist[] = {
	{
	  NGM_ETHER_COOKIE,
	  NGM_ETHER_GET_IFNAME,
	  "getifname",
	  NULL,
	  &ng_parse_string_type
	},
	{
	  NGM_ETHER_COOKIE,
	  NGM_ETHER_GET_IFINDEX,
	  "getifindex",
	  NULL,
	  &ng_parse_int32_type
	},
	{
	  NGM_ETHER_COOKIE,
	  NGM_ETHER_GET_ENADDR,
	  "getenaddr",
	  NULL,
	  &ng_ether_enaddr_type
	},
	{
	  NGM_ETHER_COOKIE,
	  NGM_ETHER_SET_ENADDR,
	  "setenaddr",
	  &ng_ether_enaddr_type,
	  NULL
	},
	{
	  NGM_ETHER_COOKIE,
	  NGM_ETHER_GET_PROMISC,
	  "getpromisc",
	  NULL,
	  &ng_parse_int32_type
	},
	{
	  NGM_ETHER_COOKIE,
	  NGM_ETHER_SET_PROMISC,
	  "setpromisc",
	  &ng_parse_int32_type,
	  NULL
	},
	{
	  NGM_ETHER_COOKIE,
	  NGM_ETHER_GET_AUTOSRC,
	  "getautosrc",
	  NULL,
	  &ng_parse_int32_type
	},
	{
	  NGM_ETHER_COOKIE,
	  NGM_ETHER_SET_AUTOSRC,
	  "setautosrc",
	  &ng_parse_int32_type,
	  NULL
	},
	{ 0 }
};

static struct ng_type ng_ether_typestruct = {
	NG_VERSION,
	NG_ETHER_NODE_TYPE,
	ng_ether_mod_event,
	ng_ether_constructor,
	ng_ether_rcvmsg,
	ng_ether_rmnode,
	ng_ether_newhook,
	NULL,
	NULL,
	ng_ether_rcvdata,
	ng_ether_rcvdata,
	ng_ether_disconnect,
	ng_ether_cmdlist,
};
NETGRAPH_INIT(ether, &ng_ether_typestruct);

/******************************************************************
		    ETHERNET FUNCTION HOOKS
******************************************************************/

/*
 * Handle a packet that has come in on an interface. We get to
 * look at it here before any upper layer protocols do.
 *
 * NOTE: this function will get called at splimp()
 */
static void
ng_ether_input(struct ifnet *ifp,
	struct mbuf **mp, struct ether_header *eh)
{
	const node_p node = IFP2NG(ifp);
	const priv_p priv = node->private;

	/* If "lower" hook not connected, let packet continue */
	if (priv->lower == NULL || priv->lowerOrphan)
		return;
	ng_ether_input2(node, mp, eh);
}

/*
 * Handle a packet that has come in on an interface, and which
 * does not match any of our known protocols (an ``orphan'').
 *
 * NOTE: this function will get called at splimp()
 */
static void
ng_ether_input_orphan(struct ifnet *ifp,
	struct mbuf *m, struct ether_header *eh)
{
	const node_p node = IFP2NG(ifp);
	const priv_p priv = node->private;

	/* If "orphan" hook not connected, let packet continue */
	if (priv->lower == NULL || !priv->lowerOrphan) {
		m_freem(m);
		return;
	}
	ng_ether_input2(node, &m, eh);
	if (m != NULL)
		m_freem(m);
}

/*
 * Handle a packet that has come in on an interface.
 * The Ethernet header has already been detached from the mbuf,
 * so we have to put it back.
 *
 * NOTE: this function will get called at splimp()
 */
static void
ng_ether_input2(node_p node, struct mbuf **mp, struct ether_header *eh)
{
	const priv_p priv = node->private;
	meta_p meta = NULL;
	int error;

	/* Glue Ethernet header back on */
	if ((error = ng_ether_glueback_header(mp, eh)) != 0)
		return;

	/* Send out lower/orphan hook */
	(void)ng_queue_data(priv->lower, *mp, meta);
	*mp = NULL;
}

/*
 * Handle a packet that is going out on an interface.
 * The Ethernet header is already attached to the mbuf.
 */
static int
ng_ether_output(struct ifnet *ifp, struct mbuf **mp)
{
	const node_p node = IFP2NG(ifp);
	const priv_p priv = node->private;
	meta_p meta = NULL;
	int error = 0;

	/* If "upper" hook not connected, let packet continue */
	if (priv->upper == NULL)
		return (0);

	/* Send it out "upper" hook */
	NG_SEND_DATA(error, priv->upper, *mp, meta);
	*mp = NULL;
	return (error);
}

/*
 * A new Ethernet interface has been attached.
 * Create a new node for it, etc.
 */
static void
ng_ether_attach(struct ifnet *ifp)
{
	char name[IFNAMSIZ + 1];
	priv_p priv;
	node_p node;

	/* Create node */
	KASSERT(!IFP2NG(ifp), ("%s: node already exists?", __FUNCTION__));
	snprintf(name, sizeof(name), "%s%d", ifp->if_name, ifp->if_unit);
	if (ng_make_node_common(&ng_ether_typestruct, &node) != 0) {
		log(LOG_ERR, "%s: can't %s for %s\n",
		    __FUNCTION__, "create node", name);
		return;
	}

	/* Allocate private data */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_NOWAIT);
	if (priv == NULL) {
		log(LOG_ERR, "%s: can't %s for %s\n",
		    __FUNCTION__, "allocate memory", name);
		ng_unref(node);
		return;
	}
	bzero(priv, sizeof(*priv));
	node->private = priv;
	priv->ifp = ifp;
	IFP2NG(ifp) = node;
	priv->autoSrcAddr = 1;

	/* Try to give the node the same name as the interface */
	if (ng_name_node(node, name) != 0) {
		log(LOG_WARNING, "%s: can't name node %s\n",
		    __FUNCTION__, name);
	}
}

/*
 * An Ethernet interface is being detached.
 * Destroy its node.
 */
static void
ng_ether_detach(struct ifnet *ifp)
{
	const node_p node = IFP2NG(ifp);
	priv_p priv;

	if (node == NULL)		/* no node (why not?), ignore */
		return;
	ng_rmnode(node);		/* break all links to other nodes */
	node->flags |= NG_INVALID;
	ng_unname(node);		/* free name (and its reference) */
	IFP2NG(ifp) = NULL;		/* detach node from interface */
	priv = node->private;		/* free node private info */
	bzero(priv, sizeof(*priv));
	FREE(priv, M_NETGRAPH);
	node->private = NULL;
	ng_unref(node);			/* free node itself */
}

/*
 * Optimization for gluing the Ethernet header back onto
 * the front of an incoming packet.
 */
static int
ng_ether_glueback_header(struct mbuf **mp, struct ether_header *eh)
{
	struct mbuf *m = *mp;
	uintfptr_t room;
	int error = 0;

	/*
	 * Possibly the header is already on the front.
	 * If this is the case so just move the markers back
	 * to re-include it. We lucked out.
	 * This allows us to avoid a yucky m_pullup
	 * in later nodes if it works.
	 */
	if (eh == mtod(m, struct ether_header *) - 1) {
		m->m_len += sizeof(*eh);
		m->m_data -= sizeof(*eh);
		m->m_pkthdr.len += sizeof(*eh);
		goto done;
	}

	/*
	 * Alternatively there may be room even though
	 * it is stored somewhere else. If so, copy it in.
	 * This only safe because we KNOW that this packet has
	 * just been generated by an ethernet card, so there are
	 * no aliases to the buffer (not so for outgoing packets).
	 * Nearly all ethernet cards will end up producing mbufs
	 * that fall into these cases. So we are not optimizing
	 * contorted cases.
	 */
	if ((m->m_flags & M_EXT) != 0) {
		room = mtod(m, caddr_t) - m->m_ext.ext_buf;
		if (room > m->m_ext.ext_size)	/* garbage, fail immediately */
			room = 0;		
	} else
		room = mtod(m, caddr_t) - m->m_pktdat;

	/*
	 * If we have room, just copy it and adjust
	 */
	if (room >= sizeof(*eh)) {
		m->m_len += sizeof(*eh);
		m->m_data -= sizeof(*eh);
		m->m_pkthdr.len += sizeof(*eh);
		goto copy;
	}

	/*
	 * Doing anything more is likely to get more
	 * expensive than it's worth..
	 * it's probable that everything else is in one
	 * big lump. The next node will do an m_pullup()
	 * for exactly the amount of data it needs and
	 * hopefully everything after that will not
	 * need one. So let's just use M_PREPEND.
	 */
	M_PREPEND(m, sizeof (*eh), M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto done;
	}

copy:
	/* Copy header and return (possibly new) mbuf */
	bcopy((caddr_t)eh, mtod(m, struct ether_header *), sizeof(*eh));
done:
	*mp = m;
	return error;
}

/******************************************************************
		    NETGRAPH NODE METHODS
******************************************************************/

/*
 * It is not possible or allowable to create a node of this type.
 * Nodes get created when the interface is attached (or, when
 * this node type's KLD is loaded).
 */
static int
ng_ether_constructor(node_p *nodep)
{
	return (EINVAL);
}

/*
 * Check for attaching a new hook.
 */
static	int
ng_ether_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = node->private;
	u_char orphan = priv->lowerOrphan;
	hook_p *hookptr;

	/* Divert hook is an alias for lower */
	if (strcmp(name, NG_ETHER_HOOK_DIVERT) == 0)
		name = NG_ETHER_HOOK_LOWER;

	/* Which hook? */
	if (strcmp(name, NG_ETHER_HOOK_UPPER) == 0)
		hookptr = &priv->upper;
	else if (strcmp(name, NG_ETHER_HOOK_LOWER) == 0) {
		hookptr = &priv->lower;
		orphan = 0;
	} else if (strcmp(name, NG_ETHER_HOOK_ORPHAN) == 0) {
		hookptr = &priv->lower;
		orphan = 1;
	} else
		return (EINVAL);

	/* Check if already connected (shouldn't be, but doesn't hurt) */
	if (*hookptr != NULL)
		return (EISCONN);

	/* OK */
	*hookptr = hook;
	priv->lowerOrphan = orphan;
	return (0);
}

/*
 * Receive an incoming control message.
 */
static int
ng_ether_rcvmsg(node_p node, struct ng_mesg *msg,
	const char *retaddr, struct ng_mesg **rptr)
{
	const priv_p priv = node->private;
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_ETHER_COOKIE:
		switch (msg->header.cmd) {
		case NGM_ETHER_GET_IFNAME:
			NG_MKRESPONSE(resp, msg, IFNAMSIZ + 1, M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			snprintf(resp->data, IFNAMSIZ + 1,
			    "%s%d", priv->ifp->if_name, priv->ifp->if_unit);
			break;
		case NGM_ETHER_GET_IFINDEX:
			NG_MKRESPONSE(resp, msg, sizeof(u_int32_t), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			*((u_int32_t *)resp->data) = priv->ifp->if_index;
			break;
		case NGM_ETHER_GET_ENADDR:
			NG_MKRESPONSE(resp, msg, ETHER_ADDR_LEN, M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy((IFP2AC(priv->ifp))->ac_enaddr,
			    resp->data, ETHER_ADDR_LEN);
			break;
		case NGM_ETHER_SET_ENADDR:
		    {
			if (msg->header.arglen != ETHER_ADDR_LEN) {
				error = EINVAL;
				break;
			}
			error = if_setlladdr(priv->ifp,
			    (u_char *)msg->data, ETHER_ADDR_LEN);
			break;
		    }
		case NGM_ETHER_GET_PROMISC:
			NG_MKRESPONSE(resp, msg, sizeof(u_int32_t), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			*((u_int32_t *)resp->data) = priv->promisc;
			break;
		case NGM_ETHER_SET_PROMISC:
		    {
			u_char want;

			if (msg->header.arglen != sizeof(u_int32_t)) {
				error = EINVAL;
				break;
			}
			want = !!*((u_int32_t *)msg->data);
			if (want ^ priv->promisc) {
				if ((error = ifpromisc(priv->ifp, want)) != 0)
					break;
				priv->promisc = want;
			}
			break;
		    }
		case NGM_ETHER_GET_AUTOSRC:
			NG_MKRESPONSE(resp, msg, sizeof(u_int32_t), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			*((u_int32_t *)resp->data) = priv->autoSrcAddr;
			break;
		case NGM_ETHER_SET_AUTOSRC:
			if (msg->header.arglen != sizeof(u_int32_t)) {
				error = EINVAL;
				break;
			}
			priv->autoSrcAddr = !!*((u_int32_t *)msg->data);
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
	if (rptr)
		*rptr = resp;
	else if (resp != NULL)
		FREE(resp, M_NETGRAPH);
	FREE(msg, M_NETGRAPH);
	return (error);
}

/*
 * Receive data on a hook.
 */
static int
ng_ether_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const node_p node = hook->node;
	const priv_p priv = node->private;

	if (hook == priv->lower)
		return ng_ether_rcv_lower(node, m, meta);
	if (hook == priv->upper)
		return ng_ether_rcv_upper(node, m, meta);
	panic("%s: weird hook", __FUNCTION__);
}

/*
 * Handle an mbuf received on the "lower" hook.
 */
static int
ng_ether_rcv_lower(node_p node, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;

	/* Make sure header is fully pulled up */
	if (m->m_pkthdr.len < sizeof(struct ether_header)) {
		NG_FREE_DATA(m, meta);
		return (EINVAL);
	}
	if (m->m_len < sizeof(struct ether_header)
	    && (m = m_pullup(m, sizeof(struct ether_header))) == NULL) {
		NG_FREE_META(meta);
		return (ENOBUFS);
	}

	/* Drop in the MAC address if desired */
	if (priv->autoSrcAddr) {
		bcopy((IFP2AC(priv->ifp))->ac_enaddr,
		    mtod(m, struct ether_header *)->ether_shost,
		    ETHER_ADDR_LEN);
	}

	/* Send it on its way */
	NG_FREE_META(meta);
	return ether_output_frame(priv->ifp, m);
}

/*
 * Handle an mbuf received on the "upper" hook.
 */
static int
ng_ether_rcv_upper(node_p node, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;
	struct ether_header *eh;

	/* Check length and pull off header */
	if (m->m_pkthdr.len < sizeof(*eh)) {
		NG_FREE_DATA(m, meta);
		return (EINVAL);
	}
	if (m->m_len < sizeof(*eh) && (m = m_pullup(m, sizeof(*eh))) == NULL) {
		NG_FREE_META(meta);
		return (ENOBUFS);
	}
	eh = mtod(m, struct ether_header *);
	m->m_data += sizeof(*eh);
	m->m_len -= sizeof(*eh);
	m->m_pkthdr.len -= sizeof(*eh);
	m->m_pkthdr.rcvif = priv->ifp;

	/* Route packet back in */
	NG_FREE_META(meta);
	ether_demux(priv->ifp, eh, m);
	return (0);
}

/*
 * Shutdown node. This resets the node but does not remove it.
 */
static int
ng_ether_rmnode(node_p node)
{
	const priv_p priv = node->private;

	ng_cutlinks(node);
	node->flags &= ~NG_INVALID;	/* bounce back to life */
	if (priv->promisc) {		/* disable promiscuous mode */
		(void)ifpromisc(priv->ifp, 0);
		priv->promisc = 0;
	}
	priv->autoSrcAddr = 1;		/* reset auto-src-addr flag */
	return (0);
}

/*
 * Hook disconnection.
 */
static int
ng_ether_disconnect(hook_p hook)
{
	const priv_p priv = hook->node->private;

	if (hook == priv->upper)
		priv->upper = NULL;
	else if (hook == priv->lower) {
		priv->lower = NULL;
		priv->lowerOrphan = 0;
	} else
		panic("%s: weird hook", __FUNCTION__);
	return (0);
}

static int
ng_enaddr_parse(const struct ng_parse_type *type,
	const char *s, int *const off, const u_char *const start,
	u_char *const buf, int *const buflen)
{
	char *eptr;
	u_long val;
	int i;

	if (*buflen < ETHER_ADDR_LEN)
		return (ERANGE);
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		val = strtoul(s + *off, &eptr, 16);
		if (val > 0xff || eptr == s + *off)
			return (EINVAL);
		buf[i] = (u_char)val;
		*off = (eptr - s);
		if (i < ETHER_ADDR_LEN - 1) {
			if (*eptr != ':')
				return (EINVAL);
			(*off)++;
		}
	}
	*buflen = ETHER_ADDR_LEN;
	return (0);
}

static int
ng_enaddr_unparse(const struct ng_parse_type *type,
	const u_char *data, int *off, char *cbuf, int cbuflen)
{
	int len;

	len = snprintf(cbuf, cbuflen, "%02x:%02x:%02x:%02x:%02x:%02x",
	    data[*off], data[*off + 1], data[*off + 2],
	    data[*off + 3], data[*off + 4], data[*off + 5]);
	if (len >= cbuflen)
		return (ERANGE);
	*off += ETHER_ADDR_LEN;
	return (0);
}

/******************************************************************
		    	INITIALIZATION
******************************************************************/

/*
 * Handle loading and unloading for this node type.
 */
static int
ng_ether_mod_event(module_t mod, int event, void *data)
{
	struct ifnet *ifp;
	int error = 0;
	int s;

	s = splnet();
	switch (event) {
	case MOD_LOAD:

		/* Register function hooks */
		if (ng_ether_attach_p != NULL) {
			error = EEXIST;
			break;
		}
		ng_ether_attach_p = ng_ether_attach;
		ng_ether_detach_p = ng_ether_detach;
		ng_ether_output_p = ng_ether_output;
		ng_ether_input_p = ng_ether_input;
		ng_ether_input_orphan_p = ng_ether_input_orphan;

		/* Create nodes for any already-existing Ethernet interfaces */
		TAILQ_FOREACH(ifp, &ifnet, if_link) {
			if (ifp->if_type == IFT_ETHER)
				ng_ether_attach(ifp);
		}
		break;

	case MOD_UNLOAD:

		/*
		 * Note that the base code won't try to unload us until
		 * all nodes have been removed, and that can't happen
		 * until all Ethernet interfaces are removed. In any
		 * case, we know there are no nodes left if the action
		 * is MOD_UNLOAD, so there's no need to detach any nodes.
		 */

		/* Unregister function hooks */
		ng_ether_attach_p = NULL;
		ng_ether_detach_p = NULL;
		ng_ether_output_p = NULL;
		ng_ether_input_p = NULL;
		ng_ether_input_orphan_p = NULL;
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	splx(s);
	return (error);
}

