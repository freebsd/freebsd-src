
/*
 * ng_pptpgre.c
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
 * Author: Archie Cobbs <archie@freebsd.org>
 *
 * $FreeBSD$
 * $Whistle: ng_pptpgre.c,v 1.7 1999/12/08 00:10:06 archie Exp $
 */

/*
 * PPTP/GRE netgraph node type.
 *
 * This node type does the GRE encapsulation as specified for the PPTP
 * protocol (RFC 2637, section 4).  This includes sequencing and
 * retransmission of frames, but not the actual packet delivery nor
 * any of the TCP control stream protocol.
 *
 * The "upper" hook of this node is suitable for attaching to a "ppp"
 * node link hook.  The "lower" hook of this node is suitable for attaching
 * to a "ksocket" node on hook "inet/raw/gre".
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_pptpgre.h>

/* GRE packet format, as used by PPTP */
struct greheader {
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char		recursion:3;		/* recursion control */
	u_char		ssr:1;			/* strict source route */
	u_char		hasSeq:1;		/* sequence number present */
	u_char		hasKey:1;		/* key present */
	u_char		hasRoute:1;		/* routing present */
	u_char		hasSum:1;		/* checksum present */
	u_char		vers:3;			/* version */
	u_char		flags:4;		/* flags */
	u_char		hasAck:1;		/* acknowlege number present */
#elif BYTE_ORDER == BIG_ENDIAN
	u_char		hasSum:1;		/* checksum present */
	u_char		hasRoute:1;		/* routing present */
	u_char		hasKey:1;		/* key present */
	u_char		hasSeq:1;		/* sequence number present */
	u_char		ssr:1;			/* strict source route */
	u_char		recursion:3;		/* recursion control */
	u_char		hasAck:1;		/* acknowlege number present */
	u_char		flags:4;		/* flags */
	u_char		vers:3;			/* version */
#else
#error BYTE_ORDER is not defined properly
#endif
	u_int16_t	proto;			/* protocol (ethertype) */
	u_int16_t	length;			/* payload length */
	u_int16_t	cid;			/* call id */
	u_int32_t	data[0];		/* opt. seq, ack, then data */
};

/* The PPTP protocol ID used in the GRE 'proto' field */
#define PPTP_GRE_PROTO		0x880b

/* Bits that must be set a certain way in all PPTP/GRE packets */
#define PPTP_INIT_VALUE		((0x2001 << 16) | PPTP_GRE_PROTO)
#define PPTP_INIT_MASK		0xef7fffff

/* Min and max packet length */
#define PPTP_MAX_PAYLOAD	(0xffff - sizeof(struct greheader) - 8)

/* All times are scaled by this (PPTP_TIME_SCALE time units = 1 sec.) */
#define PPTP_TIME_SCALE		1000			/* milliseconds */
typedef u_int64_t		pptptime_t;

/* Acknowledgment timeout parameters and functions */
#define PPTP_XMIT_WIN		16			/* max xmit window */
#define PPTP_MIN_RTT		(PPTP_TIME_SCALE / 10)	/* 100 milliseconds */
#define PPTP_MIN_TIMEOUT	(PPTP_TIME_SCALE / 83)	/* 12 milliseconds */
#define PPTP_MAX_TIMEOUT	(10 * PPTP_TIME_SCALE)	/* 10 seconds */

/* When we recieve a packet, we wait to see if there's an outgoing packet
   we can piggy-back the ACK off of. These parameters determine the mimimum
   and maxmimum length of time we're willing to wait in order to do that.
   These have no effect unless "enableDelayedAck" is turned on. */
#define PPTP_MIN_ACK_DELAY	(PPTP_TIME_SCALE / 500)	/* 2 milliseconds */
#define PPTP_MAX_ACK_DELAY	(PPTP_TIME_SCALE / 2)	/* 500 milliseconds */

/* See RFC 2637 section 4.4 */
#define PPTP_ACK_ALPHA(x)	((x) >> 3)	/* alpha = 0.125 */
#define PPTP_ACK_BETA(x)	((x) >> 2)	/* beta = 0.25 */
#define PPTP_ACK_CHI(x) 	((x) << 2)	/* chi = 4 */
#define PPTP_ACK_DELTA(x) 	((x) << 1)	/* delta = 2 */

