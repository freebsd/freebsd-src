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
 * ITU-T Q.SAAL1 - Process AA-signals (SAP_SSCOP)
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static void	sscop_estreq_ready __P((struct sscop *, int, int));
static void	sscop_datreq_outconn __P((struct sscop *, int, int));
static void	sscop_resreq_ready __P((struct sscop *, int, int));
static void	sscop_resrsp_inresyn __P((struct sscop *, int, int));
static void	sscop_resrsp_conresyn __P((struct sscop *, int, int));


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
			NULL,			/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
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
			sscop_term_all,		/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
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
			sscop_estreq_ready,	/* SOS_OUTDISC */
			sscop_estreq_ready,	/* SOS_OUTRESYN */
			sscop_estreq_ready,	/* SOS_INRESYN */
			sscop_estreq_ready,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_estreq_ready,	/* SOS_READY */
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
			NULL,			/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_aa_noop_1,	/* SOS_READY */
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
			sscop_relreq_ready,	/* SOS_INRESYN */
			sscop_relreq_outconn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_relreq_ready,	/* SOS_READY */
			sscop_aa_noop_1		/* SOS_TERM */
};

/* SSCOP_DATA_REQ */
static void	(*sscop_datreq_tab[SOS_NUMSTATES])
				__P((struct sscop *, int, int)) = {
			NULL,			/* SOS_INST */
			NULL,			/* SOS_IDLE */
			sscop_datreq_outconn,	/* SOS_OUTCONN */
			NULL,			/* SOS_INCONN */
			NULL,			/* SOS_OUTDISC */
			NULL,			/* SOS_OUTRESYN */
			sscop_datreq_ready,	/* SOS_INRESYN */
			NULL,			/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
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
			NULL,			/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
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
			sscop_resrsp_conresyn,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
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
			sscop_udtreq_all,	/* SOS_CONRESYN */
			NULL,			/* invalid */
			NULL,			/* invalid */
			sscop_udtreq_all,	/* SOS_READY */
			sscop_aa_noop_1		/* SOS_TERM */
};


/*
 * Stack command lookup table
 */
void	(*(*sscop_qsaal_aatab[SSCOP_CMD_SIZE]))
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
		NULL,
		sscop_udtreq_tab,
		NULL,
		NULL,
		NULL,
		NULL
};


/*
 * SSCOP_ESTABLISH_REQ / SOS_READY Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing SSCOP-UU data
 *	arg2	buffer release parameter
 *
 * Returns:
 *	none
 *
 */
static void
sscop_estreq_ready(sop, arg1, arg2)
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
	 * We currently only support BR=YES
	 */
	if (arg2 != SSCOP_BR_YES) {
		sscop_abort(sop, "sscop: BR != YES\n");
		return;
	}

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
	 * Initialize receiver window
	 */
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);

	/*
	 * Send first BGN PDU
	 */
	sop->so_connctl = 1;
	(void) sscop_send_bgn(sop, SSCOP_SOURCE_USER);

	/*
	 * Reset transmitter state
	 */
	qsaal1_reset_xmit(sop);

	/*
	 * Set retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

	/*
	 * Wait for BGAK
	 */
	sop->so_state = SOS_OUTCONN;

	return;
}


/*
 * SSCOP_DATA_REQ / SOS_OUTCONN Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing assured user data
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
static void
sscop_datreq_outconn(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{
	KBuffer		*m = (KBuffer *)arg1;

	/*
	 * We must have a buffer (even if it contains no data)
	 */
	if (m == NULL) {
		sscop_abort(sop, "sscop_datreq_outconn: no buffer\n");
		return;
	}

	/*
	 * Only accept data here if in the middle of an SSCOP-initiated 
	 * session reestablishment
	 */
	if ((sop->so_flags & SOF_REESTAB) == 0) {
		KB_FREEALL(m);
		sscop_abort(sop, "sscop_datreq_outconn: data not allowed\n");
		return;
	}

	/*
	 * Place data at end of transmission queue
	 */
	KB_QNEXT(m) = NULL;
	if (sop->so_xmit_hd == NULL)
		sop->so_xmit_hd = m;
	else
		KB_QNEXT(sop->so_xmit_tl) = m;
	sop->so_xmit_tl = m;

	/*
	 * Note that the transmit queues need to be serviced
	 */
	sop->so_flags |= SOF_XMITSRVC;

	return;
}


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
	 * Stop poll timer
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Stop lost poll/stat timer
	 */
	sop->so_timer[SSCOP_T_NORESP] = 0;

	/*
	 * Send first RS PDU
	 */
	sop->so_connctl = 1;
	(void) sscop_send_rs(sop);

	/*
	 * Reset transmitter state
	 */
	qsaal1_reset_xmit(sop);

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
	 * Send RSAK PDU
	 */
	(void) sscop_send_rsak(sop);

	/*
	 * Back to data transfer state
	 */
	sop->so_state = SOS_READY;

	return;
}


/*
 * SSCOP_RESYNC_RSP / SOS_CONRESYN Command Processor
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
sscop_resrsp_conresyn(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * Send RSAK PDU
	 */
	(void) sscop_send_rsak(sop);

	/*
	 * Back to waiting for peer's RSAK
	 */
	sop->so_state = SOS_OUTRESYN;

	return;
}

