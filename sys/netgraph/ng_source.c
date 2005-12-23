/*
 * ng_source.c
 */

/*-
 * Copyright (c) 2005 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright 2002 Sandvine Inc.
 * All rights reserved.
 *
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Sandvine Inc.; provided,
 * however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Sandvine Inc.
 *    trademarks, including the mark "SANDVINE" on advertising, endorsements,
 *    or otherwise except as such appears in the above copyright notice or in
 *    the software.
 *
 * THIS SOFTWARE IS BEING PROVIDED BY SANDVINE "AS IS", AND TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, SANDVINE MAKES NO REPRESENTATIONS OR WARRANTIES,
 * EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE, INCLUDING WITHOUT LIMITATION,
 * ANY AND ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT.  SANDVINE DOES NOT WARRANT, GUARANTEE, OR
 * MAKE ANY REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE
 * USE OF THIS SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY
 * OR OTHERWISE.  IN NO EVENT SHALL SANDVINE BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF SANDVINE IS ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Dave Chapeskie <dchapeskie@sandvine.com>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This node is used for high speed packet geneneration.  It queues
 * all data recieved on its 'input' hook and when told to start via
 * a control message it sends the packets out its 'output' hook.  In
 * this way this node can be preloaded with a packet stream which it
 * can then send continuously as fast as possible.
 *
 * Currently it just copies the mbufs as required.  It could do various
 * tricks to try and avoid this.  Probably the best performance would
 * be achieved by modifying the appropriate drivers to be told to
 * self-re-enqueue packets (e.g. the if_bge driver could reuse the same
 * transmit descriptors) under control of this node; perhaps via some
 * flag in the mbuf or some such.  The node could peek at an appropriate
 * ifnet flag to see if such support is available for the connected
 * interface.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <net/if_var.h>
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_ether.h>
#include <netgraph/ng_source.h>

#define NG_SOURCE_INTR_TICKS		1
#define NG_SOURCE_DRIVER_IFQ_MAXLEN	(4*1024)

/* Per node info */
struct privdata {
	node_p				node;
	hook_p				input;
	hook_p				output;
	struct ng_source_stats		stats;
	struct ifqueue			snd_queue;	/* packets to send */
	struct ifnet			*output_ifp;
	struct callout			intr_ch;
	uint64_t			packets;	/* packets to send */
	uint32_t			queueOctets;
};
typedef struct privdata *sc_p;

/* Node flags */
#define NG_SOURCE_ACTIVE	(NGF_TYPE1)

/* Netgraph methods */
static ng_constructor_t	ng_source_constructor;
static ng_rcvmsg_t	ng_source_rcvmsg;
static ng_shutdown_t	ng_source_rmnode;
static ng_newhook_t	ng_source_newhook;
static ng_connect_t	ng_source_connect;
static ng_rcvdata_t	ng_source_rcvdata;
static ng_disconnect_t	ng_source_disconnect;

/* Other functions */
static void		ng_source_intr(node_p, hook_p, void *, int);
static void		ng_source_clr_data (sc_p);
static int		ng_source_start (sc_p, uint64_t);
static void		ng_source_stop (sc_p);
static int		ng_source_send (sc_p, int, int *);
static int		ng_source_store_output_ifp(sc_p, char *);

/* Parse type for timeval */
static const struct ng_parse_struct_field ng_source_timeval_type_fields[] = {
	{ "tv_sec",		&ng_parse_int32_type	},
	{ "tv_usec",		&ng_parse_int32_type	},
	{ NULL }
};
const struct ng_parse_type ng_source_timeval_type = {
	&ng_parse_struct_type,
	&ng_source_timeval_type_fields
};

/* Parse type for struct ng_source_stats */
static const struct ng_parse_struct_field ng_source_stats_type_fields[]
	= NG_SOURCE_STATS_TYPE_INFO;
