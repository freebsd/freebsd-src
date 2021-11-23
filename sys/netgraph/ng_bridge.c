/*-
 * Copyright (c) 2000 Whistle Communications, Inc.
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
 * Author: Archie Cobbs <archie@freebsd.org>
 *
 * $FreeBSD$
 */

/*
 * ng_bridge(4) netgraph node type
 *
 * The node performs standard intelligent Ethernet bridging over
 * each of its connected hooks, or links.  A simple loop detection
 * algorithm is included which disables a link for priv->conf.loopTimeout
 * seconds when a host is seen to have jumped from one link to
 * another within priv->conf.minStableAge seconds.
 *
 * We keep a hashtable that maps Ethernet addresses to host info,
 * which is contained in struct ng_bridge_host's. These structures
 * tell us on which link the host may be found. A host's entry will
 * expire after priv->conf.maxStaleness seconds.
 *
 * This node is optimzed for stable networks, where machines jump
 * from one port to the other only rarely.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/rwlock.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/ctype.h>
#include <sys/types.h>
#include <sys/counter.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/vnet.h>

#include <netinet/in.h>
#if 0	/* not used yet */
#include <netinet/ip_fw.h>
#endif
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_bridge.h>

#ifdef NG_SEPARATE_MALLOC
static MALLOC_DEFINE(M_NETGRAPH_BRIDGE, "netgraph_bridge",
    "netgraph bridge node");
#else
#define M_NETGRAPH_BRIDGE M_NETGRAPH
#endif

/* Counter based stats */
struct ng_bridge_link_kernel_stats {
	counter_u64_t	recvOctets;	/* total octets rec'd on link */
	counter_u64_t	recvPackets;	/* total pkts rec'd on link */
	counter_u64_t	recvMulticasts;	/* multicast pkts rec'd on link */
	counter_u64_t	recvBroadcasts;	/* broadcast pkts rec'd on link */
	counter_u64_t	recvUnknown;	/* pkts rec'd with unknown dest addr */
	counter_u64_t	recvRunts;	/* pkts rec'd less than 14 bytes */
	counter_u64_t	recvInvalid;	/* pkts rec'd with bogus source addr */
	counter_u64_t	xmitOctets;	/* total octets xmit'd on link */
	counter_u64_t	xmitPackets;	/* total pkts xmit'd on link */
	counter_u64_t	xmitMulticasts;	/* multicast pkts xmit'd on link */
	counter_u64_t	xmitBroadcasts;	/* broadcast pkts xmit'd on link */
	counter_u64_t	loopDrops;	/* pkts dropped due to loopback */
	u_int64_t	loopDetects;	/* number of loop detections */
	counter_u64_t	memoryFailures;	/* times couldn't get mem or mbuf */
};

/* Per-link private data */
struct ng_bridge_link {
	hook_p				hook;		/* netgraph hook */
	u_int16_t			loopCount;	/* loop ignore timer */
	unsigned int			learnMac : 1,   /* autolearn macs */
					sendUnknown : 1;/* send unknown macs out */
	struct ng_bridge_link_kernel_stats stats;	/* link stats */
};
typedef struct ng_bridge_link const *link_cp;	/* read only access */

/* Per-node private data */
struct ng_bridge_private {
	struct ng_bridge_bucket	*tab;		/* hash table bucket array */
	struct ng_bridge_config	conf;		/* node configuration */
	node_p			node;		/* netgraph node */
	u_int			numHosts;	/* num entries in table */
	u_int			numBuckets;	/* num buckets in table */
	u_int			hashMask;	/* numBuckets - 1 */
	int			numLinks;	/* num connected links */
	unsigned int		persistent : 1,	/* can exist w/o hooks */
				sendUnknown : 1;/* links receive unknowns by default */
	struct callout		timer;		/* one second periodic timer */
};
typedef struct ng_bridge_private *priv_p;
typedef struct ng_bridge_private const *priv_cp;	/* read only access */

/* Information about a host, stored in a hash table entry */
struct ng_bridge_host {
	u_char		addr[6];	/* ethernet address */
	link_p		link;		/* link where addr can be found */
	u_int16_t	age;		/* seconds ago entry was created */
	u_int16_t	staleness;	/* seconds ago host last heard from */
	SLIST_ENTRY(ng_bridge_host)	next;	/* next entry in bucket */
};

/* Hash table bucket declaration */
SLIST_HEAD(ng_bridge_bucket, ng_bridge_host);

/* Netgraph node methods */
static ng_constructor_t	ng_bridge_constructor;
static ng_rcvmsg_t	ng_bridge_rcvmsg;
static ng_shutdown_t	ng_bridge_shutdown;
static ng_newhook_t	ng_bridge_newhook;
static ng_rcvdata_t	ng_bridge_rcvdata;
static ng_disconnect_t	ng_bridge_disconnect;

/* Other internal functions */
static void	ng_bridge_free_link(link_p link);
static struct	ng_bridge_host *ng_bridge_get(priv_cp priv, const u_char *addr);
static int	ng_bridge_put(priv_p priv, const u_char *addr, link_p link);
static void	ng_bridge_rehash(priv_p priv);
static void	ng_bridge_remove_hosts(priv_p priv, link_p link);
static void	ng_bridge_timeout(node_p node, hook_p hook, void *arg1, int arg2);
static const	char *ng_bridge_nodename(node_cp node);

