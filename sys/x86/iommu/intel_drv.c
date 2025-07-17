/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2015 The FreeBSD Foundation
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
#if defined(__amd64__)
#define	DEV_APIC
#else
#include "opt_apic.h"
#endif
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
#include <machine/bus.h>
#include <machine/pci_cfgreg.h>
#include <machine/md_var.h>
#include <machine/cputypes.h>
#include <x86/include/busdma_impl.h>
#include <dev/iommu/busdma_iommu.h>
#include <x86/iommu/intel_reg.h>
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/intel_dmar.h>

#ifdef DEV_APIC
#include "pcib_if.h"
#include <machine/intr_machdep.h>
#include <x86/apicreg.h>
#include <x86/apicvar.h>
#endif

#define	DMAR_FAULT_IRQ_RID	0
#define	DMAR_QI_IRQ_RID		1
#define	DMAR_REG_RID		2

static device_t *dmar_devs;
static int dmar_devcnt;
static bool dmar_running = false;

typedef int (*dmar_iter_t)(ACPI_DMAR_HEADER *, void *);

static void
dmar_iterate_tbl(dmar_iter_t iter, void *arg)
{
	ACPI_TABLE_DMAR *dmartbl;
	ACPI_DMAR_HEADER *dmarh;
	char *ptr, *ptrend;
	ACPI_STATUS status;

	status = AcpiGetTable(ACPI_SIG_DMAR, 1, (ACPI_TABLE_HEADER **)&dmartbl);
	if (ACPI_FAILURE(status))
		return;
	ptr = (char *)dmartbl + sizeof(*dmartbl);
	ptrend = (char *)dmartbl + dmartbl->Header.Length;
	for (;;) {
		if (ptr >= ptrend)
			break;
		dmarh = (ACPI_DMAR_HEADER *)ptr;
		if (dmarh->Length <= 0) {
			printf("dmar_identify: corrupted DMAR table, l %d\n",
			    dmarh->Length);
			break;
		}
		ptr += dmarh->Length;
		if (!iter(dmarh, arg))
			break;
	}
	AcpiPutTable((ACPI_TABLE_HEADER *)dmartbl);
}

struct find_iter_args {
	int i;
	ACPI_DMAR_HARDWARE_UNIT *res;
};

static int
dmar_find_iter(ACPI_DMAR_HEADER *dmarh, void *arg)
{
	struct find_iter_args *fia;

	if (dmarh->Type != ACPI_DMAR_TYPE_HARDWARE_UNIT)
		return (1);

	fia = arg;
	if (fia->i == 0) {
		fia->res = (ACPI_DMAR_HARDWARE_UNIT *)dmarh;
		return (0);
	}
	fia->i--;
	return (1);
}

static ACPI_DMAR_HARDWARE_UNIT *
dmar_find_by_index(int idx)
{
	struct find_iter_args fia;

	fia.i = idx;
	fia.res = NULL;
	dmar_iterate_tbl(dmar_find_iter, &fia);
	return (fia.res);
}

static int
dmar_count_iter(ACPI_DMAR_HEADER *dmarh, void *arg)
{

	if (dmarh->Type == ACPI_DMAR_TYPE_HARDWARE_UNIT)
		dmar_devcnt++;
	return (1);
}

/* Remapping Hardware Static Affinity Structure lookup */
struct rhsa_iter_arg {
	uint64_t base;
	u_int proxim_dom;
};

static int
dmar_rhsa_iter(ACPI_DMAR_HEADER *dmarh, void *arg)
{
	struct rhsa_iter_arg *ria;
	ACPI_DMAR_RHSA *adr;

	if (dmarh->Type == ACPI_DMAR_TYPE_HARDWARE_AFFINITY) {
		ria = arg;
		adr = (ACPI_DMAR_RHSA *)dmarh;
		if (adr->BaseAddress == ria->base)
			ria->proxim_dom = adr->ProximityDomain;
	}
	return (1);
}

int dmar_rmrr_enable = 1;

static int dmar_enable = 0;
static void
dmar_identify(driver_t *driver, device_t parent)
{
	ACPI_TABLE_DMAR *dmartbl;
	ACPI_DMAR_HARDWARE_UNIT *dmarh;
	struct rhsa_iter_arg ria;
	ACPI_STATUS status;
	int i, error;

	if (acpi_disabled("dmar"))
		return;
	TUNABLE_INT_FETCH("hw.dmar.enable", &dmar_enable);
	if (!dmar_enable)
		return;
	TUNABLE_INT_FETCH("hw.dmar.rmrr_enable", &dmar_rmrr_enable);

	status = AcpiGetTable(ACPI_SIG_DMAR, 1, (ACPI_TABLE_HEADER **)&dmartbl);
	if (ACPI_FAILURE(status))
		return;
	haw = dmartbl->Width + 1;
	if ((1ULL << (haw + 1)) > BUS_SPACE_MAXADDR)
		iommu_high = BUS_SPACE_MAXADDR;
	else
		iommu_high = 1ULL << (haw + 1);
	if (bootverbose) {
		printf("DMAR HAW=%d flags=<%b>\n", dmartbl->Width,
		    (unsigned)dmartbl->Flags,
		    "\020\001INTR_REMAP\002X2APIC_OPT_OUT");
	}
	AcpiPutTable((ACPI_TABLE_HEADER *)dmartbl);

	dmar_iterate_tbl(dmar_count_iter, NULL);
	if (dmar_devcnt == 0)
		return;
	dmar_devs = malloc(sizeof(device_t) * dmar_devcnt, M_DEVBUF,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < dmar_devcnt; i++) {
		dmarh = dmar_find_by_index(i);
		if (dmarh == NULL) {
			printf("dmar_identify: cannot find HWUNIT %d\n", i);
			continue;
		}
		dmar_devs[i] = BUS_ADD_CHILD(parent, 1, "dmar", i);
		if (dmar_devs[i] == NULL) {
			printf("dmar_identify: cannot create instance %d\n", i);
			continue;
		}
		error = bus_set_resource(dmar_devs[i], SYS_RES_MEMORY,
		    DMAR_REG_RID, dmarh->Address, PAGE_SIZE);
		if (error != 0) {
			printf(
	"dmar%d: unable to alloc register window at 0x%08jx: error %d\n",
			    i, (uintmax_t)dmarh->Address, error);
			device_delete_child(parent, dmar_devs[i]);
			dmar_devs[i] = NULL;
			continue;
		}

		ria.base = dmarh->Address;
		ria.proxim_dom = -1;
		dmar_iterate_tbl(dmar_rhsa_iter, &ria);
		acpi_set_domain(dmar_devs[i], ria.proxim_dom == -1 ?
		    ACPI_DEV_DOMAIN_UNKNOWN :
		    acpi_map_pxm_to_vm_domainid(ria.proxim_dom));
	}
}

