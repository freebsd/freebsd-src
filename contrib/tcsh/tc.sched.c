/* $Header: /src/pub/tcsh/tc.sched.c,v 3.21 2004/11/23 02:10:50 christos Exp $ */
/*
 * tc.sched.c: Scheduled command execution
 *
 * Karl Kleinpaste: Computer Consoles Inc. 1984
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */
#include "sh.h"

RCSID("$Id: tc.sched.c,v 3.21 2004/11/23 02:10:50 christos Exp $")

#include "ed.h"
#include "tw.h"
#include "tc.h"

extern int just_signaled;

struct sched_event {
    struct sched_event *t_next;
    time_t t_when;
    Char  **t_lex;
};
static struct sched_event *sched_ptr = NULL;


time_t
sched_next()
{
    if (sched_ptr)
	return (sched_ptr->t_when);
    return ((time_t) - 1);
}

/*ARGSUSED*/
void
dosched(v, c)
    Char **v;
    struct command *c;
{
    struct sched_event *tp, *tp1, *tp2;
    time_t  cur_time;
    int     count, hours, minutes, dif_hour, dif_min;
    Char   *cp;
    int    relative;		/* time specified as +hh:mm */
    struct tm *ltp;

    USE(c);
/* This is a major kludge because of a gcc linker  */
/* Problem.  It may or may not be needed for you   */
#if defined(_MINIX) && !defined(_MINIX_VMD)
    char kludge[10];
    extern char *sprintf();
    sprintf(kludge, CGETS(24, 1, "kludge"));
#endif /* _MINIX && !_MINIX_VMD */

    v++;
    cp = *v++;
    if (cp == NULL) {
	Char   *fmt;
	if ((fmt = varval(STRsched)) == STRNULL)
	    fmt = str2short("%h\t%T\t%R\n");
	/* print list of scheduled events */
	for (count = 1, tp = sched_ptr; tp; count++, tp = tp->t_next) {
	    Char buf[BUFSIZE], sbuf[BUFSIZE];
	    blkexpand(tp->t_lex, buf);
	    tprintf(FMT_SCHED, sbuf, fmt, sizeof(sbuf), 
		    short2str(buf), tp->t_when, (ptr_t) &count);
	    for (cp = sbuf; *cp;)
		xputwchar(*cp++);
	}
	return;
    }

    if (*cp == '-') {
	/* remove item from list */
	if (!sched_ptr)
	    stderror(ERR_NOSCHED);
	if (*v)
	    stderror(ERR_SCHEDUSAGE);
	count = atoi(short2str(++cp));
	if (count <= 0)
	    stderror(ERR_SCHEDUSAGE);
	tp = sched_ptr;
	tp1 = 0;
	while (--count) {
	    if (tp->t_next == 0)
		break;
	    else {
		tp1 = tp;
		tp = tp->t_next;
	    }
	}
	if (count)
	    stderror(ERR_SCHEDEV);
	if (tp1 == 0)
	    sched_ptr = tp->t_next;
	else
	    tp1->t_next = tp->t_next;
	blkfree(tp->t_lex);
	xfree((ptr_t) tp);
	return;
    }

