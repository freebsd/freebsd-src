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
 * ITU-T Q.2110 - Process CPCS-signals (SSCOP PDUs)
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
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
 * Local functions
 */
static void	sscop_bgn_outconn __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_bgn_inconn __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_bgn_ready __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_bgrej_outrecov __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_end_outrecov __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_end_ready __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_endak_outrecov __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_rs_outresyn __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_rs_inresyn __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_rs_outrecov __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_rs_ready __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_er_error __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_er_idle __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_er_outrecov __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_er_recovrsp __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_er_inrecov __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_er_ready __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_erak_error __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_erak_idle __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_erak_outrecov __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_sd_ready __P((struct sscop *, KBuffer *, caddr_t));
static void	sscop_poll_ready __P((struct sscop *, KBuffer *, caddr_t));


/*
 * PDU type state lookup tables
 */
/* BGN PDU */
static void	(*sscop_bgn_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_bgn_idle,		/* SOS_IDLE */
			sscop_bgn_outconn,	/* SOS_OUTCONN */
			sscop_bgn_inconn,	/* SOS_INCONN */
			sscop_bgn_outdisc,	/* SOS_OUTDISC */
			sscop_bgn_outresyn,	/* SOS_OUTRESYN */
			sscop_bgn_inresyn,	/* SOS_INRESYN */
			sscop_bgn_inresyn,	/* SOS_OUTRECOV */
			sscop_bgn_inresyn,	/* SOS_RECOVRSP */
			sscop_bgn_inresyn,	/* SOS_INRECOV */
			sscop_bgn_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* BGAK PDU */
static void	(*sscop_bgak_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_bgak_idle,	/* SOS_IDLE */
			sscop_bgak_outconn,	/* SOS_OUTCONN */
			sscop_bgak_error,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_noop,		/* SOS_OUTRESYN */
			sscop_bgak_error,	/* SOS_INRESYN */
			sscop_bgak_error,	/* SOS_OUTRECOV */
			sscop_bgak_error,	/* SOS_RECOVRSP */
			sscop_bgak_error,	/* SOS_INRECOV */
			sscop_noop,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* BGREJ PDU */
static void	(*sscop_bgrej_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_bgrej_error,	/* SOS_IDLE */
			sscop_bgrej_outconn,	/* SOS_OUTCONN */
			sscop_bgrej_inconn,	/* SOS_INCONN */
			sscop_endak_outdisc,	/* SOS_OUTDISC */
			sscop_bgrej_outresyn,	/* SOS_OUTRESYN */
			sscop_bgrej_inconn,	/* SOS_INRESYN */
			sscop_bgrej_outrecov,	/* SOS_OUTRECOV */
			sscop_bgrej_inconn,	/* SOS_RECOVRSP */
			sscop_bgrej_inconn,	/* SOS_INRECOV */
			sscop_bgrej_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* END PDU */
static void	(*sscop_end_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_end_idle,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_end_inconn,	/* SOS_INCONN */
			sscop_end_outdisc,	/* SOS_OUTDISC */
			sscop_end_inconn,	/* SOS_OUTRESYN */
			sscop_end_inconn,	/* SOS_INRESYN */
			sscop_end_outrecov,	/* SOS_OUTRECOV */
			sscop_end_inconn,	/* SOS_RECOVRSP */
			sscop_end_inconn,	/* SOS_INRECOV */
			sscop_end_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* ENDAK PDU */
static void	(*sscop_endak_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_noop,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_endak_inconn,	/* SOS_INCONN */
			sscop_endak_outdisc,	/* SOS_OUTDISC */
			sscop_endak_inconn,	/* SOS_OUTRESYN */
			sscop_endak_inconn,	/* SOS_INRESYN */
			sscop_endak_outrecov,	/* SOS_OUTRECOV */
			sscop_endak_inconn,	/* SOS_RECOVRSP */
			sscop_endak_inconn,	/* SOS_INRECOV */
			sscop_endak_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* RS PDU */
static void	(*sscop_rs_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_rs_idle,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_rs_error,		/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_rs_outresyn,	/* SOS_OUTRESYN */
			sscop_rs_inresyn,	/* SOS_INRESYN */
			sscop_rs_outrecov,	/* SOS_OUTRECOV */
			sscop_rs_outrecov,	/* SOS_RECOVRSP */
			sscop_rs_outrecov,	/* SOS_INRECOV */
			sscop_rs_ready,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* RSAK PDU */
static void	(*sscop_rsak_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_rsak_idle,	/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_rsak_error,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_rsak_outresyn,	/* SOS_OUTRESYN */
			sscop_rsak_error,	/* SOS_INRESYN */
			sscop_rsak_error,	/* SOS_OUTRECOV */
			sscop_rsak_error,	/* SOS_RECOVRSP */
			sscop_rsak_error,	/* SOS_INRECOV */
			sscop_noop,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* ER PDU */
static void	(*sscop_er_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_er_idle,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_er_error,		/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_noop,		/* SOS_OUTRESYN */
			sscop_er_error,		/* SOS_INRESYN */
			sscop_er_outrecov,	/* SOS_OUTRECOV */
			sscop_er_recovrsp,	/* SOS_RECOVRSP */
			sscop_er_inrecov,	/* SOS_INRECOV */
			sscop_er_ready,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* ERAK PDU */
static void	(*sscop_erak_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_erak_idle,	/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_erak_error,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_noop,		/* SOS_OUTRESYN */
			sscop_erak_error,	/* SOS_INRESYN */
			sscop_erak_outrecov,	/* SOS_OUTRECOV */
			sscop_noop,		/* SOS_RECOVRSP */
			sscop_erak_error,	/* SOS_INRECOV */
			sscop_noop,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* SD PDU */
static void	(*sscop_sd_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_sd_idle,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_sd_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_noop,		/* SOS_OUTRESYN */
			sscop_sd_inconn,	/* SOS_INRESYN */
			sscop_noop,		/* SOS_OUTRECOV */
			sscop_noop,		/* SOS_RECOVRSP */
			sscop_sd_inconn,	/* SOS_INRECOV */
			sscop_sd_ready,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* POLL PDU */
static void	(*sscop_poll_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_poll_idle,	/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_poll_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_noop,		/* SOS_OUTRESYN */
			sscop_poll_inconn,	/* SOS_INRESYN */
			sscop_noop,		/* SOS_OUTRECOV */
			sscop_noop,		/* SOS_RECOVRSP */
			sscop_poll_inconn,	/* SOS_INRECOV */
			sscop_poll_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* STAT PDU */
static void	(*sscop_stat_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_stat_idle,	/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_stat_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_noop,		/* SOS_OUTRESYN */
			sscop_stat_inconn,	/* SOS_INRESYN */
			sscop_noop,		/* SOS_OUTRECOV */
			sscop_stat_inconn,	/* SOS_RECOVRSP */
			sscop_stat_inconn,	/* SOS_INRECOV */
			sscop_stat_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* USTAT PDU */
static void	(*sscop_ustat_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_ustat_idle,	/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_ustat_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_noop,		/* SOS_OUTRESYN */
			sscop_ustat_inconn,	/* SOS_INRESYN */
			sscop_noop,		/* SOS_OUTRECOV */
			sscop_ustat_inconn,	/* SOS_RECOVRSP */
			sscop_ustat_inconn,	/* SOS_INRECOV */
			sscop_ustat_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* UD PDU */
static void	(*sscop_ud_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_ud_all,		/* SOS_IDLE */
			sscop_ud_all,		/* SOS_OUTCONN */
			sscop_ud_all,		/* SOS_INCONN */
			sscop_ud_all,		/* SOS_OUTDISC */
			sscop_ud_all,		/* SOS_OUTRESYN */
			sscop_ud_all,		/* SOS_INRESYN */
			sscop_ud_all,		/* SOS_OUTRECOV */
			sscop_ud_all,		/* SOS_RECOVRSP */
			sscop_ud_all,		/* SOS_INRECOV */
			sscop_ud_all,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* MD PDU */
static void	(*sscop_md_tab[SOS_NUMSTATES])
				__P((struct sscop *, KBuffer *, caddr_t)) = {
			NULL,			/* SOS_INST */
			sscop_md_all,		/* SOS_IDLE */
			sscop_md_all,		/* SOS_OUTCONN */
			sscop_md_all,		/* SOS_INCONN */
			sscop_md_all,		/* SOS_OUTDISC */
			sscop_md_all,		/* SOS_OUTRESYN */
			sscop_md_all,		/* SOS_INRESYN */
			sscop_md_all,		/* SOS_OUTRECOV */
			sscop_md_all,		/* SOS_RECOVRSP */
			sscop_md_all,		/* SOS_INRECOV */
			sscop_md_all,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};


/*
 * PDU type lookup table
 */
void	(*(*sscop_q2110_pdutab[]))
				__P((struct sscop *, KBuffer *, caddr_t)) = {
		NULL,
		sscop_bgn_tab,
		sscop_bgak_tab,
		sscop_end_tab,
		sscop_endak_tab,
		sscop_rs_tab,
		sscop_rsak_tab,
		sscop_bgrej_tab,
		sscop_sd_tab,
		sscop_er_tab,
		sscop_poll_tab,
		sscop_stat_tab,
		sscop_ustat_tab,
		sscop_ud_tab,
		sscop_md_tab,
		sscop_erak_tab
};


/*
 * BGN PDU / SOS_OUTCONN Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_bgn_outconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct bgn_pdu	*bp = (struct bgn_pdu *)trlr;
	int		err;

	/*
	 * If retransmitted BGN, ignore it
	 */
	if (sscop_is_rexmit(sop, bp->bgn_nsq)) {
		KB_FREEALL(m);
		return;
	}

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Initialize state variables
	 */
	SEQ_SET(sop->so_sendmax, ntohl(bp->bgn_nmr));
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);
	q2110_init_state(sop);

	/*
	 * Return an ACK to peer
	 */
	(void) sscop_send_bgak(sop);

	/*
	 * Notify user of connection establishment
	 */
	STACK_CALL(SSCOP_ESTABLISH_CNF, sop->so_upper, sop->so_toku, 
		sop->so_connvc, (int)m, 0, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Start data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
	sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;

	/*
	 * OK, we're ready for data
	 */
	sop->so_state = SOS_READY;

	/*
	 * See if transmit queues need servicing
	 */
	if (sop->so_flags & SOF_XMITSRVC)
		sscop_service_xmit(sop);

	return;
}


/*
 * BGN PDU / SOS_INCONN Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_bgn_inconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct bgn_pdu	*bp = (struct bgn_pdu *)trlr;
	int		err;

	/*
	 * If retransmitted BGN, ignore it
	 */
	if (sscop_is_rexmit(sop, bp->bgn_nsq)) {
		KB_FREEALL(m);
		return;
	}

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(bp->bgn_nmr));

	/*
	 * First, tell user current connection has been released
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_USER, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Now, tell user of new connection establishment
	 */
	STACK_CALL(SSCOP_ESTABLISH_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, (int)m, 0, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	return;
}


/*
 * BGN PDU / SOS_READY Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_bgn_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct bgn_pdu	*bp = (struct bgn_pdu *)trlr;
	int		err;

	/*
	 * If retransmitted BGN, just ACK it again
	 */
	if (sscop_is_rexmit(sop, bp->bgn_nsq)) {
		KB_FREEALL(m);
		sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;
		(void) sscop_send_bgak(sop);
		return;
	}

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(bp->bgn_nmr));

	/*
	 * Clear out appropriate queues
	 */
	q2110_prep_retrieve(sop);

	/*
	 * Tell user current connection has been released
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_USER, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Tell user of incoming connection
	 */
	STACK_CALL(SSCOP_ESTABLISH_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, (int)m, 0, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Wait for user's response
	 */
	sop->so_state = SOS_INCONN;

	return;
}


/*
 * BGREJ PDU / SOS_OUTRECOV Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_bgrej_outrecov(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Report protocol error
	 */
	sscop_bgrej_error(sop, m, trlr);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Clear receiver buffer
	 */
	sscop_rcvr_drain(sop);

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * END PDU / SOS_OUTRECOV Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_end_outrecov(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct end_pdu	*ep = (struct end_pdu *)trlr;
	int		err, source;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Acknowledge END
	 */
	(void) sscop_send_endak(sop);

	/*
	 * Get Source value
	 */
	if (ep->end_type & PT_SOURCE_SSCOP)
		source = SSCOP_SOURCE_SSCOP;
	else
		source = SSCOP_SOURCE_USER;

	/*
	 * Notify user of connection termination
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, (int)m, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Clear receiver buffer
	 */
	sscop_rcvr_drain(sop);

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * END PDU / SOS_READY Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_end_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct end_pdu	*ep = (struct end_pdu *)trlr;
	int		err, source;

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Acknowledge END
	 */
	(void) sscop_send_endak(sop);

	/*
	 * Get Source value
	 */
	if (ep->end_type & PT_SOURCE_SSCOP)
		source = SSCOP_SOURCE_SSCOP;
	else
		source = SSCOP_SOURCE_USER;

	/*
	 * Notify user of connection termination
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, (int)m, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Clear out appropriate queues
	 */
	q2110_prep_retrieve(sop);

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * ENDAK PDU / SOS_OUTRECOV Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_endak_outrecov(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Report protocol error
	 */
	sscop_endak_error(sop, m, trlr);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Clear receiver buffer
	 */
	sscop_rcvr_drain(sop);

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * RS PDU / SOS_OUTRESYN Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_rs_outresyn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct rs_pdu	*rp = (struct rs_pdu *)trlr;
	int		err;

	/*
	 * If retransmitted RS, ignore it
	 */
	if (sscop_is_rexmit(sop, rp->rs_nsq)) {
		KB_FREEALL(m);
		return;
	}

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Initialize state variables
	 */
	SEQ_SET(sop->so_sendmax, ntohl(rp->rs_nmr));
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);
	q2110_init_state(sop);

	/*
	 * Free PDU buffers
	 */
	KB_FREEALL(m);

	/*
	 * Return an ACK to peer
	 */
	(void) sscop_send_rsak(sop);

	/*
	 * Notify user of connection resynchronization
	 */
	STACK_CALL(SSCOP_RESYNC_CNF, sop->so_upper, sop->so_toku, 
		sop->so_connvc, 0, 0, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Start data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
	sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;

	/*
	 * OK, we're ready for data
	 */
	sop->so_state = SOS_READY;

	/*
	 * See if transmit queues need servicing
	 */
	if (sop->so_flags & SOF_XMITSRVC)
		sscop_service_xmit(sop);

	return;
}


/*
 * RS PDU / SOS_INRESYN Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_rs_inresyn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct rs_pdu	*rp = (struct rs_pdu *)trlr;

	/*
	 * If retransmitted RS, ignore it
	 */
	if (sscop_is_rexmit(sop, rp->rs_nsq)) {
		KB_FREEALL(m);
		return;
	}

	/*
	 * Report error condition
	 */
	sscop_rs_error(sop, m, trlr);

	return;
}


/*
 * RS PDU / SOS_OUTRECOV Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_rs_outrecov(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct rs_pdu	*rp = (struct rs_pdu *)trlr;
	int		err;

	/*
	 * If retransmitted RS, report an error
	 */
	if (sscop_is_rexmit(sop, rp->rs_nsq)) {
		sscop_rs_error(sop, m, trlr);
		return;
	}

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(rp->rs_nmr));

	/*
	 * Notify user of connection resynchronization
	 */
	STACK_CALL(SSCOP_RESYNC_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, (int)m, 0, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Clear receiver buffer
	 */
	sscop_rcvr_drain(sop);

	/*
	 * Wait for user response
	 */
	sop->so_state = SOS_INRESYN;

	return;
}


/*
 * RS PDU / SOS_READY Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_rs_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct rs_pdu	*rp = (struct rs_pdu *)trlr;
	int		err;

	/*
	 * If retransmitted RS, just ACK it
	 */
	if (sscop_is_rexmit(sop, rp->rs_nsq)) {
		KB_FREEALL(m);
		sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;
		sscop_send_rsak(sop);
		return;
	}

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(rp->rs_nmr));

	/*
	 * Notify user of connection resynchronization
	 */
	STACK_CALL(SSCOP_RESYNC_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, (int)m, 0, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Clear out appropriate queues
	 */
	q2110_prep_retrieve(sop);

	/*
	 * Wait for user response
	 */
	sop->so_state = SOS_INRESYN;

	return;
}

/*
 * ER PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_er_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'L');
	KB_FREEALL(m);

	return;
}


/*
 * ER PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_er_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_er_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	return;
}


/*
 * ER PDU / SOS_OUTRECOV Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_er_outrecov(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct er_pdu	*ep = (struct er_pdu *)trlr;
	int		err;

	/*
	 * If retransmitted ER, report an error
	 */
	if (sscop_is_rexmit(sop, ep->er_nsq)) {
		sscop_er_error(sop, m, trlr);
		return;
	}

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(ep->er_nmr));

	/*
	 * Initialize receiver window
	 */
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);

	/*
	 * Free PDU buffers
	 */
	KB_FREEALL(m);

	/*
	 * Acknowledge ER
	 */
	(void) sscop_send_erak(sop);

	/*
	 * Deliver any outstanding data to user
	 */
	q2110_deliver_data(sop);

	/*
	 * Notify user of connection recovery
	 */
	STACK_CALL(SSCOP_RECOVER_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, 0, 0, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Wait for user response
	 */
	sop->so_state = SOS_RECOVRSP;

	return;
}


/*
 * ER PDU / SOS_RECOVRSP Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_er_recovrsp(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct er_pdu	*ep = (struct er_pdu *)trlr;

	/*
	 * If retransmitted ER, just ACK it
	 */
	if (sscop_is_rexmit(sop, ep->er_nsq)) {
		KB_FREEALL(m);
		(void) sscop_send_erak(sop);
		return;
	}

	/*
	 * Report error condition
	 */
	sscop_er_error(sop, m, trlr);

	return;
}


/*
 * ER PDU / SOS_INRECOV Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_er_inrecov(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct er_pdu	*ep = (struct er_pdu *)trlr;

	/*
	 * If retransmitted ER, just ignore it
	 */
	if (sscop_is_rexmit(sop, ep->er_nsq)) {
		KB_FREEALL(m);
		return;
	}

	/*
	 * Report error condition
	 */
	sscop_er_error(sop, m, trlr);

	return;
}


/*
 * ER PDU / SOS_READY Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_er_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct er_pdu	*ep = (struct er_pdu *)trlr;
	int		err;

	/*
	 * If retransmitted ER, just ACK it
	 */
	if (sscop_is_rexmit(sop, ep->er_nsq)) {
		KB_FREEALL(m);
		sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;
		sscop_send_erak(sop);
		return;
	}

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(ep->er_nmr));

	/*
	 * Free PDU buffers
	 */
	KB_FREEALL(m);

	/*
	 * Clear out appropriate queues
	 */
	q2110_prep_recovery(sop);

	/*
	 * Deliver any outstanding data to user
	 */
	q2110_deliver_data(sop);

	/*
	 * Notify user of connection recovery
	 */
	STACK_CALL(SSCOP_RECOVER_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, 0, 0, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Wait for user response
	 */
	sop->so_state = SOS_INRECOV;

	return;
}


/*
 * ERAK PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_erak_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'M');
	KB_FREEALL(m);

	return;
}


/*
 * ERAK PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_erak_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_erak_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	return;
}


/*
 * ERAK PDU / SOS_OUTRECOV Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_erak_outrecov(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct erak_pdu	*ep = (struct erak_pdu *)trlr;
	int		err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(ep->erak_nmr));

	/*
	 * Free PDU buffers
	 */
	KB_FREEALL(m);

	/*
	 * Deliver any outstanding data to user
	 */
	q2110_deliver_data(sop);

	/*
	 * Notify user of connection recovery
	 */
	STACK_CALL(SSCOP_RECOVER_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, 0, 0, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Wait for user response
	 */
	sop->so_state = SOS_RECOVRSP;

	return;
}


/*
 * SD PDU / SOS_READY Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_sd_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct sd_pdu	*sp = (struct sd_pdu *)trlr;
	struct pdu_hdr	*php;
	KBuffer		*n;
	sscop_seq	ns;
	int		err, space;

	/*
	 * Get PDU sequence number
	 */
	SEQ_SET(ns, ntohl(sp->sd_ns));

	/*
	 * Ensure that the sequence number fits within the window
	 */
	if (SEQ_GEQ(ns, sop->so_rcvmax, sop->so_rcvnext)) {
		/*
		 * It doesn't, drop received data
		 */
		KB_FREEALL(m);

		/*
		 * If next highest PDU hasn't reached window end yet,
		 * then send a USTAT to inform transmitter of this gap
		 */
		if (SEQ_LT(sop->so_rcvhigh, sop->so_rcvmax, sop->so_rcvnext)) { 
			(void) sscop_send_ustat(sop, sop->so_rcvmax);
			sop->so_rcvhigh = sop->so_rcvmax;
		}
		return;
	}

	/*
	 * If this is the next in-sequence PDU, hand it to user
	 */
	if (ns == sop->so_rcvnext) {
		STACK_CALL(SSCOP_DATA_IND, sop->so_upper, sop->so_toku,
			sop->so_connvc, (int)m, ns, err);
		if (err) {
			KB_FREEALL(m);
			return;
		}

		/*
		 * Bump next expected sequence number
		 */
		SEQ_INCR(sop->so_rcvnext, 1);

		/*
		 * Slide receive window down
		 */
		SEQ_INCR(sop->so_rcvmax, 1);

		/*
		 * Is this the highest sequence PDU we've received??
		 */
		if (ns == sop->so_rcvhigh) {
			/*
			 * Yes, bump the limit and exit
			 */
			sop->so_rcvhigh = sop->so_rcvnext;
			return;
		}

		/*
		 * This is a retransmitted PDU, so see if we have
		 * more in-sequence PDUs already queued up
		 */
		while ((php = sop->so_recv_hd) &&
		       (php->ph_ns == sop->so_rcvnext)) {

			/*
			 * Yup we do, so remove next PDU from queue and
			 * pass it up to the user as well
			 */
			sop->so_recv_hd = php->ph_recv_lk;
			if (sop->so_recv_hd == NULL)
				sop->so_recv_tl = NULL;
			STACK_CALL(SSCOP_DATA_IND, sop->so_upper, sop->so_toku,
				sop->so_connvc, (int)php->ph_buf, php->ph_ns,
				err);
			if (err) {
				/*
				 * Should never happen, but...
				 */
				KB_FREEALL(php->ph_buf);
				sscop_abort(sop, "stack memory\n");
				return;
			}

			/*
			 * Bump next expected sequence number
			 */
			SEQ_INCR(sop->so_rcvnext, 1);

			/*
			 * Slide receive window down
			 */
			SEQ_INCR(sop->so_rcvmax, 1);
		}

		/*
		 * Finished with data delivery...
		 */
		return;
	}

	/*
	 * We're gonna have to queue this PDU, so find space
	 * for the PDU header
	 */
	KB_HEADROOM(m, space);

	/*
	 * If there's not enough room in the received buffer,
	 * allocate & link a new buffer for the header
	 */
	if (space < sizeof(struct pdu_hdr)) {

		KB_ALLOC(n, sizeof(struct pdu_hdr), KB_F_NOWAIT, KB_T_HEADER);
		if (n == NULL) {
			KB_FREEALL(m);
			return;
		}
		KB_HEADSET(n, sizeof(struct pdu_hdr));
		KB_LEN(n) = 0;
		KB_LINKHEAD(n, m);
		m = n;
	}

	/*
	 * Build PDU header
	 *
	 * We can at least assume/require that the start of
	 * the user data is aligned.  Also note that we don't
	 * include this header in the buffer len/offset fields.
	 */
	KB_DATASTART(m, php, struct pdu_hdr *);
	php--;
	php->ph_ns = ns;
	php->ph_buf = m;

	/*
	 * Insert PDU into the receive queue
	 */
	if (sscop_recv_insert(sop, php)) {
		/*
		 * Oops, a duplicate sequence number PDU is already on
		 * the queue, somethings wrong here.
		 */
		sscop_maa_error(sop, 'Q');

		/*
		 * Free buffers
		 */
		KB_FREEALL(m);

		/*
		 * Go into recovery mode
		 */
		q2110_error_recovery(sop);

		return;
	}

	/*
	 * Are we at the high-water mark??
	 */
	if (ns == sop->so_rcvhigh) {
		/*
		 * Yes, just bump the mark
		 */
		SEQ_INCR(sop->so_rcvhigh, 1);

		return;
	}

	/*
	 * Are we beyond the high-water mark??
	 */
	if (SEQ_GT(ns, sop->so_rcvhigh, sop->so_rcvnext)) {
		/*
		 * Yes, then there's a missing PDU, so inform the transmitter
		 */
		(void) sscop_send_ustat(sop, ns);

		/*
		 * Update high-water mark
		 */
		sop->so_rcvhigh = SEQ_ADD(ns, 1);
	}

	return;
}


/*
 * POLL PDU / SOS_READY Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
static void
sscop_poll_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct poll_pdu	*pp = (struct poll_pdu *)trlr;
	sscop_seq	nps;

	NTOHL(pp->poll_ns);

	/*
	 * If the poll sequence number is less than highest number
	 * we've already seen, something's wrong
	 */
	if (SEQ_LT(pp->poll_ns, sop->so_rcvhigh, sop->so_rcvnext)) {
		/*
		 * Record error condition
		 */
		sscop_maa_error(sop, 'Q');

		/*
		 * Free buffers
		 */
		KB_FREEALL(m);

		/*
		 * Go into recovery mode
		 */
		q2110_error_recovery(sop);

		return;
	}

	/*
	 * Set a new "next highest" sequence number expected
	 */
	if (SEQ_LT(pp->poll_ns, sop->so_rcvmax, sop->so_rcvnext))
		SEQ_SET(sop->so_rcvhigh, pp->poll_ns);
	else
		sop->so_rcvhigh = sop->so_rcvmax;

	/*
	 * Return a STAT PDU to peer
	 */
	SEQ_SET(nps, ntohl(pp->poll_nps));
	KB_FREEALL(m);
	(void) sscop_send_stat(sop, nps);

	return;
}

