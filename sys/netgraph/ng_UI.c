
/*
 * ng_UI.c
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
 * $Whistle: ng_UI.c,v 1.14 1999/11/01 09:24:51 julian Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>


#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_UI.h>

/*
 * DEFINITIONS
 */

/* Everything, starting with sdlc on has defined UI as 0x03 */
#define HDLC_UI	0x03

/* Node private data */
struct ng_UI_private {
	hook_p  downlink;
	hook_p  uplink;
};
typedef struct ng_UI_private *priv_p;

/* Netgraph node methods */
static ng_constructor_t	ng_UI_constructor;
static ng_rcvmsg_t	ng_UI_rcvmsg;
static ng_shutdown_t	ng_UI_rmnode;
static ng_newhook_t	ng_UI_newhook;
static ng_rcvdata_t	ng_UI_rcvdata;
static ng_disconnect_t	ng_UI_disconnect;

/* Node type descriptor */
static struct ng_type typestruct = {
	NG_VERSION,
	NG_UI_NODE_TYPE,
	NULL,
	ng_UI_constructor,
	ng_UI_rcvmsg,
	ng_UI_rmnode,
	ng_UI_newhook,
	NULL,
	NULL,
	ng_UI_rcvdata,
	ng_UI_rcvdata,
	ng_UI_disconnect,
	NULL
};
NETGRAPH_INIT(UI, &typestruct);

/************************************************************************
			NETGRAPH NODE STUFF
 ************************************************************************/

/*
 * Create a newborn node. We start with an implicit reference.
 */

static int
ng_UI_constructor(node_p *nodep)
{
	priv_p  priv;
	int     error;

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
 * Give our ok for a hook to be added
 */
static int
ng_UI_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = node->private;

	if (!strcmp(name, NG_UI_HOOK_DOWNSTREAM)) {
		if (priv->downlink)
			return (EISCONN);
		priv->downlink = hook;
	} else if (!strcmp(name, NG_UI_HOOK_UPSTREAM)) {
		if (priv->uplink)
			return (EISCONN);
		priv->uplink = hook;
	} else
		return (EINVAL);
	return (0);
}

/*
 * Receive a control message
 */
static int
ng_UI_rcvmsg(node_p node, struct ng_mesg *msg,
	     const char *raddr, struct ng_mesg **rp)
{
	FREE(msg, M_NETGRAPH);
	return (EINVAL);
}

#define MAX_ENCAPS_HDR	1
#define ERROUT(x)	do { error = (x); goto done; } while (0)

/*
 * Receive a data frame
 */
static int
ng_UI_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const node_p node = hook->node;
	const priv_p priv = node->private;
	int error = 0;

	if (hook == priv->downlink) {
		u_char *start, *ptr;

		if (!m || (m->m_len < MAX_ENCAPS_HDR
		    && !(m = m_pullup(m, MAX_ENCAPS_HDR))))
			ERROUT(ENOBUFS);
		ptr = start = mtod(m, u_char *);

		/* Must be UI frame */
		if (*ptr++ != HDLC_UI)
			ERROUT(0);

		m_adj(m, ptr - start);
		NG_SEND_DATA(error, priv->uplink, m, meta);	/* m -> NULL */
	} else if (hook == priv->uplink) {
		M_PREPEND(m, 1, M_DONTWAIT);	/* Prepend IP NLPID */
		if (!m)
			ERROUT(ENOBUFS);
		mtod(m, u_char *)[0] = HDLC_UI;
		NG_SEND_DATA(error, priv->downlink, m, meta);	/* m -> NULL */
	} else
		panic(__FUNCTION__);

done:
	NG_FREE_DATA(m, meta);	/* does nothing if m == NULL */
	return (error);
}

/*
 * Shutdown node
 */
static int
ng_UI_rmnode(node_p node)
{
	const priv_p priv = node->private;

	/* Take down netgraph node */
	node->flags |= NG_INVALID;
	ng_cutlinks(node);
	ng_unname(node);
	bzero(priv, sizeof(*priv));
	FREE(priv, M_NETGRAPH);
	node->private = NULL;
	ng_unref(node);
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_UI_disconnect(hook_p hook)
{
	const priv_p priv = hook->node->private;

	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	else if (hook == priv->downlink)
		priv->downlink = NULL;
	else if (hook == priv->uplink)
		priv->uplink = NULL;
	else
		panic(__FUNCTION__);
	return (0);
}

