/*-
 * Copyright (c) 1999 Peter Wemm <peter@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/kern_kthread.c,v 1.38 2007/06/05 00:00:54 jeff Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <sys/sched.h>

#include <machine/stdarg.h>

/*
 * Start a kernel process.  This is called after a fork() call in
 * mi_startup() in the file kern/init_main.c.
 *
 * This function is used to start "internal" daemons and intended
 * to be called from SYSINIT().
 */
void
kproc_start(udata)
	const void *udata;
{
	const struct kproc_desc	*kp = udata;
	int error;

	error = kthread_create((void (*)(void *))kp->func, NULL,
		    kp->global_procpp, 0, 0, "%s", kp->arg0);
	if (error)
		panic("kproc_start: %s: error %d", kp->arg0, error);
}

/*
 * Create a kernel process/thread/whatever.  It shares its address space
 * with proc0 - ie: kernel only.
 *
 * func is the function to start.
 * arg is the parameter to pass to function on first startup.
 * newpp is the return value pointing to the thread's struct proc.
 * flags are flags to fork1 (in unistd.h)
 * fmt and following will be *printf'd into (*newpp)->p_comm (for ps, etc.).
 */
int
kthread_create(void (*func)(void *), void *arg,
    struct proc **newpp, int flags, int pages, const char *fmt, ...)
{
	int error;
	va_list ap;
	struct thread *td;
	struct proc *p2;

	if (!proc0.p_stats)
		panic("kthread_create called too soon");

	error = fork1(&thread0, RFMEM | RFFDG | RFPROC | RFSTOPPED | flags,
	    pages, &p2);
	if (error)
		return error;

	/* save a global descriptor, if desired */
	if (newpp != NULL)
		*newpp = p2;

	/* this is a non-swapped system process */
	PROC_LOCK(p2);
	p2->p_flag |= P_SYSTEM | P_KTHREAD;
	mtx_lock(&p2->p_sigacts->ps_mtx);
	p2->p_sigacts->ps_flag |= PS_NOCLDWAIT;
	mtx_unlock(&p2->p_sigacts->ps_mtx);
	PROC_UNLOCK(p2);

	/* set up arg0 for 'ps', et al */
	va_start(ap, fmt);
	vsnprintf(p2->p_comm, sizeof(p2->p_comm), fmt, ap);
	va_end(ap);

	/* call the processes' main()... */
	td = FIRST_THREAD_IN_PROC(p2);
	cpu_set_fork_handler(td, func, arg);
	TD_SET_CAN_RUN(td);

	/* Delay putting it on the run queue until now. */
	if (!(flags & RFSTOPPED)) {
		thread_lock(td);
		sched_add(td, SRQ_BORING); 
		thread_unlock(td);
	}

	return 0;
}

void
kthread_exit(int ecode)
{
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;

	/*
	 * Reparent curthread from proc0 to init so that the zombie
	 * is harvested.
	 */
	sx_xlock(&proctree_lock);
	PROC_LOCK(p);
	proc_reparent(p, initproc);
	PROC_UNLOCK(p);
	sx_xunlock(&proctree_lock);

	/*
	 * Wakeup anyone waiting for us to exit.
	 */
	wakeup(p);

	/* Buh-bye! */
	exit1(td, W_EXITCODE(ecode, 0));
}

/*
 * Advise a kernel process to suspend (or resume) in its main loop.
 * Participation is voluntary.
 */
int
kthread_suspend(struct proc *p, int timo)
{
	/*
	 * Make sure this is indeed a system process and we can safely
	 * use the p_siglist field.
	 */
	PROC_LOCK(p);
	if ((p->p_flag & P_KTHREAD) == 0) {
		PROC_UNLOCK(p);
		return (EINVAL);
	}
	SIGADDSET(p->p_siglist, SIGSTOP);
	wakeup(p);
	return msleep(&p->p_siglist, &p->p_mtx, PPAUSE | PDROP, "suspkt", timo);
}

int
kthread_resume(struct proc *p)
{
	/*
	 * Make sure this is indeed a system process and we can safely
	 * use the p_siglist field.
	 */
	PROC_LOCK(p);
	if ((p->p_flag & P_KTHREAD) == 0) {
		PROC_UNLOCK(p);
		return (EINVAL);
	}
	SIGDELSET(p->p_siglist, SIGSTOP);
	PROC_UNLOCK(p);
	wakeup(&p->p_siglist);
	return (0);
}

void
kthread_suspend_check(struct proc *p)
{
	PROC_LOCK(p);
	while (SIGISMEMBER(p->p_siglist, SIGSTOP)) {
		wakeup(&p->p_siglist);
		msleep(&p->p_siglist, &p->p_mtx, PPAUSE, "ktsusp", 0);
	}
	PROC_UNLOCK(p);
}
