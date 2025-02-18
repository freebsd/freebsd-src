/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024-2025 Ruslan Bukin <br@bsdpad.com>
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
#include <sys/mman.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/vmem.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>

#include <machine/md_var.h>
#include <machine/riscvreg.h>
#include <machine/vm.h>
#include <machine/cpufunc.h>
#include <machine/cpu.h>
#include <machine/machdep.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <machine/atomic.h>
#include <machine/pmap.h>
#include <machine/intr.h>
#include <machine/encoding.h>
#include <machine/db_machdep.h>

#include <dev/vmm/vmm_mem.h>

#include "riscv.h"
#include "vmm_aplic.h"
#include "vmm_fence.h"
#include "vmm_stat.h"

MALLOC_DEFINE(M_HYP, "RISC-V VMM HYP", "RISC-V VMM HYP");

DPCPU_DEFINE_STATIC(struct hypctx *, vcpu);

static int
m_op(uint32_t insn, int match, int mask)
{

	if (((insn ^ match) & mask) == 0)
		return (1);

	return (0);
}

static inline void
riscv_set_active_vcpu(struct hypctx *hypctx)
{

	DPCPU_SET(vcpu, hypctx);
}

struct hypctx *
riscv_get_active_vcpu(void)
{

	return (DPCPU_GET(vcpu));
}

int
vmmops_modinit(void)
{

	if (!has_hyp) {
		printf("vmm: riscv hart doesn't support H-extension.\n");
		return (ENXIO);
	}

	return (0);
}

int
vmmops_modcleanup(void)
{

	return (0);
}

void *
vmmops_init(struct vm *vm, pmap_t pmap)
{
	struct hyp *hyp;
	vm_size_t size;

	size = round_page(sizeof(struct hyp) +
	    sizeof(struct hypctx *) * vm_get_maxcpus(vm));
	hyp = malloc_aligned(size, PAGE_SIZE, M_HYP, M_WAITOK | M_ZERO);
	hyp->vm = vm;
	hyp->aplic_attached = false;

	aplic_vminit(hyp);

	return (hyp);
}

static void
vmmops_delegate(void)
{
	uint64_t hedeleg;
	uint64_t hideleg;

	hedeleg  = (1UL << SCAUSE_INST_MISALIGNED);
	hedeleg |= (1UL << SCAUSE_ILLEGAL_INSTRUCTION);
	hedeleg |= (1UL << SCAUSE_BREAKPOINT);
	hedeleg |= (1UL << SCAUSE_ECALL_USER);
	hedeleg |= (1UL << SCAUSE_INST_PAGE_FAULT);
	hedeleg |= (1UL << SCAUSE_LOAD_PAGE_FAULT);
	hedeleg |= (1UL << SCAUSE_STORE_PAGE_FAULT);
	csr_write(hedeleg, hedeleg);

	hideleg  = (1UL << IRQ_SOFTWARE_HYPERVISOR);
	hideleg |= (1UL << IRQ_TIMER_HYPERVISOR);
	hideleg |= (1UL << IRQ_EXTERNAL_HYPERVISOR);
	csr_write(hideleg, hideleg);
}

static void
vmmops_vcpu_restore_csrs(struct hypctx *hypctx)
{
	struct hypcsr *csrs;

	csrs = &hypctx->guest_csrs;

	csr_write(vsstatus, csrs->vsstatus);
	csr_write(vsie, csrs->vsie);
	csr_write(vstvec, csrs->vstvec);
	csr_write(vsscratch, csrs->vsscratch);
	csr_write(vsepc, csrs->vsepc);
	csr_write(vscause, csrs->vscause);
	csr_write(vstval, csrs->vstval);
	csr_write(hvip, csrs->hvip);
	csr_write(vsatp, csrs->vsatp);
}

static void
vmmops_vcpu_save_csrs(struct hypctx *hypctx)
{
	struct hypcsr *csrs;

	csrs = &hypctx->guest_csrs;

	csrs->vsstatus = csr_read(vsstatus);
	csrs->vsie = csr_read(vsie);
	csrs->vstvec = csr_read(vstvec);
	csrs->vsscratch = csr_read(vsscratch);
	csrs->vsepc = csr_read(vsepc);
	csrs->vscause = csr_read(vscause);
	csrs->vstval = csr_read(vstval);
	csrs->hvip = csr_read(hvip);
	csrs->vsatp = csr_read(vsatp);
}

