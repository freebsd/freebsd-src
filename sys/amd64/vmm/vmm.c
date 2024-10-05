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

#include <sys/cdefs.h>
#include "opt_bhyve_snapshot.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>
#include <vm/vnode_pager.h>
#include <vm/swap_pager.h>
#include <vm/uma.h>

#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/smp.h>
#include <machine/md_var.h>
#include <x86/psl.h>
#include <x86/apicreg.h>
#include <x86/ifunc.h>

#include <machine/vmm.h>
#include <machine/vmm_instruction_emul.h>
#include <machine/vmm_snapshot.h>

#include <dev/vmm/vmm_dev.h>
#include <dev/vmm/vmm_ktr.h>

#include "vmm_ioport.h"
#include "vmm_host.h"
#include "vmm_mem.h"
#include "vmm_util.h"
#include "vatpic.h"
#include "vatpit.h"
#include "vhpet.h"
#include "vioapic.h"
#include "vlapic.h"
#include "vpmtmr.h"
#include "vrtc.h"
#include "vmm_stat.h"
#include "vmm_lapic.h"

#include "io/ppt.h"
#include "io/iommu.h"

struct vlapic;

/*
 * Initialization:
 * (a) allocated when vcpu is created
 * (i) initialized when vcpu is created and when it is reinitialized
 * (o) initialized the first time the vcpu is created
 * (x) initialized before use
 */
struct vcpu {
	struct mtx 	mtx;		/* (o) protects 'state' and 'hostcpu' */
	enum vcpu_state	state;		/* (o) vcpu state */
	int		vcpuid;		/* (o) */
	int		hostcpu;	/* (o) vcpu's host cpu */
	int		reqidle;	/* (i) request vcpu to idle */
	struct vm	*vm;		/* (o) */
	void		*cookie;	/* (i) cpu-specific data */
	struct vlapic	*vlapic;	/* (i) APIC device model */
	enum x2apic_state x2apic_state;	/* (i) APIC mode */
	uint64_t	exitintinfo;	/* (i) events pending at VM exit */
	int		nmi_pending;	/* (i) NMI pending */
	int		extint_pending;	/* (i) INTR pending */
	int	exception_pending;	/* (i) exception pending */
	int	exc_vector;		/* (x) exception collateral */
	int	exc_errcode_valid;
	uint32_t exc_errcode;
	struct savefpu	*guestfpu;	/* (a,i) guest fpu state */
	uint64_t	guest_xcr0;	/* (i) guest %xcr0 register */
	void		*stats;		/* (a,i) statistics */
	struct vm_exit	exitinfo;	/* (x) exit reason and collateral */
	cpuset_t	exitinfo_cpuset; /* (x) storage for vmexit handlers */
	uint64_t	nextrip;	/* (x) next instruction to execute */
	uint64_t	tsc_offset;	/* (o) TSC offsetting */
};

#define	vcpu_lock_init(v)	mtx_init(&((v)->mtx), "vcpu lock", 0, MTX_SPIN)
#define	vcpu_lock_destroy(v)	mtx_destroy(&((v)->mtx))
#define	vcpu_lock(v)		mtx_lock_spin(&((v)->mtx))
#define	vcpu_unlock(v)		mtx_unlock_spin(&((v)->mtx))
#define	vcpu_assert_locked(v)	mtx_assert(&((v)->mtx), MA_OWNED)

struct mem_seg {
	size_t	len;
	bool	sysmem;
	struct vm_object *object;
};
#define	VM_MAX_MEMSEGS	4

struct mem_map {
	vm_paddr_t	gpa;
	size_t		len;
	vm_ooffset_t	segoff;
	int		segid;
	int		prot;
	int		flags;
};
#define	VM_MAX_MEMMAPS	8

/*
 * Initialization:
 * (o) initialized the first time the VM is created
 * (i) initialized when VM is created and when it is reinitialized
 * (x) initialized before use
 *
 * Locking:
 * [m] mem_segs_lock
 * [r] rendezvous_mtx
 * [v] reads require one frozen vcpu, writes require freezing all vcpus
 */
struct vm {
	void		*cookie;		/* (i) cpu-specific data */
	void		*iommu;			/* (x) iommu-specific data */
	struct vhpet	*vhpet;			/* (i) virtual HPET */
	struct vioapic	*vioapic;		/* (i) virtual ioapic */
	struct vatpic	*vatpic;		/* (i) virtual atpic */
	struct vatpit	*vatpit;		/* (i) virtual atpit */
	struct vpmtmr	*vpmtmr;		/* (i) virtual ACPI PM timer */
	struct vrtc	*vrtc;			/* (o) virtual RTC */
	volatile cpuset_t active_cpus;		/* (i) active vcpus */
	volatile cpuset_t debug_cpus;		/* (i) vcpus stopped for debug */
	cpuset_t	startup_cpus;		/* (i) [r] waiting for startup */
	int		suspend;		/* (i) stop VM execution */
	bool		dying;			/* (o) is dying */
	volatile cpuset_t suspended_cpus; 	/* (i) suspended vcpus */
	volatile cpuset_t halted_cpus;		/* (x) cpus in a hard halt */
	cpuset_t	rendezvous_req_cpus;	/* (x) [r] rendezvous requested */
	cpuset_t	rendezvous_done_cpus;	/* (x) [r] rendezvous finished */
	void		*rendezvous_arg;	/* (x) [r] rendezvous func/arg */
	vm_rendezvous_func_t rendezvous_func;
	struct mtx	rendezvous_mtx;		/* (o) rendezvous lock */
	struct mem_map	mem_maps[VM_MAX_MEMMAPS]; /* (i) [m+v] guest address space */
	struct mem_seg	mem_segs[VM_MAX_MEMSEGS]; /* (o) [m+v] guest memory regions */
	struct vmspace	*vmspace;		/* (o) guest's address space */
	char		name[VM_MAX_NAMELEN+1];	/* (o) virtual machine name */
	struct vcpu	**vcpu;			/* (o) guest vcpus */
	/* The following describe the vm cpu topology */
	uint16_t	sockets;		/* (o) num of sockets */
	uint16_t	cores;			/* (o) num of cores/socket */
	uint16_t	threads;		/* (o) num of threads/core */
	uint16_t	maxcpus;		/* (o) max pluggable cpus */
	struct sx	mem_segs_lock;		/* (o) */
	struct sx	vcpus_init_lock;	/* (o) */
};

#define	VMM_CTR0(vcpu, format)						\
	VCPU_CTR0((vcpu)->vm, (vcpu)->vcpuid, format)

#define	VMM_CTR1(vcpu, format, p1)					\
	VCPU_CTR1((vcpu)->vm, (vcpu)->vcpuid, format, p1)

#define	VMM_CTR2(vcpu, format, p1, p2)					\
	VCPU_CTR2((vcpu)->vm, (vcpu)->vcpuid, format, p1, p2)

#define	VMM_CTR3(vcpu, format, p1, p2, p3)				\
	VCPU_CTR3((vcpu)->vm, (vcpu)->vcpuid, format, p1, p2, p3)

#define	VMM_CTR4(vcpu, format, p1, p2, p3, p4)				\
	VCPU_CTR4((vcpu)->vm, (vcpu)->vcpuid, format, p1, p2, p3, p4)

static int vmm_initialized;

static void	vmmops_panic(void);

static void
vmmops_panic(void)
{
	panic("vmm_ops func called when !vmm_is_intel() && !vmm_is_svm()");
}