#define PPTP_SEQ_DIFF(x,y)	((int32_t)(x) - (int32_t)(y))

/* We keep packet retransmit and acknowlegement state in this struct */
struct ng_pptpgre_ackp {
	int32_t			ato;		/* adaptive time-out value */
	int32_t			rtt;		/* round trip time estimate */
	int32_t			dev;		/* deviation estimate */
	u_int16_t		xmitWin;	/* size of xmit window */
	struct callout		sackTimer;	/* send ack timer */
	struct callout		rackTimer;	/* recv ack timer */
	node_p			*sackTimerPtr;	/* send ack timer pointer */
	node_p			*rackTimerPtr;	/* recv ack timer pointer */
	u_int32_t		winAck;		/* seq when xmitWin will grow */
	pptptime_t		timeSent[PPTP_XMIT_WIN];
#ifdef DEBUG_RAT
	pptptime_t		timerStart;	/* when rackTimer started */
	pptptime_t		timerLength;	/* rackTimer duration */
#endif
};

/* Node private data */
struct ng_pptpgre_private {
	hook_p			upper;		/* hook to upper layers */
	hook_p			lower;		/* hook to lower layers */
	struct ng_pptpgre_conf	conf;		/* configuration info */
	struct ng_pptpgre_ackp	ackp;		/* packet transmit ack state */
	u_int32_t		recvSeq;	/* last seq # we rcv'd */
	u_int32_t		xmitSeq;	/* last seq # we sent */
	u_int32_t		recvAck;	/* last seq # peer ack'd */
	u_int32_t		xmitAck;	/* last seq # we ack'd */
	u_int			timers;		/* number of pending timers */
	struct timeval		startTime;	/* time node was created */
	struct ng_pptpgre_stats	stats;		/* node statistics */
};
typedef struct ng_pptpgre_private *priv_p;

/* Netgraph node methods */
static ng_constructor_t	ng_pptpgre_constructor;
static ng_rcvmsg_t	ng_pptpgre_rcvmsg;
static ng_shutdown_t	ng_pptpgre_rmnode;
static ng_newhook_t	ng_pptpgre_newhook;
static ng_rcvdata_t	ng_pptpgre_rcvdata;
static ng_disconnect_t	ng_pptpgre_disconnect;

/* Helper functions */
static int	ng_pptpgre_xmit(node_p node, struct mbuf *m, meta_p meta);
static int	ng_pptpgre_recv(node_p node, struct mbuf *m, meta_p meta);
static void	ng_pptpgre_start_send_ack_timer(node_p node, int ackTimeout);
static void	ng_pptpgre_stop_send_ack_timer(node_p node);
static void	ng_pptpgre_start_recv_ack_timer(node_p node);
static void	ng_pptpgre_stop_recv_ack_timer(node_p node);
static void	ng_pptpgre_recv_ack_timeout(void *arg);
static void	ng_pptpgre_send_ack_timeout(void *arg);
static void	ng_pptpgre_reset(node_p node);
static pptptime_t ng_pptpgre_time(node_p node);

/* Parse type for struct ng_pptpgre_conf */
static const struct ng_parse_struct_field ng_pptpgre_conf_type_fields[]
	= NG_PPTPGRE_CONF_TYPE_INFO;
static const struct ng_parse_type ng_pptpgre_conf_type = {
	&ng_parse_struct_type,
	&ng_pptpgre_conf_type_fields,
};

/* Parse type for struct ng_pptpgre_stats */
static const struct ng_parse_struct_field ng_pptpgre_stats_type_fields[]
	= NG_PPTPGRE_STATS_TYPE_INFO;