void *
vmmops_vcpu_init(void *vmi, struct vcpu *vcpu1, int vcpuid)
{
	struct hypctx *hypctx;
	struct hyp *hyp;
	vm_size_t size;

	hyp = vmi;

	dprintf("%s: hyp %p\n", __func__, hyp);

	KASSERT(vcpuid >= 0 && vcpuid < vm_get_maxcpus(hyp->vm),
	    ("%s: Invalid vcpuid %d", __func__, vcpuid));

	size = round_page(sizeof(struct hypctx));

	hypctx = malloc_aligned(size, PAGE_SIZE, M_HYP, M_WAITOK | M_ZERO);
	hypctx->hyp = hyp;
	hypctx->vcpu = vcpu1;
	hypctx->guest_scounteren = HCOUNTEREN_CY | HCOUNTEREN_TM;

	/* Fence queue. */
	hypctx->fence_queue = mallocarray(VMM_FENCE_QUEUE_SIZE,
	    sizeof(struct vmm_fence), M_HYP, M_WAITOK | M_ZERO);
	mtx_init(&hypctx->fence_queue_mtx, "fence queue", NULL, MTX_SPIN);

	/* sstatus */
	hypctx->guest_regs.hyp_sstatus = SSTATUS_SPP | SSTATUS_SPIE;
	hypctx->guest_regs.hyp_sstatus |= SSTATUS_FS_INITIAL;

	/* hstatus */
	hypctx->guest_regs.hyp_hstatus = HSTATUS_SPV | HSTATUS_VTW;
	hypctx->guest_regs.hyp_hstatus |= HSTATUS_SPVP;

	hypctx->cpu_id = vcpuid;
	hyp->ctx[vcpuid] = hypctx;

	aplic_cpuinit(hypctx);
	vtimer_cpuinit(hypctx);

	return (hypctx);
}

static int
riscv_vmm_pinit(pmap_t pmap)
{

	dprintf("%s: pmap %p\n", __func__, pmap);

	pmap_pinit_stage(pmap, PM_STAGE2);

	return (1);
}

struct vmspace *
vmmops_vmspace_alloc(vm_offset_t min, vm_offset_t max)
{

	return (vmspace_alloc(min, max, riscv_vmm_pinit));
}

void
vmmops_vmspace_free(struct vmspace *vmspace)
{

	pmap_remove_pages(vmspace_pmap(vmspace));
	vmspace_free(vmspace);
}

static void
riscv_unpriv_read(struct hypctx *hypctx, uintptr_t guest_addr, uint64_t *data,
    struct hyptrap *trap)
{
	register struct hyptrap * htrap asm("a0");
	uintptr_t old_hstatus;
	uintptr_t old_stvec;
	uintptr_t entry;
	uint64_t val;
	uint64_t tmp;
	int intr;

	entry = (uintptr_t)&vmm_unpriv_trap;
	htrap = trap;

	intr = intr_disable();

	old_hstatus = csr_swap(hstatus, hypctx->guest_regs.hyp_hstatus);
	/*
	 * Setup a temporary exception vector, so that if hlvx.hu raises
	 * an exception we catch it in the vmm_unpriv_trap().
	 */
	old_stvec = csr_swap(stvec, entry);

	/*
	 * Read first two bytes of instruction assuming it could be a
	 * compressed one.
	 */
	__asm __volatile(".option push\n"
			 ".option norvc\n"
			"hlvx.hu %[val], (%[addr])\n"
			".option pop\n"
	    : [val] "=r" (val)
	    : [addr] "r" (guest_addr), "r" (htrap)
	    : "a1", "memory");

	/*
	 * Check if previous hlvx.hu did not raise an exception, and then
	 * read the rest of instruction if it is a full-length one.
	 */
	if (trap->scause == -1 && (val & 0x3) == 0x3) {
		guest_addr += 2;
		__asm __volatile(".option push\n"
				 ".option norvc\n"
				"hlvx.hu %[tmp], (%[addr])\n"
				".option pop\n"
		    : [tmp] "=r" (tmp)
		    : [addr] "r" (guest_addr), "r" (htrap)
		    : "a1", "memory");
		val |= (tmp << 16);
	}