static int
dmar_probe(device_t dev)
{

	if (acpi_get_handle(dev) != NULL)
		return (ENXIO);
	device_set_desc(dev, "DMA remap");
	return (BUS_PROBE_NOWILDCARD);
}

static void
dmar_release_resources(device_t dev, struct dmar_unit *unit)
{
	int i;

	iommu_fini_busdma(&unit->iommu);
	dmar_fini_irt(unit);
	dmar_fini_qi(unit);
	dmar_fini_fault_log(unit);
	for (i = 0; i < DMAR_INTR_TOTAL; i++)
		iommu_release_intr(DMAR2IOMMU(unit), i);
	if (unit->regs != NULL) {
		bus_deactivate_resource(dev, SYS_RES_MEMORY, unit->reg_rid,
		    unit->regs);
		bus_release_resource(dev, SYS_RES_MEMORY, unit->reg_rid,
		    unit->regs);
		unit->regs = NULL;
	}
	if (unit->domids != NULL) {
		delete_unrhdr(unit->domids);
		unit->domids = NULL;
	}
	if (unit->ctx_obj != NULL) {
		vm_object_deallocate(unit->ctx_obj);
		unit->ctx_obj = NULL;
	}
	sysctl_ctx_free(&unit->iommu.sysctl_ctx);
}

#ifdef DEV_APIC
static int
dmar_remap_intr(device_t dev, device_t child, u_int irq)
{
	struct dmar_unit *unit;
	struct iommu_msi_data *dmd;
	uint64_t msi_addr;
	uint32_t msi_data;
	int i, error;

	unit = device_get_softc(dev);
	for (i = 0; i < DMAR_INTR_TOTAL; i++) {
		dmd = &unit->x86c.intrs[i];
		if (irq == dmd->irq) {
			error = PCIB_MAP_MSI(device_get_parent(
			    device_get_parent(dev)),
			    dev, irq, &msi_addr, &msi_data);
			if (error != 0)
				return (error);
			DMAR_LOCK(unit);
			dmd->msi_data = msi_data;
			dmd->msi_addr = msi_addr;
			(dmd->disable_intr)(DMAR2IOMMU(unit));
			dmar_write4(unit, dmd->msi_data_reg, dmd->msi_data);
			dmar_write4(unit, dmd->msi_addr_reg, dmd->msi_addr);
			dmar_write4(unit, dmd->msi_uaddr_reg,
			    dmd->msi_addr >> 32);
			(dmd->enable_intr)(DMAR2IOMMU(unit));
			DMAR_UNLOCK(unit);
			return (0);
		}
	}
	return (ENOENT);
}
#endif

static void
dmar_print_caps(device_t dev, struct dmar_unit *unit,
    ACPI_DMAR_HARDWARE_UNIT *dmaru)
{
	uint32_t caphi, ecaphi;

	device_printf(dev, "regs@0x%08jx, ver=%d.%d, seg=%d, flags=<%b>\n",
	    (uintmax_t)dmaru->Address, DMAR_MAJOR_VER(unit->hw_ver),
	    DMAR_MINOR_VER(unit->hw_ver), dmaru->Segment,
	    dmaru->Flags, "\020\001INCLUDE_ALL_PCI");
	caphi = unit->hw_cap >> 32;
	device_printf(dev, "cap=%b,", (u_int)unit->hw_cap,
	    "\020\004AFL\005WBF\006PLMR\007PHMR\010CM\027ZLR\030ISOCH");
	printf("%b, ", caphi, "\020\010PSI\027DWD\030DRD\031FL1GP\034PSI");
	printf("ndoms=%d, sagaw=%d, mgaw=%d, fro=%d, nfr=%d, superp=%d",
	    DMAR_CAP_ND(unit->hw_cap), DMAR_CAP_SAGAW(unit->hw_cap),
	    DMAR_CAP_MGAW(unit->hw_cap), DMAR_CAP_FRO(unit->hw_cap),
	    DMAR_CAP_NFR(unit->hw_cap), DMAR_CAP_SPS(unit->hw_cap));
	if ((unit->hw_cap & DMAR_CAP_PSI) != 0)
		printf(", mamv=%d", DMAR_CAP_MAMV(unit->hw_cap));
	printf("\n");
	ecaphi = unit->hw_ecap >> 32;
	device_printf(dev, "ecap=%b,", (u_int)unit->hw_ecap,
	    "\020\001C\002QI\003DI\004IR\005EIM\007PT\010SC\031ECS\032MTS"
	    "\033NEST\034DIS\035PASID\036PRS\037ERS\040SRS");
	printf("%b, ", ecaphi, "\020\002NWFS\003EAFS");
	printf("mhmw=%d, iro=%d\n", DMAR_ECAP_MHMV(unit->hw_ecap),
	    DMAR_ECAP_IRO(unit->hw_ecap));
}

