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
 *	i4b_l4.c - kernel interface to userland
 *	-----------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sun Aug 11 12:43:14 2002]
 *
 *---------------------------------------------------------------------------*/

#include "i4b.h"
#include "i4bipr.h"

#if NI4B > 0

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include "i4bing.h"
#include "i4bisppp.h"
#include "i4brbch.h"
#include "i4btel.h"

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_cause.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/layer4/i4b_l4.h>

unsigned int i4b_l4_debug = L4_DEBUG_DEFAULT;

struct ctrl_type_desc ctrl_types[CTRL_NUMTYPES] = { { NULL, NULL} };

static int i4b_link_bchandrvr(call_desc_t *cd);
static void i4b_unlink_bchandrvr(call_desc_t *cd);
static void i4b_l4_setup_timeout(call_desc_t *cd);
static void i4b_idle_check_fix_unit(call_desc_t *cd);
static void i4b_idle_check_var_unit(call_desc_t *cd);
static void i4b_l4_setup_timeout_fix_unit(call_desc_t *cd);
static void i4b_l4_setup_timeout_var_unit(call_desc_t *cd);
static time_t i4b_get_idletime(call_desc_t *cd);

#if NI4BISPPP > 0
extern time_t i4bisppp_idletime(int);
#endif

/*---------------------------------------------------------------------------*
 *	send MSG_PDEACT_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_pdeact(int controller, int numactive)
{
	struct mbuf *m;
	int i;
	call_desc_t *cd;
	
	for(i=0; i < N_CALL_DESC; i++)
	{
		if((call_desc[i].cdid != CDID_UNUSED)				  &&
		   (ctrl_desc[call_desc[i].controller].ctrl_type == CTRL_PASSIVE) &&
		   (ctrl_desc[call_desc[i].controller].unit == controller))
		{
			cd = &call_desc[i];
			
			if(cd->timeout_active)
			{
				STOP_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd);
			}
			
			if(cd->dlt != NULL)
			{
				(*cd->dlt->line_disconnected)(cd->driver_unit, (void *)cd);
				i4b_unlink_bchandrvr(cd);
			}
		
			if((cd->channelid >= 0) & (cd->channelid < ctrl_desc[cd->controller].nbch))
			{
				ctrl_desc[cd->controller].bch_state[cd->channelid] = BCH_ST_FREE;
			}

			cd->cdid = CDID_UNUSED;
		}
	}
	
	if((m = i4b_Dgetmbuf(sizeof(msg_pdeact_ind_t))) != NULL)
	{
		msg_pdeact_ind_t *md = (msg_pdeact_ind_t *)m->m_data;

		md->header.type = MSG_PDEACT_IND;
		md->header.cdid = -1;

		md->controller = controller;
		md->numactive = numactive;

		i4bputqueue_hipri(m);		/* URGENT !!! */
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_L12STAT_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_l12stat(int controller, int layer, int state)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_l12stat_ind_t))) != NULL)
	{
		msg_l12stat_ind_t *md = (msg_l12stat_ind_t *)m->m_data;

		md->header.type = MSG_L12STAT_IND;
		md->header.cdid = -1;

		md->controller = controller;
		md->layer = layer;
		md->state = state;

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_TEIASG_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_teiasg(int controller, int tei)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_teiasg_ind_t))) != NULL)
	{
		msg_teiasg_ind_t *md = (msg_teiasg_ind_t *)m->m_data;

		md->header.type = MSG_TEIASG_IND;
		md->header.cdid = -1;

		md->controller = controller;
		md->tei = ctrl_desc[controller].tei;

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_DIALOUT_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_dialout(int driver, int driver_unit)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_dialout_ind_t))) != NULL)
	{
		msg_dialout_ind_t *md = (msg_dialout_ind_t *)m->m_data;

		md->header.type = MSG_DIALOUT_IND;
		md->header.cdid = -1;

		md->driver = driver;
		md->driver_unit = driver_unit;	

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_DIALOUTNUMBER_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_dialoutnumber(int driver, int driver_unit, int cmdlen, char *cmd)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_dialoutnumber_ind_t))) != NULL)
	{
		msg_dialoutnumber_ind_t *md = (msg_dialoutnumber_ind_t *)m->m_data;
		int i;

		md->header.type = MSG_DIALOUTNUMBER_IND;
		md->header.cdid = -1;

		md->driver = driver;
		md->driver_unit = driver_unit;

		for (i = 0; i < cmdlen; i++)
			if (cmd[i] == '*')
				break;

		/* XXX: TELNO_MAX is _with_ tailing '\0', so max is actually TELNO_MAX - 1 */
		md->cmdlen = (i < TELNO_MAX  - 1 ? i : TELNO_MAX - 1);
		/* skip the (first) '*' */
		md->subaddrlen = (cmdlen - i - 1 < SUBADDR_MAX - 1 ? cmdlen - i - 1 : SUBADDR_MAX - 1);

		bcopy(cmd, md->cmd, md->cmdlen);
		if (md->subaddrlen != -1)
			bcopy(cmd+i+1, md->subaddr, md->subaddrlen); 

		NDBGL4(L4_TIMO, "cmd[%d]=%s, subaddr[%d]=%s", md->cmdlen, md->cmd, md->subaddrlen, md->subaddr);
		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_KEYPAD_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_keypad(int driver, int driver_unit, int cmdlen, char *cmd)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_keypad_ind_t))) != NULL)
	{
		msg_keypad_ind_t *md = (msg_keypad_ind_t *)m->m_data;

		md->header.type = MSG_KEYPAD_IND;
		md->header.cdid = -1;

		md->driver = driver;
		md->driver_unit = driver_unit;

		if(cmdlen > KEYPAD_MAX)
			cmdlen = KEYPAD_MAX;

		md->cmdlen = cmdlen;
		bcopy(cmd, md->cmd, cmdlen);
		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_NEGOTIATION_COMPL message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_negcomplete(call_desc_t *cd)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_negcomplete_ind_t))) != NULL)
	{
		msg_negcomplete_ind_t *md = (msg_negcomplete_ind_t *)m->m_data;

		md->header.type = MSG_NEGCOMP_IND;
		md->header.cdid = cd->cdid;

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_IFSTATE_CHANGED_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_ifstate_changed(call_desc_t *cd, int new_state)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_ifstatechg_ind_t))) != NULL)
	{
		msg_ifstatechg_ind_t *md = (msg_ifstatechg_ind_t *)m->m_data;

		md->header.type = MSG_IFSTATE_CHANGED_IND;
		md->header.cdid = cd->cdid;
		md->state = new_state;

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_DRVRDISC_REQ message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_drvrdisc(int driver, int driver_unit)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_drvrdisc_req_t))) != NULL)
	{
		msg_drvrdisc_req_t *md = (msg_drvrdisc_req_t *)m->m_data;

		md->header.type = MSG_DRVRDISC_REQ;
		md->header.cdid = -1;

		md->driver = driver;
		md->driver_unit = driver_unit;	

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_ACCT_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_accounting(int driver, int driver_unit, int accttype, int ioutbytes,
		int iinbytes, int ro, int ri, int outbytes, int inbytes)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_accounting_ind_t))) != NULL)
	{
		msg_accounting_ind_t *md = (msg_accounting_ind_t *)m->m_data;

		md->header.type = MSG_ACCT_IND;
		md->header.cdid = -1;

		md->driver = driver;
		md->driver_unit = driver_unit;	

		md->accttype = accttype;
		md->ioutbytes = ioutbytes;
		md->iinbytes = iinbytes;
		md->outbps = ro;
		md->inbps = ri;
		md->outbytes = outbytes;
		md->inbytes = inbytes;
		
		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_CONNECT_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_connect_ind(call_desc_t *cd)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_connect_ind_t))) != NULL)
	{
		msg_connect_ind_t *mp = (msg_connect_ind_t *)m->m_data;

		mp->header.type = MSG_CONNECT_IND;
		mp->header.cdid = cd->cdid;

		mp->controller = cd->controller;
		mp->channel = cd->channelid;
		mp->bprot = cd->bprot;
		mp->bcap = cd->bcap;		

		cd->dir = DIR_INCOMING;

		if(strlen(cd->dst_telno) > 0)
			strcpy(mp->dst_telno, cd->dst_telno);
		else
			strcpy(mp->dst_telno, TELNO_EMPTY);

		if(strlen(cd->dst_subaddr) > 0)
			strcpy(mp->dst_subaddr, cd->dst_subaddr);
		else
			strcpy(mp->dst_subaddr, TELNO_EMPTY);

		if(strlen(cd->src_telno) > 0)
			strcpy(mp->src_telno, cd->src_telno);
		else
			strcpy(mp->src_telno, TELNO_EMPTY);
			
		if(strlen(cd->src_subaddr) > 0)
			strcpy(mp->src_subaddr, cd->src_subaddr);
		else
			strcpy(mp->src_subaddr, TELNO_EMPTY);

		strcpy(mp->display, cd->display);

		mp->scr_ind = cd->scr_ind;
		mp->prs_ind = cd->prs_ind;		
		
		T400_start(cd);
		
		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_CONNECT_ACTIVE_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_connect_active_ind(call_desc_t *cd)
{
	int s;
	struct mbuf *m;

	s = SPLI4B();

	cd->last_active_time = cd->connect_time = SECOND;

	NDBGL4(L4_TIMO, "last_active/connect_time=%ld", (long)cd->connect_time);
	
	i4b_link_bchandrvr(cd);

	(*cd->dlt->line_connected)(cd->driver_unit, (void *)cd);

	i4b_l4_setup_timeout(cd);
	
	splx(s);	
	
	if((m = i4b_Dgetmbuf(sizeof(msg_connect_active_ind_t))) != NULL)
	{
		msg_connect_active_ind_t *mp = (msg_connect_active_ind_t *)m->m_data;

		mp->header.type = MSG_CONNECT_ACTIVE_IND;
		mp->header.cdid = cd->cdid;
		mp->controller = cd->controller;
		mp->channel = cd->channelid;
		if(cd->datetime[0] != '\0')
			strcpy(mp->datetime, cd->datetime);
		else
			mp->datetime[0] = '\0';
		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_DISCONNECT_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_disconnect_ind(call_desc_t *cd)
{
	struct mbuf *m;

	if(cd->timeout_active)
		STOP_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd);

	if(cd->dlt != NULL)
	{
		(*cd->dlt->line_disconnected)(cd->driver_unit, (void *)cd);
		i4b_unlink_bchandrvr(cd);
	}

	if((cd->channelid >= 0) && (cd->channelid < ctrl_desc[cd->controller].nbch))
	{
		ctrl_desc[cd->controller].bch_state[cd->channelid] = BCH_ST_FREE;
	}
	else
	{
		/* no error, might be hunting call for callback */
		NDBGL4(L4_MSG, "channel free not valid but %d!", cd->channelid);
	}
	
	if((m = i4b_Dgetmbuf(sizeof(msg_disconnect_ind_t))) != NULL)
	{
		msg_disconnect_ind_t *mp = (msg_disconnect_ind_t *)m->m_data;

		mp->header.type = MSG_DISCONNECT_IND;
		mp->header.cdid = cd->cdid;
		mp->cause = cd->cause_in;

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_IDLE_TIMEOUT_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_idle_timeout_ind(call_desc_t *cd)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_idle_timeout_ind_t))) != NULL)
	{
		msg_idle_timeout_ind_t *mp = (msg_idle_timeout_ind_t *)m->m_data;

		mp->header.type = MSG_IDLE_TIMEOUT_IND;
		mp->header.cdid = cd->cdid;

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_CHARGING_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_charging_ind(call_desc_t *cd)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_charging_ind_t))) != NULL)
	{
		msg_charging_ind_t *mp = (msg_charging_ind_t *)m->m_data;

		mp->header.type = MSG_CHARGING_IND;
		mp->header.cdid = cd->cdid;
		mp->units_type = cd->units_type;

/*XXX*/		if(mp->units_type == CHARGE_CALC)
			mp->units = cd->cunits;
		else
			mp->units = cd->units;

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_STATUS_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_status_ind(call_desc_t *cd)
{
}

/*---------------------------------------------------------------------------*
 *	send MSG_ALERT_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_alert_ind(call_desc_t *cd)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_alert_ind_t))) != NULL)
	{
		msg_alert_ind_t *mp = (msg_alert_ind_t *)m->m_data;

		mp->header.type = MSG_ALERT_IND;
		mp->header.cdid = cd->cdid;

		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	send MSG_INFO_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_info_ind(call_desc_t *cd)
{
}

/*---------------------------------------------------------------------------*
 *	send MSG_INFO_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_proceeding_ind(call_desc_t *cd)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(sizeof(msg_proceeding_ind_t))) != NULL)
	{
		msg_proceeding_ind_t *mp = (msg_proceeding_ind_t *)m->m_data;

		mp->header.type = MSG_PROCEEDING_IND;
		mp->header.cdid = cd->cdid;
		mp->controller = cd->controller;
		mp->channel = cd->channelid;
		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *    send MSG_PACKET_IND message to userland
 *---------------------------------------------------------------------------*/
void
i4b_l4_packet_ind(int driver, int driver_unit, int dir, struct mbuf *pkt)
{
	struct mbuf *m;
	int len = pkt->m_pkthdr.len;
	unsigned char *ip = pkt->m_data;

	if((m = i4b_Dgetmbuf(sizeof(msg_packet_ind_t))) != NULL)
	{
		msg_packet_ind_t *mp = (msg_packet_ind_t *)m->m_data;

		mp->header.type = MSG_PACKET_IND;
		mp->header.cdid = -1;
		mp->driver = driver;
		mp->driver_unit = driver_unit;
		mp->direction = dir;
		memcpy(mp->pktdata, ip,
			len <MAX_PACKET_LOG ? len : MAX_PACKET_LOG);
		i4bputqueue(m);
	}
}

/*---------------------------------------------------------------------------*
 *	link a driver(unit) to a B-channel(controller,unit,channel)
 *---------------------------------------------------------------------------*/
static int
i4b_link_bchandrvr(call_desc_t *cd)
{
	int t = ctrl_desc[cd->controller].ctrl_type;
	
	if(t < 0 || t >= CTRL_NUMTYPES || ctrl_types[t].get_linktab == NULL)
	{
			cd->ilt = NULL;
	}
	else
	{
		cd->ilt = ctrl_types[t].get_linktab(
			ctrl_desc[cd->controller].unit,
			cd->channelid);
	}

	switch(cd->driver)
	{
#if NI4BRBCH > 0
		case BDRV_RBCH:
			cd->dlt = rbch_ret_linktab(cd->driver_unit);
			break;
#endif
		
#if NI4BTEL > 0
		case BDRV_TEL:
			cd->dlt = tel_ret_linktab(cd->driver_unit);
			break;
#endif

#if NI4BIPR > 0
		case BDRV_IPR:
			cd->dlt = ipr_ret_linktab(cd->driver_unit);
			break;
#endif

#if NI4BISPPP > 0
		case BDRV_ISPPP:
			cd->dlt = i4bisppp_ret_linktab(cd->driver_unit);
			break;
#endif

#if NIBC > 0
		case BDRV_IBC:
			cd->dlt = ibc_ret_linktab(cd->driver_unit);
			break;
#endif

#if NI4BING > 0
		case BDRV_ING:
			cd->dlt = ing_ret_linktab(cd->driver_unit);
			break;
#endif

		default:
			cd->dlt = NULL;
			break;
	}

	if(cd->dlt == NULL || cd->ilt == NULL)
		return(-1);

	if(t >= 0 && t < CTRL_NUMTYPES && ctrl_types[t].set_linktab != NULL)
	{
		ctrl_types[t].set_linktab(
				ctrl_desc[cd->controller].unit,
				cd->channelid,
				cd->dlt);
	}

	switch(cd->driver)
	{
#if NI4BRBCH > 0
		case BDRV_RBCH:
			rbch_set_linktab(cd->driver_unit, cd->ilt);
			break;
#endif

#if NI4BTEL > 0
		case BDRV_TEL:
			tel_set_linktab(cd->driver_unit, cd->ilt);
			break;
#endif

#if NI4BIPR > 0
		case BDRV_IPR:
			ipr_set_linktab(cd->driver_unit, cd->ilt);
			break;
#endif

#if NI4BISPPP > 0
		case BDRV_ISPPP:
			i4bisppp_set_linktab(cd->driver_unit, cd->ilt);
			break;
#endif

#if NIBC > 0
		case BDRV_IBC:
			ibc_set_linktab(cd->driver_unit, cd->ilt);
			break;
#endif

#if NI4BING > 0
		case BDRV_ING:
			ing_set_linktab(cd->driver_unit, cd->ilt);
			break;
#endif

		default:
			return(0);
			break;
	}

	/* activate B channel */
		
	(*cd->ilt->bch_config)(cd->ilt->unit, cd->ilt->channel, cd->bprot, 1);

	return(0);
}

/*---------------------------------------------------------------------------*
 *	unlink a driver(unit) from a B-channel(controller,unit,channel)
 *---------------------------------------------------------------------------*/
static void
i4b_unlink_bchandrvr(call_desc_t *cd)
{
	int t = ctrl_desc[cd->controller].ctrl_type;

	if(t < 0 || t >= CTRL_NUMTYPES || ctrl_types[t].get_linktab == NULL)
	{
		cd->ilt = NULL;
		return;
	}
	else
	{
		cd->ilt = ctrl_types[t].get_linktab(
				ctrl_desc[cd->controller].unit,
				cd->channelid);
	}
	
	/* deactivate B channel */
		
	(*cd->ilt->bch_config)(cd->ilt->unit, cd->ilt->channel, cd->bprot, 0);
} 

/*---------------------------------------------------------------------------

	How shorthold mode works for OUTGOING connections
	=================================================

	|<---- unchecked-window ------->|<-checkwindow->|<-safetywindow>|

idletime_state:      IST_NONCHK             IST_CHECK       IST_SAFE	
	
	|				|		|		|
  time>>+-------------------------------+---------------+---------------+-...
	|				|		|		|
	|				|<--idle_time-->|<--earlyhup--->|
	|<-----------------------unitlen------------------------------->|

	
	  unitlen - specifies the time a charging unit lasts
	idle_time - specifies the thime the line must be idle at the
		    end of the unit to be elected for hangup
	 earlyhup - is the beginning of a timing safety zone before the
		    next charging unit starts

	The algorithm works as follows: lets assume the unitlen is 100
	secons, idle_time is 40 seconds and earlyhup is 10 seconds.
	The line then must be idle 50 seconds after the begin of the
	current unit and it must then be quiet for 40 seconds. if it
	has been quiet for this 40 seconds, the line is closed 10
	seconds before the next charging unit starts. In case there was
	any traffic within the idle_time, the line is not closed.
	It does not matter whether there was any traffic between second
	0 and second 50 or not.


	How shorthold mode works for INCOMING connections
	=================================================

	it is just possible to specify a maximum idle time for incoming
	connections, after this time of no activity on the line the line
	is closed.
	
---------------------------------------------------------------------------*/	

static time_t
i4b_get_idletime(call_desc_t *cd)
{
	switch (cd->driver) {
#if NI4BISPPP > 0
		case BDRV_ISPPP:
			return i4bisppp_idletime(cd->driver_unit);
		break;
#endif
		default:
			return cd->last_active_time;
		break;
	}
}
/*---------------------------------------------------------------------------*
 *	B channel idle check timeout setup
 *---------------------------------------------------------------------------*/ 
static void
i4b_l4_setup_timeout(call_desc_t *cd)
{
	NDBGL4(L4_TIMO, "%ld: direction %d, shorthold algorithm %d",
		(long)SECOND, cd->dir, cd->shorthold_data.shorthold_algorithm);
	
	cd->timeout_active = 0;
	cd->idletime_state = IST_IDLE;
	
	if((cd->dir == DIR_INCOMING) && (cd->max_idle_time > 0))
	{
		/* incoming call: simple max idletime check */
	
		START_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd, hz/2);
		cd->timeout_active = 1;
		NDBGL4(L4_TIMO, "%ld: incoming-call, setup max_idle_time to %ld", (long)SECOND, (long)cd->max_idle_time);
	}
	else if((cd->dir == DIR_OUTGOING) && (cd->shorthold_data.idle_time > 0))
	{
		switch( cd->shorthold_data.shorthold_algorithm )
		{
			default:	/* fall into the old fix algorithm */
			case SHA_FIXU:
				i4b_l4_setup_timeout_fix_unit( cd );
				break;
				
			case SHA_VARU:
				i4b_l4_setup_timeout_var_unit( cd );
				break;
		}
	}
	else
	{
		NDBGL4(L4_TIMO, "no idle_timeout configured");
	}
}

