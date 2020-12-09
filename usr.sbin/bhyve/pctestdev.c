/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Adam Fenn <adam@fenn.io>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Emulation of selected legacy test/debug interfaces expected by KVM-unit-tests
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/mman.h>
#include <machine/vmm.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vmmapi.h>

#include "debug.h"
#include "inout.h"
#include "mem.h"
#include "pctestdev.h"

#define	DEBUGEXIT_BASE		0xf4
#define	DEBUGEXIT_LEN		4
#define	DEBUGEXIT_NAME		"isa-debug-exit"

#define	IOMEM_BASE		0xff000000
#define	IOMEM_LEN		0x10000
#define	IOMEM_NAME		"pc-testdev-iomem"

#define	IOPORT_BASE		0xe0
#define	IOPORT_LEN		4
#define	IOPORT_NAME		"pc-testdev-ioport"

#define	IRQ_BASE		0x2000
#define	IRQ_IOAPIC_PINCOUNT_MIN	24
#define	IRQ_IOAPIC_PINCOUNT_MAX	32
#define	IRQ_NAME		"pc-testdev-irq-line"

#define	PCTESTDEV_NAME		"pc-testdev"

static bool	pctestdev_inited;
static uint8_t	pctestdev_iomem_buf[IOMEM_LEN];
static uint32_t	pctestdev_ioport_data;

static int	pctestdev_debugexit_io(struct vmctx *ctx, int vcpu, int in,
		    int port, int bytes, uint32_t *eax, void *arg);
static int	pctestdev_iomem_io(struct vmctx *ctx, int vcpu, int dir,
		    uint64_t addr, int size, uint64_t *val, void *arg1,
		    long arg2);
static int	pctestdev_ioport_io(struct vmctx *ctx, int vcpu, int in,
		    int port, int bytes, uint32_t *eax, void *arg);
static int	pctestdev_irq_io(struct vmctx *ctx, int vcpu, int in,
		    int port, int bytes, uint32_t *eax, void *arg);

const char *
pctestdev_getname(void)
{
	return (PCTESTDEV_NAME);
}

int
pctestdev_parse(const char *opts)
{
	if (opts != NULL && *opts != '\0')
		return (-1);

	return (0);
}

int
pctestdev_init(struct vmctx *ctx)
{
	struct mem_range iomem;
	struct inout_port debugexit, ioport, irq;
	int err, pincount;

	if (pctestdev_inited) {
		EPRINTLN("Only one pc-testdev device is allowed.");

		return (-1);
	}

	err = vm_ioapic_pincount(ctx, &pincount);
	if (err != 0) {
		EPRINTLN("pc-testdev: Failed to obtain IOAPIC pin count.");

		return (-1);
	}
	if (pincount < IRQ_IOAPIC_PINCOUNT_MIN ||
	    pincount > IRQ_IOAPIC_PINCOUNT_MAX) {
		EPRINTLN("pc-testdev: Unsupported IOAPIC pin count: %d.",
		    pincount);

		return (-1);
	}

	debugexit.name = DEBUGEXIT_NAME;
	debugexit.port = DEBUGEXIT_BASE;
	debugexit.size = DEBUGEXIT_LEN;
	debugexit.flags = IOPORT_F_INOUT;
	debugexit.handler = pctestdev_debugexit_io;
	debugexit.arg = NULL;

	iomem.name = IOMEM_NAME;
	iomem.flags = MEM_F_RW | MEM_F_IMMUTABLE;
	iomem.handler = pctestdev_iomem_io;
	iomem.arg1 = NULL;
	iomem.arg2 = 0;
	iomem.base = IOMEM_BASE;
	iomem.size = IOMEM_LEN;

	ioport.name = IOPORT_NAME;
	ioport.port = IOPORT_BASE;
	ioport.size = IOPORT_LEN;
	ioport.flags = IOPORT_F_INOUT;
	ioport.handler = pctestdev_ioport_io;
	ioport.arg = NULL;

	irq.name = IRQ_NAME;
	irq.port = IRQ_BASE;
	irq.size = pincount;
	irq.flags = IOPORT_F_INOUT;
	irq.handler = pctestdev_irq_io;
	irq.arg = NULL;

	err = register_inout(&debugexit);
	if (err != 0)
		goto fail;

	err = register_inout(&ioport);
	if (err != 0)
		goto fail_after_debugexit_reg;

	err = register_inout(&irq);
	if (err != 0)
		goto fail_after_ioport_reg;

	err = register_mem(&iomem);
	if (err != 0)
		goto fail_after_irq_reg;

	pctestdev_inited = true;

	return (0);

fail_after_irq_reg:
	(void)unregister_inout(&irq);

fail_after_ioport_reg:
	(void)unregister_inout(&ioport);

fail_after_debugexit_reg:
	(void)unregister_inout(&debugexit);

fail:
	return (err);
}

static int
pctestdev_debugexit_io(struct vmctx *ctx, int vcpu, int in, int port,
    int bytes, uint32_t *eax, void *arg)
{
	if (in)
		*eax = 0;
	else
		exit((*eax << 1) | 1);

	return (0);
}

static int
pctestdev_iomem_io(struct vmctx *ctx, int vcpu, int dir, uint64_t addr,
    int size, uint64_t *val, void *arg1, long arg2)
{
	uint64_t offset;

	if (addr + size > IOMEM_BASE + IOMEM_LEN)
		return (-1);

	offset = addr - IOMEM_BASE;
	if (dir == MEM_F_READ) {
		(void)memcpy(val, pctestdev_iomem_buf + offset, size);
	} else {
		assert(dir == MEM_F_WRITE);
		(void)memcpy(pctestdev_iomem_buf + offset, val, size);
	}

	return (0);
}

static int
pctestdev_ioport_io(struct vmctx *ctx, int vcpu, int in, int port,
    int bytes, uint32_t *eax, void *arg)
{
	uint32_t mask;
	int lsb;

	if (port + bytes > IOPORT_BASE + IOPORT_LEN)
		return (-1);

	lsb = (port & 0x3) * 8;
	mask = (-1UL >> (32 - (bytes * 8))) << lsb;

	if (in)
		*eax = (pctestdev_ioport_data & mask) >> lsb;
	else {
		pctestdev_ioport_data &= ~mask;
		pctestdev_ioport_data |= *eax << lsb;
	}

	return (0);
}

static int
pctestdev_irq_io(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
    uint32_t *eax, void *arg)
{
	int irq;

	if (bytes != 1)
		return (-1);

	if (in) {
		*eax = 0;
		return (0);
	} else {
		irq = port - IRQ_BASE;
		if (irq < 16) {
			if (*eax)
				return (vm_isa_assert_irq(ctx, irq, irq));
			else
				return (vm_isa_deassert_irq(ctx, irq, irq));
		} else {
			if (*eax)
				return (vm_ioapic_assert_irq(ctx, irq));
			else
				return (vm_ioapic_deassert_irq(ctx, irq));
		}
	}
}
