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
 *	@(#) $FreeBSD: src/sys/netatm/uni/unisig_if.c,v 1.8 2000/01/17 20:49:56 mks Exp $
 *
 */

/*
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * System interface module
 *
 */

#include <netatm/kern_include.h>

#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>
#include <netatm/uni/uniip_var.h>

#include <netatm/uni/unisig.h>
#include <netatm/uni/unisig_var.h>
#include <netatm/uni/unisig_msg.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/netatm/uni/unisig_if.c,v 1.8 2000/01/17 20:49:56 mks Exp $");
#endif


/*
 * Global variables
 */
struct sp_info	unisig_vcpool = {
	"unisig vcc pool",		/* si_name */
	sizeof(struct unisig_vccb),	/* si_blksiz */
	10,				/* si_blkcnt */
	50				/* si_maxallow */
};

struct sp_info	unisig_msgpool = {
	"unisig message pool",		/* si_name */
	sizeof(struct unisig_msg),	/* si_blksiz */
	10,				/* si_blkcnt */
	50				/* si_maxallow */
};

struct sp_info	unisig_iepool = {
	"unisig ie pool",		/* si_name */
	sizeof(struct ie_generic),	/* si_blksiz */
	10,				/* si_blkcnt */
	50				/* si_maxallow */
};


/*
 * Local functions
 */
static int	unisig_attach __P((struct sigmgr *, struct atm_pif *));
static int	unisig_detach __P((struct atm_pif *));
static int	unisig_setup __P((Atm_connvc *, int *));
static int	unisig_release __P((struct vccb *, int *));
static int	unisig_accept __P((struct vccb *, int *));
static int	unisig_reject __P((struct vccb *, int *));
static int	unisig_abort __P((struct vccb *));
static int	unisig_ioctl __P((int, caddr_t, caddr_t));


/*
 * Local variables
 */
static struct sigmgr	unisig_mgr30 = {
	NULL,
	ATM_SIG_UNI30,
	NULL,
	unisig_attach,
	unisig_detach,
	unisig_setup,
	unisig_accept,
	unisig_reject,
	unisig_release,
	unisig_free,
	unisig_ioctl
};

static struct sigmgr	unisig_mgr31 = {
	NULL,
	ATM_SIG_UNI31,
	NULL,
	unisig_attach,
	unisig_detach,
	unisig_setup,
	unisig_accept,
	unisig_reject,
	unisig_release,
	unisig_free,
	unisig_ioctl
};


/*
 * Initialize UNISIG processing
 *
 * This will be called during module loading.  We'll just register
 * the UNISIG protocol descriptor and wait for a UNISIG ATM interface
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
int
unisig_start()
{
	int	err = 0;

	/*      
	 * Verify software version
	 */     
	if (atm_version != ATM_VERSION) { 
		log(LOG_ERR, "version mismatch: unisig=%d.%d kernel=%d.%d\n", 
				ATM_VERS_MAJ(ATM_VERSION),
				ATM_VERS_MIN(ATM_VERSION),
				ATM_VERS_MAJ(atm_version),
				ATM_VERS_MIN(atm_version));
		return (EINVAL);
	}

	/*
	 * Register ourselves with system
	 */
	err = atm_sigmgr_register(&unisig_mgr30);
	if (err)
		goto done;

	err = atm_sigmgr_register(&unisig_mgr31);

done:
	return (err);
}


/*
 * Halt UNISIG processing
 *
 * This should be called just prior to unloading the module from
 * memory.  All UNISIG interfaces must be deregistered before the
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
int
unisig_stop()
{
	int	err = 0;
	int	s = splnet();


	/*
	 * Any protocol instances still registered?
	 */
	if ((unisig_mgr30.sm_prinst != NULL) ||
	    (unisig_mgr31.sm_prinst != NULL)) {

		/* Yes, can't stop now */
		err = EBUSY;
		goto done;
	}

	/*
	 * De-register from system
	 */
	(void) atm_sigmgr_deregister(&unisig_mgr30);
	(void) atm_sigmgr_deregister(&unisig_mgr31);

	/*
	 * Free up our storage pools
	 */
	atm_release_pool(&unisig_vcpool);
	atm_release_pool(&unisig_msgpool);
	atm_release_pool(&unisig_iepool);

