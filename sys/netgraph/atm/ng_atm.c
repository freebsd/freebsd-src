/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
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
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * Netgraph module to connect NATM interfaces to netgraph.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sbuf.h>
#include <sys/ioccom.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_atm.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/atm/ng_atm.h>

/*
 * Hooks in the NATM code
 */
extern void	(*ng_atm_attach_p)(struct ifnet *);
extern void	(*ng_atm_detach_p)(struct ifnet *);
extern int	(*ng_atm_output_p)(struct ifnet *, struct mbuf **);
extern void	(*ng_atm_input_p)(struct ifnet *, struct mbuf **,
		    struct atm_pseudohdr *, void *);
extern void	(*ng_atm_input_orphan_p)(struct ifnet *, struct mbuf *,
		    struct atm_pseudohdr *, void *);
extern void	(*ng_atm_message_p)(struct ifnet *, uint32_t, uint32_t);

/*
 * Sysctl stuff.
 */
SYSCTL_NODE(_net_graph, OID_AUTO, atm, CTLFLAG_RW, 0, "atm related stuff");

#ifdef NGATM_DEBUG
static int allow_shutdown;

SYSCTL_INT(_net_graph_atm, OID_AUTO, allow_shutdown, CTLFLAG_RW,
    &allow_shutdown, 0, "allow ng_atm nodes to shutdown");
#endif

/*
 * Hook private data
 */
struct ngvcc {
	uint16_t	vpi;	/* VPI of this hook */
	uint16_t	vci;	/* VCI of this hook, 0 if none */
	uint32_t	flags;	/* private flags */
	hook_p		hook;	/* the connected hook */

	LIST_ENTRY(ngvcc) link;
};
#define	VCC_OPEN	0x0001	/* open */

/*
 * Node private data
 */
struct priv {
	struct ifnet	*ifp;		/* the ATM interface */
	hook_p		input;		/* raw input hook */
	hook_p		orphans;	/* packets to nowhere */
	hook_p		output;		/* catch output packets */
	hook_p		manage;		/* has also entry in vccs */
	uint64_t	in_packets;
	uint64_t	in_errors;
	uint64_t	out_packets;
	uint64_t	out_errors;

	LIST_HEAD(, ngvcc) vccs;
};

/*
 * Parse carrier state
 */
static const struct ng_parse_struct_field ng_atm_carrier_change_info[] =
    NGM_ATM_CARRIER_CHANGE_INFO;
static const struct ng_parse_type ng_atm_carrier_change_type = {
	&ng_parse_struct_type,
	&ng_atm_carrier_change_info
};

/*
 * Parse the configuration structure ng_atm_config
 */
static const struct ng_parse_struct_field ng_atm_config_type_info[] =
    NGM_ATM_CONFIG_INFO;

static const struct ng_parse_type ng_atm_config_type = {
	&ng_parse_struct_type,
	&ng_atm_config_type_info
};

/*
 * Parse a single vcc structure and a variable array of these ng_atm_vccs
 */
static const struct ng_parse_struct_field ng_atm_tparam_type_info[] =
    NGM_ATM_TPARAM_INFO;
static const struct ng_parse_type ng_atm_tparam_type = {
	&ng_parse_struct_type,
	&ng_atm_tparam_type_info
};
static const struct ng_parse_struct_field ng_atm_vcc_type_info[] =
    NGM_ATM_VCC_INFO;
static const struct ng_parse_type ng_atm_vcc_type = {
	&ng_parse_struct_type,
	&ng_atm_vcc_type_info
};


static int
ng_atm_vccarray_getlen(const struct ng_parse_type *type,
	const u_char *start, const u_char *buf)
{
	const struct atmio_vcctable *vp;

	vp = (const struct atmio_vcctable *)
	    (buf - offsetof(struct atmio_vcctable, vccs));

	return (vp->count);
}
static const struct ng_parse_array_info ng_atm_vccarray_info =
    NGM_ATM_VCCARRAY_INFO;
static const struct ng_parse_type ng_atm_vccarray_type = {
	&ng_parse_array_type,
	&ng_atm_vccarray_info
};


static const struct ng_parse_struct_field ng_atm_vcctable_type_info[] =
    NGM_ATM_VCCTABLE_INFO;

static const struct ng_parse_type ng_atm_vcctable_type = {
	&ng_parse_struct_type,
	&ng_atm_vcctable_type_info
};

/*
 * Parse CPCS INIT structure ng_atm_cpcs_init
 */
static const struct ng_parse_struct_field ng_atm_cpcs_init_type_info[] =
    NGM_ATM_CPCS_INIT_INFO;

static const struct ng_parse_type ng_atm_cpcs_init_type = {
	&ng_parse_struct_type,
	&ng_atm_cpcs_init_type_info
};

/*
 * Parse CPCS TERM structure ng_atm_cpcs_term
 */
static const struct ng_parse_struct_field ng_atm_cpcs_term_type_info[] =
    NGM_ATM_CPCS_TERM_INFO;

