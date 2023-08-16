/*-
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2016, 2017, 2019 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include "namespace.h"
#include <sys/capsicum.h>
#include <sys/elf.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/vdso.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"
#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/pvclock.h>
#include <machine/specialreg.h>
#include <dev/acpica/acpi_hpet.h>
#ifdef WANT_HYPERV
#include <dev/hyperv/hyperv.h>
#endif
#include <x86/ifunc.h>
#include "libc_private.h"

static inline u_int
rdtsc_low(const struct vdso_timehands *th)
{
	u_int rv;

	__asm __volatile("rdtsc; shrd %%cl, %%edx, %0"
	    : "=a" (rv) : "c" (th->th_x86_shift) : "edx");
	return (rv);
}

static inline u_int
rdtscp_low(const struct vdso_timehands *th)
{
	u_int rv;

	__asm __volatile("rdtscp; movl %%edi,%%ecx; shrd %%cl, %%edx, %0"
	    : "=a" (rv) : "D" (th->th_x86_shift) : "ecx", "edx");
	return (rv);
}

static u_int
rdtsc_low_mb_lfence(const struct vdso_timehands *th)
{
	lfence();
	return (rdtsc_low(th));
}

static u_int
rdtsc_low_mb_mfence(const struct vdso_timehands *th)
{
	mfence();
	return (rdtsc_low(th));
}

static u_int
rdtsc_low_mb_none(const struct vdso_timehands *th)
{
	return (rdtsc_low(th));
}

static u_int
rdtsc32_mb_lfence(void)
{
	lfence();
	return (rdtsc32());
}

static uint64_t
rdtsc_mb_lfence(void)
{
	lfence();
	return (rdtsc());
}

static u_int
rdtsc32_mb_mfence(void)
{
	mfence();
	return (rdtsc32());
}

static uint64_t
rdtsc_mb_mfence(void)
{
	mfence();
	return (rdtsc());
}

static u_int
rdtsc32_mb_none(void)
{
	return (rdtsc32());
}

static uint64_t
rdtsc_mb_none(void)
{
	return (rdtsc());
}

static u_int
rdtscp32_(void)
{
	return (rdtscp32());
}

static uint64_t
rdtscp_(void)
{
	return (rdtscp());
}

struct tsc_selector_tag {
	u_int (*ts_rdtsc32)(void);
	uint64_t (*ts_rdtsc)(void);
	u_int (*ts_rdtsc_low)(const struct vdso_timehands *);
};

static const struct tsc_selector_tag tsc_selector[] = {
	[0] = {				/* Intel, LFENCE */
		.ts_rdtsc32 =	rdtsc32_mb_lfence,
		.ts_rdtsc =	rdtsc_mb_lfence,
		.ts_rdtsc_low =	rdtsc_low_mb_lfence,
	},
	[1] = {				/* AMD, MFENCE */
		.ts_rdtsc32 =	rdtsc32_mb_mfence,
		.ts_rdtsc =	rdtsc_mb_mfence,
		.ts_rdtsc_low =	rdtsc_low_mb_mfence,
	},
	[2] = {				/* No SSE2 */
		.ts_rdtsc32 =	rdtsc32_mb_none,
		.ts_rdtsc =	rdtsc_mb_none,
		.ts_rdtsc_low =	rdtsc_low_mb_none,
	},
	[3] = {				/* RDTSCP */
		.ts_rdtsc32 =	rdtscp32_,
		.ts_rdtsc =	rdtscp_,
		.ts_rdtsc_low =	rdtscp_low,
	},
};

static int
tsc_selector_idx(u_int cpu_feature)
{
	u_int amd_feature, cpu_exthigh, p[4], v[3];
	static const char amd_id[] = "AuthenticAMD";
	static const char hygon_id[] = "HygonGenuine";
	bool amd_cpu;

	if (cpu_feature == 0)
		return (2);	/* should not happen due to RDTSC */

	do_cpuid(0, p);
	v[0] = p[1];
	v[1] = p[3];
	v[2] = p[2];
	amd_cpu = memcmp(v, amd_id, sizeof(amd_id) - 1) == 0 ||
	    memcmp(v, hygon_id, sizeof(hygon_id) - 1) == 0;

	if (cpu_feature != 0) {
		do_cpuid(0x80000000, p);
		cpu_exthigh = p[0];
	} else {
		cpu_exthigh = 0;
	}
	if (cpu_exthigh >= 0x80000001) {
		do_cpuid(0x80000001, p);
		amd_feature = p[3];
	} else {
		amd_feature = 0;
	}

	if ((amd_feature & AMDID_RDTSCP) != 0)
		return (3);
	if ((cpu_feature & CPUID_SSE2) == 0)
		return (2);
	return (amd_cpu ? 1 : 0);
}

