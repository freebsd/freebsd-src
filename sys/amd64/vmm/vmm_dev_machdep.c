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

#include "opt_bhyve_snapshot.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/libkern.h>
#include <sys/ioccom.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/vmparam.h>
#include <machine/vmm.h>
#include <machine/vmm_instruction_emul.h>
#include <machine/vmm_snapshot.h>
#include <x86/apicreg.h>

#include <dev/vmm/vmm_dev.h>
#include <dev/vmm/vmm_stat.h>

#include "vmm_lapic.h"
#include "vmm_mem.h"
#include "io/ppt.h"
#include "io/vatpic.h"
#include "io/vioapic.h"
#include "io/vhpet.h"
#include "io/vrtc.h"

#ifdef COMPAT_FREEBSD13
struct vm_stats_13 {
	int		cpuid;				/* in */
	int		num_entries;			/* out */
	struct timeval	tv;
	uint64_t	statbuf[MAX_VM_STATS];
};

#define	VM_STATS_13	_IOWR('v', IOCNUM_VM_STATS, struct vm_stats_13)

struct vm_snapshot_meta_13 {
	void *ctx;			/* unused */
	void *dev_data;
	const char *dev_name;      /* identify userspace devices */
	enum snapshot_req dev_req; /* identify kernel structs */

	struct vm_snapshot_buffer buffer;

	enum vm_snapshot_op op;
};

#define VM_SNAPSHOT_REQ_13 \
	_IOWR('v', IOCNUM_SNAPSHOT_REQ, struct vm_snapshot_meta_13)

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

const struct vmmdev_ioctl vmmdev_machdep_ioctls[] = {
	VMMDEV_IOCTL(VM_RUN, VMMDEV_IOCTL_LOCK_ONE_VCPU),
#ifdef COMPAT_FREEBSD13
	VMMDEV_IOCTL(VM_RUN_13, VMMDEV_IOCTL_LOCK_ONE_VCPU),
#endif
	VMMDEV_IOCTL(VM_GET_SEGMENT_DESCRIPTOR, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_SET_SEGMENT_DESCRIPTOR, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_INJECT_EXCEPTION, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_SET_X2APIC_STATE, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_GLA2GPA, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_GLA2GPA_NOFAULT, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_SET_INTINFO, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_GET_INTINFO, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_RESTART_INSTRUCTION, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_GET_KERNEMU_DEV, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_SET_KERNEMU_DEV, VMMDEV_IOCTL_LOCK_ONE_VCPU),

	VMMDEV_IOCTL(VM_BIND_PPTDEV,
	    VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_LOCK_ALL_VCPUS),
	VMMDEV_IOCTL(VM_UNBIND_PPTDEV,
	    VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_LOCK_ALL_VCPUS),

	VMMDEV_IOCTL(VM_MAP_PPTDEV_MMIO, VMMDEV_IOCTL_LOCK_ALL_VCPUS),
	VMMDEV_IOCTL(VM_UNMAP_PPTDEV_MMIO, VMMDEV_IOCTL_LOCK_ALL_VCPUS),
#ifdef BHYVE_SNAPSHOT
#ifdef COMPAT_FREEBSD13
	VMMDEV_IOCTL(VM_SNAPSHOT_REQ_13, VMMDEV_IOCTL_LOCK_ALL_VCPUS),
#endif
	VMMDEV_IOCTL(VM_SNAPSHOT_REQ, VMMDEV_IOCTL_LOCK_ALL_VCPUS),
	VMMDEV_IOCTL(VM_RESTORE_TIME, VMMDEV_IOCTL_LOCK_ALL_VCPUS),
#endif

#ifdef COMPAT_FREEBSD13
	VMMDEV_IOCTL(VM_STATS_13, VMMDEV_IOCTL_LOCK_ONE_VCPU),
#endif
	VMMDEV_IOCTL(VM_INJECT_NMI, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_LAPIC_IRQ, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_GET_X2APIC_STATE, VMMDEV_IOCTL_LOCK_ONE_VCPU),

	VMMDEV_IOCTL(VM_LAPIC_LOCAL_IRQ, VMMDEV_IOCTL_MAYBE_ALLOC_VCPU),

	VMMDEV_IOCTL(VM_PPTDEV_MSI, 0),
	VMMDEV_IOCTL(VM_PPTDEV_MSIX, 0),
	VMMDEV_IOCTL(VM_PPTDEV_DISABLE_MSIX, 0),
	VMMDEV_IOCTL(VM_LAPIC_MSI, 0),
	VMMDEV_IOCTL(VM_IOAPIC_ASSERT_IRQ, 0),
	VMMDEV_IOCTL(VM_IOAPIC_DEASSERT_IRQ, 0),
	VMMDEV_IOCTL(VM_IOAPIC_PULSE_IRQ, 0),
	VMMDEV_IOCTL(VM_IOAPIC_PINCOUNT, 0),
	VMMDEV_IOCTL(VM_ISA_ASSERT_IRQ, 0),
	VMMDEV_IOCTL(VM_ISA_DEASSERT_IRQ, 0),
	VMMDEV_IOCTL(VM_ISA_PULSE_IRQ, 0),
	VMMDEV_IOCTL(VM_ISA_SET_IRQ_TRIGGER, 0),
	VMMDEV_IOCTL(VM_GET_GPA_PMAP, 0),
	VMMDEV_IOCTL(VM_GET_HPET_CAPABILITIES, 0),
	VMMDEV_IOCTL(VM_RTC_READ, 0),
	VMMDEV_IOCTL(VM_RTC_WRITE, 0),
	VMMDEV_IOCTL(VM_RTC_GETTIME, 0),
	VMMDEV_IOCTL(VM_RTC_SETTIME, 0),
};
const size_t vmmdev_machdep_ioctl_count = nitems(vmmdev_machdep_ioctls);

