
/*
 * ng_vjc.c
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
 * $Whistle: ng_vjc.c,v 1.17 1999/11/01 09:24:52 julian Exp $
 */

/*
 * This node performs Van Jacobsen IP header (de)compression.
 * You must have included net/slcompress.c in your kernel compilation.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_vjc.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <net/slcompress.h>

/* Check agreement with slcompress.c */
#if MAX_STATES != NG_VJC_MAX_CHANNELS
#error NG_VJC_MAX_CHANNELS must be the same as MAX_STATES
#endif

#define MAX_VJHEADER		16

/* Node private data */
struct private {
	struct	ngm_vjc_config conf;
	struct	slcompress slc;
	hook_p	ip;
	hook_p	vjcomp;
	hook_p	vjuncomp;
	hook_p	vjip;
};
typedef struct private *priv_p;

#define ERROUT(x)	do { error = (x); goto done; } while (0)

/* Netgraph node methods */
static ng_constructor_t	ng_vjc_constructor;
static ng_rcvmsg_t	ng_vjc_rcvmsg;
static ng_shutdown_t	ng_vjc_rmnode;
static ng_newhook_t	ng_vjc_newhook;
static ng_rcvdata_t	ng_vjc_rcvdata;
static ng_disconnect_t	ng_vjc_disconnect;

/* Helper stuff */
static struct mbuf *ng_vjc_pulluphdrs(struct mbuf *m);

/* Node type descriptor */
static struct ng_type typestruct = {
	NG_VERSION,
	NG_VJC_NODE_TYPE,
	NULL,
	ng_vjc_constructor,
	ng_vjc_rcvmsg,
	ng_vjc_rmnode,
	ng_vjc_newhook,
	NULL,
	NULL,
	ng_vjc_rcvdata,
	ng_vjc_rcvdata,
	ng_vjc_disconnect
};
NETGRAPH_INIT(vjc, &typestruct);

/************************************************************************
			NETGRAPH NODE METHODS
 ************************************************************************/

/*
 * Create a new node
 */
static int
ng_vjc_constructor(node_p *nodep)
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
 * Add a new hook
 */
static int
ng_vjc_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = (priv_p) node->private;
	hook_p *hookp;

	/* Get hook */
	if (strcmp(name, NG_VJC_HOOK_IP) == 0)
		hookp = &priv->ip;
	else if (strcmp(name, NG_VJC_HOOK_VJCOMP) == 0)
		hookp = &priv->vjcomp;
	else if (strcmp(name, NG_VJC_HOOK_VJUNCOMP) == 0)
		hookp = &priv->vjuncomp;
	else if (strcmp(name, NG_VJC_HOOK_VJIP) == 0)
		hookp = &priv->vjip;
	else
		return (EINVAL);

	/* See if already connected */
	if (*hookp)
		return (EISCONN);

	/* OK */
	*hookp = hook;
	return (0);
}

/*
 * Receive a control message
 */
