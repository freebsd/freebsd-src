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
 * $Id: kern_time.c,v 1.44 1998/03/30 09:50:23 phk Exp $
 */

#include <sys/param.h>
#include <sys/sysproto.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

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

static int	nanosleep1 __P((struct proc *p, struct timespec *rqt,
		    struct timespec *rmt));
static int	settime __P((struct timeval *));
static void	timevalfix __P((struct timeval *));
static void	no_lease_updatetime __P((int));

static void 
no_lease_updatetime(deltat)
	int deltat;
{
}

void (*lease_updatetime) __P((int))  = no_lease_updatetime;

static int
settime(tv)
	struct timeval *tv;
{
	struct timeval delta, tv1;
	struct timespec ts;
	struct proc *p;
	int s;

	s = splclock();
	microtime(&tv1);
	delta = *tv;
	timevalsub(&delta, &tv1);

	/*
	 * If the system is secure, we do not allow the time to be 
	 * set to an earlier value (it may be slowed using adjtime,
	 * but not set back). This feature prevent interlopers from
	 * setting arbitrary time stamps on files.
	 */
	if (delta.tv_sec < 0 && securelevel > 1) {
		splx(s);
		return (EPERM);
	}

	ts.tv_sec = tv->tv_sec;
	ts.tv_nsec = tv->tv_usec * 1000;
	set_timecounter(&ts);
	(void) splsoftclock();
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if (timerisset(&p->p_realtimer.it_value))
			timevaladd(&p->p_realtimer.it_value, &delta);
	}
	lease_updatetime(delta.tv_sec);
	splx(s);
	resettodr();
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct clock_gettime_args {
	clockid_t clock_id;
	struct	timespec *tp;
};
#endif

/* ARGSUSED */
int
clock_gettime(p, uap)
	struct proc *p;
	struct clock_gettime_args *uap;
{
	struct timespec ats;

	if (SCARG(uap, clock_id) != CLOCK_REALTIME)
		return (EINVAL);
	nanotime(&ats);
	return (copyout(&ats, SCARG(uap, tp), sizeof(ats)));
}

#ifndef _SYS_SYSPROTO_H_
struct clock_settime_args {
	clockid_t clock_id;
	const struct	timespec *tp;
};
#endif

/* ARGSUSED */
int
clock_settime(p, uap)
	struct proc *p;
	struct clock_settime_args *uap;
{
	struct timeval atv;
	struct timespec ats;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	if (SCARG(uap, clock_id) != CLOCK_REALTIME)
		return (EINVAL);
	if ((error = copyin(SCARG(uap, tp), &ats, sizeof(ats))) != 0)
		return (error);
	if (ats.tv_nsec < 0 || ats.tv_nsec >= 1000000000)
		return (EINVAL);
	/* XXX Don't convert nsec->usec and back */
	TIMESPEC_TO_TIMEVAL(&atv, &ats);
	if ((error = settime(&atv)))
		return (error);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct clock_getres_args {
	clockid_t clock_id;
	struct	timespec *tp;
};
#endif

int
clock_getres(p, uap)
	struct proc *p;
	struct clock_getres_args *uap;
{
	struct timespec ts;
	int error;

	if (SCARG(uap, clock_id) != CLOCK_REALTIME)
		return (EINVAL);
	error = 0;
	if (SCARG(uap, tp)) {
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000000 / timecounter->frequency;
		error = copyout(&ts, SCARG(uap, tp), sizeof(ts));
	}
	return (error);
}

static int nanowait;

static int
nanosleep1(p, rqt, rmt)
	struct proc *p;
	struct timespec *rqt, *rmt;
{
	struct timespec ts, ts2;
	int error, timo;

	if (rqt->tv_nsec < 0 || rqt->tv_nsec >= 1000000000)
		return (EINVAL);
	if (rqt->tv_sec < 0 || rqt->tv_sec == 0 && rqt->tv_nsec == 0)
		return (0);

	getnanoruntime(&ts);
	timespecadd(&ts, rqt);
	error = 0;
	while (1) {
		getnanoruntime(&ts2);
		if (timespeccmp(&ts2, &ts, >=))
			break;
		else if (ts2.tv_sec + 60 * 60 * 24 * hz < ts.tv_sec) 
			timo = 60 * 60 * 24 * hz;
		else if (ts2.tv_sec + 2 < ts.tv_sec) {
			/* Leave one second for the difference in tv_nsec */
			timo = ts.tv_sec - ts2.tv_sec - 1;
			timo *= hz;
		} else {
			timo = (ts.tv_sec - ts2.tv_sec) * 1000000000;
			timo += ts.tv_nsec - ts2.tv_nsec;
			timo /= (1000000000 / hz);
			timo ++;
		}
		error = tsleep(&nanowait, PWAIT | PCATCH, "nanslp", timo);
		if (error == ERESTART) {
			error = EINTR;
			break;
		}
	}
	if (rmt) {
		*rmt = ts;
		timespecsub(rmt, &ts2);
	}
	return(error);
}

#ifndef _SYS_SYSPROTO_H_
struct nanosleep_args {
	struct	timespec *rqtp;
	struct	timespec *rmtp;
};
#endif

/* ARGSUSED */
int
nanosleep(p, uap)
	struct proc *p;
	struct nanosleep_args *uap;
{
	struct timespec rmt, rqt;
	int error, error2;