static const struct ng_parse_type ng_pptp_stats_type = {
	&ng_parse_struct_type,
	&ng_pptpgre_stats_type_fields
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_pptpgre_cmdlist[] = {
	{
	  NGM_PPTPGRE_COOKIE,
	  NGM_PPTPGRE_SET_CONFIG,
	  "setconfig",
	  &ng_pptpgre_conf_type,
	  NULL
	},
	{
	  NGM_PPTPGRE_COOKIE,
	  NGM_PPTPGRE_GET_CONFIG,
	  "getconfig",
	  NULL,
	  &ng_pptpgre_conf_type
	},
	{
	  NGM_PPTPGRE_COOKIE,
	  NGM_PPTPGRE_GET_STATS,
	  "getstats",
	  NULL,
	  &ng_pptp_stats_type
	},
	{
	  NGM_PPTPGRE_COOKIE,
	  NGM_PPTPGRE_CLR_STATS,
	  "clrstats",
	  NULL,
	  NULL
	},
	{
	  NGM_PPTPGRE_COOKIE,
	  NGM_PPTPGRE_GETCLR_STATS,
	  "getclrstats",
	  NULL,
	  &ng_pptp_stats_type
	},
	{ 0 }
};

/* Node type descriptor */
static struct ng_type ng_pptpgre_typestruct = {
	NG_VERSION,
	NG_PPTPGRE_NODE_TYPE,
	NULL,
	ng_pptpgre_constructor,
	ng_pptpgre_rcvmsg,
	ng_pptpgre_rmnode,
	ng_pptpgre_newhook,
	NULL,
	NULL,
	ng_pptpgre_rcvdata,
	ng_pptpgre_rcvdata,
	ng_pptpgre_disconnect,
	ng_pptpgre_cmdlist
};
NETGRAPH_INIT(pptpgre, &ng_pptpgre_typestruct);

#define ERROUT(x)	do { error = (x); goto done; } while (0)

/************************************************************************
			NETGRAPH NODE STUFF
 ************************************************************************/

/*
 * Node type constructor
 */
static int
ng_pptpgre_constructor(node_p *nodep)
{
	priv_p priv;
	int error;

	/* Allocate private structure */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_NOWAIT);
	if (priv == NULL)
		return (ENOMEM);
	bzero(priv, sizeof(*priv));

	/* Call generic node constructor */
	if ((error = ng_make_node_common(&ng_pptpgre_typestruct, nodep))) {
		FREE(priv, M_NETGRAPH);
		return (error);
	}
	(*nodep)->private = priv;

	/* Initialize state */
	callout_init(&priv->ackp.sackTimer);
	callout_init(&priv->ackp.rackTimer);

	/* Done */
	return (0);
}

/*
 * Give our OK for a hook to be added.
 */
static int
ng_pptpgre_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = node->private;
	hook_p *hookPtr;

	/* Check hook name */
	if (strcmp(name, NG_PPTPGRE_HOOK_UPPER) == 0)
		hookPtr = &priv->upper;
	else if (strcmp(name, NG_PPTPGRE_HOOK_LOWER) == 0)
		hookPtr = &priv->lower;
	else
		return (EINVAL);

	/* See if already connected */
	if (*hookPtr != NULL)
		return (EISCONN);

	/* OK */
	*hookPtr = hook;
	return (0);
}

/*
 * Receive a control message.
 */
