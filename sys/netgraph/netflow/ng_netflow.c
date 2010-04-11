/*-
 * Copyright (c) 2004-2005 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 2001-2003 Roman V. Palagin <romanp@unshadow.net>
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
 * $SourceForge: ng_netflow.c,v 1.30 2004/09/05 11:37:43 glebius Exp $
 */

static const char rcs_id[] =
    "@(#) $FreeBSD$";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ctype.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>
#include <net/bpf.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_parse.h>
#include <netgraph/netgraph.h>
#include <netgraph/netflow/netflow.h>
#include <netgraph/netflow/ng_netflow.h>

/* Netgraph methods */
static ng_constructor_t	ng_netflow_constructor;
static ng_rcvmsg_t	ng_netflow_rcvmsg;
static ng_close_t	ng_netflow_close;
static ng_shutdown_t	ng_netflow_rmnode;
static ng_newhook_t	ng_netflow_newhook;
static ng_rcvdata_t	ng_netflow_rcvdata;
static ng_disconnect_t	ng_netflow_disconnect;

/* Parse type for struct ng_netflow_info */
static const struct ng_parse_struct_field ng_netflow_info_type_fields[]
	= NG_NETFLOW_INFO_TYPE;
static const struct ng_parse_type ng_netflow_info_type = {
	&ng_parse_struct_type,
	&ng_netflow_info_type_fields
};

/*  Parse type for struct ng_netflow_ifinfo */
static const struct ng_parse_struct_field ng_netflow_ifinfo_type_fields[]
	= NG_NETFLOW_IFINFO_TYPE;
static const struct ng_parse_type ng_netflow_ifinfo_type = {
	&ng_parse_struct_type,
	&ng_netflow_ifinfo_type_fields
};

/* Parse type for struct ng_netflow_setdlt */
static const struct ng_parse_struct_field ng_netflow_setdlt_type_fields[]
	= NG_NETFLOW_SETDLT_TYPE;
static const struct ng_parse_type ng_netflow_setdlt_type = {
	&ng_parse_struct_type,
	&ng_netflow_setdlt_type_fields
};

/* Parse type for ng_netflow_setifindex */
static const struct ng_parse_struct_field ng_netflow_setifindex_type_fields[]
	= NG_NETFLOW_SETIFINDEX_TYPE;
static const struct ng_parse_type ng_netflow_setifindex_type = {
	&ng_parse_struct_type,
	&ng_netflow_setifindex_type_fields
};

/* Parse type for ng_netflow_settimeouts */
static const struct ng_parse_struct_field ng_netflow_settimeouts_type_fields[]
	= NG_NETFLOW_SETTIMEOUTS_TYPE;
static const struct ng_parse_type ng_netflow_settimeouts_type = {
	&ng_parse_struct_type,
	&ng_netflow_settimeouts_type_fields
};

/* Parse type for ng_netflow_setconfig */
static const struct ng_parse_struct_field ng_netflow_setconfig_type_fields[]
	= NG_NETFLOW_SETCONFIG_TYPE;
static const struct ng_parse_type ng_netflow_setconfig_type = {
	&ng_parse_struct_type,
	&ng_netflow_setconfig_type_fields
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_netflow_cmds[] = {
       {
	 NGM_NETFLOW_COOKIE,
	 NGM_NETFLOW_INFO,
	 "info",
	 NULL,
	 &ng_netflow_info_type
       },
       {
	NGM_NETFLOW_COOKIE,
	NGM_NETFLOW_IFINFO,
	"ifinfo",
	&ng_parse_uint16_type,
	&ng_netflow_ifinfo_type
       },
       {
	NGM_NETFLOW_COOKIE,
	NGM_NETFLOW_SETDLT,
	"setdlt",
	&ng_netflow_setdlt_type,
	NULL
       },
       {
	NGM_NETFLOW_COOKIE,
	NGM_NETFLOW_SETIFINDEX,
	"setifindex",
	&ng_netflow_setifindex_type,
	NULL
       },
       {
	NGM_NETFLOW_COOKIE,
	NGM_NETFLOW_SETTIMEOUTS,
	"settimeouts",
	&ng_netflow_settimeouts_type,
	NULL
       },
       {
	NGM_NETFLOW_COOKIE,
	NGM_NETFLOW_SETCONFIG,
	"setconfig",
	&ng_netflow_setconfig_type,
	NULL
       },
       { 0 }
};


/* Netgraph node type descriptor */
static struct ng_type ng_netflow_typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_NETFLOW_NODE_TYPE,
	.constructor =	ng_netflow_constructor,
	.rcvmsg =	ng_netflow_rcvmsg,
	.close =	ng_netflow_close,
	.shutdown =	ng_netflow_rmnode,
	.newhook =	ng_netflow_newhook,
	.rcvdata =	ng_netflow_rcvdata,
	.disconnect =	ng_netflow_disconnect,
	.cmdlist =	ng_netflow_cmds,
};
NETGRAPH_INIT(netflow, &ng_netflow_typestruct);

