/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 */

/*
 * intel_pmc -- Intel Performance Management Controller (PMC) driver
 *
 * Exposes information and controls from the PMC of Intel platforms.
 *
 * The driver reports the cumulative time spent in low power mode
 * (S0ix, Modern Standby) while in S0, and lets the administrator
 * force the PMC to ignore IP blocks whose Latency Tolerance
 * Reporting (LTR) prevents the platform from entering S0ix.
 *
 * On Sunrise Point (Skylake/Kaby Lake) the PMC is a PCI function and
 * the PWRM register space is mapped via the PWRM_BASE register at PCI
 * config offset 0x48, not through BAR0.  See Intel 100 Series Chipset
 * Family PCH Datasheet Vol 2, section 4.3.53.
 *
 * From Cannon Lake on the PMC is not PCI-enumerable: the platform is
 * identified by CPU model and PWRM is mapped at 0xFE000000.  A
 * residency counter reading all-ones fails the attach.  The LPIT
 * residency counter address is compared against PWRM + SLP_S0_RES
 * and a mismatch is reported.
 *
 * The SLP_S0_RESIDENCY counter increments while the PCH SLP_S0
 * signal is asserted.  SLP_S0 asserts when the PCH is idle and the
 * processor is in C10 state, with ModPhy lanes power-gated and PLLs
 * idle (Linux commit b740d2e9233cb).  The counter is 32-bit; the
 * tick period varies by generation (see the platform table).
 *
 * LTR_IGNORE has one bit per IP block.  Setting a bit tells the PMC
 * to ignore that IP's LTR requirements, allowing S0ix entry even if
 * the IP would otherwise block it.  A clear bit means the IP's LTR
 * requirements are considered and can prevent S0ix.
 *
 * SPT bit assignments (0-16):
 *   0=SOUTHPORT_A     1=SOUTHPORT_B     2=SATA
 *   3=GIGABIT_ETHERNET 4=XHCI           5=reserved
 *   6=ME              7=EVA             8=SOUTHPORT_C
 *   9=HD_AUDIO       10=LPSS           11=SOUTHPORT_D
 *  12=SOUTHPORT_E    13=CAMERA         14=ESPI
 *  15=SCC            16=ISH
 * Later generations widen the mask; see ltr_ignore_bits below.
 *
 * Ref: Intel 100 Series Chipset Family PCH Datasheet Vol 2, section 4.3
 * Ref: Intel PCH Family Datasheet Vol 2 per generation (SLP_S0_RES)
 * Ref: Linux commit 9c2ee19987ef (initial SPT PMC support)
 * Ref: Linux commit 2eb150558bb7 (LTR IP block documentation)
 */

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/resource.h>
#include <machine/specialreg.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

/*
 * Sunrise Point (SPT) PMC PCI config registers.
 * Reference: Intel 100 Series Chipset Family PCH Datasheet Vol 2.
 */
#define SPT_PMC_ACTL_OFFSET		0x44	/* ACPI Control */
#define SPT_PMC_ACTL_PWRM_EN		(1u << 8) /* PWRM decode enabled */
#define SPT_PMC_PWRM_BASE_OFFSET	0x48	/* PCI config offset for PWRM */
#define SPT_PMC_PWRM_BASE_MASK		0xfffff000

/* Architectural PWRM base for the PCI-less generations. */
#define INTEL_PMC_PWRM_BASE_DEFAULT	0xfe000000

/*
 * Per-generation PWRM register layout.  SLP_S0_RES offsets and
 * granularities per each generation's PCH Family Datasheet Volume 2.
 * slp_s0_step is the counter tick period in nanoseconds.
 */
struct intel_pmc_platform {
	const char	*desc;
	uint32_t	slp_s0_offset;
	uint32_t	slp_s0_step;
	uint32_t	ltr_ignore_offset;
	uint32_t	ltr_ignore_bits;
	uint32_t	mmio_len;
};

static const struct intel_pmc_platform intel_pmc_spt = {
	.desc = "Sunrise Point",
	.slp_s0_offset = 0x13c,
	.slp_s0_step = 100000,
	.ltr_ignore_offset = 0x30c,
	.ltr_ignore_bits = 17,
	.mmio_len = 0x1000,
};

