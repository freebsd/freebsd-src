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

#ifndef _VMM_H_
#define	_VMM_H_

#ifdef _KERNEL

#define	VM_MAX_NAMELEN	32

struct vm;
struct vm_memory_segment;
struct seg_desc;
struct vm_exit;
struct vm_run;
struct vlapic;

enum x2apic_state;

typedef int	(*vmm_init_func_t)(void);
typedef int	(*vmm_cleanup_func_t)(void);
typedef void *	(*vmi_init_func_t)(struct vm *vm); /* instance specific apis */
typedef int	(*vmi_run_func_t)(void *vmi, int vcpu, register_t rip);
typedef void	(*vmi_cleanup_func_t)(void *vmi);
typedef int	(*vmi_mmap_set_func_t)(void *vmi, vm_paddr_t gpa,
				       vm_paddr_t hpa, size_t length,
				       vm_memattr_t attr, int prot,
				       boolean_t superpages_ok);
typedef vm_paddr_t (*vmi_mmap_get_func_t)(void *vmi, vm_paddr_t gpa);
typedef int	(*vmi_get_register_t)(void *vmi, int vcpu, int num,
				      uint64_t *retval);
typedef int	(*vmi_set_register_t)(void *vmi, int vcpu, int num,
				      uint64_t val);
typedef int	(*vmi_get_desc_t)(void *vmi, int vcpu, int num,
				  struct seg_desc *desc);
typedef int	(*vmi_set_desc_t)(void *vmi, int vcpu, int num,
				  struct seg_desc *desc);
typedef int	(*vmi_inject_event_t)(void *vmi, int vcpu,
				      int type, int vector,
				      uint32_t code, int code_valid);
typedef int	(*vmi_get_cap_t)(void *vmi, int vcpu, int num, int *retval);
typedef int	(*vmi_set_cap_t)(void *vmi, int vcpu, int num, int val);

struct vmm_ops {
	vmm_init_func_t		init;		/* module wide initialization */
	vmm_cleanup_func_t	cleanup;

	vmi_init_func_t		vminit;		/* vm-specific initialization */
	vmi_run_func_t		vmrun;
	vmi_cleanup_func_t	vmcleanup;
	vmi_mmap_set_func_t	vmmmap_set;
	vmi_mmap_get_func_t	vmmmap_get;
	vmi_get_register_t	vmgetreg;
	vmi_set_register_t	vmsetreg;
	vmi_get_desc_t		vmgetdesc;
	vmi_set_desc_t		vmsetdesc;
	vmi_inject_event_t	vminject;
	vmi_get_cap_t		vmgetcap;
	vmi_set_cap_t		vmsetcap;
};

extern struct vmm_ops vmm_ops_intel;
extern struct vmm_ops vmm_ops_amd;

int vm_create(const char *name, struct vm **retvm);
void vm_destroy(struct vm *vm);
const char *vm_name(struct vm *vm);
int vm_malloc(struct vm *vm, vm_paddr_t gpa, size_t len);
int vm_map_mmio(struct vm *vm, vm_paddr_t gpa, size_t len, vm_paddr_t hpa);
int vm_unmap_mmio(struct vm *vm, vm_paddr_t gpa, size_t len);
vm_paddr_t vm_gpa2hpa(struct vm *vm, vm_paddr_t gpa, size_t size);
int vm_gpabase2memseg(struct vm *vm, vm_paddr_t gpabase,
	      struct vm_memory_segment *seg);
int vm_get_register(struct vm *vm, int vcpu, int reg, uint64_t *retval);
int vm_set_register(struct vm *vm, int vcpu, int reg, uint64_t val);
int vm_get_seg_desc(struct vm *vm, int vcpu, int reg,
		    struct seg_desc *ret_desc);
int vm_set_seg_desc(struct vm *vm, int vcpu, int reg,
		    struct seg_desc *desc);
int vm_run(struct vm *vm, struct vm_run *vmrun);
int vm_inject_event(struct vm *vm, int vcpu, int type,
		    int vector, uint32_t error_code, int error_code_valid);
int vm_inject_nmi(struct vm *vm, int vcpu);
int vm_nmi_pending(struct vm *vm, int vcpuid);
void vm_nmi_clear(struct vm *vm, int vcpuid);
uint64_t *vm_guest_msrs(struct vm *vm, int cpu);
struct vlapic *vm_lapic(struct vm *vm, int cpu);
int vm_get_capability(struct vm *vm, int vcpu, int type, int *val);
int vm_set_capability(struct vm *vm, int vcpu, int type, int val);
int vm_get_x2apic_state(struct vm *vm, int vcpu, enum x2apic_state *state);
int vm_set_x2apic_state(struct vm *vm, int vcpu, enum x2apic_state state);
void vm_activate_cpu(struct vm *vm, int vcpu);
cpuset_t vm_active_cpus(struct vm *vm);
struct vm_exit *vm_exitinfo(struct vm *vm, int vcpuid);

/*
 * Return 1 if device indicated by bus/slot/func is supposed to be a
 * pci passthrough device.
 *
 * Return 0 otherwise.
 */
int vmm_is_pptdev(int bus, int slot, int func);

