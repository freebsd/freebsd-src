/*	$NetBSD: linux_time.c,v 1.14 2006/05/14 03:40:54 christos Exp $ */

/*-
 * SPDX-License-Identifier: BSD-2-Clause-NetBSD
 *
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#if 0
__KERNEL_RCSID(0, "$NetBSD: linux_time.c,v 1.14 2006/05/14 03:40:54 christos Exp $");
#endif

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/ucred.h>
#include <sys/limits.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/resourcevar.h>
#include <sys/sdt.h>
#include <sys/signal.h>
#include <sys/stdint.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/proc.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_dtrace.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_timer.h>
#include <compat/linux/linux_util.h>

/* DTrace init */
LIN_SDT_PROVIDER_DECLARE(LINUX_DTRACE);

/**
 * DTrace probes in this module.
 */
LIN_SDT_PROBE_DEFINE1(time, linux_to_native_clockid, unsupported_clockid,
    "clockid_t");
LIN_SDT_PROBE_DEFINE1(time, linux_to_native_clockid, unknown_clockid,
    "clockid_t");
LIN_SDT_PROBE_DEFINE1(time, linux_common_clock_gettime, conversion_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_clock_gettime, gettime_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_clock_gettime, copyout_error, "int");
#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
LIN_SDT_PROBE_DEFINE1(time, linux_clock_gettime64, gettime_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_clock_gettime64, copyout_error, "int");
#endif
LIN_SDT_PROBE_DEFINE1(time, linux_clock_settime, conversion_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_common_clock_settime, settime_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_common_clock_settime, conversion_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_clock_settime, copyin_error, "int");
#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
LIN_SDT_PROBE_DEFINE1(time, linux_clock_settime64, conversion_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_clock_settime64, copyin_error, "int");
#endif
LIN_SDT_PROBE_DEFINE0(time, linux_common_clock_getres, nullcall);
LIN_SDT_PROBE_DEFINE1(time, linux_common_clock_getres, conversion_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_common_clock_getres, getres_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_clock_getres, copyout_error, "int");
#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
LIN_SDT_PROBE_DEFINE1(time, linux_clock_getres_time64, copyout_error, "int");
#endif
LIN_SDT_PROBE_DEFINE1(time, linux_nanosleep, copyout_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_nanosleep, copyin_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_clock_nanosleep, copyout_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_clock_nanosleep, copyin_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_common_clock_nanosleep, unsupported_flags, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_common_clock_nanosleep, unsupported_clockid, "int");
#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
LIN_SDT_PROBE_DEFINE1(time, linux_clock_nanosleep_time64, copyout_error, "int");
LIN_SDT_PROBE_DEFINE1(time, linux_clock_nanosleep_time64, copyin_error, "int");
#endif

static int	linux_common_clock_gettime(struct thread *, clockid_t,
		    struct timespec *);
static int	linux_common_clock_settime(struct thread *, clockid_t,
		    struct timespec *);
static int	linux_common_clock_getres(struct thread *, clockid_t,
		    struct timespec *);
static int	linux_common_clock_nanosleep(struct thread *, clockid_t,
		    l_int, struct timespec *, struct timespec *);

int
native_to_linux_timespec(struct l_timespec *ltp, struct timespec *ntp)
{

#ifdef COMPAT_LINUX32
	if (ntp->tv_sec > INT_MAX || ntp->tv_sec < INT_MIN)
		return (EOVERFLOW);
#endif
	ltp->tv_sec = ntp->tv_sec;
	ltp->tv_nsec = ntp->tv_nsec;

	return (0);
}

int
linux_to_native_timespec(struct timespec *ntp, struct l_timespec *ltp)
{

	if (!timespecvalid_interval(ltp))
		return (EINVAL);
	ntp->tv_sec = ltp->tv_sec;
	ntp->tv_nsec = ltp->tv_nsec;

	return (0);
}

