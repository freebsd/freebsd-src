/*-
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/*
 * This code implements a `root nexus' for Intel Architecture
 * machines.  The function of the root nexus is to serve as an
 * attachment point for both processors and buses, and to manage
 * resources which are common to all of them.  In particular,
 * this code implements the core resource managers for interrupt
 * requests, DMA requests (which rightfully should be a part of the
 * ISA code but it's easier to do it here for now), I/O port addresses,
 * and I/O memory address space.
 */

#ifdef __amd64__
#define	DEV_APIC
#else
#include "opt_apic.h"
#endif
#include "opt_isa.h"
#include "opt_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/vm_dumpset.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/nexusvar.h>
#include <machine/pc/bios.h>
#include <machine/resource.h>

#ifdef DEV_APIC
#include "pcib_if.h"
#endif

#ifdef DEV_ISA
#include <isa/isavar.h>
#include <isa/isareg.h>
#endif

static MALLOC_DEFINE(M_NEXUSDEV, "nexusdev", "Nexus device");

#define DEVTONX(dev)	((struct nexus_device *)device_get_ivars(dev))

struct rman irq_rman, drq_rman, port_rman, mem_rman;

static int nexus_print_all_resources(device_t dev);

static device_probe_t		nexus_probe;
static device_attach_t		nexus_attach;

static bus_add_child_t		nexus_add_child;
static bus_print_child_t	nexus_print_child;

static bus_alloc_resource_t	nexus_alloc_resource;
static bus_get_resource_list_t	nexus_get_reslist;
static bus_get_rman_t		nexus_get_rman;
static bus_map_resource_t	nexus_map_resource;
static bus_unmap_resource_t	nexus_unmap_resource;

#ifdef SMP
static bus_bind_intr_t		nexus_bind_intr;
#endif
static bus_config_intr_t	nexus_config_intr;
static bus_describe_intr_t	nexus_describe_intr;
static bus_resume_intr_t	nexus_resume_intr;
static bus_setup_intr_t		nexus_setup_intr;
static bus_suspend_intr_t	nexus_suspend_intr;
static bus_teardown_intr_t	nexus_teardown_intr;

static bus_get_cpus_t		nexus_get_cpus;

#if defined(DEV_APIC) && defined(DEV_PCI)
static pcib_alloc_msi_t		nexus_alloc_msi;
static pcib_release_msi_t	nexus_release_msi;
static pcib_alloc_msix_t	nexus_alloc_msix;
static pcib_release_msix_t	nexus_release_msix;
static pcib_map_msi_t		nexus_map_msi;
#endif

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	nexus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	nexus_print_child),
	DEVMETHOD(bus_add_child,	nexus_add_child),
	DEVMETHOD(bus_activate_resource, bus_generic_rman_activate_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_rman_adjust_resource),
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_rman_deactivate_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_get_resource_list, nexus_get_reslist),
	DEVMETHOD(bus_get_rman,		nexus_get_rman),
	DEVMETHOD(bus_delete_resource,	bus_generic_rl_delete_resource),
	DEVMETHOD(bus_map_resource,	nexus_map_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rman_release_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_unmap_resource,	nexus_unmap_resource),
#ifdef SMP
	DEVMETHOD(bus_bind_intr,	nexus_bind_intr),
#endif
	DEVMETHOD(bus_config_intr,	nexus_config_intr),
	DEVMETHOD(bus_describe_intr,	nexus_describe_intr),
	DEVMETHOD(bus_resume_intr,	nexus_resume_intr),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_suspend_intr,	nexus_suspend_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_get_cpus,		nexus_get_cpus),

	/* pcib interface */
#if defined(DEV_APIC) && defined(DEV_PCI)
	DEVMETHOD(pcib_alloc_msi,	nexus_alloc_msi),
	DEVMETHOD(pcib_release_msi,	nexus_release_msi),
	DEVMETHOD(pcib_alloc_msix,	nexus_alloc_msix),
	DEVMETHOD(pcib_release_msix,	nexus_release_msix),
	DEVMETHOD(pcib_map_msi,		nexus_map_msi),
