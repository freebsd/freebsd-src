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
 *
 * $FreeBSD$
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/interrupt.h>
#include <sys/pcpu.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/efi.h>
#include <machine/intr.h>
#include <machine/pmap.h>
#include <machine/resource.h>
#include <machine/vmparam.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>

#include <isa/isareg.h>
#include <sys/rtprio.h>

#include "clock_if.h"

static MALLOC_DEFINE(M_NEXUSDEV, "nexusdev", "Nexus device");
struct nexus_device {
	struct resource_list	nx_resources;
};

#define DEVTONX(dev)	((struct nexus_device *)device_get_ivars(dev))

static struct rman irq_rman, port_rman, mem_rman;

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
			     driver_filter_t filter, void (*)(void *), void *, 
			     void **);
static	int nexus_teardown_intr(device_t, device_t, struct resource *,
				void *);
static struct resource_list *nexus_get_reslist(device_t dev, device_t child);
static	int nexus_set_resource(device_t, device_t, int, int, u_long, u_long);
static	int nexus_get_resource(device_t, device_t, int, int, u_long *,
			       u_long *);
static void nexus_delete_resource(device_t, device_t, int, int);
static int nexus_bind_intr(device_t, device_t, struct resource *, int);
static	int nexus_config_intr(device_t, int, enum intr_trigger,
			      enum intr_polarity);

static int nexus_gettime(device_t, struct timespec *);
static int nexus_settime(device_t, struct timespec *);

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
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_release_resource,	nexus_release_resource),
	DEVMETHOD(bus_activate_resource, nexus_activate_resource),
	DEVMETHOD(bus_deactivate_resource, nexus_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_get_resource_list, nexus_get_reslist),
	DEVMETHOD(bus_set_resource,	nexus_set_resource),
	DEVMETHOD(bus_get_resource,	nexus_get_resource),
	DEVMETHOD(bus_delete_resource,	nexus_delete_resource),
	DEVMETHOD(bus_bind_intr,	nexus_bind_intr),
	DEVMETHOD(bus_config_intr,	nexus_config_intr),

	/* Clock interface */
	DEVMETHOD(clock_gettime,	nexus_gettime),
	DEVMETHOD(clock_settime,	nexus_settime),

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

	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupt request lines";
	irq_rman.rm_start = 0;
	irq_rman.rm_end = IA64_NXIVS - 1;
	if (rman_init(&irq_rman)
	    || rman_manage_region(&irq_rman,
				  irq_rman.rm_start, irq_rman.rm_end))
		panic("nexus_probe irq_rman");

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

	/*
	 * Mask the legacy PICs - we will use the I/O SAPIC for interrupt.
	 */
	outb(IO_ICU1+1, 0xff);
	outb(IO_ICU2+1, 0xff);

	if (acpi_identify() == 0)
		BUS_ADD_CHILD(dev, 10, "acpi", 0);
	clock_register(dev, 1000);
	bus_generic_attach(dev);
	return 0;
}

static int
nexus_print_child(device_t bus, device_t child)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct resource_list *rl = &ndev->nx_resources;
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");
	if (device_get_flags(child))
		retval += printf(" flags %#x", device_get_flags(child));
	retval += printf(" on motherboard\n");	/* XXX "motherboard", ick */

	return (retval);
}

