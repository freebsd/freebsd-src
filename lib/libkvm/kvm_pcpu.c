/*-
 * Copyright (c) 2010 Juniper Networks, Inc.
 * Copyright (c) 2009 Robert N. M. Watson
 * Copyright (c) 2009 Bjoern A. Zeeb <bz@FreeBSD.org>
 * Copyright (c) 2008 Yahoo!, Inc.
 * All rights reserved.
 *
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * This software was developed by Robert N. M. Watson under contract
 * to Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/pcpu.h>
#include <sys/sysctl.h>
#include <kvm.h>
#include <limits.h>
#include <stdlib.h>

#include "kvm_private.h"

static struct nlist kvm_pcpu_nl[] = {
	{ "_cpuid_to_pcpu" },
	{ "_mp_maxcpus" },
	{ NULL },
};

/*
 * Kernel per-CPU data state.  We cache this stuff on the first
 * access.	
 *
 * XXXRW: Possibly, this (and kvmpcpu_nl) should be per-kvm_t, in case the
 * consumer has multiple handles in flight to differently configured
 * kernels/crashdumps.
 */
static void **pcpu_data;
static int maxcpu;

#define	NL_CPUID_TO_PCPU	0
#define	NL_MP_MAXCPUS		1

static int
_kvm_pcpu_init(kvm_t *kd)
{
	size_t len;
	int max;
	void *data;

	if (kvm_nlist(kd, kvm_pcpu_nl) < 0)
		return (-1);
	if (kvm_pcpu_nl[NL_CPUID_TO_PCPU].n_value == 0) {
		_kvm_err(kd, kd->program, "unable to find cpuid_to_pcpu");
		return (-1);
	}
	if (kvm_pcpu_nl[NL_MP_MAXCPUS].n_value == 0) {
		_kvm_err(kd, kd->program, "unable to find mp_maxcpus");
		return (-1);
	}
	if (kvm_read(kd, kvm_pcpu_nl[NL_MP_MAXCPUS].n_value, &max,
	    sizeof(max)) != sizeof(max)) {
		_kvm_err(kd, kd->program, "cannot read mp_maxcpus");
		return (-1);
	}
	len = max * sizeof(void *);
	data = malloc(len);
	if (data == NULL) {
		_kvm_err(kd, kd->program, "out of memory");
		return (-1);
	}
	if (kvm_read(kd, kvm_pcpu_nl[NL_CPUID_TO_PCPU].n_value, data, len) !=
	    len) {
		_kvm_err(kd, kd->program, "cannot read cpuid_to_pcpu array");
		free(data);
		return (-1);
	}
	pcpu_data = data;
	maxcpu = max;
	return (0);
}

static void
_kvm_pcpu_clear(void)
{

	maxcpu = 0;
	free(pcpu_data);
	pcpu_data = NULL;
}

void *
kvm_getpcpu(kvm_t *kd, int cpu)
{
	char *buf;
	int i;

	if (kd == NULL) {
		_kvm_pcpu_clear();
		return (NULL);
	}

	if (maxcpu == 0)
		if (_kvm_pcpu_init(kd) < 0)
			return ((void *)-1);

	if (cpu >= maxcpu || pcpu_data[cpu] == NULL)
		return (NULL);

	buf = malloc(sizeof(struct pcpu));
	if (buf == NULL) {
		_kvm_err(kd, kd->program, "out of memory");
		return ((void *)-1);
	}
	if (kvm_read(kd, (uintptr_t)pcpu_data[cpu], buf, sizeof(struct pcpu)) !=
	    sizeof(struct pcpu)) {
		_kvm_err(kd, kd->program, "unable to read per-CPU data");
		free(buf);
		return ((void *)-1);
	}
	return (buf);
}

int
kvm_getmaxcpu(kvm_t *kd)
{

	if (kd == NULL) {
		_kvm_pcpu_clear();
		return (0);
	}

	if (maxcpu == 0)
		if (_kvm_pcpu_init(kd) < 0)
			return (-1);
	return (maxcpu);
}

