
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
 * Author: Julian Elischer <julian@whistle.com>
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
#include <netgraph/ng_tee.h>

/* Per hook info */
struct hookinfo {
	hook_p  hook;
	int     bytes;
	int     packets;
	int     flags;
};

/* Per node info */
struct privdata {
	node_p  node;
	int     flags;
	struct hookinfo left;
	struct hookinfo right;
	struct hookinfo left2right;
	struct hookinfo right2left;
};
typedef struct privdata *sc_p;

/* Netgraph methods */
static ng_constructor_t	ngt_constructor;
static ng_rcvmsg_t	ngt_rcvmsg;
static ng_shutdown_t	ngt_rmnode;
static ng_newhook_t	ngt_newhook;
static ng_rcvdata_t	ngt_rcvdata;
static ng_disconnect_t	ngt_disconnect;

/* Netgraph type descriptor */
static struct ng_type typestruct = {
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
	ngt_disconnect
};
NETGRAPH_INIT(tee, &typestruct);

/*
 * Node constructor
 */
static int
ngt_constructor(node_p *nodep)
{
	sc_p privdata;
	int error = 0;

	MALLOC(privdata, sc_p, sizeof(*privdata), M_NETGRAPH, M_WAITOK);
	if (privdata == NULL)
		return (ENOMEM);
	bzero(privdata, sizeof(*privdata));

	if ((error = ng_make_node_common(&typestruct, nodep))) {
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
		sc->right.bytes = 0;
		sc->right.packets = 0;
		hook->private = &sc->right;
	} else if (strcmp(name, NG_TEE_HOOK_LEFT) == 0) {
		sc->left.hook = hook;
		sc->left.bytes = 0;
		sc->left.packets = 0;
		hook->private = &sc->left;
	} else if (strcmp(name, NG_TEE_HOOK_RIGHT2LEFT) == 0) {
		sc->right2left.hook = hook;
		sc->right2left.bytes = 0;
		sc->right2left.packets = 0;
		hook->private = &sc->right2left;
	} else if (strcmp(name, NG_TEE_HOOK_LEFT2RIGHT) == 0) {
		sc->left2right.hook = hook;
		sc->left2right.bytes = 0;
		sc->left2right.packets = 0;
		hook->private = &sc->left2right;
	} else
		return (EINVAL);
	return (0);
}

/*
 * We don't support any type-specific messages
 */
static int
ngt_rcvmsg(node_p node, struct ng_mesg *msg, const char *retaddr,
	   struct ng_mesg **resp)
{
	FREE(msg, M_NETGRAPH);
	return (EINVAL);
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
ngt_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const sc_p sc = hook->node->private;
	struct hookinfo *hi;
	struct hookinfo *dest;
	struct hookinfo *dup;
	struct mbuf *mdup;
	int error = 0;

	if ((hi = hook->private) != NULL) {
		if (hi == &sc->left) {
			dup = &sc->left2right;
			dest = &sc->right;
		} else if (hi == &sc->right) {
			dup = &sc->right2left;
			dest = &sc->left;
		} else if (hi == &sc->right2left) {
			dup = NULL;
			dest = &sc->left;
		} else if (hi == &sc->left2right) {
			dup = NULL;
			dest = &sc->right;
		} else
			goto out;
		if (dup) {
			mdup = m_copypacket(m, M_NOWAIT);
			if (mdup) {
				/* XXX should we duplicate meta? */
				/* for now no.			 */
				void   *x = NULL;

				NG_SEND_DATA(error, dup->hook, mdup, x);
			}
		}
		NG_SEND_DATA(error, dest->hook, m, meta);
	}

out:
	NG_FREE_DATA(m, meta);
	return (error);
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
	struct hookinfo *hi;

	if ((hi = hook->private) != NULL)
		hi->hook = NULL;
	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	return (0);
}