static const struct ng_parse_type ng_source_stats_type = {
	&ng_parse_struct_type,
	&ng_source_stats_type_fields
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_source_cmds[] = {
	{
	  NGM_SOURCE_COOKIE,
	  NGM_SOURCE_GET_STATS,
	  "getstats",
	  NULL,
	  &ng_source_stats_type
	},
	{
	  NGM_SOURCE_COOKIE,
	  NGM_SOURCE_CLR_STATS,
	  "clrstats",
	  NULL,
	  NULL
	},
	{
	  NGM_SOURCE_COOKIE,
	  NGM_SOURCE_GETCLR_STATS,
	  "getclrstats",
	  NULL,
	  &ng_source_stats_type
	},
	{
	  NGM_SOURCE_COOKIE,
	  NGM_SOURCE_START,
	  "start",
	  &ng_parse_uint64_type,
	  NULL
	},
	{
	  NGM_SOURCE_COOKIE,
	  NGM_SOURCE_STOP,
	  "stop",
	  NULL,
	  NULL
	},
	{
	  NGM_SOURCE_COOKIE,
	  NGM_SOURCE_CLR_DATA,
	  "clrdata",
	  NULL,
	  NULL
	},
	{
	  NGM_SOURCE_COOKIE,
	  NGM_SOURCE_SETIFACE,
	  "setiface",
	  &ng_parse_string_type,
	  NULL
	},
	{
	  NGM_SOURCE_COOKIE,
	  NGM_SOURCE_SETPPS,
	  "setpps",
	  &ng_parse_uint32_type,
	  NULL
	},
	{ 0 }
};

/* Netgraph type descriptor */
static struct ng_type ng_source_typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_SOURCE_NODE_TYPE,
	.constructor =	ng_source_constructor,
	.rcvmsg =	ng_source_rcvmsg,
	.shutdown =	ng_source_rmnode,
	.newhook =	ng_source_newhook,
	.connect =	ng_source_connect,
	.rcvdata =	ng_source_rcvdata,
	.disconnect =	ng_source_disconnect,
	.cmdlist =	ng_source_cmds,
};
NETGRAPH_INIT(source, &ng_source_typestruct);

static int ng_source_set_autosrc(sc_p, uint32_t);

/*
 * Node constructor
 */
static int
ng_source_constructor(node_p node)
{
	sc_p sc;

	sc = malloc(sizeof(*sc), M_NETGRAPH, M_NOWAIT | M_ZERO);
	if (sc == NULL)
		return (ENOMEM);

	NG_NODE_SET_PRIVATE(node, sc);
	sc->node = node;
	sc->snd_queue.ifq_maxlen = 2048;	/* XXX not checked */
	ng_callout_init(&sc->intr_ch);

	return (0);
}

/*
 * Add a hook
 */
static int
ng_source_newhook(node_p node, hook_p hook, const char *name)
{
	sc_p sc = NG_NODE_PRIVATE(node);

	if (strcmp(name, NG_SOURCE_HOOK_INPUT) == 0) {
		sc->input = hook;
	} else if (strcmp(name, NG_SOURCE_HOOK_OUTPUT) == 0) {
		sc->output = hook;
		sc->output_ifp = 0;
		bzero(&sc->stats, sizeof(sc->stats));
	} else
		return (EINVAL);

	return (0);
}

/*
 * Hook has been added
 */
static int
ng_source_connect(hook_p hook)
{
	sc_p sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct ng_mesg *msg;
	int dummy_error = 0;

	/*
	 * If this is "output" hook, then request information
	 * from our downstream.
	 */
	if (hook == sc->output) {
		NG_MKMESSAGE(msg, NGM_ETHER_COOKIE, NGM_ETHER_GET_IFNAME,
		    0, M_NOWAIT);
		if (msg == NULL)
			return (ENOBUFS);

		/*
		 * Our hook and peer hook have HK_INVALID flag set,
		 * so we can't use NG_SEND_MSG_HOOK() macro here.
		 */
		NG_SEND_MSG_ID(dummy_error, sc->node, msg,
		    NG_NODE_ID(NG_PEER_NODE(sc->output)), NG_NODE_ID(sc->node));
	}

	return (0);
}

