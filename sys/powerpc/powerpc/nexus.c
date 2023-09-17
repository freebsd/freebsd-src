/*-
 * Copyright 1998 Massachusetts Institute of Technology
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.
 * Copyright 2006 by Marius Strobl <marius@FreeBSD.org>.
 * All rights reserved.
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
 *
 * 	from: FreeBSD: src/sys/i386/i386/nexus.c,v 1.43 2001/02/09
 */

/*
 * This code implements a `root nexus' for Power ISA Architecture
 * machines.  The function of the root nexus is to serve as an
 * attachment point for both processors and buses, and to manage
 * resources which are common to all of them.  In particular,
 * this code implements the core resource managers for interrupt
 * requests and I/O memory address space.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/intr_machdep.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

static struct rman intr_rman;
static struct rman mem_rman;

static device_probe_t		nexus_probe;
static device_attach_t		nexus_attach;

static bus_activate_resource_t	nexus_activate_resource;
static bus_adjust_resource_t	nexus_adjust_resource;
static bus_alloc_resource_t	nexus_alloc_resource;
static bus_deactivate_resource_t nexus_deactivate_resource;
static bus_map_resource_t	nexus_map_resource;
static bus_release_resource_t	nexus_release_resource;
static bus_unmap_resource_t	nexus_unmap_resource;

#ifdef SMP
static bus_bind_intr_t		nexus_bind_intr;
#endif
static bus_config_intr_t	nexus_config_intr;
static bus_setup_intr_t		nexus_setup_intr;
static bus_teardown_intr_t	nexus_teardown_intr;

static bus_get_bus_tag_t	nexus_get_bus_tag;

static ofw_bus_map_intr_t	nexus_ofw_map_intr;

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	nexus_attach),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_adjust_resource,	nexus_adjust_resource),
	DEVMETHOD(bus_activate_resource, nexus_activate_resource),
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_deactivate_resource, nexus_deactivate_resource),
	DEVMETHOD(bus_map_resource,	nexus_map_resource),
	DEVMETHOD(bus_release_resource,	nexus_release_resource),
	DEVMETHOD(bus_unmap_resource,   nexus_unmap_resource),
#ifdef SMP
	DEVMETHOD(bus_bind_intr,	nexus_bind_intr),
#endif
	DEVMETHOD(bus_config_intr,	nexus_config_intr),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_get_bus_tag,	nexus_get_bus_tag),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_map_intr,	nexus_ofw_map_intr),

	DEVMETHOD_END
};

DEFINE_CLASS_0(nexus, nexus_driver, nexus_methods, 1);
EARLY_DRIVER_MODULE(nexus, root, nexus_driver, 0, 0, BUS_PASS_BUS);
MODULE_VERSION(nexus, 1);

static int
nexus_probe(device_t dev)
{
        
	device_quiet(dev);	/* suppress attach message for neatness */

	return (BUS_PROBE_DEFAULT);
}

