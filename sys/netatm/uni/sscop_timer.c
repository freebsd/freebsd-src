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
 * SSCOP - Timer processing
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
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

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_var.h>

/*
 * Local functions
 */
static void	sscop_poll_expire(struct sscop *);
static void	sscop_noresponse_expire(struct sscop *);
static void	sscop_cc_expire(struct sscop *);
static void	sscop_idle_expire(struct sscop *);

/*
 * Local variables
 */
static void	(*sscop_expired[SSCOP_T_NUM])(struct sscop *) = {
	sscop_poll_expire,
	sscop_noresponse_expire,
	sscop_cc_expire,
	sscop_idle_expire
};


/*
 * Process an SSCOP timer tick
 * 
 * This function is called SSCOP_HZ times a second in order to update
 * all of the sscop connection timers.  The sscop expiration function
 * will be called to process all timer expirations.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to sscop timer control block
 *
 * Returns:
 *	none
 *
 */
void
sscop_timeout(tip)
	struct atm_time	*tip;
{
	struct sscop	*sop, **sprev;
	int		i;


	/*
	 * Schedule next timeout
	 */
	atm_timeout(&sscop_timer, ATM_HZ/SSCOP_HZ, sscop_timeout);

	/*
	 * Run through all connections, updating each active timer.
	 * If an expired timer is found, notify that entry.
	 */
	sprev = &sscop_head;
	while ((sop = *sprev) != NULL) {

		/*
		 * Check out each timer
		 */
		for (i =0; i < SSCOP_T_NUM; i++) {

			/*
			 * Decrement timer if it's active
			 */
			if (sop->so_timer[i] && (--sop->so_timer[i] == 0)) {

#ifdef DIAGNOSTIC
				{
					static char	*tn[] = {
						"POLL",
						"NORESPONSE",
						"CC",
						"IDLE"
					};
					ATM_DEBUG3("sscop_timer: %s expired, sop=%p, state=%d\n",
						tn[i], sop, sop->so_state);
				}
#endif

				/*
				 * Expired timer - process it
				 */
				(*sscop_expired[i])(sop);

				/*
				 * Make sure connection still exists
				 */
				if (*sprev != sop)
					break;
			}
		}

		/*
		 * Update previous pointer if current control
		 * block wasn't deleted
		 */
		if (*sprev == sop)
			sprev = &sop->so_next;
	}
}


/*
 * Process an SSCOP Timer_POLL expiration
 * 
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	none
 *
 */
static void
sscop_poll_expire(sop)
	struct sscop	*sop;
{

	/*
	 * Validate current state 
	 */
	if ((sop->so_state != SOS_READY) &&
	    ((sop->so_state != SOS_INRESYN) ||
	     (sop->so_vers != SSCOP_VERS_QSAAL))) {
		log(LOG_ERR, "sscop: invalid %s state: sop=%p, state=%d\n",
			"Timer_POLL", sop, sop->so_state);
		return;
	}

	/*
	 * Send next poll along its way
	 */
	SEQ_INCR(sop->so_pollsend, 1);
	(void) sscop_send_poll(sop);

	/*
	 * Reset data counter for this poll cycle
	 */
	sop->so_polldata = 0;

	/*
	 * Reset polling timer
	 */
	sscop_set_poll(sop);

	return;
}


/*
 * Process an SSCOP Timer_IDLE expiration
 * 
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	none
 *
 */
static void
sscop_idle_expire(sop)
	struct sscop	*sop;
{

	/*
	 * Timer_IDLE only valid in READY state
	 */
	if (sop->so_state != SOS_READY) {
		log(LOG_ERR, "sscop: invalid %s state: sop=%p, state=%d\n",
			"Timer_IDLE", sop, sop->so_state);
		return;
	}

	/*
	 * Send next poll along its way
	 */
	SEQ_INCR(sop->so_pollsend, 1);
	(void) sscop_send_poll(sop);

	/*
	 * Reset data counter for this poll cycle
	 */
	sop->so_polldata = 0;

	/*
	 * Start NO-RESPONSE timer
	 */
	sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;

	/*
	 * Reset polling timer
	 */
	sscop_set_poll(sop);

	return;
}


/*
 * Process an SSCOP Timer_NORESPONSE expiration
 * 
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	none
 *
 */
static void
sscop_noresponse_expire(sop)
	struct sscop	*sop;
{
	int		err;

	/*
	 * Validate current state 
	 */
	if ((sop->so_state != SOS_READY) &&
	    ((sop->so_state != SOS_INRESYN) ||
	     (sop->so_vers != SSCOP_VERS_QSAAL))) {
		log(LOG_ERR, "sscop: invalid %s state: sop=%p, state=%d\n",
			"Timer_NORESPONSE", sop, sop->so_state);
		return;
	}

	/*
	 * Peer seems to be dead, so terminate session
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, 
		sop->so_toku, sop->so_connvc, 
		SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		/*
		 * Error, force retry
		 */
		sop->so_timer[SSCOP_T_NORESP] = 1;
		return;
	}

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Notify peer of termination
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Report peer's failure
	 */
	sscop_maa_error(sop, 'P');

	if (sop->so_vers == SSCOP_VERS_QSAAL)
		/*
		 * Clear connection data
		 */
		qsaal1_clear_connection(sop);
	else
		/*
		 * Clear out appropriate queues
		 */
		q2110_prep_retrieve(sop);

	/*
	 * Return to IDLE state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * Process an SSCOP Timer_CC expiration
 * 
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	none
 *
 */
