/*-
 * Copyright (c) 2003 IPNET Internet Communication Company
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
 * Author: Ruslan Ermilov <ru@FreeBSD.org>
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_vlan_var.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_vlan.h>
#include <netgraph/netgraph.h>

static ng_constructor_t	ng_vlan_constructor;
static ng_rcvmsg_t	ng_vlan_rcvmsg;
static ng_shutdown_t	ng_vlan_shutdown;
static ng_newhook_t	ng_vlan_newhook;
static ng_rcvdata_t	ng_vlan_rcvdata;
static ng_disconnect_t	ng_vlan_disconnect;

/* Parse type for struct ng_vlan_filter. */
static const struct ng_parse_struct_field ng_vlan_filter_fields[] =
	NG_VLAN_FILTER_FIELDS;
static const struct ng_parse_type ng_vlan_filter_type = {
	&ng_parse_struct_type,
	&ng_vlan_filter_fields
};

static int
ng_vlan_getTableLength(const struct ng_parse_type *type,
    const u_char *start, const u_char *buf)
{
	const struct ng_vlan_table *const table =
	    (const struct ng_vlan_table *)(buf - sizeof(u_int32_t));

	return table->n;
}

/* Parse type for struct ng_vlan_table. */
static const struct ng_parse_array_info ng_vlan_table_array_info = {
	&ng_vlan_filter_type,
	ng_vlan_getTableLength
};
static const struct ng_parse_type ng_vlan_table_array_type = {
	&ng_parse_array_type,
	&ng_vlan_table_array_info
};
static const struct ng_parse_struct_field ng_vlan_table_fields[] =
	NG_VLAN_TABLE_FIELDS;
static const struct ng_parse_type ng_vlan_table_type = {
	&ng_parse_struct_type,
	&ng_vlan_table_fields
};

/* List of commands and how to convert arguments to/from ASCII. */
static const struct ng_cmdlist ng_vlan_cmdlist[] = {
	{
	  NGM_VLAN_COOKIE,
	  NGM_VLAN_ADD_FILTER,
	  "addfilter",
	  &ng_vlan_filter_type,
	  NULL
	},
	{
	  NGM_VLAN_COOKIE,
	  NGM_VLAN_DEL_FILTER,
	  "delfilter",
	  &ng_parse_hookbuf_type,
	  NULL
	},
	{
	  NGM_VLAN_COOKIE,
	  NGM_VLAN_GET_TABLE,
	  "gettable",
	  NULL,
	  &ng_vlan_table_type
	},
	{ 0 }
};

static struct ng_type ng_vlan_typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_VLAN_NODE_TYPE,
	.constructor =	ng_vlan_constructor,
	.rcvmsg =	ng_vlan_rcvmsg,
	.shutdown =	ng_vlan_shutdown,
	.newhook =	ng_vlan_newhook,
	.rcvdata =	ng_vlan_rcvdata,
	.disconnect =	ng_vlan_disconnect,
	.cmdlist =	ng_vlan_cmdlist,
};
NETGRAPH_INIT(vlan, &ng_vlan_typestruct);

struct filter {
	LIST_ENTRY(filter) next;
	u_int16_t	vlan;
	hook_p		hook;
};

#define	HASHSIZE	16
#define	HASH(id)	((((id) >> 8) ^ ((id) >> 4) ^ (id)) & 0x0f)
LIST_HEAD(filterhead, filter);

typedef struct {
	hook_p		downstream_hook;
	hook_p		nomatch_hook;
	struct filterhead hashtable[HASHSIZE];
	u_int32_t	nent;
} *priv_p;

static struct filter *
ng_vlan_findentry(priv_p priv, u_int16_t vlan)
{
	struct filterhead *chain = &priv->hashtable[HASH(vlan)];
	struct filter *f;

	LIST_FOREACH(f, chain, next)
		if (f->vlan == vlan)
			return (f);
	return (NULL);
}

static int
ng_vlan_constructor(node_p node)
{
	priv_p priv;
	int i;

	priv = malloc(sizeof(*priv), M_NETGRAPH, M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return (ENOMEM);
	for (i = 0; i < HASHSIZE; i++)
		LIST_INIT(&priv->hashtable[i]);
	NG_NODE_SET_PRIVATE(node, priv);
	return (0);
}

static int
ng_vlan_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	if (strcmp(name, NG_VLAN_HOOK_DOWNSTREAM) == 0)
		priv->downstream_hook = hook;
	else if (strcmp(name, NG_VLAN_HOOK_NOMATCH) == 0)
		priv->nomatch_hook = hook;
	else {
		/*
		 * Any other hook name is valid and can
		 * later be associated with a filter rule.
		 */
	}
	NG_HOOK_SET_PRIVATE(hook, NULL);
	return (0);
}

