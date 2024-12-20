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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/rwlock.h>
#include <sys/smp.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <sys/vmem.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <machine/pci_cfgreg.h>
#include "pcib_if.h"
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/cputypes.h>
#include <x86/apicreg.h>
#include <x86/apicvar.h>
#include <dev/iommu/iommu.h>
#include <dev/iommu/busdma_iommu.h>
#include <x86/iommu/amd_reg.h>
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/amd_iommu.h>

static void
amdiommu_event_rearm_intr(struct amdiommu_unit *unit)
{
	amdiommu_write8(unit, AMDIOMMU_CMDEV_STATUS,
	    AMDIOMMU_CMDEVS_EVLOGINT);
}

static void
amdiommu_event_log_inc_head(struct amdiommu_unit *unit)
{
	unit->event_log_head++;
	if (unit->event_log_head >= unit->event_log_size)
		unit->event_log_head = 0;
}

static void
amdiommu_event_log_print(struct amdiommu_unit *unit,
    const struct amdiommu_event_generic *evp, bool fancy)
{
	printf("amdiommu%d: event type 0x%x 0x%08x 0x%08x 0x%08x 0x%08x\n",
	    unit->iommu.unit, evp->code, evp->w0, evp->ww1, evp->w2, evp->w3);
	if (!fancy)
		return;

	AMDIOMMU_ASSERT_LOCKED(unit);
	if (evp->code == AMDIOMMU_EV_ILL_DEV_TABLE_ENTRY) {
		const struct amdiommu_event_ill_dev_table_entry *ev_dte_p;
		const struct amdiommu_dte *dte;
		const uint32_t *x;
		int i;

		ev_dte_p = (const struct
		    amdiommu_event_ill_dev_table_entry *)evp;
		dte = &unit->dev_tbl[ev_dte_p->devid];

		printf("\tIllegal Dev Tab Entry dte@%p:", dte);
		for (i = 0, x = (const uint32_t *)dte; i < sizeof(*dte) /
		    sizeof(uint32_t); i++, x++)
			printf(" 0x%08x", *x);
		printf("\n");
	} else if (evp->code == AMDIOMMU_EV_IO_PAGE_FAULT) {
		const struct amdiommu_event_io_page_fault_entry *ev_iopf_p;
		struct amdiommu_ctx *ctx;
		device_t dev;

		ev_iopf_p = (const struct
		    amdiommu_event_io_page_fault_entry *)evp;
		printf("\tPage Fault rid %#x dom %d",
		    ev_iopf_p->devid, ev_iopf_p->pasid);
		ctx = amdiommu_find_ctx_locked(unit, ev_iopf_p->devid);
		if (ctx != NULL) {
			dev = ctx->context.tag->owner;
			if (dev != NULL)
				printf(" %s", device_get_nameunit(dev));
		}
		printf("\n\t"
		    "gn %d nx %d us %d i %d pr %d rw %d pe %d rz %d tr %d"
		    "\n\tgaddr %#jx\n",
		    ev_iopf_p->gn, ev_iopf_p->nx, ev_iopf_p->us, ev_iopf_p->i,
		    ev_iopf_p->pr, ev_iopf_p->rw, ev_iopf_p->pe, ev_iopf_p->rz,
		    ev_iopf_p->tr,
		    (((uintmax_t)(ev_iopf_p->addr2)) << 32) |
		    ev_iopf_p->addr1);
	}
}

static u_int
amdiommu_event_log_tail(struct amdiommu_unit *unit)
{
	return (amdiommu_read8(unit, AMDIOMMU_EVNTLOG_TAIL) >>
	    AMDIOMMU_EV_SZ_SHIFT);
}

static u_int
amdiommu_event_copy_log_inc(u_int idx)
{
	idx++;
	if (idx == nitems(((struct amdiommu_unit *)NULL)->event_copy_log))
		idx = 0;
	return (idx);
}

