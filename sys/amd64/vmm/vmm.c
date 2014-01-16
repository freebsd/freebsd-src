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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>

#include <machine/cpu.h>
#include <machine/vm.h>
#include <machine/pcb.h>
#include <machine/smp.h>
#include <x86/psl.h>
#include <x86/apicreg.h>
#include <machine/vmparam.h>

#include <machine/vmm.h>
#include <machine/vmm_dev.h>

#include "vmm_ktr.h"
#include "vmm_host.h"
#include "vmm_mem.h"
#include "vmm_util.h"
#include "vhpet.h"
#include "vioapic.h"
#include "vlapic.h"
#include "vmm_msr.h"
#include "vmm_ipi.h"
#include "vmm_stat.h"
#include "vmm_lapic.h"

#include "io/ppt.h"
#include "io/iommu.h"

struct vlapic;

struct vcpu {
	int		flags;
	enum vcpu_state	state;
	struct mtx	mtx;
	int		hostcpu;	/* host cpuid this vcpu last ran on */
	uint64_t	guest_msrs[VMM_MSR_NUM];
	struct vlapic	*vlapic;
	int		 vcpuid;
	struct savefpu	*guestfpu;	/* guest fpu state */
	void		*stats;
	struct vm_exit	exitinfo;
	enum x2apic_state x2apic_state;
	int		nmi_pending;
};

#define	vcpu_lock_init(v)	mtx_init(&((v)->mtx), "vcpu lock", 0, MTX_SPIN)
#define	vcpu_lock(v)		mtx_lock_spin(&((v)->mtx))
#define	vcpu_unlock(v)		mtx_unlock_spin(&((v)->mtx))
#define	vcpu_assert_locked(v)	mtx_assert(&((v)->mtx), MA_OWNED)

struct mem_seg {
	vm_paddr_t	gpa;
	size_t		len;
	boolean_t	wired;
	vm_object_t	object;
};
#define	VM_MAX_MEMORY_SEGMENTS	2

struct vm {
	void		*cookie;	/* processor-specific data */
	void		*iommu;		/* iommu-specific data */
	struct vhpet	*vhpet;		/* virtual HPET */
	struct vioapic	*vioapic;	/* virtual ioapic */
	struct vmspace	*vmspace;	/* guest's address space */
	struct vcpu	vcpu[VM_MAXCPU];
	int		num_mem_segs;
	struct mem_seg	mem_segs[VM_MAX_MEMORY_SEGMENTS];
	char		name[VM_MAX_NAMELEN];

	/*
	 * Set of active vcpus.
	 * An active vcpu is one that has been started implicitly (BSP) or
	 * explicitly (AP) by sending it a startup ipi.
	 */
	cpuset_t	active_cpus;

	struct mtx	rendezvous_mtx;
	cpuset_t	rendezvous_req_cpus;
	cpuset_t	rendezvous_done_cpus;
	void		*rendezvous_arg;
	vm_rendezvous_func_t rendezvous_func;
};

static int vmm_initialized;

static struct vmm_ops *ops;
#define	VMM_INIT(num)	(ops != NULL ? (*ops->init)(num) : 0)
#define	VMM_CLEANUP()	(ops != NULL ? (*ops->cleanup)() : 0)
#define	VMM_RESUME()	(ops != NULL ? (*ops->resume)() : 0)

#define	VMINIT(vm, pmap) (ops != NULL ? (*ops->vminit)(vm, pmap): NULL)
#define	VMRUN(vmi, vcpu, rip, pmap, rptr) \
	(ops != NULL ? (*ops->vmrun)(vmi, vcpu, rip, pmap, rptr) : ENXIO)
#define	VMCLEANUP(vmi)	(ops != NULL ? (*ops->vmcleanup)(vmi) : NULL)
#define	VMSPACE_ALLOC(min, max) \
	(ops != NULL ? (*ops->vmspace_alloc)(min, max) : NULL)
#define	VMSPACE_FREE(vmspace) \
	(ops != NULL ? (*ops->vmspace_free)(vmspace) : ENXIO)
#define	VMGETREG(vmi, vcpu, num, retval)		\
	(ops != NULL ? (*ops->vmgetreg)(vmi, vcpu, num, retval) : ENXIO)
#define	VMSETREG(vmi, vcpu, num, val)		\
	(ops != NULL ? (*ops->vmsetreg)(vmi, vcpu, num, val) : ENXIO)
#define	VMGETDESC(vmi, vcpu, num, desc)		\
	(ops != NULL ? (*ops->vmgetdesc)(vmi, vcpu, num, desc) : ENXIO)
#define	VMSETDESC(vmi, vcpu, num, desc)		\
	(ops != NULL ? (*ops->vmsetdesc)(vmi, vcpu, num, desc) : ENXIO)
#define	VMINJECT(vmi, vcpu, type, vec, ec, ecv)	\
	(ops != NULL ? (*ops->vminject)(vmi, vcpu, type, vec, ec, ecv) : ENXIO)
