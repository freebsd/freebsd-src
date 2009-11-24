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
 * software without specific, written prior permission.	 M.I.T. makes
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
 *	from: src/sys/i386/i386/nexus.c,v 1.26.2.5 2000/11/16 09:30:57 nyan
 *	JNPR: mainbus.c,v 1.2.4.1 2007/08/16 13:02:11 girish
 */

/*
 * This code implements a `root mainbus' for Intel Architecture
 * machines.  The function of the root mainbus is to serve as an
 * attachment point for both processors and buses, and to manage
 * resources which are common to all of them.  In particular,
 * this code implements the core resource managers for interrupt
 * requests, DMA requests (which rightfully should be a part of the
 * ISA code but it's easier to do it here for now), I/O port addresses,
 * and I/O memory address space.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cputype.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/vmparam.h>
#include <vm/vm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <machine/resource.h>

static struct rman irq_rman, port_rman, mem_rman;

static	int mainbus_probe(device_t);
static	int mainbus_attach(device_t);
static	int mainbus_print_child(device_t, device_t);
static	device_t mainbus_add_child(device_t bus, int order, const char *name,
	    int unit);
static	struct resource *mainbus_alloc_resource(device_t, device_t, int, int *,
	    u_long, u_long, u_long, u_int);
static	int mainbus_activate_resource(device_t, device_t, int, int,
	    struct resource *);
static	int mainbus_deactivate_resource(device_t, device_t, int, int,
	    struct resource *);
static	int mainbus_release_resource(device_t, device_t, int, int,
	    struct resource *);
static	int mainbus_setup_intr(device_t, device_t, struct resource *,
	    int flags, driver_filter_t, void (*)(void *), void *, void **);
static	int mainbus_teardown_intr(device_t, device_t, struct resource *,
	    void *);

static device_method_t mainbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mainbus_probe),
	DEVMETHOD(device_attach,	mainbus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	mainbus_print_child),
	DEVMETHOD(bus_add_child,	mainbus_add_child),
	DEVMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_alloc_resource,	mainbus_alloc_resource),
	DEVMETHOD(bus_release_resource,	mainbus_release_resource),
	DEVMETHOD(bus_activate_resource, mainbus_activate_resource),
	DEVMETHOD(bus_deactivate_resource, mainbus_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	mainbus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	mainbus_teardown_intr),

	{ 0, 0 }
};

static driver_t mainbus_driver = {
	"mainbus",
	mainbus_methods,
	1,			/* no softc */
};
static devclass_t mainbus_devclass;

DRIVER_MODULE(mainbus, root, mainbus_driver, mainbus_devclass, 0, 0);

static int
mainbus_probe(device_t dev)
{

#ifdef	DEBUG_BRINGUP
	device_verbose(dev);	/* print attach message */
#else
	device_quiet(dev);	/* suppress attach message for neatness */
#endif

	irq_rman.rm_start = 0;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupt request lines";
	irq_rman.rm_end = 15;
	if (rman_init(&irq_rman) ||
	    rman_manage_region(&irq_rman, irq_rman.rm_start, irq_rman.rm_end))
		panic("mainbus_probe irq_rman");

	/*
	 * IO ports and Memory truely are global at this level,
	 * as are APIC interrupts (however many IO APICS there turn out
	 * to be on large systems..)
	 */
	port_rman.rm_start = 0;
	port_rman.rm_end = 0xffff;
	port_rman.rm_type = RMAN_ARRAY;
	port_rman.rm_descr = "I/O ports";
	if (rman_init(&port_rman) || rman_manage_region(&port_rman, 0, 0xffff))
		panic("mainbus_probe port_rman");

	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0u;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory addresses";
	if (rman_init(&mem_rman) || rman_manage_region(&mem_rman, 0, ~0))
		panic("mainbus_probe mem_rman");

	return bus_generic_probe(dev);
}

static int
mainbus_attach(device_t dev)
{
	/*
	 * First, deal with the children we know about already
	 */
	bus_generic_attach(dev);

	return 0;
}

static int
mainbus_print_child(device_t bus, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += printf(" on motherboard\n");

	return (retval);
}

static device_t
mainbus_add_child(device_t bus, int order, const char *name, int unit)
{
	return device_add_child_ordered(bus, order, name, unit);
}

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of mainbus0.
 * (Exceptions include npx.)
 */
static struct resource *
mainbus_alloc_resource(device_t bus, device_t child, int type, int *rid,
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
		return 0;

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

	if (rv == 0) {
		printf("mainbus_alloc_resource: no resource is available\n");
		return 0;
	}

	if (type == SYS_RES_MEMORY) {
		rman_set_bustag(rv, MIPS_BUS_SPACE_MEM);

	} else if (type == SYS_RES_IOPORT) {
		rman_set_bustag(rv, MIPS_BUS_SPACE_IO);
		/* IBM-PC: the type of bus_space_handle_t is u_int */
		rman_set_bushandle(rv, rman_get_start(rv));
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
mainbus_activate_resource(device_t bus, device_t child, int type, int rid,
			struct resource *r)
{
	/*
	 * If this is a memory resource, map it into the kernel.
	 */
#ifdef TARGET_OCTEON
         uint64_t temp;
#endif  
	if (rman_get_bustag(r) == MIPS_BUS_SPACE_MEM) {
		caddr_t vaddr = 0;
		{
			u_int32_t paddr, psize, poffs;

			paddr = rman_get_start(r);
			psize = rman_get_size(r);

			poffs = paddr - trunc_page(paddr);
			vaddr = (caddr_t) pmap_mapdev(paddr-poffs, psize+poffs)
			    + poffs;
		}
		rman_set_virtual(r, vaddr);
#ifdef TARGET_OCTEON
		temp = 0x0000000000000000;
		temp |= (uint32_t)vaddr;
		rman_set_bushandle(r, (bus_space_handle_t) temp);
#else		
		rman_set_bushandle(r, (bus_space_handle_t) vaddr);
#endif		
	}
	return (rman_activate_resource(r));
}

static int
mainbus_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	/*
	 * If this is a memory resource, unmap it.
	 */
	if ((rman_get_bustag(r) == MIPS_BUS_SPACE_MEM) && (rman_get_end(r) >=
	    1024 * 1024)) {
		u_int32_t psize;

		psize = rman_get_size(r);
		pmap_unmapdev((vm_offset_t)rman_get_virtual(r), psize);
	}

	return (rman_deactivate_resource(r));
}

static int
mainbus_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	if (rman_get_flags(r) & RF_ACTIVE) {
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
 *
 *  Set up handler for external interrupt events.
 *  Use CR_INT_<n> to select the proper interrupt
 *  condition to dispatch on.
 */
static int
mainbus_setup_intr(device_t bus, device_t child, struct resource *irq,
    int flags, driver_filter_t filter, void (*ihand)(void *), void *arg,
    void **cookiep)
{
	panic("can never mainbus_setup_intr");
}

static int
mainbus_teardown_intr(device_t dev, device_t child, struct resource *r,
    void *ih)
{
	panic("can never mainbus_teardown_intr");
}
