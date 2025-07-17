/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <sys/vmem.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#include <dev/pci/pcireg.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <x86/include/busdma_impl.h>
#include <dev/iommu/busdma_iommu.h>
#include <x86/iommu/amd_reg.h>
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/amd_iommu.h>

static void
amdiommu_enable_cmdbuf(struct amdiommu_unit *unit)
{
	AMDIOMMU_ASSERT_LOCKED(unit);

	unit->hw_ctrl |= AMDIOMMU_CTRL_CMDBUF_EN;
	amdiommu_write8(unit, AMDIOMMU_CTRL, unit->hw_ctrl);
}

static void
amdiommu_disable_cmdbuf(struct amdiommu_unit *unit)
{
	AMDIOMMU_ASSERT_LOCKED(unit);

	unit->hw_ctrl &= ~AMDIOMMU_CTRL_CMDBUF_EN;
	amdiommu_write8(unit, AMDIOMMU_CTRL, unit->hw_ctrl);
}


static void
amdiommu_enable_qi_intr(struct iommu_unit *iommu)
{
	struct amdiommu_unit *unit;

	unit = IOMMU2AMD(iommu);
	AMDIOMMU_ASSERT_LOCKED(unit);
	unit->hw_ctrl |= AMDIOMMU_CTRL_COMWINT_EN;
	amdiommu_write8(unit, AMDIOMMU_CTRL, unit->hw_ctrl);
	amdiommu_write8(unit, AMDIOMMU_CMDEV_STATUS,
	    AMDIOMMU_CMDEVS_COMWAITINT);
}

static void
amdiommu_disable_qi_intr(struct iommu_unit *iommu)
{
	struct amdiommu_unit *unit;

	unit = IOMMU2AMD(iommu);
	AMDIOMMU_ASSERT_LOCKED(unit);
	unit->hw_ctrl &= ~AMDIOMMU_CTRL_COMWINT_EN;
	amdiommu_write8(unit, AMDIOMMU_CTRL, unit->hw_ctrl);
}

static void
amdiommu_cmd_advance_tail(struct iommu_unit *iommu)
{
	struct amdiommu_unit *unit;

	unit = IOMMU2AMD(iommu);
	AMDIOMMU_ASSERT_LOCKED(unit);
	amdiommu_write8(unit, AMDIOMMU_CMDBUF_TAIL, unit->x86c.inv_queue_tail);
}

static void
amdiommu_cmd_ensure(struct iommu_unit *iommu, int descr_count)
{
	struct amdiommu_unit *unit;
	uint64_t head;
	int bytes;

	unit = IOMMU2AMD(iommu);
	AMDIOMMU_ASSERT_LOCKED(unit);
	bytes = descr_count << AMDIOMMU_CMD_SZ_SHIFT;
	for (;;) {
		if (bytes <= unit->x86c.inv_queue_avail)
			break;
		/* refill */
		head = amdiommu_read8(unit, AMDIOMMU_CMDBUF_HEAD);
		head &= AMDIOMMU_CMDPTR_MASK;
		unit->x86c.inv_queue_avail = head - unit->x86c.inv_queue_tail -
		    AMDIOMMU_CMD_SZ;
		if (head <= unit->x86c.inv_queue_tail)
			unit->x86c.inv_queue_avail += unit->x86c.inv_queue_size;
		if (bytes <= unit->x86c.inv_queue_avail)
			break;

		/*
		 * No space in the queue, do busy wait.  Hardware must
		 * make a progress.  But first advance the tail to
		 * inform the descriptor streamer about entries we
		 * might have already filled, otherwise they could
		 * clog the whole queue..
		 *
		 * See dmar_qi_invalidate_locked() for a discussion
		 * about data race prevention.
		 */
		amdiommu_cmd_advance_tail(iommu);
		unit->x86c.inv_queue_full++;
		cpu_spinwait();
	}
	unit->x86c.inv_queue_avail -= bytes;
}

