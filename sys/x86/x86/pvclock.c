/*-
 * Copyright (c) 2009 Adrian Chadd
 * Copyright (c) 2012 Spectra Logic Corporation
 * Copyright (c) 2014 Bryan Venteicher
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/limits.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/vdso.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/pvclock.h>

/*
 * Last system time. This is used to guarantee a monotonically non-decreasing
 * clock for the kernel codepath and approximate the same for the vDSO codepath.
 * In theory, this should be unnecessary absent hypervisor bug(s) and/or what
 * should be rare cases where TSC jitter may still be visible despite the
 * hypervisor's best efforts.
 */
static volatile uint64_t pvclock_last_systime;

static uint64_t		 pvclock_getsystime(struct pvclock *pvc);
static void		 pvclock_read_time_info(
    struct pvclock_vcpu_time_info *ti, uint64_t *ns, uint8_t *flags);
static void		 pvclock_read_wall_clock(struct pvclock_wall_clock *wc,
    struct timespec *ts);
static u_int		 pvclock_tc_get_timecount(struct timecounter *tc);
static uint32_t		 pvclock_tc_vdso_timehands(
    struct vdso_timehands *vdso_th, struct timecounter *tc);
#ifdef COMPAT_FREEBSD32
static uint32_t		 pvclock_tc_vdso_timehands32(
    struct vdso_timehands32 *vdso_th, struct timecounter *tc);
#endif

static d_open_t		 pvclock_cdev_open;
static d_mmap_t		 pvclock_cdev_mmap;

static struct cdevsw	 pvclock_cdev_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	PVCLOCK_CDEVNAME,
	.d_open =	pvclock_cdev_open,
	.d_mmap =	pvclock_cdev_mmap,
};

void
pvclock_resume(void)
{
	atomic_store_rel_64(&pvclock_last_systime, 0);
}

uint64_t
pvclock_tsc_freq(struct pvclock_vcpu_time_info *ti)
{
	uint64_t freq;

	freq = (1000000000ULL << 32) / ti->tsc_to_system_mul;
	if (ti->tsc_shift < 0)
		freq <<= -ti->tsc_shift;
	else
		freq >>= ti->tsc_shift;
	return (freq);
}

static void
pvclock_read_time_info(struct pvclock_vcpu_time_info *ti,
    uint64_t *ns, uint8_t *flags)
{
	uint64_t delta;
	uint32_t version;

	do {
		version = atomic_load_acq_32(&ti->version);
		delta = rdtsc_ordered() - ti->tsc_timestamp;
		*ns = ti->system_time + pvclock_scale_delta(delta,
		    ti->tsc_to_system_mul, ti->tsc_shift);
		*flags = ti->flags;
		atomic_thread_fence_acq();
	} while ((ti->version & 1) != 0 || ti->version != version);
}

static void
pvclock_read_wall_clock(struct pvclock_wall_clock *wc, struct timespec *ts)
{
	uint32_t version;

	do {
		version = atomic_load_acq_32(&wc->version);
		ts->tv_sec = wc->sec;
		ts->tv_nsec = wc->nsec;
		atomic_thread_fence_acq();
	} while ((wc->version & 1) != 0 || wc->version != version);
}

static uint64_t
pvclock_getsystime(struct pvclock *pvc)
{
	uint64_t now, last, ret;
	uint8_t flags;

	critical_enter();
	pvclock_read_time_info(&pvc->timeinfos[curcpu], &now, &flags);
	ret = now;
	if ((flags & PVCLOCK_FLAG_TSC_STABLE) == 0) {
		last = atomic_load_acq_64(&pvclock_last_systime);
		do {
			if (last > now) {
				ret = last;
				break;
			}
		} while (!atomic_fcmpset_rel_64(&pvclock_last_systime, &last,
		    now));
	}
	critical_exit();
	return (ret);
}

