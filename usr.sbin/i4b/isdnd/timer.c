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
 *	i4b daemon - timer/timing support routines
 *	------------------------------------------
 *
 *	$Id: timer.c,v 1.19 1999/12/13 21:25:25 hm Exp $ 
 *
 * $FreeBSD: src/usr.sbin/i4b/isdnd/timer.c,v 1.6 1999/12/14 21:07:32 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:49:13 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

static int hr_callgate(void);
static void handle_reserved(cfg_entry_t *cep, time_t now);
static void handle_active(cfg_entry_t *cep, time_t now);
static void recover_illegal(cfg_entry_t *cep);

/*---------------------------------------------------------------------------*
 *	recover from illegal state
 *---------------------------------------------------------------------------*/
static void
recover_illegal(cfg_entry_t *cep)
{
	log(LL_ERR, "recover_illegal: ERROR, entry %s attempting disconnect!", cep->name);
	sendm_disconnect_req(cep, (CAUSET_I4B << 8) | CAUSE_I4B_NORMAL);
	log(LL_ERR, "recover_illegal: ERROR, entry %s - reset state/cdid!", cep->name);
	cep->state = ST_IDLE;
	cep->cdid = CDID_UNUSED;
}

/*---------------------------------------------------------------------------*
 *	start the timer
 *---------------------------------------------------------------------------*/
void
start_timer(cfg_entry_t *cep, int seconds)
{
	cep->timerval = cep->timerremain = seconds;
}

/*---------------------------------------------------------------------------*
 *	stop the timer
 *---------------------------------------------------------------------------*/
void
stop_timer(cfg_entry_t *cep)
{
	cep->timerval = cep->timerremain = 0;	
}

/*---------------------------------------------------------------------------*
 *	callgate for handle_recovery()
 *---------------------------------------------------------------------------*/
static int
hr_callgate(void)
{
	static int tv_first = 1;
	static struct timeval tv_last;
	struct timeval tv_now;
	
	/* there must be 1 sec minimum between calls to this section */
	
	if(tv_first)
	{
		gettimeofday(&tv_last, NULL);
		tv_first = 0;
	}
	
	gettimeofday(&tv_now, NULL);
	
	if((tv_now.tv_sec - tv_last.tv_sec) < 1)
	{
	
		DBGL(DL_TIME, (log(LL_DBG, "time < 1 - last %ld:%ld now %ld:%ld",
				tv_last.tv_sec, tv_last.tv_usec,
				tv_now.tv_sec, tv_now.tv_usec)));
		return(1);
	}
	else if((tv_now.tv_sec - tv_last.tv_sec) == 1)
	{
		if(((1000000 - tv_last.tv_usec) + tv_now.tv_usec) < 900000)
		{
			DBGL(DL_TIME, (log(LL_DBG, "time < 900000us - last %ld:%ld now %ld:%ld",
					tv_last.tv_sec, tv_last.tv_usec,
					tv_now.tv_sec, tv_now.tv_usec)));
			return(1);
		}
	}
	
	DBGL(DL_TIME, (log(LL_DBG, "time OK! - last %ld:%ld now %ld:%ld",
			tv_last.tv_sec, tv_last.tv_usec,
			tv_now.tv_sec, tv_now.tv_usec)));
	
	gettimeofday(&tv_last, NULL);
	
	return(0);
}	 

/*---------------------------------------------------------------------------*
 *	timeout, recovery and retry handling
 *---------------------------------------------------------------------------*/
void
handle_recovery(void)
{
	cfg_entry_t *cep = NULL;
	int i;
	time_t now;
	
	if(hr_callgate())	/* last call to handle_recovery < 1 sec ? */
		return;		/* yes, exit */
	
	now = time(NULL);	/* get current time */
	
	/* walk thru all entries, look for work to do */
	
	for(i=0; i < nentries; i++)
	{
		cep = &cfg_entry_tab[i];	/* ptr to config entry */
	
		switch(cep->cdid)
		{
			case CDID_UNUSED:		/* entry unused */
				continue;
				break;
	
			case CDID_RESERVED:		/* entry reserved */
				handle_reserved(cep, now);
				break;
	
			default:			/* entry in use */
				handle_active(cep, now);
				break;
		}
	}
}		

