/*
 *		PPP Timer Processing Module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id:$
 *
 *  TODO:
 */
#include "defs.h"
#include <sys/time.h>
#include <signal.h>
#include "timeout.h"

void
StartTimer(tp)
struct pppTimer *tp;
{
  struct pppTimer *t, *pt;
  u_long ticks = 0;

  if (tp->state == TIMER_RUNNING) {
    StopTimer(tp);
  }
  if (tp->load == 0) {
#ifdef DEBUG
    logprintf("timer %x has 0 load!\n", tp);
#endif
    return;
  }
  pt = NULL;
  for (t = TimerList; t; t = t->next) {
#ifdef DEBUG
    logprintf("%x(%d):  ticks: %d, rest: %d\n", t, t->state, ticks, t->rest);
#endif
    if (ticks + t->rest >= tp->load)
      break;
    ticks += t->rest;
    pt = t;
  }

  tp->state = TIMER_RUNNING;
  tp->rest = tp->load - ticks;
#ifdef DEBUG
  logprintf("Inserting %x before %x, rest = %d\n", tp, t, tp->rest);
#endif
  /* Insert given *tp just before *t */
  tp->next = t;
  if (pt) {
    pt->next = tp;
  } else
    TimerList = tp;
  if (t)
    t->rest -= tp->rest;
}

void
StopTimer(tp)
struct pppTimer *tp;
{
  struct pppTimer *t, *pt;

  if (tp->state != TIMER_RUNNING) {
    tp->next = NULL;
    return;
  }

#ifdef DEBUG
  logprintf("StopTimer: %x, next = %x\n", tp, tp->next);
#endif
  pt = NULL;
  for (t = TimerList; t != tp; t = t->next)
    pt = t;
  if (t) {
    if (pt)
      pt->next = t->next;
    else
      TimerList = t->next;
    if (t->next)
      t->next->rest += tp->rest;
  } else 
    fprintf(stderr, "Oops, timer not found!!\n");
  tp->next = NULL;
  tp->state = TIMER_STOPPED;
}

void
TimerService()
{
  struct pppTimer *tp, *exp, *wt;

  if (tp = TimerList) {
    tp->rest--;
    if (tp->rest == 0) {
      /*
       * Multiple timers may expires at once. Create list of expired timers.
       */
      exp = NULL;
      do {
	tp->state = TIMER_EXPIRED;
	wt = tp->next;
	tp->enext = exp;
	exp = tp;
#ifdef DEBUG
	logprintf("Add %x to exp\n", tp);
#endif
	tp = wt;
      } while (tp && (tp->rest == 0));

      TimerList = tp;
#ifdef DEBUG
      logprintf("TimerService: next is %x(%d)\n",
		TimerList, TimerList? TimerList->rest : 0);
#endif
      /*
       * Process all expired timers.
       */
      while (exp) {
#ifdef notdef
	StopTimer(exp);
#endif
	if (exp->func)
	  (*exp->func)(exp->arg);
	exp = exp->enext;
      }
    }
  }
}

void
ShowTimers()
{
  struct pppTimer *pt;

  for (pt = TimerList; pt; pt = pt->next)
    fprintf(stderr, "%x: load = %d, rest = %d\r\n", pt, pt->load, pt->rest);
}
