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
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/domainset.h>
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
#include <x86/iommu/amd_reg.h>
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/amd_iommu.h>

static int amdiommu_enable = 0;
static bool amdiommu_running = false;

/*
 * All enumerated AMD IOMMU units.
 * Access is unlocked, the list is not modified after early
 * single-threaded startup.
 */
static TAILQ_HEAD(, amdiommu_unit) amdiommu_units =
    TAILQ_HEAD_INITIALIZER(amdiommu_units);

typedef bool (*amdiommu_itercc_t)(void *, void *);
typedef bool (*amdiommu_iter40_t)(ACPI_IVRS_HARDWARE2 *, void *);
typedef bool (*amdiommu_iter11_t)(ACPI_IVRS_HARDWARE2 *, void *);
typedef bool (*amdiommu_iter10_t)(ACPI_IVRS_HARDWARE1 *, void *);

static bool
amdiommu_ivrs_iterate_tbl_typed(amdiommu_itercc_t iter, void *arg,
    int type, ACPI_TABLE_IVRS *ivrs_tbl)
{
	char *ptr, *ptrend;
	bool done;

	done = false;
	ptr = (char *)ivrs_tbl + sizeof(*ivrs_tbl);
	ptrend = (char *)ivrs_tbl + ivrs_tbl->Header.Length;
	for (;;) {
		ACPI_IVRS_HEADER *ivrsh;

		if (ptr >= ptrend)
			break;
		ivrsh = (ACPI_IVRS_HEADER *)ptr;
		if (ivrsh->Length <= 0) {
			printf("amdiommu_iterate_tbl: corrupted IVRS table, "
			    "length %d\n", ivrsh->Length);
			break;
		}
		ptr += ivrsh->Length;
		if (ivrsh->Type ==  type) {
			done = iter((void *)ivrsh, arg);
			if (done)
				break;
		}
	}
	return (done);
}

/*
 * Walk over IVRS, calling callback iterators following priority:
 * 0x40, then 0x11, then 0x10 subtable.  First iterator returning true
 * ends the walk.
 * Returns true if any iterator returned true, otherwise false.
 */
static bool
amdiommu_ivrs_iterate_tbl(amdiommu_iter40_t iter40, amdiommu_iter11_t iter11,
    amdiommu_iter10_t iter10, void *arg)
{
	ACPI_TABLE_IVRS *ivrs_tbl;
	ACPI_STATUS status;
	bool done;

	status = AcpiGetTable(ACPI_SIG_IVRS, 1,
	    (ACPI_TABLE_HEADER **)&ivrs_tbl);
	if (ACPI_FAILURE(status))
		return (false);
	done = false;
	if (iter40 != NULL)
		done = amdiommu_ivrs_iterate_tbl_typed(
		    (amdiommu_itercc_t)iter40, arg,
		    ACPI_IVRS_TYPE_HARDWARE3, ivrs_tbl);
	if (!done && iter11 != NULL)
		done = amdiommu_ivrs_iterate_tbl_typed(
		    (amdiommu_itercc_t)iter11, arg, ACPI_IVRS_TYPE_HARDWARE2,
		    ivrs_tbl);
	if (!done && iter10 != NULL)
		done = amdiommu_ivrs_iterate_tbl_typed(
		    (amdiommu_itercc_t)iter10, arg, ACPI_IVRS_TYPE_HARDWARE1,
		    ivrs_tbl);
	AcpiPutTable((ACPI_TABLE_HEADER *)ivrs_tbl);
	return (done);
}

struct ivhd_lookup_data  {
	struct amdiommu_unit *sc;
	uint16_t devid;
};

static bool
ivrs_lookup_ivhd_0x40(ACPI_IVRS_HARDWARE2 *h2, void *arg)
{
	struct ivhd_lookup_data *ildp;

	KASSERT(h2->Header.Type == ACPI_IVRS_TYPE_HARDWARE2 ||
	    h2->Header.Type == ACPI_IVRS_TYPE_HARDWARE3,
	    ("Misparsed IVHD, h2 type %#x", h2->Header.Type));

	ildp = arg;
	if (h2->Header.DeviceId != ildp->devid)
		return (false);

	ildp->sc->unit_dom = h2->PciSegmentGroup;
	ildp->sc->efr = h2->EfrRegisterImage;
	return (true);
}

static bool
ivrs_lookup_ivhd_0x10(ACPI_IVRS_HARDWARE1 *h1, void *arg)
{
	struct ivhd_lookup_data *ildp;

	KASSERT(h1->Header.Type == ACPI_IVRS_TYPE_HARDWARE1,
	    ("Misparsed IVHD, h1 type %#x", h1->Header.Type));

	ildp = arg;
	if (h1->Header.DeviceId != ildp->devid)
		return (false);

	ildp->sc->unit_dom = h1->PciSegmentGroup;
	return (true);
}

static u_int
amdiommu_devtbl_sz(struct amdiommu_unit *sc __unused)
{
	return (sizeof(struct amdiommu_dte) * (1 << 16));
}

static void
amdiommu_free_dev_tbl(struct amdiommu_unit *sc)
{
	u_int devtbl_sz;

	devtbl_sz = amdiommu_devtbl_sz(sc);
	pmap_qremove((vm_offset_t)sc->dev_tbl, atop(devtbl_sz));
	kva_free((vm_offset_t)sc->dev_tbl, devtbl_sz);
	sc->dev_tbl = NULL;
	vm_object_deallocate(sc->devtbl_obj);
	sc->devtbl_obj = NULL;
}

