
/*
 * ng_tee.c
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
 * $Whistle: ng_tee.c,v 1.18 1999/11/01 09:24:52 julian Exp $
 */

/*
 * This node is like the tee(1) command and is useful for ``snooping.''
 * It has 4 hooks: left, right, left2right, and right2left. Data
 * entering from the right is passed to the left and duplicated on
 * right2left, and data entering from the left is passed to the right
 * and duplicated on left2right. Data entering from left2right is
 * sent to right, and data from right2left to left.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_tee.h>

/* Per hook info */
struct hookinfo {
	hook_p			hook;
	struct ng_tee_hookstat	stats;
};

/* Per node info */
struct privdata {
	node_p			node;
	struct hookinfo		left;
	struct hookinfo		right;
	struct hookinfo		left2right;
	struct hookinfo		right2left;
};
typedef struct privdata *sc_p;

/* Netgraph methods */
static ng_constructor_t	ngt_constructor;
static ng_rcvmsg_t	ngt_rcvmsg;
static ng_shutdown_t	ngt_rmnode;
static ng_newhook_t	ngt_newhook;
static ng_rcvdata_t	ngt_rcvdata;
static ng_disconnect_t	ngt_disconnect;

/* Parse type for struct ng_tee_hookstat */
static const struct ng_parse_struct_info
	ng_tee_hookstat_type_info = NG_TEE_HOOKSTAT_INFO;
static const struct ng_parse_type ng_tee_hookstat_type = {
	&ng_parse_struct_type,
	&ng_tee_hookstat_type_info,
};

/* Parse type for struct ng_tee_stats */
static const struct ng_parse_struct_info
	ng_tee_stats_type_info = NG_TEE_STATS_INFO(&ng_tee_hookstat_type);
