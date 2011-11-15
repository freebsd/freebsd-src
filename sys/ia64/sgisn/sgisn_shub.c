/*-
 * Copyright (c) 2011 Marcel Moolenaar
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/resource.h>
#include <machine/sal.h>
#include <machine/sgisn.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/actables.h>
#include <dev/acpica/acpivar.h>

#include <ia64/sgisn/sgisn_shub.h>

struct sgisn_shub_softc {
	struct sgisn_fwhub *sc_fwhub;
	device_t	sc_dev;
	vm_paddr_t	sc_membase;
	vm_size_t	sc_memsize;
	bus_addr_t	sc_mmraddr;
	bus_space_tag_t sc_tag;
	bus_space_handle_t sc_hndl;
	u_int		sc_domain;
	u_int		sc_hubtype;	/* SHub type (0=SHub1, 1=SHub2) */
	u_int		sc_nasid_mask;
	u_int		sc_nasid_shft;
	u_int		sc_nasid;
};

static int sgisn_shub_attach(device_t);
static void sgisn_shub_identify(driver_t *, device_t);
static int sgisn_shub_probe(device_t);

static int sgisn_shub_activate_resource(device_t, device_t, int, int,
    struct resource *);
static struct resource *sgisn_shub_alloc_resource(device_t, device_t, int,
    int *, u_long, u_long, u_long, u_int);
static void sgisn_shub_delete_resource(device_t, device_t, int, int);
static int sgisn_shub_get_resource(device_t, device_t, int, int, u_long *,
    u_long *);
static int sgisn_shub_read_ivar(device_t, device_t, int, uintptr_t *);
static int sgisn_shub_release_resource(device_t, device_t, int, int,
    struct resource *);
static int sgisn_shub_set_resource(device_t, device_t, int, int, u_long,
    u_long);
static int sgisn_shub_write_ivar(device_t, device_t, int, uintptr_t);

/*
 * Bus interface definitions.
 */
static device_method_t sgisn_shub_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,	sgisn_shub_attach),
	DEVMETHOD(device_identify,	sgisn_shub_identify),
	DEVMETHOD(device_probe,		sgisn_shub_probe),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	sgisn_shub_read_ivar),
	DEVMETHOD(bus_write_ivar,	sgisn_shub_write_ivar),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_activate_resource, sgisn_shub_activate_resource),
	DEVMETHOD(bus_alloc_resource,	sgisn_shub_alloc_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_delete_resource,	sgisn_shub_delete_resource),
	DEVMETHOD(bus_get_resource,	sgisn_shub_get_resource),
	DEVMETHOD(bus_release_resource,	sgisn_shub_release_resource),
	DEVMETHOD(bus_set_resource,	sgisn_shub_set_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static devclass_t sgisn_shub_devclass;
static char sgisn_shub_name[] = "shub";

static driver_t sgisn_shub_driver = {
	sgisn_shub_name,
	sgisn_shub_methods,
	sizeof(struct sgisn_shub_softc),
};


DRIVER_MODULE(shub, nexus, sgisn_shub_driver, sgisn_shub_devclass, 0, 0);

static int
sgisn_shub_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{
	int error;

	error = bus_activate_resource(dev, type, rid, res);
	return (error);
}

static struct resource *
sgisn_shub_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct resource *res;

	res = bus_alloc_resource(dev, type, rid, start, end, count, flags);
	return (res);
}

static void
sgisn_shub_delete_resource(device_t dev, device_t child, int type, int rid)
{
 
	bus_delete_resource(dev, type, rid);
}

static int
sgisn_shub_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	int error;

	error = bus_get_resource(dev, type, rid, startp, countp);
	return (error);
}

static int
sgisn_shub_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	int error;

	error = bus_release_resource(dev, type, rid, r);
	return (error);
}

static int
sgisn_shub_set_resource(device_t dev, device_t child, int type, int rid,
    u_long start, u_long count)
{
	int error;

	error =  bus_set_resource(dev, type, rid, start, count);
	return (error);
}

