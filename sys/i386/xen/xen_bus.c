
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/frame.h> 
#include <machine/intr_machdep.h> 
#include <machine/resource.h>

#include <machine/xen/xen-os.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/xen_intr.h>

static MALLOC_DEFINE(M_XENDEV, "xenintrdrv", "xen system device");

struct xenbus_device {
    struct resource_list  xen_resources;
};

#define DEVTOXEN(dev)       ((struct xenbus_device *)device_get_ivars(dev))

static void xenbus_identify(driver_t *, device_t); 
static int xenbus_probe(device_t);
static int xenbus_attach(device_t);
static int xenbus_print_child(device_t, device_t);
static device_t xenbus_add_child(device_t bus, int order, const char *name, 
				 int unit);
static struct resource *xenbus_alloc_resource(device_t, device_t, int, int *,
					      u_long, u_long, u_long, u_int);
static  int xenbus_release_resource(device_t, device_t, int, int, 
				    struct resource *); 
static  int xenbus_set_resource(device_t, device_t, int, int, u_long, u_long); 
static  int xenbus_get_resource(device_t, device_t, int, int, u_long *, u_long *); 
static void xenbus_delete_resource(device_t, device_t, int, int); 


static device_method_t xenbus_methods[] = { 
    /* Device interface */ 
    DEVMETHOD(device_identify,      xenbus_identify), 
    DEVMETHOD(device_probe,         xenbus_probe), 
    DEVMETHOD(device_attach,        xenbus_attach), 
    DEVMETHOD(device_detach,        bus_generic_detach), 
    DEVMETHOD(device_shutdown,      bus_generic_shutdown), 
    DEVMETHOD(device_suspend,       bus_generic_suspend), 
    DEVMETHOD(device_resume,        bus_generic_resume), 
 
    /* Bus interface */ 
    DEVMETHOD(bus_print_child,      xenbus_print_child),
    DEVMETHOD(bus_add_child,        xenbus_add_child), 
    DEVMETHOD(bus_read_ivar,        bus_generic_read_ivar), 
    DEVMETHOD(bus_write_ivar,       bus_generic_write_ivar), 
    DEVMETHOD(bus_set_resource,     xenbus_set_resource), 
    DEVMETHOD(bus_get_resource,     xenbus_get_resource), 
    DEVMETHOD(bus_alloc_resource,   xenbus_alloc_resource), 
    DEVMETHOD(bus_release_resource, xenbus_release_resource), 
    DEVMETHOD(bus_delete_resource,  xenbus_delete_resource), 
    DEVMETHOD(bus_activate_resource, bus_generic_activate_resource), 
    DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource), 
    DEVMETHOD(bus_setup_intr,       bus_generic_setup_intr), 
    DEVMETHOD(bus_teardown_intr,    bus_generic_teardown_intr), 
 
    { 0, 0 } 
}; 


static driver_t xenbus_driver = { 
    "xenbus", 
    xenbus_methods, 
    1,                      /* no softc */ 
}; 
static devclass_t xenbus_devclass; 
static device_t xenbus_dev;
static boolean_t xenbus_probe_delay = TRUE;	/* delay child probes */
 
DRIVER_MODULE(xenbus, nexus, xenbus_driver, xenbus_devclass, 0, 0); 
 
static void 
xenbus_identify(driver_t *driver, device_t parent) 
{ 
 
    /* 
     * Add child device with order of 0 so it gets probed 
     * first
     */ 
    xenbus_dev = BUS_ADD_CHILD(parent, 0, "xenbus", 0);
    if (xenbus_dev == NULL)
	panic("xenbus: could not attach");
} 

static int 
xenbus_probe(device_t dev) 
{ 
    device_set_desc(dev, "xen system"); 
    device_quiet(dev); 
    return (0); 
} 

static int 
xenbus_attach(device_t dev) 
{ 
    /* 
     * First, let our child driver's identify any child devices that 
     * they can find.  Once that is done attach any devices that we 
     * found. 
     */ 
    if (!xenbus_probe_delay) {
    	bus_generic_probe(dev); 
    	bus_generic_attach(dev); 
    }
 
    return 0; 
} 


static int 
xenbus_print_all_resources(device_t dev) 
{ 
    struct xenbus_device *xdev = device_get_ivars(dev); 
    struct resource_list *rl = &xdev->xen_resources;
    int retval = 0;

    if (STAILQ_FIRST(rl))
	    retval += printf(" at");
    
    retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#lx");
    retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#lx");
    retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

    return retval; 
}
 

static int 
xenbus_print_child(device_t bus, device_t child) 
{ 
    int retval = 0; 
 
    retval += bus_print_child_header(bus, child); 
    retval += xenbus_print_all_resources(child); 
    retval += printf(" on motherboard\n");	/* XXX "motherboard", ick */
 
    return (retval); 
} 

static device_t 
xenbus_add_child(device_t bus, int order, const char *name, int unit) 
{ 
    device_t child; 
    struct xenbus_device *xendev; 
 
    xendev = malloc(sizeof(struct xenbus_device), M_XENDEV, 
		   M_NOWAIT | M_ZERO); 
    if (!xendev)
	return(0); 
    resource_list_init(&xendev->xen_resources); 

    child = device_add_child_ordered(bus, order, name, unit);  
 
    /* should we free this in xenbus_child_detached? */ 
    device_set_ivars(child, xendev); 
 
    return(child); 
} 

static struct resource * 
xenbus_alloc_resource(device_t bus, device_t child, int type, int *rid, 
		      u_long start, u_long end, u_long count, u_int flags) 
{ 
    struct xenbus_device *xendev = DEVTOXEN(child); 
    struct resource_list *rl = &xendev->xen_resources; 
 
    return (resource_list_alloc(rl, bus, child, type, rid, start, end, 
				count, flags)); 
} 


static int 
xenbus_release_resource(device_t bus, device_t child, int type, int rid, 
			struct resource *r) 
{ 
    struct xenbus_device *xendev = DEVTOXEN(child); 
    struct resource_list *rl = &xendev->xen_resources; 
 
    return (resource_list_release(rl, bus, child, type, rid, r)); 
} 

static int 
xenbus_set_resource(device_t dev, device_t child, int type, int rid, 
		    u_long start, u_long count) 
{ 
    struct xenbus_device *xendev = DEVTOXEN(child); 
    struct resource_list *rl = &xendev->xen_resources; 
 
    resource_list_add(rl, type, rid, start, start + count - 1, count); 
    return(0); 
} 

static int 
xenbus_get_resource(device_t dev, device_t child, int type, int rid, 
		    u_long *startp, u_long *countp) 
{ 
    struct xenbus_device *xendev = DEVTOXEN(child); 
    struct resource_list *rl = &xendev->xen_resources; 
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
xenbus_delete_resource(device_t dev, device_t child, int type, int rid) 
{ 
    struct xenbus_device *xendev = DEVTOXEN(child); 
    struct resource_list *rl = &xendev->xen_resources; 
 
    resource_list_delete(rl, type, rid); 
} 

static void
xenbus_init(void *unused)
{
    	xenbus_probe_delay = FALSE;
	xenbus_attach(xenbus_dev);
}
SYSINIT(xenbusdev, SI_SUB_PSEUDO, SI_ORDER_FIRST, xenbus_init, NULL);
