
/*
 * ng_ppp.c
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
 * Author: Archie Cobbs <archie@freebsd.org>
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
#include <sys/time.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/ctype.h>

#include <machine/limits.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_ppp.h>
#include <netgraph/ng_vjc.h>

#define PROT_VALID(p)		(((p) & 0x0101) == 0x0001)
#define PROT_COMPRESSABLE(p)	(((p) & 0xff00) == 0x0000)

/* Some PPP protocol numbers we're interested in */
#define PROT_APPLETALK		0x0029
#define PROT_COMPD		0x00fd
#define PROT_CRYPTD		0x0053
#define PROT_IP			0x0021
#define PROT_IPV6		0x0057
#define PROT_IPX		0x002b
#define PROT_LCP		0xc021
#define PROT_MP			0x003d
#define PROT_VJCOMP		0x002d
#define PROT_VJUNCOMP		0x002f

/* Multilink PPP definitions */
#define MP_MIN_MRRU		1500		/* per RFC 1990 */
#define MP_INITIAL_SEQ		0		/* per RFC 1990 */
#define MP_MIN_LINK_MRU		32

#define MP_SHORT_SEQ_MASK	0x00000fff	/* short seq # mask */
#define MP_SHORT_SEQ_HIBIT	0x00000800	/* short seq # high bit */
#define MP_SHORT_FIRST_FLAG	0x00008000	/* first fragment in frame */
#define MP_SHORT_LAST_FLAG	0x00004000	/* last fragment in frame */

#define MP_LONG_SEQ_MASK	0x00ffffff	/* long seq # mask */
#define MP_LONG_SEQ_HIBIT	0x00800000	/* long seq # high bit */
#define MP_LONG_FIRST_FLAG	0x80000000	/* first fragment in frame */
#define MP_LONG_LAST_FLAG	0x40000000	/* last fragment in frame */

#define MP_NOSEQ		0x7fffffff	/* impossible sequence number */

/* Sign extension of MP sequence numbers */
#define MP_SHORT_EXTEND(s)	(((s) & MP_SHORT_SEQ_HIBIT) ?		\
				    ((s) | ~MP_SHORT_SEQ_MASK)		\
				    : ((s) & MP_SHORT_SEQ_MASK))
#define MP_LONG_EXTEND(s)	(((s) & MP_LONG_SEQ_HIBIT) ?		\
				    ((s) | ~MP_LONG_SEQ_MASK)		\
				    : ((s) & MP_LONG_SEQ_MASK))

/* Comparision of MP sequence numbers. Note: all sequence numbers
   except priv->xseq are stored with the sign bit extended. */
#define MP_SHORT_SEQ_DIFF(x,y)	MP_SHORT_EXTEND((x) - (y))
#define MP_LONG_SEQ_DIFF(x,y)	MP_LONG_EXTEND((x) - (y))

#define MP_RECV_SEQ_DIFF(priv,x,y)					\
				((priv)->conf.recvShortSeq ?		\
				    MP_SHORT_SEQ_DIFF((x), (y)) :	\
				    MP_LONG_SEQ_DIFF((x), (y)))

/* Increment receive sequence number */
#define MP_NEXT_RECV_SEQ(priv,seq)					    \
				(((seq) + 1) & ((priv)->conf.recvShortSeq ? \
				    MP_SHORT_SEQ_MASK : MP_LONG_SEQ_MASK))

/* Don't fragment transmitted packets smaller than this */
#define MP_MIN_FRAG_LEN		6

/* Maximum fragment reasssembly queue length */
#define MP_MAX_QUEUE_LEN	128

/* Fragment queue scanner period */
#define MP_FRAGTIMER_INTERVAL	(hz/2)

/* We store incoming fragments this way */
struct ng_ppp_frag {
	int				seq;		/* fragment seq# */
	u_char				first;		/* First in packet? */
	u_char				last;		/* Last in packet? */
	struct timeval			timestamp;	/* time of reception */
	struct mbuf			*data;		/* Fragment data */
	meta_p				meta;		/* Fragment meta */
	TAILQ_ENTRY(ng_ppp_frag)	f_qent;		/* Fragment queue */
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
	NG_PPP_HOOK_IPV6,
#define HOOK_INDEX_IPV6			12
	NULL
#define HOOK_INDEX_MAX			13
};

/* We store index numbers in the hook private pointer. The HOOK_INDEX()
   for a hook is either the index (above) for normal hooks, or the ones
   complement of the link number for link hooks. */
#define HOOK_INDEX(hook)	(*((int16_t *) &(hook)->private))

/* Per-link private information */
struct ng_ppp_link {
	struct ng_ppp_link_conf	conf;		/* link configuration */
	hook_p			hook;		/* connection to link data */
	int32_t			seq;		/* highest rec'd seq# - MSEQ */
	struct timeval		lastWrite;	/* time of last write */
	int			bytesInQueue;	/* bytes in the output queue */
	struct ng_ppp_link_stat	stats;		/* Link stats */
};

/* Total per-node private information */
struct ng_ppp_private {
	struct ng_ppp_bund_conf	conf;			/* bundle config */
	struct ng_ppp_link_stat	bundleStats;		/* bundle stats */
	struct ng_ppp_link	links[NG_PPP_MAX_LINKS];/* per-link info */
	int32_t			xseq;			/* next out MP seq # */
	int32_t			mseq;			/* min links[i].seq */
	u_char			vjCompHooked;		/* VJ comp hooked up? */
	u_char			allLinksEqual;		/* all xmit the same? */
	u_char			timerActive;		/* frag timer active? */
	u_int			numActiveLinks;		/* how many links up */
	int			activeLinks[NG_PPP_MAX_LINKS];	/* indicies */
	u_int			lastLink;		/* for round robin */
	hook_p			hooks[HOOK_INDEX_MAX];	/* non-link hooks */
	TAILQ_HEAD(ng_ppp_fraglist, ng_ppp_frag)	/* fragment queue */
				frags;
	int			qlen;			/* fraq queue length */
	struct callout_handle	fragTimer;		/* fraq queue check */
};
typedef struct ng_ppp_private *priv_p;

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
static int	ng_ppp_output(node_p node, int bypass, int proto,
			int linkNum, struct mbuf *m, meta_p meta);