	csr_write(hstatus, old_hstatus);
	csr_write(stvec, old_stvec);

	intr_restore(intr);

	*data = val;
}

static int
riscv_gen_inst_emul_data(struct hypctx *hypctx, struct vm_exit *vme_ret,
    struct hyptrap *trap)
{
	uintptr_t guest_addr;
	struct vie *vie;
	uint64_t insn;
	int reg_num;
	int rs2, rd;
	int direction;
	int sign_extend;
	int access_size;

	guest_addr = vme_ret->sepc;

	KASSERT(vme_ret->scause == SCAUSE_FETCH_GUEST_PAGE_FAULT ||
	    vme_ret->scause == SCAUSE_LOAD_GUEST_PAGE_FAULT ||
	    vme_ret->scause == SCAUSE_STORE_GUEST_PAGE_FAULT,
	    ("Invalid scause"));

	direction = vme_ret->scause == SCAUSE_STORE_GUEST_PAGE_FAULT ?
	    VM_DIR_WRITE : VM_DIR_READ;

	sign_extend = 1;

	bzero(trap, sizeof(struct hyptrap));
	trap->scause = -1;
	riscv_unpriv_read(hypctx, guest_addr, &insn, trap);
	if (trap->scause != -1)
		return (-1);

	if ((insn & 0x3) == 0x3) {
		rs2 = (insn & RS2_MASK) >> RS2_SHIFT;
		rd = (insn & RD_MASK) >> RD_SHIFT;

		if (direction == VM_DIR_WRITE) {
			if (m_op(insn, MATCH_SB, MASK_SB))
				access_size = 1;
			else if (m_op(insn, MATCH_SH, MASK_SH))
				access_size = 2;
			else if (m_op(insn, MATCH_SW, MASK_SW))
				access_size = 4;
			else if (m_op(insn, MATCH_SD, MASK_SD))
				access_size = 8;
			else {
				printf("unknown store instr at %lx",
				    guest_addr);
				return (-2);
			}
			reg_num = rs2;
		} else {
			if (m_op(insn, MATCH_LB, MASK_LB))
				access_size = 1;
			else if (m_op(insn, MATCH_LH, MASK_LH))
				access_size = 2;
			else if (m_op(insn, MATCH_LW, MASK_LW))
				access_size = 4;
			else if (m_op(insn, MATCH_LD, MASK_LD))
				access_size = 8;
			else if (m_op(insn, MATCH_LBU, MASK_LBU)) {
				access_size = 1;
				sign_extend = 0;
			} else if (m_op(insn, MATCH_LHU, MASK_LHU)) {
				access_size = 2;
				sign_extend = 0;
			} else if (m_op(insn, MATCH_LWU, MASK_LWU)) {
				access_size = 4;
				sign_extend = 0;
			} else {
				printf("unknown load instr at %lx",
				    guest_addr);
				return (-3);
			}
			reg_num = rd;
		}
		vme_ret->inst_length = 4;
	} else {
		rs2 = (insn >> 7) & 0x7;
		rs2 += 0x8;
		rd = (insn >> 2) & 0x7;
		rd += 0x8;

		if (direction == VM_DIR_WRITE) {
			if (m_op(insn, MATCH_C_SW, MASK_C_SW))
				access_size = 4;
			else if (m_op(insn, MATCH_C_SD, MASK_C_SD))
				access_size = 8;
			else {
				printf("unknown compressed store instr at %lx",
				    guest_addr);
				return (-4);
			}
		} else  {
			if (m_op(insn, MATCH_C_LW, MASK_C_LW))
				access_size = 4;
			else if (m_op(insn, MATCH_C_LD, MASK_C_LD))
				access_size = 8;
			else {
				printf("unknown load instr at %lx", guest_addr);
				return (-5);
			}
		}
		reg_num = rd;
		vme_ret->inst_length = 2;
	}

	vme_ret->u.inst_emul.gpa = (vme_ret->htval << 2) |
	    (vme_ret->stval & 0x3);

