/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_time.c	8.1 (Berkeley) 6/10/93
 * $Id: kern_time.c,v 1.13 1995/12/14 08:31:37 phk Exp $
 */

#include <sys/param.h>
#include <sys/sysproto.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <machine/cpu.h>

struct timezone tz;

/*
 * Time of day and interval timer support.
 *
 * These routines provide the kernel entry points to get and set
 * the time-of-day and per-process interval timers.  Subroutines
 * here provide support for adding and subtracting timeval structures
 * and decrementing interval timers, optionally reloading the interval
 * timers when they expire.
 */

static void	timevalfix __P((struct timeval *));

#ifndef _SYS_SYSPROTO_H_
struct gettimeofday_args {
	struct	timeval *tp;
	struct	timezone *tzp;
};
#endif
/* ARGSUSED */
int
gettimeofday(p, uap, retval)
	struct proc *p;
	register struct gettimeofday_args *uap;
	int *retval;
{
	struct timeval atv;
	int error = 0;

	if (uap->tp) {
		microtime(&atv);
		if ((error = copyout((caddr_t)&atv, (caddr_t)uap->tp,
		    sizeof (atv))))
			return (error);
	}
	if (uap->tzp)
		error = copyout((caddr_t)&tz, (caddr_t)uap->tzp,
		    sizeof (tz));
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct settimeofday_args {
	struct	timeval *tv;
	struct	timezone *tzp;
};
#endif
/* ARGSUSED */
int
settimeofday(p, uap, retval)
	struct proc *p;
	struct settimeofday_args *uap;
	int *retval;
{
	struct timeval atv, delta;
	struct timezone atz;
	int error, s;

	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);
	/* Verify all parameters before changing time. */
	if (uap->tv &&
	    (error = copyin((caddr_t)uap->tv, (caddr_t)&atv, sizeof(atv))))
		return (error);
	if (atv.tv_usec < 0 || atv.tv_usec >= 1000000)
		return (EINVAL);
	if (uap->tzp &&
	    (error = copyin((caddr_t)uap->tzp, (caddr_t)&atz, sizeof(atz))))
		return (error);
	if (uap->tv) {
		/* WHAT DO WE DO ABOUT PENDING REAL-TIME TIMEOUTS??? */
		s = splclock();
		/* nb. delta.tv_usec may be < 0, but this is OK here */
		delta.tv_sec = atv.tv_sec - time.tv_sec;
		delta.tv_usec = atv.tv_usec - time.tv_usec;
		time = atv;	/* XXX should avoid skew in tv_usec */
		(void) splsoftclock();
		timevalfix(&delta);
		timevaladd(&boottime, &delta);
		timevaladd(&runtime, &delta);
		LEASE_UPDATETIME(delta.tv_sec);
		splx(s);
		resettodr();
	}
	if (uap->tzp)
		tz = atz;
	return (0);
}

extern	int tickadj;			/* "standard" clock skew, us./tick */
int	tickdelta;			/* current clock skew, us. per tick */
long	timedelta;			/* unapplied time correction, us. */
static long	bigadj = 1000000;	/* use 10x skew above bigadj us. */

#ifndef _SYS_SYSPROTO_H_
struct adjtime_args {
	struct timeval *delta;
	struct timeval *olddelta;
};
#endif
/* ARGSUSED */
int
adjtime(p, uap, retval)
	struct proc *p;
	register struct adjtime_args *uap;
	int *retval;
{
	struct timeval atv;
	register long ndelta, ntickdelta, odelta;
	int s, error;

	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);
	if ((error =
	    copyin((caddr_t)uap->delta, (caddr_t)&atv, sizeof(struct timeval))))
		return (error);

	/*
	 * Compute the total correction and the rate at which to apply it.
	 * Round the adjustment down to a whole multiple of the per-tick
	 * delta, so that after some number of incremental changes in
	 * hardclock(), tickdelta will become zero, lest the correction
	 * overshoot and start taking us away from the desired final time.
	 */
	ndelta = atv.tv_sec * 1000000 + atv.tv_usec;
	if (ndelta > bigadj)
		ntickdelta = 10 * tickadj;
	else
		ntickdelta = tickadj;
	if (ndelta % ntickdelta)
		ndelta = ndelta / ntickdelta * ntickdelta;

	/*
	 * To make hardclock()'s job easier, make the per-tick delta negative
	 * if we want time to run slower; then hardclock can simply compute
	 * tick + tickdelta, and subtract tickdelta from timedelta.
	 */
	if (ndelta < 0)
		ntickdelta = -ntickdelta;
	s = splclock();
	odelta = timedelta;
	timedelta = ndelta;
	tickdelta = ntickdelta;
	splx(s);

	if (uap->olddelta) {
		atv.tv_sec = odelta / 1000000;
		atv.tv_usec = odelta % 1000000;
		(void) copyout((caddr_t)&atv, (caddr_t)uap->olddelta,
		    sizeof(struct timeval));
	}
	return (0);
}

