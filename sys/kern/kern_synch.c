/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_synch.c	8.9 (Berkeley) 5/19/95
 * $FreeBSD$
 */

#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#ifdef DDB
#include <ddb/ddb.h>
#endif
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>

static void sched_setup(void *dummy);
SYSINIT(sched_setup, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, sched_setup, NULL)

int	hogticks;
int	lbolt;

static struct callout loadav_callout;
static struct callout lbolt_callout;

struct loadavg averunnable =
	{ {0, 0, 0}, FSCALE };	/* load average, of runnable procs */
/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
static fixpt_t cexp[3] = {
	0.9200444146293232 * FSCALE,	/* exp(-1/12) */
	0.9834714538216174 * FSCALE,	/* exp(-1/60) */
	0.9944598480048967 * FSCALE,	/* exp(-1/180) */
};

/* kernel uses `FSCALE', userland (SHOULD) use kern.fscale */
static int      fscale __unused = FSCALE;
SYSCTL_INT(_kern, OID_AUTO, fscale, CTLFLAG_RD, 0, FSCALE, "");

static void	endtsleep(void *);
static void	loadav(void *arg);
static void	lboltcb(void *arg);

/*
 * We're only looking at 7 bits of the address; everything is
 * aligned to 4, lots of things are aligned to greater powers
 * of 2.  Shift right by 8, i.e. drop the bottom 256 worth.
 */
#define TABLESIZE	128
static TAILQ_HEAD(slpquehead, thread) slpque[TABLESIZE];
#define LOOKUP(x)	(((intptr_t)(x) >> 8) & (TABLESIZE - 1))

void
sleepinit(void)
{
	int i;

	hogticks = (hz / 10) * 2;	/* Default only. */
	for (i = 0; i < TABLESIZE; i++)
		TAILQ_INIT(&slpque[i]);
}

/*
 * General sleep call.  Suspends the current process until a wakeup is
 * performed on the specified identifier.  The process will then be made
 * runnable with the specified priority.  Sleeps at most timo/hz seconds
 * (0 means no timeout).  If pri includes PCATCH flag, signals are checked
 * before and after sleeping, else signals are not checked.  Returns 0 if
 * awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
 * signal needs to be delivered, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 *
 * The mutex argument is exited before the caller is suspended, and
 * entered before msleep returns.  If priority includes the PDROP
 * flag the mutex is not entered before returning.
 */

int
msleep(ident, mtx, priority, wmesg, timo)
	void *ident;
	struct mtx *mtx;
	int priority, timo;
	const char *wmesg;
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	int sig, catch = priority & PCATCH;
	int rval = 0;
	WITNESS_SAVE_DECL(mtx);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(1, 0);
