/*
 * ng_source.c
 *
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
 *
 * $FreeBSD$
 */

/*
 * This node is used for high speed packet geneneration.  It queues
 * all data recieved on it's 'input' hook and when told to start via
 * a control message it sends the packets out it's 'output' hook.  In
 * this way this node can be preloaded with a packet stream which is
 * continuously sent.
 *
 * Currently it just copies the mbufs as required.  It could do various
 * tricks to try and avoid this.  Probably the best performance would
 * be achieved by modifying the appropriate drivers to be told to
 * self-re-enqueue packets (e.g. the if_bge driver could reuse the same
 * transmit descriptors) under control of this node; perhaps via some
 * flag in the mbuf or some such.  The node would peak at an appropriate
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
	struct callout_handle		intr_ch;
	u_int64_t			packets;	/* packets to send */
	u_int32_t			queueOctets;
};
typedef struct privdata *sc_p;

/* Node flags */
#define NG_SOURCE_ACTIVE	(NGF_TYPE1)

/* XXX */
#if 1
#undef KASSERT
#define KASSERT(expr,msg) do {			\
		if (!(expr)) {			\
			printf msg ;		\
			panic("Assertion");	\
		}				\
	} while(0)
#endif

/* Netgraph methods */
static ng_constructor_t	ng_source_constructor;
static ng_rcvmsg_t	ng_source_rcvmsg;
static ng_shutdown_t	ng_source_rmnode;
static ng_newhook_t	ng_source_newhook;
static ng_rcvdata_t	ng_source_rcvdata;
static ng_disconnect_t	ng_source_disconnect;

/* Other functions */
static timeout_t	ng_source_intr;
static int		ng_source_get_output_ifp (sc_p);
static void		ng_source_clr_data (sc_p);
static void		ng_source_start (sc_p);
static void		ng_source_stop (sc_p);
static int		ng_source_send (sc_p, int, int *);


/* Parse type for timeval */
static const struct ng_parse_struct_field ng_source_timeval_type_fields[] =
{
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
	{ 0 }
};

/* Netgraph type descriptor */
static struct ng_type ng_source_typestruct = {
	NG_VERSION,
	NG_SOURCE_NODE_TYPE,
	NULL,					/* module event handler */
	ng_source_constructor,
	ng_source_rcvmsg,
	ng_source_rmnode,
	ng_source_newhook,
	NULL,					/* findhook */
	NULL,
	ng_source_rcvdata,			/* rcvdata */
	ng_source_rcvdata,			/* rcvdataq */
	ng_source_disconnect,
	ng_source_cmds
};
NETGRAPH_INIT(source, &ng_source_typestruct);

/*
 * Node constructor
 */
static int
ng_source_constructor(node_p *nodep)
{
	sc_p sc;
	int error = 0;

	MALLOC(sc, sc_p, sizeof(*sc), M_NETGRAPH, M_NOWAIT);
	if (sc == NULL)
		return (ENOMEM);
	bzero(sc, sizeof(*sc));

	if ((error = ng_make_node_common(&ng_source_typestruct, nodep))) {
		FREE(sc, M_NETGRAPH);
		return (error);
	}
	(*nodep)->private = sc;
	sc->node = *nodep;
	sc->snd_queue.ifq_maxlen = 2048;	/* XXX not checked */
	callout_handle_init(&sc->intr_ch);
	return (0);
}

/*
 * Add a hook
 */