static int
ng_pptpgre_rcvmsg(node_p node, struct ng_mesg *msg,
	      const char *raddr, struct ng_mesg **rptr)
{
	const priv_p priv = node->private;
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_PPTPGRE_COOKIE:
		switch (msg->header.cmd) {
		case NGM_PPTPGRE_SET_CONFIG:
		    {
			struct ng_pptpgre_conf *const newConf =
				(struct ng_pptpgre_conf *) msg->data;

			/* Check for invalid or illegal config */
			if (msg->header.arglen != sizeof(*newConf))
				ERROUT(EINVAL);
			ng_pptpgre_reset(node);		/* reset on configure */
			priv->conf = *newConf;
			break;
		    }
		case NGM_PPTPGRE_GET_CONFIG:
			NG_MKRESPONSE(resp, msg, sizeof(priv->conf), M_NOWAIT);
			if (resp == NULL)
				ERROUT(ENOMEM);
			bcopy(&priv->conf, resp->data, sizeof(priv->conf));
			break;
		case NGM_PPTPGRE_GET_STATS:
		case NGM_PPTPGRE_CLR_STATS:
		case NGM_PPTPGRE_GETCLR_STATS:
		    {
			if (msg->header.cmd != NGM_PPTPGRE_CLR_STATS) {
				NG_MKRESPONSE(resp, msg,
				    sizeof(priv->stats), M_NOWAIT);
				if (resp == NULL)
					ERROUT(ENOMEM);
				bcopy(&priv->stats,
				    resp->data, sizeof(priv->stats));
			}
			if (msg->header.cmd != NGM_PPTPGRE_GET_STATS)
				bzero(&priv->stats, sizeof(priv->stats));
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

done:
	FREE(msg, M_NETGRAPH);
	return (error);
}

/*
 * Receive incoming data on a hook.
 */
static int
ng_pptpgre_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const node_p node = hook->node;
	const priv_p priv = node->private;

	/* If not configured, reject */
	if (!priv->conf.enabled) {
		NG_FREE_DATA(m, meta);
		return (ENXIO);
	}

	/* Treat as xmit or recv data */
	if (hook == priv->upper)
		return ng_pptpgre_xmit(node, m, meta);
	if (hook == priv->lower)
		return ng_pptpgre_recv(node, m, meta);
	panic("%s: weird hook", __FUNCTION__);
}

/*
 * Destroy node
 */
static int
ng_pptpgre_rmnode(node_p node)
{
	const priv_p priv = node->private;

	/* Reset node */
	ng_pptpgre_reset(node);

	/* Take down netgraph node */
	node->flags |= NG_INVALID;
	ng_cutlinks(node);
	ng_unname(node);

	/* If no timers remain, free private info as well */
	if (priv->timers == 0) {
		FREE(priv, M_NETGRAPH);
		node->private = NULL;
	}

	/* Done */
	ng_unref(node);
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_pptpgre_disconnect(hook_p hook)
{
	const node_p node = hook->node;
	const priv_p priv = node->private;

	/* Zero out hook pointer */
	if (hook == priv->upper)
		priv->upper = NULL;
	else if (hook == priv->lower)
		priv->lower = NULL;
	else
		panic("%s: unknown hook", __FUNCTION__);

	/* Go away if no longer connected to anything */
	if (node->numhooks == 0)
		ng_rmnode(node);
	return (0);
}

/*************************************************************************
		    TRANSMIT AND RECEIVE FUNCTIONS
*************************************************************************/

/*
 * Transmit an outgoing frame, or just an ack if m is NULL.
 */
static int
ng_pptpgre_xmit(node_p node, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;
	struct ng_pptpgre_ackp *const a = &priv->ackp;
	u_char buf[sizeof(struct greheader) + 2 * sizeof(u_int32_t)];
	struct greheader *const gre = (struct greheader *)buf;
	int grelen, error;

	/* Check if there's data */
	if (m != NULL) {

		/* Is our transmit window full? */
		if ((u_int32_t)PPTP_SEQ_DIFF(priv->xmitSeq, priv->recvAck)
		      >= a->xmitWin) {
			priv->stats.xmitDrops++;
			NG_FREE_DATA(m, meta);
			return (ENOBUFS);
		}

		/* Sanity check frame length */
		if (m != NULL && m->m_pkthdr.len > PPTP_MAX_PAYLOAD) {
			priv->stats.xmitTooBig++;
			NG_FREE_DATA(m, meta);
			return (EMSGSIZE);
		}
	} else
		priv->stats.xmitLoneAcks++;

	/* Build GRE header */
	((u_int32_t *)gre)[0] = htonl(PPTP_INIT_VALUE);
	gre->length = (m != NULL) ? htons((u_short)m->m_pkthdr.len) : 0;
	gre->cid = htons(priv->conf.peerCid);

	/* Include sequence number if packet contains any data */
	if (m != NULL) {
		gre->hasSeq = 1;
		a->timeSent[priv->xmitSeq - priv->recvAck]
		    = ng_pptpgre_time(node);
		priv->xmitSeq++;
		gre->data[0] = htonl(priv->xmitSeq);
	}

	/* Include acknowledgement (and stop send ack timer) if needed */
	if (priv->conf.enableAlwaysAck || priv->xmitAck != priv->recvSeq) {
		gre->hasAck = 1;
		gre->data[gre->hasSeq] = htonl(priv->recvSeq);
		priv->xmitAck = priv->recvSeq;
		ng_pptpgre_stop_send_ack_timer(node);
	}

	/* Prepend GRE header to outgoing frame */
	grelen = sizeof(*gre) + sizeof(u_int32_t) * (gre->hasSeq + gre->hasAck);
	if (m == NULL) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			priv->stats.memoryFailures++;
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		m->m_len = m->m_pkthdr.len = grelen;
		m->m_pkthdr.rcvif = NULL;
	} else {
		M_PREPEND(m, grelen, M_NOWAIT);
		if (m == NULL || (m->m_len < grelen
		    && (m = m_pullup(m, grelen)) == NULL)) {
			priv->stats.memoryFailures++;
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
	}
	bcopy(gre, mtod(m, u_char *), grelen);

	/* Update stats */
	priv->stats.xmitPackets++;
	priv->stats.xmitOctets += m->m_pkthdr.len;

	/* Deliver packet */
	NG_SEND_DATA(error, priv->lower, m, meta);

	/* Start receive ACK timer if data was sent and not already running */
	if (error == 0 && gre->hasSeq && priv->xmitSeq == priv->recvAck + 1)
		ng_pptpgre_start_recv_ack_timer(node);
	return (error);
}

/*
 * Handle an incoming packet.  The packet includes the IP header.
 */
static int
ng_pptpgre_recv(node_p node, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;
	int iphlen, grelen, extralen;
	const struct greheader *gre;
	const struct ip *ip;
	int error = 0;

	/* Update stats */
	priv->stats.recvPackets++;
	priv->stats.recvOctets += m->m_pkthdr.len;

	/* Sanity check packet length */
	if (m->m_pkthdr.len < sizeof(*ip) + sizeof(*gre)) {
		priv->stats.recvRunts++;
bad:
		NG_FREE_DATA(m, meta);
		return (EINVAL);
	}

	/* Safely pull up the complete IP+GRE headers */
	if (m->m_len < sizeof(*ip) + sizeof(*gre)
	    && (m = m_pullup(m, sizeof(*ip) + sizeof(*gre))) == NULL) {
		priv->stats.memoryFailures++;
		NG_FREE_META(meta);
		return (ENOBUFS);
	}
	ip = mtod(m, const struct ip *);
	iphlen = ip->ip_hl << 2;
	if (m->m_len < iphlen + sizeof(*gre)) {
		if ((m = m_pullup(m, iphlen + sizeof(*gre))) == NULL) {
			priv->stats.memoryFailures++;
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		ip = mtod(m, const struct ip *);
	}
	gre = (const struct greheader *)((const u_char *)ip + iphlen);
	grelen = sizeof(*gre) + sizeof(u_int32_t) * (gre->hasSeq + gre->hasAck);
	if (m->m_pkthdr.len < iphlen + grelen) {
		priv->stats.recvRunts++;
		goto bad;
	}
	if (m->m_len < iphlen + grelen) {
		if ((m = m_pullup(m, iphlen + grelen)) == NULL) {
			priv->stats.memoryFailures++;
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		ip = mtod(m, const struct ip *);
		gre = (const struct greheader *)((const u_char *)ip + iphlen);
	}

	/* Sanity check packet length and GRE header bits */
	extralen = m->m_pkthdr.len
	    - (iphlen + grelen + gre->hasSeq * (u_int16_t)ntohs(gre->length));
	if (extralen < 0) {
		priv->stats.recvBadGRE++;
		goto bad;
	}
	if ((ntohl(*((const u_int32_t *)gre)) & PPTP_INIT_MASK)
	    != PPTP_INIT_VALUE) {
		priv->stats.recvBadGRE++;
		goto bad;
	}
	if (ntohs(gre->cid) != priv->conf.cid) {
		priv->stats.recvBadCID++;
		goto bad;
	}

	/* Look for peer ack */
	if (gre->hasAck) {
		struct ng_pptpgre_ackp *const a = &priv->ackp;
		const u_int32_t	ack = ntohl(gre->data[gre->hasSeq]);
		const int index = ack - priv->recvAck - 1;
		long sample;
		long diff;

		/* Sanity check ack value */
		if (PPTP_SEQ_DIFF(ack, priv->xmitSeq) > 0) {
			priv->stats.recvBadAcks++;
			goto badAck;		/* we never sent it! */
		}
		if (PPTP_SEQ_DIFF(ack, priv->recvAck) <= 0)
			goto badAck;		/* ack already timed out */
		priv->recvAck = ack;

		/* Update adaptive timeout stuff */
		sample = ng_pptpgre_time(node) - a->timeSent[index];
		diff = sample - a->rtt;
		a->rtt += PPTP_ACK_ALPHA(diff);
		if (diff < 0)
			diff = -diff;
		a->dev += PPTP_ACK_BETA(diff - a->dev);
		a->ato = a->rtt + PPTP_ACK_CHI(a->dev);
		if (a->ato > PPTP_MAX_TIMEOUT)
			a->ato = PPTP_MAX_TIMEOUT;
		if (a->ato < PPTP_MIN_TIMEOUT)
			a->ato = PPTP_MIN_TIMEOUT;

		/* Shift packet transmit times in our transmit window */
		ovbcopy(a->timeSent + index + 1, a->timeSent,
		    sizeof(*a->timeSent) * (PPTP_XMIT_WIN - (index + 1)));

		/* If we sent an entire window, increase window size by one */
		if (PPTP_SEQ_DIFF(ack, a->winAck) >= 0
		    && a->xmitWin < PPTP_XMIT_WIN) {
			a->xmitWin++;
			a->winAck = ack + a->xmitWin;
		}

		/* Stop/(re)start receive ACK timer as necessary */
		ng_pptpgre_stop_recv_ack_timer(node);
		if (priv->recvAck != priv->xmitSeq)
			ng_pptpgre_start_recv_ack_timer(node);
	}
badAck:

	/* See if frame contains any data */
	if (gre->hasSeq) {
		struct ng_pptpgre_ackp *const a = &priv->ackp;
		const u_int32_t seq = ntohl(gre->data[0]);

		/* Sanity check sequence number */
		if (PPTP_SEQ_DIFF(seq, priv->recvSeq) <= 0) {
			if (seq == priv->recvSeq)
				priv->stats.recvDuplicates++;
			else
				priv->stats.recvOutOfOrder++;
			goto bad;		/* out-of-order or dup */
		}
		priv->recvSeq = seq;

		/* We need to acknowledge this packet; do it soon... */
		if (a->sackTimerPtr == NULL) {
			int maxWait;

			/* Take 1/4 of the estimated round trip time */
			maxWait = (a->rtt >> 2);

			/* If delayed ACK is disabled, send it now */
			if (!priv->conf.enableDelayedAck)	/* ack now */
				ng_pptpgre_xmit(node, NULL, NULL);
			else {					/* ack later */
				if (maxWait < PPTP_MIN_ACK_DELAY)
					maxWait = PPTP_MIN_ACK_DELAY;
				if (maxWait > PPTP_MAX_ACK_DELAY)
					maxWait = PPTP_MAX_ACK_DELAY;
				ng_pptpgre_start_send_ack_timer(node, maxWait);
			}
		}

		/* Trim mbuf down to internal payload */
		m_adj(m, iphlen + grelen);
		if (extralen > 0)
			m_adj(m, -extralen);

		/* Deliver frame to upper layers */
		NG_SEND_DATA(error, priv->upper, m, meta);
	} else {
		priv->stats.recvLoneAcks++;
		NG_FREE_DATA(m, meta);		/* no data to deliver */
	}
	return (error);
}

/*************************************************************************
		    TIMER RELATED FUNCTIONS
*************************************************************************/

/*
 * Start a timer for the peer's acknowledging our oldest unacknowledged
 * sequence number.  If we get an ack for this sequence number before
 * the timer goes off, we cancel the timer.  Resets currently running
 * recv ack timer, if any.
 */
static void
ng_pptpgre_start_recv_ack_timer(node_p node)
{
	const priv_p priv = node->private;
	struct ng_pptpgre_ackp *const a = &priv->ackp;
	int remain, ticks;

	/* Compute how long until oldest unack'd packet times out,
	   and reset the timer to that time. */
	KASSERT(a->rackTimerPtr == NULL, ("%s: rackTimer", __FUNCTION__));
	remain = (a->timeSent[0] + a->ato) - ng_pptpgre_time(node);
	if (remain < 0)
		remain = 0;
#ifdef DEBUG_RAT
	a->timerLength = remain;
	a->timerStart = ng_pptpgre_time(node);
#endif

	/* Start new timer */
	MALLOC(a->rackTimerPtr, node_p *, sizeof(node_p), M_NETGRAPH, M_NOWAIT);
	if (a->rackTimerPtr == NULL) {
		priv->stats.memoryFailures++;
		return;			/* XXX potential hang here */
	}
	*a->rackTimerPtr = node;	/* ensures the correct timeout event */
	node->refs++;
	priv->timers++;

	/* Be conservative: timeout can happen up to 1 tick early */
	ticks = (((remain * hz) + PPTP_TIME_SCALE - 1) / PPTP_TIME_SCALE) + 1;
	callout_reset(&a->rackTimer, ticks,
	    ng_pptpgre_recv_ack_timeout, a->rackTimerPtr);
}

/*
 * Stop receive ack timer.
 */
static void
ng_pptpgre_stop_recv_ack_timer(node_p node)
{
	const priv_p priv = node->private;
	struct ng_pptpgre_ackp *const a = &priv->ackp;

	if (callout_stop(&a->rackTimer)) {
		FREE(a->rackTimerPtr, M_NETGRAPH);
		priv->timers--;
		ng_unref(node);
	}
	a->rackTimerPtr = NULL;
}

/*
 * The peer has failed to acknowledge the oldest unacknowledged sequence
 * number within the time allotted.  Update our adaptive timeout parameters
 * and reset/restart the recv ack timer.
 */
static void
ng_pptpgre_recv_ack_timeout(void *arg)
{
	int s = splnet();
	const node_p node = *((node_p *)arg);
	const priv_p priv = node->private;
	struct ng_pptpgre_ackp *const a = &priv->ackp;

	/* This complicated stuff is needed to avoid race conditions */
	FREE(arg, M_NETGRAPH);
	KASSERT(node->refs > 0, ("%s: no refs", __FUNCTION__));
	KASSERT(priv != NULL, ("%s: priv=NULL", __FUNCTION__));
	priv->timers--;
	if ((node->flags & NG_INVALID) != 0) {	/* shutdown race condition */
		if (priv->timers == 0) {
			FREE(priv, M_NETGRAPH);
			node->private = NULL;
		}
		ng_unref(node);
		splx(s);
		return;
	}
	if (arg != a->rackTimerPtr) {	/* timer stopped race condition */
		ng_unref(node);
		splx(s);
		return;
	}
	a->rackTimerPtr = NULL;

	/* Update adaptive timeout stuff */
	priv->stats.recvAckTimeouts++;
	a->rtt = PPTP_ACK_DELTA(a->rtt);
	a->ato = a->rtt + PPTP_ACK_CHI(a->dev);
	if (a->ato > PPTP_MAX_TIMEOUT)
		a->ato = PPTP_MAX_TIMEOUT;
	if (a->ato < PPTP_MIN_TIMEOUT)
		a->ato = PPTP_MIN_TIMEOUT;

#ifdef DEBUG_RAT
    log(LOG_DEBUG,
	"RAT now=%d seq=0x%x sent=%d tstart=%d tlen=%d ato=%d\n",
	(int)ng_pptpgre_time(node), priv->recvAck + 1,
	(int)a->timeSent[0], (int)a->timerStart, (int)a->timerLength, a->ato);
#endif

	/* Reset ack and sliding window */
	priv->recvAck = priv->xmitSeq;		/* pretend we got the ack */
	a->xmitWin = (a->xmitWin + 1) / 2;	/* shrink transmit window */
	a->winAck = priv->recvAck + a->xmitWin;	/* reset win expand time */
	ng_unref(node);
	splx(s);
}

/*
 * Start the send ack timer. This assumes the timer is not
 * already running.
 */
static void
ng_pptpgre_start_send_ack_timer(node_p node, int ackTimeout)
{
	const priv_p priv = node->private;
	struct ng_pptpgre_ackp *const a = &priv->ackp;
	int ticks;

	/* Start new timer */
	KASSERT(a->sackTimerPtr == NULL, ("%s: sackTimer", __FUNCTION__));
	MALLOC(a->sackTimerPtr, node_p *, sizeof(node_p), M_NETGRAPH, M_NOWAIT);
	if (a->sackTimerPtr == NULL) {
		priv->stats.memoryFailures++;
		return;			/* XXX potential hang here */
	}
	*a->sackTimerPtr = node;	/* ensures the correct timeout event */
	node->refs++;
	priv->timers++;

	/* Be conservative: timeout can happen up to 1 tick early */
	ticks = (((ackTimeout * hz) + PPTP_TIME_SCALE - 1) / PPTP_TIME_SCALE);
	callout_reset(&a->sackTimer, ticks,
	    ng_pptpgre_send_ack_timeout, a->sackTimerPtr);
}

/*
 * Stop send ack timer.
 */
static void
ng_pptpgre_stop_send_ack_timer(node_p node)
{
	const priv_p priv = node->private;
	struct ng_pptpgre_ackp *const a = &priv->ackp;

	if (callout_stop(&a->sackTimer)) {
		FREE(a->sackTimerPtr, M_NETGRAPH);
		priv->timers--;
		ng_unref(node);
	}
	a->sackTimerPtr = NULL;
}

/*
 * We've waited as long as we're willing to wait before sending an
 * acknowledgement to the peer for received frames. We had hoped to
 * be able to piggy back our acknowledgement on an outgoing data frame,
 * but apparently there haven't been any since. So send the ack now.
 */
static void
ng_pptpgre_send_ack_timeout(void *arg)
{
	int s = splnet();
	const node_p node = *((node_p *)arg);
	const priv_p priv = node->private;
	struct ng_pptpgre_ackp *const a = &priv->ackp;

	/* This complicated stuff is needed to avoid race conditions */
	FREE(arg, M_NETGRAPH);
	KASSERT(node->refs > 0, ("%s: no refs", __FUNCTION__));
	KASSERT(priv != NULL, ("%s: priv=NULL", __FUNCTION__));
	priv->timers--;
	if ((node->flags & NG_INVALID) != 0) {	/* shutdown race condition */
		if (priv->timers == 0) {
			FREE(priv, M_NETGRAPH);
			node->private = NULL;
		}
		ng_unref(node);
		splx(s);
		return;
	}
	if (a->sackTimerPtr != arg) {	/* timer stopped race condition */
		ng_unref(node);
		splx(s);
		return;
	}
	a->sackTimerPtr = NULL;

	/* Send a frame with an ack but no payload */
  	ng_pptpgre_xmit(node, NULL, NULL);
	ng_unref(node);
	splx(s);
}

/*************************************************************************
		    MISC FUNCTIONS
*************************************************************************/

/*
 * Reset state
 */
static void
ng_pptpgre_reset(node_p node)
{
	const priv_p priv = node->private;
	struct ng_pptpgre_ackp *const a = &priv->ackp;

	/* Reset adaptive timeout state */
	a->ato = PPTP_MAX_TIMEOUT;
	a->rtt = priv->conf.peerPpd * PPTP_TIME_SCALE / 10;  /* ppd in 10ths */
	if (a->rtt < PPTP_MIN_RTT)
		a->rtt = PPTP_MIN_RTT;
	a->dev = 0;
	a->xmitWin = (priv->conf.recvWin + 1) / 2;
	if (a->xmitWin < 2)		/* often the first packet is lost */
		a->xmitWin = 2;		/*   because the peer isn't ready */
	if (a->xmitWin > PPTP_XMIT_WIN)
		a->xmitWin = PPTP_XMIT_WIN;
	a->winAck = a->xmitWin;

	/* Reset sequence numbers */
	priv->recvSeq = ~0;
	priv->recvAck = ~0;
	priv->xmitSeq = ~0;
	priv->xmitAck = ~0;

	/* Reset start time */
	getmicrouptime(&priv->startTime);

	/* Reset stats */
	bzero(&priv->stats, sizeof(priv->stats));

	/* Stop timers */
	ng_pptpgre_stop_send_ack_timer(node);
	ng_pptpgre_stop_recv_ack_timer(node);
}

/*
 * Return the current time scaled & translated to our internally used format.
 */
static pptptime_t
ng_pptpgre_time(node_p node)
{
	const priv_p priv = node->private;
	struct timeval tv;
	pptptime_t t;

	microuptime(&tv);
	if (tv.tv_sec < priv->startTime.tv_sec
	    || (tv.tv_sec == priv->startTime.tv_sec
	      && tv.tv_usec < priv->startTime.tv_usec))
		return (0);
	timevalsub(&tv, &priv->startTime);
	t = (pptptime_t)tv.tv_sec * PPTP_TIME_SCALE;
	t += (pptptime_t)tv.tv_usec / (1000000 / PPTP_TIME_SCALE);
	return(t);
}