/*
 * NOTE: Transitional-only; this should be removed after 'dev/xen/timer/timer.c'
 * has been migrated to the 'struct pvclock' API.
 */
uint64_t
pvclock_get_timecount(struct pvclock_vcpu_time_info *ti)
{
	uint64_t now, last, ret;
	uint8_t flags;

	pvclock_read_time_info(ti, &now, &flags);
	ret = now;
	if ((flags & PVCLOCK_FLAG_TSC_STABLE) == 0) {
		last = atomic_load_acq_64(&pvclock_last_systime);
		do {
			if (last > now) {
				ret = last;
				break;
			}
		} while (!atomic_fcmpset_rel_64(&pvclock_last_systime, &last,
		    now));
	}
	return (ret);
}

/*
 * NOTE: Transitional-only; this should be removed after 'dev/xen/timer/timer.c'
 * has been migrated to the 'struct pvclock' API.
 */
void
pvclock_get_wallclock(struct pvclock_wall_clock *wc, struct timespec *ts)
{
	pvclock_read_wall_clock(wc, ts);
}

static int
pvclock_cdev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	if (oflags & FWRITE)
		return (EPERM);
	return (0);
}

static int
pvclock_cdev_mmap(struct cdev *dev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int nprot, vm_memattr_t *memattr)
{
	if (offset >= mp_ncpus * sizeof(struct pvclock_vcpu_time_info))
		return (EINVAL);
	if (PROT_EXTRACT(nprot) != PROT_READ)
		return (EACCES);
	*paddr = vtophys((uintptr_t)dev->si_drv1 + offset);
	*memattr = VM_MEMATTR_DEFAULT;
	return (0);
}

static u_int
pvclock_tc_get_timecount(struct timecounter *tc)
{
	struct pvclock *pvc = tc->tc_priv;

	return (pvclock_getsystime(pvc) & UINT_MAX);
}

static uint32_t
pvclock_tc_vdso_timehands(struct vdso_timehands *vdso_th,
    struct timecounter *tc)
{
	struct pvclock *pvc = tc->tc_priv;

	if (pvc->cdev == NULL)
		return (0);

	vdso_th->th_algo = VDSO_TH_ALGO_X86_PVCLK;
	vdso_th->th_x86_shift = 0;
	vdso_th->th_x86_hpet_idx = 0;
	vdso_th->th_x86_pvc_last_systime =
	    atomic_load_acq_64(&pvclock_last_systime);
	vdso_th->th_x86_pvc_stable_mask = !pvc->vdso_force_unstable &&
	    pvc->stable_flag_supported ? PVCLOCK_FLAG_TSC_STABLE : 0;
	bzero(vdso_th->th_res, sizeof(vdso_th->th_res));
	return ((amd_feature & AMDID_RDTSCP) != 0 ||
	    ((vdso_th->th_x86_pvc_stable_mask & PVCLOCK_FLAG_TSC_STABLE) != 0 &&
	    pvc->vdso_enable_without_rdtscp));
}

#ifdef COMPAT_FREEBSD32
static uint32_t
pvclock_tc_vdso_timehands32(struct vdso_timehands32 *vdso_th,
    struct timecounter *tc)
{
	struct pvclock *pvc = tc->tc_priv;

	if (pvc->cdev == NULL)
		return (0);

	vdso_th->th_algo = VDSO_TH_ALGO_X86_PVCLK;
	vdso_th->th_x86_shift = 0;
	vdso_th->th_x86_hpet_idx = 0;
	*(uint64_t *)&vdso_th->th_x86_pvc_last_systime[0] =
	    atomic_load_acq_64(&pvclock_last_systime);
	vdso_th->th_x86_pvc_stable_mask = !pvc->vdso_force_unstable &&
	    pvc->stable_flag_supported ? PVCLOCK_FLAG_TSC_STABLE : 0;
	bzero(vdso_th->th_res, sizeof(vdso_th->th_res));
	return ((amd_feature & AMDID_RDTSCP) != 0 ||
	    ((vdso_th->th_x86_pvc_stable_mask & PVCLOCK_FLAG_TSC_STABLE) != 0 &&
	    pvc->vdso_enable_without_rdtscp));
}
#endif

