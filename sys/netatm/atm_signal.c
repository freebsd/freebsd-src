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
 * General ATM signalling management
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local variables
 */
static struct sigmgr	*atm_sigmgr_head = NULL;
static struct stack_defn	*atm_stack_head = NULL;


/*
 * Register a new Signalling Manager
 * 
 * Each Signalling Manager must register itself here upon completing
 * its internal initialization.  This applies to both linked and loaded
 * managers.
 *
 * Arguments:
 *	smp	pointer to Signalling Manager description
 *
 * Returns:
 *	0 	registration was successful 
 *	errno	registration failed - reason indicated
 *
 */
int
atm_sigmgr_register(smp)
	struct sigmgr	*smp;
{
	struct sigmgr	*smp2;
	int		s = splnet();

	/*
	 * See if we need to be initialized
	 */
	if (!atm_init)
		atm_initialize();

	/*
	 * Make sure there's only one instance of each protocol
	 */
	for (smp2 = atm_sigmgr_head; smp2 != NULL; smp2 = smp2->sm_next) {
		if (smp->sm_proto == smp2->sm_proto) {
			(void) splx(s);
			return (EEXIST);
		}
	}

	/*
	 * Looks okay, link it in
	 */
	LINK2TAIL(smp, struct sigmgr, atm_sigmgr_head, sm_next);

	(void) splx(s);
	return (0);
}


/*
 * De-register a Signalling Manager
 * 
 * Each Signalling Manager must de-register (is this really a word?)
 * itself before removing itself from the system.  This really only
 * applies to managers about to be modunload'ed.  It is the signal
 * manager's responsibility to ensure that all its protocol instances
 * have been successfully terminated before de-registering itself.
 *
 * Arguments:
 *	smp	pointer to Signalling Manager description
 *
 * Returns:
 *	0 	deregistration was successful 
 *	errno	deregistration failed - reason indicated
 *
 */
int
atm_sigmgr_deregister(smp)
	struct sigmgr	*smp;
{
	int		found, s = splnet();

	/*
	 * Unlink descriptor
	 */
	UNLINKF(smp, struct sigmgr, atm_sigmgr_head, sm_next, found);

	(void) splx(s);

	if (!found)
		return (ENOENT);

	return (0);
}


/*
 * Attach a Signalling Manager to an ATM physical interface
 * 
 * Each ATM physical interface must have a signalling manager attached to 
 * itself for the signalling protocol to be run across this interface.  The 
 * interface must be registered and completely initialized before the attach, 
 * since the signalling manager may initiate virtual circuit activity as part 
 * its response to this call.
 *
 * Called at splnet.
 *
 * Arguments:
 *	pip	pointer to atm physical interface control block
 * 	proto	requested signalling protocol
 *
 * Returns:
 *	0	attach successful
 *	errno	attach failed - reason indicated
 *
 */
int
atm_sigmgr_attach(pip, proto)
	struct atm_pif	*pip;
	u_char		proto;
{
	struct atm_pif	*tp;
	struct sigmgr	*smp;
	int	err;

	/*
	 * Make sure interface is registered
	 */
	for (tp = atm_interface_head; tp != NULL; tp = tp->pif_next) {
		if (tp == pip)
			break;
	}
	if (tp == NULL) {
		return (ENOENT);
	}

	/*
	 * Make sure no signalling manager is already attached
	 */
	if (pip->pif_sigmgr != NULL) {
		return (EEXIST);
	}

	/*
	 * Must have at least one network interface defined
	 */
	if (pip->pif_nif == NULL)
		return (ETOOMANYREFS);

	/*
	 * Find requested protocol
	 */
	for (smp = atm_sigmgr_head; smp != NULL; smp = smp->sm_next) {
		if (smp->sm_proto == proto)
			break;
	}
	if (smp == NULL) {
		return (EPROTONOSUPPORT);
	}

	/*
	 * Tell the signal manager about it
	 */
	err = (*smp->sm_attach)(smp, pip);

	/*
	 * Tell all registered convergence modules about this
	 */
	if (!err) {
		struct atm_nif	*nip;
		struct atm_ncm	*ncp;

		for (nip = pip->pif_nif; nip; nip = nip->nif_pnext) {
			for (ncp = atm_netconv_head; ncp; ncp = ncp->ncm_next) {
				if ((err = (*ncp->ncm_stat)
						(NCM_SIGATTACH, nip, 0)) != 0)
					break;
			}
			if (err)
				break;
		}

		if (err) {
			/*
			 * Someone's unhappy, so back all this out
			 */
			(void) atm_sigmgr_detach(pip);
		}
	}

	return (err);
}


