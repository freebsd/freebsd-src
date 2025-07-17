/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Arm Ltd
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include "vgic.h"
#include "vgic_if.h"

device_t vgic_dev;

bool
vgic_present(void)
{
	return (vgic_dev != NULL);
}

void
vgic_init(void)
{
	VGIC_INIT(vgic_dev);
}

int
vgic_attach_to_vm(struct hyp *hyp, struct vm_vgic_descr *descr)
{
	return (VGIC_ATTACH_TO_VM(vgic_dev, hyp, descr));
}

void
vgic_detach_from_vm(struct hyp *hyp)
{
	VGIC_DETACH_FROM_VM(vgic_dev, hyp);
}

void
vgic_vminit(struct hyp *hyp)
{
	VGIC_VMINIT(vgic_dev, hyp);
}

void
vgic_cpuinit(struct hypctx *hypctx)
{
	VGIC_CPUINIT(vgic_dev, hypctx);
}

void
vgic_cpucleanup(struct hypctx *hypctx)
{
	VGIC_CPUCLEANUP(vgic_dev, hypctx);
}

void
vgic_vmcleanup(struct hyp *hyp)
{
	VGIC_VMCLEANUP(vgic_dev, hyp);
}

int
vgic_max_cpu_count(struct hyp *hyp)
{
	return (VGIC_MAX_CPU_COUNT(vgic_dev, hyp));
}

bool
vgic_has_pending_irq(struct hypctx *hypctx)
{
	return (VGIC_HAS_PENDING_IRQ(vgic_dev, hypctx));
}

/* TODO: vcpuid -> hypctx ? */
/* TODO: Add a vgic interface */
int
vgic_inject_irq(struct hyp *hyp, int vcpuid, uint32_t irqid, bool level)
{
	return (VGIC_INJECT_IRQ(vgic_dev, hyp, vcpuid, irqid, level));
}

int
vgic_inject_msi(struct hyp *hyp, uint64_t msg, uint64_t addr)
{
	return (VGIC_INJECT_MSI(vgic_dev, hyp, msg, addr));
}

void
vgic_flush_hwstate(struct hypctx *hypctx)
{
	VGIC_FLUSH_HWSTATE(vgic_dev, hypctx);
}

void
vgic_sync_hwstate(struct hypctx *hypctx)
{
	VGIC_SYNC_HWSTATE(vgic_dev, hypctx);
}
