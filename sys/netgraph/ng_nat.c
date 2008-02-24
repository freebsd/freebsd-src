/*-
 * Copyright 2005, Gleb Smirnoff <glebius@FreeBSD.org>
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
 * $FreeBSD: src/sys/netgraph/ng_nat.c,v 1.10 2007/05/22 12:20:05 mav Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/ctype.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <machine/in_cksum.h>

#include <netinet/libalias/alias.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_nat.h>
#include <netgraph/netgraph.h>

static ng_constructor_t	ng_nat_constructor;
static ng_rcvmsg_t	ng_nat_rcvmsg;
static ng_shutdown_t	ng_nat_shutdown;
static ng_newhook_t	ng_nat_newhook;
static ng_rcvdata_t	ng_nat_rcvdata;
static ng_disconnect_t	ng_nat_disconnect;

static unsigned int	ng_nat_translate_flags(unsigned int x);

/* Parse type for struct ng_nat_mode. */
static const struct ng_parse_struct_field ng_nat_mode_fields[]
	= NG_NAT_MODE_INFO;
static const struct ng_parse_type ng_nat_mode_type = {
	&ng_parse_struct_type,
	ng_nat_mode_fields
};

/* List of commands and how to convert arguments to/from ASCII. */
static const struct ng_cmdlist ng_nat_cmdlist[] = {
	{
	  NGM_NAT_COOKIE,
	  NGM_NAT_SET_IPADDR,
	  "setaliasaddr",
	  &ng_parse_ipaddr_type,
	  NULL
	},
	{
	  NGM_NAT_COOKIE,
	  NGM_NAT_SET_MODE,
	  "setmode",
	  &ng_nat_mode_type,
	  NULL
	},
	{
	  NGM_NAT_COOKIE,
	  NGM_NAT_SET_TARGET,
	  "settarget",
	  &ng_parse_ipaddr_type,
	  NULL
	},
	{ 0 }
};

/* Netgraph node type descriptor. */
static struct ng_type typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_NAT_NODE_TYPE,
	.constructor =	ng_nat_constructor,
	.rcvmsg =	ng_nat_rcvmsg,
	.shutdown =	ng_nat_shutdown,
	.newhook =	ng_nat_newhook,
	.rcvdata =	ng_nat_rcvdata,
	.disconnect =	ng_nat_disconnect,
	.cmdlist =	ng_nat_cmdlist,
};
NETGRAPH_INIT(nat, &typestruct);
MODULE_DEPEND(ng_nat, libalias, 1, 1, 1);

/* Information we store for each node. */
struct ng_nat_priv {
	node_p		node;		/* back pointer to node */
	hook_p		in;		/* hook for demasquerading */
	hook_p		out;		/* hook for masquerading */
	struct libalias	*lib;		/* libalias handler */
	uint32_t	flags;		/* status flags */
};
typedef struct ng_nat_priv *priv_p;

/* Values of flags */
#define	NGNAT_CONNECTED		0x1	/* We have both hooks connected */
#define	NGNAT_ADDR_DEFINED	0x2	/* NGM_NAT_SET_IPADDR happened */

static int
ng_nat_constructor(node_p node)
{
	priv_p priv;

	/* Initialize private descriptor. */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH,
		M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return (ENOMEM);

	/* Init aliasing engine. */
	priv->lib = LibAliasInit(NULL);
	if (priv->lib == NULL) {
		FREE(priv, M_NETGRAPH);
		return (ENOMEM);
	}

	/* Set same ports on. */
	(void )LibAliasSetMode(priv->lib, PKT_ALIAS_SAME_PORTS,
	    PKT_ALIAS_SAME_PORTS);

	/* Link structs together. */
	NG_NODE_SET_PRIVATE(node, priv);
	priv->node = node;

	/*
	 * libalias is not thread safe, so our node
	 * must be single threaded.
	 */
	NG_NODE_FORCE_WRITER(node);

	return (0);
}

static int
ng_nat_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	if (strcmp(name, NG_NAT_HOOK_IN) == 0) {
		priv->in = hook;
	} else if (strcmp(name, NG_NAT_HOOK_OUT) == 0) {
		priv->out = hook;
	} else
		return (EINVAL);

	if (priv->out != NULL &&
	    priv->in != NULL)
		priv->flags |= NGNAT_CONNECTED;

	return(0);
}

static int
ng_nat_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ng_mesg *resp = NULL;
	struct ng_mesg *msg;
	int error = 0;

	NGI_GET_MSG(item, msg);

	switch (msg->header.typecookie) {
	case NGM_NAT_COOKIE:
		switch (msg->header.cmd) {
		case NGM_NAT_SET_IPADDR:
		    {
			struct in_addr *const ia = (struct in_addr *)msg->data;

			if (msg->header.arglen < sizeof(*ia)) {
				error = EINVAL;
				break;
			}

			LibAliasSetAddress(priv->lib, *ia);

			priv->flags |= NGNAT_ADDR_DEFINED;
		    }
			break;
		case NGM_NAT_SET_MODE:
		    {
			struct ng_nat_mode *const mode = 
			    (struct ng_nat_mode *)msg->data;

			if (msg->header.arglen < sizeof(*mode)) {
				error = EINVAL;
				break;
			}
			
			if (LibAliasSetMode(priv->lib, 
			    ng_nat_translate_flags(mode->flags),
			    ng_nat_translate_flags(mode->mask)) < 0) {
				error = ENOMEM;
				break;
			}
		    }
			break;
		case NGM_NAT_SET_TARGET:
		    {
			struct in_addr *const ia = (struct in_addr *)msg->data;

			if (msg->header.arglen < sizeof(*ia)) {
				error = EINVAL;
				break;
			}

			LibAliasSetTarget(priv->lib, *ia);
		    }
			break;
		default:
			error = EINVAL;		/* unknown command */
			break;
		}
		break;
	default:
		error = EINVAL;			/* unknown cookie type */
		break;
	}

	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