#define	VMGETCAP(vmi, vcpu, num, retval)	\
	(ops != NULL ? (*ops->vmgetcap)(vmi, vcpu, num, retval) : ENXIO)
#define	VMSETCAP(vmi, vcpu, num, val)		\
	(ops != NULL ? (*ops->vmsetcap)(vmi, vcpu, num, val) : ENXIO)
#define	VLAPIC_INIT(vmi, vcpu)			\
	(ops != NULL ? (*ops->vlapic_init)(vmi, vcpu) : NULL)
#define	VLAPIC_CLEANUP(vmi, vlapic)		\
	(ops != NULL ? (*ops->vlapic_cleanup)(vmi, vlapic) : NULL)

#define	fpu_start_emulating()	load_cr0(rcr0() | CR0_TS)
#define	fpu_stop_emulating()	clts()

static MALLOC_DEFINE(M_VM, "vm", "vm");
CTASSERT(VMM_MSR_NUM <= 64);	/* msr_mask can keep track of up to 64 msrs */

/* statistics */
static VMM_STAT(VCPU_TOTAL_RUNTIME, "vcpu total runtime");

SYSCTL_NODE(_hw, OID_AUTO, vmm, CTLFLAG_RW, NULL, NULL);

static int vmm_ipinum;
SYSCTL_INT(_hw_vmm, OID_AUTO, ipinum, CTLFLAG_RD, &vmm_ipinum, 0,
    "IPI vector used for vcpu notifications");

static void vm_deactivate_cpu(struct vm *vm, int vcpuid);

static void
vcpu_cleanup(struct vm *vm, int i)
{
	struct vcpu *vcpu = &vm->vcpu[i];

	VLAPIC_CLEANUP(vm->cookie, vcpu->vlapic);
	vmm_stat_free(vcpu->stats);	
	fpu_save_area_free(vcpu->guestfpu);
}

static void
vcpu_init(struct vm *vm, uint32_t vcpu_id)
{
	struct vcpu *vcpu;
	
	vcpu = &vm->vcpu[vcpu_id];

	vcpu_lock_init(vcpu);
	vcpu->hostcpu = NOCPU;
	vcpu->vcpuid = vcpu_id;
	vcpu->vlapic = VLAPIC_INIT(vm->cookie, vcpu_id);
	vm_set_x2apic_state(vm, vcpu_id, X2APIC_ENABLED);
	vcpu->guestfpu = fpu_save_area_alloc();
	fpu_save_area_reset(vcpu->guestfpu);
	vcpu->stats = vmm_stat_alloc();
}

struct vm_exit *
vm_exitinfo(struct vm *vm, int cpuid)
{
	struct vcpu *vcpu;

	if (cpuid < 0 || cpuid >= VM_MAXCPU)
		panic("vm_exitinfo: invalid cpuid %d", cpuid);

	vcpu = &vm->vcpu[cpuid];

	return (&vcpu->exitinfo);
}

static void
vmm_resume(void)
{
	VMM_RESUME();
}

static int
vmm_init(void)
{
	int error;

	vmm_host_state_init();

	vmm_ipinum = vmm_ipi_alloc();
	if (vmm_ipinum == 0)
		vmm_ipinum = IPI_AST;

	error = vmm_mem_init();
	if (error)
		return (error);
	
	if (vmm_is_intel())
		ops = &vmm_ops_intel;
	else if (vmm_is_amd())
		ops = &vmm_ops_amd;
	else
		return (ENXIO);

	vmm_msr_init();
	vmm_resume_p = vmm_resume;

	return (VMM_INIT(vmm_ipinum));
}

