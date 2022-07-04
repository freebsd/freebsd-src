/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2021 Dmitry Chagin <dchagin@FreeBSD.org>
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

static int
fls(int mask)
{

	if (mask == 0)
		return (0);
	return ((__builtin_clz(mask) ^ 0x1f) + 1);
}

#ifdef _LP64
static int
flsl(long mask)
{
	int bit;

	if (mask == 0)
		return (0);
	for (bit = 1; mask != 1; bit++)
		mask = (unsigned long)mask >> 1;
	return (bit);
}
#else
static int
flsll(long long mask)
{
	int bit;

	if (mask == 0)
		return (0);
	for (bit = 1; mask != 1; bit++)
		mask = (unsigned long long)mask >> 1;
	return (bit);
}
#endif

static int
__vdso_native_to_linux_timespec(struct l_timespec *lts,
    struct timespec *nts)
{

#ifdef COMPAT_LINUX32
	if (nts->tv_sec > INT_MAX || nts->tv_sec < INT_MIN)
		return (LINUX_EOVERFLOW);
#endif
	lts->tv_sec = nts->tv_sec;
	lts->tv_nsec = nts->tv_nsec;
	return (0);
}

static int
__vdso_native_to_linux_timeval(l_timeval *ltv,
    struct timeval *ntv)
{

#ifdef COMPAT_LINUX32
	if (ntv->tv_sec > INT_MAX || ntv->tv_sec < INT_MIN)
		return (LINUX_EOVERFLOW);
#endif
	ltv->tv_sec = ntv->tv_sec;
	ltv->tv_usec = ntv->tv_usec;
	return (0);
}


#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
static int
__vdso_native_to_linux_timespec64(struct l_timespec64 *lts,
    struct timespec *nts)
{

	lts->tv_sec = nts->tv_sec;
	lts->tv_nsec = nts->tv_nsec;
	return (0);
}
#endif

static int
__vdso_linux_to_native_clockid(clockid_t *n, clockid_t l)
{

	switch (l) {
	case LINUX_CLOCK_REALTIME:
		*n = CLOCK_REALTIME;
		break;
	case LINUX_CLOCK_MONOTONIC:
		*n = CLOCK_MONOTONIC;
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
	default:
		return (LINUX_EINVAL);
	}
	return (0);
}

/*
 * The code below adapted from
 * lib/libc/sys/__vdso_gettimeofday.c
 */

static inline void
__vdso_gettimekeep(struct vdso_timekeep **tk)
{

	*tk = (struct vdso_timekeep *)kern_timekeep_base;
}

static int
tc_delta(const struct vdso_timehands *th, u_int *delta)
{
	int error;
	u_int tc;

	error = __vdso_gettc(th, &tc);
	if (error == 0)
		*delta = (tc - th->th_offset_count) & th->th_counter_mask;
	return (error);
}

/*
 * Calculate the absolute or boot-relative time from the
 * machine-specific fast timecounter and the published timehands
 * structure read from the shared page.
 *
 * The lockless reading scheme is similar to the one used to read the
 * in-kernel timehands, see sys/kern/kern_tc.c:binuptime().  This code
 * is based on the kernel implementation.
 */
static int
freebsd_binuptime(struct bintime *bt, struct vdso_timekeep *tk, bool abs)
{
	struct vdso_timehands *th;
	uint32_t curr, gen;
	uint64_t scale, x;
	u_int delta, scale_bits;
	int error;

	do {
		if (!tk->tk_enabled)
			return (ENOSYS);

		curr = atomic_load_acq_32(&tk->tk_current);
		th = &tk->tk_th[curr];
		gen = atomic_load_acq_32(&th->th_gen);
		*bt = th->th_offset;
		error = tc_delta(th, &delta);
		if (error == EAGAIN)
			continue;
		if (error != 0)
			return (error);
		scale = th->th_scale;
#ifdef _LP64
		scale_bits = flsl(scale);
#else
		scale_bits = flsll(scale);
#endif
		if (__predict_false(scale_bits + fls(delta) > 63)) {
			x = (scale >> 32) * delta;
			scale &= 0xffffffff;
			bt->sec += x >> 32;
			bintime_addx(bt, x << 32);
		}
		bintime_addx(bt, scale * delta);
		if (abs)
			bintime_add(bt, &th->th_boottime);

		/*
		 * Ensure that the load of th_offset is completed
		 * before the load of th_gen.
		 */
		atomic_thread_fence_acq();
	} while (curr != tk->tk_current || gen == 0 || gen != th->th_gen);
	return (0);
}