	dprintf("guest_addr %lx insn %lx, reg %d, gpa %lx\n", guest_addr, insn,
	    reg_num, vme_ret->u.inst_emul.gpa);

	vie = &vme_ret->u.inst_emul.vie;
	vie->dir = direction;
	vie->reg = reg_num;
	vie->sign_extend = sign_extend;
	vie->access_size = access_size;

	return (0);
}

static bool
riscv_handle_world_switch(struct hypctx *hypctx, struct vm_exit *vme,
    pmap_t pmap)
{
	struct hyptrap trap;
	uint64_t insn;
	uint64_t gpa;
	bool handled;
	int ret;
	int i;

	handled = false;

	if (vme->scause & SCAUSE_INTR) {
		/*
		 * Host interrupt? Leave critical section to handle.
		 */
		vmm_stat_incr(hypctx->vcpu, VMEXIT_IRQ, 1);
		vme->exitcode = VM_EXITCODE_BOGUS;
		vme->inst_length = 0;
		return (handled);
	}

	switch (vme->scause) {
	case SCAUSE_FETCH_GUEST_PAGE_FAULT:
	case SCAUSE_LOAD_GUEST_PAGE_FAULT:
	case SCAUSE_STORE_GUEST_PAGE_FAULT:
		gpa = (vme->htval << 2) | (vme->stval & 0x3);
		if (vm_mem_allocated(hypctx->vcpu, gpa)) {
			vme->exitcode = VM_EXITCODE_PAGING;
			vme->inst_length = 0;
			vme->u.paging.gpa = gpa;
		} else {
			ret = riscv_gen_inst_emul_data(hypctx, vme, &trap);
			if (ret != 0) {
				vme->exitcode = VM_EXITCODE_HYP;
				vme->u.hyp.scause = trap.scause;
				break;
			}
			vme->exitcode = VM_EXITCODE_INST_EMUL;
		}
		break;
	case SCAUSE_ILLEGAL_INSTRUCTION:
		/*
		 * TODO: handle illegal instruction properly.
		 */
		printf("%s: Illegal instruction at %lx stval 0x%lx htval "
		    "0x%lx\n", __func__, vme->sepc, vme->stval, vme->htval);
		vmm_stat_incr(hypctx->vcpu, VMEXIT_UNHANDLED, 1);
		vme->exitcode = VM_EXITCODE_BOGUS;
		handled = false;
		break;
	case SCAUSE_VIRTUAL_SUPERVISOR_ECALL:
		handled = vmm_sbi_ecall(hypctx->vcpu);
		if (handled == true)
			break;
		for (i = 0; i < nitems(vme->u.ecall.args); i++)
			vme->u.ecall.args[i] = hypctx->guest_regs.hyp_a[i];
		vme->exitcode = VM_EXITCODE_ECALL;
		break;
	case SCAUSE_VIRTUAL_INSTRUCTION:
		insn = vme->stval;
		if (m_op(insn, MATCH_WFI, MASK_WFI))
			vme->exitcode = VM_EXITCODE_WFI;
		else
			vme->exitcode = VM_EXITCODE_BOGUS;
		handled = false;
		break;
	default:
		printf("unknown scause %lx\n", vme->scause);
		vmm_stat_incr(hypctx->vcpu, VMEXIT_UNHANDLED, 1);
		vme->exitcode = VM_EXITCODE_BOGUS;
		handled = false;
		break;
	}

	return (handled);
}

int
vmmops_gla2gpa(void *vcpui, struct vm_guest_paging *paging, uint64_t gla,
    int prot, uint64_t *gpa, int *is_fault)
{

	/* Implement me. */

	return (ENOSYS);
}

void
riscv_send_ipi(struct hyp *hyp, cpuset_t *cpus)
{
	struct hypctx *hypctx;
	struct vm *vm;
	uint16_t maxcpus;
	int i;

	vm = hyp->vm;

	maxcpus = vm_get_maxcpus(hyp->vm);
	for (i = 0; i < maxcpus; i++) {
		if (!CPU_ISSET(i, cpus))
			continue;
		hypctx = hyp->ctx[i];
		atomic_set_32(&hypctx->ipi_pending, 1);
		vcpu_notify_event(vm_vcpu(vm, i));
	}
}