static int
vmm_handler(module_t mod, int what, void *arg)
{
	int error;

	switch (what) {
	case MOD_LOAD:
		vmmdev_init();
		iommu_init();
		error = vmm_init();
		if (error == 0)
			vmm_initialized = 1;
		break;
	case MOD_UNLOAD:
		error = vmmdev_cleanup();
		if (error == 0) {
			vmm_resume_p = NULL;
			iommu_cleanup();
			if (vmm_ipinum != IPI_AST)
				vmm_ipi_free(vmm_ipinum);
			error = VMM_CLEANUP();
			/*
			 * Something bad happened - prevent new
			 * VMs from being created
			 */
			if (error)
				vmm_initialized = 0;
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
 * - iommu initialization must happen after the pci passthru driver has had
 *   a chance to attach to any passthru devices (after SI_SUB_CONFIGURE).
 *
 * - VT-x initialization requires smp_rendezvous() and therefore must happen
 *   after SMP is fully functional (after SI_SUB_SMP).
 */
DECLARE_MODULE(vmm, vmm_kmod, SI_SUB_SMP + 1, SI_ORDER_ANY);
MODULE_VERSION(vmm, 1);

int
vm_create(const char *name, struct vm **retvm)
{
	int i;
	struct vm *vm;
	struct vmspace *vmspace;

	const int BSP = 0;

	/*
	 * If vmm.ko could not be successfully initialized then don't attempt
	 * to create the virtual machine.
	 */
	if (!vmm_initialized)
		return (ENXIO);

	if (name == NULL || strlen(name) >= VM_MAX_NAMELEN)
		return (EINVAL);

	vmspace = VMSPACE_ALLOC(VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS);
	if (vmspace == NULL)
		return (ENOMEM);

	vm = malloc(sizeof(struct vm), M_VM, M_WAITOK | M_ZERO);
	strcpy(vm->name, name);
	vm->vmspace = vmspace;
	mtx_init(&vm->rendezvous_mtx, "vm rendezvous lock", 0, MTX_DEF);
	vm->cookie = VMINIT(vm, vmspace_pmap(vmspace));
	vm->vioapic = vioapic_init(vm);
	vm->vhpet = vhpet_init(vm);

	for (i = 0; i < VM_MAXCPU; i++) {
		vcpu_init(vm, i);
		guest_msrs_init(vm, i);
	}

	vm_activate_cpu(vm, BSP);

	*retvm = vm;
	return (0);
}

static void
vm_free_mem_seg(struct vm *vm, struct mem_seg *seg)
{

	if (seg->object != NULL)
		vmm_mem_free(vm->vmspace, seg->gpa, seg->len);

	bzero(seg, sizeof(*seg));
}

void
vm_destroy(struct vm *vm)
{
	int i;

	ppt_unassign_all(vm);

	if (vm->iommu != NULL)
		iommu_destroy_domain(vm->iommu);

	vhpet_cleanup(vm->vhpet);
	vioapic_cleanup(vm->vioapic);

	for (i = 0; i < vm->num_mem_segs; i++)
		vm_free_mem_seg(vm, &vm->mem_segs[i]);

	vm->num_mem_segs = 0;

	for (i = 0; i < VM_MAXCPU; i++)
		vcpu_cleanup(vm, i);

	VMSPACE_FREE(vm->vmspace);

	VMCLEANUP(vm->cookie);

	free(vm, M_VM);
}

const char *
vm_name(struct vm *vm)
{
	return (vm->name);
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

boolean_t
vm_mem_allocated(struct vm *vm, vm_paddr_t gpa)
{
	int i;
	vm_paddr_t gpabase, gpalimit;

	for (i = 0; i < vm->num_mem_segs; i++) {
		gpabase = vm->mem_segs[i].gpa;
		gpalimit = gpabase + vm->mem_segs[i].len;
		if (gpa >= gpabase && gpa < gpalimit)
			return (TRUE);		/* 'gpa' is regular memory */
	}

	if (ppt_is_mmio(vm, gpa))
		return (TRUE);			/* 'gpa' is pci passthru mmio */

	return (FALSE);
}

int
vm_malloc(struct vm *vm, vm_paddr_t gpa, size_t len)
{
	int available, allocated;
	struct mem_seg *seg;
	vm_object_t object;
	vm_paddr_t g;

	if ((gpa & PAGE_MASK) || (len & PAGE_MASK) || len == 0)
		return (EINVAL);
	
	available = allocated = 0;
	g = gpa;
	while (g < gpa + len) {
		if (vm_mem_allocated(vm, g))
			allocated++;
		else
			available++;

		g += PAGE_SIZE;
	}

	/*
	 * If there are some allocated and some available pages in the address
	 * range then it is an error.
	 */
	if (allocated && available)
		return (EINVAL);

	/*
	 * If the entire address range being requested has already been
	 * allocated then there isn't anything more to do.
	 */
	if (allocated && available == 0)
		return (0);

	if (vm->num_mem_segs >= VM_MAX_MEMORY_SEGMENTS)
		return (E2BIG);

	seg = &vm->mem_segs[vm->num_mem_segs];

	if ((object = vmm_mem_alloc(vm->vmspace, gpa, len)) == NULL)
		return (ENOMEM);

	seg->gpa = gpa;
	seg->len = len;
	seg->object = object;
	seg->wired = FALSE;

	vm->num_mem_segs++;

	return (0);
}

static void
vm_gpa_unwire(struct vm *vm)
{
	int i, rv;
	struct mem_seg *seg;

	for (i = 0; i < vm->num_mem_segs; i++) {
		seg = &vm->mem_segs[i];
		if (!seg->wired)
			continue;

		rv = vm_map_unwire(&vm->vmspace->vm_map,
				   seg->gpa, seg->gpa + seg->len,
				   VM_MAP_WIRE_USER | VM_MAP_WIRE_NOHOLES);
		KASSERT(rv == KERN_SUCCESS, ("vm(%s) memory segment "
		    "%#lx/%ld could not be unwired: %d",
		    vm_name(vm), seg->gpa, seg->len, rv));

		seg->wired = FALSE;
	}
}

static int
vm_gpa_wire(struct vm *vm)
{
	int i, rv;
	struct mem_seg *seg;

	for (i = 0; i < vm->num_mem_segs; i++) {
		seg = &vm->mem_segs[i];
		if (seg->wired)
			continue;

		/* XXX rlimits? */
		rv = vm_map_wire(&vm->vmspace->vm_map,
				 seg->gpa, seg->gpa + seg->len,
				 VM_MAP_WIRE_USER | VM_MAP_WIRE_NOHOLES);
		if (rv != KERN_SUCCESS)
			break;

		seg->wired = TRUE;
	}

	if (i < vm->num_mem_segs) {
		/*
		 * Undo the wiring before returning an error.
		 */
		vm_gpa_unwire(vm);
		return (EAGAIN);
	}

	return (0);
}

static void
vm_iommu_modify(struct vm *vm, boolean_t map)
{
	int i, sz;
	vm_paddr_t gpa, hpa;
	struct mem_seg *seg;
	void *vp, *cookie, *host_domain;

	sz = PAGE_SIZE;
	host_domain = iommu_host_domain();

	for (i = 0; i < vm->num_mem_segs; i++) {
		seg = &vm->mem_segs[i];
		KASSERT(seg->wired, ("vm(%s) memory segment %#lx/%ld not wired",
		    vm_name(vm), seg->gpa, seg->len));

		gpa = seg->gpa;
		while (gpa < seg->gpa + seg->len) {
			vp = vm_gpa_hold(vm, gpa, PAGE_SIZE, VM_PROT_WRITE,
					 &cookie);
			KASSERT(vp != NULL, ("vm(%s) could not map gpa %#lx",
			    vm_name(vm), gpa));

			vm_gpa_release(cookie);

			hpa = DMAP_TO_PHYS((uintptr_t)vp);
			if (map) {
				iommu_create_mapping(vm->iommu, gpa, hpa, sz);
				iommu_remove_mapping(host_domain, hpa, sz);
			} else {
				iommu_remove_mapping(vm->iommu, gpa, sz);
				iommu_create_mapping(host_domain, hpa, hpa, sz);
			}

			gpa += PAGE_SIZE;
		}
	}

	/*
	 * Invalidate the cached translations associated with the domain
	 * from which pages were removed.
	 */
	if (map)
		iommu_invalidate_tlb(host_domain);
	else
		iommu_invalidate_tlb(vm->iommu);
}

#define	vm_iommu_unmap(vm)	vm_iommu_modify((vm), FALSE)
#define	vm_iommu_map(vm)	vm_iommu_modify((vm), TRUE)

int
vm_unassign_pptdev(struct vm *vm, int bus, int slot, int func)
{
	int error;

	error = ppt_unassign_device(vm, bus, slot, func);
	if (error)
		return (error);

	if (ppt_num_devices(vm) == 0) {
		vm_iommu_unmap(vm);
		vm_gpa_unwire(vm);
	}
	return (0);
}

int
vm_assign_pptdev(struct vm *vm, int bus, int slot, int func)
{
	int error;
	vm_paddr_t maxaddr;

	/*
	 * Virtual machines with pci passthru devices get special treatment:
	 * - the guest physical memory is wired
	 * - the iommu is programmed to do the 'gpa' to 'hpa' translation
	 *
	 * We need to do this before the first pci passthru device is attached.
	 */
	if (ppt_num_devices(vm) == 0) {
		KASSERT(vm->iommu == NULL,
		    ("vm_assign_pptdev: iommu must be NULL"));
		maxaddr = vmm_mem_maxaddr();
		vm->iommu = iommu_create_domain(maxaddr);

		error = vm_gpa_wire(vm);
		if (error)
			return (error);

		vm_iommu_map(vm);
	}

	error = ppt_assign_device(vm, bus, slot, func);
	return (error);
}

void *
vm_gpa_hold(struct vm *vm, vm_paddr_t gpa, size_t len, int reqprot,
	    void **cookie)
{
	int count, pageoff;
	vm_page_t m;

	pageoff = gpa & PAGE_MASK;
	if (len > PAGE_SIZE - pageoff)
		panic("vm_gpa_hold: invalid gpa/len: 0x%016lx/%lu", gpa, len);

	count = vm_fault_quick_hold_pages(&vm->vmspace->vm_map,
	    trunc_page(gpa), PAGE_SIZE, reqprot, &m, 1);

	if (count == 1) {
		*cookie = m;
		return ((void *)(PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)) + pageoff));
	} else {
		*cookie = NULL;
		return (NULL);
	}
}

void
vm_gpa_release(void *cookie)
{
	vm_page_t m = cookie;

	vm_page_lock(m);
	vm_page_unhold(m);
	vm_page_unlock(m);
}

int
vm_gpabase2memseg(struct vm *vm, vm_paddr_t gpabase,
		  struct vm_memory_segment *seg)
{
	int i;

	for (i = 0; i < vm->num_mem_segs; i++) {
		if (gpabase == vm->mem_segs[i].gpa) {
			seg->gpa = vm->mem_segs[i].gpa;
			seg->len = vm->mem_segs[i].len;
			seg->wired = vm->mem_segs[i].wired;
			return (0);
		}
	}
	return (-1);
}

int
vm_get_memobj(struct vm *vm, vm_paddr_t gpa, size_t len,
	      vm_offset_t *offset, struct vm_object **object)
{
	int i;
	size_t seg_len;
	vm_paddr_t seg_gpa;
	vm_object_t seg_obj;

	for (i = 0; i < vm->num_mem_segs; i++) {
		if ((seg_obj = vm->mem_segs[i].object) == NULL)
			continue;

		seg_gpa = vm->mem_segs[i].gpa;
		seg_len = vm->mem_segs[i].len;

		if (gpa >= seg_gpa && gpa < seg_gpa + seg_len) {
			*offset = gpa - seg_gpa;
			*object = seg_obj;
			vm_object_reference(seg_obj);
			return (0);
		}
	}

	return (EINVAL);
}

int
vm_get_register(struct vm *vm, int vcpu, int reg, uint64_t *retval)
{

	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (reg >= VM_REG_LAST)
		return (EINVAL);

	return (VMGETREG(vm->cookie, vcpu, reg, retval));
}

int
vm_set_register(struct vm *vm, int vcpu, int reg, uint64_t val)
{

	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (reg >= VM_REG_LAST)
		return (EINVAL);

	return (VMSETREG(vm->cookie, vcpu, reg, val));
}

static boolean_t
is_descriptor_table(int reg)
{

	switch (reg) {
	case VM_REG_GUEST_IDTR:
	case VM_REG_GUEST_GDTR:
		return (TRUE);
	default:
		return (FALSE);
	}
}

static boolean_t
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
		return (TRUE);
	default:
		return (FALSE);
	}
}

