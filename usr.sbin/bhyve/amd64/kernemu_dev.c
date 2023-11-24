/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Conrad Meyer <cem@FreeBSD.org>.  All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/tree.h>

#include <machine/vmm.h>
#include <x86/include/apicreg.h>
struct vm;
struct vm_hpet_cap;
#include <vmm/io/vioapic.h>
#include <vmm/io/vhpet.h>

#include <err.h>
#include <errno.h>
#include <vmmapi.h>

#include "kernemu_dev.h"
#include "mem.h"

static int
apic_handler(struct vcpu *vcpu, int dir, uint64_t addr, int size,
    uint64_t *val, void *arg1 __unused, long arg2 __unused)
{
	if (vm_readwrite_kernemu_device(vcpu, addr, (dir == MEM_F_WRITE),
	    size, val) != 0)
		return (errno);
	return (0);
}

static struct mem_range lapic_mmio = {
	.name = "kern-lapic-mmio",
	.base = DEFAULT_APIC_BASE,
	.size = PAGE_SIZE,
	.flags = MEM_F_RW | MEM_F_IMMUTABLE,
	.handler = apic_handler,

};
static struct mem_range ioapic_mmio = {
	.name = "kern-ioapic-mmio",
	.base = VIOAPIC_BASE,
	.size = VIOAPIC_SIZE,
	.flags = MEM_F_RW | MEM_F_IMMUTABLE,
	.handler = apic_handler,
};
static struct mem_range hpet_mmio = {
	.name = "kern-hpet-mmio",
	.base = VHPET_BASE,
	.size = VHPET_SIZE,
	.flags = MEM_F_RW | MEM_F_IMMUTABLE,
	.handler = apic_handler,
};

void
kernemu_dev_init(void)
{
	int rc;

	rc = register_mem(&lapic_mmio);
	if (rc != 0)
		errc(4, rc, "register_mem: LAPIC (0x%08x)",
		    (unsigned)lapic_mmio.base);
	rc = register_mem(&ioapic_mmio);
	if (rc != 0)
		errc(4, rc, "register_mem: IOAPIC (0x%08x)",
		    (unsigned)ioapic_mmio.base);
	rc = register_mem(&hpet_mmio);
	if (rc != 0)
		errc(4, rc, "register_mem: HPET (0x%08x)",
		    (unsigned)hpet_mmio.base);
}
