
/*
 * ng_rfc1490.c
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
 * $Whistle: ng_rfc1490.c,v 1.22 1999/11/01 09:24:52 julian Exp $
 */

/*
 * This node does RFC 1490 multiplexing.
 *
 * NOTE: RFC 1490 is updated by RFC 2427.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_rfc1490.h>

/*
 * DEFINITIONS
 */

/* Q.922 stuff -- see RFC 1490 */
#define HDLC_UI		0x03

#define NLPID_IP	0xCC
#define NLPID_PPP	0xCF
#define NLPID_SNAP	0x80
#define NLPID_Q933	0x08
#define NLPID_CLNP	0x81
#define NLPID_ESIS	0x82
#define NLPID_ISIS	0x83

/* Node private data */
struct ng_rfc1490_private {
	hook_p  downlink;
	hook_p  ppp;
	hook_p  inet;
};
typedef struct ng_rfc1490_private *priv_p;

/* Netgraph node methods */
static ng_constructor_t	ng_rfc1490_constructor;
static ng_rcvmsg_t	ng_rfc1490_rcvmsg;
static ng_shutdown_t	ng_rfc1490_rmnode;
static ng_newhook_t	ng_rfc1490_newhook;
static ng_rcvdata_t	ng_rfc1490_rcvdata;
static ng_disconnect_t	ng_rfc1490_disconnect;

/* Node type descriptor */
static struct ng_type typestruct = {
	NG_VERSION,
	NG_RFC1490_NODE_TYPE,
	NULL,
	ng_rfc1490_constructor,
	ng_rfc1490_rcvmsg,
	ng_rfc1490_rmnode,
	ng_rfc1490_newhook,
	NULL,
	NULL,
	ng_rfc1490_rcvdata,
	ng_rfc1490_rcvdata,
	ng_rfc1490_disconnect,
	NULL
};
NETGRAPH_INIT(rfc1490, &typestruct);

/************************************************************************
			NETGRAPH NODE STUFF
 ************************************************************************/

/*
 * Node constructor
 */
static int
ng_rfc1490_constructor(node_p *nodep)
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
 * Give our ok for a hook to be added
 */
static int
ng_rfc1490_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = node->private;

	if (!strcmp(name, NG_RFC1490_HOOK_DOWNSTREAM)) {
		if (priv->downlink)
			return (EISCONN);
		priv->downlink = hook;
	} else if (!strcmp(name, NG_RFC1490_HOOK_PPP)) {
		if (priv->ppp)
			return (EISCONN);
		priv->ppp = hook;
	} else if (!strcmp(name, NG_RFC1490_HOOK_INET)) {
		if (priv->inet)
			return (EISCONN);
		priv->inet = hook;
	} else
		return (EINVAL);
	return (0);
}

/*
 * Receive a control message. We don't support any special ones.
 */
static int
ng_rfc1490_rcvmsg(node_p node, struct ng_mesg *msg,
		  const char *raddr, struct ng_mesg **rp)
{
	FREE(msg, M_NETGRAPH);
	return (EINVAL);
}

/*
 * Receive data on a hook and encapsulate according to RFC 1490.
 * Only those nodes marked (*) are supported by this routine so far.
 *
 *                            Q.922 control
 *                                 |
 *                                 |
 *            --------------------------------------------
 *            | 0x03                                     |
 *           UI                                       I Frame
 *            |                                          |
 *      ---------------------------------         --------------
 *      | 0x08  | 0x81  |0xCC   |0xCF   | 0x00    |..01....    |..10....
 *      |       |       |       |       | 0x80    |            |
 *     Q.933   CLNP    IP(*)   PPP(*)  SNAP     ISO 8208    ISO 8208
 *      |                    (rfc1973)  |       Modulo 8    Modulo 128
 *      |                               |
 *      --------------------           OUI
 *      |                  |            |
 *     L2 ID              L3 ID      -------------------------
 *      |               User         |00-80-C2               |00-00-00
 *      |               specified    |                       |
 *      |               0x70        PID                     Ethertype
 *      |                            |                       |
 *      -------------------        --------------...        ----------
 *      |0x51 |0x4E |     |0x4C    |0x1   |0xB  |           |0x806   |
 *      |     |     |     |        |      |     |           |        |
 *     7776  Q.922 Others 802.2   802.3  802.6 Others       ARP(*)  Others
 *
 *
 */