#endif
	WITNESS_SLEEP(0, &mtx->mtx_object);
	KASSERT(timo != 0 || mtx_owned(&Giant) || mtx != NULL,
	    ("sleeping without a mutex"));
	/*
	 * If we are capable of async syscalls and there isn't already
	 * another one ready to return, start a new thread
	 * and queue it as ready to run. Note that there is danger here
	 * because we need to make sure that we don't sleep allocating
	 * the thread (recursion here might be bad).
	 * Hence the TDF_INMSLEEP flag.
	 */
	if (p->p_flag & P_KSES) {
		/*
		 * Just don't bother if we are exiting
		 * and not the exiting thread or thread was marked as
		 * interrupted.
		 */
		if (catch &&
		    (((p->p_flag & P_WEXIT) && (p->p_singlethread != td)) ||
		     (td->td_flags & TDF_INTERRUPT))) {
			td->td_flags &= ~TDF_INTERRUPT;
			return (EINTR);
		}
	}
	mtx_lock_spin(&sched_lock);
	if (cold ) {
		/*
		 * During autoconfiguration, just give interrupts
		 * a chance, then just return.
		 * Don't run any other procs or panic below,
		 * in case this is the idle process and already asleep.
		 */
		if (mtx != NULL && priority & PDROP)
			mtx_unlock(mtx);
		mtx_unlock_spin(&sched_lock);
		return (0);
	}

	DROP_GIANT();

	if (mtx != NULL) {
		mtx_assert(mtx, MA_OWNED | MA_NOTRECURSED);
		WITNESS_SAVE(&mtx->mtx_object, mtx);
		mtx_unlock(mtx);
		if (priority & PDROP)
			mtx = NULL;
	}

	KASSERT(p != NULL, ("msleep1"));
	KASSERT(ident != NULL && TD_IS_RUNNING(td), ("msleep"));

	CTR5(KTR_PROC, "msleep: thread %p (pid %d, %s) on %s (%p)",
	    td, p->p_pid, p->p_comm, wmesg, ident);

	td->td_wchan = ident;
	td->td_wmesg = wmesg;
	TAILQ_INSERT_TAIL(&slpque[LOOKUP(ident)], td, td_slpq);
	TD_SET_ON_SLEEPQ(td);
	if (timo)
		callout_reset(&td->td_slpcallout, timo, endtsleep, td);
	/*
	 * We put ourselves on the sleep queue and start our timeout
	 * before calling thread_suspend_check, as we could stop there, and
	 * a wakeup or a SIGCONT (or both) could occur while we were stopped.
	 * without resuming us, thus we must be ready for sleep
	 * when cursig is called.  If the wakeup happens while we're
	 * stopped, td->td_wchan will be 0 upon return from cursig.
	 */
	if (catch) {
		CTR3(KTR_PROC, "msleep caught: thread %p (pid %d, %s)", td,
		    p->p_pid, p->p_comm);
		td->td_flags |= TDF_SINTR;
		mtx_unlock_spin(&sched_lock);
		PROC_LOCK(p);
		sig = cursig(td);
		if (sig == 0 && thread_suspend_check(1))
			sig = SIGSTOP;
		mtx_lock_spin(&sched_lock);
		PROC_UNLOCK(p);
		if (sig != 0) {
			if (TD_ON_SLEEPQ(td))
				unsleep(td);
		} else if (!TD_ON_SLEEPQ(td))
			catch = 0;
	} else
		sig = 0;

	/*
	 * Let the scheduler know we're about to voluntarily go to sleep.
	 */
	sched_sleep(td, priority & PRIMASK);

	if (TD_ON_SLEEPQ(td)) {
		p->p_stats->p_ru.ru_nvcsw++;
		TD_SET_SLEEPING(td);
		mi_switch();
	}
	/*
	 * We're awake from voluntary sleep.
	 */
	CTR3(KTR_PROC, "msleep resume: thread %p (pid %d, %s)", td, p->p_pid,
	    p->p_comm);
	KASSERT(TD_IS_RUNNING(td), ("running but not TDS_RUNNING"));
	td->td_flags &= ~TDF_SINTR;
	if (td->td_flags & TDF_TIMEOUT) {
		td->td_flags &= ~TDF_TIMEOUT;
		if (sig == 0)
			rval = EWOULDBLOCK;
	} else if (td->td_flags & TDF_TIMOFAIL) {
		td->td_flags &= ~TDF_TIMOFAIL;
	} else if (timo && callout_stop(&td->td_slpcallout) == 0) {
		/*
		 * This isn't supposed to be pretty.  If we are here, then
		 * the endtsleep() callout is currently executing on another
		 * CPU and is either spinning on the sched_lock or will be
		 * soon.  If we don't synchronize here, there is a chance
		 * that this process may msleep() again before the callout
		 * has a chance to run and the callout may end up waking up
		 * the wrong msleep().  Yuck.
		 */
		TD_SET_SLEEPING(td);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		td->td_flags &= ~TDF_TIMOFAIL;
	} 
	if ((td->td_flags & TDF_INTERRUPT) && (priority & PCATCH) &&
	    (rval == 0)) {
		td->td_flags &= ~TDF_INTERRUPT;
		rval = EINTR;
	}
	mtx_unlock_spin(&sched_lock);

	if (rval == 0 && catch) {
		PROC_LOCK(p);
		/* XXX: shouldn't we always be calling cursig() */
		if (sig != 0 || (sig = cursig(td))) {
			if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
				rval = EINTR;
			else
				rval = ERESTART;
		}
		PROC_UNLOCK(p);
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0);
#endif
	PICKUP_GIANT();
	if (mtx != NULL) {
		mtx_lock(mtx);
		WITNESS_RESTORE(&mtx->mtx_object, mtx);
	}
	return (rval);
}

/*
 * Implement timeout for msleep()
 *
 * If process hasn't been awakened (wchan non-zero),
 * set timeout flag and undo the sleep.  If proc
 * is stopped, just unsleep so it will remain stopped.
 * MP-safe, called without the Giant mutex.
 */
