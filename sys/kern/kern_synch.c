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
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>
#include <machine/ipl.h>
#include <machine/smp.h>

static void sched_setup __P((void *dummy));
SYSINIT(sched_setup, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, sched_setup, NULL)

u_char	curpriority;
int	hogticks;
int	lbolt;
int	sched_quantum;		/* Roundrobin scheduling quantum in ticks. */

static struct callout loadav_callout;

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

static int	curpriority_cmp __P((struct proc *p));
static void	endtsleep __P((void *));
static void	loadav __P((void *arg));
static void	maybe_resched __P((struct proc *chk));
static void	roundrobin __P((void *arg));
static void	schedcpu __P((void *arg));
static void	updatepri __P((struct proc *p));

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

/*-
 * Compare priorities.  Return:
 *     <0: priority of p < current priority
 *      0: priority of p == current priority
 *     >0: priority of p > current priority
 * The priorities are the normal priorities or the normal realtime priorities
 * if p is on the same scheduler as curproc.  Otherwise the process on the
 * more realtimeish scheduler has lowest priority.  As usual, a higher
 * priority really means a lower priority.
 */
static int
curpriority_cmp(p)
	struct proc *p;
{
	int c_class, p_class;

	c_class = RTP_PRIO_BASE(curproc->p_rtprio.type);
	p_class = RTP_PRIO_BASE(p->p_rtprio.type);
	if (p_class != c_class)
		return (p_class - c_class);
	if (p_class == RTP_PRIO_NORMAL)
		return (((int)p->p_priority - (int)curpriority) / PPQ);
	return ((int)p->p_rtprio.prio - (int)curproc->p_rtprio.prio);
}

/*
 * Arrange to reschedule if necessary, taking the priorities and
 * schedulers into account.
 */
static void
maybe_resched(chk)
	struct proc *chk;
{
	struct proc *p = curproc; /* XXX */

	/*
	 * XXX idle scheduler still broken because proccess stays on idle
	 * scheduler during waits (such as when getting FS locks).  If a
	 * standard process becomes runaway cpu-bound, the system can lockup
	 * due to idle-scheduler processes in wakeup never getting any cpu.
	 */
	if (p == NULL) {
#if 0
		need_resched();
#endif
	} else if (chk == p) {
		/* We may need to yield if our priority has been raised. */
		if (curpriority_cmp(chk) > 0)
			need_resched();
	} else if (curpriority_cmp(chk) < 0)
		need_resched();
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
#ifndef SMP
 	struct proc *p = curproc; /* XXX */
#endif
 
#ifdef SMP
	need_resched();
	forward_roundrobin();
#else 
 	if (p == 0 || RTP_PRIO_NEED_RR(p->p_rtprio.type))
 		need_resched();
#endif

 	timeout(roundrobin, NULL, sched_quantum);
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
	LIST_FOREACH(p, &allproc, p_list) {
		/*
		 * Increment time in/out of memory and sleep time
		 * (if sleeping).  We ignore overflow; with 16-bit int's
		 * (remember them?) overflow takes 45 days.
		 */
		p->p_swtime++;
		if (p->p_stat == SSLEEP || p->p_stat == SSTOP)
			p->p_slptime++;
		p->p_pctcpu = (p->p_pctcpu * ccpu) >> FSHIFT;
		/*
		 * If the process has slept the entire second,
		 * stop recalculating its priority until it wakes up.
		 */
		if (p->p_slptime > 1)
			continue;
		s = splhigh();	/* prevent state changes and protect run queue */
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
		if (p->p_priority >= PUSER) {
			if ((p != curproc) &&
#ifdef SMP
			    p->p_oncpu == 0xff && 	/* idle */
#endif
			    p->p_stat == SRUN &&
			    (p->p_flag & P_INMEM) &&
			    (p->p_priority / PPQ) != (p->p_usrpri / PPQ)) {
				remrunqueue(p);
				p->p_priority = p->p_usrpri;
				setrunqueue(p);
			} else
				p->p_priority = p->p_usrpri;
		}
		splx(s);
	}
	wakeup((caddr_t)&lbolt);
	timeout(schedcpu, (void *)0, hz);
}

/*
 * Recalculate the priority of a process after it has slept for a while.
 * For all load averages >= 1 and max p_estcpu of 255, sleeping for at
 * least six times the loadfactor will decay p_estcpu to zero.
 */
static void
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

/*
 * During autoconfiguration or after a panic, a sleep will simply
 * lower the priority briefly to allow interrupts, then return.
 * The priority to be used (safepri) is machine-dependent, thus this
 * value is initialized and maintained in the machine-dependent layers.
 * This priority will typically be 0, or the lowest priority
 * that is safe for use on the interrupt stack; it can be made
 * higher to block network software interrupts after panics.
 */
