/*-
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
 */

/*---------------------------------------------------------------------------
 *
 *	i4b_l3l4.h - layer 3 / layer 4 interface
 *	------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/include/i4b_l3l4.h,v 1.14 2005/01/06 22:18:18 imp Exp $
 *
 *	last edit-date: [Sun Aug 11 12:52:41 2002]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_L3L4_H_
#define _I4B_L3L4_H_

#define T303VAL	(hz*4)			/* 4 seconds timeout		*/
#define T305VAL	(hz*30)			/* 30 seconds timeout		*/
#define T308VAL	(hz*4)			/* 4 seconds timeout		*/
#define T309VAL	(hz*90)			/* 90 seconds timeout		*/
#define T310VAL	(hz*60)			/* 30-120 seconds timeout	*/
#define T313VAL	(hz*4)			/* 4 seconds timeout		*/
#define T400DEF	(hz*10)			/* 10 seconds timeout		*/

#define MAX_BCHAN 30
#define N_CALL_DESC (MAX_CONTROLLERS*MAX_BCHAN)	/* no of call descriptors */

extern int nctrl;		/* number of controllers detected in system */

typedef struct bchan_statistics {
	int outbytes;
	int inbytes;
} bchan_statistics_t;

/*---------------------------------------------------------------------------*
 * table of things the driver needs to know about the b channel
 * it is connected to for data transfer
 *---------------------------------------------------------------------------*/
typedef struct i4l_isdn_bchan_linktab {
	int unit;
	int channel;
	void (*bch_config)(int unit, int channel, int bprot, int updown);
	void (*bch_tx_start)(int unit, int channel);
	void (*bch_stat)(int unit, int channel, bchan_statistics_t *bsp);	
	struct ifqueue *tx_queue;
	struct ifqueue *rx_queue;	/* data xfer for NON-HDLC traffic   */
	struct mbuf **rx_mbuf;		/* data xfer for HDLC based traffic */
} isdn_link_t;

/*---------------------------------------------------------------------------*
 * table of things the b channel handler needs to know  about
 * the driver it is connected to for data transfer
 *---------------------------------------------------------------------------*/
typedef struct i4l_driver_bchan_linktab {
	int unit;
	void (*bch_rx_data_ready)(int unit);
	void (*bch_tx_queue_empty)(int unit);
	void (*bch_activity)(int unit, int rxtx);
#define ACT_RX 0
#define ACT_TX 1
	void (*line_connected)(int unit, void *cde);
	void (*line_disconnected)(int unit, void *cde);
	void (*dial_response)(int unit, int stat, cause_t cause);
	void (*updown_ind)(int unit, int updown);		
} drvr_link_t;

/* global linktab functions for controller types (aka hardware drivers) */
struct ctrl_type_desc {
	isdn_link_t* (*get_linktab)(int unit, int channel);
	void (*set_linktab)(int unit, int channel, drvr_link_t *dlt);
};
extern struct ctrl_type_desc ctrl_types[];

/* global linktab functions for RBCH userland driver */

drvr_link_t *rbch_ret_linktab(int unit);
void rbch_set_linktab(int unit, isdn_link_t *ilt);

/* global linktab functions for IPR network driver */

drvr_link_t *ipr_ret_linktab(int unit);
void ipr_set_linktab(int unit, isdn_link_t *ilt);

/* global linktab functions for TEL userland driver */

drvr_link_t *tel_ret_linktab(int unit);
void tel_set_linktab(int unit, isdn_link_t *ilt);

/* global linktab functions for ISPPP userland driver */

drvr_link_t *i4bisppp_ret_linktab(int unit);
void i4bisppp_set_linktab(int unit, isdn_link_t *ilt);

/* global linktab functions for ING network driver */

drvr_link_t *ing_ret_linktab(int unit);
void ing_set_linktab(int unit, isdn_link_t *ilt);


/*---------------------------------------------------------------------------*
 *	this structure describes one call/connection on one B-channel
 *	and all its parameters
 *---------------------------------------------------------------------------*/
