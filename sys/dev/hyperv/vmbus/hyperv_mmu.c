/*-
 * Copyright (c) 2009-2012,2016-2024 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/kdb.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <dev/hyperv/vmbus/x86/hyperv_machdep.h>
#include <dev/hyperv/vmbus/x86/hyperv_reg.h>
#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/vmbus/hyperv_var.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>
#include <dev/hyperv/vmbus/hyperv_common_reg.h>
#include "hyperv_mmu.h"

static inline int fill_gva_list(uint64_t gva_list[],
    unsigned long start, unsigned long end)
{
	int gva_n = 0;
	unsigned long cur = start, diff;

	do {
		diff = end > cur ? end - cur : 0;

		gva_list[gva_n] = cur;
		/*
		 * Lower 12 bits encode the number of additional
		 * pages to flush (in addition to the 'cur' page).
		 */
		if (diff >= HV_TLB_FLUSH_UNIT) {
			gva_list[gva_n] |= PAGE_MASK;
			cur += HV_TLB_FLUSH_UNIT;
		}  else if (diff) {
			gva_list[gva_n] |= (diff - 1) >> PAGE_SHIFT;
			cur = end;
		}

		gva_n++;

	} while (cur < end);

	return gva_n;
}


inline int hv_cpumask_to_vpset(struct hv_vpset *vpset,
    const cpuset_t *cpus, struct vmbus_softc * sc)
{
	int cpu, vcpu, vcpu_bank, vcpu_offset, nr_bank = 1;
	int max_vcpu_bank = hv_max_vp_index / HV_VCPUS_PER_SPARSE_BANK;

	/*
	 * vpset.valid_bank_mask can represent up to
	 * HV_MAX_SPARSE_VCPU_BANKS banks
	 */
	if (max_vcpu_bank >= HV_MAX_SPARSE_VCPU_BANKS)
		return 0;

	/*
	 * Clear all banks up to the maximum possible bank as hv_tlb_flush_ex
	 * structs are not cleared between calls, we risk flushing unneeded
	 * vCPUs otherwise.
	 */
	for (vcpu_bank = 0; vcpu_bank <= max_vcpu_bank; vcpu_bank++)
		vpset->bank_contents[vcpu_bank] = 0;

	/*
	 * Some banks may end up being empty but this is acceptable.
	 */
	CPU_FOREACH_ISSET(cpu, cpus) {
		vcpu = VMBUS_PCPU_GET(sc, vcpuid, cpu);
		if (vcpu == -1)
			return -1;
		vcpu_bank = vcpu / HV_VCPUS_PER_SPARSE_BANK;
		vcpu_offset = vcpu % HV_VCPUS_PER_SPARSE_BANK;
		set_bit(vcpu_offset, (unsigned long *)
		    &vpset->bank_contents[vcpu_bank]);
		if (vcpu_bank >= nr_bank)
			nr_bank = vcpu_bank + 1;
	}
	vpset->valid_bank_mask = GENMASK_ULL(nr_bank - 1, 0);
	return nr_bank;
}




void
hv_vm_tlb_flush(pmap_t pmap, vm_offset_t addr1, vm_offset_t addr2,
    enum invl_op_codes op, struct vmbus_softc *sc, smp_invl_local_cb_t curcpu_cb)
{
	cpuset_t tmp_mask, mask;
	struct hyperv_tlb_flush *flush;
	int cpu, vcpu;
	int max_gvas, gva_n;
	uint64_t status = 0;
	uint64_t cr3;

	/*
	 * Hyper-V doesn't handle the invalidating cache. Let system handle it.
	 */
	if (op == INVL_OP_CACHE)
		return smp_targeted_tlb_shootdown_native(pmap, addr1, addr2,
		    curcpu_cb, op);

	flush = *VMBUS_PCPU_PTR(sc, cpu_mem, curcpu);
	if (flush == NULL)
		return smp_targeted_tlb_shootdown_native(pmap, addr1, addr2,
		    curcpu_cb, op);
	/*
	 * It is not necessary to signal other CPUs while booting or
	 * when in the debugger.
	 */
	if (__predict_false(kdb_active || KERNEL_PANICKED() || !smp_started))
		goto local_cb;

	KASSERT(curthread->td_pinned > 0, ("curthread not pinned"));

	/*
	 * Make a stable copy of the set of CPUs on which the pmap is active.
	 * See if we have to interrupt other CPUs.
	 */
	CPU_COPY(pmap_invalidate_cpu_mask(pmap), &tmp_mask);
	CPU_COPY(pmap_invalidate_cpu_mask(pmap), &mask);
	CPU_CLR(curcpu, &tmp_mask);
	if (CPU_EMPTY(&tmp_mask))
		goto local_cb;

	/*
	 * Initiator must have interrupts enabled, which prevents
	 * non-invalidation IPIs that take smp_ipi_mtx spinlock,
	 * from deadlocking with us.  On the other hand, preemption
	 * must be disabled to pin initiator to the instance of the
	 * pcpu pc_smp_tlb data and scoreboard line.
	 */
	KASSERT((read_rflags() & PSL_I) != 0,
	    ("hv_tlb_flush: interrupts disabled"));
	critical_enter();
	flush->processor_mask = 0;
	cr3 = pmap->pm_cr3;