/*---------------------------------------------------------------------------*
 *	fixed unit algorithm B channel idle check timeout setup
 *---------------------------------------------------------------------------*/
static void
i4b_l4_setup_timeout_fix_unit(call_desc_t *cd)
{
	/* outgoing call */
	
	if((cd->shorthold_data.idle_time > 0) && (cd->shorthold_data.unitlen_time == 0))
	{
		/* outgoing call: simple max idletime check */
		
		START_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd, hz/2);
		cd->timeout_active = 1;
		NDBGL4(L4_TIMO, "%ld: outgoing-call, setup idle_time to %ld",
			(long)SECOND, (long)cd->shorthold_data.idle_time);
	}
	else if((cd->shorthold_data.unitlen_time > 0) && (cd->shorthold_data.unitlen_time > (cd->shorthold_data.idle_time + cd->shorthold_data.earlyhup_time)))
	{
		/* outgoing call: full shorthold mode check */
		
		START_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd, hz*(cd->shorthold_data.unitlen_time - (cd->shorthold_data.idle_time + cd->shorthold_data.earlyhup_time)));
		cd->timeout_active = 1;
		cd->idletime_state = IST_NONCHK;
		NDBGL4(L4_TIMO, "%ld: outgoing-call, start %ld sec nocheck window", 
			(long)SECOND, (long)(cd->shorthold_data.unitlen_time - (cd->shorthold_data.idle_time + cd->shorthold_data.earlyhup_time)));

		if(cd->aocd_flag == 0)
		{
			cd->units_type = CHARGE_CALC;
			cd->cunits++;
			i4b_l4_charging_ind(cd);
		}
	}
	else
	{
		/* parms somehow got wrong .. */
		
		NDBGL4(L4_ERR, "%ld: ERROR: idletime[%ld]+earlyhup[%ld] > unitlength[%ld]!",
			(long)SECOND, (long)cd->shorthold_data.idle_time, (long)cd->shorthold_data.earlyhup_time, (long)cd->shorthold_data.unitlen_time);
	}
}