#if 0
static void
sgisn_shub_dump_sn_info(struct ia64_sal_result *r)
{

	printf("XXX: SHub type: %lu (0=SHub1, 1=SHub2)\n",
	    r->sal_result[0] & 0xff);
	printf("XXX: Max nodes in system: %u\n",
	    1 << ((r->sal_result[0] >> 8) & 0xff));
	printf("XXX: Max nodes in sharing domain: %u\n",
	    1 << ((r->sal_result[0] >> 16) & 0xff));
	printf("XXX: Partition ID: %lu\n", (r->sal_result[0] >> 24) & 0xff);
	printf("XXX: Coherency ID: %lu\n", (r->sal_result[0] >> 32) & 0xff);
	printf("XXX: Region size: %lu\n", (r->sal_result[0] >> 40) & 0xff);

	printf("XXX: NasID mask: %#lx\n", r->sal_result[1] & 0xffff);
	printf("XXX: NasID bit position: %lu\n",
	    (r->sal_result[1] >> 16) & 0xff);

}
#endif

static void
sgisn_shub_identify_srat_cb(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_SRAT_CPU_AFFINITY *cpu;
	ACPI_SRAT_MEM_AFFINITY *mem;
	device_t bus, dev;
	uint32_t domain;

	bus = arg;

	/*
	 * Use all possible entry types for learning about domains.
	 * This probably is highly redundant and could possibly be
	 * wrong, but it seems more harmful to miss a domain than
	 * anything else.
	 */
	domain = 0;
	switch (entry->Type) {
	case ACPI_SRAT_TYPE_CPU_AFFINITY:
		cpu = (ACPI_SRAT_CPU_AFFINITY *)(void *)entry;
		domain = cpu->ProximityDomainLo |
		    cpu->ProximityDomainHi[0] << 8 |
		    cpu->ProximityDomainHi[1] << 16 |
		    cpu->ProximityDomainHi[2] << 24;
		break;
	case ACPI_SRAT_TYPE_MEMORY_AFFINITY:
		mem = (ACPI_SRAT_MEM_AFFINITY *)(void *)entry;
		domain = mem->ProximityDomain;
		break;
	default:
		return;
	}

	/*
	 * We're done if we've already seen the domain.
	 */
	dev = devclass_get_device(sgisn_shub_devclass, domain);
	if (dev != NULL)
		return;

	if (bootverbose)
		printf("%s: new domain %u\n", sgisn_shub_name, domain);

	/*
	 * First encounter of this domain. Add a SHub device with a unit
	 * number equal to the domain number. Order the SHub devices by
	 * unit (and thus domain) number.
	 */
	dev = BUS_ADD_CHILD(bus, domain, sgisn_shub_name, domain);
}

static void
sgisn_shub_identify(driver_t *drv, device_t bus)
{
	struct ia64_sal_result r;
	ACPI_TABLE_HEADER *tbl;
	void *ptr;

	KASSERT(drv == &sgisn_shub_driver, ("%s: driver mismatch", __func__));

	/*
	 * The presence of SHub ASICs is conditional upon the platform
	 * (SGI Altix SN). Check that first...
	 */
	r = ia64_sal_entry(SAL_SGISN_SN_INFO, 0, 0, 0, 0, 0, 0, 0);
	if (r.sal_status != 0)
		return;

#if 0
	if (bootverbose)
		sgisn_shub_dump_sn_info(&r);
#endif

	/*
	 * The number of SHub ASICs is determined by the number of nodes
	 * in the SRAT table.
	 */
	tbl = ptr = acpi_find_table(ACPI_SIG_SRAT);
	if (tbl == NULL) {
		printf("WARNING: no SRAT table found...\n");
		return;
	}

	acpi_walk_subtables((uint8_t *)ptr + sizeof(ACPI_TABLE_SRAT),
	    (uint8_t *)ptr + tbl->Length, sgisn_shub_identify_srat_cb, bus);
}