static int
amdiommu_create_dev_tbl(struct amdiommu_unit *sc)
{
	vm_offset_t seg_vaddr;
	u_int devtbl_sz, dom, i, reclaimno, segnum_log, segnum, seg_sz;
	int error;

	static const int devtab_base_regs[] = {
		AMDIOMMU_DEVTAB_BASE,
		AMDIOMMU_DEVTAB_S1_BASE,
		AMDIOMMU_DEVTAB_S2_BASE,
		AMDIOMMU_DEVTAB_S3_BASE,
		AMDIOMMU_DEVTAB_S4_BASE,
		AMDIOMMU_DEVTAB_S5_BASE,
		AMDIOMMU_DEVTAB_S6_BASE,
		AMDIOMMU_DEVTAB_S7_BASE
	};

	segnum_log = (sc->efr & AMDIOMMU_EFR_DEVTBLSEG_MASK) >>
	    AMDIOMMU_EFR_DEVTBLSEG_SHIFT;
	segnum = 1 << segnum_log;

	KASSERT(segnum <= nitems(devtab_base_regs),
	    ("%s: unsupported devtab segment count %u", __func__, segnum));

	devtbl_sz = amdiommu_devtbl_sz(sc);
	seg_sz = devtbl_sz / segnum;
	sc->devtbl_obj = vm_pager_allocate(OBJT_PHYS, NULL, atop(devtbl_sz),
	    VM_PROT_ALL, 0, NULL);
	if (bus_get_domain(sc->iommu.dev, &dom) == 0)
		sc->devtbl_obj->domain.dr_policy = DOMAINSET_PREF(dom);

	sc->hw_ctrl &= ~AMDIOMMU_CTRL_DEVTABSEG_MASK;
	sc->hw_ctrl |= (uint64_t)segnum_log << ilog2(AMDIOMMU_CTRL_DEVTABSEG_2);
	sc->hw_ctrl |= AMDIOMMU_CTRL_COHERENT;
	amdiommu_write8(sc, AMDIOMMU_CTRL, sc->hw_ctrl);

	seg_vaddr = kva_alloc(devtbl_sz);
	if (seg_vaddr == 0)
		return (ENOMEM);
	sc->dev_tbl = (void *)seg_vaddr;

	for (i = 0; i < segnum; i++) {
		vm_page_t m;
		uint64_t rval;

		for (reclaimno = 0; reclaimno < 3; reclaimno++) {
			VM_OBJECT_WLOCK(sc->devtbl_obj);
			m = vm_page_alloc_contig(sc->devtbl_obj,
			    i * atop(seg_sz),
			    VM_ALLOC_NORMAL | VM_ALLOC_NOBUSY,
			    atop(seg_sz), 0, ~0ul, IOMMU_PAGE_SIZE, 0,
			    VM_MEMATTR_DEFAULT);
			VM_OBJECT_WUNLOCK(sc->devtbl_obj);
			if (m != NULL)
				break;
			error = vm_page_reclaim_contig(VM_ALLOC_NORMAL,
			    atop(seg_sz), 0, ~0ul, IOMMU_PAGE_SIZE, 0);
			if (error != 0)
				vm_wait(sc->devtbl_obj);
		}
		if (m == NULL) {
			amdiommu_free_dev_tbl(sc);
			return (ENOMEM);
		}

		rval = VM_PAGE_TO_PHYS(m) | (atop(seg_sz) - 1);
		for (u_int j = 0; j < atop(seg_sz);
		     j++, seg_vaddr += PAGE_SIZE, m++) {
			pmap_zero_page(m);
			pmap_qenter(seg_vaddr, &m, 1);
		}
		amdiommu_write8(sc, devtab_base_regs[i], rval);
	}

	return (0);
}

static int
amdiommu_cmd_event_intr(void *arg)
{
	struct amdiommu_unit *unit;
	uint64_t status;

	unit = arg;
	status = amdiommu_read8(unit, AMDIOMMU_CMDEV_STATUS);
	if ((status & AMDIOMMU_CMDEVS_COMWAITINT) != 0) {
		amdiommu_write8(unit, AMDIOMMU_CMDEV_STATUS,
		    AMDIOMMU_CMDEVS_COMWAITINT);
		taskqueue_enqueue(unit->x86c.qi_taskqueue,
		    &unit->x86c.qi_task);
	}
	if ((status & (AMDIOMMU_CMDEVS_EVLOGINT |
	    AMDIOMMU_CMDEVS_EVOVRFLW)) != 0)
		amdiommu_event_intr(unit, status);
	return (FILTER_HANDLED);
}

