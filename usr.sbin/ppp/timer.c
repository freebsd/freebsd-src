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
 * $FreeBSD$
 *
 *  TODO:
 */
#include "defs.h"
#include <sys/time.h>
#include <signal.h>
#include "timeout.h"
#ifdef SIGALRM
#include <errno.h>
#endif
void StopTimerNoBlock( struct pppTimer *);
void ShowTimers(void);

void
StopTimer( struct pppTimer *tp )
{
#ifdef SIGALRM
  int	omask;
  omask = sigblock(sigmask(SIGALRM));
#endif
  StopTimerNoBlock(tp);
#ifdef SIGALRM
  sigsetmask(omask);
#endif
}
void
StartTimer(tp)
struct pppTimer *tp;
{
  struct pppTimer *t, *pt;
  u_long ticks = 0;

#ifdef SIGALRM
  int	omask;
  omask = sigblock(sigmask(SIGALRM));
#endif

  if (tp->state != TIMER_STOPPED) {
    StopTimerNoBlock(tp);
  }
  if (tp->load == 0) {
#ifdef DEBUG
    logprintf("timer %x has 0 load!\n", tp);
#endif
    sigsetmask(omask);
    return;
  }
  pt = NULL;
  for (t = TimerList; t; t = t->next) {
#ifdef DEBUG
    logprintf("StartTimer: %x(%d):  ticks: %d, rest: %d\n", t, t->state, ticks, t->rest);
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

void
StopTimerNoBlock(tp)
struct pppTimer *tp;
{
  struct pppTimer *t, *pt;

  /*
   * A Running Timer should be removing TimerList,
   * But STOPPED/EXPIRED is already removing TimerList.
   * So just marked as TIMER_STOPPED.
   * Do not change tp->enext!! (Might be Called by expired proc)
   */
#ifdef DEBUG
  logprintf("StopTimer: %x, next = %x state=%x\n", tp, tp->next, tp->state);
#endif
  if (tp->state != TIMER_RUNNING) {
    tp->next   = NULL;
    tp->state  = TIMER_STOPPED;
    return;
  }

  pt = NULL;
  for (t = TimerList; t != tp && t !=NULL ; t = t->next)
    pt = t;
  if (t) {
    if (pt) {
      pt->next = t->next;
    } else {
      TimerList = t->next;
      if ( TimerList == NULL )			/* Last one ? */
	 TermTimerService();			/* Terminate Timer Service */
    }
    if (t->next)
      t->next->rest += tp->rest;
  } else {
    logprintf("Oops, timer not found!!\n");
  }
  tp->next = NULL;
  tp->state = TIMER_STOPPED;
}

void
TimerService()
{
  struct pppTimer *tp, *exp, *wt;

#ifdef DEBUG
  ShowTimers();
#endif
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
#ifdef DEBUG
	logprintf("Add %x to exp\n", tp);
#endif
	tp = wt;
      } while (tp && (tp->rest == 0));

      TimerList = tp;
      if ( TimerList == NULL )			/* No timers ? */
	 TermTimerService();			/* Terminate Timer Service */
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
	/*
         * Just Removing each item from expired list
         * And exp->enext will be intialized at next expire
         * in this funtion.
         */
	exp =  exp->enext;
      }
    }
  }
}

void
ShowTimers()
{
  struct pppTimer *pt;

  logprintf("---- Begin of Timer Service List---\n");
  for (pt = TimerList; pt; pt = pt->next)
    logprintf("%x: load = %d, rest = %d, state =%x\n",
	pt, pt->load, pt->rest, pt->state);
  logprintf("---- End of Timer Service List ---\n");
}

#ifdef SIGALRM
u_int
sleep( u_int sec )
{
  struct timeval  to,st,et;
  long sld, nwd, std;

  gettimeofday( &st, NULL );
  to.tv_sec =  sec;
  to.tv_usec = 0;
  std = st.tv_sec * 1000000 + st.tv_usec;
  for (;;) {
    if ( select ( 0, NULL, NULL, NULL, &to) == 0 ||
         errno != EINTR ) {
       break;
    } else  {
       gettimeofday( &et, NULL );
       sld = to.tv_sec * 1000000 + to.tv_sec;
       nwd = et.tv_sec * 1000000 + et.tv_usec - std;
       if ( sld > nwd )
          sld -= nwd;
       else
          sld  = 1; /* Avoid both tv_sec/usec is 0 */

       /* Calculate timeout value for select */
       to.tv_sec  = sld / 1000000;
       to.tv_usec = sld % 1000000;
    }
  }
  return (0L);
}

void usleep( u_int usec)
{
  struct timeval  to,st,et;
  long sld, nwd, std;

  gettimeofday( &st, NULL );
  to.tv_sec =  0;
  to.tv_usec = usec;
  std = st.tv_sec * 1000000 + st.tv_usec;
  for (;;) {
    if ( select ( 0, NULL, NULL, NULL, &to) == 0 ||
         errno != EINTR ) {
       break;
    } else  {
       gettimeofday( &et, NULL );
       sld = to.tv_sec * 1000000 + to.tv_sec;
       nwd = et.tv_sec * 1000000 + et.tv_usec - std;
       if ( sld > nwd )
          sld -= nwd;
       else
          sld  = 1; /* Avoid both tv_sec/usec is 0 */

       /* Calculate timeout value for select */
       to.tv_sec  = sld / 1000000;
       to.tv_usec = sld % 1000000;

    }
  }
}

void InitTimerService( void ) {
  struct itimerval itimer;

  signal(SIGALRM, (void (*)(int))TimerService);
  itimer.it_interval.tv_sec = itimer.it_value.tv_sec = 0;
  itimer.it_interval.tv_usec = itimer.it_value.tv_usec = TICKUNIT;
  setitimer(ITIMER_REAL, &itimer, NULL);
}

void TermTimerService( void ) {
  struct itimerval itimer;

  itimer.it_interval.tv_sec = itimer.it_value.tv_sec = 0;
  itimer.it_value.tv_usec = itimer.it_value.tv_sec = 0;
  setitimer(ITIMER_REAL, &itimer, NULL);
  /*
   * Notes: after disabling timer here, we will get one
   *        SIGALRM will be got.
   */
  signal(SIGALRM, SIG_IGN);
}
#endif