#endif
	DEVMETHOD_END
};

DEFINE_CLASS_0(nexus, nexus_driver, nexus_methods, 1);

DRIVER_MODULE(nexus, root, nexus_driver, 0, 0);

static int
nexus_probe(device_t dev)
{

	device_quiet(dev);	/* suppress attach message for neatness */
	return (BUS_PROBE_GENERIC);
}

void
nexus_init_resources(void)
{
	int irq;

	/*
	 * XXX working notes:
	 *
	 * - IRQ resource creation should be moved to the PIC/APIC driver.
	 * - DRQ resource creation should be moved to the DMAC driver.
	 * - The above should be sorted to probe earlier than any child buses.
	 *
	 * - Leave I/O and memory creation here, as child probes may need them.
	 *   (especially eg. ACPI)
	 */

	/*
	 * IRQ's are on the mainboard on old systems, but on the ISA part
	 * of PCI->ISA bridges.  There would be multiple sets of IRQs on
	 * multi-ISA-bus systems.  PCI interrupts are routed to the ISA
	 * component, so in a way, PCI can be a partial child of an ISA bus(!).
	 * APIC interrupts are global though.
	 */
	irq_rman.rm_start = 0;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupt request lines";
	irq_rman.rm_end = num_io_irqs - 1;
	if (rman_init(&irq_rman))
		panic("nexus_init_resources irq_rman");

	/*
	 * We search for regions of existing IRQs and add those to the IRQ
	 * resource manager.
	 */
	for (irq = 0; irq < num_io_irqs; irq++)
		if (intr_lookup_source(irq) != NULL)
			if (rman_manage_region(&irq_rman, irq, irq) != 0)
				panic("nexus_init_resources irq_rman add");

	/*
	 * ISA DMA on PCI systems is implemented in the ISA part of each
	 * PCI->ISA bridge and the channels can be duplicated if there are
	 * multiple bridges.  (eg: laptops with docking stations)
	 */
	drq_rman.rm_start = 0;
	drq_rman.rm_end = 7;
	drq_rman.rm_type = RMAN_ARRAY;
	drq_rman.rm_descr = "DMA request lines";
	/* XXX drq 0 not available on some machines */
	if (rman_init(&drq_rman)
	    || rman_manage_region(&drq_rman,
				  drq_rman.rm_start, drq_rman.rm_end))
		panic("nexus_init_resources drq_rman");

	/*
	 * However, IO ports and Memory truely are global at this level,
	 * as are APIC interrupts (however many IO APICS there turn out
	 * to be on large systems..)
	 */
	port_rman.rm_start = 0;
	port_rman.rm_end = 0xffff;
	port_rman.rm_type = RMAN_ARRAY;
	port_rman.rm_descr = "I/O ports";
	if (rman_init(&port_rman)
	    || rman_manage_region(&port_rman, 0, 0xffff))
		panic("nexus_init_resources port_rman");

	mem_rman.rm_start = 0;
	mem_rman.rm_end = cpu_getmaxphyaddr();
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory addresses";
	if (rman_init(&mem_rman)
	    || rman_manage_region(&mem_rman, 0, mem_rman.rm_end))
		panic("nexus_init_resources mem_rman");
}

static int
nexus_attach(device_t dev)
{

	nexus_init_resources();
	bus_identify_children(dev);

	/*
	 * Explicitly add the legacy0 device here.  Other platform
	 * types (such as ACPI), use their own nexus(4) subclass
	 * driver to override this routine and add their own root bus.
	 */
	if (BUS_ADD_CHILD(dev, 10, "legacy", 0) == NULL)
		panic("legacy: could not attach");
	bus_attach_children(dev);
	return (0);
}

static int
nexus_print_all_resources(device_t dev)
{
	struct	nexus_device *ndev = DEVTONX(dev);
	struct resource_list *rl = &ndev->nx_resources;
	int retval = 0;

	if (STAILQ_FIRST(rl))
		retval += printf(" at");

	retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#jx");
	retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#jx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%jd");

	return (retval);
}