static int
amdiommu_setup_intr(struct amdiommu_unit *sc)
{
	int error, msi_count, msix_count;

	msi_count = pci_msi_count(sc->iommu.dev);
	msix_count = pci_msix_count(sc->iommu.dev);
	if (msi_count == 0 && msix_count == 0) {
		device_printf(sc->iommu.dev, "needs MSI-class intr\n");
		return (ENXIO);
	}

#if 0
	/*
	 * XXXKIB how MSI-X is supposed to be organized for BAR-less
	 * function?  Practically available hardware implements only
	 * one IOMMU unit per function, and uses MSI.
	 */
	if (msix_count > 0) {
		sc->msix_table = bus_alloc_resource_any(sc->iommu.dev,
		    SYS_RES_MEMORY, &sc->msix_tab_rid, RF_ACTIVE);
		if (sc->msix_table == NULL)
			return (ENXIO);

		if (sc->msix_pba_rid != sc->msix_tab_rid) {
			/* Separate BAR for PBA */
			sc->msix_pba = bus_alloc_resource_any(sc->iommu.dev,
			    SYS_RES_MEMORY,
			    &sc->msix_pba_rid, RF_ACTIVE);
			if (sc->msix_pba == NULL) {
				bus_release_resource(sc->iommu.dev,
				    SYS_RES_MEMORY, &sc->msix_tab_rid,
				    sc->msix_table);
				return (ENXIO);
			}
		}
	}
#endif

	error = ENXIO;
	if (msix_count > 0) {
		error = pci_alloc_msix(sc->iommu.dev, &msix_count);
		if (error == 0)
			sc->numirqs = msix_count;
	}
	if (error != 0 && msi_count > 0) {
		error = pci_alloc_msi(sc->iommu.dev, &msi_count);
		if (error == 0)
			sc->numirqs = msi_count;
	}
	if (error != 0) {
		device_printf(sc->iommu.dev,
		    "Failed to allocate MSI/MSI-x (%d)\n", error);
		return (ENXIO);
	}

	/*
	 * XXXKIB Spec states that MISC0.MsiNum must be zero for IOMMU
	 * using MSI interrupts.  But at least one BIOS programmed '2'
	 * there, making driver use wrong rid and causing
	 * command/event interrupt ignored as stray.  Try to fix it
	 * with dirty force by assuming MsiNum is zero for MSI.
	 */
	sc->irq_cmdev_rid = 1;
	if (msix_count > 0) {
		sc->irq_cmdev_rid += pci_read_config(sc->iommu.dev,
		    sc->seccap_reg + PCIR_AMDIOMMU_MISC0, 4) &
		    PCIM_AMDIOMMU_MISC0_MSINUM_MASK;
	}

	sc->irq_cmdev = bus_alloc_resource_any(sc->iommu.dev, SYS_RES_IRQ,
	    &sc->irq_cmdev_rid, RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_cmdev == NULL) {
		device_printf(sc->iommu.dev,
		    "unable to map CMD/EV interrupt\n");
		return (ENXIO);
	}
	error = bus_setup_intr(sc->iommu.dev, sc->irq_cmdev,
	    INTR_TYPE_MISC, amdiommu_cmd_event_intr, NULL, sc,
	    &sc->irq_cmdev_cookie);
	if (error != 0) {
		device_printf(sc->iommu.dev,
		    "unable to setup interrupt (%d)\n", error);
		return (ENXIO);
	}
	bus_describe_intr(sc->iommu.dev, sc->irq_cmdev, sc->irq_cmdev_cookie,
	    "cmdev");

	if (x2apic_mode) {
		AMDIOMMU_LOCK(sc);
		sc->hw_ctrl |= AMDIOMMU_CTRL_GA_EN | AMDIOMMU_CTRL_XT_EN;
		amdiommu_write8(sc, AMDIOMMU_CTRL, sc->hw_ctrl);
		// XXXKIB AMDIOMMU_CTRL_INTCAPXT_EN and program x2APIC_CTRL
		AMDIOMMU_UNLOCK(sc);
	}

	return (0);
}

static int
amdiommu_probe(device_t dev)
{
	int seccap_reg;
	int error;
	uint32_t cap_h, cap_type, cap_rev;

	if (acpi_disabled("amdiommu"))
		return (ENXIO);
	TUNABLE_INT_FETCH("hw.amdiommu.enable", &amdiommu_enable);
	if (!amdiommu_enable)
		return (ENXIO);
	if (pci_get_class(dev) != PCIC_BASEPERIPH ||
	    pci_get_subclass(dev) != PCIS_BASEPERIPH_IOMMU)
		return (ENXIO);

	error = pci_find_cap(dev, PCIY_SECDEV, &seccap_reg);
	if (error != 0 || seccap_reg == 0)
		return (ENXIO);

	cap_h = pci_read_config(dev, seccap_reg + PCIR_AMDIOMMU_CAP_HEADER,
	    4);
	cap_type = cap_h & PCIM_AMDIOMMU_CAP_TYPE_MASK;
	cap_rev = cap_h & PCIM_AMDIOMMU_CAP_REV_MASK;
	if (cap_type != PCIM_AMDIOMMU_CAP_TYPE_VAL &&
	    cap_rev != PCIM_AMDIOMMU_CAP_REV_VAL)
		return (ENXIO);

	device_set_desc(dev, "DMA remap");
	return (BUS_PROBE_SPECIFIC);
}

