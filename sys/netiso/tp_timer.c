/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)tp_timer.c	7.5 (Berkeley) 5/6/91
 *	$Id: tp_timer.c,v 1.4 1993/12/19 00:53:45 wollman Exp $
 */

/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */

/* 
 * ARGO TP
 *
 * Contains all the timer code.  
 * There are two sources of calls to these routines:
 * the clock, and tp.trans. (ok, and tp_pcb.c calls it at init time)
 *
 * Timers come in two flavors - those that generally get
 * cancelled (tp_ctimeout, tp_cuntimeout)
 * and those that either usually expire (tp_etimeout, 
 * tp_euntimeout, tp_slowtimo) or may require more than one instance
 * of the timer active at a time.
 *
 * The C timers are stored in the tp_ref structure. Their "going off"
 * is manifested by a driver event of the TM_xxx form.
 *
 * The E timers are handled like the generic kernel callouts.
 * Their "going off" is manifested by a function call w/ 3 arguments.
 */

#include "param.h"
#include "systm.h"
#include "types.h"
#include "time.h"
#include "malloc.h"
#include "socket.h"

#include "tp_param.h"
#include "tp_timer.h"
#include "tp_stat.h"
#include "tp_pcb.h"
#include "tp_tpdu.h"
#include "argo_debug.h"
#include "tp_trace.h"
#include "tp_seq.h"

struct	Ecallout *TP_callfree;
struct	Ecallout *TP_callout; 
struct	tp_ref *tp_ref;
int		N_TPREF = 100;

int tp_driver();		/* XXX */
extern int tp_maxrefopen;  /* highest ref # of an open tp connection */

/*
 * CALLED FROM:
 *  at autoconfig time from tp_init() 
 * 	a combo of event, state, predicate
 * FUNCTION and ARGUMENTS:
 *  initialize data structures for the timers
 */
void
tp_timerinit()
{
	register struct Ecallout *e;
	register int s;
#define GETME(x, t, n) {s = (n)*sizeof(*x); x = (t) malloc(s, M_PCB, M_NOWAIT);\
if (x == 0) panic("tp_timerinit"); bzero((caddr_t)x, s);}
	/*
	 * Initialize storage
	 */
	GETME(TP_callout, struct Ecallout *, 2 * N_TPREF);
	GETME(tp_ref, struct tp_ref *, 1 +  N_TPREF);

	TP_callfree = TP_callout + ((2 * N_TPREF) - 1);
	for (e = TP_callfree; e > TP_callout; e--)
		e->c_next = e - 1;

	/* hate to do this but we really don't want zero to be a legit ref */
	tp_maxrefopen = 1;
	tp_ref[0].tpr_state = REF_FROZEN;  /* white lie -- no ref timer, don't
		* want this one to be allocated- ever
		* unless, of course, you make refs and address instead of an
		* index - then 0 can be allocated
		*/
#undef GETME
}

/**********************  e timers *************************/

/*
 * CALLED FROM:
 *  tp_slowtimo() every 1/2 second, for each open reference
 * FUNCTION and ARGUMENTS:
 *  (refp) indicates a reference structure that is in use.
 *  This ref structure may contain active E-type timers.
 *  Update the timers and if any expire, create an event and
 *  call the driver.
 */
static void
tp_Eclock(refp)
	struct tp_ref	*refp; /* the reference structure */
{
	register struct Ecallout *p1; /* to drift through the list of callouts */
	struct tp_event			 E; /* event to pass to tp_driver() */
	int						 tp_driver(); /* drives the FSM */

	/*
	 * Update real-time timeout queue.
	 * At front of queue are some number of events which are ``due''.
	 * The time to these is <= 0 and if negative represents the
	 * number of ticks which have passed since it was supposed to happen.
	 * The rest of the q elements (times > 0) are events yet to happen,
	 * where the time for each is given as a delta from the previous.
	 * Decrementing just the first of these serves to decrement the time
	 * to all events.
	 * 
	 * This version, which calls the driver directly, doesn't pass
	 * along the ticks - may want to add the ticks if there's any use
	 * for them.
	 */
	IncStat(ts_Eticks);
	p1 = refp->tpr_calltodo.c_next;
	while (p1) {
		if (--p1->c_time > 0)
			break;
		if (p1->c_time == 0)
			break;
		p1 = p1->c_next;
	}

