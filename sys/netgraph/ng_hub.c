/*-
 * Copyright (c) 2004 Ruslan Ermilov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/systm.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_hub.h>
#include <netgraph/netgraph.h>

static ng_constructor_t	ng_hub_constructor;
static ng_rcvdata_t	ng_hub_rcvdata;
static ng_disconnect_t	ng_hub_disconnect;

static struct ng_type ng_hub_typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_HUB_NODE_TYPE,
	.constructor =	ng_hub_constructor,
	.rcvdata =	ng_hub_rcvdata,
	.rcvdataq =	ng_hub_rcvdata,
	.disconnect =	ng_hub_disconnect,
};
NETGRAPH_INIT(hub, &ng_hub_typestruct);


static int
ng_hub_constructor(node_p *nodep)
{

	/* Call the generic node constructor. */
	return (ng_make_node_common(&ng_hub_typestruct, nodep));
}

static int
ng_hub_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const node_p node = NG_HOOK_NODE(hook);
	int error = 0;
	hook_p hook2;
	struct mbuf *m2;
	meta_p meta2;
	int nhooks;

	if ((nhooks = NG_NODE_NUMHOOKS(node)) == 1) {
		NG_FREE_DATA(m, meta);
		return (0);
	}
	LIST_FOREACH(hook2, &node->hooks, hooks) {
		if (hook2 == hook)
			continue;
		if (--nhooks == 1)
			NG_SEND_DATA(error, hook2, m, meta);
		else {
			if ((m2 = m_dup(m, M_DONTWAIT)) == NULL) {
				NG_FREE_DATA(m, meta);
				return (ENOBUFS);
			}
			if (meta != NULL) {
				if ((meta2 = ng_copy_meta(meta)) == NULL) {
					m_freem(m2);
					NG_FREE_DATA(m, meta);
					return (ENOMEM);
				}
			} else
				meta2 = NULL;
			NG_SEND_DATA(error, hook2, m2, meta2);
			if (error)
				continue;	/* don't give up */
		}
	}
	return (error);
}

static int
ng_hub_disconnect(hook_p hook)
{

	if (NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0 &&
	    NG_NODE_IS_VALID(NG_HOOK_NODE(hook)))
		ng_rmnode(NG_HOOK_NODE(hook));
	return (0);
}
