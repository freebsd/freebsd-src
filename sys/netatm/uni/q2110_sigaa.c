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
 * ITU-T Q.2110 - Process AA-signals (SAP_SSCOP)
 *
 */

#include <netatm/kern_include.h>

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static void	sscop_resreq_ready __P((struct sscop *, int, int));
static void	sscop_resrsp_inresyn __P((struct sscop *, int, int));
static void	sscop_recrsp_recovrsp __P((struct sscop *, int, int));
static void	sscop_recrsp_inrecov __P((struct sscop *, int, int));


/*
 * Stack command state lookup tables
 */
/* SSCOP_INIT */
static void	(*sscop_init_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			sscop_init_inst,	/* SOS_INST */
			NULL,			/* SOS_IDLE */
			NULL,			/* SOS_OUTCONN */
			NULL,			/* SOS_INCONN */
			NULL,			/* SOS_OUTDISC */
			NULL,			/* SOS_OUTRESYN */
			NULL,			/* SOS_INRESYN */
			NULL,			/* SOS_OUTRECOV */
			NULL,			/* SOS_RECOVRSP */
			NULL,			/* SOS_INRECOV */
			NULL,			/* SOS_READY */
			NULL			/* SOS_TERM */
};

/* SSCOP_TERM */
static void	(*sscop_term_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			sscop_term_all,		/* SOS_INST */
			sscop_term_all,		/* SOS_IDLE */
			sscop_term_all,		/* SOS_OUTCONN */
			sscop_term_all,		/* SOS_INCONN */
			sscop_term_all,		/* SOS_OUTDISC */
			sscop_term_all,		/* SOS_OUTRESYN */
			sscop_term_all,		/* SOS_INRESYN */
			sscop_term_all,		/* SOS_OUTRECOV */
			sscop_term_all,		/* SOS_RECOVRSP */
			sscop_term_all,		/* SOS_INRECOV */
			sscop_term_all,		/* SOS_READY */
			sscop_term_all		/* SOS_TERM */
};

/* SSCOP_ESTABLISH_REQ */
static void	(*sscop_estreq_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			sscop_estreq_idle,	/* SOS_IDLE */
			NULL,			/* SOS_OUTCONN */
			NULL,			/* SOS_INCONN */
			sscop_estreq_idle,	/* SOS_OUTDISC */
			NULL,			/* SOS_OUTRESYN */
			NULL,			/* SOS_INRESYN */
			NULL,			/* SOS_OUTRECOV */
			NULL,			/* SOS_RECOVRSP */
			NULL,			/* SOS_INRECOV */
			NULL,			/* SOS_READY */
			sscop_aa_noop_1		/* SOS_TERM */
};

/* SSCOP_ESTABLISH_RSP */
static void	(*sscop_estrsp_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			NULL,			/* SOS_IDLE */
			NULL,			/* SOS_OUTCONN */
			sscop_estrsp_inconn,	/* SOS_INCONN */
			NULL,			/* SOS_OUTDISC */
			NULL,			/* SOS_OUTRESYN */
			NULL,			/* SOS_INRESYN */
			NULL,			/* SOS_OUTRECOV */
			NULL,			/* SOS_RECOVRSP */
			NULL,			/* SOS_INRECOV */
			NULL,			/* SOS_READY */
			sscop_aa_noop_1		/* SOS_TERM */
};

/* SSCOP_RELEASE_REQ */
static void	(*sscop_relreq_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			NULL,			/* SOS_IDLE */
			sscop_relreq_outconn,	/* SOS_OUTCONN */
			sscop_relreq_inconn,	/* SOS_INCONN */
			NULL,			/* SOS_OUTDISC */
			sscop_relreq_outconn,	/* SOS_OUTRESYN */
			sscop_relreq_outconn,	/* SOS_INRESYN */
			sscop_relreq_ready,	/* SOS_OUTRECOV */
			sscop_relreq_outconn,	/* SOS_RECOVRSP */
			sscop_relreq_outconn,	/* SOS_INRECOV */
			sscop_relreq_ready,	/* SOS_READY */
			sscop_aa_noop_1		/* SOS_TERM */
};

