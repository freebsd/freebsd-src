/*
 * ng_pptpgre.c
 */

/*-
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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/endian.h>
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
#define PPTP_TIME_SCALE		1024			/* milliseconds */
typedef u_int64_t		pptptime_t;

/* Acknowledgment timeout parameters and functions */
#define PPTP_XMIT_WIN		16			/* max xmit window */
#define PPTP_MIN_TIMEOUT	(PPTP_TIME_SCALE / 83)	/* 12 milliseconds */
#define PPTP_MAX_TIMEOUT	(3 * PPTP_TIME_SCALE)	/* 3 seconds */

/* When we receive a packet, we wait to see if there's an outgoing packet
   we can piggy-back the ACK off of. These parameters determine the mimimum
   and maxmimum length of time we're willing to wait in order to do that.
   These have no effect unless "enableDelayedAck" is turned on. */
#define PPTP_MIN_ACK_DELAY	(PPTP_TIME_SCALE / 500)	/* 2 milliseconds */
#define PPTP_MAX_ACK_DELAY	(PPTP_TIME_SCALE / 2)	/* 500 milliseconds */

/* See RFC 2637 section 4.4 */
#define PPTP_ACK_ALPHA(x)	(((x) + 4) >> 3)	/* alpha = 0.125 */
#define PPTP_ACK_BETA(x)	(((x) + 2) >> 2)	/* beta = 0.25 */
#define PPTP_ACK_CHI(x) 	((x) << 2)	/* chi = 4 */
#define PPTP_ACK_DELTA(x) 	((x) << 1)	/* delta = 2 */

#define PPTP_SEQ_DIFF(x,y)	((int32_t)(x) - (int32_t)(y))

#define SESSHASHSIZE		0x0020
#define SESSHASH(x)		(((x) ^ ((x) >> 8)) & (SESSHASHSIZE - 1))

/* We keep packet retransmit and acknowlegement state in this struct */
struct ng_pptpgre_sess {
	node_p			node;		/* this node pointer */
	hook_p			hook;		/* hook to upper layers */
	struct ng_pptpgre_conf	conf;		/* configuration info */
	struct mtx		mtx;		/* session mutex */
	u_int32_t		recvSeq;	/* last seq # we rcv'd */
	u_int32_t		xmitSeq;	/* last seq # we sent */
	u_int32_t		recvAck;	/* last seq # peer ack'd */
	u_int32_t		xmitAck;	/* last seq # we ack'd */
	int32_t			ato;		/* adaptive time-out value */
	int32_t			rtt;		/* round trip time estimate */
	int32_t			dev;		/* deviation estimate */
	u_int16_t		xmitWin;	/* size of xmit window */
	struct callout		sackTimer;	/* send ack timer */
	struct callout		rackTimer;	/* recv ack timer */
	u_int32_t		winAck;		/* seq when xmitWin will grow */
	pptptime_t		timeSent[PPTP_XMIT_WIN];
	LIST_ENTRY(ng_pptpgre_sess) sessions;
};
typedef struct ng_pptpgre_sess *hpriv_p;

/* Node private data */
struct ng_pptpgre_private {
	hook_p			upper;		/* hook to upper layers */
	hook_p			lower;		/* hook to lower layers */
	struct ng_pptpgre_sess	uppersess;	/* default session for compat */
	LIST_HEAD(, ng_pptpgre_sess) sesshash[SESSHASHSIZE];
	struct ng_pptpgre_stats	stats;		/* node statistics */
};
typedef struct ng_pptpgre_private *priv_p;

/* Netgraph node methods */
static ng_constructor_t	ng_pptpgre_constructor;
static ng_rcvmsg_t	ng_pptpgre_rcvmsg;
static ng_shutdown_t	ng_pptpgre_shutdown;
static ng_newhook_t	ng_pptpgre_newhook;
static ng_rcvdata_t	ng_pptpgre_rcvdata;
static ng_rcvdata_t	ng_pptpgre_rcvdata_lower;
static ng_disconnect_t	ng_pptpgre_disconnect;

/* Helper functions */
static int	ng_pptpgre_xmit(hpriv_p hpriv, item_p item);
static void	ng_pptpgre_start_send_ack_timer(hpriv_p hpriv);
static void	ng_pptpgre_start_recv_ack_timer(hpriv_p hpriv);
static void	ng_pptpgre_recv_ack_timeout(node_p node, hook_p hook,
		    void *arg1, int arg2);
