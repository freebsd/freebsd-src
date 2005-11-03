/*-
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
 *	@(#) $FreeBSD: src/sys/netatm/uni/sscop_var.h,v 1.9 2005/01/07 01:45:37 imp Exp $
 *
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCOP protocol control blocks
 *
 */

#ifndef _UNI_SSCOP_VAR_H
#define _UNI_SSCOP_VAR_H

/*
 * Structure containing information for each SSCOP connection.
 */
struct sscop {
	struct sscop	*so_next;	/* Next connection in chain */
	u_char		so_state;	/* Connection state (see below) */
	u_short		so_flags;	/* Connection flags (see below) */
	enum sscop_vers	so_vers;	/* SSCOP version */

	/* Transmitter variables */
	sscop_seq	so_send;	/* VT(S) - next SD to send */
	sscop_seq	so_sendmax;	/* VT(MS) - max SD to send + 1 */
	sscop_seq	so_ack;		/* VT(A) - next expected ack */
	sscop_seq	so_pollsend;	/* VT(PS) - last POLL sent */
	sscop_seq	so_pollack;	/* VT(PA) - next expected STAT */
	short		so_polldata;	/* VT(PD) - SD's sent between POLLs */
	short		so_connctl;	/* VT(CC) - un-ack'd BGN,END,ER,RS */
	u_char		so_sendconn;	/* VT(SQ) - last BGN,ER,RS sent */

	/* Receiver variables */
	sscop_seq	so_rcvnext;	/* VR(R) - next SD to receive */
	sscop_seq	so_rcvhigh;	/* VR(H) - next highest SD to receive */
	sscop_seq	so_rcvmax;	/* VR(MR) - max SD to receive + 1 */
	u_char		so_rcvconn;	/* VR(SQ) - last BGN,ER,RS received */

	/* PDU queues */
	KBuffer		*so_xmit_hd;	/* SD transmission queue head */
	KBuffer		*so_xmit_tl;	/* SD transmission queue tail */
	struct pdu_hdr	*so_rexmit_hd;	/* SD retransmission queue head */
	struct pdu_hdr	*so_rexmit_tl;	/* SD retransmission queue head */
	struct pdu_hdr	*so_pack_hd;	/* SD pending ack queue head */
	struct pdu_hdr	*so_pack_tl;	/* SD pending ack queue tail */
	struct pdu_hdr	*so_recv_hd;	/* SD receive queue head */
	struct pdu_hdr	*so_recv_tl;	/* SD receive queue tail */

	/* Connection parameters */
	struct sscop_parms so_parm;	/* Connection parameters */

	/* Timers */
	u_short		so_timer[SSCOP_T_NUM];	/* Connection timers */

	/* Stack variables */
	Atm_connvc	*so_connvc;	/* Connection vcc for this stack */
	void		*so_toku;	/* Stack upper layer's token */
	void		*so_tokl;	/* Stack lower layer's token */
	void		(*so_upper)	/* Stack upper layer's interface */
				(int, void *, intptr_t, intptr_t);
	void		(*so_lower)	/* Stack lower layer's interface */
				(int, void *, intptr_t, intptr_t);
	u_short		so_headout;	/* Output buffer headroom */
};

/*
 * Connection States
 *
 * Notes:
 *	#  - state valid only for Q.SAAL1
 *      ## - state valid only for Q.2110
 */
#define	SOS_INST	0		/* Instantiated, waiting for INIT */
#define	SOS_IDLE	1		/* Idle connection */
#define	SOS_OUTCONN	2		/* Outgoing connection pending */
#define	SOS_INCONN	3		/* Incoming connection pending */
#define	SOS_OUTDISC	4		/* Outgoing disconnection pending */
#define	SOS_OUTRESYN	5		/* Outgoing resynchronization pending */
#define	SOS_INRESYN	6		/* Incoming resynchronization pending */
#define	SOS_CONRESYN	7		/* Concurrent resynch pending (#) */
#define	SOS_OUTRECOV	7		/* Outgoing recovery pending (##) */
#define	SOS_RECOVRSP	8		/* Recovery response pending (##) */
#define	SOS_INRECOV	9		/* Incoming recovery pending (##) */
#define	SOS_READY	10		/* Data transfer ready */
#define	SOS_TERM	11		/* Waiting for TERM */

