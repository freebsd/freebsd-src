/*-
 * Copyright (c) 2006-2008 Stanislav Sedov <stas@FreeBSD.org>
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sched.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/pmckern.h>
#include <sys/cpuctl.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

static d_open_t cpuctl_open;
static d_ioctl_t cpuctl_ioctl;

#define	CPUCTL_VERSION 1

#ifdef DEBUG
# define	DPRINTF(format,...) printf(format, __VA_ARGS__);
#else
# define	DPRINTF(...)
#endif

#define	UCODE_SIZE_MAX	(16 * 1024)

static int cpuctl_do_msr(int cpu, cpuctl_msr_args_t *data, u_long cmd,
    struct thread *td);
static int cpuctl_do_cpuid(int cpu, cpuctl_cpuid_args_t *data,
    struct thread *td);
static int cpuctl_do_update(int cpu, cpuctl_update_args_t *data,
    struct thread *td);
static int update_intel(int cpu, cpuctl_update_args_t *args,
    struct thread *td);
static int update_amd(int cpu, cpuctl_update_args_t *args, struct thread *td);
static int update_via(int cpu, cpuctl_update_args_t *args,
    struct thread *td);

static struct cdev **cpuctl_devs;
static MALLOC_DEFINE(M_CPUCTL, "cpuctl", "CPUCTL buffer");

static struct cdevsw cpuctl_cdevsw = {
        .d_version =    D_VERSION,
        .d_open =       cpuctl_open,
        .d_ioctl =      cpuctl_ioctl,
        .d_name =       "cpuctl",
};

/*
 * This function checks if specified cpu enabled or not.
 */
static int
cpu_enabled(int cpu)
{

	return (pmc_cpu_is_disabled(cpu) == 0);
}

/*
 * Check if the current thread is bound to a specific cpu.
 */
static int
cpu_sched_is_bound(struct thread *td)
{
	int ret;

	thread_lock(td);
	ret = sched_is_bound(td);
	thread_unlock(td);
	return (ret);
}

/*
 * Switch to target cpu to run.
 */
static void
set_cpu(int cpu, struct thread *td)
{

	KASSERT(cpu >= 0 && cpu < mp_ncpus && cpu_enabled(cpu),
	    ("[cpuctl,%d]: bad cpu number %d", __LINE__, cpu));
	thread_lock(td);
	sched_bind(td, cpu);
	thread_unlock(td);
	KASSERT(td->td_oncpu == cpu,
	    ("[cpuctl,%d]: cannot bind to target cpu %d", __LINE__, cpu));
}

static void
restore_cpu(int oldcpu, int is_bound, struct thread *td)
{

	KASSERT(oldcpu >= 0 && oldcpu < mp_ncpus && cpu_enabled(oldcpu),
	    ("[cpuctl,%d]: bad cpu number %d", __LINE__, oldcpu));
	thread_lock(td);
	if (is_bound == 0)
		sched_unbind(td);
	else
		sched_bind(td, oldcpu);
	thread_unlock(td);
}

int
cpuctl_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
	int flags, struct thread *td)
{
	int ret;
	int cpu = dev2unit(dev);

	if (cpu >= mp_ncpus || !cpu_enabled(cpu)) {
		DPRINTF("[cpuctl,%d]: bad cpu number %d\n", __LINE__, cpu);
		return (ENXIO);
	}
	/* Require write flag for "write" requests. */
	if ((cmd == CPUCTL_WRMSR || cmd == CPUCTL_UPDATE) &&
	    ((flags & FWRITE) == 0))
		return (EPERM);
	switch (cmd) {
	case CPUCTL_RDMSR:
		ret = cpuctl_do_msr(cpu, (cpuctl_msr_args_t *)data, cmd, td);
		break;
	case CPUCTL_MSRSBIT:
	case CPUCTL_MSRCBIT:
	case CPUCTL_WRMSR:
		ret = priv_check(td, PRIV_CPUCTL_WRMSR);
		if (ret != 0)
			goto fail;
		ret = cpuctl_do_msr(cpu, (cpuctl_msr_args_t *)data, cmd, td);
		break;
	case CPUCTL_CPUID:
		ret = cpuctl_do_cpuid(cpu, (cpuctl_cpuid_args_t *)data, td);
		break;
	case CPUCTL_UPDATE:
		ret = priv_check(td, PRIV_CPUCTL_UPDATE);
		if (ret != 0)
			goto fail;
		ret = cpuctl_do_update(cpu, (cpuctl_update_args_t *)data, td);
		break;
	default:
		ret = EINVAL;
		break;
	}
fail:
	return (ret);
}

