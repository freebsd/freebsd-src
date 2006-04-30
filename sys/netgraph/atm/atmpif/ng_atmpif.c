/*-
 * Copyright 2003 Harti Brandt
 * Copyright 2003 Vincent Jardin
 * 	All rights reserved.
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
 */

/*
 * ATM Virtal Adapter Support
 * --------------------------
 *
 * Loadable kernel module and netgraph support
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>

#include <vm/uma.h>

#include <net/if.h>

#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_var.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/atm/ng_atmpif.h>
#include <netgraph/atm/atmpif/ng_atmpif_var.h>

#ifdef NG_SEPARATE_MALLOC
MALLOC_DEFINE(M_NETGRAPH_ATMPIF, "netgraph_vatmpif",
    "netgraph HARP virtual Physical Interface");
#else
#define M_NETGRAPH_ATMPIF M_NETGRAPH
#endif

/*
 * Local definitions
 */

/*
 * Protocol header
 */
struct vatmpif_header {
	/* The cell header (minus the HEC) is contained in the least-significant
	 * 32-bits of a word.
	 */
	uint32_t	cellhdr;	/* Stored in network order */
	/* Let's use cellhdr = htonl(ATM_HDR_SET(vpi, vci, pt, clp))
	 * and vpi = ATM_HDR_GET_VPI(ntohl(cellhdr))
	 *     vci = ATM_HDR_GET_VCI(ntohl(cellhdr))
	 *     pt  = ATM_HDR_GET_PT (ntohl(cellhdr))
	 *     clp = ATM_HDR_GET_CLP(ntohl(cellhdr))
	 */
	int32_t		seq;	/* sequence number in network byte order */
	uint64_t	cookie;	/* optional field */
	uint8_t		aal;	/* AAL */
	uint8_t		__pad[3];
};

/*
 * Local functions
 */

/* Parse type for a MAC address */
static ng_parse_t		ng_macaddr_parse;
static ng_unparse_t		ng_macaddr_unparse;
const struct ng_parse_type ng_mac_addr_type = {
	parse: ng_macaddr_parse,
	unparse: ng_macaddr_unparse,
};


/* Parse type for struct ng_atmpif_config */
static const struct ng_parse_struct_field
    ng_atmpif_config_type_fields[] = NG_ATMPIF_CONFIG_TYPE_INFO;
static const struct ng_parse_type ng_atmpif_config_type = {
	&ng_parse_struct_type,
	&ng_atmpif_config_type_fields,
};

/* Parse type for struct ng_atmpif_link_status */
static const struct ng_parse_struct_field
    ng_atmpif_link_status_type_fields[] = NG_ATMPIF_LINK_STATUS_TYPE_INFO;
static const struct ng_parse_type ng_atmpif_link_status_type = {
	&ng_parse_struct_type,
	&ng_atmpif_link_status_type_fields,
};

/* Parse type for struct ng_atmpif_stats */
static const struct ng_parse_struct_field
    ng_atmpif_stats_type_fields[] = NG_ATMPIF_STATS_TYPE_INFO;
static const struct ng_parse_type ng_atmpif_stats_type = {
	&ng_parse_struct_type,
	&ng_atmpif_stats_type_fields,
};

static const struct ng_cmdlist ng_atmpif_cmdlist[] = {
	{
	  NGM_ATMPIF_COOKIE,
	  NGM_ATMPIF_SET_CONFIG,
	  "setconfig",
	  mesgType: &ng_atmpif_config_type,
	  respType: NULL
	},
	{
	  NGM_ATMPIF_COOKIE,
	  NGM_ATMPIF_GET_CONFIG,
	  "getconfig",
	  mesgType: NULL,
	  respType: &ng_atmpif_config_type
	},
	{
	  NGM_ATMPIF_COOKIE,
	  NGM_ATMPIF_GET_LINK_STATUS,
	  "getlinkstatus",
	  mesgType: NULL,
	  respType: &ng_atmpif_link_status_type
	},
	{
	  NGM_ATMPIF_COOKIE,
	  NGM_ATMPIF_GET_STATS,
	  "getstats",
	  mesgType: NULL,
	  respType: &ng_atmpif_stats_type
	},
	{
	  NGM_ATMPIF_COOKIE,
	  NGM_ATMPIF_CLR_STATS,
	  "clrstats",
	  mesgType: NULL,
	  respType: NULL
	},
	{
	  NGM_ATMPIF_COOKIE,
	  NGM_ATMPIF_GETCLR_STATS,
	  "getclrstats",
	  mesgType: NULL,
	  respType: &ng_atmpif_stats_type
	},

	{ 0 }
};