static const struct ng_parse_type ng_atm_cpcs_term_type = {
	&ng_parse_struct_type,
	&ng_atm_cpcs_term_type_info
};

/*
 * Parse statistic struct
 */
static const struct ng_parse_struct_field ng_atm_stats_type_info[] =
    NGM_ATM_STATS_INFO;

static const struct ng_parse_type ng_atm_stats_type = {
	&ng_parse_struct_type,
	&ng_atm_stats_type_info
};

static const struct ng_cmdlist ng_atm_cmdlist[] = {
	{
	  NGM_ATM_COOKIE,
	  NGM_ATM_GET_IFNAME,
	  "getifname",
	  NULL,
	  &ng_parse_string_type
	},
	{
	  NGM_ATM_COOKIE,
	  NGM_ATM_GET_CONFIG,
	  "getconfig",
	  NULL,
	  &ng_atm_config_type
	},
	{
	  NGM_ATM_COOKIE,
	  NGM_ATM_GET_VCCS,
	  "getvccs",
	  NULL,
	  &ng_atm_vcctable_type
	},
	{
	  NGM_ATM_COOKIE,
	  NGM_ATM_CPCS_INIT,
	  "cpcsinit",
	  &ng_atm_cpcs_init_type,
	  NULL
	},
	{
	  NGM_ATM_COOKIE,
	  NGM_ATM_CPCS_TERM,
	  "cpcsterm",
	  &ng_atm_cpcs_term_type,
	  NULL
	},
	{
	  NGM_ATM_COOKIE,
	  NGM_ATM_CARRIER_CHANGE,
	  "carrier",
	  &ng_atm_carrier_change_type,
	  &ng_atm_carrier_change_type,
	},
	{
	  NGM_ATM_COOKIE,
	  NGM_ATM_GET_VCC,
	  "getvcc",
	  &ng_parse_hookbuf_type,
	  &ng_atm_vcc_type
	},
	{
	  NGM_ATM_COOKIE,
	  NGM_ATM_GET_VCCID,
	  "getvccid",
	  &ng_atm_vcc_type,
	  &ng_atm_vcc_type
	},
	{
	  NGM_ATM_COOKIE,
	  NGM_ATM_GET_STATS,
	  "getstats",
	  NULL,
	  &ng_atm_stats_type
	},
	{ 0 }
};

static int ng_atm_mod_event(module_t, int, void *);

static ng_constructor_t ng_atm_constructor;
static ng_shutdown_t	ng_atm_shutdown;
static ng_rcvmsg_t	ng_atm_rcvmsg;
static ng_newhook_t	ng_atm_newhook;
static ng_connect_t	ng_atm_connect;
static ng_disconnect_t	ng_atm_disconnect;
static ng_rcvdata_t	ng_atm_rcvdata;
static ng_rcvdata_t	ng_atm_rcvdrop;

static struct ng_type ng_atm_typestruct = {
	NG_ABI_VERSION,
	NG_ATM_NODE_TYPE,
	ng_atm_mod_event,	/* Module event handler (optional) */
	ng_atm_constructor,	/* Node constructor */
	ng_atm_rcvmsg,		/* control messages come here */
	ng_atm_shutdown,	/* reset, and free resources */
	ng_atm_newhook,		/* first notification of new hook */
	NULL,			/* findhook */
	ng_atm_connect,		/* connect */
	ng_atm_rcvdata,		/* rcvdata */
	ng_atm_disconnect,	/* notify on disconnect */
	ng_atm_cmdlist,
};
NETGRAPH_INIT(atm, &ng_atm_typestruct);

static const struct {
	u_int	media;
	const char *name;
} atmmedia[] = IFM_SUBTYPE_ATM_DESCRIPTIONS;


#define	IFP2NG(IFP)	((node_p)((struct ifatm *)(IFP))->ngpriv)

#define	IFFLAGS "\020\001UP\002BROADCAST\003DEBUG\004LOOPBACK" \
		 "\005POINTOPOINT\006SMART\007RUNNING\010NOARP" \
		 "\011PROMISC\012ALLMULTI\013OACTIVE\014SIMPLEX" \
		 "\015LINK0\016LINK1\017LINK2\020MULTICAST"


/************************************************************/
/*
 * INPUT
 */
/*
 * A packet is received from an interface. 
 * If we have an input hook, prepend the pseudoheader to the data and
 * deliver it out to that hook. If not, look whether it is destined for
 * use. If so locate the appropriate hook, deliver the packet without the
 * header and we are done. If it is not for us, leave it alone.
 */
static void
ng_atm_input(struct ifnet *ifp, struct mbuf **mp,
	struct atm_pseudohdr *ah, void *rxhand)
{
	node_p node = IFP2NG(ifp);
	struct priv *priv;
	const struct ngvcc *vcc;
	int error;