int safepri;

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
 */
int
tsleep(ident, priority, wmesg, timo)
	void *ident;
	int priority, timo;
	const char *wmesg;
{
	struct proc *p = curproc;
	int s, sig, catch = priority & PCATCH;
	struct callout_handle thandle;

#ifdef KTRACE
	if (p && KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 1, 0);
#endif
	s = splhigh();
	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration,
		 * just give interrupts a chance, then just return;
		 * don't run any other procs or panic below,
		 * in case this is the idle process and already asleep.
		 */
		splx(safepri);
		splx(s);
		return (0);
	}
	KASSERT(p != NULL, ("tsleep1"));
	KASSERT(ident != NULL && p->p_stat == SRUN, ("tsleep"));
	/*
	 * Process may be sitting on a slpque if asleep() was called, remove
	 * it before re-adding.
	 */
	if (p->p_wchan != NULL)
		unsleep(p);

	p->p_wchan = ident;
	p->p_wmesg = wmesg;
	p->p_slptime = 0;
	p->p_priority = priority & PRIMASK;
	TAILQ_INSERT_TAIL(&slpque[LOOKUP(ident)], p, p_procq);
	if (timo)
		thandle = timeout(endtsleep, (void *)p, timo);
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
		p->p_flag |= P_SINTR;
		if ((sig = CURSIG(p))) {
			if (p->p_wchan)
				unsleep(p);
			p->p_stat = SRUN;
			goto resume;
		}
		if (p->p_wchan == 0) {
			catch = 0;
			goto resume;
		}
	} else
		sig = 0;
	p->p_stat = SSLEEP;
	p->p_stats->p_ru.ru_nvcsw++;
	mi_switch();
resume:
	curpriority = p->p_usrpri;
	splx(s);
	p->p_flag &= ~P_SINTR;
	if (p->p_flag & P_TIMEOUT) {
		p->p_flag &= ~P_TIMEOUT;
		if (sig == 0) {
#ifdef KTRACE
			if (KTRPOINT(p, KTR_CSW))
				ktrcsw(p->p_tracep, 0, 0);
#endif
			return (EWOULDBLOCK);
		}
	} else if (timo)
		untimeout(endtsleep, (void *)p, thandle);
	if (catch && (sig != 0 || (sig = CURSIG(p)))) {
#ifdef KTRACE
		if (KTRPOINT(p, KTR_CSW))
			ktrcsw(p->p_tracep, 0, 0);
#endif
		if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
			return (EINTR);
		return (ERESTART);
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 0, 0);
#endif
	return (0);
}

/*
 * asleep() - async sleep call.  Place process on wait queue and return 
 * immediately without blocking.  The process stays runnable until await() 
 * is called.  If ident is NULL, remove process from wait queue if it is still
 * on one.
 *
 * Only the most recent sleep condition is effective when making successive
 * calls to asleep() or when calling tsleep().
 *
 * The timeout, if any, is not initiated until await() is called.  The sleep
 * priority, signal, and timeout is specified in the asleep() call but may be
 * overriden in the await() call.
 *
 * <<<<<<<< EXPERIMENTAL, UNTESTED >>>>>>>>>>
 */

int
asleep(void *ident, int priority, const char *wmesg, int timo)
{
	struct proc *p = curproc;
	int s;

	/*
	 * splhigh() while manipulating sleep structures and slpque.
	 *
	 * Remove preexisting wait condition (if any) and place process
	 * on appropriate slpque, but do not put process to sleep.
	 */

	s = splhigh();

	if (p->p_wchan != NULL)
		unsleep(p);

	if (ident) {
		p->p_wchan = ident;
		p->p_wmesg = wmesg;
		p->p_slptime = 0;
		p->p_asleep.as_priority = priority;
		p->p_asleep.as_timo = timo;
		TAILQ_INSERT_TAIL(&slpque[LOOKUP(ident)], p, p_procq);
	}

	splx(s);

	return(0);
}

/*
 * await() - wait for async condition to occur.   The process blocks until
 * wakeup() is called on the most recent asleep() address.  If wakeup is called
 * priority to await(), await() winds up being a NOP.
 *
 * If await() is called more then once (without an intervening asleep() call),
 * await() is still effectively a NOP but it calls mi_switch() to give other
 * processes some cpu before returning.  The process is left runnable.
 *
 * <<<<<<<< EXPERIMENTAL, UNTESTED >>>>>>>>>>
 */

