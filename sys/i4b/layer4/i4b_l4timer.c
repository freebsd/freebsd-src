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
 *	i4b_l4timer.c - timer and timeout handling for layer 4
 *	--------------------------------------------------------
 *
 *	$Id: i4b_l4timer.c,v 1.15 1999/12/13 21:25:28 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer4/i4b_l4timer.c,v 1.6 1999/12/14 20:48:35 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:06:39 1999]
 *
 *---------------------------------------------------------------------------*/

#include "i4b.h"

#if NI4B > 0

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
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer4/i4b_l4.h>

/*---------------------------------------------------------------------------*
 *	timer T400 timeout function
 *---------------------------------------------------------------------------*/
static void
T400_timeout(call_desc_t *cd)
{
	DBGL4(L4_ERR, "T400_timeout", ("cr = %d\n", cd->cr));
}

/*---------------------------------------------------------------------------*
 *	timer T400 start
 *---------------------------------------------------------------------------*/
void
T400_start(call_desc_t *cd)
{
	if (cd->T400 == TIMER_ACTIVE)
		return;
		
	DBGL4(L4_MSG, "T400_start", ("cr = %d\n", cd->cr));
	cd->T400 = TIMER_ACTIVE;

#if defined(__FreeBSD__)
	cd->T400_callout = timeout((TIMEOUT_FUNC_T)T400_timeout, (void *)cd, T400DEF);
#else
	timeout((TIMEOUT_FUNC_T)T400_timeout, (void *)cd, T400DEF);
#endif
}

/*---------------------------------------------------------------------------*
 *	timer T400 stop
 *---------------------------------------------------------------------------*/
void
T400_stop(call_desc_t *cd)
{
	CRIT_VAR;
	CRIT_BEG;

	if(cd->T400 == TIMER_ACTIVE)
	{
#if defined(__FreeBSD__)
		untimeout((TIMEOUT_FUNC_T)T400_timeout, (void *)cd, cd->T400_callout);
#else
		untimeout((TIMEOUT_FUNC_T)T400_timeout, (void *)cd);
#endif
		cd->T400 = TIMER_IDLE;
	}
	CRIT_END;
	DBGL4(L4_MSG, "T400_stop", ("cr = %d\n", cd->cr));
}

#endif /* NI4B > 0 */