int
vm_get_seg_desc(struct vm *vm, int vcpu, int reg,
		struct seg_desc *desc)
{

	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (!is_segment_register(reg) && !is_descriptor_table(reg))
		return (EINVAL);

	return (VMGETDESC(vm->cookie, vcpu, reg, desc));
}

int
vm_set_seg_desc(struct vm *vm, int vcpu, int reg,
		struct seg_desc *desc)
{
	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (!is_segment_register(reg) && !is_descriptor_table(reg))
		return (EINVAL);

	return (VMSETDESC(vm->cookie, vcpu, reg, desc));
}

static void
restore_guest_fpustate(struct vcpu *vcpu)
{

	/* flush host state to the pcb */
	fpuexit(curthread);

	/* restore guest FPU state */
	fpu_stop_emulating();
	fpurestore(vcpu->guestfpu);

	/*
	 * The FPU is now "dirty" with the guest's state so turn on emulation
	 * to trap any access to the FPU by the host.
	 */
	fpu_start_emulating();
}

static void
save_guest_fpustate(struct vcpu *vcpu)
{

	if ((rcr0() & CR0_TS) == 0)
		panic("fpu emulation not enabled in host!");

	/* save guest FPU state */
	fpu_stop_emulating();
	fpusave(vcpu->guestfpu);
	fpu_start_emulating();
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
		while (vcpu->state != VCPU_IDLE)
			msleep_spin(&vcpu->state, &vcpu->mtx, "vmstat", hz);
	} else {
		KASSERT(vcpu->state != VCPU_IDLE, ("invalid transition from "
		    "vcpu idle state"));
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
	if (newstate == VCPU_IDLE)
		wakeup(&vcpu->state);

	return (0);
}

