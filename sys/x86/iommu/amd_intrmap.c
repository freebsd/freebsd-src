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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/domainset.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <sys/uio.h>
#include <sys/vmem.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/intr_machdep.h>
#include <x86/include/apicreg.h>
#include <x86/include/apicvar.h>
#include <machine/specialreg.h>
#include <x86/include/busdma_impl.h>
#include <dev/iommu/busdma_iommu.h>
#include <x86/iommu/amd_reg.h>
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/amd_iommu.h>

static struct amdiommu_ctx *amdiommu_ir_find(device_t src, uint16_t *rid,
    bool *is_iommu);
static void amdiommu_ir_free_irte(struct amdiommu_ctx *ctx, device_t src,
    u_int cookie);

int
amdiommu_alloc_msi_intr(device_t src, u_int *cookies, u_int count)
{
	struct amdiommu_ctx *ctx;
	vmem_addr_t vmem_res;
	u_int idx, i;
	int error;

	ctx = amdiommu_ir_find(src, NULL, NULL);
	if (ctx == NULL || !CTX2AMD(ctx)->irte_enabled) {
		for (i = 0; i < count; i++)
			cookies[i] = -1;
		return (EOPNOTSUPP);
	}

	error = vmem_alloc(ctx->irtids, count, M_FIRSTFIT | M_NOWAIT,
	    &vmem_res);
	if (error != 0) {
		KASSERT(error != EOPNOTSUPP,
		    ("impossible EOPNOTSUPP from vmem"));
		return (error);
	}
	idx = vmem_res;
	for (i = 0; i < count; i++)
		cookies[i] = idx + i;
	return (0);
}

int
amdiommu_map_msi_intr(device_t src, u_int cpu, u_int vector,
    u_int cookie, uint64_t *addr, uint32_t *data)
{
	struct amdiommu_ctx *ctx;
	struct amdiommu_unit *unit;
	device_t requester;
	int error __diagused;
	uint16_t rid;
	bool is_iommu;

	ctx = amdiommu_ir_find(src, &rid, &is_iommu);
	if (is_iommu) {
		if (addr != NULL) {
			*data = vector;
			*addr = MSI_INTEL_ADDR_BASE | ((cpu & 0xff) << 12);
			if (x2apic_mode)
				*addr |= ((uint64_t)cpu & 0xffffff00) << 32;
			else
				KASSERT(cpu <= 0xff,
				    ("cpu id too big %d", cpu));
		}
		return (0);
	}

	if (ctx == NULL)
		return (EOPNOTSUPP);
	unit = CTX2AMD(ctx);
	if (!unit->irte_enabled || cookie == -1)
		return (EOPNOTSUPP);
	if (cookie >= unit->irte_nentries) {
		device_printf(src, "amdiommu%d: cookie %u irte max %u\n",
		    unit->iommu.unit, cookie, unit->irte_nentries);
		return (EINVAL);
	}

	if (unit->irte_x2apic) {
		struct amdiommu_irte_basic_vapic_x2 *irte;

		irte = &ctx->irtx2[cookie];
		irte->supiopf = 0;
		irte->inttype = 0;
		irte->rqeoi = 0;
		irte->dm = 0;
		irte->guestmode = 0;
		irte->dest0 = cpu;
		irte->rsrv0 = 0;
		irte->vector = vector;
		irte->rsrv1 = 0;
		irte->rsrv2 = 0;
		irte->dest1 = cpu >> 24;
		atomic_thread_fence_rel();
		irte->remapen = 1;
	} else {
		struct amdiommu_irte_basic_novapic *irte;

		irte = &ctx->irtb[cookie];
		irte->supiopf = 0;
		irte->inttype = 0;	/* fixed */
		irte->rqeoi = 0;
		irte->dm = 0;		/* phys */
		irte->guestmode = 0;
		irte->dest = cpu;
		irte->vector = vector;
		irte->rsrv = 0;
		atomic_thread_fence_rel();
		irte->remapen = 1;
	}

	if (addr != NULL) {
		*data = cookie;
		*addr = MSI_INTEL_ADDR_BASE | ((cpu & 0xff) << 12);
		if (unit->irte_x2apic)
			*addr |= ((uint64_t)cpu & 0xffffff00) << 32;
	}

	error = iommu_get_requester(src, &requester, &rid);
	MPASS(error == 0);
	AMDIOMMU_LOCK(unit);
	amdiommu_qi_invalidate_ir_locked(unit, rid);
	AMDIOMMU_UNLOCK(unit);

	return (0);
}

int
amdiommu_unmap_msi_intr(device_t src, u_int cookie)
{
	struct amdiommu_ctx *ctx;

	if (cookie == -1)
		return (0);
	ctx = amdiommu_ir_find(src, NULL, NULL);
	amdiommu_ir_free_irte(ctx, src, cookie);
	return (0);
}

int
amdiommu_map_ioapic_intr(u_int ioapic_id, u_int cpu, u_int vector,
    bool edge, bool activehi, int irq, u_int *cookie, uint32_t *hi,
    uint32_t *lo)
{
	/* XXXKIB for early call from ioapic_create() */
	return (EOPNOTSUPP);
}

int
amdiommu_unmap_ioapic_intr(u_int ioapic_id, u_int *cookie)
{
	/* XXXKIB */
	return (0);
}

