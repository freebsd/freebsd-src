/*-
 * Copyright (c) 2012 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <machine/cpufunc.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>

#include "acpi.h"
#include "inout.h"

/*
 * The ACPI Power Management timer is a free-running 24- or 32-bit
 * timer with a frequency of 3.579545MHz
 *
 * This implementation will be 32-bits
 */

#define PMTMR_FREQ	3579545  /* 3.579545MHz */

static pthread_mutex_t pmtmr_mtx;
static pthread_once_t pmtmr_once = PTHREAD_ONCE_INIT;

static uint64_t	pmtmr_old;

static uint64_t	pmtmr_tscf;
static uint64_t	pmtmr_tsc_old;

static clockid_t clockid = CLOCK_UPTIME_FAST;
static struct timespec pmtmr_uptime_old;

#define	timespecsub(vvp, uvp)						\
	do {								\
		(vvp)->tv_sec -= (uvp)->tv_sec;				\
		(vvp)->tv_nsec -= (uvp)->tv_nsec;			\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)

static uint64_t
timespec_to_pmtmr(const struct timespec *tsnew, const struct timespec *tsold)
{
	struct timespec tsdiff;
	int64_t nsecs;

	tsdiff = *tsnew;
	timespecsub(&tsdiff, tsold);
	nsecs = tsdiff.tv_sec * 1000000000 + tsdiff.tv_nsec;
	assert(nsecs >= 0);

	return (nsecs * PMTMR_FREQ / 1000000000 + pmtmr_old);
}

static uint64_t
tsc_to_pmtmr(uint64_t tsc_new, uint64_t tsc_old)
{

	return ((tsc_new - tsc_old) * PMTMR_FREQ / pmtmr_tscf + pmtmr_old);
}

static void
pmtmr_init(void)
{
	size_t len;
	int smp_tsc, err;
	struct timespec tsnew, tsold = { 0 };

	len = sizeof(smp_tsc);
	err = sysctlbyname("kern.timecounter.smp_tsc", &smp_tsc, &len, NULL, 0);
	assert(err == 0);

	if (smp_tsc) {
		len = sizeof(pmtmr_tscf);
		err = sysctlbyname("machdep.tsc_freq", &pmtmr_tscf, &len,
				   NULL, 0);
		assert(err == 0);

		pmtmr_tsc_old = rdtsc();
		pmtmr_old = tsc_to_pmtmr(pmtmr_tsc_old, 0);
	} else {
		if (getenv("BHYVE_PMTMR_PRECISE") != NULL)
			clockid = CLOCK_UPTIME;

		err = clock_gettime(clockid, &tsnew);
		assert(err == 0);

		pmtmr_uptime_old = tsnew;
		pmtmr_old = timespec_to_pmtmr(&tsnew, &tsold);
	}
	pthread_mutex_init(&pmtmr_mtx, NULL);
}

static uint32_t
pmtmr_val(void)
{
	struct timespec	tsnew;
	uint64_t	pmtmr_tsc_new;
	uint64_t	pmtmr_new;
	int		error;

	pthread_once(&pmtmr_once, pmtmr_init);

	pthread_mutex_lock(&pmtmr_mtx);

	if (pmtmr_tscf) {
		pmtmr_tsc_new = rdtsc();
		pmtmr_new = tsc_to_pmtmr(pmtmr_tsc_new, pmtmr_tsc_old);
		pmtmr_tsc_old = pmtmr_tsc_new;
	} else {
		error = clock_gettime(clockid, &tsnew);
		assert(error == 0);

		pmtmr_new = timespec_to_pmtmr(&tsnew, &pmtmr_uptime_old);
		pmtmr_uptime_old = tsnew;
	}
	pmtmr_old = pmtmr_new;

	pthread_mutex_unlock(&pmtmr_mtx);

	return (pmtmr_new); 
}

static int
pmtmr_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	          uint32_t *eax, void *arg)
{
	assert(in == 1);

	if (bytes != 4)
		return (-1);

	*eax = pmtmr_val();

	return (0);
}

INOUT_PORT(pmtmr, IO_PMTMR, IOPORT_F_IN, pmtmr_handler);