done:
	(void) splx(s);
	return (err);
}


/*
 * Attach a UNISIG-controlled interface
 *
 * Each ATM physical interface must be attached with the signalling
 * manager for the interface's signalling protocol (via the
 * atm_sigmgr_attach function).  This function will handle the
 * attachment for UNISIG-controlled interfaces.  A new UNISIG protocol
 * instance will be created and then we'll just sit around waiting for
 * status or connection requests.
 *
 * Function must be called at splnet.
 *
 * Arguments:
 *	smp	pointer to UNISIG signalling manager control block
 *	pip	pointer to ATM physical interface control block
 *
 * Returns:
 *	0	attach successful
 *	errno	attach failed - reason indicated
 *
 */
static int
unisig_attach(smp, pip)
	struct sigmgr	*smp;
	struct atm_pif	*pip;
{
	int			err = 0, s;
	struct unisig		*usp = NULL;

	ATM_DEBUG2("unisig_attach: smp=%p, pip=%p\n", smp, pip);

	/*
	 * Allocate UNISIG protocol instance control block
	 */
	usp = (struct unisig *)
			KM_ALLOC(sizeof(struct unisig), M_DEVBUF, M_NOWAIT);
	if (usp == NULL) {
		err = ENOMEM;
		goto done;
	}
	KM_ZERO(usp, sizeof(struct unisig));

	/*
	 * Set state in UNISIG protocol instance control block
	 */
	usp->us_state = UNISIG_NULL;
	usp->us_proto = smp->sm_proto;

	/*
	 * Set initial call reference allocation value
	 */
	usp->us_cref = 1;

	/*
	 * Link instance into manager's chain
	 */
	LINK2TAIL((struct siginst *)usp, struct siginst, smp->sm_prinst,
			si_next);

	/*
	 * Link in interface
	 */
	usp->us_pif = pip;
	s = splimp();
	pip->pif_sigmgr = smp;
	pip->pif_siginst = (struct siginst *) usp;
	(void) splx(s);

	/*
	 * Clear our ATM address.  The address will be set by user
	 * command or by registration via ILMI.
	 */
	usp->us_addr.address_format = T_ATM_ABSENT;
	usp->us_addr.address_length = 0;
	usp->us_subaddr.address_format = T_ATM_ABSENT;
	usp->us_subaddr.address_length = 0;

	/*
	 * Set pointer to IP
	 */
	usp->us_ipserv = &uniip_ipserv;

	/*
	 * Kick-start the UNISIG protocol
	 */
	UNISIG_TIMER(usp, 0);

	/*
	 * Log the fact that we've attached
	 */
	log(LOG_INFO, "unisig: attached to interface %s%d\n",
			pip->pif_name, pip->pif_unit);

done:
	/*
	 * Reset our work if attach fails
	 */
	if (err) {
		if (usp) {
			UNISIG_CANCEL(usp);
			UNLINK((struct siginst *)usp, struct siginst,
					smp->sm_prinst, si_next);
			KM_FREE(usp, sizeof(struct unisig), M_DEVBUF);
		}
		s = splimp();
		pip->pif_sigmgr = NULL;
		pip->pif_siginst = NULL;
		(void) splx(s);
	}

	return (err);
}


