/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
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
 */

#include <sys/cdefs.h>
#include "opt_bhyve_snapshot.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/jail.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/libkern.h>
#include <sys/ioccom.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#include <machine/vmparam.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <machine/vmm_instruction_emul.h>
#include <machine/vmm_snapshot.h>
#include <x86/apicreg.h>

#include "vmm_lapic.h"
#include "vmm_stat.h"
#include "vmm_mem.h"
#include "io/ppt.h"
#include "io/vatpic.h"
#include "io/vioapic.h"
#include "io/vhpet.h"
#include "io/vrtc.h"

#ifdef COMPAT_FREEBSD13
struct vm_stats_old {
	int		cpuid;				/* in */
	int		num_entries;			/* out */
	struct timeval	tv;
	uint64_t	statbuf[MAX_VM_STATS];
};

#define	VM_STATS_OLD \
	_IOWR('v', IOCNUM_VM_STATS, struct vm_stats_old)

struct vm_snapshot_meta_old {
	void *ctx;			/* unused */
	void *dev_data;
	const char *dev_name;      /* identify userspace devices */
	enum snapshot_req dev_req; /* identify kernel structs */

	struct vm_snapshot_buffer buffer;

	enum vm_snapshot_op op;
};

#define VM_SNAPSHOT_REQ_OLD \
	_IOWR('v', IOCNUM_SNAPSHOT_REQ, struct vm_snapshot_meta_old)

struct vm_exit_ipi_13 {
	uint32_t	mode;
	uint8_t		vector;
	__BITSET_DEFINE(, 256) dmask;
};

struct vm_exit_13 {
	uint32_t	exitcode;
	int32_t		inst_length;
	uint64_t	rip;
	uint64_t	u[120 / sizeof(uint64_t)];
};

struct vm_run_13 {
	int		cpuid;
	struct vm_exit_13 vm_exit;
};

#define	VM_RUN_13 \
	_IOWR('v', IOCNUM_RUN, struct vm_run_13)

#endif /* COMPAT_FREEBSD13 */

struct devmem_softc {
	int	segid;
	char	*name;
	struct cdev *cdev;
	struct vmmdev_softc *sc;
	SLIST_ENTRY(devmem_softc) link;
};

struct vmmdev_softc {
	struct vm	*vm;		/* vm instance cookie */
	struct cdev	*cdev;
	struct ucred	*ucred;
	SLIST_ENTRY(vmmdev_softc) link;
	SLIST_HEAD(, devmem_softc) devmem;
	int		flags;
};
#define	VSC_LINKED		0x01

static SLIST_HEAD(, vmmdev_softc) head;

static unsigned pr_allow_flag;
static struct mtx vmmdev_mtx;
MTX_SYSINIT(vmmdev_mtx, &vmmdev_mtx, "vmm device mutex", MTX_DEF);

static MALLOC_DEFINE(M_VMMDEV, "vmmdev", "vmmdev");

SYSCTL_DECL(_hw_vmm);

static int vmm_priv_check(struct ucred *ucred);
static int devmem_create_cdev(const char *vmname, int id, char *devmem);
static void devmem_destroy(void *arg);

static int
vmm_priv_check(struct ucred *ucred)
{

	if (jailed(ucred) &&
	    !(ucred->cr_prison->pr_allow & pr_allow_flag))
		return (EPERM);

	return (0);
}

static int
vcpu_lock_one(struct vcpu *vcpu)
{
	return (vcpu_set_state(vcpu, VCPU_FROZEN, true));
}

static void
vcpu_unlock_one(struct vmmdev_softc *sc, int vcpuid, struct vcpu *vcpu)
{
	enum vcpu_state state;

	state = vcpu_get_state(vcpu, NULL);
	if (state != VCPU_FROZEN) {
		panic("vcpu %s(%d) has invalid state %d", vm_name(sc->vm),
		    vcpuid, state);
	}

	vcpu_set_state(vcpu, VCPU_IDLE, false);
}

static int
vcpu_lock_all(struct vmmdev_softc *sc)
{
	struct vcpu *vcpu;
	int error;
	uint16_t i, j, maxcpus;

	error = 0;
	vm_slock_vcpus(sc->vm);
	maxcpus = vm_get_maxcpus(sc->vm);
	for (i = 0; i < maxcpus; i++) {
		vcpu = vm_vcpu(sc->vm, i);
		if (vcpu == NULL)
			continue;
		error = vcpu_lock_one(vcpu);
		if (error)
			break;
	}

	if (error) {
		for (j = 0; j < i; j++) {
			vcpu = vm_vcpu(sc->vm, j);
			if (vcpu == NULL)
				continue;
			vcpu_unlock_one(sc, j, vcpu);
		}
		vm_unlock_vcpus(sc->vm);
	}

	return (error);
}

static void
vcpu_unlock_all(struct vmmdev_softc *sc)
{
	struct vcpu *vcpu;
	uint16_t i, maxcpus;

	maxcpus = vm_get_maxcpus(sc->vm);
	for (i = 0; i < maxcpus; i++) {
		vcpu = vm_vcpu(sc->vm, i);
		if (vcpu == NULL)
			continue;
		vcpu_unlock_one(sc, i, vcpu);
	}
	vm_unlock_vcpus(sc->vm);
}

static struct vmmdev_softc *
vmmdev_lookup(const char *name)
{
	struct vmmdev_softc *sc;

#ifdef notyet	/* XXX kernel is not compiled with invariants */
	mtx_assert(&vmmdev_mtx, MA_OWNED);
#endif

	SLIST_FOREACH(sc, &head, link) {
		if (strcmp(name, vm_name(sc->vm)) == 0)
			break;
	}

	if (sc == NULL)
		return (NULL);

	if (cr_cansee(curthread->td_ucred, sc->ucred))
		return (NULL);

	return (sc);
}

static struct vmmdev_softc *
vmmdev_lookup2(struct cdev *cdev)
{

	return (cdev->si_drv1);
}

