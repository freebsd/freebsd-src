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
#include <time.h>
#include <assert.h>
#include <pthread.h>

#include "inout.h"

/*
 * The ACPI Power Management timer is a free-running 24- or 32-bit
 * timer with a frequency of 3.579545MHz
 *
 * This implementation will be 32-bits
 */

#define	IO_PMTMR	0x408	/* 4-byte i/o port for the timer */

#define PMTMR_FREQ	3579545  /* 3.579545MHz */

static pthread_mutex_t pmtmr_mtx;
static uint64_t	pmtmr_tscf;
static uint64_t	pmtmr_old;
static uint64_t	pmtmr_tsc_old;

static uint32_t
pmtmr_val(void)
{
	uint64_t	pmtmr_tsc_new;
	uint64_t	pmtmr_new;
	static int	inited = 0;

	if (!inited) {
		size_t len;

		inited = 1;
		pthread_mutex_init(&pmtmr_mtx, NULL);
		len = sizeof(pmtmr_tscf);
		sysctlbyname("machdep.tsc_freq", &pmtmr_tscf, &len,
		    NULL, 0);
		pmtmr_tsc_old = rdtsc();
		pmtmr_old = pmtmr_tsc_old / pmtmr_tscf * PMTMR_FREQ;
	}

	pthread_mutex_lock(&pmtmr_mtx);
	pmtmr_tsc_new = rdtsc();
	pmtmr_new = (pmtmr_tsc_new - pmtmr_tsc_old) * PMTMR_FREQ / pmtmr_tscf +
	    pmtmr_old;
	pmtmr_old = pmtmr_new;
	pmtmr_tsc_old = pmtmr_tsc_new;
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