int
vmmdev_machdep_ioctl(struct vm *vm, struct vcpu *vcpu, u_long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	struct vm_seg_desc *vmsegdesc;
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
	struct vm_pptdev *pptdev;
	struct vm_pptdev_mmio *pptmmio;
	struct vm_pptdev_msi *pptmsi;
	struct vm_pptdev_msix *pptmsix;
	struct vm_x2apic *x2apic;
	struct vm_gpa_pte *gpapte;
	struct vm_gla2gpa *gg;
	struct vm_intinfo *vmii;
	struct vm_rtc_time *rtctime;
	struct vm_rtc_data *rtcdata;
	struct vm_readwrite_kernemu_device *kernemu;
#ifdef BHYVE_SNAPSHOT
	struct vm_snapshot_meta *snapshot_meta;
#ifdef COMPAT_FREEBSD13
	struct vm_snapshot_meta_13 *snapshot_13;
#endif
#endif
	int error;

	error = 0;
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
				while (p < (uint8_t *)vmrun->cpuset +
				    vmrun->cpusetsize) {
					if (subyte(p++, 0) != 0) {
						error = EFAULT;
						break;
					}
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
	case VM_STATS_13: {
		struct vm_stats_13 *vmstats_13;

		vmstats_13 = (struct vm_stats_13 *)data;
		getmicrotime(&vmstats_13->tv);
		error = vmm_stat_copy(vcpu, 0, nitems(vmstats_13->statbuf),
		    &vmstats_13->num_entries, vmstats_13->statbuf);
		break;
	}
#endif
	case VM_PPTDEV_MSI:
		pptmsi = (struct vm_pptdev_msi *)data;
		error = ppt_setup_msi(vm,
				      pptmsi->bus, pptmsi->slot, pptmsi->func,
				      pptmsi->addr, pptmsi->msg,
				      pptmsi->numvec);
		break;
	case VM_PPTDEV_MSIX:
		pptmsix = (struct vm_pptdev_msix *)data;
		error = ppt_setup_msix(vm,
				       pptmsix->bus, pptmsix->slot,
				       pptmsix->func, pptmsix->idx,
				       pptmsix->addr, pptmsix->msg,
				       pptmsix->vector_control);
		break;
	case VM_PPTDEV_DISABLE_MSIX:
		pptdev = (struct vm_pptdev *)data;
		error = ppt_disable_msix(vm, pptdev->bus, pptdev->slot,
					 pptdev->func);
		break;
	case VM_MAP_PPTDEV_MMIO:
		pptmmio = (struct vm_pptdev_mmio *)data;
		error = ppt_map_mmio(vm, pptmmio->bus, pptmmio->slot,
				     pptmmio->func, pptmmio->gpa, pptmmio->len,
				     pptmmio->hpa);
		break;
	case VM_UNMAP_PPTDEV_MMIO:
		pptmmio = (struct vm_pptdev_mmio *)data;
		error = ppt_unmap_mmio(vm, pptmmio->bus, pptmmio->slot,
				       pptmmio->func, pptmmio->gpa, pptmmio->len);
		break;
	case VM_BIND_PPTDEV:
		pptdev = (struct vm_pptdev *)data;
		error = vm_assign_pptdev(vm, pptdev->bus, pptdev->slot,
					 pptdev->func);
		break;
	case VM_UNBIND_PPTDEV:
		pptdev = (struct vm_pptdev *)data;
		error = vm_unassign_pptdev(vm, pptdev->bus, pptdev->slot,
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
		error = lapic_set_local_intr(vm, vcpu, vmirq->vector);
		break;
	case VM_LAPIC_MSI:
		vmmsi = (struct vm_lapic_msi *)data;
		error = lapic_intr_msi(vm, vmmsi->addr, vmmsi->msg);
		break;
	case VM_IOAPIC_ASSERT_IRQ:
		ioapic_irq = (struct vm_ioapic_irq *)data;
		error = vioapic_assert_irq(vm, ioapic_irq->irq);
		break;
	case VM_IOAPIC_DEASSERT_IRQ:
		ioapic_irq = (struct vm_ioapic_irq *)data;
		error = vioapic_deassert_irq(vm, ioapic_irq->irq);
		break;
	case VM_IOAPIC_PULSE_IRQ:
		ioapic_irq = (struct vm_ioapic_irq *)data;
		error = vioapic_pulse_irq(vm, ioapic_irq->irq);
		break;
	case VM_IOAPIC_PINCOUNT:
		*(int *)data = vioapic_pincount(vm);
		break;
	case VM_SET_KERNEMU_DEV:
	case VM_GET_KERNEMU_DEV: {
		mem_region_write_t mwrite;
		mem_region_read_t mread;
		int size;
		bool arg;

		kernemu = (void *)data;

		if (kernemu->access_width > 0)
			size = (1u << kernemu->access_width);
		else
			size = 1;

		if (kernemu->gpa >= DEFAULT_APIC_BASE &&
		    kernemu->gpa < DEFAULT_APIC_BASE + PAGE_SIZE) {
			mread = lapic_mmio_read;
			mwrite = lapic_mmio_write;
		} else if (kernemu->gpa >= VIOAPIC_BASE &&
		    kernemu->gpa < VIOAPIC_BASE + VIOAPIC_SIZE) {
			mread = vioapic_mmio_read;
			mwrite = vioapic_mmio_write;
		} else if (kernemu->gpa >= VHPET_BASE &&
		    kernemu->gpa < VHPET_BASE + VHPET_SIZE) {
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
		error = vatpic_assert_irq(vm, isa_irq->atpic_irq);
		if (error == 0 && isa_irq->ioapic_irq != -1)
			error = vioapic_assert_irq(vm, isa_irq->ioapic_irq);
		break;
	case VM_ISA_DEASSERT_IRQ:
		isa_irq = (struct vm_isa_irq *)data;
		error = vatpic_deassert_irq(vm, isa_irq->atpic_irq);
		if (error == 0 && isa_irq->ioapic_irq != -1)
			error = vioapic_deassert_irq(vm, isa_irq->ioapic_irq);
		break;
	case VM_ISA_PULSE_IRQ:
		isa_irq = (struct vm_isa_irq *)data;
		error = vatpic_pulse_irq(vm, isa_irq->atpic_irq);
		if (error == 0 && isa_irq->ioapic_irq != -1)
			error = vioapic_pulse_irq(vm, isa_irq->ioapic_irq);
		break;
	case VM_ISA_SET_IRQ_TRIGGER:
		isa_irq_trigger = (struct vm_isa_irq_trigger *)data;
		error = vatpic_set_irq_trigger(vm,
		    isa_irq_trigger->atpic_irq, isa_irq_trigger->trigger);
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
		pmap_get_mapping(vmspace_pmap(vm_vmspace(vm)),
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
		error = vrtc_nvram_write(vm, rtcdata->offset,
		    rtcdata->value);
		break;
	case VM_RTC_READ:
		rtcdata = (struct vm_rtc_data *)data;
		error = vrtc_nvram_read(vm, rtcdata->offset,
		    &rtcdata->value);
		break;
	case VM_RTC_SETTIME:
		rtctime = (struct vm_rtc_time *)data;
		error = vrtc_set_time(vm, rtctime->secs);
		break;
	case VM_RTC_GETTIME:
		error = 0;
		rtctime = (struct vm_rtc_time *)data;
		rtctime->secs = vrtc_get_time(vm);
		break;
	case VM_RESTART_INSTRUCTION:
		error = vm_restart_instruction(vcpu);
		break;
#ifdef BHYVE_SNAPSHOT
	case VM_SNAPSHOT_REQ:
		snapshot_meta = (struct vm_snapshot_meta *)data;
		error = vm_snapshot_req(vm, snapshot_meta);
		break;
#ifdef COMPAT_FREEBSD13
	case VM_SNAPSHOT_REQ_13:
		/*
		 * The old structure just has an additional pointer at
		 * the start that is ignored.
		 */
		snapshot_13 = (struct vm_snapshot_meta_13 *)data;
		snapshot_meta =
		    (struct vm_snapshot_meta *)&snapshot_13->dev_data;
		error = vm_snapshot_req(vm, snapshot_meta);
		break;
#endif
	case VM_RESTORE_TIME:
		error = vm_restore_time(vm);
		break;
#endif
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}
