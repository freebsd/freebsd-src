/*
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 *	FSM for isdnd
 *	-------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sat Jul 21 18:25:48 2001]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

/* table of state descriptions */

static char *state_text[N_STATES] = {
	"idle",
	"dialing",
	"waitdialretry",
	"dialretry",
	
	"pcb-dialing",
	"pcb-dialfail",
	"pcb-waitcall",

	"acb-waitdisc",
	"acb-waitdial",
	"acb-dialing",
	"acb-dialfail",

	"accepted",
	"connected",
	"waitdisconnect",	
	"down",
	"alert",

	"Illegal State"	
};

/* table of event descriptions */

static char *event_text[N_EVENTS] = {

	/* incoming messages */
	
	"msg-con-ind",
	"msg-con-act-ind",
	"msg-disc-ind",
	"msg-dialout",

	/* local events */
	
	"timeout",
	"disconnect-req",
	"callback-req",
	"alert-req",
	
	/* illegal */
	
	"Illegal Event"
};

/*---------------------------------------------------------------------------*
 *	illegal state default action
 *---------------------------------------------------------------------------*/	
static void
F_ill(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_ill: Illegal State reached !!!")));
}

/*---------------------------------------------------------------------------*
 *	No change, No action
 *---------------------------------------------------------------------------*/	
static void
F_NcNa(cfg_entry_t *cep)
{
}

/*---------------------------------------------------------------------------*
 *	incoming CONNECT, accepting call
 *---------------------------------------------------------------------------*/	
static void
F_MCI(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_MCI: tx SETUP_RESP_ACCEPT")));
	sendm_connect_resp(cep, cep->cdid, SETUP_RESP_ACCEPT, 0);
	start_timer(cep, TIMEOUT_CONNECT_ACTIVE);
}

/*---------------------------------------------------------------------------*
 *	incoming connect active, call is now active
 *---------------------------------------------------------------------------*/	
static void
F_MCAI(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_MCAI: Connection active!")));

	stop_timer(cep);

	if((cep->dialin_reaction == REACT_ANSWER) &&
	   (cep->b1protocol == BPROT_NONE))
	{
		exec_answer(cep);
	}
}

/*---------------------------------------------------------------------------*
 *	timeout
 *---------------------------------------------------------------------------*/	
static void
F_TIMO(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_TIMO: Timout occured!")));
	sendm_disconnect_req(cep, (CAUSET_I4B << 8) | CAUSE_I4B_NORMAL);
	cep->cdid = CDID_UNUSED;	
}

/*---------------------------------------------------------------------------*
 *	incoming disconnect indication
 *---------------------------------------------------------------------------*/	
static void
F_IDIS(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_IDIS: disconnect indication")));
	cep->cdid = CDID_UNUSED;
}

/*---------------------------------------------------------------------------*
 *	local disconnect request
 *---------------------------------------------------------------------------*/	
static void
F_DRQ(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_DRQ: local disconnect request")));
	sendm_disconnect_req(cep, (CAUSET_I4B << 8) | CAUSE_I4B_NORMAL);
}

/*---------------------------------------------------------------------------*
 *	disconnect indication after local disconnect req
 *---------------------------------------------------------------------------*/	
static void
F_MDI(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_MDI: disconnect indication, local disconnected")));
	cep->cdid = CDID_UNUSED;
}

/*---------------------------------------------------------------------------*
 *	local requested outgoing dial
 *---------------------------------------------------------------------------*/	
static void
F_DIAL(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_DIAL: local dial out request")));

        if(cep->dialrandincr)
                cep->randomtime = (random() & RANDOM_MASK) + cep->recoverytime;

	cep->dial_count = 0;
		
	select_first_dialno(cep);

	sendm_connect_req(cep);
}

/*---------------------------------------------------------------------------*
 *	outgoing dial successfull
 *---------------------------------------------------------------------------*/	
static void
F_DOK(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_DOK: dial out ok")));
	select_this_dialno(cep);
}

/*---------------------------------------------------------------------------*
 *	outgoing dial fail (ST_SUSE !!!)
 *---------------------------------------------------------------------------*/	
