/*-
 *
 * Copyright (c) 1999-2000, Vitaly V Belekhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	$FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_split.h>

/* Netgraph methods */
static ng_constructor_t ng_split_constructor;
static ng_rcvmsg_t ng_split_rcvmsg;
static ng_shutdown_t ng_split_rmnode;
static ng_newhook_t ng_split_newhook;
static ng_rcvdata_t ng_split_rcvdata;
static ng_connect_t ng_split_connect;
static ng_disconnect_t ng_split_disconnect;

/* Node type descriptor */
static struct ng_type typestruct = {
	NG_ABI_VERSION,
	NG_SPLIT_NODE_TYPE,
	NULL,
	ng_split_constructor,
	ng_split_rcvmsg,
	ng_split_rmnode,
	ng_split_newhook,
	NULL,
	ng_split_connect,
	ng_split_rcvdata,
	ng_split_disconnect,
	NULL
};
NETGRAPH_INIT(ng_split, &typestruct);

/* Node private data */
struct ng_split_private {
        hook_p outhook;
        hook_p inhook;
        hook_p mixed;
	node_p	node;			/* Our netgraph node */
};
typedef struct ng_split_private *priv_p;

/************************************************************************
			NETGRAPH NODE STUFF
 ************************************************************************/

/*
 * Constructor for a node
 */
static int
ng_split_constructor(node_p node)
{
	priv_p          priv;

	/* Allocate node */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_ZERO | M_NOWAIT);
	if (priv == NULL)
		return (ENOMEM);
	bzero(priv, sizeof(*priv));

	/* Link together node and private info */
	NG_NODE_SET_PRIVATE(node, priv);
	priv->node = node;

	/* Done */
	return (0);
}

/*
 * Give our ok for a hook to be added
 */
static int
ng_split_newhook(node_p node, hook_p hook, const char *name)
{
	priv_p          priv = NG_NODE_PRIVATE(node);

	if (strcmp(name, NG_SPLIT_HOOK_MIXED)) {
		if (strcmp(name, NG_SPLIT_HOOK_INHOOK)) {
			if (strcmp(name, NG_SPLIT_HOOK_OUTHOOK))
				return (EPFNOSUPPORT);
			else {
				if (priv->outhook != NULL)
					return (EISCONN);
				priv->outhook = hook;
				NG_HOOK_SET_PRIVATE(hook, &(priv->outhook));
			}
		} else {
			if (priv->inhook != NULL)
				return (EISCONN);
			priv->inhook = hook;
			NG_HOOK_SET_PRIVATE(hook, &(priv->inhook));
		}
	} else {
		if (priv->mixed != NULL)
			return (EISCONN);
		priv->mixed = hook;
		NG_HOOK_SET_PRIVATE(hook, &(priv->mixed));
	}

	return (0);
}

/*
 * Receive a control message
 */
static int
ng_split_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	NG_FREE_ITEM(item);
	return (EINVAL);
}

/*
 * Recive data from a hook.
 */
static int
ng_split_rcvdata(hook_p hook, item_p item)
{
	meta_p          meta;
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	int             error = 0;

	if (hook == priv->outhook) {
		printf("ng_split: got packet from outhook!\n");
		NG_FREE_ITEM(item);
		return (EINVAL);
	}
#if 0 /* should never happen */
	if (NGI_M(item) == NULL) {
		printf("ng_split: mbuf is null.\n");
		NG_FREE_ITEM(item);
		return (EINVAL);
	}
#endif
	/* 
	 * XXX Really here we should just remove metadata we understand.
	 */
	NGI_GET_META(item, meta);
	NG_FREE_META(meta);
	if ((hook == priv->inhook) && (priv->mixed)) {
		NG_FWD_ITEM_HOOK(error, item, priv->mixed);
	} else if ((hook == priv->mixed) && (priv->outhook)) {
		NG_FWD_ITEM_HOOK(error, item, priv->outhook);
	}
	return (error);
}

static int
ng_split_rmnode(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);
	FREE(priv, M_NETGRAPH);

	return (0);
}


/*
 * This is called once we've already connected a new hook to the other node.
 * It gives us a chance to balk at the last minute.
 */
static int
ng_split_connect(hook_p hook)
{
	/* be really amiable and just say "YUP that's OK by me! " */
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_split_disconnect(hook_p hook)
{
	if (NG_HOOK_PRIVATE(hook)) {
		*((hook_p *)NG_HOOK_PRIVATE(hook)) = (hook_p)0;
	}

	if ((NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0)
	&& (NG_NODE_IS_VALID(NG_HOOK_NODE(hook)))) {
			ng_rmnode_self(NG_HOOK_NODE(hook));
	}

	return (0);
}