static int
vmmdev_rw(struct cdev *cdev, struct uio *uio, int flags)
{
	int error, off, c, prot;
	vm_paddr_t gpa, maxaddr;
	void *hpa, *cookie;
	struct vmmdev_softc *sc;

	error = vmm_priv_check(curthread->td_ucred);
	if (error)
		return (error);

	sc = vmmdev_lookup2(cdev);
	if (sc == NULL)
		return (ENXIO);

	/*
	 * Get a read lock on the guest memory map.
	 */
	vm_slock_memsegs(sc->vm);

	prot = (uio->uio_rw == UIO_WRITE ? VM_PROT_WRITE : VM_PROT_READ);
	maxaddr = vmm_sysmem_maxaddr(sc->vm);
	while (uio->uio_resid > 0 && error == 0) {
		gpa = uio->uio_offset;
		off = gpa & PAGE_MASK;
		c = min(uio->uio_resid, PAGE_SIZE - off);

		/*
		 * The VM has a hole in its physical memory map. If we want to
		 * use 'dd' to inspect memory beyond the hole we need to
		 * provide bogus data for memory that lies in the hole.
		 *
		 * Since this device does not support lseek(2), dd(1) will
		 * read(2) blocks of data to simulate the lseek(2).
		 */
		hpa = vm_gpa_hold_global(sc->vm, gpa, c, prot, &cookie);
		if (hpa == NULL) {
			if (uio->uio_rw == UIO_READ && gpa < maxaddr)
				error = uiomove(__DECONST(void *, zero_region),
				    c, uio);
			else
				error = EFAULT;
		} else {
			error = uiomove(hpa, c, uio);
			vm_gpa_release(cookie);
		}
	}
	vm_unlock_memsegs(sc->vm);
	return (error);
}

CTASSERT(sizeof(((struct vm_memseg *)0)->name) >= VM_MAX_SUFFIXLEN + 1);

static int
get_memseg(struct vmmdev_softc *sc, struct vm_memseg *mseg, size_t len)
{
	struct devmem_softc *dsc;
	int error;
	bool sysmem;

	error = vm_get_memseg(sc->vm, mseg->segid, &mseg->len, &sysmem, NULL);
	if (error || mseg->len == 0)
		return (error);

	if (!sysmem) {
		SLIST_FOREACH(dsc, &sc->devmem, link) {
			if (dsc->segid == mseg->segid)
				break;
		}
		KASSERT(dsc != NULL, ("%s: devmem segment %d not found",
		    __func__, mseg->segid));
		error = copystr(dsc->name, mseg->name, len, NULL);
	} else {
		bzero(mseg->name, len);
	}

	return (error);
}

static int
alloc_memseg(struct vmmdev_softc *sc, struct vm_memseg *mseg, size_t len)
{
	char *name;
	int error;
	bool sysmem;

	error = 0;
	name = NULL;
	sysmem = true;

	/*
	 * The allocation is lengthened by 1 to hold a terminating NUL.  It'll
	 * by stripped off when devfs processes the full string.
	 */
	if (VM_MEMSEG_NAME(mseg)) {
		sysmem = false;
		name = malloc(len, M_VMMDEV, M_WAITOK);
		error = copystr(mseg->name, name, len, NULL);
		if (error)
			goto done;
	}

	error = vm_alloc_memseg(sc->vm, mseg->segid, mseg->len, sysmem);
	if (error)
		goto done;

	if (VM_MEMSEG_NAME(mseg)) {
		error = devmem_create_cdev(vm_name(sc->vm), mseg->segid, name);
		if (error)
			vm_free_memseg(sc->vm, mseg->segid);
		else
			name = NULL;	/* freed when 'cdev' is destroyed */
	}
done:
	free(name, M_VMMDEV);
	return (error);
}

static int
vm_get_register_set(struct vcpu *vcpu, unsigned int count, int *regnum,
    uint64_t *regval)
{
	int error, i;

	error = 0;
	for (i = 0; i < count; i++) {
		error = vm_get_register(vcpu, regnum[i], &regval[i]);
		if (error)
			break;
	}
	return (error);
}

static int
vm_set_register_set(struct vcpu *vcpu, unsigned int count, int *regnum,
    uint64_t *regval)
{
	int error, i;

	error = 0;
	for (i = 0; i < count; i++) {
		error = vm_set_register(vcpu, regnum[i], regval[i]);
		if (error)
			break;
	}
	return (error);
}

static int
vmmdev_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int fflag,
	     struct thread *td)
{
	int error, vcpuid, size;
	cpuset_t *cpuset;
	struct vmmdev_softc *sc;
	struct vcpu *vcpu;
	struct vm_register *vmreg;
	struct vm_seg_desc *vmsegdesc;
	struct vm_register_set *vmregset;
	struct vm_run *vmrun;
#ifdef COMPAT_FREEBSD13
	struct vm_run_13 *vmrun_13;
#endif
	struct vm_exception *vmexc;
	struct vm_lapic_irq *vmirq;
	struct vm_lapic_msi *vmmsi;
	struct vm_ioapic_irq *ioapic_irq;
	struct vm_isa_irq *isa_irq;
	struct vm_isa_irq_trigger *isa_irq_trigger;
	struct vm_capability *vmcap;
	struct vm_pptdev *pptdev;
	struct vm_pptdev_mmio *pptmmio;
	struct vm_pptdev_msi *pptmsi;
	struct vm_pptdev_msix *pptmsix;
#ifdef COMPAT_FREEBSD13
	struct vm_stats_old *vmstats_old;
#endif
	struct vm_stats *vmstats;
	struct vm_stat_desc *statdesc;
	struct vm_x2apic *x2apic;
	struct vm_gpa_pte *gpapte;
	struct vm_suspend *vmsuspend;
	struct vm_gla2gpa *gg;
	struct vm_cpuset *vm_cpuset;
	struct vm_intinfo *vmii;
	struct vm_rtc_time *rtctime;
	struct vm_rtc_data *rtcdata;
	struct vm_memmap *mm;
	struct vm_munmap *mu;
	struct vm_cpu_topology *topology;
	struct vm_readwrite_kernemu_device *kernemu;
	uint64_t *regvals;
	int *regnums;
	enum { NONE, SINGLE, ALL } vcpus_locked;
	bool memsegs_locked;
#ifdef BHYVE_SNAPSHOT
	struct vm_snapshot_meta *snapshot_meta;
#ifdef COMPAT_FREEBSD13
	struct vm_snapshot_meta_old *snapshot_old;
#endif
#endif

