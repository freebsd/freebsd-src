/*-
 * Copyright (c) 2013 Anish Gupta (akgupt3@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpuset.h>

#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/vmm.h>

#include "vmcb.h"
#include "svm.h"

/*
 * The VMCB aka Virtual Machine Control Block is a 4KB aligned page
 * in memory that describes the virtual machine.
 *
 * The VMCB contains:
 * - instructions or events in the guest to intercept
 * - control bits that modify execution environment of the guest
 * - guest processor state (e.g. general purpose registers)
 */

/*
 * Read from segment selector, control and general purpose register of VMCB.
 */
int
vmcb_read(struct vmcb *vmcb, int ident, uint64_t *retval)
{
	struct vmcb_state *state;
	struct vmcb_segment *seg;
	int err;

	state = &vmcb->state;
	err = 0;

	switch (ident) {
	case VM_REG_GUEST_CR0:
		*retval = state->cr0;
		break;

	case VM_REG_GUEST_CR2:
		*retval = state->cr2;
		break;

	case VM_REG_GUEST_CR3:
		*retval = state->cr3;
		break;

	case VM_REG_GUEST_CR4:
		*retval = state->cr4;
		break;

	case VM_REG_GUEST_DR7:
		*retval = state->dr7;
		break;

	case VM_REG_GUEST_EFER:
		*retval = state->efer;
		break;

	case VM_REG_GUEST_RAX:
		*retval = state->rax;
		break;

	case VM_REG_GUEST_RFLAGS:
		*retval = state->rflags;
		break;

	case VM_REG_GUEST_RIP:
		*retval = state->rip;
		break;

	case VM_REG_GUEST_RSP:
		*retval = state->rsp;
		break;

	case VM_REG_GUEST_CS:
	case VM_REG_GUEST_DS:
	case VM_REG_GUEST_ES:
	case VM_REG_GUEST_FS:
	case VM_REG_GUEST_GS:
	case VM_REG_GUEST_SS:
	case VM_REG_GUEST_GDTR:
	case VM_REG_GUEST_IDTR:
	case VM_REG_GUEST_LDTR:
	case VM_REG_GUEST_TR:
		seg = vmcb_seg(vmcb, ident);
		if (seg == NULL) {
			ERR("Invalid seg type %d\n", ident);
			err = EINVAL;
			break;
		}

		*retval = seg->selector;
		break;

	default:
		err =  EINVAL;
		break;
	}

	return (err);
}

/*
 * Write to segment selector, control and general purpose register of VMCB.
 */
int
vmcb_write(struct vmcb *vmcb, int ident, uint64_t val)
{
	struct vmcb_state *state;
	struct vmcb_segment *seg;
	int err;

	state = &vmcb->state;
	err = 0;

	switch (ident) {
	case VM_REG_GUEST_CR0:
		state->cr0 = val;
		break;

	case VM_REG_GUEST_CR2:
		state->cr2 = val;
		break;

	case VM_REG_GUEST_CR3:
		state->cr3 = val;
		break;

	case VM_REG_GUEST_CR4:
		state->cr4 = val;
		break;

	case VM_REG_GUEST_DR7:
		state->dr7 = val;
		break;

	case VM_REG_GUEST_EFER:
		/* EFER_SVM must always be set when the guest is executing */
		state->efer = val | EFER_SVM;
		break;

	case VM_REG_GUEST_RAX:
		state->rax = val;
		break;

	case VM_REG_GUEST_RFLAGS:
		state->rflags = val;
		break;

	case VM_REG_GUEST_RIP:
		state->rip = val;
		break;

	case VM_REG_GUEST_RSP:
		state->rsp = val;
		break;

	case VM_REG_GUEST_CS:
	case VM_REG_GUEST_DS:
	case VM_REG_GUEST_ES:
	case VM_REG_GUEST_FS:
	case VM_REG_GUEST_GS:
	case VM_REG_GUEST_SS:
	case VM_REG_GUEST_GDTR:
	case VM_REG_GUEST_IDTR:
	case VM_REG_GUEST_LDTR:
	case VM_REG_GUEST_TR:
		seg = vmcb_seg(vmcb, ident);
		if (seg == NULL) {
			ERR("Invalid segment type %d\n", ident);
			err = EINVAL;
			break;
		}

		seg->selector = val;
		break;

	default:
		err = EINVAL;
	}

	return (err);
}

/*
 * Return VMCB segment area.
 */
struct vmcb_segment *
vmcb_seg(struct vmcb *vmcb, int type)
{
	struct vmcb_state *state;
	struct vmcb_segment *seg;

	state = &vmcb->state;

	switch (type) {
	case VM_REG_GUEST_CS:
		seg = &state->cs;
		break;

	case VM_REG_GUEST_DS:
		seg = &state->ds;
		break;

	case VM_REG_GUEST_ES:
		seg = &state->es;
		break;

	case VM_REG_GUEST_FS:
		seg = &state->fs;
		break;

	case VM_REG_GUEST_GS:
		seg = &state->gs;
		break;

	case VM_REG_GUEST_SS:
		seg = &state->ss;
		break;

	case VM_REG_GUEST_GDTR:
		seg = &state->gdt;
		break;

	case VM_REG_GUEST_IDTR:
		seg = &state->idt;
		break;

	case VM_REG_GUEST_LDTR:
		seg = &state->ldt;
		break;

	case VM_REG_GUEST_TR:
		seg = &state->tr;
		break;

	default:
		seg = NULL;
		break;
	}

	return (seg);
}
