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
 * $FreeBSD$
 */

/*
 * This code implements a system driver for legacy systems that do not
 * support ACPI or when ACPI support is not present in the kernel.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/legacyvar.h>
#include <machine/resource.h>

static MALLOC_DEFINE(M_LEGACYDEV, "legacydrv", "legacy system device");
struct legacy_device {
	struct resource_list	lg_resources;
	int			lg_pcibus;
};

#define DEVTOAT(dev)	((struct legacy_device *)device_get_ivars(dev))

static	int legacy_probe(device_t);
static	int legacy_attach(device_t);
static	int legacy_print_child(device_t, device_t);
static device_t legacy_add_child(device_t bus, int order, const char *name,
				int unit);
static	struct resource *legacy_alloc_resource(device_t, device_t, int, int *,
					      u_long, u_long, u_long, u_int);
static	int legacy_read_ivar(device_t, device_t, int, uintptr_t *);
static	int legacy_write_ivar(device_t, device_t, int, uintptr_t);
static	int legacy_release_resource(device_t, device_t, int, int,
				   struct resource *);
static	int legacy_set_resource(device_t, device_t, int, int, u_long, u_long);
static	int legacy_get_resource(device_t, device_t, int, int, u_long *, u_long *);
static void legacy_delete_resource(device_t, device_t, int, int);

static device_method_t legacy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		legacy_probe),
	DEVMETHOD(device_attach,	legacy_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	legacy_print_child),
	DEVMETHOD(bus_add_child,	legacy_add_child),
	DEVMETHOD(bus_read_ivar,	legacy_read_ivar),
	DEVMETHOD(bus_write_ivar,	legacy_write_ivar),
	DEVMETHOD(bus_set_resource,	legacy_set_resource),
	DEVMETHOD(bus_get_resource,	legacy_get_resource),
	DEVMETHOD(bus_alloc_resource,	legacy_alloc_resource),
	DEVMETHOD(bus_release_resource,	legacy_release_resource),
	DEVMETHOD(bus_delete_resource,	legacy_delete_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t legacy_driver = {
	"legacy",
	legacy_methods,
	1,			/* no softc */
};
static devclass_t legacy_devclass;

DRIVER_MODULE(legacy, nexus, legacy_driver, legacy_devclass, 0, 0);

static int
legacy_probe(device_t dev)
{

	device_set_desc(dev, "legacy system");
	device_quiet(dev);
	return (0);
}

static int
legacy_attach(device_t dev)
{
	device_t	child;

	/*
	 * First, let our child driver's identify any child devices that
	 * they can find.  Once that is done attach any devices that we
	 * found.
	 */
	bus_generic_probe(dev);
	bus_generic_attach(dev);

	/*
	 * If we didn't see EISA or ISA on a pci bridge, create some
	 * connection points now so they show up "on motherboard".
	 */
	if (!devclass_get_device(devclass_find("eisa"), 0)) {
		child = BUS_ADD_CHILD(dev, 0, "eisa", 0);
		if (child == NULL)
			panic("legacy_attach eisa");
		device_probe_and_attach(child);
	}
	if (!devclass_get_device(devclass_find("mca"), 0)) {
        	child = BUS_ADD_CHILD(dev, 0, "mca", 0);
        	if (child == 0)
                	panic("legacy_probe mca");
		device_probe_and_attach(child);
	}
	if (!devclass_get_device(devclass_find("isa"), 0)) {
		child = BUS_ADD_CHILD(dev, 0, "isa", 0);
		if (child == NULL)
			panic("legacy_attach isa");
		device_probe_and_attach(child);
	}

	return 0;
}

static int
legacy_print_all_resources(device_t dev)
{
	struct legacy_device *atdev = DEVTOAT(dev);
	struct resource_list *rl = &atdev->lg_resources;
	int retval = 0;

	if (SLIST_FIRST(rl) || atdev->lg_pcibus != -1)
		retval += printf(" at");
	
	retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

	return retval;
}

static int
legacy_print_child(device_t bus, device_t child)
{
	struct legacy_device *atdev = DEVTOAT(child);
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += legacy_print_all_resources(child);
	if (atdev->lg_pcibus != -1)
		retval += printf(" pcibus %d", atdev->lg_pcibus);
	retval += printf(" on motherboard\n");	/* XXX "motherboard", ick */

	return (retval);
}

static device_t
legacy_add_child(device_t bus, int order, const char *name, int unit)
{
	device_t child;
	struct legacy_device *atdev;

	atdev = malloc(sizeof(struct legacy_device), M_LEGACYDEV,
	    M_NOWAIT | M_ZERO);
	if (!atdev)
		return(0);
	resource_list_init(&atdev->lg_resources);
	atdev->lg_pcibus = -1;

	child = device_add_child_ordered(bus, order, name, unit); 

	/* should we free this in legacy_child_detached? */
	device_set_ivars(child, atdev);

	return(child);
}

static int
legacy_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct legacy_device *atdev = DEVTOAT(child);

	switch (which) {
	case LEGACY_IVAR_PCIBUS:
		*result = atdev->lg_pcibus;
		break;
	default:
		return ENOENT;
	}
	return 0;
}
	

static int
legacy_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct legacy_device *atdev = DEVTOAT(child);

	switch (which) {
	case LEGACY_IVAR_PCIBUS:
		atdev->lg_pcibus = value;
		break;
	default:
		return ENOENT;
	}
	return 0;
}


static struct resource *
legacy_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     u_long start, u_long end, u_long count, u_int flags)
{
	struct legacy_device *atdev = DEVTOAT(child);
	struct resource_list *rl = &atdev->lg_resources;

	return (resource_list_alloc(rl, bus, child, type, rid, start, end,
		    count, flags));
}

static int
legacy_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	struct legacy_device *atdev = DEVTOAT(child);
	struct resource_list *rl = &atdev->lg_resources;

	return (resource_list_release(rl, bus, child, type, rid, r));
}

static int
legacy_set_resource(device_t dev, device_t child, int type, int rid,
    u_long start, u_long count)
{
	struct legacy_device *atdev = DEVTOAT(child);
	struct resource_list *rl = &atdev->lg_resources;

	resource_list_add(rl, type, rid, start, start + count - 1, count);
	return(0);
}

static int
legacy_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct legacy_device *atdev = DEVTOAT(child);
	struct resource_list *rl = &atdev->lg_resources;
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return(ENOENT);
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;
	return(0);
}

static void
legacy_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct legacy_device *atdev = DEVTOAT(child);
	struct resource_list *rl = &atdev->lg_resources;

	resource_list_delete(rl, type, rid);
}