static int
dmar_attach(device_t dev)
{
	struct dmar_unit *unit;
	ACPI_DMAR_HARDWARE_UNIT *dmaru;
	struct iommu_msi_data *dmd;
	uint64_t timeout;
	int disable_pmr;
	int i, error;

	unit = device_get_softc(dev);
	unit->iommu.unit = device_get_unit(dev);
	unit->iommu.dev = dev;
	sysctl_ctx_init(&unit->iommu.sysctl_ctx);
	dmaru = dmar_find_by_index(unit->iommu.unit);
	if (dmaru == NULL)
		return (EINVAL);
	unit->segment = dmaru->Segment;
	unit->base = dmaru->Address;
	unit->reg_rid = DMAR_REG_RID;
	unit->regs = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &unit->reg_rid, RF_ACTIVE);
	if (unit->regs == NULL) {
		device_printf(dev, "cannot allocate register window\n");
		dmar_devs[unit->iommu.unit] = NULL;
		return (ENOMEM);
	}
	unit->hw_ver = dmar_read4(unit, DMAR_VER_REG);
	unit->hw_cap = dmar_read8(unit, DMAR_CAP_REG);
	unit->hw_ecap = dmar_read8(unit, DMAR_ECAP_REG);
	if (bootverbose)
		dmar_print_caps(dev, unit, dmaru);
	dmar_quirks_post_ident(unit);
	unit->memdomain = acpi_get_domain(dev);
	timeout = dmar_get_timeout();
	TUNABLE_UINT64_FETCH("hw.iommu.dmar.timeout", &timeout);
	dmar_update_timeout(timeout);

	for (i = 0; i < DMAR_INTR_TOTAL; i++)
		unit->x86c.intrs[i].irq = -1;

	dmd = &unit->x86c.intrs[DMAR_INTR_FAULT];
	dmd->name = "fault";
	dmd->irq_rid = DMAR_FAULT_IRQ_RID;
	dmd->handler = dmar_fault_intr;
	dmd->msi_data_reg = DMAR_FEDATA_REG;
	dmd->msi_addr_reg = DMAR_FEADDR_REG;
	dmd->msi_uaddr_reg = DMAR_FEUADDR_REG;
	dmd->enable_intr = dmar_enable_fault_intr;
	dmd->disable_intr = dmar_disable_fault_intr;
	error = iommu_alloc_irq(DMAR2IOMMU(unit), DMAR_INTR_FAULT);
	if (error != 0) {
		dmar_release_resources(dev, unit);
		dmar_devs[unit->iommu.unit] = NULL;
		return (error);
	}
	dmar_write4(unit, dmd->msi_data_reg, dmd->msi_data);
	dmar_write4(unit, dmd->msi_addr_reg, dmd->msi_addr);
	dmar_write4(unit, dmd->msi_uaddr_reg, dmd->msi_addr >> 32);

	if (DMAR_HAS_QI(unit)) {
		dmd = &unit->x86c.intrs[DMAR_INTR_QI];
		dmd->name = "qi";
		dmd->irq_rid = DMAR_QI_IRQ_RID;
		dmd->handler = dmar_qi_intr;
		dmd->msi_data_reg = DMAR_IEDATA_REG;
		dmd->msi_addr_reg = DMAR_IEADDR_REG;
		dmd->msi_uaddr_reg = DMAR_IEUADDR_REG;
		dmd->enable_intr = dmar_enable_qi_intr;
		dmd->disable_intr = dmar_disable_qi_intr;
		error = iommu_alloc_irq(DMAR2IOMMU(unit), DMAR_INTR_QI);
		if (error != 0) {
			dmar_release_resources(dev, unit);
			dmar_devs[unit->iommu.unit] = NULL;
			return (error);
		}

		dmar_write4(unit, dmd->msi_data_reg, dmd->msi_data);
		dmar_write4(unit, dmd->msi_addr_reg, dmd->msi_addr);
		dmar_write4(unit, dmd->msi_uaddr_reg, dmd->msi_addr >> 32);
	}

	mtx_init(&unit->iommu.lock, "dmarhw", NULL, MTX_DEF);
	unit->domids = new_unrhdr(0, dmar_nd2mask(DMAR_CAP_ND(unit->hw_cap)),
	    &unit->iommu.lock);
	LIST_INIT(&unit->domains);

	/*
	 * 9.2 "Context Entry":
	 * When Caching Mode (CM) field is reported as Set, the
	 * domain-id value of zero is architecturally reserved.
	 * Software must not use domain-id value of zero
	 * when CM is Set.
	 */
	if ((unit->hw_cap & DMAR_CAP_CM) != 0)
		alloc_unr_specific(unit->domids, 0);

	unit->ctx_obj = vm_pager_allocate(OBJT_PHYS, NULL, IDX_TO_OFF(1 +
	    DMAR_CTX_CNT), 0, 0, NULL);
	if (unit->memdomain != -1) {
		unit->ctx_obj->domain.dr_policy = DOMAINSET_PREF(
		    unit->memdomain);
	}

	/*
	 * Allocate and load the root entry table pointer.  Enable the
	 * address translation after the required invalidations are
	 * done.
	 */
	iommu_pgalloc(unit->ctx_obj, 0, IOMMU_PGF_WAITOK | IOMMU_PGF_ZERO);
	DMAR_LOCK(unit);
	error = dmar_load_root_entry_ptr(unit);
	if (error != 0) {
		DMAR_UNLOCK(unit);
		dmar_release_resources(dev, unit);
		dmar_devs[unit->iommu.unit] = NULL;
		return (error);
	}
	error = dmar_inv_ctx_glob(unit);
	if (error != 0) {
		DMAR_UNLOCK(unit);
		dmar_release_resources(dev, unit);
		dmar_devs[unit->iommu.unit] = NULL;
		return (error);
	}
	if ((unit->hw_ecap & DMAR_ECAP_DI) != 0) {
		error = dmar_inv_iotlb_glob(unit);
		if (error != 0) {
			DMAR_UNLOCK(unit);
			dmar_release_resources(dev, unit);
			dmar_devs[unit->iommu.unit] = NULL;
			return (error);
		}
	}

	DMAR_UNLOCK(unit);
	error = dmar_init_fault_log(unit);
	if (error != 0) {
		dmar_release_resources(dev, unit);
		dmar_devs[unit->iommu.unit] = NULL;
		return (error);
	}
	error = dmar_init_qi(unit);
	if (error != 0) {
		dmar_release_resources(dev, unit);
		dmar_devs[unit->iommu.unit] = NULL;
		return (error);
	}
	error = dmar_init_irt(unit);
	if (error != 0) {
		dmar_release_resources(dev, unit);
		dmar_devs[unit->iommu.unit] = NULL;
		return (error);
	}

	disable_pmr = 0;
	TUNABLE_INT_FETCH("hw.dmar.pmr.disable", &disable_pmr);
	if (disable_pmr) {
		error = dmar_disable_protected_regions(unit);
		if (error != 0)
			device_printf(dev,
			    "Failed to disable protected regions\n");
	}

	error = iommu_init_busdma(&unit->iommu);
	if (error != 0) {
		dmar_release_resources(dev, unit);
		dmar_devs[unit->iommu.unit] = NULL;
		return (error);
	}

