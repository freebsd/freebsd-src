
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
 * $Whistle: ng_ppp.c,v 1.22 1999/01/28 23:54:53 julian Exp $
 */

/*
 * This node does PPP protocol multiplexing based on PPP protocol
 * ID numbers. This node does not add address and control fields,
 * as that is considered a ``device layer'' issue.
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

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_ppp.h>

/* Protocol stuff */
#define PROT_DOWNLINK		0xffff
#define PROT_BYPASS		0x0000

#define PROT_VALID(p)		(((p) & 0x0101) == 0x0001)
#define PROT_COMPRESSIBLE(p)	(((p) & 0xFF00) == 0x0000)

/* Extract protocol from hook private pointer */
#define HOOK_PROTO(hook)	(*((u_int16_t *) &hook->private))

/* Node private data */
struct private {
	struct	ng_ppp_stat stats;
	u_int   protocomp:1;
};
typedef struct private *priv_p;

/* Protocol aliases */
struct protoalias {
	char   *name;
	u_int16_t proto;
};

/* Netgraph node methods */
static int	ng_ppp_constructor(node_p *nodep);
static int	ng_ppp_rcvmsg(node_p node, struct ng_mesg *msg,
		    const char *retaddr, struct ng_mesg **resp);
static int	ng_ppp_rmnode(node_p node);
static int	ng_ppp_newhook(node_p node, hook_p hook, const char *name);
static int	ng_ppp_rcvdata(hook_p hook, struct mbuf *m, meta_p meta);
static int	ng_ppp_disconnect(hook_p hook);

/* Helper stuff */
static int	ng_ppp_decodehookname(const char *name);
static hook_p	ng_ppp_findhook(node_p node, int proto);

/* Node type descriptor */
static struct ng_type typestruct = {
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
NETGRAPH_INIT(ppp, &typestruct);

/* Protocol aliases */
static const struct protoalias gAliases[] =
{
	{ NG_PPP_HOOK_DOWNLINK,		PROT_DOWNLINK	},
	{ NG_PPP_HOOK_BYPASS,		PROT_BYPASS	},
	{ NG_PPP_HOOK_LCP,		0xc021		},
	{ NG_PPP_HOOK_IPCP,		0x8021		},
	{ NG_PPP_HOOK_ATCP,		0x8029		},
	{ NG_PPP_HOOK_CCP,		0x80fd		},
	{ NG_PPP_HOOK_ECP,		0x8053		},
	{ NG_PPP_HOOK_IP,		0x0021		},
	{ NG_PPP_HOOK_VJCOMP,		0x002d		},
	{ NG_PPP_HOOK_VJUNCOMP,		0x002f		},
	{ NG_PPP_HOOK_MP,		0x003d		},
	{ NG_PPP_HOOK_COMPD,		0x00fd		},
	{ NG_PPP_HOOK_CRYPTD,		0x0053		},
	{ NG_PPP_HOOK_PAP,		0xc023		},
	{ NG_PPP_HOOK_CHAP,		0xc223		},
	{ NG_PPP_HOOK_LQR,		0xc025		},
	{ NULL,				0		}
};

#define ERROUT(x)	do { error = (x); goto done; } while (0)

/************************************************************************
			NETGRAPH NODE STUFF
 ************************************************************************/

/*
 * Node constructor
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
	if ((error = ng_make_node_common(&typestruct, nodep))) {
		FREE(priv, M_NETGRAPH);
		return (error);
	}
	(*nodep)->private = priv;

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
	int proto;

	/* Decode protocol number */
	if ((proto = ng_ppp_decodehookname(name)) < 0)
		return (EINVAL);

	/* See if already connected */
	if (ng_ppp_findhook(node, proto) != NULL)
		return (EISCONN);

	/* Clear stats when downstream hook reconnected */
	if (proto == PROT_DOWNLINK)
		bzero(&priv->stats, sizeof(priv->stats));

