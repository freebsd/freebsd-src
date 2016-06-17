/*
 * NET		An implementation of the IEEE 802.2 LLC protocol for the
 *		LINUX operating system.  LLC is implemented as a set of 
 *		state machines and callbacks for higher networking layers.
 *
 *		Class 2 llc algorithm.
 *		Pseudocode interpreter, transition table lookup,
 *			data_request & indicate primitives...
 *
 *		Code for initialization, termination, registration and 
 *		MAC layer glue.
 *
 *		Copyright Tim Alpaerts, 
 *			<Tim_Alpaerts@toyota-motor-europe.com>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	Changes
 *		Alan Cox	:	Chainsawed into Linux format
 *					Modified to use llc_ names
 *					Changed callbacks
 *
 *	This file must be processed by sed before it can be compiled.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/p8022.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <asm/byteorder.h>

#include "pseudo/pseudocode.h"
#include "transit/pdutr.h"
#include "transit/timertr.h"
#include <net/llc_frame.h>
#include <net/llc.h>

/*
 *	Data_request() is called by the client to present a data unit
 *	to the llc for transmission.
 *	In the future this function should also check if the transmit window
 *	allows the sending of another pdu, and if not put the skb on the atq
 *	for deferred sending.
 */

int llc_data_request(llcptr lp, struct sk_buff *skb)
{
	if (skb_headroom(skb) < (lp->dev->hard_header_len +4)){
		printk("cl2llc: data_request() not enough headroom in skb\n");
		return -1;
	};

	skb_push(skb, 4);

	if ((lp->state != NORMAL) && (lp->state != BUSY) && (lp->state != REJECT))
	{
		printk("cl2llc: data_request() while no llc connection\n"); 
		return -1;  
	}

	if (lp->remote_busy)
	{     /* if the remote llc is BUSY, */
		ADD_TO_ATQ(skb);      /* save skb in the await transmit queue */
		return 0;
	}                           
	else
	{
		/*
		 *	Else proceed with xmit 
		 */

		switch(lp->state)
		{
			case NORMAL:
				if(lp->p_flag)
					llc_interpret_pseudo_code(lp, NORMAL2, skb, NO_FRAME);
				else
					llc_interpret_pseudo_code(lp, NORMAL1, skb, NO_FRAME);
				break;
			case BUSY:
				if (lp->p_flag)
					llc_interpret_pseudo_code(lp, BUSY2, skb, NO_FRAME);
				else
					llc_interpret_pseudo_code(lp, BUSY1, skb, NO_FRAME);
				break;
			case REJECT:
				if (lp->p_flag)
					llc_interpret_pseudo_code(lp, REJECT2, skb, NO_FRAME);
				else
					llc_interpret_pseudo_code(lp, REJECT1, skb, NO_FRAME);
				break;
			default:;
		}
		if(lp->llc_callbacks)
		{
			lp->llc_event(lp);
			lp->llc_callbacks=0;
		}
		return 0;  
	}              
}



/* 
 *	Disconnect_request() requests that the llc to terminate a connection
 */

void disconnect_request(llcptr lp)
{
	if ((lp->state == NORMAL) ||
    		(lp->state == BUSY) ||
		(lp->state == REJECT) ||
		(lp->state == AWAIT) ||
		(lp->state == AWAIT_BUSY) ||
		(lp->state == AWAIT_REJECT))
	{
		lp->state = D_CONN;
		llc_interpret_pseudo_code(lp, SH1, NULL, NO_FRAME);
		if(lp->llc_callbacks)
		{
			lp->llc_event(lp);
			lp->llc_callbacks=0;
		}
		/*
 		 *	lp may be invalid after the callback
		 */
	}
}


/*
 *	Connect_request() requests that the llc to start a connection
 */

void connect_request(llcptr lp)
{
	if (lp->state == ADM)
	{
		lp->state = SETUP;
		llc_interpret_pseudo_code(lp, ADM1, NULL, NO_FRAME);
		if(lp->llc_callbacks)
		{
			lp->llc_event(lp);
			lp->llc_callbacks=0;
		}
		/*
 		 *	lp may be invalid after the callback
		 */
	}
}


/*
 *	Interpret_pseudo_code() executes the actions in the connection component
 *	state transition table. Table 4 in document on p88.
 *
 *	If this function is called to handle an incoming pdu, skb will point
 *	to the buffer with the pdu and type will contain the decoded pdu type.
 *
 *	If called by data_request skb points to an skb that was skb_alloc-ed by 
 *	the llc client to hold the information unit to be transmitted, there is
 *	no valid type in this case.  
 *
 *	If called because a timer expired no skb is passed, and there is no 
 *	type.
 */

