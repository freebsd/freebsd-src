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
 * Core ATM Services
 * -----------------
 *
 * ATM Connection Manager
 *
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Global variables
 */
struct atm_cm_stat	atm_cm_stat = {0};

/*
 * Local functions
 */
static void		atm_cm_cpcs_upper __P((int, void *, int, int));
static void		atm_cm_saal_upper __P((int, void *, int, int));
static void		atm_cm_sscop_upper __P((int, void *, int, int));
static Atm_connvc *	atm_cm_share_llc __P((Atm_attributes *));
static void		atm_cm_closeconn __P((Atm_connection *,
				struct t_atm_cause *));
static void		atm_cm_closevc __P((Atm_connvc *));
static void		atm_cm_timeout __P((struct atm_time *));
static KTimeout_ret	atm_cm_procinq __P((void *));
static void		atm_cm_incall __P((Atm_connvc *));
static int		atm_cm_accept __P((Atm_connvc *, Atm_connection *));

/*
 * Local variables
 */
static Queue_t		atm_connection_queue = {NULL};
static Queue_t		atm_incoming_queue = {NULL};
static int		atm_incoming_qlen = 0;
static Atm_connection	*atm_listen_queue = NULL;
static struct attr_cause	atm_cause_tmpl = 
	{T_ATM_PRESENT, {T_ATM_ITU_CODING, T_ATM_LOC_USER, 0, {0, 0, 0, 0}}};

/*
 * Stack commands, indexed by API
 */
static struct {
	int		init;
	int		term;
} atm_stackcmds[] = {
	{CPCS_INIT, CPCS_TERM},		/* CMAPI_CPCS */
	{SSCF_UNI_INIT, SSCF_UNI_TERM},	/* CMAPI_SAAL */
	{SSCOP_INIT, SSCOP_TERM},	/* CMAPI_SSCOP */
};


static struct sp_info          atm_connection_pool = {
	"atm connection pool",		/* si_name */
	sizeof(Atm_connection),		/* si_blksiz */
	10,				/* si_blkcnt */
	100				/* si_maxallow */
};
static struct sp_info          atm_connvc_pool = {
	"atm connection vcc pool",	/* si_name */
	sizeof(Atm_connvc),		/* si_blksiz */
	10,				/* si_blkcnt */
	100				/* si_maxallow */
};


/*
 * Initiate Outgoing ATM Call
 * 
 * Called by an endpoint service to create a new Connection Manager API
 * instance and to initiate an outbound ATM connection.  The endpoint
 * provided token will be used in all further CM -> endpoint function
 * calls, and the returned connection block pointer must be used in all
 * subsequent endpoint -> CM function calls.
 *
 * If the return indicates that the connection setup has been immediately
 * successful (typically only for PVCs and shared SVCs), then the connection
 * is ready for data transmission.
 *
 * If the return indicates that the connection setup is still in progress,
 * then the endpoint must wait for notification from the Connection Manager
 * indicating the final status of the call setup.  If the call setup completes
 * successfully, then a "call connected" notification will be sent to the
 * endpoint by the Connection Manager.  If the call setup fails, then the
 * endpoint will receive a "call cleared" notification.
 *
 * All connection instances must be freed with an atm_cm_release() call.
 *
 * Arguments:
 *	epp	pointer to endpoint definition structure
 *	token	endpoint's connection instance token
 *	ap	pointer to requested connection attributes
 *	copp	pointer to location to return allocated connection block
 *
 * Returns:
 *	0	connection has been successfully established
 *	EINPROGRESS	connection establishment is in progress
 *	errno	connection failed - reason indicated
 *
 */