static void
vcpu_require_state(struct vm *vm, int vcpuid, enum vcpu_state newstate)
{
	int error;

	if ((error = vcpu_set_state(vm, vcpuid, newstate, false)) != 0)
		panic("Error %d setting state to %d\n", error, newstate);
}

static void
vcpu_require_state_locked(struct vcpu *vcpu, enum vcpu_state newstate)
{
	int error;

	if ((error = vcpu_set_state_locked(vcpu, newstate, false)) != 0)
		panic("Error %d setting state to %d", error, newstate);
}

static void
vm_set_rendezvous_func(struct vm *vm, vm_rendezvous_func_t func)
{

	KASSERT(mtx_owned(&vm->rendezvous_mtx), ("rendezvous_mtx not locked"));

	/*
	 * Update 'rendezvous_func' and execute a write memory barrier to
	 * ensure that it is visible across all host cpus. This is not needed
	 * for correctness but it does ensure that all the vcpus will notice
	 * that the rendezvous is requested immediately.
	 */
	vm->rendezvous_func = func;
	wmb();
}

#define	RENDEZVOUS_CTR0(vm, vcpuid, fmt)				\
	do {								\
		if (vcpuid >= 0)					\
			VCPU_CTR0(vm, vcpuid, fmt);			\
		else							\
			VM_CTR0(vm, fmt);				\
	} while (0)