static int
freebsd_getnanouptime(struct bintime *bt, struct vdso_timekeep *tk)
{
	struct vdso_timehands *th;
	uint32_t curr, gen;

	do {
		if (!tk->tk_enabled)
			return (ENOSYS);

		curr = atomic_load_acq_32(&tk->tk_current);
		th = &tk->tk_th[curr];
		gen = atomic_load_acq_32(&th->th_gen);
		*bt = th->th_offset;

		/*
		 * Ensure that the load of th_offset is completed
		 * before the load of th_gen.
		 */
		atomic_thread_fence_acq();
	} while (curr != tk->tk_current || gen == 0 || gen != th->th_gen);
	return (0);
}

static int
freebsd_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	struct vdso_timekeep *tk;
	struct bintime bt;
	int error;

	if (tz != NULL)
		return (ENOSYS);
	__vdso_gettimekeep(&tk);
	if (tk == NULL)
		return (ENOSYS);
	if (tk->tk_ver != VDSO_TK_VER_CURR)
		return (ENOSYS);
	error = freebsd_binuptime(&bt, tk, true);
	if (error == 0)
		bintime2timeval(&bt, tv);
	return (error);
}

static int
freebsd_clock_gettime(clockid_t clock_id, struct timespec *ts)
{
	struct vdso_timekeep *tk;
	struct bintime bt;
	int error;

	__vdso_gettimekeep(&tk);
	if (tk == NULL)
		return (ENOSYS);
	if (tk->tk_ver != VDSO_TK_VER_CURR)
		return (ENOSYS);
	switch (clock_id) {
	case CLOCK_REALTIME:
	case CLOCK_REALTIME_PRECISE:
	case CLOCK_REALTIME_FAST:
		error = freebsd_binuptime(&bt, tk, true);
		break;
	case CLOCK_MONOTONIC:
	case CLOCK_MONOTONIC_PRECISE:
	case CLOCK_UPTIME:
	case CLOCK_UPTIME_PRECISE:
		error = freebsd_binuptime(&bt, tk, false);
		break;
	case CLOCK_MONOTONIC_FAST:
	case CLOCK_UPTIME_FAST:
		error = freebsd_getnanouptime(&bt, tk);
		break;
	default:
		error = ENOSYS;
		break;
	}
	if (error == 0)
		bintime2timespec(&bt, ts);
	return (error);
}

/*
 * Linux vDSO interfaces
 *
 */
int
__vdso_clock_gettime(clockid_t clock_id, struct l_timespec *lts)
{
	struct timespec ts;
	clockid_t which;
	int error;

	error = __vdso_linux_to_native_clockid(&which, clock_id);
	if (error != 0)
		return (__vdso_clock_gettime_fallback(clock_id, lts));
	error = freebsd_clock_gettime(which, &ts);
	if (error == 0)
		return (-__vdso_native_to_linux_timespec(lts, &ts));
	else
		return (__vdso_clock_gettime_fallback(clock_id, lts));
}

int
__vdso_gettimeofday(l_timeval *ltv, struct timezone *tz)
{
	struct timeval tv;
	int error;

	error = freebsd_gettimeofday(&tv, tz);
	if (error != 0)
		return (__vdso_gettimeofday_fallback(ltv, tz));
	return (-__vdso_native_to_linux_timeval(ltv, &tv));
}

int
__vdso_clock_getres(clockid_t clock_id, struct l_timespec *lts)
{

	return (__vdso_clock_getres_fallback(clock_id, lts));
}

#if defined(__i386__) || defined(COMPAT_LINUX32)
int
__vdso_clock_gettime64(clockid_t clock_id, struct l_timespec64 *lts)
{
	struct timespec ts;
	clockid_t which;
	int error;

	error = __vdso_linux_to_native_clockid(&which, clock_id);
	if (error != 0)
		return (__vdso_clock_gettime64_fallback(clock_id, lts));
	error = freebsd_clock_gettime(which, &ts);
	if (error == 0)
		return(-__vdso_native_to_linux_timespec64(lts, &ts));
	else
		return(__vdso_clock_gettime64_fallback(clock_id, lts));
}

int clock_gettime64(clockid_t clock_id, struct l_timespec64 *lts)
    __attribute__((weak, alias("__vdso_clock_gettime64")));
#endif

#if defined(__i386__) || defined(__amd64__)
int
__vdso_getcpu(uint32_t *cpu, uint32_t *node, void *cache)
{
	int ret;

	if (node != NULL)
		return (__vdso_getcpu_fallback(cpu, node, cache));
	ret = __vdso_getcpu_try();
	if (ret < 0)
		return (__vdso_getcpu_fallback(cpu, node, cache));
	*cpu = ret;
	return (0);
}
#endif

#if defined(__i386__) || defined(__amd64__)
int
__vdso_time(long *tm)
{
	struct timeval tv;
	int error;

	error = freebsd_gettimeofday(&tv, NULL);
	if (error != 0)
		return (__vdso_time_fallback(tm));
	if (tm != NULL)
		*tm = tv.tv_sec;
	return (tv.tv_sec);
}
#endif
