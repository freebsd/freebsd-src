/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Mihai Carabas <mihai.carabas@gmail.com>
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

#ifndef _VMM_H_
#define	_VMM_H_

#include <sys/param.h>
#include <sys/cpuset.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include "pte.h"
#include "pmap.h"

struct vcpu;

enum vm_suspend_how {
	VM_SUSPEND_NONE,
	VM_SUSPEND_RESET,
	VM_SUSPEND_POWEROFF,
	VM_SUSPEND_HALT,
	VM_SUSPEND_DESTROY,
	VM_SUSPEND_LAST
};

/*
 * Identifiers for architecturally defined registers.
 */
enum vm_reg_name {
	VM_REG_GUEST_ZERO = 0,
	VM_REG_GUEST_RA,
	VM_REG_GUEST_SP,
	VM_REG_GUEST_GP,
	VM_REG_GUEST_TP,
	VM_REG_GUEST_T0,
	VM_REG_GUEST_T1,
	VM_REG_GUEST_T2,
	VM_REG_GUEST_S0,
	VM_REG_GUEST_S1,
	VM_REG_GUEST_A0,
	VM_REG_GUEST_A1,
	VM_REG_GUEST_A2,
	VM_REG_GUEST_A3,
	VM_REG_GUEST_A4,
	VM_REG_GUEST_A5,
	VM_REG_GUEST_A6,
	VM_REG_GUEST_A7,
	VM_REG_GUEST_S2,
	VM_REG_GUEST_S3,
	VM_REG_GUEST_S4,
	VM_REG_GUEST_S5,
	VM_REG_GUEST_S6,
	VM_REG_GUEST_S7,
	VM_REG_GUEST_S8,
	VM_REG_GUEST_S9,
	VM_REG_GUEST_S10,
	VM_REG_GUEST_S11,
	VM_REG_GUEST_T3,
	VM_REG_GUEST_T4,
	VM_REG_GUEST_T5,
	VM_REG_GUEST_T6,
	VM_REG_GUEST_SEPC,
	VM_REG_LAST
};

#define	VM_INTINFO_VECTOR(info)	((info) & 0xff)
#define	VM_INTINFO_DEL_ERRCODE	0x800
#define	VM_INTINFO_RSVD		0x7ffff000
#define	VM_INTINFO_VALID	0x80000000
#define	VM_INTINFO_TYPE		0x700
#define	VM_INTINFO_HWINTR	(0 << 8)
#define	VM_INTINFO_NMI		(2 << 8)
#define	VM_INTINFO_HWEXCEPTION	(3 << 8)
#define	VM_INTINFO_SWINTR	(4 << 8)

#ifdef _KERNEL
#include <machine/vmm_instruction_emul.h>

#define	VMM_VCPU_MD_FIELDS						\
	struct vm_exit	exitinfo;					\
	uint64_t	nextpc;		/* (x) next instruction to execute */ \
	struct fpreg	*guestfpu	/* (a,i) guest fpu state */

#define	VMM_VM_MD_FIELDS						\
	struct vmm_mmio_region mmio_region[VM_MAX_MMIO_REGIONS]

struct vm;
struct vm_eventinfo;
struct vm_exception;
struct vm_exit;
struct vm_run;
struct vm_object;
struct vm_guest_paging;
struct vm_aplic_descr;
struct pmap;

struct vmm_mmio_region {
	uint64_t start;
	uint64_t end;
	mem_region_read_t read;
	mem_region_write_t write;
};
#define	VM_MAX_MMIO_REGIONS	4

#define	DECLARE_VMMOPS_FUNC(ret_type, opname, args)		\
	ret_type vmmops_##opname args

DECLARE_VMMOPS_FUNC(int, modinit, (void));
DECLARE_VMMOPS_FUNC(int, modcleanup, (void));
DECLARE_VMMOPS_FUNC(void *, init, (struct vm *vm, struct pmap *pmap));
DECLARE_VMMOPS_FUNC(int, gla2gpa, (void *vcpui, struct vm_guest_paging *paging,
    uint64_t gla, int prot, uint64_t *gpa, int *is_fault));
DECLARE_VMMOPS_FUNC(int, run, (void *vcpui, register_t pc, struct pmap *pmap,
    struct vm_eventinfo *info));