/* Called at node creation */
static int
ng_netflow_constructor(node_p node)
{
	priv_p priv;
	int error = 0, i;

	/* Initialize private data */
	priv = malloc(sizeof(*priv), M_NETGRAPH, M_NOWAIT);
	if (priv == NULL)
		return (ENOMEM);
	bzero(priv, sizeof(*priv));

	/* Make node and its data point at each other */
	NG_NODE_SET_PRIVATE(node, priv);
	priv->node = node;

	/* Initialize timeouts to default values */
	priv->info.nfinfo_inact_t = INACTIVE_TIMEOUT;
	priv->info.nfinfo_act_t = ACTIVE_TIMEOUT;

	/* Set default config */
	for (i = 0; i < NG_NETFLOW_MAXIFACES; i++)
		priv->ifaces[i].info.conf = NG_NETFLOW_CONF_INGRESS;

	/* Initialize callout handle */
	callout_init(&priv->exp_callout, CALLOUT_MPSAFE);

	/* Allocate memory and set up flow cache */
	if ((error = ng_netflow_cache_init(priv)))
		return (error);

	return (0);
}

/*
 * ng_netflow supports two hooks: data and export.
 * Incoming traffic is expected on data, and expired
 * netflow datagrams are sent to export.
 */
static int
ng_netflow_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	if (strncmp(name, NG_NETFLOW_HOOK_DATA,	/* an iface hook? */
	    strlen(NG_NETFLOW_HOOK_DATA)) == 0) {
		iface_p iface;
		int ifnum = -1;
		const char *cp;
		char *eptr;

		cp = name + strlen(NG_NETFLOW_HOOK_DATA);
		if (!isdigit(*cp) || (cp[0] == '0' && cp[1] != '\0'))
			return (EINVAL);

		ifnum = (int)strtoul(cp, &eptr, 10);
		if (*eptr != '\0' || ifnum < 0 || ifnum >= NG_NETFLOW_MAXIFACES)
			return (EINVAL);

		/* See if hook is already connected */
		if (priv->ifaces[ifnum].hook != NULL)
			return (EISCONN);

		iface = &priv->ifaces[ifnum];

		/* Link private info and hook together */
		NG_HOOK_SET_PRIVATE(hook, iface);
		iface->hook = hook;

		/*
		 * In most cases traffic accounting is done on an
		 * Ethernet interface, so default data link type
		 * will be DLT_EN10MB.
		 */
		iface->info.ifinfo_dlt = DLT_EN10MB;

	} else if (strncmp(name, NG_NETFLOW_HOOK_OUT,
	    strlen(NG_NETFLOW_HOOK_OUT)) == 0) {
		iface_p iface;
		int ifnum = -1;
		const char *cp;
		char *eptr;

		cp = name + strlen(NG_NETFLOW_HOOK_OUT);
		if (!isdigit(*cp) || (cp[0] == '0' && cp[1] != '\0'))
			return (EINVAL);

		ifnum = (int)strtoul(cp, &eptr, 10);
		if (*eptr != '\0' || ifnum < 0 || ifnum >= NG_NETFLOW_MAXIFACES)
			return (EINVAL);

		/* See if hook is already connected */
		if (priv->ifaces[ifnum].out != NULL)
			return (EISCONN);

		iface = &priv->ifaces[ifnum];

		/* Link private info and hook together */
		NG_HOOK_SET_PRIVATE(hook, iface);
		iface->out = hook;

	} else if (strcmp(name, NG_NETFLOW_HOOK_EXPORT) == 0) {

		if (priv->export != NULL)
			return (EISCONN);

		priv->export = hook;

		/* Exporter is ready. Let's schedule expiry. */
		callout_reset(&priv->exp_callout, (1*hz), &ng_netflow_expire,
		    (void *)priv);
	} else
		return (EINVAL);

	return (0);
}

