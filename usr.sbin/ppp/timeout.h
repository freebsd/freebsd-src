/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $FreeBSD$
 *
 *	TODO:
 */

#ifndef _TIMEOUT_H_
#define	_TIMEOUT_H_
#include "cdefs.h"

#define	TICKUNIT	100000			/* Unit in usec */
#define	SECTICKS	(1000000/TICKUNIT)

struct pppTimer {
  int     state;
  u_long  rest;			/* Ticks to expire */
  u_long  load;			/* Initial load value */
  void	  (*func)();		/* Function called when timer is expired */
  void    *arg;			/* Argument passed to timeout function */
  struct pppTimer *next;	/* Link to next timer */
  struct pppTimer *enext;	/* Link to next expired timer */
};

#define	TIMER_STOPPED	0
#define	TIMER_RUNNING	1
#define	TIMER_EXPIRED	2

struct pppTimer *TimerList;
extern int TimerServiceRequest;	/* If this is >0, call TimerService() */

extern void StartTimer __P((struct pppTimer *));
extern void StopTimer __P((struct pppTimer *));
extern void TimerService __P((void));
extern void InitTimerService __P((void));
extern void TermTimerService __P((void));
extern void StartIdleTimer __P((void));
#endif	/* _TIMEOUT_H_ */
