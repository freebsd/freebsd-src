/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
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

#include <machine/machdep.h>
#include <machine/vmparam.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <machine/md_var.h>
#include <machine/sbi.h>

#include "riscv.h"
#include "vmm_fence.h"

static int
vmm_sbi_handle_rfnc(struct vcpu *vcpu, struct hypctx *hypctx)
{
	struct vmm_fence fence;
	uint64_t hart_mask;
	uint64_t hart_mask_base;
	uint64_t func_id;
	struct hyp *hyp;
	uint16_t maxcpus;
	cpuset_t cpus;
	int vcpu_id;
	int i;

	func_id = hypctx->guest_regs.hyp_a[6];
	hart_mask = hypctx->guest_regs.hyp_a[0];
	hart_mask_base = hypctx->guest_regs.hyp_a[1];

	/* Construct vma_fence. */

	fence.start = hypctx->guest_regs.hyp_a[2];
	fence.size = hypctx->guest_regs.hyp_a[3];
	fence.asid = hypctx->guest_regs.hyp_a[4];

	switch (func_id) {
	case SBI_RFNC_REMOTE_FENCE_I:
		fence.type = VMM_RISCV_FENCE_I;
		break;
	case SBI_RFNC_REMOTE_SFENCE_VMA:
		fence.type = VMM_RISCV_FENCE_VMA;
		break;
	case SBI_RFNC_REMOTE_SFENCE_VMA_ASID:
		fence.type = VMM_RISCV_FENCE_VMA_ASID;
		break;
	default:
		return (-1);
	}

	/* Construct cpuset_t from the mask supplied. */

	CPU_ZERO(&cpus);
	hyp = hypctx->hyp;
	maxcpus = vm_get_maxcpus(hyp->vm);
	for (i = 0; i < maxcpus; i++) {
		vcpu = vm_vcpu(hyp->vm, i);
		if (vcpu == NULL)
			continue;
		vcpu_id = vcpu_vcpuid(vcpu);
		if (hart_mask_base != -1UL) {
			if (vcpu_id < hart_mask_base)
				continue;
			if (!(hart_mask & (1UL << (vcpu_id - hart_mask_base))))
				continue;
		}
		CPU_SET(i, &cpus);
	}

	vmm_fence_add(hyp->vm, &cpus, &fence);

	return (0);
}

static int
vmm_sbi_handle_time(struct vcpu *vcpu, struct hypctx *hypctx)
{
	uint64_t func_id;
	uint64_t next_val;
	int ret;

	func_id = hypctx->guest_regs.hyp_a[6];
	next_val = hypctx->guest_regs.hyp_a[0];

	switch (func_id) {
	case SBI_TIME_SET_TIMER:
		vtimer_set_timer(hypctx, next_val);
		ret = 0;
		break;
	default:
		ret = -1;
		break;
	}

	hypctx->guest_regs.hyp_a[0] = ret;

	return (0);
}

static int
vmm_sbi_handle_ipi(struct vcpu *vcpu, struct hypctx *hypctx)
{
	struct hypctx *target_hypctx;
	struct vcpu *target_vcpu __unused;
	cpuset_t active_cpus;
	struct hyp *hyp;
	uint64_t hart_mask;
	uint64_t hart_mask_base;
	uint64_t func_id;
	int hart_id;
	int bit;
	int ret;

	func_id = hypctx->guest_regs.hyp_a[6];
	hart_mask = hypctx->guest_regs.hyp_a[0];
	hart_mask_base = hypctx->guest_regs.hyp_a[1];

	dprintf("%s: hart_mask %lx\n", __func__, hart_mask);

	hyp = hypctx->hyp;

	active_cpus = vm_active_cpus(hyp->vm);

	switch (func_id) {
	case SBI_IPI_SEND_IPI:
		while ((bit = ffs(hart_mask))) {
			hart_id = (bit - 1);
			hart_mask &= ~(1u << hart_id);
			if (hart_mask_base != -1)
				hart_id += hart_mask_base;
			if (CPU_ISSET(hart_id, &active_cpus)) {
				/* TODO. */
				target_vcpu = vm_vcpu(hyp->vm, hart_id);
				target_hypctx = hypctx->hyp->ctx[hart_id];
				riscv_send_ipi(target_hypctx, hart_id);
			}
		}
		ret = 0;
		break;
	default:
		printf("%s: unknown func %ld\n", __func__, func_id);
		ret = -1;
		break;
	}

	hypctx->guest_regs.hyp_a[0] = ret;

	return (0);
}

int
vmm_sbi_ecall(struct vcpu *vcpu, bool *retu)
{
	int sbi_extension_id __unused;
	struct hypctx *hypctx;
	int error;

	hypctx = riscv_get_active_vcpu();
	sbi_extension_id = hypctx->guest_regs.hyp_a[7];

	dprintf("%s: args %lx %lx %lx %lx %lx %lx %lx %lx\n", __func__,
	    hypctx->guest_regs.hyp_a[0],
	    hypctx->guest_regs.hyp_a[1],
	    hypctx->guest_regs.hyp_a[2],
	    hypctx->guest_regs.hyp_a[3],
	    hypctx->guest_regs.hyp_a[4],
	    hypctx->guest_regs.hyp_a[5],
	    hypctx->guest_regs.hyp_a[6],
	    hypctx->guest_regs.hyp_a[7]);

	switch (sbi_extension_id) {
	case SBI_EXT_ID_RFNC:
		error = vmm_sbi_handle_rfnc(vcpu, hypctx);
		hypctx->guest_regs.hyp_a[0] = error;
		break;
	case SBI_EXT_ID_TIME:
		vmm_sbi_handle_time(vcpu, hypctx);
		break;
	case SBI_EXT_ID_IPI:
		vmm_sbi_handle_ipi(vcpu, hypctx);
		break;
	default:
		*retu = true;
		break;
	}

	return (0);
}
