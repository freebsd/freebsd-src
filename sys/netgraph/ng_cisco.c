
/*
 * ng_cisco.c
 *
 * Copyright (c) 1996-1999 Whistle Communications, Inc.
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
 * Author: Julian Elischer <julian@freebsd.org>
 *
 * $FreeBSD$
 * $Whistle: ng_cisco.c,v 1.25 1999/11/01 09:24:51 julian Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netatalk/at.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_cisco.h>

#define CISCO_MULTICAST         0x8f	/* Cisco multicast address */
#define CISCO_UNICAST           0x0f	/* Cisco unicast address */
#define CISCO_KEEPALIVE         0x8035	/* Cisco keepalive protocol */
#define CISCO_ADDR_REQ          0	/* Cisco address request */
#define CISCO_ADDR_REPLY        1	/* Cisco address reply */
#define CISCO_KEEPALIVE_REQ     2	/* Cisco keepalive request */

#define KEEPALIVE_SECS		10

struct cisco_header {
	u_char  address;
	u_char  control;
	u_short protocol;
};

#define CISCO_HEADER_LEN          sizeof (struct cisco_header)

struct cisco_packet {
	u_long  type;
	u_long  par1;
	u_long  par2;
	u_short rel;
	u_short time0;
	u_short time1;
};

#define CISCO_PACKET_LEN (sizeof(struct cisco_packet))

struct protoent {
	hook_p  hook;		/* the hook for this proto */
	u_short af;		/* address family, -1 = downstream */
};

struct cisco_priv {
	u_long  local_seq;
	u_long  remote_seq;
	u_long  seqRetries;	/* how many times we've been here throwing out
				 * the same sequence number without ack */
	node_p  node;
	struct callout_handle handle;
	struct protoent downstream;
	struct protoent inet;		/* IP information */
	struct in_addr localip;
	struct in_addr localmask;
	struct protoent inet6;		/* IPv6 information */
	struct protoent atalk;		/* AppleTalk information */
	struct protoent ipx;		/* IPX information */
};
typedef struct cisco_priv *sc_p;

/* Netgraph methods */
static ng_constructor_t		cisco_constructor;
static ng_rcvmsg_t		cisco_rcvmsg;
static ng_shutdown_t		cisco_rmnode;
static ng_newhook_t		cisco_newhook;
static ng_rcvdata_t		cisco_rcvdata;
static ng_disconnect_t		cisco_disconnect;

/* Other functions */
static int	cisco_input(sc_p sc, struct mbuf *m, meta_p meta);
static void	cisco_keepalive(void *arg);
static int	cisco_send(sc_p sc, int type, long par1, long par2);

/* Parse type for struct ng_cisco_ipaddr */
static const struct ng_parse_struct_info
	ng_cisco_ipaddr_type_info = NG_CISCO_IPADDR_TYPE_INFO;
static const struct ng_parse_type ng_cisco_ipaddr_type = {
	&ng_parse_struct_type,
	&ng_cisco_ipaddr_type_info
};

/* Parse type for struct ng_async_stat */
static const struct ng_parse_struct_info
	ng_cisco_stats_type_info = NG_CISCO_STATS_TYPE_INFO;
