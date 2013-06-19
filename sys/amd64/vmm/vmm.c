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
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <machine/vm.h>
#include <machine/pcb.h>
#include <machine/smp.h>
#include <x86/apicreg.h>

#include <machine/vmm.h>
#include "vmm_host.h"
#include "vmm_mem.h"
#include "vmm_util.h"
#include <machine/vmm_dev.h>
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

#define	VM_MAX_MEMORY_SEGMENTS	2

struct vm {
	void		*cookie;	/* processor-specific data */
	void		*iommu;		/* iommu-specific data */
	struct vcpu	vcpu[VM_MAXCPU];
	int		num_mem_segs;
	struct vm_memory_segment mem_segs[VM_MAX_MEMORY_SEGMENTS];
	char		name[VM_MAX_NAMELEN];

	/*
	 * Set of active vcpus.
	 * An active vcpu is one that has been started implicitly (BSP) or
	 * explicitly (AP) by sending it a startup ipi.
	 */
	cpuset_t	active_cpus;
};

static int vmm_initialized;

static struct vmm_ops *ops;
#define	VMM_INIT()	(ops != NULL ? (*ops->init)() : 0)
#define	VMM_CLEANUP()	(ops != NULL ? (*ops->cleanup)() : 0)

#define	VMINIT(vm)	(ops != NULL ? (*ops->vminit)(vm): NULL)
#define	VMRUN(vmi, vcpu, rip) \
	(ops != NULL ? (*ops->vmrun)(vmi, vcpu, rip) : ENXIO)
#define	VMCLEANUP(vmi)	(ops != NULL ? (*ops->vmcleanup)(vmi) : NULL)
#define	VMMMAP_SET(vmi, gpa, hpa, len, attr, prot, spm)			\
    	(ops != NULL ? 							\
    	(*ops->vmmmap_set)(vmi, gpa, hpa, len, attr, prot, spm) :	\
	ENXIO)
#define	VMMMAP_GET(vmi, gpa) \
	(ops != NULL ? (*ops->vmmmap_get)(vmi, gpa) : ENXIO)
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

#define	fpu_start_emulating()	load_cr0(rcr0() | CR0_TS)
#define	fpu_stop_emulating()	clts()

static MALLOC_DEFINE(M_VM, "vm", "vm");
CTASSERT(VMM_MSR_NUM <= 64);	/* msr_mask can keep track of up to 64 msrs */

/* statistics */
static VMM_STAT(VCPU_TOTAL_RUNTIME, "vcpu total runtime");