static void
F_DFL(cfg_entry_t *cep)
{
	cep->last_release_time = time(NULL);
	
	if(cep->dialouttype == DIALOUT_NORMAL)
	{
		cep->dial_count++;
	
		if(cep->dial_count < cep->dialretries || cep->dialretries == -1) /* Added by FST <mailto:fsteevie@dds.nl> for unlimited dialing (sorry, but I needed it) */
		{
			/* inside normal retry cycle */
		
			DBGL(DL_STATE, (log(LL_DBG, "F_DFL: dial fail, dial retry")));
			select_next_dialno(cep);
			cep->cdid = CDID_RESERVED;
			cep->state = ST_DIALRTMRCHD;
			return;
		}
	
		/* retries exhausted */
		
		if(!cep->usedown)
		{
			DBGL(DL_STATE, (log(LL_DBG, "F_DFL: dial retry fail, dial retries exhausted")));
			dialresponse(cep, DSTAT_TFAIL);
			cep->cdid = CDID_UNUSED;
			cep->dial_count = 0;
			cep->state = ST_IDLE;
			return;
		}
	
		/* interface up/down active */
	
		cep->down_retry_count++;
	
		if(cep->down_retry_count > cep->downtries)
		{
			/* set interface down */
			DBGL(DL_STATE, (log(LL_DBG, "F_DFL: dial retry cycle fail, setting interface down!")));
			dialresponse(cep, DSTAT_PFAIL);
			if_down(cep);					
			cep->state = ST_DOWN;
		}
		else
		{
			/* enter new dial retry cycle */
			DBGL(DL_STATE, (log(LL_DBG, "F_DFL: dial retry cycle fail, enter new retry cycle!")));
			select_next_dialno(cep);
			cep->state = ST_DIALRTMRCHD;
		}
	
		cep->dial_count = 0;
		cep->cdid = CDID_RESERVED;
	}
	else	/* cdp->dialouttype == DIALOUT_CALLEDBACK */
	{
		DBGL(DL_STATE, (log(LL_DBG, "F_DFL: calledback dial done, wait for incoming call")));
		cep->cdid = CDID_RESERVED;
		cep->state = ST_PCB_WAITCALL;
	}
}

/*---------------------------------------------------------------------------*
 *	local requested outgoing dial
 *---------------------------------------------------------------------------*/	
static void
F_ACBW(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_ACBW: local callback, wait callback recovery time")));

        if(cep->dialrandincr)
                cep->randomtime = (random() & RANDOM_MASK) + cep->recoverytime;

	cep->dial_count = 0;

	cep->cdid = CDID_RESERVED;
}

/*---------------------------------------------------------------------------*
 *	active callback dialout retry (ST_SUSE !!!)
 *---------------------------------------------------------------------------*/	
static void
F_ACBR(cfg_entry_t *cep)
{	
	cep->dial_count++;

	if(cep->dial_count < cep->dialretries || cep->dialretries == -1) /* Added by FST <mailto:fsteevie@dds.nl> for unlimited dialing (sorry, but I needed it) */
	{
		/* inside normal retry cycle */
	
		DBGL(DL_STATE, (log(LL_DBG, "F_ACBR: dial fail, dial retry")));
		select_next_dialno(cep);
		cep->cdid = CDID_RESERVED;
		cep->state = ST_ACB_DIALFAIL;
		return;
	}

	/* retries exhausted */
	
	if(!cep->usedown)
	{
		DBGL(DL_STATE, (log(LL_DBG, "F_ACBR: dial retry fail, dial retries exhausted")));
		dialresponse(cep, DSTAT_TFAIL);
		cep->cdid = CDID_UNUSED;
		cep->dial_count = 0;
		cep->state = ST_IDLE;
		return;
	}

	/* interface up/down active */

	cep->down_retry_count++;

	if(cep->down_retry_count > cep->downtries)
	{
		/* set interface down */
		DBGL(DL_STATE, (log(LL_DBG, "F_ACBR: dial retry cycle fail, setting interface down!")));
		dialresponse(cep, DSTAT_PFAIL);
		if_down(cep);
		cep->state = ST_DOWN;
	}
	else
	{
		/* enter new dial retry cycle */
		DBGL(DL_STATE, (log(LL_DBG, "F_ACBR: dial retry cycle fail, enter new retry cycle!")));
		select_next_dialno(cep);
		cep->state = ST_ACB_DIALFAIL;
	}

	cep->dial_count = 0;
	cep->cdid = CDID_RESERVED;	
}

/*---------------------------------------------------------------------------*
 *	local requested to send ALERT message
 *---------------------------------------------------------------------------*/	
static void
F_ALRT(cfg_entry_t *cep)
{
	DBGL(DL_STATE, (log(LL_DBG, "F_ALRT: local send alert request")));

	cep->alert_time = cep->alert;
	
	sendm_alert_req(cep);
}

/*---------------------------------------------------------------------------*
 *	isdn daemon state transition table
 *---------------------------------------------------------------------------*/	
