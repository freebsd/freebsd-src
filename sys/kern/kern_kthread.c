/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpuset.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/sx.h>
#include <sys/umtxvar.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <sys/sched.h>
#include <sys/stdarg.h>
#include <sys/tslog.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

/*
 * Start a kernel process.  This is called after a fork() call in
 * mi_startup() in the file kern/init_main.c.
 *
 * This function is used to start "internal" daemons and intended
 * to be called from SYSINIT().
 */
void
kproc_start(const void *udata)
{
	const struct kproc_desc	*kp = udata;
	int error;

	error = kproc_create((void (*)(void *))kp->func, NULL,
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
static int
kproc_create1(void (*func)(void *), void *arg,
    struct proc **newpp, int flags, int pages, const char *tdname)
{
	struct fork_req fr;
	int error;
	struct thread *td;
	struct proc *p2;

	if (!proc0.p_stats)
		panic("kproc_create called too soon");

	bzero(&fr, sizeof(fr));
	fr.fr_flags = RFMEM | RFFDG | RFPROC | RFSTOPPED | flags;
	fr.fr_flags2 = FR2_KPROC;
	fr.fr_pages = pages;
	fr.fr_procp = &p2;
	error = fork1(&thread0, &fr);
	if (error != 0)
		return (error);

	/* save a global descriptor, if desired */
	if (newpp != NULL)
		*newpp = p2;

	/* set up arg0 for 'ps', et al */
	strcpy(p2->p_comm, tdname);
	td = FIRST_THREAD_IN_PROC(p2);
	strcpy(td->td_name, tdname);
#ifdef KTR
	sched_clear_tdname(td);
#endif
	TSTHREAD(td, td->td_name);
#ifdef HWPMC_HOOKS
	if (PMC_SYSTEM_SAMPLING_ACTIVE()) {
		PMC_CALL_HOOK_UNLOCKED(td, PMC_FN_PROC_CREATE_LOG, p2);
		PMC_CALL_HOOK_UNLOCKED(td, PMC_FN_THR_CREATE_LOG, NULL);
	}
#endif

	/* call the processes' main()... */
	cpu_fork_kthread_handler(td, func, arg);

	/* Avoid inheriting affinity from a random parent. */
	cpuset_kernthread(td);
	thread_lock(td);
	TD_SET_CAN_RUN(td);
	sched_prio(td, PVM);
	sched_user_prio(td, PUSER);

	/* Delay putting it on the run queue until now. */
	if ((flags & RFSTOPPED) == 0)
		sched_add(td, SRQ_BORING); 
	else
		thread_unlock(td);

	return (0);
}

int
kproc_create(void (*func)(void *), void *arg,
    struct proc **newpp, int flags, int pages, const char *fmt, ...)
{
	va_list ap;
	int error;
	char tdname[MAXCOMLEN + 1];

	va_start(ap, fmt);
	vsnprintf(tdname, sizeof(tdname), fmt, ap);
	va_end(ap);
	DROP_GIANT();
	error = kproc_create1(func, arg, newpp, flags, pages, tdname);
	PICKUP_GIANT();
	return (error);
}

void
kproc_exit(int ecode)
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
	proc_reparent(p, initproc, true);
	PROC_UNLOCK(p);
	sx_xunlock(&proctree_lock);

	/*
	 * Wakeup anyone waiting for us to exit.
	 */
	wakeup(p);

	/* Buh-bye! */
	exit1(td, ecode, 0);
}

/*
 * Advise a kernel process to suspend (or resume) in its main loop.
 * Participation is voluntary.
 */
int
kproc_suspend(struct proc *p, int timo)
{
	/*
	 * Make sure this is indeed a system process and we can safely
	 * use the p_siglist field.
	 */
	PROC_LOCK(p);
	if ((p->p_flag & P_KPROC) == 0) {
		PROC_UNLOCK(p);
		return (EINVAL);
	}
	SIGADDSET(p->p_siglist, SIGSTOP);
	wakeup(p);
	return (msleep(&p->p_siglist, &p->p_mtx, PPAUSE | PDROP,
	    "suspkp", timo));
}

int
kproc_resume(struct proc *p)
{
	/*
	 * Make sure this is indeed a system process and we can safely
	 * use the p_siglist field.
	 */
	PROC_LOCK(p);
	if ((p->p_flag & P_KPROC) == 0) {
		PROC_UNLOCK(p);
		return (EINVAL);
	}
	SIGDELSET(p->p_siglist, SIGSTOP);
	PROC_UNLOCK(p);
	wakeup(&p->p_siglist);
	return (0);
}

void
kproc_suspend_check(struct proc *p)
{
	PROC_LOCK(p);
	while (SIGISMEMBER(p->p_siglist, SIGSTOP)) {
		wakeup(&p->p_siglist);
		msleep(&p->p_siglist, &p->p_mtx, PPAUSE, "kpsusp", 0);
	}
	PROC_UNLOCK(p);
}

/*
 * Start a kernel thread.  
 *
 * This function is used to start "internal" daemons and intended
 * to be called from SYSINIT().
 */

void
kthread_start(const void *udata)
{
	const struct kthread_desc *kp = udata;
	int error;

	error = kthread_add((void (*)(void *))kp->func, NULL,
	    NULL, kp->global_threadpp, 0, 0, "%s", kp->arg0);
	if (error)
		panic("kthread_start: %s: error %d", kp->arg0, error);
}

/*
 * Create a kernel thread.  It shares its address space
 * with proc0 - ie: kernel only.
 *
 * func is the function to start.
 * arg is the parameter to pass to function on first startup.
 * newtdp is the return value pointing to the thread's struct thread.
 *  ** XXX fix this --> flags are flags to fork1 (in unistd.h) 
 * fmt and following will be *printf'd into (*newtd)->td_name (for ps, etc.).
 */
static int
kthread_add1(void (*func)(void *), void *arg, struct proc *p,
    struct thread **newtdp, int flags, int pages, const char *tdname)
{
	struct thread *newtd, *oldtd;

	if (!proc0.p_stats)
		panic("kthread_add called too soon");

	/* If no process supplied, put it on proc0 */
	if (p == NULL)
		p = &proc0;

	/* Initialize our new td  */
	newtd = thread_alloc(pages);
	if (newtd == NULL)
		return (ENOMEM);

	PROC_LOCK(p);
	if (p->p_state == PRS_ZOMBIE || (p->p_flag2 & P2_WEXIT) != 0) {
		PROC_UNLOCK(p);
		return (ESRCH);
	}
	oldtd = FIRST_THREAD_IN_PROC(p);

	/*
	 * Set the new thread pointer before the thread starts running: *newtdp
	 * could be a pointer that is referenced by "func".
	 */
	if (newtdp != NULL)
		*newtdp = newtd;

	bzero(&newtd->td_startzero,
	    __rangeof(struct thread, td_startzero, td_endzero));
	bcopy(&oldtd->td_startcopy, &newtd->td_startcopy,
	    __rangeof(struct thread, td_startcopy, td_endcopy));

	/* set up arg0 for 'ps', et al */
	strcpy(newtd->td_name, tdname);

	TSTHREAD(newtd, newtd->td_name);

	newtd->td_proc = p;  /* needed for cpu_copy_thread */
	newtd->td_pflags |= TDP_KTHREAD;

	/* might be further optimized for kthread */
	cpu_copy_thread(newtd, oldtd);

	/* put the designated function(arg) as the resume context */
	cpu_fork_kthread_handler(newtd, func, arg);

	thread_cow_get_proc(newtd, p);

	/* This code is similar to thread_create() in kern_thr.c. */
	p->p_flag |= P_HADTHREADS;
	thread_link(newtd, p);
	thread_lock(oldtd);
	/* let the scheduler know about these things. */
	sched_fork_thread(oldtd, newtd);
	TD_SET_CAN_RUN(newtd);
	thread_unlock(oldtd);
	PROC_UNLOCK(p);

	tidhash_add(newtd);

	/* Avoid inheriting affinity from a random parent. */
	cpuset_kernthread(newtd);
#ifdef HWPMC_HOOKS
	if (PMC_SYSTEM_SAMPLING_ACTIVE())
		PMC_CALL_HOOK_UNLOCKED(td, PMC_FN_THR_CREATE_LOG, NULL);
#endif
	/* Delay putting it on the run queue until now. */
	if ((flags & RFSTOPPED) == 0) {
		thread_lock(newtd);
		sched_add(newtd, SRQ_BORING); 
	}
	return (0);
}

int
kthread_add(void (*func)(void *), void *arg, struct proc *p,
    struct thread **newtdp, int flags, int pages, const char *fmt, ...)
{
	va_list ap;
	int error;
	char tdname[MAXCOMLEN + 1];

	va_start(ap, fmt);
	vsnprintf(tdname, sizeof(tdname), fmt, ap);
	va_end(ap);
	DROP_GIANT();
	error = kthread_add1(func, arg, p, newtdp, flags, pages, tdname);
	PICKUP_GIANT();
	return (error);
}

void
kthread_exit(void)
{
	struct proc *p;
	struct thread *td;

	td = curthread;
	p = td->td_proc;

#ifdef HWPMC_HOOKS
	if (PMC_SYSTEM_SAMPLING_ACTIVE())
		PMC_CALL_HOOK_UNLOCKED(td, PMC_FN_THR_EXIT_LOG, NULL);
#endif
	/* A module may be waiting for us to exit. */
	wakeup(td);

	/*
	 * The last exiting thread in a kernel process must tear down
	 * the whole process.
	 */
	PROC_LOCK(p);
	if (p->p_numthreads == 1) {
		PROC_UNLOCK(p);
		kproc_exit(0);
	}

	if (p->p_sysent->sv_ontdexit != NULL)
		p->p_sysent->sv_ontdexit(td);

	tidhash_remove(td);
	umtx_thread_exit(td);
	tdsigcleanup(td);
	PROC_SLOCK(p);
	thread_exit();
}

/*
 * Advise a kernel process to suspend (or resume) in its main loop.
 * Participation is voluntary.
 */
int
kthread_suspend(struct thread *td, int timo)
{
	struct proc *p;

	p = td->td_proc;

	/*
	 * td_pflags should not be read by any thread other than
	 * curthread, but as long as this flag is invariant during the
	 * thread's lifetime, it is OK to check its state.
	 */
	if ((td->td_pflags & TDP_KTHREAD) == 0)
		return (EINVAL);

	/*
	 * The caller of the primitive should have already checked that the
	 * thread is up and running, thus not being blocked by other
	 * conditions.
	 */
	PROC_LOCK(p);
	thread_lock(td);
	td->td_flags |= TDF_KTH_SUSP;
	thread_unlock(td);
	return (msleep(&td->td_flags, &p->p_mtx, PPAUSE | PDROP, "suspkt",
	    timo));
}

/*
 * Resume a thread previously put asleep with kthread_suspend().
 */
int
kthread_resume(struct thread *td)
{
	struct proc *p;

	p = td->td_proc;

	/*
	 * td_pflags should not be read by any thread other than
	 * curthread, but as long as this flag is invariant during the
	 * thread's lifetime, it is OK to check its state.
	 */
	if ((td->td_pflags & TDP_KTHREAD) == 0)
		return (EINVAL);

	PROC_LOCK(p);
	thread_lock(td);
	td->td_flags &= ~TDF_KTH_SUSP;
	thread_unlock(td);
	wakeup(&td->td_flags);
	PROC_UNLOCK(p);
	return (0);
}

/*
 * Used by the thread to poll as to whether it should yield/sleep
 * and notify the caller that is has happened.
 */
void
kthread_suspend_check(void)
{
	struct proc *p;
	struct thread *td;

	td = curthread;
	p = td->td_proc;

	if ((td->td_pflags & TDP_KTHREAD) == 0)
		panic("%s: curthread is not a valid kthread", __func__);

	/*
	 * Setting the TDF_KTH_SUSP flag is protected by process lock.
	 *
	 * Do an unlocked read first to avoid serializing with all other threads
	 * in the common case of not suspending.
	 */
	if ((td->td_flags & TDF_KTH_SUSP) == 0)
		return;
	PROC_LOCK(p);
	while ((td->td_flags & TDF_KTH_SUSP) != 0) {
		wakeup(&td->td_flags);
		msleep(&td->td_flags, &p->p_mtx, PPAUSE, "ktsusp", 0);
	}
	PROC_UNLOCK(p);
}

int
kproc_kthread_add(void (*func)(void *), void *arg,
    struct proc **procptr, struct thread **tdptr,
    int flags, int pages, const char *procname, const char *fmt, ...)
{
	int error;
	va_list ap;
	char buf[100];
	struct thread *td;

	if (*procptr == NULL) {
		/*
		 * Use RFSTOPPED to ensure that *tdptr is initialized before the
		 * thread starts running.
		 */
		error = kproc_create(func, arg,
		    procptr, flags | RFSTOPPED, pages, "%s", procname);
		if (error)
			return (error);
		td = FIRST_THREAD_IN_PROC(*procptr);
		if (tdptr)
			*tdptr = td;
		if ((flags & RFSTOPPED) == 0) {
			thread_lock(td);
			sched_add(td, SRQ_BORING);
		}
		va_start(ap, fmt);
		vsnprintf(td->td_name, sizeof(td->td_name), fmt, ap);
		va_end(ap);
#ifdef KTR
		sched_clear_tdname(td);
#endif
		return (0); 
	}
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	error = kthread_add(func, arg, *procptr,
	    tdptr, flags, pages, "%s", buf);
	return (error);
}
