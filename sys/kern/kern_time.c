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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

int tz_minuteswest;
int tz_dsttime;

/*
 * Time of day and interval timer support.
 *
 * These routines provide the kernel entry points to get and set
 * the time-of-day and per-process interval timers.  Subroutines
 * here provide support for adding and subtracting timeval structures
 * and decrementing interval timers, optionally reloading the interval
 * timers when they expire.
 */

static int	nanosleep1(struct thread *td, struct timespec *rqt,
		    struct timespec *rmt);
static int	settime(struct thread *, struct timeval *);
static void	timevalfix(struct timeval *);
static void	no_lease_updatetime(int);

static void 
no_lease_updatetime(deltat)
	int deltat;
{
}

void (*lease_updatetime)(int)  = no_lease_updatetime;

static int
settime(struct thread *td, struct timeval *tv)
{
	struct timeval delta, tv1, tv2;
	static struct timeval maxtime, laststep;
	struct timespec ts;
	int s;

	s = splclock();
	microtime(&tv1);
	delta = *tv;
	timevalsub(&delta, &tv1);

	/*
	 * If the system is secure, we do not allow the time to be 
	 * set to a value earlier than 1 second less than the highest
	 * time we have yet seen. The worst a miscreant can do in
	 * this circumstance is "freeze" time. He couldn't go
	 * back to the past.
	 *
	 * We similarly do not allow the clock to be stepped more
	 * than one second, nor more than once per second. This allows
	 * a miscreant to make the clock march double-time, but no worse.
	 */
	if (securelevel_gt(td->td_ucred, 1) != 0) {
		if (delta.tv_sec < 0 || delta.tv_usec < 0) {
			/*
			 * Update maxtime to latest time we've seen.
			 */
			if (tv1.tv_sec > maxtime.tv_sec)
				maxtime = tv1;
			tv2 = *tv;
			timevalsub(&tv2, &maxtime);
			if (tv2.tv_sec < -1) {
				tv->tv_sec = maxtime.tv_sec - 1;
				printf("Time adjustment clamped to -1 second\n");
			}
		} else {
			if (tv1.tv_sec == laststep.tv_sec) {
				splx(s);
				return (EPERM);
			}
			if (delta.tv_sec > 1) {
				tv->tv_sec = tv1.tv_sec + 1;
				printf("Time adjustment clamped to +1 second\n");
			}
			laststep = *tv;
		}
	}

	ts.tv_sec = tv->tv_sec;
	ts.tv_nsec = tv->tv_usec * 1000;
	mtx_lock(&Giant);
	tc_setclock(&ts);
	(void) splsoftclock();
	lease_updatetime(delta.tv_sec);
	splx(s);
	resettodr();
	mtx_unlock(&Giant);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct clock_gettime_args {
	clockid_t clock_id;
	struct	timespec *tp;
};
#endif

/*
 * MPSAFE
 */
/* ARGSUSED */
int
clock_gettime(struct thread *td, struct clock_gettime_args *uap)
{
	struct timespec ats;

	if (uap->clock_id == CLOCK_REALTIME)
		nanotime(&ats);
	else if (uap->clock_id == CLOCK_MONOTONIC)
		nanouptime(&ats);
	else
		return (EINVAL);
	return (copyout(&ats, uap->tp, sizeof(ats)));
}

#ifndef _SYS_SYSPROTO_H_
struct clock_settime_args {
	clockid_t clock_id;
	const struct	timespec *tp;
};
#endif

/*
 * MPSAFE
 */
/* ARGSUSED */
int
clock_settime(struct thread *td, struct clock_settime_args *uap)
{
	struct timeval atv;
	struct timespec ats;
	int error;

#ifdef MAC
	error = mac_check_system_settime(td->td_ucred);
	if (error)
		return (error);
#endif
	if ((error = suser(td)) != 0)
		return (error);
	if (uap->clock_id != CLOCK_REALTIME)
		return (EINVAL);
	if ((error = copyin(uap->tp, &ats, sizeof(ats))) != 0)
		return (error);
	if (ats.tv_nsec < 0 || ats.tv_nsec >= 1000000000)
		return (EINVAL);
	/* XXX Don't convert nsec->usec and back */
	TIMESPEC_TO_TIMEVAL(&atv, &ats);
	error = settime(td, &atv);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct clock_getres_args {
	clockid_t clock_id;
	struct	timespec *tp;
};
#endif

int
clock_getres(struct thread *td, struct clock_getres_args *uap)
{
	struct timespec ts;
	int error;

	if (uap->clock_id != CLOCK_REALTIME)
		return (EINVAL);
	error = 0;
	if (uap->tp) {
		ts.tv_sec = 0;
		/*
		 * Round up the result of the division cheaply by adding 1.
		 * Rounding up is especially important if rounding down
		 * would give 0.  Perfect rounding is unimportant.
		 */
		ts.tv_nsec = 1000000000 / tc_getfrequency() + 1;
		error = copyout(&ts, uap->tp, sizeof(ts));
	}
	return (error);
}

static int nanowait;

static int
nanosleep1(struct thread *td, struct timespec *rqt, struct timespec *rmt)
{
	struct timespec ts, ts2, ts3;
	struct timeval tv;
	int error;

	if (rqt->tv_nsec < 0 || rqt->tv_nsec >= 1000000000)
		return (EINVAL);
	if (rqt->tv_sec < 0 || (rqt->tv_sec == 0 && rqt->tv_nsec == 0))
		return (0);
	getnanouptime(&ts);
	timespecadd(&ts, rqt);
	TIMESPEC_TO_TIMEVAL(&tv, rqt);
	for (;;) {
		error = tsleep(&nanowait, PWAIT | PCATCH, "nanslp",
		    tvtohz(&tv));
		getnanouptime(&ts2);
		if (error != EWOULDBLOCK) {
			if (error == ERESTART)
				error = EINTR;
			if (rmt != NULL) {
				timespecsub(&ts, &ts2);
				if (ts.tv_sec < 0)
					timespecclear(&ts);
				*rmt = ts;
			}
			return (error);
		}
		if (timespeccmp(&ts2, &ts, >=))
			return (0);
		ts3 = ts;
		timespecsub(&ts3, &ts2);
		TIMESPEC_TO_TIMEVAL(&tv, &ts3);
	}
}

#ifndef _SYS_SYSPROTO_H_
struct nanosleep_args {
	struct	timespec *rqtp;
	struct	timespec *rmtp;
};
#endif

/* 
 * MPSAFE
 */
/* ARGSUSED */
int
nanosleep(struct thread *td, struct nanosleep_args *uap)
{
	struct timespec rmt, rqt;
	int error;

	error = copyin(uap->rqtp, &rqt, sizeof(rqt));
	if (error)
		return (error);

	if (uap->rmtp &&
	    !useracc((caddr_t)uap->rmtp, sizeof(rmt), VM_PROT_WRITE))
			return (EFAULT);
	error = nanosleep1(td, &rqt, &rmt);
	if (error && uap->rmtp) {
		int error2;

		error2 = copyout(&rmt, uap->rmtp, sizeof(rmt));
		if (error2)
			error = error2;
	}
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct gettimeofday_args {
	struct	timeval *tp;
	struct	timezone *tzp;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
gettimeofday(struct thread *td, struct gettimeofday_args *uap)
{
	struct timeval atv;
	struct timezone rtz;
	int error = 0;

	if (uap->tp) {
		microtime(&atv);
		error = copyout(&atv, uap->tp, sizeof (atv));
	}
	if (error == 0 && uap->tzp != NULL) {
		rtz.tz_minuteswest = tz_minuteswest;
		rtz.tz_dsttime = tz_dsttime;
		error = copyout(&rtz, uap->tzp, sizeof (rtz));
	}
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct settimeofday_args {
	struct	timeval *tv;
	struct	timezone *tzp;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
settimeofday(struct thread *td, struct settimeofday_args *uap)
{
	struct timeval atv;
	struct timezone atz;
	int error = 0;

#ifdef MAC
	error = mac_check_system_settime(td->td_ucred);
	if (error)
		return (error);
#endif
	if ((error = suser(td)))
		return (error);
	/* Verify all parameters before changing time. */
	if (uap->tv) {
		if ((error = copyin(uap->tv, &atv, sizeof(atv))))
			return (error);
		if (atv.tv_usec < 0 || atv.tv_usec >= 1000000)
			return (EINVAL);
	}
	if (uap->tzp &&
	    (error = copyin(uap->tzp, &atz, sizeof(atz))))
		return (error);
	
	if (uap->tv && (error = settime(td, &atv)))
		return (error);
	if (uap->tzp) {
		tz_minuteswest = atz.tz_minuteswest;
		tz_dsttime = atz.tz_dsttime;
	}
	return (error);
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
/*
 * MPSAFE
 */
int
getitimer(struct thread *td, struct getitimer_args *uap)
{
	struct proc *p = td->td_proc;
	struct timeval ctv;
	struct itimerval aitv;

	if (uap->which > ITIMER_PROF)
		return (EINVAL);

	if (uap->which == ITIMER_REAL) {
		/*
		 * Convert from absolute to relative time in .it_value
		 * part of real time timer.  If time for real time timer
		 * has passed return 0, else return difference between
		 * current time and time for the timer to go off.
		 */
		PROC_LOCK(p);
		aitv = p->p_realtimer;
		PROC_UNLOCK(p);
		if (timevalisset(&aitv.it_value)) {
			getmicrouptime(&ctv);
			if (timevalcmp(&aitv.it_value, &ctv, <))
				timevalclear(&aitv.it_value);
			else
				timevalsub(&aitv.it_value, &ctv);
		}
	} else {
		mtx_lock_spin(&sched_lock);
		aitv = p->p_stats->p_timer[uap->which];
		mtx_unlock_spin(&sched_lock);
	}
	return (copyout(&aitv, uap->itv, sizeof (struct itimerval)));
}

#ifndef _SYS_SYSPROTO_H_
struct setitimer_args {
	u_int	which;
	struct	itimerval *itv, *oitv;
};
#endif
/*
 * MPSAFE
 */
int
setitimer(struct thread *td, struct setitimer_args *uap)
{
	struct proc *p = td->td_proc;
	struct itimerval aitv, oitv;
	struct timeval ctv;
	int error;

	if (uap->itv == NULL) {
		uap->itv = uap->oitv;
		return (getitimer(td, (struct getitimer_args *)uap));
	}

	if (uap->which > ITIMER_PROF)
		return (EINVAL);
	if ((error = copyin(uap->itv, &aitv, sizeof(struct itimerval))))
		return (error);
	if (itimerfix(&aitv.it_value))
		return (EINVAL);
	if (!timevalisset(&aitv.it_value))
		timevalclear(&aitv.it_interval);
	else if (itimerfix(&aitv.it_interval))
		return (EINVAL);

	if (uap->which == ITIMER_REAL) {
		PROC_LOCK(p);
		if (timevalisset(&p->p_realtimer.it_value))
			callout_stop(&p->p_itcallout);
		getmicrouptime(&ctv);
		if (timevalisset(&aitv.it_value)) {
			callout_reset(&p->p_itcallout, tvtohz(&aitv.it_value),
			    realitexpire, p);
			timevaladd(&aitv.it_value, &ctv);
		}
		oitv = p->p_realtimer;
		p->p_realtimer = aitv;
		PROC_UNLOCK(p);
		if (timevalisset(&oitv.it_value)) {
			if (timevalcmp(&oitv.it_value, &ctv, <))
				timevalclear(&oitv.it_value);
			else
				timevalsub(&oitv.it_value, &ctv);
		}
	} else {
		mtx_lock_spin(&sched_lock);
		oitv = p->p_stats->p_timer[uap->which];
		p->p_stats->p_timer[uap->which] = aitv;
		mtx_unlock_spin(&sched_lock);
	}
	if (uap->oitv == NULL)
		return (0);
	return (copyout(&oitv, uap->oitv, sizeof(struct itimerval)));
}

/*
 * Real interval timer expired:
 * send process whose timer expired an alarm signal.
 * If time is not set up to reload, then just return.
 * Else compute next time timer should go off which is > current time.
 * This is where delay in processing this timeout causes multiple
 * SIGALRM calls to be compressed into one.
 * tvtohz() always adds 1 to allow for the time until the next clock
 * interrupt being strictly less than 1 clock tick, but we don't want
 * that here since we want to appear to be in sync with the clock
 * interrupt even when we're delayed.
 */
void
realitexpire(void *arg)
{
	struct proc *p;
	struct timeval ctv, ntv;

	p = (struct proc *)arg;
	PROC_LOCK(p);
	psignal(p, SIGALRM);
	if (!timevalisset(&p->p_realtimer.it_interval)) {
		timevalclear(&p->p_realtimer.it_value);
		if (p->p_flag & P_WEXIT)
			wakeup(&p->p_itcallout);
		PROC_UNLOCK(p);
		return;
	}
	for (;;) {
		timevaladd(&p->p_realtimer.it_value,
		    &p->p_realtimer.it_interval);
		getmicrouptime(&ctv);
		if (timevalcmp(&p->p_realtimer.it_value, &ctv, >)) {
			ntv = p->p_realtimer.it_value;
			timevalsub(&ntv, &ctv);
			callout_reset(&p->p_itcallout, tvtohz(&ntv) - 1,
			    realitexpire, p);
			PROC_UNLOCK(p);
			return;
		}
	}
	/*NOTREACHED*/
}

/*
 * Check that a proposed value to load into the .it_value or
 * .it_interval part of an interval timer is acceptable, and
 * fix it to have at least minimal value (i.e. if it is less
 * than the resolution of the clock, round it up.)
 */
int
itimerfix(struct timeval *tv)
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
itimerdecr(struct itimerval *itp, int usec)
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
	if (timevalisset(&itp->it_value))
		return (1);
	/* expired, exactly at end of interval */
expire:
	if (timevalisset(&itp->it_interval)) {
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
timevaladd(struct timeval *t1, const struct timeval *t2)
{

	t1->tv_sec += t2->tv_sec;
	t1->tv_usec += t2->tv_usec;
	timevalfix(t1);
}

void
timevalsub(struct timeval *t1, const struct timeval *t2)
{

	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}

static void
timevalfix(struct timeval *t1)
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

/*
 * ratecheck(): simple time-based rate-limit checking.
 */
int
ratecheck(struct timeval *lasttime, const struct timeval *mininterval)
{
	struct timeval tv, delta;
	int rv = 0;

	getmicrouptime(&tv);		/* NB: 10ms precision */
	delta = tv;
	timevalsub(&delta, lasttime);

	/*
	 * check for 0,0 is so that the message will be seen at least once,
	 * even if interval is huge.
	 */
	if (timevalcmp(&delta, mininterval, >=) ||
	    (lasttime->tv_sec == 0 && lasttime->tv_usec == 0)) {
		*lasttime = tv;
		rv = 1;
	}

	return (rv);
}

/*
 * ppsratecheck(): packets (or events) per second limitation.
 *
 * Return 0 if the limit is to be enforced (e.g. the caller
 * should drop a packet because of the rate limitation).
 *
 * maxpps of 0 always causes zero to be returned.  maxpps of -1
 * always causes 1 to be returned; this effectively defeats rate
 * limiting.
 *
 * Note that we maintain the struct timeval for compatibility
 * with other bsd systems.  We reuse the storage and just monitor
 * clock ticks for minimal overhead.  
 */
int
ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps)
{
	int now;

	/*
	 * Reset the last time and counter if this is the first call
	 * or more than a second has passed since the last update of
	 * lasttime.
	 */
	now = ticks;
	if (lasttime->tv_sec == 0 || (u_int)(now - lasttime->tv_sec) >= hz) {
		lasttime->tv_sec = now;
		*curpps = 1;
		return (maxpps != 0);
	} else {
		(*curpps)++;		/* NB: ignore potential overflow */
		return (maxpps < 0 || *curpps < maxpps);
	}
}