	error = vmm_priv_check(curthread->td_ucred);
	if (error)
		return (error);

	sc = vmmdev_lookup2(cdev);
	if (sc == NULL)
		return (ENXIO);

	vcpuid = -1;
	vcpu = NULL;
	vcpus_locked = NONE;
	memsegs_locked = false;

	/*
	 * For VMM ioctls that operate on a single vCPU, lookup the
	 * vcpu.  For VMM ioctls which require one or more vCPUs to
	 * not be running, lock necessary vCPUs.
	 *
	 * XXX fragile, handle with care
	 * Most of these assume that the first field of the ioctl data
	 * is the vcpuid.
	 */
	switch (cmd) {
	case VM_RUN:
#ifdef COMPAT_FREEBSD13
	case VM_RUN_13:
#endif
	case VM_GET_REGISTER:
	case VM_SET_REGISTER:
	case VM_GET_SEGMENT_DESCRIPTOR:
	case VM_SET_SEGMENT_DESCRIPTOR:
	case VM_GET_REGISTER_SET:
	case VM_SET_REGISTER_SET:
	case VM_INJECT_EXCEPTION:
	case VM_GET_CAPABILITY:
	case VM_SET_CAPABILITY:
	case VM_SET_X2APIC_STATE:
	case VM_GLA2GPA:
	case VM_GLA2GPA_NOFAULT:
	case VM_ACTIVATE_CPU:
	case VM_SET_INTINFO:
	case VM_GET_INTINFO:
	case VM_RESTART_INSTRUCTION:
	case VM_GET_KERNEMU_DEV:
	case VM_SET_KERNEMU_DEV:
		/*
		 * ioctls that can operate only on vcpus that are not running.
		 */
		vcpuid = *(int *)data;
		vcpu = vm_alloc_vcpu(sc->vm, vcpuid);
		if (vcpu == NULL) {
			error = EINVAL;
			goto done;
		}
		error = vcpu_lock_one(vcpu);
		if (error)
			goto done;
		vcpus_locked = SINGLE;
		break;

#ifdef COMPAT_FREEBSD12
	case VM_ALLOC_MEMSEG_FBSD12:
#endif
	case VM_ALLOC_MEMSEG:
	case VM_BIND_PPTDEV:
	case VM_UNBIND_PPTDEV:
	case VM_MMAP_MEMSEG:
	case VM_MUNMAP_MEMSEG:
	case VM_REINIT:
		/*
		 * ioctls that modify the memory map must lock memory
		 * segments exclusively.
		 */
		vm_xlock_memsegs(sc->vm);
		memsegs_locked = true;
		/* FALLTHROUGH */
	case VM_MAP_PPTDEV_MMIO:
	case VM_UNMAP_PPTDEV_MMIO:
#ifdef BHYVE_SNAPSHOT
	case VM_SNAPSHOT_REQ:
#ifdef COMPAT_FREEBSD13
	case VM_SNAPSHOT_REQ_OLD:
#endif
	case VM_RESTORE_TIME:
#endif
		/*
		 * ioctls that operate on the entire virtual machine must
		 * prevent all vcpus from running.
		 */
		error = vcpu_lock_all(sc);
		if (error)
			goto done;
		vcpus_locked = ALL;
		break;

#ifdef COMPAT_FREEBSD12
	case VM_GET_MEMSEG_FBSD12:
#endif
	case VM_GET_MEMSEG:
	case VM_MMAP_GETNEXT:
		/*
		 * Lock the memory map while it is being inspected.
		 */
		vm_slock_memsegs(sc->vm);
		memsegs_locked = true;
		break;

#ifdef COMPAT_FREEBSD13
	case VM_STATS_OLD:
#endif
	case VM_STATS:
	case VM_INJECT_NMI:
	case VM_LAPIC_IRQ:
	case VM_GET_X2APIC_STATE:
		/*
		 * These do not need the vCPU locked but do operate on
		 * a specific vCPU.
		 */
		vcpuid = *(int *)data;
		vcpu = vm_alloc_vcpu(sc->vm, vcpuid);
		if (vcpu == NULL) {
			error = EINVAL;
			goto done;
		}
		break;

	case VM_LAPIC_LOCAL_IRQ:
	case VM_SUSPEND_CPU:
	case VM_RESUME_CPU:
		/*
		 * These can either operate on all CPUs via a vcpuid of
		 * -1 or on a specific vCPU.
		 */
		vcpuid = *(int *)data;
		if (vcpuid == -1)
			break;
		vcpu = vm_alloc_vcpu(sc->vm, vcpuid);
		if (vcpu == NULL) {
			error = EINVAL;
			goto done;
		}
		break;

	default:
		break;
	}