static void
sscop_cc_expire(sop)
	struct sscop	*sop;
{
	int		err;

	/*
	 * Process timeout based on protocol state
	 */
	switch (sop->so_state) {

	case SOS_OUTCONN:
		/*
		 * No response to our BGN yet
		 */
		if (sop->so_connctl < sop->so_parm.sp_maxcc) {

			/*
			 * Send another BGN PDU
			 */
			sop->so_connctl++;
			(void) sscop_send_bgn(sop, SSCOP_SOURCE_USER);

			/*
			 * Restart retransmit timer
			 */
			sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

		} else {

			/*
			 * Retransmit limit exceeded, terminate session
			 */
			STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, 
				sop->so_toku, sop->so_connvc, 
				SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
			if (err) {
				/*
				 * Error, force retry
				 */
				sop->so_timer[SSCOP_T_CC] = 1;
				break;
			}

			/*
			 * Notify peer of termination
			 */
			(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

			/*
			 * Report establishment failure
			 */
			sscop_maa_error(sop, 'O');

			/*
			 * Clear reestablishment flag
			 */
			sop->so_flags &= ~SOF_REESTAB;

			/*
			 * Return to IDLE state
			 */
			sop->so_state = SOS_IDLE;
		}
		break;

	case SOS_OUTDISC:
		/*
		 * No response to our END yet
		 */
		if (sop->so_connctl < sop->so_parm.sp_maxcc) {

			/*
			 * Send another END PDU
			 */
			sop->so_connctl++;
			(void) sscop_send_end(sop, SSCOP_SOURCE_LAST);

			/*
			 * Restart retransmit timer
			 */
			sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

		} else {

			/*
			 * Retransmit limit exceeded, force session termination
			 */
			STACK_CALL(SSCOP_RELEASE_CNF, sop->so_upper, 
				sop->so_toku, sop->so_connvc, 0, 0, err);
			if (err) {
				/*
				 * Error, force retry
				 */
				sop->so_timer[SSCOP_T_CC] = 1;
				break;
			}

			/*
			 * Report establishment failure
			 */
			sscop_maa_error(sop, 'O');

			/*
			 * Return to IDLE state
			 */
			sop->so_state = SOS_IDLE;
		}
		break;

	case SOS_OUTRESYN:
rexmitrs:
		/*
		 * No response to our RS yet
		 */
		if (sop->so_connctl < sop->so_parm.sp_maxcc) {

			/*
			 * Send another RS PDU
			 */
			sop->so_connctl++;
			(void) sscop_send_rs(sop);

			/*
			 * Restart retransmit timer
			 */
			sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

		} else {

			/*
			 * Retransmit limit exceeded, terminate session
			 */
			STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, 
				sop->so_toku, sop->so_connvc, 
				SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
			if (err) {
				/*
				 * Error, force retry
				 */
				sop->so_timer[SSCOP_T_CC] = 1;
				break;
			}

			/*
			 * Notify peer of termination
			 */
			(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

			/*
			 * Report establishment failure
			 */
			sscop_maa_error(sop, 'O');

			if (sop->so_vers == SSCOP_VERS_QSAAL)
				/*
				 * Clear connection data
				 */
				qsaal1_clear_connection(sop);

			/*
			 * Return to IDLE state
			 */
			sop->so_state = SOS_IDLE;
		}
		break;

	case SOS_CONRESYN:	/* Q.SAAL1 */
#if (SOS_OUTRECOV != SOS_CONRESYN)
	case SOS_OUTRECOV:	/* Q.2110 */
#endif
		if (sop->so_vers == SSCOP_VERS_QSAAL) {
			/*
			 * Handle timeout for SOS_CONRESYN
			 */
			goto rexmitrs;
		} 

		/*
		 * Handle timeout for SOS_OUTRECOV
		 */

		/*
		 * No response to our ER yet
		 */
		if (sop->so_connctl < sop->so_parm.sp_maxcc) {

			/*
			 * Send another ER PDU
			 */
			sop->so_connctl++;
			(void) sscop_send_er(sop);

			/*
			 * Restart retransmit timer
			 */
			sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

		} else {

			/*
			 * Retransmit limit exceeded, terminate session
			 */
			STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, 
				sop->so_toku, sop->so_connvc, 
				SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
			if (err) {
				/*
				 * Error, force retry
				 */
				sop->so_timer[SSCOP_T_CC] = 1;
				break;
			}

			/*
			 * Notify peer of termination
			 */
			(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

			/*
			 * Report establishment failure
			 */
			sscop_maa_error(sop, 'O');

			/*
			 * Clear receiver buffer
			 */
			sscop_rcvr_drain(sop);

			/*
			 * Return to IDLE state
			 */
			sop->so_state = SOS_IDLE;
		}
		break;

	default:
		log(LOG_ERR, "sscop: invalid %s state: sop=%p, state=%d\n",
			"Timer_CC", sop, sop->so_state);
	}
}

