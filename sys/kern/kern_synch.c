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
#include <sys/signalvar.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>

static void sched_setup __P((void *dummy));
SYSINIT(sched_setup, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, sched_setup, NULL)

int	hogticks;
int	lbolt;
int	sched_quantum;		/* Roundrobin scheduling quantum in ticks. */

static struct callout schedcpu_callout;
static struct callout roundrobin_callout;

static void	endtsleep __P((void *));
static void	roundrobin __P((void *arg));
static void	schedcpu __P((void *arg));

static int
sysctl_kern_quantum(SYSCTL_HANDLER_ARGS)
{
	int error, new_val;

	new_val = sched_quantum * tick;
	error = sysctl_handle_int(oidp, &new_val, 0, req);
        if (error != 0 || req->newptr == NULL)
		return (error);
	if (new_val < tick)
		return (EINVAL);
	sched_quantum = new_val / tick;
	hogticks = 2 * sched_quantum;
	return (0);
}

SYSCTL_PROC(_kern, OID_AUTO, quantum, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof sched_quantum, sysctl_kern_quantum, "I", "");

/*
 * Arrange to reschedule if necessary, taking the priorities and
 * schedulers into account.
 */
void
maybe_resched(p)
	struct proc *p;
{

	mtx_assert(&sched_lock, MA_OWNED);
	if (p->p_pri.pri_level < curproc->p_pri.pri_level)
		need_resched(curproc);
}

int 
roundrobin_interval(void)
{
	return (sched_quantum);
}

/*
 * Force switch among equal priority processes every 100ms.
 */
/* ARGSUSED */
static void
roundrobin(arg)
	void *arg;
{

	mtx_lock_spin(&sched_lock);
	need_resched(curproc);
#ifdef SMP
	forward_roundrobin();
#endif
	mtx_unlock_spin(&sched_lock);

	callout_reset(&roundrobin_callout, sched_quantum, roundrobin, NULL);
}

/*
 * Constants for digital decay and forget:
 *	90% of (p_estcpu) usage in 5 * loadav time
 *	95% of (p_pctcpu) usage in 60 seconds (load insensitive)
 *          Note that, as ps(1) mentions, this can let percentages
 *          total over 100% (I've seen 137.9% for 3 processes).
 *
 * Note that schedclock() updates p_estcpu and p_cpticks asynchronously.
 *
 * We wish to decay away 90% of p_estcpu in (5 * loadavg) seconds.
 * That is, the system wants to compute a value of decay such
 * that the following for loop:
 * 	for (i = 0; i < (5 * loadavg); i++)
 * 		p_estcpu *= decay;
 * will compute
 * 	p_estcpu *= 0.1;
 * for all values of loadavg:
 *
 * Mathematically this loop can be expressed by saying:
 * 	decay ** (5 * loadavg) ~= .1
 *
 * The system computes decay as:
 * 	decay = (2 * loadavg) / (2 * loadavg + 1)
 *
 * We wish to prove that the system's computation of decay
 * will always fulfill the equation:
 * 	decay ** (5 * loadavg) ~= .1
 *
 * If we compute b as:
 * 	b = 2 * loadavg
 * then
 * 	decay = b / (b + 1)
 *
 * We now need to prove two things:
 *	1) Given factor ** (5 * loadavg) ~= .1, prove factor == b/(b+1)
 *	2) Given b/(b+1) ** power ~= .1, prove power == (5 * loadavg)
 *
 * Facts:
 *         For x close to zero, exp(x) =~ 1 + x, since
 *              exp(x) = 0! + x**1/1! + x**2/2! + ... .
 *              therefore exp(-1/b) =~ 1 - (1/b) = (b-1)/b.
 *         For x close to zero, ln(1+x) =~ x, since
 *              ln(1+x) = x - x**2/2 + x**3/3 - ...     -1 < x < 1
 *              therefore ln(b/(b+1)) = ln(1 - 1/(b+1)) =~ -1/(b+1).
 *         ln(.1) =~ -2.30
 *
 * Proof of (1):
 *    Solve (factor)**(power) =~ .1 given power (5*loadav):
 *	solving for factor,
 *      ln(factor) =~ (-2.30/5*loadav), or
 *      factor =~ exp(-1/((5/2.30)*loadav)) =~ exp(-1/(2*loadav)) =
 *          exp(-1/b) =~ (b-1)/b =~ b/(b+1).                    QED
 *
 * Proof of (2):
 *    Solve (factor)**(power) =~ .1 given factor == (b/(b+1)):
 *	solving for power,
 *      power*ln(b/(b+1)) =~ -2.30, or
 *      power =~ 2.3 * (b + 1) = 4.6*loadav + 2.3 =~ 5*loadav.  QED
 *
 * Actual power values for the implemented algorithm are as follows:
 *      loadav: 1       2       3       4
 *      power:  5.68    10.32   14.94   19.55
 */