static int
_kvm_dpcpu_setcpu(kvm_t *kd, u_int cpu, int report_error)
{

	if (!kd->dpcpu_initialized) {
		if (report_error)
			_kvm_err(kd, kd->program, "%s: not initialized",
			    __func__);
		return (-1);
	}
	if (cpu >= kd->dpcpu_maxcpus) {
		if (report_error)
			_kvm_err(kd, kd->program, "%s: CPU %u too big",
			    __func__, cpu);
		return (-1);
	}
	if (kd->dpcpu_off[cpu] == 0) {
		if (report_error)
			_kvm_err(kd, kd->program, "%s: CPU %u not found",
			    __func__, cpu);
		return (-1);
	}
	kd->dpcpu_curcpu = cpu;
	kd->dpcpu_curoff = kd->dpcpu_off[cpu];
	return (0);
}

/*
 * Set up libkvm to handle dynamic per-CPU memory.
 */
static int
_kvm_dpcpu_init(kvm_t *kd)
{
	struct nlist nl[] = {
#define	NLIST_START_SET_PCPU	0
		{ "___start_set_pcpu" },
#define	NLIST_STOP_SET_PCPU	1
		{ "___stop_set_pcpu" },
#define	NLIST_DPCPU_OFF		2
		{ "_dpcpu_off" },
#define	NLIST_MP_MAXCPUS	3
		{ "_mp_maxcpus" },
		{ NULL },
	};
	uintptr_t *dpcpu_off_buf;
	size_t len;
	u_int dpcpu_maxcpus;

	/*
	 * Locate and cache locations of important symbols using the internal
	 * version of _kvm_nlist, turning off initialization to avoid
	 * recursion in case of unresolveable symbols.
	 */
	if (_kvm_nlist(kd, nl, 0) != 0)
		return (-1);
	if (kvm_read(kd, nl[NLIST_MP_MAXCPUS].n_value, &dpcpu_maxcpus,
	    sizeof(dpcpu_maxcpus)) != sizeof(dpcpu_maxcpus))
		return (-1);
	len = dpcpu_maxcpus * sizeof(*dpcpu_off_buf);
	dpcpu_off_buf = malloc(len);
	if (dpcpu_off_buf == NULL)
		return (-1);
	if (kvm_read(kd, nl[NLIST_DPCPU_OFF].n_value, dpcpu_off_buf, len) !=
	    len) {
		free(dpcpu_off_buf);
		return (-1);
	}
	kd->dpcpu_start = nl[NLIST_START_SET_PCPU].n_value;
	kd->dpcpu_stop = nl[NLIST_STOP_SET_PCPU].n_value;
	kd->dpcpu_maxcpus = dpcpu_maxcpus;
	kd->dpcpu_off = dpcpu_off_buf;
	kd->dpcpu_initialized = 1;
	(void)_kvm_dpcpu_setcpu(kd, 0, 0);
	return (0);
}

/*
 * Check whether the dpcpu module has been initialized sucessfully or not,
 * initialize it if permitted.
 */
int
_kvm_dpcpu_initialized(kvm_t *kd, int intialize)
{

	if (kd->dpcpu_initialized || !intialize)
		return (kd->dpcpu_initialized);

	(void)_kvm_dpcpu_init(kd);

	return (kd->dpcpu_initialized);
}

/*
 * Check whether the value is within the dpcpu symbol range and only if so
 * adjust the offset relative to the current offset.
 */
uintptr_t
_kvm_dpcpu_validaddr(kvm_t *kd, uintptr_t value)
{

	if (value == 0)
		return (value);

	if (!kd->dpcpu_initialized)
		return (value);

	if (value < kd->dpcpu_start || value >= kd->dpcpu_stop)
		return (value);

	return (kd->dpcpu_curoff + value);
}

int
kvm_dpcpu_setcpu(kvm_t *kd, u_int cpu)
{
	int ret;

	if (!kd->dpcpu_initialized) {
		ret = _kvm_dpcpu_init(kd);
		if (ret != 0) {
			_kvm_err(kd, kd->program, "%s: init failed",
			    __func__);
			return (ret);
		}
	}

	return (_kvm_dpcpu_setcpu(kd, cpu, 1));
}