	if (node == NULL)
		return;
	priv = NG_NODE_PRIVATE(node);
	if (priv->input != NULL) {
		/*
		 * Prepend the atm_pseudoheader.
		 */
		M_PREPEND(*mp, sizeof(*ah), M_DONTWAIT);
		if (*mp == NULL)
			return;
		memcpy(mtod(*mp, struct atm_pseudohdr *), ah, sizeof(*ah));
		NG_SEND_DATA_ONLY(error, priv->input, *mp);
		if (error == 0) {
			priv->in_packets++;
			*mp = NULL;
		} else {
#ifdef NGATM_DEBUG
			printf("%s: error=%d\n", __func__, error);
#endif
			priv->in_errors++;
		}
		return;
	}
	if ((ATM_PH_FLAGS(ah) & ATMIO_FLAG_NG) == 0)
		return;

	vcc = (struct ngvcc *)rxhand;

	NG_SEND_DATA_ONLY(error, vcc->hook, *mp);
	if (error == 0) {
		priv->in_packets++;
		*mp = NULL;
	} else {
#ifdef NGATM_DEBUG
		printf("%s: error=%d\n", __func__, error);
#endif
		priv->in_errors++;
	}
}

/*
 * ATM packet is about to be output. The atm_pseudohdr is already prepended.
 * If the hook is set, reroute the packet to the hook.
 */
static int
ng_atm_output(struct ifnet *ifp, struct mbuf **mp)
{
	const node_p node = IFP2NG(ifp);
	const struct priv *priv;
	int error = 0;

	if (node == NULL)
		return (0);
	priv = NG_NODE_PRIVATE(node);
	if (priv->output) {
		NG_SEND_DATA_ONLY(error, priv->output, *mp);
		*mp = NULL;
	}

	return (error);
}

/*
 * Well, this doesn't make much sense for ATM.
 */
static void
ng_atm_input_orphans(struct ifnet *ifp, struct mbuf *m,
	struct atm_pseudohdr *ah, void *rxhand)
{
	node_p node = IFP2NG(ifp);
	struct priv *priv;
	int error;

	if (node == NULL) {
		m_freem(m);
		return;
	}
	priv = NG_NODE_PRIVATE(node);
	if (priv->orphans == NULL) {
		m_freem(m);
		return;
	}
	/*
	 * Prepend the atm_pseudoheader.
	 */
	M_PREPEND(m, sizeof(*ah), M_DONTWAIT);
	if (m == NULL)
		return;
	memcpy(mtod(m, struct atm_pseudohdr *), ah, sizeof(*ah));
	NG_SEND_DATA_ONLY(error, priv->orphans, m);
	if (error == 0)
		priv->in_packets++;
	else {
		priv->in_errors++;
#ifdef NGATM_DEBUG
		printf("%s: error=%d\n", __func__, error);
#endif
	}
}

/************************************************************/
/*
 * OUTPUT
 */
static int
ng_atm_rcvdata(hook_p hook, item_p item)
{
	node_p node = NG_HOOK_NODE(hook);
	struct priv *priv = NG_NODE_PRIVATE(node);
	const struct ngvcc *vcc = NG_HOOK_PRIVATE(hook);
	struct mbuf *m;
	struct atm_pseudohdr *aph;
	int error;

	if (vcc->vci == 0) {
		NG_FREE_ITEM(item);
		return (ENOTCONN);
	}

	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);

	/*
	 * Prepend pseudo-hdr. Drivers don't care about the flags.
	 */
	M_PREPEND(m, sizeof(*aph), M_DONTWAIT);
	if (m == NULL) {
		NG_FREE_M(m);
		return (ENOMEM);
	}
	aph = mtod(m, struct atm_pseudohdr *);
	ATM_PH_VPI(aph) = vcc->vpi;
	ATM_PH_SETVCI(aph, vcc->vci);
	ATM_PH_FLAGS(aph) = 0;

	if ((error = atm_output(priv->ifp, m, NULL, NULL)) == 0)
		priv->out_packets++;
	else
		priv->out_errors++;
	return (error);
}

static int
ng_atm_rcvdrop(hook_p hook, item_p item)
{
	NG_FREE_ITEM(item);
	return (0);
}


/************************************************************
 *
 * Message from driver.
 */