static int
nexus_print_child(device_t bus, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += nexus_print_all_resources(child);
	if (device_get_flags(child))
		retval += printf(" flags %#x", device_get_flags(child));
	retval += printf("\n");

	return (retval);
}

static device_t
nexus_add_child(device_t bus, u_int order, const char *name, int unit)
{
	device_t		child;
	struct nexus_device	*ndev;

	ndev = malloc(sizeof(struct nexus_device), M_NEXUSDEV, M_NOWAIT|M_ZERO);
	if (!ndev)
		return (0);
	resource_list_init(&ndev->nx_resources);

	child = device_add_child_ordered(bus, order, name, unit);

	/* should we free this in nexus_child_detached? */
	device_set_ivars(child, ndev);

	return (child);
}

static struct rman *
nexus_get_rman(device_t bus, int type, u_int flags)
{
	switch (type) {
	case SYS_RES_IRQ:
		return (&irq_rman);
	case SYS_RES_DRQ:
		return (&drq_rman);
	case SYS_RES_IOPORT:
		return (&port_rman);
	case SYS_RES_MEMORY:
		return (&mem_rman);
	default:
		return (NULL);
	}
}

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of nexus0.
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     rman_res_t start, rman_res_t end, rman_res_t count,
		     u_int flags)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct resource_list_entry *rle;

	/*
	 * If this is an allocation of the "default" range for a given
	 * RID, and we know what the resources for this device are
	 * (ie. they aren't maintained by a child bus), then work out
	 * the start/end values.
	 */
	if (RMAN_IS_DEFAULT_RANGE(start, end) && (count == 1)) {
		if (device_get_parent(child) != bus || ndev == NULL)
			return (NULL);
		rle = resource_list_find(&ndev->nx_resources, type, *rid);
		if (rle == NULL)
			return (NULL);
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	return (bus_generic_rman_alloc_resource(bus, child, type, rid,
	    start, end, count, flags));
}

static int
nexus_map_resource(device_t bus, device_t child, struct resource *r,
    struct resource_map_request *argsp, struct resource_map *map)
{
	struct resource_map_request args;
	rman_res_t length, start;
	int error, type;

	/* Resources must be active to be mapped. */
	if (!(rman_get_flags(r) & RF_ACTIVE))
		return (ENXIO);

	/* Mappings are only supported on I/O and memory resources. */
	type = rman_get_type(r);
	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		break;
	default:
		return (EINVAL);
	}

	resource_init_map_request(&args);
	error = resource_validate_map_request(r, argsp, &args, &start, &length);
	if (error)
		return (error);

	/*
	 * If this is a memory resource, map it into the kernel.
	 */
	switch (type) {
	case SYS_RES_IOPORT:
		map->r_bushandle = start;
		map->r_bustag = X86_BUS_SPACE_IO;
		map->r_size = length;
		map->r_vaddr = NULL;
		break;
	case SYS_RES_MEMORY:
		map->r_vaddr = pmap_mapdev_attr(start, length, args.memattr);
		map->r_bustag = X86_BUS_SPACE_MEM;
		map->r_size = length;

		/*
		 * The handle is the virtual address.
		 */
		map->r_bushandle = (bus_space_handle_t)map->r_vaddr;
		break;
	}
	return (0);
}

static int
nexus_unmap_resource(device_t bus, device_t child, struct resource *r,
    struct resource_map *map)
{