static int	ng_ppp_mp_input(node_p node, int linkNum,
			struct mbuf *m, meta_p meta);
static int	ng_ppp_check_packet(node_p node);
static void	ng_ppp_get_packet(node_p node, struct mbuf **mp, meta_p *metap);
static int	ng_ppp_frag_process(node_p node);
static int	ng_ppp_frag_trim(node_p node);
static void	ng_ppp_frag_timeout(void *arg);
static void	ng_ppp_frag_checkstale(node_p node);
static void	ng_ppp_frag_reset(node_p node);
static int	ng_ppp_mp_output(node_p node, struct mbuf *m, meta_p meta);
static void	ng_ppp_mp_strategy(node_p node, int len, int *distrib);
static int	ng_ppp_intcmp(const void *v1, const void *v2);
static struct	mbuf *ng_ppp_addproto(struct mbuf *m, int proto, int compOK);
static struct	mbuf *ng_ppp_prepend(struct mbuf *m, const void *buf, int len);
static int	ng_ppp_config_valid(node_p node,
			const struct ng_ppp_node_conf *newConf);
static void	ng_ppp_update(node_p node, int newConf);
static void	ng_ppp_start_frag_timer(node_p node);
static void	ng_ppp_stop_frag_timer(node_p node);

/* Parse type for struct ng_ppp_mp_state_type */
static const struct ng_parse_fixedarray_info ng_ppp_rseq_array_info = {
	&ng_parse_hint32_type,
	NG_PPP_MAX_LINKS
};
static const struct ng_parse_type ng_ppp_rseq_array_type = {
	&ng_parse_fixedarray_type,
	&ng_ppp_rseq_array_info,
};
static const struct ng_parse_struct_info ng_ppp_mp_state_type_info
	= NG_PPP_MP_STATE_TYPE_INFO(&ng_ppp_rseq_array_type);
static const struct ng_parse_type ng_ppp_mp_state_type = {
	&ng_parse_struct_type,
	&ng_ppp_mp_state_type_info,
};

/* Parse type for struct ng_ppp_link_conf */
static const struct ng_parse_struct_info
	ng_ppp_link_type_info = NG_PPP_LINK_TYPE_INFO;
static const struct ng_parse_type ng_ppp_link_type = {
	&ng_parse_struct_type,
	&ng_ppp_link_type_info,
};

/* Parse type for struct ng_ppp_bund_conf */
static const struct ng_parse_struct_info
	ng_ppp_bund_type_info = NG_PPP_BUND_TYPE_INFO;
static const struct ng_parse_type ng_ppp_bund_type = {
	&ng_parse_struct_type,
	&ng_ppp_bund_type_info,
};

/* Parse type for struct ng_ppp_node_conf */
static const struct ng_parse_fixedarray_info ng_ppp_array_info = {
	&ng_ppp_link_type,
	NG_PPP_MAX_LINKS
};
static const struct ng_parse_type ng_ppp_link_array_type = {
	&ng_parse_fixedarray_type,
	&ng_ppp_array_info,
};
static const struct ng_parse_struct_info ng_ppp_conf_type_info
	= NG_PPP_CONFIG_TYPE_INFO(&ng_ppp_bund_type, &ng_ppp_link_array_type);
static const struct ng_parse_type ng_ppp_conf_type = {
	&ng_parse_struct_type,
	&ng_ppp_conf_type_info
};

/* Parse type for struct ng_ppp_link_stat */
static const struct ng_parse_struct_info
	ng_ppp_stats_type_info = NG_PPP_STATS_TYPE_INFO;
static const struct ng_parse_type ng_ppp_stats_type = {
	&ng_parse_struct_type,
	&ng_ppp_stats_type_info
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_ppp_cmds[] = {
	{
	  NGM_PPP_COOKIE,
	  NGM_PPP_SET_CONFIG,
	  "setconfig",
	  &ng_ppp_conf_type,
	  NULL
	},
	{
	  NGM_PPP_COOKIE,
	  NGM_PPP_GET_CONFIG,
	  "getconfig",
	  NULL,
	  &ng_ppp_conf_type
	},
	{
	  NGM_PPP_COOKIE,
	  NGM_PPP_GET_MP_STATE,
	  "getmpstate",
	  NULL,
	  &ng_ppp_mp_state_type
	},
	{
	  NGM_PPP_COOKIE,
	  NGM_PPP_GET_LINK_STATS,
	  "getstats",
	  &ng_parse_int16_type,
	  &ng_ppp_stats_type
	},
	{
	  NGM_PPP_COOKIE,
	  NGM_PPP_CLR_LINK_STATS,
	  "clrstats",
	  &ng_parse_int16_type,
	  NULL
	},
	{
	  NGM_PPP_COOKIE,
	  NGM_PPP_GETCLR_LINK_STATS,
	  "getclrstats",
	  &ng_parse_int16_type,
	  &ng_ppp_stats_type
	},
	{ 0 }
};

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
	ng_ppp_disconnect,
	ng_ppp_cmds
};
NETGRAPH_INIT(ppp, &ng_ppp_typestruct);

static int *compareLatencies;			/* hack for ng_ppp_intcmp() */

/* Address and control field header */
static const u_char ng_ppp_acf[2] = { 0xff, 0x03 };