static const struct ng_parse_type ng_tee_stats_type = {
	&ng_parse_struct_type,
	&ng_tee_stats_type_info,
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_tee_cmds[] = {
	{
	  NGM_TEE_COOKIE,
	  NGM_TEE_GET_STATS,
	  "getstats",
	  NULL,
	  &ng_tee_stats_type
	},
	{
	  NGM_TEE_COOKIE,
	  NGM_TEE_CLR_STATS,
	  "clrstats",
	  NULL,
	  NULL
	},
	{
	  NGM_TEE_COOKIE,
	  NGM_TEE_GETCLR_STATS,
	  "getclrstats",
	  NULL,
	  &ng_tee_stats_type
	},
	{ 0 }
};

/* Netgraph type descriptor */
static struct ng_type ng_tee_typestruct = {
	NG_VERSION,
	NG_TEE_NODE_TYPE,
	NULL,
	ngt_constructor,
	ngt_rcvmsg,
	ngt_rmnode,
	ngt_newhook,
	NULL,
	NULL,
	ngt_rcvdata,
	ngt_rcvdata,
	ngt_disconnect,
	ng_tee_cmds
};
NETGRAPH_INIT(tee, &ng_tee_typestruct);

/*
 * Node constructor
 */
static int
ngt_constructor(node_p *nodep)
{
	sc_p privdata;
	int error = 0;

	MALLOC(privdata, sc_p, sizeof(*privdata), M_NETGRAPH, M_NOWAIT);
	if (privdata == NULL)
		return (ENOMEM);
	bzero(privdata, sizeof(*privdata));

	if ((error = ng_make_node_common(&ng_tee_typestruct, nodep))) {
		FREE(privdata, M_NETGRAPH);
		return (error);
	}
	(*nodep)->private = privdata;
	privdata->node = *nodep;
	return (0);
}

/*
 * Add a hook
 */
static int
ngt_newhook(node_p node, hook_p hook, const char *name)
{
	const sc_p sc = node->private;

	if (strcmp(name, NG_TEE_HOOK_RIGHT) == 0) {
		sc->right.hook = hook;
		bzero(&sc->right.stats, sizeof(sc->right.stats));
		hook->private = &sc->right;
	} else if (strcmp(name, NG_TEE_HOOK_LEFT) == 0) {
		sc->left.hook = hook;
		bzero(&sc->left.stats, sizeof(sc->left.stats));
		hook->private = &sc->left;
	} else if (strcmp(name, NG_TEE_HOOK_RIGHT2LEFT) == 0) {
		sc->right2left.hook = hook;
		bzero(&sc->right2left.stats, sizeof(sc->right2left.stats));
		hook->private = &sc->right2left;
	} else if (strcmp(name, NG_TEE_HOOK_LEFT2RIGHT) == 0) {
		sc->left2right.hook = hook;
		bzero(&sc->left2right.stats, sizeof(sc->left2right.stats));
		hook->private = &sc->left2right;
	} else
		return (EINVAL);
	return (0);
}

/*
 * Receive a control message
 */
static int
ngt_rcvmsg(node_p node, struct ng_mesg *msg, const char *retaddr,
	   struct ng_mesg **rptr, hook_p lasthook)
{
	const sc_p sc = node->private;
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_TEE_COOKIE:
		switch (msg->header.cmd) {
		case NGM_TEE_GET_STATS:
		case NGM_TEE_CLR_STATS:
		case NGM_TEE_GETCLR_STATS:
                    {
			struct ng_tee_stats *stats;

                        if (msg->header.cmd != NGM_TEE_CLR_STATS) {
                                NG_MKRESPONSE(resp, msg,
                                    sizeof(*stats), M_NOWAIT);
				if (resp == NULL) {
					error = ENOMEM;
					goto done;
				}
				stats = (struct ng_tee_stats *)resp->data;
				bcopy(&sc->right.stats, &stats->right,
				    sizeof(stats->right));
				bcopy(&sc->left.stats, &stats->left,
				    sizeof(stats->left));
				bcopy(&sc->right2left.stats, &stats->right2left,
				    sizeof(stats->right2left));
				bcopy(&sc->left2right.stats, &stats->left2right,
				    sizeof(stats->left2right));
                        }
                        if (msg->header.cmd != NGM_TEE_GET_STATS) {
				bzero(&sc->right.stats,
				    sizeof(sc->right.stats));
				bzero(&sc->left.stats,
				    sizeof(sc->left.stats));
				bzero(&sc->right2left.stats,
				    sizeof(sc->right2left.stats));
				bzero(&sc->left2right.stats,
				    sizeof(sc->left2right.stats));
			}
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
 * Receive data on a hook
 *
 * If data comes in the right link send a copy out right2left, and then
 * send the original onwards out through the left link.
 * Do the opposite for data coming in from the left link.
 * Data coming in right2left or left2right is forwarded
 * on through the appropriate destination hook as if it had come
 * from the other side.
 */
static int
ngt_rcvdata(hook_p hook, struct mbuf *m, meta_p meta,
		struct mbuf **ret_m, meta_p *ret_meta)
{
	const sc_p sc = hook->node->private;
	struct hookinfo *const hinfo = (struct hookinfo *) hook->private;
	struct hookinfo *dest;
	struct hookinfo *dup;
	int error = 0;

	/* Which hook? */
	if (hinfo == &sc->left) {
		dup = &sc->left2right;
		dest = &sc->right;
	} else if (hinfo == &sc->right) {
		dup = &sc->right2left;
		dest = &sc->left;
	} else if (hinfo == &sc->right2left) {
		dup = NULL;
		dest = &sc->left;
	} else if (hinfo == &sc->left2right) {
		dup = NULL;
		dest = &sc->right;
	} else
		panic("%s: no hook!", __FUNCTION__);

	/* Update stats on incoming hook */
	hinfo->stats.inOctets += m->m_pkthdr.len;
	hinfo->stats.inFrames++;

	/* Duplicate packet and meta info if requried */
	if (dup != NULL) {
		struct mbuf *m2;
		meta_p meta2;

		/* Copy packet */
		m2 = m_dup(m, M_NOWAIT);
		if (m2 == NULL) {
			NG_FREE_DATA(m, meta);
			return (ENOBUFS);
		}

		/* Copy meta info */
		if (meta != NULL) {
			MALLOC(meta2, meta_p,
			    meta->used_len, M_NETGRAPH, M_NOWAIT);
			if (meta2 == NULL) {
				m_freem(m2);
				NG_FREE_DATA(m, meta);
				return (ENOMEM);
			}
			bcopy(meta, meta2, meta->used_len);
			meta2->allocated_len = meta->used_len;
		} else
			meta2 = NULL;

		/* Deliver duplicate */
		dup->stats.outOctets += m->m_pkthdr.len;
		dup->stats.outFrames++;
		NG_SEND_DATA(error, dup->hook, m2, meta2);
	}

	/* Deliver frame out destination hook */
	dest->stats.outOctets += m->m_pkthdr.len;
	dest->stats.outFrames++;
	NG_SEND_DATA(error, dest->hook, m, meta);
	return (0);
}

/*
 * Shutdown processing
 *
 * This is tricky. If we have both a left and right hook, then we
 * probably want to extricate ourselves and leave the two peers
 * still linked to each other. Otherwise we should just shut down as
 * a normal node would.
 *
 * To keep the scope of info correct the routine to "extract" a node
 * from two links is in ng_base.c.
 */
static int
ngt_rmnode(node_p node)
{
	const sc_p privdata = node->private;

	node->flags |= NG_INVALID;
	if (privdata->left.hook && privdata->right.hook)
		ng_bypass(privdata->left.hook, privdata->right.hook);
	ng_cutlinks(node);
	ng_unname(node);
	node->private = NULL;
	ng_unref(privdata->node);
	FREE(privdata, M_NETGRAPH);
	return (0);
}

/*
 * Hook disconnection
 */
static int
ngt_disconnect(hook_p hook)
{
	struct hookinfo *const hinfo = (struct hookinfo *) hook->private;

	KASSERT(hinfo != NULL, ("%s: null info", __FUNCTION__));
	hinfo->hook = NULL;
	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	return (0);
}