	/*
	 * If this is a memory resource, unmap it.
	 */
	switch (rman_get_type(r)) {
	case SYS_RES_MEMORY:
		pmap_unmapdev(map->r_vaddr, map->r_size);
		/* FALLTHROUGH */
	case SYS_RES_IOPORT:
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

/*
 * Currently this uses the really grody interface from kern/kern_intr.c
 * (which really doesn't belong in kern/anything.c).  Eventually, all of
 * the code in kern_intr.c and machdep_intr.c should get moved here, since
 * this is going to be the official interface.
 */
static int
nexus_setup_intr(device_t bus, device_t child, struct resource *irq,
		 int flags, driver_filter_t filter, void (*ihand)(void *),
		 void *arg, void **cookiep)
{
	int		error, domain;
	struct intsrc	*isrc;

	/* somebody tried to setup an irq that failed to allocate! */
	if (irq == NULL)
		panic("nexus_setup_intr: NULL irq resource!");

	*cookiep = NULL;
	if ((rman_get_flags(irq) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(irq);
	if (error != 0)
		return (error);
	if (bus_get_domain(child, &domain) != 0)
		domain = 0;

	isrc = intr_lookup_source(rman_get_start(irq));
	if (isrc == NULL)
		return (EINVAL);
	error = intr_add_handler(isrc, device_get_nameunit(child),
	    filter, ihand, arg, flags, cookiep, domain);
	if (error == 0)
		rman_set_irq_cookie(irq, *cookiep);

	return (error);
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *r, void *ih)
{
	int error;

	error = intr_remove_handler(ih);
	if (error == 0)
		rman_set_irq_cookie(r, NULL);
	return (error);
}

static int
nexus_suspend_intr(device_t dev, device_t child, struct resource *irq)
{
	return (intr_event_suspend_handler(rman_get_irq_cookie(irq)));
}

static int
nexus_resume_intr(device_t dev, device_t child, struct resource *irq)
{
	return (intr_event_resume_handler(rman_get_irq_cookie(irq)));
}

#ifdef SMP
static int
nexus_bind_intr(device_t dev, device_t child, struct resource *irq, int cpu)
{
	struct intsrc *isrc;

	isrc = intr_lookup_source(rman_get_start(irq));
	if (isrc == NULL)
		return (EINVAL);
	return (intr_event_bind(isrc->is_event, cpu));
}
#endif

static int
nexus_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	struct intsrc *isrc;

	isrc = intr_lookup_source(irq);
	if (isrc == NULL)
		return (EINVAL);
	return (intr_config_intr(isrc, trig, pol));
}

static int
nexus_describe_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie, const char *descr)
{
	struct intsrc *isrc;

	isrc = intr_lookup_source(rman_get_start(irq));
	if (isrc == NULL)
		return (EINVAL);
	return (intr_describe(isrc, cookie, descr));
}

static struct resource_list *
nexus_get_reslist(device_t dev, device_t child)
{
	struct nexus_device *ndev = DEVTONX(child);

	return (&ndev->nx_resources);
}

static int
nexus_get_cpus(device_t dev, device_t child, enum cpu_sets op, size_t setsize,
    cpuset_t *cpuset)
{

	switch (op) {
#ifdef SMP
	case INTR_CPUS:
		if (setsize != sizeof(cpuset_t))
			return (EINVAL);
		*cpuset = intr_cpus;
		return (0);
#endif
	default:
		return (bus_generic_get_cpus(dev, child, op, setsize, cpuset));
	}
}

/* Called from the MSI code to add new IRQs to the IRQ rman. */
void
nexus_add_irq(u_long irq)
{

	if (rman_manage_region(&irq_rman, irq, irq) != 0)
		panic("%s: failed", __func__);
}

#if defined(DEV_APIC) && defined(DEV_PCI)
static int
nexus_alloc_msix(device_t pcib, device_t dev, int *irq)
{

	return (msix_alloc(dev, irq));
}

static int
nexus_release_msix(device_t pcib, device_t dev, int irq)
{

	return (msix_release(irq));
}

static int
nexus_alloc_msi(device_t pcib, device_t dev, int count, int maxcount, int *irqs)
{

	return (msi_alloc(dev, count, maxcount, irqs));
}

static int
nexus_release_msi(device_t pcib, device_t dev, int count, int *irqs)
{

	return (msi_release(irqs, count));
}

static int
nexus_map_msi(device_t pcib, device_t dev, int irq, uint64_t *addr, uint32_t *data)
{

	return (msi_map(irq, addr, data));
}
#endif /* DEV_APIC && DEV_PCI */

/* Placeholder for system RAM. */
static void
ram_identify(driver_t *driver, device_t parent)
{

	if (resource_disabled("ram", 0))
		return;	
	if (BUS_ADD_CHILD(parent, 0, "ram", 0) == NULL)
		panic("ram_identify");
}

static int
ram_probe(device_t dev)
{

	device_quiet(dev);
	device_set_desc(dev, "System RAM");
	return (0);
}

static int
ram_attach(device_t dev)
{
	struct bios_smap *smapbase, *smap, *smapend;
	struct resource *res;
	rman_res_t length;
	vm_paddr_t *p;
	uint32_t smapsize;
	int error, rid;

	/* Retrieve the system memory map from the loader. */
	smapbase = (struct bios_smap *)preload_search_info(preload_kmdp,
	    MODINFO_METADATA | MODINFOMD_SMAP);
	if (smapbase != NULL) {
		smapsize = *((u_int32_t *)smapbase - 1);
		smapend = (struct bios_smap *)((uintptr_t)smapbase + smapsize);

		rid = 0;
		for (smap = smapbase; smap < smapend; smap++) {
			if (smap->type != SMAP_TYPE_MEMORY ||
			    smap->length == 0)
				continue;
			if (smap->base > mem_rman.rm_end)
				continue;
			length = smap->base + smap->length > mem_rman.rm_end ?
			    mem_rman.rm_end - smap->base : smap->length;
			error = bus_set_resource(dev, SYS_RES_MEMORY, rid,
			    smap->base, length);
			if (error)
				panic(
				    "ram_attach: resource %d failed set with %d",
				    rid, error);
			res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
			    0);
			if (res == NULL)
				panic("ram_attach: resource %d failed to attach",
				    rid);
			rid++;
		}
		return (0);
	}

	/*
	 * If the system map is not available, fall back to using
	 * dump_avail[].  We use the dump_avail[] array rather than
	 * phys_avail[] for the memory map as phys_avail[] contains
	 * holes for kernel memory, page 0, the message buffer, and
	 * the dcons buffer.  We test the end address in the loop
	 * instead of the start since the start address for the first
	 * segment is 0.
	 */
	for (rid = 0, p = dump_avail; p[1] != 0; rid++, p += 2) {
		if (p[0] > mem_rman.rm_end)
			break;
		length = (p[1] > mem_rman.rm_end ? mem_rman.rm_end : p[1]) -
		    p[0];
		error = bus_set_resource(dev, SYS_RES_MEMORY, rid, p[0],
		    length);
		if (error)
			panic("ram_attach: resource %d failed set with %d", rid,
			    error);
		res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 0);
		if (res == NULL)
			panic("ram_attach: resource %d failed to attach", rid);
	}
	return (0);
}

