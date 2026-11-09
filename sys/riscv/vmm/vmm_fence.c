/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Ruslan Bukin <br@bsdpad.com>
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
#include <sys/systm.h>
#include <sys/smp.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <machine/vmm.h>

#include <dev/vmm/vmm_vm.h>

#include "riscv.h"
#include "vmm_fence.h"

static bool
vmm_fence_dequeue(struct hypctx *hypctx, struct vmm_fence *new_fence)
{
	struct vmm_fence *queue;
	struct vmm_fence *fence;

	mtx_lock_spin(&hypctx->fence_queue_mtx);
	queue = hypctx->fence_queue;
	fence = &queue[hypctx->fence_queue_head];
	if (fence->type != VMM_RISCV_FENCE_INVALID) {
		*new_fence = *fence;
		fence->type = VMM_RISCV_FENCE_INVALID;
		hypctx->fence_queue_head =
		    (hypctx->fence_queue_head + 1) % VMM_FENCE_QUEUE_SIZE;
	} else {
		mtx_unlock_spin(&hypctx->fence_queue_mtx);
		return (false);
	}
	mtx_unlock_spin(&hypctx->fence_queue_mtx);

	return (true);
}

static bool
vmm_fence_enqueue(struct hypctx *hypctx, struct vmm_fence *new_fence)
{
	struct vmm_fence *queue;
	struct vmm_fence *fence;

	mtx_lock_spin(&hypctx->fence_queue_mtx);
	queue = hypctx->fence_queue;
	fence = &queue[hypctx->fence_queue_tail];
	if (fence->type == VMM_RISCV_FENCE_INVALID) {
		*fence = *new_fence;
		hypctx->fence_queue_tail =
		    (hypctx->fence_queue_tail + 1) % VMM_FENCE_QUEUE_SIZE;
	} else {
		mtx_unlock_spin(&hypctx->fence_queue_mtx);
		return (false);
	}
	mtx_unlock_spin(&hypctx->fence_queue_mtx);

	return (true);
}

static void
vmm_fence_process_one(struct vmm_fence *fence)
{
	uint64_t va, start, end;

	KASSERT(fence->type == VMM_RISCV_FENCE_I ||
	    fence->type == VMM_RISCV_FENCE_VMA ||
	    fence->type == VMM_RISCV_FENCE_VMA_ASID,
	    ("%s: wrong fence type %d", __func__, fence->type));

	if (fence->type == VMM_RISCV_FENCE_I) {
		fence_i();
		return;
	}

	/*
	 * If both start and size are 0, size is -1, or the range exceeds
	 * the threshold, invalidate the entire address space.
	 */
	if ((fence->start == 0 && fence->size == 0) ||
	    fence->size == (uint64_t)-1 || fence->size >= (128 * PAGE_SIZE)) {
		if (fence->type == VMM_RISCV_FENCE_VMA_ASID)
			hfence_vma_asid(fence->asid);
		else
			hfence_vma();
		return;
	}

	start = rounddown2(fence->start, PAGE_SIZE);
	end = roundup2(fence->start + fence->size, PAGE_SIZE);

	switch (fence->type) {
	case VMM_RISCV_FENCE_VMA:
		for (va = start; va < end; va += PAGE_SIZE)
			hfence_vma_page(va);
		break;
	case VMM_RISCV_FENCE_VMA_ASID:
		for (va = start; va < end; va += PAGE_SIZE)
			hfence_vma_asid_page(fence->asid, va);
		break;
	default:
		break;
	}
}

void
vmm_fence_process(struct hypctx *hypctx)
{
	struct vmm_fence fence;
	int pending;

	pending = atomic_readandclear_32(&hypctx->fence_req);

	KASSERT((pending & ~(FENCE_REQ_I | FENCE_REQ_VMA)) == 0,
	    ("wrong fence bit mask"));

	if (pending & FENCE_REQ_I)
		fence_i();

	if (pending & FENCE_REQ_VMA)
		hfence_vma();

	while (vmm_fence_dequeue(hypctx, &fence) == true)
		vmm_fence_process_one(&fence);
}

static void
vmm_fence_rendezvous_action(void *arg)
{
	struct vmm_fence *fence;

	fence = arg;
	vmm_fence_process_one(fence);
}

void
vmm_fence_add(struct vm *vm, cpuset_t *cpus, struct vmm_fence *fence)
{
	struct hypctx *hypctx;
	cpuset_t running_cpus;
	struct vcpu *vcpu;
	uint16_t maxcpus;
	int hostcpu;
	bool enq;
	int i;

	CPU_ZERO(&running_cpus);

	maxcpus = vm_get_maxcpus(vm);
	for (i = 0; i < maxcpus; i++) {
		if (!CPU_ISSET(i, cpus))
			continue;
		vcpu = vm_vcpu(vm, i);
		hypctx = vcpu_get_cookie(vcpu);

		enq = false;

		/* No need to enqueue fences i and vma global. */
		switch (fence->type) {
		case VMM_RISCV_FENCE_I:
			atomic_set_32(&hypctx->fence_req, FENCE_REQ_I);
			break;
		case VMM_RISCV_FENCE_VMA:
			if ((fence->start == 0 && fence->size == 0) ||
			    fence->size == -1)
				atomic_set_32(&hypctx->fence_req,
				    FENCE_REQ_VMA);
			else
				enq = true;
			break;
		case VMM_RISCV_FENCE_VMA_ASID:
			enq = true;
			break;
		default:
			KASSERT(0, ("%s: wrong fence type %d", __func__,
			    fence->type));
			break;
		}

		/*
		 * Try to enqueue. In case of failure use more conservative
		 * request.
		 */
		if (enq)
			if (vmm_fence_enqueue(hypctx, fence) == false)
				atomic_set_32(&hypctx->fence_req,
				    FENCE_REQ_VMA);

		mb();

		if (vcpu_is_running(vcpu, &hostcpu))
			CPU_SET(hostcpu, &running_cpus);
	}

	/*
	 * Synchronously execute the fence on all running target CPUs via
	 * SMP rendezvous, ensuring the fence has taken effect before the
	 * SBI remote fence call returns to the calling hart.
	 */
	if (!CPU_EMPTY(&running_cpus)) {
		smp_rendezvous_cpus(running_cpus, smp_no_rendezvous_barrier,
		    vmm_fence_rendezvous_action, smp_no_rendezvous_barrier,
		    fence);
	}
}