/*
 * Detach an ATM physical interface from a Signalling Manager
 * 
 * The ATM interface must be detached from the signalling manager
 * before the interface can be de-registered.  
 *
 * Called at splnet.
 *
 * Arguments:
 *	pip	pointer to atm physical interface control block
 *
 * Returns:
 *	0	detach successful
 *	errno	detach failed - reason indicated
 *
 */
int
atm_sigmgr_detach(pip)
	struct atm_pif	*pip;
{
	struct atm_pif	*tp;
	struct atm_nif	*nip;
	struct atm_ncm	*ncp;
	int	err;


	/*
	 * Make sure interface is registered
	 */
	for (tp = atm_interface_head; tp != NULL; tp = tp->pif_next) {
		if (tp == pip)
			break;
	}
	if (tp == NULL) {
		return (ENOENT);
	}

	/*
	 * Make sure a signalling manager is attached
	 */
	if (pip->pif_sigmgr == NULL) {
		return (ENOENT);
	}

	/*
	 * Tell all registered convergence modules about this
	 */
	for (nip = pip->pif_nif; nip; nip = nip->nif_pnext) {
		for (ncp = atm_netconv_head; ncp; ncp = ncp->ncm_next) {
			(void) (*ncp->ncm_stat)(NCM_SIGDETACH, nip, 0);
		}
	}

	/*
	 * Tell the signal manager about it
	 *
	 * NOTE:
	 * The only reason this should ever fail is if things are really
	 * hosed up somewhere, in which case doing a bunch of NCM_SIGATTACH's
	 * here just doesn't seem to help much.
	 */
	err = (*pip->pif_sigmgr->sm_detach)(pip);

	return (err);
}


/*
 * Register an ATM Stack Service
 * 
 * Each ATM stack service provider must register its provided service(s) here.
 * Each service must be registered separately.  Service providers include 
 * both loaded and linked kernel modules.  Device driver services are NOT 
 * registered here - their service registry is performed implicitly through 
 * the device interface structure stack services list (pif_services).
 *
 * Arguments:
 *	sdp	pointer to stack service definition block
 *
 * Returns:
 *	0	registration successful
 *	errno	registration failed - reason indicated
 *
 */
int
atm_stack_register(sdp)
	struct stack_defn	*sdp;
{
	struct stack_defn	*tdp;
	int	s = splnet();

	/*
	 * See if we need to be initialized
	 */
	if (!atm_init)
		atm_initialize();

	/*
	 * Ensure no duplicates
	 */
	for (tdp = atm_stack_head; tdp != NULL; tdp = tdp->sd_next) {
		if (tdp->sd_sap == sdp->sd_sap)
			break;
	}
	if (tdp != NULL) {
		(void) splx(s);
		return (EEXIST);
	}

	/*
	 * Add stack to list
	 */
	LINK2TAIL(sdp, struct stack_defn, atm_stack_head, sd_next);

	(void) splx(s);
	return (0);
}


/*
 * De-register an ATM Stack Service
 * 
 * Each ATM stack service provider must de-register its registered service(s)
 * before terminating the service.  Specifically, loaded kernel modules
 * must de-register their services before unloading themselves.
 *
 * Arguments:
 *	sdp	pointer to stack service definition block
 *
 * Returns:
 *	0	de-registration successful 
 *	errno	de-registration failed - reason indicated
 *
 */