static int
amdiommu_attach(device_t dev)
{
	struct amdiommu_unit *sc;
	struct ivhd_lookup_data ild;
	int error;
	uint32_t base_low, base_high;
	bool res;

	sc = device_get_softc(dev);
	sc->iommu.unit = device_get_unit(dev);
	sc->iommu.dev = dev;

	error = pci_find_cap(dev, PCIY_SECDEV, &sc->seccap_reg);
	if (error != 0 || sc->seccap_reg == 0)
		return (ENXIO);

	base_low = pci_read_config(dev, sc->seccap_reg +
	    PCIR_AMDIOMMU_BASE_LOW, 4);
	base_high = pci_read_config(dev, sc->seccap_reg +
	    PCIR_AMDIOMMU_BASE_HIGH, 4);
	sc->mmio_base = (base_low & PCIM_AMDIOMMU_BASE_LOW_ADDRM) |
	    ((uint64_t)base_high << 32);

	sc->device_id = pci_get_rid(dev);
	ild.sc = sc;
	ild.devid = sc->device_id;
	res = amdiommu_ivrs_iterate_tbl(ivrs_lookup_ivhd_0x40,
	    ivrs_lookup_ivhd_0x40, ivrs_lookup_ivhd_0x10, &ild);
	if (!res) {
		device_printf(dev, "Cannot find IVHD\n");
		return (ENXIO);
	}

	mtx_init(&sc->iommu.lock, "amdihw", NULL, MTX_DEF);
	sc->domids = new_unrhdr(0, 0xffff, &sc->iommu.lock);
	LIST_INIT(&sc->domains);
	sysctl_ctx_init(&sc->iommu.sysctl_ctx);

	sc->mmio_sz = ((sc->efr & AMDIOMMU_EFR_PC_SUP) != 0 ? 512 : 16) *
	    1024;

	sc->mmio_rid = AMDIOMMU_RID;
	error = bus_set_resource(dev, SYS_RES_MEMORY, AMDIOMMU_RID,
	    sc->mmio_base, sc->mmio_sz);
	if (error != 0) {
		device_printf(dev,
		    "bus_set_resource %#jx-%#jx failed, error %d\n",
		    (uintmax_t)sc->mmio_base, (uintmax_t)sc->mmio_base +
		    sc->mmio_sz, error);
		error = ENXIO;
		goto errout1;
	}
	sc->mmio_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->mmio_rid,
	    sc->mmio_base, sc->mmio_base + sc->mmio_sz - 1, sc->mmio_sz,
	    RF_ALLOCATED | RF_ACTIVE | RF_SHAREABLE);
	if (sc->mmio_res == NULL) {
		device_printf(dev,
		    "bus_alloc_resource %#jx-%#jx failed\n",
		    (uintmax_t)sc->mmio_base, (uintmax_t)sc->mmio_base +
		    sc->mmio_sz);
		error = ENXIO;
		goto errout2;
	}

	sc->hw_ctrl = amdiommu_read8(sc, AMDIOMMU_CTRL);
	if (bootverbose)
		device_printf(dev, "ctrl reg %#jx\n", (uintmax_t)sc->hw_ctrl);
	if ((sc->hw_ctrl & AMDIOMMU_CTRL_EN) != 0) {
		device_printf(dev, "CTRL_EN is set, bailing out\n");
		error = EBUSY;
		goto errout2;
	}

	iommu_high = BUS_SPACE_MAXADDR;

	error = amdiommu_create_dev_tbl(sc);
	if (error != 0)
		goto errout3;

	error = amdiommu_init_cmd(sc);
	if (error != 0)
		goto errout4;

	error = amdiommu_init_event(sc);
	if (error != 0)
		goto errout5;

	error = amdiommu_setup_intr(sc);
	if (error != 0)
		goto errout6;

	error = iommu_init_busdma(AMD2IOMMU(sc));
	if (error != 0)
		goto errout7;

	error = amdiommu_init_irt(sc);
	if (error != 0)
		goto errout8;

	/*
	 * Unlike DMAR, AMD IOMMU does not process command queue
	 * unless IOMMU is enabled.  But since non-present devtab
	 * entry makes IOMMU ignore transactions from corresponding
	 * initiator, de-facto IOMMU operations are disabled for the
	 * DMA and intr remapping.
	 */
	AMDIOMMU_LOCK(sc);
	sc->hw_ctrl |= AMDIOMMU_CTRL_EN;
	amdiommu_write8(sc, AMDIOMMU_CTRL, sc->hw_ctrl);
	if (bootverbose) {
		printf("amdiommu%d: enabled translation\n",
		    AMD2IOMMU(sc)->unit);
	}
	AMDIOMMU_UNLOCK(sc);

	TAILQ_INSERT_TAIL(&amdiommu_units, sc, unit_next);
	amdiommu_running = true;
	return (0);

errout8:
	iommu_fini_busdma(&sc->iommu);
errout7:
	pci_release_msi(dev);
errout6:
	amdiommu_fini_event(sc);
errout5:
	amdiommu_fini_cmd(sc);
errout4:
	amdiommu_free_dev_tbl(sc);
errout3:
	bus_release_resource(dev, SYS_RES_MEMORY, sc->mmio_rid, sc->mmio_res);
errout2:
	bus_delete_resource(dev, SYS_RES_MEMORY, sc->mmio_rid);
errout1:
	sysctl_ctx_free(&sc->iommu.sysctl_ctx);
	delete_unrhdr(sc->domids);
	mtx_destroy(&sc->iommu.lock);

	return (error);
}

static int
amdiommu_detach(device_t dev)
{
	return (EBUSY);
}

static int
amdiommu_suspend(device_t dev)
{
	/* XXXKIB */
	return (0);
}

static int
amdiommu_resume(device_t dev)
{
	/* XXXKIB */
	return (0);
}

static device_method_t amdiommu_methods[] = {
	DEVMETHOD(device_probe, amdiommu_probe),
	DEVMETHOD(device_attach, amdiommu_attach),
	DEVMETHOD(device_detach, amdiommu_detach),
	DEVMETHOD(device_suspend, amdiommu_suspend),
	DEVMETHOD(device_resume, amdiommu_resume),
	DEVMETHOD_END
};

static driver_t	amdiommu_driver = {
	"amdiommu",
	amdiommu_methods,
	sizeof(struct amdiommu_unit),
};

EARLY_DRIVER_MODULE(amdiommu, pci, amdiommu_driver, 0, 0, BUS_PASS_SUPPORTDEV);
MODULE_DEPEND(amdiommu, pci, 1, 1, 1);

int
amdiommu_is_running(void)
{
	return (amdiommu_running ? 0 : ENXIO);
}

static struct amdiommu_unit *
amdiommu_unit_by_device_id(u_int pci_seg, u_int device_id)
{
	struct amdiommu_unit *unit;

	TAILQ_FOREACH(unit, &amdiommu_units, unit_next) {
		if (unit->unit_dom == pci_seg && unit->device_id == device_id)
			return (unit);
	}
	return (NULL);
}
		
