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
 * $Id: timer.c,v 1.30 1998/06/20 01:36:38 brian Exp $
 *
 *  TODO:
 */

#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <termios.h>

#include "log.h"
#include "sig.h"
#include "timer.h"
#include "descriptor.h"
#include "prompt.h"

static struct pppTimer *TimerList = NULL;

static void StopTimerNoBlock(struct pppTimer *);

static const char *
tState2Nam(u_int state)
{
  static const char *StateNames[] = { "stopped", "running", "expired" };

  if (state >= sizeof StateNames / sizeof StateNames[0])
    return "unknown";
  return StateNames[state];
}

void
timer_Stop(struct pppTimer * tp)
{
  int omask;

  omask = sigblock(sigmask(SIGALRM));
  StopTimerNoBlock(tp);
  sigsetmask(omask);
}

void
timer_Start(struct pppTimer * tp)
{
  struct pppTimer *t, *pt;
  u_long ticks = 0;
  int omask;

  omask = sigblock(sigmask(SIGALRM));

  if (tp->state != TIMER_STOPPED)
    StopTimerNoBlock(tp);

  if (tp->load == 0) {
    log_Printf(LogTIMER, "%s timer[%p] has 0 load!\n", tp->name, tp);
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
    log_Printf(LogTIMER, "timer_Start: Inserting %s timer[%p] before %s "
              "timer[%p], delta = %ld\n", tp->name, tp, t->name, t, tp->rest);
  else
    log_Printf(LogTIMER, "timer_Start: Inserting %s timer[%p]\n", tp->name, tp);

  /* Insert given *tp just before *t */
  tp->next = t;
  if (pt) {
    pt->next = tp;
  } else {
    timer_InitService();
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
   * A RUNNING timer must be removed from TimerList (->next list).
   * A STOPPED timer isn't in any list, but may have a bogus [e]next field.
   * An EXPIRED timer is in the ->enext list.
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
	timer_TermService();	/* Terminate Timer Service */
    }
    if (t->next)
      t->next->rest += tp->rest;
  } else
    log_Printf(LogERROR, "Oops, %s timer not found!!\n", tp->name);

  tp->next = NULL;
  tp->state = TIMER_STOPPED;
}

static void
TimerService(void)
{
  struct pppTimer *tp, *exp, *wt;

  if (log_IsKept(LogTIMER)) {
    static time_t t;		/* Only show timers globally every second */
    time_t n = time(NULL);

    if (n > t)
      timer_Show(LogTIMER, NULL);
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
	timer_TermService();	/* Terminate Timer Service */

      /*
       * Process all expired timers.
       */
      while (exp) {
#ifdef notdef
	timer_Stop(exp);
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
timer_Show(int LogLevel, struct prompt *prompt)
{
  struct pppTimer *pt;
  int rest = 0;

#define SECS(val)	((val) / SECTICKS)
#define HSECS(val)	(((val) % SECTICKS) * 100 / SECTICKS)
#define DISP								\
  "%s timer[%p]: freq = %ld.%02lds, next = %d.%02ds, state = %s\n",	\
  pt->name, pt, SECS(pt->load), HSECS(pt->load), SECS(rest),		\
  HSECS(rest), tState2Nam(pt->state)

  if (!prompt)
    log_Printf(LogLevel, "---- Begin of Timer Service List---\n");

  for (pt = TimerList; pt; pt = pt->next) {
    rest += pt->rest;
    if (prompt)
      prompt_Printf(prompt, DISP);
    else
      log_Printf(LogLevel, DISP);
  }

  if (!prompt)
    log_Printf(LogLevel, "---- End of Timer Service List ---\n");
}

void 
timer_InitService()
{
  struct itimerval itimer;

  sig_signal(SIGALRM, (void (*) (int)) TimerService);
  itimer.it_interval.tv_sec = itimer.it_value.tv_sec = 0;
  itimer.it_interval.tv_usec = itimer.it_value.tv_usec = TICKUNIT;
  if (setitimer(ITIMER_REAL, &itimer, NULL) == -1)
    log_Printf(LogERROR, "Unable to set itimer.\n");
}

void 
timer_TermService(void)
{
  struct itimerval itimer;

  itimer.it_interval.tv_usec = itimer.it_interval.tv_sec = 0;
  itimer.it_value.tv_usec = itimer.it_value.tv_sec = 0;
  if (setitimer(ITIMER_REAL, &itimer, NULL) == -1)
    log_Printf(LogERROR, "Unable to set itimer.\n");
  sig_signal(SIGALRM, SIG_IGN);
}