/* Ethernet broadcast */
static const u_char ng_bridge_bcast_addr[ETHER_ADDR_LEN] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* Compare Ethernet addresses using 32 and 16 bit words instead of bytewise */
#define ETHER_EQUAL(a,b)	(((const u_int32_t *)(a))[0] \
					== ((const u_int32_t *)(b))[0] \
				    && ((const u_int16_t *)(a))[2] \
					== ((const u_int16_t *)(b))[2])

/* Minimum and maximum number of hash buckets. Must be a power of two. */
#define MIN_BUCKETS		(1 << 5)	/* 32 */
#define MAX_BUCKETS		(1 << 14)	/* 16384 */

/* Configuration default values */
#define DEFAULT_LOOP_TIMEOUT	60
#define DEFAULT_MAX_STALENESS	(15 * 60)	/* same as ARP timeout */
#define DEFAULT_MIN_STABLE_AGE	1

/******************************************************************
		    NETGRAPH PARSE TYPES
******************************************************************/

/*
 * How to determine the length of the table returned by NGM_BRIDGE_GET_TABLE
 */
static int
ng_bridge_getTableLength(const struct ng_parse_type *type,
	const u_char *start, const u_char *buf)
{
	const struct ng_bridge_host_ary *const hary
	    = (const struct ng_bridge_host_ary *)(buf - sizeof(u_int32_t));

	return hary->numHosts;
}

/* Parse type for struct ng_bridge_host_ary */
static const struct ng_parse_struct_field ng_bridge_host_type_fields[]
	= NG_BRIDGE_HOST_TYPE_INFO(&ng_parse_enaddr_type);
static const struct ng_parse_type ng_bridge_host_type = {
	&ng_parse_struct_type,
	&ng_bridge_host_type_fields
};
static const struct ng_parse_array_info ng_bridge_hary_type_info = {
	&ng_bridge_host_type,
	ng_bridge_getTableLength
};
static const struct ng_parse_type ng_bridge_hary_type = {
	&ng_parse_array_type,
	&ng_bridge_hary_type_info
};
static const struct ng_parse_struct_field ng_bridge_host_ary_type_fields[]
	= NG_BRIDGE_HOST_ARY_TYPE_INFO(&ng_bridge_hary_type);
static const struct ng_parse_type ng_bridge_host_ary_type = {
	&ng_parse_struct_type,
	&ng_bridge_host_ary_type_fields
};

/* Parse type for struct ng_bridge_config */
static const struct ng_parse_struct_field ng_bridge_config_type_fields[]
	= NG_BRIDGE_CONFIG_TYPE_INFO;
static const struct ng_parse_type ng_bridge_config_type = {
	&ng_parse_struct_type,
	&ng_bridge_config_type_fields
};

/* Parse type for struct ng_bridge_link_stat */
static const struct ng_parse_struct_field ng_bridge_stats_type_fields[]
	= NG_BRIDGE_STATS_TYPE_INFO;
static const struct ng_parse_type ng_bridge_stats_type = {
	&ng_parse_struct_type,
	&ng_bridge_stats_type_fields
};
/* Parse type for struct ng_bridge_move_host */
static const struct ng_parse_struct_field ng_bridge_move_host_type_fields[]
	= NG_BRIDGE_MOVE_HOST_TYPE_INFO(&ng_parse_enaddr_type);
static const struct ng_parse_type ng_bridge_move_host_type = {
	&ng_parse_struct_type,
	&ng_bridge_move_host_type_fields
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_bridge_cmdlist[] = {
	{
	  NGM_BRIDGE_COOKIE,
	  NGM_BRIDGE_SET_CONFIG,
	  "setconfig",
	  &ng_bridge_config_type,
	  NULL
	},
	{
	  NGM_BRIDGE_COOKIE,
	  NGM_BRIDGE_GET_CONFIG,
	  "getconfig",
	  NULL,
	  &ng_bridge_config_type
	},
	{
	  NGM_BRIDGE_COOKIE,
	  NGM_BRIDGE_RESET,
	  "reset",
	  NULL,
	  NULL
	},
	{
	  NGM_BRIDGE_COOKIE,
	  NGM_BRIDGE_GET_STATS,
	  "getstats",
	  &ng_parse_uint32_type,
	  &ng_bridge_stats_type
	},
	{
	  NGM_BRIDGE_COOKIE,
	  NGM_BRIDGE_CLR_STATS,
	  "clrstats",
	  &ng_parse_uint32_type,
	  NULL
	},
	{
	  NGM_BRIDGE_COOKIE,
	  NGM_BRIDGE_GETCLR_STATS,
	  "getclrstats",
	  &ng_parse_uint32_type,
	  &ng_bridge_stats_type
	},
	{
	  NGM_BRIDGE_COOKIE,
	  NGM_BRIDGE_GET_TABLE,
	  "gettable",
	  NULL,
	  &ng_bridge_host_ary_type
	},
	{
	  NGM_BRIDGE_COOKIE,
	  NGM_BRIDGE_SET_PERSISTENT,
	  "setpersistent",
	  NULL,
	  NULL
	},
	{
	  NGM_BRIDGE_COOKIE,
	  NGM_BRIDGE_MOVE_HOST,
	  "movehost",
	  &ng_bridge_move_host_type,
	  NULL
	},
	{ 0 }
};

/* Node type descriptor */
static struct ng_type ng_bridge_typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_BRIDGE_NODE_TYPE,
	.constructor =	ng_bridge_constructor,
	.rcvmsg =	ng_bridge_rcvmsg,
	.shutdown =	ng_bridge_shutdown,
	.newhook =	ng_bridge_newhook,
	.rcvdata =	ng_bridge_rcvdata,
	.disconnect =	ng_bridge_disconnect,
	.cmdlist =	ng_bridge_cmdlist,
};
NETGRAPH_INIT(bridge, &ng_bridge_typestruct);

