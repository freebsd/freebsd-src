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
 *	i4b_l4if.c - Layer 3 interface to Layer 4
 *	-------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sat Mar  9 19:36:08 2002]
 *
 *---------------------------------------------------------------------------*/

#include "i4bq931.h"

#if NI4BQ931 > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_cause.h>

#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer3/i4b_l3.h>
#include <i4b/layer3/i4b_l3fsm.h>

#include <i4b/layer4/i4b_l4.h>

extern void isic_settrace(int unit, int val);		/*XXX*/
extern int isic_gettrace(int unit);			/*XXX*/

static void n_connect_request(u_int cdid);
static void n_connect_response(u_int cdid, int response, int cause);
static void n_disconnect_request(u_int cdid, int cause);
static void n_alert_request(u_int cdid);
static void n_mgmt_command(int unit, int cmd, void *parm);

/*---------------------------------------------------------------------------*
 *	i4b_mdl_status_ind - status indication from lower layers
 *---------------------------------------------------------------------------*/
int
i4b_mdl_status_ind(int unit, int status, int parm)
{
	int sendup;
	int i;
	
	NDBGL3(L3_MSG, "unit = %d, status = %d, parm = %d", unit, status, parm);

	switch(status)
	{
		case STI_ATTACH:
			NDBGL3(L3_MSG, "STI_ATTACH: attaching unit %d to controller %d", unit, nctrl);
		
			/* init function pointers */
			
			ctrl_desc[nctrl].N_CONNECT_REQUEST = n_connect_request;
			ctrl_desc[nctrl].N_CONNECT_RESPONSE = n_connect_response;
			ctrl_desc[nctrl].N_DISCONNECT_REQUEST = n_disconnect_request;
			ctrl_desc[nctrl].N_ALERT_REQUEST = n_alert_request;	
			ctrl_desc[nctrl].N_DOWNLOAD = NULL;	/* only used by active cards */
			ctrl_desc[nctrl].N_DIAGNOSTICS = NULL;	/* only used by active cards */
			ctrl_desc[nctrl].N_MGMT_COMMAND = n_mgmt_command;
		
			/* init type and unit */
			
			ctrl_desc[nctrl].unit = unit;
			ctrl_desc[nctrl].ctrl_type = CTRL_PASSIVE;
			ctrl_desc[nctrl].card_type = parm;
		
			/* state fields */
		
			ctrl_desc[nctrl].dl_est = DL_DOWN;
			ctrl_desc[nctrl].nbch = 2; /* XXX extra param? */
			for (i = 0; i < ctrl_desc[nctrl].nbch; i++)
			    ctrl_desc[nctrl].bch_state[i] = BCH_ST_FREE;

			ctrl_desc[nctrl].tei = -1;
			
			/* init unit to controller table */
			
			utoc_tab[unit] = nctrl;
			
			/* increment no. of controllers */
			
			nctrl++;

			break;
			
		case STI_L1STAT:
			i4b_l4_l12stat(unit, 1, parm);
			NDBGL3(L3_MSG, "STI_L1STAT: unit %d layer 1 = %s", unit, status ? "up" : "down");
			break;
			
		case STI_L2STAT:
			i4b_l4_l12stat(unit, 2, parm);
			NDBGL3(L3_MSG, "STI_L2STAT: unit %d layer 2 = %s", unit, status ? "up" : "down");
			break;

		case STI_TEIASG:
			ctrl_desc[unit].tei = parm;
			i4b_l4_teiasg(unit, parm);
			NDBGL3(L3_MSG, "STI_TEIASG: unit %d TEI = %d = 0x%02x", unit, parm, parm);
			break;

		case STI_PDEACT:	/* L1 T4 timeout */
			NDBGL3(L3_ERR, "STI_PDEACT: unit %d TEI = %d = 0x%02x", unit, parm, parm);

			sendup = 0;

			for(i=0; i < N_CALL_DESC; i++)
			{
				if( (ctrl_desc[call_desc[i].controller].ctrl_type == CTRL_PASSIVE) &&
				    (ctrl_desc[call_desc[i].controller].unit == unit))
                		{
					i4b_l3_stop_all_timers(&(call_desc[i]));
					if(call_desc[i].cdid != CDID_UNUSED)
						sendup++;
				}
			}

			ctrl_desc[utoc_tab[unit]].dl_est = DL_DOWN;
			for (i = 0; i < ctrl_desc[utoc_tab[unit]].nbch; i++)
			    ctrl_desc[utoc_tab[unit]].bch_state[i] = BCH_ST_FREE;
			ctrl_desc[utoc_tab[unit]].tei = -1;

			if(sendup)
			{
				i4b_l4_pdeact(unit, sendup);
				call_desc[i].cdid = CDID_UNUSED;
			}
			break;

		case STI_NOL1ACC:	/* no outgoing access to S0 */
			NDBGL3(L3_ERR, "STI_NOL1ACC: unit %d no outgoing access to S0", unit);

			for(i=0; i < N_CALL_DESC; i++)
			{
				if( (ctrl_desc[call_desc[i].controller].ctrl_type == CTRL_PASSIVE) &&
				    (ctrl_desc[call_desc[i].controller].unit == unit))
                		{
					if(call_desc[i].cdid != CDID_UNUSED)
					{
						SET_CAUSE_TYPE(call_desc[i].cause_in, CAUSET_I4B);
						SET_CAUSE_VAL(call_desc[i].cause_in, CAUSE_I4B_L1ERROR);
						i4b_l4_disconnect_ind(&(call_desc[i]));
					}
				}
			}

			ctrl_desc[utoc_tab[unit]].dl_est = DL_DOWN;
			for (i = 0; i < ctrl_desc[utoc_tab[unit]].nbch; i++)
			    ctrl_desc[utoc_tab[unit]].bch_state[i] = BCH_ST_FREE;
			ctrl_desc[utoc_tab[unit]].tei = -1;
			break;

		default:
			NDBGL3(L3_ERR, "ERROR, unit %d, unknown status value %d!", unit, status);
			break;
	}		
	return(0);
}