/*---------------------------------------------------------------------------*
 *	timeout, recovery and retry handling for active entry
 *---------------------------------------------------------------------------*/
static void
handle_active(cfg_entry_t *cep, time_t now)
{
	switch(cep->state)
	{
		case ST_ACCEPTED:
			if(cep->timerval && (--(cep->timerremain)) <= 0)
			{
				DBGL(DL_RCVRY, (log(LL_DBG, "handle_active: entry %s, TIMEOUT !!!", cep->name)));
				cep->timerval = cep->timerremain = 0;
				next_state(cep, EV_TIMO);
			}
			break;
			
		case ST_ALERT:
			if(cep->alert_time > 0)
			{
				cep->alert_time--;
			}
			else
			{
				log(LL_CHD, "%05d %s answering: incoming call from %s to %s",
					cep->cdid, cep->name, 
					cep->real_phone_incoming,
					cep->local_phone_incoming);
				next_state(cep, EV_MCI);
			}
			break;
				
		case ST_ILL:
			recover_illegal(cep);
			break;
			
		default:
			/* check hangup flag: if active, close connection */

			if(cep->hangup)
			{
				DBGL(DL_RCVRY, (log(LL_DBG, "handle_active: entry %s, hangup request!", cep->name)));
				cep->hangup = 0;
				next_state(cep, EV_DRQ);
			}

			/*
			 * if shorthold mode is rates based, check if
		         * we entered a time with a new unit length
		         */

			if(cep->unitlengthsrc == ULSRC_RATE)
			{
				int connecttime = (int)difftime(now, cep->connect_time);

				if((connecttime > 1) &&
				   (connecttime % 60))
				{
					int newrate = get_current_rate(cep, 0);
	
					if(newrate != cep->unitlength)
					{
						DBGL(DL_MSG, (log(LL_DBG, "handle_active: rates unit length updated %d -> %d", cep->unitlength, newrate)));
			
						cep->unitlength = newrate;
	
						unitlen_chkupd(cep);
					}
				}
			}
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	timeout, recovery and retry handling for reserved entry
 *---------------------------------------------------------------------------*/
static void
handle_reserved(cfg_entry_t *cep, time_t now)
{
	time_t waittime;
	
	switch(cep->state)
	{	
		case ST_DIALRTMRCHD:	/* wait for dial retry time reached */
	
			if(cep->dialrandincr)
				waittime = cep->randomtime;
			else
                		waittime = cep->recoverytime;
	
	                		
			if(now > (cep->last_dial_time + waittime))
			{
				DBGL(DL_RCVRY, (log(LL_DBG, "handle_reserved: entry %s, dial retry request!", cep->name)));
				cep->state = ST_DIALRETRY;
	
				if((cep->cdid = get_cdid()) == 0)
				{
					log(LL_ERR, "handle_reserved: dialretry get_cdid() returned 0!");
					cep->state = ST_IDLE;
					cep->cdid = CDID_UNUSED;
					return;
				}

				if((setup_dialout(cep)) == GOOD)
				{
					sendm_connect_req(cep);
				}
				else
				{
					log(LL_ERR, "handle_reserved: dialretry setup_dialout returned ERROR!");
					cep->state = ST_IDLE;
					cep->cdid = CDID_UNUSED;
					return;
				}					
			}
			break;
			
		
		case ST_ACB_WAITDIAL: 	/* active callback wait for time between disconnect and dial */
	
			if(now > (cep->last_release_time + cep->callbackwait))
			{
				DBGL(DL_RCVRY, (log(LL_DBG, "handle_reserved: entry %s, callback dial!", cep->name)));
				cep->state = ST_ACB_DIAL;
	
				if((cep->cdid = get_cdid()) == 0)
				{
					log(LL_ERR, "handle_reserved: callback get_cdid() returned 0!");
					cep->state = ST_IDLE;
					cep->cdid = CDID_UNUSED;
					return;
				}

				select_first_dialno(cep);

				if((setup_dialout(cep)) == GOOD)
				{
					sendm_connect_req(cep);
				}
				else
				{
					log(LL_ERR, "handle_reserved: callback setup_dialout returned ERROR!");
					cep->state = ST_IDLE;
					cep->cdid = CDID_UNUSED;
					return;
				}					
			}
			break;
	
		case ST_ACB_DIALFAIL:	/* callback to remote failed */
	
			if(cep->dialrandincr)
				waittime = cep->randomtime + cep->recoverytime;
			else
                		waittime = cep->recoverytime;
	
			if(now > (cep->last_release_time + waittime))
			{
				DBGL(DL_RCVRY, (log(LL_DBG, "handle_reserved: entry %s, callback dial retry request!", cep->name)));
				cep->state = ST_ACB_DIAL;
	
				if((cep->cdid = get_cdid()) == 0)
				{
					log(LL_ERR, "handle_reserved: callback dialretry get_cdid() returned 0!");
					cep->state = ST_IDLE;
					cep->cdid = CDID_UNUSED;
					return;
				}

				if((setup_dialout(cep)) == GOOD)
				{
					sendm_connect_req(cep);
				}
				else
				{
					log(LL_ERR, "handle_reserved: callback dialretry setup_dialout returned ERROR!");
					cep->state = ST_IDLE;
					cep->cdid = CDID_UNUSED;
					return;
				}					
			}
			break;
	
		case ST_PCB_WAITCALL:	/* wait for remote calling back */

			if(now > (cep->last_release_time + cep->calledbackwait))
			{
				cep->dial_count++;
	
				if(cep->dial_count < cep->dialretries)
				{
					/* inside normal retry cycle */
	
					DBGL(DL_RCVRY, (log(LL_DBG, "handle_reserved: entry %s, retry calledback dial #%d!",
						cep->name, cep->dial_count)));
					cep->state = ST_PCB_DIAL;
	
					if((cep->cdid = get_cdid()) == 0)
					{
						log(LL_ERR, "handle_reserved: calledback get_cdid() returned 0!");
						cep->state = ST_IDLE;
						cep->cdid = CDID_UNUSED;
						return;
					}
					select_next_dialno(cep);

					if((setup_dialout(cep)) == GOOD)
					{
						sendm_connect_req(cep);
					}
					else
					{
						log(LL_ERR, "handle_reserved: calledback setup_dialout returned ERROR!");
						cep->state = ST_IDLE;
						cep->cdid = CDID_UNUSED;
						return;
					}					
				}
				else
				{
					/* retries exhausted */
	
					DBGL(DL_RCVRY, (log(LL_DBG, "handle_reserved: calledback dial retries exhausted")));
					dialresponse(cep, DSTAT_TFAIL);
					cep->cdid = CDID_UNUSED;
					cep->dial_count = 0;
					cep->state = ST_IDLE;
				}
			}
			break;
			
		case ST_DOWN:	/* interface was taken down */

			if(now > (cep->went_down_time + cep->downtime))
			{
				DBGL(DL_RCVRY, (log(LL_DBG, "handle_reserved: taking %s%d up", bdrivername(cep->usrdevicename), cep->usrdeviceunit)));
				if_up(cep);
				cep->state = ST_IDLE;
				cep->cdid = CDID_UNUSED;
			}
			break;
			
		case ST_ILL:	/* illegal state reached, recover ! */
		
			recover_illegal(cep);
			break;
	}
}

/* EOF */
