/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 */

#ifndef _DEV_VMM_VM_H_
#define	_DEV_VMM_VM_H_

#ifdef _KERNEL
#include <sys/_cpuset.h>

#include <machine/vmm.h>

#include <dev/vmm/vmm_param.h>
#include <dev/vmm/vmm_mem.h>

struct vcpu;

enum vcpu_state {
	VCPU_IDLE,
	VCPU_FROZEN,
	VCPU_RUNNING,
	VCPU_SLEEPING,
};

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
	void		*stats;		/* (a,i) statistics */

	VMM_VCPU_MD_FIELDS;
};

#define	vcpu_lock_init(v)	mtx_init(&((v)->mtx), "vcpu lock", 0, MTX_SPIN)
#define	vcpu_lock_destroy(v)	mtx_destroy(&((v)->mtx))
#define	vcpu_lock(v)		mtx_lock_spin(&((v)->mtx))
#define	vcpu_unlock(v)		mtx_unlock_spin(&((v)->mtx))
#define	vcpu_assert_locked(v)	mtx_assert(&((v)->mtx), MA_OWNED)

extern int vmm_ipinum;

int vcpu_set_state(struct vcpu *vcpu, enum vcpu_state state, bool from_idle);
int vcpu_set_state_locked(struct vcpu *vcpu, enum vcpu_state newstate,
    bool from_idle);
int vcpu_set_state_all(struct vm *vm, enum vcpu_state state);
enum vcpu_state vcpu_get_state(struct vcpu *vcpu, int *hostcpu);
void vcpu_notify_event(struct vcpu *vcpu);
void vcpu_notify_event_locked(struct vcpu *vcpu);
int vcpu_debugged(struct vcpu *vcpu);

static inline void *
vcpu_stats(struct vcpu *vcpu)
{
	return (vcpu->stats);
}

static inline struct vm *
vcpu_vm(struct vcpu *vcpu)
{
	return (vcpu->vm);
}

static inline int
vcpu_vcpuid(struct vcpu *vcpu)
{
	return (vcpu->vcpuid);
}

static int __inline
vcpu_is_running(struct vcpu *vcpu, int *hostcpu)
{
	return (vcpu_get_state(vcpu, hostcpu) == VCPU_RUNNING);
}

#ifdef _SYS_PROC_H_
static int __inline
vcpu_should_yield(struct vcpu *vcpu)
{
	struct thread *td;

	td = curthread;
	return (td->td_ast != 0 || td->td_owepreempt != 0);
}
#endif

typedef void (*vm_rendezvous_func_t)(struct vcpu *vcpu, void *arg);
int vm_handle_rendezvous(struct vcpu *vcpu);

/*
 * Rendezvous all vcpus specified in 'dest' and execute 'func(arg)'.
 * The rendezvous 'func(arg)' is not allowed to do anything that will
 * cause the thread to be put to sleep.
 *
 * The caller cannot hold any locks when initiating the rendezvous.
 *
 * The implementation of this API may cause vcpus other than those specified
 * by 'dest' to be stalled. The caller should not rely on any vcpus making
 * forward progress when the rendezvous is in progress.
 */
int vm_smp_rendezvous(struct vcpu *vcpu, cpuset_t dest,
    vm_rendezvous_func_t func, void *arg);

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
	struct vcpu	**vcpu;			/* (o) guest vcpus */
	struct vm_mem	mem;			/* (i) [m+v] guest memory */

	char		name[VM_MAX_NAMELEN + 1]; /* (o) virtual machine name */
	struct sx	vcpus_init_lock;	/* (o) */

	bool		dying;			/* (o) is dying */
	int		suspend;		/* (i) stop VM execution */

	volatile cpuset_t active_cpus;		/* (i) active vcpus */
	volatile cpuset_t debug_cpus;		/* (i) vcpus stopped for debug */
	volatile cpuset_t suspended_cpus; 	/* (i) suspended vcpus */
	volatile cpuset_t halted_cpus;		/* (x) cpus in a hard halt */

	cpuset_t	rendezvous_req_cpus;	/* (x) [r] rendezvous requested */
	cpuset_t	rendezvous_done_cpus;	/* (x) [r] rendezvous finished */
	void		*rendezvous_arg;	/* (x) [r] rendezvous func/arg */
	vm_rendezvous_func_t rendezvous_func;
	struct mtx	rendezvous_mtx;		/* (o) rendezvous lock */

	uint16_t	sockets;		/* (o) num of sockets */
	uint16_t	cores;			/* (o) num of cores/socket */
	uint16_t	threads;		/* (o) num of threads/core */
	uint16_t	maxcpus;		/* (o) max pluggable cpus */

	VMM_VM_MD_FIELDS;
};

int vm_create(const char *name, struct vm **retvm);
struct vcpu *vm_alloc_vcpu(struct vm *vm, int vcpuid);
void vm_destroy(struct vm *vm);
int vm_reinit(struct vm *vm);
void vm_reset(struct vm *vm);

void vm_lock_vcpus(struct vm *vm);
void vm_unlock_vcpus(struct vm *vm);
void vm_disable_vcpu_creation(struct vm *vm);

int vm_suspend(struct vm *vm, enum vm_suspend_how how);
int vm_activate_cpu(struct vcpu *vcpu);
int vm_suspend_cpu(struct vm *vm, struct vcpu *vcpu);
int vm_resume_cpu(struct vm *vm, struct vcpu *vcpu);
 
cpuset_t vm_active_cpus(struct vm *vm);
cpuset_t vm_debug_cpus(struct vm *vm);
cpuset_t vm_suspended_cpus(struct vm *vm);

uint16_t vm_get_maxcpus(struct vm *vm);
void vm_get_topology(struct vm *vm, uint16_t *sockets, uint16_t *cores,
    uint16_t *threads, uint16_t *maxcpus);
int vm_set_topology(struct vm *vm, uint16_t sockets, uint16_t cores,
    uint16_t threads, uint16_t maxcpus);

static inline const char *
vm_name(struct vm *vm)
{
	return (vm->name);
}

static inline struct vm_mem *
vm_mem(struct vm *vm)
{
	return (&vm->mem);
}

static inline struct vcpu *
vm_vcpu(struct vm *vm, int vcpuid)
{
	return (vm->vcpu[vcpuid]);
}

struct vm_eventinfo {
	cpuset_t *rptr;		/* rendezvous cookie */
	int	*sptr;		/* suspend cookie */
	int	*iptr;		/* reqidle cookie */
};

static inline int
vcpu_rendezvous_pending(struct vcpu *vcpu, struct vm_eventinfo *info)
{
	/*
	 * This check isn't done with atomic operations or under a lock because
	 * there's no need to. If the vcpuid bit is set, the vcpu is part of a
	 * rendezvous and the bit won't be cleared until the vcpu enters the
	 * rendezvous. On rendezvous exit, the cpuset is cleared and the vcpu
	 * will see an empty cpuset. So, the races are harmless.
	 */
	return (CPU_ISSET(vcpu_vcpuid(vcpu), info->rptr));
}

static inline int
vcpu_suspended(struct vm_eventinfo *info)
{
	return (*info->sptr);
}

static inline int
vcpu_reqidle(struct vm_eventinfo *info)
{
	return (*info->iptr);
}
#endif /* _KERNEL */

#endif /* !_DEV_VMM_VM_H_ */
