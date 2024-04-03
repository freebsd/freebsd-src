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

#include <sys/types.h>
#include <sys/ioctl.h>

#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <machine/vmm_snapshot.h>

#include <assert.h>
#include <string.h>

#include "vmmapi.h"
#include "internal.h"

const char *vm_capstrmap[] = {
	[VM_CAP_MAX] = NULL,
};

#define	VM_MD_IOCTLS		\
	VM_GET_VGIC_VERSION,	\
	VM_ATTACH_VGIC,		\
	VM_ASSERT_IRQ,		\
	VM_DEASSERT_IRQ,	\
	VM_RAISE_MSI

const cap_ioctl_t vm_ioctl_cmds[] = {
	VM_COMMON_IOCTLS,
	VM_MD_IOCTLS,
};
size_t vm_ioctl_ncmds = nitems(vm_ioctl_cmds);

int
vm_attach_vgic(struct vmctx *ctx, uint64_t dist_start, size_t dist_size,
    uint64_t redist_start, size_t redist_size)
{
	struct vm_vgic_descr vgic;
	int error;

	bzero(&vgic, sizeof(vgic));
	error = ioctl(ctx->fd, VM_GET_VGIC_VERSION, &vgic.ver);
	if (error != 0)
		return (error);
	assert(vgic.ver.version == 3);
	vgic.v3_regs.dist_start = dist_start;
	vgic.v3_regs.dist_size = dist_size;
	vgic.v3_regs.redist_start = redist_start;
	vgic.v3_regs.redist_size = redist_size;

	return (ioctl(ctx->fd, VM_ATTACH_VGIC, &vgic));
}

int
vm_assert_irq(struct vmctx *ctx, uint32_t irq)
{
	struct vm_irq vi;

	bzero(&vi, sizeof(vi));
	vi.irq = irq;

	return (ioctl(ctx->fd, VM_ASSERT_IRQ, &vi));
}

int
vm_deassert_irq(struct vmctx *ctx, uint32_t irq)
{
	struct vm_irq vi;

	bzero(&vi, sizeof(vi));
	vi.irq = irq;

	return (ioctl(ctx->fd, VM_DEASSERT_IRQ, &vi));
}

int
vm_raise_msi(struct vmctx *ctx, uint64_t addr, uint64_t msg, int bus, int slot,
    int func)
{
	struct vm_msi vmsi;

	bzero(&vmsi, sizeof(vmsi));
	vmsi.addr = addr;
	vmsi.msg = msg;
	vmsi.bus = bus;
	vmsi.slot = slot;
	vmsi.func = func;

	return (ioctl(ctx->fd, VM_RAISE_MSI, &vmsi));
}

int
vm_inject_exception(struct vcpu *vcpu, uint64_t esr, uint64_t far)
{
	struct vm_exception vmexc;

	bzero(&vmexc, sizeof(vmexc));
	vmexc.esr = esr;
	vmexc.far = far;

	return (vcpu_ioctl(vcpu, VM_INJECT_EXCEPTION, &vmexc));
}
