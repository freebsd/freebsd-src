/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

#include <machine/smp.h>

#include <dev/vmm/vmm_vm.h>

SYSCTL_NODE(_hw, OID_AUTO, vmm, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, NULL);

int vmm_ipinum;
SYSCTL_INT(_hw_vmm, OID_AUTO, ipinum, CTLFLAG_RD, &vmm_ipinum, 0,
    "IPI vector used for vcpu notifications");

/*
 * Invoke the rendezvous function on the specified vcpu if applicable.  Return
 * true if the rendezvous is finished, false otherwise.
 */
static bool
vm_rendezvous(struct vcpu *vcpu)
{
	struct vm *vm = vcpu->vm;
	int vcpuid;

	mtx_assert(&vcpu->vm->rendezvous_mtx, MA_OWNED);
	KASSERT(vcpu->vm->rendezvous_func != NULL,
	    ("vm_rendezvous: no rendezvous pending"));

	/* 'rendezvous_req_cpus' must be a subset of 'active_cpus' */
	CPU_AND(&vm->rendezvous_req_cpus, &vm->rendezvous_req_cpus,
	    &vm->active_cpus);

	vcpuid = vcpu->vcpuid;
	if (CPU_ISSET(vcpuid, &vm->rendezvous_req_cpus) &&
	    !CPU_ISSET(vcpuid, &vm->rendezvous_done_cpus)) {
		(*vm->rendezvous_func)(vcpu, vm->rendezvous_arg);
		CPU_SET(vcpuid, &vm->rendezvous_done_cpus);
	}
	if (CPU_CMP(&vm->rendezvous_req_cpus, &vm->rendezvous_done_cpus) == 0) {
		CPU_ZERO(&vm->rendezvous_req_cpus);
		vm->rendezvous_func = NULL;
		wakeup(&vm->rendezvous_func);
		return (true);
	}
	return (false);
}

