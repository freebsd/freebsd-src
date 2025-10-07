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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpuset.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>

#include <machine/armreg.h>
#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/machdep.h>
#include <machine/pcb.h>
#include <machine/smp.h>
#include <machine/vm.h>
#include <machine/vmparam.h>
#include <machine/vmm.h>
#include <machine/vmm_instruction_emul.h>

#include <dev/pci/pcireg.h>
#include <dev/vmm/vmm_dev.h>
#include <dev/vmm/vmm_ktr.h>
#include <dev/vmm/vmm_mem.h>
#include <dev/vmm/vmm_stat.h>

#include "arm64.h"
#include "mmu.h"

#include "io/vgic.h"
#include "io/vtimer.h"

struct vcpu {
	int		flags;
	enum vcpu_state	state;
	struct mtx	mtx;
	int		hostcpu;	/* host cpuid this vcpu last ran on */
	int		vcpuid;
	void		*stats;
	struct vm_exit	exitinfo;
	uint64_t	nextpc;		/* (x) next instruction to execute */
	struct vm	*vm;		/* (o) */
	void		*cookie;	/* (i) cpu-specific data */
	struct vfpstate	*guestfpu;	/* (a,i) guest fpu state */
};

#define	vcpu_lock_initialized(v) mtx_initialized(&((v)->mtx))
#define	vcpu_lock_init(v)	mtx_init(&((v)->mtx), "vcpu lock", 0, MTX_SPIN)
#define	vcpu_lock_destroy(v)	mtx_destroy(&((v)->mtx))
#define	vcpu_lock(v)		mtx_lock_spin(&((v)->mtx))
#define	vcpu_unlock(v)		mtx_unlock_spin(&((v)->mtx))
#define	vcpu_assert_locked(v)	mtx_assert(&((v)->mtx), MA_OWNED)

struct vmm_mmio_region {
	uint64_t start;
	uint64_t end;
	mem_region_read_t read;
	mem_region_write_t write;
};
#define	VM_MAX_MMIO_REGIONS	4

struct vmm_special_reg {
	uint32_t	esr_iss;
	uint32_t	esr_mask;
	reg_read_t	reg_read;
	reg_write_t	reg_write;
	void		*arg;
};
#define	VM_MAX_SPECIAL_REGS	16

/*
 * Initialization:
 * (o) initialized the first time the VM is created
 * (i) initialized when VM is created and when it is reinitialized
 * (x) initialized before use
 */
struct vm {
	void		*cookie;		/* (i) cpu-specific data */
	volatile cpuset_t active_cpus;		/* (i) active vcpus */
	volatile cpuset_t debug_cpus;		/* (i) vcpus stopped for debug */
	int		suspend;		/* (i) stop VM execution */
	bool		dying;			/* (o) is dying */
	volatile cpuset_t suspended_cpus; 	/* (i) suspended vcpus */
	volatile cpuset_t halted_cpus;		/* (x) cpus in a hard halt */
	struct vmspace	*vmspace;		/* (o) guest's address space */
	struct vm_mem	mem;			/* (i) guest memory */
	char		name[VM_MAX_NAMELEN];	/* (o) virtual machine name */
	struct vcpu	**vcpu;			/* (i) guest vcpus */
	struct vmm_mmio_region mmio_region[VM_MAX_MMIO_REGIONS];
						/* (o) guest MMIO regions */
	struct vmm_special_reg special_reg[VM_MAX_SPECIAL_REGS];
	/* The following describe the vm cpu topology */
	uint16_t	sockets;		/* (o) num of sockets */
	uint16_t	cores;			/* (o) num of cores/socket */
	uint16_t	threads;		/* (o) num of threads/core */
	uint16_t	maxcpus;		/* (o) max pluggable cpus */
	struct sx	vcpus_init_lock;	/* (o) */
};

static bool vmm_initialized = false;

static int vm_handle_wfi(struct vcpu *vcpu,
			 struct vm_exit *vme, bool *retu);

static MALLOC_DEFINE(M_VMM, "vmm", "vmm");

/* statistics */
static VMM_STAT(VCPU_TOTAL_RUNTIME, "vcpu total runtime");

SYSCTL_NODE(_hw, OID_AUTO, vmm, CTLFLAG_RW, NULL, NULL);

static int vmm_ipinum;
SYSCTL_INT(_hw_vmm, OID_AUTO, ipinum, CTLFLAG_RD, &vmm_ipinum, 0,
    "IPI vector used for vcpu notifications");

struct vmm_regs {
	uint64_t	id_aa64afr0;
	uint64_t	id_aa64afr1;
	uint64_t	id_aa64dfr0;
	uint64_t	id_aa64dfr1;
	uint64_t	id_aa64isar0;
	uint64_t	id_aa64isar1;
	uint64_t	id_aa64isar2;
	uint64_t	id_aa64mmfr0;
	uint64_t	id_aa64mmfr1;
	uint64_t	id_aa64mmfr2;
	uint64_t	id_aa64pfr0;
	uint64_t	id_aa64pfr1;
};

static const struct vmm_regs vmm_arch_regs_masks = {
	.id_aa64dfr0 =
	    ID_AA64DFR0_CTX_CMPs_MASK |
	    ID_AA64DFR0_WRPs_MASK |
	    ID_AA64DFR0_BRPs_MASK |
	    ID_AA64DFR0_PMUVer_3 |
	    ID_AA64DFR0_DebugVer_8,
	.id_aa64isar0 =
	    ID_AA64ISAR0_TLB_TLBIOSR |
	    ID_AA64ISAR0_SHA3_IMPL |
	    ID_AA64ISAR0_RDM_IMPL |
	    ID_AA64ISAR0_Atomic_IMPL |
	    ID_AA64ISAR0_CRC32_BASE |
	    ID_AA64ISAR0_SHA2_512 |
	    ID_AA64ISAR0_SHA1_BASE |
	    ID_AA64ISAR0_AES_PMULL,
	.id_aa64mmfr0 =
	    ID_AA64MMFR0_TGran4_IMPL |
	    ID_AA64MMFR0_TGran64_IMPL |
	    ID_AA64MMFR0_TGran16_IMPL |
	    ID_AA64MMFR0_ASIDBits_16 |
	    ID_AA64MMFR0_PARange_4P,
	.id_aa64mmfr1 =
	    ID_AA64MMFR1_SpecSEI_IMPL |
	    ID_AA64MMFR1_PAN_ATS1E1 |
	    ID_AA64MMFR1_HAFDBS_AF,
	.id_aa64pfr0 =
	    ID_AA64PFR0_GIC_CPUIF_NONE |
	    ID_AA64PFR0_AdvSIMD_HP |
	    ID_AA64PFR0_FP_HP |
	    ID_AA64PFR0_EL3_64 |
	    ID_AA64PFR0_EL2_64 |
	    ID_AA64PFR0_EL1_64 |
	    ID_AA64PFR0_EL0_64,
};

