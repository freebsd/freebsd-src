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
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * Protocol control blocks
 *
 */

#ifndef _UNISIG_VAR_H
#define	_UNISIG_VAR_H

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif

#ifdef _KERNEL

#include <vm/uma.h>

/*
 * Structure containing state information for each UNI protocol
 * instance.  There will be one instance for each ATM device interface
 * using the UNI signalling manager.
 */
struct	unisig {
	struct siginst	us_inst;	/* Header */
	struct atm_time	us_time;	/* Timer controls */
	void		(*us_lower)	/* Lower command handler */
				(int, void *, intptr_t, intptr_t);
	Atm_connection	*us_conn;	/* Signalling connection */
	int		us_cref;	/* Call reference allocation */
	u_int		us_retry;	/* Protocol retry count */
	u_short		us_headout;	/* Headroom on sig ch output */
	u_char		us_proto;	/* Signalling version */
};

#define	us_next		us_inst.si_next
#define	us_pif		us_inst.si_pif
#define	us_addr		us_inst.si_addr
#define	us_subaddr	us_inst.si_subaddr
#define	us_vccq		us_inst.si_vccq
#define	us_state	us_inst.si_state
#define	us_ipserv	us_inst.si_ipserv
#endif	/* _KERNEL */

/*
 * Signalling manager states
 */
#define	UNISIG_NULL		0
#define	UNISIG_ADDR_WAIT	1
#define	UNISIG_INIT		2
#define	UNISIG_ACTIVE		3
#define	UNISIG_DETACH		4

/*
 * Signalling manager events
 */
#define	UNISIG_SIGMGR_TIMEOUT		0
#define	UNISIG_SIGMGR_SSCF_EST_IND	1
#define	UNISIG_SIGMGR_SSCF_EST_CNF	2
#define	UNISIG_SIGMGR_SSCF_RLS_IND	3
#define	UNISIG_SIGMGR_SSCF_RLS_CNF	4
#define	UNISIG_SIGMGR_SSCF_DATA_IND	5
#define	UNISIG_SIGMGR_SSCF_UDATA_IND	6
#define	UNISIG_SIGMGR_CALL_CLEARED	7
#define	UNISIG_SIGMGR_DETACH		8
#define	UNISIG_SIGMGR_ADDR_SET		9

/*
 * Signalling manager timer values
 */
#define	UNISIG_SSCF_TIMEOUT	(3 * ATM_HZ)


#ifdef _KERNEL
/*
 * UNI Virtual Channel Connection control block.  All information
 * regarding the state of a UNI-controlled VCC will be recorded here.
 * There will be one UNI VCC control block for each UNI-controlled
 * VCC.
 */
struct unisig_vccb {
	struct vccb	vcp_hdr;	/* Generic VCCB */
	u_short		uv_retry;	/* Xmit retry count */
	u_int		uv_call_ref;	/* Q.2931 call reference */
};

#define	uv_type		vcp_hdr.vc_type
#define	uv_proto	vcp_hdr.vc_proto
#define	uv_sstate	vcp_hdr.vc_sstate
#define	uv_ustate	vcp_hdr.vc_ustate
#define	uv_pif		vcp_hdr.vc_pif
#define	uv_nif		vcp_hdr.vc_nif
#define	uv_sigelem	vcp_hdr.vc_sigelem
#define	uv_time		vcp_hdr.vc_time
#define	uv_vpi		vcp_hdr.vc_vpi
#define	uv_vci		vcp_hdr.vc_vci
#define	uv_connvc	vcp_hdr.vc_connvc
#define	uv_ipdus	vcp_hdr.vc_ipdus
#define	uv_opdus	vcp_hdr.vc_opdus
#define	uv_ibytes	vcp_hdr.vc_ibytes
#define	uv_obytes	vcp_hdr.vc_obytes
#define	uv_ierrors	vcp_hdr.vc_ierrors
#define	uv_oerrors	vcp_hdr.vc_oerrors
#define	uv_tstamp	vcp_hdr.vc_tstamp
#endif	/* _KERNEL */

/*
 * UNI VCC protocol states.  Taken from The ATM Forum UNI 3.0 (section
 * 5.2.1.1)
 */