#ifdef NOTYET
	DMAR_LOCK(unit);
	error = dmar_enable_translation(unit);
	if (error != 0) {
		DMAR_UNLOCK(unit);
		dmar_release_resources(dev, unit);
		dmar_devs[unit->iommu.unit] = NULL;
		return (error);
	}
	DMAR_UNLOCK(unit);
#endif

	dmar_running = true;
	return (0);
}

static int
dmar_detach(device_t dev)
{

	return (EBUSY);
}

static int
dmar_suspend(device_t dev)
{

	return (0);
}

static int
dmar_resume(device_t dev)
{

	/* XXXKIB */
	return (0);
}

static device_method_t dmar_methods[] = {
	DEVMETHOD(device_identify, dmar_identify),
	DEVMETHOD(device_probe, dmar_probe),
	DEVMETHOD(device_attach, dmar_attach),
	DEVMETHOD(device_detach, dmar_detach),
	DEVMETHOD(device_suspend, dmar_suspend),
	DEVMETHOD(device_resume, dmar_resume),
#ifdef DEV_APIC
	DEVMETHOD(bus_remap_intr, dmar_remap_intr),
#endif
	DEVMETHOD_END
};

static driver_t	dmar_driver = {
	"dmar",
	dmar_methods,
	sizeof(struct dmar_unit),
};

DRIVER_MODULE(dmar, acpi, dmar_driver, 0, 0);
MODULE_DEPEND(dmar, acpi, 1, 1, 1);

static void
dmar_print_path(int busno, int depth, const ACPI_DMAR_PCI_PATH *path)
{
	int i;

	printf("[%d, ", busno);
	for (i = 0; i < depth; i++) {
		if (i != 0)
			printf(", ");
		printf("(%d, %d)", path[i].Device, path[i].Function);
	}
	printf("]");
}

int
dmar_dev_depth(device_t child)
{
	devclass_t pci_class;
	device_t bus, pcib;
	int depth;

	pci_class = devclass_find("pci");
	for (depth = 1; ; depth++) {
		bus = device_get_parent(child);
		pcib = device_get_parent(bus);
		if (device_get_devclass(device_get_parent(pcib)) !=
		    pci_class)
			return (depth);
		child = pcib;
	}
}

void
dmar_dev_path(device_t child, int *busno, void *path1, int depth)
{
	devclass_t pci_class;
	device_t bus, pcib;
	ACPI_DMAR_PCI_PATH *path;

	pci_class = devclass_find("pci");
	path = path1;
	for (depth--; depth != -1; depth--) {
		path[depth].Device = pci_get_slot(child);
		path[depth].Function = pci_get_function(child);
		bus = device_get_parent(child);
		pcib = device_get_parent(bus);
		if (device_get_devclass(device_get_parent(pcib)) !=
		    pci_class) {
			/* reached a host bridge */
			*busno = pcib_get_bus(bus);
			return;
		}
		child = pcib;
	}
	panic("wrong depth");
}

static int
dmar_match_pathes(int busno1, const ACPI_DMAR_PCI_PATH *path1, int depth1,
    int busno2, const ACPI_DMAR_PCI_PATH *path2, int depth2,
    enum AcpiDmarScopeType scope_type)
{
	int i, depth;

	if (busno1 != busno2)
		return (0);
	if (scope_type == ACPI_DMAR_SCOPE_TYPE_ENDPOINT && depth1 != depth2)
		return (0);
	depth = depth1;
	if (depth2 < depth)
		depth = depth2;
	for (i = 0; i < depth; i++) {
		if (path1[i].Device != path2[i].Device ||
		    path1[i].Function != path2[i].Function)
			return (0);
	}
	return (1);
}

static int
dmar_match_devscope(ACPI_DMAR_DEVICE_SCOPE *devscope, int dev_busno,
    const ACPI_DMAR_PCI_PATH *dev_path, int dev_path_len)
{
	ACPI_DMAR_PCI_PATH *path;
	int path_len;

	if (devscope->Length < sizeof(*devscope)) {
		printf("dmar_match_devscope: corrupted DMAR table, dl %d\n",
		    devscope->Length);
		return (-1);
	}
	if (devscope->EntryType != ACPI_DMAR_SCOPE_TYPE_ENDPOINT &&
	    devscope->EntryType != ACPI_DMAR_SCOPE_TYPE_BRIDGE)
		return (0);
	path_len = devscope->Length - sizeof(*devscope);
	if (path_len % 2 != 0) {
		printf("dmar_match_devscope: corrupted DMAR table, dl %d\n",
		    devscope->Length);
		return (-1);
	}
	path_len /= 2;
	path = (ACPI_DMAR_PCI_PATH *)(devscope + 1);
	if (path_len == 0) {
		printf("dmar_match_devscope: corrupted DMAR table, dl %d\n",
		    devscope->Length);
		return (-1);
	}

	return (dmar_match_pathes(devscope->Bus, path, path_len, dev_busno,
	    dev_path, dev_path_len, devscope->EntryType));
}