static const struct intel_pmc_platform intel_pmc_cnp = {
	.desc = "Cannon Point",
	.slp_s0_offset = 0x193c,
	.slp_s0_step = 100000,
	.ltr_ignore_offset = 0x1b0c,
	.ltr_ignore_bits = 19,
	.mmio_len = 0x2000,
};

static const struct intel_pmc_platform intel_pmc_icl = {
	.desc = "Ice Lake",
	.slp_s0_offset = 0x193c,
	.slp_s0_step = 100000,
	.ltr_ignore_offset = 0x1b0c,
	.ltr_ignore_bits = 20,
	.mmio_len = 0x2000,
};

static const struct intel_pmc_platform intel_pmc_tgl = {
	.desc = "Tiger Lake",
	.slp_s0_offset = 0x193c,
	.slp_s0_step = 122000,
	.ltr_ignore_offset = 0x1b0c,
	.ltr_ignore_bits = 23,
	.mmio_len = 0x2000,
};

static const struct intel_pmc_platform intel_pmc_adl = {
	.desc = "Alder Lake",
	.slp_s0_offset = 0x1098,
	.slp_s0_step = 30500,
	.ltr_ignore_offset = 0x1b0c,
	.ltr_ignore_bits = 23,
	.mmio_len = 0x2000,
};

static const struct intel_pmc_platform intel_pmc_mtl = {
	.desc = "Meteor Lake",
	.slp_s0_offset = 0x193c,
	.slp_s0_step = 122000,
	.ltr_ignore_offset = 0x1b0c,
	.ltr_ignore_bits = 25,
	.mmio_len = 0x2708,
};

/* Family 6 CPU models for the PCI-less generations. */
static const struct intel_pmc_cpu_match {
	uint32_t			model;
	const struct intel_pmc_platform	*plat;
} intel_pmc_cpu_models[] = {
	{ 0x66, &intel_pmc_cnp },	/* Cannon Lake-L */
	{ 0xa5, &intel_pmc_cnp },	/* Comet Lake */
	{ 0xa6, &intel_pmc_cnp },	/* Comet Lake-L */
	{ 0x7e, &intel_pmc_icl },	/* Ice Lake-L */
	{ 0x9c, &intel_pmc_icl },	/* Jasper Lake */
	{ 0x8c, &intel_pmc_tgl },	/* Tiger Lake-L */
	{ 0x8d, &intel_pmc_tgl },	/* Tiger Lake */
	{ 0x96, &intel_pmc_tgl },	/* Elkhart Lake */
	{ 0xa7, &intel_pmc_tgl },	/* Rocket Lake */
	{ 0x9a, &intel_pmc_tgl },	/* Alder Lake-L */
	{ 0xbe, &intel_pmc_tgl },	/* Alder Lake-N */
	{ 0xba, &intel_pmc_tgl },	/* Raptor Lake-P */
	{ 0x97, &intel_pmc_adl },	/* Alder Lake */
	{ 0xb7, &intel_pmc_adl },	/* Raptor Lake */
	{ 0xbf, &intel_pmc_adl },	/* Raptor Lake-S */
	{ 0xaa, &intel_pmc_mtl },	/* Meteor Lake-L */
};

struct intel_pmc_softc {
	bus_space_tag_t			bst;
	bus_space_handle_t		bsh;
	const struct intel_pmc_platform	*plat;
};

static const struct pci_device_table intel_pmc_devices[] = {
	{ PCI_DEV(0x8086, 0x9d21), PCI_DESCR("Sunrise Point-LP PMC")},
	{ PCI_DEV(0x8086, 0xa121), PCI_DESCR("Sunrise Point-H PMC")},
};

static inline uint32_t
intel_pmc_read(struct intel_pmc_softc *sc, uint32_t offset)
{

	return (bus_space_read_4(sc->bst, sc->bsh, offset));
}

static inline void
intel_pmc_write(struct intel_pmc_softc *sc, uint32_t offset, uint32_t val)
{

	bus_space_write_4(sc->bst, sc->bsh, offset, val);
}

static int
intel_pmc_slp_s0_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct intel_pmc_softc *sc = oidp->oid_arg1;
	uint64_t val, usec;

	val = intel_pmc_read(sc, sc->plat->slp_s0_offset);
	usec = val * sc->plat->slp_s0_step / 1000;
	return (sysctl_handle_64(oidp, &usec, 0, req));
}

