/*-
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
 */

/*
 * SPANS Signalling Manager
 * ---------------------------
 *
 * External interfaces to SPANS manager.  Includes support for
 * running as a loadable kernel module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef ATM_SPANS_MODULE
#include "opt_atm.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_ioctl.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include "spans_xdr.h"
#include <netatm/spans/spans_var.h>

/*
 * Global variables
 */
uma_zone_t	spans_vc_zone;
uma_zone_t	spans_msg_zone;

/*
 * Local functions
 */
static int	spans_start(void);
static int	spans_stop(void);
static int	spans_attach(struct sigmgr *, struct atm_pif *);
static int	spans_detach(struct atm_pif *);
static int	spans_setup(Atm_connvc *, int *);
static int	spans_release(struct vccb *, int *);
static int	spans_accept(struct vccb *, int *);
static int	spans_reject(struct vccb *, int *);
static int	spans_ioctl(int, caddr_t, caddr_t);

/*
 * Local variables
 */
static struct sigmgr	*spans_mgr = NULL;


/*
 * Initialize SPANS processing
 *
 * This will be called during module loading.  We'll just register
 * the SPANS protocol descriptor and wait for a SPANS ATM interface
 * to come online.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0	startup was successful
 *	errno	startup failed - reason indicated
 *
 */