/******************************************************************
		    NETGRAPH NODE METHODS
******************************************************************/

/*
 * Node constructor
 */
static int
ng_bridge_constructor(node_p node)
{
	priv_p priv;

	/* Allocate and initialize private info */
	priv = malloc(sizeof(*priv), M_NETGRAPH_BRIDGE, M_WAITOK | M_ZERO);
	ng_callout_init(&priv->timer);

	/* Allocate and initialize hash table, etc. */
	priv->tab = malloc(MIN_BUCKETS * sizeof(*priv->tab),
	    M_NETGRAPH_BRIDGE, M_WAITOK | M_ZERO);
	priv->numBuckets = MIN_BUCKETS;
	priv->hashMask = MIN_BUCKETS - 1;
	priv->conf.debugLevel = 1;
	priv->conf.loopTimeout = DEFAULT_LOOP_TIMEOUT;
	priv->conf.maxStaleness = DEFAULT_MAX_STALENESS;
	priv->conf.minStableAge = DEFAULT_MIN_STABLE_AGE;
	priv->sendUnknown = 1;	       /* classic bridge */

	NG_NODE_SET_PRIVATE(node, priv);
	priv->node = node;

	/* Start timer; timer is always running while node is alive */
	ng_callout(&priv->timer, node, NULL, hz, ng_bridge_timeout, NULL, 0);

	/* Done */
	return (0);
}

/*
 * Method for attaching a new hook
 */
static	int
ng_bridge_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	char linkName[NG_HOOKSIZ];
	u_int32_t linkNum;
	link_p link;
	const char *prefix = NG_BRIDGE_HOOK_LINK_PREFIX;
	bool isUplink;

	/* Check for a link hook */
	if (strlen(name) <= strlen(prefix))
		return (EINVAL);       /* Unknown hook name */

	isUplink = (name[0] == 'u');
	if (isUplink)
		prefix = NG_BRIDGE_HOOK_UPLINK_PREFIX;

	/* primitive parsing */
	linkNum = strtoul(name + strlen(prefix), NULL, 10);
	/* validation by comparing against the reconstucted name  */
	snprintf(linkName, sizeof(linkName), "%s%u", prefix, linkNum);
	if (strcmp(linkName, name) != 0)
		return (EINVAL);

	if (linkNum == 0 && isUplink)
		return (EINVAL);

	if(NG_PEER_NODE(hook) == node)
	        return (ELOOP);

	link = malloc(sizeof(*link), M_NETGRAPH_BRIDGE, M_NOWAIT | M_ZERO);
	if (link == NULL)
		return (ENOMEM);

#define	NG_BRIDGE_COUNTER_ALLOC(f) do {			\
	link->stats.f = counter_u64_alloc(M_NOWAIT);	\
	if (link->stats.f == NULL)			\
		goto nomem;				\
} while (0)
	NG_BRIDGE_COUNTER_ALLOC(recvOctets);
	NG_BRIDGE_COUNTER_ALLOC(recvPackets);
	NG_BRIDGE_COUNTER_ALLOC(recvMulticasts);
	NG_BRIDGE_COUNTER_ALLOC(recvBroadcasts);
	NG_BRIDGE_COUNTER_ALLOC(recvUnknown);
	NG_BRIDGE_COUNTER_ALLOC(recvRunts);
	NG_BRIDGE_COUNTER_ALLOC(recvInvalid);
	NG_BRIDGE_COUNTER_ALLOC(xmitOctets);
	NG_BRIDGE_COUNTER_ALLOC(xmitPackets);
	NG_BRIDGE_COUNTER_ALLOC(xmitMulticasts);
	NG_BRIDGE_COUNTER_ALLOC(xmitBroadcasts);
	NG_BRIDGE_COUNTER_ALLOC(loopDrops);
	NG_BRIDGE_COUNTER_ALLOC(memoryFailures);
#undef NG_BRIDGE_COUNTER_ALLOC

	link->hook = hook;
	if (isUplink) {
		link->learnMac = 0;
		link->sendUnknown = 1;
		if (priv->numLinks == 0)	/* if the first link is an uplink */
			priv->sendUnknown = 0;	/* switch to restrictive mode */
	} else {
		link->learnMac = 1;
		link->sendUnknown = priv->sendUnknown;
	}

	NG_HOOK_SET_PRIVATE(hook, link);
	priv->numLinks++;
	return (0);