static void
amdiommu_cmd_emit(struct amdiommu_unit *unit, const struct
    amdiommu_cmd_generic *cmd)
{
	AMDIOMMU_ASSERT_LOCKED(unit);

	memcpy(unit->x86c.inv_queue + unit->x86c.inv_queue_tail, cmd,
	    sizeof(*cmd));
	unit->x86c.inv_queue_tail += AMDIOMMU_CMD_SZ;
	KASSERT(unit->x86c.inv_queue_tail <= unit->x86c.inv_queue_size,
	    ("tail overflow 0x%x 0x%jx", unit->x86c.inv_queue_tail,
	    (uintmax_t)unit->x86c.inv_queue_size));
	unit->x86c.inv_queue_tail &= unit->x86c.inv_queue_size - 1;
}

static void
amdiommu_cmd_emit_wait_descr(struct iommu_unit *iommu, uint32_t seq,
    bool intr, bool memw, bool fence)
{
	struct amdiommu_unit *unit;
	struct amdiommu_cmd_completion_wait c;

	unit = IOMMU2AMD(iommu);
	AMDIOMMU_ASSERT_LOCKED(unit);

	bzero(&c, sizeof(c));
	c.op = AMDIOMMU_CMD_COMPLETION_WAIT;
	if (memw) {
		uint32_t x;

		c.s = 1;
		x = unit->x86c.inv_waitd_seq_hw_phys;
		x >>= 3;
		c.address0 = x;
		x = unit->x86c.inv_waitd_seq_hw_phys >> 32;
		c.address1 = x;
		c.data0 = seq;
	}
	if (fence)
		c.f = 1;
	if (intr)
		c.i = 1;
	amdiommu_cmd_emit(unit, (struct amdiommu_cmd_generic *)&c);
}

static void
amdiommu_qi_invalidate_emit(struct iommu_domain *adomain, iommu_gaddr_t base,
    iommu_gaddr_t size, struct iommu_qi_genseq *pseq, bool emit_wait)
{
	struct amdiommu_domain *domain;
	struct amdiommu_unit *unit;
	struct amdiommu_cmd_invalidate_iommu_pages c;
	u_int isize;

	domain = IODOM2DOM(adomain);
	unit = domain->unit;
	AMDIOMMU_ASSERT_LOCKED(unit);
	bzero(&c, sizeof(c));
	c.op = AMDIOMMU_CMD_INVALIDATE_IOMMU_PAGES;
	c.domainid = domain->domain;
	isize = IOMMU_PAGE_SIZE; /* XXXKIB handle superpages */

	for (; size > 0; base += isize, size -= isize) {
		amdiommu_cmd_ensure(AMD2IOMMU(unit), 1);
		c.s = 0;
		c.pde = 1;
		c.address = base >> IOMMU_PAGE_SHIFT;
		amdiommu_cmd_emit(unit, (struct amdiommu_cmd_generic *)&c);
	}
	iommu_qi_emit_wait_seq(AMD2IOMMU(unit), pseq, emit_wait);
}

void
amdiommu_qi_invalidate_all_pages_locked_nowait(struct amdiommu_domain *domain)
{
	struct amdiommu_unit *unit;
	struct amdiommu_cmd_invalidate_iommu_pages c;

	unit = domain->unit;
	AMDIOMMU_ASSERT_LOCKED(unit);
	bzero(&c, sizeof(c));
	c.op = AMDIOMMU_CMD_INVALIDATE_IOMMU_PAGES;
	c.domainid = domain->domain;

	/*
	 * The magic specified in the note for INVALIDATE_IOMMU_PAGES
	 * description.
	 */
	c.s = 1;
	c.pde = 1;
	c.address = 0x7ffffffffffff;

	amdiommu_cmd_ensure(AMD2IOMMU(unit), 1);
	amdiommu_cmd_emit(unit, (struct amdiommu_cmd_generic *)&c);
}

void
amdiommu_qi_invalidate_wait_sync(struct iommu_unit *iommu)
{
	struct iommu_qi_genseq gseq;

	amdiommu_cmd_ensure(iommu, 1);
	iommu_qi_emit_wait_seq(iommu, &gseq, true);
	IOMMU2AMD(iommu)->x86c.inv_seq_waiters++;
	amdiommu_cmd_advance_tail(iommu);
	iommu_qi_wait_for_seq(iommu, &gseq, true);
}