int
await(int priority, int timo)
{
	struct proc *p = curproc;
	int s;

	s = splhigh();

	if (p->p_wchan != NULL) {
		struct callout_handle thandle;
		int sig;
		int catch;

		/*
		 * The call to await() can override defaults specified in
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
			thandle = timeout(endtsleep, (void *)p, timo);

		sig = 0;
		catch = priority & PCATCH;

		if (catch) {
			p->p_flag |= P_SINTR;
			if ((sig = CURSIG(p))) {
				if (p->p_wchan)
					unsleep(p);
				p->p_stat = SRUN;
				goto resume;
			}
			if (p->p_wchan == NULL) {
				catch = 0;
				goto resume;
			}
		}
		p->p_stat = SSLEEP;
		p->p_stats->p_ru.ru_nvcsw++;
		mi_switch();
resume:
		curpriority = p->p_usrpri;

		splx(s);
		p->p_flag &= ~P_SINTR;
		if (p->p_flag & P_TIMEOUT) {
			p->p_flag &= ~P_TIMEOUT;
			if (sig == 0) {
#ifdef KTRACE
				if (KTRPOINT(p, KTR_CSW))
					ktrcsw(p->p_tracep, 0, 0);
#endif
				return (EWOULDBLOCK);
			}
		} else if (timo)
			untimeout(endtsleep, (void *)p, thandle);
		if (catch && (sig != 0 || (sig = CURSIG(p)))) {
#ifdef KTRACE
			if (KTRPOINT(p, KTR_CSW))
				ktrcsw(p->p_tracep, 0, 0);
#endif
			if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
				return (EINTR);
			return (ERESTART);
		}
#ifdef KTRACE
		if (KTRPOINT(p, KTR_CSW))
			ktrcsw(p->p_tracep, 0, 0);
#endif
	} else {
		/*
		 * If as_priority is 0, await() has been called without an 
		 * intervening asleep().  We are still effectively a NOP, 
		 * but we call mi_switch() for safety.
		 */

		if (p->p_asleep.as_priority == 0) {
			p->p_stats->p_ru.ru_nvcsw++;
			mi_switch();
		}
		splx(s);
	}

	/*
	 * clear p_asleep.as_priority as an indication that await() has been
	 * called.  If await() is called again without an intervening asleep(),
	 * await() is still effectively a NOP but the above mi_switch() code
	 * is triggered as a safety.
	 */
	p->p_asleep.as_priority = 0;

	return (0);
}

/*
 * Implement timeout for tsleep or asleep()/await()
 *
 * If process hasn't been awakened (wchan non-zero),
 * set timeout flag and undo the sleep.  If proc
 * is stopped, just unsleep so it will remain stopped.
 */