/* calculations for digital decay to forget 90% of usage in 5*loadav sec */
#define	loadfactor(loadav)	(2 * (loadav))
#define	decay_cpu(loadfac, cpu)	(((loadfac) * (cpu)) / ((loadfac) + FSCALE))

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
static fixpt_t	ccpu = 0.95122942450071400909 * FSCALE;	/* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

/* kernel uses `FSCALE', userland (SHOULD) use kern.fscale */
static int	fscale __unused = FSCALE;
SYSCTL_INT(_kern, OID_AUTO, fscale, CTLFLAG_RD, 0, FSCALE, "");

/*
 * If `ccpu' is not equal to `exp(-1/20)' and you still want to use the
 * faster/more-accurate formula, you'll have to estimate CCPU_SHIFT below
 * and possibly adjust FSHIFT in "param.h" so that (FSHIFT >= CCPU_SHIFT).
 *
 * To estimate CCPU_SHIFT for exp(-1/20), the following formula was used:
 *	1 - exp(-1/20) ~= 0.0487 ~= 0.0488 == 1 (fixed pt, *11* bits).
 *
 * If you don't want to bother with the faster/more-accurate formula, you
 * can set CCPU_SHIFT to (FSHIFT + 1) which will use a slower/less-accurate
 * (more general) method of calculating the %age of CPU used by a process.
 */
#define	CCPU_SHIFT	11

/*
 * Recompute process priorities, every hz ticks.
 * MP-safe, called without the Giant mutex.
 */
/* ARGSUSED */
static void
schedcpu(arg)
	void *arg;
{
	register fixpt_t loadfac = loadfactor(averunnable.ldavg[0]);
	register struct proc *p;
	register int realstathz, s;

	realstathz = stathz ? stathz : hz;
	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		/*
		 * Increment time in/out of memory and sleep time
		 * (if sleeping).  We ignore overflow; with 16-bit int's
		 * (remember them?) overflow takes 45 days.
		if (p->p_stat == SWAIT)
			continue;
		 */
		mtx_lock_spin(&sched_lock);
		p->p_swtime++;
		if (p->p_stat == SSLEEP || p->p_stat == SSTOP)
			p->p_slptime++;
		p->p_pctcpu = (p->p_pctcpu * ccpu) >> FSHIFT;
		/*
		 * If the process has slept the entire second,
		 * stop recalculating its priority until it wakes up.
		 */
		if (p->p_slptime > 1) {
			mtx_unlock_spin(&sched_lock);
			continue;
		}

		/*
		 * prevent state changes and protect run queue
		 */
		s = splhigh();

		/*
		 * p_pctcpu is only for ps.
		 */
#if	(FSHIFT >= CCPU_SHIFT)
		p->p_pctcpu += (realstathz == 100)?
			((fixpt_t) p->p_cpticks) << (FSHIFT - CCPU_SHIFT):
                	100 * (((fixpt_t) p->p_cpticks)
				<< (FSHIFT - CCPU_SHIFT)) / realstathz;
#else
		p->p_pctcpu += ((FSCALE - ccpu) *
			(p->p_cpticks * FSCALE / realstathz)) >> FSHIFT;
#endif
		p->p_cpticks = 0;
		p->p_estcpu = decay_cpu(loadfac, p->p_estcpu);
		resetpriority(p);
		if (p->p_pri.pri_level >= PUSER) {
			if ((p != curproc) &&
#ifdef SMP
			    p->p_oncpu == NOCPU && 	/* idle */
#endif
			    p->p_stat == SRUN &&
			    (p->p_sflag & PS_INMEM) &&
			    (p->p_pri.pri_level / RQ_PPQ) !=
			    (p->p_pri.pri_user / RQ_PPQ)) {
				remrunqueue(p);
				p->p_pri.pri_level = p->p_pri.pri_user;
				setrunqueue(p);
			} else
				p->p_pri.pri_level = p->p_pri.pri_user;
		}
		mtx_unlock_spin(&sched_lock);
		splx(s);
	}
	sx_sunlock(&allproc_lock);
	vmmeter();
	wakeup((caddr_t)&lbolt);
	callout_reset(&schedcpu_callout, hz, schedcpu, NULL);
}

