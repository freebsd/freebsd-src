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
 * $FreeBSD$
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
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

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

static struct mbuf * m_megapullup(struct mbuf *, int);

/* List of commands and how to convert arguments to/from ASCII. */
static const struct ng_cmdlist ng_nat_cmdlist[] = {
	{
	  NGM_NAT_COOKIE,
	  NGM_NAT_SET_IPADDR,
	  "setaliasaddr",
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
struct ng_priv_priv {
	node_p		node;		/* back pointer to node */
	hook_p		in;		/* hook for demasquerading */
	hook_p		out;		/* hook for masquerading */
	struct libalias	*lib;		/* libalias handler */
	uint32_t	flags;		/* status flags */
};
typedef struct ng_priv_priv *priv_p;

/* Values of flags */
#define	NGNAT_READY		0x1	/* We have everything to work */
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
	    priv->in != NULL &&
	    priv->flags & NGNAT_ADDR_DEFINED)
		priv->flags |= NGNAT_READY;

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
			if (priv->out != NULL &&
			    priv->in != NULL)
				priv->flags |= NGNAT_READY;
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

	if (!(priv->flags & NGNAT_READY)) {
		NG_FREE_ITEM(item);
		return (ENXIO);
	}

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
		if (rval != PKT_ALIAS_OK) {
			printf("in %u\n", rval);
			NG_FREE_ITEM(item);
			return (EINVAL);
		}
		m->m_pkthdr.len = m->m_len = ntohs(ip->ip_len);
		NG_FWD_ITEM_HOOK(error, item, priv->out);
	} else if (hook == priv->out) {
		rval = LibAliasOut(priv->lib, c, MCLBYTES);
		if (rval != PKT_ALIAS_OK) {
			printf("out %u\n", rval);
			NG_FREE_ITEM(item);
			return (EINVAL);
		}
		m->m_pkthdr.len = m->m_len = ntohs(ip->ip_len);
		NG_FWD_ITEM_HOOK(error, item, priv->in);
	} else
		panic("ng_nat: unknown hook!\n");

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

	priv->flags &= ~NGNAT_READY;

	if (hook == priv->out)
		priv->out = NULL;
	if (hook == priv->in)
		priv->in = NULL;

	if (priv->out == NULL && priv->in == NULL)
		ng_rmnode_self(NG_HOOK_NODE(hook));

	return (0);
}

/*
 * m_megapullup() function is a big hack.
 *
 * It allocates an mbuf with cluster and copies the whole
 * chain into cluster, so that it is all contigous and the
 * whole packet can be accessed via char pointer.
 *
 * This is required, because libalias doesn't have idea
 * about mbufs.
 */
static struct mbuf *
m_megapullup(struct mbuf *m, int len)
{
	struct mbuf *mcl;
	caddr_t cp;

	if (len > MCLBYTES)
		goto bad;

	if ((mcl = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR)) == NULL)
		goto bad;

	cp = mtod(mcl, caddr_t);
	m_copydata(m, 0, len, cp);
	m_move_pkthdr(mcl, m);
	mcl->m_len = mcl->m_pkthdr.len;
	m_freem(m);

	return (mcl);
bad:
	m_freem(m);
	return (NULL);
}