struct ivhd_find_unit {
	u_int		domain;
	uintptr_t	rid;
	int		devno;
	enum {
		IFU_DEV_PCI,
		IFU_DEV_IOAPIC,
		IFU_DEV_HPET,
	} type;
	u_int		device_id;
	uint16_t	rid_real;
	uint8_t		dte;
	uint32_t	edte;
};

static bool
amdiommu_find_unit_scan_ivrs(ACPI_IVRS_DE_HEADER *d, size_t tlen,
    struct ivhd_find_unit *ifu)
{
	char *db, *de;
	size_t len;

	for (de = (char *)d + tlen; (char *)d < de;
	     d = (ACPI_IVRS_DE_HEADER *)(db + len)) {
		db = (char *)d;
		if (d->Type == ACPI_IVRS_TYPE_PAD4) {
			len = sizeof(ACPI_IVRS_DEVICE4);
		} else if (d->Type == ACPI_IVRS_TYPE_ALL) {
			ACPI_IVRS_DEVICE4 *d4;

			d4 = (ACPI_IVRS_DEVICE4 *)db;
			len = sizeof(*d4);
			ifu->dte = d4->Header.DataSetting;
		} else if (d->Type == ACPI_IVRS_TYPE_SELECT) {
			ACPI_IVRS_DEVICE4 *d4;

			d4 = (ACPI_IVRS_DEVICE4 *)db;
			if (d4->Header.Id == ifu->rid) {
				ifu->dte = d4->Header.DataSetting;
				ifu->rid_real = ifu->rid;
				return (true);
			}
			len = sizeof(*d4);
		} else if (d->Type == ACPI_IVRS_TYPE_START) {
			ACPI_IVRS_DEVICE4 *d4, *d4n;

			d4 = (ACPI_IVRS_DEVICE4 *)db;
			d4n = d4 + 1;
			if (d4n->Header.Type != ACPI_IVRS_TYPE_END) {
				printf("IVRS dev4 start not followed by END "
				    "(%#x)\n", d4n->Header.Type);
				return (false);
			}
			if (d4->Header.Id <= ifu->rid &&
			    ifu->rid <= d4n->Header.Id) {
				ifu->dte = d4->Header.DataSetting;
				ifu->rid_real = ifu->rid;
				return (true);
			}
			len = 2 * sizeof(*d4);
		} else if (d->Type == ACPI_IVRS_TYPE_PAD8) {
			len = sizeof(ACPI_IVRS_DEVICE8A);
		} else if (d->Type == ACPI_IVRS_TYPE_ALIAS_SELECT) {
			ACPI_IVRS_DEVICE8A *d8a;

			d8a = (ACPI_IVRS_DEVICE8A *)db;
			if (d8a->Header.Id == ifu->rid) {
				ifu->dte = d8a->Header.DataSetting;
				ifu->rid_real = d8a->UsedId;
				return (true);
			}
			len = sizeof(*d8a);
		} else if (d->Type == ACPI_IVRS_TYPE_ALIAS_START) {
			ACPI_IVRS_DEVICE8A *d8a;
			ACPI_IVRS_DEVICE4 *d4;

			d8a = (ACPI_IVRS_DEVICE8A *)db;
			d4 = (ACPI_IVRS_DEVICE4 *)(d8a + 1);
			if (d4->Header.Type != ACPI_IVRS_TYPE_END) {
				printf("IVRS alias start not followed by END "
				    "(%#x)\n", d4->Header.Type);
				return (false);
			}
			if (d8a->Header.Id <= ifu->rid &&
			    ifu->rid <= d4->Header.Id) {
				ifu->dte = d8a->Header.DataSetting;
				ifu->rid_real = d8a->UsedId;
				return (true);
			}
			len = sizeof(*d8a) + sizeof(*d4);
		} else if (d->Type == ACPI_IVRS_TYPE_EXT_SELECT) {
			ACPI_IVRS_DEVICE8B *d8b;

			d8b = (ACPI_IVRS_DEVICE8B *)db;
			if (d8b->Header.Id == ifu->rid) {
				ifu->dte = d8b->Header.DataSetting;
				ifu->rid_real = ifu->rid;
				ifu->edte = d8b->ExtendedData;
				return (true);
			}
			len = sizeof(*d8b);
		} else if (d->Type == ACPI_IVRS_TYPE_EXT_START) {
			ACPI_IVRS_DEVICE8B *d8b;
			ACPI_IVRS_DEVICE4 *d4;

			d8b = (ACPI_IVRS_DEVICE8B *)db;
			d4 = (ACPI_IVRS_DEVICE4 *)(db + sizeof(*d8b));
			if (d4->Header.Type != ACPI_IVRS_TYPE_END) {
				printf("IVRS ext start not followed by END "
				    "(%#x)\n", d4->Header.Type);
				return (false);
			}
			if (d8b->Header.Id >= ifu->rid &&
			    ifu->rid <= d4->Header.Id) {
				ifu->dte = d8b->Header.DataSetting;
				ifu->rid_real = ifu->rid;
				ifu->edte = d8b->ExtendedData;
				return (true);
			}
			len = sizeof(*d8b) + sizeof(*d4);
		} else if (d->Type == ACPI_IVRS_TYPE_SPECIAL) {
			ACPI_IVRS_DEVICE8C *d8c;

			d8c = (ACPI_IVRS_DEVICE8C *)db;
			if (((ifu->type == IFU_DEV_IOAPIC &&
			    d8c->Variety == ACPI_IVHD_IOAPIC) ||
			    (ifu->type == IFU_DEV_HPET &&
			    d8c->Variety == ACPI_IVHD_HPET)) &&
			    ifu->devno == d8c->Handle) {
				ifu->dte = d8c->Header.DataSetting;
				ifu->rid_real = d8c->UsedId;
				return (true);
			}
			len = sizeof(*d8c);
		} else if (d->Type == ACPI_IVRS_TYPE_HID) {
			ACPI_IVRS_DEVICE_HID *dh;

			dh = (ACPI_IVRS_DEVICE_HID *)db;
			len = sizeof(*dh) + dh->UidLength;
			/* XXXKIB */
		} else {
#if 0
			printf("amdiommu: unknown IVRS device entry type %#x\n",
			    d->Type);
#endif
			if (d->Type <= 63)
				len = sizeof(ACPI_IVRS_DEVICE4);
			else if (d->Type <= 127)
				len = sizeof(ACPI_IVRS_DEVICE8A);
			else {
				printf("amdiommu: abort, cannot "
				    "advance iterator, item type %#x\n",
				    d->Type);
				return (false);
			}
		}
	}
	return (false);
}