#ifdef notyet
static void
ng_atm_message_func(node_p node, hook_p hook, void *arg1, int arg2)
{
	uint32_t msg = (uintptr_t)arg1;
	uint32_t arg = (uint32_t)arg2;
	const struct priv *priv = NG_NODE_PRIVATE(node);
	struct ngvcc *vcc;
	u_int vci, vpi, state;
	struct ng_mesg *mesg;
	int error;

	switch (msg) {

	  case ATM_MSG_FLOW_CONTROL:
	    {
		struct ngm_queue_state *qstate;

		/* find the connection */
		vci = arg & 0xffff;
		vpi = (arg >> 16) & 0xff;
		state = (arg >> 24) & 1;
		LIST_FOREACH(vcc, &priv->vccs, link)
			if (vcc->vci == vci && vcc->vpi == vpi)
				break;
		if (vcc == NULL)
			break;

		/* convert into a flow control message */
		NG_MKMESSAGE(mesg, NGM_FLOW_COOKIE,
		    state ? NGM_HIGH_WATER_PASSED : NGM_LOW_WATER_PASSED,
		    sizeof(struct ngm_queue_state), M_NOWAIT);
		if (mesg == NULL)
			break;
		qstate = (struct ngm_queue_state *)mesg->data;

		/* XXX have to figure out how to get that info */

		NG_SEND_MSG_HOOK(error, node, mesg, vcc->hook, NULL);
		break;
	    }

	  case ATM_MSG_VCC_CHANGED:
	    {
		struct ngm_atm_vcc_change *chg;

		if (priv->manage == NULL)
			break;
		NG_MKMESSAGE(mesg, NGM_ATM_COOKIE, NGM_ATM_VCC_CHANGE,
		    sizeof(struct ngm_atm_vcc_change), M_NOWAIT);
		if (mesg == NULL)
			break;
		chg = (struct ngm_atm_vcc_change *)mesg->data;
		chg->vci = arg & 0xffff;
		chg->vpi = (arg >> 16) & 0xff;
		chg->state = (arg >> 24) & 1;
		chg->node = NG_NODE_ID(node);
		NG_SEND_MSG_HOOK(error, node, mesg, priv->manage, NULL);
		break;
	    }

	  case ATM_MSG_CARRIER_CHANGE:
	    {
		struct ngm_atm_carrier_change *chg;

		if (priv->manage == NULL)
			break;
		NG_MKMESSAGE(mesg, NGM_ATM_COOKIE, NGM_ATM_CARRIER_CHANGE,
		    sizeof(struct ngm_atm_carrier_change), M_NOWAIT);
		if (mesg == NULL)
			break;
		chg = (struct ngm_atm_carrier_change *)mesg->data;
		chg->state = arg & 1;
		chg->node = NG_NODE_ID(node);
		NG_SEND_MSG_HOOK(error, node, mesg, priv->manage, NULL);
		break;
	    }
	}
}
#endif

/*
 * Use send_fn to get the right lock
 */
static void
ng_atm_message(struct ifnet *ifp, uint32_t msg, uint32_t arg)
{
#ifdef notyet
	const node_p node = IFP2NG(ifp);

	(void)ng_send_fn(node, NULL, ng_atm_message_func,
	    (void *)(uintptr_t)msg, arg);
#endif
}

/************************************************************
 *
 * CPCS
 */
/*
 * Open a channel for the user
 */
static int
ng_atm_cpcs_init(node_p node, const struct ngm_atm_cpcs_init *arg)
{
	struct priv *priv = NG_NODE_PRIVATE(node);
	const struct ifatm_mib *mib;
	struct ngvcc *vcc;
	struct atmio_openvcc data;
	int err;

	if(priv->ifp->if_ioctl == NULL)
		return (ENXIO);

	mib = (const struct ifatm_mib *)(priv->ifp->if_linkmib);

	LIST_FOREACH(vcc, &priv->vccs, link)
		if (strcmp(arg->name, NG_HOOK_NAME(vcc->hook)) == 0)
			break;
	if (vcc == NULL)
		return (ENOTCONN);
	if (vcc->vci != 0)
		return (EISCONN);

	/*
	 * Check user arguments and construct ioctl argument
	 */
	memset(&data, 0, sizeof(data));

	data.rxhand = vcc;

	switch (data.param.aal = arg->aal) {

	  case ATMIO_AAL_34:
	  case ATMIO_AAL_5:
	  case ATMIO_AAL_0:
	  case ATMIO_AAL_RAW:
		break;

	  default:
		return (EINVAL);
	}

	if (arg->vpi > 0xff)
		return (EINVAL);
	data.param.vpi = arg->vpi;

	/* allow 0.0 as catch all receive channel */
	if (arg->vci == 0 && (arg->vpi != 0 || !(arg->flags & ATMIO_FLAG_NOTX)))
		return (EINVAL);
	data.param.vci = arg->vci;

	data.param.tparam.pcr = arg->pcr;

	if (arg->mcr > arg->pcr)
		return (EINVAL);
	data.param.tparam.mcr = arg->mcr;

	if (!(arg->flags & ATMIO_FLAG_NOTX)) {
		if (arg->tmtu == 0)
			data.param.tmtu = priv->ifp->if_mtu;
		else {
			data.param.tmtu = arg->tmtu;
		}
	}
	if (!(arg->flags & ATMIO_FLAG_NORX)) {
		if (arg->rmtu == 0)
			data.param.rmtu = priv->ifp->if_mtu;
		else {
			data.param.rmtu = arg->rmtu;
		}
	}

	switch (data.param.traffic = arg->traffic) {

	  case ATMIO_TRAFFIC_UBR:
	  case ATMIO_TRAFFIC_CBR:
		break;

	  case ATMIO_TRAFFIC_VBR:
		if (arg->scr > arg->pcr)
			return (EINVAL);
		data.param.tparam.scr = arg->scr;

		if (arg->mbs > (1 << 24))
			return (EINVAL);
		data.param.tparam.mbs = arg->mbs;
		break;

	  case ATMIO_TRAFFIC_ABR:
		if (arg->icr > arg->pcr || arg->icr < arg->mcr)
			return (EINVAL);
		data.param.tparam.icr = arg->icr;

		if (arg->tbe == 0 || arg->tbe > (1 << 24))
			return (EINVAL);
		data.param.tparam.tbe = arg->tbe;

		if (arg->nrm > 0x7)
			return (EINVAL);
		data.param.tparam.nrm = arg->nrm;

		if (arg->trm > 0x7)
			return (EINVAL);
		data.param.tparam.trm = arg->trm;

		if (arg->adtf > 0x3ff)
			return (EINVAL);
		data.param.tparam.adtf = arg->adtf;

		if (arg->rif > 0xf)
			return (EINVAL);
		data.param.tparam.rif = arg->rif;

		if (arg->rdf > 0xf)
			return (EINVAL);
		data.param.tparam.rdf = arg->rdf;

		if (arg->cdf > 0x7)
			return (EINVAL);
		data.param.tparam.cdf = arg->cdf;

		break;

	  default:
		return (EINVAL);
	}

	if ((arg->flags & ATMIO_FLAG_NORX) && (arg->flags & ATMIO_FLAG_NOTX))
		return (EINVAL);

	data.param.flags = arg->flags & ~(ATM_PH_AAL5 | ATM_PH_LLCSNAP);
	data.param.flags |= ATMIO_FLAG_NG;

	err = (*priv->ifp->if_ioctl)(priv->ifp, SIOCATMOPENVCC, (caddr_t)&data);

	if (err == 0) {
		vcc->vci = data.param.vci;
		vcc->vpi = data.param.vpi;
		vcc->flags = VCC_OPEN;
	}

	return (err);
}