#define	SOS_MAXSTATE	11		/* Maximum state value */
#define	SOS_NUMSTATES	(SOS_MAXSTATE+1)/* Number of states */

/*
 * Connection Flags
 */
#define	SOF_NOCLRBUF	0x0001		/* Clear buffers = no */
#define	SOF_REESTAB	0x0002		/* SSCOP initiated reestablishment */
#define	SOF_XMITSRVC	0x0004		/* Transmit queues need servicing */
#define	SOF_KEEPALIVE	0x0008		/* Polling in transient phase */
#define	SOF_ENDSSCOP	0x0010		/* Last END PDU, SOURCE=SSCOP */
#define	SOF_NOCREDIT	0x0020		/* Transmit window closed */


/*
 * SSCOP statistics
 */
struct sscop_stat {
	u_long		sos_connects;	/* Connection instances */
	u_long		sos_aborts;	/* Connection aborts */
	u_long		sos_maa_error[MAA_ERROR_COUNT]; /* Management errors */
};

#ifdef _KERNEL

#include <vm/uma.h>

/*
 * Global function declarations
 */
	/* sscop.c */
int		sscop_start(void);
int		sscop_stop(void);
void		sscop_maa_error(struct sscop *, int);
void		sscop_abort(struct sscop *, char *);

	/* sscop_lower.c */
void		sscop_lower(int, void *, intptr_t, intptr_t);
void		sscop_aa_noop_0(struct sscop *, intptr_t, intptr_t);
void		sscop_aa_noop_1(struct sscop *, intptr_t, intptr_t);
void		sscop_init_inst(struct sscop *, intptr_t, intptr_t);
void		sscop_term_all(struct sscop *, intptr_t, intptr_t);

	/* sscop_pdu.c */
int		sscop_send_bgn(struct sscop *, int);
int		sscop_send_bgak(struct sscop *);
int		sscop_send_bgrej(struct sscop *);
int		sscop_send_end(struct sscop *, int);
int		sscop_send_endak(struct sscop *);
int		sscop_send_rs(struct sscop *);
int		sscop_send_rsak(struct sscop *);
int		sscop_send_er(struct sscop *);
int		sscop_send_erak(struct sscop *);
int		sscop_send_poll(struct sscop *);
int		sscop_send_stat(struct sscop *, sscop_seq);
int		sscop_send_ustat(struct sscop *, sscop_seq);
int		sscop_send_ud(struct sscop *, KBuffer *);
void		sscop_pdu_print(const struct sscop *,
		    const KBuffer *, const char *);

	/* sscop_sigaa.c */
void		sscop_estreq_idle(struct sscop *, intptr_t, intptr_t);
void		sscop_estrsp_inconn(struct sscop *, intptr_t, intptr_t);
void		sscop_relreq_outconn(struct sscop *, intptr_t, intptr_t);
void		sscop_relreq_inconn(struct sscop *, intptr_t, intptr_t);
void		sscop_relreq_ready(struct sscop *, intptr_t, intptr_t);
void		sscop_datreq_ready(struct sscop *, intptr_t, intptr_t);
void		sscop_udtreq_all(struct sscop *, intptr_t, intptr_t);

	/* sscop_sigcpcs.c */