nomem:
	ng_bridge_free_link(link);
	return (ENOMEM);
}

/*
 * Receive a control message
 */
static void
ng_bridge_clear_link_stats(struct ng_bridge_link_kernel_stats *p)
{
	counter_u64_zero(p->recvOctets);
	counter_u64_zero(p->recvPackets);
	counter_u64_zero(p->recvMulticasts);
	counter_u64_zero(p->recvBroadcasts);
	counter_u64_zero(p->recvUnknown);
	counter_u64_zero(p->recvRunts);
	counter_u64_zero(p->recvInvalid);
	counter_u64_zero(p->xmitOctets);
	counter_u64_zero(p->xmitPackets);
	counter_u64_zero(p->xmitMulticasts);
	counter_u64_zero(p->xmitBroadcasts);
	counter_u64_zero(p->loopDrops);
	p->loopDetects = 0;
	counter_u64_zero(p->memoryFailures);
}

static void
ng_bridge_free_link(link_p link)
{
	counter_u64_free(link->stats.recvOctets);
	counter_u64_free(link->stats.recvPackets);
	counter_u64_free(link->stats.recvMulticasts);
	counter_u64_free(link->stats.recvBroadcasts);
	counter_u64_free(link->stats.recvUnknown);
	counter_u64_free(link->stats.recvRunts);
	counter_u64_free(link->stats.recvInvalid);
	counter_u64_free(link->stats.xmitOctets);
	counter_u64_free(link->stats.xmitPackets);
	counter_u64_free(link->stats.xmitMulticasts);
	counter_u64_free(link->stats.xmitBroadcasts);
	counter_u64_free(link->stats.loopDrops);
	counter_u64_free(link->stats.memoryFailures);
	free(link, M_NETGRAPH_BRIDGE);
}

static int
ng_bridge_reset_link(hook_p hook, void *arg __unused)
{
	link_p priv = NG_HOOK_PRIVATE(hook);

	priv->loopCount = 0;
	ng_bridge_clear_link_stats(&priv->stats);
	return (1);
}