static bool
dmar_match_by_path(struct dmar_unit *unit, int dev_domain, int dev_busno,
    const ACPI_DMAR_PCI_PATH *dev_path, int dev_path_len, const char **banner)
{
	ACPI_DMAR_HARDWARE_UNIT *dmarh;
	ACPI_DMAR_DEVICE_SCOPE *devscope;
	char *ptr, *ptrend;
	int match;

	dmarh = dmar_find_by_index(unit->iommu.unit);
	if (dmarh == NULL)
		return (false);
	if (dmarh->Segment != dev_domain)
		return (false);
	if ((dmarh->Flags & ACPI_DMAR_INCLUDE_ALL) != 0) {
		if (banner != NULL)
			*banner = "INCLUDE_ALL";
		return (true);
	}
	ptr = (char *)dmarh + sizeof(*dmarh);
	ptrend = (char *)dmarh + dmarh->Header.Length;
	while (ptr < ptrend) {
		devscope = (ACPI_DMAR_DEVICE_SCOPE *)ptr;
		ptr += devscope->Length;
		match = dmar_match_devscope(devscope, dev_busno, dev_path,
		    dev_path_len);
		if (match == -1)
			return (false);
		if (match == 1) {
			if (banner != NULL)
				*banner = "specific match";
			return (true);
		}
	}
	return (false);
}

static struct dmar_unit *
dmar_find_by_scope(int dev_domain, int dev_busno,
    const ACPI_DMAR_PCI_PATH *dev_path, int dev_path_len)
{
	struct dmar_unit *unit;
	int i;

	for (i = 0; i < dmar_devcnt; i++) {
		if (dmar_devs[i] == NULL)
			continue;
		unit = device_get_softc(dmar_devs[i]);
		if (dmar_match_by_path(unit, dev_domain, dev_busno, dev_path,
		    dev_path_len, NULL))
			return (unit);
	}
	return (NULL);
}

struct dmar_unit *
dmar_find(device_t dev, bool verbose)
{
	struct dmar_unit *unit;
	const char *banner;
	int i, dev_domain, dev_busno, dev_path_len;

	/*
	 * This function can only handle PCI(e) devices.
	 */
	if (device_get_devclass(device_get_parent(dev)) !=
	    devclass_find("pci"))
		return (NULL);

	dev_domain = pci_get_domain(dev);
	dev_path_len = dmar_dev_depth(dev);
	ACPI_DMAR_PCI_PATH dev_path[dev_path_len];
	dmar_dev_path(dev, &dev_busno, dev_path, dev_path_len);
	banner = "";

	for (i = 0; i < dmar_devcnt; i++) {
		if (dmar_devs[i] == NULL)
			continue;
		unit = device_get_softc(dmar_devs[i]);
		if (dmar_match_by_path(unit, dev_domain, dev_busno,
		    dev_path, dev_path_len, &banner))
			break;
	}
	if (i == dmar_devcnt)
		return (NULL);

	if (verbose) {
		device_printf(dev, "pci%d:%d:%d:%d matched dmar%d by %s",
		    dev_domain, pci_get_bus(dev), pci_get_slot(dev),
		    pci_get_function(dev), unit->iommu.unit, banner);
		printf(" scope path ");
		dmar_print_path(dev_busno, dev_path_len, dev_path);
		printf("\n");
	}
	iommu_device_set_iommu_prop(dev, unit->iommu.dev);
	return (unit);
}

static struct dmar_unit *
dmar_find_nonpci(u_int id, u_int entry_type, uint16_t *rid)
{
	device_t dmar_dev;
	struct dmar_unit *unit;
	ACPI_DMAR_HARDWARE_UNIT *dmarh;
	ACPI_DMAR_DEVICE_SCOPE *devscope;
	ACPI_DMAR_PCI_PATH *path;
	char *ptr, *ptrend;
#ifdef DEV_APIC
	int error;
#endif
	int i;

	for (i = 0; i < dmar_devcnt; i++) {
		dmar_dev = dmar_devs[i];
		if (dmar_dev == NULL)
			continue;
		unit = (struct dmar_unit *)device_get_softc(dmar_dev);
		dmarh = dmar_find_by_index(i);
		if (dmarh == NULL)
			continue;
		ptr = (char *)dmarh + sizeof(*dmarh);
		ptrend = (char *)dmarh + dmarh->Header.Length;
		for (;;) {
			if (ptr >= ptrend)
				break;
			devscope = (ACPI_DMAR_DEVICE_SCOPE *)ptr;
			ptr += devscope->Length;
			if (devscope->EntryType != entry_type)
				continue;
			if (devscope->EnumerationId != id)
				continue;
#ifdef DEV_APIC
			if (entry_type == ACPI_DMAR_SCOPE_TYPE_IOAPIC) {
				error = ioapic_get_rid(id, rid);
				/*
				 * If our IOAPIC has PCI bindings then
				 * use the PCI device rid.
				 */
				if (error == 0)
					return (unit);
			}
#endif
			if (devscope->Length - sizeof(ACPI_DMAR_DEVICE_SCOPE)
			    == 2) {
				if (rid != NULL) {
					path = (ACPI_DMAR_PCI_PATH *)
					    (devscope + 1);
					*rid = PCI_RID(devscope->Bus,
					    path->Device, path->Function);
				}
				return (unit);
			}
			printf(
		           "dmar_find_nonpci: id %d type %d path length != 2\n",
			    id, entry_type);
			break;
		}
	}
	return (NULL);
}