#define	UNI_NULL		0	/* No call exists */
#define	UNI_CALL_INITIATED	1	/* Initiating call */
#define	UNI_CALL_OUT_PROC	3	/* Outgoing call proceeding */
#define	UNI_CALL_DELIVERED	4	/* Not supported */
#define	UNI_CALL_PRESENT	6	/* Call coming in */
#define	UNI_CALL_RECEIVED	7	/* Not supported */
#define	UNI_CONNECT_REQUEST	8	/* Call coming in */
#define	UNI_CALL_IN_PROC	9	/* Incoming call proceeding */
#define	UNI_ACTIVE		10	/* Call is established */
#define	UNI_RELEASE_REQUEST	11	/* Clearing call */
#define	UNI_RELEASE_IND		12	/* Network disconnecting */

/*
 * Additional states required for internal management of VCCs
 */
#define	UNI_SSCF_RECOV		13	/* Signalling chan recovery */
#define	UNI_FREE		14	/* Waiting for user to free */
#define	UNI_PVC_ACTIVE		15	/* PVC Active */
#define	UNI_PVC_ACT_DOWN	16	/* PVC Active - i/f down */

/*
 * UNI VCC events
 */
#define	UNI_VC_TIMEOUT		0	/* Timer expired */
#define	UNI_VC_CALLP_MSG	1	/* CALL PROCEEDING message */
#define	UNI_VC_CONNECT_MSG	2	/* CONNECT message */
#define	UNI_VC_CNCTACK_MSG	3	/* CONNECT ACK message */
#define	UNI_VC_SETUP_MSG	4	/* SETUP message */
#define	UNI_VC_RELEASE_MSG	5	/* RELEASE message */
#define	UNI_VC_RLSCMP_MSG	6	/* RELEASE COMPLETE message */
#define	UNI_VC_STATUS_MSG	7	/* STATUS message */
#define	UNI_VC_STATUSENQ_MSG	8	/* STATUS ENQ message */
#define	UNI_VC_ADDP_MSG		9	/* ADD PARTY message */
#define	UNI_VC_ADDPACK_MSG	10	/* ADD PARTY ACK message */
#define	UNI_VC_ADDPREJ_MSG	11	/* ADD PARTY REJ message */
#define	UNI_VC_DROP_MSG		12	/* DROP PARTY message */
#define	UNI_VC_DROPACK_MSG	13	/* DROP PARTY ACK message */
#define	UNI_VC_SETUP_CALL	14	/* Setup routine called */
#define	UNI_VC_ACCEPT_CALL	15	/* Accept call routine called */
#define	UNI_VC_REJECT_CALL	16	/* Reject call routine called */
#define	UNI_VC_RELEASE_CALL	17	/* Release routine called */
#define	UNI_VC_ABORT_CALL	18	/* Abort routine called */
#define	UNI_VC_SAAL_FAIL	19	/* Signalling AAL failed */
#define	UNI_VC_SAAL_ESTAB	20	/* Signalling AAL back up */


#ifdef _KERNEL
/*
 * UNI Timer Values.  These values (except for T317) are taken from
 * The ATM Forum UNI 3.0 (section 5.7.2).
 */
#define	UNI_T303	(4 * ATM_HZ)
#define	UNI_T308	(30 * ATM_HZ)
#define	UNI_T309	(10 * ATM_HZ)
#define	UNI_T310	(10 * ATM_HZ)
#define	UNI_T313	(4 * ATM_HZ)
#define	UNI_T316	(120 * ATM_HZ)
#define	UNI_T317	(60 * ATM_HZ)
#define	UNI_T322	(4 * ATM_HZ)
#define	UNI_T398	(4 * ATM_HZ)
#define	UNI_T399	(14 * ATM_HZ)


/*
 * Timer macros
 */
#define	UNISIG_TIMER(s, t)	atm_timeout(&(s)->us_time, (t), unisig_timer)
#define	UNISIG_CANCEL(s)	atm_untimeout(&(s)->us_time)
#define	UNISIG_VC_TIMER(v, t)	atm_timeout(&(v)->vc_time, (t), unisig_vctimer)
#define	UNISIG_VC_CANCEL(v)	atm_untimeout(&(v)->vc_time)


