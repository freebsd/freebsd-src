/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * SPANS Signalling Manager
 * ---------------------------
 *
 * SPANS Connectionless Datagram Service (CLS) module
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <netatm/kern_include.h>

#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>
#include "spans_xdr.h"
#include <netatm/spans/spans_var.h>
#include <netatm/spans/spans_cls.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Global variables
 */
int	spanscls_print = 0;
SYSCTL_INT(_net_harp_spans, OID_AUTO, spanscls_print, CTLFLAG_RW,
    &spanscls_print, 0, "dump SPANS packets");

struct spanscls	*spanscls_head = NULL;

struct spans_addr	spans_bcastaddr = {
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
};

struct spanscls_hdr	spanscls_hdr = {
	{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },	/* dst */
	{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },	/* src */
	0x00, 0x00, 0,
	0xaa, 0xaa, 0x03, { 0x00, 0x00, 0x00 }, 0		/* LLC SNAP */
};


/*
 * Local functions
 */
static int	spanscls_ipact __P((struct ip_nif *));
static int	spanscls_ipdact __P((struct ip_nif *));
static int	spanscls_bcast_output __P((struct ip_nif *, KBuffer *));
static void	spanscls_cpcs_data __P((void *, KBuffer *));
static void	spanscls_connected __P((void *));
static void	spanscls_cleared __P((void *, struct t_atm_cause *));
static caddr_t	spanscls_getname __P((void *));
static void	spanscls_pdu_print __P((const struct spanscls *,
		    const KBuffer *, const char *));

/*
 * Local variables
 */
static struct sp_info  spanscls_pool = {
	"spans cls pool",		/* si_name */
	sizeof(struct spanscls),	/* si_blksiz */
	2,				/* si_blkcnt */
	100				/* si_maxallow */
};

static struct ip_serv	spanscls_ipserv = {
	spanscls_ipact,
	spanscls_ipdact,
	spansarp_ioctl,
	NULL,
	spansarp_svcout,
	spansarp_svcin,
	spansarp_svcactive,
	spansarp_vcclose,
	spanscls_bcast_output,
	{
		{ATM_AAL5, ATM_ENC_NULL},
		{ATM_AAL3_4, ATM_ENC_NULL}
	}
};

static u_char	spanscls_bridged[] = {
	0x00, 0x00, 0x00, 0x00,
	0xaa, 0xaa, 0x03, 0x00, 0x80, 0xc2	/* LLC SNAP */
};

static Atm_endpoint	spanscls_endpt = {
	NULL,
	ENDPT_SPANS_CLS,
	NULL,
	spanscls_getname,
	spanscls_connected,
	spanscls_cleared,
	NULL,
	NULL,
	NULL,
	NULL,
	spanscls_cpcs_data,
	NULL,
	NULL,
	NULL,
	NULL
};

static Atm_attributes	spanscls_attr = {
	NULL,			/* nif */
	CMAPI_CPCS,		/* api */
	0,			/* api_init */
	0,			/* headin */
	0,			/* headout */
	{			/* aal */
		T_ATM_PRESENT,
		ATM_AAL3_4
	},
	{			/* traffic */
		T_ATM_PRESENT,
		{
			{
				T_ATM_ABSENT,
				0,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_NO
			},
			{
				T_ATM_ABSENT,
				0,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_NO
			},
			T_YES
		},
	},
	{			/* bearer */
		T_ATM_PRESENT,
		{
			T_ATM_CLASS_X,
			T_ATM_NULL,
			T_ATM_NULL,
			T_NO,
			T_ATM_1_TO_1
		}
	},
	{			/* bhli */
		T_ATM_ABSENT
	},
	{			/* blli */
		T_ATM_ABSENT,
		T_ATM_ABSENT
	},
	{			/* llc */
		T_ATM_ABSENT
	},
	{			/* called */
		T_ATM_PRESENT,
	},
	{			/* calling */
		T_ATM_ABSENT
	},
	{			/* qos */
		T_ATM_PRESENT,
		{
			T_ATM_NETWORK_CODING,
			{
				T_ATM_QOS_CLASS_0,
			},
			{
				T_ATM_QOS_CLASS_0
			}
		}
	},
	{			/* transit */
		T_ATM_ABSENT
	},
	{			/* cause */
		T_ATM_ABSENT
	}
};

