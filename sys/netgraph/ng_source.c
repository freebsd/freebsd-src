/*
 * ng_source.c
 */

/*-
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
#include <net/if.h>
#include <net/if_var.h>
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_ether.h>
#include <netgraph/ng_source.h>

#define NG_SOURCE_INTR_TICKS		1
#define NG_SOURCE_DRIVER_IFQ_MAXLEN	(4*1024)

/* Per hook info */
struct source_hookinfo {
	hook_p				hook;
};

/* Per node info */
struct privdata {
	node_p				node;
	struct source_hookinfo		input;
	struct source_hookinfo		output;
	struct ng_source_stats		stats;
	struct ifqueue			snd_queue;	/* packets to send */
	struct ifnet			*output_ifp;
	struct callout			intr_ch;
	u_int64_t			packets;	/* packets to send */
	u_int32_t			queueOctets;
};
typedef struct privdata *sc_p;

/* Node flags */
#define NG_SOURCE_ACTIVE	(NGF_TYPE1)

/* Netgraph methods */
static ng_constructor_t	ng_source_constructor;
static ng_rcvmsg_t	ng_source_rcvmsg;
static ng_shutdown_t	ng_source_rmnode;
static ng_newhook_t	ng_source_newhook;
static ng_rcvdata_t	ng_source_rcvdata;
static ng_disconnect_t	ng_source_disconnect;

/* Other functions */
static void		ng_source_intr(node_p, hook_p, void *, int);
static int		ng_source_request_output_ifp (sc_p);
static void		ng_source_clr_data (sc_p);
static void		ng_source_start (sc_p);
static void		ng_source_stop (sc_p);
static int		ng_source_send (sc_p, int, int *);
static int		ng_source_store_output_ifp(sc_p sc,
			    struct ng_mesg *msg);

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
	  NGM_SOURCE_START_NOW,
	  "start_now",
	  &ng_parse_uint64_type,
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
	.rcvdata =	ng_source_rcvdata,
	.disconnect =	ng_source_disconnect,
	.cmdlist =	ng_source_cmds,
};
NETGRAPH_INIT(source, &ng_source_typestruct);

static int ng_source_set_autosrc(sc_p, u_int32_t);

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
	sc_p sc;

	sc = NG_NODE_PRIVATE(node);
	KASSERT(sc != NULL, ("%s: null node private", __func__));
	if (strcmp(name, NG_SOURCE_HOOK_INPUT) == 0) {
		sc->input.hook = hook;
		NG_HOOK_SET_PRIVATE(hook, &sc->input);
	} else if (strcmp(name, NG_SOURCE_HOOK_OUTPUT) == 0) {
		sc->output.hook = hook;
		NG_HOOK_SET_PRIVATE(hook, &sc->output);
		sc->output_ifp = 0;
		bzero(&sc->stats, sizeof(sc->stats));
	} else
		return (EINVAL);
	return (0);
}

/*
 * Receive a control message
 */
static int
ng_source_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	sc_p sc;
	struct ng_mesg *resp = NULL;
	int error = 0;
	struct ng_mesg *msg;

	sc = NG_NODE_PRIVATE(node);
	NGI_GET_MSG(item, msg);
	KASSERT(sc != NULL, ("%s: null node private", __func__));
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
			u_int64_t packets = *(u_int64_t *)msg->data;
			if (sc->output.hook == NULL) {
				printf("%s: start on node with no output hook\n"
				    , __func__);
				error = EINVAL;
				break;
			}
			/* TODO validation of packets */
			sc->packets = packets;
			ng_source_start(sc);
		    }
		    break;
		case NGM_SOURCE_START_NOW:
		    {
			u_int64_t packets = *(u_int64_t *)msg->data;
			if (sc->output.hook == NULL) {
				printf("%s: start on node with no output hook\n"
				    , __func__);
				error = EINVAL;
				break;
			}
			if (sc->node->nd_flags & NG_SOURCE_ACTIVE) {
				error = EBUSY;
				break;
			}
			/* TODO validation of packets */
			sc->packets = packets;
		        sc->output_ifp = NULL;

			sc->node->nd_flags |= NG_SOURCE_ACTIVE;
			timevalclear(&sc->stats.elapsedTime);
			timevalclear(&sc->stats.endTime);
			getmicrotime(&sc->stats.startTime);
			ng_callout(&sc->intr_ch, node, NULL, 0,
			    ng_source_intr, sc, 0);
		    }
		    break;
		case NGM_SOURCE_STOP:
			ng_source_stop(sc);
			break;
		case NGM_SOURCE_CLR_DATA:
			ng_source_clr_data(sc);
			break;
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
		case NGM_ETHER_GET_IFINDEX:
			if (ng_source_store_output_ifp(sc, msg) == 0) {
				ng_source_set_autosrc(sc, 0);
				sc->node->nd_flags |= NG_SOURCE_ACTIVE;
				timevalclear(&sc->stats.elapsedTime);
				timevalclear(&sc->stats.endTime);
				getmicrotime(&sc->stats.startTime);
				ng_callout(&sc->intr_ch, node, NULL, 0,
				    ng_source_intr, sc, 0);
			}
			break;
		default:
			error = EINVAL;
		}
		break;
	default:
		error = EINVAL;
		break;
	}

