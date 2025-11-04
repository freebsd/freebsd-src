/*-
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpuset.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
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

#include <machine/riscvreg.h>
#include <machine/cpu.h>
#include <machine/fpe.h>
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

#include "vmm_stat.h"
#include "riscv.h"

#include "vmm_aplic.h"

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
	struct fpreg	*guestfpu;	/* (a,i) guest fpu state */
};

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

/*
 * Initialization:
 * (o) initialized the first time the VM is created
 * (i) initialized when VM is created and when it is reinitialized
 * (x) initialized before use
 */
struct vm {
	void		*cookie;		/* (i) cpu-specific data */
	volatile cpuset_t active_cpus;		/* (i) active vcpus */
	volatile cpuset_t debug_cpus;		/* (i) vcpus stopped for debug*/
	int		suspend;		/* (i) stop VM execution */
	bool		dying;			/* (o) is dying */
	volatile cpuset_t suspended_cpus; 	/* (i) suspended vcpus */
	volatile cpuset_t halted_cpus;		/* (x) cpus in a hard halt */
	struct vm_mem	mem;			/* (i) [m+v] guest memory */
	char		name[VM_MAX_NAMELEN + 1]; /* (o) virtual machine name */
	struct vcpu	**vcpu;			/* (i) guest vcpus */
	struct vmm_mmio_region mmio_region[VM_MAX_MMIO_REGIONS];
						/* (o) guest MMIO regions */
	/* The following describe the vm cpu topology */
	uint16_t	sockets;		/* (o) num of sockets */
	uint16_t	cores;			/* (o) num of cores/socket */
	uint16_t	threads;		/* (o) num of threads/core */
	uint16_t	maxcpus;		/* (o) max pluggable cpus */
	struct sx	vcpus_init_lock;	/* (o) */
};

static MALLOC_DEFINE(M_VMM, "vmm", "vmm");

/* statistics */
static VMM_STAT(VCPU_TOTAL_RUNTIME, "vcpu total runtime");

SYSCTL_NODE(_hw, OID_AUTO, vmm, CTLFLAG_RW, NULL, NULL);

static int vmm_ipinum;
SYSCTL_INT(_hw_vmm, OID_AUTO, ipinum, CTLFLAG_RD, &vmm_ipinum, 0,
    "IPI vector used for vcpu notifications");

static void vcpu_notify_event_locked(struct vcpu *vcpu);

/* global statistics */
VMM_STAT(VMEXIT_COUNT, "total number of vm exits");
VMM_STAT(VMEXIT_IRQ, "number of vmexits for an irq");
VMM_STAT(VMEXIT_UNHANDLED, "number of vmexits for an unhandled exception");

