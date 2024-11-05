/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (C) 2015 Mihai Carabas <mihai.carabas@gmail.com>
 * All rights reserved.
 */

#ifndef	_DEV_VMM_DEV_H_
#define	_DEV_VMM_DEV_H_

#include <sys/types.h>
#include <sys/ioccom.h>
#include <machine/vmm_dev.h>

#ifdef _KERNEL
struct thread;
struct vm;
struct vcpu;

int	vmmdev_init(void);
int	vmmdev_cleanup(void);
int	vmmdev_machdep_ioctl(struct vm *vm, struct vcpu *vcpu, u_long cmd,
	    caddr_t data, int fflag, struct thread *td);

/*
 * Entry in an ioctl handler table.  A number of generic ioctls are defined,
 * plus a table of machine-dependent ioctls.  The flags indicate the
 * required preconditions for a given ioctl.
 *
 * Some ioctls encode a vcpuid as the first member of their ioctl structure.
 * These ioctls must specify one of the following flags:
 * - ALLOC_VCPU: create the vCPU if it does not already exist
 * - LOCK_ONE_VCPU: create the vCPU if it does not already exist
 *   and lock the vCPU for the duration of the ioctl
 * - MAYBE_ALLOC_VCPU: if the vcpuid is -1, do nothing, otherwise
 *   create the vCPU if it does not already exist
 */
struct vmmdev_ioctl {
	unsigned long	cmd;
#define	VMMDEV_IOCTL_SLOCK_MEMSEGS	0x01
#define	VMMDEV_IOCTL_XLOCK_MEMSEGS	0x02
#define	VMMDEV_IOCTL_LOCK_ONE_VCPU	0x04
#define	VMMDEV_IOCTL_LOCK_ALL_VCPUS	0x08
#define	VMMDEV_IOCTL_ALLOC_VCPU		0x10
#define	VMMDEV_IOCTL_MAYBE_ALLOC_VCPU	0x20
	int		flags;
};

#define	VMMDEV_IOCTL(_cmd, _flags)	{ .cmd = (_cmd), .flags = (_flags) }

extern const struct vmmdev_ioctl vmmdev_machdep_ioctls[];
extern const size_t vmmdev_machdep_ioctl_count;

#endif /* _KERNEL */

struct vmmctl_vm_create {
	char name[VM_MAX_NAMELEN + 1];
	int reserved[16];
};

struct vmmctl_vm_destroy {
	char name[VM_MAX_NAMELEN + 1];
	int reserved[16];
};

#define	VMMCTL_VM_CREATE	_IOWR('V', 0, struct vmmctl_vm_create)
#define	VMMCTL_VM_DESTROY	_IOWR('V', 1, struct vmmctl_vm_destroy)

#endif /* _DEV_VMM_DEV_H_ */
