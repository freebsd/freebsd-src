/*
 * Copyright (c) 1997, 2002 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_l2.h - ISDN layer 2 (Q.921) definitions
 *	---------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sat Mar  9 16:12:20 2002]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_L2_H_
#define _I4B_L2_H_

typedef struct {
	int	unit;		/* unit number this entry is for */

	int	Q921_state;	/* state according to Q.921 */

	u_char	last_ril;	/* last reference number from TEI management */
	u_char	last_rih;

	int	tei_valid;	/* tei is valid flag */
#define TEI_INVALID	0
#define TEI_VALID	1	
	int	tei;		/* tei, if tei flag valid */

	int	ph_active;	/* Layer 1 active flag */
#define PH_INACTIVE	0	/* layer 1 inactive */
#define PH_ACTIVEPEND	1	/* already tried to activate */
#define PH_ACTIVE	2	/* layer 1 active */

	int	T200;		/* Multiframe timeout timer */
	int	T201;		/* min time between TEI ID check */
	int	T202;		/* min time between TEI ID Req messages */
	int	N202;		/* TEI ID Req tx counter */
	void(*T202func)(void *);/* function to be called when T202 expires */
	int	T203;		/* max line idle time */

	struct	callout_handle T200_callout;
	struct	callout_handle T202_callout;
	struct	callout_handle T203_callout;
	struct	callout_handle IFQU_callout;	

/*
 * i4b_iframe.c, i4b_i_frame_queued_up(): value of IFQU_DLY
 * some experimentation Gary did showed a minimal value of (hz/20) was
 * possible to let this work, Gary suggested using (hz/10) but i settled
 * down to using (hz/5) for now (-hm).
 */
#define IFQU_DLY (hz/5)		/* reschedule I-FRAME-QUEUED-UP 0.2 sec */

	int	vr;		/* receive sequence frame counter */
	int	vs;		/* transmit sequence frame counter */
	int	va;		/* acknowledge sequence frame counter */

	int	ack_pend;	/* acknowledge pending */
	int	rej_excpt;	/* reject exception */
	int	peer_busy;	/* peer receiver busy */
	int	own_busy;	/* own receiver busy */
	int	l3initiated;	/* layer 3 initiated */

	struct ifqueue i_queue;	/* queue of outgoing i frames */
#define IQUEUE_MAXLEN	20

	/* this implementation only supports a k-value of 1 !!! */
	struct mbuf *ua_frame;	/* last unacked frame */
	int	ua_num;		/* last unacked frame number */
#define UA_EMPTY (-1)		/* ua_frame is unused	*/	

	int	rxd_CR;		/* received Command Response bit */
	int	rxd_PF;		/* received Poll/Final bit */
	int	rxd_NR;		/* received N(R) field */
	int	RC;		/* Retry Counter */

	int	iframe_sent;	/* check if i frame acked by another i frame */
	
	int (*postfsmfunc)(int);/* function to be called at fsm exit */
	int	postfsmarg;	/* argument for above function */

	/* statistics */

	lapdstat_t	stat;	/* lapd protocol statistics */

} l2_softc_t;

extern l2_softc_t l2_softc[];

/* Q.912 system parameters (Q.921 03/93 pp 43) */

#define MAX_K_VALUE	1	/* BRI - # of outstanding frames 	*/

#define N200	3		/* max no of retransmissions */
#define N201DEF	260		/* max no of octetts in information field */
#define N202DEF	3		/* max no of TEI ID Request message transmissions */

#define T200DEF	(hz*1)		/* default T200 timer value = 1 second	*/
#define T201DEF	T200DEF		/* default T201 timer value = T200DEF	*/
#define T202DEF (hz*2)		/* default T202 timer value = 2 seconds */
#define T203DEF (hz*10)		/* default T203 timer value = 10 seconds*/

/* modulo 128 operations */

#define M128INC(v) 	(v)++;		\
			if((v)>127)	\
			{		\
				v = 0;	\
			}
			
#define M128DEC(v) 	(v)--;		\
			if((v)<0)	\
			{		\
				v = 127;\
			}
			
/* P-bit values */

typedef enum {
	P0,
	P1
} pbit_t;

/* F-bit values */

typedef enum {
	F0,
	F1
} fbit_t;

/* CR-bit values to NT */

typedef enum {
	CR_CMD_TO_NT,
	CR_RSP_TO_NT
} crbit_to_nt_t;

/* CR-bit values from NT */

typedef enum {
	CR_RSP_FROM_NT,
	CR_CMD_FROM_NT
} crbit_from_nt_t;

/* address field - octett 2 */