/*---------------------------------------------------------------------------*
 *	variable unit algorithm B channel idle check timeout setup
 *---------------------------------------------------------------------------*/
static void
i4b_l4_setup_timeout_var_unit(call_desc_t *cd)
{
	/* outgoing call: variable unit idletime check */
		
	/*
	 * start checking for an idle connect one second before the end of the unit.
	 * The one second takes into account of rounding due to the driver only
	 * using the seconds and not the uSeconds of the current time
	 */
	cd->idletime_state = IST_CHECK;	/* move directly to the checking state */

	START_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd, hz * (cd->shorthold_data.unitlen_time - 1) );
	cd->timeout_active = 1;
	NDBGL4(L4_TIMO, "%ld: outgoing-call, var idle time - setup to %ld",
		(long)SECOND, (long)cd->shorthold_data.unitlen_time);
}


/*---------------------------------------------------------------------------*
 *	B channel idle check timeout function
 *---------------------------------------------------------------------------*/ 
void
i4b_idle_check(call_desc_t *cd)
{
	int s;

	if(cd->cdid == CDID_UNUSED)
		return;
	
	s = SPLI4B();

	/* failsafe */

	if(cd->timeout_active == 0)
	{
		NDBGL4(L4_ERR, "ERROR: timeout_active == 0 !!!");
	}
	else
	{	
		cd->timeout_active = 0;
	}
	
	/* incoming connections, simple idletime check */

	if(cd->dir == DIR_INCOMING)
	{
		if((i4b_get_idletime(cd) + cd->max_idle_time) <= SECOND)
		{
			NDBGL4(L4_TIMO, "%ld: incoming-call, line idle timeout, disconnecting!", (long)SECOND);
			(*ctrl_desc[cd->controller].N_DISCONNECT_REQUEST)(cd->cdid,
					(CAUSET_I4B << 8) | CAUSE_I4B_NORMAL);
			i4b_l4_idle_timeout_ind(cd);
		}
		else
		{
			NDBGL4(L4_TIMO, "%ld: incoming-call, activity, last_active=%ld, max_idle=%ld", (long)SECOND, (long)i4b_get_idletime(cd), (long)cd->max_idle_time);

			START_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd, hz/2);
			cd->timeout_active = 1;
		}
	}

	/* outgoing connections */

	else if(cd->dir == DIR_OUTGOING)
	{
		switch( cd->shorthold_data.shorthold_algorithm )
		{
			case SHA_FIXU:
				i4b_idle_check_fix_unit( cd );
				break;
			case SHA_VARU:
				i4b_idle_check_var_unit( cd );
				break;
			default:
				NDBGL4(L4_TIMO, "%ld: bad value for shorthold_algorithm of %d",
					(long)SECOND, cd->shorthold_data.shorthold_algorithm);
				i4b_idle_check_fix_unit( cd );
				break;
		}
	}
	splx(s);
}