#define	DEFINE_VMMOPS_IFUNC(ret_type, opname, args)			\
    DEFINE_IFUNC(static, ret_type, vmmops_##opname, args)		\
    {									\
    	if (vmm_is_intel())						\
    		return (vmm_ops_intel.opname);				\
    	else if (vmm_is_svm())						\
    		return (vmm_ops_amd.opname);				\
    	else								\
    		return ((ret_type (*)args)vmmops_panic);		\
    }

DEFINE_VMMOPS_IFUNC(int, modinit, (int ipinum))
DEFINE_VMMOPS_IFUNC(int, modcleanup, (void))
DEFINE_VMMOPS_IFUNC(void, modresume, (void))
DEFINE_VMMOPS_IFUNC(void *, init, (struct vm *vm, struct pmap *pmap))
DEFINE_VMMOPS_IFUNC(int, run, (void *vcpui, register_t rip, struct pmap *pmap,
    struct vm_eventinfo *info))
DEFINE_VMMOPS_IFUNC(void, cleanup, (void *vmi))
DEFINE_VMMOPS_IFUNC(void *, vcpu_init, (void *vmi, struct vcpu *vcpu,
    int vcpu_id))
DEFINE_VMMOPS_IFUNC(void, vcpu_cleanup, (void *vcpui))
DEFINE_VMMOPS_IFUNC(int, getreg, (void *vcpui, int num, uint64_t *retval))
DEFINE_VMMOPS_IFUNC(int, setreg, (void *vcpui, int num, uint64_t val))
DEFINE_VMMOPS_IFUNC(int, getdesc, (void *vcpui, int num, struct seg_desc *desc))
DEFINE_VMMOPS_IFUNC(int, setdesc, (void *vcpui, int num, struct seg_desc *desc))
DEFINE_VMMOPS_IFUNC(int, getcap, (void *vcpui, int num, int *retval))
DEFINE_VMMOPS_IFUNC(int, setcap, (void *vcpui, int num, int val))
DEFINE_VMMOPS_IFUNC(struct vmspace *, vmspace_alloc, (vm_offset_t min,
    vm_offset_t max))
DEFINE_VMMOPS_IFUNC(void, vmspace_free, (struct vmspace *vmspace))
DEFINE_VMMOPS_IFUNC(struct vlapic *, vlapic_init, (void *vcpui))
DEFINE_VMMOPS_IFUNC(void, vlapic_cleanup, (struct vlapic *vlapic))
#ifdef BHYVE_SNAPSHOT
DEFINE_VMMOPS_IFUNC(int, vcpu_snapshot, (void *vcpui,
    struct vm_snapshot_meta *meta))
DEFINE_VMMOPS_IFUNC(int, restore_tsc, (void *vcpui, uint64_t now))
#endif

SDT_PROVIDER_DEFINE(vmm);

static MALLOC_DEFINE(M_VM, "vm", "vm");

/* statistics */
static VMM_STAT(VCPU_TOTAL_RUNTIME, "vcpu total runtime");

SYSCTL_NODE(_hw, OID_AUTO, vmm, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    NULL);

/*
 * Halt the guest if all vcpus are executing a HLT instruction with
 * interrupts disabled.
 */
static int halt_detection_enabled = 1;
SYSCTL_INT(_hw_vmm, OID_AUTO, halt_detection, CTLFLAG_RDTUN,
    &halt_detection_enabled, 0,
    "Halt VM if all vcpus execute HLT with interrupts disabled");

static int vmm_ipinum;
SYSCTL_INT(_hw_vmm, OID_AUTO, ipinum, CTLFLAG_RD, &vmm_ipinum, 0,
    "IPI vector used for vcpu notifications");

static int trace_guest_exceptions;
SYSCTL_INT(_hw_vmm, OID_AUTO, trace_guest_exceptions, CTLFLAG_RDTUN,
    &trace_guest_exceptions, 0,
    "Trap into hypervisor on all guest exceptions and reflect them back");

static int trap_wbinvd;
SYSCTL_INT(_hw_vmm, OID_AUTO, trap_wbinvd, CTLFLAG_RDTUN, &trap_wbinvd, 0,
    "WBINVD triggers a VM-exit");

u_int vm_maxcpu;
SYSCTL_UINT(_hw_vmm, OID_AUTO, maxcpu, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &vm_maxcpu, 0, "Maximum number of vCPUs");

static void vm_free_memmap(struct vm *vm, int ident);
static bool sysmem_mapping(struct vm *vm, struct mem_map *mm);
static void vcpu_notify_event_locked(struct vcpu *vcpu, bool lapic_intr);

/* global statistics */
VMM_STAT(VCPU_MIGRATIONS, "vcpu migration across host cpus");
VMM_STAT(VMEXIT_COUNT, "total number of vm exits");
VMM_STAT(VMEXIT_EXTINT, "vm exits due to external interrupt");
VMM_STAT(VMEXIT_HLT, "number of times hlt was intercepted");
VMM_STAT(VMEXIT_CR_ACCESS, "number of times %cr access was intercepted");
VMM_STAT(VMEXIT_RDMSR, "number of times rdmsr was intercepted");
VMM_STAT(VMEXIT_WRMSR, "number of times wrmsr was intercepted");
VMM_STAT(VMEXIT_MTRAP, "number of monitor trap exits");
VMM_STAT(VMEXIT_PAUSE, "number of times pause was intercepted");
VMM_STAT(VMEXIT_INTR_WINDOW, "vm exits due to interrupt window opening");
VMM_STAT(VMEXIT_NMI_WINDOW, "vm exits due to nmi window opening");
VMM_STAT(VMEXIT_INOUT, "number of times in/out was intercepted");
VMM_STAT(VMEXIT_CPUID, "number of times cpuid was intercepted");
VMM_STAT(VMEXIT_NESTED_FAULT, "vm exits due to nested page fault");
VMM_STAT(VMEXIT_INST_EMUL, "vm exits for instruction emulation");
VMM_STAT(VMEXIT_UNKNOWN, "number of vm exits for unknown reason");
VMM_STAT(VMEXIT_ASTPENDING, "number of times astpending at exit");
VMM_STAT(VMEXIT_REQIDLE, "number of times idle requested at exit");
VMM_STAT(VMEXIT_USERSPACE, "number of vm exits handled in userspace");
VMM_STAT(VMEXIT_RENDEZVOUS, "number of times rendezvous pending at exit");
VMM_STAT(VMEXIT_EXCEPTION, "number of vm exits due to exceptions");

/*
 * Upper limit on vm_maxcpu.  Limited by use of uint16_t types for CPU
 * counts as well as range of vpid values for VT-x and by the capacity
 * of cpuset_t masks.  The call to new_unrhdr() in vpid_init() in
 * vmx.c requires 'vm_maxcpu + 1 <= 0xffff', hence the '- 1' below.
 */
#define	VM_MAXCPU	MIN(0xffff - 1, CPU_SETSIZE)

#ifdef KTR
static const char *
vcpu_state2str(enum vcpu_state state)
{

	switch (state) {
	case VCPU_IDLE:
		return ("idle");
	case VCPU_FROZEN:
		return ("frozen");
	case VCPU_RUNNING:
		return ("running");
	case VCPU_SLEEPING:
		return ("sleeping");
	default:
		return ("unknown");
	}
}
#endif

static void
vcpu_cleanup(struct vcpu *vcpu, bool destroy)
{
	vmmops_vlapic_cleanup(vcpu->vlapic);
	vmmops_vcpu_cleanup(vcpu->cookie);
	vcpu->cookie = NULL;
	if (destroy) {
		vmm_stat_free(vcpu->stats);
		fpu_save_area_free(vcpu->guestfpu);
		vcpu_lock_destroy(vcpu);
		free(vcpu, M_VM);
	}
}

static struct vcpu *
vcpu_alloc(struct vm *vm, int vcpu_id)
{
	struct vcpu *vcpu;

	KASSERT(vcpu_id >= 0 && vcpu_id < vm->maxcpus,
	    ("vcpu_init: invalid vcpu %d", vcpu_id));

	vcpu = malloc(sizeof(*vcpu), M_VM, M_WAITOK | M_ZERO);
	vcpu_lock_init(vcpu);
	vcpu->state = VCPU_IDLE;
	vcpu->hostcpu = NOCPU;
	vcpu->vcpuid = vcpu_id;
	vcpu->vm = vm;
	vcpu->guestfpu = fpu_save_area_alloc();
	vcpu->stats = vmm_stat_alloc();
	vcpu->tsc_offset = 0;
	return (vcpu);
}

static void
vcpu_init(struct vcpu *vcpu)
{
	vcpu->cookie = vmmops_vcpu_init(vcpu->vm->cookie, vcpu, vcpu->vcpuid);
	vcpu->vlapic = vmmops_vlapic_init(vcpu->cookie);
	vm_set_x2apic_state(vcpu, X2APIC_DISABLED);
	vcpu->reqidle = 0;
	vcpu->exitintinfo = 0;
	vcpu->nmi_pending = 0;
	vcpu->extint_pending = 0;
	vcpu->exception_pending = 0;
	vcpu->guest_xcr0 = XFEATURE_ENABLED_X87;
	fpu_save_area_reset(vcpu->guestfpu);
	vmm_stat_init(vcpu->stats);
}

int
vcpu_trace_exceptions(struct vcpu *vcpu)
{

	return (trace_guest_exceptions);
}

int
vcpu_trap_wbinvd(struct vcpu *vcpu)
{
	return (trap_wbinvd);
}

struct vm_exit *
vm_exitinfo(struct vcpu *vcpu)
{
	return (&vcpu->exitinfo);
}

cpuset_t *
vm_exitinfo_cpuset(struct vcpu *vcpu)
{
	return (&vcpu->exitinfo_cpuset);
}

static int
vmm_init(void)
{
	int error;

	if (!vmm_is_hw_supported())
		return (ENXIO);

	vm_maxcpu = mp_ncpus;
	TUNABLE_INT_FETCH("hw.vmm.maxcpu", &vm_maxcpu);

	if (vm_maxcpu > VM_MAXCPU) {
		printf("vmm: vm_maxcpu clamped to %u\n", VM_MAXCPU);
		vm_maxcpu = VM_MAXCPU;
	}
	if (vm_maxcpu == 0)
		vm_maxcpu = 1;

	vmm_host_state_init();

	vmm_ipinum = lapic_ipi_alloc(pti ? &IDTVEC(justreturn1_pti) :
	    &IDTVEC(justreturn));
	if (vmm_ipinum < 0)
		vmm_ipinum = IPI_AST;

	error = vmm_mem_init();
	if (error)
		return (error);

	vmm_resume_p = vmmops_modresume;

	return (vmmops_modinit(vmm_ipinum));
}

static int
vmm_handler(module_t mod, int what, void *arg)
{
	int error;

	switch (what) {
	case MOD_LOAD:
		if (vmm_is_hw_supported()) {
			vmmdev_init();
			error = vmm_init();
			if (error == 0)
				vmm_initialized = 1;
		} else {
			error = ENXIO;
		}
		break;
	case MOD_UNLOAD:
		if (vmm_is_hw_supported()) {
			error = vmmdev_cleanup();
			if (error == 0) {
				vmm_resume_p = NULL;
				iommu_cleanup();
				if (vmm_ipinum != IPI_AST)
					lapic_ipi_free(vmm_ipinum);
				error = vmmops_modcleanup();
				/*
				 * Something bad happened - prevent new
				 * VMs from being created
				 */
				if (error)
					vmm_initialized = 0;
			}
		} else {
			error = 0;
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
 * - VT-x initialization requires smp_rendezvous() and therefore must happen
 *   after SMP is fully functional (after SI_SUB_SMP).
 */
DECLARE_MODULE(vmm, vmm_kmod, SI_SUB_SMP + 1, SI_ORDER_ANY);
MODULE_VERSION(vmm, 1);

static void
vm_init(struct vm *vm, bool create)
{
	vm->cookie = vmmops_init(vm, vmspace_pmap(vm->vmspace));
	vm->iommu = NULL;
	vm->vioapic = vioapic_init(vm);
	vm->vhpet = vhpet_init(vm);
	vm->vatpic = vatpic_init(vm);
	vm->vatpit = vatpit_init(vm);
	vm->vpmtmr = vpmtmr_init(vm);
	if (create)
		vm->vrtc = vrtc_init(vm);

	CPU_ZERO(&vm->active_cpus);
	CPU_ZERO(&vm->debug_cpus);
	CPU_ZERO(&vm->startup_cpus);

	vm->suspend = 0;
	CPU_ZERO(&vm->suspended_cpus);

	if (!create) {
		for (int i = 0; i < vm->maxcpus; i++) {
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

	vcpu = atomic_load_ptr(&vm->vcpu[vcpuid]);
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

/*
 * The default CPU topology is a single thread per package.
 */
u_int cores_per_package = 1;
u_int threads_per_core = 1;

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

	if (name == NULL || strnlen(name, VM_MAX_NAMELEN + 1) ==
	    VM_MAX_NAMELEN + 1)
		return (EINVAL);

	vmspace = vmmops_vmspace_alloc(0, VM_MAXUSER_ADDRESS_LA48);
	if (vmspace == NULL)
		return (ENOMEM);

	vm = malloc(sizeof(struct vm), M_VM, M_WAITOK | M_ZERO);
	strcpy(vm->name, name);
	vm->vmspace = vmspace;
	mtx_init(&vm->rendezvous_mtx, "vm rendezvous lock", 0, MTX_DEF);
	sx_init(&vm->mem_segs_lock, "vm mem_segs");
	sx_init(&vm->vcpus_init_lock, "vm vcpus");
	vm->vcpu = malloc(sizeof(*vm->vcpu) * vm_maxcpu, M_VM, M_WAITOK |
	    M_ZERO);

	vm->sockets = 1;
	vm->cores = cores_per_package;	/* XXX backwards compatibility */
	vm->threads = threads_per_core;	/* XXX backwards compatibility */
	vm->maxcpus = vm_maxcpu;

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
    uint16_t threads, uint16_t maxcpus __unused)
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
	struct mem_map *mm;
	int i;

	if (destroy)
		vm_xlock_memsegs(vm);

	ppt_unassign_all(vm);

	if (vm->iommu != NULL)
		iommu_destroy_domain(vm->iommu);

	if (destroy)
		vrtc_cleanup(vm->vrtc);
	else
		vrtc_reset(vm->vrtc);
	vpmtmr_cleanup(vm->vpmtmr);
	vatpit_cleanup(vm->vatpit);
	vhpet_cleanup(vm->vhpet);
	vatpic_cleanup(vm->vatpic);
	vioapic_cleanup(vm->vioapic);

	for (i = 0; i < vm->maxcpus; i++) {
		if (vm->vcpu[i] != NULL)
			vcpu_cleanup(vm->vcpu[i], destroy);
	}

	vmmops_cleanup(vm->cookie);

	/*
	 * System memory is removed from the guest address space only when
	 * the VM is destroyed. This is because the mapping remains the same
	 * across VM reset.
	 *
	 * Device memory can be relocated by the guest (e.g. using PCI BARs)
	 * so those mappings are removed on a VM reset.
	 */
	for (i = 0; i < VM_MAX_MEMMAPS; i++) {
		mm = &vm->mem_maps[i];
		if (destroy || !sysmem_mapping(vm, mm))
			vm_free_memmap(vm, i);
	}

	if (destroy) {
		for (i = 0; i < VM_MAX_MEMSEGS; i++)
			vm_free_memseg(vm, i);
		vm_unlock_memsegs(vm);

		vmmops_vmspace_free(vm->vmspace);
		vm->vmspace = NULL;

		free(vm->vcpu, M_VM);
		sx_destroy(&vm->vcpus_init_lock);
		sx_destroy(&vm->mem_segs_lock);
		mtx_destroy(&vm->rendezvous_mtx);
	}
}

void
vm_destroy(struct vm *vm)
{
	vm_cleanup(vm, true);
	free(vm, M_VM);
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

void
vm_slock_memsegs(struct vm *vm)
{
	sx_slock(&vm->mem_segs_lock);
}

void
vm_xlock_memsegs(struct vm *vm)
{
	sx_xlock(&vm->mem_segs_lock);
}

void
vm_unlock_memsegs(struct vm *vm)
{
	sx_unlock(&vm->mem_segs_lock);
}

int
vm_map_mmio(struct vm *vm, vm_paddr_t gpa, size_t len, vm_paddr_t hpa)
{
	vm_object_t obj;

	if ((obj = vmm_mmio_alloc(vm->vmspace, gpa, len, hpa)) == NULL)
		return (ENOMEM);
	else
		return (0);
}

int
vm_unmap_mmio(struct vm *vm, vm_paddr_t gpa, size_t len)
{

	vmm_mmio_free(vm->vmspace, gpa, len);
	return (0);
}

/*
 * Return 'true' if 'gpa' is allocated in the guest address space.
 *
 * This function is called in the context of a running vcpu which acts as
 * an implicit lock on 'vm->mem_maps[]'.
 */
bool
vm_mem_allocated(struct vcpu *vcpu, vm_paddr_t gpa)
{
	struct vm *vm = vcpu->vm;
	struct mem_map *mm;
	int i;

#ifdef INVARIANTS
	int hostcpu, state;
	state = vcpu_get_state(vcpu, &hostcpu);
	KASSERT(state == VCPU_RUNNING && hostcpu == curcpu,
	    ("%s: invalid vcpu state %d/%d", __func__, state, hostcpu));
#endif

	for (i = 0; i < VM_MAX_MEMMAPS; i++) {
		mm = &vm->mem_maps[i];
		if (mm->len != 0 && gpa >= mm->gpa && gpa < mm->gpa + mm->len)
			return (true);		/* 'gpa' is sysmem or devmem */
	}

	if (ppt_is_mmio(vm, gpa))
		return (true);			/* 'gpa' is pci passthru mmio */

	return (false);
}

int
vm_alloc_memseg(struct vm *vm, int ident, size_t len, bool sysmem)
{
	struct mem_seg *seg;
	vm_object_t obj;

	sx_assert(&vm->mem_segs_lock, SX_XLOCKED);

	if (ident < 0 || ident >= VM_MAX_MEMSEGS)
		return (EINVAL);

	if (len == 0 || (len & PAGE_MASK))
		return (EINVAL);

	seg = &vm->mem_segs[ident];
	if (seg->object != NULL) {
		if (seg->len == len && seg->sysmem == sysmem)
			return (EEXIST);
		else
			return (EINVAL);
	}

	obj = vm_object_allocate(OBJT_SWAP, len >> PAGE_SHIFT);
	if (obj == NULL)
		return (ENOMEM);

	seg->len = len;
	seg->object = obj;
	seg->sysmem = sysmem;
	return (0);
}

int
vm_get_memseg(struct vm *vm, int ident, size_t *len, bool *sysmem,
    vm_object_t *objptr)
{
	struct mem_seg *seg;

	sx_assert(&vm->mem_segs_lock, SX_LOCKED);

	if (ident < 0 || ident >= VM_MAX_MEMSEGS)
		return (EINVAL);

	seg = &vm->mem_segs[ident];
	if (len)
		*len = seg->len;
	if (sysmem)
		*sysmem = seg->sysmem;
	if (objptr)
		*objptr = seg->object;
	return (0);
}

void
vm_free_memseg(struct vm *vm, int ident)
{
	struct mem_seg *seg;

	KASSERT(ident >= 0 && ident < VM_MAX_MEMSEGS,
	    ("%s: invalid memseg ident %d", __func__, ident));

	seg = &vm->mem_segs[ident];
	if (seg->object != NULL) {
		vm_object_deallocate(seg->object);
		bzero(seg, sizeof(struct mem_seg));
	}
}

int
vm_mmap_memseg(struct vm *vm, vm_paddr_t gpa, int segid, vm_ooffset_t first,
    size_t len, int prot, int flags)
{
	struct mem_seg *seg;
	struct mem_map *m, *map;
	vm_ooffset_t last;
	int i, error;

	if (prot == 0 || (prot & ~(VM_PROT_ALL)) != 0)
		return (EINVAL);

	if (flags & ~VM_MEMMAP_F_WIRED)
		return (EINVAL);

	if (segid < 0 || segid >= VM_MAX_MEMSEGS)
		return (EINVAL);

	seg = &vm->mem_segs[segid];
	if (seg->object == NULL)
		return (EINVAL);

	last = first + len;
	if (first < 0 || first >= last || last > seg->len)
		return (EINVAL);

	if ((gpa | first | last) & PAGE_MASK)
		return (EINVAL);

	map = NULL;
	for (i = 0; i < VM_MAX_MEMMAPS; i++) {
		m = &vm->mem_maps[i];
		if (m->len == 0) {
			map = m;
			break;
		}
	}

	if (map == NULL)
		return (ENOSPC);

	error = vm_map_find(&vm->vmspace->vm_map, seg->object, first, &gpa,
	    len, 0, VMFS_NO_SPACE, prot, prot, 0);
	if (error != KERN_SUCCESS)
		return (EFAULT);

	vm_object_reference(seg->object);

	if (flags & VM_MEMMAP_F_WIRED) {
		error = vm_map_wire(&vm->vmspace->vm_map, gpa, gpa + len,
		    VM_MAP_WIRE_USER | VM_MAP_WIRE_NOHOLES);
		if (error != KERN_SUCCESS) {
			vm_map_remove(&vm->vmspace->vm_map, gpa, gpa + len);
			return (error == KERN_RESOURCE_SHORTAGE ? ENOMEM :
			    EFAULT);
		}
	}

	map->gpa = gpa;
	map->len = len;
	map->segoff = first;
	map->segid = segid;
	map->prot = prot;
	map->flags = flags;
	return (0);
}

int
vm_munmap_memseg(struct vm *vm, vm_paddr_t gpa, size_t len)
{
	struct mem_map *m;
	int i;

	for (i = 0; i < VM_MAX_MEMMAPS; i++) {
		m = &vm->mem_maps[i];
		if (m->gpa == gpa && m->len == len &&
		    (m->flags & VM_MEMMAP_F_IOMMU) == 0) {
			vm_free_memmap(vm, i);
			return (0);
		}
	}

	return (EINVAL);
}

int
vm_mmap_getnext(struct vm *vm, vm_paddr_t *gpa, int *segid,
    vm_ooffset_t *segoff, size_t *len, int *prot, int *flags)
{
	struct mem_map *mm, *mmnext;
	int i;

	mmnext = NULL;
	for (i = 0; i < VM_MAX_MEMMAPS; i++) {
		mm = &vm->mem_maps[i];
		if (mm->len == 0 || mm->gpa < *gpa)
			continue;
		if (mmnext == NULL || mm->gpa < mmnext->gpa)
			mmnext = mm;
	}

	if (mmnext != NULL) {
		*gpa = mmnext->gpa;
		if (segid)
			*segid = mmnext->segid;
		if (segoff)
			*segoff = mmnext->segoff;
		if (len)
			*len = mmnext->len;
		if (prot)
			*prot = mmnext->prot;
		if (flags)
			*flags = mmnext->flags;
		return (0);
	} else {
		return (ENOENT);
	}
}

static void
vm_free_memmap(struct vm *vm, int ident)
{
	struct mem_map *mm;
	int error __diagused;

	mm = &vm->mem_maps[ident];
	if (mm->len) {
		error = vm_map_remove(&vm->vmspace->vm_map, mm->gpa,
		    mm->gpa + mm->len);
		KASSERT(error == KERN_SUCCESS, ("%s: vm_map_remove error %d",
		    __func__, error));
		bzero(mm, sizeof(struct mem_map));
	}
}

static __inline bool
sysmem_mapping(struct vm *vm, struct mem_map *mm)
{

	if (mm->len != 0 && vm->mem_segs[mm->segid].sysmem)
		return (true);
	else
		return (false);
}

vm_paddr_t
vmm_sysmem_maxaddr(struct vm *vm)
{
	struct mem_map *mm;
	vm_paddr_t maxaddr;
	int i;

	maxaddr = 0;
	for (i = 0; i < VM_MAX_MEMMAPS; i++) {
		mm = &vm->mem_maps[i];
		if (sysmem_mapping(vm, mm)) {
			if (maxaddr < mm->gpa + mm->len)
				maxaddr = mm->gpa + mm->len;
		}
	}
	return (maxaddr);
}

static void
vm_iommu_map(struct vm *vm)
{
	vm_paddr_t gpa, hpa;
	struct mem_map *mm;
	int i;

	sx_assert(&vm->mem_segs_lock, SX_LOCKED);

	for (i = 0; i < VM_MAX_MEMMAPS; i++) {
		mm = &vm->mem_maps[i];
		if (!sysmem_mapping(vm, mm))
			continue;

		KASSERT((mm->flags & VM_MEMMAP_F_IOMMU) == 0,
		    ("iommu map found invalid memmap %#lx/%#lx/%#x",
		    mm->gpa, mm->len, mm->flags));
		if ((mm->flags & VM_MEMMAP_F_WIRED) == 0)
			continue;
		mm->flags |= VM_MEMMAP_F_IOMMU;

		for (gpa = mm->gpa; gpa < mm->gpa + mm->len; gpa += PAGE_SIZE) {
			hpa = pmap_extract(vmspace_pmap(vm->vmspace), gpa);

			/*
			 * All mappings in the vmm vmspace must be
			 * present since they are managed by vmm in this way.
			 * Because we are in pass-through mode, the
			 * mappings must also be wired.  This implies
			 * that all pages must be mapped and wired,
			 * allowing to use pmap_extract() and avoiding the
			 * need to use vm_gpa_hold_global().
			 *
			 * This could change if/when we start
			 * supporting page faults on IOMMU maps.
			 */
			KASSERT(vm_page_wired(PHYS_TO_VM_PAGE(hpa)),
			    ("vm_iommu_map: vm %p gpa %jx hpa %jx not wired",
			    vm, (uintmax_t)gpa, (uintmax_t)hpa));

			iommu_create_mapping(vm->iommu, gpa, hpa, PAGE_SIZE);
		}
	}

	iommu_invalidate_tlb(iommu_host_domain());
}

static void
vm_iommu_unmap(struct vm *vm)
{
	vm_paddr_t gpa;
	struct mem_map *mm;
	int i;

	sx_assert(&vm->mem_segs_lock, SX_LOCKED);

	for (i = 0; i < VM_MAX_MEMMAPS; i++) {
		mm = &vm->mem_maps[i];
		if (!sysmem_mapping(vm, mm))
			continue;

		if ((mm->flags & VM_MEMMAP_F_IOMMU) == 0)
			continue;
		mm->flags &= ~VM_MEMMAP_F_IOMMU;
		KASSERT((mm->flags & VM_MEMMAP_F_WIRED) != 0,
		    ("iommu unmap found invalid memmap %#lx/%#lx/%#x",
		    mm->gpa, mm->len, mm->flags));

		for (gpa = mm->gpa; gpa < mm->gpa + mm->len; gpa += PAGE_SIZE) {
			KASSERT(vm_page_wired(PHYS_TO_VM_PAGE(pmap_extract(
			    vmspace_pmap(vm->vmspace), gpa))),
			    ("vm_iommu_unmap: vm %p gpa %jx not wired",
			    vm, (uintmax_t)gpa));
			iommu_remove_mapping(vm->iommu, gpa, PAGE_SIZE);
		}
	}

	/*
	 * Invalidate the cached translations associated with the domain
	 * from which pages were removed.
	 */
	iommu_invalidate_tlb(vm->iommu);
}

int
vm_unassign_pptdev(struct vm *vm, int bus, int slot, int func)
{
	int error;

	error = ppt_unassign_device(vm, bus, slot, func);
	if (error)
		return (error);

	if (ppt_assigned_devices(vm) == 0)
		vm_iommu_unmap(vm);

	return (0);
}

int
vm_assign_pptdev(struct vm *vm, int bus, int slot, int func)
{
	int error;
	vm_paddr_t maxaddr;

	/* Set up the IOMMU to do the 'gpa' to 'hpa' translation */
	if (ppt_assigned_devices(vm) == 0) {
		KASSERT(vm->iommu == NULL,
		    ("vm_assign_pptdev: iommu must be NULL"));
		maxaddr = vmm_sysmem_maxaddr(vm);
		vm->iommu = iommu_create_domain(maxaddr);
		if (vm->iommu == NULL)
			return (ENXIO);
		vm_iommu_map(vm);
	}

	error = ppt_assign_device(vm, bus, slot, func);
	return (error);
}

static void *
_vm_gpa_hold(struct vm *vm, vm_paddr_t gpa, size_t len, int reqprot,
    void **cookie)
{
	int i, count, pageoff;
	struct mem_map *mm;
	vm_page_t m;

	pageoff = gpa & PAGE_MASK;
	if (len > PAGE_SIZE - pageoff)
		panic("vm_gpa_hold: invalid gpa/len: 0x%016lx/%lu", gpa, len);

	count = 0;
	for (i = 0; i < VM_MAX_MEMMAPS; i++) {
		mm = &vm->mem_maps[i];
		if (gpa >= mm->gpa && gpa < mm->gpa + mm->len) {
			count = vm_fault_quick_hold_pages(&vm->vmspace->vm_map,
			    trunc_page(gpa), PAGE_SIZE, reqprot, &m, 1);
			break;
		}
	}

	if (count == 1) {
		*cookie = m;
		return ((void *)(PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)) + pageoff));
	} else {
		*cookie = NULL;
		return (NULL);
	}
}

void *
vm_gpa_hold(struct vcpu *vcpu, vm_paddr_t gpa, size_t len, int reqprot,
    void **cookie)
{
#ifdef INVARIANTS
	/*
	 * The current vcpu should be frozen to ensure 'vm_memmap[]'
	 * stability.
	 */
	int state = vcpu_get_state(vcpu, NULL);
	KASSERT(state == VCPU_FROZEN, ("%s: invalid vcpu state %d",
	    __func__, state));
#endif
	return (_vm_gpa_hold(vcpu->vm, gpa, len, reqprot, cookie));
}

void *
vm_gpa_hold_global(struct vm *vm, vm_paddr_t gpa, size_t len, int reqprot,
    void **cookie)
{
	sx_assert(&vm->mem_segs_lock, SX_LOCKED);
	return (_vm_gpa_hold(vm, gpa, len, reqprot, cookie));
}

void
vm_gpa_release(void *cookie)
{
	vm_page_t m = cookie;

	vm_page_unwire(m, PQ_ACTIVE);
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
	if (error || reg != VM_REG_GUEST_RIP)
		return (error);

	/* Set 'nextrip' to match the value of %rip */
	VMM_CTR1(vcpu, "Setting nextrip to %#lx", val);
	vcpu->nextrip = val;
	return (0);
}

static bool
is_descriptor_table(int reg)
{

	switch (reg) {
	case VM_REG_GUEST_IDTR:
	case VM_REG_GUEST_GDTR:
		return (true);
	default:
		return (false);
	}
}

static bool
is_segment_register(int reg)
{

	switch (reg) {
	case VM_REG_GUEST_ES:
	case VM_REG_GUEST_CS:
	case VM_REG_GUEST_SS:
	case VM_REG_GUEST_DS:
	case VM_REG_GUEST_FS:
	case VM_REG_GUEST_GS:
	case VM_REG_GUEST_TR:
	case VM_REG_GUEST_LDTR:
		return (true);
	default:
		return (false);
	}
}

int
vm_get_seg_desc(struct vcpu *vcpu, int reg, struct seg_desc *desc)
{

	if (!is_segment_register(reg) && !is_descriptor_table(reg))
		return (EINVAL);

	return (vmmops_getdesc(vcpu->cookie, reg, desc));
}

int
vm_set_seg_desc(struct vcpu *vcpu, int reg, struct seg_desc *desc)
{

	if (!is_segment_register(reg) && !is_descriptor_table(reg))
		return (EINVAL);

	return (vmmops_setdesc(vcpu->cookie, reg, desc));
}

static void
restore_guest_fpustate(struct vcpu *vcpu)
{

	/* flush host state to the pcb */
	fpuexit(curthread);

	/* restore guest FPU state */
	fpu_enable();
	fpurestore(vcpu->guestfpu);

	/* restore guest XCR0 if XSAVE is enabled in the host */
	if (rcr4() & CR4_XSAVE)
		load_xcr(0, vcpu->guest_xcr0);

	/*
	 * The FPU is now "dirty" with the guest's state so disable
	 * the FPU to trap any access by the host.
	 */
	fpu_disable();
}

static void
save_guest_fpustate(struct vcpu *vcpu)
{

	if ((rcr0() & CR0_TS) == 0)
		panic("fpu emulation not enabled in host!");

	/* save guest XCR0 and restore host XCR0 */
	if (rcr4() & CR4_XSAVE) {
		vcpu->guest_xcr0 = rxcr(0);
		load_xcr(0, vmm_get_host_xcr0());
	}

	/* save guest FPU state */
	fpu_enable();
	fpusave(vcpu->guestfpu);
	fpu_disable();
}

static VMM_STAT(VCPU_IDLE_TICKS, "number of ticks vcpu was idle");

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
			vcpu->reqidle = 1;
			vcpu_notify_event_locked(vcpu, false);
			VMM_CTR1(vcpu, "vcpu state change from %s to "
			    "idle requested", vcpu_state2str(vcpu->state));
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

	VMM_CTR2(vcpu, "vcpu state changed from %s to %s",
	    vcpu_state2str(vcpu->state), vcpu_state2str(newstate));

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

static int
vm_handle_rendezvous(struct vcpu *vcpu)
{
	struct vm *vm = vcpu->vm;
	struct thread *td;
	int error, vcpuid;

	error = 0;
	vcpuid = vcpu->vcpuid;
	td = curthread;
	mtx_lock(&vm->rendezvous_mtx);
	while (vm->rendezvous_func != NULL) {
		/* 'rendezvous_req_cpus' must be a subset of 'active_cpus' */
		CPU_AND(&vm->rendezvous_req_cpus, &vm->rendezvous_req_cpus, &vm->active_cpus);

		if (CPU_ISSET(vcpuid, &vm->rendezvous_req_cpus) &&
		    !CPU_ISSET(vcpuid, &vm->rendezvous_done_cpus)) {
			VMM_CTR0(vcpu, "Calling rendezvous func");
			(*vm->rendezvous_func)(vcpu, vm->rendezvous_arg);
			CPU_SET(vcpuid, &vm->rendezvous_done_cpus);
		}
		if (CPU_CMP(&vm->rendezvous_req_cpus,
		    &vm->rendezvous_done_cpus) == 0) {
			VMM_CTR0(vcpu, "Rendezvous completed");
			CPU_ZERO(&vm->rendezvous_req_cpus);
			vm->rendezvous_func = NULL;
			wakeup(&vm->rendezvous_func);
			break;
		}
		VMM_CTR0(vcpu, "Wait for rendezvous completion");
		mtx_sleep(&vm->rendezvous_func, &vm->rendezvous_mtx, 0,
		    "vmrndv", hz);
		if (td_ast_pending(td, TDA_SUSPEND)) {
			mtx_unlock(&vm->rendezvous_mtx);
			error = thread_check_susp(td, true);
			if (error != 0)
				return (error);
			mtx_lock(&vm->rendezvous_mtx);
		}
	}
	mtx_unlock(&vm->rendezvous_mtx);
	return (0);
}

/*
 * Emulate a guest 'hlt' by sleeping until the vcpu is ready to run.
 */
static int
vm_handle_hlt(struct vcpu *vcpu, bool intr_disabled, bool *retu)
{
	struct vm *vm = vcpu->vm;
	const char *wmesg;
	struct thread *td;
	int error, t, vcpuid, vcpu_halted, vm_halted;

	vcpuid = vcpu->vcpuid;
	vcpu_halted = 0;
	vm_halted = 0;
	error = 0;
	td = curthread;

	KASSERT(!CPU_ISSET(vcpuid, &vm->halted_cpus), ("vcpu already halted"));

	vcpu_lock(vcpu);
	while (1) {
		/*
		 * Do a final check for pending NMI or interrupts before
		 * really putting this thread to sleep. Also check for
		 * software events that would cause this vcpu to wakeup.
		 *
		 * These interrupts/events could have happened after the
		 * vcpu returned from vmmops_run() and before it acquired the
		 * vcpu lock above.
		 */
		if (vm->rendezvous_func != NULL || vm->suspend || vcpu->reqidle)
			break;
		if (vm_nmi_pending(vcpu))
			break;
		if (!intr_disabled) {
			if (vm_extint_pending(vcpu) ||
			    vlapic_pending_intr(vcpu->vlapic, NULL)) {
				break;
			}
		}

		/* Don't go to sleep if the vcpu thread needs to yield */
		if (vcpu_should_yield(vcpu))
			break;

		if (vcpu_debugged(vcpu))
			break;

		/*
		 * Some Linux guests implement "halt" by having all vcpus
		 * execute HLT with interrupts disabled. 'halted_cpus' keeps
		 * track of the vcpus that have entered this state. When all
		 * vcpus enter the halted state the virtual machine is halted.
		 */
		if (intr_disabled) {
			wmesg = "vmhalt";
			VMM_CTR0(vcpu, "Halted");
			if (!vcpu_halted && halt_detection_enabled) {
				vcpu_halted = 1;
				CPU_SET_ATOMIC(vcpuid, &vm->halted_cpus);
			}
			if (CPU_CMP(&vm->halted_cpus, &vm->active_cpus) == 0) {
				vm_halted = 1;
				break;
			}
		} else {
			wmesg = "vmidle";
		}

		t = ticks;
		vcpu_require_state_locked(vcpu, VCPU_SLEEPING);
		/*
		 * XXX msleep_spin() cannot be interrupted by signals so
		 * wake up periodically to check pending signals.
		 */
		msleep_spin(vcpu, &vcpu->mtx, wmesg, hz);
		vcpu_require_state_locked(vcpu, VCPU_FROZEN);
		vmm_stat_incr(vcpu, VCPU_IDLE_TICKS, ticks - t);
		if (td_ast_pending(td, TDA_SUSPEND)) {
			vcpu_unlock(vcpu);
			error = thread_check_susp(td, false);
			if (error != 0) {
				if (vcpu_halted) {
					CPU_CLR_ATOMIC(vcpuid,
					    &vm->halted_cpus);
				}
				return (error);
			}
			vcpu_lock(vcpu);
		}
	}

	if (vcpu_halted)
		CPU_CLR_ATOMIC(vcpuid, &vm->halted_cpus);

	vcpu_unlock(vcpu);

	if (vm_halted)
		vm_suspend(vm, VM_SUSPEND_HALT);

	return (0);
}

static int
vm_handle_paging(struct vcpu *vcpu, bool *retu)
{
	struct vm *vm = vcpu->vm;
	int rv, ftype;
	struct vm_map *map;
	struct vm_exit *vme;

	vme = &vcpu->exitinfo;

	KASSERT(vme->inst_length == 0, ("%s: invalid inst_length %d",
	    __func__, vme->inst_length));

	ftype = vme->u.paging.fault_type;
	KASSERT(ftype == VM_PROT_READ ||
	    ftype == VM_PROT_WRITE || ftype == VM_PROT_EXECUTE,
	    ("vm_handle_paging: invalid fault_type %d", ftype));

	if (ftype == VM_PROT_READ || ftype == VM_PROT_WRITE) {
		rv = pmap_emulate_accessed_dirty(vmspace_pmap(vm->vmspace),
		    vme->u.paging.gpa, ftype);
		if (rv == 0) {
			VMM_CTR2(vcpu, "%s bit emulation for gpa %#lx",
			    ftype == VM_PROT_READ ? "accessed" : "dirty",
			    vme->u.paging.gpa);
			goto done;
		}
	}

	map = &vm->vmspace->vm_map;
	rv = vm_fault(map, vme->u.paging.gpa, ftype, VM_FAULT_NORMAL, NULL);

	VMM_CTR3(vcpu, "vm_handle_paging rv = %d, gpa = %#lx, "
	    "ftype = %d", rv, vme->u.paging.gpa, ftype);

	if (rv != KERN_SUCCESS)
		return (EFAULT);
done:
	return (0);
}

static int
vm_handle_inst_emul(struct vcpu *vcpu, bool *retu)
{
	struct vie *vie;
	struct vm_exit *vme;
	uint64_t gla, gpa, cs_base;
	struct vm_guest_paging *paging;
	mem_region_read_t mread;
	mem_region_write_t mwrite;
	enum vm_cpu_mode cpu_mode;
	int cs_d, error, fault;

	vme = &vcpu->exitinfo;

	KASSERT(vme->inst_length == 0, ("%s: invalid inst_length %d",
	    __func__, vme->inst_length));

	gla = vme->u.inst_emul.gla;
	gpa = vme->u.inst_emul.gpa;
	cs_base = vme->u.inst_emul.cs_base;
	cs_d = vme->u.inst_emul.cs_d;
	vie = &vme->u.inst_emul.vie;
	paging = &vme->u.inst_emul.paging;
	cpu_mode = paging->cpu_mode;

	VMM_CTR1(vcpu, "inst_emul fault accessing gpa %#lx", gpa);

	/* Fetch, decode and emulate the faulting instruction */
	if (vie->num_valid == 0) {
		error = vmm_fetch_instruction(vcpu, paging, vme->rip + cs_base,
		    VIE_INST_SIZE, vie, &fault);
	} else {
		/*
		 * The instruction bytes have already been copied into 'vie'
		 */
		error = fault = 0;
	}
	if (error || fault)
		return (error);

	if (vmm_decode_instruction(vcpu, gla, cpu_mode, cs_d, vie) != 0) {
		VMM_CTR1(vcpu, "Error decoding instruction at %#lx",
		    vme->rip + cs_base);
		*retu = true;	    /* dump instruction bytes in userspace */
		return (0);
	}

	/*
	 * Update 'nextrip' based on the length of the emulated instruction.
	 */
	vme->inst_length = vie->num_processed;
	vcpu->nextrip += vie->num_processed;
	VMM_CTR1(vcpu, "nextrip updated to %#lx after instruction decoding",
	    vcpu->nextrip);

	/* return to userland unless this is an in-kernel emulated device */
	if (gpa >= DEFAULT_APIC_BASE && gpa < DEFAULT_APIC_BASE + PAGE_SIZE) {
		mread = lapic_mmio_read;
		mwrite = lapic_mmio_write;
	} else if (gpa >= VIOAPIC_BASE && gpa < VIOAPIC_BASE + VIOAPIC_SIZE) {
		mread = vioapic_mmio_read;
		mwrite = vioapic_mmio_write;
	} else if (gpa >= VHPET_BASE && gpa < VHPET_BASE + VHPET_SIZE) {
		mread = vhpet_mmio_read;
		mwrite = vhpet_mmio_write;
	} else {
		*retu = true;
		return (0);
	}

	error = vmm_emulate_instruction(vcpu, gpa, vie, paging, mread, mwrite,
	    retu);

	return (error);
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
		if (CPU_CMP(&vm->suspended_cpus, &vm->active_cpus) == 0) {
			VMM_CTR0(vcpu, "All vcpus suspended");
			break;
		}

		if (vm->rendezvous_func == NULL) {
			VMM_CTR0(vcpu, "Sleeping during suspend");
			vcpu_require_state_locked(vcpu, VCPU_SLEEPING);
			msleep_spin(vcpu, &vcpu->mtx, "vmsusp", hz);
			vcpu_require_state_locked(vcpu, VCPU_FROZEN);
			if (td_ast_pending(td, TDA_SUSPEND)) {
				vcpu_unlock(vcpu);
				error = thread_check_susp(td, false);
				vcpu_lock(vcpu);
			}
		} else {
			VMM_CTR0(vcpu, "Rendezvous during suspend");
			vcpu_unlock(vcpu);
			error = vm_handle_rendezvous(vcpu);
			vcpu_lock(vcpu);
		}
	}
	vcpu_unlock(vcpu);

	/*
	 * Wakeup the other sleeping vcpus and return to userspace.
	 */
	for (i = 0; i < vm->maxcpus; i++) {
		if (CPU_ISSET(i, &vm->suspended_cpus)) {
			vcpu_notify_event(vm_vcpu(vm, i), false);
		}
	}

	*retu = true;
	return (error);
}

static int
vm_handle_reqidle(struct vcpu *vcpu, bool *retu)
{
	vcpu_lock(vcpu);
	KASSERT(vcpu->reqidle, ("invalid vcpu reqidle %d", vcpu->reqidle));
	vcpu->reqidle = 0;
	vcpu_unlock(vcpu);
	*retu = true;
	return (0);
}

static int
vm_handle_db(struct vcpu *vcpu, struct vm_exit *vme, bool *retu)
{
	int error, fault;
	uint64_t rsp;
	uint64_t rflags;
	struct vm_copyinfo copyinfo[2];

	*retu = true;
	if (!vme->u.dbg.pushf_intercept || vme->u.dbg.tf_shadow_val != 0) {
		return (0);
	}

	vm_get_register(vcpu, VM_REG_GUEST_RSP, &rsp);
	error = vm_copy_setup(vcpu, &vme->u.dbg.paging, rsp, sizeof(uint64_t),
	    VM_PROT_RW, copyinfo, nitems(copyinfo), &fault);
	if (error != 0 || fault != 0) {
		*retu = false;
		return (EINVAL);
	}

	/* Read pushed rflags value from top of stack. */
	vm_copyin(copyinfo, &rflags, sizeof(uint64_t));

	/* Clear TF bit. */
	rflags &= ~(PSL_T);

	/* Write updated value back to memory. */
	vm_copyout(&rflags, copyinfo, sizeof(uint64_t));
	vm_copy_teardown(copyinfo, nitems(copyinfo));

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
			vcpu_notify_event(vm_vcpu(vm, i), false);
	}

	return (0);
}

void
vm_exit_suspended(struct vcpu *vcpu, uint64_t rip)
{
	struct vm *vm = vcpu->vm;
	struct vm_exit *vmexit;

	KASSERT(vm->suspend > VM_SUSPEND_NONE && vm->suspend < VM_SUSPEND_LAST,
	    ("vm_exit_suspended: invalid suspend type %d", vm->suspend));

	vmexit = vm_exitinfo(vcpu);
	vmexit->rip = rip;
	vmexit->inst_length = 0;
	vmexit->exitcode = VM_EXITCODE_SUSPENDED;
	vmexit->u.suspended.how = vm->suspend;
}

void
vm_exit_debug(struct vcpu *vcpu, uint64_t rip)
{
	struct vm_exit *vmexit;

	vmexit = vm_exitinfo(vcpu);
	vmexit->rip = rip;
	vmexit->inst_length = 0;
	vmexit->exitcode = VM_EXITCODE_DEBUG;
}

void
vm_exit_rendezvous(struct vcpu *vcpu, uint64_t rip)
{
	struct vm_exit *vmexit;

	vmexit = vm_exitinfo(vcpu);
	vmexit->rip = rip;
	vmexit->inst_length = 0;
	vmexit->exitcode = VM_EXITCODE_RENDEZVOUS;
	vmm_stat_incr(vcpu, VMEXIT_RENDEZVOUS, 1);
}

void
vm_exit_reqidle(struct vcpu *vcpu, uint64_t rip)
{
	struct vm_exit *vmexit;

	vmexit = vm_exitinfo(vcpu);
	vmexit->rip = rip;
	vmexit->inst_length = 0;
	vmexit->exitcode = VM_EXITCODE_REQIDLE;
	vmm_stat_incr(vcpu, VMEXIT_REQIDLE, 1);
}

void
vm_exit_astpending(struct vcpu *vcpu, uint64_t rip)
{
	struct vm_exit *vmexit;

	vmexit = vm_exitinfo(vcpu);
	vmexit->rip = rip;
	vmexit->inst_length = 0;
	vmexit->exitcode = VM_EXITCODE_BOGUS;
	vmm_stat_incr(vcpu, VMEXIT_ASTPENDING, 1);
}

int
vm_run(struct vcpu *vcpu)
{
	struct vm *vm = vcpu->vm;
	struct vm_eventinfo evinfo;
	int error, vcpuid;
	struct pcb *pcb;
	uint64_t tscval;
	struct vm_exit *vme;
	bool retu, intr_disabled;
	pmap_t pmap;

	vcpuid = vcpu->vcpuid;

	if (!CPU_ISSET(vcpuid, &vm->active_cpus))
		return (EINVAL);

	if (CPU_ISSET(vcpuid, &vm->suspended_cpus))
		return (EINVAL);

	pmap = vmspace_pmap(vm->vmspace);
	vme = &vcpu->exitinfo;
	evinfo.rptr = &vm->rendezvous_req_cpus;
	evinfo.sptr = &vm->suspend;
	evinfo.iptr = &vcpu->reqidle;
restart:
	critical_enter();

	KASSERT(!CPU_ISSET(curcpu, &pmap->pm_active),
	    ("vm_run: absurd pm_active"));

	tscval = rdtsc();

	pcb = PCPU_GET(curpcb);
	set_pcb_flags(pcb, PCB_FULL_IRET);

	restore_guest_fpustate(vcpu);

	vcpu_require_state(vcpu, VCPU_RUNNING);
	error = vmmops_run(vcpu->cookie, vcpu->nextrip, pmap, &evinfo);
	vcpu_require_state(vcpu, VCPU_FROZEN);

	save_guest_fpustate(vcpu);

	vmm_stat_incr(vcpu, VCPU_TOTAL_RUNTIME, rdtsc() - tscval);

	critical_exit();

	if (error == 0) {
		retu = false;
		vcpu->nextrip = vme->rip + vme->inst_length;
		switch (vme->exitcode) {
		case VM_EXITCODE_REQIDLE:
			error = vm_handle_reqidle(vcpu, &retu);
			break;
		case VM_EXITCODE_SUSPENDED:
			error = vm_handle_suspend(vcpu, &retu);
			break;
		case VM_EXITCODE_IOAPIC_EOI:
			vioapic_process_eoi(vm, vme->u.ioapic_eoi.vector);
			break;
		case VM_EXITCODE_RENDEZVOUS:
			error = vm_handle_rendezvous(vcpu);
			break;
		case VM_EXITCODE_HLT:
			intr_disabled = ((vme->u.hlt.rflags & PSL_I) == 0);
			error = vm_handle_hlt(vcpu, intr_disabled, &retu);
			break;
		case VM_EXITCODE_PAGING:
			error = vm_handle_paging(vcpu, &retu);
			break;
		case VM_EXITCODE_INST_EMUL:
			error = vm_handle_inst_emul(vcpu, &retu);
			break;
		case VM_EXITCODE_INOUT:
		case VM_EXITCODE_INOUT_STR:
			error = vm_handle_inout(vcpu, vme, &retu);
			break;
		case VM_EXITCODE_DB:
			error = vm_handle_db(vcpu, vme, &retu);
			break;
		case VM_EXITCODE_MONITOR:
		case VM_EXITCODE_MWAIT:
		case VM_EXITCODE_VMINSN:
			vm_inject_ud(vcpu);
			break;
		default:
			retu = true;	/* handled in userland */
			break;
		}
	}

	/*
	 * VM_EXITCODE_INST_EMUL could access the apic which could transform the
	 * exit code into VM_EXITCODE_IPI.
	 */
	if (error == 0 && vme->exitcode == VM_EXITCODE_IPI)
		error = vm_handle_ipi(vcpu, vme, &retu);

	if (error == 0 && retu == false)
		goto restart;

	vmm_stat_incr(vcpu, VMEXIT_USERSPACE, 1);
	VMM_CTR2(vcpu, "retu %d/%d", error, vme->exitcode);

	return (error);
}

int
vm_restart_instruction(struct vcpu *vcpu)
{
	enum vcpu_state state;
	uint64_t rip;
	int error __diagused;

	state = vcpu_get_state(vcpu, NULL);
	if (state == VCPU_RUNNING) {
		/*
		 * When a vcpu is "running" the next instruction is determined
		 * by adding 'rip' and 'inst_length' in the vcpu's 'exitinfo'.
		 * Thus setting 'inst_length' to zero will cause the current
		 * instruction to be restarted.
		 */
		vcpu->exitinfo.inst_length = 0;
		VMM_CTR1(vcpu, "restarting instruction at %#lx by "
		    "setting inst_length to zero", vcpu->exitinfo.rip);
	} else if (state == VCPU_FROZEN) {
		/*
		 * When a vcpu is "frozen" it is outside the critical section
		 * around vmmops_run() and 'nextrip' points to the next
		 * instruction. Thus instruction restart is achieved by setting
		 * 'nextrip' to the vcpu's %rip.
		 */
		error = vm_get_register(vcpu, VM_REG_GUEST_RIP, &rip);
		KASSERT(!error, ("%s: error %d getting rip", __func__, error));
		VMM_CTR2(vcpu, "restarting instruction by updating "
		    "nextrip from %#lx to %#lx", vcpu->nextrip, rip);
		vcpu->nextrip = rip;
	} else {
		panic("%s: invalid state %d", __func__, state);
	}
	return (0);
}

int
vm_exit_intinfo(struct vcpu *vcpu, uint64_t info)
{
	int type, vector;

	if (info & VM_INTINFO_VALID) {
		type = info & VM_INTINFO_TYPE;
		vector = info & 0xff;
		if (type == VM_INTINFO_NMI && vector != IDT_NMI)
			return (EINVAL);
		if (type == VM_INTINFO_HWEXCEPTION && vector >= 32)
			return (EINVAL);
		if (info & VM_INTINFO_RSVD)
			return (EINVAL);
	} else {
		info = 0;
	}
	VMM_CTR2(vcpu, "%s: info1(%#lx)", __func__, info);
	vcpu->exitintinfo = info;
	return (0);
}

enum exc_class {
	EXC_BENIGN,
	EXC_CONTRIBUTORY,
	EXC_PAGEFAULT
};

#define	IDT_VE	20	/* Virtualization Exception (Intel specific) */

static enum exc_class
exception_class(uint64_t info)
{
	int type, vector;

	KASSERT(info & VM_INTINFO_VALID, ("intinfo must be valid: %#lx", info));
	type = info & VM_INTINFO_TYPE;
	vector = info & 0xff;

	/* Table 6-4, "Interrupt and Exception Classes", Intel SDM, Vol 3 */
	switch (type) {
	case VM_INTINFO_HWINTR:
	case VM_INTINFO_SWINTR:
	case VM_INTINFO_NMI:
		return (EXC_BENIGN);
	default:
		/*
		 * Hardware exception.
		 *
		 * SVM and VT-x use identical type values to represent NMI,
		 * hardware interrupt and software interrupt.
		 *
		 * SVM uses type '3' for all exceptions. VT-x uses type '3'
		 * for exceptions except #BP and #OF. #BP and #OF use a type
		 * value of '5' or '6'. Therefore we don't check for explicit
		 * values of 'type' to classify 'intinfo' into a hardware
		 * exception.
		 */
		break;
	}

	switch (vector) {
	case IDT_PF:
	case IDT_VE:
		return (EXC_PAGEFAULT);
	case IDT_DE:
	case IDT_TS:
	case IDT_NP:
	case IDT_SS:
	case IDT_GP:
		return (EXC_CONTRIBUTORY);
	default:
		return (EXC_BENIGN);
	}
}

static int
nested_fault(struct vcpu *vcpu, uint64_t info1, uint64_t info2,
    uint64_t *retinfo)
{
	enum exc_class exc1, exc2;
	int type1, vector1;

	KASSERT(info1 & VM_INTINFO_VALID, ("info1 %#lx is not valid", info1));
	KASSERT(info2 & VM_INTINFO_VALID, ("info2 %#lx is not valid", info2));

	/*
	 * If an exception occurs while attempting to call the double-fault
	 * handler the processor enters shutdown mode (aka triple fault).
	 */
	type1 = info1 & VM_INTINFO_TYPE;
	vector1 = info1 & 0xff;
	if (type1 == VM_INTINFO_HWEXCEPTION && vector1 == IDT_DF) {
		VMM_CTR2(vcpu, "triple fault: info1(%#lx), info2(%#lx)",
		    info1, info2);
		vm_suspend(vcpu->vm, VM_SUSPEND_TRIPLEFAULT);
		*retinfo = 0;
		return (0);
	}

	/*
	 * Table 6-5 "Conditions for Generating a Double Fault", Intel SDM, Vol3
	 */
	exc1 = exception_class(info1);
	exc2 = exception_class(info2);
	if ((exc1 == EXC_CONTRIBUTORY && exc2 == EXC_CONTRIBUTORY) ||
	    (exc1 == EXC_PAGEFAULT && exc2 != EXC_BENIGN)) {
		/* Convert nested fault into a double fault. */
		*retinfo = IDT_DF;
		*retinfo |= VM_INTINFO_VALID | VM_INTINFO_HWEXCEPTION;
		*retinfo |= VM_INTINFO_DEL_ERRCODE;
	} else {
		/* Handle exceptions serially */
		*retinfo = info2;
	}
	return (1);
}

static uint64_t
vcpu_exception_intinfo(struct vcpu *vcpu)
{
	uint64_t info = 0;

	if (vcpu->exception_pending) {
		info = vcpu->exc_vector & 0xff;
		info |= VM_INTINFO_VALID | VM_INTINFO_HWEXCEPTION;
		if (vcpu->exc_errcode_valid) {
			info |= VM_INTINFO_DEL_ERRCODE;
			info |= (uint64_t)vcpu->exc_errcode << 32;
		}
	}
	return (info);
}

int
vm_entry_intinfo(struct vcpu *vcpu, uint64_t *retinfo)
{
	uint64_t info1, info2;
	int valid;

	info1 = vcpu->exitintinfo;
	vcpu->exitintinfo = 0;

	info2 = 0;
	if (vcpu->exception_pending) {
		info2 = vcpu_exception_intinfo(vcpu);
		vcpu->exception_pending = 0;
		VMM_CTR2(vcpu, "Exception %d delivered: %#lx",
		    vcpu->exc_vector, info2);
	}

	if ((info1 & VM_INTINFO_VALID) && (info2 & VM_INTINFO_VALID)) {
		valid = nested_fault(vcpu, info1, info2, retinfo);
	} else if (info1 & VM_INTINFO_VALID) {
		*retinfo = info1;
		valid = 1;
	} else if (info2 & VM_INTINFO_VALID) {
		*retinfo = info2;
		valid = 1;
	} else {
		valid = 0;
	}

	if (valid) {
		VMM_CTR4(vcpu, "%s: info1(%#lx), info2(%#lx), "
		    "retinfo(%#lx)", __func__, info1, info2, *retinfo);
	}

	return (valid);
}

int
vm_get_intinfo(struct vcpu *vcpu, uint64_t *info1, uint64_t *info2)
{
	*info1 = vcpu->exitintinfo;
	*info2 = vcpu_exception_intinfo(vcpu);
	return (0);
}

int
vm_inject_exception(struct vcpu *vcpu, int vector, int errcode_valid,
    uint32_t errcode, int restart_instruction)
{
	uint64_t regval;
	int error __diagused;

	if (vector < 0 || vector >= 32)
		return (EINVAL);

	/*
	 * A double fault exception should never be injected directly into
	 * the guest. It is a derived exception that results from specific
	 * combinations of nested faults.
	 */
	if (vector == IDT_DF)
		return (EINVAL);

	if (vcpu->exception_pending) {
		VMM_CTR2(vcpu, "Unable to inject exception %d due to "
		    "pending exception %d", vector, vcpu->exc_vector);
		return (EBUSY);
	}

	if (errcode_valid) {
		/*
		 * Exceptions don't deliver an error code in real mode.
		 */
		error = vm_get_register(vcpu, VM_REG_GUEST_CR0, &regval);
		KASSERT(!error, ("%s: error %d getting CR0", __func__, error));
		if (!(regval & CR0_PE))
			errcode_valid = 0;
	}

	/*
	 * From section 26.6.1 "Interruptibility State" in Intel SDM:
	 *
	 * Event blocking by "STI" or "MOV SS" is cleared after guest executes
	 * one instruction or incurs an exception.
	 */
	error = vm_set_register(vcpu, VM_REG_GUEST_INTR_SHADOW, 0);
	KASSERT(error == 0, ("%s: error %d clearing interrupt shadow",
	    __func__, error));

	if (restart_instruction)
		vm_restart_instruction(vcpu);

	vcpu->exception_pending = 1;
	vcpu->exc_vector = vector;
	vcpu->exc_errcode = errcode;
	vcpu->exc_errcode_valid = errcode_valid;
	VMM_CTR1(vcpu, "Exception %d pending", vector);
	return (0);
}

void
vm_inject_fault(struct vcpu *vcpu, int vector, int errcode_valid, int errcode)
{
	int error __diagused, restart_instruction;

	restart_instruction = 1;

	error = vm_inject_exception(vcpu, vector, errcode_valid,
	    errcode, restart_instruction);
	KASSERT(error == 0, ("vm_inject_exception error %d", error));
}

void
vm_inject_pf(struct vcpu *vcpu, int error_code, uint64_t cr2)
{
	int error __diagused;

	VMM_CTR2(vcpu, "Injecting page fault: error_code %#x, cr2 %#lx",
	    error_code, cr2);

	error = vm_set_register(vcpu, VM_REG_GUEST_CR2, cr2);
	KASSERT(error == 0, ("vm_set_register(cr2) error %d", error));

	vm_inject_fault(vcpu, IDT_PF, 1, error_code);
}

static VMM_STAT(VCPU_NMI_COUNT, "number of NMIs delivered to vcpu");

int
vm_inject_nmi(struct vcpu *vcpu)
{

	vcpu->nmi_pending = 1;
	vcpu_notify_event(vcpu, false);
	return (0);
}

int
vm_nmi_pending(struct vcpu *vcpu)
{
	return (vcpu->nmi_pending);
}

void
vm_nmi_clear(struct vcpu *vcpu)
{
	if (vcpu->nmi_pending == 0)
		panic("vm_nmi_clear: inconsistent nmi_pending state");

	vcpu->nmi_pending = 0;
	vmm_stat_incr(vcpu, VCPU_NMI_COUNT, 1);
}

static VMM_STAT(VCPU_EXTINT_COUNT, "number of ExtINTs delivered to vcpu");

int
vm_inject_extint(struct vcpu *vcpu)
{

	vcpu->extint_pending = 1;
	vcpu_notify_event(vcpu, false);
	return (0);
}

int
vm_extint_pending(struct vcpu *vcpu)
{
	return (vcpu->extint_pending);
}

void
vm_extint_clear(struct vcpu *vcpu)
{
	if (vcpu->extint_pending == 0)
		panic("vm_extint_clear: inconsistent extint_pending state");

	vcpu->extint_pending = 0;
	vmm_stat_incr(vcpu, VCPU_EXTINT_COUNT, 1);
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

struct vcpu *
vm_vcpu(struct vm *vm, int vcpuid)
{
	return (vm->vcpu[vcpuid]);
}

struct vlapic *
vm_lapic(struct vcpu *vcpu)
{
	return (vcpu->vlapic);
}

struct vioapic *
vm_ioapic(struct vm *vm)
{

	return (vm->vioapic);
}

struct vhpet *
vm_hpet(struct vm *vm)
{

	return (vm->vhpet);
}

bool
vmm_is_pptdev(int bus, int slot, int func)
{
	int b, f, i, n, s;
	char *val, *cp, *cp2;
	bool found;

	/*
	 * XXX
	 * The length of an environment variable is limited to 128 bytes which
	 * puts an upper limit on the number of passthru devices that may be
	 * specified using a single environment variable.
	 *
	 * Work around this by scanning multiple environment variable
	 * names instead of a single one - yuck!
	 */
	const char *names[] = { "pptdevs", "pptdevs2", "pptdevs3", NULL };

	/* set pptdevs="1/2/3 4/5/6 7/8/9 10/11/12" */
	found = false;
	for (i = 0; names[i] != NULL && !found; i++) {
		cp = val = kern_getenv(names[i]);
		while (cp != NULL && *cp != '\0') {
			if ((cp2 = strchr(cp, ' ')) != NULL)
				*cp2 = '\0';

			n = sscanf(cp, "%d/%d/%d", &b, &s, &f);
			if (n == 3 && bus == b && slot == s && func == f) {
				found = true;
				break;
			}

			if (cp2 != NULL)
				*cp2++ = ' ';

			cp = cp2;
		}
		freeenv(val);
	}
	return (found);
}

void *
vm_iommu_domain(struct vm *vm)
{

	return (vm->iommu);
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
vm_activate_cpu(struct vcpu *vcpu)
{
	struct vm *vm = vcpu->vm;

	if (CPU_ISSET(vcpu->vcpuid, &vm->active_cpus))
		return (EBUSY);

	VMM_CTR0(vcpu, "activated");
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
				vcpu_notify_event(vm_vcpu(vm, i), false);
		}
	} else {
		if (!CPU_ISSET(vcpu->vcpuid, &vm->active_cpus))
			return (EINVAL);

		CPU_SET_ATOMIC(vcpu->vcpuid, &vm->debug_cpus);
		vcpu_notify_event(vcpu, false);
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

/*
 * Returns the subset of vCPUs in tostart that are awaiting startup.
 * These vCPUs are also marked as no longer awaiting startup.
 */
cpuset_t
vm_start_cpus(struct vm *vm, const cpuset_t *tostart)
{
	cpuset_t set;

	mtx_lock(&vm->rendezvous_mtx);
	CPU_AND(&set, &vm->startup_cpus, tostart);
	CPU_ANDNOT(&vm->startup_cpus, &vm->startup_cpus, &set);
	mtx_unlock(&vm->rendezvous_mtx);
	return (set);
}

void
vm_await_start(struct vm *vm, const cpuset_t *waiting)
{
	mtx_lock(&vm->rendezvous_mtx);
	CPU_OR(&vm->startup_cpus, &vm->startup_cpus, waiting);
	mtx_unlock(&vm->rendezvous_mtx);
}

void *
vcpu_stats(struct vcpu *vcpu)
{

	return (vcpu->stats);
}

int
vm_get_x2apic_state(struct vcpu *vcpu, enum x2apic_state *state)
{
	*state = vcpu->x2apic_state;

	return (0);
}

int
vm_set_x2apic_state(struct vcpu *vcpu, enum x2apic_state state)
{
	if (state >= X2APIC_STATE_LAST)
		return (EINVAL);

	vcpu->x2apic_state = state;

	vlapic_set_x2apic_state(vcpu, state);

	return (0);
}

/*
 * This function is called to ensure that a vcpu "sees" a pending event
 * as soon as possible:
 * - If the vcpu thread is sleeping then it is woken up.
 * - If the vcpu is running on a different host_cpu then an IPI will be directed
 *   to the host_cpu to cause the vcpu to trap into the hypervisor.
 */
static void
vcpu_notify_event_locked(struct vcpu *vcpu, bool lapic_intr)
{
	int hostcpu;

	hostcpu = vcpu->hostcpu;
	if (vcpu->state == VCPU_RUNNING) {
		KASSERT(hostcpu != NOCPU, ("vcpu running on invalid hostcpu"));
		if (hostcpu != curcpu) {
			if (lapic_intr) {
				vlapic_post_intr(vcpu->vlapic, hostcpu,
				    vmm_ipinum);
			} else {
				ipi_cpu(hostcpu, vmm_ipinum);
			}
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
vcpu_notify_event(struct vcpu *vcpu, bool lapic_intr)
{
	vcpu_lock(vcpu);
	vcpu_notify_event_locked(vcpu, lapic_intr);
	vcpu_unlock(vcpu);
}

struct vmspace *
vm_get_vmspace(struct vm *vm)
{

	return (vm->vmspace);
}

int
vm_apicid2vcpuid(struct vm *vm, int apicid)
{
	/*
	 * XXX apic id is assumed to be numerically identical to vcpu id
	 */
	return (apicid);
}

int
vm_smp_rendezvous(struct vcpu *vcpu, cpuset_t dest,
    vm_rendezvous_func_t func, void *arg)
{
	struct vm *vm = vcpu->vm;
	int error, i;

	/*
	 * Enforce that this function is called without any locks
	 */
	WITNESS_WARN(WARN_PANIC, NULL, "vm_smp_rendezvous");

restart:
	mtx_lock(&vm->rendezvous_mtx);
	if (vm->rendezvous_func != NULL) {
		/*
		 * If a rendezvous is already in progress then we need to
		 * call the rendezvous handler in case this 'vcpu' is one
		 * of the targets of the rendezvous.
		 */
		VMM_CTR0(vcpu, "Rendezvous already in progress");
		mtx_unlock(&vm->rendezvous_mtx);
		error = vm_handle_rendezvous(vcpu);
		if (error != 0)
			return (error);
		goto restart;
	}
	KASSERT(vm->rendezvous_func == NULL, ("vm_smp_rendezvous: previous "
	    "rendezvous is still in progress"));

	VMM_CTR0(vcpu, "Initiating rendezvous");
	vm->rendezvous_req_cpus = dest;
	CPU_ZERO(&vm->rendezvous_done_cpus);
	vm->rendezvous_arg = arg;
	vm->rendezvous_func = func;
	mtx_unlock(&vm->rendezvous_mtx);

	/*
	 * Wake up any sleeping vcpus and trigger a VM-exit in any running
	 * vcpus so they handle the rendezvous as soon as possible.
	 */
	for (i = 0; i < vm->maxcpus; i++) {
		if (CPU_ISSET(i, &dest))
			vcpu_notify_event(vm_vcpu(vm, i), false);
	}

	return (vm_handle_rendezvous(vcpu));
}

struct vatpic *
vm_atpic(struct vm *vm)
{
	return (vm->vatpic);
}

struct vatpit *
vm_atpit(struct vm *vm)
{
	return (vm->vatpit);
}

struct vpmtmr *
vm_pmtmr(struct vm *vm)
{

	return (vm->vpmtmr);
}

struct vrtc *
vm_rtc(struct vm *vm)
{

	return (vm->vrtc);
}

enum vm_reg_name
vm_segment_name(int seg)
{
	static enum vm_reg_name seg_names[] = {
		VM_REG_GUEST_ES,
		VM_REG_GUEST_CS,
		VM_REG_GUEST_SS,
		VM_REG_GUEST_DS,
		VM_REG_GUEST_FS,
		VM_REG_GUEST_GS
	};

	KASSERT(seg >= 0 && seg < nitems(seg_names),
	    ("%s: invalid segment encoding %d", __func__, seg));
	return (seg_names[seg]);
}

void
vm_copy_teardown(struct vm_copyinfo *copyinfo, int num_copyinfo)
{
	int idx;

	for (idx = 0; idx < num_copyinfo; idx++) {
		if (copyinfo[idx].cookie != NULL)
			vm_gpa_release(copyinfo[idx].cookie);
	}
	bzero(copyinfo, num_copyinfo * sizeof(struct vm_copyinfo));
}

int
vm_copy_setup(struct vcpu *vcpu, struct vm_guest_paging *paging,
    uint64_t gla, size_t len, int prot, struct vm_copyinfo *copyinfo,
    int num_copyinfo, int *fault)
{
	int error, idx, nused;
	size_t n, off, remaining;
	void *hva, *cookie;
	uint64_t gpa;

	bzero(copyinfo, sizeof(struct vm_copyinfo) * num_copyinfo);

	nused = 0;
	remaining = len;
	while (remaining > 0) {
		if (nused >= num_copyinfo)
			return (EFAULT);
		error = vm_gla2gpa(vcpu, paging, gla, prot, &gpa, fault);
		if (error || *fault)
			return (error);
		off = gpa & PAGE_MASK;
		n = min(remaining, PAGE_SIZE - off);
		copyinfo[nused].gpa = gpa;
		copyinfo[nused].len = n;
		remaining -= n;
		gla += n;
		nused++;
	}

	for (idx = 0; idx < nused; idx++) {
		hva = vm_gpa_hold(vcpu, copyinfo[idx].gpa,
		    copyinfo[idx].len, prot, &cookie);
		if (hva == NULL)
			break;
		copyinfo[idx].hva = hva;
		copyinfo[idx].cookie = cookie;
	}

	if (idx != nused) {
		vm_copy_teardown(copyinfo, num_copyinfo);
		return (EFAULT);
	} else {
		*fault = 0;
		return (0);
	}
}

void
vm_copyin(struct vm_copyinfo *copyinfo, void *kaddr, size_t len)
{
	char *dst;
	int idx;

	dst = kaddr;
	idx = 0;
	while (len > 0) {
		bcopy(copyinfo[idx].hva, dst, copyinfo[idx].len);
		len -= copyinfo[idx].len;
		dst += copyinfo[idx].len;
		idx++;
	}
}

void
vm_copyout(const void *kaddr, struct vm_copyinfo *copyinfo, size_t len)
{
	const char *src;
	int idx;

	src = kaddr;
	idx = 0;
	while (len > 0) {
		bcopy(src, copyinfo[idx].hva, copyinfo[idx].len);
		len -= copyinfo[idx].len;
		src += copyinfo[idx].len;
		idx++;
	}
}

/*
 * Return the amount of in-use and wired memory for the VM. Since
 * these are global stats, only return the values with for vCPU 0
 */
VMM_STAT_DECLARE(VMM_MEM_RESIDENT);
VMM_STAT_DECLARE(VMM_MEM_WIRED);

static void
vm_get_rescnt(struct vcpu *vcpu, struct vmm_stat_type *stat)
{

	if (vcpu->vcpuid == 0) {
		vmm_stat_set(vcpu, VMM_MEM_RESIDENT, PAGE_SIZE *
		    vmspace_resident_count(vcpu->vm->vmspace));
	}
}

static void
vm_get_wiredcnt(struct vcpu *vcpu, struct vmm_stat_type *stat)
{

	if (vcpu->vcpuid == 0) {
		vmm_stat_set(vcpu, VMM_MEM_WIRED, PAGE_SIZE *
		    pmap_wired_count(vmspace_pmap(vcpu->vm->vmspace)));
	}
}

VMM_STAT_FUNC(VMM_MEM_RESIDENT, "Resident memory", vm_get_rescnt);
VMM_STAT_FUNC(VMM_MEM_WIRED, "Wired memory", vm_get_wiredcnt);

#ifdef BHYVE_SNAPSHOT
static int
vm_snapshot_vcpus(struct vm *vm, struct vm_snapshot_meta *meta)
{
	uint64_t tsc, now;
	int ret;
	struct vcpu *vcpu;
	uint16_t i, maxcpus;

	now = rdtsc();
	maxcpus = vm_get_maxcpus(vm);
	for (i = 0; i < maxcpus; i++) {
		vcpu = vm->vcpu[i];
		if (vcpu == NULL)
			continue;

		SNAPSHOT_VAR_OR_LEAVE(vcpu->x2apic_state, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(vcpu->exitintinfo, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(vcpu->exc_vector, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(vcpu->exc_errcode_valid, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(vcpu->exc_errcode, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(vcpu->guest_xcr0, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(vcpu->exitinfo, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(vcpu->nextrip, meta, ret, done);

		/*
		 * Save the absolute TSC value by adding now to tsc_offset.
		 *
		 * It will be turned turned back into an actual offset when the
		 * TSC restore function is called
		 */
		tsc = now + vcpu->tsc_offset;
		SNAPSHOT_VAR_OR_LEAVE(tsc, meta, ret, done);
		if (meta->op == VM_SNAPSHOT_RESTORE)
			vcpu->tsc_offset = tsc;
	}

done:
	return (ret);
}

static int
vm_snapshot_vm(struct vm *vm, struct vm_snapshot_meta *meta)
{
	int ret;

	ret = vm_snapshot_vcpus(vm, meta);
	if (ret != 0)
		goto done;

	SNAPSHOT_VAR_OR_LEAVE(vm->startup_cpus, meta, ret, done);
done:
	return (ret);
}

static int
vm_snapshot_vcpu(struct vm *vm, struct vm_snapshot_meta *meta)
{
	int error;
	struct vcpu *vcpu;
	uint16_t i, maxcpus;

	error = 0;

	maxcpus = vm_get_maxcpus(vm);
	for (i = 0; i < maxcpus; i++) {
		vcpu = vm->vcpu[i];
		if (vcpu == NULL)
			continue;

		error = vmmops_vcpu_snapshot(vcpu->cookie, meta);
		if (error != 0) {
			printf("%s: failed to snapshot vmcs/vmcb data for "
			       "vCPU: %d; error: %d\n", __func__, i, error);
			goto done;
		}
	}

done:
	return (error);
}

/*
 * Save kernel-side structures to user-space for snapshotting.
 */
int
vm_snapshot_req(struct vm *vm, struct vm_snapshot_meta *meta)
{
	int ret = 0;

	switch (meta->dev_req) {
	case STRUCT_VMCX:
		ret = vm_snapshot_vcpu(vm, meta);
		break;
	case STRUCT_VM:
		ret = vm_snapshot_vm(vm, meta);
		break;
	case STRUCT_VIOAPIC:
		ret = vioapic_snapshot(vm_ioapic(vm), meta);
		break;
	case STRUCT_VLAPIC:
		ret = vlapic_snapshot(vm, meta);
		break;
	case STRUCT_VHPET:
		ret = vhpet_snapshot(vm_hpet(vm), meta);
		break;
	case STRUCT_VATPIC:
		ret = vatpic_snapshot(vm_atpic(vm), meta);
		break;
	case STRUCT_VATPIT:
		ret = vatpit_snapshot(vm_atpit(vm), meta);
		break;
	case STRUCT_VPMTMR:
		ret = vpmtmr_snapshot(vm_pmtmr(vm), meta);
		break;
	case STRUCT_VRTC:
		ret = vrtc_snapshot(vm_rtc(vm), meta);
		break;
	default:
		printf("%s: failed to find the requested type %#x\n",
		       __func__, meta->dev_req);
		ret = (EINVAL);
	}
	return (ret);
}

void
vm_set_tsc_offset(struct vcpu *vcpu, uint64_t offset)
{
	vcpu->tsc_offset = offset;
}

int
vm_restore_time(struct vm *vm)
{
	int error;
	uint64_t now;
	struct vcpu *vcpu;
	uint16_t i, maxcpus;

	now = rdtsc();

	error = vhpet_restore_time(vm_hpet(vm));
	if (error)
		return (error);

	maxcpus = vm_get_maxcpus(vm);
	for (i = 0; i < maxcpus; i++) {
		vcpu = vm->vcpu[i];
		if (vcpu == NULL)
			continue;

		error = vmmops_restore_tsc(vcpu->cookie,
		    vcpu->tsc_offset - now);
		if (error)
			return (error);
	}

	return (0);
}
#endif