struct dmar_unit *
dmar_find_hpet(device_t dev, uint16_t *rid)
{
	struct dmar_unit *unit;

	unit = dmar_find_nonpci(hpet_get_uid(dev), ACPI_DMAR_SCOPE_TYPE_HPET,
	    rid);
	if (unit != NULL)
		iommu_device_set_iommu_prop(dev, unit->iommu.dev);
	return (unit);
}

struct dmar_unit *
dmar_find_ioapic(u_int apic_id, uint16_t *rid)
{
	struct dmar_unit *unit;
	device_t apic_dev;

	unit = dmar_find_nonpci(apic_id, ACPI_DMAR_SCOPE_TYPE_IOAPIC, rid);
	if (unit != NULL) {
		apic_dev = ioapic_get_dev(apic_id);
		if (apic_dev != NULL)
			iommu_device_set_iommu_prop(apic_dev, unit->iommu.dev);
	}
	return (unit);
}

struct rmrr_iter_args {
	struct dmar_domain *domain;
	int dev_domain;
	int dev_busno;
	const ACPI_DMAR_PCI_PATH *dev_path;
	int dev_path_len;
	struct iommu_map_entries_tailq *rmrr_entries;
};

static int
dmar_rmrr_iter(ACPI_DMAR_HEADER *dmarh, void *arg)
{
	struct rmrr_iter_args *ria;
	ACPI_DMAR_RESERVED_MEMORY *resmem;
	ACPI_DMAR_DEVICE_SCOPE *devscope;
	struct iommu_map_entry *entry;
	char *ptr, *ptrend;
	int match;

	if (!dmar_rmrr_enable)
		return (1);

	if (dmarh->Type != ACPI_DMAR_TYPE_RESERVED_MEMORY)
		return (1);

	ria = arg;
	resmem = (ACPI_DMAR_RESERVED_MEMORY *)dmarh;
	if (resmem->Segment != ria->dev_domain)
		return (1);

	ptr = (char *)resmem + sizeof(*resmem);
	ptrend = (char *)resmem + resmem->Header.Length;
	for (;;) {
		if (ptr >= ptrend)
			break;
		devscope = (ACPI_DMAR_DEVICE_SCOPE *)ptr;
		ptr += devscope->Length;
		match = dmar_match_devscope(devscope, ria->dev_busno,
		    ria->dev_path, ria->dev_path_len);
		if (match == 1) {
			entry = iommu_gas_alloc_entry(DOM2IODOM(ria->domain),
			    IOMMU_PGF_WAITOK);
			entry->start = resmem->BaseAddress;
			/* The RMRR entry end address is inclusive. */
			entry->end = resmem->EndAddress;
			TAILQ_INSERT_TAIL(ria->rmrr_entries, entry,
			    dmamap_link);
		}
	}

	return (1);
}

void
dmar_dev_parse_rmrr(struct dmar_domain *domain, int dev_domain, int dev_busno,
    const void *dev_path, int dev_path_len,
    struct iommu_map_entries_tailq *rmrr_entries)
{
	struct rmrr_iter_args ria;

	ria.domain = domain;
	ria.dev_domain = dev_domain;
	ria.dev_busno = dev_busno;
	ria.dev_path = (const ACPI_DMAR_PCI_PATH *)dev_path;
	ria.dev_path_len = dev_path_len;
	ria.rmrr_entries = rmrr_entries;
	dmar_iterate_tbl(dmar_rmrr_iter, &ria);
}

struct inst_rmrr_iter_args {
	struct dmar_unit *dmar;
};

static device_t
dmar_path_dev(int segment, int path_len, int busno,
    const ACPI_DMAR_PCI_PATH *path, uint16_t *rid)
{
	device_t dev;
	int i;

	dev = NULL;
	for (i = 0; i < path_len; i++) {
		dev = pci_find_dbsf(segment, busno, path->Device,
		    path->Function);
		if (i != path_len - 1) {
			busno = pci_cfgregread(segment, busno, path->Device,
			    path->Function, PCIR_SECBUS_1, 1);
			path++;
		}
	}
	*rid = PCI_RID(busno, path->Device, path->Function);
	return (dev);
}

static int
dmar_inst_rmrr_iter(ACPI_DMAR_HEADER *dmarh, void *arg)
{
	const ACPI_DMAR_RESERVED_MEMORY *resmem;
	const ACPI_DMAR_DEVICE_SCOPE *devscope;
	struct inst_rmrr_iter_args *iria;
	const char *ptr, *ptrend;
	device_t dev;
	struct dmar_unit *unit;
	int dev_path_len;
	uint16_t rid;

	iria = arg;

	if (!dmar_rmrr_enable)
		return (1);

	if (dmarh->Type != ACPI_DMAR_TYPE_RESERVED_MEMORY)
		return (1);

	resmem = (ACPI_DMAR_RESERVED_MEMORY *)dmarh;
	if (resmem->Segment != iria->dmar->segment)
		return (1);

	ptr = (const char *)resmem + sizeof(*resmem);
	ptrend = (const char *)resmem + resmem->Header.Length;
	for (;;) {
		if (ptr >= ptrend)
			break;
		devscope = (const ACPI_DMAR_DEVICE_SCOPE *)ptr;
		ptr += devscope->Length;
		/* XXXKIB bridge */
		if (devscope->EntryType != ACPI_DMAR_SCOPE_TYPE_ENDPOINT)
			continue;
		rid = 0;
		dev_path_len = (devscope->Length -
		    sizeof(ACPI_DMAR_DEVICE_SCOPE)) / 2;
		dev = dmar_path_dev(resmem->Segment, dev_path_len,
		    devscope->Bus,
		    (const ACPI_DMAR_PCI_PATH *)(devscope + 1), &rid);
		if (dev == NULL) {
			if (bootverbose) {
				printf("dmar%d no dev found for RMRR "
				    "[%#jx, %#jx] rid %#x scope path ",
				    iria->dmar->iommu.unit,
				    (uintmax_t)resmem->BaseAddress,
				    (uintmax_t)resmem->EndAddress,
				    rid);
				dmar_print_path(devscope->Bus, dev_path_len,
				    (const ACPI_DMAR_PCI_PATH *)(devscope + 1));
				printf("\n");
			}
			unit = dmar_find_by_scope(resmem->Segment,
			    devscope->Bus,
			    (const ACPI_DMAR_PCI_PATH *)(devscope + 1),
			    dev_path_len);
			if (iria->dmar != unit)
				continue;
			dmar_get_ctx_for_devpath(iria->dmar, rid,
			    resmem->Segment, devscope->Bus, 
			    (const ACPI_DMAR_PCI_PATH *)(devscope + 1),
			    dev_path_len, false, true);
		} else {
			unit = dmar_find(dev, false);
			if (iria->dmar != unit)
				continue;
			iommu_instantiate_ctx(&(iria)->dmar->iommu,
			    dev, true);
		}
	}

	return (1);

}