static bool
amdiommu_find_unit_scan_0x11(ACPI_IVRS_HARDWARE2 *ivrs, void *arg)
{
	struct ivhd_find_unit *ifu = arg;
	ACPI_IVRS_DE_HEADER *d;
	bool res;

	KASSERT(ivrs->Header.Type == ACPI_IVRS_TYPE_HARDWARE2 ||
	    ivrs->Header.Type == ACPI_IVRS_TYPE_HARDWARE3,
	    ("Misparsed IVHD h2, ivrs type %#x", ivrs->Header.Type));

	if (ifu->domain != ivrs->PciSegmentGroup)
		return (false);
	d = (ACPI_IVRS_DE_HEADER *)(ivrs + 1);
	res = amdiommu_find_unit_scan_ivrs(d, ivrs->Header.Length, ifu);
	if (res)
		ifu->device_id = ivrs->Header.DeviceId;
	return (res);
}

static bool
amdiommu_find_unit_scan_0x10(ACPI_IVRS_HARDWARE1 *ivrs, void *arg)
{
	struct ivhd_find_unit *ifu = arg;
	ACPI_IVRS_DE_HEADER *d;
	bool res;

	KASSERT(ivrs->Header.Type == ACPI_IVRS_TYPE_HARDWARE1,
	    ("Misparsed IVHD h1, ivrs type %#x", ivrs->Header.Type));

	if (ifu->domain != ivrs->PciSegmentGroup)
		return (false);
	d = (ACPI_IVRS_DE_HEADER *)(ivrs + 1);
	res = amdiommu_find_unit_scan_ivrs(d, ivrs->Header.Length, ifu);
	if (res)
		ifu->device_id = ivrs->Header.DeviceId;
	return (res);
}

static void
amdiommu_dev_prop_dtr(device_t dev, const char *name, void *val, void *dtr_ctx)
{
	free(val, M_DEVBUF);
}

static int *
amdiommu_dev_fetch_flagsp(struct amdiommu_unit *unit, device_t dev)
{
	int *flagsp, error;

	bus_topo_assert();
	error = device_get_prop(dev, device_get_nameunit(unit->iommu.dev),
	    (void **)&flagsp);
	if (error == ENOENT) {
		flagsp = malloc(sizeof(int), M_DEVBUF, M_WAITOK | M_ZERO);
		device_set_prop(dev, device_get_nameunit(unit->iommu.dev),
		    flagsp, amdiommu_dev_prop_dtr, unit);
	}
	return (flagsp);
}

static int
amdiommu_get_dev_prop_flags(struct amdiommu_unit *unit, device_t dev)
{
	int *flagsp, flags;

	bus_topo_lock();
	flagsp = amdiommu_dev_fetch_flagsp(unit, dev);
	flags = *flagsp;
	bus_topo_unlock();
	return (flags);
}

static void
amdiommu_set_dev_prop_flags(struct amdiommu_unit *unit, device_t dev,
    int flag)
{
	int *flagsp;

	bus_topo_lock();
	flagsp = amdiommu_dev_fetch_flagsp(unit, dev);
	*flagsp |= flag;
	bus_topo_unlock();
}

int
amdiommu_find_unit(device_t dev, struct amdiommu_unit **unitp, uint16_t *ridp,
    uint8_t *dtep, uint32_t *edtep, bool verbose)
{
	struct ivhd_find_unit ifu;
	struct amdiommu_unit *unit;
	int error, flags;
	bool res;

	if (!amdiommu_enable)
		return (ENXIO);

	if (device_get_devclass(device_get_parent(dev)) !=
	    devclass_find("pci"))
		return (ENXIO);

	bzero(&ifu, sizeof(ifu));
	ifu.type = IFU_DEV_PCI;
	
	error = pci_get_id(dev, PCI_ID_RID, &ifu.rid);
	if (error != 0) {
		if (verbose)
			device_printf(dev,
			    "amdiommu cannot get rid, error %d\n", error);
		return (ENXIO);
	}

	ifu.domain = pci_get_domain(dev);
	res = amdiommu_ivrs_iterate_tbl(amdiommu_find_unit_scan_0x11,
	    amdiommu_find_unit_scan_0x11, amdiommu_find_unit_scan_0x10, &ifu);
	if (!res) {
		if (verbose)
			device_printf(dev,
			    "(%#06x:%#06x) amdiommu cannot match rid in IVHD\n",
			    ifu.domain, (unsigned)ifu.rid);
		return (ENXIO);
	}

	unit = amdiommu_unit_by_device_id(ifu.domain, ifu.device_id);
	if (unit == NULL) {
		if (verbose)
			device_printf(dev,
			    "(%#06x:%#06x) amdiommu cannot find unit\n",
			    ifu.domain, (unsigned)ifu.rid);
		return (ENXIO);
	}
	*unitp = unit;
	iommu_device_set_iommu_prop(dev, unit->iommu.dev);
	if (ridp != NULL)
		*ridp = ifu.rid_real;
	if (dtep != NULL)
		*dtep = ifu.dte;
	if (edtep != NULL)
		*edtep = ifu.edte;
	if (verbose) {
		flags = amdiommu_get_dev_prop_flags(unit, dev);
		if ((flags & AMDIOMMU_DEV_REPORTED) == 0) {
			amdiommu_set_dev_prop_flags(unit, dev,
			    AMDIOMMU_DEV_REPORTED);
			device_printf(dev, "amdiommu%d "
			    "initiator rid %#06x dte %#x edte %#x\n",
			    unit->iommu.unit, ifu.rid_real, ifu.dte, ifu.edte);
		}
	}
	return (0);
}

