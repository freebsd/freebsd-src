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
 *	$Id$
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#include <machine/ipl.h>
#include <machine/resource.h>
#ifdef APIC_IO
#include <machine/smp.h>
#include <machine/mpapic.h>
#endif

#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#include <i386/isa/intr_machdep.h>

#include <pci/pcivar.h>

#include "eisa.h"
#include "isa.h"
#include "pci.h"
#include "npx.h"
#include "apm.h"

static struct rman irq_rman, drq_rman, port_rman, mem_rman;

static	int nexus_probe(device_t);
static	void nexus_print_child(device_t, device_t);
static	struct resource *nexus_alloc_resource(device_t, device_t, int, int *,
					      u_long, u_long, u_long, u_int);
static	int nexus_activate_resource(device_t, device_t, int, int,
				    struct resource *);
static	int nexus_deactivate_resource(device_t, device_t, int, int,
				      struct resource *);
static	int nexus_release_resource(device_t, device_t, int, int,
				   struct resource *);
static	int nexus_setup_intr(device_t, device_t, struct resource *,
			     void (*)(void *), void *, void **);
static	int nexus_teardown_intr(device_t, device_t, struct resource *,
				void *);

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	nexus_print_child),
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
	DRIVER_TYPE_MISC,
	1,			/* no softc */
};
static devclass_t nexus_devclass;

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, 0, 0);

#ifdef APIC_IO
#define LASTIRQ	(NINTR - 1)
#else
#define LASTIRQ 15
#endif

static int
nexus_probe(device_t dev)
{
	device_t	child;

	device_quiet(dev);	/* suppress attach message for neatness */

	irq_rman.rm_start = 0;
	irq_rman.rm_end = LASTIRQ;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupt request lines";
	if (rman_init(&irq_rman)
	    || rman_manage_region(&irq_rman, 0, 1)
	    || rman_manage_region(&irq_rman, 3, LASTIRQ))
		panic("nexus_probe irq_rman");

	drq_rman.rm_start = 0;
	drq_rman.rm_end = 7;
	drq_rman.rm_type = RMAN_ARRAY;
	drq_rman.rm_descr = "DMA request lines";
	/* XXX drq 0 not available on some machines */
	if (rman_init(&drq_rman)
	    || rman_manage_region(&drq_rman, 0, 7))
		panic("nexus_probe drq_rman");

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

#if NNPX > 0
	child = device_add_child(dev, "npx", 0, 0);
	if (child == 0)
		panic("nexus_probe npx");
#endif /* NNPX > 0 */
#if NAPM > 0
	child = device_add_child(dev, "apm", 0, 0);
	if (child == 0)
		panic("nexus_probe apm");
#endif /* NAPM > 0 */
#if NPCI > 0
	/* Add a PCI bridge if pci bus is present */
	if (pci_cfgopen() != 0) {
		child = device_add_child(dev, "pcib", 0, 0);
		if (child == 0)
			panic("nexus_probe pcib");
	}
#endif
#if 0 && NEISA > 0
	child = device_add_child(dev, "eisa", 0, 0);
	if (child == 0)
		panic("nexus_probe eisa");
#endif
#if NISA > 0
	/* Add an ISA bus directly if pci bus is not present */
	if (pci_cfgopen() == 0) {
		child = device_add_child(dev, "isa", 0, 0);
		if (child == 0)
			panic("nexus_probe isa");
	}
#endif
	return 0;
}

static void
nexus_print_child(device_t bus, device_t child)
{
	printf(" on motherboard");
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
		caddr_t vaddr = 0;

		if (rv->r_end < 1024 * 1024 * 1024) {
			/*
			 * The first 1Mb is mapped at KERNBASE.
			 */
			vaddr = (caddr_t)((uintptr_t)KERNBASE + rv->r_start);
		} else {
			u_int32_t paddr;
			u_int32_t psize;
			u_int32_t poffs;

			paddr = rv->r_start;
			psize = rv->r_end - rv->r_start;

			poffs = paddr - trunc_page(paddr);
			vaddr = (caddr_t) pmap_mapdev(paddr-poffs, psize+poffs) + poffs;
		}
		rman_set_virtual(rv, vaddr);
		rman_set_bustag(rv, I386_BUS_SPACE_MEM);
		rman_set_bushandle(rv, (bus_space_handle_t) vaddr);
	} else if (type == SYS_RES_IOPORT) {
		rman_set_bustag(rv, I386_BUS_SPACE_IO);
		rman_set_bushandle(rv, rv->r_start);
	}
	return rv;
}

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
			struct resource *r)
{
	return (rman_activate_resource(r));
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
			  struct resource *r)
{
	return (rman_deactivate_resource(r));
}

static int
nexus_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
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
		 void (*ihand)(void *), void *arg, void **cookiep)
{
	intrmask_t	*mask;
	driver_t	*driver;
	int	error, icflags;

	if (child)
		device_printf(child, "interrupting at irq %d\n",
			      (int)irq->r_start);

	*cookiep = 0;
	if (irq->r_flags & RF_SHAREABLE)
		icflags = 0;
	else
		icflags = INTR_EXCL;

	driver = device_get_driver(child);
	switch (driver->type) {
	case DRIVER_TYPE_TTY:
		mask = &tty_imask;
		break;
	case (DRIVER_TYPE_TTY | DRIVER_TYPE_FAST):
		mask = &tty_imask;
		icflags |= INTR_FAST;
		break;
	case DRIVER_TYPE_BIO:
		mask = &bio_imask;
		break;
	case DRIVER_TYPE_NET:
		mask = &net_imask;
		break;
	case DRIVER_TYPE_CAM:
		mask = &cam_imask;
		break;
	case DRIVER_TYPE_MISC:
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

	*cookiep = intr_create((void *)(intptr_t)-1, irq->r_start, ihand, arg,
			    mask, icflags);
	if (*cookiep)
		error = intr_connect(*cookiep);
	else
		error = EINVAL;	/* XXX ??? */

	return (error);
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *r, void *ih)
{
	return (intr_destroy(ih));
}

static devclass_t	pcib_devclass;

static int
nexus_pcib_probe(device_t dev)
{
	device_set_desc(dev, "PCI host bus adapter");

	device_add_child(dev, "pci", 0, 0);

	return 0;
}

static device_method_t nexus_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_pcib_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t nexus_pcib_driver = {
	"pcib",
	nexus_pcib_methods,
	DRIVER_TYPE_MISC,
	1,
};

DRIVER_MODULE(pcib, nexus, nexus_pcib_driver, pcib_devclass, 0, 0);