/*
 * Detach a UNISIG-controlled interface
 *
 * Each ATM physical interface may be detached from its signalling
 * manager (via the atm_sigmgr_detach function).  This function will
 * handle the detachment for all UNISIG-controlled interfaces.  All
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
unisig_detach(pip)
	struct atm_pif	*pip;
{
	struct unisig		*usp;
	int			err;

	ATM_DEBUG1("unisig_detach: pip=%p\n", pip);

	/*
	 * Get UNISIG protocol instance
	 */
	usp = (struct unisig *)pip->pif_siginst;

	/*
	 * Return an error if we're already detaching
	 */
	if (usp->us_state == UNISIG_DETACH) {
		return(EALREADY);
	}

	/*
	 * Pass the detach event to the signalling manager
	 * state machine
	 */
	err = unisig_sigmgr_state(usp, UNISIG_SIGMGR_DETACH,
			(KBuffer *)0);

	/*
	 * Log the fact that we've detached
	 */
	if (!err)
		log(LOG_INFO, "unisig: detached from interface %s%d\n",
				pip->pif_name, pip->pif_unit);

	return (0);
}


/*
 * Open a UNISIG ATM Connection
 *
 * All service user requests to open a VC connection (via
 * atm_open_connection) over an ATM interface attached to the UNISIG
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
unisig_setup(cvp, errp)
	Atm_connvc	*cvp;
	int		*errp;
{
	struct atm_pif	*pip = cvp->cvc_attr.nif->nif_pif;
	struct unisig	*usp = (struct unisig *)pip->pif_siginst;
	int		rc = 0;

	ATM_DEBUG1("unisig_setup: cvp=%p\n", cvp);

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
		*errp = unisig_open_vcc(usp, cvp);
		rc = (*errp ? CALL_FAILED : CALL_CONNECTED);
		break;

	case T_ATM_ENDSYS_ADDR:
	case T_ATM_E164_ADDR:

		/*
		 * Create an SVC
		 */
		*errp = unisig_open_vcc(usp, cvp);
		rc = (*errp ? CALL_FAILED : CALL_PROCEEDING);
		break;

	default:
		*errp = EPROTONOSUPPORT;
		rc = CALL_FAILED;
	}

	return (rc);
}


/*
 * Close a UNISIG ATM Connection
 *
 * All service user requests to terminate a previously open VC
 * connection (via the atm_close_connection function), which is running
 * over an interface attached to the UNISIG signalling manager, are
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
unisig_release(vcp, errp)
	struct vccb	*vcp;
	int		*errp;
{
	int		rc = 0;
	struct atm_pif	*pip = vcp->vc_pif;
	struct unisig	*usp = (struct unisig *)pip->pif_siginst;

	ATM_DEBUG1("unisig_release: vcp=%p\n", vcp);

	/*
	 * Initialize returned error code
	 */
	*errp = 0;

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
	*errp = unisig_close_vcc(usp, (struct unisig_vccb *)vcp);

	/*
	 * Set the return code
	 */
	if (*errp) {
		rc = CALL_FAILED;
	} else if (vcp->vc_sstate == UNI_NULL ||
			vcp->vc_sstate == UNI_FREE) {
		rc = CALL_CLEARED;
	} else {
		rc = CALL_PROCEEDING;
	}

	return (rc);
}


/*
 * Accept a UNISIG Open from a remote host
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
unisig_accept(vcp, errp)
	struct vccb	*vcp;
	int		*errp;
{
	struct unisig_vccb	*uvp = (struct unisig_vccb *)vcp;
	struct atm_pif		*pip = uvp->uv_pif;
	struct unisig		*usp = (struct unisig *)pip->pif_siginst;

	ATM_DEBUG1("unisig_accept: vcp=%p\n", vcp);

	/*
	 * Initialize the returned error code
	 */
	*errp = 0;

	/*
	 * Return an error if we're detaching
	 */
	if (usp->us_state == UNISIG_DETACH) {
		*errp = ENETDOWN;
		goto free;
	}

	/*
	 * Return an error if we lost the connection
	 */
	if (uvp->uv_sstate == UNI_FREE) {
		*errp = ENETDOWN;
		goto free;
	}

	/*
	 * Pass the acceptance to the VC state machine
	 */
	*errp = unisig_vc_state(usp, uvp, UNI_VC_ACCEPT_CALL,
			(struct unisig_msg *) 0);
	if (*errp)
		goto failed;

	return(CALL_PROCEEDING);

