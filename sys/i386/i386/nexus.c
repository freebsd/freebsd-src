/*
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
 *
 * $FreeBSD: src/sys/i386/i386/nexus.c,v 1.26.2.2 2000/04/23 09:59:11 nyan Exp $
 */

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

#include "opt_smp.h"
#include "mca.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#include <machine/resource.h>
#ifdef APIC_IO
#include <machine/smp.h>
#include <machine/mpapic.h>
#endif

#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/intr_machdep.h>

static struct rman irq_rman, drq_rman, port_rman, mem_rman;

static	int nexus_probe(device_t);
static	int nexus_attach(device_t);
static	int nexus_print_child(device_t, device_t);
static device_t nexus_add_child(device_t bus, int order, const char *name,
				int unit);
static	struct resource *nexus_alloc_resource(device_t, device_t, int, int *,
					      u_long, u_long, u_long, u_int);
static	int nexus_activate_resource(device_t, device_t, int, int,
				    struct resource *);
static	int nexus_deactivate_resource(device_t, device_t, int, int,
				      struct resource *);
static	int nexus_release_resource(device_t, device_t, int, int,
				   struct resource *);
static	int nexus_setup_intr(device_t, device_t, struct resource *, int flags,
			     void (*)(void *), void *, void **);
static	int nexus_teardown_intr(device_t, device_t, struct resource *,
				void *);

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
	DEVMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_release_resource,	nexus_release_resource),
	DEVMETHOD(bus_activate_resource, nexus_activate_resource),
	DEVMETHOD(bus_deactivate_resource, nexus_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),

	{ 0, 0 }
};

static driver_t nexus_driver = {
	"nexus",
	nexus_methods,
	1,			/* no softc */
};
static devclass_t nexus_devclass;

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, 0, 0);

static int
nexus_probe(device_t dev)
{

	device_quiet(dev);	/* suppress attach message for neatness */

	/*
	 * IRQ's are on the mainboard on old systems, but on the ISA part
	 * of PCI->ISA bridges.  There would be multiple sets of IRQs on
	 * multi-ISA-bus systems.  PCI interrupts are routed to the ISA
	 * component, so in a way, PCI can be a partial child of an ISA bus(!).
	 * APIC interrupts are global though.
	 * In the non-APIC case, disallow the use of IRQ 2.
	 */
	irq_rman.rm_start = 0;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupt request lines";
#ifdef APIC_IO
	irq_rman.rm_end = APIC_INTMAPSIZE - 1;
	if (rman_init(&irq_rman)
	    || rman_manage_region(&irq_rman,
				  irq_rman.rm_start, irq_rman.rm_end))
		panic("nexus_probe irq_rman");
#else
	irq_rman.rm_end = 15;
#ifdef PC98
	if (rman_init(&irq_rman)
	    || rman_manage_region(&irq_rman,
				  irq_rman.rm_start, irq_rman.rm_end))
		panic("nexus_probe irq_rman");
#else
	if (rman_init(&irq_rman)
	    || rman_manage_region(&irq_rman, irq_rman.rm_start, 1)
	    || rman_manage_region(&irq_rman, 3, irq_rman.rm_end))
		panic("nexus_probe irq_rman");
#endif /* PC98 */
#endif

	/*
	 * ISA DMA on PCI systems is implemented in the ISA part of each
	 * PCI->ISA bridge and the channels can be duplicated if there are
	 * multiple bridges.  (eg: laptops with docking stations)
	 */
	drq_rman.rm_start = 0;
#ifdef PC98
	drq_rman.rm_end = 3;
#else
	drq_rman.rm_end = 7;
#endif
	drq_rman.rm_type = RMAN_ARRAY;
	drq_rman.rm_descr = "DMA request lines";
	/* XXX drq 0 not available on some machines */
	if (rman_init(&drq_rman)
	    || rman_manage_region(&drq_rman,
				  drq_rman.rm_start, drq_rman.rm_end))
		panic("nexus_probe drq_rman");

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
		panic("nexus_probe port_rman");

	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0u;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory addresses";
	if (rman_init(&mem_rman)
	    || rman_manage_region(&mem_rman, 0, ~0))
		panic("nexus_probe mem_rman");

	return bus_generic_probe(dev);
}

static int
nexus_attach(device_t dev)
{
	device_t	child;

	/*
	 * First, deal with the children we know about already
	 */
	bus_generic_attach(dev);
	/*
	 * And if we didn't see EISA or ISA on a pci bridge, create some
	 * connection points now so they show up "on motherboard".
	 */
	if (!devclass_get_device(devclass_find("eisa"), 0)) {
		child = device_add_child(dev, "eisa", 0);
		if (child == NULL)
			panic("nexus_attach eisa");
		device_probe_and_attach(child);
	}
#if NMCA > 0
	if (!devclass_get_device(devclass_find("mca"), 0)) {
        	child = device_add_child(dev, "mca", 0);
        	if (child == 0)
                	panic("nexus_probe mca");
		device_probe_and_attach(child);
	}
#endif
	if (!devclass_get_device(devclass_find("isa"), 0)) {
		child = device_add_child(dev, "isa", 0);
		if (child == NULL)
			panic("nexus_attach isa");
		device_probe_and_attach(child);
	}

	return 0;
}

static int
nexus_print_child(device_t bus, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += printf(" on motherboard\n");

	return (retval);
}