/* Maximum time we'll let a complete incoming packet sit in the queue */
static const struct timeval ng_ppp_max_staleness = { 2, 0 };	/* 2 seconds */

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
	int i, error;

	/* Allocate private structure */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return (ENOMEM);

	/* Call generic node constructor */
	if ((error = ng_make_node_common(&ng_ppp_typestruct, nodep))) {
		FREE(priv, M_NETGRAPH);
		return (error);
	}
	(*nodep)->private = priv;

	/* Initialize state */
	TAILQ_INIT(&priv->frags);
	for (i = 0; i < NG_PPP_MAX_LINKS; i++)
		priv->links[i].seq = MP_NOSEQ;
	callout_handle_init(&priv->fragTimer);

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
		const char *cp;
		char *eptr;

		cp = name + strlen(NG_PPP_HOOK_LINK_PREFIX);
		if (!isdigit(*cp) || (cp[0] == '0' && cp[1] != '\0'))
			return (EINVAL);
		linkNum = (int)strtoul(cp, &eptr, 10);
		if (*eptr != '\0' || linkNum < 0 || linkNum >= NG_PPP_MAX_LINKS)
			return (EINVAL);
		hookPtr = &priv->links[linkNum].hook;
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
	if (linkNum != -1 && priv->links[linkNum].conf.enableLink
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
	      const char *raddr, struct ng_mesg **rptr, hook_p lasthook)
{
	const priv_p priv = node->private;
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_PPP_COOKIE:
		switch (msg->header.cmd) {
		case NGM_PPP_SET_CONFIG:
		    {
			struct ng_ppp_node_conf *const conf =
			    (struct ng_ppp_node_conf *)msg->data;
			int i;

			/* Check for invalid or illegal config */
			if (msg->header.arglen != sizeof(*conf))
				ERROUT(EINVAL);
			if (!ng_ppp_config_valid(node, conf))
				ERROUT(EINVAL);

			/* Copy config */
			priv->conf = conf->bund;
			for (i = 0; i < NG_PPP_MAX_LINKS; i++)
				priv->links[i].conf = conf->links[i];
			ng_ppp_update(node, 1);
			break;
		    }
		case NGM_PPP_GET_CONFIG:
		    {
			struct ng_ppp_node_conf *conf;
			int i;

			NG_MKRESPONSE(resp, msg, sizeof(*conf), M_NOWAIT);
			if (resp == NULL)
				ERROUT(ENOMEM);
			conf = (struct ng_ppp_node_conf *)resp->data;
			conf->bund = priv->conf;
			for (i = 0; i < NG_PPP_MAX_LINKS; i++)
				conf->links[i] = priv->links[i].conf;
			break;
		    }
		case NGM_PPP_GET_MP_STATE:
		    {
			struct ng_ppp_mp_state *info;
			int i;

			NG_MKRESPONSE(resp, msg, sizeof(*info), M_NOWAIT);
			if (resp == NULL)
				ERROUT(ENOMEM);
			info = (struct ng_ppp_mp_state *)resp->data;
			bzero(info, sizeof(*info));
			for (i = 0; i < NG_PPP_MAX_LINKS; i++) {
				if (priv->links[i].seq != MP_NOSEQ)
					info->rseq[i] = priv->links[i].seq;
			}
			info->mseq = priv->mseq;
			info->xseq = priv->xseq;
			break;
		    }
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
			    &priv->bundleStats : &priv->links[linkNum].stats;
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

		if ((error = ng_path2node(node,
		    raddr, &origNode, NULL, NULL)) != 0)
			ERROUT(error);
		snprintf(path, sizeof(path), "[%lx]:%s",
		    (long)node, NG_PPP_HOOK_VJC_IP);
		return ng_send_msg(origNode, msg, path, rptr);
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
ng_ppp_rcvdata(hook_p hook, struct mbuf *m, meta_p meta,
		struct mbuf **ret_m, meta_p *ret_meta)
{
	const node_p node = hook->node;
	const priv_p priv = node->private;
	const int index = HOOK_INDEX(hook);
	u_int16_t linkNum = NG_PPP_BUNDLE_LINKNUM;
	hook_p outHook = NULL;
	int proto = 0, error;

	/* Did it come from a link hook? */
	if (index < 0) {
		struct ng_ppp_link *link;

		/* Convert index into a link number */
		linkNum = (u_int16_t)~index;
		KASSERT(linkNum < NG_PPP_MAX_LINKS,
		    ("%s: bogus index 0x%x", __FUNCTION__, index));
		link = &priv->links[linkNum];

		/* Stats */
		link->stats.recvFrames++;
		link->stats.recvOctets += m->m_pkthdr.len;

		/* Strip address and control fields, if present */
		if (m->m_pkthdr.len >= 2) {
			if (m->m_len < 2 && (m = m_pullup(m, 2)) == NULL) {
				NG_FREE_DATA(m, meta);
				return (ENOBUFS);
			}
			if (bcmp(mtod(m, u_char *), &ng_ppp_acf, 2) == 0)
				m_adj(m, 2);
		}

		/* Dispatch incoming frame (if not enabled, to bypass) */
		return ng_ppp_input(node,
		    !link->conf.enableLink, linkNum, m, meta);
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
	case HOOK_INDEX_IPV6:
		if (!priv->conf.enableIPv6) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		proto = PROT_IPV6;
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
			NG_FREE_DATA(m, meta);
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
		    && linkNum != NG_PPP_BUNDLE_LINKNUM) {
			NG_FREE_DATA(m, meta);
			return (EINVAL);
		}
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
	case HOOK_INDEX_IPV6:
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
		return ng_ppp_output(node, 0,
		    proto, NG_PPP_BUNDLE_LINKNUM, m, meta);

	case HOOK_INDEX_BYPASS:
		return ng_ppp_output(node, 1, proto, linkNum, m, meta);

	/* Incoming data */
	case HOOK_INDEX_DECRYPT:
	case HOOK_INDEX_DECOMPRESS:
		return ng_ppp_input(node, 0, NG_PPP_BUNDLE_LINKNUM, m, meta);

	case HOOK_INDEX_VJC_IP:
		outHook = priv->hooks[HOOK_INDEX_INET];
		break;
	}

	/* Send packet out hook */
	NG_SEND_DATA_RET(error, outHook, m, meta);
	if (m != NULL || meta != NULL)
		return ng_ppp_rcvdata(outHook, m, meta, NULL, NULL);
	return (error);
}

/*
 * Destroy node
 */
static int
ng_ppp_rmnode(node_p node)
{
	const priv_p priv = node->private;

	/* Stop fragment queue timer */
	ng_ppp_stop_frag_timer(node);

	/* Take down netgraph node */
	node->flags |= NG_INVALID;
	ng_cutlinks(node);
	ng_unname(node);
	ng_ppp_frag_reset(node);
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
	const node_p node = hook->node;
	const priv_p priv = node->private;
	const int index = HOOK_INDEX(hook);

	/* Zero out hook pointer */
	if (index < 0)
		priv->links[~index].hook = NULL;
	else
		priv->hooks[index] = NULL;

	/* Update derived info (or go away if no hooks left) */
	if (node->numhooks > 0)
		ng_ppp_update(node, 0);
	else
		ng_rmnode(node);
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
			priv->links[linkNum].stats.badProtos++;
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
		if (priv->conf.enableMultilink
		    && linkNum != NG_PPP_BUNDLE_LINKNUM)
			return ng_ppp_mp_input(node, linkNum, m, meta);
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
	case PROT_IPV6:
		if (priv->conf.enableIPv6)
			outHook = priv->hooks[HOOK_INDEX_IPV6];
		break;
	}

bypass:
	/* For unknown/inactive protocols, forward out the bypass hook */
	if (outHook == NULL) {
		u_int16_t hdr[2];

		hdr[0] = htons(linkNum);
		hdr[1] = htons((u_int16_t)proto);
		if ((m = ng_ppp_prepend(m, &hdr, 4)) == NULL) {
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		outHook = priv->hooks[HOOK_INDEX_BYPASS];
	}

	/* Forward frame */
	NG_SEND_DATA(error, outHook, m, meta);
	return (error);
}

/*
 * Deliver a frame out a link, either a real one or NG_PPP_BUNDLE_LINKNUM
 * If the link is not enabled then ENXIO is returned, unless "bypass" is != 0.
 */
static int
ng_ppp_output(node_p node, int bypass,
	int proto, int linkNum, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;
	struct ng_ppp_link *link;
	int len, error;

	/* If not doing MP, map bundle virtual link to (the only) link */
	if (linkNum == NG_PPP_BUNDLE_LINKNUM && !priv->conf.enableMultilink)
		linkNum = priv->activeLinks[0];

	/* Get link pointer (optimization) */
	link = (linkNum != NG_PPP_BUNDLE_LINKNUM) ?
	    &priv->links[linkNum] : NULL;

	/* Check link status (if real) */
	if (linkNum != NG_PPP_BUNDLE_LINKNUM) {
		if (!bypass && !link->conf.enableLink) {
			NG_FREE_DATA(m, meta);
			return (ENXIO);
		}
		if (link->hook == NULL) {
			NG_FREE_DATA(m, meta);
			return (ENETDOWN);
		}
	}

	/* Prepend protocol number, possibly compressed */
	if ((m = ng_ppp_addproto(m, proto,
	    linkNum == NG_PPP_BUNDLE_LINKNUM
	      || link->conf.enableProtoComp)) == NULL) {
		NG_FREE_META(meta);
		return (ENOBUFS);
	}

	/* Special handling for the MP virtual link */
	if (linkNum == NG_PPP_BUNDLE_LINKNUM)
		return ng_ppp_mp_output(node, m, meta);

	/* Prepend address and control field (unless compressed) */
	if (proto == PROT_LCP || !link->conf.enableACFComp) {
		if ((m = ng_ppp_prepend(m, &ng_ppp_acf, 2)) == NULL) {
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
	}

	/* Deliver frame */
	len = m->m_pkthdr.len;
	NG_SEND_DATA(error, link->hook, m, meta);

	/* Update stats and 'bytes in queue' counter */
	if (error == 0) {
		link->stats.xmitFrames++;
		link->stats.xmitOctets += len;
		link->bytesInQueue += len;
		getmicrouptime(&link->lastWrite);
	}
	return error;
}

/*
 * Handle an incoming multi-link fragment
 *
 * The fragment reassembly algorithm is somewhat complex. This is mainly
 * because we are required not to reorder the reconstructed packets, yet
 * fragments are only guaranteed to arrive in order on a per-link basis.
 * In other words, when we have a complete packet ready, but the previous
 * packet is still incomplete, we have to decide between delivering the
 * complete packet and throwing away the incomplete one, or waiting to
 * see if the remainder of the incomplete one arrives, at which time we
 * can deliver both packets, in order.
 *
 * This problem is exacerbated by "sequence number slew", which is when
 * the sequence numbers coming in from different links are far apart from
 * each other. In particular, certain unnamed equipment (*cough* Ascend)
 * has been seen to generate sequence number slew of up to 10 on an ISDN
 * 2B-channel MP link. There is nothing invalid about sequence number slew
 * but it makes the reasssembly process have to work harder.
 *
 * However, the peer is required to transmit fragments in order on each
 * link. That means if we define MSEQ as the minimum over all links of
 * the highest sequence number received on that link, then we can always
 * give up any hope of receiving a fragment with sequence number < MSEQ in
 * the future (all of this using 'wraparound' sequence number space).
 * Therefore we can always immediately throw away incomplete packets
 * missing fragments with sequence numbers < MSEQ.
 *
 * Here is an overview of our algorithm:
 *
 *    o Received fragments are inserted into a queue, for which we
 *	maintain these invariants between calls to this function:
 *
 *	- Fragments are ordered in the queue by sequence number
 *	- If a complete packet is at the head of the queue, then
 *	  the first fragment in the packet has seq# > MSEQ + 1
 *	  (otherwise, we could deliver it immediately)
 *	- If any fragments have seq# < MSEQ, then they are necessarily
 *	  part of a packet whose missing seq#'s are all > MSEQ (otherwise,
 *	  we can throw them away because they'll never be completed)
 *	- The queue contains at most MP_MAX_QUEUE_LEN fragments
 *
 *    o We have a periodic timer that checks the queue for the first
 *	complete packet that has been sitting in the queue "too long".
 *	When one is detected, all previous (incomplete) fragments are
 *	discarded, their missing fragments are declared lost and MSEQ
 *	is increased.
 *
 *    o If we recieve a fragment with seq# < MSEQ, we throw it away
 *	because we've already delcared it lost.
 *
 * This assumes linkNum != NG_PPP_BUNDLE_LINKNUM.
 */
static int
ng_ppp_mp_input(node_p node, int linkNum, struct mbuf *m, meta_p meta)
{
	const priv_p priv = node->private;
	struct ng_ppp_link *const link = &priv->links[linkNum];
	struct ng_ppp_frag frag0, *frag = &frag0;
	struct ng_ppp_frag *qent;
	int i, diff, inserted;

	/* Stats */
	priv->bundleStats.recvFrames++;
	priv->bundleStats.recvOctets += m->m_pkthdr.len;

	/* Extract fragment information from MP header */
	if (priv->conf.recvShortSeq) {
		u_int16_t shdr;

		if (m->m_pkthdr.len < 2) {
			link->stats.runts++;
			NG_FREE_DATA(m, meta);
			return (EINVAL);
		}
		if (m->m_len < 2 && (m = m_pullup(m, 2)) == NULL) {
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		shdr = ntohs(*mtod(m, u_int16_t *));
		frag->seq = MP_SHORT_EXTEND(shdr);
		frag->first = (shdr & MP_SHORT_FIRST_FLAG) != 0;
		frag->last = (shdr & MP_SHORT_LAST_FLAG) != 0;
		diff = MP_SHORT_SEQ_DIFF(frag->seq, priv->mseq);
		m_adj(m, 2);
	} else {
		u_int32_t lhdr;

		if (m->m_pkthdr.len < 4) {
			link->stats.runts++;
			NG_FREE_DATA(m, meta);
			return (EINVAL);
		}
		if (m->m_len < 4 && (m = m_pullup(m, 4)) == NULL) {
			NG_FREE_META(meta);
			return (ENOBUFS);
		}
		lhdr = ntohl(*mtod(m, u_int32_t *));
		frag->seq = MP_LONG_EXTEND(lhdr);
		frag->first = (lhdr & MP_LONG_FIRST_FLAG) != 0;
		frag->last = (lhdr & MP_LONG_LAST_FLAG) != 0;
		diff = MP_LONG_SEQ_DIFF(frag->seq, priv->mseq);
		m_adj(m, 4);
	}
	frag->data = m;
	frag->meta = meta;
	getmicrouptime(&frag->timestamp);

	/* If sequence number is < MSEQ, we've already declared this
	   fragment as lost, so we have no choice now but to drop it */
	if (diff < 0) {
		link->stats.dropFragments++;
		NG_FREE_DATA(m, meta);
		return (0);
	}

	/* Update highest received sequence number on this link and MSEQ */
	priv->mseq = link->seq = frag->seq;
	for (i = 0; i < priv->numActiveLinks; i++) {
		struct ng_ppp_link *const alink =
		    &priv->links[priv->activeLinks[i]];

		if (MP_RECV_SEQ_DIFF(priv, alink->seq, priv->mseq) < 0)
			priv->mseq = alink->seq;
	}

	/* Allocate a new frag struct for the queue */
	MALLOC(frag, struct ng_ppp_frag *, sizeof(*frag), M_NETGRAPH, M_NOWAIT);
	if (frag == NULL) {
		NG_FREE_DATA(m, meta);
		ng_ppp_frag_process(node);
		return (ENOMEM);
	}
	*frag = frag0;

	/* Add fragment to queue, which is sorted by sequence number */
	inserted = 0;
	TAILQ_FOREACH_REVERSE(qent, &priv->frags, ng_ppp_fraglist, f_qent) {
		diff = MP_RECV_SEQ_DIFF(priv, frag->seq, qent->seq);
		if (diff > 0) {
			TAILQ_INSERT_AFTER(&priv->frags, qent, frag, f_qent);
			inserted = 1;
			break;
		} else if (diff == 0) {	     /* should never happen! */
			link->stats.dupFragments++;
			NG_FREE_DATA(frag->data, frag->meta);
			FREE(frag, M_NETGRAPH);
			return (EINVAL);
		}
	}
	if (!inserted)
		TAILQ_INSERT_HEAD(&priv->frags, frag, f_qent);
	priv->qlen++;

	/* Process the queue */
	return ng_ppp_frag_process(node);
}

/*
 * Examine our list of fragments, and determine if there is a
 * complete and deliverable packet at the head of the list.
 * Return 1 if so, zero otherwise.
 */
static int
ng_ppp_check_packet(node_p node)
{
	const priv_p priv = node->private;
	struct ng_ppp_frag *qent, *qnext;

	/* Check for empty queue */
	if (TAILQ_EMPTY(&priv->frags))
		return (0);

	/* Check first fragment is the start of a deliverable packet */
	qent = TAILQ_FIRST(&priv->frags);
	if (!qent->first || MP_RECV_SEQ_DIFF(priv, qent->seq, priv->mseq) > 1)
		return (0);

	/* Check that all the fragments are there */
	while (!qent->last) {
		qnext = TAILQ_NEXT(qent, f_qent);
		if (qnext == NULL)	/* end of queue */
			return (0);
		if (qnext->seq != MP_NEXT_RECV_SEQ(priv, qent->seq))
			return (0);
		qent = qnext;
	}

	/* Got one */
	return (1);
}

/*
 * Pull a completed packet off the head of the incoming fragment queue.
 * This assumes there is a completed packet there to pull off.
 */
static void
ng_ppp_get_packet(node_p node, struct mbuf **mp, meta_p *metap)
{
	const priv_p priv = node->private;
	struct ng_ppp_frag *qent, *qnext;
	struct mbuf *m = NULL, *tail;

	qent = TAILQ_FIRST(&priv->frags);
	KASSERT(!TAILQ_EMPTY(&priv->frags) && qent->first,
	    ("%s: no packet", __FUNCTION__));
	for (tail = NULL; qent != NULL; qent = qnext) {
		qnext = TAILQ_NEXT(qent, f_qent);
		KASSERT(!TAILQ_EMPTY(&priv->frags),
		    ("%s: empty q", __FUNCTION__));
		TAILQ_REMOVE(&priv->frags, qent, f_qent);
		if (tail == NULL) {
			tail = m = qent->data;
			*metap = qent->meta;	/* inherit first frag's meta */
		} else {
			m->m_pkthdr.len += qent->data->m_pkthdr.len;
			tail->m_next = qent->data;
			NG_FREE_META(qent->meta); /* drop other frags' metas */
		}
		while (tail->m_next != NULL)
			tail = tail->m_next;
		if (qent->last)
			qnext = NULL;
		FREE(qent, M_NETGRAPH);
		priv->qlen--;
	}
	*mp = m;
}

/*
 * Trim fragments from the queue whose packets can never be completed.
 * This assumes a complete packet is NOT at the beginning of the queue.
 * Returns 1 if fragments were removed, zero otherwise.
 */
static int
ng_ppp_frag_trim(node_p node)
{
	const priv_p priv = node->private;
	struct ng_ppp_frag *qent, *qnext = NULL;
	int removed = 0;

	/* Scan for "dead" fragments and remove them */
	while (1) {
		int dead = 0;

		/* If queue is empty, we're done */
		if (TAILQ_EMPTY(&priv->frags))
			break;

		/* Determine whether first fragment can ever be completed */
		TAILQ_FOREACH(qent, &priv->frags, f_qent) {
			if (MP_RECV_SEQ_DIFF(priv, qent->seq, priv->mseq) >= 0)
				break;
			qnext = TAILQ_NEXT(qent, f_qent);
			KASSERT(qnext != NULL,
			    ("%s: last frag < MSEQ?", __FUNCTION__));
			if (qnext->seq != MP_NEXT_RECV_SEQ(priv, qent->seq)
			    || qent->last || qnext->first) {
				dead = 1;
				break;
			}
		}
		if (!dead)
			break;

		/* Remove fragment and all others in the same packet */
		while ((qent = TAILQ_FIRST(&priv->frags)) != qnext) {
			KASSERT(!TAILQ_EMPTY(&priv->frags),
			    ("%s: empty q", __FUNCTION__));
			priv->bundleStats.dropFragments++;
			TAILQ_REMOVE(&priv->frags, qent, f_qent);
			NG_FREE_DATA(qent->data, qent->meta);
			FREE(qent, M_NETGRAPH);
			priv->qlen--;
			removed = 1;
		}
	}
	return (removed);
}

/*
 * Run the queue, restoring the queue invariants
 */
static int
ng_ppp_frag_process(node_p node)
{
	const priv_p priv = node->private;
	struct mbuf *m;
	meta_p meta;

	/* Deliver any deliverable packets */
	while (ng_ppp_check_packet(node)) {
		ng_ppp_get_packet(node, &m, &meta);
		ng_ppp_input(node, 0, NG_PPP_BUNDLE_LINKNUM, m, meta);
	}

	/* Delete dead fragments and try again */
	if (ng_ppp_frag_trim(node)) {
		while (ng_ppp_check_packet(node)) {
			ng_ppp_get_packet(node, &m, &meta);
			ng_ppp_input(node, 0, NG_PPP_BUNDLE_LINKNUM, m, meta);
		}
	}

	/* Check for stale fragments while we're here */
	ng_ppp_frag_checkstale(node);

	/* Check queue length */
	if (priv->qlen > MP_MAX_QUEUE_LEN) {
		struct ng_ppp_frag *qent;
		int i;

		/* Get oldest fragment */
		KASSERT(!TAILQ_EMPTY(&priv->frags),
		    ("%s: empty q", __FUNCTION__));
		qent = TAILQ_FIRST(&priv->frags);

		/* Bump MSEQ if necessary */
		if (MP_RECV_SEQ_DIFF(priv, priv->mseq, qent->seq) < 0) {
			priv->mseq = qent->seq;
			for (i = 0; i < priv->numActiveLinks; i++) {
				struct ng_ppp_link *const alink =
				    &priv->links[priv->activeLinks[i]];

				if (MP_RECV_SEQ_DIFF(priv,
				    alink->seq, priv->mseq) < 0)
					alink->seq = priv->mseq;
			}
		}

		/* Drop it */
		priv->bundleStats.dropFragments++;
		TAILQ_REMOVE(&priv->frags, qent, f_qent);
		NG_FREE_DATA(qent->data, qent->meta);
		FREE(qent, M_NETGRAPH);
		priv->qlen--;

		/* Process queue again */
		return ng_ppp_frag_process(node);
	}

	/* Done */
	return (0);
}

/*
 * Check for 'stale' completed packets that need to be delivered
 *
 * If a link goes down or has a temporary failure, MSEQ can get
 * "stuck", because no new incoming fragments appear on that link.
 * This can cause completed packets to never get delivered if
 * their sequence numbers are all > MSEQ + 1.
 *
 * This routine checks how long all of the completed packets have
 * been sitting in the queue, and if too long, removes fragments
 * from the queue and increments MSEQ to allow them to be delivered.
 */
static void
ng_ppp_frag_checkstale(node_p node)
{
	const priv_p priv = node->private;
	struct ng_ppp_frag *qent, *beg, *end;
	struct timeval now, age;
	struct mbuf *m;
	meta_p meta;
	int i, seq;

	now.tv_sec = 0;			/* uninitialized state */
	while (1) {

		/* If queue is empty, we're done */
		if (TAILQ_EMPTY(&priv->frags))
			break;

		/* Find the first complete packet in the queue */
		beg = end = NULL;
		seq = TAILQ_FIRST(&priv->frags)->seq;
		TAILQ_FOREACH(qent, &priv->frags, f_qent) {
			if (qent->first)
				beg = qent;
			else if (qent->seq != seq)
				beg = NULL;
			if (beg != NULL && qent->last) {
				end = qent;
				break;
			}
			seq = MP_NEXT_RECV_SEQ(priv, seq);
		}

		/* If none found, exit */
		if (end == NULL)
			break;

		/* Get current time (we assume we've been up for >= 1 second) */
		if (now.tv_sec == 0)
			getmicrouptime(&now);

		/* Check if packet has been queued too long */
		age = now;
		timevalsub(&age, &beg->timestamp);
		if (timevalcmp(&age, &ng_ppp_max_staleness, < ))
			break;

		/* Throw away junk fragments in front of the completed packet */
		while ((qent = TAILQ_FIRST(&priv->frags)) != beg) {
			KASSERT(!TAILQ_EMPTY(&priv->frags),
			    ("%s: empty q", __FUNCTION__));
			priv->bundleStats.dropFragments++;
			TAILQ_REMOVE(&priv->frags, qent, f_qent);
			NG_FREE_DATA(qent->data, qent->meta);
			FREE(qent, M_NETGRAPH);
			priv->qlen--;
		}

		/* Extract completed packet */
		ng_ppp_get_packet(node, &m, &meta);

		/* Bump MSEQ if necessary */
		if (MP_RECV_SEQ_DIFF(priv, priv->mseq, end->seq) < 0) {
			priv->mseq = end->seq;
			for (i = 0; i < priv->numActiveLinks; i++) {
				struct ng_ppp_link *const alink =
				    &priv->links[priv->activeLinks[i]];

				if (MP_RECV_SEQ_DIFF(priv,
				    alink->seq, priv->mseq) < 0)
					alink->seq = priv->mseq;
			}
		}

		/* Deliver packet */
		ng_ppp_input(node, 0, NG_PPP_BUNDLE_LINKNUM, m, meta);
	}
}

/*
 * Periodically call ng_ppp_frag_checkstale()
 */
static void
ng_ppp_frag_timeout(void *arg)
{
	const node_p node = arg;
	const priv_p priv = node->private;
	int s = splnet();

	/* Handle the race where shutdown happens just before splnet() above */
	if ((node->flags & NG_INVALID) != 0) {
		ng_unref(node);
		splx(s);
		return;
	}

	/* Reset timer state after timeout */
	KASSERT(priv->timerActive, ("%s: !timerActive", __FUNCTION__));
	priv->timerActive = 0;
	KASSERT(node->refs > 1, ("%s: refs=%d", __FUNCTION__, node->refs));
	ng_unref(node);

	/* Start timer again */
	ng_ppp_start_frag_timer(node);

	/* Scan the fragment queue */
	ng_ppp_frag_checkstale(node);
	splx(s);
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
		struct ng_ppp_link *const link = &priv->links[linkNum];

		/* Deliver fragment(s) out the next link */
		for ( ; distrib[activeLinkNum] > 0; firstFragment = 0) {
			int len, lastFragment, error;
			struct mbuf *m2;
			meta_p meta2;

			/* Calculate fragment length; don't exceed link MTU */
			len = distrib[activeLinkNum];
			if (len > link->conf.mru)
				len = link->conf.mru;
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

				shdr = priv->xseq;
				priv->xseq =
				    (priv->xseq + 1) & MP_SHORT_SEQ_MASK;
				if (firstFragment)
					shdr |= MP_SHORT_FIRST_FLAG;
				if (lastFragment)
					shdr |= MP_SHORT_LAST_FLAG;
				shdr = htons(shdr);
				m2 = ng_ppp_prepend(m2, &shdr, 2);
			} else {
				u_int32_t lhdr;

				lhdr = priv->xseq;
				priv->xseq =
				    (priv->xseq + 1) & MP_LONG_SEQ_MASK;
				if (firstFragment)
					lhdr |= MP_LONG_FIRST_FLAG;
				if (lastFragment)
					lhdr |= MP_LONG_LAST_FLAG;
				lhdr = htonl(lhdr);
				m2 = ng_ppp_prepend(m2, &lhdr, 4);
			}
			if (m2 == NULL) {
				if (!lastFragment)
					m_freem(m);
				NG_FREE_META(meta);
				return (ENOBUFS);
			}

			/* Copy the meta information, if any */
			meta2 = lastFragment ? meta : ng_copy_meta(meta);

			/* Send fragment */
			error = ng_ppp_output(node, 0,
			    PROT_MP, linkNum, m2, meta2);
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
	int activeLinkNum;
	int t0, total, topSum, botSum;
	struct timeval now;
	int i, numFragments;

	/* If only one link, this gets real easy */
	if (priv->numActiveLinks == 1) {
		distrib[0] = len;
		return;
	}

	/* Get current time */
	getmicrouptime(&now);

	/* Compute latencies for each link at this point in time */
	for (activeLinkNum = 0;
	    activeLinkNum < priv->numActiveLinks; activeLinkNum++) {
		struct ng_ppp_link *alink;
		struct timeval diff;
		int xmitBytes;

		/* Start with base latency value */
		alink = &priv->links[priv->activeLinks[activeLinkNum]];
		latency[activeLinkNum] = alink->conf.latency;
		sortByLatency[activeLinkNum] = activeLinkNum;	/* see below */

		/* Any additional latency? */
		if (alink->bytesInQueue == 0)
			continue;

		/* Compute time delta since last write */
		diff = now;
		timevalsub(&diff, &alink->lastWrite);
		if (now.tv_sec < 0 || diff.tv_sec >= 10) {	/* sanity */
			alink->bytesInQueue = 0;
			continue;
		}

		/* How many bytes could have transmitted since last write? */
		xmitBytes = (alink->conf.bandwidth * diff.tv_sec)
		    + (alink->conf.bandwidth * (diff.tv_usec / 1000)) / 100;
		alink->bytesInQueue -= xmitBytes;
		if (alink->bytesInQueue < 0)
			alink->bytesInQueue = 0;
		else
			latency[activeLinkNum] +=
			    (100 * alink->bytesInQueue) / alink->conf.bandwidth;
	}

	/* Sort active links by latency */
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
			total += ((flowTime * priv->links[
			    priv->activeLinks[sortByLatency[i]]].conf.bandwidth)
			    	+ 99) / 100;
		}
		if (total >= len)
			break;
	}

	/* Solve for t_0 in that interval */
	for (topSum = botSum = i = 0; i < numFragments; i++) {
		int bw = priv->links[
		    priv->activeLinks[sortByLatency[i]]].conf.bandwidth;

		topSum += latency[sortByLatency[i]] * bw;	/* / 100 */
		botSum += bw;					/* / 100 */
	}
	t0 = ((len * 100) + topSum + botSum / 2) / botSum;

	/* Compute f_i(t_0) all i */
	bzero(distrib, priv->numActiveLinks * sizeof(*distrib));
	for (total = i = 0; i < numFragments; i++) {
		int bw = priv->links[
		    priv->activeLinks[sortByLatency[i]]].conf.bandwidth;

		distrib[sortByLatency[i]] =
		    (bw * (t0 - latency[sortByLatency[i]]) + 50) / 100;
		total += distrib[sortByLatency[i]];
	}

	/* Deal with any rounding error */
	if (total < len) {
		struct ng_ppp_link *fastLink =
		    &priv->links[priv->activeLinks[sortByLatency[0]]];
		int fast = 0;

		/* Find the fastest link */
		for (i = 1; i < numFragments; i++) {
			struct ng_ppp_link *const link =
			    &priv->links[priv->activeLinks[sortByLatency[i]]];

			if (link->conf.bandwidth > fastLink->conf.bandwidth) {
				fast = i;
				fastLink = link;
			}
		}
		distrib[sortByLatency[fast]] += len - total;
	} else while (total > len) {
		struct ng_ppp_link *slowLink =
		    &priv->links[priv->activeLinks[sortByLatency[0]]];
		int delta, slow = 0;

		/* Find the slowest link that still has bytes to remove */
		for (i = 1; i < numFragments; i++) {
			struct ng_ppp_link *const link =
			    &priv->links[priv->activeLinks[sortByLatency[i]]];

			if (distrib[sortByLatency[slow]] == 0
			  || (distrib[sortByLatency[i]] > 0
			    && link->conf.bandwidth <
			      slowLink->conf.bandwidth)) {
				slow = i;
				slowLink = link;
			}
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
	if (compOK && PROT_COMPRESSABLE(proto)) {
		u_char pbyte = (u_char)proto;

		return ng_ppp_prepend(m, &pbyte, 1);
	} else {
		u_int16_t pword = htons((u_int16_t)proto);

		return ng_ppp_prepend(m, &pword, 2);
	}
}

/*
 * Prepend some bytes to an mbuf
 */
static struct mbuf *
ng_ppp_prepend(struct mbuf *m, const void *buf, int len)
{
	M_PREPEND(m, len, M_NOWAIT);
	if (m == NULL || (m->m_len < len && (m = m_pullup(m, len)) == NULL))
		return (NULL);
	bcopy(buf, mtod(m, u_char *), len);
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

			hdrBytes = (priv->links[i].conf.enableACFComp ? 0 : 2)
			    + (priv->links[i].conf.enableProtoComp ? 1 : 2)
			    + (priv->conf.xmitShortSeq ? 2 : 4);
			priv->links[i].conf.latency +=
			    ((hdrBytes * priv->links[i].conf.bandwidth) + 50)
				/ 100;
		}
	}

	/* Update list of active links */
	bzero(&priv->activeLinks, sizeof(priv->activeLinks));
	priv->numActiveLinks = 0;
	priv->allLinksEqual = 1;
	for (i = 0; i < NG_PPP_MAX_LINKS; i++) {
		struct ng_ppp_link *const link = &priv->links[i];

		/* Is link active? */
		if (link->conf.enableLink && link->hook != NULL) {
			struct ng_ppp_link *link0;

			/* Add link to list of active links */
			priv->activeLinks[priv->numActiveLinks++] = i;
			link0 = &priv->links[priv->activeLinks[0]];

			/* Determine if all links are still equal */
			if (link->conf.latency != link0->conf.latency
			  || link->conf.bandwidth != link0->conf.bandwidth)
				priv->allLinksEqual = 0;

			/* Initialize rec'd sequence number */
			if (link->seq == MP_NOSEQ) {
				link->seq = (link == link0) ?
				    MP_INITIAL_SEQ : link0->seq;
			}
		} else
			link->seq = MP_NOSEQ;
	}

	/* Update MP state as multi-link is active or not */
	if (priv->conf.enableMultilink && priv->numActiveLinks > 0)
		ng_ppp_start_frag_timer(node);
	else {
		ng_ppp_stop_frag_timer(node);
		ng_ppp_frag_reset(node);
		priv->xseq = MP_INITIAL_SEQ;
		priv->mseq = MP_INITIAL_SEQ;
		for (i = 0; i < NG_PPP_MAX_LINKS; i++) {
			struct ng_ppp_link *const link = &priv->links[i];

			bzero(&link->lastWrite, sizeof(link->lastWrite));
			link->bytesInQueue = 0;
			link->seq = MP_NOSEQ;
		}
	}
}

/*
 * Determine if a new configuration would represent a valid change
 * from the current configuration and link activity status.
 */
static int
ng_ppp_config_valid(node_p node, const struct ng_ppp_node_conf *newConf)
{
	const priv_p priv = node->private;
	int i, newNumLinksActive;

	/* Check per-link config and count how many links would be active */
	for (newNumLinksActive = i = 0; i < NG_PPP_MAX_LINKS; i++) {
		if (newConf->links[i].enableLink && priv->links[i].hook != NULL)
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
	if (newConf->bund.enableMultilink && newConf->bund.mrru < MP_MIN_MRRU)
		return (0);

	/* Disallow changes to multi-link configuration while MP is active */
	if (priv->numActiveLinks > 0 && newNumLinksActive > 0) {
		if (!priv->conf.enableMultilink
				!= !newConf->bund.enableMultilink
		    || !priv->conf.xmitShortSeq != !newConf->bund.xmitShortSeq
		    || !priv->conf.recvShortSeq != !newConf->bund.recvShortSeq)
			return (0);
	}

	/* At most one link can be active unless multi-link is enabled */
	if (!newConf->bund.enableMultilink && newNumLinksActive > 1)
		return (0);

	/* Configuration change would be valid */
	return (1);
}

/*
 * Free all entries in the fragment queue
 */
static void
ng_ppp_frag_reset(node_p node)
{
	const priv_p priv = node->private;
	struct ng_ppp_frag *qent, *qnext;

	for (qent = TAILQ_FIRST(&priv->frags); qent; qent = qnext) {
		qnext = TAILQ_NEXT(qent, f_qent);
		NG_FREE_DATA(qent->data, qent->meta);
		FREE(qent, M_NETGRAPH);
	}
	TAILQ_INIT(&priv->frags);
	priv->qlen = 0;
}

/*
 * Start fragment queue timer
 */
static void
ng_ppp_start_frag_timer(node_p node)
{
	const priv_p priv = node->private;

	if (!priv->timerActive) {
		priv->fragTimer = timeout(ng_ppp_frag_timeout,
		    node, MP_FRAGTIMER_INTERVAL);
		priv->timerActive = 1;
		node->refs++;
	}
}

/*
 * Stop fragment queue timer
 */
static void
ng_ppp_stop_frag_timer(node_p node)
{
	const priv_p priv = node->private;

	if (priv->timerActive) {
		untimeout(ng_ppp_frag_timeout, node, priv->fragTimer);
		priv->timerActive = 0;
		KASSERT(node->refs > 1,
		    ("%s: refs=%d", __FUNCTION__, node->refs));
		ng_unref(node);
	}
}