/*
 * Global function declarations
 */
struct usfmt;
struct unisig_msg;

	/* unisig_decode.c */
int		usf_dec_msg(struct usfmt *, struct unisig_msg *);

	/* unisig_encode.c */
int		usf_enc_msg(struct usfmt *, struct unisig_msg *);

	/* unisig_if.c */
int		unisig_start(void);
int		unisig_stop(void);
int		unisig_free(struct vccb *);

	/* unisig_mbuf.c */
int		usf_init(struct usfmt *, struct unisig *, KBuffer *, int, int);
int		usf_byte(struct usfmt *, u_char *);
int		usf_short(struct usfmt *, u_short *);
int		usf_int3(struct usfmt *, u_int *);
int		usf_int(struct usfmt *, u_int *);
int		usf_ext(struct usfmt *, u_int *);
int		usf_count(struct usfmt *);
int		usf_byte_mark(struct usfmt *, u_char *, u_char **);

	/* unisig_msg.c */
struct		ie_generic;
void		unisig_cause_from_attr(struct ie_generic *,
				Atm_attributes *);
void		unisig_cause_from_msg(struct ie_generic *,
				struct unisig_msg *, int);
int		unisig_send_msg(struct unisig *,
				struct unisig_msg *);
int		unisig_send_setup(struct unisig *,
				struct unisig_vccb *);
int		unisig_send_release(struct unisig *,
				struct unisig_vccb *,
				struct unisig_msg *,
				int);
int		unisig_send_release_complete(struct unisig *,
				struct unisig_vccb *,
				struct unisig_msg *,
				int);
int		unisig_send_status(struct unisig *,
				struct unisig_vccb *,
				struct unisig_msg *,
				int);
int		unisig_rcv_msg(struct unisig *, KBuffer *);

	/* unisig_print.c */
void		usp_print_msg(struct unisig_msg *, int);

	/* unisig_proto.c */
void		unisig_timer(struct atm_time *);
void		unisig_vctimer(struct atm_time *);
void		unisig_saal_ctl(int, void *, void *);
void		unisig_saal_data(void *, KBuffer *);
caddr_t		unisig_getname(void *);
void		unisig_connected(void *);
void		unisig_cleared(void *, struct t_atm_cause *);

	/* unisig_sigmgr_state.c */
int		unisig_sigmgr_state(struct unisig *, int,
				KBuffer *);

	/* unisig_subr.c */
void		unisig_cause_attr_from_user(Atm_attributes *, int);
void		unisig_cause_attr_from_ie(Atm_attributes *,
				struct ie_generic *);
int		unisig_open_vcc(struct unisig *, Atm_connvc *);
int		unisig_close_vcc(struct unisig *,
				struct unisig_vccb *);
int		unisig_clear_vcc(struct unisig *,
				struct unisig_vccb *, int);
void		unisig_switch_reset(struct unisig *, int);
void		unisig_save_attrs(struct unisig *, struct unisig_msg *,
				Atm_attributes *);
int		unisig_set_attrs(struct unisig *, struct unisig_msg *,
				Atm_attributes *);

	/* unisig_util.c */
void		unisig_free_msg(struct unisig_msg *);
int		unisig_verify_vccb(struct unisig *,
				struct unisig_vccb *);
struct unisig_vccb *
		unisig_find_conn(struct unisig *, u_int);
struct unisig_vccb *
		unisig_find_vpvc(struct unisig *, int, int,
				u_char);
int		unisig_alloc_call_ref(struct unisig *);
char *		unisig_addr_print(Atm_addr *);
void		unisig_print_mbuf(KBuffer *);
void		unisig_print_buffer(KBuffer *);

	/* unisig_vc_state.c */
int		unisig_vc_state(struct unisig *,
				struct unisig_vccb *,
				int,
				struct unisig_msg *);


/*
 * External variables
 */
extern uma_zone_t	unisig_vc_zone;
extern uma_zone_t	unisig_msg_zone;
extern uma_zone_t	unisig_ie_zone;
#endif	/* _KERNEL */
#endif	/* _UNISIG_VAR_H */