void
amdiommu_qi_invalidate_ctx_locked_nowait(struct amdiommu_ctx *ctx)
{
	struct amdiommu_cmd_invalidate_devtab_entry c;

	amdiommu_cmd_ensure(AMD2IOMMU(CTX2AMD(ctx)), 1);
	bzero(&c, sizeof(c));
	c.op = AMDIOMMU_CMD_INVALIDATE_DEVTAB_ENTRY;
	c.devid = ctx->context.rid;
	amdiommu_cmd_emit(CTX2AMD(ctx), (struct amdiommu_cmd_generic *)&c);
}


void
amdiommu_qi_invalidate_ctx_locked(struct amdiommu_ctx *ctx)
{
	amdiommu_qi_invalidate_ctx_locked_nowait(ctx);
	amdiommu_qi_invalidate_wait_sync(AMD2IOMMU(CTX2AMD(ctx)));
}

void
amdiommu_qi_invalidate_ir_locked_nowait(struct amdiommu_unit *unit,
    uint16_t devid)
{
	struct amdiommu_cmd_invalidate_interrupt_table c;

	AMDIOMMU_ASSERT_LOCKED(unit);

	amdiommu_cmd_ensure(AMD2IOMMU(unit), 1);
	bzero(&c, sizeof(c));
	c.op = AMDIOMMU_CMD_INVALIDATE_INTERRUPT_TABLE;
	c.devid = devid;
	amdiommu_cmd_emit(unit, (struct amdiommu_cmd_generic *)&c);
}

void
amdiommu_qi_invalidate_ir_locked(struct amdiommu_unit *unit, uint16_t devid)
{
	amdiommu_qi_invalidate_ir_locked_nowait(unit, devid);
	amdiommu_qi_invalidate_wait_sync(AMD2IOMMU(unit));
}

static void
amdiommu_qi_task(void *arg, int pending __unused)
{
	struct amdiommu_unit *unit;

	unit = IOMMU2AMD(arg);
	iommu_qi_drain_tlb_flush(AMD2IOMMU(unit));

	AMDIOMMU_LOCK(unit);
	if (unit->x86c.inv_seq_waiters > 0)
		wakeup(&unit->x86c.inv_seq_waiters);
	AMDIOMMU_UNLOCK(unit);
}

int
amdiommu_init_cmd(struct amdiommu_unit *unit)
{
	uint64_t qi_sz, rv;

	unit->x86c.qi_buf_maxsz = ilog2(AMDIOMMU_CMDBUF_MAX / PAGE_SIZE);
	unit->x86c.qi_cmd_sz = AMDIOMMU_CMD_SZ;
	iommu_qi_common_init(AMD2IOMMU(unit), amdiommu_qi_task);
	get_x86_iommu()->qi_ensure = amdiommu_cmd_ensure;
	get_x86_iommu()->qi_emit_wait_descr = amdiommu_cmd_emit_wait_descr;
	get_x86_iommu()->qi_advance_tail = amdiommu_cmd_advance_tail;
	get_x86_iommu()->qi_invalidate_emit = amdiommu_qi_invalidate_emit;

	rv = pmap_kextract((uintptr_t)unit->x86c.inv_queue);

	/*
	 * See the description of the ComLen encoding for Command
	 * buffer Base Address Register.
	 */
	qi_sz = ilog2(unit->x86c.inv_queue_size / PAGE_SIZE) + 8;
	rv |= qi_sz << AMDIOMMU_CMDBUF_BASE_SZSHIFT;

	AMDIOMMU_LOCK(unit);
	amdiommu_write8(unit, AMDIOMMU_CMDBUF_BASE, rv);
	amdiommu_enable_cmdbuf(unit);
	amdiommu_enable_qi_intr(AMD2IOMMU(unit));
	AMDIOMMU_UNLOCK(unit);

	return (0);
}

static void
amdiommu_fini_cmd_helper(struct iommu_unit *iommu)
{
	amdiommu_disable_cmdbuf(IOMMU2AMD(iommu));
	amdiommu_disable_qi_intr(iommu);
}

void
amdiommu_fini_cmd(struct amdiommu_unit *unit)
{
	iommu_qi_common_fini(AMD2IOMMU(unit), amdiommu_fini_cmd_helper);
}
