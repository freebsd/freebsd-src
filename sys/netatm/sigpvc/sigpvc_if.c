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
 * PVC-only Signalling Manager
 * ---------------------------
 *
 * External interfaces to SigPVC manager.  Includes support for 
 * running as a loadable kernel module.
 *
 */

#ifndef ATM_SIGPVC_MODULE
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

#include <netatm/sigpvc/sigpvc_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Global variables
 */
struct sp_info	sigpvc_vcpool = {
	"sigpvc vcc pool",		/* si_name */
	sizeof(struct sigpvc_vccb),	/* si_blksiz */
	10,				/* si_blkcnt */
	50				/* si_maxallow */
};

/*
 * Local functions
 */
static int	sigpvc_start(void);
static int	sigpvc_stop(void);
static int	sigpvc_attach(struct sigmgr *, struct atm_pif *);
static int	sigpvc_detach(struct atm_pif *);
static int	sigpvc_setup(Atm_connvc *, int *);
static int	sigpvc_release(struct vccb *, int *);
static int	sigpvc_free(struct vccb *);
static int	sigpvc_ioctl(int, caddr_t, caddr_t);

/*
 * Local variables
 */
static int	sigpvc_registered = 0;
static struct sigmgr	sigpvc_mgr = {
	NULL,
	ATM_SIG_PVC,
	NULL,
	sigpvc_attach,
	sigpvc_detach,
	sigpvc_setup,
	NULL,
	NULL,
	sigpvc_release,
	sigpvc_free,
	sigpvc_ioctl
};

static struct attr_cause	sigpvc_cause = {
	T_ATM_PRESENT,
	{
		T_ATM_ITU_CODING,
		T_ATM_LOC_USER,
		T_ATM_CAUSE_UNSPECIFIED_NORMAL,
		{0, 0, 0, 0}
	}
};


/*
 * Initialize sigpvc processing
 * 
 * This will be called during module loading.  We'll just register
 * the sigpvc protocol descriptor and wait for a SigPVC ATM interface 
 * to come online.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	startup was successful 
 *	errno	startup failed - reason indicated
 *
 */
static int
sigpvc_start()
{
	int	err = 0;

	/*
	 * Verify software version
	 */
	if (atm_version != ATM_VERSION) {
		log(LOG_ERR, "version mismatch: sigpvc=%d.%d kernel=%d.%d\n",
			ATM_VERS_MAJ(ATM_VERSION), ATM_VERS_MIN(ATM_VERSION),
			ATM_VERS_MAJ(atm_version), ATM_VERS_MIN(atm_version));
		return (EINVAL);
	}

	/*
	 * Register ourselves with system
	 */
	err = atm_sigmgr_register(&sigpvc_mgr);
	if (err == 0)
		sigpvc_registered = 1;

	return (err);
}


/*
 * Halt sigpvc processing 
 * 
 * This should be called just prior to unloading the module from
 * memory.  All sigpvc interfaces must be deregistered before the
 * protocol can be shutdown.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	shutdown was successful 
 *	errno	shutdown failed - reason indicated
 *
 */
static int
sigpvc_stop()
{
	int	err = 0;
	int	s = splnet();

	/*
	 * Is protocol even setup?
	 */
	if (sigpvc_registered) {
		
		/*
		 * Any protocol instances still registered??
		 */
		if (sigpvc_mgr.sm_prinst) {

			/* Yes, can't stop now */
			err = EBUSY;
			goto done;
		}

		/*
		 * De-register from system
		 */
		err = atm_sigmgr_deregister(&sigpvc_mgr);
		sigpvc_registered = 0;

		/*
		 * Free up our vccb storage pool
		 */
		atm_release_pool(&sigpvc_vcpool);
	} else
		err = ENXIO;

done:
	(void) splx(s);
	return (err);
}


