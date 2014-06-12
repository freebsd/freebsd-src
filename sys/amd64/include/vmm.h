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

enum vm_suspend_how {
	VM_SUSPEND_NONE,
	VM_SUSPEND_RESET,
	VM_SUSPEND_POWEROFF,
	VM_SUSPEND_HALT,
	VM_SUSPEND_LAST
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
	VM_REG_GUEST_CR2,
	VM_REG_LAST
};

enum x2apic_state {
	X2APIC_DISABLED,
	X2APIC_ENABLED,
	X2APIC_STATE_LAST
};

#ifdef _KERNEL

#define	VM_MAX_NAMELEN	32

struct vm;
struct vm_exception;
struct vm_memory_segment;
struct seg_desc;
struct vm_exit;
struct vm_run;
struct vhpet;
struct vioapic;
struct vlapic;
struct vmspace;
struct vm_object;
struct pmap;

typedef int	(*vmm_init_func_t)(int ipinum);
typedef int	(*vmm_cleanup_func_t)(void);
typedef void	(*vmm_resume_func_t)(void);
typedef void *	(*vmi_init_func_t)(struct vm *vm, struct pmap *pmap);
typedef int	(*vmi_run_func_t)(void *vmi, int vcpu, register_t rip,
				  struct pmap *pmap, void *rendezvous_cookie,
				  void *suspend_cookie);
typedef void	(*vmi_cleanup_func_t)(void *vmi);
typedef int	(*vmi_get_register_t)(void *vmi, int vcpu, int num,
				      uint64_t *retval);
typedef int	(*vmi_set_register_t)(void *vmi, int vcpu, int num,
				      uint64_t val);
typedef int	(*vmi_get_desc_t)(void *vmi, int vcpu, int num,
				  struct seg_desc *desc);
typedef int	(*vmi_set_desc_t)(void *vmi, int vcpu, int num,
				  struct seg_desc *desc);
typedef int	(*vmi_get_cap_t)(void *vmi, int vcpu, int num, int *retval);
typedef int	(*vmi_set_cap_t)(void *vmi, int vcpu, int num, int val);
typedef struct vmspace * (*vmi_vmspace_alloc)(vm_offset_t min, vm_offset_t max);
typedef void	(*vmi_vmspace_free)(struct vmspace *vmspace);
typedef struct vlapic * (*vmi_vlapic_init)(void *vmi, int vcpu);
typedef void	(*vmi_vlapic_cleanup)(void *vmi, struct vlapic *vlapic);

struct vmm_ops {
	vmm_init_func_t		init;		/* module wide initialization */
	vmm_cleanup_func_t	cleanup;
	vmm_resume_func_t	resume;

	vmi_init_func_t		vminit;		/* vm-specific initialization */
	vmi_run_func_t		vmrun;
	vmi_cleanup_func_t	vmcleanup;
	vmi_get_register_t	vmgetreg;
	vmi_set_register_t	vmsetreg;
	vmi_get_desc_t		vmgetdesc;
	vmi_set_desc_t		vmsetdesc;
	vmi_get_cap_t		vmgetcap;
	vmi_set_cap_t		vmsetcap;
	vmi_vmspace_alloc	vmspace_alloc;
	vmi_vmspace_free	vmspace_free;
	vmi_vlapic_init		vlapic_init;
	vmi_vlapic_cleanup	vlapic_cleanup;
};

extern struct vmm_ops vmm_ops_intel;
extern struct vmm_ops vmm_ops_amd;

int vm_create(const char *name, struct vm **retvm);
void vm_destroy(struct vm *vm);
int vm_reinit(struct vm *vm);
const char *vm_name(struct vm *vm);
int vm_malloc(struct vm *vm, vm_paddr_t gpa, size_t len);
int vm_map_mmio(struct vm *vm, vm_paddr_t gpa, size_t len, vm_paddr_t hpa);
int vm_unmap_mmio(struct vm *vm, vm_paddr_t gpa, size_t len);
void *vm_gpa_hold(struct vm *, vm_paddr_t gpa, size_t len, int prot,
		  void **cookie);
