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
 *	i4b_l3timer.c - timer and timeout handling for layer 3
 *	------------------------------------------------------
 *
 *	$Id: i4b_l3timer.c,v 1.14 1999/12/13 21:25:27 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer3/i4b_l3timer.c,v 1.6 1999/12/14 20:48:31 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:05:18 1999]
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
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_isdnq931.h>
#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer3/i4b_l3.h>
#include <i4b/layer3/i4b_l3fsm.h>
#include <i4b/layer3/i4b_q931.h>

#include <i4b/layer4/i4b_l4.h>

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
	DBGL3(L3_T_ERR, "T303_timeout", ("SETUP not answered, cr = %d\n", cd->cr));
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
		
	DBGL3(L3_T_MSG, "T303_start", ("cr = %d\n", cd->cr));
	cd->T303 = TIMER_ACTIVE;

#if defined(__FreeBSD__)
	cd->T303_callout = timeout((TIMEOUT_FUNC_T)T303_timeout, (void *)cd, T303VAL);
#else
	timeout((TIMEOUT_FUNC_T)T303_timeout, (void *)cd, T303VAL);
#endif
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
#if defined(__FreeBSD__)
		untimeout((TIMEOUT_FUNC_T)T303_timeout, (void *)cd, cd->T303_callout);
#else
		untimeout((TIMEOUT_FUNC_T)T303_timeout, (void *)cd);
#endif
		cd->T303 = TIMER_IDLE;
	}
	CRIT_END;
	DBGL3(L3_T_MSG, "T303_stop", ("cr = %d\n", cd->cr));
}

/*---------------------------------------------------------------------------*
 *	timer T305 timeout function
 *---------------------------------------------------------------------------*/
static void
T305_timeout(call_desc_t *cd)
{
	DBGL3(L3_T_ERR, "T305_timeout", ("DISC not answered, cr = %d\n", cd->cr));
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
		
	DBGL3(L3_T_MSG, "T305_start", ("cr = %d\n", cd->cr));
	cd->T305 = TIMER_ACTIVE;

#if defined(__FreeBSD__)
	cd->T305_callout = timeout((TIMEOUT_FUNC_T)T305_timeout, (void *)cd, T305VAL);
#else
	timeout((TIMEOUT_FUNC_T)T305_timeout, (void *)cd, T305VAL);
#endif
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
#if defined(__FreeBSD__)
		untimeout((TIMEOUT_FUNC_T)T305_timeout, (void *)cd, cd->T305_callout);
#else
		untimeout((TIMEOUT_FUNC_T)T305_timeout, (void *)cd);
#endif
		cd->T305 = TIMER_IDLE;
	}
	CRIT_END;
	
	DBGL3(L3_T_MSG, "T305_stop", ("cr = %d\n", cd->cr));
}

/*---------------------------------------------------------------------------*
 *	timer T308 timeout function
 *---------------------------------------------------------------------------*/
static void
T308_timeout(call_desc_t *cd)
{
	DBGL3(L3_T_ERR, "T308_timeout", ("REL not answered, cr = %d\n", cd->cr));
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
		
	DBGL3(L3_T_MSG, "T308_start", ("cr = %d\n", cd->cr));
	cd->T308 = TIMER_ACTIVE;

#if defined(__FreeBSD__)
	cd->T308_callout = timeout((TIMEOUT_FUNC_T)T308_timeout, (void *)cd, T308VAL);
#else
	timeout((TIMEOUT_FUNC_T)T308_timeout, (void *)cd, T308VAL);
#endif
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
#if defined(__FreeBSD__)
		untimeout((TIMEOUT_FUNC_T)T308_timeout, (void *)cd, cd->T308_callout);
#else
		untimeout((TIMEOUT_FUNC_T)T308_timeout, (void *)cd);
#endif
		cd->T308 = TIMER_IDLE;
	}
	CRIT_END;
	
	DBGL3(L3_T_MSG, "T308_stop", ("cr = %d\n", cd->cr));
}

/*---------------------------------------------------------------------------*
 *	timer T309 timeout function
 *---------------------------------------------------------------------------*/
static void
T309_timeout(call_desc_t *cd)
{
	DBGL3(L3_T_ERR, "T309_timeout", ("datalink not reconnected, cr = %d\n", cd->cr));
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

	DBGL3(L3_T_MSG, "T309_start", ("cr = %d\n", cd->cr));
	cd->T309 = TIMER_ACTIVE;

#if defined(__FreeBSD__)
	cd->T309_callout = timeout((TIMEOUT_FUNC_T)T309_timeout, (void *)cd, T309VAL);
#else
	timeout((TIMEOUT_FUNC_T)T309_timeout, (void *)cd, T309VAL);
#endif
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
#if defined(__FreeBSD__)
		untimeout((TIMEOUT_FUNC_T)T309_timeout, (void *)cd, cd->T309_callout);
#else
		untimeout((TIMEOUT_FUNC_T)T309_timeout, (void *)cd);
#endif
		cd->T309 = TIMER_IDLE;
	}
	CRIT_END;
	
	DBGL3(L3_T_MSG, "T309_stop", ("cr = %d\n", cd->cr));
}

/*---------------------------------------------------------------------------*
 *	timer T310 timeout function
 *---------------------------------------------------------------------------*/
static void
T310_timeout(call_desc_t *cd)
{
	DBGL3(L3_T_ERR, "T310_timeout", ("CALL PROC timeout, cr = %d\n", cd->cr));
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
		
	DBGL3(L3_T_MSG, "T310_start", ("cr = %d\n", cd->cr));
	cd->T310 = TIMER_ACTIVE;

#if defined(__FreeBSD__)
	cd->T310_callout = timeout((TIMEOUT_FUNC_T)T310_timeout, (void *)cd, T310VAL);
#else
	timeout((TIMEOUT_FUNC_T)T310_timeout, (void *)cd, T310VAL);
#endif
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
#if defined(__FreeBSD__)
		untimeout((TIMEOUT_FUNC_T)T310_timeout, (void *)cd, cd->T310_callout);
#else
		untimeout((TIMEOUT_FUNC_T)T310_timeout, (void *)cd);
#endif
		cd->T310 = TIMER_IDLE;
	}
	CRIT_END;

	DBGL3(L3_T_MSG, "T310_stop", ("cr = %d\n", cd->cr));
}

/*---------------------------------------------------------------------------*
 *	timer T313 timeout function
 *---------------------------------------------------------------------------*/
static void
T313_timeout(call_desc_t *cd)
{
	DBGL3(L3_T_ERR, "T313_timeout", ("CONN ACK not received, cr = %d\n", cd->cr));
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
		
	DBGL3(L3_T_MSG, "T313_start", ("cr = %d\n", cd->cr));
	cd->T313 = TIMER_ACTIVE;

#if defined(__FreeBSD__)
	cd->T313_callout = timeout((TIMEOUT_FUNC_T)T313_timeout, (void *)cd, T313VAL);
#else
	timeout((TIMEOUT_FUNC_T)T313_timeout, (void *)cd, T313VAL);
#endif
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
#if defined(__FreeBSD__)
		untimeout((TIMEOUT_FUNC_T)T313_timeout, (void *)cd, cd->T313_callout);
#else
		untimeout((TIMEOUT_FUNC_T)T313_timeout, (void *)cd);
#endif
	}
	CRIT_END;
	
	DBGL3(L3_T_MSG, "T313_stop", ("cr = %d\n", cd->cr));
}

#endif /* NI4BQ931 > 0 */