static int
intel_pmc_ltr_ignore_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct intel_pmc_softc *sc = oidp->oid_arg1;
	uint32_t val;
	int error;

	val = intel_pmc_read(sc, sc->plat->ltr_ignore_offset);
	error = sysctl_handle_32(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	val &= (1u << sc->plat->ltr_ignore_bits) - 1;
	intel_pmc_write(sc, sc->plat->ltr_ignore_offset, val);
	return (0);
}

static int
intel_pmc_attach_common(device_t dev, vm_paddr_t pwrm_base)
{
	struct intel_pmc_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;

	sc->bst = X86_BUS_SPACE_MEM;
	sc->bsh = (bus_space_handle_t)pmap_mapdev(pwrm_base,
	    sc->plat->mmio_len);

	if (intel_pmc_read(sc, sc->plat->slp_s0_offset) == 0xffffffff) {
		device_printf(dev, "PWRM does not decode at %#jx\n",
		    (uintmax_t)pwrm_base);
		pmap_unmapdev((void *)sc->bsh, sc->plat->mmio_len);
		return (ENXIO);
	}

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "ltr_ignore",
	    CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, intel_pmc_ltr_ignore_sysctl, "IU",
	    "LTR ignore mask: set bit = ignore IP, allowing S0ix");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "slp_s0_residency",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    sc, 0, intel_pmc_slp_s0_sysctl, "QU",
	    "SLP_S0 residency time in microseconds");

	return (0);
}

static int
intel_pmc_detach(device_t dev)
{
	struct intel_pmc_softc *sc = device_get_softc(dev);

	pmap_unmapdev((void *)sc->bsh, sc->plat->mmio_len);
	return (0);
}

static int
intel_pmc_pci_probe(device_t dev)
{
	const struct pci_device_table *tbl;

	if (device_get_unit(dev) != 0)
		return (ENXIO);
	tbl = PCI_MATCH(dev, intel_pmc_devices);
	if (tbl == NULL)
		return (ENXIO);
	device_set_desc(dev, tbl->descr);
	return (BUS_PROBE_DEFAULT);
}

static int
intel_pmc_pci_attach(device_t dev)
{
	struct intel_pmc_softc *sc = device_get_softc(dev);
	vm_paddr_t pwrm_base;

	if ((pci_read_config(dev, SPT_PMC_ACTL_OFFSET, 4) &
	    SPT_PMC_ACTL_PWRM_EN) == 0) {
		device_printf(dev, "PWRM decode not enabled by firmware\n");
		return (ENXIO);
	}
	pwrm_base = pci_read_config(dev, SPT_PMC_PWRM_BASE_OFFSET, 4) &
	    SPT_PMC_PWRM_BASE_MASK;

	sc->plat = &intel_pmc_spt;
	return (intel_pmc_attach_common(dev, pwrm_base));
}

static const struct intel_pmc_platform *
intel_pmc_cpu_platform(void)
{

	if (cpu_vendor_id != CPU_VENDOR_INTEL ||
	    CPUID_TO_FAMILY(cpu_id) != 6)
		return (NULL);
	for (u_int i = 0; i < nitems(intel_pmc_cpu_models); i++) {
		if (intel_pmc_cpu_models[i].model == CPUID_TO_MODEL(cpu_id))
			return (intel_pmc_cpu_models[i].plat);
	}
	return (NULL);
}

/*
 * Report whether any LPIT native C-state entry describes a residency
 * counter at want.  Firmware may describe several counters, so a
 * mismatch is only meaningful once every entry has been considered.
 * first receives the address of the first entry examined.
 */