/*
 * Receive a control message
 */
static int
ng_source_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	sc_p sc = NG_NODE_PRIVATE(node);
	struct ng_mesg *msg, *resp = NULL;
	int error = 0;

	NGI_GET_MSG(item, msg);

	switch (msg->header.typecookie) {
	case NGM_SOURCE_COOKIE:
		if (msg->header.flags & NGF_RESP) {
			error = EINVAL;
			break;
		}
		switch (msg->header.cmd) {
		case NGM_SOURCE_GET_STATS:
		case NGM_SOURCE_CLR_STATS:
		case NGM_SOURCE_GETCLR_STATS:
                    {
			struct ng_source_stats *stats;

                        if (msg->header.cmd != NGM_SOURCE_CLR_STATS) {
                                NG_MKRESPONSE(resp, msg,
                                    sizeof(*stats), M_NOWAIT);
				if (resp == NULL) {
					error = ENOMEM;
					goto done;
				}
				sc->stats.queueOctets = sc->queueOctets;
				sc->stats.queueFrames = sc->snd_queue.ifq_len;
				if ((sc->node->nd_flags & NG_SOURCE_ACTIVE)
				    && !timevalisset(&sc->stats.endTime)) {
					getmicrotime(&sc->stats.elapsedTime);
					timevalsub(&sc->stats.elapsedTime,
					    &sc->stats.startTime);
				}
				stats = (struct ng_source_stats *)resp->data;
				bcopy(&sc->stats, stats, sizeof(* stats));
                        }
                        if (msg->header.cmd != NGM_SOURCE_GET_STATS)
				bzero(&sc->stats, sizeof(sc->stats));
		    }
		    break;
		case NGM_SOURCE_START:
		    {
			uint64_t packets;

			if (msg->header.arglen != sizeof(uint64_t)) {
				error = EINVAL;
				break;
			}

			packets = *(uint64_t *)msg->data;

			error = ng_source_start(sc, packets);

		    	break;
		    }
		case NGM_SOURCE_STOP:
			ng_source_stop(sc);
			break;
		case NGM_SOURCE_CLR_DATA:
			ng_source_clr_data(sc);
			break;
		case NGM_SOURCE_SETIFACE:
		    {
			char *ifname = (char *)msg->data;

			if (msg->header.arglen < 2) {
				error = EINVAL;
				break;
			}

			ng_source_store_output_ifp(sc, ifname);
			break;
		    }
		case NGM_SOURCE_SETPPS:
		    {
			uint32_t pps;

			if (msg->header.arglen != sizeof(uint32_t)) {
				error = EINVAL;
				break;
			}

			pps = *(uint32_t *)msg->data;

			sc->stats.maxPps = pps;

			break;
		    }
		default:
			error = EINVAL;
			break;
		}
		break;
	case NGM_ETHER_COOKIE:
		if (!(msg->header.flags & NGF_RESP)) {
			error = EINVAL;
			break;
		}
		switch (msg->header.cmd) {
		case NGM_ETHER_GET_IFNAME:
		    {
			char *ifname = (char *)msg->data;

			if (msg->header.arglen < 2) {
				error = EINVAL;
				break;
			}

			if (ng_source_store_output_ifp(sc, ifname) == 0)
				ng_source_set_autosrc(sc, 0);
			break;
		    }
		default:
			error = EINVAL;
		}
		break;
	default:
		error = EINVAL;
		break;
	}

done:
	/* Take care of synchronous response, if any. */
	NG_RESPOND_MSG(error, node, item, resp);
	/* Free the message and return. */
	NG_FREE_MSG(msg);
	return (error);
}

/*
 * Receive data on a hook
 *
 * If data comes in the input hook, enqueue it on the send queue.
 * If data comes in the output hook, discard it.
 */