int
linux_put_timespec(struct timespec *ntp, struct l_timespec *ltp)
{
	struct l_timespec lts;
	int error;

	error = native_to_linux_timespec(&lts, ntp);
	if (error != 0)
		return (error);
	return (copyout(&lts, ltp, sizeof(lts)));
}

int
linux_get_timespec(struct timespec *ntp, const struct l_timespec *ultp)
{
	struct l_timespec lts;
	int error;

	error = copyin(ultp, &lts, sizeof(lts));
	if (error != 0)
		return (error);
	return (linux_to_native_timespec(ntp, &lts));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
native_to_linux_timespec64(struct l_timespec64 *ltp64, struct timespec *ntp)
{

	ltp64->tv_sec = ntp->tv_sec;
	ltp64->tv_nsec = ntp->tv_nsec;

	return (0);
}

int
linux_to_native_timespec64(struct timespec *ntp, struct l_timespec64 *ltp64)
{

#if defined(__i386__)
	/* i386 time_t is still 32-bit */
	if (ltp64->tv_sec > INT_MAX || ltp64->tv_sec < INT_MIN)
		return (EOVERFLOW);
#endif
	/* Zero out the padding in compat mode. */
	ntp->tv_nsec = ltp64->tv_nsec & 0xFFFFFFFFUL;
	ntp->tv_sec = ltp64->tv_sec;

	if (!timespecvalid_interval(ntp))
		return (EINVAL);

	return (0);
}

int
linux_put_timespec64(struct timespec *ntp, struct l_timespec64 *ltp)
{
	struct l_timespec64 lts;
	int error;

	error = native_to_linux_timespec64(&lts, ntp);
	if (error != 0)
		return (error);
	return (copyout(&lts, ltp, sizeof(lts)));
}

int
linux_get_timespec64(struct timespec *ntp, const struct l_timespec64 *ultp)
{
	struct l_timespec64 lts;
	int error;

	error = copyin(ultp, &lts, sizeof(lts));
	if (error != 0)
		return (error);
	return (linux_to_native_timespec64(ntp, &lts));
}
#endif

int
native_to_linux_itimerspec(struct l_itimerspec *ltp, struct itimerspec *ntp)
{
	int error;

	error = native_to_linux_timespec(&ltp->it_interval, &ntp->it_interval);
	if (error == 0)
		error = native_to_linux_timespec(&ltp->it_value, &ntp->it_value);
	return (error);
}

int
linux_to_native_itimerspec(struct itimerspec *ntp, struct l_itimerspec *ltp)
{
	int error;

	error = linux_to_native_timespec(&ntp->it_interval, &ltp->it_interval);
	if (error == 0)
		error = linux_to_native_timespec(&ntp->it_value, &ltp->it_value);
	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_to_native_itimerspec64(struct itimerspec *ntp, struct l_itimerspec64 *ltp)
{
	int error;

	error = linux_to_native_timespec64(&ntp->it_interval, &ltp->it_interval);
	if (error == 0)
		error = linux_to_native_timespec64(&ntp->it_value, &ltp->it_value);
	return (error);
}

int
native_to_linux_itimerspec64(struct l_itimerspec64 *ltp, struct itimerspec *ntp)
{
	int error;

	error = native_to_linux_timespec64(&ltp->it_interval, &ntp->it_interval);
	if (error == 0)
		error = native_to_linux_timespec64(&ltp->it_value, &ntp->it_value);
	return (error);
}
#endif

int
linux_to_native_clockid(clockid_t *n, clockid_t l)
{

	if (l < 0) {
		/* cpu-clock */
		if (LINUX_CPUCLOCK_WHICH(l) == LINUX_CLOCKFD) {
			LIN_SDT_PROBE1(time, linux_to_native_clockid,
			    unsupported_clockid, l);
			return (ENOTSUP);
		}
		if ((l & LINUX_CLOCKFD_MASK) == LINUX_CLOCKFD_MASK)
			return (EINVAL);

		if (LINUX_CPUCLOCK_PERTHREAD(l))
			*n = CLOCK_THREAD_CPUTIME_ID;
		else
			*n = CLOCK_PROCESS_CPUTIME_ID;
		return (0);
	}

	switch (l) {
	case LINUX_CLOCK_REALTIME:
		*n = CLOCK_REALTIME;
		break;
	case LINUX_CLOCK_MONOTONIC:
		*n = CLOCK_MONOTONIC;
		break;
	case LINUX_CLOCK_PROCESS_CPUTIME_ID:
		*n = CLOCK_PROCESS_CPUTIME_ID;
		break;
	case LINUX_CLOCK_THREAD_CPUTIME_ID:
		*n = CLOCK_THREAD_CPUTIME_ID;
		break;
	case LINUX_CLOCK_REALTIME_COARSE:
		*n = CLOCK_REALTIME_FAST;
		break;
	case LINUX_CLOCK_MONOTONIC_COARSE:
	case LINUX_CLOCK_MONOTONIC_RAW:
		*n = CLOCK_MONOTONIC_FAST;
		break;
	case LINUX_CLOCK_BOOTTIME:
		*n = CLOCK_UPTIME;
		break;
	case LINUX_CLOCK_REALTIME_ALARM:
	case LINUX_CLOCK_BOOTTIME_ALARM:
	case LINUX_CLOCK_SGI_CYCLE:
	case LINUX_CLOCK_TAI:
		LIN_SDT_PROBE1(time, linux_to_native_clockid,
		    unsupported_clockid, l);
		return (ENOTSUP);
	default:
		LIN_SDT_PROBE1(time, linux_to_native_clockid,
		    unknown_clockid, l);
		return (ENOTSUP);
	}

	return (0);
}

int
linux_to_native_timerflags(int *nflags, int flags)
{

	if (flags & ~LINUX_TIMER_ABSTIME)
		return (EINVAL);
	*nflags = 0;
	if (flags & LINUX_TIMER_ABSTIME)
		*nflags |= TIMER_ABSTIME;
	return (0);
}

static int
linux_common_clock_gettime(struct thread *td, clockid_t which,
    struct timespec *tp)
{
	struct rusage ru;
	struct thread *targettd;
	struct proc *p;
	int error, clockwhich;
	clockid_t nwhich;
	pid_t pid;
	lwpid_t tid;

	error = linux_to_native_clockid(&nwhich, which);
	if (error != 0) {
		linux_msg(curthread,
		    "unsupported clock_gettime clockid %d", which);
		LIN_SDT_PROBE1(time, linux_common_clock_gettime,
		    conversion_error, error);
		return (error);
	}

	switch (nwhich) {
	case CLOCK_PROCESS_CPUTIME_ID:
		if (which < 0) {
			clockwhich = LINUX_CPUCLOCK_WHICH(which);
			pid = LINUX_CPUCLOCK_ID(which);
		} else {
			clockwhich = LINUX_CPUCLOCK_SCHED;
			pid = 0;
		}
		if (pid == 0) {
			p = td->td_proc;
			PROC_LOCK(p);
		} else {
			error = pget(pid, PGET_CANSEE, &p);
			if (error != 0)
				return (EINVAL);
		}
		switch (clockwhich) {
		case LINUX_CPUCLOCK_PROF:
			PROC_STATLOCK(p);
			calcru(p, &ru.ru_utime, &ru.ru_stime);
			PROC_STATUNLOCK(p);
			PROC_UNLOCK(p);
			timevaladd(&ru.ru_utime, &ru.ru_stime);
			TIMEVAL_TO_TIMESPEC(&ru.ru_utime, tp);
			break;
		case LINUX_CPUCLOCK_VIRT:
			PROC_STATLOCK(p);
			calcru(p, &ru.ru_utime, &ru.ru_stime);
			PROC_STATUNLOCK(p);
			PROC_UNLOCK(p);
			TIMEVAL_TO_TIMESPEC(&ru.ru_utime, tp);
			break;
		case LINUX_CPUCLOCK_SCHED:
			kern_process_cputime(p, tp);
			PROC_UNLOCK(p);
			break;
		default:
			PROC_UNLOCK(p);
			return (EINVAL);
		}

		break;

	case CLOCK_THREAD_CPUTIME_ID:
		if (which < 0) {
			clockwhich = LINUX_CPUCLOCK_WHICH(which);
			tid = LINUX_CPUCLOCK_ID(which);
		} else {
			clockwhich = LINUX_CPUCLOCK_SCHED;
			tid = 0;
		}
		p = td->td_proc;
		if (tid == 0) {
			targettd = td;
			PROC_LOCK(p);
		} else {
			targettd = linux_tdfind(td, tid, p->p_pid);
			if (targettd == NULL)
				return (EINVAL);
		}
		switch (clockwhich) {
		case LINUX_CPUCLOCK_PROF:
			PROC_STATLOCK(p);
			thread_lock(targettd);
			rufetchtd(targettd, &ru);
			thread_unlock(targettd);
			PROC_STATUNLOCK(p);
			PROC_UNLOCK(p);
			timevaladd(&ru.ru_utime, &ru.ru_stime);
			TIMEVAL_TO_TIMESPEC(&ru.ru_utime, tp);
			break;
		case LINUX_CPUCLOCK_VIRT:
			PROC_STATLOCK(p);
			thread_lock(targettd);
			rufetchtd(targettd, &ru);
			thread_unlock(targettd);
			PROC_STATUNLOCK(p);
			PROC_UNLOCK(p);
			TIMEVAL_TO_TIMESPEC(&ru.ru_utime, tp);
			break;
		case LINUX_CPUCLOCK_SCHED:
			if (td == targettd)
				targettd = NULL;
			kern_thread_cputime(targettd, tp);
			PROC_UNLOCK(p);
			break;
		default:
			PROC_UNLOCK(p);
			return (EINVAL);
		}
		break;

	default:
		error = kern_clock_gettime(td, nwhich, tp);
		break;
	}

	return (error);
}

int
linux_clock_gettime(struct thread *td, struct linux_clock_gettime_args *args)
{
	struct timespec tp;
	int error;

	error = linux_common_clock_gettime(td, args->which, &tp);
	if (error != 0) {
		LIN_SDT_PROBE1(time, linux_clock_gettime, gettime_error, error);
		return (error);
	}
	error = linux_put_timespec(&tp, args->tp);
	if (error != 0)
		LIN_SDT_PROBE1(time, linux_clock_gettime, copyout_error, error);

	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_clock_gettime64(struct thread *td, struct linux_clock_gettime64_args *args)
{
	struct timespec tp;
	int error;

	error = linux_common_clock_gettime(td, args->which, &tp);
	if (error != 0) {
		LIN_SDT_PROBE1(time, linux_clock_gettime64, gettime_error, error);
		return (error);
	}
	error = linux_put_timespec64(&tp, args->tp);
	if (error != 0)
		LIN_SDT_PROBE1(time, linux_clock_gettime64, copyout_error, error);

	return (error);
}
#endif

static int
linux_common_clock_settime(struct thread *td, clockid_t which,
    struct timespec *ts)
{
	int error;
	clockid_t nwhich;

	error = linux_to_native_clockid(&nwhich, which);
	if (error != 0) {
		linux_msg(curthread,
		    "unsupported clock_settime clockid %d", which);
		LIN_SDT_PROBE1(time, linux_common_clock_settime, conversion_error,
		    error);
		return (error);
	}

	error = kern_clock_settime(td, nwhich, ts);
	if (error != 0)
		LIN_SDT_PROBE1(time, linux_common_clock_settime,
		    settime_error, error);

	return (error);
}

int
linux_clock_settime(struct thread *td, struct linux_clock_settime_args *args)
{
	struct timespec ts;
	int error;

	error = linux_get_timespec(&ts, args->tp);
	if (error != 0) {
		LIN_SDT_PROBE1(time, linux_clock_settime, copyin_error, error);
		return (error);
	}
	return (linux_common_clock_settime(td, args->which, &ts));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_clock_settime64(struct thread *td, struct linux_clock_settime64_args *args)
{
	struct timespec ts;
	int error;

	error = linux_get_timespec64(&ts, args->tp);
	if (error != 0) {
		LIN_SDT_PROBE1(time, linux_clock_settime64, copyin_error, error);
		return (error);
	}
	return (linux_common_clock_settime(td, args->which, &ts));
}
#endif

static int
linux_common_clock_getres(struct thread *td, clockid_t which,
    struct timespec *ts)
{
	struct proc *p;
	int error, clockwhich;
	clockid_t nwhich;
	pid_t pid;
	lwpid_t tid;

	error = linux_to_native_clockid(&nwhich, which);
	if (error != 0) {
		linux_msg(curthread,
		    "unsupported clock_getres clockid %d", which);
		LIN_SDT_PROBE1(time, linux_common_clock_getres,
		    conversion_error, error);
		return (error);
	}

	/*
	 * Check user supplied clock id in case of per-process
	 * or thread-specific cpu-time clock.
	 */
	if (which < 0) {
		switch (nwhich) {
		case CLOCK_THREAD_CPUTIME_ID:
			tid = LINUX_CPUCLOCK_ID(which);
			if (tid != 0) {
				p = td->td_proc;
				if (linux_tdfind(td, tid, p->p_pid) == NULL)
					return (EINVAL);
				PROC_UNLOCK(p);
			}
			break;
		case CLOCK_PROCESS_CPUTIME_ID:
			pid = LINUX_CPUCLOCK_ID(which);
			if (pid != 0) {
				error = pget(pid, PGET_CANSEE, &p);
				if (error != 0)
					return (EINVAL);
				PROC_UNLOCK(p);
			}
			break;
		}
	}

	if (ts == NULL) {
		LIN_SDT_PROBE0(time, linux_common_clock_getres, nullcall);
		return (0);
	}

	switch (nwhich) {
	case CLOCK_THREAD_CPUTIME_ID:
	case CLOCK_PROCESS_CPUTIME_ID:
		clockwhich = LINUX_CPUCLOCK_WHICH(which);
		/*
		 * In both cases (when the clock id obtained by a call to
		 * clock_getcpuclockid() or using the clock
		 * ID CLOCK_PROCESS_CPUTIME_ID Linux hardcodes precision
		 * of clock. The same for the CLOCK_THREAD_CPUTIME_ID clock.
		 *
		 * See Linux posix_cpu_clock_getres() implementation.
		 */
		if (which > 0 || clockwhich == LINUX_CPUCLOCK_SCHED) {
			ts->tv_sec = 0;
			ts->tv_nsec = 1;
			goto out;
		}

		switch (clockwhich) {
		case LINUX_CPUCLOCK_PROF:
			nwhich = CLOCK_PROF;
			break;
		case LINUX_CPUCLOCK_VIRT:
			nwhich = CLOCK_VIRTUAL;
			break;
		default:
			return (EINVAL);
		}
		break;

	default:
		break;
	}
	error = kern_clock_getres(td, nwhich, ts);
	if (error != 0) {
		LIN_SDT_PROBE1(time, linux_common_clock_getres,
		    getres_error, error);
		return (error);
	}

out:
	return (error);
}

int
linux_clock_getres(struct thread *td,
    struct linux_clock_getres_args *args)
{
	struct timespec ts;
	int error;

	error = linux_common_clock_getres(td, args->which, &ts);
	if (error != 0 || args->tp == NULL)
		return (error);
	error = linux_put_timespec(&ts, args->tp);
	if (error != 0)
		LIN_SDT_PROBE1(time, linux_clock_getres,
		    copyout_error, error);
	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_clock_getres_time64(struct thread *td,
    struct linux_clock_getres_time64_args *args)
{
	struct timespec ts;
	int error;

	error = linux_common_clock_getres(td, args->which, &ts);
	if (error != 0 || args->tp == NULL)
		return (error);
	error = linux_put_timespec64(&ts, args->tp);
	if (error != 0)
		LIN_SDT_PROBE1(time, linux_clock_getres_time64,
		    copyout_error, error);
	return (error);
}
#endif

int
linux_nanosleep(struct thread *td, struct linux_nanosleep_args *args)
{
	struct timespec *rmtp;
	struct timespec rqts, rmts;
	int error, error2;

	error = linux_get_timespec(&rqts, args->rqtp);
	if (error != 0) {
		LIN_SDT_PROBE1(time, linux_nanosleep, copyin_error, error);
		return (error);
	}
	if (args->rmtp != NULL)
		rmtp = &rmts;
	else
		rmtp = NULL;

	error = kern_nanosleep(td, &rqts, rmtp);
	if (error == EINTR && args->rmtp != NULL) {
		error2 = linux_put_timespec(rmtp, args->rmtp);
		if (error2 != 0) {
			LIN_SDT_PROBE1(time, linux_nanosleep, copyout_error,
			    error2);
			return (error2);
		}
	}

	return (error);
}

static int
linux_common_clock_nanosleep(struct thread *td, clockid_t which,
    l_int lflags, struct timespec *rqtp, struct timespec *rmtp)
{
	int error, flags;
	clockid_t clockid;

	error = linux_to_native_timerflags(&flags, lflags);
	if (error != 0) {
		LIN_SDT_PROBE1(time, linux_common_clock_nanosleep,
		    unsupported_flags, lflags);
		return (error);
	}

	error = linux_to_native_clockid(&clockid, which);
	if (error != 0) {
		linux_msg(curthread,
		    "unsupported clock_nanosleep clockid %d", which);
		LIN_SDT_PROBE1(time, linux_common_clock_nanosleep,
		    unsupported_clockid, which);
		return (error);
	}
	if (clockid == CLOCK_THREAD_CPUTIME_ID)
		return (ENOTSUP);

	return (kern_clock_nanosleep(td, clockid, flags, rqtp, rmtp));
}

int
linux_clock_nanosleep(struct thread *td,
    struct linux_clock_nanosleep_args *args)
{
	struct timespec *rmtp;
	struct timespec rqts, rmts;
	int error, error2;

	error = linux_get_timespec(&rqts, args->rqtp);
	if (error != 0) {
		LIN_SDT_PROBE1(time, linux_clock_nanosleep, copyin_error,
		    error);
		return (error);
	}
	if (args->rmtp != NULL)
		rmtp = &rmts;
	else
		rmtp = NULL;

	error = linux_common_clock_nanosleep(td, args->which, args->flags,
	    &rqts, rmtp);
	if (error == EINTR && (args->flags & LINUX_TIMER_ABSTIME) == 0 &&
	    args->rmtp != NULL) {
		error2 = linux_put_timespec(rmtp, args->rmtp);
		if (error2 != 0) {
			LIN_SDT_PROBE1(time, linux_clock_nanosleep,
			    copyout_error, error2);
			return (error2);
		}
	}
	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_clock_nanosleep_time64(struct thread *td,
    struct linux_clock_nanosleep_time64_args *args)
{
	struct timespec *rmtp;
	struct timespec rqts, rmts;
	int error, error2;

	error = linux_get_timespec64(&rqts, args->rqtp);
	if (error != 0) {
		LIN_SDT_PROBE1(time, linux_clock_nanosleep_time64,
		    copyin_error, error);
		return (error);
	}
	if (args->rmtp != NULL)
		rmtp = &rmts;
	else
		rmtp = NULL;

	error = linux_common_clock_nanosleep(td, args->which, args->flags,
	    &rqts, rmtp);
	if (error == EINTR && (args->flags & LINUX_TIMER_ABSTIME) == 0 &&
	    args->rmtp != NULL) {
		error2 = linux_put_timespec64(rmtp, args->rmtp);
		if (error2 != 0) {
			LIN_SDT_PROBE1(time, linux_clock_nanosleep_time64,
			    copyout_error, error2);
			return (error2);
		}
	}
	return (error);
}
#endif
