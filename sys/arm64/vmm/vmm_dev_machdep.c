/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (C) 2015 Mihai Carabas <mihai.carabas@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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

#include <machine/machdep.h>
#include <machine/vmparam.h>
#include <machine/vmm.h>

#include <dev/vmm/vmm_dev.h>
#include <dev/vmm/vmm_mem.h>

#include "io/vgic.h"

const struct vmmdev_ioctl vmmdev_machdep_ioctls[] = {
	VMMDEV_IOCTL(VM_RUN, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_INJECT_EXCEPTION, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_GLA2GPA_NOFAULT, VMMDEV_IOCTL_LOCK_ONE_VCPU),

	VMMDEV_IOCTL(VM_ATTACH_VGIC,
	    VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_LOCK_ALL_VCPUS),

	VMMDEV_IOCTL(VM_GET_VGIC_VERSION, 0),
	VMMDEV_IOCTL(VM_RAISE_MSI, 0),
	VMMDEV_IOCTL(VM_ASSERT_IRQ, 0),
	VMMDEV_IOCTL(VM_DEASSERT_IRQ, 0),
};
const size_t vmmdev_machdep_ioctl_count = nitems(vmmdev_machdep_ioctls);

int
vmmdev_machdep_ioctl(struct vm *vm, struct vcpu *vcpu, u_long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	struct vm_run *vmrun;
	struct vm_vgic_version *vgv;
	struct vm_vgic_descr *vgic;
	struct vm_irq *vi;
	struct vm_exception *vmexc;
	struct vm_gla2gpa *gg;
	struct vm_msi *vmsi;
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
		break;
	}
	case VM_INJECT_EXCEPTION:
		vmexc = (struct vm_exception *)data;
		error = vm_inject_exception(vcpu, vmexc->esr, vmexc->far);
		break;
	case VM_GLA2GPA_NOFAULT:
		gg = (struct vm_gla2gpa *)data;
		error = vm_gla2gpa_nofault(vcpu, &gg->paging, gg->gla,
		    gg->prot, &gg->gpa, &gg->fault);
		KASSERT(error == 0 || error == EFAULT,
		    ("%s: vm_gla2gpa unknown error %d", __func__, error));
		break;
	case VM_GET_VGIC_VERSION:
		vgv = (struct vm_vgic_version *)data;
		/* TODO: Query the vgic driver for this */
		vgv->version = 3;
		vgv->flags = 0;
		error = 0;
		break;
	case VM_ATTACH_VGIC:
		vgic = (struct vm_vgic_descr *)data;
		error = vm_attach_vgic(vm, vgic);
		break;
	case VM_RAISE_MSI:
		vmsi = (struct vm_msi *)data;
		error = vm_raise_msi(vm, vmsi->msg, vmsi->addr, vmsi->bus,
		    vmsi->slot, vmsi->func);
		break;
	case VM_ASSERT_IRQ:
		vi = (struct vm_irq *)data;
		error = vm_assert_irq(vm, vi->irq);
		break;
	case VM_DEASSERT_IRQ:
		vi = (struct vm_irq *)data;
		error = vm_deassert_irq(vm, vi->irq);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}