/* SSCOP_DATA_REQ */
static void	(*sscop_datreq_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			NULL,			/* SOS_IDLE */
			NULL,			/* SOS_OUTCONN */
			NULL,			/* SOS_INCONN */
			NULL,			/* SOS_OUTDISC */
			NULL,			/* SOS_OUTRESYN */
			NULL,			/* SOS_INRESYN */
			sscop_aa_noop_1,	/* SOS_OUTRECOV */
			NULL,			/* SOS_RECOVRSP */
			NULL,			/* SOS_INRECOV */
			sscop_datreq_ready,	/* SOS_READY */
			sscop_aa_noop_1		/* SOS_TERM */
};

/* SSCOP_RESYNC_REQ */
static void	(*sscop_resreq_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			NULL,			/* SOS_IDLE */
			NULL,			/* SOS_OUTCONN */
			NULL,			/* SOS_INCONN */
			NULL,			/* SOS_OUTDISC */
			NULL,			/* SOS_OUTRESYN */
			NULL,			/* SOS_INRESYN */
			sscop_resreq_ready,	/* SOS_OUTRECOV */
			sscop_resreq_ready,	/* SOS_RECOVRSP */
			sscop_resreq_ready,	/* SOS_INRECOV */
			sscop_resreq_ready,	/* SOS_READY */
			sscop_aa_noop_1		/* SOS_TERM */
};

/* SSCOP_RESYNC_RSP */
static void	(*sscop_resrsp_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			NULL,			/* SOS_IDLE */
			NULL,			/* SOS_OUTCONN */
			NULL,			/* SOS_INCONN */
			NULL,			/* SOS_OUTDISC */
			NULL,			/* SOS_OUTRESYN */
			sscop_resrsp_inresyn,	/* SOS_INRESYN */
			NULL,			/* SOS_OUTRECOV */
			NULL,			/* SOS_RECOVRSP */
			NULL,			/* SOS_INRECOV */
			NULL,			/* SOS_READY */
			sscop_aa_noop_0		/* SOS_TERM */
};

/* SSCOP_RECOVER_RSP */
static void	(*sscop_recrsp_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			NULL,			/* SOS_IDLE */
			NULL,			/* SOS_OUTCONN */
			NULL,			/* SOS_INCONN */
			NULL,			/* SOS_OUTDISC */
			NULL,			/* SOS_OUTRESYN */
			NULL,			/* SOS_INRESYN */
			NULL,			/* SOS_OUTRECOV */
			sscop_recrsp_recovrsp,	/* SOS_RECOVRSP */
			sscop_recrsp_inrecov,	/* SOS_INRECOV */
			NULL,			/* SOS_READY */
			sscop_aa_noop_0		/* SOS_TERM */
};

/* SSCOP_UNITDATA_REQ */
static void	(*sscop_udtreq_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			sscop_udtreq_all,	/* SOS_IDLE */
			sscop_udtreq_all,	/* SOS_OUTCONN */
			sscop_udtreq_all,	/* SOS_INCONN */
			sscop_udtreq_all,	/* SOS_OUTDISC */
			sscop_udtreq_all,	/* SOS_OUTRESYN */
			sscop_udtreq_all,	/* SOS_INRESYN */
			sscop_udtreq_all,	/* SOS_OUTRECOV */
			sscop_udtreq_all,	/* SOS_RECOVRSP */
			sscop_udtreq_all,	/* SOS_INRECOV */
			sscop_udtreq_all,	/* SOS_READY */
			sscop_aa_noop_1		/* SOS_TERM */
};