/* Get a netgraph control message. */
static int
ng_netflow_rcvmsg (node_p node, item_p item, hook_p lasthook)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ng_mesg *resp = NULL;
	int error = 0;
	struct ng_mesg *msg;

	NGI_GET_MSG(item, msg);

	/* Deal with message according to cookie and command */
	switch (msg->header.typecookie) {
	case NGM_NETFLOW_COOKIE:
		switch (msg->header.cmd) {
		case NGM_NETFLOW_INFO:
		{
			struct ng_netflow_info *i;

			NG_MKRESPONSE(resp, msg, sizeof(struct ng_netflow_info),
			    M_NOWAIT);
			i = (struct ng_netflow_info *)resp->data;
			ng_netflow_copyinfo(priv, i);

			break;
		}
		case NGM_NETFLOW_IFINFO:
		{
			struct ng_netflow_ifinfo *i;
			const uint16_t *index;

			if (msg->header.arglen != sizeof(uint16_t))
				 ERROUT(EINVAL);

			index  = (uint16_t *)msg->data;
			if (*index >= NG_NETFLOW_MAXIFACES)
				ERROUT(EINVAL);

			/* connected iface? */
			if (priv->ifaces[*index].hook == NULL)
				 ERROUT(EINVAL);

			NG_MKRESPONSE(resp, msg,
			     sizeof(struct ng_netflow_ifinfo), M_NOWAIT);
			i = (struct ng_netflow_ifinfo *)resp->data;
			memcpy((void *)i, (void *)&priv->ifaces[*index].info,
			    sizeof(priv->ifaces[*index].info));

			break;
		}
		case NGM_NETFLOW_SETDLT:
		{
			struct ng_netflow_setdlt *set;
			struct ng_netflow_iface *iface;

			if (msg->header.arglen != sizeof(struct ng_netflow_setdlt))
				ERROUT(EINVAL);

			set = (struct ng_netflow_setdlt *)msg->data;
			if (set->iface >= NG_NETFLOW_MAXIFACES)
				ERROUT(EINVAL);
			iface = &priv->ifaces[set->iface];

			/* connected iface? */
			if (iface->hook == NULL)
				ERROUT(EINVAL);

			switch (set->dlt) {
			case	DLT_EN10MB:
				iface->info.ifinfo_dlt = DLT_EN10MB;
				break;
			case	DLT_RAW:
				iface->info.ifinfo_dlt = DLT_RAW;
				break;
			default:
				ERROUT(EINVAL);
			}
			break;
		}
		case NGM_NETFLOW_SETIFINDEX:
		{
			struct ng_netflow_setifindex *set;
			struct ng_netflow_iface *iface;

			if (msg->header.arglen != sizeof(struct ng_netflow_setifindex))
				ERROUT(EINVAL);

			set = (struct ng_netflow_setifindex *)msg->data;
			if (set->iface >= NG_NETFLOW_MAXIFACES)
				ERROUT(EINVAL);
			iface = &priv->ifaces[set->iface];

			/* connected iface? */
			if (iface->hook == NULL)
				ERROUT(EINVAL);

			iface->info.ifinfo_index = set->index;

			break;
		}
		case NGM_NETFLOW_SETTIMEOUTS:
		{
			struct ng_netflow_settimeouts *set;

			if (msg->header.arglen != sizeof(struct ng_netflow_settimeouts))
				ERROUT(EINVAL);

			set = (struct ng_netflow_settimeouts *)msg->data;

			priv->info.nfinfo_inact_t = set->inactive_timeout;
			priv->info.nfinfo_act_t = set->active_timeout;

			break;
		}
		case NGM_NETFLOW_SETCONFIG:
		{
			struct ng_netflow_setconfig *set;

			if (msg->header.arglen != sizeof(struct ng_netflow_setconfig))
				ERROUT(EINVAL);

			set = (struct ng_netflow_setconfig *)msg->data;

			if (set->iface >= NG_NETFLOW_MAXIFACES)
				ERROUT(EINVAL);
			
			priv->ifaces[set->iface].info.conf = set->conf;
	
			break;
		}
		case NGM_NETFLOW_SHOW:
		{
			uint32_t *last;

			if (msg->header.arglen != sizeof(uint32_t))
				ERROUT(EINVAL);

			last = (uint32_t *)msg->data;

			NG_MKRESPONSE(resp, msg, NGRESP_SIZE, M_NOWAIT);

			if (!resp)
				ERROUT(ENOMEM);

			error = ng_netflow_flow_show(priv, *last, resp);

			break;
		}
		default:
			ERROUT(EINVAL);		/* unknown command */
			break;
		}
		break;
	default:
		ERROUT(EINVAL);		/* incorrect cookie */
		break;
	}

	/*
	 * Take care of synchronous response, if any.
	 * Free memory and return.
	 */