static int
ng_nat_rcvdata(hook_p hook, item_p item )
{
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct mbuf	*m;
	struct ip	*ip;
	int rval, error = 0;
	char *c;

	/* We have no required hooks. */
	if (!(priv->flags & NGNAT_CONNECTED)) {
		NG_FREE_ITEM(item);
		return (ENXIO);
	}

	/* We have no alias address yet to do anything. */
	if (!(priv->flags & NGNAT_ADDR_DEFINED))
		goto send;

	m = NGI_M(item);

	if ((m = m_megapullup(m, m->m_pkthdr.len)) == NULL) {
		NGI_M(item) = NULL;	/* avoid double free */
		NG_FREE_ITEM(item);
		return (ENOBUFS);
	}

	NGI_M(item) = m;

	c = mtod(m, char *);
	ip = mtod(m, struct ip *);

	KASSERT(m->m_pkthdr.len == ntohs(ip->ip_len),
	    ("ng_nat: ip_len != m_pkthdr.len"));

	if (hook == priv->in) {
		rval = LibAliasIn(priv->lib, c, MCLBYTES);
		if (rval != PKT_ALIAS_OK &&
		    rval != PKT_ALIAS_FOUND_HEADER_FRAGMENT) {
			NG_FREE_ITEM(item);
			return (EINVAL);
		}
	} else if (hook == priv->out) {
		rval = LibAliasOut(priv->lib, c, MCLBYTES);
		if (rval != PKT_ALIAS_OK) {
			NG_FREE_ITEM(item);
			return (EINVAL);
		}
	} else
		panic("ng_nat: unknown hook!\n");

	m->m_pkthdr.len = m->m_len = ntohs(ip->ip_len);

	if ((ip->ip_off & htons(IP_OFFMASK)) == 0 &&
	    ip->ip_p == IPPROTO_TCP) {
		struct tcphdr *th = (struct tcphdr *)((caddr_t)ip +
		    (ip->ip_hl << 2));

		/*
		 * Here is our terrible HACK.
		 *
		 * Sometimes LibAlias edits contents of TCP packet.
		 * In this case it needs to recompute full TCP
		 * checksum. However, the problem is that LibAlias
		 * doesn't have any idea about checksum offloading
		 * in kernel. To workaround this, we do not do
		 * checksumming in LibAlias, but only mark the
		 * packets in th_x2 field. If we receive a marked
		 * packet, we calculate correct checksum for it
		 * aware of offloading.
		 *
		 * Why do I do such a terrible hack instead of
		 * recalculating checksum for each packet?
		 * Because the previous checksum was not checked!
		 * Recalculating checksums for EVERY packet will
		 * hide ALL transmission errors. Yes, marked packets
		 * still suffer from this problem. But, sigh, natd(8)
		 * has this problem, too.
		 */

		if (th->th_x2) {
			th->th_x2 = 0;
			ip->ip_len = ntohs(ip->ip_len);
			th->th_sum = in_pseudo(ip->ip_src.s_addr,
			    ip->ip_dst.s_addr, htons(IPPROTO_TCP +
			    ip->ip_len - (ip->ip_hl << 2)));
	
			if ((m->m_pkthdr.csum_flags & CSUM_TCP) == 0) {
				m->m_pkthdr.csum_data = offsetof(struct tcphdr,
				    th_sum);
				in_delayed_cksum(m);
			}
			ip->ip_len = htons(ip->ip_len);
		}
	}

send:
	if (hook == priv->in)
		NG_FWD_ITEM_HOOK(error, item, priv->out);
	else
		NG_FWD_ITEM_HOOK(error, item, priv->in);

	return (error);
}

static int
ng_nat_shutdown(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);
	LibAliasUninit(priv->lib);
	FREE(priv, M_NETGRAPH);

	return (0);
}

static int
ng_nat_disconnect(hook_p hook)
{
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));

	priv->flags &= ~NGNAT_CONNECTED;

	if (hook == priv->out)
		priv->out = NULL;
	if (hook == priv->in)
		priv->in = NULL;

	if (priv->out == NULL && priv->in == NULL)
		ng_rmnode_self(NG_HOOK_NODE(hook));

	return (0);
}

static unsigned int
ng_nat_translate_flags(unsigned int x)
{
	unsigned int	res = 0;
	
	if (x & NG_NAT_LOG)
		res |= PKT_ALIAS_LOG;
	if (x & NG_NAT_DENY_INCOMING)
		res |= PKT_ALIAS_DENY_INCOMING;
	if (x & NG_NAT_SAME_PORTS)
		res |= PKT_ALIAS_SAME_PORTS;
	if (x & NG_NAT_UNREGISTERED_ONLY)
		res |= PKT_ALIAS_UNREGISTERED_ONLY;
	if (x & NG_NAT_RESET_ON_ADDR_CHANGE)
		res |= PKT_ALIAS_RESET_ON_ADDR_CHANGE;
	if (x & NG_NAT_PROXY_ONLY)
		res |= PKT_ALIAS_PROXY_ONLY;
	if (x & NG_NAT_REVERSE)
		res |= PKT_ALIAS_REVERSE;

	return (res);
}