	switch (cmd) {
	case VM_RUN: {
		struct vm_exit *vme;

		vmrun = (struct vm_run *)data;
		vme = vm_exitinfo(vcpu);

		error = vm_run(vcpu);
		if (error != 0)
			break;

		error = copyout(vme, vmrun->vm_exit, sizeof(*vme));
		if (error != 0)
			break;
		if (vme->exitcode == VM_EXITCODE_IPI) {
			error = copyout(vm_exitinfo_cpuset(vcpu),
			    vmrun->cpuset,
			    min(vmrun->cpusetsize, sizeof(cpuset_t)));
			if (error != 0)
				break;
			if (sizeof(cpuset_t) < vmrun->cpusetsize) {
				uint8_t *p;

				p = (uint8_t *)vmrun->cpuset +
				    sizeof(cpuset_t);
				while (error == 0 &&
				    p < (uint8_t *)vmrun->cpuset +
				    vmrun->cpusetsize) {
					error = subyte(p++, 0);
				}
			}
		}
		break;
	}
#ifdef COMPAT_FREEBSD13
	case VM_RUN_13: {
		struct vm_exit *vme;
		struct vm_exit_13 *vme_13;

		vmrun_13 = (struct vm_run_13 *)data;
		vme_13 = &vmrun_13->vm_exit;
		vme = vm_exitinfo(vcpu);

		error = vm_run(vcpu);
		if (error == 0) {
			vme_13->exitcode = vme->exitcode;
			vme_13->inst_length = vme->inst_length;
			vme_13->rip = vme->rip;
			memcpy(vme_13->u, &vme->u, sizeof(vme_13->u));
			if (vme->exitcode == VM_EXITCODE_IPI) {
				struct vm_exit_ipi_13 *ipi;
				cpuset_t *dmask;
				int cpu;

				dmask = vm_exitinfo_cpuset(vcpu);
				ipi = (struct vm_exit_ipi_13 *)&vme_13->u[0];
				BIT_ZERO(256, &ipi->dmask);
				CPU_FOREACH_ISSET(cpu, dmask) {
					if (cpu >= 256)
						break;
					BIT_SET(256, cpu, &ipi->dmask);
				}
			}
		}
		break;
	}
#endif
	case VM_SUSPEND:
		vmsuspend = (struct vm_suspend *)data;
		error = vm_suspend(sc->vm, vmsuspend->how);
		break;
	case VM_REINIT:
		error = vm_reinit(sc->vm);
		break;
	case VM_STAT_DESC: {
		statdesc = (struct vm_stat_desc *)data;
		error = vmm_stat_desc_copy(statdesc->index,
					statdesc->desc, sizeof(statdesc->desc));
		break;
	}
#ifdef COMPAT_FREEBSD13
	case VM_STATS_OLD:
		vmstats_old = (struct vm_stats_old *)data;
		getmicrotime(&vmstats_old->tv);
		error = vmm_stat_copy(vcpu, 0,
				      nitems(vmstats_old->statbuf),
				      &vmstats_old->num_entries,
				      vmstats_old->statbuf);
		break;
#endif
	case VM_STATS: {
		vmstats = (struct vm_stats *)data;
		getmicrotime(&vmstats->tv);
		error = vmm_stat_copy(vcpu, vmstats->index,
				      nitems(vmstats->statbuf),
				      &vmstats->num_entries, vmstats->statbuf);
		break;
	}
	case VM_PPTDEV_MSI:
		pptmsi = (struct vm_pptdev_msi *)data;
		error = ppt_setup_msi(sc->vm,
				      pptmsi->bus, pptmsi->slot, pptmsi->func,
				      pptmsi->addr, pptmsi->msg,
				      pptmsi->numvec);
		break;
	case VM_PPTDEV_MSIX:
		pptmsix = (struct vm_pptdev_msix *)data;
		error = ppt_setup_msix(sc->vm,
				       pptmsix->bus, pptmsix->slot, 
				       pptmsix->func, pptmsix->idx,
				       pptmsix->addr, pptmsix->msg,
				       pptmsix->vector_control);
		break;
	case VM_PPTDEV_DISABLE_MSIX:
		pptdev = (struct vm_pptdev *)data;
		error = ppt_disable_msix(sc->vm, pptdev->bus, pptdev->slot,
					 pptdev->func);
		break;
	case VM_MAP_PPTDEV_MMIO:
		pptmmio = (struct vm_pptdev_mmio *)data;
		error = ppt_map_mmio(sc->vm, pptmmio->bus, pptmmio->slot,
				     pptmmio->func, pptmmio->gpa, pptmmio->len,
				     pptmmio->hpa);
		break;
	case VM_UNMAP_PPTDEV_MMIO:
		pptmmio = (struct vm_pptdev_mmio *)data;
		error = ppt_unmap_mmio(sc->vm, pptmmio->bus, pptmmio->slot,
				       pptmmio->func, pptmmio->gpa, pptmmio->len);
		break;
	case VM_BIND_PPTDEV:
		pptdev = (struct vm_pptdev *)data;
		error = vm_assign_pptdev(sc->vm, pptdev->bus, pptdev->slot,
					 pptdev->func);
		break;
	case VM_UNBIND_PPTDEV:
		pptdev = (struct vm_pptdev *)data;
		error = vm_unassign_pptdev(sc->vm, pptdev->bus, pptdev->slot,
					   pptdev->func);
		break;
	case VM_INJECT_EXCEPTION:
		vmexc = (struct vm_exception *)data;
		error = vm_inject_exception(vcpu,
		    vmexc->vector, vmexc->error_code_valid, vmexc->error_code,
		    vmexc->restart_instruction);
		break;
	case VM_INJECT_NMI:
		error = vm_inject_nmi(vcpu);
		break;
	case VM_LAPIC_IRQ:
		vmirq = (struct vm_lapic_irq *)data;
		error = lapic_intr_edge(vcpu, vmirq->vector);
		break;
	case VM_LAPIC_LOCAL_IRQ:
		vmirq = (struct vm_lapic_irq *)data;
		error = lapic_set_local_intr(sc->vm, vcpu, vmirq->vector);
		break;
	case VM_LAPIC_MSI:
		vmmsi = (struct vm_lapic_msi *)data;
		error = lapic_intr_msi(sc->vm, vmmsi->addr, vmmsi->msg);
		break;
	case VM_IOAPIC_ASSERT_IRQ:
		ioapic_irq = (struct vm_ioapic_irq *)data;
		error = vioapic_assert_irq(sc->vm, ioapic_irq->irq);
		break;
	case VM_IOAPIC_DEASSERT_IRQ:
		ioapic_irq = (struct vm_ioapic_irq *)data;
		error = vioapic_deassert_irq(sc->vm, ioapic_irq->irq);
		break;
	case VM_IOAPIC_PULSE_IRQ:
		ioapic_irq = (struct vm_ioapic_irq *)data;
		error = vioapic_pulse_irq(sc->vm, ioapic_irq->irq);
		break;
	case VM_IOAPIC_PINCOUNT:
		*(int *)data = vioapic_pincount(sc->vm);
		break;
	case VM_SET_KERNEMU_DEV:
	case VM_GET_KERNEMU_DEV: {
		mem_region_write_t mwrite;
		mem_region_read_t mread;
		bool arg;

		kernemu = (void *)data;

		if (kernemu->access_width > 0)
			size = (1u << kernemu->access_width);
		else
			size = 1;

		if (kernemu->gpa >= DEFAULT_APIC_BASE && kernemu->gpa < DEFAULT_APIC_BASE + PAGE_SIZE) {
			mread = lapic_mmio_read;
			mwrite = lapic_mmio_write;
		} else if (kernemu->gpa >= VIOAPIC_BASE && kernemu->gpa < VIOAPIC_BASE + VIOAPIC_SIZE) {
			mread = vioapic_mmio_read;
			mwrite = vioapic_mmio_write;
		} else if (kernemu->gpa >= VHPET_BASE && kernemu->gpa < VHPET_BASE + VHPET_SIZE) {
			mread = vhpet_mmio_read;
			mwrite = vhpet_mmio_write;
		} else {
			error = EINVAL;
			break;
		}

		if (cmd == VM_SET_KERNEMU_DEV)
			error = mwrite(vcpu, kernemu->gpa,
			    kernemu->value, size, &arg);
		else
			error = mread(vcpu, kernemu->gpa,
			    &kernemu->value, size, &arg);
		break;
		}
	case VM_ISA_ASSERT_IRQ:
		isa_irq = (struct vm_isa_irq *)data;
		error = vatpic_assert_irq(sc->vm, isa_irq->atpic_irq);
		if (error == 0 && isa_irq->ioapic_irq != -1)
			error = vioapic_assert_irq(sc->vm,
			    isa_irq->ioapic_irq);
		break;
	case VM_ISA_DEASSERT_IRQ:
		isa_irq = (struct vm_isa_irq *)data;
		error = vatpic_deassert_irq(sc->vm, isa_irq->atpic_irq);
		if (error == 0 && isa_irq->ioapic_irq != -1)
			error = vioapic_deassert_irq(sc->vm,
			    isa_irq->ioapic_irq);
		break;
	case VM_ISA_PULSE_IRQ:
		isa_irq = (struct vm_isa_irq *)data;
		error = vatpic_pulse_irq(sc->vm, isa_irq->atpic_irq);
		if (error == 0 && isa_irq->ioapic_irq != -1)
			error = vioapic_pulse_irq(sc->vm, isa_irq->ioapic_irq);
		break;
	case VM_ISA_SET_IRQ_TRIGGER:
		isa_irq_trigger = (struct vm_isa_irq_trigger *)data;
		error = vatpic_set_irq_trigger(sc->vm,
		    isa_irq_trigger->atpic_irq, isa_irq_trigger->trigger);
		break;
	case VM_MMAP_GETNEXT:
		mm = (struct vm_memmap *)data;
		error = vm_mmap_getnext(sc->vm, &mm->gpa, &mm->segid,
		    &mm->segoff, &mm->len, &mm->prot, &mm->flags);
		break;
	case VM_MMAP_MEMSEG:
		mm = (struct vm_memmap *)data;
		error = vm_mmap_memseg(sc->vm, mm->gpa, mm->segid, mm->segoff,
		    mm->len, mm->prot, mm->flags);
		break;
	case VM_MUNMAP_MEMSEG:
		mu = (struct vm_munmap *)data;
		error = vm_munmap_memseg(sc->vm, mu->gpa, mu->len);
		break;
#ifdef COMPAT_FREEBSD12
	case VM_ALLOC_MEMSEG_FBSD12:
		error = alloc_memseg(sc, (struct vm_memseg *)data,
		    sizeof(((struct vm_memseg_fbsd12 *)0)->name));
		break;
#endif
	case VM_ALLOC_MEMSEG:
		error = alloc_memseg(sc, (struct vm_memseg *)data,
		    sizeof(((struct vm_memseg *)0)->name));
		break;
#ifdef COMPAT_FREEBSD12
	case VM_GET_MEMSEG_FBSD12:
		error = get_memseg(sc, (struct vm_memseg *)data,
		    sizeof(((struct vm_memseg_fbsd12 *)0)->name));
		break;
#endif
	case VM_GET_MEMSEG:
		error = get_memseg(sc, (struct vm_memseg *)data,
		    sizeof(((struct vm_memseg *)0)->name));
		break;
	case VM_GET_REGISTER:
		vmreg = (struct vm_register *)data;
		error = vm_get_register(vcpu, vmreg->regnum, &vmreg->regval);
		break;
	case VM_SET_REGISTER:
		vmreg = (struct vm_register *)data;
		error = vm_set_register(vcpu, vmreg->regnum, vmreg->regval);
		break;
	case VM_SET_SEGMENT_DESCRIPTOR:
		vmsegdesc = (struct vm_seg_desc *)data;
		error = vm_set_seg_desc(vcpu,
					vmsegdesc->regnum,
					&vmsegdesc->desc);
		break;
	case VM_GET_SEGMENT_DESCRIPTOR:
		vmsegdesc = (struct vm_seg_desc *)data;
		error = vm_get_seg_desc(vcpu,
					vmsegdesc->regnum,
					&vmsegdesc->desc);
		break;
	case VM_GET_REGISTER_SET:
		vmregset = (struct vm_register_set *)data;
		if (vmregset->count > VM_REG_LAST) {
			error = EINVAL;
			break;
		}
		regvals = malloc(sizeof(regvals[0]) * vmregset->count, M_VMMDEV,
		    M_WAITOK);
		regnums = malloc(sizeof(regnums[0]) * vmregset->count, M_VMMDEV,
		    M_WAITOK);
		error = copyin(vmregset->regnums, regnums, sizeof(regnums[0]) *
		    vmregset->count);
		if (error == 0)
			error = vm_get_register_set(vcpu,
			    vmregset->count, regnums, regvals);
		if (error == 0)
			error = copyout(regvals, vmregset->regvals,
			    sizeof(regvals[0]) * vmregset->count);
		free(regvals, M_VMMDEV);
		free(regnums, M_VMMDEV);
		break;
	case VM_SET_REGISTER_SET:
		vmregset = (struct vm_register_set *)data;
		if (vmregset->count > VM_REG_LAST) {
			error = EINVAL;
			break;
		}
		regvals = malloc(sizeof(regvals[0]) * vmregset->count, M_VMMDEV,
		    M_WAITOK);
		regnums = malloc(sizeof(regnums[0]) * vmregset->count, M_VMMDEV,
		    M_WAITOK);
		error = copyin(vmregset->regnums, regnums, sizeof(regnums[0]) *
		    vmregset->count);
		if (error == 0)
			error = copyin(vmregset->regvals, regvals,
			    sizeof(regvals[0]) * vmregset->count);
		if (error == 0)
			error = vm_set_register_set(vcpu,
			    vmregset->count, regnums, regvals);
		free(regvals, M_VMMDEV);
		free(regnums, M_VMMDEV);
		break;
	case VM_GET_CAPABILITY:
		vmcap = (struct vm_capability *)data;
		error = vm_get_capability(vcpu,
					  vmcap->captype,
					  &vmcap->capval);
		break;
	case VM_SET_CAPABILITY:
		vmcap = (struct vm_capability *)data;
		error = vm_set_capability(vcpu,
					  vmcap->captype,
					  vmcap->capval);
		break;
	case VM_SET_X2APIC_STATE:
		x2apic = (struct vm_x2apic *)data;
		error = vm_set_x2apic_state(vcpu, x2apic->state);
		break;
	case VM_GET_X2APIC_STATE:
		x2apic = (struct vm_x2apic *)data;
		error = vm_get_x2apic_state(vcpu, &x2apic->state);
		break;
	case VM_GET_GPA_PMAP:
		gpapte = (struct vm_gpa_pte *)data;
		pmap_get_mapping(vmspace_pmap(vm_get_vmspace(sc->vm)),
				 gpapte->gpa, gpapte->pte, &gpapte->ptenum);
		error = 0;
		break;
	case VM_GET_HPET_CAPABILITIES:
		error = vhpet_getcap((struct vm_hpet_cap *)data);
		break;
	case VM_GLA2GPA: {
		CTASSERT(PROT_READ == VM_PROT_READ);
		CTASSERT(PROT_WRITE == VM_PROT_WRITE);
		CTASSERT(PROT_EXEC == VM_PROT_EXECUTE);
		gg = (struct vm_gla2gpa *)data;
		error = vm_gla2gpa(vcpu, &gg->paging, gg->gla,
		    gg->prot, &gg->gpa, &gg->fault);
		KASSERT(error == 0 || error == EFAULT,
		    ("%s: vm_gla2gpa unknown error %d", __func__, error));
		break;
	}
	case VM_GLA2GPA_NOFAULT:
		gg = (struct vm_gla2gpa *)data;
		error = vm_gla2gpa_nofault(vcpu, &gg->paging, gg->gla,
		    gg->prot, &gg->gpa, &gg->fault);
		KASSERT(error == 0 || error == EFAULT,
		    ("%s: vm_gla2gpa unknown error %d", __func__, error));
		break;
	case VM_ACTIVATE_CPU:
		error = vm_activate_cpu(vcpu);
		break;
	case VM_GET_CPUS:
		error = 0;
		vm_cpuset = (struct vm_cpuset *)data;
		size = vm_cpuset->cpusetsize;
		if (size < sizeof(cpuset_t) || size > CPU_MAXSIZE / NBBY) {
			error = ERANGE;
			break;
		}
		cpuset = malloc(size, M_TEMP, M_WAITOK | M_ZERO);
		if (vm_cpuset->which == VM_ACTIVE_CPUS)
			*cpuset = vm_active_cpus(sc->vm);
		else if (vm_cpuset->which == VM_SUSPENDED_CPUS)
			*cpuset = vm_suspended_cpus(sc->vm);
		else if (vm_cpuset->which == VM_DEBUG_CPUS)
			*cpuset = vm_debug_cpus(sc->vm);
		else
			error = EINVAL;
		if (error == 0)
			error = copyout(cpuset, vm_cpuset->cpus, size);
		free(cpuset, M_TEMP);
		break;
	case VM_SUSPEND_CPU:
		error = vm_suspend_cpu(sc->vm, vcpu);
		break;
	case VM_RESUME_CPU:
		error = vm_resume_cpu(sc->vm, vcpu);
		break;
	case VM_SET_INTINFO:
		vmii = (struct vm_intinfo *)data;
		error = vm_exit_intinfo(vcpu, vmii->info1);
		break;
	case VM_GET_INTINFO:
		vmii = (struct vm_intinfo *)data;
		error = vm_get_intinfo(vcpu, &vmii->info1, &vmii->info2);
		break;
	case VM_RTC_WRITE:
		rtcdata = (struct vm_rtc_data *)data;
		error = vrtc_nvram_write(sc->vm, rtcdata->offset,
		    rtcdata->value);
		break;
	case VM_RTC_READ:
		rtcdata = (struct vm_rtc_data *)data;
		error = vrtc_nvram_read(sc->vm, rtcdata->offset,
		    &rtcdata->value);
		break;
	case VM_RTC_SETTIME:
		rtctime = (struct vm_rtc_time *)data;
		error = vrtc_set_time(sc->vm, rtctime->secs);
		break;
	case VM_RTC_GETTIME:
		error = 0;
		rtctime = (struct vm_rtc_time *)data;
		rtctime->secs = vrtc_get_time(sc->vm);
		break;
	case VM_RESTART_INSTRUCTION:
		error = vm_restart_instruction(vcpu);
		break;
	case VM_SET_TOPOLOGY:
		topology = (struct vm_cpu_topology *)data;
		error = vm_set_topology(sc->vm, topology->sockets,
		    topology->cores, topology->threads, topology->maxcpus);
		break;
	case VM_GET_TOPOLOGY:
		topology = (struct vm_cpu_topology *)data;
		vm_get_topology(sc->vm, &topology->sockets, &topology->cores,
		    &topology->threads, &topology->maxcpus);
		error = 0;
		break;
#ifdef BHYVE_SNAPSHOT
	case VM_SNAPSHOT_REQ:
		snapshot_meta = (struct vm_snapshot_meta *)data;
		error = vm_snapshot_req(sc->vm, snapshot_meta);
		break;
#ifdef COMPAT_FREEBSD13
	case VM_SNAPSHOT_REQ_OLD:
		/*
		 * The old structure just has an additional pointer at
		 * the start that is ignored.
		 */
		snapshot_old = (struct vm_snapshot_meta_old *)data;
		snapshot_meta =
		    (struct vm_snapshot_meta *)&snapshot_old->dev_data;
		error = vm_snapshot_req(sc->vm, snapshot_meta);
		break;
#endif
	case VM_RESTORE_TIME:
		error = vm_restore_time(sc->vm);
		break;
#endif
	default:
		error = ENOTTY;
		break;
	}

done:
	if (vcpus_locked == SINGLE)
		vcpu_unlock_one(sc, vcpuid, vcpu);
	else if (vcpus_locked == ALL)
		vcpu_unlock_all(sc);
	if (memsegs_locked)
		vm_unlock_memsegs(sc->vm);