done:
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);

	return (error);
}

/* Receive data on hook. */
static int
ng_netflow_rcvdata (hook_p hook, item_p item)
{
	const node_p node = NG_HOOK_NODE(hook);
	const priv_p priv = NG_NODE_PRIVATE(node);
	const iface_p iface = NG_HOOK_PRIVATE(hook);
	hook_p out;
	struct mbuf *m = NULL;
	struct ip *ip;
	struct m_tag *mtag;
	int pullup_len = 0;
	int error = 0, bypass = 0;
	unsigned int src_if_index;

	if (hook == priv->export) {
		/*
		 * Data arrived on export hook.
		 * This must not happen.
		 */
		log(LOG_ERR, "ng_netflow: incoming data on export hook!\n");
		ERROUT(EINVAL);
	};

	if (hook == iface->hook) {
		if ((iface->info.conf & NG_NETFLOW_CONF_INGRESS) == 0)
			bypass = 1;
		out = iface->out;
	} else if (hook == iface->out) {
		if ((iface->info.conf & NG_NETFLOW_CONF_EGRESS) == 0)
			bypass = 1;
		out = iface->hook;
	} else 
		ERROUT(EINVAL);
	
	if ((!bypass) &&
	    (iface->info.conf & (NG_NETFLOW_CONF_ONCE | NG_NETFLOW_CONF_THISONCE))) {
		mtag = m_tag_locate(NGI_M(item), MTAG_NETFLOW,
		    MTAG_NETFLOW_CALLED, NULL);
		while (mtag != NULL) {
			if ((iface->info.conf & NG_NETFLOW_CONF_ONCE) ||
			    ((ng_ID_t *)(mtag + 1))[0] == NG_NODE_ID(node)) {
				bypass = 1;
				break;
			}
			mtag = m_tag_locate(NGI_M(item), MTAG_NETFLOW,
			    MTAG_NETFLOW_CALLED, mtag);
		}
	}
	
	if (bypass) {
		if (out == NULL)
			ERROUT(ENOTCONN);

		NG_FWD_ITEM_HOOK(error, item, out);
		return (error);
	}
	
	if (iface->info.conf & (NG_NETFLOW_CONF_ONCE | NG_NETFLOW_CONF_THISONCE)) {
		mtag = m_tag_alloc(MTAG_NETFLOW, MTAG_NETFLOW_CALLED,
		    sizeof(ng_ID_t), M_NOWAIT);
		if (mtag) {
			((ng_ID_t *)(mtag + 1))[0] = NG_NODE_ID(node);
			m_tag_prepend(NGI_M(item), mtag);
		}
	}

	NGI_GET_M(item, m);

	/* Increase counters. */
	iface->info.ifinfo_packets++;

	/*
	 * Depending on interface data link type and packet contents
	 * we pullup enough data, so that ng_netflow_flow_add() does not
	 * need to know about mbuf at all. We keep current length of data
	 * needed to be contiguous in pullup_len. mtod() is done at the
	 * very end one more time, since m can had changed after pulluping.
	 *
	 * In case of unrecognized data we don't return error, but just
	 * pass data to downstream hook, if it is available.
	 */

#define	M_CHECK(length)	do {					\
	pullup_len += length;					\
	if ((m)->m_pkthdr.len < (pullup_len)) {			\
		error = EINVAL;					\
		goto bypass;					\
	} 							\
	if ((m)->m_len < (pullup_len) &&			\
	   (((m) = m_pullup((m),(pullup_len))) == NULL)) {	\
		error = ENOBUFS;				\
		goto done;					\
	}							\
} while (0)

	switch (iface->info.ifinfo_dlt) {
	case DLT_EN10MB:	/* Ethernet */
	    {
		struct ether_header *eh;
		uint16_t etype;

		M_CHECK(sizeof(struct ether_header));
		eh = mtod(m, struct ether_header *);

		/* Make sure this is IP frame. */
		etype = ntohs(eh->ether_type);
		switch (etype) {
		case ETHERTYPE_IP:
			M_CHECK(sizeof(struct ip));
			eh = mtod(m, struct ether_header *);
			ip = (struct ip *)(eh + 1);
			break;
		case ETHERTYPE_VLAN:
		    {
			struct ether_vlan_header *evh;

			M_CHECK(sizeof(struct ether_vlan_header) -
			    sizeof(struct ether_header));
			evh = mtod(m, struct ether_vlan_header *);
			if (ntohs(evh->evl_proto) == ETHERTYPE_IP) {
				M_CHECK(sizeof(struct ip));
				ip = (struct ip *)(evh + 1);
				break;
			}
		    }
		default:
			goto bypass;	/* pass this frame */
		}
		break;
	    }
	case DLT_RAW:		/* IP packets */
		M_CHECK(sizeof(struct ip));
		ip = mtod(m, struct ip *);
		break;
	default:
		goto bypass;
		break;
	}

	if ((ip->ip_off & htons(IP_OFFMASK)) == 0) {
		/*
		 * In case of IP header with options, we haven't pulled
		 * up enough, yet.
		 */
		pullup_len += (ip->ip_hl << 2) - sizeof(struct ip);

		switch (ip->ip_p) {
		case IPPROTO_TCP:
			M_CHECK(sizeof(struct tcphdr));
			break;
		case IPPROTO_UDP:
			M_CHECK(sizeof(struct udphdr));
			break;
		}
	}

	switch (iface->info.ifinfo_dlt) {
	case DLT_EN10MB:
	    {
		struct ether_header *eh;

		eh = mtod(m, struct ether_header *);
		switch (ntohs(eh->ether_type)) {
		case ETHERTYPE_IP:
			ip = (struct ip *)(eh + 1);
			break;
		case ETHERTYPE_VLAN:
		    {
			struct ether_vlan_header *evh;

			evh = mtod(m, struct ether_vlan_header *);
			ip = (struct ip *)(evh + 1);
			break;
		     }
		default:
			panic("ng_netflow entered deadcode");
		}
		break;
	     }
	case DLT_RAW:
		ip = mtod(m, struct ip *);
		break;
	default:
		panic("ng_netflow entered deadcode");
	}

