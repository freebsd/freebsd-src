/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *	@(#)sched.c	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 *
 */

/*
 * Process scheduler
 */

#include "am.h"
#include <sys/signal.h>
#include WAIT
#include <setjmp.h>
extern jmp_buf select_intr;
extern int select_intr_valid;

typedef struct pjob pjob;
struct pjob {
	qelem hdr;			/* Linked list */
	int pid;			/* Process ID of job */
	cb_fun cb_fun;			/* Callback function */
	voidp cb_closure;		/* Closure for callback */
	union wait w;			/* Status filled in by sigchld */
	voidp wchan;			/* Wait channel */
};

extern qelem proc_list_head;
qelem proc_list_head = { &proc_list_head, &proc_list_head };
extern qelem proc_wait_list;
qelem proc_wait_list = { &proc_wait_list, &proc_wait_list };

int task_notify_todo;

void ins_que(elem, pred)
qelem *elem, *pred;
{
	qelem *p = pred->q_forw;
	elem->q_back = pred;
	elem->q_forw = p;
	pred->q_forw = elem;
	p->q_back = elem;
}

void rem_que(elem)
qelem *elem;
{
	qelem *p = elem->q_forw;
	qelem *p2 = elem->q_back;
	p2->q_forw = p;
	p->q_back = p2;
}

static pjob *sched_job(cf, ca)
cb_fun cf;
voidp ca;
{
	pjob *p = ALLOC(pjob);

	p->cb_fun = cf;
	p->cb_closure = ca;

	/*
	 * Now place on wait queue
	 */
	ins_que(&p->hdr, &proc_wait_list);

	return p;
}

void run_task(tf, ta, cf, ca)
task_fun tf;
voidp ta;
cb_fun cf;
voidp ca;
{
	pjob *p = sched_job(cf, ca);
	int mask;

	p->wchan = (voidp) p;

	mask = sigblock(sigmask(SIGCHLD));

	if (p->pid = background()) {
		sigsetmask(mask);
		return;
	}

	exit((*tf)(ta));
	/* firewall... */
	abort();
}

/*
 * Schedule a task to be run when woken up
 */
void sched_task(cf, ca, wchan)
cb_fun cf;
voidp ca;
voidp wchan;
{
	/*
	 * Allocate a new task
	 */
	pjob *p = sched_job(cf, ca);
#ifdef DEBUG_SLEEP
	dlog("SLEEP on %#x", wchan);
#endif
	p->wchan = wchan;
	p->pid = 0;
	bzero((voidp) &p->w, sizeof(p->w));
}

static void wakeupjob(p)
pjob *p;
{
	rem_que(&p->hdr);
	ins_que(&p->hdr, &proc_list_head);
	task_notify_todo++;
}

void wakeup(wchan)
voidp wchan;
{
	pjob *p, *p2;
#ifdef DEBUG_SLEEP
	int done = 0;
#endif
	if (!foreground)
		return;

#ifdef DEBUG_SLEEP
	/*dlog("wakeup(%#x)", wchan);*/
#endif
	/*
	 * Can't user ITER() here because
	 * wakeupjob() juggles the list.
	 */
	for (p = FIRST(pjob, &proc_wait_list);
			p2 = NEXT(pjob, p), p != HEAD(pjob, &proc_wait_list);
			p = p2) {
		if (p->wchan == wchan) {
#ifdef DEBUG_SLEEP
			done = 1;
#endif
			wakeupjob(p);
		}
	}

#ifdef DEBUG_SLEEP
	if (!done)
		dlog("Nothing SLEEPing on %#x", wchan);
#endif
}

void wakeup_task(rc, term, cl)
int rc;
int term;
voidp cl;
{
	wakeup(cl);
}

/*ARGSUSED*/

void sigchld(sig)
int sig;
{
	union wait w;
	int pid;

#ifdef SYS5_SIGNALS
	if ((pid = wait(&w)) > 0) {
#else
	while ((pid = wait3((int *) &w, WNOHANG, (struct rusage *) 0)) > 0) {
#endif /* SYS5_SIGNALS */
		pjob *p, *p2;

		if (WIFSIGNALED(w))
			plog(XLOG_ERROR, "Process %d exited with signal %d",
				pid, w.w_termsig);
#ifdef DEBUG
		else
			dlog("Process %d exited with status %d",
				pid, w.w_retcode);
#endif /* DEBUG */

		for (p = FIRST(pjob, &proc_wait_list);
				p2 = NEXT(pjob, p), p != HEAD(pjob, &proc_wait_list);
				p = p2) {
			if (p->pid == pid) {
				p->w = w;
				wakeupjob(p);
				break;
			}
		}

#ifdef DEBUG
		if (p) ; else dlog("can't locate task block for pid %d", pid);
#endif /* DEBUG */
	}

#ifdef SYS5_SIGNALS
	signal(sig, sigchld);
#endif /* SYS5_SIGNALS */
	if (select_intr_valid)
		longjmp(select_intr, sig);
}

/*
 * Run any pending tasks.
 * This must be called with SIGCHLD disabled
 */
void do_task_notify(P_void)
{
	/*
	 * Keep taking the first item off the list and processing it.
	 *
	 * Done this way because the the callback can, quite reasonably,
	 * queue a new task, so no local reference into the list can be
	 * held here.
	 */
	while (FIRST(pjob, &proc_list_head) != HEAD(pjob, &proc_list_head)) {
		pjob *p = FIRST(pjob, &proc_list_head);
		rem_que(&p->hdr);
		/*
		 * This job has completed
		 */
		--task_notify_todo;

		/*
		 * Do callback if it exists
		 */
		if (p->cb_fun)
			(*p->cb_fun)(p->w.w_retcode,
				p->w.w_termsig, p->cb_closure);

		free((voidp) p);
	}
}

#ifdef HAS_SVR3_SIGNALS
/*
 * 4.2 signal library based on svr3 (4.1+ bsd) interface
 * From Stephen C. Pope <scp@acl.lanl.gov).
 */

static int current_mask = 0;

int sigblock(mask)
int mask;
{
    int sig;
    int m;
    int oldmask;

    oldmask = current_mask;
    for ( sig = 1, m = 1; sig <= MAXSIG; sig++, m <<= 1 ) {
        if (mask & m)  {
	    sighold(sig);
            current_mask |= m;
        }
    }
    return oldmask;
}

int sigsetmask(mask)
int mask;
{
    int sig;
    int m;
    int oldmask;

    oldmask = current_mask;
    for ( sig = 1, m = 1; sig <= MAXSIG; sig++, m <<= 1 ) {
        if (mask & m)  {
            sighold(sig);
            current_mask |= m;
        }
        else  {
            sigrelse(sig);
            current_mask &= ~m;
        }
    }
    return oldmask;
}

#endif /* HAS_SVR3_SIGNALS */