/*
 * Recalculate the priority of a process after it has slept for a while.
 * For all load averages >= 1 and max p_estcpu of 255, sleeping for at
 * least six times the loadfactor will decay p_estcpu to zero.
 */
void
updatepri(p)
	register struct proc *p;
{
	register unsigned int newcpu = p->p_estcpu;
	register fixpt_t loadfac = loadfactor(averunnable.ldavg[0]);

	if (p->p_slptime > 5 * loadfac)
		p->p_estcpu = 0;
	else {
		p->p_slptime--;	/* the first time was done in schedcpu */
		while (newcpu && --p->p_slptime)
			newcpu = decay_cpu(loadfac, newcpu);
		p->p_estcpu = newcpu;
	}
	resetpriority(p);
}

/*
 * We're only looking at 7 bits of the address; everything is
 * aligned to 4, lots of things are aligned to greater powers
 * of 2.  Shift right by 8, i.e. drop the bottom 256 worth.
 */
#define TABLESIZE	128
static TAILQ_HEAD(slpquehead, proc) slpque[TABLESIZE];
#define LOOKUP(x)	(((intptr_t)(x) >> 8) & (TABLESIZE - 1))

void
sleepinit(void)
{
	int i;

	sched_quantum = hz/10;
	hogticks = 2 * sched_quantum;
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
	struct proc *p = curproc;
	int sig, catch = priority & PCATCH;
	int rval = 0;
	WITNESS_SAVE_DECL(mtx);

	KASSERT(ident == &proc0 ||	/* XXX: swapper */
	    timo != 0 ||		/* XXX: we might still miss a wakeup */
	    mtx_owned(&Giant) || mtx != NULL,
	    ("indefinite sleep without mutex, wmesg: \"%s\" ident: %p",
	     wmesg, ident));
	if (mtx_owned(&vm_mtx) && mtx != &vm_mtx)
		panic("sleeping with vm_mtx held.");
#ifdef KTRACE
	if (p && KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 1, 0);
#endif
	WITNESS_SLEEP(0, &mtx->mtx_object);
	mtx_lock_spin(&sched_lock);
	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration,
		 * just give interrupts a chance, then just return;
		 * don't run any other procs or panic below,
		 * in case this is the idle process and already asleep.
		 */
		if (mtx != NULL && priority & PDROP)
			mtx_unlock_flags(mtx, MTX_NOSWITCH);
		mtx_unlock_spin(&sched_lock);
		return (0);
	}

	DROP_GIANT_NOSWITCH();

	if (mtx != NULL) {
		mtx_assert(mtx, MA_OWNED | MA_NOTRECURSED);
		WITNESS_SAVE(&mtx->mtx_object, mtx);
		mtx_unlock_flags(mtx, MTX_NOSWITCH);
		if (priority & PDROP)
			mtx = NULL;
	}

	KASSERT(p != NULL, ("msleep1"));
	KASSERT(ident != NULL && p->p_stat == SRUN, ("msleep"));
	/*
	 * Process may be sitting on a slpque if asleep() was called, remove
	 * it before re-adding.
	 */
	if (p->p_wchan != NULL)
		unsleep(p);

	p->p_wchan = ident;
	p->p_wmesg = wmesg;
	p->p_slptime = 0;
	p->p_pri.pri_level = priority & PRIMASK;
	CTR4(KTR_PROC, "msleep: proc %p (pid %d, %s), schedlock %p",
		p, p->p_pid, p->p_comm, (void *) sched_lock.mtx_lock);
	TAILQ_INSERT_TAIL(&slpque[LOOKUP(ident)], p, p_slpq);
	if (timo)
		callout_reset(&p->p_slpcallout, timo, endtsleep, p);
	/*
	 * We put ourselves on the sleep queue and start our timeout
	 * before calling CURSIG, as we could stop there, and a wakeup
	 * or a SIGCONT (or both) could occur while we were stopped.
	 * A SIGCONT would cause us to be marked as SSLEEP
	 * without resuming us, thus we must be ready for sleep
	 * when CURSIG is called.  If the wakeup happens while we're
	 * stopped, p->p_wchan will be 0 upon return from CURSIG.
	 */
	if (catch) {
		CTR4(KTR_PROC,
		        "msleep caught: proc %p (pid %d, %s), schedlock %p",
			p, p->p_pid, p->p_comm, (void *) sched_lock.mtx_lock);
		p->p_sflag |= PS_SINTR;
		mtx_unlock_spin(&sched_lock);
		if ((sig = CURSIG(p))) {
			mtx_lock_spin(&sched_lock);
			if (p->p_wchan)
				unsleep(p);
			p->p_stat = SRUN;
			goto resume;
		}
		mtx_lock_spin(&sched_lock);
		if (p->p_wchan == NULL) {
			catch = 0;
			goto resume;
		}
	} else
		sig = 0;
	p->p_stat = SSLEEP;
	p->p_stats->p_ru.ru_nvcsw++;
	mi_switch();
	CTR4(KTR_PROC,
	        "msleep resume: proc %p (pid %d, %s), schedlock %p",
		p, p->p_pid, p->p_comm, (void *) sched_lock.mtx_lock);