void
pvclock_gettime(struct pvclock *pvc, struct timespec *ts)
{
	struct timespec system_ts;
	uint64_t system_ns;

	pvclock_read_wall_clock(pvc->get_wallclock(pvc->get_wallclock_arg), ts);
	system_ns = pvclock_getsystime(pvc);
	system_ts.tv_sec = system_ns / 1000000000ULL;
	system_ts.tv_nsec = system_ns % 1000000000ULL;
	timespecadd(ts, &system_ts, ts);
}

void
pvclock_init(struct pvclock *pvc, device_t dev, const char *tc_name,
    int tc_quality, u_int tc_flags)
{
	struct make_dev_args mda;
	int err;

	KASSERT(((uintptr_t)pvc->timeinfos & PAGE_MASK) == 0,
	    ("Specified time info page(s) address is not page-aligned."));

	/* Set up vDSO stable-flag suppression test facility: */
	pvc->vdso_force_unstable = false;
	SYSCTL_ADD_BOOL(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "vdso_force_unstable", CTLFLAG_RW, &pvc->vdso_force_unstable, 0,
	    "Forcibly deassert stable flag in vDSO codepath");

	/*
	 * Make it possible to use the vDSO page even when the hypervisor does
	 * not support the rdtscp instruction.  This is disabled by default for
	 * compatibility with old libc.
	 */
	pvc->vdso_enable_without_rdtscp = false;
	SYSCTL_ADD_BOOL(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "vdso_enable_without_rdtscp", CTLFLAG_RWTUN,
	    &pvc->vdso_enable_without_rdtscp, 0,
	    "Allow the use of a vDSO when rdtscp is not available");

	/* Set up timecounter and timecounter-supporting members: */
	pvc->tc.tc_get_timecount = pvclock_tc_get_timecount;
	pvc->tc.tc_poll_pps = NULL;
	pvc->tc.tc_counter_mask = ~0U;
	pvc->tc.tc_frequency = 1000000000ULL;
	pvc->tc.tc_name = tc_name;
	pvc->tc.tc_quality = tc_quality;
	pvc->tc.tc_flags = tc_flags;
	pvc->tc.tc_priv = pvc;
	pvc->tc.tc_fill_vdso_timehands = pvclock_tc_vdso_timehands;
#ifdef COMPAT_FREEBSD32
	pvc->tc.tc_fill_vdso_timehands32 = pvclock_tc_vdso_timehands32;
#endif

	/* Set up cdev for userspace mmapping of vCPU 0 time info page: */
	make_dev_args_init(&mda);
	mda.mda_devsw = &pvclock_cdev_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_WHEEL;
	mda.mda_mode = 0444;
	mda.mda_si_drv1 = pvc->timeinfos;
	err = make_dev_s(&mda, &pvc->cdev, PVCLOCK_CDEVNAME);
	if (err != 0) {
		device_printf(dev, "Could not create /dev/%s, error %d. Fast "
		    "time of day will be unavailable for this timecounter.\n",
		    PVCLOCK_CDEVNAME, err);
		KASSERT(pvc->cdev == NULL,
		    ("Failed make_dev_s() unexpectedly inited cdev."));
	}

	/* Register timecounter: */
	tc_init(&pvc->tc);

	/*
	 * Register wallclock:
	 *     The RTC registration API expects a resolution in microseconds;
	 *     pvclock's 1ns resolution is rounded up to 1us.
	 */
	clock_register(dev, 1);
}

int
pvclock_destroy(struct pvclock *pvc)
{
	/*
	 * Not currently possible since there is no teardown counterpart of
	 * 'tc_init()'.
	 */
	return (EBUSY);
}