/*
 * Actually perform cpuid operation.
 */
static int
cpuctl_do_cpuid(int cpu, cpuctl_cpuid_args_t *data, struct thread *td)
{
	int is_bound = 0;
	int oldcpu;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[cpuctl,%d]: bad cpu number %d", __LINE__, cpu));

	/* Explicitly clear cpuid data to avoid returning stale info. */
	bzero(data->data, sizeof(data->data));
	DPRINTF("[cpuctl,%d]: retriving cpuid level %#0x for %d cpu\n",
	    __LINE__, data->level, cpu);
	oldcpu = td->td_oncpu;
	is_bound = cpu_sched_is_bound(td);
	set_cpu(cpu, td);
	cpuid_count(data->level, 0, data->data);
	restore_cpu(oldcpu, is_bound, td);
	return (0);
}

/*
 * Actually perform MSR operations.
 */
static int
cpuctl_do_msr(int cpu, cpuctl_msr_args_t *data, u_long cmd, struct thread *td)
{
	uint64_t reg;
	int is_bound = 0;
	int oldcpu;
	int ret;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[cpuctl,%d]: bad cpu number %d", __LINE__, cpu));

	/*
	 * Explicitly clear cpuid data to avoid returning stale
	 * info
	 */
	DPRINTF("[cpuctl,%d]: operating on MSR %#0x for %d cpu\n", __LINE__,
	    data->msr, cpu);
	oldcpu = td->td_oncpu;
	is_bound = cpu_sched_is_bound(td);
	set_cpu(cpu, td);
	if (cmd == CPUCTL_RDMSR) {
		data->data = 0;
		ret = rdmsr_safe(data->msr, &data->data);
	} else if (cmd == CPUCTL_WRMSR) {
		ret = wrmsr_safe(data->msr, data->data);
	} else if (cmd == CPUCTL_MSRSBIT) {
		critical_enter();
		ret = rdmsr_safe(data->msr, &reg);
		if (ret == 0)
			ret = wrmsr_safe(data->msr, reg | data->data);
		critical_exit();
	} else if (cmd == CPUCTL_MSRCBIT) {
		critical_enter();
		ret = rdmsr_safe(data->msr, &reg);
		if (ret == 0)
			ret = wrmsr_safe(data->msr, reg & ~data->data);
		critical_exit();
	} else
		panic("[cpuctl,%d]: unknown operation requested: %lu", __LINE__, cmd);
	restore_cpu(oldcpu, is_bound, td);
	return (ret);
}

/*
 * Actually perform microcode update.
 */
static int
cpuctl_do_update(int cpu, cpuctl_update_args_t *data, struct thread *td)
{
	cpuctl_cpuid_args_t args = {
		.level = 0,
	};
	char vendor[13];
	int ret;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[cpuctl,%d]: bad cpu number %d", __LINE__, cpu));
	DPRINTF("[cpuctl,%d]: XXX %d", __LINE__, cpu);

	ret = cpuctl_do_cpuid(cpu, &args, td);
	if (ret != 0) {
		DPRINTF("[cpuctl,%d]: cannot retrive cpuid info for cpu %d",
		    __LINE__, cpu);
		return (ENXIO);
	}
	((uint32_t *)vendor)[0] = args.data[1];
	((uint32_t *)vendor)[1] = args.data[3];
	((uint32_t *)vendor)[2] = args.data[2];
	vendor[12] = '\0';
	if (strncmp(vendor, INTEL_VENDOR_ID, sizeof(INTEL_VENDOR_ID)) == 0)
		ret = update_intel(cpu, data, td);
	else if(strncmp(vendor, AMD_VENDOR_ID, sizeof(AMD_VENDOR_ID)) == 0)
		ret = update_amd(cpu, data, td);
	else if(strncmp(vendor, CENTAUR_VENDOR_ID, sizeof(CENTAUR_VENDOR_ID)) == 0)
		ret = update_via(cpu, data, td);
	else
		ret = ENXIO;
	return (ret);
}