static void
vcpu_cleanup(struct vcpu *vcpu, bool destroy)
{
	vmmops_vcpu_cleanup(vcpu->cookie);
	vcpu->cookie = NULL;
	if (destroy) {
		vmm_stat_free(vcpu->stats);
		fpu_save_area_free(vcpu->guestfpu);
		vcpu_lock_destroy(vcpu);
		free(vcpu, M_VMM);
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

int
vmm_modinit(void)
{
	return (vmmops_modinit());
}

int
vmm_modcleanup(void)
{
	return (vmmops_modcleanup());
}

static void
vm_init(struct vm *vm, bool create)
{
	int i;

	vm->cookie = vmmops_init(vm, vmspace_pmap(vm_vmspace(vm)));
	MPASS(vm->cookie != NULL);

	CPU_ZERO(&vm->active_cpus);
	CPU_ZERO(&vm->debug_cpus);

	vm->suspend = 0;
	CPU_ZERO(&vm->suspended_cpus);

	memset(vm->mmio_region, 0, sizeof(vm->mmio_region));

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
vm_lock_vcpus(struct vm *vm)
{
	sx_xlock(&vm->vcpus_init_lock);
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
	int error;

	vm = malloc(sizeof(struct vm), M_VMM, M_WAITOK | M_ZERO);
	error = vm_mem_init(&vm->mem, 0, 1ul << 39);
	if (error != 0) {
		free(vm, M_VMM);
		return (error);
	}
	strcpy(vm->name, name);
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
	int i;

	if (destroy)
		vm_xlock_memsegs(vm);
	else
		vm_assert_memseg_xlocked(vm);

	aplic_detach_from_vm(vm->cookie);

	for (i = 0; i < vm->maxcpus; i++) {
		if (vm->vcpu[i] != NULL)
			vcpu_cleanup(vm->vcpu[i], destroy);
	}

	vmmops_cleanup(vm->cookie);

	vm_mem_cleanup(vm);
	if (destroy) {
		vm_mem_destroy(vm);

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
	if (!hyp->aplic_attached)
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

struct vm_mem *
vm_mem(struct vm *vm)
{
	return (&vm->mem);
}

static void
restore_guest_fpustate(struct vcpu *vcpu)
{

	/* Flush host state to the pcb. */
	fpe_state_save(curthread);

	/* Ensure the VFP state will be re-loaded when exiting the guest. */
	PCPU_SET(fpcurthread, NULL);

	/* restore guest FPU state */
	fpe_enable();
	fpe_restore(vcpu->guestfpu);

	/*
	 * The FPU is now "dirty" with the guest's state so turn on emulation
	 * to trap any access to the FPU by the host.
	 */
	fpe_disable();
}

static void
save_guest_fpustate(struct vcpu *vcpu)
{

	/* Save guest FPE state. */
	fpe_enable();
	fpe_store(vcpu->guestfpu);
	fpe_disable();

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
	if (reg < 0 || reg >= VM_REG_LAST)
		return (EINVAL);

	return (vmmops_getreg(vcpu->cookie, reg, retval));
}

int
vm_set_register(struct vcpu *vcpu, int reg, uint64_t val)
{
	int error;

	if (reg < 0 || reg >= VM_REG_LAST)
		return (EINVAL);
	error = vmmops_setreg(vcpu->cookie, reg, val);
	if (error || reg != VM_REG_GUEST_SEPC)
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
vm_inject_exception(struct vcpu *vcpu, uint64_t scause)
{

	return (vmmops_exception(vcpu->cookie, scause));
}

int
vm_attach_aplic(struct vm *vm, struct vm_aplic_descr *descr)
{

	return (aplic_attach_to_vm(vm->cookie, descr));
}

int
vm_assert_irq(struct vm *vm, uint32_t irq)
{

	return (aplic_inject_irq(vm->cookie, -1, irq, true));
}

int
vm_deassert_irq(struct vm *vm, uint32_t irq)
{

	return (aplic_inject_irq(vm->cookie, -1, irq, false));
}

int
vm_raise_msi(struct vm *vm, uint64_t msg, uint64_t addr, int bus, int slot,
    int func)
{

	return (aplic_inject_msi(vm->cookie, msg, addr));
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

		if (aplic_check_pending(vcpu->cookie))
			break;

		if (riscv_check_ipi(vcpu->cookie, false))
			break;

		if (riscv_check_interrupts_pending(vcpu->cookie))
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
	struct vm *vm;
	struct vm_exit *vme;
	struct vm_map *map;
	uint64_t addr;
	pmap_t pmap;
	int ftype, rv;

	vm = vcpu->vm;
	vme = &vcpu->exitinfo;

	pmap = vmspace_pmap(vm_vmspace(vm));
	addr = (vme->htval << 2) & ~(PAGE_SIZE - 1);

	dprintf("%s: %lx\n", __func__, addr);

	switch (vme->scause) {
	case SCAUSE_STORE_GUEST_PAGE_FAULT:
		ftype = VM_PROT_WRITE;
		break;
	case SCAUSE_FETCH_GUEST_PAGE_FAULT:
		ftype = VM_PROT_EXECUTE;
		break;
	case SCAUSE_LOAD_GUEST_PAGE_FAULT:
		ftype = VM_PROT_READ;
		break;
	default:
		panic("unknown page trap: %lu", vme->scause);
	}

	/* The page exists, but the page table needs to be updated. */
	if (pmap_fault(pmap, addr, ftype))
		return (0);

	map = &vm_vmspace(vm)->vm_map;
	rv = vm_fault(map, addr, ftype, VM_FAULT_NORMAL, NULL);
	if (rv != KERN_SUCCESS) {
		printf("%s: vm_fault failed, addr %lx, ftype %d, err %d\n",
		    __func__, addr, ftype, rv);
		return (EFAULT);
	}

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
	struct vm_eventinfo evinfo;
	struct vm_exit *vme;
	struct vm *vm;
	pmap_t pmap;
	int error;
	int vcpuid;
	bool retu;

	vm = vcpu->vm;

	dprintf("%s\n", __func__);

	vcpuid = vcpu->vcpuid;

	if (!CPU_ISSET(vcpuid, &vm->active_cpus))
		return (EINVAL);

	if (CPU_ISSET(vcpuid, &vm->suspended_cpus))
		return (EINVAL);

	pmap = vmspace_pmap(vm_vmspace(vm));
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
		case VM_EXITCODE_WFI:
			vcpu->nextpc = vme->pc + vme->inst_length;
			error = vm_handle_wfi(vcpu, vme, &retu);
			break;
		case VM_EXITCODE_ECALL:
			/* Handle in userland. */
			vcpu->nextpc = vme->pc + vme->inst_length;
			retu = true;
			break;
		case VM_EXITCODE_PAGING:
			vcpu->nextpc = vme->pc;
			error = vm_handle_paging(vcpu, &retu);
			break;
		case VM_EXITCODE_BOGUS:
			vcpu->nextpc = vme->pc;
			retu = false;
			error = 0;
			break;
		case VM_EXITCODE_SUSPENDED:
			vcpu->nextpc = vme->pc;
			error = vm_handle_suspend(vcpu, &retu);
			break;
		default:
			/* Handle in userland. */
			vcpu->nextpc = vme->pc;
			retu = true;
			break;
		}
	}

	if (error == 0 && retu == false)
		goto restart;

	return (error);
}