void vm_gpa_release(void *cookie);
int vm_gpabase2memseg(struct vm *vm, vm_paddr_t gpabase,
	      struct vm_memory_segment *seg);
int vm_get_memobj(struct vm *vm, vm_paddr_t gpa, size_t len,
		  vm_offset_t *offset, struct vm_object **object);
boolean_t vm_mem_allocated(struct vm *vm, vm_paddr_t gpa);
int vm_get_register(struct vm *vm, int vcpu, int reg, uint64_t *retval);
int vm_set_register(struct vm *vm, int vcpu, int reg, uint64_t val);
int vm_get_seg_desc(struct vm *vm, int vcpu, int reg,
		    struct seg_desc *ret_desc);
int vm_set_seg_desc(struct vm *vm, int vcpu, int reg,
		    struct seg_desc *desc);
int vm_run(struct vm *vm, struct vm_run *vmrun);
int vm_suspend(struct vm *vm, enum vm_suspend_how how);
int vm_inject_nmi(struct vm *vm, int vcpu);
int vm_nmi_pending(struct vm *vm, int vcpuid);
void vm_nmi_clear(struct vm *vm, int vcpuid);
int vm_inject_extint(struct vm *vm, int vcpu);
int vm_extint_pending(struct vm *vm, int vcpuid);
void vm_extint_clear(struct vm *vm, int vcpuid);
uint64_t *vm_guest_msrs(struct vm *vm, int cpu);
struct vlapic *vm_lapic(struct vm *vm, int cpu);
struct vioapic *vm_ioapic(struct vm *vm);
struct vhpet *vm_hpet(struct vm *vm);
int vm_get_capability(struct vm *vm, int vcpu, int type, int *val);
int vm_set_capability(struct vm *vm, int vcpu, int type, int val);
int vm_get_x2apic_state(struct vm *vm, int vcpu, enum x2apic_state *state);
int vm_set_x2apic_state(struct vm *vm, int vcpu, enum x2apic_state state);
int vm_apicid2vcpuid(struct vm *vm, int apicid);
int vm_activate_cpu(struct vm *vm, int vcpu);
cpuset_t vm_active_cpus(struct vm *vm);
cpuset_t vm_suspended_cpus(struct vm *vm);
struct vm_exit *vm_exitinfo(struct vm *vm, int vcpuid);
void vm_exit_suspended(struct vm *vm, int vcpuid, uint64_t rip);
void vm_exit_rendezvous(struct vm *vm, int vcpuid, uint64_t rip);
void vm_exit_astpending(struct vm *vm, int vcpuid, uint64_t rip);

/*
 * Rendezvous all vcpus specified in 'dest' and execute 'func(arg)'.
 * The rendezvous 'func(arg)' is not allowed to do anything that will
 * cause the thread to be put to sleep.
 *
 * If the rendezvous is being initiated from a vcpu context then the
 * 'vcpuid' must refer to that vcpu, otherwise it should be set to -1.
 *
 * The caller cannot hold any locks when initiating the rendezvous.
 *
 * The implementation of this API may cause vcpus other than those specified
 * by 'dest' to be stalled. The caller should not rely on any vcpus making
 * forward progress when the rendezvous is in progress.
 */
typedef void (*vm_rendezvous_func_t)(struct vm *vm, int vcpuid, void *arg);
void vm_smp_rendezvous(struct vm *vm, int vcpuid, cpuset_t dest,
    vm_rendezvous_func_t func, void *arg);

static __inline int
vcpu_rendezvous_pending(void *rendezvous_cookie)
{

	return (*(uintptr_t *)rendezvous_cookie != 0);
}

static __inline int
vcpu_suspended(void *suspend_cookie)
{

	return (*(int *)suspend_cookie);
}

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
	VCPU_FROZEN,
	VCPU_RUNNING,
	VCPU_SLEEPING,
};