static void	ng_pptpgre_send_ack_timeout(node_p node, hook_p hook,
		    void *arg1, int arg2);
static hpriv_p	ng_pptpgre_find_session(priv_p privp, u_int16_t cid);
static void	ng_pptpgre_reset(hpriv_p hpriv);
static pptptime_t ng_pptpgre_time(void);

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
	  &ng_parse_hint16_type,
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
	.version =	NG_ABI_VERSION,
	.name =		NG_PPTPGRE_NODE_TYPE,
	.constructor =	ng_pptpgre_constructor,
	.rcvmsg =	ng_pptpgre_rcvmsg,
	.shutdown =	ng_pptpgre_shutdown,
	.newhook =	ng_pptpgre_newhook,
	.rcvdata =	ng_pptpgre_rcvdata,
	.disconnect =	ng_pptpgre_disconnect,
	.cmdlist =	ng_pptpgre_cmdlist,
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
ng_pptpgre_constructor(node_p node)
{
	priv_p priv;
	int i;

	/* Allocate private structure */
	priv = malloc(sizeof(*priv), M_NETGRAPH, M_WAITOK | M_ZERO);

	NG_NODE_SET_PRIVATE(node, priv);

	/* Initialize state */
	mtx_init(&priv->uppersess.mtx, "ng_pptp", NULL, MTX_DEF);
	ng_callout_init(&priv->uppersess.sackTimer);
	ng_callout_init(&priv->uppersess.rackTimer);
	priv->uppersess.node = node;

	for (i = 0; i < SESSHASHSIZE; i++)
	    LIST_INIT(&priv->sesshash[i]);

	LIST_INSERT_HEAD(&priv->sesshash[0], &priv->uppersess, sessions);

	/* Done */
	return (0);
}

/*
 * Give our OK for a hook to be added.
 */
static int
ng_pptpgre_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	/* Check hook name */
	if (strcmp(name, NG_PPTPGRE_HOOK_UPPER) == 0) {
		priv->upper = hook;
		priv->uppersess.hook = hook;
		NG_HOOK_SET_PRIVATE(hook, &priv->uppersess);
	} else if (strcmp(name, NG_PPTPGRE_HOOK_LOWER) == 0) {
		priv->lower = hook;
		NG_HOOK_SET_RCVDATA(hook, ng_pptpgre_rcvdata_lower);
	} else {
		static const char hexdig[16] = "0123456789abcdef";
		const char *hex;
		hpriv_p hpriv;
		int i, j;
		uint16_t cid, hash;

		/* Parse hook name to get session ID */
		if (strncmp(name, NG_PPTPGRE_HOOK_SESSION_P,
		    sizeof(NG_PPTPGRE_HOOK_SESSION_P) - 1) != 0)
			return (EINVAL);
		hex = name + sizeof(NG_PPTPGRE_HOOK_SESSION_P) - 1;
		for (cid = i = 0; i < 4; i++) {
			for (j = 0; j < 16 && hex[i] != hexdig[j]; j++);
			if (j == 16)
				return (EINVAL);
			cid = (cid << 4) | j;
		}
		if (hex[i] != '\0')
			return (EINVAL);

		hpriv = malloc(sizeof(*hpriv), M_NETGRAPH, M_NOWAIT | M_ZERO);
		if (hpriv == NULL)
			return (ENOMEM);
	
		/* Initialize state */
		mtx_init(&hpriv->mtx, "ng_pptp", NULL, MTX_DEF);
		ng_callout_init(&hpriv->sackTimer);
		ng_callout_init(&hpriv->rackTimer);
		hpriv->conf.cid = cid;
		hpriv->node = node;
		hpriv->hook = hook;
		NG_HOOK_SET_PRIVATE(hook, hpriv);

		hash = SESSHASH(cid);
		LIST_INSERT_HEAD(&priv->sesshash[hash], hpriv, sessions);
	}

	return (0);
}

/*
 * Receive a control message.
 */