static int
nexus_attach(device_t dev)
{

	intr_rman.rm_type = RMAN_ARRAY;
	intr_rman.rm_descr = "Interrupts";
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory addresses";
	if (rman_init(&intr_rman) != 0 || rman_init(&mem_rman) != 0 ||
	    rman_manage_region(&intr_rman, 0, ~0) != 0 ||
	    rman_manage_region(&mem_rman, 0, BUS_SPACE_MAXADDR) != 0)
		panic("%s: failed to set up rmans.", __func__);

	/* Add ofwbus0. */
	device_add_child(dev, "ofwbus", 0);

	/* Now, probe children. */
	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

static int
nexus_setup_intr(device_t bus __unused, device_t child, struct resource *r,
    int flags, driver_filter_t *filt, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	int error, domain;

	if (r == NULL)
		panic("%s: NULL interrupt resource!", __func__);

	if (cookiep != NULL)
		*cookiep = NULL;
	if ((rman_get_flags(r) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/* We depend here on rman_activate_resource() being idempotent. */
	error = rman_activate_resource(r);
	if (error)
		return (error);

	if (bus_get_domain(child, &domain) != 0) {
		if(bootverbose)
			device_printf(child, "no domain found\n");
		domain = 0;
	}
	error = powerpc_setup_intr(device_get_nameunit(child),
	    rman_get_start(r), filt, intr, arg, flags, cookiep, domain);

	return (error);
}

static int
nexus_teardown_intr(device_t bus __unused, device_t child __unused,
    struct resource *r, void *ih)
{
        
	if (r == NULL)
		return (EINVAL);

	return (powerpc_teardown_intr(ih));
}

static bus_space_tag_t
nexus_get_bus_tag(device_t bus __unused, device_t child __unused)
{

#if BYTE_ORDER == LITTLE_ENDIAN
	return(&bs_le_tag);
#else
	return(&bs_be_tag);
#endif
}

#ifdef SMP
static int
nexus_bind_intr(device_t bus __unused, device_t child __unused,
    struct resource *r, int cpu)
{

	return (powerpc_bind_intr(rman_get_start(r), cpu));
}
#endif

static int
nexus_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{

	return (powerpc_config_intr(irq, trig, pol));
}

static int
nexus_ofw_map_intr(device_t dev, device_t child, phandle_t iparent, int icells,
    pcell_t *irq)
{
	u_int intr = MAP_IRQ(iparent, irq[0]);
	if (icells > 1)
		powerpc_fw_config_intr(intr, irq[1]);
	return (intr);
}

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of nexus0.
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct rman *rm;
	struct resource *rv;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &intr_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &mem_rman;
		break;
	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags & ~RF_ACTIVE,
	    child);
	if (rv == NULL)
		return (NULL);
	rman_set_rid(rv, *rid);

	if ((flags & RF_ACTIVE) != 0) {
		if (bus_activate_resource(child, type, *rid, rv) != 0) {
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
nexus_activate_resource(device_t bus __unused, device_t child __unused,
    int type, int rid __unused, struct resource *r)
{

	if (type == SYS_RES_MEMORY) {
		vm_paddr_t start;
		void *p;

		start = (vm_paddr_t) rman_get_start(r);
		if (bootverbose)
			printf("nexus mapdev: start %jx, len %jd\n",
			    (uintmax_t)start, rman_get_size(r));

		p = pmap_mapdev(start, (vm_size_t) rman_get_size(r));
		if (p == NULL)
			return (ENOMEM);
		rman_set_virtual(r, p);
		rman_set_bustag(r, &bs_be_tag);
		rman_set_bushandle(r, (u_long)p);
	}
	return (rman_activate_resource(r));
}

static int
nexus_deactivate_resource(device_t bus __unused, device_t child __unused,
    int type __unused, int rid __unused, struct resource *r)
{

	/*
	 * If this is a memory resource, unmap it.
	 */
	if ((type == SYS_RES_MEMORY) || (type == SYS_RES_IOPORT)) {
		bus_size_t psize;

		psize = rman_get_size(r);
		pmap_unmapdev(rman_get_virtual(r), psize);
	}

	return (rman_deactivate_resource(r));
}

static int
nexus_adjust_resource(device_t bus, device_t child __unused, int type,
    struct resource *r, rman_res_t start, rman_res_t end)
{
	struct rman *rm;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &intr_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &mem_rman;
		break;
	default:
		return (EINVAL);
	}
	if (rm == NULL)
		return (ENXIO);
	if (rman_is_region_manager(r, rm) == 0)
		return (EINVAL);
	return (rman_adjust_resource(r, start, end));
}

static int
nexus_release_resource(device_t bus, device_t child, int type,
    int rid, struct resource *r)
{
	int error;

	if ((rman_get_flags(r) & RF_ACTIVE) != 0) {
		error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return (error);
	}
	return (rman_release_resource(r));
}

static int
nexus_map_resource(device_t bus, device_t child, int type, struct resource *r,
    struct resource_map_request *argsp, struct resource_map *map)
{

	struct resource_map_request args;
	rman_res_t end, length, start;

	/* Resources must be active to be mapped. */
	if (!(rman_get_flags(r) & RF_ACTIVE))
		return (ENXIO);

	/* Mappings are only supported on I/O and memory resources. */
	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		break;
	default:
		return (EINVAL);
	}

	resource_init_map_request(&args);
	if (argsp != NULL)
		bcopy(argsp, &args, imin(argsp->size, args.size));

	start = rman_get_start(r) + args.offset;
	if (args.length == 0)
		length = rman_get_size(r);
	else
		length = args.length;

	end = start + length - 1;
	if (start > rman_get_end(r) || start < rman_get_start(r))
		return (EINVAL);

	if (end > rman_get_end(r) || end < start)
		return (EINVAL);

	/*
	 * If this is a memory resource, map it into the kernel.
	 */
	switch (type) {
	case SYS_RES_IOPORT:
		panic("%s:%d SYS_RES_IOPORT handling not implemented", __func__, __LINE__);
		/*   XXX: untested
		map->r_bushandle = start;
		map->r_bustag = nexus_get_bus_tag(NULL, NULL);
		map->r_size = length;
		map->r_vaddr = NULL;
		*/
		break;
	case SYS_RES_MEMORY:
		map->r_vaddr = pmap_mapdev_attr(start, length, args.memattr);
		map->r_bustag = nexus_get_bus_tag(NULL, NULL);
		map->r_size = length;
		map->r_bushandle = (bus_space_handle_t)map->r_vaddr;
		break;
	}

	return (0);

}

static int
nexus_unmap_resource(device_t bus, device_t child, int type, struct resource *r,
    struct resource_map *map)
{

	/*
	 * If this is a memory resource, unmap it.
	 */
	switch (type) {
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