	/*
	 * Make sure that no handler returns a kernel-internal
	 * error value to userspace.
	 */
	KASSERT(error == ERESTART || error >= 0,
	    ("vmmdev_ioctl: invalid error return %d", error));
	return (error);
}

static int
vmmdev_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t mapsize,
    struct vm_object **objp, int nprot)
{
	struct vmmdev_softc *sc;
	vm_paddr_t gpa;
	size_t len;
	vm_ooffset_t segoff, first, last;
	int error, found, segid;
	bool sysmem;

	error = vmm_priv_check(curthread->td_ucred);
	if (error)
		return (error);

	first = *offset;
	last = first + mapsize;
	if ((nprot & PROT_EXEC) || first < 0 || first >= last)
		return (EINVAL);

	sc = vmmdev_lookup2(cdev);
	if (sc == NULL) {
		/* virtual machine is in the process of being created */
		return (EINVAL);
	}

	/*
	 * Get a read lock on the guest memory map.
	 */
	vm_slock_memsegs(sc->vm);

	gpa = 0;
	found = 0;
	while (!found) {
		error = vm_mmap_getnext(sc->vm, &gpa, &segid, &segoff, &len,
		    NULL, NULL);
		if (error)
			break;

		if (first >= gpa && last <= gpa + len)
			found = 1;
		else
			gpa += len;
	}