static bool
amdiommu_event_copy_log_hasspace(struct amdiommu_unit *unit)
{
	return (unit->event_copy_tail != amdiommu_event_copy_log_inc(
	    unit->event_copy_head));
}

void
amdiommu_event_intr(struct amdiommu_unit *unit, uint64_t status)
{
	struct amdiommu_event_generic *evp;
	u_int hw_tail, hw_tail1;
	bool enqueue;

	enqueue = (status & AMDIOMMU_CMDEVS_EVOVRFLW) != 0;

	hw_tail1 = amdiommu_event_log_tail(unit);
	do {
		hw_tail = hw_tail1;
		for (; hw_tail != unit->event_log_head;
		     amdiommu_event_log_inc_head(unit)) {
			evp = &unit->event_log[unit->event_log_head];
			mtx_lock_spin(&unit->event_lock);
			if (amdiommu_event_copy_log_hasspace(unit)) {
				unit->event_copy_log[unit->event_copy_head] =
				    *evp;
				unit->event_copy_head =
				    amdiommu_event_copy_log_inc(unit->
				    event_copy_head);
				enqueue = true;
			} else {
				amdiommu_event_log_print(unit, evp, false);
			}
			mtx_unlock_spin(&unit->event_lock);
		}
		amdiommu_write8(unit, AMDIOMMU_EVNTLOG_HEAD,
		    unit->event_log_head << AMDIOMMU_EV_SZ_SHIFT);
		hw_tail1 = amdiommu_event_log_tail(unit);
	} while (hw_tail1 != hw_tail);
	amdiommu_event_rearm_intr(unit);

	if (enqueue)
		taskqueue_enqueue(unit->event_taskqueue, &unit->event_task);
}

static void
amdiommu_event_task(void *arg, int pending __unused)
{
	struct amdiommu_unit *unit;
	uint64_t hwev_status, status;
	struct amdiommu_event_generic hwev;

	unit = arg;
	AMDIOMMU_LOCK(unit);

	if ((unit->efr & AMDIOMMU_EFR_HWEV_SUP) != 0) {
		hwev_status = amdiommu_read8(unit, AMDIOMMU_HWEV_STATUS);
		if ((hwev_status & AMDIOMMU_HWEVS_HEV) != 0) {
			*(uint64_t *)&hwev = amdiommu_read8(unit,
			    AMDIOMMU_HWEV_LOWER);
			*((uint64_t *)&hwev + 1) = amdiommu_read8(unit,
			    AMDIOMMU_HWEV_UPPER);
			printf("amdiommu%d: hw event%s\n", unit->iommu.unit,
			    (hwev_status & AMDIOMMU_HWEVS_HEO) != 0 ?
			    " (overflown)" : "");
			amdiommu_event_log_print(unit, &hwev, true);
			amdiommu_write8(unit, AMDIOMMU_HWEV_STATUS,
			    hwev_status);
		}
	}

	status = amdiommu_read8(unit, AMDIOMMU_CMDEV_STATUS);
	if ((status & AMDIOMMU_CMDEVS_EVOVRFLW) != 0) {
		printf("amdiommu%d: event log overflow\n", unit->iommu.unit);

		while ((status & AMDIOMMU_CMDEVS_EVLOGRUN) != 0) {
			DELAY(1);
			status = amdiommu_read8(unit, AMDIOMMU_CMDEV_STATUS);
		}

		unit->hw_ctrl &= ~AMDIOMMU_CTRL_EVNTLOG_EN;
		amdiommu_write8(unit, AMDIOMMU_CTRL, unit->hw_ctrl);

		unit->event_log_head = 0;
		amdiommu_write8(unit, AMDIOMMU_EVNTLOG_HEAD, 0);

		amdiommu_write8(unit, AMDIOMMU_CMDEV_STATUS,
		    AMDIOMMU_CMDEVS_EVOVRFLW);		/* RW1C */

		unit->hw_ctrl |= AMDIOMMU_CTRL_EVNTLOG_EN;
		amdiommu_write8(unit, AMDIOMMU_CTRL, unit->hw_ctrl);

		amdiommu_event_rearm_intr(unit);
	}

	mtx_lock_spin(&unit->event_lock);
	while (unit->event_copy_head != unit->event_copy_tail) {
		mtx_unlock_spin(&unit->event_lock);
		amdiommu_event_log_print(unit, &unit->event_copy_log[
		    unit->event_copy_tail], true);
		mtx_lock_spin(&unit->event_lock);
		unit->event_copy_tail = amdiommu_event_copy_log_inc(unit->
		    event_copy_tail);
	}
	mtx_unlock_spin(&unit->event_lock);

	AMDIOMMU_UNLOCK(unit);
}