/* Host registers masked by vmm_arch_regs_masks. */
static struct vmm_regs vmm_arch_regs;

u_int vm_maxcpu;
SYSCTL_UINT(_hw_vmm, OID_AUTO, maxcpu, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &vm_maxcpu, 0, "Maximum number of vCPUs");

static void vcpu_notify_event_locked(struct vcpu *vcpu);

/* global statistics */
VMM_STAT(VMEXIT_COUNT, "total number of vm exits");
VMM_STAT(VMEXIT_UNKNOWN, "number of vmexits for the unknown exception");
VMM_STAT(VMEXIT_WFI, "number of times wfi was intercepted");
VMM_STAT(VMEXIT_WFE, "number of times wfe was intercepted");
VMM_STAT(VMEXIT_HVC, "number of times hvc was intercepted");
VMM_STAT(VMEXIT_MSR, "number of times msr/mrs was intercepted");
VMM_STAT(VMEXIT_DATA_ABORT, "number of vmexits for a data abort");
VMM_STAT(VMEXIT_INSN_ABORT, "number of vmexits for an instruction abort");
VMM_STAT(VMEXIT_UNHANDLED_SYNC, "number of vmexits for an unhandled synchronous exception");
VMM_STAT(VMEXIT_IRQ, "number of vmexits for an irq");
VMM_STAT(VMEXIT_FIQ, "number of vmexits for an interrupt");
VMM_STAT(VMEXIT_BRK, "number of vmexits for a breakpoint exception");
VMM_STAT(VMEXIT_SS, "number of vmexits for a single-step exception");
VMM_STAT(VMEXIT_UNHANDLED_EL2, "number of vmexits for an unhandled EL2 exception");
VMM_STAT(VMEXIT_UNHANDLED, "number of vmexits for an unhandled exception");

/*
 * Upper limit on vm_maxcpu. We could increase this to 28 bits, but this
 * is a safe value for now.
 */
#define	VM_MAXCPU	MIN(0xffff - 1, CPU_SETSIZE)

static int
vmm_regs_init(struct vmm_regs *regs, const struct vmm_regs *masks)
{
#define	_FETCH_KERN_REG(reg, field) do {				\
	regs->field = vmm_arch_regs_masks.field;			\
	if (!get_kernel_reg_iss_masked(reg ## _ISS, &regs->field,	\
	    masks->field))						\
		regs->field = 0;					\
} while (0)
	_FETCH_KERN_REG(ID_AA64AFR0_EL1, id_aa64afr0);
	_FETCH_KERN_REG(ID_AA64AFR1_EL1, id_aa64afr1);
	_FETCH_KERN_REG(ID_AA64DFR0_EL1, id_aa64dfr0);
	_FETCH_KERN_REG(ID_AA64DFR1_EL1, id_aa64dfr1);
	_FETCH_KERN_REG(ID_AA64ISAR0_EL1, id_aa64isar0);
	_FETCH_KERN_REG(ID_AA64ISAR1_EL1, id_aa64isar1);
	_FETCH_KERN_REG(ID_AA64ISAR2_EL1, id_aa64isar2);
	_FETCH_KERN_REG(ID_AA64MMFR0_EL1, id_aa64mmfr0);
	_FETCH_KERN_REG(ID_AA64MMFR1_EL1, id_aa64mmfr1);
	_FETCH_KERN_REG(ID_AA64MMFR2_EL1, id_aa64mmfr2);
	_FETCH_KERN_REG(ID_AA64PFR0_EL1, id_aa64pfr0);
	_FETCH_KERN_REG(ID_AA64PFR1_EL1, id_aa64pfr1);
#undef _FETCH_KERN_REG
	return (0);
}

static void
vcpu_cleanup(struct vcpu *vcpu, bool destroy)
{
	vmmops_vcpu_cleanup(vcpu->cookie);
	vcpu->cookie = NULL;
	if (destroy) {
		vmm_stat_free(vcpu->stats);
		fpu_save_area_free(vcpu->guestfpu);
		vcpu_lock_destroy(vcpu);
	}
}

static struct vcpu *
vcpu_alloc(struct vm *vm, int vcpu_id)
{
	struct vcpu *vcpu;

	KASSERT(vcpu_id >= 0 && vcpu_id < vm->maxcpus,
	    ("vcpu_alloc: invalid vcpu %d", vcpu_id));

	vcpu = malloc(sizeof(*vcpu), M_VMM, M_WAITOK | M_ZERO);
	vcpu_lock_init(vcpu);
	vcpu->state = VCPU_IDLE;
	vcpu->hostcpu = NOCPU;
	vcpu->vcpuid = vcpu_id;
	vcpu->vm = vm;
	vcpu->guestfpu = fpu_save_area_alloc();
	vcpu->stats = vmm_stat_alloc();
	return (vcpu);
}

static void
vcpu_init(struct vcpu *vcpu)
{
	vcpu->cookie = vmmops_vcpu_init(vcpu->vm->cookie, vcpu, vcpu->vcpuid);
	MPASS(vcpu->cookie != NULL);
	fpu_save_area_reset(vcpu->guestfpu);
	vmm_stat_init(vcpu->stats);
}

struct vm_exit *
vm_exitinfo(struct vcpu *vcpu)
{
	return (&vcpu->exitinfo);
}

static int
vmm_unsupported_quirk(void)
{
	/*
	 * Known to not load on Ampere eMAG
	 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=285051
	 */
	if (CPU_MATCH(CPU_IMPL_MASK | CPU_PART_MASK, CPU_IMPL_APM,
	    CPU_PART_EMAG8180, 0, 0))
		return (ENXIO);

	return (0);
}