#define OFF_SAPI	0	/* SAPI offset, HDLC flag is eaten by L1 */
#define SAPI_CCP	0	/* SAPI = 0 - call control procedures */
#define SAPI_X25	16	/* SAPI = 16 - X.25 packet procedures */
#define SAPI_L2M	63	/* SAPI = 63 - Layer 2 management procedures */

/* extract and insert macros for SAPI octett */

#define GETSAPI(octett)		(((octett) >> 2) & 0x3f)
#define PUTSAPI(sapi,cr,octett)	((octett) = (((sapi << 2) & 0xfc) | ((cr & 0x01) << 1)))
#define GETCR(octett)		(((octett) >> 1) & 0x01)
#define GETEA(octett)		((octett) & 0x01)

/* address field - octett 3 */

#define OFF_TEI		1	/* TEI offset */
#define GETTEI(octett) (((octett) >> 1) & 0x7f)
#define PUTTEI(tei, octett) ((octett) = ((((tei) << 1) & 0xfe)) | 0x01) 
#define GROUP_TEI	127	/* broadcast TEI for LME */

/* control field - octett 4 */

#define OFF_CNTL	2	/* 1st byte of control field */

/* S frames */

#define S_FRAME_LEN	4	/* lenght of a U-frame */
#define OFF_SRCR	2	/* 1st byte of control field,	*/
				/* R-commands and R-responses	*/
#define OFF_SNR		3	/* 2nd byte of control field, N(R) and PF */
#define SPFBIT		0x01	/* poll/final bit mask */
#define SPBITSET	SPFBIT
#define SFBITSET	SPFBIT
#define GETSNR(octett) (((octett) >> 1) & 0x7f)
#define GETSPF(octett) ((octett) & SPFBIT)
#define RR		0x01	/* RR and bit 0 set */
#define RNR		0x05	/* RNR and bit 0 set */
#define REJ		0x09	/* REJ and bit 0 set */

/* U frames */

#define UI_HDR_LEN	3	/* length of UI header in front of L3 frame */
#define U_FRAME_LEN	3	/* lenght of a U-frame */
#define UPFBIT		0x10	/* poll/final bit mask */
#define UPBITSET	UPFBIT
#define UFBITSET	UPFBIT
#define GETUPF(octett) (((octett) >> 4) & 0x01)

/* commands/responses with pf bit set to 0 */

#define SABME		0x6f
#define	DM		0x0f
#define UI		0x03
#define DISC		0x43
#define UA		0x63
#define FRMR		0x87
#define XID		0xaf

/* control field - octett 3 */

#define OFF_MEI		3	/* 2nd byte of control field */

/* control field - octett 4,5 */

#define OFF_RIL		4	/* Ri low byte */
#define OFF_RIH		5	/* Ri high byte */

/* control field - octett 6 */

#define OFF_MT		6	/* Message Type */
#define OFF_AI		7	/* Action Indicator  */
#define GET_TEIFROMAI(octett) (((octett) >> 1) & 0x7f)

/* I frame */

#define I_HDR_LEN	4	/* length of I header in front of L3 frame */
#define OFF_INS		2	/* transmit sequence number */
#define OFF_INR		3	/* receive sequence number */
#define IPFBIT		0x01	/* poll/final bit mask */
#define IPBITSET	0x01
#define GETINR(octett)	(((octett) >> 1) & 0x7f)
#define GETINS(octett)	(((octett) >> 1) & 0x7f)
#define GETIP(octett)	((octett) & IPFBIT)

/* structure of a TEI management frame */

#define TEI_MGMT_FRM_LEN   8		/* frame length */
#define TEIM_SAPIO	0x00		/* SAPI, CR, EA */
#define TEIM_TEIO	0x01		/* TEI, EA */
#define TEIM_UIO	0x02		/* frame type = UI = 0x03 */
#define TEIM_MEIO	0x03		/* management entity id = 0x0f */
#define 	MEI	0x0f	
#define TEIM_RILO	0x04		/* reference number, low  */
#define TEIM_RIHO	0x05		/* reference number, high */
#define TEIM_MTO	0x06		/* message type */
#define 	MT_ID_REQEST	0x01
#define 	MT_ID_ASSIGN	0x02
#define 	MT_ID_DENY	0x03
#define 	MT_ID_CHK_REQ	0x04
#define 	MT_ID_CHK_RSP	0x05
#define 	MT_ID_REMOVE	0x06
#define 	MT_ID_VERIFY	0x07
#define TEIM_AIO	0x07		/* action indicator */

/* i4b_mdl_error_ind codes */