    /* else, add an item to the list */
    if (!*v)
	stderror(ERR_SCHEDCOM);
    relative = 0;
    if (!Isdigit(*cp)) {	/* not abs. time */
	if (*cp != '+')
	    stderror(ERR_SCHEDUSAGE);
	cp++, relative++;
    }
    minutes = 0;
    hours = atoi(short2str(cp));
    while (*cp && *cp != ':' && *cp != 'a' && *cp != 'p')
	cp++;
    if (*cp && *cp == ':')
	minutes = atoi(short2str(++cp));
    if ((hours < 0) || (minutes < 0) ||
	(hours > 23) || (minutes > 59))
	stderror(ERR_SCHEDTIME);
    while (*cp && *cp != 'p' && *cp != 'a')
	cp++;
    if (*cp && relative)
	stderror(ERR_SCHEDREL);
    if (*cp == 'p')
	hours += 12;
    (void) time(&cur_time);
    ltp = localtime(&cur_time);
    if (relative) {
	dif_hour = hours;
	dif_min = minutes;
    }
    else {
	if ((dif_hour = hours - ltp->tm_hour) < 0)
	    dif_hour += 24;
	if ((dif_min = minutes - ltp->tm_min) < 0) {
	    dif_min += 60;
	    if ((--dif_hour) < 0)
		dif_hour = 23;
	}
    }
    tp = (struct sched_event *) xcalloc(1, sizeof *tp);
#ifdef _SX
    tp->t_when = cur_time - ltp->tm_sec + dif_hour * 3600 + dif_min * 60;
#else	/* _SX */	
    tp->t_when = cur_time - ltp->tm_sec + dif_hour * 3600L + dif_min * 60L;
#endif /* _SX */
    /* use of tm_sec: get to beginning of minute. */
    if (!sched_ptr || tp->t_when < sched_ptr->t_when) {
	tp->t_next = sched_ptr;
	sched_ptr = tp;
    }
    else {
	tp1 = sched_ptr->t_next;
	tp2 = sched_ptr;
	while (tp1 && tp->t_when >= tp1->t_when) {
	    tp2 = tp1;
	    tp1 = tp1->t_next;
	}
	tp->t_next = tp1;
	tp2->t_next = tp;
    }
    tp->t_lex = saveblk(v);
}

/*
 * Execute scheduled events
 */
/*ARGSUSED*/
void
sched_run(n)
    int n;
{
    time_t   cur_time;
    struct sched_event *tp, *tp1;
    struct wordent cmd, *nextword, *lastword;
    struct command *t;
    Char  **v, *cp;
#ifdef BSDSIGS
    sigmask_t omask;

    omask = sigblock(sigmask(SIGINT)) & ~sigmask(SIGINT);
#else
    (void) sighold(SIGINT);
#endif

    USE(n);

    (void) time(&cur_time);
    tp = sched_ptr;

    /* bugfix by: Justin Bur at Universite de Montreal */
    /*
     * this test wouldn't be necessary if this routine were not called before
     * each prompt (in sh.c).  But it is, to catch missed alarms.  Someone
     * ought to fix it all up.  -jbb
     */
    if (!(tp && tp->t_when < cur_time)) {
#ifdef BSDSIGS
	(void) sigsetmask(omask);
#else
	(void) sigrelse(SIGINT);
#endif
	return;
    }

    if (GettingInput)
	(void) Cookedmode();

    while (tp && tp->t_when < cur_time) {
	if (seterr) {
	    xfree((ptr_t) seterr);
	    seterr = NULL;
	}
	cmd.word = STRNULL;
	lastword = &cmd;
	v = tp->t_lex;
	for (cp = *v; cp; cp = *++v) {
	    nextword = (struct wordent *) xcalloc(1, sizeof cmd);
	    nextword->word = Strsave(cp);
	    lastword->next = nextword;
	    nextword->prev = lastword;
	    lastword = nextword;
	}
	lastword->next = &cmd;
	cmd.prev = lastword;
	tp1 = tp;
	sched_ptr = tp = tp1->t_next;	/* looping termination cond: */
	blkfree(tp1->t_lex);	/* straighten out in case of */
	xfree((ptr_t) tp1);	/* command blow-up. */

	/* expand aliases like process() does. */
	alias(&cmd);
	/* build a syntax tree for the command. */
	t = syntax(cmd.next, &cmd, 0);
	if (seterr)
	    stderror(ERR_OLD);
	/* execute the parse tree. */
	execute(t, -1, NULL, NULL, TRUE);
	/* done. free the lex list and parse tree. */
	freelex(&cmd), freesyn(t);
    }
    if (GettingInput && !just_signaled) {	/* PWP */
	(void) Rawmode();
	ClearLines();		/* do a real refresh since something may */
	ClearDisp();		/* have printed to the screen */
	Refresh();
    }
    just_signaled = 0;

#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else
    (void) sigrelse(SIGINT);
#endif
}