void		sscop_noop(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgn_idle(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgn_outdisc(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgn_outresyn(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgn_inresyn(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgak_error(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgak_idle(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgak_outconn(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgrej_error(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgrej_outconn(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgrej_inconn(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgrej_outresyn(struct sscop *, KBuffer *, caddr_t);
void		sscop_bgrej_ready(struct sscop *, KBuffer *, caddr_t);
void		sscop_end_idle(struct sscop *, KBuffer *, caddr_t);
void		sscop_end_inconn(struct sscop *, KBuffer *, caddr_t);
void		sscop_end_outdisc(struct sscop *, KBuffer *, caddr_t);
void		sscop_endak_error(struct sscop *, KBuffer *, caddr_t);
void		sscop_endak_inconn(struct sscop *, KBuffer *, caddr_t);
void		sscop_endak_outdisc(struct sscop *, KBuffer *, caddr_t);
void		sscop_endak_ready(struct sscop *, KBuffer *, caddr_t);
void		sscop_rs_error(struct sscop *, KBuffer *, caddr_t);
void		sscop_rs_idle(struct sscop *, KBuffer *, caddr_t);
void		sscop_rsak_error(struct sscop *, KBuffer *, caddr_t);
void		sscop_rsak_idle(struct sscop *, KBuffer *, caddr_t);
void		sscop_rsak_outresyn(struct sscop *, KBuffer *, caddr_t);
void		sscop_sd_error(struct sscop *, KBuffer *, caddr_t);
void		sscop_sd_idle(struct sscop *, KBuffer *, caddr_t);
void		sscop_sd_inconn(struct sscop *, KBuffer *, caddr_t);
void		sscop_poll_error(struct sscop *, KBuffer *, caddr_t);
void		sscop_poll_idle(struct sscop *, KBuffer *, caddr_t);
void		sscop_poll_inconn(struct sscop *, KBuffer *, caddr_t);
void		sscop_stat_error(struct sscop *, KBuffer *, caddr_t);
void		sscop_stat_idle(struct sscop *, KBuffer *, caddr_t);
void		sscop_stat_inconn(struct sscop *, KBuffer *, caddr_t);
void		sscop_stat_ready(struct sscop *, KBuffer *, caddr_t);
void		sscop_ustat_error(struct sscop *, KBuffer *, caddr_t);
void		sscop_ustat_idle(struct sscop *, KBuffer *, caddr_t);
void		sscop_ustat_inconn(struct sscop *, KBuffer *, caddr_t);
void		sscop_ustat_ready(struct sscop *, KBuffer *, caddr_t);
void		sscop_ud_all(struct sscop *, KBuffer *, caddr_t);
void		sscop_md_all(struct sscop *, KBuffer *, caddr_t);

	/* sscop_subr.c */
KBuffer *	sscop_stat_getelem(KBuffer *, sscop_seq *);
struct pdu_hdr *sscop_pack_locate(struct sscop *, sscop_seq);
void		sscop_pack_free(struct sscop *, sscop_seq);
void		sscop_rexmit_insert(struct sscop *, struct pdu_hdr *);
void		sscop_rexmit_unlink(struct sscop *, struct pdu_hdr *);
void		sscop_xmit_drain(struct sscop *);
int		sscop_recv_insert(struct sscop *, struct pdu_hdr *);
void		sscop_rcvr_drain(struct sscop *);
void		sscop_service_xmit(struct sscop *);
int		sscop_is_rexmit(struct sscop *, u_char);
void		sscop_set_poll(struct sscop *);

	/* sscop_timer.c */
void		sscop_timeout(struct atm_time *);

	/* sscop_upper.c */
void		sscop_upper(int, void *, intptr_t, intptr_t);

	/* q2110_sigaa.c */

	/* q2110_sigcpcs.c */

	/* q2110_subr.c */
void		q2110_clear_xmit(struct sscop *);
void		q2110_init_state(struct sscop *);
void		q2110_prep_retrieve(struct sscop *);
void		q2110_prep_recovery(struct sscop *);
void		q2110_deliver_data(struct sscop *);
void		q2110_error_recovery(struct sscop *);

	/* qsaal1_sigaa.c */

	/* qsaal1_sigcpcs.c */

	/* qsaal1_subr.c */
void		qsaal1_reestablish(struct sscop *);
void		qsaal1_reset_xmit(struct sscop *);
void		qsaal1_reset_rcvr(struct sscop *);
void		qsaal1_clear_connection(struct sscop *);


/*
 * External variables
 */
extern uma_zone_t	sscop_zone;
extern int		sscop_vccnt;
extern struct sscop	*sscop_head;
extern struct sscop_stat	sscop_stat;
extern struct atm_time	sscop_timer;
extern void		(*(*sscop_qsaal_aatab[]))
				(struct sscop *, intptr_t, intptr_t);
extern void		(*(*sscop_q2110_aatab[]))
				(struct sscop *, intptr_t, intptr_t);
extern void		(*(*sscop_qsaal_pdutab[]))
				(struct sscop *, KBuffer *, caddr_t);
extern void		(*(*sscop_q2110_pdutab[]))
				(struct sscop *, KBuffer *, caddr_t);

#endif	/* _KERNEL */

#endif	/* _UNI_SSCOP_VAR_H */
