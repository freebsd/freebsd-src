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

#include <machine/specialreg.h>

#include "vmmapi.h"
#include "internal.h"

/*
 * From Intel Vol 3a:
 * Table 9-1. IA-32 Processor States Following Power-up, Reset or INIT
 */
int
vcpu_reset(struct vcpu *vcpu)
{
	int error;
	uint64_t rflags, rip, cr0, cr4, zero, desc_base, rdx;
	uint32_t desc_access, desc_limit;
	uint16_t sel;

	zero = 0;

	rflags = 0x2;
	error = vm_set_register(vcpu, VM_REG_GUEST_RFLAGS, rflags);
	if (error)
		goto done;

	rip = 0xfff0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RIP, rip)) != 0)
		goto done;

	/*
	 * According to Intels Software Developer Manual CR0 should be
	 * initialized with CR0_ET | CR0_NW | CR0_CD but that crashes some
	 * guests like Windows.
	 */
	cr0 = CR0_NE;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CR0, cr0)) != 0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CR2, zero)) != 0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CR3, zero)) != 0)
		goto done;

	cr4 = 0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CR4, cr4)) != 0)
		goto done;

	/*
	 * CS: present, r/w, accessed, 16-bit, byte granularity, usable
	 */
	desc_base = 0xffff0000;
	desc_limit = 0xffff;
	desc_access = 0x0093;
	error = vm_set_desc(vcpu, VM_REG_GUEST_CS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	sel = 0xf000;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CS, sel)) != 0)
		goto done;

	/*
	 * SS,DS,ES,FS,GS: present, r/w, accessed, 16-bit, byte granularity
	 */
	desc_base = 0;
	desc_limit = 0xffff;
	desc_access = 0x0093;
	error = vm_set_desc(vcpu, VM_REG_GUEST_SS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_DS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_ES,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_FS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_GS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	sel = 0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_SS, sel)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_DS, sel)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_ES, sel)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_FS, sel)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_GS, sel)) != 0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_EFER, zero)) != 0)
		goto done;

	/* General purpose registers */
	rdx = 0xf00;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RAX, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RBX, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RCX, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RDX, rdx)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RSI, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RDI, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RBP, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RSP, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R8, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R9, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R10, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R11, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R12, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R13, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R14, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R15, zero)) != 0)
		goto done;

	/* GDTR, IDTR */
	desc_base = 0;
	desc_limit = 0xffff;
	desc_access = 0;
	error = vm_set_desc(vcpu, VM_REG_GUEST_GDTR,
			    desc_base, desc_limit, desc_access);
	if (error != 0)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_IDTR,
			    desc_base, desc_limit, desc_access);
	if (error != 0)
		goto done;

	/* TR */
	desc_base = 0;
	desc_limit = 0xffff;
	desc_access = 0x0000008b;
	error = vm_set_desc(vcpu, VM_REG_GUEST_TR, 0, 0, desc_access);
	if (error)
		goto done;

	sel = 0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_TR, sel)) != 0)
		goto done;

	/* LDTR */
	desc_base = 0;
	desc_limit = 0xffff;
	desc_access = 0x00000082;
	error = vm_set_desc(vcpu, VM_REG_GUEST_LDTR, desc_base,
			    desc_limit, desc_access);
	if (error)
		goto done;

	sel = 0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_LDTR, 0)) != 0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_DR6,
		 0xffff0ff0)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_DR7, 0x400)) !=
	    0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_INTR_SHADOW,
		 zero)) != 0)
		goto done;

	error = 0;
done:
	return (error);
}
