/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __VMMAPI_INTERNAL_H__
#define	__VMMAPI_INTERNAL_H__

struct vmctx {
	int	fd;
	uint32_t lowmem_limit;
	int	memflags;
	size_t	lowmem;
	size_t	highmem;
	char	*baseaddr;
	char	*name;
};

struct vcpu {
	struct vmctx *ctx;
	int vcpuid;
};

int	vcpu_ioctl(struct vcpu *vcpu, u_long cmd, void *arg);

extern const char *vm_capstrmap[];

#endif /* !__VMMAPI_INTERNAL_H__ */