	if (found) {
		error = vm_get_memseg(sc->vm, segid, &len, &sysmem, objp);
		KASSERT(error == 0 && *objp != NULL,
		    ("%s: invalid memory segment %d", __func__, segid));
		if (sysmem) {
			vm_object_reference(*objp);
			*offset = segoff + (first - gpa);
		} else {
			error = EINVAL;
		}
	}
	vm_unlock_memsegs(sc->vm);
	return (error);
}

static void
vmmdev_destroy(void *arg)
{
	struct vmmdev_softc *sc = arg;
	struct devmem_softc *dsc;
	int error __diagused;

	vm_disable_vcpu_creation(sc->vm);
	error = vcpu_lock_all(sc);
	KASSERT(error == 0, ("%s: error %d freezing vcpus", __func__, error));
	vm_unlock_vcpus(sc->vm);

	while ((dsc = SLIST_FIRST(&sc->devmem)) != NULL) {
		KASSERT(dsc->cdev == NULL, ("%s: devmem not free", __func__));
		SLIST_REMOVE_HEAD(&sc->devmem, link);
		free(dsc->name, M_VMMDEV);
		free(dsc, M_VMMDEV);
	}

	if (sc->cdev != NULL)
		destroy_dev(sc->cdev);

	if (sc->vm != NULL)
		vm_destroy(sc->vm);

	if (sc->ucred != NULL)
		crfree(sc->ucred);

	if ((sc->flags & VSC_LINKED) != 0) {
		mtx_lock(&vmmdev_mtx);
		SLIST_REMOVE(&head, sc, vmmdev_softc, link);
		mtx_unlock(&vmmdev_mtx);
	}

	free(sc, M_VMMDEV);
}