static void
vm_handle_rendezvous(struct vm *vm, int vcpuid)
{

	KASSERT(vcpuid == -1 || (vcpuid >= 0 && vcpuid < VM_MAXCPU),
	    ("vm_handle_rendezvous: invalid vcpuid %d", vcpuid));

	mtx_lock(&vm->rendezvous_mtx);
	while (vm->rendezvous_func != NULL) {
		if (vcpuid != -1 &&
		    CPU_ISSET(vcpuid, &vm->rendezvous_req_cpus)) {
			VCPU_CTR0(vm, vcpuid, "Calling rendezvous func");
			(*vm->rendezvous_func)(vm, vcpuid, vm->rendezvous_arg);
			CPU_SET(vcpuid, &vm->rendezvous_done_cpus);
		}
		if (CPU_CMP(&vm->rendezvous_req_cpus,
		    &vm->rendezvous_done_cpus) == 0) {
			VCPU_CTR0(vm, vcpuid, "Rendezvous completed");
			vm_set_rendezvous_func(vm, NULL);
			wakeup(&vm->rendezvous_func);
			break;
		}
		RENDEZVOUS_CTR0(vm, vcpuid, "Wait for rendezvous completion");
		mtx_sleep(&vm->rendezvous_func, &vm->rendezvous_mtx, 0,
		    "vmrndv", 0);
	}
	mtx_unlock(&vm->rendezvous_mtx);
}

/*
 * Emulate a guest 'hlt' by sleeping until the vcpu is ready to run.
 */
static int
vm_handle_hlt(struct vm *vm, int vcpuid, bool intr_disabled, bool *retu)
{
	struct vm_exit *vmexit;
	struct vcpu *vcpu;
	int t, timo;

	vcpu = &vm->vcpu[vcpuid];

	vcpu_lock(vcpu);

	/*
	 * Do a final check for pending NMI or interrupts before
	 * really putting this thread to sleep.
	 *
	 * These interrupts could have happened any time after we
	 * returned from VMRUN() and before we grabbed the vcpu lock.
	 */
	if (!vm_nmi_pending(vm, vcpuid) &&
	    (intr_disabled || !vlapic_pending_intr(vcpu->vlapic, NULL))) {
		t = ticks;
		vcpu_require_state_locked(vcpu, VCPU_SLEEPING);
		if (vlapic_enabled(vcpu->vlapic)) {
			/*
			 * XXX msleep_spin() is not interruptible so use the
			 * 'timo' to put an upper bound on the sleep time.
			 */
			timo = hz;
			msleep_spin(vcpu, &vcpu->mtx, "vmidle", timo);
		} else {
			/*
			 * Spindown the vcpu if the apic is disabled and it
			 * had entered the halted state.
			 */
			*retu = true;
			vmexit = vm_exitinfo(vm, vcpuid);
			vmexit->exitcode = VM_EXITCODE_SPINDOWN_CPU;
			vm_deactivate_cpu(vm, vcpuid);
			VCPU_CTR0(vm, vcpuid, "spinning down cpu");
		}
		vcpu_require_state_locked(vcpu, VCPU_FROZEN);
		vmm_stat_incr(vm, vcpuid, VCPU_IDLE_TICKS, ticks - t);
	}
	vcpu_unlock(vcpu);

	return (0);
}

static int
vm_handle_paging(struct vm *vm, int vcpuid, bool *retu)
{
	int rv, ftype;
	struct vm_map *map;
	struct vcpu *vcpu;
	struct vm_exit *vme;

	vcpu = &vm->vcpu[vcpuid];
	vme = &vcpu->exitinfo;

	ftype = vme->u.paging.fault_type;
	KASSERT(ftype == VM_PROT_READ ||
	    ftype == VM_PROT_WRITE || ftype == VM_PROT_EXECUTE,
	    ("vm_handle_paging: invalid fault_type %d", ftype));

	if (ftype == VM_PROT_READ || ftype == VM_PROT_WRITE) {
		rv = pmap_emulate_accessed_dirty(vmspace_pmap(vm->vmspace),
		    vme->u.paging.gpa, ftype);
		if (rv == 0)
			goto done;
	}

	map = &vm->vmspace->vm_map;
	rv = vm_fault(map, vme->u.paging.gpa, ftype, VM_FAULT_NORMAL);

	VCPU_CTR3(vm, vcpuid, "vm_handle_paging rv = %d, gpa = %#lx, "
	    "ftype = %d", rv, vme->u.paging.gpa, ftype);

	if (rv != KERN_SUCCESS)
		return (EFAULT);
done:
	/* restart execution at the faulting instruction */
	vme->inst_length = 0;

	return (0);
}

static int
vm_handle_inst_emul(struct vm *vm, int vcpuid, bool *retu)
{
	struct vie *vie;
	struct vcpu *vcpu;
	struct vm_exit *vme;
	int error, inst_length;
	uint64_t rip, gla, gpa, cr3;
	mem_region_read_t mread;
	mem_region_write_t mwrite;

	vcpu = &vm->vcpu[vcpuid];
	vme = &vcpu->exitinfo;

	rip = vme->rip;
	inst_length = vme->inst_length;

	gla = vme->u.inst_emul.gla;
	gpa = vme->u.inst_emul.gpa;
	cr3 = vme->u.inst_emul.cr3;
	vie = &vme->u.inst_emul.vie;

	vie_init(vie);

	/* Fetch, decode and emulate the faulting instruction */
	if (vmm_fetch_instruction(vm, vcpuid, rip, inst_length, cr3, vie) != 0)
		return (EFAULT);

	if (vmm_decode_instruction(vm, vcpuid, gla, vie) != 0)
		return (EFAULT);

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

	error = vmm_emulate_instruction(vm, vcpuid, gpa, vie, mread, mwrite,
	    retu);

	return (error);
}