uma_zone_t vatmpif_nif_zone;
uma_zone_t vatmpif_vcc_zone;

/*
 * Netgraph node methods
 */
static ng_constructor_t ng_atmpif_constructor;
static ng_rcvmsg_t      ng_atmpif_rcvmsg;
static ng_shutdown_t    ng_atmpif_rmnode;
static ng_newhook_t     ng_atmpif_newhook;
static ng_rcvdata_t     ng_atmpif_rcvdata;
static ng_disconnect_t  ng_atmpif_disconnect;
static int		ng_atmpif_mod_event(module_t, int, void *);

/*
 * Node type descriptor
 */
static struct ng_type ng_atmpif_typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_ATMPIF_NODE_TYPE,
	.mod_event =	ng_atmpif_mod_event,
	.constructor =	ng_atmpif_constructor,
	.rcvmsg =	ng_atmpif_rcvmsg,
	.shutdown =	ng_atmpif_rmnode,
	.newhook =	ng_atmpif_newhook,
	.rcvdata =	ng_atmpif_rcvdata,
	.disconnect =	ng_atmpif_disconnect,
	.cmdlist =	ng_atmpif_cmdlist,
};
NETGRAPH_INIT(atmpif, &ng_atmpif_typestruct);

/******************************************************************
		    NETGRAPH NODE METHODS
******************************************************************/

/*
 * Node constructor
 *
 * Called at splnet()
 */
static int
ng_atmpif_constructor(node_p nodep)
{
	priv_p priv;

	/*
	 * Allocate and initialize private info
	 */
	priv = malloc(sizeof(*priv), M_NETGRAPH_ATMPIF, M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return (ENOMEM);

	priv->conf.debug = 0x00;
	priv->conf.pcr = ATM_PCR_OC3C;
	priv->conf.macaddr.ma_data[0] = 0x02; /* XXX : non unique bit */
	priv->conf.macaddr.ma_data[1] = 0x09; /* XXX */
	priv->conf.macaddr.ma_data[2] = 0xc0; /* XXX */
	priv->conf.macaddr.ma_data[3] = (u_char)((random() & 0xff0000) >> 16);
	priv->conf.macaddr.ma_data[4] = (u_char)((random() & 0x00ff00) >> 8);
	priv->conf.macaddr.ma_data[5] = (u_char)((random() & 0x0000ff) >> 0);

	NG_NODE_SET_PRIVATE(nodep, priv);
	priv->node = nodep;

	/* Done */
	return (0);
}

/*
 * Method for attaching a new hook
 * A hook is a virtual ATM link.
 */
static int
ng_atmpif_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	/*
	 * Check for a link hook
	 */
	if (strcmp(name, NG_ATMPIF_HOOK_LINK) == 0) {
		int error;

		/*
		 * Do not create twice a link hook
		 */
		if (priv->link != NULL)
			return (EEXIST);

		priv->link = malloc(sizeof(*priv->link),
		    M_NETGRAPH_ATMPIF, M_NOWAIT | M_ZERO);
		if (priv->link == NULL)
			return (ENOMEM);

		/*
		 * Register as an HARP device
		 */
		if ((error = vatmpif_harp_attach(node))) {
			free(priv->link, M_NETGRAPH_ATMPIF);
			priv->link = NULL;
			return (error);
		}

		priv->link->hook = hook;
		return (0);
	}

	/* Unknown hook name */
	return (EINVAL);
}