int vcpu_set_state(struct vm *vm, int vcpu, enum vcpu_state state,
    bool from_idle);
enum vcpu_state vcpu_get_state(struct vm *vm, int vcpu, int *hostcpu);

static int __inline
vcpu_is_running(struct vm *vm, int vcpu, int *hostcpu)
{
	return (vcpu_get_state(vm, vcpu, hostcpu) == VCPU_RUNNING);
}

void *vcpu_stats(struct vm *vm, int vcpu);
void vcpu_notify_event(struct vm *vm, int vcpuid, bool lapic_intr);
struct vmspace *vm_get_vmspace(struct vm *vm);
int vm_assign_pptdev(struct vm *vm, int bus, int slot, int func);
int vm_unassign_pptdev(struct vm *vm, int bus, int slot, int func);
struct vatpic *vm_atpic(struct vm *vm);
struct vatpit *vm_atpit(struct vm *vm);

/*
 * Inject exception 'vme' into the guest vcpu. This function returns 0 on
 * success and non-zero on failure.
 *
 * Wrapper functions like 'vm_inject_gp()' should be preferred to calling
 * this function directly because they enforce the trap-like or fault-like
 * behavior of an exception.
 *
 * This function should only be called in the context of the thread that is
 * executing this vcpu.
 */
int vm_inject_exception(struct vm *vm, int vcpuid, struct vm_exception *vme);

/*
 * Returns 0 if there is no exception pending for this vcpu. Returns 1 if an
 * exception is pending and also updates 'vme'. The pending exception is
 * cleared when this function returns.
 *
 * This function should only be called in the context of the thread that is
 * executing this vcpu.
 */
int vm_exception_pending(struct vm *vm, int vcpuid, struct vm_exception *vme);

void vm_inject_gp(struct vm *vm, int vcpuid); /* general protection fault */
void vm_inject_ud(struct vm *vm, int vcpuid); /* undefined instruction fault */
void vm_inject_pf(struct vm *vm, int vcpuid, int error_code, uint64_t cr2);

enum vm_reg_name vm_segment_name(int seg_encoding);

#endif	/* KERNEL */

#define	VM_MAXCPU	16			/* maximum virtual cpus */

/*
 * Identifiers for optional vmm capabilities
 */
enum vm_cap_type {
	VM_CAP_HALT_EXIT,
	VM_CAP_MTRAP_EXIT,
	VM_CAP_PAUSE_EXIT,
	VM_CAP_UNRESTRICTED_GUEST,
	VM_CAP_ENABLE_INVPCID,
	VM_CAP_MAX
};

enum vm_intr_trigger {
	EDGE_TRIGGER,
	LEVEL_TRIGGER
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
#define	SEG_DESC_TYPE(desc)		((desc)->access & 0x001f)
#define	SEG_DESC_PRESENT(desc)		((desc)->access & 0x0080)
#define	SEG_DESC_DEF32(desc)		((desc)->access & 0x4000)
#define	SEG_DESC_GRANULARITY(desc)	((desc)->access & 0x8000)
#define	SEG_DESC_UNUSABLE(desc)		((desc)->access & 0x10000)

enum vm_cpu_mode {
	CPU_MODE_COMPATIBILITY,		/* IA-32E mode (CS.L = 0) */
	CPU_MODE_64BIT,			/* IA-32E mode (CS.L = 1) */
};

enum vm_paging_mode {
	PAGING_MODE_FLAT,
	PAGING_MODE_32,
	PAGING_MODE_PAE,
	PAGING_MODE_64,
};

struct vm_guest_paging {
	uint64_t	cr3;
	int		cpl;
	enum vm_cpu_mode cpu_mode;
	enum vm_paging_mode paging_mode;
};

/*
 * The data structures 'vie' and 'vie_op' are meant to be opaque to the
 * consumers of instruction decoding. The only reason why their contents
 * need to be exposed is because they are part of the 'vm_exit' structure.
 */
struct vie_op {
	uint8_t		op_byte;	/* actual opcode byte */
	uint8_t		op_type;	/* type of operation (e.g. MOV) */
	uint16_t	op_flags;
};

#define	VIE_INST_SIZE	15
struct vie {
	uint8_t		inst[VIE_INST_SIZE];	/* instruction bytes */
	uint8_t		num_valid;		/* size of the instruction */
	uint8_t		num_processed;