static struct t_atm_cause	spanscls_cause = {
	T_ATM_ITU_CODING,
	T_ATM_LOC_USER,
	T_ATM_CAUSE_UNSPECIFIED_NORMAL,
	{0, 0, 0, 0}
};


/*
 * Process module loading
 * 
 * Called whenever the spans module is initializing.  
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0	initialization successful
 *	errno	initialization failed - reason indicated
 *
 */
int
spanscls_start()
{
	int	err;

	/*
	 * Fill in union fields
	 */
	spanscls_attr.aal.v.aal4.forward_max_SDU_size = ATM_NIF_MTU;
	spanscls_attr.aal.v.aal4.backward_max_SDU_size = ATM_NIF_MTU;
	spanscls_attr.aal.v.aal4.SSCS_type = T_ATM_NULL;
	spanscls_attr.aal.v.aal4.mid_low = 0;
	spanscls_attr.aal.v.aal4.mid_high = 1023;

	/*
	 * Register our endpoint
	 */
	err = atm_endpoint_register(&spanscls_endpt);

	return (err);
}


/*
 * Process module unloading notification
 * 
 * Called whenever the spans module is about to be unloaded.  All signalling
 * instances will have been previously detached.  All spanscls resources
 * must be freed now.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
spanscls_stop()
{
	int	s = splnet();

	/*
	 * Tell ARP to stop
	 */
	spansarp_stop();

	/*
	 * Nothing should be left here...
	 */
	if (spanscls_head) {
		panic("spanscls_stop: bad state");
	}
	(void) splx(s);

	/*
	 * De-register ourselves
	 */
	(void) atm_endpoint_deregister(&spanscls_endpt);

	/*
	 * Free our storage pools
	 */
	atm_release_pool(&spanscls_pool);
}


/*
 * Process signalling interface attach
 * 
 * This function is called whenever a physical interface has been attached
 * to spans.  We will open the CLS PVC and await further events.
 *
 * Called at splnet.
 *
 * Arguments:
 *	spp	pointer to spans signalling protocol instance
 *
 * Returns:
 *	0	attach successful
 *	errno	attach failed - reason indicated
 *
 */
int
spanscls_attach(spp)
	struct spans	*spp;
{
	struct spanscls	*clp;
	Atm_addr_pvc	*pvcp;
	int	err;

	/*
	 * Get a new cls control block
	 */
	clp = (struct spanscls *)atm_allocate(&spanscls_pool);
	if (clp == NULL)
		return (ENOMEM);

	/*
	 * Initialize some stuff
	 */
	clp->cls_state = CLS_CLOSED;
	clp->cls_spans = spp;
	spp->sp_ipserv = &spanscls_ipserv;

	/*
	 * Fill out connection attributes
	 */
	spanscls_attr.nif = spp->sp_pif->pif_nif;
	spanscls_attr.traffic.v.forward.PCR_all_traffic = spp->sp_pif->pif_pcr;
	spanscls_attr.traffic.v.backward.PCR_all_traffic = spp->sp_pif->pif_pcr;
	spanscls_attr.called.addr.address_format = T_ATM_PVC_ADDR;
	spanscls_attr.called.addr.address_length = sizeof(Atm_addr_pvc);
	pvcp = (Atm_addr_pvc *)spanscls_attr.called.addr.address;
	ATM_PVC_SET_VPI(pvcp, SPANS_CLS_VPI);
	ATM_PVC_SET_VCI(pvcp, SPANS_CLS_VCI);
	spanscls_attr.called.subaddr.address_format = T_ATM_ABSENT;
	spanscls_attr.called.subaddr.address_length = 0;