/*---------------------------------------------------------------------------*
 *	fixed unit algorithm B channel idle check timeout function
 *---------------------------------------------------------------------------*/
static void
i4b_idle_check_fix_unit(call_desc_t *cd)
{

	/* simple idletime calculation */

	if((cd->shorthold_data.idle_time > 0) && (cd->shorthold_data.unitlen_time == 0))
	{
		if((i4b_get_idletime(cd) + cd->shorthold_data.idle_time) <= SECOND)
		{
			NDBGL4(L4_TIMO, "%ld: outgoing-call-st, idle timeout, disconnecting!", (long)SECOND);
			(*ctrl_desc[cd->controller].N_DISCONNECT_REQUEST)(cd->cdid, (CAUSET_I4B << 8) | CAUSE_I4B_NORMAL);
			i4b_l4_idle_timeout_ind(cd);
		}
		else
		{
			NDBGL4(L4_TIMO, "%ld: outgoing-call-st, activity, last_active=%ld, max_idle=%ld",
					(long)SECOND, (long)i4b_get_idletime(cd), (long)cd->shorthold_data.idle_time);
			START_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd, hz/2);
			cd->timeout_active = 1;
		}
	}

	/* full shorthold mode calculation */

	else if((cd->shorthold_data.unitlen_time > 0)
	         && (cd->shorthold_data.unitlen_time > (cd->shorthold_data.idle_time + cd->shorthold_data.earlyhup_time)))
	{
		switch(cd->idletime_state)
		{

		case IST_NONCHK:	/* end of non-check time */

			START_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd, hz*(cd->shorthold_data.idle_time));
			cd->idletimechk_start = SECOND;
			cd->idletime_state = IST_CHECK;
			cd->timeout_active = 1;
			NDBGL4(L4_TIMO, "%ld: outgoing-call, idletime check window reached!", (long)SECOND);
			break;

		case IST_CHECK:		/* end of idletime chk */
			if((i4b_get_idletime(cd) > cd->idletimechk_start) &&
			   (i4b_get_idletime(cd) <= SECOND))
			{	/* activity detected */
				START_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd, hz*(cd->shorthold_data.earlyhup_time));
				cd->timeout_active = 1;
				cd->idletime_state = IST_SAFE;
				NDBGL4(L4_TIMO, "%ld: outgoing-call, activity at %ld, wait earlyhup-end", (long)SECOND, (long)i4b_get_idletime(cd));
			}
			else
			{	/* no activity, hangup */
				NDBGL4(L4_TIMO, "%ld: outgoing-call, idle timeout, last activity at %ld", (long)SECOND, (long)i4b_get_idletime(cd));
				(*ctrl_desc[cd->controller].N_DISCONNECT_REQUEST)(cd->cdid, (CAUSET_I4B << 8) | CAUSE_I4B_NORMAL);
				i4b_l4_idle_timeout_ind(cd);
				cd->idletime_state = IST_IDLE;
			}
			break;

		case IST_SAFE:	/* end of earlyhup time */

			START_TIMER(cd->idle_timeout_handle, i4b_idle_check, cd, hz*(cd->shorthold_data.unitlen_time - (cd->shorthold_data.idle_time+cd->shorthold_data.earlyhup_time)));
			cd->timeout_active = 1;
			cd->idletime_state = IST_NONCHK;

			if(cd->aocd_flag == 0)
			{
				cd->units_type = CHARGE_CALC;
				cd->cunits++;
				i4b_l4_charging_ind(cd);
			}
			
			NDBGL4(L4_TIMO, "%ld: outgoing-call, earlyhup end, wait for idletime start", (long)SECOND);
			break;

		default:
			NDBGL4(L4_ERR, "outgoing-call: invalid idletime_state value!");
			cd->idletime_state = IST_IDLE;
			break;
		}
	}
}