int
atm_stack_deregister(sdp)
	struct stack_defn	*sdp;
{
	int	found, s = splnet();

	/*
	 * Remove service from list
	 */
	UNLINKF(sdp, struct stack_defn, atm_stack_head, sd_next, found);
	(void) splx(s);

	if (!found)
		return (ENOENT);

	return (0);
}


/*
 * Create and Instantiate a Stack
 * 
 * For the requested stack list, locate the stack service definitions 
 * necessary to build the stack to implement the listed services.
 * The stack service definitions provided by the interface device-driver
 * are always preferred, since they are (hopefully) done with 
 * hardware assistance from the interface card.
 *
 * After the stack has been built, the selected services are called to 
 * notify them of the new stack instantiation.  Each service should then 
 * allocate all the resources it requires for this new stack instance.  
 * The service should then wait for subsequent protocol notification
 * via its stack command handlers.
 *
 * Must be called at splnet.
 *
 * Arguments:
 *	cvp	pointer to connection vcc block for the created stack
 *	tlp	pointer to stack list
 *	upf	top-of-stack CM upper command handler
 *
 * Returns:
 *	0	stack successfully created
 *	errno	failed - reason indicated
 *
 */
int
atm_create_stack(cvp, tlp, upf)
	Atm_connvc		*cvp;
	struct stack_list	*tlp;
	void			(*upf)__P((int, void *, int, int));
{
	struct stack_defn	*sdp, usd;
	struct stack_inst	svs;
	struct atm_pif		*pip = cvp->cvc_attr.nif->nif_pif;
	int		i, err;


	/*
	 * Initialize stack (element 0 is for owner's services)
	 */
	svs.si_srvc[1] = sdp = NULL;

	/*
	 * Locate service provider for each service in the
	 * stack list.  We prefer interface driver providers
	 * over kernel module providers.
	 */
	for (i = 0; i < STACK_CNT; i++) {
		Sap_t		sap;

		/* Stack list is 0-terminated */
		if ((sap = tlp->sl_sap[i]) == 0)
			break;

		/*
		 * Search interface's services
		 */
		for (sdp = pip->pif_services; sdp; sdp = sdp->sd_next)
			if (sdp->sd_sap == sap)
				break;
		if (sdp == NULL) {

			/*
			 * Search kernel services
			 */
			for (sdp = atm_stack_head; sdp; 
						 sdp = sdp->sd_next)
				if (sdp->sd_sap == sap)
					break;
		}
		if (sdp == NULL) {

			/*
			 * Requested service id not found
			 */
			return (ENOENT);
		}

		/*
		 * Save stack definition for this service
		 */
		svs.si_srvc[i+1] = sdp;

		/*
		 * Quit loop if this service is terminal, ie. if
		 * it takes care of the rest of the stack.
		 */
		if (sdp->sd_flag & SDF_TERM)
			break;
	}

	/*
	 * Ensure stack instance array is located and terminated
	 */
	if ((svs.si_srvc[1] == NULL) || !(sdp->sd_flag & SDF_TERM)) {
		return (ENOENT);
	}

	/*
	 * Setup owner service definition
	 */
	KM_ZERO((caddr_t)&usd, sizeof(struct stack_defn));
	usd.sd_upper = upf;
	usd.sd_toku = cvp;
	svs.si_srvc[0] = &usd;

	/*
	 * Instantiate the stack
	 */
	err = (*svs.si_srvc[1]->sd_inst)(&svs.si_srvc[0], cvp);
	if (err) {
		return (err);
	}

	/*
	 * Save top 'o stack info
	 */
	cvp->cvc_lower = svs.si_srvc[1]->sd_lower;
	cvp->cvc_tokl = svs.si_srvc[1]->sd_toku;

	return (0);
}