static void
endtsleep(arg)
	void *arg;
{
	register struct thread *td = arg;

	CTR3(KTR_PROC, "endtsleep: thread %p (pid %d, %s)",
	    td, td->td_proc->p_pid, td->td_proc->p_comm);
	mtx_lock_spin(&sched_lock);
	/*
	 * This is the other half of the synchronization with msleep()
	 * described above.  If the TDS_TIMEOUT flag is set, we lost the
	 * race and just need to put the process back on the runqueue.
	 */
	if (TD_ON_SLEEPQ(td)) {
		TAILQ_REMOVE(&slpque[LOOKUP(td->td_wchan)], td, td_slpq);
		TD_CLR_ON_SLEEPQ(td);
		td->td_flags |= TDF_TIMEOUT;
	} else {
		td->td_flags |= TDF_TIMOFAIL;
	}
	TD_CLR_SLEEPING(td);
	setrunnable(td);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Abort a thread, as if an interrupt had occured.  Only abort
 * interruptable waits (unfortunatly it isn't only safe to abort others).
 * This is about identical to cv_abort().
 * Think about merging them?
 * Also, whatever the signal code does...
 */
void
abortsleep(struct thread *td)
{

	mtx_assert(&sched_lock, MA_OWNED);
	/*
	 * If the TDF_TIMEOUT flag is set, just leave. A
	 * timeout is scheduled anyhow.
	 */
	if ((td->td_flags & (TDF_TIMEOUT | TDF_SINTR)) == TDF_SINTR) {
		if (TD_ON_SLEEPQ(td)) {
			unsleep(td);
			TD_CLR_SLEEPING(td);
			setrunnable(td);
		}
	}
}

/*
 * Remove a process from its wait queue
 */
void
unsleep(struct thread *td)
{

	mtx_lock_spin(&sched_lock);
	if (TD_ON_SLEEPQ(td)) {
		TAILQ_REMOVE(&slpque[LOOKUP(td->td_wchan)], td, td_slpq);
		TD_CLR_ON_SLEEPQ(td);
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * Make all processes sleeping on the specified identifier runnable.
 */
void
wakeup(ident)
	register void *ident;
{
	register struct slpquehead *qp;
	register struct thread *td;
	struct thread *ntd;
	struct proc *p;

	mtx_lock_spin(&sched_lock);
	qp = &slpque[LOOKUP(ident)];
restart:
	for (td = TAILQ_FIRST(qp); td != NULL; td = ntd) {
		ntd = TAILQ_NEXT(td, td_slpq);
		if (td->td_wchan == ident) {
			unsleep(td);
			TD_CLR_SLEEPING(td);
			setrunnable(td);
			p = td->td_proc;
			CTR3(KTR_PROC,"wakeup: thread %p (pid %d, %s)",
			    td, p->p_pid, p->p_comm);
			goto restart;
		}
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * Make a process sleeping on the specified identifier runnable.
 * May wake more than one process if a target process is currently
 * swapped out.
 */
void
wakeup_one(ident)
	register void *ident;
{
	register struct slpquehead *qp;
	register struct thread *td;
	register struct proc *p;
	struct thread *ntd;

	mtx_lock_spin(&sched_lock);
	qp = &slpque[LOOKUP(ident)];
	for (td = TAILQ_FIRST(qp); td != NULL; td = ntd) {
		ntd = TAILQ_NEXT(td, td_slpq);
		if (td->td_wchan == ident) {
			unsleep(td);
			TD_CLR_SLEEPING(td);
			setrunnable(td);
			p = td->td_proc;
			CTR3(KTR_PROC,"wakeup1: thread %p (pid %d, %s)",
			    td, p->p_pid, p->p_comm);
			break;
		}
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * The machine independent parts of mi_switch().
 */
void
mi_switch(void)
{
	struct bintime new_switchtime;
	struct thread *td = curthread;	/* XXX */
	struct proc *p = td->td_proc;	/* XXX */
	struct kse *ke = td->td_kse;
	u_int sched_nest;

	mtx_assert(&sched_lock, MA_OWNED | MA_NOTRECURSED);

	KASSERT(!TD_ON_RUNQ(td), ("mi_switch: called by old code"));
#ifdef INVARIANTS
	if (!TD_ON_LOCK(td) &&
	    !TD_ON_RUNQ(td) &&
	    !TD_IS_RUNNING(td))
		mtx_assert(&Giant, MA_NOTOWNED);
#endif
	KASSERT(td->td_critnest == 1,
	    ("mi_switch: switch in a critical section"));

	/*
	 * Compute the amount of time during which the current
	 * process was running, and add that to its total so far.
	 */
	binuptime(&new_switchtime);
	bintime_add(&p->p_runtime, &new_switchtime);
	bintime_sub(&p->p_runtime, PCPU_PTR(switchtime));

#ifdef DDB
	/*
	 * Don't perform context switches from the debugger.
	 */
	if (db_active) {
		mtx_unlock_spin(&sched_lock);
		db_print_backtrace();
		db_error("Context switches not allowed in the debugger.");
	}
#endif

	/*
	 * Check if the process exceeds its cpu resource allocation.  If
	 * over max, arrange to kill the process in ast().
	 */
	if (p->p_cpulimit != RLIM_INFINITY &&
	    p->p_runtime.sec > p->p_cpulimit) {
		p->p_sflag |= PS_XCPU;
		ke->ke_flags |= KEF_ASTPENDING;
	}

	/*
	 * Finish up stats for outgoing thread.
	 */
	cnt.v_swtch++;
	PCPU_SET(switchtime, new_switchtime);
	CTR3(KTR_PROC, "mi_switch: old thread %p (pid %d, %s)", td, p->p_pid,
	    p->p_comm);

	sched_nest = sched_lock.mtx_recurse;
	sched_switchout(td);

	cpu_switch();		/* SHAZAM!!*/

	sched_lock.mtx_recurse = sched_nest;
	sched_lock.mtx_lock = (uintptr_t)td;
	sched_switchin(td);

	/* 
	 * Start setting up stats etc. for the incoming thread.
	 * Similar code in fork_exit() is returned to by cpu_switch()
	 * in the case of a new thread/process.
	 */
	CTR3(KTR_PROC, "mi_switch: new thread %p (pid %d, %s)", td, p->p_pid,
	    p->p_comm);
	if (PCPU_GET(switchtime.sec) == 0)
		binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	/*
	 * Call the switchin function while still holding the scheduler lock
	 * (used by the idlezero code and the general page-zeroing code)
	 */
	if (td->td_switchin)
		td->td_switchin();

	/* 
	 * If the last thread was exiting, finish cleaning it up.
	 */
	if ((td = PCPU_GET(deadthread))) {
		PCPU_SET(deadthread, NULL);
		thread_stash(td);
	}
}

/*
 * Change process state to be runnable,
 * placing it on the run queue if it is in memory,
 * and awakening the swapper if it isn't in memory.
 */
void
setrunnable(struct thread *td)
{
	struct proc *p = td->td_proc;

	mtx_assert(&sched_lock, MA_OWNED);
	switch (p->p_state) {
	case PRS_ZOMBIE:
		panic("setrunnable(1)");
	default:
		break;
	}
	switch (td->td_state) {
	case TDS_RUNNING:
	case TDS_RUNQ:
		return;
	case TDS_INHIBITED:
		/*
		 * If we are only inhibited because we are swapped out
		 * then arange to swap in this process. Otherwise just return.
		 */
		if (td->td_inhibitors != TDI_SWAPPED)
			return;
	case TDS_CAN_RUN:
		break;
	default:
		printf("state is 0x%x", td->td_state);
		panic("setrunnable(2)");
	}
	if ((p->p_sflag & PS_INMEM) == 0) {
		if ((p->p_sflag & PS_SWAPPINGIN) == 0) {
			p->p_sflag |= PS_SWAPINREQ;
			wakeup(&proc0);
		}
	} else
		sched_wakeup(td);
}

/*
 * Compute a tenex style load average of a quantity on
 * 1, 5 and 15 minute intervals.
 * XXXKSE   Needs complete rewrite when correct info is available.
 * Completely Bogus.. only works with 1:1 (but compiles ok now :-)
 */
static void
loadav(void *arg)
{
	int i, nrun;
	struct loadavg *avg;
	struct proc *p;
	struct thread *td;

	avg = &averunnable;
	sx_slock(&allproc_lock);
	nrun = 0;
	FOREACH_PROC_IN_SYSTEM(p) {
		FOREACH_THREAD_IN_PROC(p, td) {
			switch (td->td_state) {
			case TDS_RUNQ:
			case TDS_RUNNING:
				if ((p->p_flag & P_NOLOAD) != 0)
					goto nextproc;
				nrun++; /* XXXKSE */
			default:
				break;
			}
nextproc:
			continue;
		}
	}
	sx_sunlock(&allproc_lock);
	for (i = 0; i < 3; i++)
		avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
		    nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;

	/*
	 * Schedule the next update to occur after 5 seconds, but add a
	 * random variation to avoid synchronisation with processes that
	 * run at regular intervals.
	 */
	callout_reset(&loadav_callout, hz * 4 + (int)(random() % (hz * 2 + 1)),
	    loadav, NULL);
}

static void
lboltcb(void *arg)
{
	wakeup(&lbolt);
	callout_reset(&lbolt_callout, hz, lboltcb, NULL);
}

/* ARGSUSED */
static void
sched_setup(dummy)
	void *dummy;
{
	callout_init(&loadav_callout, 0);
	callout_init(&lbolt_callout, 1);

	/* Kick off timeout driven events by calling first time. */
	loadav(NULL);
	lboltcb(NULL);
}

/*
 * General purpose yield system call
 */
int
yield(struct thread *td, struct yield_args *uap)
{
	struct ksegrp *kg = td->td_ksegrp;

	mtx_assert(&Giant, MA_NOTOWNED);
	mtx_lock_spin(&sched_lock);
	kg->kg_proc->p_stats->p_ru.ru_nvcsw++;
	sched_prio(td, PRI_MAX_TIMESHARE);
	mi_switch();
	mtx_unlock_spin(&sched_lock);
	td->td_retval[0] = 0;

	return (0);
}

