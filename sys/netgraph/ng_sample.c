
/*
 * ng_sample.c
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
 * $Whistle: ng_sample.c,v 1.13 1999/11/01 09:24:52 julian Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/ctype.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_sample.h>
#include <netgraph/netgraph.h>

/*
 * This section contains the netgraph method declarations for the
 * sample node. These methods define the netgraph 'type'.
 */

static ng_constructor_t	ng_xxx_constructor;
static ng_rcvmsg_t	ng_xxx_rcvmsg;
static ng_shutdown_t	ng_xxx_rmnode;
static ng_newhook_t	ng_xxx_newhook;
static ng_connect_t	ng_xxx_connect;
static ng_rcvdata_t	ng_xxx_rcvdata;	 /* note these are both ng_rcvdata_t */
static ng_rcvdata_t	ng_xxx_rcvdataq; /* note these are both ng_rcvdata_t */
static ng_disconnect_t	ng_xxx_disconnect;

/* Parse type for struct ngxxxstat */
static const struct ng_parse_struct_field ng_xxx_stat_type_fields[]
	= NG_XXX_STATS_TYPE_INFO;
static const struct ng_parse_type ng_xxx_stat_type = {
	&ng_parse_struct_type,
	&ng_xxx_stat_type_fields
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_xxx_cmdlist[] = {
	{
	  NGM_XXX_COOKIE,
	  NGM_XXX_GET_STATUS,
	  "getstatus",
	  NULL,
	  &ng_xxx_stat_type,
	},
	{
	  NGM_XXX_COOKIE,
	  NGM_XXX_SET_FLAG,
	  "setflag",
	  &ng_parse_int32_type,
	  NULL
	},
	{ 0 }
};

/* Netgraph node type descriptor */
static struct ng_type typestruct = {
	NG_VERSION,
	NG_XXX_NODE_TYPE,
	NULL,
	ng_xxx_constructor,
	ng_xxx_rcvmsg,
	ng_xxx_rmnode,
	ng_xxx_newhook,
	NULL,
	ng_xxx_connect,
	ng_xxx_rcvdata,
	ng_xxx_rcvdataq,
	ng_xxx_disconnect,
	ng_xxx_cmdlist
};
NETGRAPH_INIT(xxx, &typestruct);

/* Information we store for each hook on each node */
struct XXX_hookinfo {
	int     dlci;		/* The DLCI it represents, -1 == downstream */
	int     channel;	/* The channel representing this DLCI */
	hook_p  hook;
};

/* Information we store for each node */
struct XXX {
	struct XXX_hookinfo channel[XXX_NUM_DLCIS];
	struct XXX_hookinfo downstream_hook;
	node_p		node;		/* back pointer to node */
	hook_p  	debughook;
	u_int   	packets_in;	/* packets in from downstream */
	u_int   	packets_out;	/* packets out towards downstream */
	u_int32_t	flags;
};
typedef struct XXX *xxx_p;

/*
 * Allocate the private data structure and the generic node
 * and link them together.
 *
 * ng_make_node_common() returns with a generic node struct
 * with a single reference for us.. we transfer it to the
 * private structure.. when we free the private struct we must
 * unref the node so it gets freed too.
 *
 * If this were a device node than this work would be done in the attach()
 * routine and the constructor would return EINVAL as you should not be able
 * to creatednodes that depend on hardware (unless you can add the hardware :)
 */
static int
ng_xxx_constructor(node_p *nodep)
{
	xxx_p privdata;
	int i, error;

	/* Initialize private descriptor */
	MALLOC(privdata, xxx_p, sizeof(*privdata), M_NETGRAPH, M_NOWAIT);
	if (privdata == NULL)
		return (ENOMEM);
	bzero(privdata, sizeof(struct XXX));
	for (i = 0; i < XXX_NUM_DLCIS; i++) {
		privdata->channel[i].dlci = -2;
		privdata->channel[i].channel = i;
	}

	/* Call the 'generic' (ie, superclass) node constructor */
	if ((error = ng_make_node_common(&typestruct, nodep))) {
		FREE(privdata, M_NETGRAPH);
		return (error);
	}

	/* Link structs together; this counts as our one reference to *nodep */
	(*nodep)->private = privdata;
	privdata->node = *nodep;
	return (0);
}

/*
 * Give our ok for a hook to be added...
 * If we are not running this might kick a device into life.
 * Possibly decode information out of the hook name.
 * Add the hook's private info to the hook structure.
 * (if we had some). In this example, we assume that there is a
 * an array of structs, called 'channel' in the private info,
 * one for each active channel. The private
 * pointer of each hook points to the appropriate XXX_hookinfo struct
 * so that the source of an input packet is easily identified.
 * (a dlci is a frame relay channel)
 */
static int
ng_xxx_newhook(node_p node, hook_p hook, const char *name)
{
	const xxx_p xxxp = node->private;
	const char *cp;
	int dlci = 0;
	int chan;

#if 0
	/* Possibly start up the device if it's not already going */
	if ((xxxp->flags & SCF_RUNNING) == 0) {
		ng_xxx_start_hardware(xxxp);
	}
#endif

	/* Example of how one might use hooks with embedded numbers: All
	 * hooks start with 'dlci' and have a decimal trailing channel
	 * number up to 4 digits Use the leadin defined int he associated .h
	 * file. */
	if (strncmp(name,
	    NG_XXX_HOOK_DLCI_LEADIN, strlen(NG_XXX_HOOK_DLCI_LEADIN)) == 0) {
		char *eptr;

		cp = name + sizeof(NG_XXX_HOOK_DLCI_LEADIN);
		if (!isdigit(*cp) || (cp[0] == '0' && cp[1] != '\0'))
			return (EINVAL);
		dlci = (int)strtoul(cp, &eptr, 10);
		if (*eptr != '\0' || dlci < 0 || dlci > 1023)
			return (EINVAL);

		/* We have a dlci, now either find it, or allocate it */
		for (chan = 0; chan < XXX_NUM_DLCIS; chan++)
			if (xxxp->channel[chan].dlci == dlci)
				break;
		if (chan == XXX_NUM_DLCIS) {
			for (chan = 0; chan < XXX_NUM_DLCIS; chan++)
				if (xxxp->channel[chan].dlci != -2)
					continue;
			if (chan == XXX_NUM_DLCIS)
				return (ENOBUFS);
		}
		if (xxxp->channel[chan].hook != NULL)
			return (EADDRINUSE);
		hook->private = xxxp->channel + chan;
		xxxp->channel[chan].hook = hook;
		return (0);
	} else if (strcmp(name, NG_XXX_HOOK_DOWNSTREAM) == 0) {
		/* Example of simple predefined hooks. */
		/* do something specific to the downstream connection */
		xxxp->downstream_hook.hook = hook;
		hook->private = &xxxp->downstream_hook;
	} else if (strcmp(name, NG_XXX_HOOK_DEBUG) == 0) {
		/* do something specific to a debug connection */
		xxxp->debughook = hook;
		hook->private = NULL;
	} else
		return (EINVAL);	/* not a hook we know about */
	return(0);
}

/*
 * Get a netgraph control message.
 * Check it is one we understand. If needed, send a response.
 * We could save the address for an async action later, but don't here.
 * Always free the message.
 * The response should be in a malloc'd region that the caller can 'free'.
 * A response is not required.
 * Theoretically you could respond defferently to old message types if
 * the cookie in the header didn't match what we consider to be current
 * (so that old userland programs could continue to work).
 */
static int
ng_xxx_rcvmsg(node_p node,
	   struct ng_mesg *msg, const char *retaddr, struct ng_mesg **rptr)
{
	const xxx_p xxxp = node->private;
	struct ng_mesg *resp = NULL;
	int error = 0;

	/* Deal with message according to cookie and command */
	switch (msg->header.typecookie) {
	case NGM_XXX_COOKIE: 
		switch (msg->header.cmd) {
		case NGM_XXX_GET_STATUS:
		    {
			struct ngxxxstat *stats;

			NG_MKRESPONSE(resp, msg, sizeof(*stats), M_NOWAIT);
			if (!resp) {
				error = ENOMEM;
				break;
			}
			stats = (struct ngxxxstat *) resp->data;
			stats->packets_in = xxxp->packets_in;
			stats->packets_out = xxxp->packets_out;
			break;
		    }
		case NGM_XXX_SET_FLAG:
			if (msg->header.arglen != sizeof(u_int32_t)) {
				error = EINVAL;
				break;
			}
			xxxp->flags = *((u_int32_t *) msg->data);
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

	/* Take care of synchronous response, if any */
	if (rptr)
		*rptr = resp;
	else if (resp)
		FREE(resp, M_NETGRAPH);

	/* Free the message and return */
	FREE(msg, M_NETGRAPH);
	return(error);
}

/*
 * Receive data, and do something with it.
 * Possibly send it out on another link after processing.
 * Possibly do something different if it comes from different
 * hooks. the caller will never free m or meta, so
 * if we use up this data or abort we must free BOTH of these.
 *
 * If we want, we may decide to force this data to be queued and reprocessed
 * at the netgraph NETISR time. (at which time it will be entered using ng_xxx_rcvdataq().
 */
static int
ng_xxx_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	int dlci = -2;

	if (hook->private) {
		/* 
		 * If it's dlci 1023, requeue it so that it's handled
		 * at a lower priority. This is how a node decides to
		 * defer a data message.
		 */
		dlci = ((struct XXX_hookinfo *) hook->private)->dlci;
		if (dlci == 1023) {
			return(ng_queue_data(hook->peer, m, meta));
		}
	}
	return(ng_xxx_rcvdataq(hook, m, meta));
}

/*
 * Always accept the data. This version of rcvdata is called from the dequeueing routine.
 */
static int
ng_xxx_rcvdataq(hook_p hook, struct mbuf *m, meta_p meta)
{
	const xxx_p xxxp = hook->node->private;
	int chan = -2;
	int dlci = -2;
	int error;

	if (hook->private) {
		dlci = ((struct XXX_hookinfo *) hook->private)->dlci;
		chan = ((struct XXX_hookinfo *) hook->private)->channel;
		if (dlci != -1) {
			/* If received on a DLCI hook process for this
			 * channel and pass it to the downstream module.
			 * Normally one would add a multiplexing header at
			 * the front here */
			/* M_PREPEND(....)	; */
			/* mtod(m, xxxxxx)->dlci = dlci; */
			error = ng_send_data(xxxp->downstream_hook.hook,
			    m, meta);
			xxxp->packets_out++;
		} else {
			/* data came from the multiplexed link */
			dlci = 1;	/* get dlci from header */
			/* madjust(....) *//* chop off header */
			for (chan = 0; chan < XXX_NUM_DLCIS; chan++)
				if (xxxp->channel[chan].dlci == dlci)
					break;
			if (chan == XXX_NUM_DLCIS) {
				NG_FREE_DATA(m, meta);
				return (ENETUNREACH);
			}
			/* If we were called at splnet, use the following:
			 * NG_SEND_DATA(error, otherhook, m, meta); if this
			 * node is running at some SPL other than SPLNET
			 * then you should use instead: error =
			 * ng_queueit(otherhook, m, meta); m = NULL: meta =
			 * NULL; this queues the data using the standard
			 * NETISR system and schedules the data to be picked
			 * up again once the system has moved to SPLNET and
			 * the processing of the data can continue. after
			 * these are run 'm' and 'meta' should be considered
			 * as invalid and NG_SEND_DATA actually zaps them. */
			NG_SEND_DATA(error, xxxp->channel[chan].hook, m, meta);
			xxxp->packets_in++;
		}
	} else {
		/* It's the debug hook, throw it away.. */
		if (hook == xxxp->downstream_hook.hook)
			NG_FREE_DATA(m, meta);
	}
	return 0;
}

#if 0
/*
 * If this were a device node, the data may have been received in response
 * to some interrupt.
 * in which case it would probably look as follows:
 */
devintr()
{
	meta_p  meta = NULL;	/* whatever metadata we might imagine goes
				 * here */

	/* get packet from device and send on */
	m = MGET(blah blah)
	    error = ng_queueit(upstream, m, meta);	/* see note above in
							 * xxx_rcvdata() */
}

#endif				/* 0 */

/*
 * Do local shutdown processing..
 * If we are a persistant device, we might refuse to go away, and
 * we'd only remove our links and reset ourself.
 */
static int
ng_xxx_rmnode(node_p node)
{
	const xxx_p privdata = node->private;

	node->flags |= NG_INVALID;
	ng_cutlinks(node);
#ifndef PERSISTANT_NODE
	ng_unname(node);
	node->private = NULL;
	ng_unref(privdata->node);
	FREE(privdata, M_NETGRAPH);
#else
	privdata->packets_in = 0;		/* reset stats */
	privdata->packets_out = 0;
	node->flags &= ~NG_INVALID;		/* reset invalid flag */
#endif /* PERSISTANT_NODE */
	return (0);
}

/*
 * This is called once we've already connected a new hook to the other node.
 * It gives us a chance to balk at the last minute.
 */
static int
ng_xxx_connect(hook_p hook)
{
	/* be really amiable and just say "YUP that's OK by me! " */
	return (0);
}

/*
 * Dook disconnection
 *
 * For this type, removal of the last link destroys the node
 */
static int
ng_xxx_disconnect(hook_p hook)
{
	if (hook->private)
		((struct XXX_hookinfo *) (hook->private))->hook = NULL;
	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	return (0);
}