static int
sysctl_vmm_destroy(SYSCTL_HANDLER_ARGS)
{
	struct devmem_softc *dsc;
	struct vmmdev_softc *sc;
	struct cdev *cdev;
	char *buf;
	int error, buflen;

	error = vmm_priv_check(req->td->td_ucred);
	if (error)
		return (error);

	buflen = VM_MAX_NAMELEN + 1;
	buf = malloc(buflen, M_VMMDEV, M_WAITOK | M_ZERO);
	strlcpy(buf, "beavis", buflen);
	error = sysctl_handle_string(oidp, buf, buflen, req);
	if (error != 0 || req->newptr == NULL)
		goto out;

	mtx_lock(&vmmdev_mtx);
	sc = vmmdev_lookup(buf);
	if (sc == NULL || sc->cdev == NULL) {
		mtx_unlock(&vmmdev_mtx);
		error = EINVAL;
		goto out;
	}

	/*
	 * Setting 'sc->cdev' to NULL is used to indicate that the VM
	 * is scheduled for destruction.
	 */
	cdev = sc->cdev;
	sc->cdev = NULL;		
	mtx_unlock(&vmmdev_mtx);

	/*
	 * Destroy all cdevs:
	 *
	 * - any new operations on the 'cdev' will return an error (ENXIO).
	 *
	 * - the 'devmem' cdevs are destroyed before the virtual machine 'cdev'
	 */
	SLIST_FOREACH(dsc, &sc->devmem, link) {
		KASSERT(dsc->cdev != NULL, ("devmem cdev already destroyed"));
		destroy_dev(dsc->cdev);
		devmem_destroy(dsc);
	}
	destroy_dev(cdev);
	vmmdev_destroy(sc);
	error = 0;

out:
	free(buf, M_VMMDEV);
	return (error);
}
SYSCTL_PROC(_hw_vmm, OID_AUTO, destroy,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_vmm_destroy, "A",
    NULL);