	/*
	 * Create SPANS Connectionless Service (CLS) PVC
	 */
	err = atm_cm_connect(&spanscls_endpt, clp, &spanscls_attr,
			&clp->cls_conn);
	if (err) {
		atm_free((caddr_t)clp);
		return (err);
	}

	/*
	 * Set new state and link instance
	 */
	clp->cls_state = CLS_OPEN;
	LINK2TAIL(clp, struct spanscls, spanscls_head, cls_next);
	spp->sp_cls = clp;

	return (0);
}


/*
 * Process signalling interface detach
 * 
 * This function is called whenever a physical interface has been detached
 * from spans.  We will close the CLS PVC and clean up everything.
 *
 * Called at splnet.
 *
 * Arguments:
 *	spp	pointer to spans signalling protocol instance
 *
 * Returns:
 *	none
 *
 */
void
spanscls_detach(spp)
	struct spans	*spp;
{
	struct spanscls	*clp;

	/*
	 * Get our control block
	 */
	clp = spp->sp_cls;
	if (clp == NULL)
		return;

	/*
	 * Just checking up on things...
	 */
	if (clp->cls_ipnif)
		panic("spanscls_detach: IP interface still active");

	/*
	 * Close CLS PVC
	 */
	spanscls_closevc(clp, &spanscls_cause);

	/*
	 * Sever links and free server block, if possible
	 */
	clp->cls_spans = NULL;
	spp->sp_cls = NULL;
	if (clp->cls_state == CLS_CLOSED) {
		UNLINK(clp, struct spanscls, spanscls_head, cls_next);
		atm_free((caddr_t)clp);
	}
}


/*
 * Process IP Network Interface Activation
 * 
 * Called whenever an IP network interface becomes active.
 *
 * Called at splnet.
 *
 * Arguments:
 *	inp	pointer to IP network interface
 *
 * Returns:
 *	0 	command successful
 *	errno	command failed - reason indicated
 *
 */
static int
spanscls_ipact(inp)
	struct ip_nif	*inp;
{
	struct spans		*spp;
	struct spanscls		*clp;

	/*
	 * Get corresponding cls instance
	 */
	spp = (struct spans *)inp->inf_nif->nif_pif->pif_siginst;
	if ((spp == NULL) || ((clp = spp->sp_cls) == NULL))
		return (ENXIO);

	/*
	 * Make sure it's not already activated
	 */
	if (clp->cls_ipnif)
		return (EEXIST);

	/*
	 * Set two-way links with IP world
	 */
	clp->cls_ipnif = inp;
	inp->inf_isintf = (caddr_t)clp;

	/*
	 * Tell arp about new interface
	 */
	spansarp_ipact(clp);

	return (0);
}


/*
 * Process IP Network Interface Deactivation
 * 
 * Called whenever an IP network interface becomes inactive.
 *
 * Called at splnet.
 *
 * Arguments:
 *	inp	pointer to IP network interface
 *
 * Returns:
 *	0 	command successful
 *	errno	command failed - reason indicated
 *
 */
static int
spanscls_ipdact(inp)
	struct ip_nif	*inp;
{
	struct spanscls		*clp;

	/*
	 * Get cls instance and make sure it's been activated
	 */
	clp = (struct spanscls *)inp->inf_isintf;
	if ((clp == NULL) || (clp->cls_ipnif == NULL))
		return (ENXIO);

	/*
	 * Let arp know about this
	 */
	spansarp_ipdact(clp);

	/*
	 * Clear IP interface pointer
	 */
	clp->cls_ipnif = NULL;
	return (0);
}


/*
 * Output IP Broadcast Packet
 * 
 * Called whenever an IP broadcast packet is sent to this interface.
 *
 * Arguments:
 *	inp	pointer to IP network interface
 *	m	pointer to packet buffer chain
 *
 * Returns:
 *	0 	packet sent successfully
 *	errno	send failed - reason indicated
 *
 */