DECLARE_VMMOPS_FUNC(void, cleanup, (void *vmi));
DECLARE_VMMOPS_FUNC(void *, vcpu_init, (void *vmi, struct vcpu *vcpu,
    int vcpu_id));
DECLARE_VMMOPS_FUNC(void, vcpu_cleanup, (void *vcpui));
DECLARE_VMMOPS_FUNC(int, exception, (void *vcpui, uint64_t scause));
DECLARE_VMMOPS_FUNC(int, getreg, (void *vcpui, int num, uint64_t *retval));
DECLARE_VMMOPS_FUNC(int, setreg, (void *vcpui, int num, uint64_t val));
DECLARE_VMMOPS_FUNC(int, getcap, (void *vcpui, int num, int *retval));
DECLARE_VMMOPS_FUNC(int, setcap, (void *vcpui, int num, int val));
DECLARE_VMMOPS_FUNC(struct vmspace *, vmspace_alloc, (vm_offset_t min,
    vm_offset_t max));
DECLARE_VMMOPS_FUNC(void, vmspace_free, (struct vmspace *vmspace));

int vm_get_register(struct vcpu *vcpu, int reg, uint64_t *retval);
int vm_set_register(struct vcpu *vcpu, int reg, uint64_t val);
int vm_run(struct vcpu *vcpu);
void *vm_get_cookie(struct vm *vm);
void *vcpu_get_cookie(struct vcpu *vcpu);
int vm_get_capability(struct vcpu *vcpu, int type, int *val);
int vm_set_capability(struct vcpu *vcpu, int type, int val);
int vm_inject_exception(struct vcpu *vcpu, uint64_t scause);
int vm_attach_aplic(struct vm *vm, struct vm_aplic_descr *descr);
int vm_assert_irq(struct vm *vm, uint32_t irq);
int vm_deassert_irq(struct vm *vm, uint32_t irq);
int vm_raise_msi(struct vm *vm, uint64_t msg, uint64_t addr, int bus, int slot,
    int func);
struct vm_exit *vm_exitinfo(struct vcpu *vcpu);
void vm_exit_suspended(struct vcpu *vcpu, uint64_t pc);
void vm_exit_debug(struct vcpu *vcpu, uint64_t pc);
void vm_exit_astpending(struct vcpu *vcpu, uint64_t pc);
#endif	/* _KERNEL */

#define	VM_DIR_READ	0
#define	VM_DIR_WRITE	1

#define	VM_GP_M_MASK		0x1f
#define	VM_GP_MMU_ENABLED	(1 << 5)

struct vm_guest_paging {
	int		flags;
	int		padding;
};

struct vie {
	uint8_t access_size:4, sign_extend:1, dir:1, unused:2;
	enum vm_reg_name reg;
};

struct vre {
	uint32_t inst_syndrome;
	uint8_t dir:1, unused:7;
	enum vm_reg_name reg;
};

/*
 * Identifiers for optional vmm capabilities
 */
enum vm_cap_type {
	VM_CAP_UNRESTRICTED_GUEST,
	VM_CAP_SSTC,
	VM_CAP_MAX
};

enum vm_exitcode {
	VM_EXITCODE_BOGUS,
	VM_EXITCODE_ECALL,
	VM_EXITCODE_HYP,
	VM_EXITCODE_PAGING,
	VM_EXITCODE_SUSPENDED,
	VM_EXITCODE_DEBUG,
	VM_EXITCODE_INST_EMUL,
	VM_EXITCODE_WFI,
	VM_EXITCODE_MAX
};

struct vm_exit {
	uint64_t scause;
	uint64_t sepc;
	uint64_t stval;
	uint64_t htval;
	uint64_t htinst;
	enum vm_exitcode exitcode;
	int inst_length;
	uint64_t pc;
	union {
		struct {
			uint64_t gpa;
		} paging;

		struct {
			uint64_t gpa;
			struct vm_guest_paging paging;
			struct vie vie;
		} inst_emul;

		struct {
			uint64_t args[8];
		} ecall;

		struct {
			enum vm_suspend_how how;
		} suspended;

		struct {
			uint64_t scause;
		} hyp;
	} u;
};

#endif	/* _VMM_H_ */