int
amdiommu_find_unit_for_ioapic(int apic_id, struct amdiommu_unit **unitp,
    uint16_t *ridp, uint8_t *dtep, uint32_t *edtep, bool verbose)
{
	struct ivhd_find_unit ifu;
	struct amdiommu_unit *unit;
	device_t apic_dev;
	bool res;

	if (!amdiommu_enable)
		return (ENXIO);

	bzero(&ifu, sizeof(ifu));
	ifu.type = IFU_DEV_IOAPIC;
	ifu.devno = apic_id;
	ifu.rid = -1;
	
	res = amdiommu_ivrs_iterate_tbl(amdiommu_find_unit_scan_0x11,
	    amdiommu_find_unit_scan_0x11, amdiommu_find_unit_scan_0x10, &ifu);
	if (!res) {
		if (verbose)
			printf("amdiommu cannot match ioapic no %d in IVHD\n",
			    apic_id);
		return (ENXIO);
	}

	unit = amdiommu_unit_by_device_id(0, ifu.device_id);
	apic_dev = ioapic_get_dev(apic_id);
	if (apic_dev != NULL)
		iommu_device_set_iommu_prop(apic_dev, unit->iommu.dev);
	if (unit == NULL) {
		if (verbose)
			printf("amdiommu cannot find unit by dev id %#x\n",
			    ifu.device_id);
		return (ENXIO);
	}
	*unitp = unit;
	if (ridp != NULL)
		*ridp = ifu.rid_real;
	if (dtep != NULL)
		*dtep = ifu.dte;
	if (edtep != NULL)
		*edtep = ifu.edte;
	if (verbose) {
		printf("amdiommu%d IOAPIC %d "
		    "initiator rid %#06x dte %#x edte %#x\n",
		    unit->iommu.unit, apic_id, ifu.rid_real, ifu.dte,
		    ifu.edte);
	}
	return (0);
}

int
amdiommu_find_unit_for_hpet(device_t hpet, struct amdiommu_unit **unitp,
    uint16_t *ridp, uint8_t *dtep, uint32_t *edtep, bool verbose)
{
	struct ivhd_find_unit ifu;
	struct amdiommu_unit *unit;
	int hpet_no;
	bool res;

	if (!amdiommu_enable)
		return (ENXIO);

	hpet_no = hpet_get_uid(hpet);
	bzero(&ifu, sizeof(ifu));
	ifu.type = IFU_DEV_HPET;
	ifu.devno = hpet_no;
	ifu.rid = -1;
	
	res = amdiommu_ivrs_iterate_tbl(amdiommu_find_unit_scan_0x11,
	    amdiommu_find_unit_scan_0x11, amdiommu_find_unit_scan_0x10, &ifu);
	if (!res) {
		if (verbose)
			printf("amdiommu cannot match hpet no %d in IVHD\n",
			    hpet_no);
		return (ENXIO);
	}

	unit = amdiommu_unit_by_device_id(0, ifu.device_id);
	if (unit == NULL) {
		if (verbose)
			printf("amdiommu cannot find unit id %d\n",
			    hpet_no);
		return (ENXIO);
	}
	*unitp = unit;
	iommu_device_set_iommu_prop(hpet, unit->iommu.dev);
	if (ridp != NULL)
		*ridp = ifu.rid_real;
	if (dtep != NULL)
		*dtep = ifu.dte;
	if (edtep != NULL)
		*edtep = ifu.edte;
	if (verbose) {
		printf("amdiommu%d HPET no %d "
		    "initiator rid %#06x dte %#x edte %#x\n",
		    unit->iommu.unit, hpet_no, ifu.rid_real, ifu.dte,
		    ifu.edte);
	}
	return (0);
}

static struct iommu_unit *
amdiommu_find_method(device_t dev, bool verbose)
{
	struct amdiommu_unit *unit;
	int error;
	uint32_t edte;
	uint16_t rid;
	uint8_t dte;

	error = amdiommu_find_unit(dev, &unit, &rid, &dte, &edte, verbose);
	if (error != 0) {
		if (verbose && amdiommu_enable)
			device_printf(dev,
			    "cannot find amdiommu unit, error %d\n",
			    error);
		return (NULL);
	}
	return (&unit->iommu);
}

static struct x86_unit_common *
amdiommu_get_x86_common(struct iommu_unit *unit)
{
	struct amdiommu_unit *iommu;

	iommu = IOMMU2AMD(unit);
	return (&iommu->x86c);
}

static void
amdiommu_unit_pre_instantiate_ctx(struct iommu_unit *unit)
{
}