/*---------------------------------------------------------------------------*
 *	send command to the lower layers
 *---------------------------------------------------------------------------*/
static void
n_mgmt_command(int unit, int cmd, void *parm)
{
	int i;

	switch(cmd)
	{
		case CMR_DOPEN:
			NDBGL3(L3_MSG, "CMR_DOPEN for unit %d", unit);
			
			for(i=0; i < N_CALL_DESC; i++)
			{
				if( (ctrl_desc[call_desc[i].controller].ctrl_type == CTRL_PASSIVE) &&
				    (ctrl_desc[call_desc[i].controller].unit == unit))
                		{
                			call_desc[i].cdid = CDID_UNUSED;
				}
			}

			ctrl_desc[utoc_tab[unit]].dl_est = DL_DOWN;
			for (i = 0; i < ctrl_desc[utoc_tab[unit]].nbch; i++)
			    ctrl_desc[utoc_tab[unit]].bch_state[i] = BCH_ST_FREE;
			ctrl_desc[utoc_tab[unit]].tei = -1;
			break;

		case CMR_DCLOSE:
			NDBGL3(L3_MSG, "CMR_DCLOSE for unit %d", unit);
			break;
			
		case CMR_SETTRACE:
			NDBGL3(L3_MSG, "CMR_SETTRACE for unit %d", unit);
			break;
			
		default:
			NDBGL3(L3_MSG, "unknown cmd %d for unit %d", cmd, unit);
			break;
	}

	MDL_Command_Req(unit, cmd, parm);
	
}

/*---------------------------------------------------------------------------*
 *	handle connect request message from userland
 *---------------------------------------------------------------------------*/
static void
n_connect_request(u_int cdid)
{
	call_desc_t *cd;

	cd = cd_by_cdid(cdid);

	next_l3state(cd, EV_SETUPRQ);	
}

/*---------------------------------------------------------------------------*
 *	handle setup response message from userland
 *---------------------------------------------------------------------------*/
static void
n_connect_response(u_int cdid, int response, int cause)
{
	call_desc_t *cd;
	int chstate;

	cd = cd_by_cdid(cdid);

	T400_stop(cd);
	
	cd->response = response;
	cd->cause_out = cause;

	switch(response)
	{
		case SETUP_RESP_ACCEPT:
			next_l3state(cd, EV_SETACRS);
			chstate = BCH_ST_USED;
			break;
		
		case SETUP_RESP_REJECT:
			next_l3state(cd, EV_SETRJRS);
			chstate = BCH_ST_FREE;
			break;
			
		case SETUP_RESP_DNTCRE:
			next_l3state(cd, EV_SETDCRS);
			chstate = BCH_ST_FREE;
			break;

		default:	/* failsafe */
			next_l3state(cd, EV_SETDCRS);
			chstate = BCH_ST_FREE;
			NDBGL3(L3_ERR, "unknown response, doing SETUP_RESP_DNTCRE");
			break;
	}

	if((cd->channelid >= 0) && (cd->channelid < ctrl_desc[cd->controller].nbch))
	{
		ctrl_desc[cd->controller].bch_state[cd->channelid] = chstate;
	}
	else
	{
		NDBGL3(L3_MSG, "Warning, invalid channelid %d, response = %d\n", cd->channelid, response);
	}
}

/*---------------------------------------------------------------------------*
 *	handle disconnect request message from userland
 *---------------------------------------------------------------------------*/
static void
n_disconnect_request(u_int cdid, int cause)
{
	call_desc_t *cd;

	cd = cd_by_cdid(cdid);

	cd->cause_out = cause;

	next_l3state(cd, EV_DISCRQ);
}

/*---------------------------------------------------------------------------*
 *	handle alert request message from userland
 *---------------------------------------------------------------------------*/
static void
n_alert_request(u_int cdid)
{
	call_desc_t *cd;

	cd = cd_by_cdid(cdid);

	next_l3state(cd, EV_ALERTRQ);
}

#endif /* NI4BQ931 > 0 */