static int
spanscls_bcast_output(inp, m)
	struct ip_nif	*inp;
	KBuffer		*m;
{
	struct spans		*spp;
	struct spanscls		*clp;
	struct spanscls_hdr	*chp;
	int			err, space;

	/*
	 * Get cls instance and make sure it's been activated
	 */
	clp = (struct spanscls *)inp->inf_isintf;
	if ((clp == NULL) || (clp->cls_ipnif == NULL)) {
		KB_FREEALL(m);
		return (ENETDOWN);
	}

	/*
	 * Make sure that we know our addresses
	 */
	spp = clp->cls_spans;
	if (spp->sp_addr.address_format != T_ATM_SPANS_ADDR) {
		KB_FREEALL(m);
		return (ENETDOWN);
	}

	/*
	 * See if there's room to add CLS header to front of packet.
	 */
	KB_HEADROOM(m, space);
	if (space < sizeof(struct spanscls_hdr)) {
		KBuffer		*n;

		/*
		 * We have to allocate another buffer and tack it
		 * onto the front of the packet
		 */
		KB_ALLOCPKT(n, sizeof(struct spanscls_hdr),
			KB_F_NOWAIT, KB_T_HEADER);
		if (n == 0) {
			KB_FREEALL(m);
			return (ENOBUFS);
		}
		KB_TAILALIGN(n, sizeof(struct spanscls_hdr));
		KB_LINKHEAD(n, m);
		m = n;
	} else {
		/*
		 * Header fits, just adjust buffer controls
		 */
		KB_HEADADJ(m, sizeof(struct spanscls_hdr));
	}

	/*
	 * Now, build the CLS header
	 */
	KB_DATASTART(m, chp, struct spanscls_hdr *);
	spans_addr_copy(&spans_bcastaddr, &chp->ch_dst);
	spans_addr_copy(spp->sp_addr.address, &chp->ch_src);
	*(u_int *)&chp->ch_proto = *(u_int *)&spanscls_hdr.ch_proto;
	*(u_int *)&chp->ch_dsap = *(u_int *)&spanscls_hdr.ch_dsap;
	*(u_short *)&chp->ch_oui[1] = *(u_short *)&spanscls_hdr.ch_oui[1];
	chp->ch_pid = htons(ETHERTYPE_IP);

	if (spanscls_print)
		spanscls_pdu_print(clp, m, "output");

	/*
	 * Finally, send the pdu via the CLS service
	 */
	err = atm_cm_cpcs_data(clp->cls_conn, m);
	if (err) {
		KB_FREEALL(m);
		return (ENOBUFS);
	}

	return (0);
}


/*
 * Process VCC Input Data
 * 
 * All input packets received from CLS VCC lower layers are processed here.
 *
 * Arguments:
 *	tok	connection token (pointer to CLS VCC control block)
 *	m	pointer to input packet buffer chain
 *
 * Returns:
 *	none
 *
 */