static struct x86_iommu amd_x86_iommu = {
	.get_x86_common = amdiommu_get_x86_common,
	.unit_pre_instantiate_ctx = amdiommu_unit_pre_instantiate_ctx,
	.find = amdiommu_find_method,
	.domain_unload_entry = amdiommu_domain_unload_entry,
	.domain_unload = amdiommu_domain_unload,
	.get_ctx = amdiommu_get_ctx,
	.free_ctx_locked = amdiommu_free_ctx_locked_method,
	.alloc_msi_intr = amdiommu_alloc_msi_intr,
	.map_msi_intr = amdiommu_map_msi_intr,
	.unmap_msi_intr = amdiommu_unmap_msi_intr,
	.map_ioapic_intr = amdiommu_map_ioapic_intr,
	.unmap_ioapic_intr = amdiommu_unmap_ioapic_intr,
};

static void
x86_iommu_set_amd(void *arg __unused)
{
	if (cpu_vendor_id == CPU_VENDOR_AMD)
		set_x86_iommu(&amd_x86_iommu);
}

SYSINIT(x86_iommu, SI_SUB_TUNABLES, SI_ORDER_ANY, x86_iommu_set_amd, NULL);

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_lex.h>

static void
amdiommu_print_domain(struct amdiommu_domain *domain, bool show_mappings)
{
	struct iommu_domain *iodom;

	iodom = DOM2IODOM(domain);

	db_printf(
	    "  @%p dom %d pglvl %d end %jx refs %d\n"
	    "   ctx_cnt %d flags %x pgobj %p map_ents %u\n",
	    domain, domain->domain, domain->pglvl,
	    (uintmax_t)domain->iodom.end, domain->refs, domain->ctx_cnt,
	    domain->iodom.flags, domain->pgtbl_obj, domain->iodom.entries_cnt);

	iommu_db_domain_print_contexts(iodom);

	if (show_mappings)
		iommu_db_domain_print_mappings(iodom);
}

static void
amdiommu_print_one(struct amdiommu_unit *unit, bool show_domains,
    bool show_mappings, bool show_cmdq)
{
	struct amdiommu_domain *domain;
	struct amdiommu_cmd_generic *cp;
	u_int cmd_head, cmd_tail, ci;

	cmd_head = amdiommu_read4(unit, AMDIOMMU_CMDBUF_HEAD);
	cmd_tail = amdiommu_read4(unit, AMDIOMMU_CMDBUF_TAIL);
	db_printf("amdiommu%d at %p, mmio at %#jx/sz %#jx\n",
	    unit->iommu.unit, unit, (uintmax_t)unit->mmio_base,
	    (uintmax_t)unit->mmio_sz);
	db_printf("  hw ctrl %#018jx cmdevst %#018jx\n",
	    (uintmax_t)amdiommu_read8(unit, AMDIOMMU_CTRL),
	    (uintmax_t)amdiommu_read8(unit, AMDIOMMU_CMDEV_STATUS));
	db_printf("  devtbl at %p\n", unit->dev_tbl);
	db_printf("  hwseq at %p phys %#jx val %#jx\n",
	    &unit->x86c.inv_waitd_seq_hw,
	    pmap_kextract((vm_offset_t)&unit->x86c.inv_waitd_seq_hw),
	    unit->x86c.inv_waitd_seq_hw);
	db_printf("  invq at %p base %#jx hw head/tail %#x/%#x\n",
	    unit->x86c.inv_queue,
	    (uintmax_t)amdiommu_read8(unit, AMDIOMMU_CMDBUF_BASE),
	    cmd_head, cmd_tail);

	if (show_cmdq) {
		db_printf("  cmd q:\n");
		for (ci = cmd_head; ci != cmd_tail;) {
			cp = (struct amdiommu_cmd_generic *)(unit->
			    x86c.inv_queue + ci);
			db_printf(
		    "    idx %#x op %#x %#010x %#010x %#010x %#010x\n",
			    ci >> AMDIOMMU_CMD_SZ_SHIFT, cp->op,
		    	    cp->w0, cp->ww1, cp->w2, cp->w3);

			ci += AMDIOMMU_CMD_SZ;
			if (ci == unit->x86c.inv_queue_size)
				ci = 0;
		}
	}

	if (show_domains) {
		db_printf("  domains:\n");
		LIST_FOREACH(domain, &unit->domains, link) {
			amdiommu_print_domain(domain, show_mappings);
			if (db_pager_quit)
				break;
		}
	}
}

DB_SHOW_COMMAND(amdiommu, db_amdiommu_print)
{
	struct amdiommu_unit *unit;
	bool show_domains, show_mappings, show_cmdq;

	show_domains = strchr(modif, 'd') != NULL;
	show_mappings = strchr(modif, 'm') != NULL;
	show_cmdq = strchr(modif, 'q') != NULL;
	if (!have_addr) {
		db_printf("usage: show amdiommu [/d] [/m] [/q] index\n");
		return;
	}
	if ((vm_offset_t)addr < 0x10000)
		unit = amdiommu_unit_by_device_id(0, (u_int)addr);
	else
		unit = (struct amdiommu_unit *)addr;
	amdiommu_print_one(unit, show_domains, show_mappings, show_cmdq);
}

DB_SHOW_ALL_COMMAND(amdiommus, db_show_all_amdiommus)
{
	struct amdiommu_unit *unit;
	bool show_domains, show_mappings, show_cmdq;

	show_domains = strchr(modif, 'd') != NULL;
	show_mappings = strchr(modif, 'm') != NULL;
	show_cmdq = strchr(modif, 'q') != NULL;

	TAILQ_FOREACH(unit, &amdiommu_units, unit_next) {
		amdiommu_print_one(unit, show_domains, show_mappings,
		    show_cmdq);
		if (db_pager_quit)
			break;
	}
}
#endif