void llc_interpret_pseudo_code(llcptr lp, int pc_label, struct sk_buff *skb, 
		char type)
{    
	short int pc;	/* program counter in pseudo code array */ 
	char p_flag_received;
	frameptr fr;
	int resend_count;   /* number of pdus resend by llc_resend_ipdu() */
	int ack_count;      /* number of pdus acknowledged */
	struct sk_buff *skb2;

	if (skb != NULL) 
	{
		fr = (frameptr) skb->data;
	}
	else
		fr = NULL;

	pc = pseudo_code_idx[pc_label];
	while(pseudo_code[pc])
	{
		switch(pseudo_code[pc])
		{
			case 9:
				if ((type != I_CMD) || (fr->i_hdr.i_pflag == 0))
					break;
			case 1:
				lp->remote_busy = 0;
				llc_stop_timer(lp, BUSY_TIMER);
				if ((lp->state == NORMAL) ||
					(lp->state == REJECT) ||
					(lp->state == BUSY))
				{
					skb2 = llc_pull_from_atq(lp);
					if (skb2 != NULL) 
						llc_start_timer(lp, ACK_TIMER);
					while (skb2 != NULL)
					{
						llc_sendipdu( lp, I_CMD, 0, skb2);
						skb2 = llc_pull_from_atq(lp);
					}
				}	   
				break;
			case 2:
				lp->state = NORMAL;  /* needed to eliminate connect_response() */
				lp->llc_mode = MODE_ABM;
				lp->llc_callbacks|=LLC_CONN_INDICATION;
				break;
			case 3:
				lp->llc_mode = MODE_ABM;
				lp->llc_callbacks|=LLC_CONN_CONFIRM;
				break;
			case 4:
				skb_pull(skb, 4);
				lp->inc_skb=skb;
				lp->llc_callbacks|=LLC_DATA_INDIC;
				break;
			case 5:
				lp->llc_mode = MODE_ADM;
				lp->llc_callbacks|=LLC_DISC_INDICATION;
				break;
			case 70:
				lp->llc_callbacks|=LLC_RESET_INDIC_LOC;
				break;
			case 71:
				lp->llc_callbacks|=LLC_RESET_INDIC_REM;
				break;
			case 7:
				lp->llc_callbacks|=LLC_RST_CONFIRM;
				break;
			case 66:
				lp->llc_callbacks|=LLC_FRMR_RECV;
				break;
			case 67:
				lp->llc_callbacks|=LLC_FRMR_SENT;
				break;
			case 68:
				lp->llc_callbacks|=LLC_REMOTE_BUSY;
				break;
			case 69:
				lp->llc_callbacks|=LLC_REMOTE_NOTBUSY;
				break;
			case 11:
				llc_sendpdu(lp, DISC_CMD, lp->f_flag, 0, NULL);
				break;
			case 12:
				llc_sendpdu(lp, DM_RSP, 0, 0, NULL);
				break;
			case 13:                        
				lp->frmr_info_fld.cntl1 = fr->pdu_cntl.byte1;
				lp->frmr_info_fld.cntl2 = fr->pdu_cntl.byte2;
				lp->frmr_info_fld.vs = lp->vs;
				lp->frmr_info_fld.vr_cr = lp->vr;
				llc_sendpdu(lp, FRMR_RSP, 0, 5, (char *) &lp->frmr_info_fld);
				break;
			case 14:
				llc_sendpdu(lp, FRMR_RSP, 0, 5, (char *) &lp->frmr_info_fld);
				break;
			case 15:
				llc_sendpdu(lp, FRMR_RSP, lp->p_flag,
					5, (char *) &lp->frmr_info_fld);
				break;
			case 16:
				llc_sendipdu(lp, I_CMD, 1, skb);   
				break;
			case 17:
				resend_count = llc_resend_ipdu(lp, fr->i_hdr.nr, I_CMD, 1);
				break;
			case 18:
				resend_count = llc_resend_ipdu(lp, fr->i_hdr.nr, I_CMD, 1);
				if (resend_count == 0) 
				{
					llc_sendpdu(lp, RR_CMD, 1, 0, NULL);
				}    
				break;
			case 19:
				llc_sendipdu(lp, I_CMD, 0, skb);   
				break;
			case 20:
				resend_count = llc_resend_ipdu(lp, fr->i_hdr.nr, I_CMD, 0);
				break;
			case 21:
				resend_count = llc_resend_ipdu(lp, fr->i_hdr.nr, I_CMD, 0);
				if (resend_count == 0) 
				{
					llc_sendpdu(lp, RR_CMD, 0, 0, NULL);
				}    
				break;
			case 22:
				resend_count = llc_resend_ipdu(lp, fr->i_hdr.nr, I_RSP, 1);
				break;
			case 23:
				llc_sendpdu(lp, REJ_CMD, 1, 0, NULL);
				break;
			case 24:
				llc_sendpdu(lp, REJ_RSP, 1, 0, NULL);
				break;
			case 25:
				if (IS_RSP(fr))
					llc_sendpdu(lp, REJ_CMD, 0, 0, NULL);
				else
					llc_sendpdu(lp, REJ_RSP, 0, 0, NULL);
				break;
			case 26:
				llc_sendpdu(lp, RNR_CMD, 1, 0, NULL);
				break;
			case 27:
				llc_sendpdu(lp, RNR_RSP, 1, 0, NULL);
				break;
			case 28:
				if (IS_RSP(fr))
					llc_sendpdu(lp, RNR_CMD, 0, 0, NULL);
				else
					llc_sendpdu(lp, RNR_RSP, 0, 0, NULL);
				break;
			case 29:
				if (lp->remote_busy == 0)
				{
					lp->remote_busy = 1;
					llc_start_timer(lp, BUSY_TIMER);
					lp->llc_callbacks|=LLC_REMOTE_BUSY;
				}
				else if (lp->timer_state[BUSY_TIMER] == TIMER_IDLE)
				{
					llc_start_timer(lp, BUSY_TIMER);
				}
				break;
			case 30:
				if (IS_RSP(fr)) 
					llc_sendpdu(lp, RNR_CMD, 0, 0, NULL);
				else
					llc_sendpdu(lp, RNR_RSP, 0, 0, NULL);
				break;
			case 31:
				llc_sendpdu(lp, RR_CMD, 1, 0, NULL);
				break;
			case 32:
				llc_sendpdu(lp, RR_CMD, 1, 0, NULL);
				break;
			case 33:
				llc_sendpdu(lp, RR_RSP, 1, 0, NULL);
				break;
			case 34:
				llc_sendpdu(lp, RR_RSP, 1, 0, NULL);
				break;
			case 35:
				llc_sendpdu(lp, RR_RSP, 0, 0, NULL);
				break;
			case 36:
				if (IS_RSP(fr)) 
					llc_sendpdu(lp, RR_CMD, 0, 0, NULL);
				else
					llc_sendpdu(lp, RR_RSP, 0, 0, NULL);
				break;
			case 37:
				llc_sendpdu(lp, SABME_CMD, 0, 0, NULL);
				lp->f_flag = 0;
				break;
			case 38:
				llc_sendpdu(lp, UA_RSP, lp->f_flag, 0, NULL);
				break;
			case 39:
				lp->s_flag = 0;
				break;
			case 40:
				lp->s_flag = 1;
				break;
			case 41:
				if(lp->timer_state[P_TIMER] == TIMER_RUNNING)
					llc_stop_timer(lp, P_TIMER);
				llc_start_timer(lp, P_TIMER);
				if (lp->p_flag == 0)
				{
					lp->retry_count = 0;
					lp->p_flag = 1;
				}
				break;
			case 44:
				if (lp->timer_state[ACK_TIMER] == TIMER_IDLE)
					llc_start_timer(lp, ACK_TIMER);
				break;
			case 42:
				llc_start_timer(lp, ACK_TIMER);
				break;
			case 43:
				llc_start_timer(lp, REJ_TIMER);
				break;
			case 45:
				llc_stop_timer(lp, ACK_TIMER);
				break;
			case 46:
				llc_stop_timer(lp, ACK_TIMER);
				lp->p_flag = 0;
				break;
			case 10:
				if (lp->data_flag == 2)
					llc_stop_timer(lp, REJ_TIMER);
				break;
			case 47:
				llc_stop_timer(lp, REJ_TIMER);
				break;
			case 48:
				llc_stop_timer(lp, ACK_TIMER);
				llc_stop_timer(lp, P_TIMER);
				llc_stop_timer(lp, REJ_TIMER);
				llc_stop_timer(lp, BUSY_TIMER);
				break;
			case 49:
				llc_stop_timer(lp, P_TIMER);
				llc_stop_timer(lp, REJ_TIMER);
				llc_stop_timer(lp, BUSY_TIMER);
				break;
			case 50:             
				ack_count = llc_free_acknowledged_skbs(lp,
					(unsigned char) fr->s_hdr.nr);
				if (ack_count > 0)
				{
					lp->retry_count = 0;
					llc_stop_timer(lp, ACK_TIMER);  
					if (skb_peek(&lp->rtq) != NULL)
					{
						/*
 						 *	Re-transmit queue not empty 
						 */
						llc_start_timer(lp, ACK_TIMER);  
					}
				}        
				break;
			case 51:
				if (IS_UFRAME(fr)) 
					p_flag_received = fr->u_hdr.u_pflag;
				else
					p_flag_received = fr->i_hdr.i_pflag;
				if ((fr->pdu_hdr.ssap & 0x01) && (p_flag_received))
				{
					lp->p_flag = 0;
					llc_stop_timer(lp, P_TIMER);  
				}
				break;
			case 52:
				lp->data_flag = 2;
				break;
			case 53:
				lp->data_flag = 0;
				break;
			case 54:
				lp->data_flag = 1;
				break;
			case 55:
				if (lp->data_flag == 0)
					lp->data_flag = 1;
				break;
			case 56:
				lp->p_flag = 0;
				break;
			case 57:
				lp->p_flag = lp->f_flag;
				break;
			case 58:
				lp->remote_busy = 0;
				break;
			case 59:
				lp->retry_count = 0;
				break;
			case 60:
				lp->retry_count++;
				break;
			case 61:
				lp->vr = 0;
				break;
			case 62:
				lp->vr++;
				break;
			case 63:
				lp->vs = 0;
				break;
			case 64:
				lp->vs = fr->i_hdr.nr;
				break;
			case 65:
				if (IS_UFRAME(fr)) 
					lp->f_flag = fr->u_hdr.u_pflag;
				else
					lp->f_flag = fr->i_hdr.i_pflag;
				break;
			default:;
		}
		pc++;	
	}
}