static int
ng_vjc_rcvmsg(node_p node, struct ng_mesg *msg,
	      const char *raddr, struct ng_mesg **rptr)
{
	const priv_p priv = (priv_p) node->private;
	struct ng_mesg *resp = NULL;
	int error = 0;

	/* Check type cookie */
	switch (msg->header.typecookie) {
	case NGM_VJC_COOKIE:
		switch (msg->header.cmd) {
		case NGM_VJC_SET_CONFIG:
		    {
			struct ngm_vjc_config *const c =
				(struct ngm_vjc_config *) msg->data;

			if (msg->header.arglen != sizeof(*c))
				ERROUT(EINVAL);
			if ((priv->conf.enableComp || priv->conf.enableDecomp)
			    && (c->enableComp || c->enableDecomp))
				ERROUT(EALREADY);
			if (c->enableComp) {
				if (c->maxChannel > NG_VJC_MAX_CHANNELS - 1
				    || c->maxChannel < NG_VJC_MIN_CHANNELS - 1)
					ERROUT(EINVAL);
			} else {
				c->maxChannel = NG_VJC_MAX_CHANNELS;
			}
			if (c->enableComp != 0 || c->enableDecomp != 0) {
				bzero(&priv->slc, sizeof(priv->slc));
				sl_compress_init(&priv->slc, c->maxChannel);
			}
			priv->conf = *c;
			break;
		    }
		case NGM_VJC_GET_STATE:
			NG_MKRESPONSE(resp, msg, sizeof(priv->slc), M_NOWAIT);
			if (resp == NULL)
				ERROUT(ENOMEM);
			*((struct slcompress *) resp->data) = priv->slc;
			break;
		case NGM_VJC_CLR_STATS:
			priv->slc.sls_packets = 0;
			priv->slc.sls_compressed = 0;
			priv->slc.sls_searches = 0;
			priv->slc.sls_misses = 0;
			priv->slc.sls_uncompressedin = 0;
			priv->slc.sls_compressedin = 0;
			priv->slc.sls_errorin = 0;
			priv->slc.sls_tossed = 0;
			break;
		case NGM_VJC_RECV_ERROR:
			sl_uncompress_tcp(NULL, 0, TYPE_ERROR, &priv->slc);
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
 * Receive data
 */
static int
ng_vjc_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const node_p node = hook->node;
	const priv_p priv = (priv_p) node->private;
	int error = 0;

	if (hook == priv->ip) {			/* outgoing packet */
		u_int   type;

		if (!priv->conf.enableComp)	/* compression not enabled */
			type = TYPE_IP;
		else {
			struct ip *ip;

			if ((m = ng_vjc_pulluphdrs(m)) == NULL)
				ERROUT(ENOBUFS);
			ip = mtod(m, struct ip *);
			type = (ip->ip_p == IPPROTO_TCP) ?
			    sl_compress_tcp(m, ip,
				&priv->slc, priv->conf.compressCID) : TYPE_IP;
		}
		switch (type) {
		case TYPE_IP:
			hook = priv->vjip;
			break;
		case TYPE_UNCOMPRESSED_TCP:
			hook = priv->vjuncomp;
			break;
		case TYPE_COMPRESSED_TCP:
			hook = priv->vjcomp;
			break;
		default:
			panic("%s: type=%d", __FUNCTION__, type);
		}
	} else if (hook == priv->vjcomp) {	/* incoming compressed packet */
		int     vjlen;
		u_int   hlen;
		u_char *hdr;
		struct mbuf *mp;

		/* Are we decompressing? */
		if (!priv->conf.enableDecomp) {
			NG_FREE_DATA(m, meta);
			ERROUT(ENETDOWN);
		}

		/* Uncompress packet to reconstruct TCP/IP header */
		if (m->m_len < MAX_VJHEADER
		    && (m = m_pullup(m, MAX_VJHEADER)) == NULL) {
			priv->slc.sls_tossed++;
			NG_FREE_META(meta);
			ERROUT(ENOBUFS);
		}
		vjlen = sl_uncompress_tcp_core(mtod(m, u_char *),
		    m->m_len, m->m_pkthdr.len, TYPE_COMPRESSED_TCP,
		    &priv->slc, &hdr, &hlen);
		if (vjlen <= 0) {
			NG_FREE_DATA(m, meta);
			ERROUT(EINVAL);
		}
		m_adj(m, vjlen);

		/* Copy the reconstructed TCP/IP headers into a new mbuf */
		MGETHDR(mp, M_DONTWAIT, MT_DATA);
		if (mp == NULL)
			goto compfailmem;
		mp->m_len = 0;
		mp->m_next = NULL;
		if (hlen > MHLEN) {
			MCLGET(mp, M_DONTWAIT);
			if (M_TRAILINGSPACE(mp) < hlen) {
				m_freem(mp);	/* can't get a cluster, drop */
compfailmem:
				priv->slc.sls_tossed++;
				NG_FREE_DATA(m, meta);
				ERROUT(ENOBUFS);
			}
		}
		bcopy(hdr, mtod(mp, u_char *), hlen);
		mp->m_len = hlen;

		/* Stick header and rest of packet together */
		mp->m_next = m;
		mp->m_pkthdr.len = hlen + m->m_pkthdr.len;
		m = mp;
		hook = priv->ip;
	} else if (hook == priv->vjuncomp) {	/* incoming uncompressed pkt */
		u_char *hdr;
		u_int hlen;

		/* Are we decompressing? */
		if (!priv->conf.enableDecomp) {
			NG_FREE_DATA(m, meta);
			ERROUT(ENETDOWN);
		}

		/* Run packet through uncompressor */
		if ((m = ng_vjc_pulluphdrs(m)) == NULL)
			ERROUT(ENOBUFS);
		if (sl_uncompress_tcp_core(mtod(m, u_char *),
		    m->m_len, m->m_pkthdr.len, TYPE_UNCOMPRESSED_TCP,
		    &priv->slc, &hdr, &hlen) < 0) {
			NG_FREE_DATA(m, meta);
			ERROUT(EINVAL);
		}
		hook = priv->ip;
	} else if (hook == priv->vjip)	/* incoming regular packet (bypass) */
		hook = priv->ip;
	else
		panic("%s: unknown hook", __FUNCTION__);

done:
	if (m != NULL)
		NG_SEND_DATA(error, hook, m, meta);
	else
		NG_FREE_META(meta);
	return (error);
}

/*
 * Shutdown node
 */
static int
ng_vjc_rmnode(node_p node)
{
	const priv_p priv = (priv_p) node->private;

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
ng_vjc_disconnect(hook_p hook)
{
	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	return (0);
}

/************************************************************************
			HELPER STUFF
 ************************************************************************/

/*
 * Pull up the full IP and TCP headers of a packet. If packet is not
 * a TCP packet, just pull up the IP header.
 */
static struct mbuf *
ng_vjc_pulluphdrs(struct mbuf *m)
{
	struct ip *ip;
	struct tcphdr *tcp;
	int ihlen, thlen;

	if (m->m_len < sizeof(*ip) && (m = m_pullup(m, sizeof(*ip))) == NULL)
		return (NULL);
	ip = mtod(m, struct ip *);
	if (ip->ip_p != IPPROTO_TCP)
		return (m);
	ihlen = ip->ip_hl << 2;
	if (m->m_len < ihlen + sizeof(*tcp)) {
		if ((m = m_pullup(m, ihlen + sizeof(*tcp))) == NULL)
			return (NULL);
		ip = mtod(m, struct ip *);
	}
	tcp = (struct tcphdr *) ((u_char *) ip + ihlen);
	thlen = tcp->th_off << 2;
	if (m->m_len < ihlen + thlen)
		m = m_pullup(m, ihlen + thlen);
	return (m);
}