int
riscv_check_ipi(struct hypctx *hypctx, bool clear)
{
	int val;

	if (clear)
		val = atomic_swap_32(&hypctx->ipi_pending, 0);
	else
		val = hypctx->ipi_pending;

	return (val);
}

bool
riscv_check_interrupts_pending(struct hypctx *hypctx)
{

	if (hypctx->interrupts_pending)
		return (true);

	return (false);
}

static void
riscv_sync_interrupts(struct hypctx *hypctx)
{
	int pending;

	pending = aplic_check_pending(hypctx);
	if (pending)
		hypctx->guest_csrs.hvip |= HVIP_VSEIP;
	else
		hypctx->guest_csrs.hvip &= ~HVIP_VSEIP;

	/* Guest clears VSSIP bit manually. */
	if (riscv_check_ipi(hypctx, true))
		hypctx->guest_csrs.hvip |= HVIP_VSSIP;

	if (riscv_check_interrupts_pending(hypctx))
		hypctx->guest_csrs.hvip |= HVIP_VSTIP;
	else
		hypctx->guest_csrs.hvip &= ~HVIP_VSTIP;

	csr_write(hvip, hypctx->guest_csrs.hvip);
}

int
vmmops_run(void *vcpui, register_t pc, pmap_t pmap, struct vm_eventinfo *evinfo)
{
	struct hypctx *hypctx;
	struct vm_exit *vme;
	struct vcpu *vcpu;
	register_t val;
	uint64_t hvip;
	bool handled;

	hypctx = (struct hypctx *)vcpui;
	vcpu = hypctx->vcpu;
	vme = vm_exitinfo(vcpu);

	hypctx->guest_regs.hyp_sepc = (uint64_t)pc;

	vmmops_delegate();

	/*
	 * From The RISC-V Instruction Set Manual
	 * Volume II: RISC-V Privileged Architectures
	 *
	 * If the new virtual machine's guest physical page tables
	 * have been modified, it may be necessary to execute an HFENCE.GVMA
	 * instruction (see Section 5.3.2) before or after writing hgatp.
	 */
	__asm __volatile("hfence.gvma" ::: "memory");

	csr_write(hgatp, pmap->pm_satp);
	if (has_sstc)
		csr_write(henvcfg, HENVCFG_STCE);
	csr_write(hie, HIE_VSEIE | HIE_VSSIE | HIE_SGEIE);
	/* TODO: should we trap rdcycle / rdtime? */
	csr_write(hcounteren, HCOUNTEREN_CY | HCOUNTEREN_TM);

	vmmops_vcpu_restore_csrs(hypctx);

	for (;;) {
		dprintf("%s: pc %lx\n", __func__, pc);

		if (hypctx->has_exception) {
			hypctx->has_exception = false;
			/*
			 * TODO: implement exception injection.
			 */
		}

		val = intr_disable();

		/* Check if the vcpu is suspended */
		if (vcpu_suspended(evinfo)) {
			intr_restore(val);
			vm_exit_suspended(vcpu, pc);
			break;
		}

		if (vcpu_debugged(vcpu)) {
			intr_restore(val);
			vm_exit_debug(vcpu, pc);
			break;
		}

		/*
		 * TODO: What happens if a timer interrupt is asserted exactly
		 * here, but for the previous VM?
		 */
		riscv_set_active_vcpu(hypctx);
		aplic_flush_hwstate(hypctx);
		riscv_sync_interrupts(hypctx);
		vmm_fence_process(hypctx);

		dprintf("%s: Entering guest VM, vsatp %lx, ss %lx hs %lx\n",
		    __func__, csr_read(vsatp), hypctx->guest_regs.hyp_sstatus,
		    hypctx->guest_regs.hyp_hstatus);

		vmm_switch(hypctx);

		dprintf("%s: Leaving guest VM, hstatus %lx\n", __func__,
		    hypctx->guest_regs.hyp_hstatus);

		/* Guest can clear VSSIP. It can't clear VSTIP or VSEIP. */
		hvip = csr_read(hvip);
		if ((hypctx->guest_csrs.hvip ^ hvip) & HVIP_VSSIP) {
			if (hvip & HVIP_VSSIP) {
				/* TODO: VSSIP was set by guest. */
			} else {
				/* VSSIP was cleared by guest. */
				hypctx->guest_csrs.hvip &= ~HVIP_VSSIP;
			}
		}

		aplic_sync_hwstate(hypctx);

		/*
		 * TODO: deactivate stage 2 pmap here if needed.
		 */

		vme->scause = csr_read(scause);
		vme->sepc = csr_read(sepc);
		vme->stval = csr_read(stval);
		vme->htval = csr_read(htval);
		vme->htinst = csr_read(htinst);

		intr_restore(val);

		vmm_stat_incr(vcpu, VMEXIT_COUNT, 1);
		vme->pc = hypctx->guest_regs.hyp_sepc;
		vme->inst_length = INSN_SIZE;

		handled = riscv_handle_world_switch(hypctx, vme, pmap);
		if (handled == false)
			/* Exit loop to emulate instruction. */
			break;
		else {
			/* Resume guest execution from the next instruction. */
			hypctx->guest_regs.hyp_sepc += vme->inst_length;
		}
	}

	vmmops_vcpu_save_csrs(hypctx);

	return (0);
}

