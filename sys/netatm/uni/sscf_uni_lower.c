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
 * SSCF UNI - SSCF_UNI SAP interface processing
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
#include <netatm/uni/sscf_uni.h>
#include <netatm/uni/sscf_uni_var.h>

#include <vm/uma.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

extern uma_zone_t	unisig_vc_zone;

/*
 * Local variables
 */
static struct sscop_parms	sscf_uni_sscop_parms = {
	4096,				/* sp_maxinfo */
	4096,				/* sp_maxuu */
	4,				/* sp_maxcc */
	25,				/* sp_maxpd */
	1 * ATM_HZ,			/* sp_timecc */
	2 * ATM_HZ,			/* sp_timekeep */
	7 * ATM_HZ,			/* sp_timeresp */
	1 * ATM_HZ,			/* sp_timepoll */
	15 * ATM_HZ,			/* sp_timeidle */
	80				/* sp_rcvwin */
};


/*
 * SSCF_UNI Lower Stack Command Handler
 * 
 * This function will receive all of the stack commands issued from the 
 * layer above SSCF UNI (ie. Q.2931).
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
sscf_uni_lower(cmd, tok, arg1, arg2)
	int	cmd;
	void	*tok;
	intptr_t	arg1;
	intptr_t	arg2;
{
	struct univcc	*uvp = (struct univcc *)tok;
	Atm_connvc	*cvp = uvp->uv_connvc;
	enum sscop_vers	vers;
	int		err;

	ATM_DEBUG5("sscf_uni_lower: cmd=0x%x, uvp=%p, ustate=%d, arg1=%p, arg2=%p\n",
		cmd, uvp, uvp->uv_ustate, (void *)arg1, (void *)arg2);

	switch (cmd) {

	case SSCF_UNI_INIT:
		/*
		 * Validate state
		 */
		if (uvp->uv_ustate != UVU_INST) {
			log(LOG_ERR, "sscf_uni_lower: SSCF_INIT in ustate=%d\n",
				uvp->uv_ustate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
			break;
		}

		/*
		 * Validate UNI version
		 */
		if ((enum uni_vers)arg1 == UNI_VERS_3_0)
			vers = SSCOP_VERS_QSAAL;
		else if ((enum uni_vers)arg1 == UNI_VERS_3_1)
			vers = SSCOP_VERS_Q2110;
		else {
			sscf_uni_abort(uvp, "sscf_uni: bad version\n");
			break;
		}
		uvp->uv_vers = (enum uni_vers)arg1;

		/*
		 * Make ourselves ready and pass on the INIT
		 */
		uvp->uv_ustate = UVU_RELEASED;
		uvp->uv_lstate = UVL_IDLE;

		STACK_CALL(SSCOP_INIT, uvp->uv_lower, uvp->uv_tokl, cvp, 
			(int)vers, (int)&sscf_uni_sscop_parms, err);
		if (err) {
			/*
			 * Should never happen
			 */
			sscf_uni_abort(uvp, "sscf_uni: INIT failure\n");
		}
		break;

	case SSCF_UNI_TERM:
		/*
		 * Set termination states
		 */
		uvp->uv_ustate = UVU_TERM;
		uvp->uv_lstate = UVL_TERM;

		/*
		 * Pass the TERM down the stack
		 */
		STACK_CALL(SSCOP_TERM, uvp->uv_lower, uvp->uv_tokl, cvp,
			0, 0, err);
		if (err) {
			/*
			 * Should never happen
			 */
			sscf_uni_abort(uvp, "sscf_uni: TERM failure\n");
			return;
		}
		uma_zfree(unisig_vc_zone, uvp);
		sscf_uni_vccnt--;
		break;

	case SSCF_UNI_ESTABLISH_REQ:
		/*
		 * Validation based on user state
		 */
		switch (uvp->uv_ustate) {

		case UVU_RELEASED:
		case UVU_PRELEASE:
			/*
			 * Establishing a new connection
			 */
			uvp->uv_ustate = UVU_PACTIVE;
			uvp->uv_lstate = UVL_OUTCONN;
			STACK_CALL(SSCOP_ESTABLISH_REQ, uvp->uv_lower, 
				uvp->uv_tokl, cvp, 
				SSCOP_UU_NULL, SSCOP_BR_YES, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVU_ACTIVE:
			/*
			 * Resynchronizing a connection
			 */
			uvp->uv_ustate = UVU_PACTIVE;
			if (uvp->uv_vers == UNI_VERS_3_0) {
				uvp->uv_lstate = UVL_OUTCONN;
				STACK_CALL(SSCOP_ESTABLISH_REQ, uvp->uv_lower, 
					uvp->uv_tokl, cvp, 
					SSCOP_UU_NULL, SSCOP_BR_YES, err);
			} else {
				uvp->uv_lstate = UVL_OUTRESYN;
				STACK_CALL(SSCOP_RESYNC_REQ, uvp->uv_lower, 
					uvp->uv_tokl, cvp, 
					SSCOP_UU_NULL, 0, err);
			}
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVU_TERM:
			/* Ignore */
			break;

		case UVU_INST:
		case UVU_PACTIVE:
		default:
			log(LOG_ERR, "sscf_uni_lower: cmd=0x%x, ustate=%d\n",
				cmd, uvp->uv_ustate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCF_UNI_RELEASE_REQ:
		/*
		 * Validate re-establishment parameter
		 */
		switch (arg1) {

		case SSCF_UNI_ESTIND_YES:
			uvp->uv_flags &= ~UVF_NOESTIND;
			break;

		case SSCF_UNI_ESTIND_NO:
			uvp->uv_flags |= UVF_NOESTIND;
			break;

		default:
			sscf_uni_abort(uvp, "sscf_uni: bad estind value\n");
			return;
		}

		/*
		 * Validation based on user state
		 */
		switch (uvp->uv_ustate) {

		case UVU_RELEASED:
			/*
			 * Releasing a non-existant connection
			 */
			STACK_CALL(SSCF_UNI_RELEASE_CNF, uvp->uv_upper, 
				uvp->uv_toku, cvp, 
				0, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVU_PACTIVE:
		case UVU_ACTIVE:
			/*
			 * Releasing a connection
			 */
			uvp->uv_ustate = UVU_PRELEASE;
			uvp->uv_lstate = UVL_OUTDISC;
			STACK_CALL(SSCOP_RELEASE_REQ, uvp->uv_lower, 
				uvp->uv_tokl, cvp, 
				SSCOP_UU_NULL, 0, err);
			if (err) {
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVU_TERM:
			/* Ignore */
			break;

		case UVU_INST:
		case UVU_PRELEASE:
		default:
			log(LOG_ERR, "sscf_uni_lower: cmd=0x%x, ustate=%d\n",
				cmd, uvp->uv_ustate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCF_UNI_DATA_REQ:
#ifdef notdef
		sscf_uni_pdu_print(uvp, (KBuffer *)arg1, "DATA_REQ");
#endif

		/*
		 * Validation based on user state
		 */
		switch (uvp->uv_ustate) {

		case UVU_ACTIVE:
			/*
			 * Send assured data on connection
			 */
			STACK_CALL(SSCOP_DATA_REQ, uvp->uv_lower, 
				uvp->uv_tokl, cvp, 
				arg1, 0, err);
			if (err) {
				KB_FREEALL((KBuffer *)arg1);
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVU_RELEASED:
		case UVU_TERM:
			/*
			 * Release supplied buffers and ignore
			 */
			KB_FREEALL((KBuffer *)arg1);
			break;

		case UVU_INST:
		case UVU_PACTIVE:
		case UVU_PRELEASE:
		default:
			KB_FREEALL((KBuffer *)arg1);
			log(LOG_ERR, "sscf_uni_lower: cmd=0x%x, ustate=%d\n",
				cmd, uvp->uv_ustate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	case SSCF_UNI_UNITDATA_REQ:
#ifdef notdef
		sscf_uni_pdu_print(uvp, (KBuffer *)arg1, "UNITDATA_REQ");
#endif

		/*
		 * Validation based on user state
		 */
		switch (uvp->uv_ustate) {

		case UVU_RELEASED:
		case UVU_PACTIVE:
		case UVU_PRELEASE:
		case UVU_ACTIVE:
			/*
			 * Send unassured data on connection
			 */
			STACK_CALL(SSCOP_UNITDATA_REQ, uvp->uv_lower, 
				uvp->uv_tokl, cvp, 
				arg1, 0, err);
			if (err) {
				KB_FREEALL((KBuffer *)arg1);
				sscf_uni_abort(uvp, "sscf_uni: stack memory\n");
				return;
			}
			break;

		case UVU_TERM:
			/*
			 * Release supplied buffers and ignore
			 */
			KB_FREEALL((KBuffer *)arg1);
			break;

		case UVU_INST:
		default:
			KB_FREEALL((KBuffer *)arg1);
			log(LOG_ERR, "sscf_uni_lower: cmd=0x%x, ustate=%d\n",
				cmd, uvp->uv_ustate);
			sscf_uni_abort(uvp, "sscf_uni: sequence err\n");
		}
		break;

	default:
		log(LOG_ERR, "sscf_uni_lower: unknown cmd 0x%x, uvp=%p\n",
			cmd, uvp);
	}

	return;
}

