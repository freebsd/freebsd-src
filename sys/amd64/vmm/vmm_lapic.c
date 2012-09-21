/*-
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/smp.h>

#include <x86/specialreg.h>

#include <machine/vmm.h>
#include "vmm_ipi.h"
#include "vmm_lapic.h"
#include "vlapic.h"

static int
lapic_write(struct vlapic *vlapic, u_int offset, uint64_t val)
{
	int handled;

	if (vlapic_op_mem_write(vlapic, offset, DWORD, val) == 0)
		handled = 1;
	else
		handled = 0;

	return (handled);
}

static int
lapic_read(struct vlapic *vlapic, u_int offset, uint64_t *rv)
{
	int handled;

	if (vlapic_op_mem_read(vlapic, offset, DWORD, rv) == 0)
		handled = 1;
	else
		handled = 0;

	return (handled);
}

int
lapic_pending_intr(struct vm *vm, int cpu)
{
	struct vlapic *vlapic;

	vlapic = vm_lapic(vm, cpu);

	return (vlapic_pending_intr(vlapic));
}

void
lapic_intr_accepted(struct vm *vm, int cpu, int vector)
{
	struct vlapic *vlapic;

	vlapic = vm_lapic(vm, cpu);

	vlapic_intr_accepted(vlapic, vector);
}

int
lapic_set_intr(struct vm *vm, int cpu, int vector)
{
	struct vlapic *vlapic;

	if (cpu < 0 || cpu >= VM_MAXCPU)
		return (EINVAL);

	if (vector < 32 || vector > 255)
		return (EINVAL);

	vlapic = vm_lapic(vm, cpu);
	vlapic_set_intr_ready(vlapic, vector);

	vm_interrupt_hostcpu(vm, cpu);

	return (0);
}

void
lapic_timer_tick(struct vm *vm, int cpu)
{
	struct vlapic *vlapic;

	vlapic = vm_lapic(vm, cpu);

	vlapic_timer_tick(vlapic);
}

static boolean_t
x2apic_msr(u_int msr)
{
	if (msr >= 0x800 && msr <= 0xBFF)
		return (TRUE);
	else
		return (FALSE);
}

static u_int
x2apic_msr_to_regoff(u_int msr)
{

	return ((msr - 0x800) << 4);
}

boolean_t
lapic_msr(u_int msr)
{

	if (x2apic_msr(msr) || (msr == MSR_APICBASE))
		return (TRUE);
	else
		return (FALSE);
}

int
lapic_rdmsr(struct vm *vm, int cpu, u_int msr, uint64_t *rval)
{
	int handled;
	struct vlapic *vlapic;

	vlapic = vm_lapic(vm, cpu);

	if (msr == MSR_APICBASE) {
		*rval = vlapic_get_apicbase(vlapic);
		handled = 1;
	} else
		handled = lapic_read(vlapic, x2apic_msr_to_regoff(msr), rval);

	return (handled);
}

int
lapic_wrmsr(struct vm *vm, int cpu, u_int msr, uint64_t val)
{
	int handled;
	struct vlapic *vlapic;

	vlapic = vm_lapic(vm, cpu);

	if (msr == MSR_APICBASE) {
		vlapic_set_apicbase(vlapic, val);
		handled = 1;
	} else
		handled = lapic_write(vlapic, x2apic_msr_to_regoff(msr), val);

	return (handled);
}