/*---------------------------------------------------------------------------*
 *	variable unit algorithm B channel idle check timeout function
 *---------------------------------------------------------------------------*/
static void
i4b_idle_check_var_unit(call_desc_t *cd)
{
	switch(cd->idletime_state)
	{

	/* see if there has been any activity within the last idle_time seconds */
	case IST_CHECK:
		if( i4b_get_idletime(cd) > (SECOND - cd->shorthold_data.idle_time))
		{	/* activity detected */
			/* check again in one second */		
			cd->idle_timeout_handle =
				START_TIMER (cd->idle_timeout_handle, i4b_idle_check, cd, hz);
			cd->timeout_active = 1;
			cd->idletime_state = IST_CHECK;
			NDBGL4(L4_TIMO, "%ld: outgoing-call, var idle timeout - activity at %ld, continuing", (long)SECOND, (long)i4b_get_idletime(cd));
		}
		else
		{	/* no activity, hangup */
			NDBGL4(L4_TIMO, "%ld: outgoing-call, var idle timeout - last activity at %ld", (long)SECOND, (long)i4b_get_idletime(cd));
			(*ctrl_desc[cd->controller].N_DISCONNECT_REQUEST)(cd->cdid, (CAUSET_I4B << 8) | CAUSE_I4B_NORMAL);
			i4b_l4_idle_timeout_ind(cd);
			cd->idletime_state = IST_IDLE;
		}
		break;

	default:
		NDBGL4(L4_ERR, "outgoing-call: var idle timeout invalid idletime_state value!");
		cd->idletime_state = IST_IDLE;
		break;
	}
}

#endif /* NI4B > 0 */