int
dmar_is_running(void)
{

	return (dmar_running ? 0 : ENXIO);
}

/*
 * Pre-create all contexts for the DMAR which have RMRR entries.
 */
int
dmar_instantiate_rmrr_ctxs(struct iommu_unit *unit)
{
	struct dmar_unit *dmar;
	struct inst_rmrr_iter_args iria;
	int error;

	dmar = IOMMU2DMAR(unit);

	if (!dmar_barrier_enter(dmar, DMAR_BARRIER_RMRR))
		return (0);

	error = 0;
	iria.dmar = dmar;
	dmar_iterate_tbl(dmar_inst_rmrr_iter, &iria);
	DMAR_LOCK(dmar);
	if (!LIST_EMPTY(&dmar->domains)) {
		KASSERT((dmar->hw_gcmd & DMAR_GCMD_TE) == 0,
	    ("dmar%d: RMRR not handled but translation is already enabled",
		    dmar->iommu.unit));
		error = dmar_disable_protected_regions(dmar);
		if (error != 0)
			printf("dmar%d: Failed to disable protected regions\n",
			    dmar->iommu.unit);
		error = dmar_enable_translation(dmar);
		if (bootverbose) {
			if (error == 0) {
				printf("dmar%d: enabled translation\n",
				    dmar->iommu.unit);
			} else {
				printf("dmar%d: enabling translation failed, "
				    "error %d\n", dmar->iommu.unit, error);
			}
		}
	}
	dmar_barrier_exit(dmar, DMAR_BARRIER_RMRR);
	return (error);
}

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_lex.h>

static void
dmar_print_domain(struct dmar_domain *domain, bool show_mappings)
{
	struct iommu_domain *iodom;

	iodom = DOM2IODOM(domain);

	db_printf(
	    "  @%p dom %d mgaw %d agaw %d pglvl %d end %jx refs %d\n"
	    "   ctx_cnt %d flags %x pgobj %p map_ents %u\n",
	    domain, domain->domain, domain->mgaw, domain->agaw, domain->pglvl,
	    (uintmax_t)domain->iodom.end, domain->refs, domain->ctx_cnt,
	    domain->iodom.flags, domain->pgtbl_obj, domain->iodom.entries_cnt);

	iommu_db_domain_print_contexts(iodom);

	if (show_mappings)
		iommu_db_domain_print_mappings(iodom);
}

DB_SHOW_COMMAND_FLAGS(dmar_domain, db_dmar_print_domain, CS_OWN)
{
	struct dmar_unit *unit;
	struct dmar_domain *domain;
	struct iommu_ctx *ctx;
	bool show_mappings, valid;
	int pci_domain, bus, device, function, i, t;
	db_expr_t radix;

	valid = false;
	radix = db_radix;
	db_radix = 10;
	t = db_read_token();
	if (t == tSLASH) {
		t = db_read_token();
		if (t != tIDENT) {
			db_printf("Bad modifier\n");
			db_radix = radix;
			db_skip_to_eol();
			return;
		}
		show_mappings = strchr(db_tok_string, 'm') != NULL;
		t = db_read_token();
	} else {
		show_mappings = false;
	}
	if (t == tNUMBER) {
		pci_domain = db_tok_number;
		t = db_read_token();
		if (t == tNUMBER) {
			bus = db_tok_number;
			t = db_read_token();
			if (t == tNUMBER) {
				device = db_tok_number;
				t = db_read_token();
				if (t == tNUMBER) {
					function = db_tok_number;
					valid = true;
				}
			}
		}
	}
			db_radix = radix;
	db_skip_to_eol();
	if (!valid) {
		db_printf("usage: show dmar_domain [/m] "
		    "<domain> <bus> <device> <func>\n");
		return;
	}
	for (i = 0; i < dmar_devcnt; i++) {
		unit = device_get_softc(dmar_devs[i]);
		LIST_FOREACH(domain, &unit->domains, link) {
			LIST_FOREACH(ctx, &domain->iodom.contexts, link) {
				if (pci_domain == unit->segment && 
				    bus == pci_get_bus(ctx->tag->owner) &&
				    device == pci_get_slot(ctx->tag->owner) &&
				    function == pci_get_function(ctx->tag->
				    owner)) {
					dmar_print_domain(domain,
					    show_mappings);
					goto out;
				}
			}
		}
	}
out:;
}