failed:
	/*
	 * On error, free the VCCB and return CALL_FAILED
	 */

free:
	uvp->uv_sstate = UNI_FREE;
	uvp->uv_ustate = VCCU_CLOSED;
	DEQUEUE(uvp, struct unisig_vccb, uv_sigelem, usp->us_vccq);
	unisig_free((struct vccb *)uvp);

	return(CALL_FAILED);
}


/*
 * Reject a UNISIG Open from a remote host
 *
 * A user calls this routine (via the atm_reject_call function)
 * after it is notified that an open request was received for it.
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	uvp	pointer to user's VCCB
 *	errp	pointer to an int for extended error information
 *
 * Returns:
 *	CALL_CLEARED	call request rejected
 *	CALL_FAILED	call rejection failed
 *
 */
static int
unisig_reject(vcp, errp)
	struct vccb	*vcp;
	int		*errp;
{
	struct unisig_vccb	*uvp = (struct unisig_vccb *)vcp;
	struct atm_pif		*pip = uvp->uv_pif;
	struct unisig		*usp = (struct unisig *)pip->pif_siginst;

	ATM_DEBUG1("unisig_reject: uvp=%p\n", uvp);

	/*
	 * Initialize the returned error code
	 */
	*errp = 0;


	/*
	 * Return an error if we're detaching
	 */
	if (usp->us_state == UNISIG_DETACH) {
		*errp = ENETDOWN;
		goto failed;
	}

	/*
	 * Call the VC state machine
	 */
	*errp = unisig_vc_state(usp, uvp, UNI_VC_REJECT_CALL,
			(struct unisig_msg *) 0);
	if (*errp)
		goto failed;

	return(CALL_CLEARED);

failed:
	/*
	 * On error, free the VCCB and return CALL_FAILED
	 */
	uvp->uv_sstate = UNI_FREE;
	uvp->uv_ustate = VCCU_CLOSED;
	DEQUEUE(uvp, struct unisig_vccb, uv_sigelem, usp->us_vccq);
	(void) unisig_free((struct vccb *)uvp);
	return(CALL_FAILED);
}


/*
 * Abort a UNISIG ATM Connection
 *
 * All (non-user) requests to abort a previously open VC connection (via
 * the atm_abort_connection function), which is running over an
 * interface attached to the UNISIG signalling manager, are handled here.
 * The VCC owner will be notified of the request, in order to initiate
 * termination of the connection.
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *      vcp     pointer to connection's VC control block
 *
 * Returns:
 *      0       connection release was successful
 *      errno   connection release failed - reason indicated
 *
 */
static int
unisig_abort(vcp)
	struct vccb	*vcp;
{

	ATM_DEBUG1("unisig_abort: vcp=%p\n", vcp);

	/*
	 * Only abort once
	 */
	if (vcp->vc_ustate == VCCU_ABORT) {
		return (EALREADY);
	}

	/*
	 * Cancel any timer that might be running
	 */
	UNISIG_VC_CANCEL(vcp);

	/*
	 * Set immediate timer to schedule connection termination
	 */
	vcp->vc_ustate = VCCU_ABORT;
	UNISIG_VC_TIMER(vcp, 0);

	return (0);
}