DEFINE_UIFUNC(static, u_int, __vdso_gettc_rdtsc_low,
    (const struct vdso_timehands *th))
{
	return (tsc_selector[tsc_selector_idx(cpu_feature)].ts_rdtsc_low);
}

DEFINE_UIFUNC(static, u_int, __vdso_gettc_rdtsc32, (void))
{
	return (tsc_selector[tsc_selector_idx(cpu_feature)].ts_rdtsc32);
}

DEFINE_UIFUNC(static, uint64_t, __vdso_gettc_rdtsc, (void))
{
	return (tsc_selector[tsc_selector_idx(cpu_feature)].ts_rdtsc);
}

#define	HPET_DEV_MAP_MAX	10
static volatile char *hpet_dev_map[HPET_DEV_MAP_MAX];

static void
__vdso_init_hpet(uint32_t u)
{
	static const char devprefix[] = "/dev/hpet";
	char devname[64], *c, *c1, t;
	volatile char *new_map, *old_map;
	unsigned int mode;
	uint32_t u1;
	int fd;

	c1 = c = stpcpy(devname, devprefix);
	u1 = u;
	do {
		*c++ = u1 % 10 + '0';
		u1 /= 10;
	} while (u1 != 0);
	*c = '\0';
	for (c--; c1 != c; c1++, c--) {
		t = *c1;
		*c1 = *c;
		*c = t;
	}

	old_map = hpet_dev_map[u];
	if (old_map != NULL)
		return;

	/*
	 * Explicitely check for the capability mode to avoid
	 * triggering trap_enocap on the device open by absolute path.
	 */
	if ((cap_getmode(&mode) == 0 && mode != 0) ||
	    (fd = _open(devname, O_RDONLY | O_CLOEXEC)) == -1) {
		/* Prevent the caller from re-entering. */
		atomic_cmpset_rel_ptr((volatile uintptr_t *)&hpet_dev_map[u],
		    (uintptr_t)old_map, (uintptr_t)MAP_FAILED);
		return;
	}

	new_map = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	_close(fd);
	if (atomic_cmpset_rel_ptr((volatile uintptr_t *)&hpet_dev_map[u],
	    (uintptr_t)old_map, (uintptr_t)new_map) == 0 &&
	    new_map != MAP_FAILED)
		munmap((void *)new_map, PAGE_SIZE);
}

#ifdef WANT_HYPERV

#define HYPERV_REFTSC_DEVPATH	"/dev/" HYPERV_REFTSC_DEVNAME

/*
 * NOTE:
 * We use 'NULL' for this variable to indicate that initialization
 * is required.  And if this variable is 'MAP_FAILED', then Hyper-V
 * reference TSC can not be used, e.g. in misconfigured jail.
 */
static struct hyperv_reftsc *hyperv_ref_tsc;