static device_method_t ram_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	ram_identify),
	DEVMETHOD(device_probe,		ram_probe),
	DEVMETHOD(device_attach,	ram_attach),

	DEVMETHOD_END
};

static driver_t ram_driver = {
	"ram",
	ram_methods,
	1,		/* no softc */
};

DRIVER_MODULE(ram, nexus, ram_driver, 0, 0);

#ifdef DEV_ISA
/*
 * Placeholder which claims PnP 'devices' which describe system
 * resources.
 */
static struct isa_pnp_id sysresource_ids[] = {
	{ 0x010cd041 /* PNP0c01 */, "System Memory" },
	{ 0x020cd041 /* PNP0c02 */, "System Resource" },
	{ 0 }
};

static int
sysresource_probe(device_t dev)
{
	int	result;

	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, sysresource_ids)) <= 0) {
		device_quiet(dev);
	}
	return (result);
}

static int
sysresource_attach(device_t dev)
{
	return (0);
}

static device_method_t sysresource_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sysresource_probe),
	DEVMETHOD(device_attach,	sysresource_attach),

	DEVMETHOD_END
};

static driver_t sysresource_driver = {
	"sysresource",
	sysresource_methods,
	1,		/* no softc */
};

DRIVER_MODULE(sysresource, isa, sysresource_driver, 0, 0);
ISA_PNP_INFO(sysresource_ids);
#endif /* DEV_ISA */