/*
 * Attach a SigPVC-controlled interface
 * 
 * Each ATM physical interface must be attached with the signalling manager for
 * the interface's signalling protocol (via the atm_sigmgr_attach function).  
 * This function will handle the attachment for SigPVC-controlled interfaces.
 * A new sigpvc protocol instance will be created and then we'll just sit
 * around waiting for connection requests.
 *
 * Function must be called at splnet.
 *
 * Arguments:
 *	smp	pointer to sigpvc signalling manager control block
 *	pip	pointer to atm physical interface control block
 *
 * Returns:
 *	0 	attach successful 
 *	errno	attach failed - reason indicated
 *
 */
static int
sigpvc_attach(smp, pip)
	struct sigmgr	*smp;
	struct atm_pif	*pip;
{
	int	err = 0;
	struct sigpvc	*pvp = NULL;

	/*
	 * Allocate sigpvc protocol instance control block
	 */
	pvp = (struct sigpvc *)
		KM_ALLOC(sizeof(struct sigpvc), M_DEVBUF, M_NOWAIT);
	if (pvp == NULL) {
		err = ENOMEM;
		goto done;
	}
	KM_ZERO(pvp, sizeof(struct sigpvc));

	/*
	 * Link instance into manager's chain
	 */
	LINK2TAIL((struct siginst *)pvp, struct siginst, 
		smp->sm_prinst, si_next);

	/*
	 * Finally, set state and link in interface
	 */
	pvp->pv_pif = pip;
	pvp->pv_state = SIGPVC_ACTIVE;
	pip->pif_sigmgr = smp;
	pip->pif_siginst = (struct siginst *)pvp;

done:
	/*
	 * Reset our work if attach fails
	 */
	if (err) {
		pip->pif_sigmgr = NULL;
		pip->pif_siginst = NULL;
		if (pvp) {
			UNLINK((struct siginst *)pvp, struct siginst, 
				smp->sm_prinst, si_next);
			KM_FREE(pvp, sizeof(struct sigpvc), M_DEVBUF);
		}
	}

	return (err);
}


/*
 * Detach a SigPVC-controlled interface
 * 
 * Each ATM physical interface may be detached from its signalling manager 
 * (via the atm_sigmgr_detach function).  This function will handle the 
 * detachment for all SigPVC-controlled interfaces.  All circuits will be 
 * immediately terminated.
 *
 * Function must be called at splnet.
 *
 * Arguments:
 *	pip	pointer to atm physical interface control block
 *
 * Returns:
 *	0 	detach successful 
 *	errno	detach failed - reason indicated
 *
 */
static int
sigpvc_detach(pip)
	struct atm_pif	*pip;
{
	struct sigpvc	*pvp;
	struct vccb	*vcp, *vnext;

	/*
	 * Get SigPVC protocol instance
	 */
	pvp = (struct sigpvc *)pip->pif_siginst;

	/*
	 * Terminate all of our VCCs
	 */
	for (vcp = Q_HEAD(pvp->pv_vccq, struct vccb); vcp; vcp = vnext){
		u_char	oustate;

		vnext = Q_NEXT(vcp, struct vccb, vc_sigelem);

		/*
		 * Close VCC and notify owner
		 */
		oustate = vcp->vc_ustate;
		sigpvc_close_vcc(vcp);
		if (oustate == VCCU_OPEN) {
			vcp->vc_connvc->cvc_attr.cause = sigpvc_cause;
			atm_cm_cleared(vcp->vc_connvc);
		}
	}

	/*
	 * If there are no vcc's queued, then get rid of the protocol 
	 * instance.  
	 */
	if (Q_HEAD(pvp->pv_vccq, struct vccb) == NULL) {
		struct sigmgr	*smp = pip->pif_sigmgr;

		pip->pif_sigmgr = NULL;
		pip->pif_siginst = NULL;
		UNLINK((struct siginst *)pvp, struct siginst, smp->sm_prinst, 
				si_next);
		KM_FREE(pvp, sizeof(struct sigpvc), M_DEVBUF);
	} else {

		/*
		 * Otherwise, set new state indicating detach in progress.
		 * The protocol instance will be freed during sigpvc_free 
		 * processing for the last queued vcc.
		 */
		pvp->pv_state = SIGPVC_DETACH;
	}