static int
vmm_init(void)
{
	int error;

	vm_maxcpu = mp_ncpus;
	TUNABLE_INT_FETCH("hw.vmm.maxcpu", &vm_maxcpu);

	if (vm_maxcpu > VM_MAXCPU) {
		printf("vmm: vm_maxcpu clamped to %u\n", VM_MAXCPU);
		vm_maxcpu = VM_MAXCPU;
	}
	if (vm_maxcpu == 0)
		vm_maxcpu = 1;

	error = vmm_regs_init(&vmm_arch_regs, &vmm_arch_regs_masks);
	if (error != 0)
		return (error);

	return (vmmops_modinit(0));
}

static int
vmm_handler(module_t mod, int what, void *arg)
{
	int error;

	switch (what) {
	case MOD_LOAD:
		error = vmm_unsupported_quirk();
		if (error != 0)
			break;
		error = vmmdev_init();
		if (error != 0)
			break;
		error = vmm_init();
		if (error == 0)
			vmm_initialized = true;
		else
			(void)vmmdev_cleanup();
		break;
	case MOD_UNLOAD:
		error = vmmdev_cleanup();
		if (error == 0 && vmm_initialized) {
			error = vmmops_modcleanup();
			if (error) {
				/*
				 * Something bad happened - prevent new
				 * VMs from being created
				 */
				vmm_initialized = false;
			}
		}
		break;
	default:
		error = 0;
		break;
	}
	return (error);
}

static moduledata_t vmm_kmod = {
	"vmm",
	vmm_handler,
	NULL
};

/*
 * vmm initialization has the following dependencies:
 *
 * - HYP initialization requires smp_rendezvous() and therefore must happen
 *   after SMP is fully functional (after SI_SUB_SMP).
 * - vmm device initialization requires an initialized devfs.
 */
DECLARE_MODULE(vmm, vmm_kmod, MAX(SI_SUB_SMP, SI_SUB_DEVFS) + 1, SI_ORDER_ANY);
MODULE_VERSION(vmm, 1);

static void
vm_init(struct vm *vm, bool create)
{
	int i;

	vm->cookie = vmmops_init(vm, vmspace_pmap(vm->vmspace));
	MPASS(vm->cookie != NULL);

	CPU_ZERO(&vm->active_cpus);
	CPU_ZERO(&vm->debug_cpus);

	vm->suspend = 0;
	CPU_ZERO(&vm->suspended_cpus);

	memset(vm->mmio_region, 0, sizeof(vm->mmio_region));
	memset(vm->special_reg, 0, sizeof(vm->special_reg));

	if (!create) {
		for (i = 0; i < vm->maxcpus; i++) {
			if (vm->vcpu[i] != NULL)
				vcpu_init(vm->vcpu[i]);
		}
	}
}

void
vm_disable_vcpu_creation(struct vm *vm)
{
	sx_xlock(&vm->vcpus_init_lock);
	vm->dying = true;
	sx_xunlock(&vm->vcpus_init_lock);
}

struct vcpu *
vm_alloc_vcpu(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= vm_get_maxcpus(vm))
		return (NULL);

	/* Some interrupt controllers may have a CPU limit */
	if (vcpuid >= vgic_max_cpu_count(vm->cookie))
		return (NULL);

	vcpu = (struct vcpu *)
	    atomic_load_acq_ptr((uintptr_t *)&vm->vcpu[vcpuid]);
	if (__predict_true(vcpu != NULL))
		return (vcpu);

	sx_xlock(&vm->vcpus_init_lock);
	vcpu = vm->vcpu[vcpuid];
	if (vcpu == NULL && !vm->dying) {
		vcpu = vcpu_alloc(vm, vcpuid);
		vcpu_init(vcpu);

		/*
		 * Ensure vCPU is fully created before updating pointer
		 * to permit unlocked reads above.
		 */
		atomic_store_rel_ptr((uintptr_t *)&vm->vcpu[vcpuid],
		    (uintptr_t)vcpu);
	}
	sx_xunlock(&vm->vcpus_init_lock);
	return (vcpu);
}

void
vm_slock_vcpus(struct vm *vm)
{
	sx_slock(&vm->vcpus_init_lock);
}

void
vm_unlock_vcpus(struct vm *vm)
{
	sx_unlock(&vm->vcpus_init_lock);
}

int
vm_create(const char *name, struct vm **retvm)
{
	struct vm *vm;
	struct vmspace *vmspace;

	/*
	 * If vmm.ko could not be successfully initialized then don't attempt
	 * to create the virtual machine.
	 */
	if (!vmm_initialized)
		return (ENXIO);

	if (name == NULL || strlen(name) >= VM_MAX_NAMELEN)
		return (EINVAL);

	vmspace = vmmops_vmspace_alloc(0, 1ul << 39);
	if (vmspace == NULL)
		return (ENOMEM);

	vm = malloc(sizeof(struct vm), M_VMM, M_WAITOK | M_ZERO);
	strcpy(vm->name, name);
	vm->vmspace = vmspace;
	vm_mem_init(&vm->mem);
	sx_init(&vm->vcpus_init_lock, "vm vcpus");

	vm->sockets = 1;
	vm->cores = 1;			/* XXX backwards compatibility */
	vm->threads = 1;		/* XXX backwards compatibility */
	vm->maxcpus = vm_maxcpu;

	vm->vcpu = malloc(sizeof(*vm->vcpu) * vm->maxcpus, M_VMM,
	    M_WAITOK | M_ZERO);

	vm_init(vm, true);

	*retvm = vm;
	return (0);
}

void
vm_get_topology(struct vm *vm, uint16_t *sockets, uint16_t *cores,
    uint16_t *threads, uint16_t *maxcpus)
{
	*sockets = vm->sockets;
	*cores = vm->cores;
	*threads = vm->threads;
	*maxcpus = vm->maxcpus;
}

uint16_t
vm_get_maxcpus(struct vm *vm)
{
	return (vm->maxcpus);
}

int
vm_set_topology(struct vm *vm, uint16_t sockets, uint16_t cores,
    uint16_t threads, uint16_t maxcpus)
{
	/* Ignore maxcpus. */
	if ((sockets * cores * threads) > vm->maxcpus)
		return (EINVAL);
	vm->sockets = sockets;
	vm->cores = cores;
	vm->threads = threads;
	return(0);
}

