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
 * $Id: timer.h,v 1.5.4.5 1998/05/08 01:15:19 brian Exp $
 *
 *	TODO:
 */

#define	TICKUNIT	100000			/* usec's per Unit */
#define	SECTICKS	(1000000/TICKUNIT)	/* Units per second */

struct pppTimer {
  int state;
  const char *name;
  u_long rest;			/* Ticks to expire */
  u_long load;			/* Initial load value */
  void (*func)(void *);		/* Function called when timer is expired */
  void *arg;			/* Argument passed to timeout function */
  struct pppTimer *next;	/* Link to next timer */
  struct pppTimer *enext;	/* Link to next expired timer */
};

#define	TIMER_STOPPED	0
#define	TIMER_RUNNING	1
#define	TIMER_EXPIRED	2

struct prompt;

extern void timer_Start(struct pppTimer *);
extern void timer_Stop(struct pppTimer *);
extern void timer_TermService(void);
extern void timer_Show(int LogLevel, struct prompt *);
