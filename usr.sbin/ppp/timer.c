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
 * $FreeBSD: src/usr.sbin/ppp/timer.c,v 1.38.2.1 2000/03/21 10:23:20 brian Exp $
 *
 *  TODO:
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>

#include "log.h"
#include "sig.h"
#include "timer.h"
#include "descriptor.h"
#include "prompt.h"


#define RESTVAL(t) \
    ((t).it_value.tv_sec * SECTICKS + (t).it_value.tv_usec / TICKUNIT + \
     ((((t).it_value.tv_usec % TICKUNIT) >= (TICKUNIT >> 1)) ? 1 : 0))

static struct pppTimer *TimerList = NULL, *ExpiredList = NULL;

static void StopTimerNoBlock(struct pppTimer *);

static const char *
tState2Nam(u_int state)
{
  static const char * const StateNames[] = { "stopped", "running", "expired" };

  if (state >= sizeof StateNames / sizeof StateNames[0])
    return "unknown";
  return StateNames[state];
}

void
timer_Stop(struct pppTimer *tp)
{
  int omask;

  omask = sigblock(sigmask(SIGALRM));
  StopTimerNoBlock(tp);
  sigsetmask(omask);
}

void
timer_Start(struct pppTimer *tp)
{
  struct itimerval itimer;
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

  /* Adjust our first delta so that it reflects what's really happening */
  if (TimerList && getitimer(ITIMER_REAL, &itimer) == 0)
    TimerList->rest = RESTVAL(itimer);

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
    TimerList = tp;
    timer_InitService(t != NULL);	/* [re]Start the Timer Service */
  }
  if (t)
    t->rest -= tp->rest;

  sigsetmask(omask);
}

static void
StopTimerNoBlock(struct pppTimer *tp)
{
  struct pppTimer *t, *pt;

  /*
   * A RUNNING timer must be removed from TimerList (->next list).
   * A STOPPED timer isn't in any list, but may have a bogus [e]next field.
   * An EXPIRED timer is in the ->enext list.
   */

  if (tp->state == TIMER_STOPPED)
    return;

  pt = NULL;
  for (t = TimerList; t != tp && t != NULL; t = t->next)
    pt = t;

  if (t) {
    if (pt)
      pt->next = t->next;
    else {
      TimerList = t->next;
      if (TimerList == NULL)	/* Last one ? */
	timer_TermService();	/* Terminate Timer Service */
    }
    if (t->next) {
      if (!pt) {		/* t (tp) was the first in the list */
        struct itimerval itimer;

        if (getitimer(ITIMER_REAL, &itimer) == 0)
          t->rest = RESTVAL(itimer);
      }
      t->next->rest += t->rest;
      if (!pt)			/* t->next is now the first in the list */
        timer_InitService(1);
    }
  } else {
    /* Search for any pending expired timers */
    pt = NULL;
    for (t = ExpiredList; t != tp && t != NULL; t = t->enext)
      pt = t;

    if (t) {
      if (pt)
        pt->enext = t->enext;
      else
        ExpiredList = t->enext;
    } else if (tp->state == TIMER_RUNNING)
      log_Printf(LogERROR, "Oops, %s timer not found!!\n", tp->name);
  }

  tp->next = tp->enext = NULL;
  tp->state = TIMER_STOPPED;
}

static void
TimerService(void)
{
  struct pppTimer *tp, *exp, *next;

  if (log_IsKept(LogTIMER)) {
    static time_t t;		/* Only show timers globally every second */
    time_t n = time(NULL);

    if (n > t)
      timer_Show(LogTIMER, NULL);
    t = n;
  }

  tp = TimerList;
  if (tp) {
    tp->rest = 0;

    /* Multiple timers might expire at once. Create a list of expired timers */
    exp = NULL;
    do {
      tp->state = TIMER_EXPIRED;
      next = tp->next;
      tp->enext = exp;
      exp = tp;
      tp = next;
    } while (tp && tp->rest == 0);
  
    TimerList = tp;
    if (TimerList != NULL)	/* Any timers remaining ? */
      timer_InitService(1);	/* Restart the Timer Service */
    else
      timer_TermService();	/* Stop the Timer Service */
  
    /* Process all expired timers */
    while (exp) {
      ExpiredList = exp->enext;
      exp->enext = NULL;
      if (exp->func)
        (*exp->func)(exp->arg);
      exp = ExpiredList;
    }
  }
}

void
timer_Show(int LogLevel, struct prompt *prompt)
{
  struct itimerval itimer;
  struct pppTimer *pt;
  u_long rest = 0;

  /* Adjust our first delta so that it reflects what's really happening */
  if (TimerList && getitimer(ITIMER_REAL, &itimer) == 0)
    TimerList->rest = RESTVAL(itimer);

#define SECS(val)	((val) / SECTICKS)
#define HSECS(val)	(((val) % SECTICKS) * 100 / SECTICKS)
#define DISP								\
  "%s timer[%p]: freq = %ld.%02lds, next = %lu.%02lus, state = %s\n",	\
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
timer_InitService(int restart)
{
  struct itimerval itimer;

  if (TimerList) {
    if (!restart)
      sig_signal(SIGALRM, (void (*)(int))TimerService);
    itimer.it_interval.tv_sec = 0;
    itimer.it_interval.tv_usec = 0;
    itimer.it_value.tv_sec = TimerList->rest / SECTICKS;
    itimer.it_value.tv_usec = (TimerList->rest % SECTICKS) * TICKUNIT;
    if (setitimer(ITIMER_REAL, &itimer, NULL) == -1)
      log_Printf(LogERROR, "Unable to set itimer (%s)\n", strerror(errno));
  }
}

void 
timer_TermService(void)
{
  struct itimerval itimer;

  itimer.it_interval.tv_usec = itimer.it_interval.tv_sec = 0;
  itimer.it_value.tv_usec = itimer.it_value.tv_sec = 0;
  if (setitimer(ITIMER_REAL, &itimer, NULL) == -1)
    log_Printf(LogERROR, "Unable to set itimer (%s)\n", strerror(errno));
  sig_signal(SIGALRM, SIG_IGN);
}