done:
	/* Take care of synchronous response, if any */
	NG_RESPOND_MSG(error, node, item, resp);
	/* Free the message and return */
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
	sc_p sc;
	struct source_hookinfo *hinfo;
	int error = 0;
	struct mbuf *m;

	sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);
	hinfo = NG_HOOK_PRIVATE(hook);
	KASSERT(sc != NULL, ("%s: null node private", __func__));
	KASSERT(hinfo != NULL, ("%s: null hook info", __func__));

	/* Which hook? */
	if (hinfo == &sc->output) {
		/* discard */
		NG_FREE_M(m);
		return (error);
	}
	KASSERT(hinfo == &sc->input, ("%s: no hook!", __func__));

	if ((m->m_flags & M_PKTHDR) == 0) {
		printf("%s: mbuf without PKTHDR\n", __func__);
		NG_FREE_M(m);
		return (EINVAL);
	}

	/* enque packet */
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
	sc_p sc;

	sc = NG_NODE_PRIVATE(node);
	KASSERT(sc != NULL, ("%s: null node private", __func__));
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
	struct source_hookinfo *hinfo;
	sc_p sc;

	hinfo = NG_HOOK_PRIVATE(hook);
	sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	KASSERT(sc != NULL, ("%s: null node private", __func__));
	hinfo->hook = NULL;
	if (NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0 || hinfo == &sc->output)
		ng_rmnode_self(NG_HOOK_NODE(hook));
	return (0);
}

/*
 * 
 * Ask out neighbour on the output hook side to send us it's interface
 * information.
 */
static int
ng_source_request_output_ifp(sc_p sc)
{
	struct ng_mesg *msg;
	int error = 0;

	sc->output_ifp = NULL;

	/* Ask the attached node for the connected interface's index */
	NG_MKMESSAGE(msg, NGM_ETHER_COOKIE, NGM_ETHER_GET_IFINDEX, 0, M_NOWAIT);
	if (msg == NULL)
		return (ENOBUFS);

	NG_SEND_MSG_HOOK(error, sc->node, msg, sc->output.hook, 0);
	return (error);
}

/*
 * Set sc->output_ifp to point to the the struct ifnet of the interface
 * reached via our output hook.
 */
static int
ng_source_store_output_ifp(sc_p sc, struct ng_mesg *msg)
{
	struct ifnet *ifp;
	u_int32_t if_index;
	int s;

	if (msg->header.arglen < sizeof(u_int32_t))
		return (EINVAL);

	if_index = *(u_int32_t *)msg->data;
	/* Could use ifindex2ifnet[if_index] except that we have no
	 * way of verifying if_index is valid since if_indexlim is
	 * local to if_attach()
	 */
	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (ifp->if_index == if_index)
			break;
	}
	IFNET_RUNLOCK();

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
ng_source_set_autosrc(sc_p sc, u_int32_t flag)
{
	struct ng_mesg *msg;
	int error = 0;

	NG_MKMESSAGE(msg, NGM_ETHER_COOKIE, NGM_ETHER_SET_AUTOSRC,
	    sizeof (u_int32_t), M_NOWAIT);
	if (msg == NULL)
		return(ENOBUFS);

	*(u_int32_t *)msg->data = flag;
	NG_SEND_MSG_HOOK(error, sc->node, msg, sc->output.hook, 0);
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
static void
ng_source_start (sc_p sc)
{
	KASSERT(sc->output.hook != NULL,
			("%s: output hook unconnected", __func__));
	if (((sc->node->nd_flags & NG_SOURCE_ACTIVE) == 0) &&
	    (sc->output_ifp == NULL))
		ng_source_request_output_ifp(sc);
}

/*
 * Stop sending queued data out the output hook
 */
static void
ng_source_stop (sc_p sc)
{
	if (sc->node->nd_flags & NG_SOURCE_ACTIVE) {
		ng_uncallout(&sc->intr_ch, sc->node);
		sc->node->nd_flags &= ~NG_SOURCE_ACTIVE;
		getmicrotime(&sc->stats.endTime);
		sc->stats.elapsedTime = sc->stats.endTime;
		timevalsub(&sc->stats.elapsedTime, &sc->stats.startTime);
		/* XXX should set this to the initial value instead */
		ng_source_set_autosrc(sc, 1);
	}
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

	if (sc->packets == 0 || sc->output.hook == NULL
	    || (sc->node->nd_flags & NG_SOURCE_ACTIVE) == 0) {
		ng_source_stop(sc);
		return;
	}

	if (sc->output_ifp != NULL) {
		ifq = (struct ifqueue *)&sc->output_ifp->if_snd;
		packets = ifq->ifq_maxlen - ifq->ifq_len;
	} else
		packets = sc->snd_queue.ifq_len;

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

	KASSERT(sc != NULL, ("%s: null node private", __func__));
	KASSERT(tosend >= 0, ("%s: negative tosend param", __func__));
	KASSERT(sc->node->nd_flags & NG_SOURCE_ACTIVE,
	    ("%s: inactive node", __func__));

	if ((u_int64_t)tosend > sc->packets)
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

		/* re-enqueue the original packet for us */
		_IF_ENQUEUE(&sc->snd_queue, m);

		/* queue the copy for sending at smplimp */
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
			NG_SEND_DATA_ONLY(error, sc->output.hook, m2);
			if (error)
				printf("%s: error=%d\n", __func__, error);
		} else {
			NG_FREE_M(m2);
		}
	}

	sc->packets -= sent;
	if (sent_p != NULL)
		*sent_p = sent;
	return (error);
}