static int
ng_bridge_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ng_mesg *resp = NULL;
	int error = 0;
	struct ng_mesg *msg;

	NGI_GET_MSG(item, msg);
	switch (msg->header.typecookie) {
	case NGM_BRIDGE_COOKIE:
		switch (msg->header.cmd) {
		case NGM_BRIDGE_GET_CONFIG:
		    {
			struct ng_bridge_config *conf;

			NG_MKRESPONSE(resp, msg,
			    sizeof(struct ng_bridge_config), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			conf = (struct ng_bridge_config *)resp->data;
			*conf = priv->conf;	/* no sanity checking needed */
			break;
		    }
		case NGM_BRIDGE_SET_CONFIG:
		    {
			struct ng_bridge_config *conf;

			if (msg->header.arglen
			    != sizeof(struct ng_bridge_config)) {
				error = EINVAL;
				break;
			}
			conf = (struct ng_bridge_config *)msg->data;
			priv->conf = *conf;
			break;
		    }
		case NGM_BRIDGE_RESET:
		    {
			hook_p rethook;

			/* Flush all entries in the hash table */
			ng_bridge_remove_hosts(priv, NULL);

			/* Reset all loop detection counters and stats */
			NG_NODE_FOREACH_HOOK(node, ng_bridge_reset_link, NULL,
			    rethook);
			break;
		    }
		case NGM_BRIDGE_GET_STATS:
		case NGM_BRIDGE_CLR_STATS:
		case NGM_BRIDGE_GETCLR_STATS:
		    {
			hook_p hook;
			link_p link;
			char linkName[NG_HOOKSIZ];
			int linkNum;
			    
			/* Get link number */
			if (msg->header.arglen != sizeof(u_int32_t)) {
				error = EINVAL;
				break;
			}
			linkNum = *((int32_t *)msg->data);
			if (linkNum < 0)
				snprintf(linkName, sizeof(linkName),
				    "%s%u", NG_BRIDGE_HOOK_UPLINK_PREFIX, -linkNum);
			else
				snprintf(linkName, sizeof(linkName),
				    "%s%u", NG_BRIDGE_HOOK_LINK_PREFIX, linkNum);
			    
			if ((hook = ng_findhook(node, linkName)) == NULL) {
				error = ENOTCONN;
				break;
			}
			link = NG_HOOK_PRIVATE(hook);

			/* Get/clear stats */
			if (msg->header.cmd != NGM_BRIDGE_CLR_STATS) {
				struct ng_bridge_link_stats *rs;

				NG_MKRESPONSE(resp, msg,
				    sizeof(link->stats), M_NOWAIT);
				if (resp == NULL) {
					error = ENOMEM;
					break;
				}
				rs = (struct ng_bridge_link_stats *)resp->data;
#define FETCH(x)	rs->x = counter_u64_fetch(link->stats.x)
				FETCH(recvOctets);
				FETCH(recvPackets);
				FETCH(recvMulticasts);
				FETCH(recvBroadcasts);
				FETCH(recvUnknown);
				FETCH(recvRunts);
				FETCH(recvInvalid);
				FETCH(xmitOctets);
				FETCH(xmitPackets);
				FETCH(xmitMulticasts);
				FETCH(xmitBroadcasts);
				FETCH(loopDrops);
				rs->loopDetects = link->stats.loopDetects;
				FETCH(memoryFailures);
#undef FETCH
			}
			if (msg->header.cmd != NGM_BRIDGE_GET_STATS)
				ng_bridge_clear_link_stats(&link->stats);
			break;
		    }
		case NGM_BRIDGE_GET_TABLE:
		    {
			struct ng_bridge_host_ary *ary;
			struct ng_bridge_host *host;
			int i = 0, bucket;

			NG_MKRESPONSE(resp, msg, sizeof(*ary)
			    + (priv->numHosts * sizeof(*ary->hosts)), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			ary = (struct ng_bridge_host_ary *)resp->data;
			ary->numHosts = priv->numHosts;
			for (bucket = 0; bucket < priv->numBuckets; bucket++) {
				SLIST_FOREACH(host, &priv->tab[bucket], next) {
					memcpy(ary->hosts[i].addr,
					       host->addr,
					       sizeof(ary->hosts[i].addr));
					ary->hosts[i].age       = host->age;
					ary->hosts[i].staleness = host->staleness;
					strncpy(ary->hosts[i].hook,
						NG_HOOK_NAME(host->link->hook),
						sizeof(ary->hosts[i].hook));
					i++;
				}
			}
			break;
		    }
		case NGM_BRIDGE_SET_PERSISTENT:
		    {
			priv->persistent = 1;
			break;
		    }
		case NGM_BRIDGE_MOVE_HOST:
		{
			struct ng_bridge_move_host *mh;
			hook_p hook;

			if (msg->header.arglen < sizeof(*mh)) {
				error = EINVAL;
				break;
			}
			mh = (struct ng_bridge_move_host *)msg->data;
			hook = (mh->hook[0] == 0)
			    ? lasthook
			    : ng_findhook(node, mh->hook);
			if (hook == NULL) {
				error = ENOENT;
				break;
			}
			error = ng_bridge_put(priv, mh->addr, NG_HOOK_PRIVATE(hook));
			break;
		}
		default:
			error = EINVAL;
			break;
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	/* Done */
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

/*
 * Receive data on a hook
 */
struct ng_bridge_send_ctx {
	link_p foundFirst, incoming;
	struct mbuf * m;
	int manycast, error;
};

/*
 * Update stats and send out
 */
static inline int
ng_bridge_send_data(link_cp dst, int manycast, struct mbuf *m, item_p item) {
	int error = 0;
	size_t len = m->m_pkthdr.len;

	if(item != NULL)
		NG_FWD_NEW_DATA(error, item, dst->hook, m);
	else
		NG_SEND_DATA_ONLY(error, dst->hook, m);

	if (error) {
		if (error == ENOMEM)
			counter_u64_add(dst->stats.memoryFailures, 1);
		/* The packet is still ours */
		if (item != NULL)
			NG_FREE_ITEM(item);
		if (m != NULL)
			NG_FREE_M(m);
		return (error);
	}

	counter_u64_add(dst->stats.xmitPackets, 1);
	counter_u64_add(dst->stats.xmitOctets, len);
	switch (manycast) {
	default:		       /* unknown unicast */
		break;
	case 1:			       /* multicast */
		counter_u64_add(dst->stats.xmitMulticasts, 1);
		break;
	case 2:			       /* broadcast */
		counter_u64_add(dst->stats.xmitBroadcasts, 1);
		break;
	}
	return (0);
}

/*
 * Loop body for sending to multiple destinations
 * return 0 to stop looping
 */
static int
ng_bridge_send_ctx(hook_p dst, void *arg)
{
	struct ng_bridge_send_ctx *ctx = arg;
	link_p destLink = NG_HOOK_PRIVATE(dst);
	struct mbuf *m2 = NULL;
	int error = 0;

	/* Skip incoming link */
	if (destLink == ctx->incoming) {
		return (1);
	}

	/* Skip sending unknowns to undesired links  */
	if (!ctx->manycast && !destLink->sendUnknown)
		return (1);

	if (ctx->foundFirst == NULL) {
		/*
		 * This is the first usable link we have found.
		 * Reserve it for the originals.
		 * If we never find another we save a copy.
		 */
		ctx->foundFirst = destLink;
		return (1);
	}

	/*
	 * It's usable link but not the reserved (first) one.
	 * Copy mbuf info for sending.
	 */
	m2 = m_dup(ctx->m, M_NOWAIT);
	if (m2 == NULL) {
		counter_u64_add(ctx->incoming->stats.memoryFailures, 1);
		ctx->error = ENOBUFS;
		return (0);	       /* abort loop, do not try again and again */
	}

	/* Send packet */
	error = ng_bridge_send_data(destLink, ctx->manycast, m2, NULL);
	if (error)
	  ctx->error = error;
	return (1);
}

static int
ng_bridge_rcvdata(hook_p hook, item_p item)
{
	const node_p node = NG_HOOK_NODE(hook);
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ng_bridge_host *host;
	struct ether_header *eh;
	struct ng_bridge_send_ctx ctx = { 0 };
	hook_p ret;

	NGI_GET_M(item, ctx.m);

	ctx.incoming = NG_HOOK_PRIVATE(hook);
	/* Sanity check packet and pull up header */
	if (ctx.m->m_pkthdr.len < ETHER_HDR_LEN) {
		counter_u64_add(ctx.incoming->stats.recvRunts, 1);
		NG_FREE_ITEM(item);
		NG_FREE_M(ctx.m);
		return (EINVAL);
	}
	if (ctx.m->m_len < ETHER_HDR_LEN && !(ctx.m = m_pullup(ctx.m, ETHER_HDR_LEN))) {
		counter_u64_add(ctx.incoming->stats.memoryFailures, 1);
		NG_FREE_ITEM(item);
		return (ENOBUFS);
	}
	eh = mtod(ctx.m, struct ether_header *);
	if ((eh->ether_shost[0] & 1) != 0) {
		counter_u64_add(ctx.incoming->stats.recvInvalid, 1);
		NG_FREE_ITEM(item);
		NG_FREE_M(ctx.m);
		return (EINVAL);
	}

	/* Is link disabled due to a loopback condition? */
	if (ctx.incoming->loopCount != 0) {
		counter_u64_add(ctx.incoming->stats.loopDrops, 1);
		NG_FREE_ITEM(item);
		NG_FREE_M(ctx.m);
		return (ELOOP);
	}

	/* Update stats */
	counter_u64_add(ctx.incoming->stats.recvPackets, 1);
	counter_u64_add(ctx.incoming->stats.recvOctets, ctx.m->m_pkthdr.len);
	if ((ctx.manycast = (eh->ether_dhost[0] & 1)) != 0) {
		if (ETHER_EQUAL(eh->ether_dhost, ng_bridge_bcast_addr)) {
			counter_u64_add(ctx.incoming->stats.recvBroadcasts, 1);
			ctx.manycast = 2;
		} else
			counter_u64_add(ctx.incoming->stats.recvMulticasts, 1);
	}

	/* Look up packet's source Ethernet address in hashtable */
	if ((host = ng_bridge_get(priv, eh->ether_shost)) != NULL)
		/* Update time since last heard from this host.
		 * This is safe without locking, because it's
		 * the only operation during shared access.
		 */
		if (__predict_false(host->staleness > 0))
			host->staleness = 0;

	if ((host == NULL && ctx.incoming->learnMac) ||
	    (host != NULL && host->link != ctx.incoming)) {
		struct ng_mesg *msg;
		struct ng_bridge_move_host *mh;
		int error = 0;

		NG_MKMESSAGE(msg, NGM_BRIDGE_COOKIE, NGM_BRIDGE_MOVE_HOST,
		    sizeof(*mh), M_NOWAIT);
		if (msg == NULL) {
			counter_u64_add(ctx.incoming->stats.memoryFailures, 1);
			NG_FREE_ITEM(item);
			NG_FREE_M(ctx.m);
			return (ENOMEM);
		}
		mh = (struct ng_bridge_move_host *)msg->data;
		strncpy(mh->hook, NG_HOOK_NAME(ctx.incoming->hook),
		    sizeof(mh->hook));
		memcpy(mh->addr, eh->ether_shost, sizeof(mh->addr));
		NG_SEND_MSG_ID(error, node, msg, NG_NODE_ID(node),
		    NG_NODE_ID(node));
		if (error)
			counter_u64_add(ctx.incoming->stats.memoryFailures, 1);
	}

	if (host != NULL && host->link != ctx.incoming) {
		if (host->age < priv->conf.minStableAge) {
			/* Drop packet on instable links */
			counter_u64_add(ctx.incoming->stats.loopDrops, 1);
			NG_FREE_ITEM(item);
			NG_FREE_M(ctx.m);
			return (ELOOP);
		}
	}

	/* Run packet through ipfw processing, if enabled */
#if 0
	if (priv->conf.ipfw[linkNum] && V_fw_enable && V_ip_fw_chk_ptr != NULL) {
		/* XXX not implemented yet */
	}
#endif

	/*
	 * If unicast and destination host known, deliver to host's link,
	 * unless it is the same link as the packet came in on.
	 */
	if (!ctx.manycast) {
		/* Determine packet destination link */
		if ((host = ng_bridge_get(priv, eh->ether_dhost)) != NULL) {
			link_p destLink = host->link;

			/* If destination same as incoming link, do nothing */
			if (destLink == ctx.incoming) {
				NG_FREE_ITEM(item);
				NG_FREE_M(ctx.m);
				return (0);
			}

			/* Deliver packet out the destination link */
			return (ng_bridge_send_data(destLink, ctx.manycast, ctx.m, item));
		}

		/* Destination host is not known */
		counter_u64_add(ctx.incoming->stats.recvUnknown, 1);
	}

	/* Distribute unknown, multicast, broadcast pkts to all other links */
	NG_NODE_FOREACH_HOOK(node, ng_bridge_send_ctx, &ctx, ret);

	/* Finally send out on the first link found */
	if (ctx.foundFirst != NULL) {
		int error = ng_bridge_send_data(ctx.foundFirst, ctx.manycast, ctx.m, item);
		if (error)
			ctx.error = error;
	} else {		       /* nothing to send at all */
		NG_FREE_ITEM(item);
		NG_FREE_M(ctx.m);
	}

	return (ctx.error);
}

/*
 * Shutdown node
 */
static int
ng_bridge_shutdown(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	/*
	 * Shut down everything including the timer.  Even if the
	 * callout has already been dequeued and is about to be
	 * run, ng_bridge_timeout() won't be fired as the node
	 * is already marked NGF_INVALID, so we're safe to free
	 * the node now.
	 */
	KASSERT(priv->numLinks == 0 && priv->numHosts == 0,
	    ("%s: numLinks=%d numHosts=%d",
	    __func__, priv->numLinks, priv->numHosts));
	ng_uncallout(&priv->timer, node);
	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);
	free(priv->tab, M_NETGRAPH_BRIDGE);
	free(priv, M_NETGRAPH_BRIDGE);
	return (0);
}

/*
 * Hook disconnection.
 */
static int
ng_bridge_disconnect(hook_p hook)
{
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	link_p link = NG_HOOK_PRIVATE(hook);

	/* Remove all hosts associated with this link */
	ng_bridge_remove_hosts(priv, link);

	/* Free associated link information */
	ng_bridge_free_link(link);
	priv->numLinks--;

	/* If no more hooks, go away */
	if ((NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0)
	    && (NG_NODE_IS_VALID(NG_HOOK_NODE(hook)))
	    && !priv->persistent) {
		ng_rmnode_self(NG_HOOK_NODE(hook));
	}
	return (0);
}

/******************************************************************
		    HASH TABLE FUNCTIONS
******************************************************************/

/*
 * Hash algorithm
 */
#define HASH(addr,mask)		( (((const u_int16_t *)(addr))[0] 	\
				 ^ ((const u_int16_t *)(addr))[1] 	\
				 ^ ((const u_int16_t *)(addr))[2]) & (mask) )

/*
 * Find a host entry in the table.
 */
static struct ng_bridge_host *
ng_bridge_get(priv_cp priv, const u_char *addr)
{
	const int bucket = HASH(addr, priv->hashMask);
	struct ng_bridge_host *host;

	SLIST_FOREACH(host, &priv->tab[bucket], next) {
		if (ETHER_EQUAL(host->addr, addr))
			return (host);
	}
	return (NULL);
}

/*
 * Add a host entry to the table. If it already exists, move it
 * to the new link. Returns 0 on success.
 */
static int
ng_bridge_put(priv_p priv, const u_char *addr, link_p link)
{
	const int bucket = HASH(addr, priv->hashMask);
	struct ng_bridge_host *host;

	if ((host = ng_bridge_get(priv, addr)) != NULL) {
		/* Host already on the correct link? */
		if (host->link == link)
			return 0;

		/* Move old host over to new link */
		if (host->age >= priv->conf.minStableAge) {
			host->link = link;
			host->age = 0;
			return (0);
		}
		/*
		 * If the host was recently moved to the old link and
		 * it's now jumping to a new link, declare a loopback
		 * condition.
		 */
		if (priv->conf.debugLevel >= 2)
		    log(LOG_WARNING, "ng_bridge: %s:"
			" loopback detected on %s\n",
			ng_bridge_nodename(priv->node),
			NG_HOOK_NAME(link->hook));

		/* Mark link as linka non grata */
		link->loopCount = priv->conf.loopTimeout;
		link->stats.loopDetects++;

		/* Forget all hosts on this link */
		ng_bridge_remove_hosts(priv, link);
		return (ELOOP);
	}

	/* Allocate and initialize new hashtable entry */
	host = malloc(sizeof(*host), M_NETGRAPH_BRIDGE, M_NOWAIT);
	if (host == NULL)
		return (ENOMEM);
	bcopy(addr, host->addr, ETHER_ADDR_LEN);
	host->link = link;
	host->staleness = 0;
	host->age = 0;

	/* Add new element to hash bucket */
	SLIST_INSERT_HEAD(&priv->tab[bucket], host, next);
	priv->numHosts++;

	/* Resize table if necessary */
	ng_bridge_rehash(priv);
	return (0);
}

/*
 * Resize the hash table. We try to maintain the number of buckets
 * such that the load factor is in the range 0.25 to 1.0.
 *
 * If we can't get the new memory then we silently fail. This is OK
 * because things will still work and we'll try again soon anyway.
 */
static void
ng_bridge_rehash(priv_p priv)
{
	struct ng_bridge_bucket *newTab;
	int oldBucket, newBucket;
	int newNumBuckets;
	u_int newMask;

	/* Is table too full or too empty? */
	if (priv->numHosts > priv->numBuckets
	    && (priv->numBuckets << 1) <= MAX_BUCKETS)
		newNumBuckets = priv->numBuckets << 1;
	else if (priv->numHosts < (priv->numBuckets >> 2)
	    && (priv->numBuckets >> 2) >= MIN_BUCKETS)
		newNumBuckets = priv->numBuckets >> 2;
	else
		return;
	newMask = newNumBuckets - 1;

	/* Allocate and initialize new table */
	newTab = malloc(newNumBuckets * sizeof(*newTab),
	    M_NETGRAPH_BRIDGE, M_NOWAIT | M_ZERO);
	if (newTab == NULL)
		return;

	/* Move all entries from old table to new table */
	for (oldBucket = 0; oldBucket < priv->numBuckets; oldBucket++) {
		struct ng_bridge_bucket *const oldList = &priv->tab[oldBucket];

		while (!SLIST_EMPTY(oldList)) {
			struct ng_bridge_host *const host
			    = SLIST_FIRST(oldList);

			SLIST_REMOVE_HEAD(oldList, next);
			newBucket = HASH(host->addr, newMask);
			SLIST_INSERT_HEAD(&newTab[newBucket], host, next);
		}
	}

	/* Replace old table with new one */
	if (priv->conf.debugLevel >= 3) {
		log(LOG_INFO, "ng_bridge: %s: table size %d -> %d\n",
		    ng_bridge_nodename(priv->node),
		    priv->numBuckets, newNumBuckets);
	}
	free(priv->tab, M_NETGRAPH_BRIDGE);
	priv->numBuckets = newNumBuckets;
	priv->hashMask = newMask;
	priv->tab = newTab;
	return;
}

/******************************************************************
		    MISC FUNCTIONS
******************************************************************/

/*
 * Remove all hosts associated with a specific link from the hashtable.
 * If linkNum == -1, then remove all hosts in the table.
 */
static void
ng_bridge_remove_hosts(priv_p priv, link_p link)
{
	int bucket;

	for (bucket = 0; bucket < priv->numBuckets; bucket++) {
		struct ng_bridge_host **hptr = &SLIST_FIRST(&priv->tab[bucket]);

		while (*hptr != NULL) {
			struct ng_bridge_host *const host = *hptr;

			if (link == NULL || host->link == link) {
				*hptr = SLIST_NEXT(host, next);
				free(host, M_NETGRAPH_BRIDGE);
				priv->numHosts--;
			} else
				hptr = &SLIST_NEXT(host, next);
		}
	}
}

/*
 * Handle our once-per-second timeout event. We do two things:
 * we decrement link->loopCount for those links being muted due to
 * a detected loopback condition, and we remove any hosts from
 * the hashtable whom we haven't heard from in a long while.
 */
static int
ng_bridge_unmute(hook_p hook, void *arg)
{
	link_p link = NG_HOOK_PRIVATE(hook);
	node_p node = NG_HOOK_NODE(hook);
	priv_p priv = NG_NODE_PRIVATE(node);
	int *counter = arg;

	if (link->loopCount != 0) {
		link->loopCount--;
		if (link->loopCount == 0 && priv->conf.debugLevel >= 2) {
			log(LOG_INFO, "ng_bridge: %s:"
			    " restoring looped back %s\n",
			    ng_bridge_nodename(node), NG_HOOK_NAME(hook));
		}
	}
	(*counter)++;
	return (1);
}

static void
ng_bridge_timeout(node_p node, hook_p hook, void *arg1, int arg2)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	int bucket;
	int counter = 0;
	hook_p ret;

	/* Update host time counters and remove stale entries */
	for (bucket = 0; bucket < priv->numBuckets; bucket++) {
		struct ng_bridge_host **hptr = &SLIST_FIRST(&priv->tab[bucket]);

		while (*hptr != NULL) {
			struct ng_bridge_host *const host = *hptr;

			/* Remove hosts we haven't heard from in a while */
			if (++host->staleness >= priv->conf.maxStaleness) {
				*hptr = SLIST_NEXT(host, next);
				free(host, M_NETGRAPH_BRIDGE);
				priv->numHosts--;
			} else {
				if (host->age < 0xffff)
					host->age++;
				hptr = &SLIST_NEXT(host, next);
				counter++;
			}
		}
	}
	KASSERT(priv->numHosts == counter,
	    ("%s: hosts: %d != %d", __func__, priv->numHosts, counter));

	/* Decrease table size if necessary */
	ng_bridge_rehash(priv);

	/* Decrease loop counter on muted looped back links */
	counter = 0;
	NG_NODE_FOREACH_HOOK(node, ng_bridge_unmute, &counter, ret);
	KASSERT(priv->numLinks == counter,
	    ("%s: links: %d != %d", __func__, priv->numLinks, counter));

	/* Register a new timeout, keeping the existing node reference */
	ng_callout(&priv->timer, node, NULL, hz, ng_bridge_timeout, NULL, 0);
}

/*
 * Return node's "name", even if it doesn't have one.
 */
static const char *
ng_bridge_nodename(node_cp node)
{
	static char name[NG_NODESIZ];

	if (NG_NODE_HAS_NAME(node))
		snprintf(name, sizeof(name), "%s", NG_NODE_NAME(node));
	else
		snprintf(name, sizeof(name), "[%x]", ng_node2ID(node));
	return name;
}