#define MAX_ENCAPS_HDR	8
#define ERROUT(x)	do { error = (x); goto done; } while (0)
#define OUICMP(P,A,B,C)	((P)[0]==(A) && (P)[1]==(B) && (P)[2]==(C))

static int
ng_rfc1490_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
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

		/* Eat optional zero pad byte */
		if (*ptr == 0x00)
			ptr++;

		/* Multiplex on NLPID */
		switch (*ptr++) {
		case NLPID_SNAP:
			if (OUICMP(ptr, 0, 0, 0)) {	/* It's an ethertype */
				u_int16_t etype;

				ptr += 3;
				etype = ntohs(*((u_int16_t *) ptr));
				ptr += 2;
				m_adj(m, ptr - start);
				switch (etype) {
				case ETHERTYPE_IP:
					NG_SEND_DATA(error,
					    priv->inet, m, meta);
					break;
				case ETHERTYPE_ARP:
				case ETHERTYPE_REVARP:
				default:
					ERROUT(0);
				}
			} else if (OUICMP(ptr, 0x00, 0x80, 0xc2))	/* 802.1 bridging */
				ERROUT(0);
			else	/* Other weird stuff... */
				ERROUT(0);
			break;
		case NLPID_IP:
			m_adj(m, ptr - start);
			NG_SEND_DATA(error, priv->inet, m, meta);
			break;
		case NLPID_PPP:
			m_adj(m, ptr - start);
			NG_SEND_DATA(error, priv->ppp, m, meta);
			break;
		case NLPID_Q933:
		case NLPID_CLNP:
		case NLPID_ESIS:
		case NLPID_ISIS:
			ERROUT(0);
		default:	/* Try PPP (see RFC 1973) */
			ptr--;	/* NLPID becomes PPP proto */
			if ((*ptr & 0x01) == 0x01)
				ERROUT(0);
			m_adj(m, ptr - start);
			NG_SEND_DATA(error, priv->ppp, m, meta);
			break;
		}
	} else if (hook == priv->ppp) {
		M_PREPEND(m, 2, M_DONTWAIT);	/* Prepend PPP NLPID */
		if (!m)
			ERROUT(ENOBUFS);
		mtod(m, u_char *)[0] = HDLC_UI;
		mtod(m, u_char *)[1] = NLPID_PPP;
		NG_SEND_DATA(error, priv->downlink, m, meta);
	} else if (hook == priv->inet) {
		M_PREPEND(m, 2, M_DONTWAIT);	/* Prepend IP NLPID */
		if (!m)
			ERROUT(ENOBUFS);
		mtod(m, u_char *)[0] = HDLC_UI;
		mtod(m, u_char *)[1] = NLPID_IP;
		NG_SEND_DATA(error, priv->downlink, m, meta);
	} else
		panic(__FUNCTION__);

done:
	NG_FREE_DATA(m, meta);
	return (error);
}

/*
 * Nuke node
 */
static int
ng_rfc1490_rmnode(node_p node)
{
	const priv_p priv = node->private;

	/* Take down netgraph node */
	node->flags |= NG_INVALID;
	ng_cutlinks(node);
	ng_unname(node);
	bzero(priv, sizeof(*priv));
	node->private = NULL;
	ng_unref(node);		/* let the node escape */
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_rfc1490_disconnect(hook_p hook)
{
	const priv_p priv = hook->node->private;

	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	else if (hook == priv->downlink)
		priv->downlink = NULL;
	else if (hook == priv->inet)
		priv->inet = NULL;
	else if (hook == priv->ppp)
		priv->ppp = NULL;
	else
		panic(__FUNCTION__);
	return (0);
}