static void
riscv_pcpu_vmcleanup(void *arg)
{
	struct hyp *hyp;
	int i, maxcpus;

	hyp = arg;
	maxcpus = vm_get_maxcpus(hyp->vm);
	for (i = 0; i < maxcpus; i++) {
		if (riscv_get_active_vcpu() == hyp->ctx[i]) {
			riscv_set_active_vcpu(NULL);
			break;
		}
	}
}

void
vmmops_vcpu_cleanup(void *vcpui)
{
	struct hypctx *hypctx;

	hypctx = vcpui;

	dprintf("%s\n", __func__);

	aplic_cpucleanup(hypctx);

	mtx_destroy(&hypctx->fence_queue_mtx);
	free(hypctx->fence_queue, M_HYP);
	free(hypctx, M_HYP);
}

void
vmmops_cleanup(void *vmi)
{
	struct hyp *hyp;

	hyp = vmi;

	dprintf("%s\n", __func__);

	aplic_vmcleanup(hyp);

	smp_rendezvous(NULL, riscv_pcpu_vmcleanup, NULL, hyp);

	free(hyp, M_HYP);
}

/*
 * Return register value. Registers have different sizes and an explicit cast
 * must be made to ensure proper conversion.
 */
static uint64_t *
hypctx_regptr(struct hypctx *hypctx, int reg)
{

	switch (reg) {
	case VM_REG_GUEST_RA:
		return (&hypctx->guest_regs.hyp_ra);
	case VM_REG_GUEST_SP:
		return (&hypctx->guest_regs.hyp_sp);
	case VM_REG_GUEST_GP:
		return (&hypctx->guest_regs.hyp_gp);
	case VM_REG_GUEST_TP:
		return (&hypctx->guest_regs.hyp_tp);
	case VM_REG_GUEST_T0:
		return (&hypctx->guest_regs.hyp_t[0]);
	case VM_REG_GUEST_T1:
		return (&hypctx->guest_regs.hyp_t[1]);
	case VM_REG_GUEST_T2:
		return (&hypctx->guest_regs.hyp_t[2]);
	case VM_REG_GUEST_S0:
		return (&hypctx->guest_regs.hyp_s[0]);
	case VM_REG_GUEST_S1:
		return (&hypctx->guest_regs.hyp_s[1]);
	case VM_REG_GUEST_A0:
		return (&hypctx->guest_regs.hyp_a[0]);
	case VM_REG_GUEST_A1:
		return (&hypctx->guest_regs.hyp_a[1]);
	case VM_REG_GUEST_A2:
		return (&hypctx->guest_regs.hyp_a[2]);
	case VM_REG_GUEST_A3:
		return (&hypctx->guest_regs.hyp_a[3]);
	case VM_REG_GUEST_A4:
		return (&hypctx->guest_regs.hyp_a[4]);
	case VM_REG_GUEST_A5:
		return (&hypctx->guest_regs.hyp_a[5]);
	case VM_REG_GUEST_A6:
		return (&hypctx->guest_regs.hyp_a[6]);
	case VM_REG_GUEST_A7:
		return (&hypctx->guest_regs.hyp_a[7]);
	case VM_REG_GUEST_S2:
		return (&hypctx->guest_regs.hyp_s[2]);
	case VM_REG_GUEST_S3:
		return (&hypctx->guest_regs.hyp_s[3]);
	case VM_REG_GUEST_S4:
		return (&hypctx->guest_regs.hyp_s[4]);
	case VM_REG_GUEST_S5:
		return (&hypctx->guest_regs.hyp_s[5]);
	case VM_REG_GUEST_S6:
		return (&hypctx->guest_regs.hyp_s[6]);
	case VM_REG_GUEST_S7:
		return (&hypctx->guest_regs.hyp_s[7]);
	case VM_REG_GUEST_S8:
		return (&hypctx->guest_regs.hyp_s[8]);
	case VM_REG_GUEST_S9:
		return (&hypctx->guest_regs.hyp_s[9]);
	case VM_REG_GUEST_S10:
		return (&hypctx->guest_regs.hyp_s[10]);
	case VM_REG_GUEST_S11:
		return (&hypctx->guest_regs.hyp_s[11]);
	case VM_REG_GUEST_T3:
		return (&hypctx->guest_regs.hyp_t[3]);
	case VM_REG_GUEST_T4:
		return (&hypctx->guest_regs.hyp_t[4]);
	case VM_REG_GUEST_T5:
		return (&hypctx->guest_regs.hyp_t[5]);
	case VM_REG_GUEST_T6:
		return (&hypctx->guest_regs.hyp_t[6]);
	case VM_REG_GUEST_SEPC:
		return (&hypctx->guest_regs.hyp_sepc);
	default:
		break;
	}

	return (NULL);
}