static int
sgisn_shub_probe(device_t dev)
{
	struct ia64_sal_result r;
	char desc[80];
	u_int v;

	/*
	 * NOTICE: This can only be done on a CPU that's connected to the
	 * FSB of the SHub ASIC. As such, the BSP can only validly probe
	 * the SHub it's connected to.
	 *
	 * In order to probe and attach SHubs in other domains, we need to
	 * defer to some CPU connected to that SHub.
	 *
	 * XXX For now, we assume that SHub types are the same across the
	 * system, so we simply query the SHub in our domain and pretend
	 * we queried the one corresponding to the domain this instance
	 * refers to.
	 */
	r = ia64_sal_entry(SAL_SGISN_SN_INFO, 0, 0, 0, 0, 0, 0, 0);
	if (r.sal_status != 0)
		return (ENXIO);

	v = (r.sal_result[0] & 0xff) + 1;;
	snprintf(desc, sizeof(desc), "SGI SHub%u ASIC", v);
	device_set_desc_copy(dev, desc);
	return (BUS_PROBE_DEFAULT);
}

static void
sgisn_shub_attach_srat_cb(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	device_t dev = arg;
	ACPI_SRAT_MEM_AFFINITY *mem;
	struct sgisn_shub_softc *sc;
 
	if (entry->Type != ACPI_SRAT_TYPE_MEMORY_AFFINITY)
		return;

	sc = device_get_softc(dev);

	mem = (ACPI_SRAT_MEM_AFFINITY *)(void *)entry;
	if (mem->ProximityDomain != sc->sc_domain)
		return;
	if ((mem->Flags & ACPI_SRAT_MEM_ENABLED) == 0)
		return;

	sc->sc_membase = mem->BaseAddress;
	sc->sc_memsize = mem->Length;
}