void *vm_iommu_domain(struct vm *vm);

enum vcpu_state {
	VCPU_IDLE,
	VCPU_RUNNING,
	VCPU_CANNOT_RUN,
};

int vcpu_set_state(struct vm *vm, int vcpu, enum vcpu_state state);
enum vcpu_state vcpu_get_state(struct vm *vm, int vcpu);

static int __inline
vcpu_is_running(struct vm *vm, int vcpu)
{
	return (vcpu_get_state(vm, vcpu) == VCPU_RUNNING);
}

void *vcpu_stats(struct vm *vm, int vcpu);
void vm_interrupt_hostcpu(struct vm *vm, int vcpu);

#endif	/* KERNEL */

#include <machine/vmm_instruction_emul.h>

#define	VM_MAXCPU	8			/* maximum virtual cpus */

/*
 * Identifiers for events that can be injected into the VM
 */
enum vm_event_type {
	VM_EVENT_NONE,
	VM_HW_INTR,
	VM_NMI,
	VM_HW_EXCEPTION,
	VM_SW_INTR,
	VM_PRIV_SW_EXCEPTION,
	VM_SW_EXCEPTION,
	VM_EVENT_MAX
};

/*
 * Identifiers for architecturally defined registers.
 */
enum vm_reg_name {
	VM_REG_GUEST_RAX,
	VM_REG_GUEST_RBX,
	VM_REG_GUEST_RCX,
	VM_REG_GUEST_RDX,
	VM_REG_GUEST_RSI,
	VM_REG_GUEST_RDI,
	VM_REG_GUEST_RBP,
	VM_REG_GUEST_R8,
	VM_REG_GUEST_R9,
	VM_REG_GUEST_R10,
	VM_REG_GUEST_R11,
	VM_REG_GUEST_R12,
	VM_REG_GUEST_R13,
	VM_REG_GUEST_R14,
	VM_REG_GUEST_R15,
	VM_REG_GUEST_CR0,
	VM_REG_GUEST_CR3,
	VM_REG_GUEST_CR4,
	VM_REG_GUEST_DR7,
	VM_REG_GUEST_RSP,
	VM_REG_GUEST_RIP,
	VM_REG_GUEST_RFLAGS,
	VM_REG_GUEST_ES,
	VM_REG_GUEST_CS,
	VM_REG_GUEST_SS,
	VM_REG_GUEST_DS,
	VM_REG_GUEST_FS,
	VM_REG_GUEST_GS,
	VM_REG_GUEST_LDTR,
	VM_REG_GUEST_TR,
	VM_REG_GUEST_IDTR,
	VM_REG_GUEST_GDTR,
	VM_REG_GUEST_EFER,
	VM_REG_LAST
};

/*
 * Identifiers for optional vmm capabilities
 */
enum vm_cap_type {
	VM_CAP_HALT_EXIT,
	VM_CAP_MTRAP_EXIT,
	VM_CAP_PAUSE_EXIT,
	VM_CAP_UNRESTRICTED_GUEST,
	VM_CAP_MAX
};

enum x2apic_state {
	X2APIC_ENABLED,
	X2APIC_AVAILABLE,
	X2APIC_DISABLED,
	X2APIC_STATE_LAST
};

/*
 * The 'access' field has the format specified in Table 21-2 of the Intel
 * Architecture Manual vol 3b.
 *
 * XXX The contents of the 'access' field are architecturally defined except
 * bit 16 - Segment Unusable.
 */
struct seg_desc {
	uint64_t	base;
	uint32_t	limit;
	uint32_t	access;
};

enum vm_exitcode {
	VM_EXITCODE_INOUT,
	VM_EXITCODE_VMX,
	VM_EXITCODE_BOGUS,
	VM_EXITCODE_RDMSR,
	VM_EXITCODE_WRMSR,
	VM_EXITCODE_HLT,
	VM_EXITCODE_MTRAP,
	VM_EXITCODE_PAUSE,
	VM_EXITCODE_PAGING,
	VM_EXITCODE_SPINUP_AP,
	VM_EXITCODE_MAX
};

struct vm_exit {
	enum vm_exitcode	exitcode;
	int			inst_length;	/* 0 means unknown */
	uint64_t		rip;
	union {
		struct {
			uint16_t	bytes:3;	/* 1 or 2 or 4 */
			uint16_t	in:1;		/* out is 0, in is 1 */
			uint16_t	string:1;
			uint16_t	rep:1;
			uint16_t	port;
			uint32_t	eax;		/* valid for out */
		} inout;
		struct {
			uint64_t	gpa;
			struct vie	vie;
		} paging;
		/*
		 * VMX specific payload. Used when there is no "better"
		 * exitcode to represent the VM-exit.
		 */
		struct {
			int		error;		/* vmx inst error */
			uint32_t	exit_reason;
			uint64_t	exit_qualification;
		} vmx;
		struct {
			uint32_t	code;		/* ecx value */
			uint64_t	wval;
		} msr;
		struct {
			int		vcpu;
			uint64_t	rip;
		} spinup_ap;
	} u;
};

#endif	/* _VMM_H_ */