/*
 * Free UNISIG ATM connection resources
 *
 * All service user requests to free the resources of a closed VCC
 * connection (via the atm_free_connection function), which is running
 * over an interface attached to the UNISIG signalling manager, are
 *handled here.
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
unisig_free(vcp)
	struct vccb	*vcp;
{
	struct atm_pif *pip = vcp->vc_pif;
	struct unisig *usp = (struct unisig *)pip->pif_siginst;

	ATM_DEBUG1("unisig_free: vcp = %p\n", vcp);

	/*
	 * Make sure VCC has been closed
	 */
	if ((vcp->vc_ustate != VCCU_CLOSED &&
			vcp->vc_ustate != VCCU_ABORT) ||
			vcp->vc_sstate != UNI_FREE) {
		ATM_DEBUG2("unisig_free: bad state, sstate=%d, ustate=%d\n",
				vcp->vc_sstate, vcp->vc_ustate);
		return(EEXIST);
	}

	/*
	 * Remove VCCB from protocol queue
	 */
	DEQUEUE(vcp, struct vccb, vc_sigelem, usp->us_vccq);

	/*
	 * Free VCCB storage
	 */
	vcp->vc_ustate = VCCU_NULL;
	vcp->vc_sstate = UNI_NULL;
	atm_free((caddr_t)vcp);

	/*
	 * If we're detaching and this was the last VCC queued,
	 * get rid of the protocol instance
	 */
	if ((usp->us_state == UNISIG_DETACH) &&
			(Q_HEAD(usp->us_vccq, struct vccb) == NULL)) {
		struct sigmgr   *smp = pip->pif_sigmgr;
		int     s = splimp();

		pip->pif_sigmgr = NULL;
		pip->pif_siginst = NULL;
		(void) splx(s);

		UNLINK((struct siginst *)usp, struct siginst,
				smp->sm_prinst, si_next);
		KM_FREE(usp, sizeof(struct unisig), M_DEVBUF);
	}

	return (0);
}