static int
ng_vlan_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	int error = 0;
	struct ng_mesg *msg, *resp = NULL;
	struct ng_vlan_filter *vf;
	struct filter *f;
	hook_p hook;
	struct ng_vlan_table *t;
	int i;

	NGI_GET_MSG(item, msg);
	/* Deal with message according to cookie and command. */
	switch (msg->header.typecookie) {
	case NGM_VLAN_COOKIE:
		switch (msg->header.cmd) {
		case NGM_VLAN_ADD_FILTER:
			/* Check that message is long enough. */
			if (msg->header.arglen != sizeof(*vf)) {
				error = EINVAL;
				break;
			}
			vf = (struct ng_vlan_filter *)msg->data;
			/* Sanity check the VLAN ID value. */
			if (vf->vlan & ~EVL_VLID_MASK) {
				error = EINVAL;
				break;
			}
			/* Check that a referenced hook exists. */
			hook = ng_findhook(node, vf->hook);
			if (hook == NULL) {
				error = ENOENT;
				break;
			}
			/* And is not one of the special hooks. */
			if (hook == priv->downstream_hook ||
			    hook == priv->nomatch_hook) {
				error = EINVAL;
				break;
			}
			/* And is not already in service. */
			if (NG_HOOK_PRIVATE(hook) != NULL) {
				error = EEXIST;
				break;
			}
			/* Check we don't already trap this VLAN. */
			if (ng_vlan_findentry(priv, vf->vlan)) {
				error = EEXIST;
				break;
			}
			/* Create filter. */
			f = malloc(sizeof(*f),
			    M_NETGRAPH, M_NOWAIT | M_ZERO);
			if (f == NULL) {
				error = ENOMEM;
				break;
			}
			/* Link filter and hook together. */
			f->hook = hook;
			f->vlan = vf->vlan;
			NG_HOOK_SET_PRIVATE(hook, f);
			/* Register filter in a hash table. */
			LIST_INSERT_HEAD(
			    &priv->hashtable[HASH(f->vlan)], f, next);
			priv->nent++;
			break;
		case NGM_VLAN_DEL_FILTER:
			/* Check that message is long enough. */
			if (msg->header.arglen != NG_HOOKSIZ) {
				error = EINVAL;
				break;
			}
			/* Check that hook exists and is active. */
			hook = ng_findhook(node, (char *)msg->data);
			if (hook == NULL ||
			    (f = NG_HOOK_PRIVATE(hook)) == NULL) {
				error = ENOENT;
				break;
			}
			/* Purge a rule that refers to this hook. */
			NG_HOOK_SET_PRIVATE(hook, NULL);
			LIST_REMOVE(f, next);
			priv->nent--;
			free(f, M_NETGRAPH);
			break;
		case NGM_VLAN_GET_TABLE:
			NG_MKRESPONSE(resp, msg, sizeof(*t) +
			    priv->nent * sizeof(*t->filter), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			t = (struct ng_vlan_table *)resp->data;
			t->n = priv->nent;
			vf = &t->filter[0];
			for (i = 0; i < HASHSIZE; i++) {
				LIST_FOREACH(f, &priv->hashtable[i], next) {
					vf->vlan = f->vlan;
					strncpy(vf->hook, NG_HOOK_NAME(f->hook),
					    NG_HOOKSIZ);
					vf++;
				}
			}
			break;
		default:		/* Unknown command. */
			error = EINVAL;
			break;
		}
		break;
	case NGM_FLOW_COOKIE:
	    {
		struct ng_mesg *copy;
		struct filterhead *chain;
		struct filter *f;

		/*
		 * Flow control messages should come only
		 * from downstream.
		 */

		if (lasthook == NULL)
			break;
		if (lasthook != priv->downstream_hook)
			break;

		/* Broadcast the event to all uplinks. */
		for (i = 0, chain = priv->hashtable; i < HASHSIZE;
		    i++, chain++)
		LIST_FOREACH(f, chain, next) {
			NG_COPYMESSAGE(copy, msg, M_NOWAIT);
			if (copy == NULL)
				continue;
			NG_SEND_MSG_HOOK(error, node, copy, f->hook, 0);
		}

		break;
	    }
	default:			/* Unknown type cookie. */
		error = EINVAL;
		break;
	}
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

static int
ng_vlan_rcvdata(hook_p hook, item_p item)
{
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct ether_header *eh;
	struct ether_vlan_header *evl = NULL;
	int error;
	u_int16_t vlan;
	struct mbuf *m;
	struct filter *f;

	/* Make sure we have an entire header. */
	NGI_GET_M(item, m);
	if (m->m_len < sizeof(*eh) &&
	    (m = m_pullup(m, sizeof(*eh))) == NULL) {
		NG_FREE_ITEM(item);
		return (EINVAL);
	}
	eh = mtod(m, struct ether_header *);
	if (hook == priv->downstream_hook) {
		/*
		 * If from downstream, select between a match hook
		 * or the nomatch hook.
		 */
		if (m->m_flags & M_VLANTAG ||
		    eh->ether_type == htons(ETHERTYPE_VLAN)) {
			if (m->m_flags & M_VLANTAG) {
				/*
				 * Packet is tagged, m contains a normal
				 * Ethernet frame; tag is stored out-of-band.
				 */
				vlan = EVL_VLANOFTAG(m->m_pkthdr.ether_vtag);
			} else {
				if (m->m_len < sizeof(*evl) &&
				    (m = m_pullup(m, sizeof(*evl))) == NULL) {
					NG_FREE_ITEM(item);
					return (EINVAL);
				}
				evl = mtod(m, struct ether_vlan_header *);
				vlan = EVL_VLANOFTAG(ntohs(evl->evl_tag));
			}
			if ((f = ng_vlan_findentry(priv, vlan)) != NULL) {
				if (m->m_flags & M_VLANTAG) {
					m->m_pkthdr.ether_vtag = 0;
					m->m_flags &= ~M_VLANTAG;
				} else {
					evl->evl_encap_proto = evl->evl_proto;
					bcopy(mtod(m, caddr_t),
					    mtod(m, caddr_t) +
					    ETHER_VLAN_ENCAP_LEN,
					    ETHER_HDR_LEN);
					m_adj(m, ETHER_VLAN_ENCAP_LEN);
				}
			}
		} else
			f = NULL;
		if (f != NULL)
			NG_FWD_NEW_DATA(error, item, f->hook, m);
		else
			NG_FWD_NEW_DATA(error, item, priv->nomatch_hook, m);
	} else {
		/*
		 * It is heading towards the downstream.
		 * If from nomatch, pass it unmodified.
		 * Otherwise, do the VLAN encapsulation.
		 */
		if (hook != priv->nomatch_hook) {
			if ((f = NG_HOOK_PRIVATE(hook)) == NULL) {
				NG_FREE_ITEM(item);
				NG_FREE_M(m);
				return (EOPNOTSUPP);
			}
			M_PREPEND(m, ETHER_VLAN_ENCAP_LEN, M_DONTWAIT);
			/* M_PREPEND takes care of m_len and m_pkthdr.len. */
			if (m == NULL || (m->m_len < sizeof(*evl) &&
			    (m = m_pullup(m, sizeof(*evl))) == NULL)) {
				NG_FREE_ITEM(item);
				return (ENOMEM);
			}
			/*
			 * Transform the Ethernet header into an Ethernet header
			 * with 802.1Q encapsulation.
			 */
			bcopy(mtod(m, char *) + ETHER_VLAN_ENCAP_LEN,
			    mtod(m, char *), ETHER_HDR_LEN);
			evl = mtod(m, struct ether_vlan_header *);
			evl->evl_proto = evl->evl_encap_proto;
			evl->evl_encap_proto = htons(ETHERTYPE_VLAN);
			evl->evl_tag = htons(f->vlan);
		}
		NG_FWD_NEW_DATA(error, item, priv->downstream_hook, m);
	}
	return (error);
}

static int
ng_vlan_shutdown(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);
	free(priv, M_NETGRAPH);
	return (0);
}

static int
ng_vlan_disconnect(hook_p hook)
{
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct filter *f;

	if (hook == priv->downstream_hook)
		priv->downstream_hook = NULL;
	else if (hook == priv->nomatch_hook)
		priv->nomatch_hook = NULL;
	else {
		/* Purge a rule that refers to this hook. */
		if ((f = NG_HOOK_PRIVATE(hook)) != NULL) {
			LIST_REMOVE(f, next);
			priv->nent--;
			free(f, M_NETGRAPH);
		}
	}
	NG_HOOK_SET_PRIVATE(hook, NULL);
	if ((NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0) &&
	    (NG_NODE_IS_VALID(NG_HOOK_NODE(hook))))
		ng_rmnode_self(NG_HOOK_NODE(hook));
	return (0);
}
