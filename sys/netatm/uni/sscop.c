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
 * ATM Forum UNI Support
 * ---------------------
 *
 * Service Specific Connection Oriented Protocol (SSCOP)
 *
 */

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

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_pdu.h>
#include <netatm/uni/sscop_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Global variables
 */
int	sscop_vccnt = 0;

struct sscop		*sscop_head = NULL;

struct sscop_stat	sscop_stat = {0};

struct atm_time		sscop_timer = {0, 0};

struct sp_info	sscop_pool = {
	"sscop pool",			/* si_name */
	sizeof(struct sscop),		/* si_blksiz */
	5,				/* si_blkcnt */
	100				/* si_maxallow */
};


/*
 * Local functions
 */
static int	sscop_inst(struct stack_defn **, Atm_connvc *);


/*
 * Local variables
 */
static struct stack_defn	sscop_service = {
	NULL,
	SAP_SSCOP,
	0,
	sscop_inst,
	sscop_lower,
	sscop_upper,
	0
};

static struct t_atm_cause       sscop_cause = {
	T_ATM_ITU_CODING,
	T_ATM_LOC_USER,
	T_ATM_CAUSE_TEMPORARY_FAILURE,
	{0, 0, 0, 0}
};

static u_char	sscop_maa_log[MAA_ERROR_COUNT] = {
	1,	/* A */
	1,	/* B */
	1,	/* C */
	1,	/* D */
	1,	/* E */
	1,	/* F */
	1,	/* G */
	1,	/* H */
	1,	/* I */
	1,	/* J */
	1,	/* K */
	1,	/* L */
	1,	/* M */
	0,	/* N */
	0,	/* O */
	0,	/* P */
	1,	/* Q */
	1,	/* R */
	1,	/* S */
	1,	/* T */
	1,	/* U */
	0,	/* V */
	0,	/* W */
	0,	/* X */
	1	/* INVAL */
};


/*
 * Initialize SSCOP processing
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
sscop_start()
{
	int	err = 0;

	/*
	 * Register stack service
	 */
	if ((err = atm_stack_register(&sscop_service)) != 0)
		goto done;

	/*
	 * Start up timer
	 */
	atm_timeout(&sscop_timer, ATM_HZ/SSCOP_HZ, sscop_timeout);

done:
	return (err);
}


/*
 * Terminate SSCOP processing 
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
sscop_stop()
{
	int	err = 0;

	/*
	 * Any connections still exist??
	 */
	if (sscop_vccnt) {

		/*
		 * Yes, can't stop yet
		 */
		return (EBUSY);
	}

	/*
	 * Stop our timer
	 */
	(void) atm_untimeout(&sscop_timer);

	/*
	 * Deregister the stack service
	 */
	(void) atm_stack_deregister(&sscop_service);

	/*
	 * Free our storage pools
	 */
	atm_release_pool(&sscop_pool);

	return (err);
}


/*
 * SSCOP Stack Instantiation 
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
sscop_inst(ssp, cvp)
	struct stack_defn	**ssp;
	Atm_connvc		*cvp;
{
	struct stack_defn	*sdp_up = ssp[0],
				*sdp_me = ssp[1],
				*sdp_low = ssp[2];
	struct sscop	*sop;
	int		err;

	ATM_DEBUG2("sscop_inst: ssp=%p, cvp=%p\n", ssp, cvp);

	/*
	 * Validate lower SAP
	 */
	if ((sdp_low->sd_sap & SAP_CLASS_MASK) != SAP_CPCS)
		return (EINVAL);

	/*
	 * Allocate our control block
	 */
	sop = (struct sscop *)atm_allocate(&sscop_pool);
	if (sop == NULL)
		return (ENOMEM);

	sop->so_state = SOS_INST;
	sop->so_connvc = cvp;
	sop->so_toku = sdp_up->sd_toku;
	sop->so_upper = sdp_up->sd_upper;

	/*
	 * Store my token into service definition
	 */
	sdp_me->sd_toku = sop;

	/*
	 * Update and save input buffer headroom
	 */
	HEADIN(cvp, sizeof(struct pdu_hdr), 0);
	/* sop->so_headin = cvp->cvc_attr.headin; */

	/*
	 * Pass instantiation down the stack
	 */
	err = sdp_low->sd_inst(ssp + 1, cvp);
	if (err) {
		/*
		 * Lower layer instantiation failed, free our resources
		 */
		atm_free((caddr_t)sop);
		return (err);
	}

	/*
	 * Link in connection block
	 */
	LINK2TAIL(sop, struct sscop, sscop_head, so_next);
	sscop_vccnt++;
	sscop_stat.sos_connects++;

	/*
	 * Save and update output buffer headroom
	 */
	sop->so_headout = cvp->cvc_attr.headout;
	HEADOUT(cvp, sizeof(struct pdu_hdr), 0);

	/*
	 * Save lower layer's interface info
	 */
	sop->so_lower = sdp_low->sd_lower;
	sop->so_tokl = sdp_low->sd_toku;

	/*
	 * Initialize version (until INIT received)
	 */
	sop->so_vers = SSCOP_VERS_Q2110;

	return (0);
}


/*
 * Report Management Error
 * 
 * Called to report an error to the layer management entity.
 *
 * Arguments:
 *	sop	pointer to sscop control block
 *	code	error code
 *
 * Returns:
 *	none
 *
 */
void
sscop_maa_error(sop, code)
	struct sscop	*sop;
	int		code;
{
	int		i;

	/*
	 * Validate error code
	 */
	if ((code < MAA_ERROR_MIN) ||
	    (code > MAA_ERROR_MAX))
		code = MAA_ERROR_INVAL;
	i = code - MAA_ERROR_MIN;

	/*
	 * Bump statistics counters
	 */
	sscop_stat.sos_maa_error[i]++;

	/*
	 * Log error message
	 */
	if (sscop_maa_log[i] != 0) {
		struct vccb	*vcp = sop->so_connvc->cvc_vcc;
		struct atm_pif	*pip = vcp->vc_pif;

		log(LOG_ERR,
			"sscop_maa_error: intf=%s%d vpi=%d vci=%d code=%c state=%d\n",
			pip->pif_name, pip->pif_unit,
			vcp->vc_vpi, vcp->vc_vci, code, sop->so_state);
	}
}


/*
 * Abort an SSCOP connection
 * 
 * Called when an unrecoverable or "should never happen" error occurs.
 * We log a message, send an END PDU to our peer and request the signalling
 * manager to abort the connection.
 *
 * Arguments:
 *	sop	pointer to sscop control block
 *	msg	pointer to error message
 *
 * Returns:
 *	none
 *
 */
void
sscop_abort(sop, msg)
	struct sscop	*sop;
	char		*msg;
{
	Atm_connvc	*cvp = sop->so_connvc;

	/*
	 * Log and count error
	 */
	log(LOG_ERR, "%s", msg);
	sscop_stat.sos_aborts++;

	/*
	 * Send an END PDU as a courtesy to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Set termination state
	 */
	sop->so_state = SOS_TERM;

	/*
	 * Flush all of our queues
	 */
	sscop_xmit_drain(sop);
	sscop_rcvr_drain(sop);

	/*
	 * Tell Connection Manager to abort this connection
	 */
	(void) atm_cm_abort(cvp, &sscop_cause);
}