	return (0);
}


/*
 * Open a SigPVC ATM Connection
 * 
 * All service user requests to open a VC connection (via atm_open_connection)
 * over an ATM interface attached to the SigPVC signalling manager are handled 
 * here.  Only PVC requests are allowed.
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	cvp	pointer to CM's connection VCC
 *	errp	location to store an error code if CALL_FAILED is returned
 *
 * Returns:
 *	CALL_PROCEEDING	- connection establishment is in progress
 *	CALL_FAILED	- connection establishment failed
 *	CALL_CONNECTED	- connection has been successfully established
 *
 */
static int
sigpvc_setup(cvp, errp)
	Atm_connvc	*cvp;
	int		*errp;
{
	struct sigpvc	*pvp =
		(struct sigpvc *)cvp->cvc_attr.nif->nif_pif->pif_siginst;
	int	ret;

	/*
	 * See what signalling has to say
	 */
	switch (pvp->pv_state) {

	case SIGPVC_ACTIVE:
		break;

	default:
		*errp = ENXIO;
		ret = CALL_FAILED;
		goto done;
	}
	
	/*
	 * Open requested type of connection
	 */
	switch (cvp->cvc_attr.called.addr.address_format) {

	case T_ATM_PVC_ADDR:
		/*
		 * Create a PVC
		 */
		ret = sigpvc_create_pvc(pvp, cvp, errp);
		break;

	default:
		*errp = EPROTONOSUPPORT;
		ret = CALL_FAILED;
	}

done:
	return (ret);
}


/*
 * Close a SigPVC ATM Connection
 * 
 * All service user requests to terminate a previously open VC connection 
 * (via the atm_close_connection function), which is running over an interface 
 * attached to the SigPVC signalling manager, are handled here.
 *
 * Function will be called at splnet.
 * 
 * Arguments:
 *	vcp	pointer to connection's VC control block
 *	errp	location to store an error code if CALL_FAILED is returned
 *
 * Returns:
 *	CALL_PROCEEDING	- connection termination is in progress
 *	CALL_FAILED	- connection termination failed
 *	CALL_CLEARED	- connection has been successfully terminated
 *
 */
static int
sigpvc_release(vcp, errp)
	struct vccb	*vcp;
	int		*errp;
{

	/*
	 * Make sure VCC is open
	 */
	if ((vcp->vc_sstate == VCCS_NULL) || (vcp->vc_sstate == VCCS_FREE) ||
	    (vcp->vc_ustate == VCCU_NULL) || (vcp->vc_ustate == VCCU_CLOSED)) {
		*errp = EALREADY;
		return (CALL_FAILED);
	}

	/*
	 * Not much else to do except close the vccb
	 */
	sigpvc_close_vcc(vcp);

	return (CALL_CLEARED);
}


/*
 * Free SigPVC ATM Connection Resources
 * 
 * All service user requests to free the resources of a closed VCC connection
 * (via the atm_free_connection function), which is running over an interface 
 * attached to the SigPVC signalling manager, are handled here.
 *
 * Function will be called at splnet.
 * 
 * Arguments:
 *	vcp	pointer to connection's VCC control block
 *
 * Returns:
 *	0 	connection free was successful 
 *	errno	connection free failed - reason indicated
 *
 */
static int
sigpvc_free(vcp)
	struct vccb	*vcp;
{
	struct atm_pif	*pip = vcp->vc_pif;
	struct sigpvc	*pvp = (struct sigpvc *)pip->pif_siginst;

	/*
	 * Make sure VCC has been closed
	 */
	if ((vcp->vc_ustate != VCCU_CLOSED) || (vcp->vc_sstate != VCCS_FREE))
		return (EEXIST);