/*
 * UNISIG IOCTL support
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
unisig_ioctl(code, data, arg1)
        int		code;
        caddr_t		data;
        caddr_t		arg1;
{
	struct atmdelreq	*adp;
	struct atminfreq	*aip;
	struct atmsetreq	*asp;
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct air_vcc_rsp	rsp;
	struct atm_pif		*pip;
	Atm_connection		*cop;
	u_int			vpi, vci;
	int			err = 0, buf_len, i;
	caddr_t			buf_addr;

	ATM_DEBUG1("unisig_ioctl: code=%d\n", code);

	switch (code) {

	case AIOCS_DEL_PVC:
	case AIOCS_DEL_SVC:
		/*
		 * Delete a VCC
		 */
		adp = (struct atmdelreq *)data;
		usp = (struct unisig *)arg1;

		/*
		 * Don't let a user close the UNISIG signalling VC
		 */
		vpi = adp->adr_pvc_vpi;
		vci = adp->adr_pvc_vci;
		if ((vpi == UNISIG_SIG_VPI && vci == UNISIG_SIG_VCI))
			return(EINVAL);

		/*
		 * Find requested VCC
		 */
		for (uvp = Q_HEAD(usp->us_vccq, struct unisig_vccb); uvp;
				uvp = Q_NEXT(uvp, struct unisig_vccb, uv_sigelem)) {
			if ((uvp->uv_vpi == vpi) && (uvp->uv_vci == vci))
				break;
		}
		if (uvp == NULL)
			return (ENOENT);

		/*
		 * Check VCC type
		 */
		switch (code) {
		case AIOCS_DEL_PVC:
			if (!(uvp->uv_type & VCC_PVC)) {
				return(EINVAL);
			}
			break;
		case AIOCS_DEL_SVC:
			if (!(uvp->uv_type & VCC_SVC)) {
				return(EINVAL);
			}
			break;
		}

		/*
		 * Schedule VCC termination
		 */
		unisig_cause_attr_from_user(&uvp->uv_connvc->cvc_attr,
				T_ATM_CAUSE_UNSPECIFIED_NORMAL);
		err = unisig_abort((struct vccb *)uvp);
		break;

	case AIOCS_INF_VCC:
		/*
		 * Return VCC information
		 */
		aip = (struct atminfreq *)data;
		usp = (struct unisig *)arg1;

		buf_addr = aip->air_buf_addr;
		buf_len = aip->air_buf_len;

		/*
		 * Loop through the VCC queue
		 */
		for (uvp = Q_HEAD(usp->us_vccq, struct unisig_vccb); uvp;
				uvp = Q_NEXT(uvp, struct unisig_vccb, uv_sigelem)) {
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
					usp->us_pif->pif_name,
					usp->us_pif->pif_unit);
			rsp.avp_vpi = uvp->uv_vpi;
			rsp.avp_vci = uvp->uv_vci;
			rsp.avp_type = uvp->uv_type;
			rsp.avp_aal = uvp->uv_connvc->cvc_attr.aal.type;
			rsp.avp_sig_proto = uvp->uv_proto;
			cop = uvp->uv_connvc->cvc_conn;
			if (cop)
				rsp.avp_encaps = cop->co_mpx;
			else
				rsp.avp_encaps = 0;
			rsp.avp_state = uvp->uv_sstate;
			if (uvp->uv_connvc->cvc_flags & CVCF_CALLER) {
				rsp.avp_daddr = uvp->uv_connvc->cvc_attr.called.addr;
			} else {
				rsp.avp_daddr = uvp->uv_connvc->cvc_attr.calling.addr;
			}
			rsp.avp_dsubaddr.address_format = T_ATM_ABSENT;
			rsp.avp_dsubaddr.address_length = 0;
			rsp.avp_ipdus = uvp->uv_ipdus;
			rsp.avp_opdus = uvp->uv_opdus;
			rsp.avp_ibytes = uvp->uv_ibytes;
			rsp.avp_obytes = uvp->uv_obytes;
			rsp.avp_ierrors = uvp->uv_ierrors;
			rsp.avp_oerrors = uvp->uv_oerrors;
			rsp.avp_tstamp = uvp->uv_tstamp;
			KM_ZERO(rsp.avp_owners,
					sizeof(rsp.avp_owners));
			for (i = 0; cop && i < sizeof(rsp.avp_owners);
					cop = cop->co_next,
					i += T_ATM_APP_NAME_LEN+1) {
				strncpy(&rsp.avp_owners[i],
					cop->co_endpt->ep_getname(cop->co_toku),
					T_ATM_APP_NAME_LEN);
			}

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

	case AIOCS_INF_ARP:
	case AIOCS_INF_ASV:
	case AIOCS_SET_ASV:
		/*
		 * Get ARP table information or get/set ARP server address
		 */
		err = uniarp_ioctl(code, data, arg1);
		break;

	case AIOCS_SET_PRF:
		/*
		 * Set NSAP prefix
		 */
		asp = (struct atmsetreq *)data;
		usp = (struct unisig *)arg1;
		pip = usp->us_pif;
		if (usp->us_addr.address_format != T_ATM_ABSENT) {
			if (KM_CMP(asp->asr_prf_pref, usp->us_addr.address,
					sizeof(asp->asr_prf_pref)) != 0)
				err = EALREADY;
			break;
		}
		usp->us_addr.address_format = T_ATM_ENDSYS_ADDR;
		usp->us_addr.address_length = sizeof(Atm_addr_nsap);
		KM_COPY(&pip->pif_macaddr,
			((Atm_addr_nsap *)usp->us_addr.address)->aan_esi,
			sizeof(pip->pif_macaddr));
		KM_COPY((caddr_t) asp->asr_prf_pref,
			&((Atm_addr_nsap *)usp->us_addr.address)->aan_afi,
			sizeof(asp->asr_prf_pref));
		log(LOG_INFO, "uni: set address %s on interface %s\n",
				unisig_addr_print(&usp->us_addr),
				asp->asr_prf_intf);

		/*
		 * Pass event to signalling manager state machine
		 */
		err = unisig_sigmgr_state(usp, UNISIG_SIGMGR_ADDR_SET,
				(KBuffer *) NULL);

		/*
		 * Clean up if there was an error
		 */
		if (err) {
			usp->us_addr.address_format = T_ATM_ABSENT;
			usp->us_addr.address_length = 0;
			break;
		}

		/*
		 * Inform ARP code of new address
		 */
		uniarp_ifaddr((struct siginst *)usp);
		break;

	default:
		err = EOPNOTSUPP;
	}

	return (err);
}