int
vmmops_getreg(void *vcpui, int reg, uint64_t *retval)
{
	uint64_t *regp;
	int running, hostcpu;
	struct hypctx *hypctx;

	hypctx = vcpui;

	running = vcpu_is_running(hypctx->vcpu, &hostcpu);
	if (running && hostcpu != curcpu)
		panic("%s: %s%d is running", __func__, vm_name(hypctx->hyp->vm),
		    vcpu_vcpuid(hypctx->vcpu));

	if (reg == VM_REG_GUEST_ZERO) {
		*retval = 0;
		return (0);
	}

	regp = hypctx_regptr(hypctx, reg);
	if (regp == NULL)
		return (EINVAL);

	*retval = *regp;

	return (0);
}

int
vmmops_setreg(void *vcpui, int reg, uint64_t val)
{
	struct hypctx *hypctx;
	int running, hostcpu;
	uint64_t *regp;

	hypctx = vcpui;

	running = vcpu_is_running(hypctx->vcpu, &hostcpu);
	if (running && hostcpu != curcpu)
		panic("%s: %s%d is running", __func__, vm_name(hypctx->hyp->vm),
		    vcpu_vcpuid(hypctx->vcpu));

	regp = hypctx_regptr(hypctx, reg);
	if (regp == NULL)
		return (EINVAL);

	*regp = val;

	return (0);
}

int
vmmops_exception(void *vcpui, uint64_t scause)
{
	struct hypctx *hypctx;
	int running, hostcpu;

	hypctx = vcpui;

	running = vcpu_is_running(hypctx->vcpu, &hostcpu);
	if (running && hostcpu != curcpu)
		panic("%s: %s%d is running", __func__, vm_name(hypctx->hyp->vm),
		    vcpu_vcpuid(hypctx->vcpu));

	/* TODO: implement me. */

	return (ENOSYS);
}

int
vmmops_getcap(void *vcpui, int num, int *retval)
{
	int ret;

	ret = ENOENT;

	switch (num) {
	case VM_CAP_SSTC:
		*retval = has_sstc;
		ret = 0;
		break;
	case VM_CAP_UNRESTRICTED_GUEST:
		*retval = 1;
		ret = 0;
		break;
	default:
		break;
	}

	return (ret);
}

int
vmmops_setcap(void *vcpui, int num, int val)
{

	return (ENOENT);
}
