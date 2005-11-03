/*-
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
 * ITU-T Q.SAAL1 - Process CPCS-signals (SSCOP PDUs)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netatm/uni/qsaal1_sigcpcs.c,v 1.12 2005/01/07 01:45:37 imp Exp $");

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

/*
 * Local functions
 */
static void	sscop_bgn_outconn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_end_outresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_end_conresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_end_ready(struct sscop *, KBuffer *, caddr_t);
static void	sscop_endak_outresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_rs_outresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_rs_ready(struct sscop *, KBuffer *, caddr_t);
static void	sscop_rsak_conresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_sd_inresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_sd_conresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_sd_process(struct sscop *, KBuffer *, caddr_t, int);
static void	sscop_sd_ready(struct sscop *, KBuffer *, caddr_t);
static void	sscop_sdp_ready(struct sscop *, KBuffer *, caddr_t);
static void	sscop_poll_inresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_poll_conresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_poll_ready(struct sscop *, KBuffer *, caddr_t);
static void	sscop_stat_conresyn(struct sscop *, KBuffer *, caddr_t);
static void	sscop_ustat_conresyn(struct sscop *, KBuffer *, caddr_t);


/*
 * PDU type state lookup tables
 */
/* BGN PDU */
static void	(*sscop_bgn_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_bgn_idle,		/* SOS_IDLE */
			sscop_bgn_outconn,	/* SOS_OUTCONN */
			sscop_noop,		/* SOS_INCONN */
			sscop_bgn_outdisc,	/* SOS_OUTDISC */
			sscop_bgn_outresyn,	/* SOS_OUTRESYN */
			sscop_bgn_inresyn,	/* SOS_INRESYN */
			sscop_bgn_outresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_bgn_inresyn,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* BGAK PDU */
static void	(*sscop_bgak_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_bgak_idle,	/* SOS_IDLE */
			sscop_bgak_outconn,	/* SOS_OUTCONN */
			sscop_bgak_error,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_bgak_error,	/* SOS_OUTRESYN */
			sscop_bgak_error,	/* SOS_INRESYN */
			sscop_bgak_error,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_noop,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* BGREJ PDU */
static void	(*sscop_bgrej_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_bgrej_error,	/* SOS_IDLE */
			sscop_bgrej_outconn,	/* SOS_OUTCONN */
			sscop_bgrej_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_bgrej_outresyn,	/* SOS_OUTRESYN */
			sscop_bgrej_ready,	/* SOS_INRESYN */
			sscop_bgrej_outresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_bgrej_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* END PDU */
static void	(*sscop_end_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_end_idle,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_end_inconn,	/* SOS_INCONN */
			sscop_end_outdisc,	/* SOS_OUTDISC */
			sscop_end_outresyn,	/* SOS_OUTRESYN */
			sscop_end_ready,	/* SOS_INRESYN */
			sscop_end_conresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_end_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* ENDAK PDU */
static void	(*sscop_endak_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_noop,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_endak_inconn,	/* SOS_INCONN */
			sscop_endak_outdisc,	/* SOS_OUTDISC */
			sscop_endak_outresyn,	/* SOS_OUTRESYN */
			sscop_endak_ready,	/* SOS_INRESYN */
			sscop_endak_outresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_endak_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* RS PDU */
static void	(*sscop_rs_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_rs_idle,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_rs_error,		/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_rs_outresyn,	/* SOS_OUTRESYN */
			sscop_noop,		/* SOS_INRESYN */
			sscop_noop,		/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_rs_ready,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* RSAK PDU */
static void	(*sscop_rsak_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_rsak_idle,	/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_rsak_error,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_rsak_outresyn,	/* SOS_OUTRESYN */
			sscop_rsak_error,	/* SOS_INRESYN */
			sscop_rsak_conresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_rsak_error,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* SD PDU */
static void	(*sscop_sd_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_sd_idle,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_sd_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_sd_ready,		/* SOS_OUTRESYN */
			sscop_sd_inresyn,	/* SOS_INRESYN */
			sscop_sd_conresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_sd_ready,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* SDP PDU */
static void	(*sscop_sdp_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_sd_idle,		/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_sd_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_sdp_ready,	/* SOS_OUTRESYN */
			sscop_sd_inresyn,	/* SOS_INRESYN */
			sscop_sd_conresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_sdp_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* POLL PDU */
static void	(*sscop_poll_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_poll_idle,	/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_poll_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_poll_ready,	/* SOS_OUTRESYN */
			sscop_poll_inresyn,	/* SOS_INRESYN */
			sscop_poll_conresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_poll_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* STAT PDU */
static void	(*sscop_stat_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_stat_idle,	/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_stat_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_noop,		/* SOS_OUTRESYN */
			sscop_stat_ready,	/* SOS_INRESYN */
			sscop_stat_conresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_stat_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* USTAT PDU */
static void	(*sscop_ustat_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_ustat_idle,	/* SOS_IDLE */
			sscop_noop,		/* SOS_OUTCONN */
			sscop_ustat_inconn,	/* SOS_INCONN */
			sscop_noop,		/* SOS_OUTDISC */
			sscop_noop,		/* SOS_OUTRESYN */
			sscop_ustat_ready,	/* SOS_INRESYN */
			sscop_ustat_conresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_ustat_ready,	/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* UD PDU */
static void	(*sscop_ud_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_ud_all,		/* SOS_IDLE */
			sscop_ud_all,		/* SOS_OUTCONN */
			sscop_ud_all,		/* SOS_INCONN */
			sscop_ud_all,		/* SOS_OUTDISC */
			sscop_ud_all,		/* SOS_OUTRESYN */
			sscop_ud_all,		/* SOS_INRESYN */
			sscop_ud_all,		/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_ud_all,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};

/* MD PDU */
static void	(*sscop_md_tab[SOS_NUMSTATES])
				(struct sscop *, KBuffer *, caddr_t) = {
			NULL,			/* SOS_INST */
			sscop_md_all,		/* SOS_IDLE */
			sscop_md_all,		/* SOS_OUTCONN */
			sscop_md_all,		/* SOS_INCONN */
			sscop_md_all,		/* SOS_OUTDISC */
			sscop_md_all,		/* SOS_OUTRESYN */
			sscop_md_all,		/* SOS_INRESYN */
			sscop_md_all,		/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_md_all,		/* SOS_READY */
			sscop_noop		/* SOS_TERM */
};


/*
 * PDU type lookup table
 */
void	(*(*sscop_qsaal_pdutab[]))
				(struct sscop *, KBuffer *, caddr_t) = {
		NULL,
		sscop_bgn_tab,
		sscop_bgak_tab,
		sscop_end_tab,
		sscop_endak_tab,
		sscop_rs_tab,
		sscop_rsak_tab,
		sscop_bgrej_tab,
		sscop_sd_tab,
		sscop_sdp_tab,
		sscop_poll_tab,
		sscop_stat_tab,
		sscop_ustat_tab,
		sscop_ud_tab,
		sscop_md_tab,
		NULL
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
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(bp->bgn_nmr));

	/*
	 * Notify user of connection establishment
	 */
	if (sop->so_flags & SOF_REESTAB) {
		KB_FREEALL(m);
		STACK_CALL(SSCOP_ESTABLISH_IND, sop->so_upper, sop->so_toku, 
			sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
		if (err) {
			sscop_abort(sop, "sscop_bgn_outconn: stack memory\n");
			return;
		}
		sop->so_flags &= ~SOF_REESTAB;
	} else {
		STACK_CALL(SSCOP_ESTABLISH_CNF, sop->so_upper, sop->so_toku, 
			sop->so_connvc, (intptr_t)m, 0, err);
		if (err) {
			KB_FREEALL(m);
			sscop_abort(sop, "sscop_bgn_outconn: stack memory\n");
			return;
		}
	}

	/*
	 * Return an ACK to peer
	 */
	(void) sscop_send_bgak(sop);

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Reset receiver variables
	 */
	qsaal1_reset_rcvr(sop);
	
	/*
	 * Start polling timer
	 */
	sscop_set_poll(sop);

	/*
	 * Start lost poll/stat timer
	 */
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
 * END PDU / SOS_OUTRESYN Processor
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
sscop_end_outresyn(sop, m, trlr)
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
		sop->so_connvc, (intptr_t)m, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "sscop_end_outresyn: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * END PDU / SOS_CONRESYN Processor
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
sscop_end_conresyn(sop, m, trlr)
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
	 * Free up buffers
	 */
	KB_FREEALL(m);

	/*
	 * Acknowledge END
	 */
	(void) sscop_send_endak(sop);

	/*
	 * Notify user of connection termination
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "sscop_end_conresyn: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

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
	 * Stop poll timer
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Stop lost poll/stat timer
	 */
	sop->so_timer[SSCOP_T_NORESP] = 0;

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
		sop->so_connvc, (intptr_t)m, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "sscop_end_ready: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * ENDAK PDU / SOS_OUTRESYN Processor
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
sscop_endak_outresyn(sop, m, trlr)
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
		sscop_abort(sop, "sscop_endak_outresyn: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

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
	int		err;

	/*
	 * Notify user of resynchronization
	 */
	STACK_CALL(SSCOP_RESYNC_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, (intptr_t)m, 0, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "sscop_rs_outresyn: stack memory\n");
		return;
	}

	/*
	 * Reset receiver state variables
	 */
	qsaal1_reset_rcvr(sop);

	/*
	 * Wait for both peer and user responses
	 */
	sop->so_state = SOS_CONRESYN;

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
	int		err;

	/*
	 * Notify user of resynchronization
	 */
	STACK_CALL(SSCOP_RESYNC_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, (intptr_t)m, 0, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "sscop_rs_ready: stack memory\n");
		return;
	}

	/*
	 * Reset receiver state variables
	 */
	qsaal1_reset_rcvr(sop);

	/*
	 * Wait for user response
	 */
	sop->so_state = SOS_INRESYN;

	return;
}


/*
 * RSAK PDU / SOS_CONRESYN Processor
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
sscop_rsak_conresyn(sop, m, trlr)
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
	 * Free buffers
	 */
	KB_FREEALL(m);

	/*
	 * Notify user of resynchronization completion
	 */
	STACK_CALL(SSCOP_RESYNC_CNF, sop->so_upper, sop->so_toku, 
		sop->so_connvc, 0, 0, err);
	if (err) {
		sscop_abort(sop, "sscop_rsak_conresyn: stack memory\n");
		return;
	}

	/*
	 * Start the polling timer
	 */
	sscop_set_poll(sop);

	/*
	 * Start lost poll/stat timer
	 */
	sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;

	/*
	 * Continue waiting for user response
	 */
	sop->so_state = SOS_INRESYN;

	/*
	 * See if transmit queues need servicing
	 */
	if (sop->so_flags & SOF_XMITSRVC)
		sscop_service_xmit(sop);

	return;
}


/*
 * SD PDU / SOS_INRESYN Processor
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
sscop_sd_inresyn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop poll timer
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Stop lost poll/stat timer
	 */
	sop->so_timer[SSCOP_T_NORESP] = 0;

	/*
	 * Record error condition
	 */
	sscop_sd_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "sscop_sd_inresyn: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

	/*
	 * Go back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * SD PDU / SOS_CONRESYN Processor
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
sscop_sd_conresyn(sop, m, trlr)
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
	 * Record error condition
	 */
	sscop_sd_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "sscop_sd_conresyn: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

	/*
	 * Go back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * SD/SDP PDU Common Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU user data buffer chain
 *	trlr	pointer to PDU trailer
 *	type	PDU type (SD or SDP)
 *
 * Returns:
 *	none
 *
 */
static void
sscop_sd_process(sop, m, trlr, type)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
	int		type;
{
	struct sd_pdu	*sp;
	struct sdp_pdu	*spp;
	struct poll_pdu	poll;
	struct pdu_hdr	*php;
	KBuffer		*n;
	sscop_seq	ns, nps;
	int		err, space;

	/*
	 * Get PDU sequence number(s)
	 */
	if (type == PT_SD) {
		sp = (struct sd_pdu *)trlr;
		SEQ_SET(ns, ntohl(sp->sd_ns));
		SEQ_SET(nps, 0);
	} else {
		spp = (struct sdp_pdu *)trlr;
		SEQ_SET(ns, ntohl(spp->sdp_ns));
		SEQ_SET(nps, ntohl(spp->sdp_nps));
	}

	/*
	 * Ensure that the sequence number fits within the window
	 */
	if (SEQ_GEQ(ns, sop->so_rcvmax, sop->so_rcvnext)) {
		KB_FREEALL(m);
		return;
	}

	/*
	 * If this is the next in-sequence PDU, hand it to user
	 */
	if (ns == sop->so_rcvnext) {
		STACK_CALL(SSCOP_DATA_IND, sop->so_upper, sop->so_toku, 
			sop->so_connvc, (intptr_t)m, ns, err);
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
			if (type == PT_SDP)
				goto dopoll;
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
				sop->so_connvc, (intptr_t)php->ph_buf,
				php->ph_ns, err);
			if (err) {
				/*
				 * Should never happen, but...
				 */
				KB_FREEALL(php->ph_buf);
				sscop_abort(sop,
					"sscop_sd_process: stack memory\n");
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
		 * Finished with data...see if we need to poll
		 */
		if (type == PT_SDP)
			goto dopoll;
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
		 * Reestablish a new connection
		 */
		qsaal1_reestablish(sop);

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

		if (type == PT_SDP)
			goto dopoll;
		return;
	}

	/*
	 * Are we beyond the high-water mark??
	 */
	if (SEQ_GT(ns, sop->so_rcvhigh, sop->so_rcvnext)) {
		/*
		 * Yes, then there's a missing PDU, so inform the transmitter
		 */
		if (type == PT_SD)
			(void) sscop_send_ustat(sop, ns);

		/*
		 * Update high-water mark
		 */
		sop->so_rcvhigh = SEQ_ADD(ns, 1);
	}

	if (type == PT_SD)
		return;

dopoll:
	/*
	 * Do the "poll" part of an SDP PDU
	 */
	poll.poll_nps = htonl(nps);
	poll.poll_ns = htonl((PT_POLL << PT_TYPE_SHIFT) | ns);
	sscop_poll_ready(sop, NULL, (caddr_t)&poll);
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
	/*
	 * Just call common SD/SDP processor
	 */
	sscop_sd_process(sop, m, trlr, PT_SD);

	return;
}


/*
 * SDP PDU / SOS_READY Processor
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
sscop_sdp_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	/*
	 * Just call common SD/SDP processor
	 */
	sscop_sd_process(sop, m, trlr, PT_SDP);

	return;
}


/*
 * POLL PDU / SOS_INRESYN Processor
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
sscop_poll_inresyn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop poll timer
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Stop lost poll/stat timer
	 */
	sop->so_timer[SSCOP_T_NORESP] = 0;

	/*
	 * Report protocol error
	 */
	sscop_poll_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "sscop_poll_inresyn: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * POLL PDU / SOS_CONRESYN Processor
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
sscop_poll_conresyn(sop, m, trlr)
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
	 * Record error condition
	 */
	sscop_poll_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "sscop_poll_conresyn: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

	/*
	 * Go back to idle state
	 */
	sop->so_state = SOS_IDLE;

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

	pp->poll_ns = ntohl(pp->poll_ns);

	/*
	 * If the poll sequence number is less than highest number
	 * we've already seen, something's wrong - so attempt to
	 * reestablish a new connection.
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
		 * Reestablish a new connection
		 */
		qsaal1_reestablish(sop);

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


/*
 * STAT PDU / SOS_CONRESYN Processor
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
sscop_stat_conresyn(sop, m, trlr)
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
	 * Record error condition
	 */
	sscop_stat_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "sscop_stat_conresyn: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

	/*
	 * Go back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * USTAT PDU / SOS_CONRESYN Processor
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
sscop_ustat_conresyn(sop, m, trlr)
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
	 * Record error condition
	 */
	sscop_ustat_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku, 
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "sscop_ustat_conresyn: stack memory\n");
		return;
	}

	/*
	 * Clear connection data
	 */
	qsaal1_clear_connection(sop);

	/*
	 * Go back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}