	/*
	 * Remove vccb from protocol queue
	 */
	DEQUEUE(vcp, struct vccb, vc_sigelem, pvp->pv_vccq);

	/*
	 * Free vccb storage
	 */
	vcp->vc_ustate = VCCU_NULL;
	vcp->vc_sstate = VCCS_NULL;
	atm_free((caddr_t)vcp);

	/*
	 * If we're detaching and this was the last vcc queued,
	 * get rid of the protocol instance
	 */
	if ((pvp->pv_state == SIGPVC_DETACH) && 
	    (Q_HEAD(pvp->pv_vccq, struct vccb) == NULL)) {
		struct sigmgr	*smp = pip->pif_sigmgr;

		pip->pif_sigmgr = NULL;
		pip->pif_siginst = NULL;
		UNLINK((struct siginst *)pvp, struct siginst, smp->sm_prinst, 
				si_next);
		KM_FREE(pvp, sizeof(struct sigpvc), M_DEVBUF);
	}

	return (0);
}


/*
 * Process Signalling Manager PF_ATM ioctls
 * 
 * Function will be called at splnet.
 *
 * Arguments:
 *	code	PF_ATM sub-operation code
 *	data	pointer to code specific parameter data area
 *	arg1	pointer to code specific argument
 *
 * Returns:
 *	0 	request procesed
 *	errno	error processing request - reason indicated
 *
 */
static int
sigpvc_ioctl(code, data, arg1)
	int		code;
	caddr_t		data;
	caddr_t		arg1;
{
	struct atmdelreq	*adp;
	struct atminfreq	*aip;
	struct air_vcc_rsp	avr;
	struct sigpvc	*pvp;
	struct vccb	*vcp;
	Atm_connection	*cop;
	caddr_t		cp;
	u_int	vpi, vci;
	int	i, space, err = 0;


	switch (code) {

	case AIOCS_DEL_PVC:
		/*
		 * Delete a PVC
		 */
		adp = (struct atmdelreq *)data;
		pvp = (struct sigpvc *)arg1;

		/*
		 * Find requested VCC
		 */
		vpi = adp->adr_pvc_vpi;
		vci = adp->adr_pvc_vci;
		for (vcp = Q_HEAD(pvp->pv_vccq, struct vccb); vcp; 
				vcp = Q_NEXT(vcp, struct vccb, vc_sigelem)) {
			if ((vcp->vc_vpi == vpi) && (vcp->vc_vci == vci))
				break;
		}
		if (vcp == NULL)
			return (ENOENT);

		/*
		 * Schedule VCC termination
		 */
		err = atm_cm_abort(vcp->vc_connvc, &sigpvc_cause.v);
		break;

	case AIOCS_DEL_SVC:
		/*
		 * Delete a SVC
		 */
		err = ENOENT;
		break;

	case AIOCS_INF_VCC:
		/*
		 * Get VCC information
		 */
		aip = (struct atminfreq *)data;
		pvp = (struct sigpvc *)arg1;

		cp = aip->air_buf_addr;
		space = aip->air_buf_len;

		/*
		 * Get info for all VCCs on interface
		 */
		for (vcp = Q_HEAD(pvp->pv_vccq, struct vccb); vcp; 
				vcp = Q_NEXT(vcp, struct vccb, vc_sigelem)) {
			/*
			 * Make sure there's room in user buffer
			 */
			if (space < sizeof(avr)) {
				err = ENOSPC;
				break;
			}

			/*
			 * Fill in info to be returned
			 */
			(void) snprintf(avr.avp_intf, sizeof(avr.avp_intf),
				"%s%d",
				pvp->pv_pif->pif_name, pvp->pv_pif->pif_unit);
			avr.avp_vpi = vcp->vc_vpi;
			avr.avp_vci = vcp->vc_vci;
			avr.avp_type = vcp->vc_type;
			avr.avp_sig_proto = ATM_SIG_PVC;
			avr.avp_aal = vcp->vc_connvc->cvc_attr.aal.type;
			cop = vcp->vc_connvc->cvc_conn;
			if  (cop)
				avr.avp_encaps = cop->co_mpx;
			else
				avr.avp_encaps = 0;
			KM_ZERO(avr.avp_owners, sizeof(avr.avp_owners));
			for (i = 0; cop && i < sizeof(avr.avp_owners);
					cop = cop->co_next,
					i += T_ATM_APP_NAME_LEN+1) {
				strncpy(&avr.avp_owners[i],
					cop->co_endpt->ep_getname(cop->co_toku),
					T_ATM_APP_NAME_LEN);
			}
			avr.avp_state = vcp->vc_sstate;
			avr.avp_daddr.address_format = T_ATM_ABSENT;
			avr.avp_dsubaddr.address_format = T_ATM_ABSENT;
			avr.avp_ipdus = vcp->vc_ipdus;
			avr.avp_opdus = vcp->vc_opdus;
			avr.avp_ibytes = vcp->vc_ibytes;
			avr.avp_obytes = vcp->vc_obytes;
			avr.avp_ierrors = vcp->vc_ierrors;
			avr.avp_oerrors = vcp->vc_oerrors;
			avr.avp_tstamp = vcp->vc_tstamp;

			/*
			 * Copy data to user buffer and update buffer info
			 */
			if ((err = copyout((caddr_t)&avr, cp, sizeof(avr))) != 0)
				break;
			cp += sizeof(avr);
			space -= sizeof(avr);
		}

		/*
		 * Update buffer pointer/count
		 */
		aip->air_buf_addr = cp;
		aip->air_buf_len = space;
		break;

	case AIOCS_INF_ARP:
	case AIOCS_INF_ASV:
		/*
		 * Get ARP table/server information
		 */
		/* We don't maintain any ARP information */
		break;

	default:
		err = EOPNOTSUPP;
	}

	return (err);
}