/*
 * Get value of an interval timer.  The process virtual and
 * profiling virtual time timers are kept in the p_stats area, since
 * they can be swapped out.  These are kept internally in the
 * way they are specified externally: in time until they expire.
 *
 * The real time interval timer is kept in the process table slot
 * for the process, and its value (it_value) is kept as an
 * absolute time rather than as a delta, so that it is easy to keep
 * periodic real-time signals from drifting.
 *
 * Virtual time timers are processed in the hardclock() routine of
 * kern_clock.c.  The real time timer is processed by a timeout
 * routine, called from the softclock() routine.  Since a callout
 * may be delayed in real time due to interrupt processing in the system,
 * it is possible for the real time timeout routine (realitexpire, given below),
 * to be delayed in real time past when it is supposed to occur.  It
 * does not suffice, therefore, to reload the real timer .it_value from the
 * real time timers .it_interval.  Rather, we compute the next time in
 * absolute time the timer should go off.
 */
#ifndef _SYS_SYSPROTO_H_
struct getitimer_args {
	u_int	which;
	struct	itimerval *itv;
};
#endif
/* ARGSUSED */
int
getitimer(p, uap, retval)
	struct proc *p;
	register struct getitimer_args *uap;
	int *retval;
{
	struct itimerval aitv;
	int s;

	if (uap->which > ITIMER_PROF)
		return (EINVAL);
	s = splclock();
	if (uap->which == ITIMER_REAL) {
		/*
		 * Convert from absoulte to relative time in .it_value
		 * part of real time timer.  If time for real time timer
		 * has passed return 0, else return difference between
		 * current time and time for the timer to go off.
		 */
		aitv = p->p_realtimer;
		if (timerisset(&aitv.it_value))
			if (timercmp(&aitv.it_value, &time, <))
				timerclear(&aitv.it_value);
			else
				timevalsub(&aitv.it_value,
				    (struct timeval *)&time);
	} else
		aitv = p->p_stats->p_timer[uap->which];
	splx(s);
	return (copyout((caddr_t)&aitv, (caddr_t)uap->itv,
	    sizeof (struct itimerval)));
}

#ifndef _SYS_SYSPROTO_H_
struct setitimer_args {
	u_int	which;
	struct	itimerval *itv, *oitv;
};
#endif
/* ARGSUSED */
int
setitimer(p, uap, retval)
	struct proc *p;
	register struct setitimer_args *uap;
	int *retval;
{
	struct itimerval aitv;
	register struct itimerval *itvp;
	int s, error;

	if (uap->which > ITIMER_PROF)
		return (EINVAL);
	itvp = uap->itv;
	if (itvp && (error = copyin((caddr_t)itvp, (caddr_t)&aitv,
	    sizeof(struct itimerval))))
		return (error);
	if ((uap->itv = uap->oitv) &&
	    (error = getitimer(p, (struct getitimer_args *)uap, retval)))
		return (error);
	if (itvp == 0)
		return (0);
	if (itimerfix(&aitv.it_value) || itimerfix(&aitv.it_interval))
		return (EINVAL);
	s = splclock();
	if (uap->which == ITIMER_REAL) {
		untimeout(realitexpire, (caddr_t)p);
		if (timerisset(&aitv.it_value)) {
			timevaladd(&aitv.it_value, (struct timeval *)&time);
			timeout(realitexpire, (caddr_t)p, hzto(&aitv.it_value));
		}
		p->p_realtimer = aitv;
	} else
		p->p_stats->p_timer[uap->which] = aitv;
	splx(s);
	return (0);
}