static struct cdevsw vmmdevsw = {
	.d_name		= "vmmdev",
	.d_version	= D_VERSION,
	.d_ioctl	= vmmdev_ioctl,
	.d_mmap_single	= vmmdev_mmap_single,
	.d_read		= vmmdev_rw,
	.d_write	= vmmdev_rw,
};

static int
sysctl_vmm_create(SYSCTL_HANDLER_ARGS)
{
	struct vm *vm;
	struct cdev *cdev;
	struct vmmdev_softc *sc, *sc2;
	char *buf;
	int error, buflen;

	error = vmm_priv_check(req->td->td_ucred);
	if (error)
		return (error);

	buflen = VM_MAX_NAMELEN + 1;
	buf = malloc(buflen, M_VMMDEV, M_WAITOK | M_ZERO);
	strlcpy(buf, "beavis", buflen);
	error = sysctl_handle_string(oidp, buf, buflen, req);
	if (error != 0 || req->newptr == NULL)
		goto out;

	mtx_lock(&vmmdev_mtx);
	sc = vmmdev_lookup(buf);
	mtx_unlock(&vmmdev_mtx);
	if (sc != NULL) {
		error = EEXIST;
		goto out;
	}

	error = vm_create(buf, &vm);
	if (error != 0)
		goto out;

	sc = malloc(sizeof(struct vmmdev_softc), M_VMMDEV, M_WAITOK | M_ZERO);
	sc->ucred = crhold(curthread->td_ucred);
	sc->vm = vm;
	SLIST_INIT(&sc->devmem);

	/*
	 * Lookup the name again just in case somebody sneaked in when we
	 * dropped the lock.
	 */
	mtx_lock(&vmmdev_mtx);
	sc2 = vmmdev_lookup(buf);
	if (sc2 == NULL) {
		SLIST_INSERT_HEAD(&head, sc, link);
		sc->flags |= VSC_LINKED;
	}
	mtx_unlock(&vmmdev_mtx);

	if (sc2 != NULL) {
		vmmdev_destroy(sc);
		error = EEXIST;
		goto out;
	}

	error = make_dev_p(MAKEDEV_CHECKNAME, &cdev, &vmmdevsw, sc->ucred,
	    UID_ROOT, GID_WHEEL, 0600, "vmm/%s", buf);
	if (error != 0) {
		vmmdev_destroy(sc);
		goto out;
	}

	mtx_lock(&vmmdev_mtx);
	sc->cdev = cdev;
	sc->cdev->si_drv1 = sc;
	mtx_unlock(&vmmdev_mtx);

out:
	free(buf, M_VMMDEV);
	return (error);
}
SYSCTL_PROC(_hw_vmm, OID_AUTO, create,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_vmm_create, "A",
    NULL);

void
vmmdev_init(void)
{
	pr_allow_flag = prison_add_allow(NULL, "vmm", NULL,
	    "Allow use of vmm in a jail.");
}

int
vmmdev_cleanup(void)
{
	int error;

	if (SLIST_EMPTY(&head))
		error = 0;
	else
		error = EBUSY;

	return (error);
}

static int
devmem_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t len,
    struct vm_object **objp, int nprot)
{
	struct devmem_softc *dsc;
	vm_ooffset_t first, last;
	size_t seglen;
	int error;
	bool sysmem;

	dsc = cdev->si_drv1;
	if (dsc == NULL) {
		/* 'cdev' has been created but is not ready for use */
		return (ENXIO);
	}

	first = *offset;
	last = *offset + len;
	if ((nprot & PROT_EXEC) || first < 0 || first >= last)
		return (EINVAL);

	vm_slock_memsegs(dsc->sc->vm);

	error = vm_get_memseg(dsc->sc->vm, dsc->segid, &seglen, &sysmem, objp);
	KASSERT(error == 0 && !sysmem && *objp != NULL,
	    ("%s: invalid devmem segment %d", __func__, dsc->segid));

	if (seglen >= last)
		vm_object_reference(*objp);
	else
		error = EINVAL;

	vm_unlock_memsegs(dsc->sc->vm);
	return (error);
}

static struct cdevsw devmemsw = {
	.d_name		= "devmem",
	.d_version	= D_VERSION,
	.d_mmap_single	= devmem_mmap_single,
};

static int
devmem_create_cdev(const char *vmname, int segid, char *devname)
{
	struct devmem_softc *dsc;
	struct vmmdev_softc *sc;
	struct cdev *cdev;
	int error;

	error = make_dev_p(MAKEDEV_CHECKNAME, &cdev, &devmemsw, NULL,
	    UID_ROOT, GID_WHEEL, 0600, "vmm.io/%s.%s", vmname, devname);
	if (error)
		return (error);

	dsc = malloc(sizeof(struct devmem_softc), M_VMMDEV, M_WAITOK | M_ZERO);

	mtx_lock(&vmmdev_mtx);
	sc = vmmdev_lookup(vmname);
	KASSERT(sc != NULL, ("%s: vm %s softc not found", __func__, vmname));
	if (sc->cdev == NULL) {
		/* virtual machine is being created or destroyed */
		mtx_unlock(&vmmdev_mtx);
		free(dsc, M_VMMDEV);
		destroy_dev_sched_cb(cdev, NULL, 0);
		return (ENODEV);
	}

	dsc->segid = segid;
	dsc->name = devname;
	dsc->cdev = cdev;
	dsc->sc = sc;
	SLIST_INSERT_HEAD(&sc->devmem, dsc, link);
	mtx_unlock(&vmmdev_mtx);

	/* The 'cdev' is ready for use after 'si_drv1' is initialized */
	cdev->si_drv1 = dsc;
	return (0);
}

static void
devmem_destroy(void *arg)
{
	struct devmem_softc *dsc = arg;

	KASSERT(dsc->cdev, ("%s: devmem cdev already destroyed", __func__));
	dsc->cdev = NULL;
	dsc->sc = NULL;
}
