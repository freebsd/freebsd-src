/*-
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/cpuset.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <machine/vmm.h>
#include <x86/psl.h>

#include "vatpic.h"
#include "vatpit.h"
#include "vmm_ioport.h"
#include "vmm_ktr.h"

#define	MAX_IOPORTS		1280

ioport_handler_func_t ioport_handler[MAX_IOPORTS] = {
	[TIMER_MODE] = vatpit_handler,
	[TIMER_CNTR0] = vatpit_handler,
	[TIMER_CNTR1] = vatpit_handler,
	[TIMER_CNTR2] = vatpit_handler,
	[NMISC_PORT] = vatpit_nmisc_handler,
	[IO_ICU1] = vatpic_master_handler,
	[IO_ICU1 + ICU_IMR_OFFSET] = vatpic_master_handler,
	[IO_ICU2] = vatpic_slave_handler,
	[IO_ICU2 + ICU_IMR_OFFSET] = vatpic_slave_handler,
	[IO_ELCR1] = vatpic_elc_handler,
	[IO_ELCR2] = vatpic_elc_handler,
};

#ifdef KTR
static const char *
inout_instruction(struct vm_exit *vmexit)
{
	int index;

	static const char *iodesc[] = {
		"outb", "outw", "outl",
		"inb", "inw", "inl",
		"outsb", "outsw", "outsd"
		"insb", "insw", "insd",
	};

	switch (vmexit->u.inout.bytes) {
	case 1:
		index = 0;
		break;
	case 2:
		index = 1;
		break;
	default:
		index = 2;
		break;
	}

	if (vmexit->u.inout.in)
		index += 3;

	if (vmexit->u.inout.string)
		index += 6;

	KASSERT(index < nitems(iodesc), ("%s: invalid index %d",
	    __func__, index));

	return (iodesc[index]);
}
#endif	/* KTR */

static int
emulate_inout_port(struct vm *vm, int vcpuid, struct vm_exit *vmexit,
    bool *retu)
{
	ioport_handler_func_t handler;
	uint32_t mask, val;
	int error;

	error = 0;
	*retu = true;

	if (vmexit->u.inout.port >= MAX_IOPORTS)
		goto done;

	handler = ioport_handler[vmexit->u.inout.port];
	if (handler == NULL)
		goto done;

	mask = vie_size2mask(vmexit->u.inout.bytes);

	if (!vmexit->u.inout.in) {
		val = vmexit->u.inout.eax & mask;
	}

	error = (*handler)(vm, vcpuid, vmexit->u.inout.in,
	    vmexit->u.inout.port, vmexit->u.inout.bytes, &val);

	if (!error) {
		*retu = false;
		if (vmexit->u.inout.in) {
			vmexit->u.inout.eax &= ~mask;
			vmexit->u.inout.eax |= val & mask;
			error = vm_set_register(vm, vcpuid,
			    VM_REG_GUEST_RAX, vmexit->u.inout.eax);
			KASSERT(error == 0, ("emulate_ioport: error %d "
			    "setting guest rax register", error));
		}
	}
done:
	return (error);
}

static int
emulate_inout_str(struct vm *vm, int vcpuid, struct vm_exit *vmexit, bool *retu)
{
	struct vm_inout_str *vis;
	uint64_t gla, index, segbase;
	int bytes, error, in;

	vis = &vmexit->u.inout_str;
	in = vis->inout.in;

	/*
	 * ins/outs VM exit takes precedence over the following error
	 * conditions that would ordinarily be checked by the processor:
	 *
	 * - #GP(0) due to segment being unusable.
	 * - #GP(0) due to memory operand effective address outside the limit
	 *   of the segment.
	 * - #AC(0) if alignment checking is enabled and an unaligned memory
	 *   reference is made at CPL=3
	 */

	/*
	 * XXX
	 * inout string emulation only supported in 64-bit mode and only
	 * for byte instructions.
	 *
	 * The #GP(0) fault conditions described above don't apply in
	 * 64-bit mode.
	 *
	 * The #AC(0) fault condition described above does not apply
	 * because byte accesses don't have alignment constraints.
	 */
	if (vis->cpu_mode != CPU_MODE_64BIT) { 
		VCPU_CTR1(vm, vcpuid, "ins/outs not emulated in cpu mode %d",
		    vis->cpu_mode);
		return (EINVAL);
	}

	bytes = vis->inout.bytes;
	if (bytes != 1) {
		VCPU_CTR1(vm, vcpuid, "ins/outs operand size %d not supported",
		    bytes);
		return (EINVAL);
	}

	/*
	 * XXX insb/insw/insd instructions not emulated at this time.
	 */
	if (in) {
		VCPU_CTR0(vm, vcpuid, "ins emulation not implemented");
		return (EINVAL);
	}

	segbase = vie_segbase(vis->seg_name, vis->cpu_mode, &vis->seg_desc);
	index = vis->index & vie_size2mask(vis->addrsize);
	gla = segbase + index;

	/*
	 * Verify that the computed linear address matches with the one
	 * provided by hardware.
	 */
	if (vis->gla != VIE_INVALID_GLA) {
		KASSERT(gla == vis->gla, ("%s: gla mismatch "
		    "%#lx/%#lx", __func__, gla, vis->gla));
	}
	vis->gla = gla;

	error = vmm_gla2gpa(vm, vcpuid, gla, vis->cr3, &vis->gpa,
	    vis->paging_mode, vis->cpl, in ? VM_PROT_WRITE : VM_PROT_READ);
	KASSERT(error == 0 || error == 1 || error == -1,
	    ("%s: vmm_gla2gpa unexpected error %d", __func__, error));
	if (error == -1) {
		return (EFAULT);
	} else if (error == 1) {
		return (0);	/* Resume guest to handle page fault */
	} else {
		*retu = true;
		return (0);	/* Return to userspace to finish emulation */
	}
}

int
vm_handle_inout(struct vm *vm, int vcpuid, struct vm_exit *vmexit, bool *retu)
{
	int bytes, error;

	bytes = vmexit->u.inout.bytes;
	KASSERT(bytes == 1 || bytes == 2 || bytes == 4,
	    ("vm_handle_inout: invalid operand size %d", bytes));

	if (vmexit->u.inout.string)
		error = emulate_inout_str(vm, vcpuid, vmexit, retu);
	else
		error = emulate_inout_port(vm, vcpuid, vmexit, retu);

	VCPU_CTR4(vm, vcpuid, "%s%s 0x%04x: %s",
	    vmexit->u.inout.rep ? "rep " : "",
	    inout_instruction(vmexit),
	    vmexit->u.inout.port,
	    error ? "error" : (*retu ? "userspace" : "handled"));

	return (error);
}