resume:
	p->p_sflag &= ~PS_SINTR;
	if (p->p_sflag & PS_TIMEOUT) {
		p->p_sflag &= ~PS_TIMEOUT;
		if (sig == 0) {
#ifdef KTRACE
			if (KTRPOINT(p, KTR_CSW))
				ktrcsw(p->p_tracep, 0, 0);
#endif
			rval = EWOULDBLOCK;
			mtx_unlock_spin(&sched_lock);
			goto out;
		}
	} else if (timo)
		callout_stop(&p->p_slpcallout);
	mtx_unlock_spin(&sched_lock);

	if (catch && (sig != 0 || (sig = CURSIG(p)))) {
#ifdef KTRACE
		if (KTRPOINT(p, KTR_CSW))
			ktrcsw(p->p_tracep, 0, 0);
#endif
		PROC_LOCK(p);
		if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
			rval = EINTR;
		else
			rval = ERESTART;
		PROC_UNLOCK(p);
		goto out;
	}
out:
#ifdef KTRACE
	if (KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 0, 0);
#endif
	PICKUP_GIANT();
	if (mtx != NULL) {
		mtx_lock(mtx);
		WITNESS_RESTORE(&mtx->mtx_object, mtx);
	}
	return (rval);
}

/*
 * asleep() - async sleep call.  Place process on wait queue and return 
 * immediately without blocking.  The process stays runnable until mawait() 
 * is called.  If ident is NULL, remove process from wait queue if it is still
 * on one.
 *
 * Only the most recent sleep condition is effective when making successive
 * calls to asleep() or when calling msleep().
 *
 * The timeout, if any, is not initiated until mawait() is called.  The sleep
 * priority, signal, and timeout is specified in the asleep() call but may be
 * overriden in the mawait() call.
 *
 * <<<<<<<< EXPERIMENTAL, UNTESTED >>>>>>>>>>
 */