static void
vcpu_cleanup(struct vcpu *vcpu)
{
	vlapic_cleanup(vcpu->vlapic);
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
	vcpu->vlapic = vlapic_init(vm, vcpu_id);
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

static int
vmm_init(void)
{
	int error;

	vmm_host_state_init();
	vmm_ipi_init();

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

	return (VMM_INIT());
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
			iommu_cleanup();
			vmm_ipi_cleanup();
			error = VMM_CLEANUP();
		}
		vmm_initialized = 0;
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

SYSCTL_NODE(_hw, OID_AUTO, vmm, CTLFLAG_RW, NULL, NULL);

int
vm_create(const char *name, struct vm **retvm)
{
	int i;
	struct vm *vm;
	vm_paddr_t maxaddr;

	const int BSP = 0;

	/*
	 * If vmm.ko could not be successfully initialized then don't attempt
	 * to create the virtual machine.
	 */
	if (!vmm_initialized)
		return (ENXIO);

	if (name == NULL || strlen(name) >= VM_MAX_NAMELEN)
		return (EINVAL);

	vm = malloc(sizeof(struct vm), M_VM, M_WAITOK | M_ZERO);
	strcpy(vm->name, name);
	vm->cookie = VMINIT(vm);

	for (i = 0; i < VM_MAXCPU; i++) {
		vcpu_init(vm, i);
		guest_msrs_init(vm, i);
	}

	maxaddr = vmm_mem_maxaddr();
	vm->iommu = iommu_create_domain(maxaddr);
	vm_activate_cpu(vm, BSP);

	*retvm = vm;
	return (0);
}

static void
vm_free_mem_seg(struct vm *vm, struct vm_memory_segment *seg)
{
	size_t len;
	vm_paddr_t hpa;
	void *host_domain;

	host_domain = iommu_host_domain();

	len = 0;
	while (len < seg->len) {
		hpa = vm_gpa2hpa(vm, seg->gpa + len, PAGE_SIZE);
		if (hpa == (vm_paddr_t)-1) {
			panic("vm_free_mem_segs: cannot free hpa "
			      "associated with gpa 0x%016lx", seg->gpa + len);
		}

		/*
		 * Remove the 'gpa' to 'hpa' mapping in VMs domain.
		 * And resurrect the 1:1 mapping for 'hpa' in 'host_domain'.
		 */
		iommu_remove_mapping(vm->iommu, seg->gpa + len, PAGE_SIZE);
		iommu_create_mapping(host_domain, hpa, hpa, PAGE_SIZE);

		vmm_mem_free(hpa, PAGE_SIZE);

		len += PAGE_SIZE;
	}

	/*
	 * Invalidate cached translations associated with 'vm->iommu' since
	 * we have now moved some pages from it.
	 */
	iommu_invalidate_tlb(vm->iommu);

	bzero(seg, sizeof(struct vm_memory_segment));
}

void
vm_destroy(struct vm *vm)
{
	int i;

	ppt_unassign_all(vm);

	for (i = 0; i < vm->num_mem_segs; i++)
		vm_free_mem_seg(vm, &vm->mem_segs[i]);

	vm->num_mem_segs = 0;

	for (i = 0; i < VM_MAXCPU; i++)
		vcpu_cleanup(&vm->vcpu[i]);

	iommu_destroy_domain(vm->iommu);

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
	const boolean_t spok = TRUE;	/* superpage mappings are ok */

	return (VMMMAP_SET(vm->cookie, gpa, hpa, len, VM_MEMATTR_UNCACHEABLE,
			   VM_PROT_RW, spok));
}

int
vm_unmap_mmio(struct vm *vm, vm_paddr_t gpa, size_t len)
{
	const boolean_t spok = TRUE;	/* superpage mappings are ok */

	return (VMMMAP_SET(vm->cookie, gpa, 0, len, 0,
			   VM_PROT_NONE, spok));
}

/*
 * Returns TRUE if 'gpa' is available for allocation and FALSE otherwise
 */
static boolean_t
vm_gpa_available(struct vm *vm, vm_paddr_t gpa)
{
	int i;
	vm_paddr_t gpabase, gpalimit;

	if (gpa & PAGE_MASK)
		panic("vm_gpa_available: gpa (0x%016lx) not page aligned", gpa);

	for (i = 0; i < vm->num_mem_segs; i++) {
		gpabase = vm->mem_segs[i].gpa;
		gpalimit = gpabase + vm->mem_segs[i].len;
		if (gpa >= gpabase && gpa < gpalimit)
			return (FALSE);
	}

	return (TRUE);
}

int
vm_malloc(struct vm *vm, vm_paddr_t gpa, size_t len)
{
	int error, available, allocated;
	struct vm_memory_segment *seg;
	vm_paddr_t g, hpa;
	void *host_domain;

	const boolean_t spok = TRUE;	/* superpage mappings are ok */

	if ((gpa & PAGE_MASK) || (len & PAGE_MASK) || len == 0)
		return (EINVAL);
	
	available = allocated = 0;
	g = gpa;
	while (g < gpa + len) {
		if (vm_gpa_available(vm, g))
			available++;
		else
			allocated++;

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

	host_domain = iommu_host_domain();

	seg = &vm->mem_segs[vm->num_mem_segs];

	error = 0;
	seg->gpa = gpa;
	seg->len = 0;
	while (seg->len < len) {
		hpa = vmm_mem_alloc(PAGE_SIZE);
		if (hpa == 0) {
			error = ENOMEM;
			break;
		}

		error = VMMMAP_SET(vm->cookie, gpa + seg->len, hpa, PAGE_SIZE,
				   VM_MEMATTR_WRITE_BACK, VM_PROT_ALL, spok);
		if (error)
			break;

		/*
		 * Remove the 1:1 mapping for 'hpa' from the 'host_domain'.
		 * Add mapping for 'gpa + seg->len' to 'hpa' in the VMs domain.
		 */
		iommu_remove_mapping(host_domain, hpa, PAGE_SIZE);
		iommu_create_mapping(vm->iommu, gpa + seg->len, hpa, PAGE_SIZE);

		seg->len += PAGE_SIZE;
	}

	if (error) {
		vm_free_mem_seg(vm, seg);
		return (error);
	}

	/*
	 * Invalidate cached translations associated with 'host_domain' since
	 * we have now moved some pages from it.
	 */
	iommu_invalidate_tlb(host_domain);

	vm->num_mem_segs++;

	return (0);
}

vm_paddr_t
vm_gpa2hpa(struct vm *vm, vm_paddr_t gpa, size_t len)
{
	vm_paddr_t nextpage;

	nextpage = rounddown(gpa + PAGE_SIZE, PAGE_SIZE);
	if (len > nextpage - gpa)
		panic("vm_gpa2hpa: invalid gpa/len: 0x%016lx/%lu", gpa, len);

	return (VMMMAP_GET(vm->cookie, gpa));
}

int
vm_gpabase2memseg(struct vm *vm, vm_paddr_t gpabase,
		  struct vm_memory_segment *seg)
{
	int i;

	for (i = 0; i < vm->num_mem_segs; i++) {
		if (gpabase == vm->mem_segs[i].gpa) {
			*seg = vm->mem_segs[i];
			return (0);
		}
	}
	return (-1);
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

int
vm_run(struct vm *vm, struct vm_run *vmrun)
{
	int error, vcpuid, sleepticks, t;
	struct vcpu *vcpu;
	struct pcb *pcb;
	uint64_t tscval, rip;
	struct vm_exit *vme;

	vcpuid = vmrun->cpuid;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	vcpu = &vm->vcpu[vcpuid];
	vme = &vmrun->vm_exit;
	rip = vmrun->rip;
restart:
	critical_enter();

	tscval = rdtsc();

	pcb = PCPU_GET(curpcb);
	set_pcb_flags(pcb, PCB_FULL_IRET);

	restore_guest_msrs(vm, vcpuid);	
	restore_guest_fpustate(vcpu);

	vcpu->hostcpu = curcpu;
	error = VMRUN(vm->cookie, vcpuid, rip);
	vcpu->hostcpu = NOCPU;

	save_guest_fpustate(vcpu);
	restore_host_msrs(vm, vcpuid);

	vmm_stat_incr(vm, vcpuid, VCPU_TOTAL_RUNTIME, rdtsc() - tscval);

	/* copy the exit information */
	bcopy(&vcpu->exitinfo, vme, sizeof(struct vm_exit));

	critical_exit();

	/*
	 * Oblige the guest's desire to 'hlt' by sleeping until the vcpu
	 * is ready to run.
	 */
	if (error == 0 && vme->exitcode == VM_EXITCODE_HLT) {
		vcpu_lock(vcpu);

		/*
		 * Figure out the number of host ticks until the next apic
		 * timer interrupt in the guest.
		 */
		sleepticks = lapic_timer_tick(vm, vcpuid);

		/*
		 * If the guest local apic timer is disabled then sleep for
		 * a long time but not forever.
		 */
		if (sleepticks < 0)
			sleepticks = hz;

		/*
		 * Do a final check for pending NMI or interrupts before
		 * really putting this thread to sleep.
		 *
		 * These interrupts could have happened any time after we
		 * returned from VMRUN() and before we grabbed the vcpu lock.
		 */
		if (!vm_nmi_pending(vm, vcpuid) &&
		    lapic_pending_intr(vm, vcpuid) < 0) {
			if (sleepticks <= 0)
				panic("invalid sleepticks %d", sleepticks);
			t = ticks;
			msleep_spin(vcpu, &vcpu->mtx, "vmidle", sleepticks);
			vmm_stat_incr(vm, vcpuid, VCPU_IDLE_TICKS, ticks - t);
		}

		vcpu_unlock(vcpu);

		rip = vme->rip + vme->inst_length;
		goto restart;
	}

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
	vm_interrupt_hostcpu(vm, vcpuid);
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
vcpu_set_state(struct vm *vm, int vcpuid, enum vcpu_state state)
{
	int error;
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_set_run_state: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	vcpu_lock(vcpu);

	/*
	 * The following state transitions are allowed:
	 * IDLE -> RUNNING -> IDLE
	 * IDLE -> CANNOT_RUN -> IDLE
	 */
	if ((vcpu->state == VCPU_IDLE && state != VCPU_IDLE) ||
	    (vcpu->state != VCPU_IDLE && state == VCPU_IDLE)) {
		error = 0;
		vcpu->state = state;
	} else {
		error = EBUSY;
	}

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

void
vm_interrupt_hostcpu(struct vm *vm, int vcpuid)
{
	int hostcpu;
	struct vcpu *vcpu;

	vcpu = &vm->vcpu[vcpuid];

	vcpu_lock(vcpu);
	hostcpu = vcpu->hostcpu;
	if (hostcpu == NOCPU) {
		/*
		 * If the vcpu is 'RUNNING' but without a valid 'hostcpu' then
		 * the host thread must be sleeping waiting for an event to
		 * kick the vcpu out of 'hlt'.
		 *
		 * XXX this is racy because the condition exists right before
		 * and after calling VMRUN() in vm_run(). The wakeup() is
		 * benign in this case.
		 */
		if (vcpu->state == VCPU_RUNNING)
			wakeup_one(vcpu);
	} else {
		if (vcpu->state != VCPU_RUNNING)
			panic("invalid vcpu state %d", vcpu->state);
		if (hostcpu != curcpu)
			ipi_cpu(hostcpu, vmm_ipinum);
	}
	vcpu_unlock(vcpu);
}