/*
 *	Process_otype2_frame will handle incoming frames
 *	for 802.2 Type 2 Procedure.
 */

void llc_process_otype2_frame(llcptr lp, struct sk_buff *skb, char type)
{
	int idx;		/*	index in transition table */
	int pc_label;		/*	action to perform, from tr tbl */
	int validation;		/*	result of validate_seq_nos */
	int p_flag_received;	/*	p_flag in received frame */
	frameptr fr;

	fr = (frameptr) skb->data;

	if (IS_UFRAME(fr))
		p_flag_received = fr->u_hdr.u_pflag;
	else
		p_flag_received = fr->i_hdr.i_pflag;

	switch(lp->state)
	{
		/*	Compute index in transition table: */
		case ADM:
			idx = type;
			idx = (idx << 1) + p_flag_received;
			break;
		case CONN:
		case RESET_WAIT:
		case RESET_CHECK:
		case ERROR:
			idx = type;
			break;
		case SETUP:
		case RESET:
		case D_CONN:
			idx = type;
			idx = (idx << 1) + lp->p_flag;
			break;
		case NORMAL:
		case BUSY:
		case REJECT:
		case AWAIT:
		case AWAIT_BUSY:
		case AWAIT_REJECT:
			validation = llc_validate_seq_nos(lp, fr);
			if (validation > 3) 
				type = BAD_FRAME;
			idx = type;
			idx = (idx << 1);
			if (validation & 1) 
				idx = idx +1;
			idx = (idx << 1) + p_flag_received;
			idx = (idx << 1) + lp->p_flag;
		default:
			printk("llc_proc: bad state\n");
			return;
	}
	idx = (idx << 1) + pdutr_offset[lp->state];
	lp->state = pdutr_entry[idx +1]; 
	pc_label = pdutr_entry[idx];
	if (pc_label != 0)
	{ 
		llc_interpret_pseudo_code(lp, pc_label, skb, type);
		if(lp->llc_callbacks)
		{
			lp->llc_event(lp);
			lp->llc_callbacks=0;
		}
		/*
 		 *	lp may no longer be valid after this point. Be
		 *	careful what is added!
		 */
	}
}


void llc_timer_expired(llcptr lp, int t)
{
	int idx;		/* index in transition table	*/
	int pc_label;       	/* action to perform, from tr tbl */

	lp->timer_state[t] = TIMER_EXPIRED;
	idx = lp->state;            /* Compute index in transition table: */
	idx = (idx << 2) + t;
	idx = idx << 1;
	if (lp->retry_count >= lp->n2) 
		idx = idx + 1;
	idx = (idx << 1) + lp->s_flag;
	idx = (idx << 1) + lp->p_flag;
	idx = idx << 1;             /* 2 bytes per entry: action & newstate */

	pc_label = timertr_entry[idx];
	if (pc_label != 0)
	{
		llc_interpret_pseudo_code(lp, pc_label, NULL, NO_FRAME);
		lp->state = timertr_entry[idx +1];
	}
	lp->timer_state[t] = TIMER_IDLE;
	if(lp->llc_callbacks)
	{
		lp->llc_event(lp);
		lp->llc_callbacks=0;
	}
	/*
 	 *	And lp may have vanished in the event callback
 	 */
}