static void
dmar_print_one(int idx, bool show_domains, bool show_mappings)
{
	struct dmar_unit *unit;
	struct dmar_domain *domain;
	int i, frir;

	unit = device_get_softc(dmar_devs[idx]);
	db_printf("dmar%d at %p, root at 0x%jx, ver 0x%x\n", unit->iommu.unit,
	    unit, dmar_read8(unit, DMAR_RTADDR_REG),
	    dmar_read4(unit, DMAR_VER_REG));
	db_printf("cap 0x%jx ecap 0x%jx gsts 0x%x fsts 0x%x fectl 0x%x\n",
	    (uintmax_t)dmar_read8(unit, DMAR_CAP_REG),
	    (uintmax_t)dmar_read8(unit, DMAR_ECAP_REG),
	    dmar_read4(unit, DMAR_GSTS_REG),
	    dmar_read4(unit, DMAR_FSTS_REG),
	    dmar_read4(unit, DMAR_FECTL_REG));
	if (unit->ir_enabled) {
		db_printf("ir is enabled; IRT @%p phys 0x%jx maxcnt %d\n",
		    unit->irt, (uintmax_t)unit->irt_phys, unit->irte_cnt);
	}
	db_printf("fed 0x%x fea 0x%x feua 0x%x\n",
	    dmar_read4(unit, DMAR_FEDATA_REG),
	    dmar_read4(unit, DMAR_FEADDR_REG),
	    dmar_read4(unit, DMAR_FEUADDR_REG));
	db_printf("primary fault log:\n");
	for (i = 0; i < DMAR_CAP_NFR(unit->hw_cap); i++) {
		frir = (DMAR_CAP_FRO(unit->hw_cap) + i) * 16;
		db_printf("  %d at 0x%x: %jx %jx\n", i, frir,
		    (uintmax_t)dmar_read8(unit, frir),
		    (uintmax_t)dmar_read8(unit, frir + 8));
	}
	if (DMAR_HAS_QI(unit)) {
		db_printf("ied 0x%x iea 0x%x ieua 0x%x\n",
		    dmar_read4(unit, DMAR_IEDATA_REG),
		    dmar_read4(unit, DMAR_IEADDR_REG),
		    dmar_read4(unit, DMAR_IEUADDR_REG));
		if (unit->qi_enabled) {
			db_printf("qi is enabled: queue @0x%jx (IQA 0x%jx) "
			    "size 0x%jx\n"
		    "  head 0x%x tail 0x%x avail 0x%x status 0x%x ctrl 0x%x\n"
		    "  hw compl 0x%jx@%p/phys@%jx next seq 0x%x gen 0x%x\n",
			    (uintmax_t)unit->x86c.inv_queue,
			    (uintmax_t)dmar_read8(unit, DMAR_IQA_REG),
			    (uintmax_t)unit->x86c.inv_queue_size,
			    dmar_read4(unit, DMAR_IQH_REG),
			    dmar_read4(unit, DMAR_IQT_REG),
			    unit->x86c.inv_queue_avail,
			    dmar_read4(unit, DMAR_ICS_REG),
			    dmar_read4(unit, DMAR_IECTL_REG),
			    (uintmax_t)unit->x86c.inv_waitd_seq_hw,
			    &unit->x86c.inv_waitd_seq_hw,
			    (uintmax_t)unit->x86c.inv_waitd_seq_hw_phys,
			    unit->x86c.inv_waitd_seq,
			    unit->x86c.inv_waitd_gen);
		} else {
			db_printf("qi is disabled\n");
		}
	}
	if (show_domains) {
		db_printf("domains:\n");
		LIST_FOREACH(domain, &unit->domains, link) {
			dmar_print_domain(domain, show_mappings);
			if (db_pager_quit)
				break;
		}
	}
}

DB_SHOW_COMMAND(dmar, db_dmar_print)
{
	bool show_domains, show_mappings;

	show_domains = strchr(modif, 'd') != NULL;
	show_mappings = strchr(modif, 'm') != NULL;
	if (!have_addr) {
		db_printf("usage: show dmar [/d] [/m] index\n");
		return;
	}
	dmar_print_one((int)addr, show_domains, show_mappings);
}

DB_SHOW_ALL_COMMAND(dmars, db_show_all_dmars)
{
	int i;
	bool show_domains, show_mappings;

	show_domains = strchr(modif, 'd') != NULL;
	show_mappings = strchr(modif, 'm') != NULL;

	for (i = 0; i < dmar_devcnt; i++) {
		dmar_print_one(i, show_domains, show_mappings);
		if (db_pager_quit)
			break;
	}
}
#endif

static struct iommu_unit *
dmar_find_method(device_t dev, bool verbose)
{
	struct dmar_unit *dmar;

	dmar = dmar_find(dev, verbose);
	return (&dmar->iommu);
}

static struct x86_unit_common *
dmar_get_x86_common(struct iommu_unit *unit)
{
	struct dmar_unit *dmar;

	dmar = IOMMU2DMAR(unit);
	return (&dmar->x86c);
}

static void
dmar_unit_pre_instantiate_ctx(struct iommu_unit *unit)
{
	dmar_quirks_pre_use(unit);
	dmar_instantiate_rmrr_ctxs(unit);
}

static struct x86_iommu dmar_x86_iommu = {
	.get_x86_common = dmar_get_x86_common,
	.unit_pre_instantiate_ctx = dmar_unit_pre_instantiate_ctx,
	.domain_unload_entry = dmar_domain_unload_entry,
	.domain_unload = dmar_domain_unload,
	.get_ctx = dmar_get_ctx,
	.free_ctx_locked = dmar_free_ctx_locked_method,
	.find = dmar_find_method,
	.alloc_msi_intr = dmar_alloc_msi_intr,
	.map_msi_intr = dmar_map_msi_intr,
	.unmap_msi_intr = dmar_unmap_msi_intr,
	.map_ioapic_intr = dmar_map_ioapic_intr,
	.unmap_ioapic_intr = dmar_unmap_ioapic_intr,
};

static void
x86_iommu_set_intel(void *arg __unused)
{
	if (cpu_vendor_id == CPU_VENDOR_INTEL)
		set_x86_iommu(&dmar_x86_iommu);
}

SYSINIT(x86_iommu, SI_SUB_TUNABLES, SI_ORDER_ANY, x86_iommu_set_intel, NULL);