/* SSCOP_RETRIEVE_REQ */
static void	(*sscop_retreq_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			NULL,			/* SOS_IDLE */
			NULL,			/* SOS_OUTCONN */
			NULL,			/* SOS_INCONN */
			NULL,			/* SOS_OUTDISC */
			NULL,			/* SOS_OUTRESYN */
			NULL,			/* SOS_INRESYN */
			NULL,			/* SOS_OUTRECOV */
			NULL,			/* SOS_RECOVRSP */
			NULL,			/* SOS_INRECOV */
			NULL,			/* SOS_READY */
			NULL			/* SOS_TERM */
};


/*
 * Stack command lookup table
 */
void	(*(*sscop_q2110_aatab[SSCOP_CMD_SIZE]))
				__P((struct sscop *, int, int)) = {
		NULL,
		sscop_init_tab,
		sscop_term_tab,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		sscop_estreq_tab,
		NULL,
		sscop_estrsp_tab,
		NULL,
		sscop_relreq_tab,
		NULL,
		NULL,
		sscop_datreq_tab,
		NULL,
		sscop_resreq_tab,
		NULL,
		sscop_resrsp_tab,
		NULL,
		NULL,
		sscop_recrsp_tab,
		sscop_udtreq_tab,
		NULL,
		sscop_retreq_tab,
		NULL,
		NULL
};


/*
 * SSCOP_RESYNC_REQ / SOS_READY Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing SSCOP-UU data
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
static void
sscop_resreq_ready(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * We don't support SSCOP-UU data
	 */
	if (arg1 != SSCOP_UU_NULL)
		KB_FREEALL((KBuffer *)arg1);

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/* 
	 * Initialize receiver window
	 */
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);

	/*
	 * Send first RS PDU
	 */
	sop->so_connctl = 1;
	SEQ_INCR(sop->so_sendconn, 1);
	(void) sscop_send_rs(sop);

	/*
	 * Drain transmit and receive queues
	 */
	sscop_xmit_drain(sop);
	sscop_rcvr_drain(sop);

	/*
	 * Set retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

	/*
	 * Wait for RSAK
	 */
	sop->so_state = SOS_OUTRESYN;

	return;
}


/*
 * SSCOP_RESYNC_RSP / SOS_INRESYN Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	unused
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
static void
sscop_resrsp_inresyn(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * Clear transmitter buffers
	 */
	q2110_clear_xmit(sop);

	/* 
	 * Initialize state variables
	 */
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);
	q2110_init_state(sop);

	/*
	 * Send RSAK PDU
	 */
	(void) sscop_send_rsak(sop);

	/*
	 * Start data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
	sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;

	/*
	 * Back to data transfer state
	 */
	sop->so_state = SOS_READY;

	return;
}


/*
 * SSCOP_RECOVER_RSP / SOS_RECOVRSP Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	unused
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
static void
sscop_recrsp_recovrsp(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * Clear transmitter buffers, if not done earlier
	 */
	if (sop->so_flags & SOF_NOCLRBUF)
		q2110_clear_xmit(sop);

	/* 
	 * Initialize state variables
	 */
	q2110_init_state(sop);

	/*
	 * Start data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
	sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;

	/*
	 * Back to data transfer state
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
 * SSCOP_RECOVER_RSP / SOS_INRECOV Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	unused
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
static void
sscop_recrsp_inrecov(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * Clear transmitter buffers, if not done earlier
	 */
	if (sop->so_flags & SOF_NOCLRBUF)
		q2110_clear_xmit(sop);

	/* 
	 * Initialize state variables
	 */
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);
	q2110_init_state(sop);

	/*
	 * Send ERAK PDU
	 */
	(void) sscop_send_erak(sop);

	/*
	 * Start data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
	sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;

	/*
	 * Back to data transfer state
	 */
	sop->so_state = SOS_READY;

	/*
	 * See if transmit queues need servicing
	 */
	if (sop->so_flags & SOF_XMITSRVC)
		sscop_service_xmit(sop);

	return;
}