#undef	M_CHECK

	/* Determine packet input interface. Prefer configured. */
	src_if_index = 0;
	if (hook == iface->out || iface->info.ifinfo_index == 0) {
		if (m->m_pkthdr.rcvif != NULL)
			src_if_index = m->m_pkthdr.rcvif->if_index;
	} else
		src_if_index = iface->info.ifinfo_index;

	error = ng_netflow_flow_add(priv, ip, src_if_index);

bypass:
	if (out != NULL) {
		/* XXX: error gets overwritten here */
		NG_FWD_NEW_DATA(error, item, out, m);
		return (error);
	}
done:
	if (item)
		NG_FREE_ITEM(item);
	if (m)
		NG_FREE_M(m);

	return (error);	
}

/* We will be shut down in a moment */
static int
ng_netflow_close(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	callout_drain(&priv->exp_callout);
	ng_netflow_cache_flush(priv);

	return (0);
}

/* Do local shutdown processing. */
static int
ng_netflow_rmnode(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(priv->node);

	free(priv, M_NETGRAPH);

	return (0);
}

/* Hook disconnection. */
static int
ng_netflow_disconnect(hook_p hook)
{
	node_p node = NG_HOOK_NODE(hook);
	priv_p priv = NG_NODE_PRIVATE(node);
	iface_p iface = NG_HOOK_PRIVATE(hook);

	if (iface != NULL) {
		if (iface->hook == hook)
			iface->hook = NULL;
		if (iface->out == hook)
			iface->out = NULL;
	}

	/* if export hook disconnected stop running expire(). */
	if (hook == priv->export) {
		callout_drain(&priv->exp_callout);
		priv->export = NULL;
	}

	/* Removal of the last link destroys the node. */
	if (NG_NODE_NUMHOOKS(node) == 0)
		ng_rmnode_self(node);

	return (0);
}