int
amdiommu_init_event(struct amdiommu_unit *unit)
{
	uint64_t base_reg;

	mtx_init(&unit->event_lock, "amdevl", NULL, MTX_SPIN);

	/*  event log entries */
	unit->event_log_size = AMDIOMMU_EVNTLOG_MIN;
	TUNABLE_INT_FETCH("hw.amdiommu.event_log_size", &unit->event_log_size);
	if (unit->event_log_size < AMDIOMMU_EVNTLOG_MIN ||
	    unit->event_log_size > AMDIOMMU_EVNTLOG_MAX ||
	    !powerof2(unit->event_log_size))
		panic("invalid hw.amdiommu.event_log_size");
	unit->event_log = kmem_alloc_contig(AMDIOMMU_EV_SZ *
	    unit->event_log_size, M_WAITOK | M_ZERO, 0, ~0ull, PAGE_SIZE,
	    0, VM_MEMATTR_DEFAULT);

	TASK_INIT(&unit->event_task, 0, amdiommu_event_task, unit);
	unit->event_taskqueue = taskqueue_create_fast("amdiommuff", M_WAITOK,
	    taskqueue_thread_enqueue, &unit->event_taskqueue);
	taskqueue_start_threads(&unit->event_taskqueue, 1, PI_AV,
	    "amdiommu%d event taskq", unit->iommu.unit);

	base_reg = pmap_kextract((vm_offset_t)unit->event_log) |
	    (((uint64_t)0x8 + ilog2(unit->event_log_size /
	    AMDIOMMU_EVNTLOG_MIN)) << AMDIOMMU_EVNTLOG_BASE_SZSHIFT);
	AMDIOMMU_LOCK(unit);
	/*
	 * Re-arm before enabling interrupt, to not loose it when
	 * re-arming in the interrupt handler.
	 */
	amdiommu_event_rearm_intr(unit);
	amdiommu_write8(unit, AMDIOMMU_EVNTLOG_BASE, base_reg);
	unit->hw_ctrl |= AMDIOMMU_CTRL_EVNTLOG_EN | AMDIOMMU_CTRL_EVENTINT_EN;
	amdiommu_write8(unit, AMDIOMMU_CTRL, unit->hw_ctrl);
	AMDIOMMU_UNLOCK(unit);

	return (0);
}

void
amdiommu_fini_event(struct amdiommu_unit *unit)
{
	AMDIOMMU_LOCK(unit);
	unit->hw_ctrl &= ~(AMDIOMMU_CTRL_EVNTLOG_EN |
	    AMDIOMMU_CTRL_EVENTINT_EN);
	amdiommu_write8(unit, AMDIOMMU_CTRL, unit->hw_ctrl);
	amdiommu_write8(unit, AMDIOMMU_EVNTLOG_BASE, 0);
	AMDIOMMU_UNLOCK(unit);

	taskqueue_drain(unit->event_taskqueue, &unit->event_task);
	taskqueue_free(unit->event_taskqueue);
	unit->event_taskqueue = NULL;

	kmem_free(unit->event_log, unit->event_log_size * AMDIOMMU_EV_SZ);
	unit->event_log = NULL;
	unit->event_log_head = unit->event_log_tail = 0;

	mtx_destroy(&unit->event_lock);
}