int
asleep(void *ident, int priority, const char *wmesg, int timo)
{
	struct proc *p = curproc;
	int s;

	/*
	 * obtain sched_lock while manipulating sleep structures and slpque.
	 *
	 * Remove preexisting wait condition (if any) and place process
	 * on appropriate slpque, but do not put process to sleep.
	 */

	s = splhigh();
	mtx_lock_spin(&sched_lock);

	if (p->p_wchan != NULL)
		unsleep(p);

	if (ident) {
		p->p_wchan = ident;
		p->p_wmesg = wmesg;
		p->p_slptime = 0;
		p->p_asleep.as_priority = priority;
		p->p_asleep.as_timo = timo;
		TAILQ_INSERT_TAIL(&slpque[LOOKUP(ident)], p, p_slpq);
	}

	mtx_unlock_spin(&sched_lock);
	splx(s);

	return(0);
}

/*
 * mawait() - wait for async condition to occur.   The process blocks until
 * wakeup() is called on the most recent asleep() address.  If wakeup is called
 * prior to mawait(), mawait() winds up being a NOP.
 *
 * If mawait() is called more then once (without an intervening asleep() call),
 * mawait() is still effectively a NOP but it calls mi_switch() to give other
 * processes some cpu before returning.  The process is left runnable.
 *
 * <<<<<<<< EXPERIMENTAL, UNTESTED >>>>>>>>>>
 */

int
mawait(struct mtx *mtx, int priority, int timo)
{
	struct proc *p = curproc;
	int rval = 0;
	int s;
	WITNESS_SAVE_DECL(mtx);

	WITNESS_SLEEP(0, &mtx->mtx_object);
	mtx_lock_spin(&sched_lock);
	DROP_GIANT_NOSWITCH();
	if (mtx != NULL) {
		mtx_assert(mtx, MA_OWNED | MA_NOTRECURSED);
		WITNESS_SAVE(&mtx->mtx_object, mtx);
		mtx_unlock_flags(mtx, MTX_NOSWITCH);
		if (priority & PDROP)
			mtx = NULL;
	}

	s = splhigh();

	if (p->p_wchan != NULL) {
		int sig;
		int catch;

		/*
		 * The call to mawait() can override defaults specified in
		 * the original asleep().
		 */
		if (priority < 0)
			priority = p->p_asleep.as_priority;
		if (timo < 0)
			timo = p->p_asleep.as_timo;

		/*
		 * Install timeout
		 */

		if (timo)
			callout_reset(&p->p_slpcallout, timo, endtsleep, p);

		sig = 0;
		catch = priority & PCATCH;

		if (catch) {
			p->p_sflag |= PS_SINTR;
			mtx_unlock_spin(&sched_lock);
			if ((sig = CURSIG(p))) {
				mtx_lock_spin(&sched_lock);
				if (p->p_wchan)
					unsleep(p);
				p->p_stat = SRUN;
				goto resume;
			}
			mtx_lock_spin(&sched_lock);
			if (p->p_wchan == NULL) {
				catch = 0;
				goto resume;
			}
		}
		p->p_stat = SSLEEP;
		p->p_stats->p_ru.ru_nvcsw++;
		mi_switch();
resume:

		splx(s);
		p->p_sflag &= ~PS_SINTR;
		if (p->p_sflag & PS_TIMEOUT) {
			p->p_sflag &= ~PS_TIMEOUT;
			if (sig == 0) {
#ifdef KTRACE
				if (KTRPOINT(p, KTR_CSW))
					ktrcsw(p->p_tracep, 0, 0);
#endif
				rval = EWOULDBLOCK;
				mtx_unlock_spin(&sched_lock);
				goto out;
			}
		} else if (timo)
			callout_stop(&p->p_slpcallout);
		mtx_unlock_spin(&sched_lock);

		if (catch && (sig != 0 || (sig = CURSIG(p)))) {
#ifdef KTRACE
			if (KTRPOINT(p, KTR_CSW))
				ktrcsw(p->p_tracep, 0, 0);
#endif
			PROC_LOCK(p);
			if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
				rval = EINTR;
			else
				rval = ERESTART;
			PROC_UNLOCK(p);
			goto out;
		}
#ifdef KTRACE
		if (KTRPOINT(p, KTR_CSW))
			ktrcsw(p->p_tracep, 0, 0);
#endif
	} else {
		/*
		 * If as_priority is 0, mawait() has been called without an 
		 * intervening asleep().  We are still effectively a NOP, 
		 * but we call mi_switch() for safety.
		 */

		if (p->p_asleep.as_priority == 0) {
			p->p_stats->p_ru.ru_nvcsw++;
			mi_switch();
		}
		mtx_unlock_spin(&sched_lock);
		splx(s);
	}

	/*
	 * clear p_asleep.as_priority as an indication that mawait() has been
	 * called.  If mawait() is called again without an intervening asleep(),
	 * mawait() is still effectively a NOP but the above mi_switch() code
	 * is triggered as a safety.
	 */
	p->p_asleep.as_priority = 0;

out:
	PICKUP_GIANT();
	if (mtx != NULL) {
		mtx_lock(mtx);
		WITNESS_RESTORE(&mtx->mtx_object, mtx);
	}
	return (rval);
}