static int
ng_source_rcvdata(hook_p hook, item_p item)
{
	sc_p sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct mbuf *m;
	int error = 0;

	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);

	/* Which hook? */
	if (hook == sc->output) {
		/* discard */
		NG_FREE_M(m);
		return (error);
	}
	KASSERT(hook == sc->input, ("%s: no hook!", __func__));

	/* Enqueue packet. */
	/* XXX should we check IF_QFULL() ? */
	_IF_ENQUEUE(&sc->snd_queue, m);
	sc->queueOctets += m->m_pkthdr.len;

	return (0);
}

/*
 * Shutdown processing
 */
static int
ng_source_rmnode(node_p node)
{
	sc_p sc = NG_NODE_PRIVATE(node);

	ng_source_stop(sc);
	ng_source_clr_data(sc);
	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);
	free(sc, M_NETGRAPH);

	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_source_disconnect(hook_p hook)
{
	sc_p sc;

	sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	KASSERT(sc != NULL, ("%s: null node private", __func__));
	if (NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0 || hook == sc->output)
		ng_rmnode_self(NG_HOOK_NODE(hook));
	return (0);
}

/*
 * Set sc->output_ifp to point to the the struct ifnet of the interface
 * reached via our output hook.
 */
static int
ng_source_store_output_ifp(sc_p sc, char *ifname)
{
	struct ifnet *ifp;
	int s;

	ifp = ifunit(ifname);

	if (ifp == NULL) {
		printf("%s: can't find interface %d\n", __func__, if_index);
		return (EINVAL);
	}
	sc->output_ifp = ifp;

#if 1
	/* XXX mucking with a drivers ifqueue size is ugly but we need it
	 * to queue a lot of packets to get close to line rate on a gigabit
	 * interface with small packets.
	 * XXX we should restore the original value at stop or disconnect
	 */
	s = splimp();		/* XXX is this required? */
	if (ifp->if_snd.ifq_maxlen < NG_SOURCE_DRIVER_IFQ_MAXLEN) {
		printf("ng_source: changing ifq_maxlen from %d to %d\n",
		    ifp->if_snd.ifq_maxlen, NG_SOURCE_DRIVER_IFQ_MAXLEN);
		ifp->if_snd.ifq_maxlen = NG_SOURCE_DRIVER_IFQ_MAXLEN;
	}
	splx(s);
#endif
	return (0);
}

/*
 * Set the attached ethernet node's ethernet source address override flag.
 */
static int
ng_source_set_autosrc(sc_p sc, uint32_t flag)
{
	struct ng_mesg *msg;
	int error = 0;

	NG_MKMESSAGE(msg, NGM_ETHER_COOKIE, NGM_ETHER_SET_AUTOSRC,
	    sizeof (uint32_t), M_NOWAIT);
	if (msg == NULL)
		return(ENOBUFS);

	*(uint32_t *)msg->data = flag;
	NG_SEND_MSG_HOOK(error, sc->node, msg, sc->output, 0);
	return (error);
}

/*
 * Clear out the data we've queued
 */
static void
ng_source_clr_data (sc_p sc)
{
	struct mbuf *m;

	for (;;) {
		_IF_DEQUEUE(&sc->snd_queue, m);
		if (m == NULL)
			break;
		NG_FREE_M(m);
	}
	sc->queueOctets = 0;
}

/*
 * Start sending queued data out the output hook
 */
static int
ng_source_start(sc_p sc, uint64_t packets)
{
	if (sc->output_ifp == NULL) {
		printf("ng_source: start without iface configured\n");
		return (ENXIO);
	}

	if (sc->node->nd_flags & NG_SOURCE_ACTIVE)
		return (EBUSY);

	sc->node->nd_flags |= NG_SOURCE_ACTIVE;

	sc->packets = packets;
	timevalclear(&sc->stats.elapsedTime);
	timevalclear(&sc->stats.endTime);
	getmicrotime(&sc->stats.startTime);
	getmicrotime(&sc->stats.lastTime);
	ng_callout(&sc->intr_ch, sc->node, NULL, 0,
	    ng_source_intr, sc, 0);

	return (0);
}

/*
 * Stop sending queued data out the output hook
 */