#ifdef ATM_SIGPVC_MODULE
/*
 *******************************************************************
 *
 * Loadable Module Support
 *
 *******************************************************************
 */
static int	sigpvc_doload(void);
static int	sigpvc_dounload(void);

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
sigpvc_doload()
{
	int	err = 0;

	/*
	 * Start us up
	 */
	err = sigpvc_start();
	if (err)
		/* Problems, clean up */
		(void)sigpvc_stop();

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
sigpvc_dounload()
{
	int	err = 0;

	/*
	 * OK, try to clean up our mess
	 */
	err = sigpvc_stop();

	return (err);
}




#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

/*
 * Loadable miscellaneous module description
 */
MOD_MISC(sigpvc);


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
sigpvc_load(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(sigpvc_doload());
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
sigpvc_unload(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(sigpvc_dounload());
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
sigpvc_mod(lkmtp, cmd, ver)
	struct lkm_table	*lkmtp;
	int		cmd;
	int		ver;
{
	MOD_DISPATCH(sigpvc, lkmtp, cmd, ver,
		sigpvc_load, sigpvc_unload, lkm_nullcmd);
}

#else	/* !ATM_SIGPVC_MODULE */

/*
 *******************************************************************
 *
 * Kernel Compiled Module Support
 *
 *******************************************************************
 */
static void	sigpvc_doload(void *);

SYSINIT(atmsigpvc, SI_SUB_PROTO_END, SI_ORDER_ANY, sigpvc_doload, NULL)

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
sigpvc_doload(void *arg)
{
	int	err = 0;

	/*
	 * Start us up
	 */
	err = sigpvc_start();
	if (err) {
		/* Problems, clean up */
		(void)sigpvc_stop();

		log(LOG_ERR, "ATM SIGPVC unable to initialize (%d)!!\n", err);
	}
	return;
}
#endif	/* ATM_SIGPVC_MODULE */

