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
 * ATM Forum UNI Support
 * ---------------------
 *
 * Signalling AAL SSCF at the UNI
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
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
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/uni/uni.h>
#include <netatm/uni/sscf_uni_var.h>

#include <vm/uma.h>

/*
 * Global variables
 */
int	sscf_uni_vccnt = 0;

/*
 * Local functions
 */
static int	sscf_uni_inst(struct stack_defn **, Atm_connvc *);

/*
 * Local variables
 */

static uma_zone_t	sscf_uni_zone;

static struct stack_defn	sscf_uni_service = {
	NULL,
	SAP_SSCF_UNI,
	0,
	sscf_uni_inst,
	sscf_uni_lower,
	sscf_uni_upper,
	0
};

static struct t_atm_cause	sscf_uni_cause = {
	T_ATM_ITU_CODING,
	T_ATM_LOC_USER,
	T_ATM_CAUSE_TEMPORARY_FAILURE,
	{0, 0, 0, 0}
};


/*
 * Initialize SSCF UNI processing
 * 
 * This will be called during module loading.  We will register our stack
 * service and wait for someone to talk to us.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	initialization was successful 
 *	errno	initialization failed - reason indicated
 *
 */
int
sscf_uni_start()
{
	int	err = 0;

	sscf_uni_zone = uma_zcreate("sscf uni", sizeof(struct univcc), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (sscf_uni_zone == NULL)
		panic("sscf_uni_start: uma_zcreate");

	/*
	 * Register stack service
	 */
	err = atm_stack_register(&sscf_uni_service);
	return (err);
}


/*
 * Terminate SSCF UNI processing 
 * 
 * This will be called just prior to unloading the module from memory.  All 
 * signalling instances should have been terminated by now, so we just free
 * up all of our resources.
 *
 * Called at splnet.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	termination was successful 
 *	errno	termination failed - reason indicated
 *
 */
int
sscf_uni_stop()
{
	/*
	 * Any connections still exist??
	 */
	if (sscf_uni_vccnt) {

		/*
		 * Yes, can't stop yet
		 */
		return (EBUSY);
	}

	/*
	 * Deregister the stack service
	 */
	(void)atm_stack_deregister(&sscf_uni_service);
	uma_zdestroy(sscf_uni_zone);
	return (0);
}


/*
 * SSCF_UNI Stack Instantiation 
 * 
 * Called at splnet.
 *
 * Arguments:
 *	ssp	pointer to array of stack definition pointers for connection
 *		ssp[0] points to upper layer's stack service definition
 *		ssp[1] points to this layer's stack service definition
 *		ssp[2] points to lower layer's stack service definition
 *	cvp	pointer to connection vcc for this stack
 *
 * Returns:
 *	0 	instantiation successful
 *	errno	instantiation failed - reason indicated
 *
 */
static int
sscf_uni_inst(ssp, cvp)
	struct stack_defn	**ssp;
	Atm_connvc		*cvp;
{
	struct stack_defn	*sdp_up = ssp[0],
				*sdp_me = ssp[1],
				*sdp_low = ssp[2];
	struct univcc	*uvp;
	int		err;

	ATM_DEBUG2("sscf_uni_inst: ssp=%p, cvp=%p\n", ssp, cvp);

	/*
	 * Validate lower SAP
	 */
	if (sdp_low->sd_sap != SAP_SSCOP)
		return (EINVAL);

	/*
	 * Allocate our control block
	 */
	uvp = uma_zalloc(sscf_uni_zone, M_WAITOK);
	if (uvp == NULL)
		return (ENOMEM);
	uvp->uv_ustate = UVU_INST;
	uvp->uv_lstate = UVL_INST;
	uvp->uv_connvc = cvp;
	uvp->uv_toku = sdp_up->sd_toku;
	uvp->uv_upper = sdp_up->sd_upper;
	sscf_uni_vccnt++;

	/*
	 * Store my token into service definition
	 */
	sdp_me->sd_toku = uvp;

	/*
	 * Update and save input buffer headroom
	 */
	HEADIN(cvp, 0, 0);
	/* uvp->uv_headin = cvp->cvc_attr.headin; */

	/*
	 * Pass instantiation down the stack
	 */
	err = sdp_low->sd_inst(ssp + 1, cvp);
	if (err) {
		/*
		 * Lower layer instantiation failed, free our resources
		 */
		uma_zfree(sscf_uni_zone, uvp);
		sscf_uni_vccnt--;
		return (err);
	}

	/*
	 * Save and update output buffer headroom
	 */
	/* uvp->uv_headout = cvp->cvc_attr.headout; */
	HEADOUT(cvp, 0, 0);

	/*
	 * Save lower layer's interface info
	 */
	uvp->uv_lower = sdp_low->sd_lower;
	uvp->uv_tokl = sdp_low->sd_toku;

	return (0);
}


/*
 * Abort an SSCF_UNI connection
 * 
 * Called when an unrecoverable or "should never happen" error occurs.
 * We just log a message and request the signalling manager to abort the
 * connection.
 *
 * Arguments:
 *	uvp	pointer to univcc control block
 *	msg	pointer to error message
 *
 * Returns:
 *	none
 *
 */
void
sscf_uni_abort(uvp, msg)
	struct univcc	*uvp;
	char		*msg;
{
	/*
	 * Log error message
	 */
	log(LOG_ERR, "%s", msg);

	/*
	 * Set termination states
	 */
	uvp->uv_ustate = UVU_TERM;
	uvp->uv_lstate = UVL_TERM;

	/*
	 * Tell Connection Manager to abort this connection
	 */
	(void) atm_cm_abort(uvp->uv_connvc, &sscf_uni_cause);
}


/*
 * Print an SSCF PDU
 * 
 * Arguments:
 *	uvp	pointer to univcc control block
 *	m	pointer to pdu buffer chain
 *	msg	pointer to message string
 *
 * Returns:
 *	none
 *
 */
void
sscf_uni_pdu_print(const struct univcc *uvp, const KBuffer *m, const char *msg)
{
	char		buf[128];
	struct vccb	*vcp;

	vcp = uvp->uv_connvc->cvc_vcc;
	snprintf(buf, sizeof(buf), "sscf_uni %s: vcc=(%d,%d)\n",
			msg, vcp->vc_vpi, vcp->vc_vci);
	atm_pdu_print(m, buf);
}