/*
 * Implement timeout for msleep or asleep()/mawait()
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
	register struct proc *p;
	int s;

	p = (struct proc *)arg;
	CTR4(KTR_PROC,
	        "endtsleep: proc %p (pid %d, %s), schedlock %p",
		p, p->p_pid, p->p_comm, (void *) sched_lock.mtx_lock);
	s = splhigh();
	mtx_lock_spin(&sched_lock);
	if (p->p_wchan) {
		if (p->p_stat == SSLEEP)
			setrunnable(p);
		else
			unsleep(p);
		p->p_sflag |= PS_TIMEOUT;
	}
	mtx_unlock_spin(&sched_lock);
	splx(s);
}

/*
 * Remove a process from its wait queue
 */
void
unsleep(p)
	register struct proc *p;
{
	int s;

	s = splhigh();
	mtx_lock_spin(&sched_lock);
	if (p->p_wchan) {
		TAILQ_REMOVE(&slpque[LOOKUP(p->p_wchan)], p, p_slpq);
		p->p_wchan = NULL;
	}
	mtx_unlock_spin(&sched_lock);
	splx(s);
}

/*
 * Make all processes sleeping on the specified identifier runnable.
 */
void
wakeup(ident)
	register void *ident;
{
	register struct slpquehead *qp;
	register struct proc *p;
	int s;

	s = splhigh();
	mtx_lock_spin(&sched_lock);
	qp = &slpque[LOOKUP(ident)];
restart:
	TAILQ_FOREACH(p, qp, p_slpq) {
		if (p->p_wchan == ident) {
			TAILQ_REMOVE(qp, p, p_slpq);
			p->p_wchan = NULL;
			if (p->p_stat == SSLEEP) {
				/* OPTIMIZED EXPANSION OF setrunnable(p); */
				CTR4(KTR_PROC,
				        "wakeup: proc %p (pid %d, %s), schedlock %p",
					p, p->p_pid, p->p_comm, (void *) sched_lock.mtx_lock);
				if (p->p_slptime > 1)
					updatepri(p);
				p->p_slptime = 0;
				p->p_stat = SRUN;
				if (p->p_sflag & PS_INMEM) {
					setrunqueue(p);
					maybe_resched(p);
				} else {
					p->p_sflag |= PS_SWAPINREQ;
					wakeup((caddr_t)&proc0);
				}
				/* END INLINE EXPANSION */
				goto restart;
			}
		}
	}
	mtx_unlock_spin(&sched_lock);
	splx(s);
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
	register struct proc *p;
	int s;

	s = splhigh();
	mtx_lock_spin(&sched_lock);
	qp = &slpque[LOOKUP(ident)];

	TAILQ_FOREACH(p, qp, p_slpq) {
		if (p->p_wchan == ident) {
			TAILQ_REMOVE(qp, p, p_slpq);
			p->p_wchan = NULL;
			if (p->p_stat == SSLEEP) {
				/* OPTIMIZED EXPANSION OF setrunnable(p); */
				CTR4(KTR_PROC,
				        "wakeup1: proc %p (pid %d, %s), schedlock %p",
					p, p->p_pid, p->p_comm, (void *) sched_lock.mtx_lock);
				if (p->p_slptime > 1)
					updatepri(p);
				p->p_slptime = 0;
				p->p_stat = SRUN;
				if (p->p_sflag & PS_INMEM) {
					setrunqueue(p);
					maybe_resched(p);
					break;
				} else {
					p->p_sflag |= PS_SWAPINREQ;
					wakeup((caddr_t)&proc0);
				}
				/* END INLINE EXPANSION */
			}
		}
	}
	mtx_unlock_spin(&sched_lock);
	splx(s);
}