int
vm_run(struct vm *vm, struct vm_run *vmrun)
{
	int error, vcpuid;
	struct vcpu *vcpu;
	struct pcb *pcb;
	uint64_t tscval, rip;
	struct vm_exit *vme;
	bool retu, intr_disabled;
	pmap_t pmap;

	vcpuid = vmrun->cpuid;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	pmap = vmspace_pmap(vm->vmspace);
	vcpu = &vm->vcpu[vcpuid];
	vme = &vcpu->exitinfo;
	rip = vmrun->rip;
restart:
	critical_enter();

	KASSERT(!CPU_ISSET(curcpu, &pmap->pm_active),
	    ("vm_run: absurd pm_active"));

	tscval = rdtsc();

	pcb = PCPU_GET(curpcb);
	set_pcb_flags(pcb, PCB_FULL_IRET);

	restore_guest_msrs(vm, vcpuid);	
	restore_guest_fpustate(vcpu);

	vcpu_require_state(vm, vcpuid, VCPU_RUNNING);
	vcpu->hostcpu = curcpu;
	error = VMRUN(vm->cookie, vcpuid, rip, pmap, &vm->rendezvous_func);
	vcpu->hostcpu = NOCPU;
	vcpu_require_state(vm, vcpuid, VCPU_FROZEN);

	save_guest_fpustate(vcpu);
	restore_host_msrs(vm, vcpuid);

	vmm_stat_incr(vm, vcpuid, VCPU_TOTAL_RUNTIME, rdtsc() - tscval);

	critical_exit();

	if (error == 0) {
		retu = false;
		switch (vme->exitcode) {
		case VM_EXITCODE_RENDEZVOUS:
			vm_handle_rendezvous(vm, vcpuid);
			error = 0;
			break;
		case VM_EXITCODE_HLT:
			intr_disabled = ((vme->u.hlt.rflags & PSL_I) == 0);
			error = vm_handle_hlt(vm, vcpuid, intr_disabled, &retu);
			break;
		case VM_EXITCODE_PAGING:
			error = vm_handle_paging(vm, vcpuid, &retu);
			break;
		case VM_EXITCODE_INST_EMUL:
			error = vm_handle_inst_emul(vm, vcpuid, &retu);
			break;
		default:
			retu = true;	/* handled in userland */
			break;
		}
	}

	if (error == 0 && retu == false) {
		rip = vme->rip + vme->inst_length;
		goto restart;
	}

	/* copy the exit information */
	bcopy(vme, &vmrun->vm_exit, sizeof(struct vm_exit));
	return (error);
}

int
vm_inject_event(struct vm *vm, int vcpuid, int type,
		int vector, uint32_t code, int code_valid)
{
	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	if ((type > VM_EVENT_NONE && type < VM_EVENT_MAX) == 0)
		return (EINVAL);

	if (vector < 0 || vector > 255)
		return (EINVAL);

	return (VMINJECT(vm->cookie, vcpuid, type, vector, code, code_valid));
}

static VMM_STAT(VCPU_NMI_COUNT, "number of NMIs delivered to vcpu");

int
vm_inject_nmi(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	vcpu = &vm->vcpu[vcpuid];

	vcpu->nmi_pending = 1;
	vcpu_notify_event(vm, vcpuid, false);
	return (0);
}

int
vm_nmi_pending(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_nmi_pending: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	return (vcpu->nmi_pending);
}

void
vm_nmi_clear(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_nmi_pending: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	if (vcpu->nmi_pending == 0)
		panic("vm_nmi_clear: inconsistent nmi_pending state");

	vcpu->nmi_pending = 0;
	vmm_stat_incr(vm, vcpuid, VCPU_NMI_COUNT, 1);
}

int
vm_get_capability(struct vm *vm, int vcpu, int type, int *retval)
{
	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (type < 0 || type >= VM_CAP_MAX)
		return (EINVAL);

	return (VMGETCAP(vm->cookie, vcpu, type, retval));
}

int
vm_set_capability(struct vm *vm, int vcpu, int type, int val)
{
	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (type < 0 || type >= VM_CAP_MAX)
		return (EINVAL);

	return (VMSETCAP(vm->cookie, vcpu, type, val));
}

uint64_t *
vm_guest_msrs(struct vm *vm, int cpu)
{
	return (vm->vcpu[cpu].guest_msrs);
}