static int
intel_pmc_lpit_match(uint64_t want, uint64_t *first)
{
	ACPI_TABLE_LPIT *lpit;
	ACPI_LPIT_HEADER *hdr;
	ACPI_LPIT_NATIVE *nat;
	ACPI_STATUS status;
	uint64_t addr;
	char *p, *end;
	int nentries;

	status = AcpiGetTable(ACPI_SIG_LPIT, 0, (ACPI_TABLE_HEADER **)&lpit);
	if (ACPI_FAILURE(status))
		return (ENOENT);

	nentries = 0;
	p = (char *)(lpit + 1);
	end = (char *)lpit + lpit->Header.Length;
	while (p + sizeof(*hdr) <= end) {
		hdr = (ACPI_LPIT_HEADER *)p;
		if (hdr->Length == 0 || p + hdr->Length > end)
			break;
		if (hdr->Type == ACPI_LPIT_TYPE_NATIVE_CSTATE &&
		    (hdr->Flags & (ACPI_LPIT_STATE_DISABLED |
		    ACPI_LPIT_NO_COUNTER)) == 0 &&
		    hdr->Length >= sizeof(*nat)) {
			nat = (ACPI_LPIT_NATIVE *)p;
			addr = nat->ResidencyCounter.Address;
			if (nat->ResidencyCounter.SpaceId ==
			    ACPI_ADR_SPACE_SYSTEM_MEMORY && addr != 0) {
				if (addr == want)
					return (0);
				if (nentries++ == 0)
					*first = addr;
			}
		}
		p += hdr->Length;
	}
	return (nentries == 0 ? ENOENT : EINVAL);
}

static void
intel_pmc_acpi_identify(driver_t *driver, device_t parent)
{

	if (intel_pmc_cpu_platform() == NULL)
		return;
	if (device_find_child(parent, "intel_pmc", -1) != NULL)
		return;
	if (BUS_ADD_CHILD(parent, 10, "intel_pmc", -1) == NULL)
		device_printf(parent, "add intel_pmc child failed\n");
}

static int
intel_pmc_acpi_probe(device_t dev)
{
	const struct intel_pmc_platform *plat;
	char desc[64];

	if (acpi_disabled("intel_pmc"))
		return (ENXIO);
	if (device_get_unit(dev) != 0)
		return (ENXIO);
	/* Only the handle-less child added by identify. */
	if (acpi_get_handle(dev) != NULL)
		return (ENXIO);
	plat = intel_pmc_cpu_platform();
	if (plat == NULL)
		return (ENXIO);
	snprintf(desc, sizeof(desc), "%s PMC", plat->desc);
	device_set_desc_copy(dev, desc);
	return (BUS_PROBE_DEFAULT);
}

static int
intel_pmc_acpi_attach(device_t dev)
{
	struct intel_pmc_softc *sc = device_get_softc(dev);
	uint64_t slp_s0_addr;
	vm_paddr_t pwrm_base;

	sc->plat = intel_pmc_cpu_platform();
	pwrm_base = INTEL_PMC_PWRM_BASE_DEFAULT;
	slp_s0_addr = 0;
	if (intel_pmc_lpit_match(pwrm_base + sc->plat->slp_s0_offset,
	    &slp_s0_addr) == EINVAL)
		device_printf(dev,
		    "LPIT counter %#jx differs from PWRM+%#x\n",
		    (uintmax_t)slp_s0_addr, sc->plat->slp_s0_offset);

	return (intel_pmc_attach_common(dev, pwrm_base));
}

static device_method_t intel_pmc_pci_methods[] = {
	DEVMETHOD(device_probe,		intel_pmc_pci_probe),
	DEVMETHOD(device_attach,	intel_pmc_pci_attach),
	DEVMETHOD(device_detach,	intel_pmc_detach),
	DEVMETHOD_END
};

static driver_t intel_pmc_pci_driver = {
	"intel_pmc",
	intel_pmc_pci_methods,
	sizeof(struct intel_pmc_softc)
};

static device_method_t intel_pmc_acpi_methods[] = {
	DEVMETHOD(device_identify,	intel_pmc_acpi_identify),
	DEVMETHOD(device_probe,		intel_pmc_acpi_probe),
	DEVMETHOD(device_attach,	intel_pmc_acpi_attach),
	DEVMETHOD(device_detach,	intel_pmc_detach),
	DEVMETHOD_END
};

static driver_t intel_pmc_acpi_driver = {
	"intel_pmc",
	intel_pmc_acpi_methods,
	sizeof(struct intel_pmc_softc)
};

DRIVER_MODULE(intel_pmc, pci, intel_pmc_pci_driver, 0, 0);
DRIVER_MODULE(intel_pmc, acpi, intel_pmc_acpi_driver, 0, 0);
PCI_PNP_INFO(intel_pmc_devices);
MODULE_VERSION(intel_pmc, 1);
MODULE_DEPEND(intel_pmc, pci, 1, 1, 1);
MODULE_DEPEND(intel_pmc, acpi, 1, 1, 1);