static struct amdiommu_ctx *
amdiommu_ir_find(device_t src, uint16_t *ridp, bool *is_iommu)
{
	devclass_t src_class;
	device_t requester;
	struct amdiommu_unit *unit;
	struct amdiommu_ctx *ctx;
	uint32_t edte;
	uint16_t rid;
	uint8_t dte;
	int error;

	/*
	 * We need to determine if the interrupt source generates FSB
	 * interrupts.  If yes, it is either IOMMU, in which case
	 * interrupts are not remapped.  Or it is HPET, and interrupts
	 * are remapped.  For HPET, source id is reported by HPET
	 * record in IVHD ACPI table.
	 */
	if (is_iommu != NULL)
		*is_iommu = false;

	ctx = NULL;

	src_class = device_get_devclass(src);
	if (src_class == devclass_find("amdiommu")) {
		if (is_iommu != NULL)
			*is_iommu = true;
	} else if (src_class == devclass_find("hpet")) {
		error = amdiommu_find_unit_for_hpet(src, &unit, &rid, &dte,
		    &edte, bootverbose);
		ctx = NULL; // XXXKIB allocate ctx
	} else {
		error = amdiommu_find_unit(src, &unit, &rid, &dte, &edte,
		    bootverbose);
		if (error == 0) {
			error = iommu_get_requester(src, &requester, &rid);
			MPASS(error == 0);
			ctx = amdiommu_get_ctx_for_dev(unit, src,
			    rid, 0, false /* XXXKIB */, false, dte, edte);
		}
	}
	if (ridp != NULL)
		*ridp = rid;
	return (ctx);
}

static void
amdiommu_ir_free_irte(struct amdiommu_ctx *ctx, device_t src,
    u_int cookie)
{
	struct amdiommu_unit *unit;
	device_t requester;
	int error __diagused;
	uint16_t rid;

	MPASS(ctx != NULL);
	unit = CTX2AMD(ctx);

	KASSERT(unit->irte_enabled,
	    ("unmap: cookie %d ctx %p unit %p", cookie, ctx, unit));
	KASSERT(cookie < unit->irte_nentries,
	    ("bad cookie %u %u", cookie, unit->irte_nentries));

	if (unit->irte_x2apic) {
		struct amdiommu_irte_basic_vapic_x2 *irte;

		irte = &ctx->irtx2[cookie];
		irte->remapen = 0;
		atomic_thread_fence_rel();
		bzero(irte, sizeof(*irte));
	} else {
		struct amdiommu_irte_basic_novapic *irte;

		irte = &ctx->irtb[cookie];
		irte->remapen = 0;
		atomic_thread_fence_rel();
		bzero(irte, sizeof(*irte));
	}
	error = iommu_get_requester(src, &requester, &rid);
	MPASS(error == 0);
	AMDIOMMU_LOCK(unit);
	amdiommu_qi_invalidate_ir_locked(unit, rid);
	AMDIOMMU_UNLOCK(unit);
}

int
amdiommu_ctx_init_irte(struct amdiommu_ctx *ctx)
{
	struct amdiommu_unit *unit;
	void *ptr;
	unsigned long sz;
	int dom;

	unit = CTX2AMD(ctx);
	if (!unit->irte_enabled)
		return (0);

	KASSERT(unit->irte_nentries > 0 &&
	    unit->irte_nentries <= 2048 &&
	    powerof2(unit->irte_nentries),
	    ("amdiommu%d: unit %p irte_nentries %u", unit->iommu.unit,
	    unit, unit->irte_nentries));

	if (bus_get_domain(unit->iommu.dev, &dom) != 0)
		dom = -1;
	sz = unit->irte_nentries;
	sz *= unit->irte_x2apic ? sizeof(struct amdiommu_irte_basic_vapic_x2) :
	    sizeof(struct amdiommu_irte_basic_novapic);

	if (dom != -1) {
		ptr = contigmalloc_domainset(sz, M_DEVBUF, DOMAINSET_PREF(dom),
		    M_WAITOK | M_ZERO, 0, ~0ull, 128, 0);
	} else {
		ptr = contigmalloc(sz, M_DEVBUF, M_WAITOK | M_ZERO,
		    0, ~0ull, 128, 0);
	}
	if (unit->irte_x2apic)
		ctx->irtx2 = ptr;
	else
		ctx->irtb = ptr;
	ctx->irtids = vmem_create("amdirt", 0, unit->irte_nentries, 1, 0,
	    M_FIRSTFIT | M_NOWAIT);

	intr_reprogram();	// XXXKIB

	return (0);
}

void
amdiommu_ctx_fini_irte(struct amdiommu_ctx *ctx)
{
	struct amdiommu_unit *unit;

	unit = CTX2AMD(ctx);
	if (!unit->irte_enabled)
		return;
	if (unit->irte_x2apic)
		free(ctx->irtx2, M_DEVBUF);
	else
		free(ctx->irtb, M_DEVBUF);
	vmem_destroy(ctx->irtids);
}

int
amdiommu_init_irt(struct amdiommu_unit *unit)
{
	int enabled, nentries;

	SYSCTL_ADD_INT(&unit->iommu.sysctl_ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(unit->iommu.dev)),
	    OID_AUTO, "ir", CTLFLAG_RD, &unit->irte_enabled, 0,
	    "Interrupt remapping ops enabled");

	enabled = 1;
	TUNABLE_INT_FETCH("hw.iommu.ir", &enabled);

	unit->irte_enabled = enabled != 0;
	if (!unit->irte_enabled)
		return (0);

	nentries = 32;
	TUNABLE_INT_FETCH("hw.iommu.amd.ir_num", &nentries);
	nentries = roundup_pow_of_two(nentries);
	if (nentries < 1)
		nentries = 1;
	if (nentries > 2048)
		nentries = 2048;
	unit->irte_nentries = nentries;

	unit->irte_x2apic = x2apic_mode;
	return (0);
}

void
amdiommu_fini_irt(struct amdiommu_unit *unit)
{
}
