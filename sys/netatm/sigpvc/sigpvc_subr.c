/*
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
 * PVC-only Signalling Manager
 * ---------------------------
 *
 * Subroutines
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_sigmgr.h>

#include <netatm/sigpvc/sigpvc_var.h>

#include <vm/uma.h>

extern uma_zone_t	sigpvc_vc_zone;

/*
 * Create a SigPVC Permanent Virtual Channel
 * 
 * This function will construct a vccb for a "sigpvc-controlled" PVC
 * and create the service stack requested by the user.
 *
 * Must be called at splnet.
 *
 * Arguments:
 *	pvp	pointer to sigpvc protocol instance
 *	cvp	pointer to CM's connection VCC
 *	errp	location to store an error code if CALL_FAILED is returned
 *
 * Returns:
 *	CALL_FAILED	- pvc creation failed
 *	CALL_CONNECTED	- pvc has been successfully created
 *
 */
int
sigpvc_create_pvc(pvp, cvp, errp)
	struct sigpvc	*pvp;
	Atm_connvc	*cvp;
	int		*errp;
{
	Atm_addr_pvc	*pp;
	struct vccb	*vcp;
	u_int	vpi, vci;

	pp = (Atm_addr_pvc *)cvp->cvc_attr.called.addr.address;
	vpi = ATM_PVC_GET_VPI(pp);
	vci = ATM_PVC_GET_VCI(pp);

	/*
	 * Verify requested VPI,VCI
	 */
	if ((vpi > pvp->pv_pif->pif_maxvpi) ||
	    (vci == 0) || (vci > pvp->pv_pif->pif_maxvci)) {
		*errp = ERANGE;
		return (CALL_FAILED);
	}

	for (vcp = Q_HEAD(pvp->pv_vccq, struct vccb); vcp;
			vcp = Q_NEXT(vcp, struct vccb, vc_sigelem)) {

		if ((vcp->vc_vpi == vpi) &&
		    (vcp->vc_vci == vci)) {
			*errp = EADDRINUSE;
			return (CALL_FAILED);
		}
	}

	/*
	 * Verify network interface
	 */
	if (cvp->cvc_attr.nif) {
		if (cvp->cvc_attr.nif->nif_pif != pvp->pv_pif) {
			*errp = EINVAL;
			return (CALL_FAILED);
		}
	}

	/*
	 * Allocate control block for PVC
	 */
	vcp = uma_zalloc(sigpvc_vc_zone, M_WAITOK | M_ZERO);
	if (vcp == NULL) {
		*errp = ENOMEM;
		return (CALL_FAILED);
	}

	/*
	 * Fill in VCCB
	 */
	vcp->vc_type = VCC_PVC | VCC_IN | VCC_OUT;
	vcp->vc_proto = ATM_SIG_PVC;
	vcp->vc_sstate = VCCS_ACTIVE;
	vcp->vc_ustate = VCCU_OPEN;
	vcp->vc_pif = pvp->pv_pif;
	vcp->vc_nif = cvp->cvc_attr.nif;
	vcp->vc_vpi = vpi;
	vcp->vc_vci = vci;
	vcp->vc_connvc = cvp;

	/*
	 * Put VCCB on sigpvc queue
	 */
	ENQUEUE(vcp, struct vccb, vc_sigelem, pvp->pv_vccq);

	/*
	 * Pass back VCCB to connection manager
	 */
	cvp->cvc_vcc = vcp;

	/*
	 * PVC is ready to go!
	 */
	return (CALL_CONNECTED);
}

/*
 * Close a SigPVC VCC 
 * 
 * Clean up vccb, note that it's closing and wait for its freeing.
 *
 * Arguments:
 *	vcp	pointer to connection's VCC control block
 *
 * Returns:
 *	none
 *
 */
void
sigpvc_close_vcc(vcp)
	struct vccb	*vcp;
{

	/*
	 * Sanity check (actually design-flaw check)
	 */
	if (vcp->vc_connvc->cvc_upcnt || vcp->vc_connvc->cvc_downcnt)
		panic("sigpvc_close_vcc: stack call");

	/*
	 * Set state variables
	 */
	vcp->vc_ustate = VCCU_CLOSED;
	vcp->vc_sstate = VCCS_FREE;

	/*
	 * Wait for user to free resources
	 */
}