	for (;;) {
		struct tp_pcb *tpcb;
		if ((p1 = refp->tpr_calltodo.c_next) == 0 || p1->c_time > 0) {
			break;
		}
		refp->tpr_calltodo.c_next = p1->c_next;
		p1->c_next = TP_callfree;

#ifndef lint
		E.ev_number = p1->c_func;
		E.ATTR(TM_data_retrans).e_low = (SeqNum) p1->c_arg1;
		E.ATTR(TM_data_retrans).e_high = (SeqNum) p1->c_arg2;
		E.ATTR(TM_data_retrans).e_retrans =  p1->c_arg3;
#endif lint
		IFDEBUG(D_TIMER)
			printf("E expired! event 0x%x (0x%x,0x%x), pcb 0x%x ref %d\n",
				p1->c_func, p1->c_arg1, p1->c_arg2, refp->tpr_pcb,
				refp-tp_ref);
		ENDDEBUG

		TP_callfree = p1;
		IncStat(ts_Eexpired);
		(void) tp_driver( tpcb = refp->tpr_pcb, &E);
		if (p1->c_func == TM_reference && tpcb->tp_state == TP_CLOSED)
			free((caddr_t)tpcb, M_PCB); /* XXX wart; where else to do it? */
	}
}

/*
 * CALLED FROM:
 *  tp.trans all over
 * FUNCTION and ARGUMENTS:
 * Set an E type timer.  (refp) is the ref structure.
 * Causes  fun(arg1,arg2,arg3) to be called after time t.
 */
void
tp_etimeout(refp, fun, arg1, arg2, arg3, ticks)
	struct tp_ref	*refp;		
	int 			fun; 	/* function to be called */
	u_int			arg1, arg2; 
	int				arg3;
	register int	ticks;
{
	register struct Ecallout *p1, *p2, *pnew;
		/* p1 and p2 drift through the list of timeout callout structures,
		 * pnew points to the newly created callout structure
		 */

	IFDEBUG(D_TIMER)
		printf("etimeout pcb 0x%x state 0x%x\n", refp->tpr_pcb,
		refp->tpr_pcb->tp_state);
	ENDDEBUG
	IFTRACE(D_TIMER)
		tptrace(TPPTmisc, "tp_etimeout ref refstate tks Etick", refp-tp_ref,
		refp->tpr_state, ticks, tp_stat.ts_Eticks);
	ENDTRACE

	IncStat(ts_Eset);
	if (ticks == 0)
		ticks = 1;
	pnew = TP_callfree;
	if (pnew == (struct Ecallout *)0)
		panic("tp timeout table overflow");
	TP_callfree = pnew->c_next;
	pnew->c_arg1 = arg1;
	pnew->c_arg2 = arg2;
	pnew->c_arg3 = arg3;
	pnew->c_func = fun;
	for (p1 = &(refp->tpr_calltodo); 
							(p2 = p1->c_next) && p2->c_time < ticks; p1 = p2)
		if (p2->c_time > 0)
			ticks -= p2->c_time;
	p1->c_next = pnew;
	pnew->c_next = p2;
	pnew->c_time = ticks;
	if (p2)
		p2->c_time -= ticks;
}

/*
 * CALLED FROM:
 *  tp.trans all over
 * FUNCTION and ARGUMENTS:
 *  Cancel all occurrences of E-timer function (fun) for reference (refp)
 */
void
tp_euntimeout(refp, fun)
	struct tp_ref *refp;
	int			  fun;
{
	register struct Ecallout *p1, *p2; /* ptrs to drift through the list */

	IFTRACE(D_TIMER)
		tptrace(TPPTmisc, "tp_euntimeout ref", refp-tp_ref, 0, 0, 0);
	ENDTRACE

	p1 = &refp->tpr_calltodo; 
	while ( (p2 = p1->c_next) != 0) {
		if (p2->c_func == fun)  {
			if (p2->c_next && p2->c_time > 0) 
				p2->c_next->c_time += p2->c_time;
			p1->c_next = p2->c_next;
			p2->c_next = TP_callfree;
			TP_callfree = p2;
			IncStat(ts_Ecan_act);
			continue;
		}
		p1 = p2;
	}
}

/*
 * CALLED FROM:
 *  tp.trans, when an incoming ACK causes things to be dropped
 *  from the retransmission queue, and we want their associated
 *  timers to be cancelled.
 * FUNCTION and ARGUMENTS:
 *  cancel all occurrences of function (fun) where (arg2) < (seq)
 */
void
tp_euntimeout_lss(refp, fun, seq)
	struct tp_ref *refp;
	int			  fun;
	SeqNum		  seq;
{
	register struct Ecallout *p1, *p2;

	IFTRACE(D_TIMER)
		tptrace(TPPTmisc, "tp_euntimeoutLSS ref", refp-tp_ref, seq, 0, 0);
	ENDTRACE

	p1 = &refp->tpr_calltodo; 
	while ( (p2 = p1->c_next) != 0) {
		if ((p2->c_func == fun) && SEQ_LT(refp->tpr_pcb, p2->c_arg2, seq))  {
			if (p2->c_next && p2->c_time > 0) 
				p2->c_next->c_time += p2->c_time;
			p1->c_next = p2->c_next;
			p2->c_next = TP_callfree;
			TP_callfree = p2;
			IncStat(ts_Ecan_act);
			continue;
		}
		p1 = p2;
	}
}

