/*********************************************************************
 *                
 *                
 * Filename:      irlap_event.h
 * Version:       0.1
 * Description:   
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sat Aug 16 00:59:29 1997
 * Modified at:   Tue Dec 21 11:20:30 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-1999 Dag Brattli <dagb@cs.uit.no>, 
 *     All Rights Reserved.
 *     Copyright (c) 2000-2001 Jean Tourrilhes <jt@hpl.hp.com>
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *     
 ********************************************************************/

#ifndef IRLAP_EVENT_H
#define IRLAP_EVENT_H

#include <net/irda/irda.h>
#include <net/irda/discovery.h>

struct irlap_cb;

/* IrLAP States */
typedef enum {
	LAP_NDM,         /* Normal disconnected mode */
	LAP_QUERY,
	LAP_REPLY,
	LAP_CONN,        /* Connect indication */
	LAP_SETUP,       /* Setting up connection */
	LAP_OFFLINE,     /* A really boring state */
	LAP_XMIT_P,
	LAP_PCLOSE,
	LAP_NRM_P,       /* Normal response mode as primary */
	LAP_RESET_WAIT,
	LAP_RESET,
	LAP_NRM_S,       /* Normal response mode as secondary */
	LAP_XMIT_S,
	LAP_SCLOSE,
	LAP_RESET_CHECK,
} IRLAP_STATE;

/* IrLAP Events */
typedef enum {
	/* Services events */
	DISCOVERY_REQUEST,
	CONNECT_REQUEST,
	CONNECT_RESPONSE,
	DISCONNECT_REQUEST,
	DATA_REQUEST,
	RESET_REQUEST,
	RESET_RESPONSE,

	/* Send events */
	SEND_I_CMD,
	SEND_UI_FRAME,

	/* Receive events */
	RECV_DISCOVERY_XID_CMD,
	RECV_DISCOVERY_XID_RSP,
	RECV_SNRM_CMD,
	RECV_TEST_CMD,
	RECV_TEST_RSP,
	RECV_UA_RSP,
	RECV_DM_RSP,
	RECV_RD_RSP,
	RECV_I_CMD,
	RECV_I_RSP,
	RECV_UI_FRAME,
	RECV_FRMR_RSP,
	RECV_RR_CMD,
	RECV_RR_RSP,
	RECV_RNR_CMD,
	RECV_RNR_RSP,
	RECV_REJ_CMD,
	RECV_REJ_RSP,
	RECV_SREJ_CMD,
	RECV_SREJ_RSP,
	RECV_DISC_CMD,

	/* Timer events */
	SLOT_TIMER_EXPIRED,
	QUERY_TIMER_EXPIRED,
	FINAL_TIMER_EXPIRED,
	POLL_TIMER_EXPIRED,
	DISCOVERY_TIMER_EXPIRED,
	WD_TIMER_EXPIRED,
	BACKOFF_TIMER_EXPIRED,
	MEDIA_BUSY_TIMER_EXPIRED,
} IRLAP_EVENT;

/*
 *  Various things used by the IrLAP state machine
 */
struct irlap_info {
	__u8 caddr;   /* Connection address */
	__u8 control; /* Frame type */
        __u8 cmd;

	__u32 saddr;
	__u32 daddr;
	
	int pf;        /* Poll/final bit set */

	__u8  nr;      /* Sequence number of next frame expected */
	__u8  ns;      /* Sequence number of frame sent */

	int  S;        /* Number of slots */
	int  slot;     /* Random chosen slot */
	int  s;        /* Current slot */

	discovery_t *discovery; /* Discovery information */
};

extern const char *irlap_state[];

void irlap_do_event(struct irlap_cb *self, IRLAP_EVENT event, 
		    struct sk_buff *skb, struct irlap_info *info);
void irlap_print_event(IRLAP_EVENT event);

extern int irlap_qos_negotiate(struct irlap_cb *self, struct sk_buff *skb);

#endif