/*
 * Receive a control message from ngctl or the netgraph's API
 */
static int
ng_atmpif_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ng_mesg *msg;
	struct ng_mesg *resp = NULL;
	int error = 0;

	NGI_GET_MSG(item, msg); 

	switch (msg->header.typecookie) {
	case NGM_ATMPIF_COOKIE:
		switch (msg->header.cmd) {
		case NGM_ATMPIF_GET_CONFIG:
		    {
			struct ng_vatmpif_config *conf;

			NG_MKRESPONSE(resp, msg,
			    sizeof(struct ng_vatmpif_config), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			conf = (struct ng_vatmpif_config *)resp->data;
			*conf = priv->conf;	/* no sanity checking needed */
			break;
		    }
		case NGM_ATMPIF_SET_CONFIG:
		    {
			struct ng_vatmpif_config *conf;

			if (msg->header.arglen != sizeof(*conf)) {
				error = EINVAL;
				break;
			}
			conf = (struct ng_vatmpif_config *)msg->data;
			priv->conf = *conf;
			break;
		    }
		case NGM_ATMPIF_GET_LINK_STATUS:
		    {
			struct ng_vatmpif_hook *link;
			struct ng_atmpif_link_status *status;

			if ((link = priv->link) == NULL) {
				error = ENOTCONN;
				break;
			}

			NG_MKRESPONSE(resp, msg, sizeof(*status), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			status = (struct ng_atmpif_link_status *)resp->data;
			status->InSeq = link->InSeq;
			status->OutSeq = link->OutSeq;
			status->cur_pcr = link->cur_pcr;
			break;
		    }
		case NGM_ATMPIF_GET_STATS:
		case NGM_ATMPIF_CLR_STATS:
		case NGM_ATMPIF_GETCLR_STATS:
		    {
			struct ng_vatmpif_hook *link;

			if ((link = priv->link) == NULL) {
				error = ENOTCONN;
				break;
			}

			/* Get/clear stats */
			if (msg->header.cmd != NGM_ATMPIF_CLR_STATS) {
				NG_MKRESPONSE(resp, msg,
				    sizeof(link->stats), M_NOWAIT);
				if (resp == NULL) {
					error = ENOMEM;
					break;
				}
				bcopy(&link->stats,
				    resp->data, sizeof(link->stats));
			}
			if (msg->header.cmd != NGM_ATMPIF_GET_STATS)
				bzero(&link->stats, sizeof(link->stats));
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
 * Hook disconnection.
 * It shutdown the virtual ATM link however the node is kept.
 */
static int
ng_atmpif_disconnect(hook_p hook)
{
	const node_p node = NG_HOOK_NODE(hook);
	const priv_p priv = NG_NODE_PRIVATE(node);

	/*
	 * Deregister from the HARP stack
	 */
	vatmpif_harp_detach(node);

	/*
	 * Free associated link information
	 */
	KASSERT(priv->link != NULL, ("%s: no link", __func__));
	FREE(priv->link, M_NETGRAPH_ATMPIF);
	priv->link = NULL;

	/* Shutdown the physical interface */
	priv->vu_pif.pif_flags &= ~PIF_UP;

	/* No more hooks, however I prefer to keep the node
	 * instead of going away
	 * However, if we are interested in removing it, let's
	 * call ng_rmnode(hook->node); here.
	 */
	return (0);
}

/*
 * Shutdown node
 *
 * Free the private data.
 */
static int
ng_atmpif_rmnode(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);

	/* Free private data */
	FREE(priv, M_NETGRAPH_ATMPIF);
	NG_NODE_SET_PRIVATE(node, NULL);

	/* Unref node */
	NG_NODE_UNREF(node);

	return (0);
}

/*
 * Receive data
 *
 * Then vatmpif_harp_recv_drain will schedule a call into the kernel
 * to process the atm_intrq.
 * It means that it should be processing at splimp() if
 * the node was a regular hw driver.
 */
static int
ng_atmpif_rcvdata(hook_p hook, item_p item)
{
	const node_p node = NG_HOOK_NODE(hook);
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct vatmpif_header *h;
	struct vatmpif_header hdrbuf;
	int error = 0;
	struct mbuf *m;

	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);

	/* Is the Physical Interface UP ? */
	if (!(priv->vu_pif.pif_flags & PIF_UP)) {
		log(LOG_ERR, "%s%d: down while %s",
		    priv->vu_pif.pif_name, priv->vu_pif.pif_unit, __func__);
		error = ENETDOWN;
		goto drop;
	}

	/* Sanity check header length */
	if (m->m_pkthdr.len < sizeof(*h)) {
		priv->link->stats.hva_st_ng.ng_badpdu++;
		error = EINVAL;
		goto drop;
	}

	/* Get the Virtual ATM Physical Interface header */
	if (m->m_len >= sizeof(*h)) {		/* the common case */
		h = mtod(m, struct vatmpif_header *);
	} else {
		m_copydata(m, 0, sizeof(*h), (caddr_t)&hdrbuf);
		h = &hdrbuf;	/* allocated on the stack */
	}

	/*
	 * Consume the vatmpif header
	 */
	m_adj(m, sizeof(*h));

	/*
	 * Parse the header h
	 */

	/*
	 * duplication and out of order test.
	 *
	 *  . let's SEQ_MAX be the highest sequence number
	 *  . let's assume that h->seq = SEQ_MAX, (1)
	 */
	if (ntohl(h->seq) < priv->link->InSeq) {
		/*  . is false due to (1) */
		/* duplicate or out of order */
		priv->link->stats.hva_st_ng.ng_errseq++;
		error = EINVAL;
		goto drop;
	}
	/*  . then the mbuf is not dropped */

	/* PDUs have been lost ?? */
	if (priv->link->InSeq < ntohl(h->seq)) {
		/*  . it is true only if a PDU has been lost,
		 *  . else due to (1) priv->link->InSeq is
		 *  . already equal to SEQ_MAX. 
		 */
		priv->link->stats.hva_st_ng.ng_lostpdu++;
		priv->link->InSeq = ntohl(h->seq);
	}

	/* Save the sequence number */
	priv->link->InSeq = ntohl(h->seq) + 1;
	/*  . it leads to InSeq = SEQ_MAX + 1 = SEQ_MIN */

	/*  . it means that InSeq is always the next intended
	 *  . sequence number if none is lost, doesn't it ?
	 */

	/*
	 * Send the packet to the stack.
	 */
	priv->link->stats.hva_st_ng.ng_rx_pdu++;
	error = vatmpif_harp_recv_drain(priv, m,
	    ATM_HDR_GET_VPI(ntohl(h->cellhdr)),
	    ATM_HDR_GET_VCI(ntohl(h->cellhdr)),
	    ATM_HDR_GET_PT (ntohl(h->cellhdr)),
	    ATM_HDR_GET_CLP(ntohl(h->cellhdr)), h->aal);

	return (error);

drop:
	m_freem(m);
	return (error);
}

/*
 * Transmit data. Called by the HARP's outpout function. You should
 * notice that the return value is not returned upward by the HARP
 * stack. It is only used in order to update the stats.
 */
int
ng_atmpif_transmit(const priv_p priv, struct mbuf *m,
    uint8_t vpi, uint16_t vci, uint8_t pt, uint8_t clp, Vatmpif_aal aal)
{
	struct vatmpif_header *h;
	int error = 0;

	/* Is the Physical Interface UP ? */
	if (!(priv->vu_pif.pif_flags & PIF_UP)) {
		log(LOG_ERR, "%s%d: down while %s",
		    priv->vu_pif.pif_name, priv->vu_pif.pif_unit, __func__);
		error = ENETDOWN;
		goto drop;
	}

	/* If the hook is not connected, free the mbuf */
	if (priv->link == NULL) {
		log(LOG_ERR, "%s%d: no hook while %s",
		    priv->vu_pif.pif_name, priv->vu_pif.pif_unit, __func__);
		error = ENETDOWN;
		goto drop;
	}

	M_PREPEND(m, sizeof(*h), M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto drop;
	}
	m = m_pullup(m, sizeof(*h));
	if (m == NULL) {
		error = ENOBUFS;
		goto drop;
	}
	h = mtod(m, struct vatmpif_header *);

	/* htonl is linear */
	h->cellhdr  = htonl(ATM_HDR_SET_VPI(vpi));
	h->cellhdr += htonl(ATM_HDR_SET_VCI(vci));
	h->cellhdr += htonl(ATM_HDR_SET_PT (pt));
	h->cellhdr += htonl(ATM_HDR_SET_CLP(clp));
	h->aal      = aal;
	priv->link->OutSeq++;
	h->seq      = htonl(priv->link->OutSeq);
	h->cookie   = 0;

	if (IS_VATMPIF_DEBUG_PACKET(priv))
		atm_pdu_print(m, __func__);

	/* Send it out to the "link" hook */
	priv->link->stats.hva_st_ng.ng_tx_pdu++;
	NG_SEND_DATA_ONLY(error, priv->link->hook, m);

	return (error);

drop:
	if (m != NULL)
		m_freem(m);
	return (error);
}

/******************************************************************
 			MAC Address parser
 *****************************************************************/
static int
ng_macaddr_parse(const struct ng_parse_type *type, const char *s,
    int *const off, const u_char *const start, u_char *const buf,
    int *const buflen)
{
	char *eptr;
	u_long val;
	int i;

	if (*buflen < 6)
		return (ERANGE);
	for (i = 0; i < 6; i++) {
		val = strtoul(s + *off, &eptr, 16);
		if (val > 0xff || eptr == s + *off)
			return (EINVAL);
		buf[i] = (u_char)val;
		*off = (eptr - s);
		if (i < 6 - 1) {
			if (*eptr != ':')
				return (EINVAL);
			(*off)++;
		}
	}
	*buflen = 6;
	return (0);
}

static int
ng_macaddr_unparse(const struct ng_parse_type *type, const u_char *data,
    int *off, char *cbuf, int cbuflen)
{
	int len;

	len = snprintf(cbuf, cbuflen, "%02x:%02x:%02x:%02x:%02x:%02x",
	    data[*off], data[*off + 1], data[*off + 2],
	    data[*off + 3], data[*off + 4], data[*off + 5]);
	if (len >= cbuflen)
		return (ERANGE);
	*off += 6;
	return (0);
}

/* 
 * this holds all the stuff that should be done at load time 
 */
static int
ng_atmpif_mod_event(module_t mod, int event, void *data)
{
	int error = 0;

	switch (event) {

	case MOD_LOAD:
		vatmpif_nif_zone = uma_zcreate("vatmpif nif",
		    sizeof(struct atm_nif), NULL, NULL, NULL, NULL,
		    UMA_ALIGN_PTR, 0);
		if (vatmpif_nif_zone == NULL) {
			error = ENOMEM;
			break;
		}

		vatmpif_vcc_zone = uma_zcreate("vatmpif vcc",
		    sizeof(Vatmpif_vcc), NULL, NULL, NULL, NULL,
		    UMA_ALIGN_PTR, 0);
		if (vatmpif_vcc_zone == NULL) {
			uma_zdestroy(vatmpif_nif_zone);
			error = ENOMEM;
			break;
		}
		break;

	case MOD_UNLOAD:
		uma_zdestroy(vatmpif_nif_zone);
		uma_zdestroy(vatmpif_vcc_zone);
		break;
	
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}
