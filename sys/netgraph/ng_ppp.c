
/*
 * ng_ppp.c
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
 * Author: Archie Cobbs <archie@whistle.com>
 *
 * $FreeBSD$
 * $Whistle: ng_ppp.c,v 1.24 1999/11/01 09:24:52 julian Exp $
 */

/*
 * PPP node type.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ctype.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_ppp.h>
#include <netgraph/ng_vjc.h>

#define PROT_VALID(p)		(((p) & 0x0101) == 0x0001)
#define PROT_COMPRESSABLE(p)	(((p) & 0xff00) == 0x0000)

/* Some PPP protocol numbers we're interested in */
#define PROT_APPLETALK		0x0029
#define PROT_COMPD		0x00fd
#define PROT_CRYPTD		0x0053
#define PROT_IP			0x0021
#define PROT_IPX		0x002b
#define PROT_MP			0x003d
#define PROT_VJCOMP		0x002d
#define PROT_VJUNCOMP		0x002f

/* Multilink PPP definitions */
#define MP_MIN_MRRU		1500		/* per RFC 1990 */
#define MP_INITIAL_SEQ		0		/* per RFC 1990 */
#define MP_MIN_LINK_MRU		32

#define MP_MAX_SEQ_LINGER	64		/* max frags we will hold */
#define MP_INSANE_SEQ_JUMP	128		/* a sequence # jump too far */
#define MP_MIN_FRAG_LEN		6		/* don't frag smaller frames */

#define MP_SHORT_SEQ_MASK	0x00000fff	/* short seq # mask */
#define MP_SHORT_SEQ_HIBIT	0x00000800	/* short seq # high bit */
#define MP_SHORT_FIRST_FLAG	0x00008000	/* first fragment in frame */
#define MP_SHORT_LAST_FLAG	0x00004000	/* last fragment in frame */

#define MP_LONG_SEQ_MASK	0x00ffffff	/* long seq # mask */
#define MP_LONG_SEQ_HIBIT	0x00800000	/* long seq # high bit */
#define MP_LONG_FIRST_FLAG	0x80000000	/* first fragment in frame */
#define MP_LONG_LAST_FLAG	0x40000000	/* last fragment in frame */

#define MP_SEQ_MASK		(priv->conf.recvShortSeq ? \
				    MP_SHORT_SEQ_MASK : MP_LONG_SEQ_MASK)

/* Sign extension of MP sequence numbers */
#define MP_SHORT_EXTEND(s)	(((s) & MP_SHORT_SEQ_HIBIT) ? \
				    ((s) | ~MP_SHORT_SEQ_MASK) : (s))
#define MP_LONG_EXTEND(s)	(((s) & MP_LONG_SEQ_HIBIT) ? \
				    ((s) | ~MP_LONG_SEQ_MASK) : (s))

/* Comparision of MP sequence numbers */
#define MP_SHORT_SEQ_DIFF(x,y)	(MP_SHORT_EXTEND(x) - MP_SHORT_EXTEND(y))
#define MP_LONG_SEQ_DIFF(x,y)	(MP_LONG_EXTEND(x) - MP_LONG_EXTEND(y))

#define MP_SEQ_DIFF(x,y)	(priv->conf.recvShortSeq ? \
				    MP_SHORT_SEQ_DIFF((x), (y)) : \
				    MP_LONG_SEQ_DIFF((x), (y)))

/* We store incoming fragments this way */
struct ng_ppp_frag {
	int				seq;
	u_char				first;
	u_char				last;
	struct mbuf			*data;
	meta_p				meta;
	CIRCLEQ_ENTRY(ng_ppp_frag)	f_qent;
};

/* We keep track of link queue status this way */
struct ng_ppp_link_qstat {
	struct timeval		lastWrite;	/* time of last write */
	int			bytesInQueue;	/* bytes in the queue then */
};

/* We use integer indicies to refer to the non-link hooks */
static const char *const ng_ppp_hook_names[] = {
	NG_PPP_HOOK_ATALK,
#define HOOK_INDEX_ATALK		0
	NG_PPP_HOOK_BYPASS,
#define HOOK_INDEX_BYPASS		1
	NG_PPP_HOOK_COMPRESS,
#define HOOK_INDEX_COMPRESS		2
	NG_PPP_HOOK_ENCRYPT,
#define HOOK_INDEX_ENCRYPT		3
	NG_PPP_HOOK_DECOMPRESS,
#define HOOK_INDEX_DECOMPRESS		4
	NG_PPP_HOOK_DECRYPT,
#define HOOK_INDEX_DECRYPT		5
	NG_PPP_HOOK_INET,
#define HOOK_INDEX_INET			6
	NG_PPP_HOOK_IPX,
#define HOOK_INDEX_IPX			7
	NG_PPP_HOOK_VJC_COMP,
#define HOOK_INDEX_VJC_COMP		8
	NG_PPP_HOOK_VJC_IP,
#define HOOK_INDEX_VJC_IP		9
	NG_PPP_HOOK_VJC_UNCOMP,
#define HOOK_INDEX_VJC_UNCOMP		10
	NG_PPP_HOOK_VJC_VJIP,
#define HOOK_INDEX_VJC_VJIP		11
	NULL
#define HOOK_INDEX_MAX			12
};

/* We store index numbers in the hook private pointer. The HOOK_INDEX()
   for a hook is either the index (above) for normal hooks, or the ones
   complement of the link number for link hooks. */
#define HOOK_INDEX(hook)	(*((int16_t *) &(hook)->private))

/* Node private data */
struct private {
	struct ng_ppp_node_config	conf;
	struct ng_ppp_link_stat		bundleStats;
	struct ng_ppp_link_stat		linkStats[NG_PPP_MAX_LINKS];
	hook_p				links[NG_PPP_MAX_LINKS];
	hook_p				hooks[HOOK_INDEX_MAX];
	u_char				vjCompHooked;
	u_char				allLinksEqual;
	u_short				activeLinks[NG_PPP_MAX_LINKS];
	u_int				numActiveLinks;
	u_int				lastLink;	/* for round robin */
	struct ng_ppp_link_qstat	qstat[NG_PPP_MAX_LINKS];
	CIRCLEQ_HEAD(ng_ppp_fraglist, ng_ppp_frag)
					frags;		/* incoming fragments */
	int				mpSeqOut;	/* next out MP seq # */
};
typedef struct private *priv_p;