static void
endtsleep(arg)
	void *arg;
{
	register struct proc *p;
	int s;

	p = (struct proc *)arg;
	s = splhigh();
	if (p->p_wchan) {
		if (p->p_stat == SSLEEP)
			setrunnable(p);
		else
			unsleep(p);
		p->p_flag |= P_TIMEOUT;
	}
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
	if (p->p_wchan) {
		TAILQ_REMOVE(&slpque[LOOKUP(p->p_wchan)], p, p_procq);
		p->p_wchan = 0;
	}
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
	struct proc *np;
	int s;

	s = splhigh();
	qp = &slpque[LOOKUP(ident)];
restart:
	for (p = TAILQ_FIRST(qp); p != NULL; p = np) {
		np = TAILQ_NEXT(p, p_procq);
		if (p->p_wchan == ident) {
			TAILQ_REMOVE(qp, p, p_procq);
			p->p_wchan = 0;
			if (p->p_stat == SSLEEP) {
				/* OPTIMIZED EXPANSION OF setrunnable(p); */
				if (p->p_slptime > 1)
					updatepri(p);
				p->p_slptime = 0;
				p->p_stat = SRUN;
				if (p->p_flag & P_INMEM) {
					setrunqueue(p);
					maybe_resched(p);
				} else {
					p->p_flag |= P_SWAPINREQ;
					wakeup((caddr_t)&proc0);
				}
				/* END INLINE EXPANSION */
				goto restart;
			}
		}
	}
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
	struct proc *np;
	int s;

	s = splhigh();
	qp = &slpque[LOOKUP(ident)];

restart:
	for (p = TAILQ_FIRST(qp); p != NULL; p = np) {
		np = TAILQ_NEXT(p, p_procq);
		if (p->p_wchan == ident) {
			TAILQ_REMOVE(qp, p, p_procq);
			p->p_wchan = 0;
			if (p->p_stat == SSLEEP) {
				/* OPTIMIZED EXPANSION OF setrunnable(p); */
				if (p->p_slptime > 1)
					updatepri(p);
				p->p_slptime = 0;
				p->p_stat = SRUN;
				if (p->p_flag & P_INMEM) {
					setrunqueue(p);
					maybe_resched(p);
					break;
				} else {
					p->p_flag |= P_SWAPINREQ;
					wakeup((caddr_t)&proc0);
				}
				/* END INLINE EXPANSION */
				goto restart;
			}
		}
	}
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
	register struct rlimit *rlim;
	int x;

	/*
	 * XXX this spl is almost unnecessary.  It is partly to allow for
	 * sloppy callers that don't do it (issignal() via CURSIG() is the
	 * main offender).  It is partly to work around a bug in the i386
	 * cpu_switch() (the ipl is not preserved).  We ran for years
	 * without it.  I think there was only a interrupt latency problem.
	 * The main caller, tsleep(), does an splx() a couple of instructions
	 * after calling here.  The buggy caller, issignal(), usually calls
	 * here at spl0() and sometimes returns at splhigh().  The process
	 * then runs for a little too long at splhigh().  The ipl gets fixed
	 * when the process returns to user mode (or earlier).
	 *
	 * It would probably be better to always call here at spl0(). Callers
	 * are prepared to give up control to another process, so they must
	 * be prepared to be interrupted.  The clock stuff here may not
	 * actually need splstatclock().
	 */
	x = splstatclock();

#ifdef SIMPLELOCK_DEBUG
	if (p->p_simple_locks)
		printf("sleep: holding simple lock\n");
#endif
	/*
	 * Compute the amount of time during which the current
	 * process was running, and add that to its total so far.
	 */
	microuptime(&new_switchtime);
	if (timevalcmp(&new_switchtime, &switchtime, <)) {
		printf("microuptime() went backwards (%ld.%06ld -> %ld.%06ld)\n",
		    switchtime.tv_sec, switchtime.tv_usec, 
		    new_switchtime.tv_sec, new_switchtime.tv_usec);
		new_switchtime = switchtime;
	} else {
		p->p_runtime += (new_switchtime.tv_usec - switchtime.tv_usec) +
		    (new_switchtime.tv_sec - switchtime.tv_sec) * (int64_t)1000000;
	}

	/*
	 * Check if the process exceeds its cpu resource allocation.
	 * If over max, kill it.
	 */
	if (p->p_stat != SZOMB && p->p_limit->p_cpulimit != RLIM_INFINITY &&
	    p->p_runtime > p->p_limit->p_cpulimit) {
		rlim = &p->p_rlimit[RLIMIT_CPU];
		if (p->p_runtime / (rlim_t)1000000 >= rlim->rlim_max) {
			killproc(p, "exceeded maximum CPU limit");
		} else {
			psignal(p, SIGXCPU);
			if (rlim->rlim_cur < rlim->rlim_max) {
				/* XXX: we should make a private copy */
				rlim->rlim_cur += 5;
			}
		}
	}

	/*
	 * Pick a new current process and record its start time.
	 */
	cnt.v_swtch++;
	switchtime = new_switchtime;
	cpu_switch(p);
	if (switchtime.tv_sec == 0)
		microuptime(&switchtime);
	switchticks = ticks;

	splx(x);
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
	switch (p->p_stat) {
	case 0:
	case SRUN:
	case SZOMB:
	default:
		panic("setrunnable");
	case SSTOP:
	case SSLEEP:
		unsleep(p);		/* e.g. when sending signals */
		break;

	case SIDL:
		break;
	}
	p->p_stat = SRUN;
	if (p->p_flag & P_INMEM)
		setrunqueue(p);
	splx(s);
	if (p->p_slptime > 1)
		updatepri(p);
	p->p_slptime = 0;
	if ((p->p_flag & P_INMEM) == 0) {
		p->p_flag |= P_SWAPINREQ;
		wakeup((caddr_t)&proc0);
	}
	else
		maybe_resched(p);
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

	if (p->p_rtprio.type == RTP_PRIO_NORMAL) {
		newpriority = PUSER + p->p_estcpu / INVERSE_ESTCPU_WEIGHT +
		    NICE_WEIGHT * p->p_nice;
		newpriority = min(newpriority, MAXPRI);
		p->p_usrpri = newpriority;
	}
	maybe_resched(p);
}

/*
 * Compute a tenex style load average of a quantity on
 * 1, 5 and 15 minute intervals.
 */
static void
loadav(void *arg)
{
	int i, nrun;
	struct loadavg *avg;
	struct proc *p;

	avg = &averunnable;
	nrun = 0;
	LIST_FOREACH(p, &allproc, p_list) {
		switch (p->p_stat) {
		case SRUN:
		case SIDL:
			nrun++;
		}
	}
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

/* ARGSUSED */
static void
sched_setup(dummy)
	void *dummy;
{

	callout_init(&loadav_callout);

	/* Kick off timeout driven events by calling first time. */
	roundrobin(NULL);
	schedcpu(NULL);
	loadav(NULL);
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
		if (p->p_priority >= PUSER)
			p->p_priority = p->p_usrpri;
	}
}
