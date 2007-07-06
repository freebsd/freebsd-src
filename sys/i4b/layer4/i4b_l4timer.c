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
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_l4timer.c - timer and timeout handling for layer 4
 *	--------------------------------------------------------
 *      last edit-date: [Sat Mar  9 19:49:13 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer4/i4b_l4.h>

/*---------------------------------------------------------------------------*
 *	timer T400 timeout function
 *---------------------------------------------------------------------------*/
static void
T400_timeout(call_desc_t *cd)
{
	NDBGL4(L4_ERR, "cr = %d", cd->cr);
}

/*---------------------------------------------------------------------------*
 *	timer T400 start
 *---------------------------------------------------------------------------*/
void
T400_start(call_desc_t *cd)
{
	if (cd->T400 == TIMER_ACTIVE)
		return;
		
	NDBGL4(L4_MSG, "cr = %d", cd->cr);
	cd->T400 = TIMER_ACTIVE;

	START_TIMER(cd->T400_callout, T400_timeout, cd, T400DEF);
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
		STOP_TIMER(cd->T400_callout, T400_timeout, cd);
		cd->T400 = TIMER_IDLE;
	}
	CRIT_END;
	NDBGL4(L4_MSG, "cr = %d", cd->cr);
}
