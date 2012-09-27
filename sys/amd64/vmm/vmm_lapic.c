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
#include "vmm_instruction_emul.h"

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

int
lapic_mmio(struct vm *vm, int cpu, u_int offset, int read, struct vie *vie)
{
	int handled, error;
	uint64_t val;
	struct vlapic *vlapic;

	const int UNHANDLED = 0;

	vlapic = vm_lapic(vm, cpu);

	/* Only 32-bit accesses to local apic */
	if (vie->op_size != VIE_OP_SIZE_32BIT)
		return (UNHANDLED);

	/*
	 * XXX
	 * The operand register in which we store the result of the
	 * read must be a GPR that we can modify even if the vcpu
	 * is "running". All the GPRs qualify except for %rsp.
	 *
	 * This is a limitation of the vm_set_register() API
	 * and can be fixed if necessary.
	 */
	if (vie->operand_register == VM_REG_GUEST_RSP)
		return (UNHANDLED);

	if (read) {
		if ((vie->opcode_flags & VIE_F_TO_REG) == 0)
			return (UNHANDLED);

		if (vie->operand_register >= VM_REG_LAST)
			return (UNHANDLED);

		handled = lapic_read(vlapic, offset, &val);
		if (handled) {
			error = vm_set_register(vm, cpu, vie->operand_register,
						val);
			if (error)
				panic("lapic_mmio: error %d setting gpr %d",
				      error, vie->operand_register);
		}
	} else {
		if ((vie->opcode_flags & VIE_F_FROM_REG) &&
		    (vie->operand_register < VM_REG_LAST)) {
			error = vm_get_register(vm, cpu, vie->operand_register,
						&val);
			if (error) {
				panic("lapic_mmio: error %d getting gpr %d",
				      error, vie->operand_register);
			}
		} else if (vie->opcode_flags & VIE_F_FROM_IMM) {
			val = vie->immediate;
		} else {
			return (UNHANDLED);
		}

		handled = lapic_write(vlapic, offset, val);
	}

	return (handled);
}