/* Netgraph node methods */
static ng_constructor_t	ng_ppp_constructor;
static ng_rcvmsg_t	ng_ppp_rcvmsg;
static ng_shutdown_t	ng_ppp_rmnode;
static ng_newhook_t	ng_ppp_newhook;
static ng_rcvdata_t	ng_ppp_rcvdata;
static ng_disconnect_t	ng_ppp_disconnect;

/* Helper functions */
static int	ng_ppp_input(node_p node, int bypass,
			int linkNum, struct mbuf *m, meta_p meta);
static int	ng_ppp_output(node_p node, int bypass,
			int linkNum, struct mbuf *m, meta_p meta);
static int	ng_ppp_mp_input(node_p node, int linkNum,
			struct mbuf *m, meta_p meta);
static int	ng_ppp_mp_output(node_p node, struct mbuf *m, meta_p meta);
static void	ng_ppp_mp_strategy(node_p node, int len, int *distrib);
static int	ng_ppp_intcmp(const void *v1, const void *v2);
static struct	mbuf *ng_ppp_addproto(struct mbuf *m, int proto, int compOK);
static int	ng_ppp_config_valid(node_p node,
			const struct ng_ppp_node_config *newConf);
static void	ng_ppp_update(node_p node, int newConf);
static void	ng_ppp_free_frags(node_p node);

/* Node type descriptor */
static struct ng_type ng_ppp_typestruct = {
	NG_VERSION,
	NG_PPP_NODE_TYPE,
	NULL,
	ng_ppp_constructor,
	ng_ppp_rcvmsg,
	ng_ppp_rmnode,
	ng_ppp_newhook,
	NULL,
	NULL,
	ng_ppp_rcvdata,
	ng_ppp_rcvdata,
	ng_ppp_disconnect
};
NETGRAPH_INIT(ppp, &ng_ppp_typestruct);

static int	*compareLatencies;		/* hack for ng_ppp_intcmp() */

#define ERROUT(x)	do { error = (x); goto done; } while (0)

/************************************************************************
			NETGRAPH NODE STUFF
 ************************************************************************/

/*
 * Node type constructor
 */
static int
ng_ppp_constructor(node_p *nodep)
{
	priv_p priv;
	int error;

	/* Allocate private structure */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_WAITOK);
	if (priv == NULL)
		return (ENOMEM);
	bzero(priv, sizeof(*priv));

	/* Call generic node constructor */
	if ((error = ng_make_node_common(&ng_ppp_typestruct, nodep))) {
		FREE(priv, M_NETGRAPH);
		return (error);
	}
	(*nodep)->private = priv;

	/* Initialize state */
	CIRCLEQ_INIT(&priv->frags);

	/* Done */
	return (0);
}

/*
 * Give our OK for a hook to be added
 */
static int
ng_ppp_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = node->private;
	int linkNum = -1;
	hook_p *hookPtr = NULL;
	int hookIndex = -1;

	/* Figure out which hook it is */
	if (strncmp(name, NG_PPP_HOOK_LINK_PREFIX,	/* a link hook? */
	    strlen(NG_PPP_HOOK_LINK_PREFIX)) == 0) {
		const char *cp, *eptr;

		cp = name + strlen(NG_PPP_HOOK_LINK_PREFIX);
		if (!isdigit(*cp) || (cp[0] == '0' && cp[1] != '\0'))
			return (EINVAL);
		linkNum = (int)strtoul(cp, &eptr, 10);
		if (*eptr != '\0' || linkNum < 0 || linkNum >= NG_PPP_MAX_LINKS)
			return (EINVAL);
		hookPtr = &priv->links[linkNum];
		hookIndex = ~linkNum;
	} else {				/* must be a non-link hook */
		int i;

		for (i = 0; ng_ppp_hook_names[i] != NULL; i++) {
			if (strcmp(name, ng_ppp_hook_names[i]) == 0) {
				hookPtr = &priv->hooks[i];
				hookIndex = i;
				break;
			}
		}
		if (ng_ppp_hook_names[i] == NULL)
			return (EINVAL);	/* no such hook */
	}

	/* See if hook is already connected */
	if (*hookPtr != NULL)
		return (EISCONN);

	/* Disallow more than one link unless multilink is enabled */
	if (linkNum != -1 && priv->conf.links[linkNum].enableLink
	    && !priv->conf.enableMultilink && priv->numActiveLinks >= 1)
		return (ENODEV);

	/* OK */
	*hookPtr = hook;
	HOOK_INDEX(hook) = hookIndex;
	ng_ppp_update(node, 0);
	return (0);
}

/*
 * Receive a control message
 */