static int
update_intel(int cpu, cpuctl_update_args_t *args, struct thread *td)
{
	void *ptr;
	uint64_t rev0, rev1;
	uint32_t tmp[4];
	int is_bound;
	int oldcpu;
	int ret;

	if (args->size == 0 || args->data == NULL) {
		DPRINTF("[cpuctl,%d]: zero-sized firmware image", __LINE__);
		return (EINVAL);
	}
	if (args->size > UCODE_SIZE_MAX) {
		DPRINTF("[cpuctl,%d]: firmware image too large", __LINE__);
		return (EINVAL);
	}

	/*
	 * 16 byte alignment required.  Rely on the fact that
	 * malloc(9) always returns the pointer aligned at least on
	 * the size of the allocation.
	 */
	ptr = malloc(args->size + 16, M_CPUCTL, M_WAITOK);
	if (copyin(args->data, ptr, args->size) != 0) {
		DPRINTF("[cpuctl,%d]: copyin %p->%p of %zd bytes failed",
		    __LINE__, args->data, ptr, args->size);
		ret = EFAULT;
		goto fail;
	}
	oldcpu = td->td_oncpu;
	is_bound = cpu_sched_is_bound(td);
	set_cpu(cpu, td);
	critical_enter();
	rdmsr_safe(MSR_BIOS_SIGN, &rev0); /* Get current microcode revision. */

	/*
	 * Perform update.
	 */
	wrmsr_safe(MSR_BIOS_UPDT_TRIG, (uintptr_t)(ptr));
	wrmsr_safe(MSR_BIOS_SIGN, 0);

	/*
	 * Serialize instruction flow.
	 */
	do_cpuid(0, tmp);
	critical_exit();
	rdmsr_safe(MSR_BIOS_SIGN, &rev1); /* Get new microcode revision. */
	restore_cpu(oldcpu, is_bound, td);
	if (rev1 > rev0)
		ret = 0;
	else
		ret = EEXIST;
fail:
	free(ptr, M_CPUCTL);
	return (ret);
}

static int
update_amd(int cpu, cpuctl_update_args_t *args, struct thread *td)
{
	void *ptr = NULL;
	uint32_t tmp[4];
	int is_bound = 0;
	int oldcpu;
	int ret;

	if (args->size == 0 || args->data == NULL) {
		DPRINTF("[cpuctl,%d]: zero-sized firmware image", __LINE__);
		return (EINVAL);
	}
	if (args->size > UCODE_SIZE_MAX) {
		DPRINTF("[cpuctl,%d]: firmware image too large", __LINE__);
		return (EINVAL);
	}
	/*
	 * XXX Might not require contignous address space - needs check
	 */
	ptr = contigmalloc(args->size, M_CPUCTL, 0, 0, 0xffffffff, 16, 0);
	if (ptr == NULL) {
		DPRINTF("[cpuctl,%d]: cannot allocate %zd bytes of memory",
		    __LINE__, args->size);
		return (ENOMEM);
	}
	if (copyin(args->data, ptr, args->size) != 0) {
		DPRINTF("[cpuctl,%d]: copyin %p->%p of %zd bytes failed",
		    __LINE__, args->data, ptr, args->size);
		ret = EFAULT;
		goto fail;
	}
	oldcpu = td->td_oncpu;
	is_bound = cpu_sched_is_bound(td);
	set_cpu(cpu, td);
	critical_enter();

	/*
	 * Perform update.
	 */
	wrmsr_safe(MSR_K8_UCODE_UPDATE, (uintptr_t)ptr);

	/*
	 * Serialize instruction flow.
	 */
	do_cpuid(0, tmp);
	critical_exit();
	restore_cpu(oldcpu, is_bound, td);
	ret = 0;
fail:
	if (ptr != NULL)
		contigfree(ptr, args->size, M_CPUCTL);
	return (ret);
}