static int
ng_source_newhook(node_p node, hook_p hook, const char *name)
{
	const sc_p sc = node->private;

	KASSERT(sc != NULL, ("%s: null node private", __FUNCTION__));
	if (strcmp(name, NG_SOURCE_HOOK_INPUT) == 0) {
		sc->input.hook = hook;
		hook->private = &sc->input;
	} else if (strcmp(name, NG_SOURCE_HOOK_OUTPUT) == 0) {
		sc->output.hook = hook;
		hook->private = &sc->output;
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
ng_source_rcvmsg(node_p node, struct ng_mesg *msg, const char *retaddr,
	   struct ng_mesg **rptr)
{
	const sc_p sc = node->private;
	struct ng_mesg *resp = NULL;
	int error = 0;

	KASSERT(sc != NULL, ("%s: null node private", __FUNCTION__));
	switch (msg->header.typecookie) {
	case NGM_SOURCE_COOKIE:
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
				if ((sc->node->flags & NG_SOURCE_ACTIVE)
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
				    , __FUNCTION__);
				error = EINVAL;
				break;
			}
			/* TODO validation of packets */
			sc->packets = packets;
			ng_source_start(sc);
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
 * Receive data on a hook
 *
 * If data comes in the input hook, enqueue it on the send queue.
 * If data comes in the output hook, discard it.
 */
static int
ng_source_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const sc_p sc = hook->node->private;
	struct source_hookinfo *const hinfo;
	int error = 0;

	hinfo = (struct source_hookinfo *) hook->private;
	KASSERT(sc != NULL, ("%s: null node private", __FUNCTION__));
	KASSERT(hinfo != NULL, ("%s: null hook info", __FUNCTION__));

	/* Which hook? */
	if (hinfo == &sc->output) {
		/* discard */
		NG_FREE_DATA(m, meta);
		return (error);
	}
	KASSERT(hinfo == &sc->input, ("%s: no hook!", __FUNCTION__));

	if ((m->m_flags & M_PKTHDR) == 0) {
		printf("%s: mbuf without PKTHDR\n", __FUNCTION__);
		NG_FREE_DATA(m, meta);
		return (EINVAL);
	}

	/* XXX we discard the meta data for now */
	NG_FREE_META(meta);

	/* enque packet */
	/* XXX should we check IF_QFULL() ? */
	IF_ENQUEUE(&sc->snd_queue, m);
	sc->queueOctets += m->m_pkthdr.len;

	return (0);
}

/*
 * Shutdown processing
 */
static int
ng_source_rmnode(node_p node)
{
	const sc_p sc = node->private;

	KASSERT(sc != NULL, ("%s: null node private", __FUNCTION__));
	node->flags |= NG_INVALID;
	ng_source_stop(sc);
	ng_cutlinks(node);
	ng_source_clr_data(sc);
	ng_unname(node);
	node->private = NULL;
	ng_unref(sc->node);
	FREE(sc, M_NETGRAPH);
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_source_disconnect(hook_p hook)
{
	struct source_hookinfo *const hinfo;
	sc_p sc;

	hinfo = (struct source_hookinfo *) hook->private;
	sc = (sc_p) hinfo->hook->node->private;
	KASSERT(sc != NULL, ("%s: null node private", __FUNCTION__));
	hinfo->hook = NULL;
	if (hook->node->numhooks == 0 || hinfo == &sc->output)
		ng_rmnode(hook->node);
	return (0);
}

/*
 * Set sc->output_ifp to point to the the struct ifnet of the interface
 * reached via our output hook.
 */
static int
ng_source_get_output_ifp(sc_p sc)
{
	struct ng_mesg *msg, *rsp;
	struct ifnet *ifp;
	u_int32_t if_index;
	int error = 0;
	int s;

	sc->output_ifp = NULL;

	/* Ask the attached node for the connected interface's index */
	NG_MKMESSAGE(msg, NGM_ETHER_COOKIE, NGM_ETHER_GET_IFINDEX, 0, M_NOWAIT);
	if (msg == NULL)
		return (ENOBUFS);

	error = ng_send_msg(sc->node, msg, NG_SOURCE_HOOK_OUTPUT, &rsp);
	if (error != 0)
		return (error);

	if (rsp == NULL)
		return (EINVAL);

	if (rsp->header.arglen < sizeof(u_int32_t))
		return (EINVAL);

	if_index = *(u_int32_t *)rsp->data;
	/* Could use ifindex2ifnet[if_index] except that we have no
	 * way of verifying if_index is valid since if_indexlim is
	 * local to if_attach()
	 */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (ifp->if_index == if_index)
			break;
	}

	if (ifp == NULL) {
		printf("%s: can't find interface %d\n", __FUNCTION__, if_index);
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
	if (ifp->if_snd.ifq_maxlen < NG_SOURCE_DRIVER_IFQ_MAXLEN)
	{
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
	error = ng_send_msg(sc->node, msg, NG_SOURCE_HOOK_OUTPUT, NULL);
	return (error);
}

/*
 * Clear out the data we've queued
 */
static void
ng_source_clr_data (sc_p sc)
{
	struct mbuf *m;

	SPLASSERT(net, __FUNCTION__);
	for (;;) {
		IF_DEQUEUE(&sc->snd_queue, m);
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
	SPLASSERT(net, __FUNCTION__);
	KASSERT(sc->output.hook != NULL,
			("%s: output hook unconnected", __FUNCTION__));
	if ((sc->node->flags & NG_SOURCE_ACTIVE) == 0) {
		if (sc->output_ifp == NULL && ng_source_get_output_ifp(sc) != 0)
			return;
		ng_source_set_autosrc(sc, 0);
		sc->node->flags |= NG_SOURCE_ACTIVE;
		timevalclear(&sc->stats.elapsedTime);
		timevalclear(&sc->stats.endTime);
		getmicrotime(&sc->stats.startTime);
		sc->intr_ch = timeout(ng_source_intr, sc, 0);
	}
}

/*
 * Stop sending queued data out the output hook
 */
static void
ng_source_stop (sc_p sc)
{
	SPLASSERT(net, __FUNCTION__);
	if (sc->node->flags & NG_SOURCE_ACTIVE) {
		untimeout(ng_source_intr, sc, sc->intr_ch);
		sc->node->flags &= ~NG_SOURCE_ACTIVE;
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
ng_source_intr (void *arg)
{
	const sc_p sc = (sc_p) arg;
	struct ifqueue *ifq;
	int packets;

	KASSERT(sc != NULL, ("%s: null node private", __FUNCTION__));

	callout_handle_init(&sc->intr_ch);
	if (sc->packets == 0 || sc->output.hook == NULL
	    || (sc->node->flags & NG_SOURCE_ACTIVE) == 0) {
		ng_source_stop(sc);
		return;
	}

	ifq = &sc->output_ifp->if_snd;
	packets = ifq->ifq_maxlen - ifq->ifq_len;
	ng_source_send(sc, packets, NULL);
	if (sc->packets == 0) {
		int s = splnet();
		ng_source_stop(sc);
		splx(s);
	} else
		sc->intr_ch = timeout(ng_source_intr, sc, NG_SOURCE_INTR_TICKS);
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
	int s, s2;

	KASSERT(sc != NULL, ("%s: null node private", __FUNCTION__));
	KASSERT(tosend >= 0, ("%s: negative tosend param", __FUNCTION__));
	KASSERT(sc->node->flags & NG_SOURCE_ACTIVE,
			("%s: inactive node", __FUNCTION__));

	if ((u_int64_t)tosend > sc->packets)
		tosend = sc->packets;

	/* Copy the required number of packets to a temporary queue */
	bzero (&tmp_queue, sizeof (tmp_queue));
	for (sent = 0; error == 0 && sent < tosend; ++sent) {
		s = splnet();
		IF_DEQUEUE(&sc->snd_queue, m);
		splx(s);
		if (m == NULL)
			break;

		/* duplicate the packet */
		m2 = m_copypacket(m, M_NOWAIT);
		if (m2 == NULL) {
			s = splnet();
			IF_PREPEND(&sc->snd_queue, m);
			splx(s);
			error = ENOBUFS;
			break;
		}

		/* re-enqueue the original packet for us */
		s = splnet();
		IF_ENQUEUE(&sc->snd_queue, m);
		splx(s);

		/* queue the copy for sending at smplimp */
		IF_ENQUEUE(&tmp_queue, m2);
	}

	sent = 0;
	s = splimp();
	for (;;) {
		IF_DEQUEUE(&tmp_queue, m2);
		if (m2 == NULL)
			break;
		if (error == 0) {
			++sent;
			sc->stats.outFrames++;
			sc->stats.outOctets += m2->m_pkthdr.len;
			s2 = splnet();
			NG_SEND_DATA_ONLY(error, sc->output.hook, m2);
			splx(s2);
		} else {
			NG_FREE_M(m2);
		}
	}
	splx(s);

	sc->packets -= sent;
	if (sent_p != NULL)
		*sent_p = sent;
	return (error);
}