static int
ng_pptpgre_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ng_mesg *resp = NULL;
	int error = 0;
	struct ng_mesg *msg;

	NGI_GET_MSG(item, msg);
	switch (msg->header.typecookie) {
	case NGM_PPTPGRE_COOKIE:
		switch (msg->header.cmd) {
		case NGM_PPTPGRE_SET_CONFIG:
		    {
			struct ng_pptpgre_conf *const newConf =
				(struct ng_pptpgre_conf *) msg->data;
			hpriv_p hpriv;
			uint16_t hash;

			/* Check for invalid or illegal config */
			if (msg->header.arglen != sizeof(*newConf))
				ERROUT(EINVAL);
			/* Try to find session by cid. */
			hpriv = ng_pptpgre_find_session(priv, newConf->cid);
			/* If not present - use upper. */
			if (hpriv == NULL) {
				hpriv = &priv->uppersess;
				LIST_REMOVE(hpriv, sessions);
				hash = SESSHASH(newConf->cid);
				LIST_INSERT_HEAD(&priv->sesshash[hash], hpriv,
				    sessions);
			}
			ng_pptpgre_reset(hpriv);	/* reset on configure */
			hpriv->conf = *newConf;
			break;
		    }
		case NGM_PPTPGRE_GET_CONFIG:
		    {
			hpriv_p hpriv;

			if (msg->header.arglen == 2) {
				/* Try to find session by cid. */
	    			hpriv = ng_pptpgre_find_session(priv,
				    *((uint16_t *)msg->data));
				if (hpriv == NULL)
					ERROUT(EINVAL);
			} else if (msg->header.arglen == 0) {
				/* Use upper. */
				hpriv = &priv->uppersess;
			} else
				ERROUT(EINVAL);
			NG_MKRESPONSE(resp, msg, sizeof(hpriv->conf), M_NOWAIT);
			if (resp == NULL)
				ERROUT(ENOMEM);
			bcopy(&hpriv->conf, resp->data, sizeof(hpriv->conf));
			break;
		    }
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
done:
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

/*
 * Receive incoming data on a hook.
 */
static int
ng_pptpgre_rcvdata(hook_p hook, item_p item)
{
	const hpriv_p hpriv = NG_HOOK_PRIVATE(hook);
	int rval;

	/* If not configured, reject */
	if (!hpriv->conf.enabled) {
		NG_FREE_ITEM(item);
		return (ENXIO);
	}

	mtx_lock(&hpriv->mtx);

	rval = ng_pptpgre_xmit(hpriv, item);

	mtx_assert(&hpriv->mtx, MA_NOTOWNED);

	return (rval);
}

/*
 * Hook disconnection
 */
static int
ng_pptpgre_disconnect(hook_p hook)
{
	const node_p node = NG_HOOK_NODE(hook);
	const priv_p priv = NG_NODE_PRIVATE(node);
	const hpriv_p hpriv = NG_HOOK_PRIVATE(hook);

	/* Zero out hook pointer */
	if (hook == priv->upper) {
		priv->upper = NULL;
		priv->uppersess.hook = NULL;
	} else if (hook == priv->lower) {
		priv->lower = NULL;
	} else {
		/* Reset node (stops timers) */
		ng_pptpgre_reset(hpriv);

		LIST_REMOVE(hpriv, sessions);
		mtx_destroy(&hpriv->mtx);
		free(hpriv, M_NETGRAPH);
	}

	/* Go away if no longer connected to anything */
	if ((NG_NODE_NUMHOOKS(node) == 0)
	&& (NG_NODE_IS_VALID(node)))
		ng_rmnode_self(node);
	return (0);
}

/*
 * Destroy node
 */
static int
ng_pptpgre_shutdown(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	/* Reset node (stops timers) */
	ng_pptpgre_reset(&priv->uppersess);

	LIST_REMOVE(&priv->uppersess, sessions);
	mtx_destroy(&priv->uppersess.mtx);

	free(priv, M_NETGRAPH);

	/* Decrement ref count */
	NG_NODE_UNREF(node);
	return (0);
}

/*************************************************************************
		    TRANSMIT AND RECEIVE FUNCTIONS
*************************************************************************/

/*
 * Transmit an outgoing frame, or just an ack if m is NULL.
 */
static int
ng_pptpgre_xmit(hpriv_p hpriv, item_p item)
{
	const priv_p priv = NG_NODE_PRIVATE(hpriv->node);
	u_char buf[sizeof(struct greheader) + 2 * sizeof(u_int32_t)];
	struct greheader *const gre = (struct greheader *)buf;
	int grelen, error;
	struct mbuf *m;

	mtx_assert(&hpriv->mtx, MA_OWNED);

	if (item) {
		NGI_GET_M(item, m);
	} else {
		m = NULL;
	}
	/* Check if there's data */
	if (m != NULL) {

		/* Check if windowing is enabled */
		if (hpriv->conf.enableWindowing) {
			/* Is our transmit window full? */
			if ((u_int32_t)PPTP_SEQ_DIFF(hpriv->xmitSeq,
			    hpriv->recvAck) >= hpriv->xmitWin) {
				priv->stats.xmitDrops++;
				ERROUT(ENOBUFS);
			}
		}

		/* Sanity check frame length */
		if (m->m_pkthdr.len > PPTP_MAX_PAYLOAD) {
			priv->stats.xmitTooBig++;
			ERROUT(EMSGSIZE);
		}
	} else {
		priv->stats.xmitLoneAcks++;
	}

	/* Build GRE header */
	be32enc(gre, PPTP_INIT_VALUE);
	be16enc(&gre->length, (m != NULL) ? m->m_pkthdr.len : 0);
	be16enc(&gre->cid, hpriv->conf.peerCid);

	/* Include sequence number if packet contains any data */
	if (m != NULL) {
		gre->hasSeq = 1;
		if (hpriv->conf.enableWindowing) {
			hpriv->timeSent[hpriv->xmitSeq - hpriv->recvAck]
			    = ng_pptpgre_time();
		}
		hpriv->xmitSeq++;
		be32enc(&gre->data[0], hpriv->xmitSeq);
	}

	/* Include acknowledgement (and stop send ack timer) if needed */
	if (hpriv->conf.enableAlwaysAck || hpriv->xmitAck != hpriv->recvSeq) {
		gre->hasAck = 1;
		be32enc(&gre->data[gre->hasSeq], hpriv->recvSeq);
		hpriv->xmitAck = hpriv->recvSeq;
		if (hpriv->conf.enableDelayedAck)
			ng_uncallout(&hpriv->sackTimer, hpriv->node);
	}

	/* Prepend GRE header to outgoing frame */
	grelen = sizeof(*gre) + sizeof(u_int32_t) * (gre->hasSeq + gre->hasAck);
	if (m == NULL) {
		MGETHDR(m, M_NOWAIT, MT_DATA);
		if (m == NULL) {
			priv->stats.memoryFailures++;
			ERROUT(ENOBUFS);
		}
		m->m_len = m->m_pkthdr.len = grelen;
		m->m_pkthdr.rcvif = NULL;
	} else {
		M_PREPEND(m, grelen, M_NOWAIT);
		if (m == NULL || (m->m_len < grelen
		    && (m = m_pullup(m, grelen)) == NULL)) {
			priv->stats.memoryFailures++;
			ERROUT(ENOBUFS);
		}
	}
	bcopy(gre, mtod(m, u_char *), grelen);

	/* Update stats */
	priv->stats.xmitPackets++;
	priv->stats.xmitOctets += m->m_pkthdr.len;

	/*
	 * XXX: we should reset timer only after an item has been sent
	 * successfully.
	 */
	if (hpriv->conf.enableWindowing &&
	    gre->hasSeq && hpriv->xmitSeq == hpriv->recvAck + 1)
		ng_pptpgre_start_recv_ack_timer(hpriv);

	mtx_unlock(&hpriv->mtx);

	/* Deliver packet */
	if (item) {
		NG_FWD_NEW_DATA(error, item, priv->lower, m);
	} else {
		NG_SEND_DATA_ONLY(error, priv->lower, m);
	}

	return (error);

done:
	mtx_unlock(&hpriv->mtx);
	NG_FREE_M(m);
	if (item)
		NG_FREE_ITEM(item);
	return (error);
}

/*
 * Handle an incoming packet.  The packet includes the IP header.
 */
static int
ng_pptpgre_rcvdata_lower(hook_p hook, item_p item)
{
	hpriv_p hpriv;
	node_p node = NG_HOOK_NODE(hook);
	const priv_p priv = NG_NODE_PRIVATE(node);
	int iphlen, grelen, extralen;
	const struct greheader *gre;
	const struct ip *ip;
	int error = 0;
	struct mbuf *m;

	NGI_GET_M(item, m);
	/* Update stats */
	priv->stats.recvPackets++;
	priv->stats.recvOctets += m->m_pkthdr.len;

	/* Sanity check packet length */
	if (m->m_pkthdr.len < sizeof(*ip) + sizeof(*gre)) {
		priv->stats.recvRunts++;
		ERROUT(EINVAL);
	}

	/* Safely pull up the complete IP+GRE headers */
	if (m->m_len < sizeof(*ip) + sizeof(*gre)
	    && (m = m_pullup(m, sizeof(*ip) + sizeof(*gre))) == NULL) {
		priv->stats.memoryFailures++;
		ERROUT(ENOBUFS);
	}
	ip = mtod(m, const struct ip *);
	iphlen = ip->ip_hl << 2;
	if (m->m_len < iphlen + sizeof(*gre)) {
		if ((m = m_pullup(m, iphlen + sizeof(*gre))) == NULL) {
			priv->stats.memoryFailures++;
			ERROUT(ENOBUFS);
		}
		ip = mtod(m, const struct ip *);
	}
	gre = (const struct greheader *)((const u_char *)ip + iphlen);
	grelen = sizeof(*gre) + sizeof(u_int32_t) * (gre->hasSeq + gre->hasAck);
	if (m->m_pkthdr.len < iphlen + grelen) {
		priv->stats.recvRunts++;
		ERROUT(EINVAL);
	}
	if (m->m_len < iphlen + grelen) {
		if ((m = m_pullup(m, iphlen + grelen)) == NULL) {
			priv->stats.memoryFailures++;
			ERROUT(ENOBUFS);
		}
		ip = mtod(m, const struct ip *);
		gre = (const struct greheader *)((const u_char *)ip + iphlen);
	}

	/* Sanity check packet length and GRE header bits */
	extralen = m->m_pkthdr.len
	    - (iphlen + grelen + gre->hasSeq * be16dec(&gre->length));
	if (extralen < 0) {
		priv->stats.recvBadGRE++;
		ERROUT(EINVAL);
	}
	if ((be32dec(gre) & PPTP_INIT_MASK) != PPTP_INIT_VALUE) {
		priv->stats.recvBadGRE++;
		ERROUT(EINVAL);
	}

	hpriv = ng_pptpgre_find_session(priv, be16dec(&gre->cid));
	if (hpriv == NULL || hpriv->hook == NULL || !hpriv->conf.enabled) {
		priv->stats.recvBadCID++;
		ERROUT(EINVAL);
	}
	mtx_lock(&hpriv->mtx);

	/* Look for peer ack */
	if (gre->hasAck) {
		const u_int32_t	ack = be32dec(&gre->data[gre->hasSeq]);
		const int index = ack - hpriv->recvAck - 1;
		long sample;
		long diff;

		/* Sanity check ack value */
		if (PPTP_SEQ_DIFF(ack, hpriv->xmitSeq) > 0) {
			priv->stats.recvBadAcks++;
			goto badAck;		/* we never sent it! */
		}
		if (PPTP_SEQ_DIFF(ack, hpriv->recvAck) <= 0)
			goto badAck;		/* ack already timed out */
		hpriv->recvAck = ack;

		/* Update adaptive timeout stuff */
		if (hpriv->conf.enableWindowing) {
			sample = ng_pptpgre_time() - hpriv->timeSent[index];
			diff = sample - hpriv->rtt;
			hpriv->rtt += PPTP_ACK_ALPHA(diff);
			if (diff < 0)
				diff = -diff;
			hpriv->dev += PPTP_ACK_BETA(diff - hpriv->dev);
			    /* +2 to compensate low precision of int math */
			hpriv->ato = hpriv->rtt + PPTP_ACK_CHI(hpriv->dev + 2);
			if (hpriv->ato > PPTP_MAX_TIMEOUT)
				hpriv->ato = PPTP_MAX_TIMEOUT;
			else if (hpriv->ato < PPTP_MIN_TIMEOUT)
				hpriv->ato = PPTP_MIN_TIMEOUT;

			/* Shift packet transmit times in our transmit window */
			bcopy(hpriv->timeSent + index + 1, hpriv->timeSent,
			    sizeof(*hpriv->timeSent)
			      * (PPTP_XMIT_WIN - (index + 1)));

			/* If we sent an entire window, increase window size */
			if (PPTP_SEQ_DIFF(ack, hpriv->winAck) >= 0
			    && hpriv->xmitWin < PPTP_XMIT_WIN) {
				hpriv->xmitWin++;
				hpriv->winAck = ack + hpriv->xmitWin;
			}

			/* Stop/(re)start receive ACK timer as necessary */
			ng_uncallout(&hpriv->rackTimer, hpriv->node);
			if (hpriv->recvAck != hpriv->xmitSeq)
				ng_pptpgre_start_recv_ack_timer(hpriv);
		}
	}
badAck:

	/* See if frame contains any data */
	if (gre->hasSeq) {
		const u_int32_t seq = be32dec(&gre->data[0]);

		/* Sanity check sequence number */
		if (PPTP_SEQ_DIFF(seq, hpriv->recvSeq) <= 0) {
			if (seq == hpriv->recvSeq)
				priv->stats.recvDuplicates++;
			else
				priv->stats.recvOutOfOrder++;
			mtx_unlock(&hpriv->mtx);
			ERROUT(EINVAL);
		}
		hpriv->recvSeq = seq;

		/* We need to acknowledge this packet; do it soon... */
		if (!(callout_pending(&hpriv->sackTimer))) {
			/* If delayed ACK is disabled, send it now */
			if (!hpriv->conf.enableDelayedAck) {	/* ack now */
				ng_pptpgre_xmit(hpriv, NULL);
				/* ng_pptpgre_xmit() drops the mutex */
			} else {				/* ack later */
				ng_pptpgre_start_send_ack_timer(hpriv);
				mtx_unlock(&hpriv->mtx);
			}
		} else
			mtx_unlock(&hpriv->mtx);

		/* Trim mbuf down to internal payload */
		m_adj(m, iphlen + grelen);
		if (extralen > 0)
			m_adj(m, -extralen);

		mtx_assert(&hpriv->mtx, MA_NOTOWNED);

		/* Deliver frame to upper layers */
		NG_FWD_NEW_DATA(error, item, hpriv->hook, m);
	} else {
		priv->stats.recvLoneAcks++;
		mtx_unlock(&hpriv->mtx);
		NG_FREE_ITEM(item);
		NG_FREE_M(m);		/* no data to deliver */
	}

	return (error);

done:
	NG_FREE_ITEM(item);
	NG_FREE_M(m);
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
ng_pptpgre_start_recv_ack_timer(hpriv_p hpriv)
{
	int remain, ticks;

	/* Compute how long until oldest unack'd packet times out,
	   and reset the timer to that time. */
	remain = (hpriv->timeSent[0] + hpriv->ato) - ng_pptpgre_time();
	if (remain < 0)
		remain = 0;

	/* Be conservative: timeout can happen up to 1 tick early */
	ticks = howmany(remain * hz, PPTP_TIME_SCALE) + 1;
	ng_callout(&hpriv->rackTimer, hpriv->node, hpriv->hook,
	    ticks, ng_pptpgre_recv_ack_timeout, hpriv, 0);
}

/*
 * The peer has failed to acknowledge the oldest unacknowledged sequence
 * number within the time allotted.  Update our adaptive timeout parameters
 * and reset/restart the recv ack timer.
 */
static void
ng_pptpgre_recv_ack_timeout(node_p node, hook_p hook, void *arg1, int arg2)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	const hpriv_p hpriv = arg1;

	/* Update adaptive timeout stuff */
	priv->stats.recvAckTimeouts++;
	hpriv->rtt = PPTP_ACK_DELTA(hpriv->rtt) + 1; /* +1 to avoid delta*0 case */
	hpriv->ato = hpriv->rtt + PPTP_ACK_CHI(hpriv->dev);
	if (hpriv->ato > PPTP_MAX_TIMEOUT)
		hpriv->ato = PPTP_MAX_TIMEOUT;
	else if (hpriv->ato < PPTP_MIN_TIMEOUT)
		hpriv->ato = PPTP_MIN_TIMEOUT;

	/* Reset ack and sliding window */
	hpriv->recvAck = hpriv->xmitSeq;		/* pretend we got the ack */
	hpriv->xmitWin = (hpriv->xmitWin + 1) / 2;	/* shrink transmit window */
	hpriv->winAck = hpriv->recvAck + hpriv->xmitWin;	/* reset win expand time */
}

/*
 * Start the send ack timer. This assumes the timer is not
 * already running.
 */
static void
ng_pptpgre_start_send_ack_timer(hpriv_p hpriv)
{
	int ackTimeout, ticks;

	/* Take 1/4 of the estimated round trip time */
	ackTimeout = (hpriv->rtt >> 2);
	if (ackTimeout < PPTP_MIN_ACK_DELAY)
		ackTimeout = PPTP_MIN_ACK_DELAY;
	else if (ackTimeout > PPTP_MAX_ACK_DELAY)
		ackTimeout = PPTP_MAX_ACK_DELAY;

	/* Be conservative: timeout can happen up to 1 tick early */
	ticks = howmany(ackTimeout * hz, PPTP_TIME_SCALE);
	ng_callout(&hpriv->sackTimer, hpriv->node, hpriv->hook,
	    ticks, ng_pptpgre_send_ack_timeout, hpriv, 0);
}

/*
 * We've waited as long as we're willing to wait before sending an
 * acknowledgement to the peer for received frames. We had hoped to
 * be able to piggy back our acknowledgement on an outgoing data frame,
 * but apparently there haven't been any since. So send the ack now.
 */
static void
ng_pptpgre_send_ack_timeout(node_p node, hook_p hook, void *arg1, int arg2)
{
	const hpriv_p hpriv = arg1;

	mtx_lock(&hpriv->mtx);
	/* Send a frame with an ack but no payload */
  	ng_pptpgre_xmit(hpriv, NULL);
	mtx_assert(&hpriv->mtx, MA_NOTOWNED);
}

/*************************************************************************
		    MISC FUNCTIONS
*************************************************************************/

/*
 * Find the hook with a given session ID.
 */
static hpriv_p
ng_pptpgre_find_session(priv_p privp, u_int16_t cid)
{
	uint16_t	hash = SESSHASH(cid);
	hpriv_p	hpriv = NULL;

	LIST_FOREACH(hpriv, &privp->sesshash[hash], sessions) {
		if (hpriv->conf.cid == cid)
			break;
	}

	return (hpriv);
}

/*
 * Reset state (must be called with lock held or from writer)
 */
static void
ng_pptpgre_reset(hpriv_p hpriv)
{
	/* Reset adaptive timeout state */
	hpriv->ato = PPTP_MAX_TIMEOUT;
	hpriv->rtt = PPTP_TIME_SCALE / 10;
	if (hpriv->conf.peerPpd > 1)	/* ppd = 0 treat as = 1 */
		hpriv->rtt *= hpriv->conf.peerPpd;
	hpriv->dev = 0;
	hpriv->xmitWin = (hpriv->conf.recvWin + 1) / 2;
	if (hpriv->xmitWin < 2)		/* often the first packet is lost */
		hpriv->xmitWin = 2;		/*   because the peer isn't ready */
	else if (hpriv->xmitWin > PPTP_XMIT_WIN)
		hpriv->xmitWin = PPTP_XMIT_WIN;
	hpriv->winAck = hpriv->xmitWin;

	/* Reset sequence numbers */
	hpriv->recvSeq = ~0;
	hpriv->recvAck = ~0;
	hpriv->xmitSeq = ~0;
	hpriv->xmitAck = ~0;

	/* Stop timers */
	ng_uncallout(&hpriv->sackTimer, hpriv->node);
	ng_uncallout(&hpriv->rackTimer, hpriv->node);
}

/*
 * Return the current time scaled & translated to our internally used format.
 */
static pptptime_t
ng_pptpgre_time(void)
{
	struct timeval tv;
	pptptime_t t;

	microuptime(&tv);
	t = (pptptime_t)tv.tv_sec * PPTP_TIME_SCALE;
	t += tv.tv_usec / (1000000 / PPTP_TIME_SCALE);
	return(t);
}
