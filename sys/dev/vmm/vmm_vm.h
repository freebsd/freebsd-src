/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 */

#ifndef _DEV_VMM_VM_H_
#define	_DEV_VMM_VM_H_

#ifdef _KERNEL

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

int vcpu_set_state(struct vcpu *vcpu, enum vcpu_state state, bool from_idle);
#ifdef __amd64__
int vcpu_set_state_all(struct vm *vm, enum vcpu_state state);
#endif
enum vcpu_state vcpu_get_state(struct vcpu *vcpu, int *hostcpu);

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

#endif /* _KERNEL */

#endif /* !_DEV_VMM_VM_H_ */