	if (op == INVL_OP_TLB || op == INVL_OP_TLB_INVPCID ||
	    op == INVL_OP_TLB_INVPCID_PTI || op == INVL_OP_TLB_PCID) {
		flush->address_space = 0;
		flush->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
	} else {

		flush->address_space = cr3;
		flush->address_space &= ~CR3_PCID_MASK;
		flush->flags = 0;
	}
	if(CPU_CMP(&mask, &all_cpus) == 0) {
		flush->flags |= HV_FLUSH_ALL_PROCESSORS;
	} else {
		if (CPU_FLS(&mask) < mp_ncpus && CPU_FLS(&mask) >= 64)
			goto do_ex_hypercall;

		CPU_FOREACH_ISSET(cpu, &mask) {
			vcpu = VMBUS_PCPU_GET(sc, vcpuid, cpu);
			if (vcpu >= 64)
				goto do_ex_hypercall;

			set_bit(vcpu, &flush->processor_mask);
		}
		if (!flush->processor_mask )
			goto native;
	}
	max_gvas = (PAGE_SIZE - sizeof(*flush)) / sizeof(flush->gva_list[0]);
	if (addr2 == 0) {
		flush->flags |= HV_FLUSH_NON_GLOBAL_MAPPINGS_ONLY;
		status = hypercall_do_md(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE,
		    (uint64_t)flush, (uint64_t)NULL);
	} else if ((addr2 && (addr2 -addr1)/HV_TLB_FLUSH_UNIT) > max_gvas) {
		status = hypercall_do_md(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE,
		    (uint64_t)flush, (uint64_t)NULL);
	} else {
		gva_n = fill_gva_list(flush->gva_list, addr1, addr2);

		status = hv_do_rep_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST,
		    gva_n, 0, (uint64_t)flush, (uint64_t)NULL);

	}
	if(status)
		goto native;
	sched_unpin();
	critical_exit();
	return;

local_cb:
	critical_enter();
	curcpu_cb(pmap, addr1, addr2);
	sched_unpin();
	critical_exit();
	return;
do_ex_hypercall:
	status = hv_flush_tlb_others_ex(pmap, addr1, addr2, mask, op, sc);
	if (status)
		goto native;
	sched_unpin();
	critical_exit();
	return;
native:
	critical_exit();
	return smp_targeted_tlb_shootdown_native(pmap, addr1,
	    addr2, curcpu_cb, op);
}

uint64_t
hv_flush_tlb_others_ex(pmap_t pmap, vm_offset_t addr1, vm_offset_t addr2,
    const cpuset_t mask, enum invl_op_codes op, struct vmbus_softc *sc)
{
	int nr_bank = 0, max_gvas, gva_n;
	struct hv_tlb_flush_ex *flush;
	if(*VMBUS_PCPU_PTR(sc, cpu_mem, curcpu) == NULL)
		return EINVAL;
	flush = *VMBUS_PCPU_PTR(sc, cpu_mem, curcpu);
	uint64_t status = 0;
	uint64_t cr3;

	if (!(hyperv_recommends & HYPERV_X64_EX_PROCESSOR_MASKS_RECOMMENDED))
	       return EINVAL;

	cr3 = pmap->pm_cr3;
	if (op == INVL_OP_TLB) {
		flush->address_space = 0;
		flush->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
	} else {

		flush->address_space = cr3;
		flush->address_space &= ~CR3_PCID_MASK;
		flush->flags = 0;
	}

	flush->hv_vp_set.valid_bank_mask = 0;

	flush->hv_vp_set.format = HV_GENERIC_SET_SPARSE_4K;
	nr_bank = hv_cpumask_to_vpset(&flush->hv_vp_set, &mask, sc);
	if (nr_bank < 0)
		return EINVAL;

	/*
	 * We can flush not more than max_gvas with one hypercall. Flush the
	 * whole address space if we were asked to do more.
	 */
	max_gvas = (PAGE_SIZE - sizeof(*flush) - nr_bank *
	    sizeof(flush->hv_vp_set.bank_contents[0])) /
	    sizeof(flush->hv_vp_set.bank_contents[0]);

	if (addr2 == 0) {
		flush->flags |= HV_FLUSH_NON_GLOBAL_MAPPINGS_ONLY;
		status = hv_do_rep_hypercall(
				HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE_EX,
				0, nr_bank, (uint64_t)flush, (uint64_t)NULL);
	} else if (addr2 &&
	    ((addr2 - addr1)/HV_TLB_FLUSH_UNIT) > max_gvas) {
		status = hv_do_rep_hypercall(
		    HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE_EX,
		    0, nr_bank, (uint64_t)flush, (uint64_t)NULL);
	} else {
		gva_n = fill_gva_list(&flush->hv_vp_set.bank_contents[nr_bank],
		    addr1, addr2);
		status = hv_do_rep_hypercall(
		    HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST_EX,
		    gva_n, nr_bank, (uint64_t)flush, (uint64_t)NULL);
	}
	return status;
}