	/* OK */
	HOOK_PROTO(hook) = proto;
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
		case NGM_PPP_SET_PROTOCOMP:
			if (msg->header.arglen < sizeof(int))
				ERROUT(EINVAL);
			priv->protocomp = !!*((int *) msg->data);
			break;
		case NGM_PPP_GET_STATS:
			NG_MKRESPONSE(resp, msg, sizeof(priv->stats), M_NOWAIT);
			if (resp == NULL)
				ERROUT(ENOMEM);
			*((struct ng_ppp_stat *) resp->data) = priv->stats;
			break;
		case NGM_PPP_CLR_STATS:
			bzero(&priv->stats, sizeof(priv->stats));
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
	u_int16_t proto = HOOK_PROTO(hook);
	int error = 0;

	switch (proto) {

	/* Prepend the (possibly compressed) protocol number */
	default:
	    {
		int psize = (priv->protocomp
				&& PROT_COMPRESSIBLE(proto)) ? 1 : 2;

		M_PREPEND(m, psize, M_NOWAIT);
		if (!m || !(m = m_pullup(m, psize)))
			ERROUT(ENOBUFS);
		if (psize == 1)
			*mtod(m, u_char *) = proto;
		else
			*mtod(m, u_short *) = htons(proto);
		hook = ng_ppp_findhook(node, PROT_DOWNLINK);
		break;
	    }

	/* Extract the protocol number and direct to the corresponding hook */
	case PROT_DOWNLINK:
	    {
		/* Stats */
		priv->stats.recvFrames++;
		priv->stats.recvOctets += m->m_pkthdr.len;

		/* Extract protocol number */
		for (proto = 0;
		     !PROT_VALID(proto);
		     proto = (proto << 8) + *mtod(m, u_char *), m_adj(m, 1)) {
			if (m == NULL) {
				priv->stats.badProto++;
				ERROUT(EINVAL);
			}
			if ((m = m_pullup(m, 1)) == NULL)
				ERROUT(ENOBUFS);
		}

		/* Find corresponding hook; if none, use the "unhooked"
		   hook and leave the two-byte protocol prepended */
		if ((hook = ng_ppp_findhook(node, proto)) == NULL) {
			priv->stats.unknownProto++;
			hook = ng_ppp_findhook(node, PROT_BYPASS);
			M_PREPEND(m, 2, M_NOWAIT);
			if (m == NULL || (m = m_pullup(m, 2)) == NULL)
				ERROUT(ENOBUFS);
			*mtod(m, u_short *) = htons(proto);
		}
		break;
	    }

	/* Send raw data from "unhooked" hook as-is; we assume the
	   protocol is already prepended */
	case PROT_BYPASS:
		hook = ng_ppp_findhook(node, PROT_DOWNLINK);
		break;
	}

	/* Stats */
	if (m != NULL && hook != NULL && HOOK_PROTO(hook) == PROT_DOWNLINK) {
		priv->stats.xmitFrames++;
		priv->stats.xmitOctets += m->m_pkthdr.len;
	}

	/* Forward packet on hook */
	NG_SEND_DATA(error, hook, m, meta);
	return (error);

done:
	/* Something went wrong */
	NG_FREE_DATA(m, meta);
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
 * Decode ASCII protocol name
 */
static int
ng_ppp_decodehookname(const char *name)
{
	int     k, proto;

	for (k = 0; gAliases[k].name; k++)
		if (!strcmp(gAliases[k].name, name))
			return (gAliases[k].proto);
	if (strlen(name) != 6 || name[0] != '0' || name[1] != 'x')
		return (-1);
	for (proto = k = 2; k < 6; k++) {
		const u_char ch = name[k] | 0x20;
		int dig;

		if (ch >= '0' && ch <= '9')
			dig = ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			dig = ch - 'a' + 10;
		else
			return (-1);
		proto = (proto << 4) + dig;
	}
	if (!PROT_VALID(proto))
		return(-1);
	return (proto);
}

/*
 * Find a hook by protocol number
 */
static hook_p
ng_ppp_findhook(node_p node, int proto)
{
	hook_p hook;

	LIST_FOREACH(hook, &node->hooks, hooks) {
		if (HOOK_PROTO(hook) == proto)
			return (hook);
	}
	return (NULL);
}