/*
 * The machine independent parts of mi_switch().
 * Must be called at splstatclock() or higher.
 */
void
mi_switch()
{
	struct timeval new_switchtime;
	register struct proc *p = curproc;	/* XXX */
#if 0
	register struct rlimit *rlim;
#endif
	u_int sched_nest;

	mtx_assert(&sched_lock, MA_OWNED | MA_NOTRECURSED);

	/*
	 * Compute the amount of time during which the current
	 * process was running, and add that to its total so far.
	 */
	microuptime(&new_switchtime);
	if (timevalcmp(&new_switchtime, PCPU_PTR(switchtime), <)) {
#if 0
		/* XXX: This doesn't play well with sched_lock right now. */
		printf("microuptime() went backwards (%ld.%06ld -> %ld.%06ld)\n",
		    PCPU_GET(switchtime.tv_sec), PCPU_GET(switchtime.tv_usec),
		    new_switchtime.tv_sec, new_switchtime.tv_usec);
#endif
		new_switchtime = PCPU_GET(switchtime);
	} else {
		p->p_runtime += (new_switchtime.tv_usec - PCPU_GET(switchtime.tv_usec)) +
		    (new_switchtime.tv_sec - PCPU_GET(switchtime.tv_sec)) *
		    (int64_t)1000000;
	}

#if 0
	/*
	 * Check if the process exceeds its cpu resource allocation.
	 * If over max, kill it.
	 *
	 * XXX drop sched_lock, pickup Giant
	 */
	if (p->p_stat != SZOMB && p->p_limit->p_cpulimit != RLIM_INFINITY &&
	    p->p_runtime > p->p_limit->p_cpulimit) {
		rlim = &p->p_rlimit[RLIMIT_CPU];
		if (p->p_runtime / (rlim_t)1000000 >= rlim->rlim_max) {
			mtx_unlock_spin(&sched_lock);
			PROC_LOCK(p);
			killproc(p, "exceeded maximum CPU limit");
			mtx_lock_spin(&sched_lock);
			PROC_UNLOCK_NOSWITCH(p);
		} else {
			mtx_unlock_spin(&sched_lock);
			PROC_LOCK(p);
			psignal(p, SIGXCPU);
			mtx_lock_spin(&sched_lock);
			PROC_UNLOCK_NOSWITCH(p);
			if (rlim->rlim_cur < rlim->rlim_max) {
				/* XXX: we should make a private copy */
				rlim->rlim_cur += 5;
			}
		}
	}
#endif

	/*
	 * Pick a new current process and record its start time.
	 */
	cnt.v_swtch++;
	PCPU_SET(switchtime, new_switchtime);
	CTR4(KTR_PROC, "mi_switch: old proc %p (pid %d, %s), schedlock %p",
		p, p->p_pid, p->p_comm, (void *) sched_lock.mtx_lock);
	sched_nest = sched_lock.mtx_recurse;
	curproc->p_lastcpu = curproc->p_oncpu;
	curproc->p_oncpu = NOCPU;
	clear_resched(curproc);
	cpu_switch();
	curproc->p_oncpu = PCPU_GET(cpuid);
	sched_lock.mtx_recurse = sched_nest;
	sched_lock.mtx_lock = (uintptr_t)curproc;
	CTR4(KTR_PROC, "mi_switch: new proc %p (pid %d, %s), schedlock %p",
		p, p->p_pid, p->p_comm, (void *) sched_lock.mtx_lock);
	if (PCPU_GET(switchtime.tv_sec) == 0)
		microuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);
}