static void
vm_cleanup(struct vm *vm, bool destroy)
{
	pmap_t pmap __diagused;
	int i;

	if (destroy) {
		vm_xlock_memsegs(vm);
		pmap = vmspace_pmap(vm->vmspace);
		sched_pin();
		PCPU_SET(curvmpmap, NULL);
		sched_unpin();
		CPU_FOREACH(i) {
			MPASS(cpuid_to_pcpu[i]->pc_curvmpmap != pmap);
		}
	} else
		vm_assert_memseg_xlocked(vm);


	vgic_detach_from_vm(vm->cookie);

	for (i = 0; i < vm->maxcpus; i++) {
		if (vm->vcpu[i] != NULL)
			vcpu_cleanup(vm->vcpu[i], destroy);
	}

	vmmops_cleanup(vm->cookie);

	vm_mem_cleanup(vm);
	if (destroy) {
		vm_mem_destroy(vm);

		vmmops_vmspace_free(vm->vmspace);
		vm->vmspace = NULL;

		for (i = 0; i < vm->maxcpus; i++)
			free(vm->vcpu[i], M_VMM);
		free(vm->vcpu, M_VMM);
		sx_destroy(&vm->vcpus_init_lock);
	}
}

void
vm_destroy(struct vm *vm)
{
	vm_cleanup(vm, true);
	free(vm, M_VMM);
}

int
vm_reinit(struct vm *vm)
{
	int error;

	/*
	 * A virtual machine can be reset only if all vcpus are suspended.
	 */
	if (CPU_CMP(&vm->suspended_cpus, &vm->active_cpus) == 0) {
		vm_cleanup(vm, false);
		vm_init(vm, false);
		error = 0;
	} else {
		error = EBUSY;
	}

	return (error);
}

const char *
vm_name(struct vm *vm)
{
	return (vm->name);
}

int
vm_gla2gpa_nofault(struct vcpu *vcpu, struct vm_guest_paging *paging,
    uint64_t gla, int prot, uint64_t *gpa, int *is_fault)
{
	return (vmmops_gla2gpa(vcpu->cookie, paging, gla, prot, gpa, is_fault));
}

static int
vmm_reg_raz(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	*rval = 0;
	return (0);
}

static int
vmm_reg_read_arg(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	*rval = *(uint64_t *)arg;
	return (0);
}

static int
vmm_reg_wi(struct vcpu *vcpu, uint64_t wval, void *arg)
{
	return (0);
}

static int
vmm_write_oslar_el1(struct vcpu *vcpu, uint64_t wval, void *arg)
{
	struct hypctx *hypctx;

	hypctx = vcpu_get_cookie(vcpu);
	/* All other fields are RES0 & we don't do anything with this */
	/* TODO: Disable access to other debug state when locked */
	hypctx->dbg_oslock = (wval & OSLAR_OSLK) == OSLAR_OSLK;
	return (0);
}

static int
vmm_read_oslsr_el1(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	struct hypctx *hypctx;
	uint64_t val;

	hypctx = vcpu_get_cookie(vcpu);
	val = OSLSR_OSLM_1;
	if (hypctx->dbg_oslock)
		val |= OSLSR_OSLK;
	*rval = val;

	return (0);
}