static void
spanscls_cpcs_data(tok, m)
	void		*tok;
	KBuffer		*m;
{
	struct spanscls	*clp = tok;
	struct spans	*spp = clp->cls_spans;
	struct spanscls_hdr	*chp;
	struct ip_nif	*inp;

	/*
	 * Make sure we're ready
	 */
	if ((clp->cls_state != CLS_OPEN) || (spp->sp_state != SPANS_ACTIVE)) {
		KB_FREEALL(m);
		return;
	}

	if (spanscls_print)
		spanscls_pdu_print(clp, m, "input");

	/*
	 * Get CLS header into buffer
	 */
	if (KB_LEN(m) < sizeof(struct spanscls_hdr)) {
		KB_PULLUP(m, sizeof(struct spanscls_hdr), m);
		if (m == 0)
			return;
	}
	KB_DATASTART(m, chp, struct spanscls_hdr *);

	/*
	 * Verify packet information
	 */
	if ((*(u_int *)&chp->ch_proto != *(u_int *)&spanscls_hdr.ch_proto) ||
	    (*(u_int *)&chp->ch_dsap != *(u_int *)&spanscls_hdr.ch_dsap) ||
	    (*(u_short *)&chp->ch_oui[1] != 
				*(u_short *)&spanscls_hdr.ch_oui[1])) {

		/*
		 * Check for bridged PDU
		 */
		if (bcmp((char *)&chp->ch_proto, (char *)spanscls_bridged, 
				sizeof(spanscls_bridged))) {
			log(LOG_ERR, "spanscls_input: bad format\n");
			spanscls_pdu_print(clp, m, "input error"); 
		}

		KB_FREEALL(m);
		return;
	}

	/*
	 * Make sure packet is for us
	 */
	if (spans_addr_cmp(&chp->ch_dst, spp->sp_addr.address) &&
	    spans_addr_cmp(&chp->ch_dst, &spans_bcastaddr)) {
		KB_FREEALL(m);
		return;
	}

	/*
	 * Do protocol processing
	 */
	switch (ntohs(chp->ch_pid)) {

	case ETHERTYPE_IP:
		/*
		 * Drop CLS header
		 */
		KB_HEADADJ(m, -sizeof(struct spanscls_hdr));
		KB_PLENADJ(m, -sizeof(struct spanscls_hdr));

		/*
		 * Packet is ready for input to IP
		 */
		if ((inp = clp->cls_ipnif) != NULL)
			(void) (*inp->inf_ipinput)(inp, m);
		else
			KB_FREEALL(m);
		break;

	case ETHERTYPE_ARP:
		spansarp_input(clp, m);
		break;

	default:
		log(LOG_ERR, "spanscls_input: unknown protocol 0x%x\n",
			chp->ch_pid);
		KB_FREEALL(m);
		return;
	}
}


/*
 * Close a SPANS CLS VCC
 * 
 * This function will close a SPANS CLS VCC.
 *
 * Arguments:
 *	clp	pointer to CLS instance
 *	cause	pointer to cause code
 *
 * Returns:
 *	none
 *
 */
void
spanscls_closevc(clp, cause)
	struct spanscls	*clp;
	struct t_atm_cause	*cause;
{
	int	err;

	/*
	 * Close VCC
	 */
	if (clp->cls_conn) {
		err = atm_cm_release(clp->cls_conn, cause);
		if (err) {
			log(LOG_ERR, "spanscls_closevc: release err=%d\n", err);
		}
		clp->cls_conn = NULL;
	}

	clp->cls_state = CLS_CLOSED;
}


/*
 * Process CLS VCC Connected Notification
 * 
 * Arguments:
 *	toku	user's connection token (spanscls protocol block)
 *
 * Returns:
 *	none
 *
 */
static void
spanscls_connected(toku)
	void		*toku;
{
	/*
	 * We should never get one of these
	 */
	log(LOG_ERR, "spanscls: unexpected connected event\n");
}


/*
 * Process CLS VCC Cleared Notification
 * 
 * Arguments:
 *	toku	user's connection token (spanscls protocol block)
 *	cause	pointer to cause code
 *
 * Returns:
 *	none
 *
 */
static void
spanscls_cleared(toku, cause)
	void		*toku;
	struct t_atm_cause	*cause;
{
	struct spanscls	*clp = (struct spanscls *)toku;

	/*
	 * CLS VCC has been closed, so clean up our side
	 */
	clp->cls_conn = NULL;
	spanscls_closevc(clp, cause);
}


/*
 * Get Connection's Application/Owner Name
 * 
 * Arguments:
 *	tok	spanscls connection token
 *
 * Returns:
 *	addr	pointer to string containing our name
 *
 */
static caddr_t
spanscls_getname(tok)
	void		*tok;
{
	return ("SPANSCLS");
}


/*
 * Print a SPANS CLS PDU
 * 
 * Arguments:
 *	clp	pointer to cls instance
 *	m	pointer to pdu buffer chain
 *	msg	pointer to message string
 *
 * Returns:
 *	none
 *
 */
static void
spanscls_pdu_print(const struct spanscls *clp, const KBuffer *m,
    const char *msg)
{
	char		buf[128];

	snprintf(buf, sizeof(buf), "spanscls %s:\n", msg);
	atm_pdu_print(m, buf);
}
