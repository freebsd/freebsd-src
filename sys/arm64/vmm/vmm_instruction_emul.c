/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/pcpu.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/machdep.h>
#include <machine/vmm.h>
#else
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/_iovec.h>

#include <machine/vmm.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <vmmapi.h>
#endif

#include <machine/vmm_instruction_emul.h>

int
vmm_emulate_instruction(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
    struct vm_guest_paging *paging __unused, mem_region_read_t memread,
    mem_region_write_t memwrite, void *memarg)
{
	uint64_t val;
	int error;

	if (vie->dir == VM_DIR_READ) {
		error = memread(vcpu, gpa, &val, vie->access_size, memarg);
		if (error)
			goto out;
		error = vm_set_register(vcpu, vie->reg, val);
	} else {
		error = vm_get_register(vcpu, vie->reg, &val);
		if (error)
			goto out;
		/* Mask any unneeded bits from the register */
		if (vie->access_size < 8)
			val &= (1ul << (vie->access_size * 8)) - 1;
		error = memwrite(vcpu, gpa, val, vie->access_size, memarg);
	}

out:
	return (error);
}

int
vmm_emulate_register(struct vcpu *vcpu, struct vre *vre, reg_read_t regread,
    reg_write_t regwrite, void *regarg)
{
	uint64_t val;
	int error;

	if (vre->dir == VM_DIR_READ) {
		error = regread(vcpu, &val, regarg);
		if (error)
			goto out;
		error = vm_set_register(vcpu, vre->reg, val);
	} else {
		error = vm_get_register(vcpu, vre->reg, &val);
		if (error)
			goto out;
		error = regwrite(vcpu, val, regarg);
	}

out:
	return (error);
}
