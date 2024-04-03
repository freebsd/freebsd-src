/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __VMMAPI_INTERNAL_H__
#define	__VMMAPI_INTERNAL_H__

#include <sys/types.h>

enum {
	VM_MEMSEG_LOW,
	VM_MEMSEG_HIGH,
	VM_MEMSEG_COUNT,
};

struct vmctx {
	int	fd;
	struct {
		vm_paddr_t base;
		vm_size_t size;
	} memsegs[VM_MEMSEG_COUNT];
	int	memflags;
	char	*baseaddr;
	char	*name;
};

struct vcpu {
	struct vmctx *ctx;
	int vcpuid;
};

int	vcpu_ioctl(struct vcpu *vcpu, u_long cmd, void *arg);

extern const char *vm_capstrmap[];

#define	VM_COMMON_IOCTLS	\
	VM_RUN,			\
	VM_SUSPEND,		\
	VM_REINIT,		\
	VM_ALLOC_MEMSEG,	\
	VM_GET_MEMSEG,		\
	VM_MMAP_MEMSEG,		\
	VM_MMAP_MEMSEG,		\
	VM_MMAP_GETNEXT,	\
	VM_MUNMAP_MEMSEG,	\
	VM_SET_REGISTER,	\
	VM_GET_REGISTER,	\
	VM_SET_REGISTER_SET,	\
	VM_GET_REGISTER_SET,	\
	VM_INJECT_EXCEPTION,	\
	VM_SET_CAPABILITY,	\
	VM_GET_CAPABILITY,	\
	VM_STATS,		\
	VM_STAT_DESC,		\
	VM_GET_GPA_PMAP,	\
	VM_GLA2GPA,		\
	VM_GLA2GPA_NOFAULT,	\
	VM_ACTIVATE_CPU,	\
	VM_GET_CPUS,		\
	VM_SUSPEND_CPU,		\
	VM_RESUME_CPU,		\
	VM_SET_INTINFO,		\
	VM_GET_INTINFO,		\
	VM_RESTART_INSTRUCTION,	\
	VM_SET_TOPOLOGY,	\
	VM_GET_TOPOLOGY,	\
	VM_SNAPSHOT_REQ,	\
	VM_RESTORE_TIME

#define	VM_PPT_IOCTLS		\
	VM_BIND_PPTDEV,		\
	VM_UNBIND_PPTDEV,	\
	VM_MAP_PPTDEV_MMIO,	\
	VM_PPTDEV_MSI,		\
	VM_PPTDEV_MSIX,		\
	VM_UNMAP_PPTDEV_MMIO,	\
	VM_PPTDEV_DISABLE_MSIX

extern const cap_ioctl_t vm_ioctl_cmds[];
extern size_t vm_ioctl_ncmds;

#endif /* !__VMMAPI_INTERNAL_H__ */