/****************  c timers **********************
 *
 * These are not chained together; they sit
 * in the tp_ref structure. they are the kind that
 * are typically cancelled so it's faster not to
 * mess with the chains
 */

/*
 * CALLED FROM:
 *  the clock, every 500 ms
 * FUNCTION and ARGUMENTS:
 *  Look for open references with active timers.
 *  If they exist, call the appropriate timer routines to update
 *  the timers and possibly generate events.
 *  (The E timers are done in other procedures; the C timers are
 *  updated here, and events for them are generated here.)
 */
ProtoHook
tp_slowtimo()
{
	register int 		r,t;
	struct Ccallout 	*cp;
	struct tp_ref		*rp = tp_ref;
	struct tp_event		E;
	int 				s = splnet();

	/* check only open reference structures */
	IncStat(ts_Cticks);
	rp++;	/* tp_ref[0] is never used */
	for(  r=1 ; (r <= tp_maxrefopen) ; r++,rp++ ) {
		if (rp->tpr_state < REF_OPEN) 
			continue;

		/* check the C-type timers */
		cp = rp->tpr_callout;
		for (t=0 ; t < N_CTIMERS; t++,cp++) {
			if( cp->c_active ) {
				if( --cp->c_time <= 0 ) {
					cp->c_active = FALSE;
					E.ev_number = t;
					IFDEBUG(D_TIMER)
						printf("C expired! type 0x%x\n", t);
					ENDDEBUG
					IncStat(ts_Cexpired);
					tp_driver( rp->tpr_pcb, &E);
				}
			}
		}
		/* now update the list */
		tp_Eclock(rp);
	}
	splx(s);
	return 0;
}

/*
 * CALLED FROM:
 *  tp.trans, tp_emit()
 * FUNCTION and ARGUMENTS:
 * 	Set a C type timer of type (which) to go off after (ticks) time.
 */
void
tp_ctimeout(refp, which, ticks)
	register struct tp_ref	*refp;
	int 					which, ticks; 
{
	register struct Ccallout *cp = &(refp->tpr_callout[which]);

	IFTRACE(D_TIMER)
		tptrace(TPPTmisc, "tp_ctimeout ref which tpcb active", 
			(int)(refp - tp_ref), which, refp->tpr_pcb, cp->c_active);
	ENDTRACE
	if(cp->c_active)
		IncStat(ts_Ccan_act);
	IncStat(ts_Cset);
	cp->c_time = ticks;
	cp->c_active = TRUE;
}

/*
 * CALLED FROM:
 *  tp.trans 
 * FUNCTION and ARGUMENTS:
 * 	Version of tp_ctimeout that resets the C-type time if the 
 * 	parameter (ticks) is > the current value of the timer.
 */
void
tp_ctimeout_MIN(refp, which, ticks)
	register struct tp_ref	*refp;
	int						which, ticks; 
{
	register struct Ccallout *cp = &(refp->tpr_callout[which]);

	IFTRACE(D_TIMER)
		tptrace(TPPTmisc, "tp_ctimeout_MIN ref which tpcb active", 
			(int)(refp - tp_ref), which, refp->tpr_pcb, cp->c_active);
	ENDTRACE
	if(cp->c_active)
		IncStat(ts_Ccan_act);
	IncStat(ts_Cset);
	if( cp->c_active ) 
		cp->c_time = MIN(ticks, cp->c_time);
	else  {
		cp->c_time = ticks;
		cp->c_active = TRUE;
	}
}

/*
 * CALLED FROM:
 *  tp.trans
 * FUNCTION and ARGUMENTS:
 *  Cancel the (which) timer in the ref structure indicated by (refp).
 */
void
tp_cuntimeout(refp, which)
	int						which;
	register struct tp_ref	*refp;
{
	register struct Ccallout *cp;

	cp = &(refp->tpr_callout[which]);

	IFDEBUG(D_TIMER)
		printf("tp_cuntimeout(0x%x, %d) active %d\n", refp, which, cp->c_active);
	ENDDEBUG

	IFTRACE(D_TIMER)
		tptrace(TPPTmisc, "tp_cuntimeout ref which, active", refp-tp_ref, 
			which, cp->c_active, 0);
	ENDTRACE

	if(cp->c_active)
		IncStat(ts_Ccan_act);
	else
		IncStat(ts_Ccan_inact);
	cp->c_active = FALSE;
}