static void
ng_source_stop(sc_p sc)
{
	ng_uncallout(&sc->intr_ch, sc->node);
	sc->node->nd_flags &= ~NG_SOURCE_ACTIVE;
	getmicrotime(&sc->stats.endTime);
	sc->stats.elapsedTime = sc->stats.endTime;
	timevalsub(&sc->stats.elapsedTime, &sc->stats.startTime);
}

/*
 * While active called every NG_SOURCE_INTR_TICKS ticks.
 * Sends as many packets as the interface connected to our
 * output hook is able to enqueue.
 */
static void
ng_source_intr(node_p node, hook_p hook, void *arg1, int arg2)
{
	sc_p sc = (sc_p)arg1;
	struct ifqueue *ifq;
	int packets;

	KASSERT(sc != NULL, ("%s: null node private", __func__));

	if (sc->packets == 0 || sc->output == NULL
	    || (sc->node->nd_flags & NG_SOURCE_ACTIVE) == 0) {
		ng_source_stop(sc);
		return;
	}

	if (sc->output_ifp != NULL) {
		ifq = (struct ifqueue *)&sc->output_ifp->if_snd;
		packets = ifq->ifq_maxlen - ifq->ifq_len;
	} else
		packets = sc->snd_queue.ifq_len;

	if (sc->stats.maxPps != 0) {
		struct timeval	now, elapsed;
		uint64_t	usec;
		int		maxpkt;

		getmicrotime(&now);
		elapsed = now;
		timevalsub(&elapsed, &sc->stats.lastTime);
		usec = elapsed.tv_sec * 1000000 + elapsed.tv_usec;
		maxpkt = (uint64_t)sc->stats.maxPps * usec / 1000000;
		sc->stats.lastTime = now;
		if (packets > maxpkt)
			packets = maxpkt;
	}

	ng_source_send(sc, packets, NULL);
	if (sc->packets == 0)
		ng_source_stop(sc);
	else
		ng_callout(&sc->intr_ch, node, NULL, NG_SOURCE_INTR_TICKS,
		    ng_source_intr, sc, 0);
}

/*
 * Send packets out our output hook
 */
static int
ng_source_send (sc_p sc, int tosend, int *sent_p)
{
	struct ifqueue tmp_queue;
	struct mbuf *m, *m2;
	int sent = 0;
	int error = 0;

	KASSERT(tosend >= 0, ("%s: negative tosend param", __func__));
	KASSERT(sc->node->nd_flags & NG_SOURCE_ACTIVE,
	    ("%s: inactive node", __func__));

	if ((uint64_t)tosend > sc->packets)
		tosend = sc->packets;

	/* Copy the required number of packets to a temporary queue */
	bzero (&tmp_queue, sizeof (tmp_queue));
	for (sent = 0; error == 0 && sent < tosend; ++sent) {
		_IF_DEQUEUE(&sc->snd_queue, m);
		if (m == NULL)
			break;

		/* duplicate the packet */
		m2 = m_copypacket(m, M_DONTWAIT);
		if (m2 == NULL) {
			_IF_PREPEND(&sc->snd_queue, m);
			error = ENOBUFS;
			break;
		}

		/* Re-enqueue the original packet for us. */
		_IF_ENQUEUE(&sc->snd_queue, m);

		/* Queue the copy for sending at splimp. */
		_IF_ENQUEUE(&tmp_queue, m2);
	}

	sent = 0;
	for (;;) {
		_IF_DEQUEUE(&tmp_queue, m2);
		if (m2 == NULL)
			break;
		if (error == 0) {
			++sent;
			sc->stats.outFrames++;
			sc->stats.outOctets += m2->m_pkthdr.len;
			NG_SEND_DATA_ONLY(error, sc->output, m2);
			if (error)
				log(LOG_DEBUG, "%s: error=%d", __func__, error);
		} else {
			NG_FREE_M(m2);
		}
	}

	sc->packets -= sent;
	if (sent_p != NULL)
		*sent_p = sent;
	return (error);
}