/*
 * Change process state to be runnable,
 * placing it on the run queue if it is in memory,
 * and awakening the swapper if it isn't in memory.
 */
void
setrunnable(p)
	register struct proc *p;
{
	register int s;

	s = splhigh();
	mtx_lock_spin(&sched_lock);
	switch (p->p_stat) {
	case 0:
	case SRUN:
	case SZOMB:
	case SWAIT:
	default:
		panic("setrunnable");
	case SSTOP:
	case SSLEEP:			/* e.g. when sending signals */
		if (p->p_sflag & PS_CVWAITQ)
			cv_waitq_remove(p);
		else
			unsleep(p);
		break;

	case SIDL:
		break;
	}
	p->p_stat = SRUN;
	if (p->p_sflag & PS_INMEM)
		setrunqueue(p);
	splx(s);
	if (p->p_slptime > 1)
		updatepri(p);
	p->p_slptime = 0;
	if ((p->p_sflag & PS_INMEM) == 0) {
		p->p_sflag |= PS_SWAPINREQ;
		wakeup((caddr_t)&proc0);
	}
	else
		maybe_resched(p);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Compute the priority of a process when running in user mode.
 * Arrange to reschedule if the resulting priority is better
 * than that of the current process.
 */
void
resetpriority(p)
	register struct proc *p;
{
	register unsigned int newpriority;

	mtx_lock_spin(&sched_lock);
	if (p->p_pri.pri_class == PRI_TIMESHARE) {
		newpriority = PUSER + p->p_estcpu / INVERSE_ESTCPU_WEIGHT +
		    NICE_WEIGHT * (p->p_nice - PRIO_MIN);
		newpriority = min(max(newpriority, PRI_MIN_TIMESHARE),
		    PRI_MAX_TIMESHARE);
		p->p_pri.pri_user = newpriority;
	}
	maybe_resched(p);
	mtx_unlock_spin(&sched_lock);
}

/* ARGSUSED */
static void
sched_setup(dummy)
	void *dummy;
{

	callout_init(&schedcpu_callout, 1);
	callout_init(&roundrobin_callout, 0);

	/* Kick off timeout driven events by calling first time. */
	roundrobin(NULL);
	schedcpu(NULL);
}

/*
 * We adjust the priority of the current process.  The priority of
 * a process gets worse as it accumulates CPU time.  The cpu usage
 * estimator (p_estcpu) is increased here.  resetpriority() will
 * compute a different priority each time p_estcpu increases by
 * INVERSE_ESTCPU_WEIGHT
 * (until MAXPRI is reached).  The cpu usage estimator ramps up
 * quite quickly when the process is running (linearly), and decays
 * away exponentially, at a rate which is proportionally slower when
 * the system is busy.  The basic principle is that the system will
 * 90% forget that the process used a lot of CPU time in 5 * loadav
 * seconds.  This causes the system to favor processes which haven't
 * run much recently, and to round-robin among other processes.
 */
void
schedclock(p)
	struct proc *p;
{

	p->p_cpticks++;
	p->p_estcpu = ESTCPULIM(p->p_estcpu + 1);
	if ((p->p_estcpu % INVERSE_ESTCPU_WEIGHT) == 0) {
		resetpriority(p);
		if (p->p_pri.pri_level >= PUSER)
			p->p_pri.pri_level = p->p_pri.pri_user;
	}
}

/*
 * General purpose yield system call
 */
int
yield(struct proc *p, struct yield_args *uap)
{
	int s;

	p->p_retval[0] = 0;

	s = splhigh();
	mtx_lock_spin(&sched_lock);
	DROP_GIANT_NOSWITCH();
	p->p_pri.pri_level = PRI_MAX_TIMESHARE;
	setrunqueue(p);
	p->p_stats->p_ru.ru_nvcsw++;
	mi_switch();
	mtx_unlock_spin(&sched_lock);
	PICKUP_GIANT();
	splx(s);

	return (0);
}