static int
sgisn_shub_attach(device_t dev)
{
	struct ia64_sal_result r;
	struct sgisn_shub_softc *sc;
	struct sgisn_fwbus *fwbus;
	ACPI_TABLE_HEADER *tbl;
	device_t child;
	void *ptr;
	u_long addr;
	u_int bus, seg, wdgt;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_domain = device_get_unit(dev);

	/*
	 * Get the physical memory region that is connected to the MD I/F
	 * of this SHub. It allows us to allocate memory that's close to
	 * this SHub. Fail the attach if we don't have local memory, as
	 * we really depend on it.
	 */
	tbl = ptr = acpi_find_table(ACPI_SIG_SRAT);
	acpi_walk_subtables((uint8_t *)ptr + sizeof(ACPI_TABLE_SRAT),
	    (uint8_t *)ptr + tbl->Length, sgisn_shub_attach_srat_cb, dev);
	if (sc->sc_memsize == 0)
		return (ENXIO);

	if (bootverbose)
		device_printf(dev, "%#lx bytes of attached memory at %#lx\n",
		    sc->sc_memsize, sc->sc_membase);

	/*
	 * Obtain our NASID.
	 */
	r = ia64_sal_entry(SAL_SGISN_SN_INFO, 0, 0, 0, 0, 0, 0, 0);
	if (r.sal_status != 0)
		return (ENXIO);

	sc->sc_hubtype = r.sal_result[0] & 0xff;
	sc->sc_nasid_mask = r.sal_result[1] & 0xffff;
	sc->sc_nasid_shft = (r.sal_result[1] >> 16) & 0xff;
	sc->sc_nasid = (sc->sc_membase >> sc->sc_nasid_shft) &
	    sc->sc_nasid_mask;

	sc->sc_mmraddr = (sc->sc_nasid << sc->sc_nasid_shft) |
	    (1UL << (sc->sc_nasid_shft - 3)) |
	    (((sc->sc_hubtype == 0) ? 1UL : 0UL) << 32);
	sc->sc_tag = IA64_BUS_SPACE_MEM;
	bus_space_map(sc->sc_tag, sc->sc_mmraddr, 1UL << 32, 0, &sc->sc_hndl);

	if (bootverbose)
		device_printf(dev, "NASID=%#x\n", sc->sc_nasid);

	/*
	 * Allocate contiguous memory, local to the SHub, for collecting
	 * SHub information from the PROM and for discovering the PCI
	 * host controllers connected to the SHub.
	 */
	sc->sc_fwhub = contigmalloc(sizeof(struct sgisn_fwhub), M_DEVBUF,
	    M_ZERO, sc->sc_membase, sc->sc_membase + sc->sc_memsize, 16, 0);

	sc->sc_fwhub->hub_pci_maxseg = 0xffffffff;
	sc->sc_fwhub->hub_pci_maxbus = 0xff;
	r = ia64_sal_entry(SAL_SGISN_IOHUB_INFO, sc->sc_nasid,
	    ia64_tpa((uintptr_t)sc->sc_fwhub), 0, 0, 0, 0, 0);
	if (r.sal_status != 0) {
		contigfree(sc->sc_fwhub, sizeof(struct sgisn_fwhub), M_DEVBUF);
		return (ENXIO);
	}

	for (wdgt = 0; wdgt < SGISN_HUB_NWIDGETS; wdgt++)
		sc->sc_fwhub->hub_widget[wdgt].wgt_hub = sc->sc_fwhub;

	/* Create a child for the SAL-based console. */
	r = ia64_sal_entry(SAL_SGISN_MASTER_NASID, 0, 0, 0, 0, 0, 0, 0);
	if (r.sal_status == 0 && r.sal_result[0] == sc->sc_nasid) {
		child = device_add_child(dev, "sncon", -1);
		device_set_ivars(child, (void *)(uintptr_t)~0UL);
	}

	for (seg = 0; seg <= sc->sc_fwhub->hub_pci_maxseg; seg++) {
		for (bus = 0; bus <= sc->sc_fwhub->hub_pci_maxbus; bus++) {
			r = ia64_sal_entry(SAL_SGISN_IOBUS_INFO, seg, bus,
			    ia64_tpa((uintptr_t)&addr), 0, 0, 0, 0);
			if (r.sal_status != 0 || addr == 0)
				continue;

			fwbus = (void *)IA64_PHYS_TO_RR7(addr);
			if (((fwbus->bus_base >> sc->sc_nasid_shft) &
			    sc->sc_nasid_mask) != sc->sc_nasid)
				continue;

			child = device_add_child(dev, "pcib", -1);
			device_set_ivars(child,
			    (void *)(uintptr_t) ((seg << 8) | (bus & 0xff)));
		}
	}

	return (bus_generic_attach(dev));
}

static int
sgisn_shub_read_ivar(device_t dev, device_t child, int which, uintptr_t *res)
{
	uintptr_t ivars;

	ivars = (uintptr_t)device_get_ivars(child);
	switch (which) {
	case SHUB_IVAR_PCIBUS:
		*res = ivars & 0xff;
		return (0);
	case SHUB_IVAR_PCISEG:
		*res = ivars >> 8;
		return (0);
	}
	return (ENOENT);
}

static int
sgisn_shub_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct sgisn_shub_softc *sc = device_get_softc(dev);
	uint64_t ev;

	if (which != SHUB_IVAR_EVENT)
		return (ENOENT);

	ev = bus_space_read_8(sc->sc_tag, sc->sc_hndl, SHUB_MMR_EVENT);
	if (ev & value)
		bus_space_write_8(sc->sc_tag, sc->sc_hndl, SHUB_MMR_EVENT_WR,
		    value);
	device_printf(dev, "XXX: %s: child=%p, event=%lx, mask=%lx\n",
	    __func__, child, ev, value);
	return (0);
}