/*
 * Issue the close command to the driver
 */
static int
cpcs_term(const struct priv *priv, u_int vpi, u_int vci)
{
	struct atmio_closevcc data;

	if (priv->ifp->if_ioctl == NULL)
		return ENXIO;

	data.vpi = vpi;
	data.vci = vci;

	return ((*priv->ifp->if_ioctl)(priv->ifp,
	    SIOCATMCLOSEVCC, (caddr_t)&data));
}


/*
 * Close a channel by request of the user
 */
static int
ng_atm_cpcs_term(node_p node, const struct ngm_atm_cpcs_term *arg)
{
	struct priv *priv = NG_NODE_PRIVATE(node);
	struct ngvcc *vcc;
	int error;

	LIST_FOREACH(vcc, &priv->vccs, link)
		if(strcmp(arg->name, NG_HOOK_NAME(vcc->hook)) == 0)
			break;
	if (vcc == NULL)
		return (ENOTCONN);
	if (vcc->vci == 0)
		return (ENOTCONN);

	error = cpcs_term(priv, vcc->vpi, vcc->vci);

	vcc->vci = 0;
	vcc->vpi = 0;
	vcc->flags = 0;

	return (error);
}

/************************************************************/
/*
 * CONTROL MESSAGES
 */

/*
 * Produce a textual description of the current status
 */
static int
text_status(node_p node, char *arg, u_int len)
{
	const struct priv *priv = NG_NODE_PRIVATE(node);
	const struct ifatm_mib *mib;
	struct sbuf sbuf;
	u_int i;

	static const struct {
		const char	*name;
		const char	*vendor;
	} devices[] = {
		ATM_DEVICE_NAMES
	};

	mib = (const struct ifatm_mib *)(priv->ifp->if_linkmib);

	sbuf_new(&sbuf, arg, len, SBUF_FIXEDLEN);
	sbuf_printf(&sbuf, "interface: %s%d\n", priv->ifp->if_name,
	    priv->ifp->if_unit);

	if (mib->device >= sizeof(devices) / sizeof(devices[0]))
		sbuf_printf(&sbuf, "device=unknown\nvendor=unknown\n");
	else
		sbuf_printf(&sbuf, "device=%s\nvendor=%s\n",
		    devices[mib->device].name, devices[mib->device].vendor);

	for (i = 0; atmmedia[i].name; i++)
		if(mib->media == atmmedia[i].media) {
			sbuf_printf(&sbuf, "media=%s\n", atmmedia[i].name);
			break;
		}
	if(atmmedia[i].name == NULL)
		sbuf_printf(&sbuf, "media=unknown\n");

	sbuf_printf(&sbuf, "serial=%u esi=%6D hardware=%u software=%u\n",
	    mib->serial, mib->esi, ":", mib->hw_version, mib->sw_version);
	sbuf_printf(&sbuf, "pcr=%u vpi_bits=%u vci_bits=%u max_vpcs=%u "
	    "max_vccs=%u\n", mib->pcr, mib->vpi_bits, mib->vci_bits,
	    mib->max_vpcs, mib->max_vccs);
	sbuf_printf(&sbuf, "ifflags=%b\n", priv->ifp->if_flags, IFFLAGS);

	sbuf_finish(&sbuf);

	return (sbuf_len(&sbuf));
}