static device_t
nexus_add_child(device_t bus, int order, const char *name, int unit)
{
	return device_add_child_ordered(bus, order, name, unit);
}

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of nexus0.
 * (Exceptions include npx.)
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     u_long start, u_long end, u_long count, u_int flags)
{
	struct	resource *rv;
	struct	rman *rm;
	int needactivate = flags & RF_ACTIVE;

	flags &= ~RF_ACTIVE;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &irq_rman;
		break;

	case SYS_RES_DRQ:
		rm = &drq_rman;
		break;

	case SYS_RES_IOPORT:
		rm = &port_rman;
		break;

	case SYS_RES_MEMORY:
		rm = &mem_rman;
		break;

	default:
		return 0;
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == 0)
		return 0;

	if (type == SYS_RES_MEMORY) {
		rman_set_bustag(rv, I386_BUS_SPACE_MEM);
	} else if (type == SYS_RES_IOPORT) {
		rman_set_bustag(rv, I386_BUS_SPACE_IO);
#ifdef PC98
		/* PC-98: the type of bus_space_handle_t is the structure. */
		rv->r_bushandle.bsh_base = rv->r_start;
		rv->r_bushandle.bsh_iat = NULL;
		rv->r_bushandle.bsh_iatsz = 0;
		rv->r_bushandle.bsh_res = NULL;
		rv->r_bushandle.bsh_ressz = 0;
#else
		/* IBM-PC: the type of bus_space_handle_t is u_int */
		rman_set_bushandle(rv, rv->r_start);
#endif
	}

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return 0;
		}
	}
	
	return rv;
}

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
			struct resource *r)
{
	/*
	 * If this is a memory resource, map it into the kernel.
	 */
	if (rman_get_bustag(r) == I386_BUS_SPACE_MEM) {
		caddr_t vaddr = 0;

		if (r->r_end < 1024 * 1024) {
			/*
			 * The first 1Mb is mapped at KERNBASE.
			 */
			vaddr = (caddr_t)(uintptr_t)(KERNBASE + r->r_start);
		} else {
			u_int32_t paddr;
			u_int32_t psize;
			u_int32_t poffs;

			paddr = r->r_start;
			psize = r->r_end - r->r_start;

			poffs = paddr - trunc_page(paddr);
			vaddr = (caddr_t) pmap_mapdev(paddr-poffs, psize+poffs) + poffs;
		}
		rman_set_virtual(r, vaddr);
#ifdef PC98
		/* PC-98: the type of bus_space_handle_t is the structure. */
		r->r_bushandle.bsh_base = (bus_addr_t) vaddr;
		r->r_bushandle.bsh_iat = NULL;
		r->r_bushandle.bsh_iatsz = 0;
		r->r_bushandle.bsh_res = NULL;
		r->r_bushandle.bsh_ressz = 0;
#else
		/* IBM-PC: the type of bus_space_handle_t is u_int */
		rman_set_bushandle(r, (bus_space_handle_t) vaddr);
#endif
	}
	return (rman_activate_resource(r));
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
			  struct resource *r)
{
	/*
	 * If this is a memory resource, unmap it.
	 */
	if ((rman_get_bustag(r) == I386_BUS_SPACE_MEM) && (r->r_end >= 1024 * 1024)) {
		u_int32_t psize;

		psize = r->r_end - r->r_start;
		pmap_unmapdev((vm_offset_t)rman_get_virtual(r), psize);
	}
		
	return (rman_deactivate_resource(r));
}

static int
nexus_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	if (r->r_flags & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return error;
	}
	return (rman_release_resource(r));
}

/*
 * Currently this uses the really grody interface from kern/kern_intr.c
 * (which really doesn't belong in kern/anything.c).  Eventually, all of
 * the code in kern_intr.c and machdep_intr.c should get moved here, since
 * this is going to be the official interface.
 */
static int
nexus_setup_intr(device_t bus, device_t child, struct resource *irq,
		 int flags, void (*ihand)(void *), void *arg, void **cookiep)
{
	intrmask_t	*mask;
	driver_t	*driver;
	int	error, icflags;

	/* somebody tried to setup an irq that failed to allocate! */
	if (irq == NULL)
		panic("nexus_setup_intr: NULL irq resource!");

	*cookiep = 0;
	if (irq->r_flags & RF_SHAREABLE)
		icflags = 0;
	else
		icflags = INTR_EXCL;

	driver = device_get_driver(child);
	switch (flags) {
	case INTR_TYPE_TTY:
		mask = &tty_imask;
		break;
	case (INTR_TYPE_TTY | INTR_TYPE_FAST):
		mask = &tty_imask;
		icflags |= INTR_FAST;
		break;
	case INTR_TYPE_BIO:
		mask = &bio_imask;
		break;
	case INTR_TYPE_NET:
		mask = &net_imask;
		break;
	case INTR_TYPE_CAM:
		mask = &cam_imask;
		break;
	case INTR_TYPE_MISC:
		mask = 0;
		break;
	default:
		panic("still using grody create_intr interface");
	}

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(irq);
	if (error)
		return (error);

	*cookiep = inthand_add(device_get_nameunit(child), irq->r_start,
	    ihand, arg, mask, icflags);
	if (*cookiep == NULL)
		error = EINVAL;	/* XXX ??? */

	return (error);
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *r, void *ih)
{
	return (inthand_remove(ih));
}
