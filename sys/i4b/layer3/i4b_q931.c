/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_q931.c - Q931 received messages handling
 *	--------------------------------------------
 *
 *	$Id: i4b_q931.c,v 1.23 1999/12/13 21:25:27 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer3/i4b_q931.c,v 1.6 1999/12/14 20:48:32 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:05:33 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__
#include "i4bq931.h"
#else
#define	NI4BQ931	1
#endif

#if NI4BQ931 > 0

#include <sys/param.h>

#if defined(__FreeBSD__)
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif

#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_cause.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#include <i4b/i4b_cause.h>
#endif

#include <i4b/include/i4b_isdnq931.h>
#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer3/i4b_l3.h>
#include <i4b/layer3/i4b_l3fsm.h>
#include <i4b/layer3/i4b_q931.h>

#include <i4b/layer4/i4b_l4.h>

#ifndef __FreeBSD__
#define	memcpy(d,s,l)	bcopy(s,d,l)
#endif

unsigned int i4b_l3_debug = L3_DEBUG_DEFAULT;

call_desc_t call_desc[N_CALL_DESC];	/* call descriptor array */
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
		DBGL3(L3_P_ERR, "i4b_decode_q931", ("protocol discriminator 0x%x != Q.931\n", *msg_ptr));
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
			
	DBGL3(L3_P_MSG, "i4b_decode_q931", ("Call Ref, len %d, val %d, flag %d\n", crlen, crval, crflag));

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
				DBGL3(L3_P_ERR, "i4b_decode_q931", ("cannot find calldescriptor for cr = 0x%x, crflag = 0x%x, msg = 0x%x, frame = ", crval, crflag, *msg_ptr));
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
				DBGL3(L3_P_ERR, "i4b_decode_q931", ("Q.931 lockingshift proc violation, shift %d -> %d\n", old_codeset, codeset));
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
				DBGL3(L3_P_ERR, "i4b_decode_q931", ("unknown codeset %d, ", codeset));
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
		/* single byte IE's */
		
		case IEI_SENDCOMPL:
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_SENDCOMPL\n"));
			return(1);
			break;

		/* multi byte IE's */
		
		case IEI_BEARERCAP:	/* bearer capability */
			switch(msg_ptr[2])
			{
				case 0x80:	/* speech */
				case 0x89:	/* restricted digital info */
				case 0x90:	/* 3.1KHz audio */
/* XXX */				cd->bprot = BPROT_NONE;
					DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_BEARERCAP - Telephony\n"));
					break;

				case 0x88:	/* unrestricted digital info */
/* XXX */				cd->bprot = BPROT_RHDLC;
					DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_BEARERCAP - Raw HDLC\n"));
					break;

				default:
/* XXX */				cd->bprot = BPROT_NONE;
					DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_BEARERCAP - No Protocol\n"));
					break;
			}
			break;
	
		case IEI_CAUSE:		/* cause */
			if(msg_ptr[2] & 0x80)
			{
				cd->cause_in = msg_ptr[3] & 0x7f;
				DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_CAUSE = %d\n", msg_ptr[3] & 0x7f));
			}
			else
			{
				cd->cause_in = msg_ptr[4] & 0x7f;
				DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_CAUSE = %d\n", msg_ptr[4] & 0x7f));
			}
			break;
	
		case IEI_CHANNELID:	/* channel id */
			if((msg_ptr[2] & 0xf4) != 0x80)
			{
				cd->channelid = CHAN_NO;
				DBGL3(L3_P_ERR, "i4b_decode_q931_cs0_ie", ("IEI_CHANNELID, unsupported value 0x%x\n", msg_ptr[2]));
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

				DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_CHANNELID - channel %d, exclusive = %d\n", cd->channelid, cd->channelexcl));

				/* if this is a setup message, reserve channel */
				
				if(cd->event == EV_SETUP)
				{
					if((cd->channelid == CHAN_B1) || (cd->channelid == CHAN_B2))
					{
						if(ctrl_desc[cd->controller].bch_state[cd->channelid] == BCH_ST_FREE)
							ctrl_desc[cd->controller].bch_state[cd->channelid] = BCH_ST_RSVD;
						else
							DBGL3(L3_P_ERR, "i4b_decode_q931_cs0_ie", ("IE ChannelID, Channel NOT free!!\n"));
					}
					else if(cd->channelid == CHAN_NO)
					{
						DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IE ChannelID, SETUP with channel = No channel (CW)\n"));
					}
					else /* cd->channelid == CHAN_ANY */
					{
						DBGL3(L3_P_ERR, "i4b_decode_q931_cs0_ie", ("ERROR: IE ChannelID, SETUP with channel = Any channel!\n"));
					}
				}
			}
			break;				
	
		case IEI_CALLINGPN:	/* calling party no */
			if(msg_ptr[2] & 0x80) /* no presentation/screening indicator ? */
			{
				memcpy(cd->src_telno, &msg_ptr[3], min(TELNO_MAX, msg_ptr[1]-1));
				cd->src_telno[min(TELNO_MAX, msg_ptr[1] - 1)] = '\0';
				cd->scr_ind = SCR_NONE;
			}
			else
			{
				memcpy(cd->src_telno, &msg_ptr[4], min(TELNO_MAX, msg_ptr[1]-2));
				cd->src_telno[min(TELNO_MAX, msg_ptr[1] - 2)] = '\0';
				cd->scr_ind = (msg_ptr[3] & 0x03) + SCR_USR_NOSC;
			}
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_CALLINGPN = %s\n", cd->src_telno));
			break;
	
		case IEI_CALLEDPN:	/* called party number */
			memcpy(cd->dst_telno, &msg_ptr[3], min(TELNO_MAX, msg_ptr[1]-1));
			cd->dst_telno[min(TELNO_MAX, msg_ptr [1] - 1)] = '\0';
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_CALLED = %s\n", cd->dst_telno)); 
			break;
	
		case IEI_CALLSTATE:	/* call state		*/
			cd->call_state = msg_ptr[2] & 0x3f;		
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_CALLSTATE = %d\n", cd->call_state));
			break;
			
		case IEI_PROGRESSI:	/* progress indicator	*/
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_PROGRESSINDICATOR\n"));
			break;
			
		case IEI_DISPLAY:	/* display		*/
			/* CHANGED BY <chris@medis.de> */
			memcpy(cd->display, &msg_ptr[2], min(DISPLAY_MAX, msg_ptr[1]));
			cd->display[min(DISPLAY_MAX, msg_ptr[1])] = '\0';
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_DISPLAY = %s\n", cd->display));
  			break;
			
		case IEI_DATETIME:	/* date/time		*/
			i = 2;
			j = msg_ptr[1];
			p = &(cd->datetime[0]);
			*p = '\0';
			
			for(j = msg_ptr[1]; j > 0; j--, i++)
				sprintf(p+strlen(p), "%02d", msg_ptr[i]);
			
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_DATETIME = %s\n", cd->datetime));
			break;
			
		case IEI_FACILITY:	/* facility		*/
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_FACILITY\n"));
			if(i4b_aoc(msg_ptr, cd) > -1)
				i4b_l4_charging_ind(cd);
			break;
			
		case IEI_CONCTDNO:	/* connected number	*/
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_CONCTDNO\n"));
			break;
			
		case IEI_NETSPCFAC:	/* network specific fac */
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_NETSPCFAC\n"));
			break;
			
		case IEI_LLCOMPAT:	/* low layer compat	*/
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_LLCOMPAT\n"));
			break;
			
		case IEI_HLCOMPAT:	/* high layer compat	*/
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_HLCOMPAT\n"));
			break;
			
		case IEI_CALLINGPS:	/* calling party subaddress */
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_CALLINGPS\n"));
			break;
			
		case IEI_CALLEDPS:	/* called party subaddress */
			DBGL3(L3_P_MSG, "i4b_decode_q931_cs0_ie", ("IEI_CALLEDPS\n"));
			break;
			
		default:
			DBGL3(L3_P_ERR, "i4b_decode_q931_cs0_ie", ("Unknown IE %d - ", *msg_ptr));
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
	cd->event = EV_ILL;
	
	switch(message_type)
	{
		/* call establishment */

		case ALERT:
			cd->event = EV_ALERT;			
			DBGL3(L3_PRIM, "rx ALERT", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case CALL_PROCEEDING:
			cd->event = EV_CALLPRC;
			DBGL3(L3_PRIM, "rx CALL-PROC", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case PROGRESS:
			cd->event = EV_PROGIND;
			DBGL3(L3_PRIM, "rx PROGRESS", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case SETUP:
			DBGL3(L3_PRIM, "rx SETUP", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
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
			DBGL3(L3_PRIM, "rx CONNECT", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->datetime[0] = '\0';		
			cd->event = EV_CONNECT;			
			break;
			
		case SETUP_ACKNOWLEDGE:
			DBGL3(L3_PRIM, "rx SETUP-ACK", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->event = EV_SETUPAK;
			break;
			
		case CONNECT_ACKNOWLEDGE:
			DBGL3(L3_PRIM, "rx CONNECT-ACK", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->event = EV_CONACK;
			break;
			
		/* call information */

		case USER_INFORMATION:
			DBGL3(L3_PRIM, "rx USER-INFO", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
	
		case SUSPEND_REJECT:
			DBGL3(L3_PRIM, "rx SUSPEND-REJ", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case RESUME_REJECT:
			DBGL3(L3_PRIM, "rx RESUME-REJ", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case HOLD:
			DBGL3(L3_PRIM, "rx HOLD", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case SUSPEND:
			DBGL3(L3_PRIM, "rx SUSPEND", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case RESUME:
			DBGL3(L3_PRIM, "rx RESUME", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case HOLD_ACKNOWLEDGE:
			DBGL3(L3_PRIM, "rx HOLD-ACK", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case SUSPEND_ACKNOWLEDGE:
			DBGL3(L3_PRIM, "rx SUSPEND-ACK", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case RESUME_ACKNOWLEDGE:
			DBGL3(L3_PRIM, "rx RESUME-ACK", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case HOLD_REJECT:
			DBGL3(L3_PRIM, "rx HOLD-REJ", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case RETRIEVE:
			DBGL3(L3_PRIM, "rx RETRIEVE", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case RETRIEVE_ACKNOWLEDGE:
			DBGL3(L3_PRIM, "rx RETRIEVE-ACK", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case RETRIEVE_REJECT:
			DBGL3(L3_PRIM, "rx RETRIEVE-REJ", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		/* call clearing */

		case DISCONNECT:
			DBGL3(L3_PRIM, "rx DISCONNECT", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->event = EV_DISCONN;
			break;
	
		case RESTART:
			DBGL3(L3_PRIM, "rx RESTART", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case RELEASE:
			DBGL3(L3_PRIM, "rx RELEASE", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->event = EV_RELEASE;
			break;
			
		case RESTART_ACKNOWLEDGE:
			DBGL3(L3_PRIM, "rx RESTART-ACK", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case RELEASE_COMPLETE:
			DBGL3(L3_PRIM, "rx RELEASE-COMPLETE", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->event = EV_RELCOMP;		
			break;
			
		/* misc messages */

		case SEGMENT:
			DBGL3(L3_PRIM, "rx SEGMENT", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
	
		case FACILITY:
			DBGL3(L3_PRIM, "rx FACILITY", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->event = EV_FACILITY;		
			break;
			
		case REGISTER:
			DBGL3(L3_PRIM, "rx REGISTER", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case NOTIFY:
			DBGL3(L3_PRIM, "rx NOTIFY", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case STATUS_ENQUIRY:
			DBGL3(L3_PRIM, "rx STATUS-ENQ", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->event = EV_STATENQ;		
			break;
			
		case CONGESTION_CONTROL:
			DBGL3(L3_PRIM, "rx CONGESTION-CONTROL", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			break;
			
		case INFORMATION:
			DBGL3(L3_PRIM, "rx INFORMATION", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->event = EV_INFO;		
			break;
			
		case STATUS:
			DBGL3(L3_PRIM, "rx STATUS", ("unit %d, cr = 0x%02x\n", unit, cd->cr));
			cd->event = EV_STATUS;		
			break;
			
		default:
			DBGL3(L3_P_ERR, "rx UNKNOWN msg", ("unit %d, cr = 0x%02x, msg = 0x%02x\n", unit, cd->cr, message_type));
			break;
	}
}

#endif /* NI4BQ931 > 0 */