/*
 * Get control message
 */
static int
ng_atm_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const struct priv *priv = NG_NODE_PRIVATE(node);
	struct ng_mesg *resp = NULL;
	struct ng_mesg *msg;
	struct ifatm_mib *mib = (struct ifatm_mib *)(priv->ifp->if_linkmib);
	int error = 0;

	NGI_GET_MSG(item, msg);

	switch (msg->header.typecookie) {

	  case NGM_GENERIC_COOKIE:
		switch (msg->header.cmd) {

		  case NGM_TEXT_STATUS:
			NG_MKRESPONSE(resp, msg, NG_TEXTRESPONSE, M_NOWAIT);
			if(resp == NULL) {
				error = ENOMEM;
				break;
			}

			resp->header.arglen = text_status(node,
			    (char *)resp->data, resp->header.arglen) + 1;
			break;

		  default:
			error = EINVAL;
			break;
		}
		break;

	  case NGM_ATM_COOKIE:
		switch (msg->header.cmd) {

		  case NGM_ATM_GET_IFNAME:
			NG_MKRESPONSE(resp, msg, IFNAMSIZ + 1, M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			snprintf(resp->data, IFNAMSIZ + 1, "%s%d",
			    priv->ifp->if_name, priv->ifp->if_unit);
			break;

		  case NGM_ATM_GET_CONFIG:
		    {
			struct ngm_atm_config *config;

			NG_MKRESPONSE(resp, msg, sizeof(*config), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			config = (struct ngm_atm_config *)resp->data;
			config->pcr = mib->pcr;
			config->vpi_bits = mib->vpi_bits;
			config->vci_bits = mib->vci_bits;
			config->max_vpcs = mib->max_vpcs;
			config->max_vccs = mib->max_vccs;
			break;
		    }

		  case NGM_ATM_GET_VCCS:
		    {
			struct atmio_vcctable *vccs;
			size_t len;

			if (priv->ifp->if_ioctl == NULL) {
				error = ENXIO;
				break;
			}
			error = (*priv->ifp->if_ioctl)(priv->ifp,
			    SIOCATMGETVCCS, (caddr_t)&vccs);
			if (error)
				break;

			len = sizeof(*vccs) +
			    vccs->count * sizeof(vccs->vccs[0]);
			NG_MKRESPONSE(resp, msg, len, M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				free(vccs, M_DEVBUF);
				break;
			}

			(void)memcpy(resp->data, vccs, len);
			free(vccs, M_DEVBUF);

			break;
		    }

		  case NGM_ATM_GET_VCC:
		    {
			char hook[NG_HOOKLEN + 1];
			struct atmio_vcctable *vccs;
			struct ngvcc *vcc;
			u_int i;

			if (priv->ifp->if_ioctl == NULL) {
				error = ENXIO;
				break;
			}
			if (msg->header.arglen != NG_HOOKLEN + 1) {
				error = EINVAL;
				break;
			}
			strncpy(hook, msg->data, NG_HOOKLEN + 1);
			hook[NG_HOOKLEN] = '\0';
			LIST_FOREACH(vcc, &priv->vccs, link)
				if (strcmp(NG_HOOK_NAME(vcc->hook), hook) == 0)
					break;
			if (vcc == NULL) {
				error = ENOTCONN;
				break;
			}
			error = (*priv->ifp->if_ioctl)(priv->ifp,
			    SIOCATMGETVCCS, (caddr_t)&vccs);
			if (error)
				break;

			for (i = 0; i < vccs->count; i++)
				if (vccs->vccs[i].vpi == vcc->vpi &&
				    vccs->vccs[i].vci == vcc->vci)
					break;
			if (i == vccs->count) {
				error = ENOTCONN;
				free(vccs, M_DEVBUF);
				break;
			}

			NG_MKRESPONSE(resp, msg, sizeof(vccs->vccs[0]),
			    M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				free(vccs, M_DEVBUF);
				break;
			}

			*(struct atmio_vcc *)resp->data = vccs->vccs[i];
			free(vccs, M_DEVBUF);
			break;
		    }

		  case NGM_ATM_GET_VCCID:
		    {
			struct atmio_vcc *arg;
			struct atmio_vcctable *vccs;
			u_int i;

			if (priv->ifp->if_ioctl == NULL) {
				error = ENXIO;
				break;
			}
			if (msg->header.arglen != sizeof(*arg)) {
				error = EINVAL;
				break;
			}
			arg = (struct atmio_vcc *)msg->data;

			error = (*priv->ifp->if_ioctl)(priv->ifp,
			    SIOCATMGETVCCS, (caddr_t)&vccs);
			if (error)
				break;

			for (i = 0; i < vccs->count; i++)
				if (vccs->vccs[i].vpi == arg->vpi &&
				    vccs->vccs[i].vci == arg->vci)
					break;
			if (i == vccs->count) {
				error = ENOTCONN;
				free(vccs, M_DEVBUF);
				break;
			}

			NG_MKRESPONSE(resp, msg, sizeof(vccs->vccs[0]),
			    M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				free(vccs, M_DEVBUF);
				break;
			}

			*(struct atmio_vcc *)resp->data = vccs->vccs[i];
			free(vccs, M_DEVBUF);
			break;
		    }

		  case NGM_ATM_CPCS_INIT:
			if (msg->header.arglen !=
			    sizeof(struct ngm_atm_cpcs_init)) {
				error = EINVAL;
				break;
			}
			error = ng_atm_cpcs_init(node,
			    (struct ngm_atm_cpcs_init *)msg->data);
			break;

		  case NGM_ATM_CPCS_TERM:
			if (msg->header.arglen !=
			    sizeof(struct ngm_atm_cpcs_term)) {
				error = EINVAL;
				break;
			}
			error = ng_atm_cpcs_term(node,
			    (struct ngm_atm_cpcs_term *)msg->data);
			break;

		  case NGM_ATM_GET_STATS:
		    {
			struct ngm_atm_stats *p;

			if (msg->header.arglen != 0) {
				error = EINVAL;
				break;
			}
			NG_MKRESPONSE(resp, msg, sizeof(*p), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			p = (struct ngm_atm_stats *)resp->data;
			p->in_packets = priv->in_packets;
			p->out_packets = priv->out_packets;
			p->in_errors = priv->in_errors;
			p->out_errors = priv->out_errors;

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

	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

/************************************************************/
/*
 * HOOK MANAGEMENT
 */

/*
 * A new hook is create that will be connected to the node.
 * Check, whether the name is one of the predefined ones.
 * If not, create a new entry into the vcc list.
 */
static int
ng_atm_newhook(node_p node, hook_p hook, const char *name)
{
	struct priv *priv = NG_NODE_PRIVATE(node);
	struct ngvcc *vcc;

	if (strcmp(name, "input") == 0) {
		priv->input = hook;
		NG_HOOK_SET_RCVDATA(hook, ng_atm_rcvdrop);
		return (0);
	}
	if (strcmp(name, "output") == 0) {
		priv->output = hook;
		NG_HOOK_SET_RCVDATA(hook, ng_atm_rcvdrop);
		return (0);
	}
	if (strcmp(name, "orphans") == 0) {
		priv->orphans = hook;
		NG_HOOK_SET_RCVDATA(hook, ng_atm_rcvdrop);
		return (0);
	}

	/*
	 * Allocate a new entry
	 */
	vcc = malloc(sizeof(*vcc), M_NETGRAPH, M_NOWAIT | M_ZERO);
	if (vcc == NULL)
		return (ENOMEM);

	vcc->hook = hook;
	NG_HOOK_SET_PRIVATE(hook, vcc);

	LIST_INSERT_HEAD(&priv->vccs, vcc, link);

	if (strcmp(name, "manage") == 0)
		priv->manage = hook;

	return (0);
}

/*
 * Connect. Set the peer to queuing.
 */
static int
ng_atm_connect(hook_p hook)
{
	if (NG_HOOK_PRIVATE(hook) != NULL)
		NG_HOOK_FORCE_QUEUE(NG_HOOK_PEER(hook));

	return (0);
}

/*
 * Disconnect a HOOK
 */
static int
ng_atm_disconnect(hook_p hook)
{
	struct priv *priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct ngvcc *vcc = NG_HOOK_PRIVATE(hook);

	if (vcc == NULL) {
		if (hook == priv->output) {
			priv->output = NULL;
			return (0);
		}
		if (hook == priv->input) {
			priv->input = NULL;
			return (0);
		}
		if (hook == priv->orphans) {
			priv->orphans = NULL;
			return (0);
		}
		log(LOG_ERR, "ng_atm: bad hook '%s'", NG_HOOK_NAME(hook));
		return (0);
	}

	/* don't terminate if we are detaching from the interface */
	if ((vcc->flags & VCC_OPEN) && priv->ifp != NULL)
		(void)cpcs_term(priv, vcc->vpi, vcc->vci);

	NG_HOOK_SET_PRIVATE(hook, NULL);

	LIST_REMOVE(vcc, link);
	free(vcc, M_NETGRAPH);

	if (hook == priv->manage)
		priv->manage = NULL;

	return (0);
}

/************************************************************/
/*
 * NODE MANAGEMENT
 */

/*
 * ATM interface attached - create a node and name it like the interface.
 */
static void
ng_atm_attach(struct ifnet *ifp)
{
	char name[IFNAMSIZ+1];
	node_p node;
	struct priv *priv;

	KASSERT(IFP2NG(ifp) == 0, ("%s: node alreay exists?", __FUNCTION__));

	snprintf(name, sizeof(name), "%s%d", ifp->if_name, ifp->if_unit);

	if (ng_make_node_common(&ng_atm_typestruct, &node) != 0) {
		log(LOG_ERR, "%s: can't create node for %s\n",
		    __FUNCTION__, name);
		return;
	}

	priv = malloc(sizeof(*priv), M_NETGRAPH, M_NOWAIT | M_ZERO);
	if (priv == NULL) {
		log(LOG_ERR, "%s: can't allocate memory for %s\n",
		    __FUNCTION__, name);
		NG_NODE_UNREF(node);
		return;
	}
	NG_NODE_SET_PRIVATE(node, priv);
	priv->ifp = ifp;
	LIST_INIT(&priv->vccs);
	IFP2NG(ifp) = node;

	if (ng_name_node(node, name) != 0) {
		log(LOG_WARNING, "%s: can't name node %s\n",
		    __FUNCTION__, name);
	}
}

/*
 * ATM interface detached - destroy node.
 */
static void
ng_atm_detach(struct ifnet *ifp)
{
	const node_p node = IFP2NG(ifp);
	struct priv *priv;

	if(node == NULL)
		return;

	NG_NODE_REALLY_DIE(node);

	priv = NG_NODE_PRIVATE(node);
	IFP2NG(priv->ifp) = NULL;
	priv->ifp = NULL;

	ng_rmnode_self(node);
}

/*
 * Shutdown the node. This is called from the shutdown message processing.
 */
static int
ng_atm_shutdown(node_p node)
{
	struct priv *priv = NG_NODE_PRIVATE(node);

	if (node->nd_flags & NG_REALLY_DIE) {
		/*
		 * We are called from unloading the ATM driver. Really,
		 * really need to shutdown this node. The ifp was
		 * already handled in the detach routine.
		 */
		NG_NODE_SET_PRIVATE(node, NULL);
		free(priv, M_NETGRAPH);

		NG_NODE_UNREF(node);
		return (0);
	}

#ifdef NGATM_DEBUG
	if (!allow_shutdown)
		node->nd_flags &= ~NG_INVALID;
	else {
		IFP2NG(priv->ifp) = NULL;
		NG_NODE_SET_PRIVATE(node, NULL);
		free(priv, M_NETGRAPH);
		NG_NODE_UNREF(node);
	}
#else
	/*
	 * We are persistant - reinitialize
	 */
	node->nd_flags &= ~NG_INVALID;
#endif
	return (0);
}

/*
 * Nodes are constructed only via interface attaches.
 */
static int
ng_atm_constructor(node_p nodep)
{
	return (EINVAL);
}

/************************************************************/
/*
 * INITIALISATION
 */
/*
 * Loading and unloading of node type
 *
 * The assignments to the globals for the hooks should be ok without
 * a special hook. The use pattern is generally: check that the pointer
 * is not NULL, call the function. In the attach case this is no problem.
 * In the detach case we can detach only when no ATM node exists. That
 * means that there is no ATM interface anymore. So we are sure that
 * we are not in the code path in if_atmsubr.c. To prevent someone
 * from adding an interface after we have started to unload the node, we
 * take the iflist lock so an if_attach will be blocked until we are done.
 * XXX: perhaps the function pointers should be 'volatile' for this to work
 * properly.
 */
static int
ng_atm_mod_event(module_t mod, int event, void *data)
{
	struct ifnet *ifp;
	int error = 0;

	switch (event) {

	  case MOD_LOAD:
		/*
		 * Register function hooks
		 */
		if (ng_atm_attach_p != NULL) {
			error = EEXIST;
			break;
		}
		IFNET_RLOCK();

		ng_atm_attach_p = ng_atm_attach;
		ng_atm_detach_p = ng_atm_detach;
		ng_atm_output_p = ng_atm_output;
		ng_atm_input_p = ng_atm_input;
		ng_atm_input_orphan_p = ng_atm_input_orphans;
		ng_atm_message_p = ng_atm_message;

		/* Create nodes for existing ATM interfaces */
		TAILQ_FOREACH(ifp, &ifnet, if_link) {
			if (ifp->if_type == IFT_ATM)
				ng_atm_attach(ifp);
		}
		IFNET_RUNLOCK();
		break;

	  case MOD_UNLOAD:
		IFNET_RLOCK();

		ng_atm_attach_p = NULL;
		ng_atm_detach_p = NULL;
		ng_atm_output_p = NULL;
		ng_atm_input_p = NULL;
		ng_atm_input_orphan_p = NULL;
		ng_atm_message_p = NULL;

		TAILQ_FOREACH(ifp, &ifnet, if_link) {
			if (ifp->if_type == IFT_ATM)
				ng_atm_detach(ifp);
		}
		IFNET_RUNLOCK();
		break;

	  default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}