static int
spans_start()
{
	int	err = 0;

	/*
	 * Verify software version
	 */
	if (atm_version != ATM_VERSION) {
		log(LOG_ERR, "version mismatch: spans=%d.%d kernel=%d.%d\n",
				ATM_VERS_MAJ(ATM_VERSION),
				ATM_VERS_MIN(ATM_VERSION),
				ATM_VERS_MAJ(atm_version),
				ATM_VERS_MIN(atm_version));
		return (EINVAL);
	}

	spans_vc_zone = uma_zcreate("spans vc", sizeof(struct spans_vccb),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (spans_vc_zone == NULL)
		panic("spans_vc_zone");

	spans_msg_zone = uma_zcreate("spans msg", sizeof(spans_msg), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (spans_msg_zone == NULL)
		panic("spans_msg_zone");

	/*
	 * Allocate protocol definition structure
	 */
	spans_mgr = malloc(sizeof(struct sigmgr), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (spans_mgr == NULL) {
		err = ENOMEM;
		goto done;
	}

	/*
	 * Initialize protocol invariant values
	 */
	spans_mgr->sm_proto = ATM_SIG_SPANS;
	spans_mgr->sm_attach = spans_attach;
	spans_mgr->sm_detach = spans_detach;
	spans_mgr->sm_setup = spans_setup;
	spans_mgr->sm_release = spans_release;
	spans_mgr->sm_accept = spans_accept;
	spans_mgr->sm_reject = spans_reject;
	spans_mgr->sm_free = spans_free;
	spans_mgr->sm_ioctl = spans_ioctl;

	/*
	 * Register ourselves with system
	 */
	err = atm_sigmgr_register(spans_mgr);
	if (err)
		goto done;

	/*
	 * Start the arp service
	 */
	spansarp_start();

	/*
	 * Start up Connectionless Service
	 */
	err = spanscls_start();
	if (err)
		goto done;

done:
	return (err);
}


/*
 * Halt SPANS processing
 *
 * This should be called just prior to unloading the module from
 * memory.  All SPANS interfaces must be deregistered before the
 * protocol can be shutdown.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0	startup was successful
 *	errno	startup failed - reason indicated
 *
 */
static int
spans_stop()
{
	int	err = 0;
	int	s = splnet();

	/*
	 * Is protocol even set up?
	 */
	if (spans_mgr) {

		/*
		 * Any protocol instances still registered?
		 */
		if (spans_mgr->sm_prinst) {

			/* Yes, can't stop now */
			err = EBUSY;
			goto done;
		}

		/*
		 * Stop Connectionless Service
		 */
		spanscls_stop();

		/*
		 * De-register from system
		 */
		err = atm_sigmgr_deregister(spans_mgr);

		/*
		 * Free up protocol block
		 */
		free(spans_mgr, M_DEVBUF);
		spans_mgr = NULL;

		/*
		 * Free up our storage pools
		 */
		uma_zdestroy(spans_vc_zone);
		uma_zdestroy(spans_msg_zone);
	} else
		err = ENXIO;

done:
	(void) splx(s);
	return (err);
}


/*
 * Attach a SPANS-controlled interface
 *
 * Each ATM physical interface must be attached with the signalling
 * manager for the interface's signalling protocol (via the
 * atm_sigmgr_attach function).  This function will handle the
 * attachment for SPANS-controlled interfaces.  A new SPANS protocol
 * instance will be created and then we'll just sit around waiting for
 * status or connection requests.
 *
 * Function must be called at splnet.
 *
 * Arguments:
 *	smp	pointer to SPANS signalling manager control block
 *	pip	pointer to ATM physical interface control block
 *
 * Returns:
 *	0	attach successful
 *	errno	attach failed - reason indicated
 *
 */
static int
spans_attach(smp, pip)
	struct sigmgr	*smp;
	struct atm_pif	*pip;
{
	int		err = 0, n = 0, s;
	struct spans	*spp = NULL;
	struct atm_nif	*np;

	ATM_DEBUG2("spans_attach: smp=%p, pip=%p\n", smp, pip);

	/*
	 * Count network interfaces attached to the physical interface.
	 * If there are more or less than one, we have big problems.
	 */
	np = pip->pif_nif;
	while (np) {
		n++;
		np = np->nif_pnext;
	}
	if (n != 1) {
		err = ETOOMANYREFS;
		goto done;
	}

	/*
	 * Allocate SPANS protocol instance control block
	 */
	spp = malloc(sizeof(struct spans), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (spp == NULL) {
		err = ENOMEM;
		goto done;
	}

	/*
	 * Set variables in SPANS protocol instance control block
	 */
	spp->sp_state = SPANS_INIT;
	spp->sp_h_epoch = time_second;
	spp->sp_s_epoch = 0;
	spp->sp_addr.address_format = T_ATM_ABSENT;
	spp->sp_addr.address_length = 0;
	spp->sp_subaddr.address_format = T_ATM_ABSENT;
	spp->sp_subaddr.address_length = 0;
	spp->sp_probe_ct = 0;
	spp->sp_alloc_vci = SPANS_MIN_VCI;
	spp->sp_alloc_vpi = SPANS_VPI;
	spp->sp_min_vci = SPANS_MIN_VCI;
	spp->sp_max_vci = pip->pif_maxvci;

	/*
	 * Link instance into manager's chain
	 */
	LINK2TAIL((struct siginst *)spp, struct siginst, smp->sm_prinst,
			si_next);

	/*
	 * Link in interface
	 */
	spp->sp_pif = pip;
	pip->pif_sigmgr = smp;
	pip->pif_siginst = (struct siginst *) spp;

	/*
	 * Kick-start the SPANS protocol
	 */
	SPANS_TIMER(spp, 0);

	/*
	 * Notify Connectionless Service
	 */
	err = spanscls_attach(spp);

	/*
	 * Log the fact that we've attached
	 */
	if (!err)
		log(LOG_INFO, "spans: attached to interface %s%d\n",
				pip->pif_name, pip->pif_unit);

done:
	/*
	 * Reset our work if attach fails
	 */
	if (err) {
		if (spp) {
			SPANS_CANCEL(spp);
			UNLINK((struct siginst *)spp, struct siginst,
					smp->sm_prinst, si_next);
			free(spp, M_DEVBUF);
		}
		s = splimp();
		pip->pif_sigmgr = NULL;
		pip->pif_siginst = NULL;
		(void) splx(s);
	}

	return (err);
}


/*
 * Detach a SPANS-controlled interface
 *
 * Each ATM physical interface may be detached from its signalling
 * manager (via the atm_sigmgr_detach function).  This function will
 * handle the detachment for all SPANS-controlled interfaces.  All
 * circuits will be immediately terminated.
 *
 * Function must be called at splnet.
 *
 * Arguments:
 *	pip	pointer to ATM physical interface control block
 *
 * Returns:
 *	0	detach successful
 *	errno	detach failed - reason indicated
 *
 */
static int
spans_detach(pip)
	struct atm_pif	*pip;
{
	struct spans		*spp;
	struct vccb		*vcp, *vnext;
	Atm_connection		*cop;
	int			err;

	ATM_DEBUG1("spans_detach: pip=%p\n", pip);

	/*
	 * Get SPANS protocol instance
	 */
	spp = (struct spans *)pip->pif_siginst;

	/*
	 * Return an error if we're already detaching
	 */
	if (spp->sp_state == SPANS_DETACH) {
		return(EALREADY);
	}

	/*
	 * Cancel any outstanding timer
	 */
	SPANS_CANCEL(spp);

	/*
	 * Notify Connectionless Service
	 */
	spanscls_detach(spp);

	/*
	 * Terminate all of our VCCs
	 */
	for (vcp = Q_HEAD(spp->sp_vccq, struct vccb); vcp; vcp = vnext) {

		vnext = Q_NEXT(vcp, struct vccb, vc_sigelem);

		/*
		 * Don't close the signalling VCC yet
		 */
		if (vcp->vc_connvc && vcp->vc_connvc->cvc_conn ==
				spp->sp_conn)
			continue;

		/*
		 * Close VCC and notify owner
		 */
		err = spans_clear_vcc(spp, (struct spans_vccb *)vcp);
		if (err) {
			log(LOG_ERR, "spans: error %d clearing VCCB %p\n",
					err, vcp);
		}
	}

	/*
	 * Now close the SPANS signalling VCC
	 */
	if ((cop = spp->sp_conn) != NULL) {
		err = atm_cm_release(cop, &spans_cause);
		if (err)
			ATM_DEBUG2(
					"spans_detach: close failed for SPANS signalling channel; cop=%p, err=%d\n",
					cop, err);
	}
	

	/*
	 * Get rid of protocol instance if there are no VCCs queued
	 */
	if (Q_HEAD(spp->sp_vccq, struct vccb) == NULL) {
		struct sigmgr   *smp = pip->pif_sigmgr;

		pip->pif_sigmgr = NULL;
		pip->pif_siginst = NULL;
		UNLINK((struct siginst *)spp, struct siginst,
				smp->sm_prinst, si_next);
		free(spp, M_DEVBUF);
	} else {
		/*
		 * Otherwise, wait for protocol instance to be freed
		 * during spans_free processing for the last queued VCC.
		 */
		spp->sp_state = SPANS_DETACH;
	}

	/*
	 * Log the fact that we've detached
	 */
	log(LOG_INFO, "spans: detached from interface %s%d\n",
			pip->pif_name, pip->pif_unit);

	return (0);
}


/*
 * Open a SPANS ATM Connection
 *
 * All service user requests to open a VC connection (via
 * atm_open_connection) over an ATM interface attached to the SPANS
 * signalling manager are handled here.
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	cvp	pointer to user's requested connection parameters
 *	errp	pointer to an int for extended error information
 *
 * Returns:
 *	CALL_PROCEEDING	connection establishment is in progress
 *	CALL_FAILED	connection establishment failed
 *	CALL_CONNECTED	connection has been successfully established
 *
 */
static int
spans_setup(cvp, errp)
	Atm_connvc	*cvp;
	int		*errp;
{
	struct atm_pif	*pip = cvp->cvc_attr.nif->nif_pif;
	struct spans	*spp = (struct spans *)pip->pif_siginst;
	int		rc = 0;

	ATM_DEBUG1("spans_setup: cvp=%p\n", cvp);

	/*
	 * Intialize the returned error code
	 */
	*errp = 0;

	/*
	 * Open the connection
	 */
	switch (cvp->cvc_attr.called.addr.address_format) {
	case T_ATM_PVC_ADDR:
		/*
		 * Create a PVC
		 */
		*errp = spans_open_vcc(spp, cvp);
		rc = (*errp ? CALL_FAILED : CALL_CONNECTED);
		break;

	case T_ATM_SPANS_ADDR:

		/*
		 * Create an SVC
		 */
		*errp = spans_open_vcc(spp, cvp);
		rc = (*errp ? CALL_FAILED : CALL_PROCEEDING);
		break;

	default:
		*errp = EPROTONOSUPPORT;
		rc = CALL_FAILED;
	}

	return (rc);
}


/*
 * Close a SPANS ATM Connection
 *
 * All service user requests to terminate a previously open VC
 * connection (via the atm_close_connection function), which is running
 * over an interface attached to the SPANS signalling manager, are
 * handled here.
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	vcp	pointer to connection's VC control block
 *	errp	pointer to an int for extended error information
 *
 * Returns:
 *	CALL_PROCEEDING	connection termination is in progress
 *	CALL_FAILED	connection termination failed
 *	CALL_CLEARED	connection has been successfully terminated
 *
 */
static int
spans_release(vcp, errp)
	struct vccb	*vcp;
	int		*errp;
{
	int		rc = 0;
	struct atm_pif	*pip = vcp->vc_pif;
	struct spans	*spp = (struct spans *)pip->pif_siginst;

	ATM_DEBUG1("spans_release: vcp=%p\n", vcp);

	/*
	 * Initialize returned error code
	 */
	*errp = 0;

	/*
	 * Make sure VCC is open
	 */
	if ((vcp->vc_sstate == SPANS_VC_NULL) ||
			(vcp->vc_sstate == SPANS_VC_CLOSE) ||
			(vcp->vc_sstate == SPANS_VC_FREE) ||
			(vcp->vc_ustate == VCCU_NULL) ||
			(vcp->vc_ustate == VCCU_CLOSED)) {
		*errp = EALREADY;
		return(CALL_FAILED);
	}

	/*
	 * Validate the connection type (PVC or SVC)
	 */
	if (!(vcp->vc_type & (VCC_PVC | VCC_SVC))) {
		*errp = EPROTONOSUPPORT;
		return(CALL_FAILED);
	}

	/*
	 * Close the VCCB
	 */
	*errp = spans_close_vcc(spp, (struct spans_vccb *)vcp, FALSE);

	/*
	 * Set the return code
	 */
	if (vcp->vc_type & VCC_PVC) {
		rc = (*errp ? CALL_FAILED : CALL_CLEARED);
	} else {
		rc = (*errp ? CALL_FAILED : CALL_PROCEEDING);
	}

	return (rc);
}


/*
 * Accept a SPANS Open from a remote host
 *
 * A user calls this routine (via the atm_accept_call function)
 * after it is notified that an open request was received for it.
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	vcp	pointer to user's VCCB
 *	errp	pointer to an int for extended error information
 *
 * Returns:
 *	CALL_PROCEEDING	connection establishment is in progress
 *	CALL_FAILED	connection establishment failed
 *	CALL_CONNECTED	connection has been successfully established
 *
 */
static int
spans_accept(vcp, errp)
	struct vccb	*vcp;
	int		*errp;
{
	struct atm_pif		*pip = vcp->vc_pif;
	struct spans		*spp = (struct spans *)pip->pif_siginst;
	struct spans_vccb	*svp = (struct spans_vccb *)vcp;

	ATM_DEBUG1("spans_accept: vcp=%p\n", vcp);

	/*
	 * Initialize the returned error code
	 */
	*errp = 0;

	/*
	 * Return an error if we're detaching
	 */
	if (spp->sp_state == SPANS_DETACH) {
		*errp = ENETDOWN;
		ATM_DEBUG0("spans_accept: detaching\n");
		return(CALL_FAILED);
	}

	/*
	 * Respond to the open request
	 */
	*errp = spans_send_open_rsp(spp, svp, SPANS_OK);
	if (*errp) {
		ATM_DEBUG0("spans_accept: spans_send_open_rsp failed\n");
		goto failed;
	}

	/*
	 * Update the VCC states
	 */
	svp->sv_sstate = SPANS_VC_OPEN;
	svp->sv_ustate = VCCU_OPEN;

	return(CALL_CONNECTED);

failed:
	/*
	 * On error, free the VCCB and return CALL_FAILED
	 */
	svp->sv_sstate = SPANS_VC_FREE;
	svp->sv_ustate = VCCU_CLOSED;
	DEQUEUE(svp, struct spans_vccb, sv_sigelem, spp->sp_vccq);
	spans_free((struct vccb *)svp);

	return(CALL_FAILED);
}


/*
 * Reject a SPANS Open from a remote host
 *
 * A user calls this routine (via the atm_reject_call function)
 * after it is notified that an open request was received for it.
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	vcp	pointer to user's VCCB
 *	errp	pointer to an int for extended error information
 *
 * Returns:
 *	CALL_CLEARED	call request rejected
 *	CALL_FAILED	call rejection failed
 *
 */
static int
spans_reject(vcp, errp)
	struct vccb	*vcp;
	int		*errp;
{
	struct atm_pif		*pip = vcp->vc_pif;
	struct spans		*spp = (struct spans *)pip->pif_siginst;
	struct spans_vccb	*svp = (struct spans_vccb *)vcp;

	ATM_DEBUG1("spans_reject: vcp=%p\n", vcp);

	/*
	 * Initialize the returned error code
	 */
	*errp = 0;

	/*
	 * Return an error if we're detaching
	 */
	if (spp->sp_state == SPANS_DETACH) {
		*errp = ENETDOWN;
		ATM_DEBUG0("spans_reject: detaching\n");
		return(CALL_FAILED);
	}

	ATM_DEBUG1("spans_reject: cause code is %d\n",
			vcp->vc_connvc->cvc_attr.cause.v.cause_value);

	/*
	 * Clean up the VCCB--the connection manager will free it
	 * spans_close_vcc will send a SPANS open response
	 */
	if ((*errp = spans_close_vcc(spp, svp, TRUE)) != 0) {
		ATM_DEBUG0("spans_reject: spans_close_vcc failed\n");
		return(CALL_FAILED);
	}

	return(CALL_CLEARED);
}


/*
 * Abort a SPANS ATM Connection
 *
 * All (non-user) requests to abort a previously open VC connection (via
 * the atm_abort_connection function), which is running over an
 * interface attached to the SPANS signalling manager, are handled here.
 * The VCC owner will be notified of the request, in order to initiate
 * termination of the connection.
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	vcp	pointer to connection's VC control block
 *
 * Returns:
 *	0	connection release was succesful
 *	errno	connection release failed - reason indicated
 *
 */
int
spans_abort(vcp)
	struct vccb	*vcp;
{

	/*
	 * Make sure VCC is available
	 */
	if ((vcp->vc_sstate == SPANS_VC_NULL) ||
			(vcp->vc_sstate == SPANS_VC_CLOSE) ||
			(vcp->vc_sstate == SPANS_VC_FREE) ||
			(vcp->vc_ustate == VCCU_NULL) ||
			(vcp->vc_ustate == VCCU_CLOSED)) {
		return(EALREADY);
	}

	/*
	 * Only abort once
	 */
	if (vcp->vc_sstate == SPANS_VC_ABORT) {
		return (EALREADY);
	}

	/*
	 * Cancel any timer that might be running
	 */
	SPANS_VC_CANCEL(vcp);

	/*
	 * Set immediate timer to schedule connection termination
	 */
	vcp->vc_sstate = SPANS_VC_ABORT;
	SPANS_VC_TIMER(vcp, 0);

	return (0);
}


/*
 * Free SPANS ATM connection resources
 *
 * All service user requests to free the resources of a closed
 * VCC connection (via the atm_free_connection function), which
 * is running over an interface attached to the SigPVC signalling
 * manager, are handled here.
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	vcp	pointer to connection's VC control block
 *
 * Returns:
 *	0	connection free was successful
 *	errno	connection free failed - reason indicated
 *
 */
int
spans_free(vcp)
	struct vccb	*vcp;
{
	struct atm_pif *pip = vcp->vc_pif;
	struct spans *spp = (struct spans *)pip->pif_siginst;

	ATM_DEBUG1("spans_free: vcp = %p\n", vcp);

	/*
	 * Make sure VCC has been closed
	 */
	if ((vcp->vc_ustate != VCCU_CLOSED) ||
			(vcp->vc_sstate != SPANS_VC_FREE)) {
		ATM_DEBUG2("spans_free: bad state, sstate=%d, ustate=%d\n",
				vcp->vc_sstate, vcp->vc_ustate);
		return(EEXIST);
	}

	/*
	 * Remove VCCB from protocol queue
	 */
	DEQUEUE(vcp, struct vccb, vc_sigelem, spp->sp_vccq);

	/*
	 * Free VCCB storage
	 */
	vcp->vc_ustate = VCCU_NULL;
	vcp->vc_sstate = SPANS_VC_NULL;
	uma_zfree(spans_vc_zone, vcp);

	/*
	 * If we're detaching and this was the last VCC queued,
	 * get rid of the protocol instance
	 */
	if ((spp->sp_state == SPANS_DETACH) &&
			(Q_HEAD(spp->sp_vccq, struct vccb) == NULL)) {
		struct sigmgr   *smp = pip->pif_sigmgr;

		pip->pif_sigmgr = NULL;
		pip->pif_siginst = NULL;
		UNLINK((struct siginst *)spp, struct siginst, smp->sm_prinst,
				si_next);
		free(spp, M_DEVBUF);
	}

	return (0);
}


/*
 * SPANS IOCTL support
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	code	PF_ATM sub-operation code
 *      data    pointer to code specific parameter data area
 *      arg1    pointer to code specific argument
 *
 * Returns:
 *	0	request procesed
 *	errno	error processing request - reason indicated
 *
 */
static int
spans_ioctl(code, data, arg1)
        int		code;
        caddr_t		data;
        caddr_t		arg1;
{
	struct atmdelreq	*adp;
	struct atminfreq	*aip;
	struct spans		*spp;
	struct spans_vccb	*svp;
	struct air_vcc_rsp	rsp;
	Atm_connection		*cop;
	int			err = 0, i, vpi, vci;
	size_t buf_len;
	caddr_t			buf_addr;


	switch (code) {

	case AIOCS_DEL_PVC:
	case AIOCS_DEL_SVC:
		/*
		 * Delete a VCC
		 */
		adp = (struct atmdelreq *)data;
		spp = (struct spans *)arg1;

		/*
		 * Don't let a user close the SPANS signalling VC or
		 * the SPANS CLS VC
		 */
		vpi = adp->adr_pvc_vpi;
		vci = adp->adr_pvc_vci;
		if ((vpi == SPANS_SIG_VPI && vci == SPANS_SIG_VCI) ||
				(vpi == SPANS_CLS_VPI &&
				vci == SPANS_CLS_VCI))
			return(EINVAL);

		/*
		 * Find requested VCC
		 */
		for (svp = Q_HEAD(spp->sp_vccq, struct spans_vccb); svp;
				svp = Q_NEXT(svp, struct spans_vccb, sv_sigelem)) {
			if ((svp->sv_vpi == vpi) && (svp->sv_vci == vci))
				break;
		}
		if (svp == NULL)
			return (ENOENT);

		/*
		 * Check VCC type
		 */
		switch (code) {
		case AIOCS_DEL_PVC:
			if (!(svp->sv_type & VCC_PVC)) {
				return(EINVAL);
			}
			break;
		case AIOCS_DEL_SVC:
			if (!(svp->sv_type & VCC_SVC)) {
				return(EINVAL);
			}
			break;
		}

		/*
		 * Schedule VCC termination
		 */
		err = spans_abort((struct vccb *)svp);
		break;

	case AIOCS_INF_VCC:
		/*
		 * Return VCC information
		 */
		aip = (struct atminfreq *)data;
		spp = (struct spans *)arg1;

		buf_addr = aip->air_buf_addr;
		buf_len = aip->air_buf_len;

		/*
		 * Loop through the VCC queue
		 */
		for (svp = Q_HEAD(spp->sp_vccq, struct spans_vccb); svp;
				svp = Q_NEXT(svp, struct spans_vccb, sv_sigelem)) {
			/*
			 * Make sure there's room in the user's buffer
			 */
			if (buf_len < sizeof(rsp)) {
				err = ENOSPC;
				break;
			}

			/*
			 * Fill out the response struct for the VCC
			 */
			(void) snprintf(rsp.avp_intf,
				    sizeof(rsp.avp_intf), "%s%d",
					spp->sp_pif->pif_name,
					spp->sp_pif->pif_unit);
			rsp.avp_vpi = svp->sv_vpi;
			rsp.avp_vci = svp->sv_vci;
			rsp.avp_type = svp->sv_type;
			rsp.avp_aal = svp->sv_connvc->cvc_attr.aal.type;
			rsp.avp_sig_proto = svp->sv_proto;
			cop = svp->sv_connvc->cvc_conn;
			if (cop)
				rsp.avp_encaps = cop->co_mpx;
			else
				rsp.avp_encaps = 0;
			rsp.avp_state = svp->sv_sstate;
			bzero(rsp.avp_owners, sizeof(rsp.avp_owners));
			for (i = 0; cop && i < sizeof(rsp.avp_owners);
					cop = cop->co_next,
					i += T_ATM_APP_NAME_LEN+1) {
				strncpy(&rsp.avp_owners[i],
					cop->co_endpt->ep_getname(cop->co_toku),
					T_ATM_APP_NAME_LEN);
			}
			rsp.avp_daddr.address_format = T_ATM_SPANS_ADDR;
			rsp.avp_daddr.address_length = 
					sizeof(Atm_addr_spans);
			if (svp->sv_type & VCC_OUT) {
				spans_addr_copy(&svp->sv_conn.con_dst,
						rsp.avp_daddr.address);
			} else {
				spans_addr_copy(&svp->sv_conn.con_src,
						rsp.avp_daddr.address);
			}
			rsp.avp_dsubaddr.address_format = T_ATM_ABSENT;
			rsp.avp_dsubaddr.address_length = 0;
			rsp.avp_ipdus = svp->sv_ipdus;
			rsp.avp_opdus = svp->sv_opdus;
			rsp.avp_ibytes = svp->sv_ibytes;
			rsp.avp_obytes = svp->sv_obytes;
			rsp.avp_ierrors = svp->sv_ierrors;
			rsp.avp_oerrors = svp->sv_oerrors;
			rsp.avp_tstamp = svp->sv_tstamp;

			/*
			 * Copy the response into the user's buffer
			 */
			if ((err = copyout((caddr_t)&rsp, buf_addr,
					sizeof(rsp))) != 0)
				break;
			buf_addr += sizeof(rsp);
			buf_len -= sizeof(rsp);
		}

		/*
		 * Update the buffer pointer and length
		 */
		aip->air_buf_addr = buf_addr;
		aip->air_buf_len = buf_len;
		break;

	case AIOCS_ADD_ARP:
	case AIOCS_DEL_ARP:
	case AIOCS_INF_ARP:
	case AIOCS_INF_ASV:
		/*
		 * ARP specific ioctl's
		 */
		err = spansarp_ioctl(code, data, arg1);
		break;

	default:
		err = EOPNOTSUPP;
	}

	return (err);
}


#ifdef ATM_SPANS_MODULE
/*
 *******************************************************************
 *
 * Loadable Module Support
 *
 *******************************************************************
 */
static int	spans_doload(void);
static int	spans_dounload(void);

/*
 * Generic module load processing
 * 
 * This function is called by an OS-specific function when this
 * module is being loaded.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	load was successful 
 *	errno	load failed - reason indicated
 *
 */
static int
spans_doload()
{
	int	err = 0;

	/*
	 * Start us up
	 */
	err = spans_start();
	if (err)
		/* Problems, clean up */
		(void)spans_stop();

	return (err);
}


/*
 * Generic module unload processing
 * 
 * This function is called by an OS-specific function when this
 * module is being unloaded.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	unload was successful 
 *	errno	unload failed - reason indicated
 *
 */
static int
spans_dounload()
{
	int	err = 0;

	/*
	 * OK, try to clean up our mess
	 */
	err = spans_stop();

	return (err);
}




#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

/*
 * Loadable miscellaneous module description
 */
MOD_MISC(spans);


/*
 * Loadable module support "load" entry point
 * 
 * This is the routine called by the lkm driver whenever the
 * modload(1) command is issued for this module.
 *
 * Arguments:
 *	lkmtp	pointer to lkm drivers's structure
 *	cmd	lkm command code
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
static int
spans_load(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(spans_doload());
}


/*
 * Loadable module support "unload" entry point
 * 
 * This is the routine called by the lkm driver whenever the
 * modunload(1) command is issued for this module.
 *
 * Arguments:
 *	lkmtp	pointer to lkm drivers's structure
 *	cmd	lkm command code
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
static int
spans_unload(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(spans_dounload());
}


/*
 * Loadable module support entry point
 * 
 * This is the routine called by the lkm driver for all loadable module
 * functions for this driver.  This routine name must be specified
 * on the modload(1) command.  This routine will be called whenever the
 * modload(1), modunload(1) or modstat(1) commands are issued for this
 * module.
 *
 * Arguments:
 *	lkmtp	pointer to lkm drivers's structure
 *	cmd	lkm command code
 *	ver	lkm version
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
int
spans_mod(lkmtp, cmd, ver)
	struct lkm_table	*lkmtp;
	int		cmd;
	int		ver;
{
	MOD_DISPATCH(spans, lkmtp, cmd, ver,
		spans_load, spans_unload, lkm_nullcmd);
}

#else	/* !ATM_SPANS_MODULE */

/*
 *******************************************************************
 *
 * Kernel Compiled Module Support
 *
 *******************************************************************
 */
static void	spans_doload(void *);

SYSINIT(atmspans, SI_SUB_PROTO_END, SI_ORDER_ANY, spans_doload, NULL)

/*
 * Kernel initialization
 * 
 * Arguments:
 *	arg	Not used
 *
 * Returns:
 *	none
 *
 */
static void
spans_doload(void *arg)
{
	int	err = 0;

	/*
	 * Start us up
	 */
	err = spans_start();
	if (err) {
		/* Problems, clean up */
		(void)spans_stop();

		log(LOG_ERR, "ATM SPANS unable to initialize (%d)!!\n", err);
	}
	return;
}
#endif	/* ATM_SPANS_MODULE */