int
vm_handle_rendezvous(struct vcpu *vcpu)
{
	struct vm *vm;
	struct thread *td;

	td = curthread;
	vm = vcpu->vm;

	mtx_lock(&vm->rendezvous_mtx);
	while (vm->rendezvous_func != NULL) {
		if (vm_rendezvous(vcpu))
			break;

		mtx_sleep(&vm->rendezvous_func, &vm->rendezvous_mtx, 0,
		    "vmrndv", hz);
		if (td_ast_pending(td, TDA_SUSPEND)) {
			int error;

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

static void
vcpu_wait_idle(struct vcpu *vcpu)
{
	KASSERT(vcpu->state != VCPU_IDLE, ("vcpu already idle"));

	vcpu->reqidle = 1;
	vcpu_notify_event_locked(vcpu);
	msleep_spin(&vcpu->state, &vcpu->mtx, "vmstat", hz);
}

int
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
			vcpu_wait_idle(vcpu);
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

/*
 * Try to lock all of the vCPUs in the VM while taking care to avoid deadlocks
 * with vm_smp_rendezvous().
 *
 * The complexity here suggests that the rendezvous mechanism needs a rethink.
 */
int
vcpu_set_state_all(struct vm *vm, enum vcpu_state newstate)
{
	cpuset_t locked;
	struct vcpu *vcpu;
	int error, i;
	uint16_t maxcpus;

	KASSERT(newstate != VCPU_IDLE,
	    ("vcpu_set_state_all: invalid target state %d", newstate));

	error = 0;
	CPU_ZERO(&locked);
	maxcpus = vm->maxcpus;

	mtx_lock(&vm->rendezvous_mtx);
restart:
	if (vm->rendezvous_func != NULL) {
		/*
		 * If we have a pending rendezvous, then the initiator may be
		 * blocked waiting for other vCPUs to execute the callback.  The
		 * current thread may be a vCPU thread so we must not block
		 * waiting for the initiator, otherwise we get a deadlock.
		 * Thus, execute the callback on behalf of any idle vCPUs.
		 */
		for (i = 0; i < maxcpus; i++) {
			vcpu = vm_vcpu(vm, i);
			if (vcpu == NULL)
				continue;
			vcpu_lock(vcpu);
			if (vcpu->state == VCPU_IDLE) {
				(void)vcpu_set_state_locked(vcpu, VCPU_FROZEN,
				    true);
				CPU_SET(i, &locked);
			}
			if (CPU_ISSET(i, &locked)) {
				/*
				 * We can safely execute the callback on this
				 * vCPU's behalf.
				 */
				vcpu_unlock(vcpu);
				(void)vm_rendezvous(vcpu);
				vcpu_lock(vcpu);
			}
			vcpu_unlock(vcpu);
		}
	}

	/*
	 * Now wait for remaining vCPUs to become idle.  This may include the
	 * initiator of a rendezvous that is currently blocked on the rendezvous
	 * mutex.
	 */
	CPU_FOREACH_ISCLR(i, &locked) {
		if (i >= maxcpus)
			break;
		vcpu = vm_vcpu(vm, i);
		if (vcpu == NULL)
			continue;
		vcpu_lock(vcpu);
		while (vcpu->state != VCPU_IDLE) {
			mtx_unlock(&vm->rendezvous_mtx);
			vcpu_wait_idle(vcpu);
			vcpu_unlock(vcpu);
			mtx_lock(&vm->rendezvous_mtx);
			if (vm->rendezvous_func != NULL)
				goto restart;
			vcpu_lock(vcpu);
		}
		error = vcpu_set_state_locked(vcpu, newstate, true);
		vcpu_unlock(vcpu);
		if (error != 0) {
			/* Roll back state changes. */
			CPU_FOREACH_ISSET(i, &locked)
				(void)vcpu_set_state(vcpu, VCPU_IDLE, false);
			break;
		}
		CPU_SET(i, &locked);
	}
	mtx_unlock(&vm->rendezvous_mtx);
	return (error);
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

/*
 * This function is called to ensure that a vcpu "sees" a pending event
 * as soon as possible:
 * - If the vcpu thread is sleeping then it is woken up.
 * - If the vcpu is running on a different host_cpu then an IPI will be directed
 *   to the host_cpu to cause the vcpu to trap into the hypervisor.
 */
void
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

int
vcpu_debugged(struct vcpu *vcpu)
{
	return (CPU_ISSET(vcpu->vcpuid, &vcpu->vm->debug_cpus));
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

void
vm_disable_vcpu_creation(struct vm *vm)
{
	sx_xlock(&vm->vcpus_init_lock);
	vm->dying = true;
	sx_xunlock(&vm->vcpus_init_lock);
}

uint16_t
vm_get_maxcpus(struct vm *vm)
{
	return (vm->maxcpus);
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

int
vm_set_topology(struct vm *vm, uint16_t sockets, uint16_t cores,
    uint16_t threads, uint16_t maxcpus __unused)
{
	/* Ignore maxcpus. */
	if (sockets * cores * threads > vm->maxcpus)
		return (EINVAL);
	vm->sockets = sockets;
	vm->cores = cores;
	vm->threads = threads;
	return (0);
}

int
vm_suspend(struct vm *vm, enum vm_suspend_how how)
{
	int i;

	if (how <= VM_SUSPEND_NONE || how >= VM_SUSPEND_LAST)
		return (EINVAL);

	if (atomic_cmpset_int(&vm->suspend, 0, how) == 0)
		return (EALREADY);

	/*
	 * Notify all active vcpus that they are now suspended.
	 */
	for (i = 0; i < vm->maxcpus; i++) {
		if (CPU_ISSET(i, &vm->active_cpus))
			vcpu_notify_event(vm_vcpu(vm, i));
	}

	return (0);
}

int
vm_reinit(struct vm *vm)
{
	int error;

	/*
	 * A virtual machine can be reset only if all vcpus are suspended.
	 */
	if (CPU_CMP(&vm->suspended_cpus, &vm->active_cpus) == 0) {
		vm_reset(vm);
		error = 0;
	} else {
		error = EBUSY;
	}

	return (error);
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
