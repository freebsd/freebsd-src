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
 *	i4b_l3timer.c - timer and timeout handling for layer 3
 *	------------------------------------------------------
 *      last edit-date: [Sat Mar  9 19:35:31 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer3/i4b_l3.h>
#include <i4b/layer3/i4b_l3fsm.h>

/*---------------------------------------------------------------------------*
 *	stop all layer 3 timers
 *---------------------------------------------------------------------------*/
void i4b_l3_stop_all_timers(call_desc_t *cd)
{
	T303_stop(cd);
	T305_stop(cd);
	T308_stop(cd);
	T309_stop(cd);
	T310_stop(cd);
	T313_stop(cd);	
}
	
/*---------------------------------------------------------------------------*
 *	timer T303 timeout function
 *---------------------------------------------------------------------------*/
static void
T303_timeout(call_desc_t *cd)
{
	NDBGL3(L3_T_ERR, "SETUP not answered, cr = %d", cd->cr);
	next_l3state(cd, EV_T303EXP);
}

/*---------------------------------------------------------------------------*
 *	timer T303 start
 *---------------------------------------------------------------------------*/
void
T303_start(call_desc_t *cd)
{
	if (cd->T303 == TIMER_ACTIVE)
		return;
		
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
	cd->T303 = TIMER_ACTIVE;

	START_TIMER(cd->T303_callout, T303_timeout, cd, T303VAL);
}

/*---------------------------------------------------------------------------*
 *	timer T303 stop
 *---------------------------------------------------------------------------*/
void
T303_stop(call_desc_t *cd)
{
	CRIT_VAR;
	CRIT_BEG;
	
	if(cd->T303 != TIMER_IDLE)
	{
		STOP_TIMER(cd->T303_callout, T303_timeout, cd);
		cd->T303 = TIMER_IDLE;
	}
	CRIT_END;
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
}

/*---------------------------------------------------------------------------*
 *	timer T305 timeout function
 *---------------------------------------------------------------------------*/
static void
T305_timeout(call_desc_t *cd)
{
	NDBGL3(L3_T_ERR, "DISC not answered, cr = %d", cd->cr);
	next_l3state(cd, EV_T305EXP);
}

/*---------------------------------------------------------------------------*
 *	timer T305 start
 *---------------------------------------------------------------------------*/
void
T305_start(call_desc_t *cd)
{
	if (cd->T305 == TIMER_ACTIVE)
		return;
		
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
	cd->T305 = TIMER_ACTIVE;

	START_TIMER(cd->T305_callout, T305_timeout, cd, T305VAL);
}

/*---------------------------------------------------------------------------*
 *	timer T305 stop
 *---------------------------------------------------------------------------*/
void
T305_stop(call_desc_t *cd)
{
	CRIT_VAR;
	CRIT_BEG;
	
	if(cd->T305 != TIMER_IDLE)
	{
		STOP_TIMER(cd->T305_callout, T305_timeout, cd);
		cd->T305 = TIMER_IDLE;
	}
	CRIT_END;
	
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
}

/*---------------------------------------------------------------------------*
 *	timer T308 timeout function
 *---------------------------------------------------------------------------*/
static void
T308_timeout(call_desc_t *cd)
{
	NDBGL3(L3_T_ERR, "REL not answered, cr = %d", cd->cr);
	next_l3state(cd, EV_T308EXP);
}

/*---------------------------------------------------------------------------*
 *	timer T308 start
 *---------------------------------------------------------------------------*/
void
T308_start(call_desc_t *cd)
{
	if(cd->T308 == TIMER_ACTIVE)
		return;
		
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
	cd->T308 = TIMER_ACTIVE;

	START_TIMER(cd->T308_callout, T308_timeout, cd, T308VAL);
}

/*---------------------------------------------------------------------------*
 *	timer T308 stop
 *---------------------------------------------------------------------------*/
void
T308_stop(call_desc_t *cd)
{
	CRIT_VAR;
	CRIT_BEG;
	
	if(cd->T308 != TIMER_IDLE)
	{
		STOP_TIMER(cd->T308_callout, T308_timeout, cd);
		cd->T308 = TIMER_IDLE;
	}
	CRIT_END;
	
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
}

/*---------------------------------------------------------------------------*
 *	timer T309 timeout function
 *---------------------------------------------------------------------------*/
static void
T309_timeout(call_desc_t *cd)
{
	NDBGL3(L3_T_ERR, "datalink not reconnected, cr = %d", cd->cr);
	next_l3state(cd, EV_T309EXP);
}

/*---------------------------------------------------------------------------*
 *	timer T309 start
 *---------------------------------------------------------------------------*/
void
T309_start(call_desc_t *cd)
{
	if (cd->T309 == TIMER_ACTIVE)
		return;

	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
	cd->T309 = TIMER_ACTIVE;

	START_TIMER(cd->T309_callout, T309_timeout, cd, T309VAL);
}

/*---------------------------------------------------------------------------*
 *	timer T309 stop
 *---------------------------------------------------------------------------*/
void
T309_stop(call_desc_t *cd)
{
	CRIT_VAR;
	CRIT_BEG;
	
	if(cd->T309 != TIMER_IDLE)
	{
		STOP_TIMER(cd->T309_callout, T309_timeout, cd);
		cd->T309 = TIMER_IDLE;
	}
	CRIT_END;
	
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
}

/*---------------------------------------------------------------------------*
 *	timer T310 timeout function
 *---------------------------------------------------------------------------*/
static void
T310_timeout(call_desc_t *cd)
{
	NDBGL3(L3_T_ERR, "CALL PROC timeout, cr = %d", cd->cr);
	next_l3state(cd, EV_T310EXP);
}

/*---------------------------------------------------------------------------*
 *	timer T310 start
 *---------------------------------------------------------------------------*/
void
T310_start(call_desc_t *cd)
{
	if (cd->T310 == TIMER_ACTIVE)
		return;
		
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
	cd->T310 = TIMER_ACTIVE;

	START_TIMER(cd->T310_callout, T310_timeout, cd, T310VAL);
}

/*---------------------------------------------------------------------------*
 *	timer T310 stop
 *---------------------------------------------------------------------------*/
void
T310_stop(call_desc_t *cd)
{
	CRIT_VAR;
	CRIT_BEG;
	
	if(cd->T310 != TIMER_IDLE)
	{
		STOP_TIMER(cd->T310_callout, T310_timeout, cd);
		cd->T310 = TIMER_IDLE;
	}
	CRIT_END;

	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
}

/*---------------------------------------------------------------------------*
 *	timer T313 timeout function
 *---------------------------------------------------------------------------*/
static void
T313_timeout(call_desc_t *cd)
{
	NDBGL3(L3_T_ERR, "CONN ACK not received, cr = %d", cd->cr);
	next_l3state(cd, EV_T313EXP);
}

/*---------------------------------------------------------------------------*
 *	timer T313 start
 *---------------------------------------------------------------------------*/
void
T313_start(call_desc_t *cd)
{
	if (cd->T313 == TIMER_ACTIVE)
		return;
		
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
	cd->T313 = TIMER_ACTIVE;

	START_TIMER(cd->T313_callout, T313_timeout, cd, T313VAL);
}

/*---------------------------------------------------------------------------*
 *	timer T313 stop
 *---------------------------------------------------------------------------*/
void
T313_stop(call_desc_t *cd)
{
	CRIT_VAR;
	CRIT_BEG;
	
	if(cd->T313 != TIMER_IDLE)
	{
		cd->T313 = TIMER_IDLE;
		STOP_TIMER(cd->T313_callout, T313_timeout, cd);
	}
	CRIT_END;
	
	NDBGL3(L3_T_MSG, "cr = %d", cd->cr);
}