static device_t
nexus_add_child(device_t bus, int order, const char *name, int unit)
{
	device_t		child;
	struct nexus_device	*ndev;

	ndev = malloc(sizeof(struct nexus_device), M_NEXUSDEV, M_NOWAIT|M_ZERO);
	if (!ndev)
		return(0);
	resource_list_init(&ndev->nx_resources);

	child = device_add_child_ordered(bus, order, name, unit); 

	/* should we free this in nexus_child_detached? */
	device_set_ivars(child, ndev);

	return(child);
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
	struct nexus_device *ndev = DEVTONX(child);
	struct	resource *rv;
	struct resource_list_entry *rle;
	struct	rman *rm;
	int needactivate = flags & RF_ACTIVE;

	/*
	 * If this is an allocation of the "default" range for a given RID, and
	 * we know what the resources for this device are (ie. they aren't maintained
	 * by a child bus), then work out the start/end values.
	 */
	if ((start == 0UL) && (end == ~0UL) && (count == 1)) {
		if (ndev == NULL)
			return(NULL);
		rle = resource_list_find(&ndev->nx_resources, type, *rid);
		if (rle == NULL)
			return(NULL);
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	flags &= ~RF_ACTIVE;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &irq_rman;
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
	rman_set_rid(rv, *rid);

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
	vm_paddr_t paddr;
	void *vaddr;

	paddr = rman_get_start(r);

	switch (type) {
	case SYS_RES_IOPORT:
		rman_set_bustag(r, IA64_BUS_SPACE_IO);
		rman_set_bushandle(r, paddr);
		break;
	case SYS_RES_MEMORY:
		vaddr = pmap_mapdev(paddr, rman_get_size(r));
		rman_set_bustag(r, IA64_BUS_SPACE_MEM);
		rman_set_bushandle(r, (bus_space_handle_t) vaddr);
		rman_set_virtual(r, vaddr);
		break;
	}
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
 */
static int
nexus_setup_intr(device_t bus, device_t child, struct resource *irq,
		 int flags, driver_filter_t filter, void (*ihand)(void *), 
		 void *arg, void **cookiep)
{
	driver_t	*driver;
	int		error;

	/* somebody tried to setup an irq that failed to allocate! */
	if (irq == NULL)
		panic("nexus_setup_intr: NULL irq resource!");

	*cookiep = 0;
	if ((rman_get_flags(irq) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	driver = device_get_driver(child);

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(irq);
	if (error)
		return (error);

	error = ia64_setup_intr(device_get_nameunit(child),
	    rman_get_start(irq), filter, ihand, arg, flags, cookiep);

	return (error);
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *ires,
    void *cookie)
{

	return (ia64_teardown_intr(cookie));
}

static struct resource_list *
nexus_get_reslist(device_t dev, device_t child)
{
	struct nexus_device *ndev = DEVTONX(child);

	return (&ndev->nx_resources);
}

static int
nexus_set_resource(device_t dev, device_t child, int type, int rid,
    u_long start, u_long count)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;

	if (type == SYS_RES_IOPORT && start > (0x10000 - count)) {
		/*
		 * Work around a firmware bug in the HP rx2660, where in ACPI
		 * an I/O port is really a memory mapped I/O address. The bug
		 * is in the GAS that describes the address and in particular
		 * the SpaceId field. The field should not say the address is
		 * an I/O port when it is in fact an I/O memory address.
		 */
		if (bootverbose)
			printf("%s: invalid port range (%#lx-%#lx); "
			    "assuming I/O memory range.\n", __func__, start,
			    start + count - 1);
		type = SYS_RES_MEMORY;
	}

	/* XXX this should return a success/failure indicator */
	resource_list_add(rl, type, rid, start, start + count - 1, count);
	return(0);
}

static int
nexus_get_resource(device_t dev, device_t child, int type, int rid, u_long *startp, u_long *countp)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	device_printf(child, "type %d  rid %d  startp %p  countp %p - got %p\n",
		      type, rid, startp, countp, rle);
	if (!rle)
		return(ENOENT);
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;
	return(0);
}

static void
nexus_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;

	resource_list_delete(rl, type, rid);
}

static int
nexus_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{

	return (sapic_config_intr(irq, trig, pol));
}

static int
nexus_bind_intr(device_t dev, device_t child, struct resource *irq, int cpu)
{
	struct pcpu *pc;

	pc = cpuid_to_pcpu[cpu];
	if (pc == NULL)
		return (EINVAL);
	return (sapic_bind_intr(rman_get_start(irq), pc));
}

static int
nexus_gettime(device_t dev, struct timespec *ts)
{
	struct clocktime ct;
	struct efi_tm tm;

	efi_get_time(&tm);

	/*
	 * This code was written in 2005, so logically EFI cannot return
	 * a year smaller than that. Assume the EFI clock is out of whack
	 * in that case and reset the EFI clock.
	 */
	if (tm.tm_year < 2005)
		return (EINVAL);

	ct.nsec = tm.tm_nsec;
	ct.sec = tm.tm_sec;
	ct.min = tm.tm_min;
	ct.hour = tm.tm_hour;
	ct.day = tm.tm_mday;
	ct.mon = tm.tm_mon;
	ct.year = tm.tm_year;
	ct.dow = -1;
	return (clock_ct_to_ts(&ct, ts));
}

static int
nexus_settime(device_t dev, struct timespec *ts)
{
	struct clocktime ct;
	struct efi_tm tm;

	efi_get_time(&tm);

	clock_ts_to_ct(ts, &ct);
	tm.tm_nsec = ts->tv_nsec;
	tm.tm_sec = ct.sec;
	tm.tm_min = ct.min;
	tm.tm_hour = ct.hour;
	tm.tm_year = ct.year;
	tm.tm_mon = ct.mon;
	tm.tm_mday = ct.day;
	return (efi_set_time(&tm));
}