typedef struct
{
	u_int	cdid;			/* call descriptor id		*/
	int	controller;		/* isdn controller number	*/
	int	cr;			/* call reference value		*/

	int	crflag;			/* call reference flag		*/
#define CRF_ORIG	0		/* originating side		*/
#define CRF_DEST	1		/* destinating side		*/

	int	channelid;		/* channel id value		*/
	int	channelexcl;		/* channel exclusive		*/

	int	bprot;			/* B channel protocol BPROT_XXX */

	int	bcap;			/* special bearer capabilities BCAP_XXX */	

	int	driver;			/* driver to use for B channel	*/
	int	driver_unit;		/* unit for above driver number	*/
	
	cause_t	cause_in;		/* cause value from NT	*/
	cause_t	cause_out;		/* cause value to NT	*/

	int	call_state;		/* from incoming SETUP	*/
	
	u_char	dst_telno[TELNO_MAX];	/* destination number	*/
	u_char	dst_subaddr[SUBADDR_MAX];	/* destination subaddr	*/
	u_char	src_telno[TELNO_MAX];	/* source number	*/
	u_char	src_subaddr[SUBADDR_MAX];	/* source subaddr	*/

	int	dst_ton;		/* destination type of number */
	int	src_ton;		/* source type of number */

	int	scr_ind;		/* screening ind for incoming call */
	int	prs_ind;		/* presentation ind for incoming call */
	
	int	Q931state;		/* Q.931 state for call	*/
	int	event;			/* event to be processed */

	int	response;		/* setup response type	*/

	int	T303;			/* SETUP sent response timeout	*/
	int	T303_first_to;		/* first timeout flag		*/

	int	T305;			/* DISC without PROG IND	*/

	int	T308;			/* RELEASE sent response timeout*/
	int	T308_first_to;		/* first timeout flag		*/

	int	T309;			/* data link disconnect timeout	*/

	int	T310;			/* CALL PROC received		*/

	int	T313;			/* CONNECT sent timeout		*/ 

	int	T400;			/* L4 timeout */

	isdn_link_t	*ilt;		/* isdn B channel linktab	*/
	drvr_link_t	*dlt;		/* driver linktab		*/

	int	dir;			/* outgoing or incoming call	*/
#define DIR_OUTGOING	0
#define DIR_INCOMING	1

	int	timeout_active;		/* idle timeout() active flag	*/

	int	callouts_inited;	/* must init before use */
	struct	callout_handle	idle_timeout_handle;
	struct	callout_handle	T303_callout;
	struct	callout_handle	T305_callout;
	struct	callout_handle	T308_callout;
	struct	callout_handle	T309_callout;
	struct	callout_handle	T310_callout;
	struct	callout_handle	T313_callout;
	struct	callout_handle	T400_callout;

	int	idletime_state;		/* wait for idle_time begin	*/
#define IST_IDLE	0	/* shorthold mode disabled 	*/
#define IST_NONCHK	1	/* in non-checked window	*/
#define IST_CHECK	2	/* in idle check window		*/
#define IST_SAFE	3	/* in safety zone		*/

	time_t	idletimechk_start;	/* check idletime window start	*/
	time_t	connect_time;		/* time connect was made	*/
	time_t	last_active_time;	/* last time with activity	*/

					/* for incoming connections:	*/
	time_t	max_idle_time;		/* max time without activity	*/

					/* for outgoing connections:	*/	
	msg_shorthold_t shorthold_data;	/* shorthold data to use */

	int	aocd_flag;		/* AOCD used for unitlength calc*/
	time_t	last_aocd_time;		/* last time AOCD received	*/
	int	units;			/* number of AOCD charging units*/
	int	units_type;		/* units type: AOCD, AOCE	*/
	int	cunits;			/* calculated units		*/

	int	isdntxdelay;		/* isdn tx delay after connect	*/

	u_char	display[DISPLAY_MAX];	/* display information element	*/
	char	datetime[DATETIME_MAX];	/* date/time information element*/
	u_char	keypad[KEYPAD_MAX];	/* keypad facility		*/	
} call_desc_t;

extern call_desc_t call_desc[N_CALL_DESC];

/* forward decl. */
struct isdn_diagnostic_request;
struct isdn_dr_prot;

/*---------------------------------------------------------------------------*
 *	this structure "describes" one controller
 *---------------------------------------------------------------------------*/
typedef struct
{
	int	unit;			/* unit number of this contr.	*/
	int	ctrl_type;		/* controller type   (CTRL_XXX)	*/
	int	card_type;		/* card manufacturer (CARD_XXX) */

	int	protocol;		/* D-channel protocol type */

	int	dl_est;			/* layer 2 established	*/
#define DL_DOWN	0
#define DL_UP	1	

        int     nbch;                   /* number of b channels */
	int	bch_state[MAX_BCHAN];	/* states of the b channels */
#define BCH_ST_FREE	0	/* free to be used, idle */
#define BCH_ST_RSVD	1	/* reserved, may become free or used */
#define BCH_ST_USED	2	/* in use for data transfer */

	int	tei;			/* current tei or -1 if invalid */

	/* pointers to functions to be called from L4 */
	
	void	(*N_CONNECT_REQUEST)	(unsigned int);	
	void	(*N_CONNECT_RESPONSE)	(unsigned int, int, int);
	void	(*N_DISCONNECT_REQUEST)	(unsigned int, int);
	void	(*N_ALERT_REQUEST)	(unsigned int);	
	int     (*N_DOWNLOAD)		(int unit, int numprotos, struct isdn_dr_prot *protocols);
	int     (*N_DIAGNOSTICS)	(int unit, struct isdn_diagnostic_request*);
	void	(*N_MGMT_COMMAND)	(int unit, int cmd, void *);
} ctrl_desc_t;

extern ctrl_desc_t ctrl_desc[MAX_CONTROLLERS];

#endif /* _I4B_Q931_H_ */
