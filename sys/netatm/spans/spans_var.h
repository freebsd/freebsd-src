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
 * SPANS Signalling Manager
 * ---------------------------
 *
 * Protocol control blocks
 *
 */

#ifndef _SPANS_SPANS_VAR_H
#define _SPANS_SPANS_VAR_H

#ifdef _KERNEL
/*
 * Constants to indicate the state of the signalling interface
 */
#define SPANS_UNI_UP	1
#define SPANS_UNI_DOWN	-1


/*
 * Structure containing state information for each SPANS protocol
 * instance.  There will be one instance for each ATM device interface
 * using the SPANS signalling manager.
 */
struct	spans {
	struct siginst	sp_inst;	/* Header */
	struct atm_time	sp_time;	/* Timer controls */
	void		(*sp_lower)	/* Lower command handler */
				(int, void *, int, int);
	Atm_connection	*sp_conn;	/* Signalling connection */
	long		sp_s_epoch;	/* Switch epoch */
	long		sp_h_epoch;	/* Host epoch */
	u_int		sp_probe_ct;	/* Status_req msgs unanswered */
	u_int		sp_alloc_vci;	/* Next VCI to allocate */
	u_int		sp_alloc_vpi;	/* Next VPI to allocate */
	u_int		sp_min_vci;	/* Lowest VCI to allocate */
	u_int		sp_max_vci;	/* Highest VCI to allocate */
	struct spanscls	*sp_cls;	/* CLS instance */
};

#define sp_next		sp_inst.si_next
#define sp_pif		sp_inst.si_pif
#define sp_addr		sp_inst.si_addr
#define sp_subaddr	sp_inst.si_subaddr
#define sp_vccq		sp_inst.si_vccq
#define sp_state	sp_inst.si_state
#define sp_ipserv	sp_inst.si_ipserv
#endif	/* _KERNEL */

/*
 * SPANS Protocol States
 */
#define	SPANS_ACTIVE	1		/* Active */
#define	SPANS_DETACH	2		/* Detach in progress */
#define SPANS_INIT	3		/* Initializing */
#define SPANS_PROBE	4		/* Exchanging status info */

#define	SPANS_PROBE_INTERVAL	(ATM_HZ)	/* Interval between SPANS_STAT_REQs */
#define	SPANS_PROBE_THRESH	10		/* Probe time-out threshold */
#define	SPANS_PROBE_ERR_WAIT	(3 * ATM_HZ)	/* Time to wait if send probe fails */


#ifdef _KERNEL
/*
 * SPANS Virtual Channel Connection control block.  All information
 * regarding the state of a SPANS-controlled VCC will be recorded here.
 * There will be one SPANS VCC control block for each SPANS-controlled
 * VCC.
 */
struct spans_vccb {
	struct vccb	vcp_hdr;	/* Generic VCCB */
	u_short		sv_retry;	/* Xmit retry count */
	spans_atm_conn	sv_conn;	/* SPANS connection info */
	spans_resrc sv_spans_qos;	/* QoS for VCC */
	spans_aal	sv_spans_aal;	/* AAL for VCC */
};

#define	sv_type		vcp_hdr.vc_type
#define	sv_proto	vcp_hdr.vc_proto
#define	sv_sstate	vcp_hdr.vc_sstate
#define	sv_ustate	vcp_hdr.vc_ustate
#define	sv_pif		vcp_hdr.vc_pif
#define	sv_nif		vcp_hdr.vc_nif
#define	sv_sigelem	vcp_hdr.vc_sigelem
#define	sv_time		vcp_hdr.vc_time
#define	sv_vpi		vcp_hdr.vc_vpi
#define	sv_vci		vcp_hdr.vc_vci
#define	sv_connvc	vcp_hdr.vc_connvc
#define	sv_ipdus	vcp_hdr.vc_ipdus
#define	sv_opdus	vcp_hdr.vc_opdus
#define	sv_ibytes	vcp_hdr.vc_ibytes
#define	sv_obytes	vcp_hdr.vc_obytes
#define	sv_ierrors	vcp_hdr.vc_ierrors
#define	sv_oerrors	vcp_hdr.vc_oerrors
#define	sv_tstamp	vcp_hdr.vc_tstamp
#define	sv_daddr	sv_conn.daddr
#define	sv_saddr	sv_conn.saddr
#define	sv_dsap		sv_conn.dsap
#define	sv_ssap		sv_conn.ssap

#define SV_MAX_RETRY	3
#define SV_TIMEOUT	(ATM_HZ)

#endif	/* _KERNEL */


/*
 * SPANS VCC Signalling Protocol States
 */