static const struct vmm_special_reg vmm_special_regs[] = {
#define	SPECIAL_REG(_reg, _read, _write)				\
	{								\
		.esr_iss = ((_reg ## _op0) << ISS_MSR_OP0_SHIFT) |	\
		    ((_reg ## _op1) << ISS_MSR_OP1_SHIFT) |		\
		    ((_reg ## _CRn) << ISS_MSR_CRn_SHIFT) |		\
		    ((_reg ## _CRm) << ISS_MSR_CRm_SHIFT) |		\
		    ((_reg ## _op2) << ISS_MSR_OP2_SHIFT),		\
		.esr_mask = ISS_MSR_REG_MASK,				\
		.reg_read = (_read),					\
		.reg_write = (_write),					\
		.arg = NULL,						\
	}
#define	ID_SPECIAL_REG(_reg, _name)					\
	{								\
		.esr_iss = ((_reg ## _op0) << ISS_MSR_OP0_SHIFT) |	\
		    ((_reg ## _op1) << ISS_MSR_OP1_SHIFT) |		\
		    ((_reg ## _CRn) << ISS_MSR_CRn_SHIFT) |		\
		    ((_reg ## _CRm) << ISS_MSR_CRm_SHIFT) |		\
		    ((_reg ## _op2) << ISS_MSR_OP2_SHIFT),		\
		.esr_mask = ISS_MSR_REG_MASK,				\
		.reg_read = vmm_reg_read_arg,				\
		.reg_write = vmm_reg_wi,				\
		.arg = &(vmm_arch_regs._name),				\
	}

	/* ID registers */
	ID_SPECIAL_REG(ID_AA64PFR0_EL1, id_aa64pfr0),
	ID_SPECIAL_REG(ID_AA64DFR0_EL1, id_aa64dfr0),
	ID_SPECIAL_REG(ID_AA64ISAR0_EL1, id_aa64isar0),
	ID_SPECIAL_REG(ID_AA64MMFR0_EL1, id_aa64mmfr0),
	ID_SPECIAL_REG(ID_AA64MMFR1_EL1, id_aa64mmfr1),

	/*
	 * All other ID registers are read as zero.
	 * They are all in the op0=3, op1=0, CRn=0, CRm={0..7} space.
	 */
	{
		.esr_iss = (3 << ISS_MSR_OP0_SHIFT) |
		    (0 << ISS_MSR_OP1_SHIFT) |
		    (0 << ISS_MSR_CRn_SHIFT) |
		    (0 << ISS_MSR_CRm_SHIFT),
		.esr_mask = ISS_MSR_OP0_MASK | ISS_MSR_OP1_MASK |
		    ISS_MSR_CRn_MASK | (0x8 << ISS_MSR_CRm_SHIFT),
		.reg_read = vmm_reg_raz,
		.reg_write = vmm_reg_wi,
		.arg = NULL,
	},

	/* Counter physical registers */
	SPECIAL_REG(CNTP_CTL_EL0, vtimer_phys_ctl_read, vtimer_phys_ctl_write),
	SPECIAL_REG(CNTP_CVAL_EL0, vtimer_phys_cval_read,
	    vtimer_phys_cval_write),
	SPECIAL_REG(CNTP_TVAL_EL0, vtimer_phys_tval_read,
	    vtimer_phys_tval_write),
	SPECIAL_REG(CNTPCT_EL0, vtimer_phys_cnt_read, vtimer_phys_cnt_write),

	/* Debug registers */
	SPECIAL_REG(DBGPRCR_EL1, vmm_reg_raz, vmm_reg_wi),
	SPECIAL_REG(OSDLR_EL1, vmm_reg_raz, vmm_reg_wi),
	/* TODO: Exceptions on invalid access */
	SPECIAL_REG(OSLAR_EL1, vmm_reg_raz, vmm_write_oslar_el1),
	SPECIAL_REG(OSLSR_EL1, vmm_read_oslsr_el1, vmm_reg_wi),
#undef SPECIAL_REG
};

void
vm_register_reg_handler(struct vm *vm, uint64_t iss, uint64_t mask,
    reg_read_t reg_read, reg_write_t reg_write, void *arg)
{
	int i;

	for (i = 0; i < nitems(vm->special_reg); i++) {
		if (vm->special_reg[i].esr_iss == 0 &&
		    vm->special_reg[i].esr_mask == 0) {
			vm->special_reg[i].esr_iss = iss;
			vm->special_reg[i].esr_mask = mask;
			vm->special_reg[i].reg_read = reg_read;
			vm->special_reg[i].reg_write = reg_write;
			vm->special_reg[i].arg = arg;
			return;
		}
	}

	panic("%s: No free special register slot", __func__);
}

void
vm_deregister_reg_handler(struct vm *vm, uint64_t iss, uint64_t mask)
{
	int i;

	for (i = 0; i < nitems(vm->special_reg); i++) {
		if (vm->special_reg[i].esr_iss == iss &&
		    vm->special_reg[i].esr_mask == mask) {
			memset(&vm->special_reg[i], 0,
			    sizeof(vm->special_reg[i]));
			return;
		}
	}

	panic("%s: Invalid special register: iss %lx mask %lx", __func__, iss,
	    mask);
}

static int
vm_handle_reg_emul(struct vcpu *vcpu, bool *retu)
{
	struct vm *vm;
	struct vm_exit *vme;
	struct vre *vre;
	int i, rv;

	vm = vcpu->vm;
	vme = &vcpu->exitinfo;
	vre = &vme->u.reg_emul.vre;

	for (i = 0; i < nitems(vm->special_reg); i++) {
		if (vm->special_reg[i].esr_iss == 0 &&
		    vm->special_reg[i].esr_mask == 0)
			continue;

		if ((vre->inst_syndrome & vm->special_reg[i].esr_mask) ==
		    vm->special_reg[i].esr_iss) {
			rv = vmm_emulate_register(vcpu, vre,
			    vm->special_reg[i].reg_read,
			    vm->special_reg[i].reg_write,
			    vm->special_reg[i].arg);
			if (rv == 0) {
				*retu = false;
			}
			return (rv);
		}
	}
	for (i = 0; i < nitems(vmm_special_regs); i++) {
		if ((vre->inst_syndrome & vmm_special_regs[i].esr_mask) ==
		    vmm_special_regs[i].esr_iss) {
			rv = vmm_emulate_register(vcpu, vre,
			    vmm_special_regs[i].reg_read,
			    vmm_special_regs[i].reg_write,
			    vmm_special_regs[i].arg);
			if (rv == 0) {
				*retu = false;
			}
			return (rv);
		}
	}


	*retu = true;
	return (0);
}

void
vm_register_inst_handler(struct vm *vm, uint64_t start, uint64_t size,
    mem_region_read_t mmio_read, mem_region_write_t mmio_write)
{
	int i;

	for (i = 0; i < nitems(vm->mmio_region); i++) {
		if (vm->mmio_region[i].start == 0 &&
		    vm->mmio_region[i].end == 0) {
			vm->mmio_region[i].start = start;
			vm->mmio_region[i].end = start + size;
			vm->mmio_region[i].read = mmio_read;
			vm->mmio_region[i].write = mmio_write;
			return;
		}
	}

	panic("%s: No free MMIO region", __func__);
}

void
vm_deregister_inst_handler(struct vm *vm, uint64_t start, uint64_t size)
{
	int i;

	for (i = 0; i < nitems(vm->mmio_region); i++) {
		if (vm->mmio_region[i].start == start &&
		    vm->mmio_region[i].end == start + size) {
			memset(&vm->mmio_region[i], 0,
			    sizeof(vm->mmio_region[i]));
			return;
		}
	}

	panic("%s: Invalid MMIO region: %lx - %lx", __func__, start,
	    start + size);
}

static int
vm_handle_inst_emul(struct vcpu *vcpu, bool *retu)
{
	struct vm *vm;
	struct vm_exit *vme;
	struct vie *vie;
	struct hyp *hyp;
	uint64_t fault_ipa;
	struct vm_guest_paging *paging;
	struct vmm_mmio_region *vmr;
	int error, i;

	vm = vcpu->vm;
	hyp = vm->cookie;
	if (!hyp->vgic_attached)
		goto out_user;

	vme = &vcpu->exitinfo;
	vie = &vme->u.inst_emul.vie;
	paging = &vme->u.inst_emul.paging;

	fault_ipa = vme->u.inst_emul.gpa;

	vmr = NULL;
	for (i = 0; i < nitems(vm->mmio_region); i++) {
		if (vm->mmio_region[i].start <= fault_ipa &&
		    vm->mmio_region[i].end > fault_ipa) {
			vmr = &vm->mmio_region[i];
			break;
		}
	}
	if (vmr == NULL)
		goto out_user;

	error = vmm_emulate_instruction(vcpu, fault_ipa, vie, paging,
	    vmr->read, vmr->write, retu);
	return (error);

out_user:
	*retu = true;
	return (0);
}

int
vm_suspend(struct vm *vm, enum vm_suspend_how how)
{
	int i;

	if (how <= VM_SUSPEND_NONE || how >= VM_SUSPEND_LAST)
		return (EINVAL);

	if (atomic_cmpset_int(&vm->suspend, 0, how) == 0) {
		VM_CTR2(vm, "virtual machine already suspended %d/%d",
		    vm->suspend, how);
		return (EALREADY);
	}

	VM_CTR1(vm, "virtual machine successfully suspended %d", how);

	/*
	 * Notify all active vcpus that they are now suspended.
	 */
	for (i = 0; i < vm->maxcpus; i++) {
		if (CPU_ISSET(i, &vm->active_cpus))
			vcpu_notify_event(vm_vcpu(vm, i));
	}

	return (0);
}

void
vm_exit_suspended(struct vcpu *vcpu, uint64_t pc)
{
	struct vm *vm = vcpu->vm;
	struct vm_exit *vmexit;

	KASSERT(vm->suspend > VM_SUSPEND_NONE && vm->suspend < VM_SUSPEND_LAST,
	    ("vm_exit_suspended: invalid suspend type %d", vm->suspend));

	vmexit = vm_exitinfo(vcpu);
	vmexit->pc = pc;
	vmexit->inst_length = 4;
	vmexit->exitcode = VM_EXITCODE_SUSPENDED;
	vmexit->u.suspended.how = vm->suspend;
}

void
vm_exit_debug(struct vcpu *vcpu, uint64_t pc)
{
	struct vm_exit *vmexit;

	vmexit = vm_exitinfo(vcpu);
	vmexit->pc = pc;
	vmexit->inst_length = 4;
	vmexit->exitcode = VM_EXITCODE_DEBUG;
}

int
vm_activate_cpu(struct vcpu *vcpu)
{
	struct vm *vm = vcpu->vm;

	if (CPU_ISSET(vcpu->vcpuid, &vm->active_cpus))
		return (EBUSY);

	CPU_SET_ATOMIC(vcpu->vcpuid, &vm->active_cpus);
	return (0);

}

int
vm_suspend_cpu(struct vm *vm, struct vcpu *vcpu)
{
	if (vcpu == NULL) {
		vm->debug_cpus = vm->active_cpus;
		for (int i = 0; i < vm->maxcpus; i++) {
			if (CPU_ISSET(i, &vm->active_cpus))
				vcpu_notify_event(vm_vcpu(vm, i));
		}
	} else {
		if (!CPU_ISSET(vcpu->vcpuid, &vm->active_cpus))
			return (EINVAL);

		CPU_SET_ATOMIC(vcpu->vcpuid, &vm->debug_cpus);
		vcpu_notify_event(vcpu);
	}
	return (0);
}

int
vm_resume_cpu(struct vm *vm, struct vcpu *vcpu)
{

	if (vcpu == NULL) {
		CPU_ZERO(&vm->debug_cpus);
	} else {
		if (!CPU_ISSET(vcpu->vcpuid, &vm->debug_cpus))
			return (EINVAL);

		CPU_CLR_ATOMIC(vcpu->vcpuid, &vm->debug_cpus);
	}
	return (0);
}

int
vcpu_debugged(struct vcpu *vcpu)
{

	return (CPU_ISSET(vcpu->vcpuid, &vcpu->vm->debug_cpus));
}

cpuset_t
vm_active_cpus(struct vm *vm)
{

	return (vm->active_cpus);
}

cpuset_t
vm_debug_cpus(struct vm *vm)
{

	return (vm->debug_cpus);
}

cpuset_t
vm_suspended_cpus(struct vm *vm)
{

	return (vm->suspended_cpus);
}


void *
vcpu_stats(struct vcpu *vcpu)
{

	return (vcpu->stats);
}

/*
 * This function is called to ensure that a vcpu "sees" a pending event
 * as soon as possible:
 * - If the vcpu thread is sleeping then it is woken up.
 * - If the vcpu is running on a different host_cpu then an IPI will be directed
 *   to the host_cpu to cause the vcpu to trap into the hypervisor.
 */
static void
vcpu_notify_event_locked(struct vcpu *vcpu)
{
	int hostcpu;

	hostcpu = vcpu->hostcpu;
	if (vcpu->state == VCPU_RUNNING) {
		KASSERT(hostcpu != NOCPU, ("vcpu running on invalid hostcpu"));
		if (hostcpu != curcpu) {
			ipi_cpu(hostcpu, vmm_ipinum);
		} else {
			/*
			 * If the 'vcpu' is running on 'curcpu' then it must
			 * be sending a notification to itself (e.g. SELF_IPI).
			 * The pending event will be picked up when the vcpu
			 * transitions back to guest context.
			 */
		}
	} else {
		KASSERT(hostcpu == NOCPU, ("vcpu state %d not consistent "
		    "with hostcpu %d", vcpu->state, hostcpu));
		if (vcpu->state == VCPU_SLEEPING)
			wakeup_one(vcpu);
	}
}

void
vcpu_notify_event(struct vcpu *vcpu)
{
	vcpu_lock(vcpu);
	vcpu_notify_event_locked(vcpu);
	vcpu_unlock(vcpu);
}

struct vmspace *
vm_vmspace(struct vm *vm)
{
	return (vm->vmspace);
}

struct vm_mem *
vm_mem(struct vm *vm)
{
	return (&vm->mem);
}

static void
restore_guest_fpustate(struct vcpu *vcpu)
{

	/* flush host state to the pcb */
	vfp_save_state(curthread, curthread->td_pcb);
	/* Ensure the VFP state will be re-loaded when exiting the guest */
	PCPU_SET(fpcurthread, NULL);

	/* restore guest FPU state */
	vfp_enable();
	vfp_restore(vcpu->guestfpu);

	/*
	 * The FPU is now "dirty" with the guest's state so turn on emulation
	 * to trap any access to the FPU by the host.
	 */
	vfp_disable();
}

static void
save_guest_fpustate(struct vcpu *vcpu)
{
	if ((READ_SPECIALREG(cpacr_el1) & CPACR_FPEN_MASK) !=
	    CPACR_FPEN_TRAP_ALL1)
		panic("VFP not enabled in host!");

	/* save guest FPU state */
	vfp_enable();
	vfp_store(vcpu->guestfpu);
	vfp_disable();

	KASSERT(PCPU_GET(fpcurthread) == NULL,
	    ("%s: fpcurthread set with guest registers", __func__));
}
static int
vcpu_set_state_locked(struct vcpu *vcpu, enum vcpu_state newstate,
    bool from_idle)
{
	int error;

	vcpu_assert_locked(vcpu);

	/*
	 * State transitions from the vmmdev_ioctl() must always begin from
	 * the VCPU_IDLE state. This guarantees that there is only a single
	 * ioctl() operating on a vcpu at any point.
	 */
	if (from_idle) {
		while (vcpu->state != VCPU_IDLE) {
			vcpu_notify_event_locked(vcpu);
			msleep_spin(&vcpu->state, &vcpu->mtx, "vmstat", hz);
		}
	} else {
		KASSERT(vcpu->state != VCPU_IDLE, ("invalid transition from "
		    "vcpu idle state"));
	}

	if (vcpu->state == VCPU_RUNNING) {
		KASSERT(vcpu->hostcpu == curcpu, ("curcpu %d and hostcpu %d "
		    "mismatch for running vcpu", curcpu, vcpu->hostcpu));
	} else {
		KASSERT(vcpu->hostcpu == NOCPU, ("Invalid hostcpu %d for a "
		    "vcpu that is not running", vcpu->hostcpu));
	}

	/*
	 * The following state transitions are allowed:
	 * IDLE -> FROZEN -> IDLE
	 * FROZEN -> RUNNING -> FROZEN
	 * FROZEN -> SLEEPING -> FROZEN
	 */
	switch (vcpu->state) {
	case VCPU_IDLE:
	case VCPU_RUNNING:
	case VCPU_SLEEPING:
		error = (newstate != VCPU_FROZEN);
		break;
	case VCPU_FROZEN:
		error = (newstate == VCPU_FROZEN);
		break;
	default:
		error = 1;
		break;
	}

	if (error)
		return (EBUSY);

	vcpu->state = newstate;
	if (newstate == VCPU_RUNNING)
		vcpu->hostcpu = curcpu;
	else
		vcpu->hostcpu = NOCPU;

	if (newstate == VCPU_IDLE)
		wakeup(&vcpu->state);

	return (0);
}

static void
vcpu_require_state(struct vcpu *vcpu, enum vcpu_state newstate)
{
	int error;

	if ((error = vcpu_set_state(vcpu, newstate, false)) != 0)
		panic("Error %d setting state to %d\n", error, newstate);
}

static void
vcpu_require_state_locked(struct vcpu *vcpu, enum vcpu_state newstate)
{
	int error;

	if ((error = vcpu_set_state_locked(vcpu, newstate, false)) != 0)
		panic("Error %d setting state to %d", error, newstate);
}

int
vm_get_capability(struct vcpu *vcpu, int type, int *retval)
{
	if (type < 0 || type >= VM_CAP_MAX)
		return (EINVAL);

	return (vmmops_getcap(vcpu->cookie, type, retval));
}

int
vm_set_capability(struct vcpu *vcpu, int type, int val)
{
	if (type < 0 || type >= VM_CAP_MAX)
		return (EINVAL);

	return (vmmops_setcap(vcpu->cookie, type, val));
}

struct vm *
vcpu_vm(struct vcpu *vcpu)
{
	return (vcpu->vm);
}

int
vcpu_vcpuid(struct vcpu *vcpu)
{
	return (vcpu->vcpuid);
}

void *
vcpu_get_cookie(struct vcpu *vcpu)
{
	return (vcpu->cookie);
}

struct vcpu *
vm_vcpu(struct vm *vm, int vcpuid)
{
	return (vm->vcpu[vcpuid]);
}

int
vcpu_set_state(struct vcpu *vcpu, enum vcpu_state newstate, bool from_idle)
{
	int error;

	vcpu_lock(vcpu);
	error = vcpu_set_state_locked(vcpu, newstate, from_idle);
	vcpu_unlock(vcpu);

	return (error);
}

enum vcpu_state
vcpu_get_state(struct vcpu *vcpu, int *hostcpu)
{
	enum vcpu_state state;

	vcpu_lock(vcpu);
	state = vcpu->state;
	if (hostcpu != NULL)
		*hostcpu = vcpu->hostcpu;
	vcpu_unlock(vcpu);

	return (state);
}

int
vm_get_register(struct vcpu *vcpu, int reg, uint64_t *retval)
{

	if (reg >= VM_REG_LAST)
		return (EINVAL);

	return (vmmops_getreg(vcpu->cookie, reg, retval));
}

int
vm_set_register(struct vcpu *vcpu, int reg, uint64_t val)
{
	int error;

	if (reg >= VM_REG_LAST)
		return (EINVAL);
	error = vmmops_setreg(vcpu->cookie, reg, val);
	if (error || reg != VM_REG_GUEST_PC)
		return (error);

	vcpu->nextpc = val;

	return (0);
}

void *
vm_get_cookie(struct vm *vm)
{
	return (vm->cookie);
}

int
vm_inject_exception(struct vcpu *vcpu, uint64_t esr, uint64_t far)
{
	return (vmmops_exception(vcpu->cookie, esr, far));
}

int
vm_attach_vgic(struct vm *vm, struct vm_vgic_descr *descr)
{
	return (vgic_attach_to_vm(vm->cookie, descr));
}

int
vm_assert_irq(struct vm *vm, uint32_t irq)
{
	return (vgic_inject_irq(vm->cookie, -1, irq, true));
}

int
vm_deassert_irq(struct vm *vm, uint32_t irq)
{
	return (vgic_inject_irq(vm->cookie, -1, irq, false));
}

int
vm_raise_msi(struct vm *vm, uint64_t msg, uint64_t addr, int bus, int slot,
    int func)
{
	/* TODO: Should we raise an SError? */
	return (vgic_inject_msi(vm->cookie, msg, addr));
}

static int
vm_handle_smccc_call(struct vcpu *vcpu, struct vm_exit *vme, bool *retu)
{
	struct hypctx *hypctx;
	int i;

	hypctx = vcpu_get_cookie(vcpu);

	if ((hypctx->tf.tf_esr & ESR_ELx_ISS_MASK) != 0)
		return (1);

	vme->exitcode = VM_EXITCODE_SMCCC;
	vme->u.smccc_call.func_id = hypctx->tf.tf_x[0];
	for (i = 0; i < nitems(vme->u.smccc_call.args); i++)
		vme->u.smccc_call.args[i] = hypctx->tf.tf_x[i + 1];

	*retu = true;
	return (0);
}

static int
vm_handle_wfi(struct vcpu *vcpu, struct vm_exit *vme, bool *retu)
{
	struct vm *vm;

	vm = vcpu->vm;
	vcpu_lock(vcpu);
	while (1) {
		if (vm->suspend)
			break;

		if (vgic_has_pending_irq(vcpu->cookie))
			break;

		if (vcpu_should_yield(vcpu))
			break;

		vcpu_require_state_locked(vcpu, VCPU_SLEEPING);
		/*
		 * XXX msleep_spin() cannot be interrupted by signals so
		 * wake up periodically to check pending signals.
		 */
		msleep_spin(vcpu, &vcpu->mtx, "vmidle", hz);
		vcpu_require_state_locked(vcpu, VCPU_FROZEN);
	}
	vcpu_unlock(vcpu);

	*retu = false;
	return (0);
}

static int
vm_handle_paging(struct vcpu *vcpu, bool *retu)
{
	struct vm *vm = vcpu->vm;
	struct vm_exit *vme;
	struct vm_map *map;
	uint64_t addr, esr;
	pmap_t pmap;
	int ftype, rv;

	vme = &vcpu->exitinfo;

	pmap = vmspace_pmap(vcpu->vm->vmspace);
	addr = vme->u.paging.gpa;
	esr = vme->u.paging.esr;

	/* The page exists, but the page table needs to be updated. */
	if (pmap_fault(pmap, esr, addr) == KERN_SUCCESS)
		return (0);

	switch (ESR_ELx_EXCEPTION(esr)) {
	case EXCP_INSN_ABORT_L:
	case EXCP_DATA_ABORT_L:
		ftype = VM_PROT_EXECUTE | VM_PROT_READ | VM_PROT_WRITE;
		break;
	default:
		panic("%s: Invalid exception (esr = %lx)", __func__, esr);
	}

	map = &vm->vmspace->vm_map;
	rv = vm_fault(map, vme->u.paging.gpa, ftype, VM_FAULT_NORMAL, NULL);
	if (rv != KERN_SUCCESS)
		return (EFAULT);

	return (0);
}

static int
vm_handle_suspend(struct vcpu *vcpu, bool *retu)
{
	struct vm *vm = vcpu->vm;
	int error, i;
	struct thread *td;

	error = 0;
	td = curthread;

	CPU_SET_ATOMIC(vcpu->vcpuid, &vm->suspended_cpus);

	/*
	 * Wait until all 'active_cpus' have suspended themselves.
	 *
	 * Since a VM may be suspended at any time including when one or
	 * more vcpus are doing a rendezvous we need to call the rendezvous
	 * handler while we are waiting to prevent a deadlock.
	 */
	vcpu_lock(vcpu);
	while (error == 0) {
		if (CPU_CMP(&vm->suspended_cpus, &vm->active_cpus) == 0)
			break;

		vcpu_require_state_locked(vcpu, VCPU_SLEEPING);
		msleep_spin(vcpu, &vcpu->mtx, "vmsusp", hz);
		vcpu_require_state_locked(vcpu, VCPU_FROZEN);
		if (td_ast_pending(td, TDA_SUSPEND)) {
			vcpu_unlock(vcpu);
			error = thread_check_susp(td, false);
			vcpu_lock(vcpu);
		}
	}
	vcpu_unlock(vcpu);

	/*
	 * Wakeup the other sleeping vcpus and return to userspace.
	 */
	for (i = 0; i < vm->maxcpus; i++) {
		if (CPU_ISSET(i, &vm->suspended_cpus)) {
			vcpu_notify_event(vm_vcpu(vm, i));
		}
	}

	*retu = true;
	return (error);
}

int
vm_run(struct vcpu *vcpu)
{
	struct vm *vm = vcpu->vm;
	struct vm_eventinfo evinfo;
	int error, vcpuid;
	struct vm_exit *vme;
	bool retu;
	pmap_t pmap;

	vcpuid = vcpu->vcpuid;

	if (!CPU_ISSET(vcpuid, &vm->active_cpus))
		return (EINVAL);

	if (CPU_ISSET(vcpuid, &vm->suspended_cpus))
		return (EINVAL);

	pmap = vmspace_pmap(vm->vmspace);
	vme = &vcpu->exitinfo;
	evinfo.rptr = NULL;
	evinfo.sptr = &vm->suspend;
	evinfo.iptr = NULL;
restart:
	critical_enter();

	restore_guest_fpustate(vcpu);

	vcpu_require_state(vcpu, VCPU_RUNNING);
	error = vmmops_run(vcpu->cookie, vcpu->nextpc, pmap, &evinfo);
	vcpu_require_state(vcpu, VCPU_FROZEN);

	save_guest_fpustate(vcpu);

	critical_exit();

	if (error == 0) {
		retu = false;
		switch (vme->exitcode) {
		case VM_EXITCODE_INST_EMUL:
			vcpu->nextpc = vme->pc + vme->inst_length;
			error = vm_handle_inst_emul(vcpu, &retu);
			break;

		case VM_EXITCODE_REG_EMUL:
			vcpu->nextpc = vme->pc + vme->inst_length;
			error = vm_handle_reg_emul(vcpu, &retu);
			break;

		case VM_EXITCODE_HVC:
			/*
			 * The HVC instruction saves the address for the
			 * next instruction as the return address.
			 */
			vcpu->nextpc = vme->pc;
			/*
			 * The PSCI call can change the exit information in the
			 * case of suspend/reset/poweroff/cpu off/cpu on.
			 */
			error = vm_handle_smccc_call(vcpu, vme, &retu);
			break;

		case VM_EXITCODE_WFI:
			vcpu->nextpc = vme->pc + vme->inst_length;
			error = vm_handle_wfi(vcpu, vme, &retu);
			break;

		case VM_EXITCODE_PAGING:
			vcpu->nextpc = vme->pc;
			error = vm_handle_paging(vcpu, &retu);
			break;

		case VM_EXITCODE_SUSPENDED:
			vcpu->nextpc = vme->pc;
			error = vm_handle_suspend(vcpu, &retu);
			break;

		default:
			/* Handle in userland */
			vcpu->nextpc = vme->pc;
			retu = true;
			break;
		}
	}

	if (error == 0 && retu == false)
		goto restart;

	return (error);
}