	error = copyin(SCARG(uap, rqtp), &rqt, sizeof(rqt));
	if (error)
		return (error);
	if (SCARG(uap, rmtp))
		if (!useracc((caddr_t)SCARG(uap, rmtp), sizeof(rmt), B_WRITE))
			return (EFAULT);
	error = nanosleep1(p, &rqt, &rmt);
	if (SCARG(uap, rmtp)) {
		error2 = copyout(&rmt, SCARG(uap, rmtp), sizeof(rmt));
		if (error2)	/* XXX shouldn't happen, did useracc() above */
			return (error2);
	}
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct signanosleep_args {
	struct	timespec *rqtp;
	struct	timespec *rmtp;
	sigset_t *mask;
};
#endif

/* ARGSUSED */
int
signanosleep(p, uap)
	struct proc *p;
	struct signanosleep_args *uap;
{
	struct timespec rmt, rqt;
	int error, error2;
	sigset_t mask;

	error = copyin(SCARG(uap, rqtp), &rqt, sizeof(rqt));
	if (error)
		return (error);
	if (SCARG(uap, rmtp))
		if (!useracc((caddr_t)SCARG(uap, rmtp), sizeof(rmt), B_WRITE))
			return (EFAULT);
	error = copyin(SCARG(uap, mask), &mask, sizeof(mask));
	if (error)
		return (error);

	/* change mask for sleep */
	p->p_sigmask = mask &~ sigcantmask;

	error = nanosleep1(p, &rqt, &rmt);

	if (SCARG(uap, rmtp)) {
		error2 = copyout(&rmt, SCARG(uap, rmtp), sizeof(rmt));
		if (error2)	/* XXX shouldn't happen, did useracc() above */
			return (error2);
	}
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct gettimeofday_args {
	struct	timeval *tp;
	struct	timezone *tzp;
};
#endif
/* ARGSUSED */
int
gettimeofday(p, uap)
	struct proc *p;
	register struct gettimeofday_args *uap;
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
settimeofday(p, uap)
	struct proc *p;
	struct settimeofday_args *uap;
{
	struct timeval atv;
	struct timezone atz;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);
	/* Verify all parameters before changing time. */
	if (uap->tv) {
		if ((error = copyin((caddr_t)uap->tv, (caddr_t)&atv,
		    sizeof(atv))))
			return (error);
		if (atv.tv_usec < 0 || atv.tv_usec >= 1000000)
			return (EINVAL);
	}
	if (uap->tzp &&
	    (error = copyin((caddr_t)uap->tzp, (caddr_t)&atz, sizeof(atz))))
		return (error);
	if (uap->tv && (error = settime(&atv)))
		return (error);
	if (uap->tzp)
		tz = atz;
	return (0);
}

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
adjtime(p, uap)
	struct proc *p;
	register struct adjtime_args *uap;
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
	if (ndelta > bigadj || ndelta < -bigadj)
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
getitimer(p, uap)
	struct proc *p;
	register struct getitimer_args *uap;
{
	struct timeval ctv;
	struct itimerval aitv;
	int s;

	if (uap->which > ITIMER_PROF)
		return (EINVAL);
	s = splclock(); /* XXX still needed ? */
	if (uap->which == ITIMER_REAL) {
		/*
		 * Convert from absoulte to relative time in .it_value
		 * part of real time timer.  If time for real time timer
		 * has passed return 0, else return difference between
		 * current time and time for the timer to go off.
		 */
		aitv = p->p_realtimer;
		if (timerisset(&aitv.it_value)) {
			getmicrotime(&ctv);
			if (timercmp(&aitv.it_value, &ctv, <))
				timerclear(&aitv.it_value);
			else
				timevalsub(&aitv.it_value, &ctv);
		}
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
setitimer(p, uap)
	struct proc *p;
	register struct setitimer_args *uap;
{
	struct itimerval aitv;
	struct timeval ctv;
	register struct itimerval *itvp;
	int s, error;

	if (uap->which > ITIMER_PROF)
		return (EINVAL);
	itvp = uap->itv;
	if (itvp && (error = copyin((caddr_t)itvp, (caddr_t)&aitv,
	    sizeof(struct itimerval))))
		return (error);
	if ((uap->itv = uap->oitv) &&
	    (error = getitimer(p, (struct getitimer_args *)uap)))
		return (error);
	if (itvp == 0)
		return (0);
	if (itimerfix(&aitv.it_value))
		return (EINVAL);
	if (!timerisset(&aitv.it_value))
		timerclear(&aitv.it_interval);
	else if (itimerfix(&aitv.it_interval))
		return (EINVAL);
	s = splclock(); /* XXX: still needed ? */
	if (uap->which == ITIMER_REAL) {
		if (timerisset(&p->p_realtimer.it_value))
			untimeout(realitexpire, (caddr_t)p, p->p_ithandle);
		if (timerisset(&aitv.it_value)) {
			getmicrotime(&ctv);
			timevaladd(&aitv.it_value, &ctv);
			p->p_ithandle = timeout(realitexpire, (caddr_t)p,
						hzto(&aitv.it_value));
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
	struct timeval ctv;
	int s;

	p = (struct proc *)arg;
	psignal(p, SIGALRM);
	if (!timerisset(&p->p_realtimer.it_interval)) {
		timerclear(&p->p_realtimer.it_value);
		return;
	}
	for (;;) {
		s = splclock(); /* XXX: still neeeded ? */
		timevaladd(&p->p_realtimer.it_value,
		    &p->p_realtimer.it_interval);
		getmicrotime(&ctv);
		if (timercmp(&p->p_realtimer.it_value, &ctv, >)) {
			p->p_ithandle =
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