#define	SPANS_VC_NULL		0	/* No state */
#define	SPANS_VC_ACTIVE		1	/* Active */
#define	SPANS_VC_ACT_DOWN	2	/* Active - Interface down */
#define	SPANS_VC_POPEN		3	/* VCC open in progress */
#define	SPANS_VC_R_POPEN	4	/* VCC rmt open in progress */
#define	SPANS_VC_OPEN		5	/* VCC open */
#define	SPANS_VC_CLOSE		6	/* VCC close in progress */
#define	SPANS_VC_ABORT		7	/* VCC abort in progress */
#define	SPANS_VC_FREE		8	/* Waiting for user to free resources */


#ifdef _KERNEL
/*
 * Macro to compare two SPANS addresses.  
 *
 * Returns 0 if the addresses are equal.
 */
#define spans_addr_cmp(a, b)	\
	(bcmp((caddr_t)a, (caddr_t)b, sizeof(struct spans_addr)))

/*
 * Macro to copy a SPANS address from a to b.  
 */
#define spans_addr_copy(a, b)	\
	(KM_COPY((caddr_t)a, (caddr_t)b, sizeof(struct spans_addr)))


/*
 * Timer macros
 */
#define	SPANS_TIMER(s, t)	atm_timeout(&(s)->sp_time, (t), spans_timer)
#define	SPANS_CANCEL(s)	atm_untimeout(&(s)->sp_time)
#define	SPANS_VC_TIMER(v, t)	atm_timeout(&(v)->vc_time, (t), spans_vctimer)
#define	SPANS_VC_CANCEL(v)	atm_untimeout(&(v)->vc_time)


/*
 * Global function declarations
 */
struct ipvcc;

	/* spans_arp.c */
int		spansarp_svcout(struct ipvcc *, struct in_addr *);
int		spansarp_svcin(struct ipvcc *, Atm_addr *, Atm_addr *);
int		spansarp_svcactive(struct ipvcc *);
void		spansarp_vcclose(struct ipvcc *);
void		spansarp_ipact(struct spanscls *);
void		spansarp_ipdact(struct spanscls *);
void		spansarp_stop(void);
void		spansarp_input(struct spanscls *, KBuffer *);
int		spansarp_ioctl(int, caddr_t, caddr_t);

	/* spans_cls.c */
int		spanscls_start(void);
void		spanscls_stop(void);
int		spanscls_attach(struct spans *);
void		spanscls_detach(struct spans *);
void		spanscls_closevc(struct spanscls *, struct t_atm_cause *);

	/* spans_if.c */
int		spans_abort(struct vccb *);
int		spans_free(struct vccb *);

	/* spans_msg.c */
int		spans_send_msg(struct spans *, spans_msg *);
int		spans_send_open_req(struct spans *, struct spans_vccb *);
int		spans_send_open_rsp(struct spans *,
				struct spans_vccb *,
				spans_result);
int		spans_send_close_req(struct spans *,
				struct spans_vccb *);
void		spans_rcv_msg(struct spans *, KBuffer *);

	/* spans_print.c */
void		spans_print_msg(spans_msg *);

	/* spans_proto.c */
void		spans_timer(struct atm_time *);
void		spans_vctimer(struct atm_time *);
void		spans_upper(int, void *, int, int);
void		spans_notify(void *, int, int);

	/* spans_subr.c */
int		spans_open_vcc(struct spans *, Atm_connvc *);
int		spans_close_vcc(struct spans *, struct spans_vccb *, int);
int		spans_clear_vcc(struct spans *, struct spans_vccb *);
void		spans_switch_reset(struct spans *, int);

	/* spans_util.c */
int		spans_get_spans_sap(Sap_t, spans_sap *);
int		spans_get_local_sap(spans_sap, Sap_t *);
int		spans_ephemeral_sap(struct spans *);
int		spans_get_spans_aal(Aal_t, spans_aal *);
int		spans_get_local_aal(spans_aal, Aal_t *);
int		spans_verify_vccb(struct spans *, struct spans_vccb *);
struct spans_vccb *
		spans_find_vpvc(struct spans *, int, int, u_char);
struct spans_vccb *
		spans_find_conn(struct spans *, struct spans_atm_conn *);
spans_vpvc	spans_alloc_vpvc(struct spans *);
char *		spans_addr_print(struct spans_addr *);
void		spans_dump_buffer(KBuffer *);


/*
 * External variables
 */
extern struct spans_addr	spans_bcastaddr;
extern struct sp_info		spans_vcpool;
extern struct sp_info		spans_msgpool;
extern struct t_atm_cause	spans_cause;

#endif	/* _KERNEL */

#endif	/* _SPANS_SPANS_VAR_H */