static void
__vdso_init_hyperv_tsc(void)
{
	int fd;
	unsigned int mode;

	if (cap_getmode(&mode) == 0 && mode != 0)
		goto fail;

	fd = _open(HYPERV_REFTSC_DEVPATH, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		goto fail;
	hyperv_ref_tsc = mmap(NULL, sizeof(*hyperv_ref_tsc), PROT_READ,
	    MAP_SHARED, fd, 0);
	_close(fd);

	return;
fail:
	/* Prevent the caller from re-entering. */
	hyperv_ref_tsc = MAP_FAILED;
}

static int
__vdso_hyperv_tsc(struct hyperv_reftsc *tsc_ref, u_int *tc)
{
	uint64_t disc, ret, tsc, scale;
	uint32_t seq;
	int64_t ofs;

	while ((seq = atomic_load_acq_int(&tsc_ref->tsc_seq)) != 0) {
		scale = tsc_ref->tsc_scale;
		ofs = tsc_ref->tsc_ofs;

		mfence();	/* XXXKIB */
		tsc = rdtsc();

		/* ret = ((tsc * scale) >> 64) + ofs */
		__asm__ __volatile__ ("mulq %3" :
		    "=d" (ret), "=a" (disc) :
		    "a" (tsc), "r" (scale));
		ret += ofs;

		atomic_thread_fence_acq();
		if (tsc_ref->tsc_seq == seq) {
			*tc = ret;
			return (0);
		}

		/* Sequence changed; re-sync. */
	}
	return (ENOSYS);
}

#endif	/* WANT_HYPERV */

static struct pvclock_vcpu_time_info *pvclock_timeinfos;

static int
__vdso_pvclock_gettc(const struct vdso_timehands *th, u_int *tc)
{
	uint64_t delta, ns, tsc;
	struct pvclock_vcpu_time_info *ti;
	uint32_t cpuid_ti, cpuid_tsc, version;
	bool stable;

	do {
		ti = &pvclock_timeinfos[0];
		version = atomic_load_acq_32(&ti->version);
		stable = (ti->flags & th->th_x86_pvc_stable_mask) != 0;
		if (stable) {
			tsc = __vdso_gettc_rdtsc();
		} else {
			(void)rdtscp_aux(&cpuid_ti);
			ti = &pvclock_timeinfos[cpuid_ti];
			version = atomic_load_acq_32(&ti->version);
			tsc = rdtscp_aux(&cpuid_tsc);
		}
		delta = tsc - ti->tsc_timestamp;
		ns = ti->system_time + pvclock_scale_delta(delta,
		    ti->tsc_to_system_mul, ti->tsc_shift);
		atomic_thread_fence_acq();
	} while ((ti->version & 1) != 0 || ti->version != version ||
	    (!stable && cpuid_ti != cpuid_tsc));
	*tc = MAX(ns, th->th_x86_pvc_last_systime);
	return (0);
}

static void
__vdso_init_pvclock_timeinfos(void)
{
	struct pvclock_vcpu_time_info *timeinfos;
	size_t len;
	int fd, ncpus;
	unsigned int mode;

	timeinfos = MAP_FAILED;
	if (_elf_aux_info(AT_NCPUS, &ncpus, sizeof(ncpus)) != 0 ||
	    (cap_getmode(&mode) == 0 && mode != 0) ||
	    (fd = _open("/dev/" PVCLOCK_CDEVNAME, O_RDONLY | O_CLOEXEC)) < 0)
		goto leave;
	len = ncpus * sizeof(*pvclock_timeinfos);
	timeinfos = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	_close(fd);
leave:
	if (atomic_cmpset_rel_ptr(
	    (volatile uintptr_t *)&pvclock_timeinfos, (uintptr_t)NULL,
	    (uintptr_t)timeinfos) == 0 && timeinfos != MAP_FAILED)
		(void)munmap((void *)timeinfos, len);
}

#pragma weak __vdso_gettc
int
__vdso_gettc(const struct vdso_timehands *th, u_int *tc)
{
	volatile char *map;
	uint32_t idx;

	switch (th->th_algo) {
	case VDSO_TH_ALGO_X86_TSC:
		*tc = th->th_x86_shift > 0 ? __vdso_gettc_rdtsc_low(th) :
		    __vdso_gettc_rdtsc32();
		return (0);
	case VDSO_TH_ALGO_X86_HPET:
		idx = th->th_x86_hpet_idx;
		if (idx >= HPET_DEV_MAP_MAX)
			return (ENOSYS);
		map = (volatile char *)atomic_load_acq_ptr(
		    (volatile uintptr_t *)&hpet_dev_map[idx]);
		if (map == NULL) {
			__vdso_init_hpet(idx);
			map = (volatile char *)atomic_load_acq_ptr(
			    (volatile uintptr_t *)&hpet_dev_map[idx]);
		}
		if (map == MAP_FAILED)
			return (ENOSYS);
		*tc = *(volatile uint32_t *)(map + HPET_MAIN_COUNTER);
		return (0);
#ifdef WANT_HYPERV
	case VDSO_TH_ALGO_X86_HVTSC:
		if (hyperv_ref_tsc == NULL)
			__vdso_init_hyperv_tsc();
		if (hyperv_ref_tsc == MAP_FAILED)
			return (ENOSYS);
		return (__vdso_hyperv_tsc(hyperv_ref_tsc, tc));
#endif
	case VDSO_TH_ALGO_X86_PVCLK:
		if (pvclock_timeinfos == NULL)
			__vdso_init_pvclock_timeinfos();
		if (pvclock_timeinfos == MAP_FAILED)
			return (ENOSYS);
		return (__vdso_pvclock_gettc(th, tc));
	default:
		return (ENOSYS);
	}
}

#pragma weak __vdso_gettimekeep
int
__vdso_gettimekeep(struct vdso_timekeep **tk)
{

	return (_elf_aux_info(AT_TIMEKEEP, tk, sizeof(*tk)));
}