static const struct ng_parse_type ng_cisco_stats_type = {
	&ng_parse_struct_type,
	&ng_cisco_stats_type_info,
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_cisco_cmdlist[] = {
	{
	  NGM_CISCO_COOKIE,
	  NGM_CISCO_SET_IPADDR,
	  "setipaddr",
	  &ng_cisco_ipaddr_type,
	  NULL
	},
	{
	  NGM_CISCO_COOKIE,
	  NGM_CISCO_GET_IPADDR,
	  "getipaddr",
	  NULL,
	  &ng_cisco_ipaddr_type
	},
	{
	  NGM_CISCO_COOKIE,
	  NGM_CISCO_GET_STATUS,
	  "getstats",
	  NULL,
	  &ng_cisco_stats_type
	},
	{ 0 }
};

/* Node type */
static struct ng_type typestruct = {
	NG_VERSION,
	NG_CISCO_NODE_TYPE,
	NULL,
	cisco_constructor,
	cisco_rcvmsg,
	cisco_rmnode,
	cisco_newhook,
	NULL,
	NULL,
	cisco_rcvdata,
	cisco_rcvdata,
	cisco_disconnect,
	ng_cisco_cmdlist
};
NETGRAPH_INIT(cisco, &typestruct);

/*
 * Node constructor
 */
static int
cisco_constructor(node_p *nodep)
{
	sc_p sc;
	int error = 0;

	MALLOC(sc, sc_p, sizeof(*sc), M_NETGRAPH, M_NOWAIT);
	if (sc == NULL)
		return (ENOMEM);
	bzero(sc, sizeof(struct cisco_priv));

	callout_handle_init(&sc->handle);
	if ((error = ng_make_node_common(&typestruct, nodep))) {
		FREE(sc, M_NETGRAPH);
		return (error);
	}
	(*nodep)->private = sc;
	sc->node = *nodep;

	/* Initialise the varous protocol hook holders */
	sc->downstream.af = 0xffff;
	sc->inet.af = AF_INET;
	sc->inet6.af = AF_INET6;
	sc->atalk.af = AF_APPLETALK;
	sc->ipx.af = AF_IPX;
	return (0);
}

/*
 * Check new hook
 */
static int
cisco_newhook(node_p node, hook_p hook, const char *name)
{
	const sc_p sc = node->private;

	if (strcmp(name, NG_CISCO_HOOK_DOWNSTREAM) == 0) {
		sc->downstream.hook = hook;
		hook->private = &sc->downstream;

		/* Start keepalives */
		sc->handle = timeout(cisco_keepalive, sc, hz * KEEPALIVE_SECS);
	} else if (strcmp(name, NG_CISCO_HOOK_INET) == 0) {
		sc->inet.hook = hook;
		hook->private = &sc->inet;
	} else if (strcmp(name, NG_CISCO_HOOK_APPLETALK) == 0) {
		sc->atalk.hook = hook;
		hook->private = &sc->atalk;
	} else if (strcmp(name, NG_CISCO_HOOK_IPX) == 0) {
		sc->ipx.hook = hook;
		hook->private = &sc->ipx;
	} else if (strcmp(name, NG_CISCO_HOOK_DEBUG) == 0) {
		hook->private = NULL;	/* unimplemented */
	} else
		return (EINVAL);
	return 0;
}

/*
 * Receive control message.
 */
static int
cisco_rcvmsg(node_p node, struct ng_mesg *msg,
	const char *retaddr, struct ng_mesg **rptr, hook_p lasthook)
{
	const sc_p sc = node->private;
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_GENERIC_COOKIE:
		switch (msg->header.cmd) {
		case NGM_TEXT_STATUS:
		    {
			char *arg;
			int pos;

			NG_MKRESPONSE(resp, msg, sizeof(struct ng_mesg)
			    + NG_TEXTRESPONSE, M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			arg = (char *) resp->data;
			pos = sprintf(arg,
			  "keepalive period: %d sec; ", KEEPALIVE_SECS);
			pos += sprintf(arg + pos,
			  "unacknowledged keepalives: %ld", sc->seqRetries);
			resp->header.arglen = pos + 1;
			break;
		    }
		default:
			error = EINVAL;
			break;
		}
		break;
	case NGM_CISCO_COOKIE:
		switch (msg->header.cmd) {
		case NGM_CISCO_GET_IPADDR:	/* could be a late reply! */
			if ((msg->header.flags & NGF_RESP) == 0) {
				struct in_addr *ips;

				NG_MKRESPONSE(resp, msg,
				    2 * sizeof(*ips), M_NOWAIT);
				if (!resp) {
					error = ENOMEM;
					break;
				}
				ips = (struct in_addr *) resp->data;
				ips[0] = sc->localip;
				ips[1] = sc->localmask;
				break;
			}
			/* FALLTHROUGH */	/* ...if it's a reply */
		case NGM_CISCO_SET_IPADDR:
		    {
			struct in_addr *const ips = (struct in_addr *)msg->data;

			if (msg->header.arglen < 2 * sizeof(*ips)) {
				error = EINVAL;
				break;
			}
			sc->localip = ips[0];
			sc->localmask = ips[1];
			break;
		    }
		case NGM_CISCO_GET_STATUS:
		    {
			struct ng_cisco_stats *stat;

			NG_MKRESPONSE(resp, msg, sizeof(*stat), M_NOWAIT);
			if (!resp) {
				error = ENOMEM;
				break;
			}
			stat = (struct ng_cisco_stats *)resp->data;
			stat->seqRetries = sc->seqRetries;
			stat->keepAlivePeriod = KEEPALIVE_SECS;
			break;
		    }
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
	else if (resp)
		FREE(resp, M_NETGRAPH);
	FREE(msg, M_NETGRAPH);
	return (error);
}

/*
 * Receive data
 */
static int
cisco_rcvdata(hook_p hook, struct mbuf *m, meta_p meta,
		struct mbuf **ret_m, meta_p *ret_meta)
{
	const sc_p sc = hook->node->private;
	struct protoent *pep;
	struct cisco_header *h;
	int error = 0;

	if ((pep = hook->private) == NULL)
		goto out;

	/* If it came from our downlink, deal with it separately */
	if (pep->af == 0xffff)
		return (cisco_input(sc, m, meta));

	/* OK so it came from a protocol, heading out. Prepend general data
	   packet header. For now, IP,IPX only  */
	M_PREPEND(m, CISCO_HEADER_LEN, M_DONTWAIT);
	if (!m) {
		error = ENOBUFS;
		goto out;
	}
	h = mtod(m, struct cisco_header *);
	h->address = CISCO_UNICAST;
	h->control = 0;

	switch (pep->af) {
	case AF_INET:		/* Internet Protocol */
		h->protocol = htons(ETHERTYPE_IP);
		break;
	case AF_INET6:
		h->protocol = htons(ETHERTYPE_IPV6);
		break;
	case AF_APPLETALK:	/* AppleTalk Protocol */
		h->protocol = htons(ETHERTYPE_AT);
		break;
	case AF_IPX:		/* Novell IPX Protocol */
		h->protocol = htons(ETHERTYPE_IPX);
		break;
	default:
		error = EAFNOSUPPORT;
		goto out;
	}

	/* Send it */
	NG_SEND_DATA(error, sc->downstream.hook, m, meta);
	return (error);

out:
	NG_FREE_DATA(m, meta);
	return (error);
}

/*
 * Shutdown node
 */
static int
cisco_rmnode(node_p node)
{
	const sc_p sc = node->private;

	node->flags |= NG_INVALID;
	ng_cutlinks(node);
	ng_unname(node);
	node->private = NULL;
	ng_unref(sc->node);
	FREE(sc, M_NETGRAPH);
	return (0);
}

/*
 * Disconnection of a hook
 *
 * For this type, removal of the last link destroys the node
 */
static int
cisco_disconnect(hook_p hook)
{
	const sc_p sc = hook->node->private;
	struct protoent *pep;

	/* Check it's not the debug hook */
	if ((pep = hook->private)) {
		pep->hook = NULL;
		if (pep->af == 0xffff) {
			/* If it is the downstream hook, stop the timers */
			untimeout(cisco_keepalive, sc, sc->handle);
		}
	}

	/* If no more hooks, remove the node */
	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	return (0);
}

/*
 * Receive data
 */
static int
cisco_input(sc_p sc, struct mbuf *m, meta_p meta)
{
	struct cisco_header *h;
	struct cisco_packet *p;
	struct protoent *pep;
	int error = 0;

	if (m->m_pkthdr.len <= CISCO_HEADER_LEN)
		goto drop;

	/* Strip off cisco header */
	h = mtod(m, struct cisco_header *);
	m_adj(m, CISCO_HEADER_LEN);

	switch (h->address) {
	default:		/* Invalid Cisco packet. */
		goto drop;
	case CISCO_UNICAST:
	case CISCO_MULTICAST:
		/* Don't check the control field here (RFC 1547). */
		switch (ntohs(h->protocol)) {
		default:
			goto drop;
		case CISCO_KEEPALIVE:
			p = mtod(m, struct cisco_packet *);
			switch (ntohl(p->type)) {
			default:
				log(LOG_WARNING,
				    "cisco: unknown cisco packet type: 0x%lx\n",
				       ntohl(p->type));
				break;
			case CISCO_ADDR_REPLY:
				/* Reply on address request, ignore */
				break;
			case CISCO_KEEPALIVE_REQ:
				sc->remote_seq = ntohl(p->par1);
				if (sc->local_seq == ntohl(p->par2)) {
					sc->local_seq++;
					sc->seqRetries = 0;
				}
				break;
			case CISCO_ADDR_REQ:
			    {
				struct ng_mesg *msg, *resp;

				/* Ask inet peer for IP address information */
				if (sc->inet.hook == NULL)
					goto nomsg;
				NG_MKMESSAGE(msg, NGM_CISCO_COOKIE,
				    NGM_CISCO_GET_IPADDR, 0, M_NOWAIT);
				if (msg == NULL)
					goto nomsg;
				ng_send_msg(sc->node, msg,
				    NG_CISCO_HOOK_INET, &resp);
				if (resp != NULL)
					cisco_rcvmsg(sc->node, resp, ".",
								NULL, NULL);

		nomsg:
				/* Send reply to peer device */
				error = cisco_send(sc, CISCO_ADDR_REPLY,
					    ntohl(sc->localip.s_addr),
					    ntohl(sc->localmask.s_addr));
				break;
			    }
			}
			goto drop;
		case ETHERTYPE_IP:
			pep = &sc->inet;
			break;
		case ETHERTYPE_IPV6:
			pep = &sc->inet6;
			break;
		case ETHERTYPE_AT:
			pep = &sc->atalk;
			break;
		case ETHERTYPE_IPX:
			pep = &sc->ipx;
			break;
		}
		break;
	}

	/* Send it on */
	if (pep->hook == NULL)
		goto drop;
	NG_SEND_DATA(error, pep->hook, m, meta);
	return (error);

drop:
	NG_FREE_DATA(m, meta);
	return (error);
}


/*
 * Send keepalive packets, every 10 seconds.
 */
static void
cisco_keepalive(void *arg)
{
	const sc_p sc = arg;
	int s = splimp();

	cisco_send(sc, CISCO_KEEPALIVE_REQ, sc->local_seq, sc->remote_seq);
	sc->seqRetries++;
	splx(s);
	sc->handle = timeout(cisco_keepalive, sc, hz * KEEPALIVE_SECS);
}

/*
 * Send Cisco keepalive packet.
 */
static int
cisco_send(sc_p sc, int type, long par1, long par2)
{
	struct cisco_header *h;
	struct cisco_packet *ch;
	struct mbuf *m;
	u_long  t;
	int     error = 0;
	meta_p  meta = NULL;
	struct timeval time;

	getmicrotime(&time);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (!m)
		return (ENOBUFS);

	t = (time.tv_sec - boottime.tv_sec) * 1000;
	m->m_pkthdr.len = m->m_len = CISCO_HEADER_LEN + CISCO_PACKET_LEN;
	m->m_pkthdr.rcvif = 0;

	h = mtod(m, struct cisco_header *);
	h->address = CISCO_MULTICAST;
	h->control = 0;
	h->protocol = htons(CISCO_KEEPALIVE);

	ch = (struct cisco_packet *) (h + 1);
	ch->type = htonl(type);
	ch->par1 = htonl(par1);
	ch->par2 = htonl(par2);
	ch->rel = -1;
	ch->time0 = htons((u_short) (t >> 16));
	ch->time1 = htons((u_short) t);

	NG_SEND_DATA(error, sc->downstream.hook, m, meta);
	return (error);
}