static int
ng_ppp_rcvmsg(node_p node, struct ng_mesg *msg,
	      const char *raddr, struct ng_mesg **rptr)
{
	const priv_p priv = node->private;
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_PPP_COOKIE:
		switch (msg->header.cmd) {
		case NGM_PPP_SET_CONFIG:
		    {
			struct ng_ppp_node_config *const newConf =
				(struct ng_ppp_node_config *) msg->data;

			/* Check for invalid or illegal config */
			if (msg->header.arglen != sizeof(*newConf))
				ERROUT(EINVAL);
			if (!ng_ppp_config_valid(node, newConf))
				ERROUT(EINVAL);
			priv->conf = *newConf;
			ng_ppp_update(node, 1);
			break;
		    }
		case NGM_PPP_GET_CONFIG:
			NG_MKRESPONSE(resp, msg, sizeof(priv->conf), M_NOWAIT);
			if (resp == NULL)
				ERROUT(ENOMEM);
			bcopy(&priv->conf, resp->data, sizeof(priv->conf));
			break;
		case NGM_PPP_GET_LINK_STATS:
		case NGM_PPP_CLR_LINK_STATS:
		case NGM_PPP_GETCLR_LINK_STATS:
		    {
			struct ng_ppp_link_stat *stats;
			u_int16_t linkNum;

			if (msg->header.arglen != sizeof(u_int16_t))
				ERROUT(EINVAL);
			linkNum = *((u_int16_t *) msg->data);
			if (linkNum >= NG_PPP_MAX_LINKS
			    && linkNum != NG_PPP_BUNDLE_LINKNUM)
				ERROUT(EINVAL);
			stats = (linkNum == NG_PPP_BUNDLE_LINKNUM) ?
				&priv->bundleStats : &priv->linkStats[linkNum];
			if (msg->header.cmd != NGM_PPP_CLR_LINK_STATS) {
				NG_MKRESPONSE(resp, msg,
				    sizeof(struct ng_ppp_link_stat), M_NOWAIT);
				if (resp == NULL)
					ERROUT(ENOMEM);
				bcopy(stats, resp->data, sizeof(*stats));
			}
			if (msg->header.cmd != NGM_PPP_GET_LINK_STATS)
				bzero(stats, sizeof(*stats));
			break;
		    }
		default:
			error = EINVAL;
			break;
		}
		break;
	case NGM_VJC_COOKIE:
	   {
	       char path[NG_PATHLEN + 1];
	       node_p origNode;

	       if ((error = ng_path2node(node, raddr, &origNode, NULL)) != 0)
		       ERROUT(error);
	       snprintf(path, sizeof(path), "[%lx]:%s",
		   (long) node, NG_PPP_HOOK_VJC_IP);
	       return ng_send_msg(origNode, msg, path, rptr);
	       break;
	   }
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
 * Receive data on a hook
 */
static int
ng_ppp_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const node_p node = hook->node;
	const priv_p priv = node->private;
	const int index = HOOK_INDEX(hook);
	u_int16_t linkNum = NG_PPP_BUNDLE_LINKNUM;
	hook_p outHook = NULL;
	int proto = 0, error;

	/* Did it come from a link hook? */
	if (index < 0) {

		/* Convert index into a link number */
		linkNum = (u_int16_t)~index;
		KASSERT(linkNum < NG_PPP_MAX_LINKS,
		    ("%s: bogus index 0x%x", __FUNCTION__, index));

		/* Stats */
		priv->linkStats[linkNum].recvFrames++;
		priv->linkStats[linkNum].recvOctets += m->m_pkthdr.len;

		/* Dispatch incoming frame (if not enabled, to bypass) */
		return ng_ppp_input(node,
		    !priv->conf.links[linkNum].enableLink, linkNum, m, meta);
	}

	/* Get protocol & check if data allowed from this hook */
	switch (index) {

	/* Outgoing data */
	case HOOK_INDEX_ATALK:
		if (!priv->conf.enableAtalk) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		proto = PROT_APPLETALK;
		break;
	case HOOK_INDEX_IPX:
		if (!priv->conf.enableIPX) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		proto = PROT_IPX;
		break;
	case HOOK_INDEX_INET:
	case HOOK_INDEX_VJC_VJIP:
		if (!priv->conf.enableIP) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		proto = PROT_IP;
		break;
	case HOOK_INDEX_VJC_COMP:
		if (!priv->conf.enableVJCompression) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		proto = PROT_VJCOMP;
		break;
	case HOOK_INDEX_VJC_UNCOMP:
		if (!priv->conf.enableVJCompression) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		proto = PROT_VJUNCOMP;
		break;
	case HOOK_INDEX_COMPRESS:
		if (!priv->conf.enableCompression) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		proto = PROT_COMPD;
		break;
	case HOOK_INDEX_ENCRYPT:
		if (!priv->conf.enableEncryption) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		proto = PROT_CRYPTD;
		break;
	case HOOK_INDEX_BYPASS:
		if (m->m_pkthdr.len < 4) {
			NG_FREE_META(meta);
			return (EINVAL);
		}
		if (m->m_len < 4 && (m = m_pullup(m, 4)) == NULL) {
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		linkNum = ntohs(mtod(m, u_int16_t *)[0]);
		proto = ntohs(mtod(m, u_int16_t *)[1]);
		m_adj(m, 4);
		if (linkNum >= NG_PPP_MAX_LINKS
		    && linkNum != NG_PPP_BUNDLE_LINKNUM)
			return (EINVAL);
		break;

	/* Incoming data */
	case HOOK_INDEX_VJC_IP:
		if (!priv->conf.enableIP || !priv->conf.enableVJDecompression) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		break;
	case HOOK_INDEX_DECOMPRESS:
		if (!priv->conf.enableDecompression) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		break;
	case HOOK_INDEX_DECRYPT:
		if (!priv->conf.enableDecryption) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		break;
	default:
		panic("%s: bogus index 0x%x", __FUNCTION__, index);
	}

	/* Now figure out what to do with the frame */
	switch (index) {

	/* Outgoing data */
	case HOOK_INDEX_INET:
		if (priv->conf.enableVJCompression && priv->vjCompHooked) {
			outHook = priv->hooks[HOOK_INDEX_VJC_IP];
			break;
		}
		/* FALLTHROUGH */
	case HOOK_INDEX_ATALK:
	case HOOK_INDEX_IPX:
	case HOOK_INDEX_VJC_COMP:
	case HOOK_INDEX_VJC_UNCOMP:
	case HOOK_INDEX_VJC_VJIP:
		if (priv->conf.enableCompression
		    && priv->hooks[HOOK_INDEX_COMPRESS] != NULL) {
			if ((m = ng_ppp_addproto(m, proto, 1)) == NULL) {
				NG_FREE_META(meta);
				return (ENOBUFS);
			}
			outHook = priv->hooks[HOOK_INDEX_COMPRESS];
			break;
		}
		/* FALLTHROUGH */
	case HOOK_INDEX_COMPRESS:
		if (priv->conf.enableEncryption
		    && priv->hooks[HOOK_INDEX_ENCRYPT] != NULL) {
			if ((m = ng_ppp_addproto(m, proto, 1)) == NULL) {
				NG_FREE_META(meta);
				return (ENOBUFS);
			}
			outHook = priv->hooks[HOOK_INDEX_ENCRYPT];
			break;
		}
		/* FALLTHROUGH */
	case HOOK_INDEX_ENCRYPT:
	case HOOK_INDEX_BYPASS:
		if ((m = ng_ppp_addproto(m, proto,
		    linkNum == NG_PPP_BUNDLE_LINKNUM
		      || priv->conf.links[linkNum].enableProtoComp)) == NULL) {
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		return ng_ppp_output(node,
		    index == HOOK_INDEX_BYPASS, NG_PPP_BUNDLE_LINKNUM, m, meta);

	/* Incoming data */
	case HOOK_INDEX_DECRYPT:
	case HOOK_INDEX_DECOMPRESS:
		return ng_ppp_input(node, 0, NG_PPP_BUNDLE_LINKNUM, m, meta);

	case HOOK_INDEX_VJC_IP:
		outHook = priv->hooks[HOOK_INDEX_INET];
		break;
	}

	/* Send packet out hook */
	NG_SEND_DATA(error, outHook, m, meta);
	return (error);
}

/*
 * Destroy node
 */
static int
ng_ppp_rmnode(node_p node)
{
	const priv_p priv = node->private;

	/* Take down netgraph node */
	node->flags |= NG_INVALID;
	ng_cutlinks(node);
	ng_unname(node);
	ng_ppp_free_frags(node);
	bzero(priv, sizeof(*priv));
	FREE(priv, M_NETGRAPH);
	node->private = NULL;
	ng_unref(node);		/* let the node escape */
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_ppp_disconnect(hook_p hook)
{
	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	return (0);
}

/************************************************************************
			HELPER STUFF
 ************************************************************************/

/*
 * Handle an incoming frame.  Extract the PPP protocol number
 * and dispatch accordingly.
 */
static int
ng_ppp_input(node_p node, int bypass, int linkNum, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;
	hook_p outHook = NULL;
	int proto, error;

	/* Extract protocol number */
	for (proto = 0; !PROT_VALID(proto) && m->m_pkthdr.len > 0; ) {
		if (m->m_len < 1 && (m = m_pullup(m, 1)) == NULL) {
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		proto = (proto << 8) + *mtod(m, u_char *);
		m_adj(m, 1);
	}
	if (!PROT_VALID(proto)) {
		if (linkNum == NG_PPP_BUNDLE_LINKNUM)
			priv->bundleStats.badProtos++;
		else
			priv->linkStats[linkNum].badProtos++;
		NG_FREE_DATA(m, meta);
		return (EINVAL);
	}

	/* Bypass frame? */
	if (bypass)
		goto bypass;

	/* Check protocol */
	switch (proto) {
	case PROT_COMPD:
		if (priv->conf.enableDecompression)
			outHook = priv->hooks[HOOK_INDEX_DECOMPRESS];
		break;
	case PROT_CRYPTD:
		if (priv->conf.enableDecryption)
			outHook = priv->hooks[HOOK_INDEX_DECRYPT];
		break;
	case PROT_VJCOMP:
		if (priv->conf.enableVJDecompression && priv->vjCompHooked)
			outHook = priv->hooks[HOOK_INDEX_VJC_COMP];
		break;
	case PROT_VJUNCOMP:
		if (priv->conf.enableVJDecompression && priv->vjCompHooked)
			outHook = priv->hooks[HOOK_INDEX_VJC_UNCOMP];
		break;
	case PROT_MP:
		if (priv->conf.enableMultilink) {
			NG_FREE_META(meta);
			return ng_ppp_mp_input(node, linkNum, m, meta);
		}
		break;
	case PROT_APPLETALK:
		if (priv->conf.enableAtalk)
			outHook = priv->hooks[HOOK_INDEX_ATALK];
		break;
	case PROT_IPX:
		if (priv->conf.enableIPX)
			outHook = priv->hooks[HOOK_INDEX_IPX];
		break;
	case PROT_IP:
		if (priv->conf.enableIP)
			outHook = priv->hooks[HOOK_INDEX_INET];
		break;
	}

	/* For unknown/inactive protocols, forward out the bypass hook */
bypass:
	if (outHook == NULL) {
		M_PREPEND(m, 4, M_NOWAIT);
		if (m == NULL || (m->m_len < 4 && (m = m_pullup(m, 4)) == NULL))
			return (ENOBUFS);
		mtod(m, u_int16_t *)[0] = htons(linkNum);
		mtod(m, u_int16_t *)[1] = htons(proto);
		outHook = priv->hooks[HOOK_INDEX_BYPASS];
	}

	/* Forward frame */
	NG_SEND_DATA(error, outHook, m, meta);
	return (error);
}

/*
 * Deliver a frame out a link, either a real one or NG_PPP_BUNDLE_LINKNUM
 */
static int
ng_ppp_output(node_p node, int bypass, int linkNum, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;
	int len, error;

	/* Check for bundle virtual link */
	if (linkNum == NG_PPP_BUNDLE_LINKNUM) {
		if (priv->conf.enableMultilink)
			return ng_ppp_mp_output(node, m, meta);
		linkNum = priv->activeLinks[0];
	}

	/* Check link status */
	if (!bypass && !priv->conf.links[linkNum].enableLink)
		return (ENXIO);
	if (priv->links[linkNum] == NULL) {
		NG_FREE_DATA(m, meta);
		return (ENETDOWN);
	}

	/* Deliver frame */
	len = m->m_pkthdr.len;
	NG_SEND_DATA(error, priv->links[linkNum], m, meta);

	/* Update stats and 'bytes in queue' counter */
	if (error == 0) {
		priv->linkStats[linkNum].xmitFrames++;
		priv->linkStats[linkNum].xmitOctets += len;
		priv->qstat[linkNum].bytesInQueue += len;
		microtime(&priv->qstat[linkNum].lastWrite);
	}
	return error;
}

/*
 * Handle an incoming multi-link fragment
 */
static int
ng_ppp_mp_input(node_p node, int linkNum, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;
	struct ng_ppp_frag frag0, *frag = &frag0;
	struct ng_ppp_frag *qent, *qnext;
	struct ng_ppp_frag *first = NULL, *last = NULL;
	int diff, highSeq, nextSeq;
	struct mbuf *tail;

	/* Extract fragment information from MP header */
	if (priv->conf.recvShortSeq) {
		u_int16_t shdr;

		if (m->m_pkthdr.len < 2) {
			NG_FREE_DATA(m, meta);
			return (EINVAL);
		}
		if (m->m_len < 2 && (m = m_pullup(m, 2)) == NULL) {
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		shdr = ntohs(*mtod(m, u_int16_t *));
		frag->seq = shdr & MP_SHORT_SEQ_MASK;
		frag->first = (shdr & MP_SHORT_FIRST_FLAG) != 0;
		frag->last = (shdr & MP_SHORT_LAST_FLAG) != 0;
		highSeq = CIRCLEQ_EMPTY(&priv->frags) ?
		    frag->seq : CIRCLEQ_FIRST(&priv->frags)->seq;
		diff = MP_SHORT_SEQ_DIFF(frag->seq, highSeq);
		m_adj(m, 2);
	} else {
		u_int32_t lhdr;

		if (m->m_pkthdr.len < 4) {
			NG_FREE_DATA(m, meta);
			return (EINVAL);
		}
		if (m->m_len < 4 && (m = m_pullup(m, 4)) == NULL) {
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		lhdr = ntohl(*mtod(m, u_int32_t *));
		frag->seq = lhdr & MP_LONG_SEQ_MASK;
		frag->first = (lhdr & MP_LONG_FIRST_FLAG) != 0;
		frag->last = (lhdr & MP_LONG_LAST_FLAG) != 0;
		highSeq = CIRCLEQ_EMPTY(&priv->frags) ?
		    frag->seq : CIRCLEQ_FIRST(&priv->frags)->seq;
		diff = MP_LONG_SEQ_DIFF(frag->seq, highSeq);
		m_adj(m, 4);
	}
	frag->data = m;
	frag->meta = meta;

	/* If the sequence number makes a large jump, empty the queue */
	if (diff <= -MP_INSANE_SEQ_JUMP || diff >= MP_INSANE_SEQ_JUMP)
		ng_ppp_free_frags(node);

	/* Optimization: handle a frame that's all in one fragment */
	if (frag->first && frag->last)
		return ng_ppp_input(node, 0, NG_PPP_BUNDLE_LINKNUM, m, meta);

	/* Allocate a new frag struct for the queue */
	MALLOC(frag, struct ng_ppp_frag *, sizeof(*frag), M_NETGRAPH, M_NOWAIT);
	if (frag == NULL) {
		NG_FREE_DATA(m, meta);
		return (ENOMEM);
	}
	*frag = frag0;
	meta = NULL;
	m = NULL;

	/* Add fragment to queue, which is reverse sorted by sequence number */
	CIRCLEQ_FOREACH(qent, &priv->frags, f_qent) {
		diff = MP_SEQ_DIFF(frag->seq, qent->seq);
		if (diff > 0) {
			CIRCLEQ_INSERT_BEFORE(&priv->frags, qent, frag, f_qent);
			break;
		} else if (diff == 0) {	     /* should never happen! */
			log(LOG_ERR, "%s: rec'd dup MP fragment\n", node->name);
			if (linkNum == NG_PPP_BUNDLE_LINKNUM)
				priv->linkStats[linkNum].dupFragments++;
			else
				priv->bundleStats.dupFragments++;
			NG_FREE_DATA(frag->data, frag->meta);
			FREE(frag, M_NETGRAPH);
			return (EINVAL);
		}
	}
	if (qent == NULL)
		CIRCLEQ_INSERT_TAIL(&priv->frags, frag, f_qent);

	/* Find the first fragment in the possibly newly completed frame */
	for (nextSeq = frag->seq, qent = frag;
	    qent != (void *) &priv->frags;
	    qent = CIRCLEQ_PREV(qent, f_qent)) {
		if (qent->seq != nextSeq)
			goto pruneQueue;
		if (qent->first) {
			first = qent;
			break;
		}
		nextSeq = (nextSeq + 1) & MP_SEQ_MASK;
	}

	/* Find the last fragment in the possibly newly completed frame */
	for (nextSeq = frag->seq, qent = frag;
	    qent != (void *) &priv->frags;
	    qent = CIRCLEQ_NEXT(qent, f_qent)) {
		if (qent->seq != nextSeq)
			goto pruneQueue;
		if (qent->last) {
			last = qent;
			break;
		}
		nextSeq = (nextSeq - 1) & MP_SEQ_MASK;
	}

	/* We have a complete frame, extract it from the queue */
	for (tail = NULL, qent = first; qent != NULL; qent = qnext) {
		qnext = CIRCLEQ_PREV(qent, f_qent);
		CIRCLEQ_REMOVE(&priv->frags, qent, f_qent);
		if (tail == NULL) {
			tail = m = qent->data;
			meta = qent->meta;	/* inherit first frag's meta */
		} else {
			m->m_pkthdr.len += qent->data->m_pkthdr.len;
			tail->m_next = qent->data;
			NG_FREE_META(qent->meta); /* drop other frag's metas */
		}
		while (tail->m_next != NULL)
			tail = tail->m_next;
		if (qent == last)
			qnext = NULL;
		FREE(qent, M_NETGRAPH);
	}

pruneQueue:
	/* Prune out stale entries in the queue */
	for (qent = CIRCLEQ_LAST(&priv->frags); 
	    qent != (void *) &priv->frags; qent = qnext) {
		if (MP_SEQ_DIFF(highSeq, qent->seq) <= MP_MAX_SEQ_LINGER)
			break;
		qnext = CIRCLEQ_PREV(qent, f_qent);
		CIRCLEQ_REMOVE(&priv->frags, qent, f_qent);
		NG_FREE_DATA(qent->data, qent->meta);
		FREE(qent, M_NETGRAPH);
	}

	/* Deliver newly completed frame, if any */
	return m ? ng_ppp_input(node, 0, NG_PPP_BUNDLE_LINKNUM, m, meta) : 0;
}

/*
 * Deliver a frame out on the bundle, i.e., figure out how to fragment
 * the frame across the individual PPP links and do so.
 */
static int
ng_ppp_mp_output(node_p node, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;
	int distrib[NG_PPP_MAX_LINKS];
	int firstFragment;
	int activeLinkNum;

	/* At least one link must be active */
	if (priv->numActiveLinks == 0) {
		NG_FREE_DATA(m, meta);
		return (ENETDOWN);
	}

	/* Round-robin strategy */
	if (priv->conf.enableRoundRobin || m->m_pkthdr.len < MP_MIN_FRAG_LEN) {
		activeLinkNum = priv->lastLink++ % priv->numActiveLinks;
		bzero(&distrib, priv->numActiveLinks * sizeof(distrib[0]));
		distrib[activeLinkNum] = m->m_pkthdr.len;
		goto deliver;
	}

	/* Strategy when all links are equivalent (optimize the common case) */
	if (priv->allLinksEqual) {
		const int fraction = m->m_pkthdr.len / priv->numActiveLinks;
		int i, remain;

		for (i = 0; i < priv->numActiveLinks; i++)
			distrib[priv->lastLink++ % priv->numActiveLinks]
			    = fraction;
		remain = m->m_pkthdr.len - (fraction * priv->numActiveLinks);
		while (remain > 0) {
			distrib[priv->lastLink++ % priv->numActiveLinks]++;
			remain--;
		}
		goto deliver;
	}

	/* Strategy when all links are not equivalent */
	ng_ppp_mp_strategy(node, m->m_pkthdr.len, distrib);

deliver:
	/* Update stats */
	priv->bundleStats.xmitFrames++;
	priv->bundleStats.xmitOctets += m->m_pkthdr.len;

	/* Send alloted portions of frame out on the link(s) */
	for (firstFragment = 1, activeLinkNum = priv->numActiveLinks - 1;
	    activeLinkNum >= 0; activeLinkNum--) {
		const int linkNum = priv->activeLinks[activeLinkNum];

		/* Deliver fragment(s) out the next link */
		for ( ; distrib[activeLinkNum] > 0; firstFragment = 0) {
			int len, lastFragment, error;
			struct mbuf *m2;
			meta_p meta2;

			/* Calculate fragment length; don't exceed link MTU */
			len = distrib[activeLinkNum];
			if (len > priv->conf.links[linkNum].mru)
				len = priv->conf.links[linkNum].mru;
			distrib[activeLinkNum] -= len;
			lastFragment = (len == m->m_pkthdr.len);

			/* Split off next fragment as "m2" */
			m2 = m;
			if (!lastFragment) {
				struct mbuf *n = m_split(m, len, M_NOWAIT);

				if (n == NULL) {
					NG_FREE_DATA(m, meta);
					return (ENOMEM);
				}
				m = n;
			}

			/* Prepend MP header */
			if (priv->conf.xmitShortSeq) {
				u_int16_t shdr;

				M_PREPEND(m2, 2, M_NOWAIT);
				if (m2 == NULL
				    || (m2->m_len < 2
				      && (m2 = m_pullup(m2, 2)) == NULL)) {
					if (!lastFragment)
						m_freem(m);
					NG_FREE_META(meta);
					return (ENOBUFS);
				}
				shdr = priv->mpSeqOut;
				priv->mpSeqOut =
				    (priv->mpSeqOut + 1) % MP_SHORT_SEQ_MASK;
				if (firstFragment)
					shdr |= MP_SHORT_FIRST_FLAG;
				if (lastFragment)
					shdr |= MP_SHORT_LAST_FLAG;
				*mtod(m2, u_int16_t *) = htons(shdr);
			} else {
				u_int32_t lhdr;

				M_PREPEND(m2, 4, M_NOWAIT);
				if (m2 == NULL
				    || (m2->m_len < 4
				      && (m2 = m_pullup(m2, 4)) == NULL)) {
					if (!lastFragment)
						m_freem(m);
					NG_FREE_META(meta);
					return (ENOBUFS);
				}
				lhdr = priv->mpSeqOut;
				priv->mpSeqOut =
				    (priv->mpSeqOut + 1) % MP_LONG_SEQ_MASK;
				if (firstFragment)
					lhdr |= MP_LONG_FIRST_FLAG;
				if (lastFragment)
					lhdr |= MP_LONG_LAST_FLAG;
				*mtod(m2, u_int32_t *) = htonl(lhdr);
			}

			/* Add MP protocol number */
			m2 = ng_ppp_addproto(m, PROT_MP,
			    priv->conf.links[linkNum].enableProtoComp);
			if (m2 == NULL) {
				if (!lastFragment)
					m_freem(m);
				NG_FREE_META(meta);
				return (ENOBUFS);
			}

			/* Copy the meta information, if any */
			if (meta != NULL && !lastFragment) {
				MALLOC(meta2, meta_p,
				    meta->used_len, M_NETGRAPH, M_NOWAIT);
				if (meta2 == NULL) {
					m_freem(m2);
					NG_FREE_DATA(m, meta);
					return (ENOMEM);
				}
				meta2->allocated_len = meta->used_len;
				bcopy(meta, meta2, meta->used_len);
			} else
				meta2 = meta;

			/* Send fragment */
			error = ng_ppp_output(node, 0, linkNum, m2, meta2);

			/* Abort for error */
			if (error != 0) {
				if (!lastFragment)
					NG_FREE_DATA(m, meta);
				return (error);
			}
		}
	}

	/* Done */
	return (0);
}

/*
 * Computing the optimal fragmentation
 * -----------------------------------
 *
 * This routine tries to compute the optimal fragmentation pattern based
 * on each link's latency, bandwidth, and calculated additional latency.
 * The latter quantity is the additional latency caused by previously
 * written data that has not been transmitted yet.
 *
 * This algorithm is only useful when not all of the links have the
 * same latency and bandwidth values.
 *
 * The essential idea is to make the last bit of each fragment of the
 * frame arrive at the opposite end at the exact same time. This greedy
 * algorithm is optimal, in that no other scheduling could result in any
 * packet arriving any sooner unless packets are delivered out of order.
 *
 * Suppose link i has bandwidth b_i (in tens of bytes per milisecond) and
 * latency l_i (in miliseconds). Consider the function function f_i(t)
 * which is equal to the number of bytes that will have arrived at
 * the peer after t miliseconds if we start writing continuously at
 * time t = 0. Then f_i(t) = b_i * (t - l_i) = ((b_i * t) - (l_i * b_i).
 * That is, f_i(t) is a line with slope b_i and y-intersect -(l_i * b_i).
 * Note that the y-intersect is always <= zero because latency can't be
 * negative.  Note also that really the function is f_i(t) except when
 * f_i(t) is negative, in which case the function is zero.  To take
 * care of this, let Q_i(t) = { if (f_i(t) > 0) return 1; else return 0; }.
 * So the actual number of bytes that will have arrived at the peer after
 * t miliseconds is f_i(t) * Q_i(t).
 *
 * At any given time, each link has some additional latency a_i >= 0
 * due to previously written fragment(s) which are still in the queue.
 * This value is easily computed from the time since last transmission,
 * the previous latency value, the number of bytes written, and the
 * link's bandwidth.
 *
 * Assume that l_i includes any a_i already, and that the links are
 * sorted by latency, so that l_i <= l_{i+1}.
 *
 * Let N be the total number of bytes in the current frame we are sending.
 *
 * Suppose we were to start writing bytes at time t = 0 on all links
 * simultaneously, which is the most we can possibly do.  Then let
 * F(t) be equal to the total number of bytes received by the peer
 * after t miliseconds. Then F(t) = Sum_i (f_i(t) * Q_i(t)).
 *
 * Our goal is simply this: fragment the frame across the links such
 * that the peer is able to reconstruct the completed frame as soon as
 * possible, i.e., at the least possible value of t. Call this value t_0.
 *
 * Then it follows that F(t_0) = N. Our strategy is first to find the value
 * of t_0, and then deduce how many bytes to write to each link.
 *
 * Rewriting F(t_0):
 *
 *   t_0 = ( N + Sum_i ( l_i * b_i * Q_i(t_0) ) ) / Sum_i ( b_i * Q_i(t_0) )
 *
 * Now, we note that Q_i(t) is constant for l_i <= t <= l_{i+1}. t_0 will
 * lie in one of these ranges.  To find it, we just need to find the i such
 * that F(l_i) <= N <= F(l_{i+1}).  Then we compute all the constant values
 * for Q_i() in this range, plug in the remaining values, solving for t_0.
 *
 * Once t_0 is known, then the number of bytes to send on link i is
 * just f_i(t_0) * Q_i(t_0).
 *
 * In other words, we start allocating bytes to the links one at a time.
 * We keep adding links until the frame is completely sent.  Some links
 * may not get any bytes because their latency is too high.
 *
 * Is all this work really worth the trouble?  Depends on the situation.
 * The bigger the ratio of computer speed to link speed, and the more
 * important total bundle latency is (e.g., for interactive response time),
 * the more it's worth it.  There is however the cost of calling this
 * function for every frame.  The running time is O(n^2) where n is the
 * number of links that receive a non-zero number of bytes.
 *
 * Since latency is measured in miliseconds, the "resolution" of this
 * algorithm is one milisecond.
 *
 * To avoid this algorithm altogether, configure all links to have the
 * same latency and bandwidth.
 */
static void
ng_ppp_mp_strategy(node_p node, int len, int *distrib)
{
	const priv_p priv = node->private;
	int latency[NG_PPP_MAX_LINKS];
	int sortByLatency[NG_PPP_MAX_LINKS];
	int activeLinkNum, linkNum;
	int t0, total, topSum, botSum;
	struct timeval now;
	int i, numFragments;

	/* If only one link, this gets real easy */
	if (priv->numActiveLinks == 1) {
		distrib[0] = len;
		return;
	}

	/* Get current time */
	microtime(&now);

	/* Compute latencies for each link at this point in time */
	for (activeLinkNum = 0;
	    activeLinkNum < priv->numActiveLinks; activeLinkNum++) {
		struct timeval diff;
		int xmitBytes;

		/* Start with base latency value */
		linkNum = priv->activeLinks[activeLinkNum];
		latency[activeLinkNum] = priv->conf.links[linkNum].latency;
		sortByLatency[activeLinkNum] = activeLinkNum;	/* see below */

		/* Any additional latency? */
		if (priv->qstat[activeLinkNum].bytesInQueue == 0)
			continue;

		/* Compute time delta since last write */
		diff = now;
		timevalsub(&diff, &priv->qstat[activeLinkNum].lastWrite);
		if (now.tv_sec < 0 || diff.tv_sec >= 10) {	/* sanity */
			priv->qstat[activeLinkNum].bytesInQueue = 0;
			continue;
		}

		/* How many bytes could have transmitted since last write? */
		xmitBytes = priv->conf.links[linkNum].bandwidth * diff.tv_sec
		    + (priv->conf.links[linkNum].bandwidth
			* (diff.tv_usec / 1000)) / 100;
		priv->qstat[activeLinkNum].bytesInQueue -= xmitBytes;
		if (priv->qstat[activeLinkNum].bytesInQueue < 0)
			priv->qstat[activeLinkNum].bytesInQueue = 0;
		else
			latency[activeLinkNum] +=
			    (100 * priv->qstat[activeLinkNum].bytesInQueue)
				/ priv->conf.links[linkNum].bandwidth;
	}

	/* Sort links by latency */
	compareLatencies = latency;
	qsort(sortByLatency,
	    priv->numActiveLinks, sizeof(*sortByLatency), ng_ppp_intcmp);
	compareLatencies = NULL;

	/* Find the interval we need (add links in sortByLatency[] order) */
	for (numFragments = 1;
	    numFragments < priv->numActiveLinks; numFragments++) {
		for (total = i = 0; i < numFragments; i++) {
			int flowTime;

			flowTime = latency[sortByLatency[numFragments]]
			    - latency[sortByLatency[i]];
			total += ((flowTime * priv->conf.links[
			    priv->activeLinks[sortByLatency[i]]].bandwidth)
			    	+ 99) / 100;
		}
		if (total >= len)
			break;
	}

	/* Solve for t_0 in that interval */
	for (topSum = botSum = i = 0; i < numFragments; i++) {
		int bw = priv->conf.links[
		    priv->activeLinks[sortByLatency[i]]].bandwidth;

		topSum += latency[sortByLatency[i]] * bw;	/* / 100 */
		botSum += bw;					/* / 100 */
	}
	t0 = ((len * 100) + topSum + botSum / 2) / botSum;

	/* Compute f_i(t_0) all i */
	bzero(distrib, priv->numActiveLinks * sizeof(*distrib));
	for (total = i = 0; i < numFragments; i++) {
		int bw = priv->conf.links[
		    priv->activeLinks[sortByLatency[i]]].bandwidth;

		distrib[sortByLatency[i]] =
		    (bw * (t0 - latency[sortByLatency[i]]) + 50) / 100;
		total += distrib[sortByLatency[i]];
	}

	/* Deal with any rounding error */
	if (total < len) {
		int fast = 0;

		/* Find the fastest link */
		for (i = 1; i < numFragments; i++) {
			if (priv->conf.links[
			      priv->activeLinks[sortByLatency[i]]].bandwidth >
			    priv->conf.links[
			      priv->activeLinks[sortByLatency[fast]]].bandwidth)
				fast = i;
		}
		distrib[sortByLatency[fast]] += len - total;
	} else while (total > len) {
		int delta, slow = 0;

		/* Find the slowest link that still has bytes to remove */
		for (i = 1; i < numFragments; i++) {
			if (distrib[sortByLatency[slow]] == 0
			  || (distrib[sortByLatency[i]] > 0
			    && priv->conf.links[priv->activeLinks[
					sortByLatency[i]]].bandwidth <
			      priv->conf.links[priv->activeLinks[
					sortByLatency[slow]]].bandwidth))
				slow = i;
		}
		delta = total - len;
		if (delta > distrib[sortByLatency[slow]])
			delta = distrib[sortByLatency[slow]];
		distrib[sortByLatency[slow]] -= delta;
		total -= delta;
	}
}

/*
 * Compare two integers
 */
static int
ng_ppp_intcmp(const void *v1, const void *v2)
{
	const int index1 = *((const int *) v1);
	const int index2 = *((const int *) v2);

	return compareLatencies[index1] - compareLatencies[index2];
}

/*
 * Prepend a possibly compressed PPP protocol number in front of a frame
 */
static struct mbuf *
ng_ppp_addproto(struct mbuf *m, int proto, int compOK)
{
	int psize = (PROT_COMPRESSABLE(proto) && compOK) ? 1 : 2;

	/* Add protocol number */
	M_PREPEND(m, psize, M_NOWAIT);
	if (m == NULL || (m->m_len < psize && (m = m_pullup(m, psize)) == NULL))
		return (NULL);
	if (psize == 1)
		*mtod(m, u_char *) = (u_char)proto;
	else
		*mtod(m, u_int16_t *) = htons((u_int16_t)proto);
	return (m);
}

/*
 * Update private information that is derived from other private information
 */
static void
ng_ppp_update(node_p node, int newConf)
{
	const priv_p priv = node->private;
	int i;

	/* Update active status for VJ Compression */
	priv->vjCompHooked = priv->hooks[HOOK_INDEX_VJC_IP] != NULL
	    && priv->hooks[HOOK_INDEX_VJC_COMP] != NULL
	    && priv->hooks[HOOK_INDEX_VJC_UNCOMP] != NULL
	    && priv->hooks[HOOK_INDEX_VJC_VJIP] != NULL;

	/* Increase latency for each link an amount equal to one MP header */
	if (newConf) {
		for (i = 0; i < NG_PPP_MAX_LINKS; i++) {
			int hdrBytes;

			hdrBytes = (priv->conf.links[i].enableProtoComp ? 1 : 2)
			    + (priv->conf.xmitShortSeq ? 2 : 4);
			priv->conf.links[i].latency +=
			    ((hdrBytes * priv->conf.links[i].bandwidth) + 50)
				/ 100;
		}
	}

	/* Update list of active links */
	bzero(&priv->activeLinks, sizeof(priv->activeLinks));
	priv->numActiveLinks = 0;
	priv->allLinksEqual = 1;
	for (i = 0; i < NG_PPP_MAX_LINKS; i++) {
		if (priv->conf.links[i].enableLink && priv->links[i] != NULL) {
			priv->activeLinks[priv->numActiveLinks++] = i;
			if (priv->conf.links[i].latency
				  != priv->conf.links[0].latency
			    || priv->conf.links[i].bandwidth
				  != priv->conf.links[0].bandwidth)
				priv->allLinksEqual = 0;
		}
	}

	/* Reset MP state if no longer active */
	if (!priv->conf.enableMultilink || priv->numActiveLinks == 0) {
		ng_ppp_free_frags(node);
		priv->mpSeqOut = MP_INITIAL_SEQ;
		bzero(&priv->qstat, sizeof(priv->qstat));
	}
}

/*
 * Determine if a new configuration would represent a valid change
 * from the current configuration and link activity status.
 */
static int
ng_ppp_config_valid(node_p node, const struct ng_ppp_node_config *newConf)
{
	const priv_p priv = node->private;
	int i, newNumLinksActive;

	/* Check per-link config and count how many links would be active */
	for (newNumLinksActive = i = 0; i < NG_PPP_MAX_LINKS; i++) {
		if (newConf->links[i].enableLink && priv->links[i] != NULL)
			newNumLinksActive++;
		if (!newConf->links[i].enableLink)
			continue;
		if (newConf->links[i].mru < MP_MIN_LINK_MRU)
			return (0);
		if (newConf->links[i].bandwidth == 0)
			return (0);
		if (newConf->links[i].bandwidth > NG_PPP_MAX_BANDWIDTH)
			return (0);
		if (newConf->links[i].latency > NG_PPP_MAX_LATENCY)
			return (0);
	}

	/* Check bundle parameters */
	if (newConf->enableMultilink && newConf->mrru < MP_MIN_MRRU)
		return (0);

	/* Disallow changes to multi-link configuration while MP is active */
	if (priv->numActiveLinks > 0 && newNumLinksActive > 0) {
		if (!priv->conf.enableMultilink != !newConf->enableMultilink
		    || !priv->conf.xmitShortSeq != !newConf->xmitShortSeq
		    || !priv->conf.recvShortSeq != !newConf->recvShortSeq)
			return (0);
	}

	/* At most one link can be active unless multi-link is enabled */
	if (!newConf->enableMultilink && newNumLinksActive > 1)
		return (0);

	/* Configuration change would be valid */
	return (1);
}

/*
 * Free all entries in the fragment queue
 */
static void
ng_ppp_free_frags(node_p node)
{
	const priv_p priv = node->private;
	struct ng_ppp_frag *qent, *next;

	for (qent = CIRCLEQ_FIRST(&priv->frags);
	    qent != (void *) &priv->frags; qent = next) {
		next = CIRCLEQ_NEXT(qent, f_qent);
		NG_FREE_DATA(qent->data, qent->meta);
		FREE(qent, M_NETGRAPH);
	}
	CIRCLEQ_INIT(&priv->frags);
}

