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
 * $Id: timer.c,v 1.27.2.5 1998/04/17 22:04:36 brian Exp $
 *
 *  TODO:
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "log.h"
#include "sig.h"
#include "timer.h"
#include "descriptor.h"
#include "prompt.h"

static struct pppTimer *TimerList = NULL;

static void StopTimerNoBlock(struct pppTimer *);
static void InitTimerService(void);

static const char *
tState2Nam(u_int state)
{
  static const char *StateNames[] = { "stopped", "running", "expired" };

  if (state >= sizeof StateNames / sizeof StateNames[0])
    return "unknown";
  return StateNames[state];
}

void
StopTimer(struct pppTimer * tp)
{
  int omask;

  omask = sigblock(sigmask(SIGALRM));
  StopTimerNoBlock(tp);
  sigsetmask(omask);
}

void
StartTimer(struct pppTimer * tp)
{
  struct pppTimer *t, *pt;
  u_long ticks = 0;
  int omask;

  omask = sigblock(sigmask(SIGALRM));

  if (tp->state != TIMER_STOPPED) {
    StopTimerNoBlock(tp);
  }
  if (tp->load == 0) {
    LogPrintf(LogDEBUG, "%s timer[%p] has 0 load!\n", tp->name, tp);
    sigsetmask(omask);
    return;
  }
  pt = NULL;
  for (t = TimerList; t; t = t->next) {
    if (ticks + t->rest >= tp->load)
      break;
    ticks += t->rest;
    pt = t;
  }

  tp->state = TIMER_RUNNING;
  tp->rest = tp->load - ticks;

  if (t)
    LogPrintf(LogDEBUG, "StartTimer: Inserting %s timer[%p] before %s "
              "timer[%p], delta = %d\n", tp->name, tp, t->name, t, tp->rest);
  else
    LogPrintf(LogDEBUG, "StartTimer: Inserting %s timer[%p]\n", tp->name, tp);

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

  sigsetmask(omask);
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
    LogPrintf(LogERROR, "Oops, %s timer not found!!\n", tp->name);

  tp->next = NULL;
  tp->state = TIMER_STOPPED;
}

static void
TimerService(void)
{
  struct pppTimer *tp, *exp, *wt;

  if (LogIsKept(LogDEBUG)) {
    static time_t t;
    time_t n = time(NULL);  /* Only show timers every second */

    if (n > t)
      ShowTimers(LogDEBUG, NULL);
    t = n;
  }
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
	tp = wt;
      } while (tp && (tp->rest == 0));

      TimerList = tp;
      if (TimerList == NULL)	/* No timers ? */
	TermTimerService();	/* Terminate Timer Service */

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
ShowTimers(int LogLevel, struct prompt *prompt)
{
  struct pppTimer *pt;
  int rest = 0;

#define SECS(val)	((val) / SECTICKS)
#define HSECS(val)	(((val) % SECTICKS) * 100 / SECTICKS)
#define DISP								\
  "%s timer[%p]: freq = %d.%02ds, next = %d.%02ds, state = %s\n",	\
  pt->name, pt, SECS(pt->load), HSECS(pt->load), SECS(rest),		\
  HSECS(rest), tState2Nam(pt->state)

  if (!prompt)
    LogPrintf(LogLevel, "---- Begin of Timer Service List---\n");

  for (pt = TimerList; pt; pt = pt->next) {
    rest += pt->rest;
    if (prompt)
      prompt_Printf(prompt, DISP);
    else
      LogPrintf(LogLevel, DISP);
  }

  if (!prompt)
    LogPrintf(LogLevel, "---- End of Timer Service List ---\n");
}

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