	uint8_t		rex_w:1,		/* REX prefix */
			rex_r:1,
			rex_x:1,
			rex_b:1,
			rex_present:1;

	uint8_t		mod:2,			/* ModRM byte */
			reg:4,
			rm:4;

	uint8_t		ss:2,			/* SIB byte */
			index:4,
			base:4;

	uint8_t		disp_bytes;
	uint8_t		imm_bytes;

	uint8_t		scale;
	int		base_register;		/* VM_REG_GUEST_xyz */
	int		index_register;		/* VM_REG_GUEST_xyz */

	int64_t		displacement;		/* optional addr displacement */
	int64_t		immediate;		/* optional immediate operand */

	uint8_t		decoded;	/* set to 1 if successfully decoded */

	struct vie_op	op;			/* opcode description */
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
	VM_EXITCODE_INST_EMUL,
	VM_EXITCODE_SPINUP_AP,
	VM_EXITCODE_DEPRECATED1,	/* used to be SPINDOWN_CPU */
	VM_EXITCODE_RENDEZVOUS,
	VM_EXITCODE_IOAPIC_EOI,
	VM_EXITCODE_SUSPENDED,
	VM_EXITCODE_INOUT_STR,
	VM_EXITCODE_MAX
};

struct vm_inout {
	uint16_t	bytes:3;	/* 1 or 2 or 4 */
	uint16_t	in:1;
	uint16_t	string:1;
	uint16_t	rep:1;
	uint16_t	port;
	uint32_t	eax;		/* valid for out */
};

struct vm_inout_str {
	struct vm_inout	inout;		/* must be the first element */
	struct vm_guest_paging paging;
	uint64_t	rflags;
	uint64_t	cr0;
	uint64_t	index;
	uint64_t	count;		/* rep=1 (%rcx), rep=0 (1) */
	int		addrsize;
	enum vm_reg_name seg_name;
	struct seg_desc seg_desc;
};

struct vm_exit {
	enum vm_exitcode	exitcode;
	int			inst_length;	/* 0 means unknown */
	uint64_t		rip;
	union {
		struct vm_inout	inout;
		struct vm_inout_str inout_str;
		struct {
			uint64_t	gpa;
			int		fault_type;
		} paging;
		struct {
			uint64_t	gpa;
			uint64_t	gla;
			struct vm_guest_paging paging;
			struct vie	vie;
		} inst_emul;
		/*
		 * VMX specific payload. Used when there is no "better"
		 * exitcode to represent the VM-exit.
		 */
		struct {
			int		status;		/* vmx inst status */
			/*
			 * 'exit_reason' and 'exit_qualification' are valid
			 * only if 'status' is zero.
			 */
			uint32_t	exit_reason;
			uint64_t	exit_qualification;
			/*
			 * 'inst_error' and 'inst_type' are valid
			 * only if 'status' is non-zero.
			 */
			int		inst_type;
			int		inst_error;
		} vmx;
		struct {
			uint32_t	code;		/* ecx value */
			uint64_t	wval;
		} msr;
		struct {
			int		vcpu;
			uint64_t	rip;
		} spinup_ap;
		struct {
			uint64_t	rflags;
		} hlt;
		struct {
			int		vector;
		} ioapic_eoi;
		struct {
			enum vm_suspend_how how;
		} suspended;
	} u;
};

#endif	/* _VMM_H_ */
