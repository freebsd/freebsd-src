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
 * $Id: timer.c,v 1.5.2.7 1997/10/24 23:15:43 brian Exp $
 *
 *  TODO:
 */

#include <signal.h>
#ifdef SIGALRM
#include <errno.h>
#endif
#include <sys/time.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "sig.h"
#include "timer.h"

static struct pppTimer *TimerList = NULL;

static void StopTimerNoBlock(struct pppTimer *);
static void InitTimerService(void);

void
StopTimer(struct pppTimer * tp)
{
#ifdef SIGALRM
  int omask;

  omask = sigblock(sigmask(SIGALRM));
#endif
  StopTimerNoBlock(tp);
#ifdef SIGALRM
  sigsetmask(omask);
#endif
}

void
StartTimer(struct pppTimer * tp)
{
  struct pppTimer *t, *pt;
  u_long ticks = 0;

#ifdef SIGALRM
  int omask;

  omask = sigblock(sigmask(SIGALRM));
#endif

  if (tp->state != TIMER_STOPPED) {
    StopTimerNoBlock(tp);
  }
  if (tp->load == 0) {
    LogPrintf(LogDEBUG, "timer %x has 0 load!\n", tp);
    sigsetmask(omask);
    return;
  }
  pt = NULL;
  for (t = TimerList; t; t = t->next) {
    LogPrintf(LogDEBUG, "StartTimer: %x(%d):  ticks: %d, rest: %d\n",
	      t, t->state, ticks, t->rest);
    if (ticks + t->rest >= tp->load)
      break;
    ticks += t->rest;
    pt = t;
  }

  tp->state = TIMER_RUNNING;
  tp->rest = tp->load - ticks;
  LogPrintf(LogDEBUG, "StartTimer: Inserting %x before %x, rest = %d\n",
	    tp, t, tp->rest);
  /* Insert given *tp just before *t */
  tp->next = t;
  if (pt) {
    pt->next = tp;
  } else {
    InitTimerService();
    TimerList = tp;
  }
  if (t)
    t->rest -= tp->rest;

#ifdef SIGALRM
  sigsetmask(omask);
#endif
}

static void
StopTimerNoBlock(struct pppTimer * tp)
{
  struct pppTimer *t, *pt;

  /*
   * A Running Timer should be removing TimerList, But STOPPED/EXPIRED is
   * already removing TimerList. So just marked as TIMER_STOPPED. Do not
   * change tp->enext!! (Might be Called by expired proc)
   */
  LogPrintf(LogDEBUG, "StopTimer: %x, next = %x state=%x\n",
	    tp, tp->next, tp->state);
  if (tp->state != TIMER_RUNNING) {
    tp->next = NULL;
    tp->state = TIMER_STOPPED;
    return;
  }
  pt = NULL;
  for (t = TimerList; t != tp && t != NULL; t = t->next)
    pt = t;
  if (t) {
    if (pt) {
      pt->next = t->next;
    } else {
      TimerList = t->next;
      if (TimerList == NULL)	/* Last one ? */
	TermTimerService();	/* Terminate Timer Service */
    }
    if (t->next)
      t->next->rest += tp->rest;
  } else
    LogPrintf(LogERROR, "Oops, timer not found!!\n");

  tp->next = NULL;
  tp->state = TIMER_STOPPED;
}

static void
TimerService()
{
  struct pppTimer *tp, *exp, *wt;

  if (LogIsKept(LogDEBUG))
    ShowTimers();
  tp = TimerList;
  if (tp) {
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
	LogPrintf(LogDEBUG, "TimerService: Add %x to exp\n", tp);
	tp = wt;
      } while (tp && (tp->rest == 0));

      TimerList = tp;
      if (TimerList == NULL)	/* No timers ? */
	TermTimerService();	/* Terminate Timer Service */
      LogPrintf(LogDEBUG, "TimerService: next is %x(%d)\n",
		TimerList, TimerList ? TimerList->rest : 0);

      /*
       * Process all expired timers.
       */
      while (exp) {
#ifdef notdef
	StopTimer(exp);
#endif
	if (exp->func)
	  (*exp->func) (exp->arg);

	/*
	 * Just Removing each item from expired list And exp->enext will be
	 * intialized at next expire in this funtion.
	 */
	exp = exp->enext;
      }
    }
  }
}

void
ShowTimers()
{
  struct pppTimer *pt;

  LogPrintf(LogDEBUG, "---- Begin of Timer Service List---\n");
  for (pt = TimerList; pt; pt = pt->next)
    LogPrintf(LogDEBUG, "%x: load = %d, rest = %d, state =%x\n",
	      pt, pt->load, pt->rest, pt->state);
  LogPrintf(LogDEBUG, "---- End of Timer Service List ---\n");
}

#ifdef SIGALRM

static void
nointr_dosleep(u_int sec, u_int usec)
{
  struct timeval to, st, et;

  gettimeofday(&st, NULL);
  et.tv_sec = st.tv_sec + sec;
  et.tv_usec = st.tv_usec + usec;
  to.tv_sec = sec;
  to.tv_usec = usec;
  for (;;) {
    if (select(0, NULL, NULL, NULL, &to) == 0 ||
	errno != EINTR) {
      break;
    } else {
      gettimeofday(&to, NULL);
      if (to.tv_sec > et.tv_sec + 1 ||
          (to.tv_sec == et.tv_sec + 1 && to.tv_usec > et.tv_usec) ||
          to.tv_sec < st.tv_sec ||
          (to.tv_sec == st.tv_sec && to.tv_usec < st.tv_usec)) {
        LogPrintf(LogWARN, "Clock adjusted between %d and %d seconds "
                  "during sleep !\n",
                  to.tv_sec - st.tv_sec, sec + to.tv_sec - st.tv_sec);
        st.tv_sec = to.tv_sec;
        st.tv_usec = to.tv_usec;
        et.tv_sec = st.tv_sec + sec;
        et.tv_usec = st.tv_usec + usec;
        to.tv_sec = sec;
        to.tv_usec = usec;
      } else if (to.tv_sec > et.tv_sec ||
                 (to.tv_sec == et.tv_sec && to.tv_usec >= et.tv_usec)) {
        break;
      } else {
        to.tv_sec = et.tv_sec - to.tv_sec;
        if (et.tv_usec < to.tv_usec) {
          to.tv_sec--;
          to.tv_usec = 1000000 + et.tv_usec - to.tv_usec;
        } else
          to.tv_usec = et.tv_usec - to.tv_usec;
      }
    }
  }
}

void
nointr_sleep(u_int sec)
{
  nointr_dosleep(sec, 0);
}

void
nointr_usleep(u_int usec)
{
  nointr_dosleep(0, usec);
}

static void 
InitTimerService()
{
  struct itimerval itimer;

  pending_signal(SIGALRM, (void (*) (int)) TimerService);
  itimer.it_interval.tv_sec = itimer.it_value.tv_sec = 0;
  itimer.it_interval.tv_usec = itimer.it_value.tv_usec = TICKUNIT;
  if (setitimer(ITIMER_REAL, &itimer, NULL) == -1)
    LogPrintf(LogERROR, "Unable to set itimer.\n");
}

void 
TermTimerService(void)
{
  struct itimerval itimer;

  itimer.it_interval.tv_usec = itimer.it_interval.tv_sec = 0;
  itimer.it_value.tv_usec = itimer.it_value.tv_sec = 0;
  if (setitimer(ITIMER_REAL, &itimer, NULL) == -1)
    LogPrintf(LogERROR, "Unable to set itimer.\n");
  pending_signal(SIGALRM, SIG_IGN);
}

#endif