static int
update_via(int cpu, cpuctl_update_args_t *args, struct thread *td)
{
	void *ptr;
	uint64_t rev0, rev1, res;
	uint32_t tmp[4];
	int is_bound;
	int oldcpu;
	int ret;

	if (args->size == 0 || args->data == NULL) {
		DPRINTF("[cpuctl,%d]: zero-sized firmware image", __LINE__);
		return (EINVAL);
	}
	if (args->size > UCODE_SIZE_MAX) {
		DPRINTF("[cpuctl,%d]: firmware image too large", __LINE__);
		return (EINVAL);
	}

	/*
	 * 4 byte alignment required.
	 */
	ptr = malloc(args->size, M_CPUCTL, M_WAITOK);
	if (copyin(args->data, ptr, args->size) != 0) {
		DPRINTF("[cpuctl,%d]: copyin %p->%p of %zd bytes failed",
		    __LINE__, args->data, ptr, args->size);
		ret = EFAULT;
		goto fail;
	}
	oldcpu = td->td_oncpu;
	is_bound = cpu_sched_is_bound(td);
	set_cpu(cpu, td);
	critical_enter();
	rdmsr_safe(MSR_BIOS_SIGN, &rev0); /* Get current microcode revision. */

	/*
	 * Perform update.
	 */
	wrmsr_safe(MSR_BIOS_UPDT_TRIG, (uintptr_t)(ptr));
	do_cpuid(1, tmp);

	/*
	 * Result are in low byte of MSR FCR5:
	 * 0x00: No update has been attempted since RESET.
	 * 0x01: The last attempted update was successful.
	 * 0x02: The last attempted update was unsuccessful due to a bad
	 *       environment. No update was loaded and any preexisting
	 *       patches are still active.
	 * 0x03: The last attempted update was not applicable to this processor.
	 *       No update was loaded and any preexisting patches are still
	 *       active.
	 * 0x04: The last attempted update was not successful due to an invalid
	 *       update data block. No update was loaded and any preexisting
	 *       patches are still active
	 */
	rdmsr_safe(0x1205, &res);
	res &= 0xff;
	critical_exit();
	rdmsr_safe(MSR_BIOS_SIGN, &rev1); /* Get new microcode revision. */
	restore_cpu(oldcpu, is_bound, td);

	DPRINTF("[cpu,%d]: rev0=%x rev1=%x res=%x\n", __LINE__,
	    (unsigned)(rev0 >> 32), (unsigned)(rev1 >> 32), (unsigned)res);

	if (res != 0x01)
		ret = EINVAL;
	else
		ret = 0;
fail:
	free(ptr, M_CPUCTL);
	return (ret);
}

int
cpuctl_open(struct cdev *dev, int flags, int fmt __unused, struct thread *td)
{
	int ret = 0;
	int cpu;

	cpu = dev2unit(dev);
	if (cpu >= mp_ncpus || !cpu_enabled(cpu)) {
		DPRINTF("[cpuctl,%d]: incorrect cpu number %d\n", __LINE__,
		    cpu);
		return (ENXIO);
	}
	if (flags & FWRITE)
		ret = securelevel_gt(td->td_ucred, 0);
	return (ret);
}

static int
cpuctl_modevent(module_t mod __unused, int type, void *data __unused)
{
	int cpu;

	switch(type) {
	case MOD_LOAD:
		if ((cpu_feature & CPUID_MSR) == 0) {
			if (bootverbose)
				printf("cpuctl: not available.\n");
			return (ENODEV);
		}
		if (bootverbose)
			printf("cpuctl: access to MSR registers/cpuid info.\n");
		cpuctl_devs = (struct cdev **)malloc(sizeof(void *) * mp_ncpus,
		    M_CPUCTL, M_WAITOK | M_ZERO);
		if (cpuctl_devs == NULL) {
			DPRINTF("[cpuctl,%d]: cannot allocate memory\n",
			    __LINE__);
			return (ENOMEM);
		}
		for (cpu = 0; cpu < mp_ncpus; cpu++)
			if (cpu_enabled(cpu))
				cpuctl_devs[cpu] = make_dev(&cpuctl_cdevsw, cpu,
				    UID_ROOT, GID_KMEM, 0640, "cpuctl%d", cpu);
		break;
	case MOD_UNLOAD:
		for (cpu = 0; cpu < mp_ncpus; cpu++) {
			if (cpuctl_devs[cpu] != NULL)
				destroy_dev(cpuctl_devs[cpu]);
		}
		free(cpuctl_devs, M_CPUCTL);
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		return (EOPNOTSUPP);
        }
	return (0);
}

DEV_MODULE(cpuctl, cpuctl_modevent, NULL);
MODULE_VERSION(cpuctl, CPUCTL_VERSION);