struct state_tab {
	void(*func)(cfg_entry_t *cep);		/* function to execute */
	int newstate;				/* next state */
} state_tab[N_EVENTS][N_STATES] = {

/* STATE:	ST_IDLE			ST_DIAL			ST_DIALRTMRCHD		ST_DIALRETRY		ST_PCB_DIAL		ST_PCB_DIALFAIL		ST_PCB_WAITCALL		ST_ACB_WAITDISC		ST_ACB_WAITDIAL 	ST_ACB_DIAL		ST_ACB_DIALFAIL		ST_ACCEPTED		ST_CONNECTED		ST_WAITDISCI		ST_DOWN			ST_ALERT		ST_ILLEGAL		*/
/* -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* messages */
/* EV_MCI   */{{F_MCI, ST_ACCEPTED},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_MCI, ST_ACCEPTED},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_MCI, ST_ACCEPTED},   {F_ill, ST_ILL}},
/* EV_MCAI  */{{F_ill, ST_ILL},		{F_DOK, ST_CONNECTED},	{F_ill, ST_ILL},	{F_DOK, ST_CONNECTED},	{F_DOK, ST_CONNECTED},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_DOK, ST_CONNECTED},	{F_ill, ST_ILL},	{F_MCAI,ST_CONNECTED},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},        {F_ill, ST_ILL}},
/* EV_MDI   */{{F_ill, ST_ILL},		{F_DFL, ST_SUSE},	{F_ill, ST_ILL},	{F_DFL, ST_SUSE},	{F_DFL, ST_SUSE},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ACBW,ST_ACB_WAITDIAL},{F_ill, ST_ILL},	{F_ACBR, ST_SUSE},	{F_ACBR,ST_SUSE},	{F_IDIS,ST_IDLE},	{F_IDIS,ST_IDLE},	{F_MDI, ST_IDLE},	{F_ill, ST_ILL},	{F_MDI, ST_IDLE},       {F_ill, ST_ILL}},
/* EV_MDO   */{{F_DIAL,ST_DIAL},	{F_NcNa,ST_DIAL},	{F_NcNa,ST_DIALRTMRCHD},{F_NcNa,ST_DIALRETRY},	{F_NcNa,ST_PCB_DIAL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},        {F_ill, ST_ILL}},

/* local requests */
/* EV_TIMO  */{{F_ill, ST_ILL},		{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_TIMO,ST_IDLE},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},        {F_ill, ST_ILL}},
/* EV_DRQ   */{{F_NcNa, ST_IDLE},		{F_DRQ, ST_WAITDISCI},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_DRQ, ST_WAITDISCI},	{F_NcNa,ST_WAITDISCI},	{F_NcNa, ST_DOWN},	{F_ill, ST_ILL},        {F_ill, ST_ILL}},
/* EV_CBRQ  */{{F_NcNa,ST_ACB_WAITDIAL},{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},       {F_NcNa,ST_ACB_WAITDIAL},{F_NcNa, ST_ACB_DIAL}, {F_NcNa,ST_ACB_DIALFAIL},{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},        {F_ill, ST_ILL}},
/* EV_ALRT  */{{F_ALRT,ST_ALERT},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},        {F_ill, ST_ILL}},

/* illegal  */

/* EV_ILL   */{{F_ill, ST_ILL},		{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},	{F_ill, ST_ILL},        {F_ill, ST_ILL}}
};

/*---------------------------------------------------------------------------*
 *	event handler
 *---------------------------------------------------------------------------*/	
void
next_state(cfg_entry_t *cep, int event)
{
	int currstate, newstate;

	if(event > N_EVENTS)
	{
		log(LL_ERR, "next_state: event > N_EVENTS");
		error_exit(1, "next_state: event > N_EVENTS");
	}

	currstate = cep->state;

	if(currstate > N_STATES)
	{
		log(LL_ERR, "next_state: currstate > N_STATES");
		error_exit(1, "next_state: currstate > N_STATES");
	}

	newstate = state_tab[event][currstate].newstate;

	if(newstate > N_STATES)
	{
		log(LL_ERR, "next_state: newstate > N_STATES");
		error_exit(1, "next_state: newstate > N_STATES");
	}

	if(newstate != ST_SUSE)
	{
		DBGL(DL_STATE, (log(LL_DBG, "FSM event [%s]: [%s => %s]", event_text[event],
				state_text[currstate],
				state_text[newstate])));
	}

        (*state_tab[event][currstate].func)(cep);

	if(newstate == ST_ILL)
	{
		log(LL_ERR, "FSM ILLEGAL STATE, event=%s: oldstate=%s => newstate=%s]",
				event_text[event],
                                state_text[currstate],
                                state_text[newstate]);
	}

	if(newstate == ST_SUSE)
	{
		DBGL(DL_STATE, (log(LL_DBG, "FSM (SUSE) event [%s]: [%s => %s]", event_text[event],
				state_text[currstate],
				state_text[cep->state])));
	}
	else
	{
		cep->state = newstate;
	}
}

/*---------------------------------------------------------------------------*
 *	return pointer to current state description
 *---------------------------------------------------------------------------*/	
char *
printstate(cfg_entry_t *cep)
{
	return((char *) state_text[cep->state]);
}

/* EOF */
