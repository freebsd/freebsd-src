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
 * SSCF UNI - SSCOP SAP interface processing
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
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
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/uni/uni.h>
#include <netatm/uni/sscop.h>
#include <netatm/uni/sscf_uni_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * SSCF_UNI Upper Stack Command Handler
 * 
 * This function will receive all of the stack commands issued from the 
 * layer below SSCF UNI (ie. SSCOP).
 *
 * Arguments:
 *	cmd	stack command code
 *	tok	session token
 *	arg1	command specific argument
 *	arg2	command specific argument
 *
 * Returns:
 *	none
 *
 */
void
sscf_uni_upper(cmd, tok, arg1, arg2)
	int	cmd;
	void	*tok;
	intptr_t	arg1;
	intptr_t	arg2;
{
	struct univcc	*uvp = (struct univcc *)tok;
	Atm_connvc	*cvp = uvp->uv_connvc;
	int		err;

	ATM_DEBUG5("sscf_uni_upper: cmd=0x%x, uvp=%p, lstate=%d, arg1=%p, arg2=%p\n",
		cmd, uvp, uvp->uv_lstate, (void *)arg1, (void *)arg2);

	switch (cmd) {

	case SSCOP_ESTABLISH_IND:
		/*
		 * We don't support SSCOP User-to-User data, so just
		 * get rid of any supplied to us
		 */
		if (arg1 != SSCOP_UU_NULL)
			KB_FREEALL((KBuffer *)arg1);

		/*
		 * Validation based on sscop state
		 */
		switch (uvp->uv_lstate) {

		case UVL_READY:
			if (uvp->uv_vers != UNI_VERS_3_0) {
				goto seqerr;
			}
			goto doestind;

		case UVL_IDLE:
			/*
			 * Incoming connection establishment request
			 */

			/*
			 * If user doesn't want any more incoming sessions
			 * accepted, then refuse request
			 */
			if (uvp->uv_flags & UVF_NOESTIND) {
				STACK_CALL(SSCOP_RELEASE_REQ, uvp->uv_lower,
					uvp->uv_tokl, cvp,
					SSCOP_UU_NULL, 0, err);
				if (err) {
					sscf_uni_abort(uvp,
						"sscf_uni: stack memory\n");
					return;
				}
				break;
			}

doestind:
			/*
			 * Tell sscop we've accepted the new connection
			 */
			uvp->uv_lstate = UVL_READY;
			STACK_CALL(SSCOP_ESTABLISH_RSP, uvp->uv_lower, 
				uvp->uv_tokl, cvp, 
				SSCOP_UU_NULL, SSCOP_BR_YES, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}

			/*
			 * Now notify the user of the new connection
			 */
			uvp->uv_ustate = UVU_ACTIVE;
			STACK_CALL(SSCF_UNI_ESTABLISH_IND, uvp->uv_upper, 
				uvp->uv_toku, cvp, 
				SSCOP_UU_NULL, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVL_TERM:
			/*
			 * Ignoring everything
			 */
			break;

		case UVL_INST:
		case UVL_OUTCONN:
		case UVL_INCONN:
		case UVL_OUTDISC:
		case UVL_OUTRESYN:
		case UVL_INRESYN:
		case UVL_RECOVERY:
		default:
seqerr:
			log(LOG_ERR, "sscf_uni_upper: cmd=0x%x, lstate=%d\n",
				cmd, uvp->uv_lstate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCOP_ESTABLISH_CNF:
		/*
		 * We don't support SSCOP User-to-User data, so just
		 * get rid of any supplied to us
		 */
		if (arg1 != SSCOP_UU_NULL)
			KB_FREEALL((KBuffer *)arg1);

		/*
		 * Validation based on sscop state
		 */
		switch (uvp->uv_lstate) {

		case UVL_OUTCONN:
			/*
			 * Outgoing connection establishment completed
			 */

			/*
			 * Tell the user that the connection is established
			 */
			uvp->uv_ustate = UVU_ACTIVE;
			uvp->uv_lstate = UVL_READY;
			STACK_CALL(SSCF_UNI_ESTABLISH_CNF, uvp->uv_upper, 
				uvp->uv_toku, cvp, 
				SSCOP_UU_NULL, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVL_TERM:
			/*
			 * Ignoring everything
			 */
			break;

		case UVL_INST:
		case UVL_IDLE:
		case UVL_INCONN:
		case UVL_OUTDISC:
		case UVL_OUTRESYN:
		case UVL_INRESYN:
		case UVL_RECOVERY:
		case UVL_READY:
		default:
			log(LOG_ERR, "sscf_uni_upper: cmd=0x%x, lstate=%d\n",
				cmd, uvp->uv_lstate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCOP_RELEASE_IND:
		/*
		 * We don't support SSCOP User-to-User data, so just
		 * get rid of any supplied to us
		 */
		if (arg1 != SSCOP_UU_NULL)
			KB_FREEALL((KBuffer *)arg1);

		/*
		 * Validation based on sscop state
		 */
		switch (uvp->uv_lstate) {

		case UVL_OUTCONN:
		case UVL_OUTRESYN:
		case UVL_READY:
			/*
			 * Peer requesting connection termination
			 */

			/*
			 * Notify the user that the connection 
			 * has been terminated
			 */
			uvp->uv_ustate = UVU_RELEASED;
			uvp->uv_lstate = UVL_IDLE;
			STACK_CALL(SSCF_UNI_RELEASE_IND, uvp->uv_upper, 
				uvp->uv_toku, cvp, 
				SSCOP_UU_NULL, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVL_TERM:
			/*
			 * Ignoring everything
			 */
			break;

		case UVL_INST:
		case UVL_IDLE:
		case UVL_INCONN:
		case UVL_OUTDISC:
		case UVL_INRESYN:
		case UVL_RECOVERY:
		default:
			log(LOG_ERR, "sscf_uni_upper: cmd=0x%x, lstate=%d\n",
				cmd, uvp->uv_lstate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCOP_RELEASE_CNF:
		/*
		 * Validation based on sscop state
		 */
		switch (uvp->uv_lstate) {

		case UVL_OUTDISC:
			/*
			 * Peer acknowledging connection termination
			 */

			/*
			 * Notify the user that the connection 
			 * termination is completed
			 */
			uvp->uv_ustate = UVU_RELEASED;
			uvp->uv_lstate = UVL_IDLE;
			STACK_CALL(SSCF_UNI_RELEASE_CNF, uvp->uv_upper, 
				uvp->uv_toku, cvp, 
				SSCOP_UU_NULL, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVL_TERM:
			/*
			 * Ignoring everything
			 */
			break;

		case UVL_INST:
		case UVL_IDLE:
		case UVL_OUTCONN:
		case UVL_INCONN:
		case UVL_OUTRESYN:
		case UVL_INRESYN:
		case UVL_RECOVERY:
		case UVL_READY:
		default:
			log(LOG_ERR, "sscf_uni_upper: cmd=0x%x, lstate=%d\n",
				cmd, uvp->uv_lstate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCOP_DATA_IND:
#ifdef notdef
		sscf_uni_pdu_print(uvp, (KBuffer *)arg1, "DATA_IND");
#endif

		/*
		 * Validation based on sscop state
		 */
		switch (uvp->uv_lstate) {

		case UVL_READY:
			/*
			 * Incoming assured data from peer
			 */

			/*
			 * Pass the data up to the user
			 */
			STACK_CALL(SSCF_UNI_DATA_IND, uvp->uv_upper, 
				uvp->uv_toku, cvp, 
				arg1, 0, err);
			if (err) {
				KB_FREEALL((KBuffer *)arg1);
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVL_TERM:
			/*
			 * Ignoring everything
			 */
			KB_FREEALL((KBuffer *)arg1);
			break;

		case UVL_INST:
		case UVL_IDLE:
		case UVL_OUTCONN:
		case UVL_INCONN:
		case UVL_OUTDISC:
		case UVL_OUTRESYN:
		case UVL_INRESYN:
		case UVL_RECOVERY:
		default:
			KB_FREEALL((KBuffer *)arg1);
			log(LOG_ERR, "sscf_uni_upper: cmd=0x%x, lstate=%d\n",
				cmd, uvp->uv_lstate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCOP_RESYNC_IND:
		/*
		 * We don't support SSCOP User-to-User data, so just
		 * get rid of any supplied to us
		 */
		if (arg1 != SSCOP_UU_NULL)
			KB_FREEALL((KBuffer *)arg1);

		/*
		 * Validation based on sscop state
		 */
		switch (uvp->uv_lstate) {

		case UVL_READY:
			/*
			 * Incoming connection resynchronization request
			 */

			/*
			 * Send resynch acknowledgement to sscop
			 */
			STACK_CALL(SSCOP_RESYNC_RSP, uvp->uv_lower, 
				uvp->uv_tokl, cvp, 
				0, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}

			if (uvp->uv_vers != UNI_VERS_3_0) {

				/*
				 * Notify the user that the connection 
				 * has been resynced
				 */
				STACK_CALL(SSCF_UNI_ESTABLISH_IND, 
					uvp->uv_upper, uvp->uv_toku, cvp, 
					SSCOP_UU_NULL, 0, err);
				if (err) {
					sscf_uni_abort(uvp, 
						"sscf_uni: stack memory\n");
					return;
				}
			}
			break;

		case UVL_TERM:
			/*
			 * Ignoring everything
			 */
			break;

		case UVL_INST:
		case UVL_IDLE:
		case UVL_OUTCONN:
		case UVL_INCONN:
		case UVL_OUTDISC:
		case UVL_OUTRESYN:
		case UVL_INRESYN:
		case UVL_RECOVERY:
		default:
			log(LOG_ERR, "sscf_uni_upper: cmd=0x%x, lstate=%d\n",
				cmd, uvp->uv_lstate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCOP_RESYNC_CNF:
		/*
		 * Not supported in version 3.0
		 */
		if (uvp->uv_vers == UNI_VERS_3_0) {
			sscf_uni_abort(uvp, 
				"sscf_uni: SSCOP_RESYNC_CNF in 3.0\n");
			return;
		}

		/*
		 * Validation based on sscop state
		 */
		switch (uvp->uv_lstate) {

		case UVL_OUTRESYN:
			/*
			 * Peer acknowledging connection resynchronization
			 */

			/*
			 * Now notify the user that the connection 
			 * has been resynced
			 */
			uvp->uv_ustate = UVU_ACTIVE;
			uvp->uv_lstate = UVL_READY;
			STACK_CALL(SSCF_UNI_ESTABLISH_CNF, uvp->uv_upper, 
				uvp->uv_toku, cvp, 
				SSCOP_UU_NULL, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVL_TERM:
			/*
			 * Ignoring everything
			 */
			break;

		case UVL_INST:
		case UVL_IDLE:
		case UVL_OUTCONN:
		case UVL_INCONN:
		case UVL_OUTDISC:
		case UVL_INRESYN:
		case UVL_RECOVERY:
		case UVL_READY:
		default:
			log(LOG_ERR, "sscf_uni_upper: cmd=0x%x, lstate=%d\n",
				cmd, uvp->uv_lstate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCOP_RECOVER_IND:
		/*
		 * Not supported in version 3.0
		 */
		if (uvp->uv_vers == UNI_VERS_3_0) {
			sscf_uni_abort(uvp, 
				"sscf_uni: SSCOP_RECOVER_IND in 3.0\n");
			return;
		}

		/*
		 * Validation based on sscop state
		 */
		switch (uvp->uv_lstate) {

		case UVL_READY:
			/*
			 * Recover connection due to internal problems
			 */

			/*
			 * Send recovery acknowledgement to sscop
			 */
			STACK_CALL(SSCOP_RECOVER_RSP, uvp->uv_lower, 
				uvp->uv_tokl, cvp, 
				0, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}

			/*
			 * Now notify the user that the connection 
			 * has been recovered
			 */
			STACK_CALL(SSCF_UNI_ESTABLISH_IND, uvp->uv_upper, 
				uvp->uv_toku, cvp, 
				SSCOP_UU_NULL, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVL_TERM:
			/*
			 * Ignoring everything
			 */
			break;

		case UVL_INST:
		case UVL_IDLE:
		case UVL_OUTCONN:
		case UVL_INCONN:
		case UVL_OUTDISC:
		case UVL_OUTRESYN:
		case UVL_INRESYN:
		case UVL_RECOVERY:
		default:
			log(LOG_ERR, "sscf_uni_upper: cmd=0x%x, lstate=%d\n",
				cmd, uvp->uv_lstate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCOP_UNITDATA_IND:
#ifdef notdef
		sscf_uni_pdu_print(uvp, (KBuffer *)arg1, "UNITDATA_IND");
#endif

		/*
		 * Validation based on sscop state
		 */
		switch (uvp->uv_lstate) {

		case UVL_IDLE:
		case UVL_OUTCONN:
		case UVL_INCONN:
		case UVL_OUTDISC:
		case UVL_OUTRESYN:
		case UVL_INRESYN:
		case UVL_RECOVERY:
		case UVL_READY:
			/*
			 * Incoming unassured data from peer
			 */

			/*
			 * Pass the data up to the user
			 */
			STACK_CALL(SSCF_UNI_UNITDATA_IND, uvp->uv_upper, 
				uvp->uv_toku, cvp, 
				arg1, 0, err);
			if (err) {
				KB_FREEALL((KBuffer *)arg1);
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVL_TERM:
			/*
			 * Ignoring everything
			 */
			KB_FREEALL((KBuffer *)arg1);
			break;

		case UVL_INST:
		default:
			KB_FREEALL((KBuffer *)arg1);
			log(LOG_ERR, "sscf_uni_upper: cmd=0x%x, lstate=%d\n",
				cmd, uvp->uv_lstate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCOP_RETRIEVE_IND:
	case SSCOP_RETRIEVECMP_IND:
		/*
		 * Not supported
		 */
	default:
		log(LOG_ERR, "sscf_uni_upper: unknown cmd 0x%x, uvp=%p\n",
			cmd, uvp);
	}

	return;
}