struct vlapic *
vm_lapic(struct vm *vm, int cpu)
{
	return (vm->vcpu[cpu].vlapic);
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

boolean_t
vmm_is_pptdev(int bus, int slot, int func)
{
	int found, i, n;
	int b, s, f;
	char *val, *cp, *cp2;

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
	found = 0;
	for (i = 0; names[i] != NULL && !found; i++) {
		cp = val = getenv(names[i]);
		while (cp != NULL && *cp != '\0') {
			if ((cp2 = strchr(cp, ' ')) != NULL)
				*cp2 = '\0';

			n = sscanf(cp, "%d/%d/%d", &b, &s, &f);
			if (n == 3 && bus == b && slot == s && func == f) {
				found = 1;
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
vcpu_set_state(struct vm *vm, int vcpuid, enum vcpu_state newstate,
    bool from_idle)
{
	int error;
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_set_run_state: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	vcpu_lock(vcpu);
	error = vcpu_set_state_locked(vcpu, newstate, from_idle);
	vcpu_unlock(vcpu);

	return (error);
}

enum vcpu_state
vcpu_get_state(struct vm *vm, int vcpuid, int *hostcpu)
{
	struct vcpu *vcpu;
	enum vcpu_state state;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_get_run_state: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	vcpu_lock(vcpu);
	state = vcpu->state;
	if (hostcpu != NULL)
		*hostcpu = vcpu->hostcpu;
	vcpu_unlock(vcpu);

	return (state);
}

void
vm_activate_cpu(struct vm *vm, int vcpuid)
{

	if (vcpuid >= 0 && vcpuid < VM_MAXCPU)
		CPU_SET(vcpuid, &vm->active_cpus);
}

static void
vm_deactivate_cpu(struct vm *vm, int vcpuid)
{

	if (vcpuid >= 0 && vcpuid < VM_MAXCPU)
		CPU_CLR(vcpuid, &vm->active_cpus);
}

cpuset_t
vm_active_cpus(struct vm *vm)
{

	return (vm->active_cpus);
}

void *
vcpu_stats(struct vm *vm, int vcpuid)
{

	return (vm->vcpu[vcpuid].stats);
}

int
vm_get_x2apic_state(struct vm *vm, int vcpuid, enum x2apic_state *state)
{
	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	*state = vm->vcpu[vcpuid].x2apic_state;

	return (0);
}

int
vm_set_x2apic_state(struct vm *vm, int vcpuid, enum x2apic_state state)
{
	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	if (state >= X2APIC_STATE_LAST)
		return (EINVAL);

	vm->vcpu[vcpuid].x2apic_state = state;

	vlapic_set_x2apic_state(vm, vcpuid, state);

	return (0);
}

/*
 * This function is called to ensure that a vcpu "sees" a pending event
 * as soon as possible:
 * - If the vcpu thread is sleeping then it is woken up.
 * - If the vcpu is running on a different host_cpu then an IPI will be directed
 *   to the host_cpu to cause the vcpu to trap into the hypervisor.
 */
void
vcpu_notify_event(struct vm *vm, int vcpuid, bool lapic_intr)
{
	int hostcpu;
	struct vcpu *vcpu;

	vcpu = &vm->vcpu[vcpuid];

	vcpu_lock(vcpu);
	hostcpu = vcpu->hostcpu;
	if (hostcpu == NOCPU) {
		if (vcpu->state == VCPU_SLEEPING)
			wakeup_one(vcpu);
	} else {
		if (vcpu->state != VCPU_RUNNING)
			panic("invalid vcpu state %d", vcpu->state);
		if (hostcpu != curcpu) {
			if (lapic_intr)
				vlapic_post_intr(vcpu->vlapic, hostcpu,
				    vmm_ipinum);
			else
				ipi_cpu(hostcpu, vmm_ipinum);
		}
	}
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

void
vm_smp_rendezvous(struct vm *vm, int vcpuid, cpuset_t dest,
    vm_rendezvous_func_t func, void *arg)
{
	/*
	 * Enforce that this function is called without any locks
	 */
	WITNESS_WARN(WARN_PANIC, NULL, "vm_smp_rendezvous");
	KASSERT(vcpuid == -1 || (vcpuid >= 0 && vcpuid < VM_MAXCPU),
	    ("vm_smp_rendezvous: invalid vcpuid %d", vcpuid));

restart:
	mtx_lock(&vm->rendezvous_mtx);
	if (vm->rendezvous_func != NULL) {
		/*
		 * If a rendezvous is already in progress then we need to
		 * call the rendezvous handler in case this 'vcpuid' is one
		 * of the targets of the rendezvous.
		 */
		RENDEZVOUS_CTR0(vm, vcpuid, "Rendezvous already in progress");
		mtx_unlock(&vm->rendezvous_mtx);
		vm_handle_rendezvous(vm, vcpuid);
		goto restart;
	}
	KASSERT(vm->rendezvous_func == NULL, ("vm_smp_rendezvous: previous "
	    "rendezvous is still in progress"));

	RENDEZVOUS_CTR0(vm, vcpuid, "Initiating rendezvous");
	vm->rendezvous_req_cpus = dest;
	CPU_ZERO(&vm->rendezvous_done_cpus);
	vm->rendezvous_arg = arg;
	vm_set_rendezvous_func(vm, func);
	mtx_unlock(&vm->rendezvous_mtx);

	vm_handle_rendezvous(vm, vcpuid);
}