enum MDL_ERROR_CODES {
	MDL_ERR_A,
	MDL_ERR_B,
	MDL_ERR_C,
	MDL_ERR_D,
	MDL_ERR_E,
	MDL_ERR_F,
	MDL_ERR_G,
	MDL_ERR_H,
	MDL_ERR_I,
	MDL_ERR_J,
	MDL_ERR_K,
	MDL_ERR_L,
	MDL_ERR_M,
	MDL_ERR_N,
	MDL_ERR_O,
	MDL_ERR_MAX	
};

/* forward decl */

extern void i4b_acknowledge_pending ( l2_softc_t *l2sc );
extern struct mbuf * i4b_build_s_frame ( l2_softc_t *l2sc, crbit_to_nt_t crbit, pbit_t pbit, u_char type );
extern struct mbuf * i4b_build_u_frame ( l2_softc_t *l2sc, crbit_to_nt_t crbit, pbit_t pbit, u_char type );
extern void i4b_clear_exception_conditions ( l2_softc_t *l2sc );
extern int i4b_dl_data_req ( int unit, struct mbuf *m );
extern int i4b_dl_establish_req ( int unit );
extern int i4b_dl_release_req ( int unit );
extern int i4b_dl_unit_data_req ( int unit, struct mbuf *m );
extern void i4b_enquiry_response ( l2_softc_t *l2sc );
extern void i4b_establish_data_link ( l2_softc_t *l2sc );
extern void i4b_invoke_retransmission ( l2_softc_t *l2sc, int nr );
extern void i4b_i_frame_queued_up ( l2_softc_t *l2sc );
extern void i4b_l1_activate ( l2_softc_t *l2sc );
extern int i4b_l2_nr_ok ( int nr, int va, int vs );
extern void i4b_make_rand_ri ( l2_softc_t *l2sc );
extern void i4b_mdl_assign_ind ( l2_softc_t *l2sc );
extern void i4b_mdl_error_ind ( l2_softc_t *l2sc, char *where, int errorcode );
extern int i4b_mph_status_ind ( int unit, int status, int parm );
extern void i4b_next_l2state ( l2_softc_t *l2sc, int event );
extern void i4b_nr_error_recovery ( l2_softc_t *l2sc );
extern int i4b_ph_activate_ind ( int unit );
extern int i4b_ph_deactivate_ind ( int unit );
extern int i4b_ph_data_ind ( int unit, struct mbuf *m );
extern void i4b_print_frame ( int len, u_char *buf );
extern char *i4b_print_l2state ( l2_softc_t *l2sc );
extern void i4b_print_l2var ( l2_softc_t *l2sc );
extern void i4b_rxd_ack(l2_softc_t *l2sc, int nr);
extern void i4b_rxd_i_frame ( int unit, struct mbuf *m );
extern void i4b_rxd_s_frame ( int unit, struct mbuf *m );
extern void i4b_rxd_u_frame ( int unit, struct mbuf *m );
extern void i4b_T200_restart ( l2_softc_t *l2sc );
extern void i4b_T200_start ( l2_softc_t *l2sc );
extern void i4b_T200_stop ( l2_softc_t *l2sc );
extern void i4b_T202_start ( l2_softc_t *l2sc );
extern void i4b_T202_stop ( l2_softc_t *l2sc );
extern void i4b_T203_restart ( l2_softc_t *l2sc );
extern void i4b_T203_start ( l2_softc_t *l2sc );
extern void i4b_T203_stop ( l2_softc_t *l2sc );
extern void i4b_tei_assign ( l2_softc_t *l2sc );
extern void i4b_tei_chkresp ( l2_softc_t *l2sc );
extern void i4b_tei_rxframe ( int unit, struct mbuf *m );
extern void i4b_tei_verify ( l2_softc_t *l2sc );
extern void i4b_transmit_enquire ( l2_softc_t *l2sc );
extern void i4b_tx_disc ( l2_softc_t *l2sc, pbit_t pbit );
extern void i4b_tx_dm ( l2_softc_t *l2sc, fbit_t fbit );
extern void i4b_tx_frmr ( l2_softc_t *l2sc, fbit_t fbit );
extern void i4b_tx_rej_response ( l2_softc_t *l2sc, fbit_t fbit );
extern void i4b_tx_rnr_command ( l2_softc_t *l2sc, pbit_t pbit );
extern void i4b_tx_rnr_response ( l2_softc_t *l2sc, fbit_t fbit );
extern void i4b_tx_rr_command ( l2_softc_t *l2sc, pbit_t pbit );
extern void i4b_tx_rr_response ( l2_softc_t *l2sc, fbit_t fbit );
extern void i4b_tx_sabme ( l2_softc_t *l2sc, pbit_t pbit );
extern void i4b_tx_ua ( l2_softc_t *l2sc, fbit_t fbit );

#endif /* _I4B_L2_H_ */
