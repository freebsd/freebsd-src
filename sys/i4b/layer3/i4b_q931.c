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
 *	i4b_q931.c - Q931 received messages handling
 *	--------------------------------------------
 *      last edit-date: [Sun Aug 11 19:18:08 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/layer3/i4b_q931.c,v 1.19 2007/07/06 07:17:22 bz Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_cause.h>

#include <i4b/include/i4b_isdnq931.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer3/i4b_l3.h>
#include <i4b/layer3/i4b_l3fsm.h>
#include <i4b/layer3/i4b_q931.h>

#include <i4b/layer4/i4b_l4.h>

unsigned int i4b_l3_debug = L3_DEBUG_DEFAULT;

ctrl_desc_t ctrl_desc[MAX_CONTROLLERS];	/* controller description array */
int utoc_tab[MAX_CONTROLLERS];		/* unit to controller conversion */

/* protocol independent causes -> Q.931 causes */

unsigned char cause_tab_q931[CAUSE_I4B_MAX] = {
	CAUSE_Q850_NCCLR, 	/* CAUSE_I4B_NORMAL -> normal call clearing */
	CAUSE_Q850_USRBSY,	/* CAUSE_I4B_BUSY -> user busy */
	CAUSE_Q850_NOCAVAIL,	/* CAUSE_I4B_NOCHAN -> no circuit/channel available*/
	CAUSE_Q850_INCDEST,	/* CAUSE_I4B_INCOMP -> incompatible destination */
	CAUSE_Q850_CALLREJ,	/* CAUSE_I4B_REJECT -> call rejected */
	CAUSE_Q850_DSTOOORDR,	/* CAUSE_I4B_OOO -> destination out of order */
	CAUSE_Q850_TMPFAIL,	/* CAUSE_I4B_TMPFAIL -> temporary failure */
	CAUSE_Q850_USRBSY,	/* CAUSE_I4B_L1ERROR -> L1 error / persistent deact XXX */
	CAUSE_Q850_USRBSY,	/* CAUSE_I4B_LLDIAL -> no dialout on leased line XXX */
};	

/*---------------------------------------------------------------------------*
 *	setup cr ref flag according to direction
 *---------------------------------------------------------------------------*/
unsigned char
setup_cr(call_desc_t *cd, unsigned char cr)
{
	if(cd->crflag == CRF_ORIG)
		return(cr & 0x7f);	/* clear cr ref flag */
	else if(cd->crflag == CRF_DEST)
		return(cr | 0x80);	/* set cr ref flag */
	else
		panic("setup_cr: invalid crflag!\n"); 
}

/*---------------------------------------------------------------------------*
 *	decode and process a Q.931 message
 *---------------------------------------------------------------------------*/
void
i4b_decode_q931(int unit, int msg_len, u_char *msg_ptr)
{
	call_desc_t *cd;
	int codeset = CODESET_0;
	int old_codeset = CODESET_0;
	int shift_flag = UNSHIFTED;
	int crlen = 0;
	int crval = 0;
	int crflag = 0;
	int i;	
	int offset;
	int s;
	
	/* check protocol discriminator */
	
	if(*msg_ptr != PD_Q931)
	{
		static int protoflag = -1;	/* print only once .. */

		if(*msg_ptr != protoflag)
		{
			NDBGL3(L3_P_ERR, "unknown protocol discriminator 0x%x!", *msg_ptr);
			protoflag = *msg_ptr;
		}			
		return;
	}

	msg_ptr++;
	msg_len--;

	s = SPLI4B();		/* this has to be protected ! */
	
	/* extract call reference */

	crlen = *msg_ptr & CRLENGTH_MASK;
	msg_ptr++;
	msg_len--;
	
	if(crlen != 0)
	{
		crval += *msg_ptr & 0x7f;
		crflag = (*msg_ptr >> 7) & 0x01;
		msg_ptr++;
		msg_len--;
		
		for(i=1; i < crlen; i++)
		{
			crval += *msg_ptr;
			msg_ptr++;
			msg_len--;			
		}
	}
	else
	{
		crval = 0;
		crflag = 0;
	}
			
	NDBGL3(L3_P_MSG, "Call Ref, len %d, val %d, flag %d", crlen, crval, crflag);

	/* find or allocate calldescriptor */

	if((cd = cd_by_unitcr(unit, crval,
			crflag == CRF_DEST ? CRF_ORIG : CRF_DEST)) == NULL)
	{
		if(*msg_ptr == SETUP)
		{
			/* get and init new calldescriptor */

			cd = reserve_cd();	/* cdid filled in */
			cd->controller = utoc_tab[unit];
			cd->cr = crval;		
			cd->crflag = CRF_DEST;	/* we are the dest side */
			cd->ilt = NULL;		/* reset link tab ptrs */
			cd->dlt = NULL;
		}
		else
		{
/*XXX*/			if(crval != 0)	/* ignore global call references */
			{
				NDBGL3(L3_P_ERR, "cannot find calldescriptor for cr = 0x%x, crflag = 0x%x, msg = 0x%x, frame = ", crval, crflag, *msg_ptr);
				i4b_print_frame(msg_len, msg_ptr);
			}
			splx(s);
			return;
		}
	}

	splx(s);

	/* decode and handle message type */
	
	i4b_decode_q931_message(unit, cd, *msg_ptr);
	msg_ptr++;
	msg_len--;
	
	/* process information elements */

	while(msg_len > 0)
	{
		/* check for shift codeset IE */
		
		if((*msg_ptr & 0x80) && ((*msg_ptr & 0xf0) == SOIE_SHIFT))
		{
			if(!(*msg_ptr & SHIFT_LOCK))
				shift_flag = SHIFTED;

			old_codeset = codeset;
			codeset = *msg_ptr & CODESET_MASK;

			if((shift_flag != SHIFTED) &&
			   (codeset <= old_codeset))
			{
				NDBGL3(L3_P_ERR, "Q.931 lockingshift proc violation, shift %d -> %d", old_codeset, codeset);
				codeset = old_codeset;
			}
			msg_len--;
			msg_ptr++;
		}

		/* process one IE for selected codeset */
		
		switch(codeset)
		{
			case CODESET_0:
				offset = i4b_decode_q931_cs0_ie(unit, cd, msg_len, msg_ptr);
				msg_len -= offset;
				msg_ptr += offset;
				break;
				
			default:
				NDBGL3(L3_P_ERR, "unknown codeset %d, ", codeset);
				i4b_print_frame(msg_len, msg_ptr);
				msg_len = 0;
				break;
		}

		/* check for non-locking shifts */
		
		if(shift_flag == SHIFTED)
		{
			shift_flag = UNSHIFTED;
			codeset = old_codeset;
		}
	}
	next_l3state(cd, cd->event);
}

/*---------------------------------------------------------------------------*
 *	decode and process one Q.931 codeset 0 information element
 *---------------------------------------------------------------------------*/
int
i4b_decode_q931_cs0_ie(int unit, call_desc_t *cd, int msg_len, u_char *msg_ptr)
{
	int i, j;
	char *p;
	
	switch(*msg_ptr)
	{

/*********/
/* Q.931 */
/*********/
		/* single byte IE's */
		
		case IEI_SENDCOMPL:
			NDBGL3(L3_P_MSG, "IEI_SENDCOMPL");
			return(1);
			break;

		/* multi byte IE's */
		
		case IEI_SEGMMSG:	/* segmented message */
			NDBGL3(L3_P_MSG, "IEI_SEGMENTED_MESSAGE");
			break;
			
		case IEI_BEARERCAP:	/* bearer capability */
			switch(msg_ptr[2])
			{
				case 0x80:	/* speech */
				case 0x89:	/* restricted digital info */
				case 0x90:	/* 3.1KHz audio */
/* XXX */				cd->bprot = BPROT_NONE;
					NDBGL3(L3_P_MSG, "IEI_BEARERCAP - Telephony");
					break;

				case 0x88:	/* unrestricted digital info */
/* XXX */				cd->bprot = BPROT_RHDLC;
					NDBGL3(L3_P_MSG, "IEI_BEARERCAP - Raw HDLC");
					break;

				default:
/* XXX */				cd->bprot = BPROT_NONE;
					NDBGL3(L3_P_ERR, "IEI_BEARERCAP - Unsupported B-Protocol 0x%x", msg_ptr[2]);
					break;
			}
			break;
	
		case IEI_CAUSE:		/* cause */
			if(msg_ptr[2] & 0x80)
			{
				cd->cause_in = msg_ptr[3] & 0x7f;
				NDBGL3(L3_P_MSG, "IEI_CAUSE = %d", msg_ptr[3] & 0x7f);
			}
			else
			{
				cd->cause_in = msg_ptr[4] & 0x7f;
				NDBGL3(L3_P_MSG, "IEI_CAUSE = %d", msg_ptr[4] & 0x7f);
			}
			break;
	
		case IEI_CALLID:	/* call identity */
			NDBGL3(L3_P_MSG, "IEI_CALL_IDENTITY");
			break;

		case IEI_CALLSTATE:	/* call state		*/
			cd->call_state = msg_ptr[2] & 0x3f;		
			NDBGL3(L3_P_MSG, "IEI_CALLSTATE = %d", cd->call_state);
			break;
			
		case IEI_CHANNELID:	/* channel id */
			if((msg_ptr[2] & 0xf4) != 0x80)
			{
				cd->channelid = CHAN_NO;
				NDBGL3(L3_P_ERR, "IEI_CHANNELID, unsupported value 0x%x", msg_ptr[2]);
			}
			else
			{
				switch(msg_ptr[2] & 0x03)
				{
					case IE_CHAN_ID_NO:
						cd->channelid = CHAN_NO;
						break;
					case IE_CHAN_ID_B1:
						cd->channelid = CHAN_B1;
						break;
					case IE_CHAN_ID_B2:
						cd->channelid = CHAN_B2;
						break;
					case IE_CHAN_ID_ANY:
						cd->channelid = CHAN_ANY;
						break;
				}
				cd->channelexcl = (msg_ptr[2] & 0x08) >> 3;

				NDBGL3(L3_P_MSG, "IEI_CHANNELID - channel %d, exclusive = %d", cd->channelid, cd->channelexcl);

				/* if this is a setup message, reserve channel */
				
				if(cd->event == EV_SETUP)
				{
					if((cd->channelid == CHAN_B1) || (cd->channelid == CHAN_B2))
					{
						if(ctrl_desc[cd->controller].bch_state[cd->channelid] == BCH_ST_FREE)
							ctrl_desc[cd->controller].bch_state[cd->channelid] = BCH_ST_RSVD;
						else
							NDBGL3(L3_P_ERR, "IE ChannelID, Channel NOT free!!");
					}
					else if(cd->channelid == CHAN_NO)
					{
						NDBGL3(L3_P_MSG, "IE ChannelID, SETUP with channel = No channel (CW)");
					}
					else /* cd->channelid == CHAN_ANY */
					{
						NDBGL3(L3_P_ERR, "ERROR: IE ChannelID, SETUP with channel = Any channel!");
					}
				}
			}
			break;				
	
		case IEI_PROGRESSI:	/* progress indicator	*/
			NDBGL3(L3_P_MSG, "IEI_PROGRESSINDICATOR");
			break;
			
		case IEI_NETSPCFAC:	/* network specific fac */
			NDBGL3(L3_P_MSG, "IEI_NETSPCFAC");
			break;
			
		case IEI_NOTIFIND:	/* notification indicator */
			NDBGL3(L3_P_MSG, "IEI_NOTIFICATION_INDICATOR");
			break;
			
		case IEI_DISPLAY:	/* display		*/
			memcpy(cd->display, &msg_ptr[2], min(DISPLAY_MAX, msg_ptr[1]));
			cd->display[min(DISPLAY_MAX, msg_ptr[1])] = '\0';
			NDBGL3(L3_P_MSG, "IEI_DISPLAY = %s", cd->display);
  			break;
			
		case IEI_DATETIME:	/* date/time		*/
			i = 2;
			j = msg_ptr[1];
			p = &(cd->datetime[0]);
			*p = '\0';
			
			for(j = msg_ptr[1]; j > 0; j--, i++)
				sprintf(p+strlen(p), "%02d", msg_ptr[i]);
			
			NDBGL3(L3_P_MSG, "IEI_DATETIME = %s", cd->datetime);
			break;
			
		case IEI_KEYPAD:	/* keypad facility */
			NDBGL3(L3_P_MSG, "IEI_KEYPAD_FACILITY");
			break;
			
		case IEI_SIGNAL:	/* signal type */
			NDBGL3(L3_P_MSG, "IEI_SIGNAL = %d", msg_ptr[2]);
			break;

		case IEI_INFRATE:	/* information rate */
			NDBGL3(L3_P_MSG, "IEI_INFORMATION_RATE");
			break;

		case IEI_ETETDEL:	/* end to end transit delay */
			NDBGL3(L3_P_MSG, "IEI_END_TO_END_TRANSIT_DELAY");
			break;

		case IEI_CUG:		/* closed user group */
			NDBGL3(L3_P_MSG, "IEI_CLOSED_USER_GROUP");
			break;

		case IEI_CALLINGPN:	/* calling party no */
			if(msg_ptr[2] & 0x80) /* no presentation/screening indicator ? */
			{
				memcpy(cd->src_telno, &msg_ptr[3], min(TELNO_MAX, msg_ptr[1]-1));
				cd->src_telno[min(TELNO_MAX, msg_ptr[1] - 1)] = '\0';
				cd->scr_ind = SCR_NONE;
				cd->prs_ind = PRS_NONE;				
			}
			else
			{
				memcpy(cd->src_telno, &msg_ptr[4], min(TELNO_MAX, msg_ptr[1]-2));
				cd->src_telno[min(TELNO_MAX, msg_ptr[1] - 2)] = '\0';
				cd->scr_ind = (msg_ptr[3] & 0x03) + SCR_USR_NOSC;
				cd->prs_ind = ((msg_ptr[3] >> 5) & 0x03) + PRS_ALLOWED;
			}

			/* type of number (source) */
			switch ((msg_ptr[2] & 0x70) >> 4)
			{
				case 1:	
					cd->src_ton = TON_INTERNAT;
					break;
				case 2:
					cd->src_ton = TON_NATIONAL;
					break;
				default:
					cd->src_ton = TON_OTHER;
					break;
			}
			NDBGL3(L3_P_MSG, "IEI_CALLINGPN = %s", cd->src_telno);
			break;
	
		case IEI_CALLINGPS:	/* calling party subaddress */
			memcpy(cd->src_subaddr, &msg_ptr[3], min(SUBADDR_MAX, msg_ptr[1]-1));
			cd->src_subaddr[min(SUBADDR_MAX, msg_ptr[1] - 1)] = '\0';
			NDBGL3(L3_P_MSG, "IEI_CALLINGPS = %s", cd->src_subaddr);
			break;
			
		case IEI_CALLEDPN:	/* called party number */
			memcpy(cd->dst_telno, &msg_ptr[3], min(TELNO_MAX, msg_ptr[1]-1));
			cd->dst_telno[min(TELNO_MAX, msg_ptr[1] - 1)] = '\0';

			/* type of number (destination) */
			switch ((msg_ptr[2] & 0x70) >> 4)
			{
				case 1:	
					cd->dst_ton = TON_INTERNAT;
					break;
				case 2:
					cd->dst_ton = TON_NATIONAL;
					break;
				default:
					cd->dst_ton = TON_OTHER;
					break;
			}

			NDBGL3(L3_P_MSG, "IEI_CALLED = %s", cd->dst_telno); 
			break;
	
		case IEI_CALLEDPS:	/* called party subaddress */
			memcpy(cd->dst_subaddr, &msg_ptr[3], min(SUBADDR_MAX, msg_ptr[1]-1));
			cd->dst_subaddr[min(SUBADDR_MAX, msg_ptr[1] - 1)] = '\0';
			NDBGL3(L3_P_MSG, "IEI_CALLEDPS = %s", cd->dst_subaddr);
			break;

		case IEI_REDIRNO:	/* redirecting number */
			NDBGL3(L3_P_MSG, "IEI_REDIRECTING_NUMBER");
			break;

		case IEI_TRNSEL:	/* transit network selection */
			NDBGL3(L3_P_MSG, "IEI_TRANSIT_NETWORK_SELECTION");
			break;

		case IEI_RESTARTI:	/* restart indicator */
			NDBGL3(L3_P_MSG, "IEI_RESTART_INDICATOR");
			break;

		case IEI_LLCOMPAT:	/* low layer compat */
			NDBGL3(L3_P_MSG, "IEI_LLCOMPAT");
			break;
			
		case IEI_HLCOMPAT:	/* high layer compat	*/
			NDBGL3(L3_P_MSG, "IEI_HLCOMPAT");
			break;
			
		case IEI_USERUSER:	/* user-user */
			NDBGL3(L3_P_MSG, "IEI_USER_USER");
			break;
			
		case IEI_ESCAPE:	/* escape for extension */
			NDBGL3(L3_P_MSG, "IEI_ESCAPE");
			break;
			
/*********/
/* Q.932 */
/*********/
		case IEI_FACILITY:	/* facility		*/
			NDBGL3(L3_P_MSG, "IEI_FACILITY");
			if(i4b_aoc(msg_ptr, cd) > -1)
				i4b_l4_charging_ind(cd);
			break;
			
/*********/
/* Q.95x */
/*********/
		case IEI_CONCTDNO:	/* connected number	*/
			NDBGL3(L3_P_MSG, "IEI_CONCTDNO");
			break;
			
			
		default:
			NDBGL3(L3_P_ERR, "Unknown IE %d - ", *msg_ptr);
			i4b_print_frame(msg_ptr[1]+2, msg_ptr);
			break;
	}
	return(msg_ptr[1] + 2);
}

/*---------------------------------------------------------------------------*
 *	decode and process one Q.931 codeset 0 information element
 *---------------------------------------------------------------------------*/
void
i4b_decode_q931_message(int unit, call_desc_t *cd, u_char message_type)
{
	char *m = NULL;
	
	cd->event = EV_ILL;

	switch(message_type)
	{
		/* call establishment */

		case ALERT:
			cd->event = EV_ALERT;			
			m = "ALERT";
			break;
			
		case CALL_PROCEEDING:
			cd->event = EV_CALLPRC;
			m = "CALL_PROCEEDING";
			break;
			
		case PROGRESS:
			cd->event = EV_PROGIND;
			m = "PROGRESS";
			break;
			
		case SETUP:
			m = "SETUP";
			cd->bprot = BPROT_NONE;
			cd->cause_in = 0;
			cd->cause_out = 0;			
			cd->dst_telno[0] = '\0';
			cd->src_telno[0] = '\0';
			cd->channelid = CHAN_NO;
			cd->channelexcl = 0;
			cd->display[0] = '\0';
			cd->datetime[0] = '\0';			
			cd->event = EV_SETUP;
			break;
			
		case CONNECT:
			m = "CONNECT";
			cd->datetime[0] = '\0';		
			cd->event = EV_CONNECT;			
			break;
			
		case SETUP_ACKNOWLEDGE:
			m = "SETUP_ACKNOWLEDGE";
			cd->event = EV_SETUPAK;
			break;
			
		case CONNECT_ACKNOWLEDGE:
			m = "CONNECT_ACKNOWLEDGE";
			cd->event = EV_CONACK;
			break;
			
		/* call information */

		case USER_INFORMATION:
			m = "USER_INFORMATION";
			break;
	
		case SUSPEND_REJECT:
			m = "SUSPEND_REJECT";
			break;
			
		case RESUME_REJECT:
			m = "RESUME_REJECT";
			break;
			
		case HOLD:
			m = "HOLD";
			break;
			
		case SUSPEND:
			m = "SUSPEND";
			break;
			
		case RESUME:
			m = "RESUME";
			break;
			
		case HOLD_ACKNOWLEDGE:
			m = "HOLD_ACKNOWLEDGE";
			break;
			
		case SUSPEND_ACKNOWLEDGE:
			m = "SUSPEND_ACKNOWLEDGE";
			break;
			
		case RESUME_ACKNOWLEDGE:
			m = "RESUME_ACKNOWLEDGE";
			break;
			
		case HOLD_REJECT:
			m = "HOLD_REJECT";
			break;
			
		case RETRIEVE:
			m = "RETRIEVE";
			break;
			
		case RETRIEVE_ACKNOWLEDGE:
			m = "RETRIEVE_ACKNOWLEDGE";
			break;
			
		case RETRIEVE_REJECT:
			m = "RETRIEVE_REJECT";
			break;
			
		/* call clearing */

		case DISCONNECT:
			m = "DISCONNECT";
			cd->event = EV_DISCONN;
			break;
	
		case RESTART:
			m = "RESTART";
			break;
			
		case RELEASE:
			m = "RELEASE";
			cd->event = EV_RELEASE;
			break;
			
		case RESTART_ACKNOWLEDGE:
			m = "RESTART_ACKNOWLEDGE";
			break;
			
		case RELEASE_COMPLETE:
			m = "RELEASE_COMPLETE";
			cd->event = EV_RELCOMP;		
			break;
			
		/* misc messages */

		case SEGMENT:
			m = "SEGMENT";
			break;
	
		case FACILITY:
			m = "FACILITY";
			cd->event = EV_FACILITY;		
			break;
			
		case REGISTER:
			m = "REGISTER";
			break;
			
		case NOTIFY:
			m = "NOTIFY";
			break;
			
		case STATUS_ENQUIRY:
			m = "STATUS_ENQUIRY";
			cd->event = EV_STATENQ;		
			break;
			
		case CONGESTION_CONTROL:
			m = "CONGESTION_CONTROL";
			break;
			
		case INFORMATION:
			m = "INFORMATION";
			cd->event = EV_INFO;		
			break;
			
		case STATUS:
			m = "STATUS";
			cd->event = EV_STATUS;		
			break;
			
		default:
			NDBGL3(L3_P_ERR, "unit %d, cr = 0x%02x, msg = 0x%02x", unit, cd->cr, message_type);
			break;
	}
	if(m)
	{
		NDBGL3(L3_PRIM, "%s: unit %d, cr = 0x%02x\n", m, unit, cd->cr);
	}
}