int
atm_cm_connect(epp, token, ap, copp)
	Atm_endpoint	*epp;
	void		*token;
	Atm_attributes	*ap;
	Atm_connection	**copp;
{
	Atm_connection	*cop;
	Atm_connvc	*cvp;
	struct atm_pif	*pip;
	struct sigmgr	*smp;
	struct stack_list	sl;
	void		(*upf)__P((int, void *, int, int));
	int		s, sli, err, err2;

	*copp = NULL;
	cvp = NULL;

	/*
	 * Get a connection block
	 */
	cop = (Atm_connection *)atm_allocate(&atm_connection_pool);
	if (cop == NULL)
		return (ENOMEM);

	/*
	 * Initialize connection info
	 */
	cop->co_endpt = epp;
	cop->co_toku = token;

	/*
	 * Initialize stack list index
	 */
	sli = 0;

	/*
	 * Validate and extract useful attribute information
	 */

	/*
	 * Must specify a network interface (validated below)
	 */
	if (ap->nif == NULL) {
		err = EINVAL;
		goto done;
	}

	/*
	 * Check out Data API
	 */
	switch (ap->api) {

	case CMAPI_CPCS:
		upf = atm_cm_cpcs_upper;
		break;

	case CMAPI_SAAL:
		sl.sl_sap[sli++] = SAP_SSCF_UNI;
		sl.sl_sap[sli++] = SAP_SSCOP;
		upf = atm_cm_saal_upper;
		break;

	case CMAPI_SSCOP:
		sl.sl_sap[sli++] = SAP_SSCOP;
		upf = atm_cm_sscop_upper;
		break;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * AAL Attributes
	 */
	if (ap->aal.tag != T_ATM_PRESENT) {
		err = EINVAL;
		goto done;
	}

	switch (ap->aal.type) {

	case ATM_AAL5:
		sl.sl_sap[sli++] = SAP_CPCS_AAL5;
		sl.sl_sap[sli++] = SAP_SAR_AAL5;
		sl.sl_sap[sli++] = SAP_ATM;
		break;

	case ATM_AAL3_4:
		sl.sl_sap[sli++] = SAP_CPCS_AAL3_4;
		sl.sl_sap[sli++] = SAP_SAR_AAL3_4;
		sl.sl_sap[sli++] = SAP_ATM;
		break;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * Broadband Bearer Attributes
	 */
	if (ap->bearer.tag != T_ATM_PRESENT) {
		err = EINVAL;
		goto done;
	}

	switch (ap->bearer.v.connection_configuration) {

	case T_ATM_1_TO_1:
		cop->co_flags |= COF_P2P;
		break;

	case T_ATM_1_TO_MANY:
		/* Not supported */
		cop->co_flags |= COF_P2MP;
		err = EINVAL;
		goto done;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * Logical Link Control Attributes
	 */
	if (ap->llc.tag == T_ATM_PRESENT) {
		if ((ap->blli.tag_l2 != T_ATM_PRESENT) ||
		    (ap->blli.v.layer_2_protocol.ID_type != T_ATM_SIMPLE_ID) ||
		    (ap->blli.v.layer_2_protocol.ID.simple_ID != 
				T_ATM_BLLI2_I8802) ||
		    (ap->llc.v.llc_len < T_ATM_LLC_MIN_LEN) ||
		    (ap->llc.v.llc_len > T_ATM_LLC_MAX_LEN)) {
			err = EINVAL;
			goto done;
		}
		cop->co_mpx = ATM_ENC_LLC;
		cop->co_llc = ap->llc;
	} else
		cop->co_mpx = ATM_ENC_NULL;

	/*
	 * Called Party Attributes
	 */
	if (ap->called.tag != T_ATM_PRESENT) {
		err = EINVAL;
		goto done;
	}

	if ((ap->called.addr.address_format == T_ATM_ABSENT) ||
	    (ap->called.addr.address_length == 0)) {
		err = EINVAL;
		goto done;
	}

	/*
	 * Calling Party Attributes
	 */
	if (ap->calling.tag != T_ATM_ABSENT) {
		err = EINVAL;
		goto done;
	}

	/*
	 * Quality of Service Attributes
	 */
	if (ap->qos.tag != T_ATM_PRESENT) {
		err = EINVAL;
		goto done;
	}

	/*
	 * Terminate stack list 
	 */
	sl.sl_sap[sli] = 0;

	s = splnet();

	/*
	 * Let multiplexors decide whether we need a new VCC
	 */
	switch (cop->co_mpx) {

	case ATM_ENC_NULL:
		/*
		 * All of these connections require a new VCC
		 */
		break;

	case ATM_ENC_LLC:
		/*
		 * See if we can share an existing LLC connection
		 */
		cvp = atm_cm_share_llc(ap);
		if (cvp == NULL)
			break;

		/*
		 * We've got a connection to share
		 */
		cop->co_connvc = cvp;
		if (cvp->cvc_state == CVCS_ACTIVE) {
			cop->co_state = COS_ACTIVE;
			err = 0;
		} else {
			cop->co_state = COS_OUTCONN;
			err = EINPROGRESS;
		}
		LINK2TAIL(cop, Atm_connection, cvp->cvc_conn->co_mxh, co_next);
		cop->co_mxh = cvp->cvc_conn->co_mxh;
		*copp = cop;

		(void) splx(s);
		return (err);

	default:
		panic("atm_cm_connect: unknown mpx");
	}

	/*
	 * If we get here, it means we need to create 
	 * a new VCC for this connection
	 */

	/*
	 * Validate that network interface is registered and that 
	 * a signalling manager is attached
	 */
	for (pip = atm_interface_head; pip != NULL; pip = pip->pif_next) {
		struct atm_nif	*nip;
		for (nip = pip->pif_nif; nip; nip = nip->nif_pnext) {
			if (nip == ap->nif)
				break;
		}
		if (nip)
			break;
	}
	if (pip == NULL) {
		err = ENXIO;
		goto donex;
	}

	if ((smp = pip->pif_sigmgr) == NULL) {
		err = ENXIO;
		goto donex;
	}

	/*
	 * Get a connection VCC block
	 */
	cvp = (Atm_connvc *)atm_allocate(&atm_connvc_pool);
	if (cvp == NULL) {
		err = ENOMEM;
		goto donex;
	}

	/*
	 * Save VCC attributes
	 */
	cvp->cvc_attr = *ap;
	cvp->cvc_flags |= CVCF_CALLER;

	/*
	 * Link the control blocks
	 */
	cop->co_connvc = cvp;
	cvp->cvc_conn = cop;
	cvp->cvc_sigmgr = smp;

	/*
	 * Create a service stack
	 */
	err = atm_create_stack(cvp, &sl, upf);
	if (err) {
		cvp->cvc_state = CVCS_CLEAR;
		atm_cm_closevc(cvp);
		goto donex;
	}

	/*
	 * Let the signalling manager handle the VCC creation
	 */
	cvp->cvc_state = CVCS_SETUP;
	switch ((*smp->sm_setup)(cvp, &err)) {

	case CALL_CONNECTED:
		/*
		 * Connection is fully setup - initialize the stack
		 */
		cvp->cvc_state = CVCS_INIT;
		STACK_CALL(atm_stackcmds[ap->api].init, cvp->cvc_lower,
			cvp->cvc_tokl, cvp, ap->api_init, 0, err2);
		if (err2)
			panic("atm_cm_connect: init");

		if (cvp->cvc_flags & CVCF_ABORTING) {
			/*
			 * Someone on the stack bailed out...schedule the 
			 * VCC and stack termination
			 */
			atm_cm_closevc(cvp);
			err = EFAULT;
		} else {
			/*
			 * Everything looks fine from here
			 */
			cvp->cvc_state = CVCS_ACTIVE;
			cop->co_state = COS_ACTIVE;
		}
		break;

	case CALL_FAILED:
		/*
		 * Terminate stack and clean up before we leave
		 */
		cvp->cvc_state = CVCS_CLEAR;
		atm_cm_closevc(cvp);
		break;

	case CALL_PROCEEDING:
		/*
		 * We'll just wait for final call status
		 */
		cop->co_state = COS_OUTCONN;
		err = EINPROGRESS;
		break;

	default:
		panic("atm_cm_connect: setup");
	}

donex:
	(void) splx(s);

done:
	if (err && err != EINPROGRESS) {
		/*
		 * Undo any partial setup stuff
		 */
		if (cop)
			atm_free((caddr_t)cop);
	} else {
		/*
		 * Finish connection setup
		 */
		s = splnet();
		cvp->cvc_flags |= CVCF_CONNQ;
		ENQUEUE(cvp, Atm_connvc, cvc_q, atm_connection_queue);
		LINK2TAIL(cop, Atm_connection, cop->co_mxh, co_next);
		(void) splx(s);
		*copp = cop;
	}
	return (err);
}


/*
 * Listen for Incoming ATM Calls
 * 
 * Called by an endpoint service in order to indicate its willingness to
 * accept certain incoming calls.  The types of calls which the endpoint
 * is prepared to accept are specified in the Atm_attributes parameter.
 *
 * For each call which meets the criteria specified by the endpoint, the
 * endpoint service will receive an incoming call notification via the
 * endpoint's ep_incoming() function.
 *
 * To cancel the listening connection, the endpoint user should invoke 
 * atm_cm_release().
 *
 * Arguments:
 *	epp	pointer to endpoint definition structure
 *	token	endpoint's listen instance token
 *	ap	pointer to listening connection attributes
 *	copp	pointer to location to return allocated connection block
 *
 * Returns:
 *	0	listening connection installed
 *	errno	listen failed - reason indicated
 *
 */
int
atm_cm_listen(epp, token, ap, copp)
	Atm_endpoint	*epp;
	void		*token;
	Atm_attributes	*ap;
	Atm_connection	**copp;
{
	Atm_connection	*cop;
	int		s, err = 0;

	*copp = NULL;

	/*
	 * Get a connection block
	 */
	cop = (Atm_connection *)atm_allocate(&atm_connection_pool);
	if (cop == NULL)
		return (ENOMEM);

	/*
	 * Initialize connection info
	 */
	cop->co_endpt = epp;
	cop->co_toku = token;
	cop->co_mxh = cop;

	/*
	 * Validate and extract useful attribute information
	 */

	/*
	 * Check out Data API
	 */
	switch (ap->api) {

	case CMAPI_CPCS:
	case CMAPI_SAAL:
	case CMAPI_SSCOP:
		break;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * AAL Attributes
	 */
	switch (ap->aal.tag) {

	case T_ATM_PRESENT:

		switch (ap->aal.type) {

		case ATM_AAL5:
		case ATM_AAL3_4:
			break;

		default:
			err = EINVAL;
			goto done;
		}
		break;

	case T_ATM_ABSENT:
	case T_ATM_ANY:
		break;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * Broadband High Layer Information Attributes
	 */
	switch (ap->bhli.tag) {

	case T_ATM_PRESENT:
	case T_ATM_ABSENT:
	case T_ATM_ANY:
		break;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * Broadband Low Layer Information Attributes
	 */
	switch (ap->blli.tag_l2) {

	case T_ATM_PRESENT:
	case T_ATM_ABSENT:
	case T_ATM_ANY:
		break;

	default:
		err = EINVAL;
		goto done;
	}

	switch (ap->blli.tag_l3) {

	case T_ATM_PRESENT:
	case T_ATM_ABSENT:
	case T_ATM_ANY:
		break;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * Logical Link Control Attributes
	 */
	switch (ap->llc.tag) {

	case T_ATM_PRESENT:
		if ((ap->blli.tag_l2 != T_ATM_PRESENT) ||
		    (ap->blli.v.layer_2_protocol.ID_type != T_ATM_SIMPLE_ID) ||
		    (ap->blli.v.layer_2_protocol.ID.simple_ID != 
				T_ATM_BLLI2_I8802) ||
		    (ap->llc.v.llc_len < T_ATM_LLC_MIN_LEN) ||
		    (ap->llc.v.llc_len > T_ATM_LLC_MAX_LEN)) {
			err = EINVAL;
			goto done;
		}
		cop->co_mpx = ATM_ENC_LLC;
		cop->co_llc = ap->llc;
		break;

	case T_ATM_ABSENT:
	case T_ATM_ANY:
		cop->co_mpx = ATM_ENC_NULL;
		break;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * Called Party Attributes
	 */
	switch (ap->called.tag) {

	case T_ATM_PRESENT:
		switch (ap->called.addr.address_format) {

		case T_ATM_ABSENT:
			ap->called.tag = T_ATM_ABSENT;
			break;

		case T_ATM_PVC_ADDR:
			err = EINVAL;
			goto done;
		}
		break;

	case T_ATM_ABSENT:
	case T_ATM_ANY:
		break;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * Get an attribute block and save listening attributes
	 */
	cop->co_lattr = (Atm_attributes *)atm_allocate(&atm_attributes_pool);
	if (cop->co_lattr == NULL) {
		err = ENOMEM;
		goto done;
	}
	*cop->co_lattr = *ap;

	/*
	 * Now try to register the listening connection
	 */
	s = splnet();
	if (atm_cm_match(cop->co_lattr, NULL) != NULL) {
		/*
		 * Can't have matching listeners
		 */
		err = EADDRINUSE;
		goto donex;
	}
	cop->co_state = COS_LISTEN;
	LINK2TAIL(cop, Atm_connection, atm_listen_queue, co_next);

donex:
	(void) splx(s);

done:
	if (err) {
		/*
		 * Undo any partial setup stuff
		 */
		if (cop) {
			if (cop->co_lattr)
				atm_free((caddr_t)cop->co_lattr);
			atm_free((caddr_t)cop);
		}
	} else {
		/*
		 * Finish connection setup
		 */
		*copp = cop;
	}
	return (err);
}


/*
 * Add to LLC Connection
 * 
 * Called by an endpoint service to create a new Connection Manager API
 * instance to be associated with an LLC-multiplexed connection instance
 * which has been previously created.  The endpoint provided token will
 * be used in all further CM -> endpoint function calls, and the returned
 * connection block pointer must be used in all subsequent endpoint -> CM
 * function calls.
 *
 * If the return indicates that the connection setup has been immediately
 * successful, then the connection is ready for data transmission.
 *
 * If the return indicates that the connection setup is still in progress,
 * then the endpoint must wait for notification from the Connection Manager
 * indicating the final status of the call setup.  If the call setup completes
 * successfully, then a "call connected" notification will be sent to the
 * endpoint by the Connection Manager.  If the call setup fails, then the
 * endpoint will receive a "call cleared" notification.
 *
 * All connection instances must be freed with an atm_cm_release() call.
 *
 * Arguments:
 *	epp	pointer to endpoint definition structure
 *	token	endpoint's connection instance token
 *	llc	pointer to llc attributes for new connection
 *	ecop	pointer to existing connection block
 *	copp	pointer to location to return allocated connection block
 *
 * Returns:
 *	0	connection has been successfully established
 *	EINPROGRESS	connection establishment is in progress
 *	errno	addllc failed - reason indicated
 *
 */
int
atm_cm_addllc(epp, token, llc, ecop, copp)
	Atm_endpoint	*epp;
	void		*token;
	struct attr_llc	*llc;
	Atm_connection	*ecop;
	Atm_connection	**copp;
{
	Atm_connection	*cop, *cop2;
	Atm_connvc	*cvp;
	int		s, err;

	*copp = NULL;

	/*
	 * Check out requested LLC attributes
	 */
	if ((llc->tag != T_ATM_PRESENT) ||
	    ((llc->v.flags & T_ATM_LLC_SHARING) == 0) ||
	    (llc->v.llc_len < T_ATM_LLC_MIN_LEN) ||
	    (llc->v.llc_len > T_ATM_LLC_MAX_LEN))
		return (EINVAL);

	/*
	 * Get a connection block
	 */
	cop = (Atm_connection *)atm_allocate(&atm_connection_pool);
	if (cop == NULL)
		return (ENOMEM);

	/*
	 * Initialize connection info
	 */
	cop->co_endpt = epp;
	cop->co_toku = token;
	cop->co_llc = *llc;

	s = splnet();

	/*
	 * Ensure that supplied connection is really valid
	 */
	cop2 = NULL;
	for (cvp = Q_HEAD(atm_connection_queue, Atm_connvc); cvp;
			cvp = Q_NEXT(cvp, Atm_connvc, cvc_q)) {
		for (cop2 = cvp->cvc_conn; cop2; cop2 = cop2->co_next) {
			if (ecop == cop2)
				break;
		}
		if (cop2)
			break;
	}
	if (cop2 == NULL) {
		err = ENOENT;
		goto done;
	}

	switch (ecop->co_state) {

	case COS_OUTCONN:
	case COS_INACCEPT:
		err = EINPROGRESS;
		break;

	case COS_ACTIVE:
		err = 0;
		break;

	default:
		err = EINVAL;
		goto done;
	}

	/*
	 * Connection must be LLC multiplexed and shared
	 */
	if ((ecop->co_mpx != ATM_ENC_LLC) ||
	    ((ecop->co_llc.v.flags & T_ATM_LLC_SHARING) == 0)) {
		err = EINVAL;
		goto done;
	}

	/*
	 * This new LLC header must be unique for this VCC
	 */
	cop2 = ecop->co_mxh;
	while (cop2) {
		int	i = MIN(llc->v.llc_len, cop2->co_llc.v.llc_len);

		if (KM_CMP(llc->v.llc_info, cop2->co_llc.v.llc_info, i) == 0) {
			err = EINVAL;
			goto done;
		}

		cop2 = cop2->co_next;
	}

	/*
	 * Everything seems to check out
	 */
	cop->co_flags = ecop->co_flags;
	cop->co_state = ecop->co_state;
	cop->co_mpx = ecop->co_mpx;
	cop->co_connvc = ecop->co_connvc;

	LINK2TAIL(cop, Atm_connection, ecop->co_mxh, co_next);
	cop->co_mxh = ecop->co_mxh;

done:
	(void) splx(s);

	if (err && err != EINPROGRESS) {
		/*
		 * Undo any partial setup stuff
		 */
		if (cop)
			atm_free((caddr_t)cop);
	} else {
		/*
		 * Pass new connection back to caller
		 */
		*copp = cop;
	}
	return (err);
}


/*
 * XXX
 * 
 * Arguments:
 *	cop	pointer to connection block
 *	id	identifier for party to be added
 *	addr	address of party to be added
 *
 * Returns:
 *	0	addparty successful
 *	errno	addparty failed - reason indicated
 *
 */
int
atm_cm_addparty(cop, id, addr)
	Atm_connection	*cop;
	int		id;
	struct t_atm_sap	*addr;
{
	return (0);
}


/*
 * XXX
 * 
 * Arguments:
 *	cop	pointer to connection block
 *	id	identifier for party to be added
 *	cause	pointer to cause of drop
 *
 * Returns:
 *	0	dropparty successful
 *	errno	dropparty failed - reason indicated
 *
 */
int
atm_cm_dropparty(cop, id, cause)
	Atm_connection	*cop;
	int		id;
	struct t_atm_cause	*cause;
{
	return (0);
}


/*
 * Release Connection Resources
 * 
 * Called by the endpoint service in order to terminate an ATM connection 
 * and to release all system resources for the connection.  This function
 * must be called for every allocated connection instance and must only 
 * be called by the connection's owner.
 *
 * Arguments:
 *	cop	pointer to connection block
 *	cause	pointer to cause of release
 *
 * Returns:
 *	0	release successful
 *	errno	release failed - reason indicated
 *
 */
int
atm_cm_release(cop, cause)
	Atm_connection	*cop;
	struct t_atm_cause	*cause;
{
	Atm_connvc	*cvp;
	int		s;

	s = splnet();

	/*
	 * First, a quick state validation check
	 */
	switch (cop->co_state) {

	case COS_OUTCONN:
	case COS_LISTEN:
	case COS_INACCEPT:
	case COS_ACTIVE:
	case COS_CLEAR:
		/*
		 * Break link to user
		 */
		cop->co_toku = NULL;
		break;

	case COS_INCONN:
		(void) splx(s);
		return (EFAULT);

	default:
		panic("atm_cm_release: bogus conn state");
	}

	/*
	 * Check out the VCC state too
	 */
	if ((cvp = cop->co_connvc) != NULL) {

		switch (cvp->cvc_state) {

		case CVCS_SETUP:
		case CVCS_INIT:
		case CVCS_ACCEPT:
		case CVCS_ACTIVE:
			break;

		case CVCS_INCOMING:
			(void) splx(s);
			return (EFAULT);

		case CVCS_CLEAR:
			(void) splx(s);
			return (EALREADY);

		default:
			panic("atm_cm_release: bogus connvc state");
		}

		/*
		 * If we're the only connection, terminate the VCC
		 */
		if ((cop->co_mxh == cop) && (cop->co_next == NULL)) {
			cvp->cvc_attr.cause.tag = T_ATM_PRESENT;
			cvp->cvc_attr.cause.v = *cause;
			atm_cm_closevc(cvp);
		}
	}

	/*
	 * Now get rid of the connection
	 */
	atm_cm_closeconn(cop, cause);

	return (0);
}


/*
 * Abort an ATM Connection VCC
 * 
 * This function allows any non-owner kernel entity to request an 
 * immediate termination of an ATM VCC.  This will normally be called
 * when encountering a catastrophic error condition that cannot be 
 * resolved via the available stack protocols.  The connection manager 
 * will schedule the connection's termination, including notifying the 
 * connection owner of the termination.  
 *
 * This function should only be called by a stack entity instance.  After 
 * calling the function, the caller should set a protocol state which just 
 * waits for a <sap>_TERM stack command to be delivered.
 *
 * Arguments:
 *	cvp	pointer to connection VCC block
 *	cause	pointer to cause of abort
 *
 * Returns:
 *	0	abort successful
 *	errno	abort failed - reason indicated
 *
 */
int
atm_cm_abort(cvp, cause)
	Atm_connvc	*cvp;
	struct t_atm_cause	*cause;
{
	ATM_DEBUG2("atm_cm_abort: cvp=%p cause=%d\n",
		cvp, cause->cause_value);

	/*
	 * Note that we're aborting
	 */
	cvp->cvc_flags |= CVCF_ABORTING;

	switch (cvp->cvc_state) {

	case CVCS_INIT:
		/*
		 * In-line code will handle this
		 */
		cvp->cvc_attr.cause.tag = T_ATM_PRESENT;
		cvp->cvc_attr.cause.v = *cause;
		break;

	case CVCS_SETUP:
	case CVCS_ACCEPT:
	case CVCS_ACTIVE:
		/*
		 * Schedule connection termination, since we want
		 * to avoid any sequencing interactions
		 */
		cvp->cvc_attr.cause.tag = T_ATM_PRESENT;
		cvp->cvc_attr.cause.v = *cause;
		CVC_TIMER(cvp, 0);
		break;

	case CVCS_REJECT:
	case CVCS_RELEASE:
	case CVCS_CLEAR:
	case CVCS_TERM:
		/*
		 * Ignore abort, as we're already terminating
		 */
		break;

	default:
		log(LOG_ERR,
			"atm_cm_abort: invalid state: cvp=%p, state=%d\n",
			cvp, cvp->cvc_state);
	}
	return (0);
}


/*
 * Incoming ATM Call Received
 * 
 * Called by a signalling manager to indicate that a new call request has
 * been received.  This function will allocate and initialize the connection
 * manager control blocks and queue this call request.  The call request 
 * processing function, atm_cm_procinq(), will be scheduled to perform the
 * call processing.
 *
 * Arguments:
 *	vcp	pointer to incoming call's VCC control block
 *	ap	pointer to incoming call's attributes
 *
 * Returns:
 *	0	call queuing successful
 *	errno	call queuing failed - reason indicated
 *
 */
int
atm_cm_incoming(vcp, ap)
	struct vccb	*vcp;
	Atm_attributes	*ap;
{
	Atm_connvc	*cvp;
	int		s, err;


	/*
	 * Do some minimal attribute validation
	 */

	/*
	 * Must specify a network interface
	 */
	if (ap->nif == NULL)
		return (EINVAL);

	/*
	 * AAL Attributes
	 */
	if ((ap->aal.tag != T_ATM_PRESENT) ||
	    ((ap->aal.type != ATM_AAL5) &&
	     (ap->aal.type != ATM_AAL3_4)))
		return (EINVAL);

	/*
	 * Traffic Descriptor Attributes
	 */
	if ((ap->traffic.tag != T_ATM_PRESENT) &&
	    (ap->traffic.tag != T_ATM_ABSENT))
		return (EINVAL);

	/*
	 * Broadband Bearer Attributes
	 */
	if ((ap->bearer.tag != T_ATM_PRESENT) ||
	    ((ap->bearer.v.connection_configuration != T_ATM_1_TO_1) &&
	     (ap->bearer.v.connection_configuration != T_ATM_1_TO_MANY)))
		return (EINVAL);

	/*
	 * Broadband High Layer Attributes
	 */
	if ((ap->bhli.tag != T_ATM_PRESENT) &&
	    (ap->bhli.tag != T_ATM_ABSENT))
		return (EINVAL);

	/*
	 * Broadband Low Layer Attributes
	 */
	if ((ap->blli.tag_l2 != T_ATM_PRESENT) &&
	    (ap->blli.tag_l2 != T_ATM_ABSENT))
		return (EINVAL);
	if ((ap->blli.tag_l3 != T_ATM_PRESENT) &&
	    (ap->blli.tag_l3 != T_ATM_ABSENT))
		return (EINVAL);

	/*
	 * Logical Link Control Attributes
	 */
	if (ap->llc.tag == T_ATM_PRESENT)
		return (EINVAL);
	ap->llc.tag = T_ATM_ANY;

	/*
	 * Called Party Attributes
	 */
	if ((ap->called.tag != T_ATM_PRESENT) ||
	    (ap->called.addr.address_format == T_ATM_ABSENT))
		return (EINVAL);
	if (ap->called.tag == T_ATM_ABSENT) {
		ap->called.addr.address_format = T_ATM_ABSENT;
		ap->called.addr.address_length = 0;
		ap->called.subaddr.address_format = T_ATM_ABSENT;
		ap->called.subaddr.address_length = 0;
	}

	/*
	 * Calling Party Attributes
	 */
	if ((ap->calling.tag != T_ATM_PRESENT) &&
	    (ap->calling.tag != T_ATM_ABSENT))
		return (EINVAL);
	if (ap->calling.tag == T_ATM_ABSENT) {
		ap->calling.addr.address_format = T_ATM_ABSENT;
		ap->calling.addr.address_length = 0;
		ap->calling.subaddr.address_format = T_ATM_ABSENT;
		ap->calling.subaddr.address_length = 0;
	}

	/*
	 * Quality of Service Attributes
	 */
	if (ap->qos.tag != T_ATM_PRESENT)
		return (EINVAL);

	/*
	 * Transit Network Attributes
	 */
	if ((ap->transit.tag != T_ATM_PRESENT) &&
	    (ap->transit.tag != T_ATM_ABSENT))
		return (EINVAL);

	/*
	 * Cause Attributes
	 */
	if ((ap->cause.tag != T_ATM_PRESENT) &&
	    (ap->cause.tag != T_ATM_ABSENT))
		return (EINVAL);

	/*
	 * Get a connection VCC block
	 */
	cvp = (Atm_connvc *)atm_allocate(&atm_connvc_pool);
	if (cvp == NULL) {
		err = ENOMEM;
		goto fail;
	}

	/*
	 * Initialize the control block
	 */
	cvp->cvc_vcc = vcp;
	cvp->cvc_sigmgr = vcp->vc_pif->pif_sigmgr;
	cvp->cvc_attr = *ap;
	cvp->cvc_state = CVCS_INCOMING;

	/*
	 * Control queue length
	 */
	s = splnet();
	if (atm_incoming_qlen >= ATM_CALLQ_MAX) {
		(void) splx(s);
		err = EBUSY;
		goto fail;
	}

	/*
	 * Queue request and schedule call processing function
	 */
	cvp->cvc_flags |= CVCF_INCOMQ;
	ENQUEUE(cvp, Atm_connvc, cvc_q, atm_incoming_queue);
	if (atm_incoming_qlen++ == 0) {
		timeout(atm_cm_procinq, (void *)0, 0);
	}

	/*
	 * Link for signalling manager
	 */
	vcp->vc_connvc = cvp;

	(void) splx(s);

	return (0);

fail:
	/*
	 * Free any resources
	 */
	if (cvp)
		atm_free((caddr_t)cvp);
	return (err);
}


/*
 * VCC Connected Notification
 * 
 * This function is called by a signalling manager as notification that a
 * VCC call setup has been successful.
 *
 * Arguments:
 *	cvp	pointer to connection VCC block
 *
 * Returns:
 *	none
 *
 */
void
atm_cm_connected(cvp)
	Atm_connvc	*cvp;
{
	Atm_connection	*cop, *cop2;
	KBuffer		*m;
	int		s, err;

	s = splnet();

	/*
	 * Validate connection vcc
	 */
	switch (cvp->cvc_state) {

	case CVCS_SETUP:
		/*
		 * Initialize the stack
		 */
		cvp->cvc_state = CVCS_INIT;
		STACK_CALL(atm_stackcmds[cvp->cvc_attr.api].init,
				cvp->cvc_lower, cvp->cvc_tokl,
				cvp, cvp->cvc_attr.api_init, 0, err);
		if (err)
			panic("atm_cm_connected: init");

		if (cvp->cvc_flags & CVCF_ABORTING) {
			/*
			 * Someone on the stack bailed out...notify all of the
			 * connections and schedule the VCC termination
			 */
			cop = cvp->cvc_conn;
			while (cop) {
				cop2 = cop->co_next;
				atm_cm_closeconn(cop, &cvp->cvc_attr.cause.v);
				cop = cop2;
			}
			atm_cm_closevc(cvp);
			(void) splx(s);
			return;
		}
		break;

	case CVCS_ACCEPT:
		/*
		 * Stack already initialized
		 */
		break;

	default:
		panic("atm_cm_connected: connvc state");
	}

	/*
	 * VCC is ready for action
	 */
	cvp->cvc_state = CVCS_ACTIVE;

	/*
	 * Notify all connections that the call has completed
	 */
	cop = cvp->cvc_conn;
	while (cop) {
		cop2 = cop->co_next;

		switch (cop->co_state) {

		case COS_OUTCONN:
		case COS_INACCEPT:
			cop->co_state = COS_ACTIVE;
			(*cop->co_endpt->ep_connected)(cop->co_toku);
			break;

		case COS_ACTIVE:
			/*
			 * May get here if an ep_connected() call (from
			 * above) results in an atm_cm_addllc() call for 
			 * the just connected connection.
			 */
			break;

		default:
			panic("atm_cm_connected: connection state");
		}

		cop = cop2;
	}

	(void) splx(s);

	/*
	 * Input any queued packets
	 */
	while ((m = cvp->cvc_rcvq) != NULL) {
		cvp->cvc_rcvq = KB_QNEXT(m);
		cvp->cvc_rcvqlen--;
		KB_QNEXT(m) = NULL;

		/*
		 * Currently only supported for CPCS API
		 */
		atm_cm_cpcs_upper(CPCS_UNITDATA_SIG, cvp, (int)m, 0);
	}

	return;
}


/*
 * VCC Cleared Notification
 * 
 * This function is called by a signalling manager as notification that a
 * VCC call has been cleared.  The cause information describing the reason
 * for the call clearing will be contained in the connection VCC attributes.
 *
 * Arguments:
 *	cvp	pointer to connection VCC block
 *
 * Returns:
 *	none
 *
 */
void
atm_cm_cleared(cvp)
	Atm_connvc	*cvp;
{
	Atm_connection	*cop, *cop2;
	int		s;

#ifdef DIAGNOSTIC
	if ((cvp->cvc_state == CVCS_FREE) ||
	    (cvp->cvc_state >= CVCS_CLEAR))
		panic("atm_cm_cleared");
#endif

	cvp->cvc_state = CVCS_CLEAR;

	s = splnet();

	/*
	 * Terminate all connections
	 */
	cop = cvp->cvc_conn;
	while (cop) {
		cop2 = cop->co_next;
		atm_cm_closeconn(cop, &cvp->cvc_attr.cause.v);
		cop = cop2;
	}

	/*
	 * Clean up connection VCC
	 */
	atm_cm_closevc(cvp);

	(void) splx(s);

	return;
}


/*
 * Process Incoming Call Queue
 * 
 * This function is scheduled by atm_cm_incoming() in order to process
 * all the entries on the incoming call queue.
 *
 * Arguments:
 *	arg	argument passed on timeout() call
 *
 * Returns:
 *	none
 *
 */
static KTimeout_ret
atm_cm_procinq(arg)
	void	*arg;
{
	Atm_connvc	*cvp;
	int		cnt = 0, s;

	/*
	 * Only process incoming calls up to our quota
	 */
	while (cnt++ < ATM_CALLQ_MAX) {

		s = splnet();

		/*
		 * Get next awaiting call
		 */
		cvp = Q_HEAD(atm_incoming_queue, Atm_connvc);
		if (cvp == NULL) {
			(void) splx(s);
			break;
		}
		DEQUEUE(cvp, Atm_connvc, cvc_q, atm_incoming_queue);
		atm_incoming_qlen--;
		cvp->cvc_flags &= ~CVCF_INCOMQ;

		/*
		 * Handle the call
		 */
		atm_cm_incall(cvp);

		(void) splx(s);
	}

	/*
	 * If we've expended our quota, reschedule ourselves
	 */
	if (cnt >= ATM_CALLQ_MAX)
		timeout(atm_cm_procinq, (void *)0, 0);
}


/*
 * Process Incoming Call
 * 
 * This function will search through the listening queue and try to find
 * matching endpoint(s) for the incoming call.  If we find any, we will
 * notify the endpoint service(s) of the incoming call and will then
 * notify the signalling manager to progress the call to an active status.
 * 
 * If there are no listeners for the call, the signalling manager will be
 * notified of a call rejection.
 *
 * Called at splnet.
 *
 * Arguments:
 *	cvp	pointer to connection VCC for incoming call
 *
 * Returns:
 *	none
 *
 */
static void
atm_cm_incall(cvp)
	Atm_connvc	*cvp;
{
	Atm_connection	*cop, *lcop, *hcop;
	Atm_attributes	attr;
	int		err;

	hcop = NULL;
	lcop = NULL;
	cop = NULL;
	attr = cvp->cvc_attr;

	/*
	 * Look for matching listeners
	 */
	while ((lcop = atm_cm_match(&attr, lcop)) != NULL) {

		if (cop == NULL) {
			/*
			 * Need a new connection block
			 */
			cop = (Atm_connection *)
				atm_allocate(&atm_connection_pool);
			if (cop == NULL) {
				cvp->cvc_attr.cause = atm_cause_tmpl;
				cvp->cvc_attr.cause.v.cause_value =
						T_ATM_CAUSE_TEMPORARY_FAILURE;
				goto fail;
			}
		}

		/*
		 * Initialize connection from listener and incoming call
		 */
		cop->co_mxh = NULL;
		cop->co_state = COS_INCONN;
		cop->co_mpx = lcop->co_mpx;
		cop->co_endpt = lcop->co_endpt;
		cop->co_llc = lcop->co_llc;

		switch (attr.bearer.v.connection_configuration) {

		case T_ATM_1_TO_1:
			cop->co_flags |= COF_P2P;
			break;

		case T_ATM_1_TO_MANY:
			/* Not supported */
			cop->co_flags |= COF_P2MP;
			cvp->cvc_attr.cause = atm_cause_tmpl;
			cvp->cvc_attr.cause.v.cause_value =
				T_ATM_CAUSE_BEARER_CAPABILITY_NOT_IMPLEMENTED;
			goto fail;
		}

		/*
		 * Notify endpoint of incoming call
		 */
		err = (*cop->co_endpt->ep_incoming)
			(lcop->co_toku, cop, &cvp->cvc_attr, &cop->co_toku);

		if (err == 0) {

			/*
			 * Endpoint has accepted the call
			 *
			 * Setup call attributes
			 */
			if (hcop == NULL) {
				cvp->cvc_attr.api = lcop->co_lattr->api;
				cvp->cvc_attr.api_init =
					lcop->co_lattr->api_init;
				cvp->cvc_attr.llc = lcop->co_lattr->llc;
			}
			cvp->cvc_attr.headin = MAX(cvp->cvc_attr.headin,
				lcop->co_lattr->headin);

			/*
			 * Setup connection info and queueing
			 */
			cop->co_state = COS_INACCEPT;
			cop->co_connvc = cvp;
			LINK2TAIL(cop, Atm_connection, hcop, co_next);
			cop->co_mxh = hcop;

			/*
			 * Need a new connection block next time around
			 */
			cop = NULL;

		} else {
			/*
			 * Endpoint refuses call
			 */
			goto fail;
		}
	}

	/*
	 * We're done looking for listeners
	 */
	if (hcop) {
		/*
		 * Someone actually wants the call, so notify
		 * the signalling manager to continue
		 */
		cvp->cvc_flags |= CVCF_CONNQ;
		ENQUEUE(cvp, Atm_connvc, cvc_q, atm_connection_queue);
		if (atm_cm_accept(cvp, hcop))
			goto fail;

	} else {
		/*
		 * Nobody around to take the call
		 */
		cvp->cvc_attr.cause = atm_cause_tmpl;
		cvp->cvc_attr.cause.v.cause_value =
				T_ATM_CAUSE_INCOMPATIBLE_DESTINATION;
		goto fail;
	}

	/*
	 * Clean up loose ends
	 */
	if (cop)
		atm_free((caddr_t)cop);

	/*
	 * Call has been accepted
	 */
	return;

fail:
	/*
	 * Call failed - notify any endpoints of the call failure
	 */

	/*
	 * Clean up loose ends
	 */
	if (cop)
		atm_free((caddr_t)cop);

	if (cvp->cvc_attr.cause.tag != T_ATM_PRESENT) {
		cvp->cvc_attr.cause = atm_cause_tmpl;
		cvp->cvc_attr.cause.v.cause_value =
				T_ATM_CAUSE_UNSPECIFIED_NORMAL;
	}
	cop = hcop;
	while (cop) {
		Atm_connection	*cop2 = cop->co_next;
		atm_cm_closeconn(cop, &cvp->cvc_attr.cause.v);
		cop = cop2;
	}

	/*
	 * Tell the signalling manager to reject the call
	 */
	atm_cm_closevc(cvp);

	return;
}


/*
 * Accept an Incoming ATM Call
 * 
 * Some endpoint service(s) wants to accept an incoming call, so the
 * signalling manager will be notified to attempt to progress the call
 * to an active status.
 *
 * If the signalling manager indicates that connection activation has 
 * been immediately successful, then all of the endpoints will be notified
 * that the connection is ready for data transmission.
 * 
 * If the return indicates that connection activation is still in progress,
 * then the endpoints must wait for notification from the Connection Manager
 * indicating the final status of the call setup.  If the call setup completes
 * successfully, then a "call connected" notification will be sent to the
 * endpoints by the Connection Manager.  If the call setup fails, then the
 * endpoints will receive a "call cleared" notification.
 *
 * Called at splnet.
 *
 * Arguments:
 *	cvp	pointer to connection VCC for incoming call
 *	cop	pointer to head of accepted connections
 *
 * Returns:
 *	0	connection has been successfully activated
 *	errno	accept failed - reason indicated
 *
 */
static int
atm_cm_accept(cvp, cop)
	Atm_connvc	*cvp;
	Atm_connection	*cop;
{
	struct stack_list	sl;
	void		(*upf)__P((int, void *, int, int));
	int		sli, err, err2;


	/*
	 * Link vcc to connections
	 */
	cvp->cvc_conn = cop;

	/*
	 * Initialize stack list index
	 */
	sli = 0;

	/*
	 * Check out Data API
	 */
	switch (cvp->cvc_attr.api) {

	case CMAPI_CPCS:
		upf = atm_cm_cpcs_upper;
		break;

	case CMAPI_SAAL:
		sl.sl_sap[sli++] = SAP_SSCF_UNI;
		sl.sl_sap[sli++] = SAP_SSCOP;
		upf = atm_cm_saal_upper;
		break;

	case CMAPI_SSCOP:
		sl.sl_sap[sli++] = SAP_SSCOP;
		upf = atm_cm_sscop_upper;
		break;

	default:
		upf = NULL;
	}

	/*
	 * AAL Attributes
	 */
	switch (cvp->cvc_attr.aal.type) {

	case ATM_AAL5:
		sl.sl_sap[sli++] = SAP_CPCS_AAL5;
		sl.sl_sap[sli++] = SAP_SAR_AAL5;
		sl.sl_sap[sli++] = SAP_ATM;
		break;

	case ATM_AAL3_4:
		sl.sl_sap[sli++] = SAP_CPCS_AAL3_4;
		sl.sl_sap[sli++] = SAP_SAR_AAL3_4;
		sl.sl_sap[sli++] = SAP_ATM;
		break;
	}

	/*
	 * Terminate stack list 
	 */
	sl.sl_sap[sli] = 0;

	/*
	 * Create a service stack
	 */
	err = atm_create_stack(cvp, &sl, upf);
	if (err) {
		goto done;
	}

	/*
	 * Let the signalling manager finish the VCC activation
	 */
	switch ((*cvp->cvc_sigmgr->sm_accept)(cvp->cvc_vcc, &err)) {

	case CALL_PROCEEDING:
		/*
		 * Note that we're not finished yet
		 */
		err = EINPROGRESS;
		/* FALLTHRU */

	case CALL_CONNECTED:
		/*
		 * Initialize the stack now, even if the call isn't totally
		 * active yet.  We want to avoid the delay between getting
		 * the "call connected" event and actually notifying the 
		 * adapter to accept cells on the new VCC - if the delay is 
		 * too long, then we end up dropping the first pdus sent by 
		 * the caller.
		 */
		cvp->cvc_state = CVCS_INIT;
		STACK_CALL(atm_stackcmds[cvp->cvc_attr.api].init,
				cvp->cvc_lower, cvp->cvc_tokl, cvp,
				cvp->cvc_attr.api_init, 0, err2);
		if (err2)
			panic("atm_cm_accept: init");

		if (cvp->cvc_flags & CVCF_ABORTING) {
			/*
			 * Someone on the stack bailed out...schedule the 
			 * VCC and stack termination
			 */
			err = ECONNABORTED;
		} else {
			/*
			 * Everything looks fine from here
			 */
			if (err)
				cvp->cvc_state = CVCS_ACCEPT;
			else
				cvp->cvc_state = CVCS_ACTIVE;
		}
		break;

	case CALL_FAILED:
		/*
		 * Terminate stack and clean up before we leave
		 */
		cvp->cvc_state = CVCS_CLEAR;
		break;

	default:
		panic("atm_cm_accept: accept");
	}

done:
	if (err == 0) {
		/*
		 * Call has been connected, notify endpoints
		 */
		while (cop) {
			Atm_connection	*cop2 = cop->co_next;

			cop->co_state = COS_ACTIVE;
			(*cop->co_endpt->ep_connected)(cop->co_toku);
			cop = cop2;
		}

	} else if (err == EINPROGRESS) {
		/*
		 * Call is still in progress, endpoint must wait
		 */
		err = 0;

	} else {
		/*
		 * Let caller know we failed
		 */
		cvp->cvc_attr.cause = atm_cause_tmpl;
		cvp->cvc_attr.cause.v.cause_value =
				T_ATM_CAUSE_UNSPECIFIED_RESOURCE_UNAVAILABLE;
	}

	return (err);
}


/*
 * Match Attributes on Listening Queue
 * 
 * This function will attempt to match the supplied connection attributes
 * with one of the registered attributes in the listening queue.  The pcop
 * argument may be supplied in order to allow multiple listeners to share 
 * an incoming call (if supported by the listeners).
 *
 * Called at splnet.
 *
 * Arguments:
 *	ap	pointer to attributes to be matched
 *	pcop	pointer to the previously matched connection
 *
 * Returns:
 *	addr	connection with which a match was found
 *	0	no match found
 *
 */
Atm_connection *
atm_cm_match(ap, pcop)
	Atm_attributes	*ap;
	Atm_connection	*pcop;
{
	Atm_connection	*cop;
	Atm_attributes	*lap;


	/*
	 * If we've already matched a listener...
	 */
	if (pcop) {
		/*
		 * Make sure already matched listener supports sharing
		 */
		if ((pcop->co_mpx != ATM_ENC_LLC) ||
		    ((pcop->co_llc.v.flags & T_ATM_LLC_SHARING) == 0))
			return (NULL);

		/*
		 * Position ourselves after the matched entry
		 */
		for (cop = atm_listen_queue; cop; cop = cop->co_next) {
			if (cop == pcop) {
				cop = pcop->co_next;
				break;
			}
		}
	} else {
		/*
		 * Start search at top of listening queue
		 */
		cop = atm_listen_queue;
	}

	/*
	 * Search through listening queue
	 */
	for (; cop; cop = cop->co_next) {

		lap = cop->co_lattr;

		/*
		 * If we're trying to share, check that this entry allows it
		 */
		if (pcop) {
			if ((cop->co_mpx != ATM_ENC_LLC) ||
			    ((cop->co_llc.v.flags & T_ATM_LLC_SHARING) == 0))
				continue;
		}

		/*
		 * ALL "matchable" attributes must match
		 */

		/*
		 * BHLI
		 */
		if (lap->bhli.tag == T_ATM_ABSENT) {
			if (ap->bhli.tag == T_ATM_PRESENT)
				continue;
		} else if (lap->bhli.tag == T_ATM_PRESENT) {
			if (ap->bhli.tag == T_ATM_ABSENT)
				continue;
			if (ap->bhli.tag == T_ATM_PRESENT)
				if (KM_CMP(&lap->bhli.v, &ap->bhli.v, 
						sizeof(struct t_atm_bhli)))
					continue;
		}

		/*
		 * BLLI Layer 2
		 */
		if (lap->blli.tag_l2 == T_ATM_ABSENT) {
			if (ap->blli.tag_l2 == T_ATM_PRESENT)
				continue;
		} else if (lap->blli.tag_l2 == T_ATM_PRESENT) {
			if (ap->blli.tag_l2 == T_ATM_ABSENT)
				continue;
			if (ap->blli.tag_l2 == T_ATM_PRESENT) {
				if (KM_CMP(&lap->blli.v.layer_2_protocol.ID,
					   &ap->blli.v.layer_2_protocol.ID, 
					   sizeof(
					      ap->blli.v.layer_2_protocol.ID)))
					continue;
			}
		}

		/*
		 * BLLI Layer 3
		 */
		if (lap->blli.tag_l3 == T_ATM_ABSENT) {
			if (ap->blli.tag_l3 == T_ATM_PRESENT)
				continue;
		} else if (lap->blli.tag_l3 == T_ATM_PRESENT) {
			if (ap->blli.tag_l3 == T_ATM_ABSENT)
				continue;
			if (ap->blli.tag_l3 == T_ATM_PRESENT) {
				if (KM_CMP(&lap->blli.v.layer_3_protocol.ID,
					   &ap->blli.v.layer_3_protocol.ID, 
					   sizeof(
					      ap->blli.v.layer_3_protocol.ID)))
					continue;
			}
		}

		/*
		 * LLC
		 */
		if (lap->llc.tag == T_ATM_ABSENT) {
			if (ap->llc.tag == T_ATM_PRESENT)
				continue;
		} else if (lap->llc.tag == T_ATM_PRESENT) {
			if (ap->llc.tag == T_ATM_ABSENT)
				continue;
			if (ap->llc.tag == T_ATM_PRESENT) {
				int	i = MIN(lap->llc.v.llc_len,
							ap->llc.v.llc_len);

				if (KM_CMP(lap->llc.v.llc_info,
							ap->llc.v.llc_info, i))
					continue;
			}
		}

		/*
		 * AAL
		 */
		if (lap->aal.tag == T_ATM_ABSENT) {
			if (ap->aal.tag == T_ATM_PRESENT)
				continue;
		} else if (lap->aal.tag == T_ATM_PRESENT) {
			if (ap->aal.tag == T_ATM_ABSENT)
				continue;
			if (ap->aal.tag == T_ATM_PRESENT) {
				if (lap->aal.type != ap->aal.type)
					continue;
				if (lap->aal.type == ATM_AAL5) {
					if (lap->aal.v.aal5.SSCS_type !=
						    ap->aal.v.aal5.SSCS_type)
						continue;
				} else {
					if (lap->aal.v.aal4.SSCS_type !=
						    ap->aal.v.aal4.SSCS_type)
						continue;
				}
			}
		}

		/*
		 * Called Party
		 */
		if (lap->called.tag == T_ATM_ABSENT) {
			if (ap->called.tag == T_ATM_PRESENT)
				continue;
		} else if (lap->called.tag == T_ATM_PRESENT) {
			if (ap->called.tag == T_ATM_ABSENT)
				continue;
			if (ap->called.tag == T_ATM_PRESENT) {
				if ((!ATM_ADDR_EQUAL(&lap->called.addr,
						&ap->called.addr)) ||
				    (!ATM_ADDR_EQUAL(&lap->called.subaddr,
						&ap->called.subaddr)))
					continue;
			}
		}

		/*
		 * Found a full match - return it
		 */
		break;
	}

	return (cop);
}


/*
 * Find Shareable LLC VCC
 * 
 * Given a endpoint-supplied connection attribute using LLC multiplexing,
 * this function will attempt to locate an existing connection which meets
 * the requirements of the supplied attributes.
 *
 * Called at splnet.
 *
 * Arguments:
 *	ap	pointer to requested attributes
 *
 * Returns:
 *	addr	shareable LLC connection VCC
 *	0	no shareable VCC available
 *
 */
static Atm_connvc *
atm_cm_share_llc(ap)
	Atm_attributes	*ap;
{
	Atm_connection	*cop;
	Atm_connvc	*cvp;

	/*
	 * Is requestor willing to share?
	 */
	if ((ap->llc.v.flags & T_ATM_LLC_SHARING) == 0)
		return (NULL);

	/*
	 * Try to find a shareable connection
	 */
	for (cvp = Q_HEAD(atm_connection_queue, Atm_connvc); cvp;
			cvp = Q_NEXT(cvp, Atm_connvc, cvc_q)) {

		/*
		 * Dont use terminating connections
		 */
		switch (cvp->cvc_state) {

		case CVCS_SETUP:
		case CVCS_ACCEPT:
		case CVCS_ACTIVE:
			break;

		default:
			continue;
		}

		/*
		 * Is connection LLC and shareable?
		 */
		if ((cvp->cvc_attr.llc.tag != T_ATM_PRESENT) ||
		    ((cvp->cvc_attr.llc.v.flags & T_ATM_LLC_SHARING) == 0))
			continue;

		/*
		 * Match requested attributes with existing connection
		 */
		if (ap->nif != cvp->cvc_attr.nif)
			continue;

		if ((ap->api != cvp->cvc_attr.api) ||
		    (ap->api_init != cvp->cvc_attr.api_init))
			continue;

		/*
		 * Remote Party
		 */
		if (cvp->cvc_flags & CVCF_CALLER) {
			if ((!ATM_ADDR_EQUAL(&ap->called.addr,
					&cvp->cvc_attr.called.addr)) ||
			    (!ATM_ADDR_EQUAL(&ap->called.subaddr,
					&cvp->cvc_attr.called.subaddr)))
				continue;
		} else {
			if (cvp->cvc_attr.calling.tag != T_ATM_PRESENT)
				continue;
			if ((!ATM_ADDR_EQUAL(&ap->called.addr,
					&cvp->cvc_attr.calling.addr)) ||
			    (!ATM_ADDR_EQUAL(&ap->called.subaddr,
					&cvp->cvc_attr.calling.subaddr)))
				continue;
		}

		/*
		 * AAL
		 */
		if (ap->aal.type == ATM_AAL5) {
			struct t_atm_aal5	*ap5, *cv5;

			ap5 = &ap->aal.v.aal5;
			cv5 = &cvp->cvc_attr.aal.v.aal5;

			if ((cvp->cvc_attr.aal.type != ATM_AAL5) ||
			    (ap5->SSCS_type != cv5->SSCS_type))
				continue;

			if (cvp->cvc_flags & CVCF_CALLER) {
				if (ap5->forward_max_SDU_size >
						cv5->forward_max_SDU_size)
					continue;
			} else {
				if (ap5->forward_max_SDU_size >
						cv5->backward_max_SDU_size)
					continue;
			}
		} else {
			struct t_atm_aal4	*ap4, *cv4;

			ap4 = &ap->aal.v.aal4;
			cv4 = &cvp->cvc_attr.aal.v.aal4;

			if ((cvp->cvc_attr.aal.type != ATM_AAL3_4) ||
			    (ap4->SSCS_type != cv4->SSCS_type))
				continue;

			if (cvp->cvc_flags & CVCF_CALLER) {
				if (ap4->forward_max_SDU_size >
						cv4->forward_max_SDU_size)
					continue;
			} else {
				if (ap4->forward_max_SDU_size >
						cv4->backward_max_SDU_size)
					continue;
			}
		}

		/*
		 * Traffic Descriptor
		 */
		if ((ap->traffic.tag != T_ATM_PRESENT) ||
		    (cvp->cvc_attr.traffic.tag != T_ATM_PRESENT) ||
		    (ap->traffic.v.best_effort != T_YES) ||
		    (cvp->cvc_attr.traffic.v.best_effort != T_YES))
			continue;

		/*
		 * Broadband Bearer
		 */
		if (ap->bearer.v.connection_configuration !=
				cvp->cvc_attr.bearer.v.connection_configuration)
			continue;

		/*
		 * QOS
		 */
		if (cvp->cvc_flags & CVCF_CALLER) {
			if ((ap->qos.v.forward.qos_class !=
				cvp->cvc_attr.qos.v.forward.qos_class) ||
			    (ap->qos.v.backward.qos_class !=
				cvp->cvc_attr.qos.v.backward.qos_class))
				continue;
		} else {
			if ((ap->qos.v.forward.qos_class !=
				cvp->cvc_attr.qos.v.backward.qos_class) ||
			    (ap->qos.v.backward.qos_class !=
				cvp->cvc_attr.qos.v.forward.qos_class))
				continue;
		}

		/*
		 * The new LLC header must also be unique for this VCC
		 */
		for (cop = cvp->cvc_conn; cop; cop = cop->co_next) {
			int	i = MIN(ap->llc.v.llc_len,
					cop->co_llc.v.llc_len);

			if (KM_CMP(ap->llc.v.llc_info, 
				   cop->co_llc.v.llc_info, i) == 0)
				break;
		}

		/*
		 * If no header overlaps, then we're done
		 */
		if (cop == NULL)
			break;
	}

	return (cvp);
}


/*
 * Close Connection
 * 
 * This function will terminate a connection, including notifying the
 * user, if necessary, and freeing up control block memory.  The caller
 * is responsible for managing the connection VCC.
 *
 * Called at splnet.
 *
 * Arguments:
 *	cop	pointer to connection block
 *	cause	pointer to cause of close
 *
 * Returns:
 *	none
 *
 */
static void
atm_cm_closeconn(cop, cause)
	Atm_connection	*cop;
	struct t_atm_cause	*cause;
{

	/*
	 * Decide whether user needs notification
	 */
	switch (cop->co_state) {

	case COS_OUTCONN:
	case COS_LISTEN:
	case COS_INCONN:
	case COS_INACCEPT:
	case COS_ACTIVE:
		/*
		 * Yup, let 'em know connection is gone
		 */
		if (cop->co_toku)
			(*cop->co_endpt->ep_cleared)(cop->co_toku, cause);
		break;

	case COS_CLEAR:
		/*
		 * Nope,they should know already
		 */
		break;

	default:
		panic("atm_cm_closeconn: bogus state");
	}

	/*
	 * Unlink connection from its queues
	 */
	switch (cop->co_state) {

	case COS_LISTEN:
		atm_free((caddr_t)cop->co_lattr);
		UNLINK(cop, Atm_connection, atm_listen_queue, co_next);
		break;

	default:
		/*
		 * Remove connection from multiplexor queue
		 */
		if (cop->co_mxh != cop) {
			/*
			 * Connection is down the chain, just unlink it
			 */
			UNLINK(cop, Atm_connection, cop->co_mxh, co_next);

		} else if (cop->co_next != NULL) {
			/*
			 * Connection is at the head of a non-singleton chain,
			 * so unlink and reset the chain head
			 */
			Atm_connection	*t, *nhd;

			t = nhd = cop->co_next;
			while (t) {
				t->co_mxh = nhd;
				t = t->co_next;
			}
			if (nhd->co_connvc)
				nhd->co_connvc->cvc_conn = nhd;
		}
	}

	/*
	 * Free the connection block
	 */
	cop->co_state = COS_FREE;
	atm_free((caddr_t)cop);

	return;
}


/*
 * Close Connection VCC
 * 
 * This function will terminate a connection VCC, including releasing the
 * the call to the signalling manager, terminating the VCC protocol stack,
 * and freeing up control block memory.
 *
 * Called at splnet.
 *
 * Arguments:
 *	cvp	pointer to connection VCC block
 *
 * Returns:
 *	none
 *
 */
static void
atm_cm_closevc(cvp)
	Atm_connvc	*cvp;
{
	int	err;

	/*
	 * Break links with the connection block
	 */
	cvp->cvc_conn = NULL;

	/*
	 * Cancel any running timer
	 */
	CVC_CANCEL(cvp);

	/*
	 * Free queued packets
	 */
	while (cvp->cvc_rcvq) {
		KBuffer		*m;

		m = cvp->cvc_rcvq;
		cvp->cvc_rcvq = KB_QNEXT(m);
		KB_QNEXT(m) = NULL;
		KB_FREEALL(m);
	}

	/*
	 * Unlink from any queues
	 */
	if (cvp->cvc_flags & CVCF_INCOMQ) {
		DEQUEUE(cvp, Atm_connvc, cvc_q, atm_incoming_queue);
		atm_incoming_qlen--;
		cvp->cvc_flags &=  ~CVCF_INCOMQ;

	} else if (cvp->cvc_flags & CVCF_CONNQ) {
		DEQUEUE(cvp, Atm_connvc, cvc_q, atm_connection_queue);
		cvp->cvc_flags &=  ~CVCF_CONNQ;
	}

	/*
	 * Release the signalling call
	 */
	switch (cvp->cvc_state) {

	case CVCS_SETUP:
	case CVCS_INIT:
	case CVCS_ACCEPT:
	case CVCS_ACTIVE:
	case CVCS_RELEASE:
		if (cvp->cvc_vcc) {
			cvp->cvc_state = CVCS_RELEASE;
			switch ((*cvp->cvc_sigmgr->sm_release)
				(cvp->cvc_vcc, &err)) {

			case CALL_CLEARED:
				/*
				 * Looks good so far...
				 */
				break;

			case CALL_PROCEEDING:
				/*
				 * We'll have to wait for the call to clear
				 */
				return;

			case CALL_FAILED:
				/*
				 * If there's a memory shortage, retry later.
				 * Otherwise who knows what's going on....
				 */
				if ((err == ENOMEM) || (err == ENOBUFS)) {
					CVC_TIMER(cvp, 1 * ATM_HZ);
					return;
				}
				log(LOG_ERR,
					"atm_cm_closevc: release %d\n", err);
				break;
			}
		}
		break;

	case CVCS_INCOMING:
	case CVCS_REJECT:
		if (cvp->cvc_vcc) {
			cvp->cvc_state = CVCS_REJECT;
			switch ((*cvp->cvc_sigmgr->sm_reject)
				(cvp->cvc_vcc, &err)) {

			case CALL_CLEARED:
				/*
				 * Looks good so far...
				 */
				break;

			case CALL_FAILED:
				/*
				 * If there's a memory shortage, retry later.
				 * Otherwise who knows what's going on....
				 */
				if ((err == ENOMEM) || (err == ENOBUFS)) {
					CVC_TIMER(cvp, 1 * ATM_HZ);
					return;
				}
				log(LOG_ERR,
					"atm_cm_closevc: reject %d\n", err);
				break;
			}
		}
		break;

	case CVCS_CLEAR:
	case CVCS_TERM:
		/*
		 * No need for anything here
		 */
		break;

	default:
		panic("atm_cm_closevc: bogus state");
	}

	/*
	 * Now terminate the stack
	 */
	if (cvp->cvc_tokl) {
		cvp->cvc_state = CVCS_TERM;

		/*
		 * Wait until stack is unwound before terminating
		 */
		if ((cvp->cvc_downcnt > 0) || (cvp->cvc_upcnt > 0)) {
			CVC_TIMER(cvp, 0);
			return;
		}

		STACK_CALL(atm_stackcmds[cvp->cvc_attr.api].term,
			cvp->cvc_lower, cvp->cvc_tokl, cvp, 0, 0, err);

		cvp->cvc_tokl = NULL;
	}

	/*
	 * Let signalling manager finish up
	 */
	cvp->cvc_state = CVCS_FREE;
	if (cvp->cvc_vcc) {
		(void) (*cvp->cvc_sigmgr->sm_free)(cvp->cvc_vcc);
	}

	/*
	 * Finally, free our own control blocks
	 */
	atm_free((caddr_t)cvp);

	return;
}


/*
 * Process a Connection VCC timeout
 * 
 * Called when a previously scheduled cvc control block timer expires.  
 * Processing will be based on the current cvc state.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to cvc timer control block
 *
 * Returns:
 *	none
 *
 */
static void
atm_cm_timeout(tip)
	struct atm_time	*tip;
{
	Atm_connection	*cop, *cop2;
	Atm_connvc	*cvp;

	/*
	 * Back-off to cvc control block
	 */
	cvp = (Atm_connvc *)
			((caddr_t)tip - (int)(&((Atm_connvc *)0)->cvc_time));

	/*
	 * Process timeout based on protocol state
	 */
	switch (cvp->cvc_state) {

	case CVCS_SETUP:
	case CVCS_ACCEPT:
	case CVCS_ACTIVE:
		/*
		 * Handle VCC abort
		 */
		if ((cvp->cvc_flags & CVCF_ABORTING) == 0)
			goto logerr;

		/*
		 * Terminate all connections
		 */
		cop = cvp->cvc_conn;
		while (cop) {
			cop2 = cop->co_next;
			atm_cm_closeconn(cop, &cvp->cvc_attr.cause.v);
			cop = cop2;
		}

		/*
		 * Terminate VCC
		 */
		atm_cm_closevc(cvp);

		break;

	case CVCS_REJECT:
	case CVCS_RELEASE:
	case CVCS_TERM:
		/*
		 * Retry failed operation
		 */
		atm_cm_closevc(cvp);
		break;

	default:
logerr:
		log(LOG_ERR,
			"atm_cm_timeout: invalid state: cvp=%p, state=%d\n",
			cvp, cvp->cvc_state);
	}
}


/*
 * CPCS User Control Commands
 * 
 * This function is called by an endpoint user to pass a control command
 * across a CPCS data API.  Mostly we just send these down the stack.
 *
 * Arguments:
 *	cmd	stack command code
 *	cop	pointer to connection block
 *	arg	argument
 *
 * Returns:
 *	0	command output successful
 *	errno	output failed - reason indicated
 *
 */
int
atm_cm_cpcs_ctl(cmd, cop, arg)
	int		cmd;
	Atm_connection	*cop;
	void		*arg;
{
	Atm_connvc	*cvp;
	int		err = 0;

	/*
	 * Validate connection state
	 */
	if (cop->co_state != COS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	cvp = cop->co_connvc;
	if (cvp->cvc_state != CVCS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	if (cvp->cvc_attr.api != CMAPI_CPCS) {
		err = EFAULT;
		goto done;
	}

	switch (cmd) {

	default:
		err = EINVAL;
	}

done:
	return (err);
}


/*
 * CPCS Data Output
 * 
 * This function is called by an endpoint user to output a data packet
 * across a CPCS data API.   After we've validated the connection state, the
 * packet will be encapsulated (if necessary) and sent down the data stack.
 *
 * Arguments:
 *	cop	pointer to connection block
 *	m	pointer to packet buffer chain to be output
 *
 * Returns:
 *	0	packet output successful
 *	errno	output failed - reason indicated
 *
 */
int
atm_cm_cpcs_data(cop, m)
	Atm_connection	*cop;
	KBuffer		*m;
{
	Atm_connvc	*cvp;
	struct attr_llc	*llcp;
	int		err, space;
	void		*bp;


	/*
	 * Validate connection state
	 */
	if (cop->co_state != COS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	cvp = cop->co_connvc;
	if (cvp->cvc_state != CVCS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	if (cvp->cvc_attr.api != CMAPI_CPCS) {
		err = EFAULT;
		goto done;
	}

	/*
	 * Add any packet encapsulation
	 */
	switch (cop->co_mpx) {

	case ATM_ENC_NULL:
		/*
		 * None needed...
		 */
		break;

	case ATM_ENC_LLC:
		/*
		 * Need to add an LLC header
		 */
		llcp = &cop->co_llc;

		/*
		 * See if there's room to add LLC header to front of packet.
		 */
		KB_HEADROOM(m, space);
		if (space < llcp->v.llc_len) {
			KBuffer		*n;

			/*
			 * We have to allocate another buffer and tack it
			 * onto the front of the packet
			 */
			KB_ALLOCPKT(n, llcp->v.llc_len, KB_F_NOWAIT,
					KB_T_HEADER);
			if (n == 0) {
				err = ENOMEM;
				goto done;
			}
			KB_TAILALIGN(n, llcp->v.llc_len);
			KB_LINKHEAD(n, m);
			m = n;
		} else {
			/*
			 * Header fits, just adjust buffer controls
			 */
			KB_HEADADJ(m, llcp->v.llc_len);
		}

		/*
		 * Add the LLC header
		 */
		KB_DATASTART(m, bp, void *);
		KM_COPY(llcp->v.llc_info, bp, llcp->v.llc_len);
		KB_PLENADJ(m, llcp->v.llc_len);
		break;

	default:
		panic("atm_cm_cpcs_data: mpx");
	}

	/*
	 * Finally, we can send the packet on its way
	 */
	STACK_CALL(CPCS_UNITDATA_INV, cvp->cvc_lower, cvp->cvc_tokl, 
		cvp, (int)m, 0, err);

done:
	return (err);
}


/*
 * Process CPCS Stack Commands
 * 
 * This is the top of the CPCS API data stack.  All upward stack commands 
 * for the CPCS data API will be received and processed here.
 *
 * Arguments:
 *	cmd	stack command code
 *	tok	session token (pointer to connection VCC control block)
 *	arg1	argument 1
 *	arg2	argument 2
 *
 * Returns:
 *	none
 *
 */
static void
atm_cm_cpcs_upper(cmd, tok, arg1, arg2)
	int		cmd;
	void		*tok;
	int		arg1;
	int		arg2;
{
	Atm_connection	*cop;
	Atm_connvc	*cvp = tok;
	KBuffer		*m;
	void		*bp;
	int		s;

	switch (cmd) {

	case CPCS_UNITDATA_SIG:
		/*
		 * Input data packet
		 */
		m = (KBuffer *)arg1;

		if (cvp->cvc_state != CVCS_ACTIVE) {
			if (cvp->cvc_state == CVCS_ACCEPT) {
				KBuffer	*n;

				/*
				 * Queue up any packets received before sigmgr
				 * notifies us of incoming call completion
				 */
				if (cvp->cvc_rcvqlen >= CVC_RCVQ_MAX) {
					KB_FREEALL(m);
					atm_cm_stat.cms_rcvconnvc++;
					return;
				}
				KB_QNEXT(m) = NULL;
				if (cvp->cvc_rcvq == NULL) {
					cvp->cvc_rcvq = m;
				} else {
					for (n = cvp->cvc_rcvq; 
					     KB_QNEXT(n) != NULL; 
					     n = KB_QNEXT(n))
						;
					KB_QNEXT(n) = m;
				}
				cvp->cvc_rcvqlen++;
				return;
			} else {
				KB_FREEALL(m);
				atm_cm_stat.cms_rcvconnvc++;
				return;
			}
		}

		/*
		 * Locate packet's connection
		 */
		cop = cvp->cvc_conn;
		switch (cop->co_mpx) {

		case ATM_ENC_NULL:
			/*
			 * We're already there...
			 */
			break;

		case ATM_ENC_LLC:
			/*
			 * Find connection with matching LLC header
			 */
			if (KB_LEN(m) < T_ATM_LLC_MAX_LEN) {
				KB_PULLUP(m, T_ATM_LLC_MAX_LEN, m);
				if (m == 0) {
					atm_cm_stat.cms_llcdrop++;
					return;
				}
			}
			KB_DATASTART(m, bp, void *);

			s = splnet();

			while (cop) {
				if (KM_CMP(bp, cop->co_llc.v.llc_info,
						cop->co_llc.v.llc_len) == 0)
					break;
				cop = cop->co_next;
			}

			(void) splx(s);

			if (cop == NULL) {
				/*
				 * No connected user for this LLC
				 */
				KB_FREEALL(m);
				atm_cm_stat.cms_llcid++;
				return;
			}

			/*
			 * Strip off the LLC header
			 */
			KB_HEADADJ(m, -cop->co_llc.v.llc_len);
			KB_PLENADJ(m, -cop->co_llc.v.llc_len);
			break;

		default:
			panic("atm_cm_cpcs_upper: mpx");
		}

		/*
		 * We've found our connection, so hand the packet off
		 */
		if (cop->co_state != COS_ACTIVE) {
			KB_FREEALL(m);
			atm_cm_stat.cms_rcvconn++;
			return;
		}
		(*cop->co_endpt->ep_cpcs_data)(cop->co_toku, m);
		break;

	case CPCS_UABORT_SIG:
	case CPCS_PABORT_SIG:
		/*
		 * We don't support these (yet), so just fall thru...
		 */

	default:
		log(LOG_ERR, "atm_cm_cpcs_upper: unknown cmd 0x%x\n", cmd);
	}
}


/*
 * SAAL User Control Commands
 * 
 * This function is called by an endpoint user to pass a control command
 * across a SAAL data API.  Mostly we just send these down the stack.
 *
 * Arguments:
 *	cmd	stack command code
 *	cop	pointer to connection block
 *	arg	argument
 *
 * Returns:
 *	0	command output successful
 *	errno	output failed - reason indicated
 *
 */
int
atm_cm_saal_ctl(cmd, cop, arg)
	int		cmd;
	Atm_connection	*cop;
	void		*arg;
{
	Atm_connvc	*cvp;
	int		err = 0;

	/*
	 * Validate connection state
	 */
	if (cop->co_state != COS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	cvp = cop->co_connvc;
	if (cvp->cvc_state != CVCS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	if (cvp->cvc_attr.api != CMAPI_SAAL) {
		err = EFAULT;
		goto done;
	}

	switch (cmd) {

	case SSCF_UNI_ESTABLISH_REQ:
	case SSCF_UNI_RELEASE_REQ:
		/*
		 * Pass command down the stack
		 */
		STACK_CALL(cmd, cvp->cvc_lower, cvp->cvc_tokl, cvp, 
			(int)arg, 0, err);
		break;

	default:
		err = EINVAL;
	}

done:
	return (err);
}


/*
 * SAAL Data Output
 * 
 * This function is called by an endpoint user to output a data packet
 * across a SAAL data API.   After we've validated the connection state,
 * the packet will be sent down the data stack.
 *
 * Arguments:
 *	cop	pointer to connection block
 *	m	pointer to packet buffer chain to be output
 *
 * Returns:
 *	0	packet output successful
 *	errno	output failed - reason indicated
 *
 */
int
atm_cm_saal_data(cop, m)
	Atm_connection	*cop;
	KBuffer		*m;
{
	Atm_connvc	*cvp;
	int		err;


	/*
	 * Validate connection state
	 */
	if (cop->co_state != COS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	cvp = cop->co_connvc;
	if (cvp->cvc_state != CVCS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	if (cvp->cvc_attr.api != CMAPI_SAAL) {
		err = EFAULT;
		goto done;
	}

	/*
	 * Finally, we can send the packet on its way
	 */
	STACK_CALL(SSCF_UNI_DATA_REQ, cvp->cvc_lower, cvp->cvc_tokl, 
		cvp, (int)m, 0, err);

done:
	return (err);
}


/*
 * Process SAAL Stack Commands
 * 
 * This is the top of the SAAL API data stack.  All upward stack commands 
 * for the SAAL data API will be received and processed here.
 *
 * Arguments:
 *	cmd	stack command code
 *	tok	session token (pointer to connection VCC control block)
 *	arg1	argument 1
 *	arg2	argument 2
 *
 * Returns:
 *	none
 *
 */
static void
atm_cm_saal_upper(cmd, tok, arg1, arg2)
	int		cmd;
	void		*tok;
	int		arg1;
	int		arg2;
{
	Atm_connection	*cop;
	Atm_connvc	*cvp = tok;


	switch (cmd) {

	case SSCF_UNI_ESTABLISH_IND:
	case SSCF_UNI_ESTABLISH_CNF:
	case SSCF_UNI_RELEASE_IND:
	case SSCF_UNI_RELEASE_CNF:
		/*
		 * Control commands
		 */
		cop = cvp->cvc_conn;
		if (cvp->cvc_state != CVCS_ACTIVE)
			break;
		if (cop->co_state != COS_ACTIVE)
			break;

		(*cop->co_endpt->ep_saal_ctl)(cmd, cop->co_toku, (void *)arg1);
		break;

	case SSCF_UNI_DATA_IND:
		/*
		 * User data
		 */
		cop = cvp->cvc_conn;
		if (cvp->cvc_state != CVCS_ACTIVE) {
			atm_cm_stat.cms_rcvconnvc++;
			KB_FREEALL((KBuffer *)arg1);
			break;
		}
		if (cop->co_state != COS_ACTIVE) {
			atm_cm_stat.cms_rcvconn++;
			KB_FREEALL((KBuffer *)arg1);
			break;
		}

		(*cop->co_endpt->ep_saal_data)(cop->co_toku, (KBuffer *)arg1);
		break;

	case SSCF_UNI_UNITDATA_IND:
		/*
		 * Not supported
		 */
		KB_FREEALL((KBuffer *)arg1);

		/* FALLTHRU */

	default:
		log(LOG_ERR, "atm_cm_saal_upper: unknown cmd 0x%x\n", cmd);
	}
}


/*
 * SSCOP User Control Commands
 * 
 * This function is called by an endpoint user to pass a control command
 * across a SSCOP data API.  Mostly we just send these down the stack.
 *
 * Arguments:
 *	cmd	stack command code
 *	cop	pointer to connection block
 *	arg1	argument
 *	arg2	argument
 *
 * Returns:
 *	0	command output successful
 *	errno	output failed - reason indicated
 *
 */
int
atm_cm_sscop_ctl(cmd, cop, arg1, arg2)
	int		cmd;
	Atm_connection	*cop;
	void		*arg1;
	void		*arg2;
{
	Atm_connvc	*cvp;
	int		err = 0;

	/*
	 * Validate connection state
	 */
	if (cop->co_state != COS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	cvp = cop->co_connvc;
	if (cvp->cvc_state != CVCS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	if (cvp->cvc_attr.api != CMAPI_SSCOP) {
		err = EFAULT;
		goto done;
	}

	switch (cmd) {

	case SSCOP_ESTABLISH_REQ:
	case SSCOP_ESTABLISH_RSP:
	case SSCOP_RELEASE_REQ:
	case SSCOP_RESYNC_REQ:
	case SSCOP_RESYNC_RSP:
	case SSCOP_RECOVER_RSP:
	case SSCOP_RETRIEVE_REQ:
		/*
		 * Pass command down the stack
		 */
		STACK_CALL(cmd, cvp->cvc_lower, cvp->cvc_tokl, cvp, 
			(int)arg1, (int)arg2, err);
		break;

	default:
		err = EINVAL;
	}

done:
	return (err);
}


/*
 * SSCOP Data Output
 * 
 * This function is called by an endpoint user to output a data packet
 * across a SSCOP data API.   After we've validated the connection state,
 * the packet will be encapsulated and sent down the data stack.
 *
 * Arguments:
 *	cop	pointer to connection block
 *	m	pointer to packet buffer chain to be output
 *
 * Returns:
 *	0	packet output successful
 *	errno	output failed - reason indicated
 *
 */
int
atm_cm_sscop_data(cop, m)
	Atm_connection	*cop;
	KBuffer		*m;
{
	Atm_connvc	*cvp;
	int		err;


	/*
	 * Validate connection state
	 */
	if (cop->co_state != COS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	cvp = cop->co_connvc;
	if (cvp->cvc_state != CVCS_ACTIVE) {
		err = EFAULT;
		goto done;
	}

	if (cvp->cvc_attr.api != CMAPI_SSCOP) {
		err = EFAULT;
		goto done;
	}

	/*
	 * Finally, we can send the packet on its way
	 */
	STACK_CALL(SSCOP_DATA_REQ, cvp->cvc_lower, cvp->cvc_tokl, 
		cvp, (int)m, 0, err);

done:
	return (err);
}


/*
 * Process SSCOP Stack Commands
 * 
 * This is the top of the SSCOP API data stack.  All upward stack commands 
 * for the SSCOP data API will be received and processed here.
 *
 * Arguments:
 *	cmd	stack command code
 *	tok	session token (pointer to connection VCC control block)
 *	arg1	argument 1
 *	arg2	argument 2
 *
 * Returns:
 *	none
 *
 */
static void
atm_cm_sscop_upper(cmd, tok, arg1, arg2)
	int		cmd;
	void		*tok;
	int		arg1;
	int		arg2;
{
	Atm_connection	*cop;
	Atm_connvc	*cvp = tok;

	switch (cmd) {

	case SSCOP_ESTABLISH_IND:
	case SSCOP_ESTABLISH_CNF:
	case SSCOP_RELEASE_IND:
	case SSCOP_RESYNC_IND:
		/*
		 * Control commands
		 */
		cop = cvp->cvc_conn;
		if ((cvp->cvc_state != CVCS_ACTIVE) ||
		    (cop->co_state != COS_ACTIVE)) {
			KB_FREEALL((KBuffer *)arg1);
			break;
		}

		(*cop->co_endpt->ep_sscop_ctl)
			(cmd, cop->co_toku, (void *)arg1, (void *)arg2);
		break;

	case SSCOP_RELEASE_CNF:
	case SSCOP_RESYNC_CNF:
	case SSCOP_RECOVER_IND:
	case SSCOP_RETRIEVE_IND:
	case SSCOP_RETRIEVECMP_IND:
		/*
		 * Control commands
		 */
		cop = cvp->cvc_conn;
		if ((cvp->cvc_state != CVCS_ACTIVE) ||
		    (cop->co_state != COS_ACTIVE))
			break;

		(*cop->co_endpt->ep_sscop_ctl)
			(cmd, cop->co_toku, (void *)arg1, (void *)arg2);
		break;

	case SSCOP_DATA_IND:
		/*
		 * User data
		 */
		cop = cvp->cvc_conn;
		if (cvp->cvc_state != CVCS_ACTIVE) {
			atm_cm_stat.cms_rcvconnvc++;
			KB_FREEALL((KBuffer *)arg1);
			break;
		}
		if (cop->co_state != COS_ACTIVE) {
			atm_cm_stat.cms_rcvconn++;
			KB_FREEALL((KBuffer *)arg1);
			break;
		}

		(*cop->co_endpt->ep_sscop_data)
				(cop->co_toku, (KBuffer *)arg1, arg2);
		break;

	case SSCOP_UNITDATA_IND:
		/*
		 * Not supported
		 */
		KB_FREEALL((KBuffer *)arg1);

		/* FALLTHRU */

	default:
		log(LOG_ERR, "atm_cm_sscop_upper: unknown cmd 0x%x\n", cmd);
	}
}


/*
 * Register an ATM Endpoint Service
 * 
 * Every ATM endpoint service must register itself here before it can 
 * issue or receive any connection requests.
 *
 * Arguments:
 *	epp	pointer to endpoint definition structure
 *
 * Returns:
 *	0	registration successful
 *	errno	registration failed - reason indicated
 *
 */
int
atm_endpoint_register(epp)
	Atm_endpoint	*epp;
{
	int		s = splnet();

	/*
	 * See if we need to be initialized
	 */
	if (!atm_init)
		atm_initialize();

	/*
	 * Validate endpoint
	 */
	if (epp->ep_id > ENDPT_MAX) {
		(void) splx(s);
		return (EINVAL);
	}
	if (atm_endpoints[epp->ep_id] != NULL) {
		(void) splx(s);
		return (EEXIST);
	}

	/*
	 * Add endpoint to list
	 */
	atm_endpoints[epp->ep_id] = epp;

	(void) splx(s);
	return (0);
}


/*
 * De-register an ATM Endpoint Service
 * 
 * Each ATM endpoint service provider must de-register its registered 
 * endpoint(s) before terminating.  Specifically, loaded kernel modules
 * must de-register their services before unloading themselves.
 *
 * Arguments:
 *	epp	pointer to endpoint definition structure
 *
 * Returns:
 *	0	de-registration successful 
 *	errno	de-registration failed - reason indicated
 *
 */
int
atm_endpoint_deregister(epp)
	Atm_endpoint	*epp;
{
	int	s = splnet();

	/*
	 * Validate endpoint
	 */
	if (epp->ep_id > ENDPT_MAX) {
		(void) splx(s);
		return (EINVAL);
	}
	if (atm_endpoints[epp->ep_id] != epp) {
		(void) splx(s);
		return (ENOENT);
	}

	/*
	 * Remove endpoint from list
	 */
	atm_endpoints[epp->ep_id] = NULL;

	(void) splx(s);
	return (0);
}