/*
 * Real interval timer expired:
 * send process whose timer expired an alarm signal.
 * If time is not set up to reload, then just return.
 * Else compute next time timer should go off which is > current time.
 * This is where delay in processing this timeout causes multiple
 * SIGALRM calls to be compressed into one.
 * hzto() always adds 1 to allow for the time until the next clock
 * interrupt being strictly less than 1 clock tick, but we don't want
 * that here since we want to appear to be in sync with the clock
 * interrupt even when we're delayed.
 */
void
realitexpire(arg)
	void *arg;
{
	register struct proc *p;
	int s;

	p = (struct proc *)arg;
	psignal(p, SIGALRM);
	if (!timerisset(&p->p_realtimer.it_interval)) {
		timerclear(&p->p_realtimer.it_value);
		return;
	}
	for (;;) {
		s = splclock();
		timevaladd(&p->p_realtimer.it_value,
		    &p->p_realtimer.it_interval);
		if (timercmp(&p->p_realtimer.it_value, &time, >)) {
			timeout(realitexpire, (caddr_t)p,
			    hzto(&p->p_realtimer.it_value) - 1);
			splx(s);
			return;
		}
		splx(s);
	}
}

/*
 * Check that a proposed value to load into the .it_value or
 * .it_interval part of an interval timer is acceptable, and
 * fix it to have at least minimal value (i.e. if it is less
 * than the resolution of the clock, round it up.)
 */
int
itimerfix(tv)
	struct timeval *tv;
{

	if (tv->tv_sec < 0 || tv->tv_sec > 100000000 ||
	    tv->tv_usec < 0 || tv->tv_usec >= 1000000)
		return (EINVAL);
	if (tv->tv_sec == 0 && tv->tv_usec != 0 && tv->tv_usec < tick)
		tv->tv_usec = tick;
	return (0);
}

/*
 * Decrement an interval timer by a specified number
 * of microseconds, which must be less than a second,
 * i.e. < 1000000.  If the timer expires, then reload
 * it.  In this case, carry over (usec - old value) to
 * reduce the value reloaded into the timer so that
 * the timer does not drift.  This routine assumes
 * that it is called in a context where the timers
 * on which it is operating cannot change in value.
 */
int
itimerdecr(itp, usec)
	register struct itimerval *itp;
	int usec;
{

	if (itp->it_value.tv_usec < usec) {
		if (itp->it_value.tv_sec == 0) {
			/* expired, and already in next interval */
			usec -= itp->it_value.tv_usec;
			goto expire;
		}
		itp->it_value.tv_usec += 1000000;
		itp->it_value.tv_sec--;
	}
	itp->it_value.tv_usec -= usec;
	usec = 0;
	if (timerisset(&itp->it_value))
		return (1);
	/* expired, exactly at end of interval */
expire:
	if (timerisset(&itp->it_interval)) {
		itp->it_value = itp->it_interval;
		itp->it_value.tv_usec -= usec;
		if (itp->it_value.tv_usec < 0) {
			itp->it_value.tv_usec += 1000000;
			itp->it_value.tv_sec--;
		}
	} else
		itp->it_value.tv_usec = 0;		/* sec is already 0 */
	return (0);
}

/*
 * Add and subtract routines for timevals.
 * N.B.: subtract routine doesn't deal with
 * results which are before the beginning,
 * it just gets very confused in this case.
 * Caveat emptor.
 */
void
timevaladd(t1, t2)
	struct timeval *t1, *t2;
{

	t1->tv_sec += t2->tv_sec;
	t1->tv_usec += t2->tv_usec;
	timevalfix(t1);
}

void
timevalsub(t1, t2)
	struct timeval *t1, *t2;
{

	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}

static void
timevalfix(t1)
	struct timeval *t1;
{

	if (t1->tv_usec < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
	if (t1->tv_usec >= 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}
