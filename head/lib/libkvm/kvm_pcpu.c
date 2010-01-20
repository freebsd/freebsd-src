/*-
 * Copyright (c) 2008 Yahoo!, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
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
