/*-
 * Copyright (c) 2000 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 *
 *	$FreeBSD$
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/ctype.h>

#include <machine/clock.h>

#include <machine/resource.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

MALLOC_DEFINE(M_ACPIDEV, "acpidev", "ACPI devices");

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_BUS_MANAGER
MODULE_NAME("ACPI")

/*
 * Character device 
 */

static d_open_t		acpiopen;
static d_close_t	acpiclose;
static d_ioctl_t	acpiioctl;

#define CDEV_MAJOR 152
static struct cdevsw acpi_cdevsw = {
    acpiopen,
    acpiclose,
    noread,
    nowrite,
    acpiioctl,
    nopoll,
    nommap,
    nostrategy,
    "acpi",
    CDEV_MAJOR,
    nodump,
    nopsize,
    0
};

static const char* sleep_state_names[] = {
    "S0", "S1", "S2", "S3", "S4", "S5", "S4B" };

/* this has to be static, as the softc is gone when we need it */
static int acpi_off_state = ACPI_STATE_S5;

static void	acpi_identify(driver_t *driver, device_t parent);
static int	acpi_probe(device_t dev);
static int	acpi_attach(device_t dev);
static device_t	acpi_add_child(device_t bus, int order, const char *name, int unit);
static int	acpi_print_resources(struct resource_list *rl, const char *name, int type,
				     const char *format);
static int	acpi_print_child(device_t bus, device_t child);
static int	acpi_read_ivar(device_t dev, device_t child, int index, uintptr_t *result);
static int	acpi_write_ivar(device_t dev, device_t child, int index, uintptr_t value);
static int	acpi_set_resource(device_t dev, device_t child, int type, int rid, u_long start,
				  u_long count);
static int	acpi_get_resource(device_t dev, device_t child, int type, int rid, u_long *startp,
				  u_long *countp);
static struct resource *acpi_alloc_resource(device_t bus, device_t child, int type, int *rid,
					    u_long start, u_long end, u_long count, u_int flags);
static int	acpi_release_resource(device_t bus, device_t child, int type, int rid, struct resource *r);

static void	acpi_probe_children(device_t bus);
static ACPI_STATUS acpi_probe_child(ACPI_HANDLE handle, UINT32 level, void *context, void **status);

static void	acpi_shutdown_pre_sync(void *arg, int howto);
static void	acpi_shutdown_final(void *arg, int howto);

static void	acpi_enable_fixed_events(struct acpi_softc *sc);

static void	acpi_system_eventhandler_sleep(void *arg, int state);
static void	acpi_system_eventhandler_wakeup(void *arg, int state);
static int	acpi_sleep_state_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t acpi_methods[] = {
    /* Device interface */
    DEVMETHOD(device_identify,		acpi_identify),
    DEVMETHOD(device_probe,		acpi_probe),
    DEVMETHOD(device_attach,		acpi_attach),
    DEVMETHOD(device_shutdown,		bus_generic_shutdown),
    DEVMETHOD(device_suspend,		bus_generic_suspend),
    DEVMETHOD(device_resume,		bus_generic_resume),

    /* Bus interface */
    DEVMETHOD(bus_add_child,		acpi_add_child),
    DEVMETHOD(bus_print_child,		acpi_print_child),
    DEVMETHOD(bus_read_ivar,		acpi_read_ivar),
    DEVMETHOD(bus_write_ivar,		acpi_write_ivar),
    DEVMETHOD(bus_set_resource,		acpi_set_resource),
    DEVMETHOD(bus_get_resource,		acpi_get_resource),
    DEVMETHOD(bus_alloc_resource,	acpi_alloc_resource),
    DEVMETHOD(bus_release_resource,	acpi_release_resource),
    DEVMETHOD(bus_driver_added,		bus_generic_driver_added),
    DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
    DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
    DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
    DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

    {0, 0}
};

static driver_t acpi_driver = {
    "acpi",
    acpi_methods,
    sizeof(struct acpi_softc),
};

devclass_t acpi_devclass;
DRIVER_MODULE(acpi, nexus, acpi_driver, acpi_devclass, 0, 0);

SYSCTL_INT(_debug, OID_AUTO, acpi_debug_layer, CTLFLAG_RW, &AcpiDbgLayer, 0, "");
SYSCTL_INT(_debug, OID_AUTO, acpi_debug_level, CTLFLAG_RW, &AcpiDbgLevel, 0, "");

/*
 * Detect ACPI, perform early initialisation
 */
static void
acpi_identify(driver_t *driver, device_t parent)
{
    device_t			child;
    ACPI_PHYSICAL_ADDRESS	rsdp;
    int				error;
#ifdef ENABLE_DEBUGGER
    char			*debugpoint = getenv("debug.acpi.debugger");
#endif

    FUNCTION_TRACE(__func__);

    if(!cold){
	    printf("Don't load this driver from userland!!\n");
	    return ;
    }

    /*
     * Make sure we're not being doubly invoked.
     */
    if (device_find_child(parent, "acpi", 0) != NULL)
	return_VOID;

    /*
     * Start up the ACPI CA subsystem.
     */
#ifdef ENABLE_DEBUGGER
    if (debugpoint && !strcmp(debugpoint, "init"))
	acpi_EnterDebugger();
#endif
    if ((error = AcpiInitializeSubsystem()) != AE_OK) {
	printf("ACPI: initialisation failed: %s\n", acpi_strerror(error));
	return_VOID;
    }
#ifdef ENABLE_DEBUGGER
    if (debugpoint && !strcmp(debugpoint, "tables"))
	acpi_EnterDebugger();
#endif
    if (((error = AcpiFindRootPointer(&rsdp)) != AE_OK) ||
	((error = AcpiLoadTables(rsdp)) != AE_OK)) {
	printf("ACPI: table load failed: %s\n", acpi_strerror(error));
	return_VOID;
    }
    
    /*
     * Attach the actual ACPI device.
     */
    if ((child = BUS_ADD_CHILD(parent, 0, "acpi", 0)) == NULL) {
	    device_printf(parent, "ACPI: could not attach\n");
	    return_VOID;
    }
}

/*
 * Fetch some descriptive data from ACPI to put in our attach message
 */
static int
acpi_probe(device_t dev)
{
    ACPI_TABLE_HEADER	th;
    char		buf[20];
    int			error;

    FUNCTION_TRACE(__func__);

    if ((error = AcpiGetTableHeader(ACPI_TABLE_XSDT, 1, &th)) != AE_OK) {
	device_printf(dev, "couldn't get XSDT header: %s\n", acpi_strerror(error));
	return_VALUE(ENXIO);
    }
    sprintf(buf, "%.6s %.8s", th.OemId, th.OemTableId);
    device_set_desc_copy(dev, buf);

    return_VALUE(0);
}

static int
acpi_attach(device_t dev)
{
    struct acpi_softc	*sc;
    int			error;
#ifdef ENABLE_DEBUGGER
    char		*debugpoint = getenv("debug.acpi.debugger");
#endif

    FUNCTION_TRACE(__func__);

    sc = device_get_softc(dev);
    bzero(sc, sizeof(*sc));
    sc->acpi_dev = dev;

#ifdef ENABLE_DEBUGGER
    if (debugpoint && !strcmp(debugpoint, "spaces"))
	acpi_EnterDebugger();
#endif

    /*
     * Install the default address space handlers.
     */
    if ((error = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
						ACPI_ADR_SPACE_SYSTEM_MEMORY,
						ACPI_DEFAULT_HANDLER,
						NULL, NULL)) != AE_OK) {
	device_printf(dev, "could not initialise SystemMemory handler: %s\n", acpi_strerror(error));
	return_VALUE(ENXIO);
    }
    if ((error = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
						ACPI_ADR_SPACE_SYSTEM_IO,
						ACPI_DEFAULT_HANDLER,
						NULL, NULL)) != AE_OK) {
	device_printf(dev, "could not initialise SystemIO handler: %s\n", acpi_strerror(error));
	return_VALUE(ENXIO);
    }
    if ((error = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
						ACPI_ADR_SPACE_PCI_CONFIG,
						ACPI_DEFAULT_HANDLER,
						NULL, NULL)) != AE_OK) {
	device_printf(dev, "could not initialise PciConfig handler: %s\n", acpi_strerror(error));
	return_VALUE(ENXIO);
    }

    /*
     * Bring ACPI fully online.
     *
     * Note that we request that device _STA and _INI methods not be run (ACPI_NO_DEVICE_INIT)
     * and the final object initialisation pass be skipped (ACPI_NO_OBJECT_INIT). 
     *
     * XXX We need to arrange for the object init pass after we have attached all our 
     *     child devices.
     */
#ifdef ENABLE_DEBUGGER
    if (debugpoint && !strcmp(debugpoint, "enable"))
	acpi_EnterDebugger();
#endif
    if ((error = AcpiEnableSubsystem(ACPI_NO_DEVICE_INIT | ACPI_NO_OBJECT_INIT)) != AE_OK) {
	device_printf(dev, "could not enable ACPI: %s\n", acpi_strerror(error));
	return_VALUE(ENXIO);
    }

    /*
     * Setup our sysctl tree.
     *
     * XXX: This doesn't check to make sure that none of these fail.
     */
    sysctl_ctx_init(&sc->acpi_sysctl_ctx);
    sc->acpi_sysctl_tree = SYSCTL_ADD_NODE(&sc->acpi_sysctl_ctx,
			       SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO,
			       device_get_name(dev), CTLFLAG_RD, 0, "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "power_button_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_power_button_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "sleep_button_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_sleep_button_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "lid_switch_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_lid_switch_sx, 0, acpi_sleep_state_sysctl, "A", "");
    
    /*
     * Dispatch the default sleep state to devices.
     * TBD: should be configured from userland policy manager.
     */
    sc->acpi_power_button_sx = ACPI_POWER_BUTTON_DEFAULT_SX;
    sc->acpi_sleep_button_sx = ACPI_SLEEP_BUTTON_DEFAULT_SX;
    sc->acpi_lid_switch_sx = ACPI_LID_SWITCH_DEFAULT_SX;

    acpi_enable_fixed_events(sc);

    /*
     * Scan the namespace and attach/initialise children.
     */
#ifdef ENABLE_DEBUGGER
    if (debugpoint && !strcmp(debugpoint, "probe"))
	acpi_EnterDebugger();
#endif
    if (!acpi_disabled("bus"))
	acpi_probe_children(dev);

    /*
     * Register our shutdown handlers
     */
    EVENTHANDLER_REGISTER(shutdown_pre_sync, acpi_shutdown_pre_sync, sc, SHUTDOWN_PRI_LAST);
    EVENTHANDLER_REGISTER(shutdown_final, acpi_shutdown_final, sc, SHUTDOWN_PRI_LAST);

    /*
     * Register our acpi event handlers.
     * XXX should be configurable eg. via userland policy manager.
     */
    EVENTHANDLER_REGISTER(acpi_sleep_event, acpi_system_eventhandler_sleep, sc, ACPI_EVENT_PRI_LAST);
    EVENTHANDLER_REGISTER(acpi_wakeup_event, acpi_system_eventhandler_wakeup, sc, ACPI_EVENT_PRI_LAST);

    /*
     * Flag our initial states.
     */
    sc->acpi_enabled = 1;
    sc->acpi_sstate = ACPI_STATE_S0;

    /*
     * Create the control device
     */
    sc->acpi_dev_t = make_dev(&acpi_cdevsw, 0, 0, 5, 0660, "acpi");
    sc->acpi_dev_t->si_drv1 = sc;

#ifdef ENABLE_DEBUGGER
    if (debugpoint && !strcmp(debugpoint, "running"))
	acpi_EnterDebugger();
#endif
    return_VALUE(0);
}

/*
 * Handle a new device being added
 */
static device_t
acpi_add_child(device_t bus, int order, const char *name, int unit)
{
    struct acpi_device	*ad;
    device_t		child;

    if ((ad = malloc(sizeof(*ad), M_ACPIDEV, M_NOWAIT)) == NULL)
	return(NULL);
    bzero(ad, sizeof(*ad));

    resource_list_init(&ad->ad_rl);
    
    child = device_add_child_ordered(bus, order, name, unit);
    if (child != NULL)
	device_set_ivars(child, ad);
    return(child);
}

/*
 * Print child device resource usage
 */
static int
acpi_print_resources(struct resource_list *rl, const char *name, int type, const char *format)
{
    struct resource_list_entry	*rle;
    int				printed, retval;

    printed = 0;
    retval = 0;

    if (!SLIST_FIRST(rl))
	return(0);

    /* Yes, this is kinda cheating */
    SLIST_FOREACH(rle, rl, link) {
	if (rle->type == type) {
	    if (printed == 0)
		retval += printf(" %s ", name);
	    else if (printed > 0)
		retval += printf(",");
	    printed++;
	    retval += printf(format, rle->start);
	    if (rle->count > 1) {
		retval += printf("-");
		retval += printf(format, rle->start +
				 rle->count - 1);
	    }
	}
    }
    return(retval);
}

static int
acpi_print_child(device_t bus, device_t child)
{
    struct acpi_device		*adev = device_get_ivars(child);
    struct resource_list	*rl = &adev->ad_rl;
    int retval = 0;

    retval += bus_print_child_header(bus, child);
    retval += acpi_print_resources(rl, "port",  SYS_RES_IOPORT, "%#lx");
    retval += acpi_print_resources(rl, "iomem", SYS_RES_MEMORY, "%#lx");
    retval += acpi_print_resources(rl, "irq",   SYS_RES_IRQ,    "%ld");
    retval += bus_print_child_footer(bus, child);

    return(retval);
}


/*
 * Handle per-device ivars
 */
static int
acpi_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
    struct acpi_device	*ad;

    if ((ad = device_get_ivars(child)) == NULL) {
	printf("device has no ivars\n");
	return(ENOENT);
    }

    switch(index) {
	/* ACPI ivars */
    case ACPI_IVAR_HANDLE:
	*(ACPI_HANDLE *)result = ad->ad_handle;
	break;
    case ACPI_IVAR_MAGIC:
	*(int *)result = ad->ad_magic;
	break;
    case ACPI_IVAR_PRIVATE:
	*(void **)result = ad->ad_private;
	break;

    default:
	panic("bad ivar read request (%d)\n", index);
	return(ENOENT);
    }
    return(0);
}

static int
acpi_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
    struct acpi_device	*ad;

    if ((ad = device_get_ivars(child)) == NULL) {
	printf("device has no ivars\n");
	return(ENOENT);
    }

    switch(index) {
	/* ACPI ivars */
    case ACPI_IVAR_HANDLE:
	ad->ad_handle = (ACPI_HANDLE)value;
	break;
    case ACPI_IVAR_MAGIC:
	ad->ad_magic = (int )value;
	break;
    case ACPI_IVAR_PRIVATE:
	ad->ad_private = (void *)value;
	break;

    default:
	panic("bad ivar write request (%d)\n", index);
	return(ENOENT);
    }
    return(0);
}

/*
 * Handle child resource allocation/removal
 */
static int
acpi_set_resource(device_t dev, device_t child, int type, int rid, u_long start, u_long count)
{
    struct acpi_device		*ad = device_get_ivars(child);
    struct resource_list	*rl = &ad->ad_rl;

    resource_list_add(rl, type, rid, start, start + count -1, count);

    return(0);
}

static int
acpi_get_resource(device_t dev, device_t child, int type, int rid, u_long *startp, u_long *countp)
{
    struct acpi_device		*ad = device_get_ivars(child);
    struct resource_list	*rl = &ad->ad_rl;
    struct resource_list_entry	*rle;

    rle = resource_list_find(rl, type, rid);
    if (!rle)
	return(ENOENT);
	
    if (startp)
	*startp = rle->start;
    if (countp)
	*countp = rle->count;

    return(0);
}

static struct resource *
acpi_alloc_resource(device_t bus, device_t child, int type, int *rid,
		    u_long start, u_long end, u_long count, u_int flags)
{
    struct acpi_device *ad = device_get_ivars(child);
    struct resource_list *rl = &ad->ad_rl;

    return(resource_list_alloc(rl, bus, child, type, rid, start, end, count, flags));
}

static int
acpi_release_resource(device_t bus, device_t child, int type, int rid, struct resource *r)
{
    struct acpi_device *ad = device_get_ivars(child);
    struct resource_list *rl = &ad->ad_rl;

    return(resource_list_release(rl, bus, child, type, rid, r));
}

/*
 * Scan relevant portions of the ACPI namespace and attach child devices.
 *
 * Note that we only expect to find devices in the \_TZ_, \_SI_ and \_SB_ scopes, 
 * and \_TZ_ becomes obsolete in the ACPI 2.0 spec.
 */
static void
acpi_probe_children(device_t bus)
{
    ACPI_HANDLE		parent;
    static char		*scopes[] = {"\\_TZ_", "\\_SI", "\\_SB_", NULL};
    int			i;

    FUNCTION_TRACE(__func__);

    /*
     * Create any static children by calling device identify methods.
     */
    DEBUG_PRINT(TRACE_OBJECTS, ("device identify routines\n"));
    bus_generic_probe(bus);

    /*
     * Scan the namespace and insert placeholders for all the devices that
     * we find.
     *
     * Note that we use AcpiWalkNamespace rather than AcpiGetDevices because
     * we want to create nodes for all devices, not just those that are currently
     * present. (This assumes that we don't want to create/remove devices as they
     * appear, which might be smarter.)
     */
    DEBUG_PRINT(TRACE_OBJECTS, ("namespace scan\n"));
    for (i = 0; scopes[i] != NULL; i++)
	if ((AcpiGetHandle(ACPI_ROOT_OBJECT, scopes[i], &parent)) == AE_OK)
	    AcpiWalkNamespace(ACPI_TYPE_ANY, parent, 100, acpi_probe_child, bus, NULL);

    /*
     * Scan all of the child devices we have created and let them probe/attach.
     */
    DEBUG_PRINT(TRACE_OBJECTS, ("first bus_generic_attach\n"));
    bus_generic_attach(bus);

    /*
     * Some of these children may have attached others as part of their attach
     * process (eg. the root PCI bus driver), so rescan.
     */
    DEBUG_PRINT(TRACE_OBJECTS, ("second bus_generic_attach\n"));
    bus_generic_attach(bus);

    return_VOID;
}

/*
 * Evaluate a child device and determine whether we might attach a device to
 * it.
 */
static ACPI_STATUS
acpi_probe_child(ACPI_HANDLE handle, UINT32 level, void *context, void **status)
{
    ACPI_OBJECT_TYPE	type;
    device_t		child, bus = (device_t)context;

    FUNCTION_TRACE(__func__);

    /*
     * Skip this device if we think we'll have trouble with it.
     */
    if (acpi_avoid(handle))
	return_ACPI_STATUS(AE_OK);

    if (AcpiGetType(handle, &type) == AE_OK) {
	switch(type) {
	case ACPI_TYPE_DEVICE:
	case ACPI_TYPE_PROCESSOR:
	case ACPI_TYPE_THERMAL:
	case ACPI_TYPE_POWER:
	    if (acpi_disabled("children"))
		break;
	    /* 
	     * Create a placeholder device for this node.  Sort the placeholder
	     * so that the probe/attach passes will run breadth-first.
	     */
	    DEBUG_PRINT(TRACE_OBJECTS, ("scanning '%s'\n", acpi_name(handle)))
	    child = BUS_ADD_CHILD(bus, level * 10, NULL, -1);
	    acpi_set_handle(child, handle);
	    DEBUG_EXEC(device_probe_and_attach(child));
	    break;
	}
    }
    return_ACPI_STATUS(AE_OK);
}

static void
acpi_shutdown_pre_sync(void *arg, int howto)
{
    /*
     * Disable all ACPI events before soft off, otherwise the system
     * will be turned on again on some laptops.
     *
     * XXX this should probably be restricted to masking some events just
     *     before powering down, since we may still need ACPI during the
     *     shutdown process.
     */
    acpi_Disable((struct acpi_softc *)arg);
}

static void
acpi_shutdown_final(void *arg, int howto)
{
    ACPI_STATUS	status;

    if (howto & RB_POWEROFF) {
	printf("Power system off using ACPI...\n");
	if ((status = AcpiEnterSleepState(acpi_off_state)) != AE_OK) {
	    printf("ACPI power-off failed - %s\n", acpi_strerror(status));
	} else {
	    DELAY(1000000);
	    printf("ACPI power-off failed - timeout\n");
	}
    }
}

static void
acpi_enable_fixed_events(struct acpi_softc *sc)
{
	static int	first_time = 1;
#define MSGFORMAT "%s button is handled as a fixed feature programming model.\n"

	/* Enable and clear fixed events and install handlers. */
	if (AcpiGbl_FADT != NULL && AcpiGbl_FADT->PwrButton == 0) {
		AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, ACPI_EVENT_FIXED);
		AcpiClearEvent(ACPI_EVENT_POWER_BUTTON, ACPI_EVENT_FIXED);
		AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
		    acpi_eventhandler_power_button_for_sleep, sc);
		if (first_time) {
			device_printf(sc->acpi_dev, MSGFORMAT, "power");
		}
	}
	if (AcpiGbl_FADT != NULL && AcpiGbl_FADT->SleepButton == 0) {
		AcpiEnableEvent(ACPI_EVENT_SLEEP_BUTTON, ACPI_EVENT_FIXED);
		AcpiClearEvent(ACPI_EVENT_SLEEP_BUTTON, ACPI_EVENT_FIXED);
		AcpiInstallFixedEventHandler(ACPI_EVENT_SLEEP_BUTTON,
		    acpi_eventhandler_sleep_button_for_sleep, sc);
		if (first_time) {
			device_printf(sc->acpi_dev, MSGFORMAT, "sleep");
		}
	}

	first_time = 0;
}

/*
 * Returns true if the device is actually present and should
 * be attached to.  This requires the present, enabled, UI-visible 
 * and diagnostics-passed bits to be set.
 */
BOOLEAN
acpi_DeviceIsPresent(device_t dev)
{
    ACPI_HANDLE		h;
    ACPI_DEVICE_INFO	devinfo;
    ACPI_STATUS		error;
    
    if ((h = acpi_get_handle(dev)) == NULL)
	return(FALSE);
    if ((error = AcpiGetObjectInfo(h, &devinfo)) != AE_OK)
	return(FALSE);
    if ((devinfo.Valid & ACPI_VALID_HID) && (devinfo.CurrentStatus & 0xf))
	return(TRUE);
    return(FALSE);
}

/*
 * Match a HID string against a device
 */
BOOLEAN
acpi_MatchHid(device_t dev, char *hid) 
{
    ACPI_HANDLE		h;
    ACPI_DEVICE_INFO	devinfo;
    ACPI_STATUS		error;

    if (hid == NULL)
	return(FALSE);
    if ((h = acpi_get_handle(dev)) == NULL)
	return(FALSE);
    if ((error = AcpiGetObjectInfo(h, &devinfo)) != AE_OK)
	return(FALSE);
    if ((devinfo.Valid & ACPI_VALID_HID) && !strcmp(hid, devinfo.HardwareId))
	return(TRUE);
    return(FALSE);
}

/*
 * Return the handle of a named object within our scope, ie. that of (parent)
 * or one if its parents.
 */
ACPI_STATUS
acpi_GetHandleInScope(ACPI_HANDLE parent, char *path, ACPI_HANDLE *result)
{
    ACPI_HANDLE		r;
    ACPI_STATUS		status;

    /* walk back up the tree to the root */
    for (;;) {
	status = AcpiGetHandle(parent, path, &r);
	if (status == AE_OK) {
	    *result = r;
	    return(AE_OK);
	}
	if (status != AE_NOT_FOUND)
	    return(AE_OK);
	if (AcpiGetParent(parent, &r) != AE_OK)
	    return(AE_NOT_FOUND);
	parent = r;
    }
}

/*
 * Allocate a buffer with a preset data size.
 */
ACPI_BUFFER *
acpi_AllocBuffer(int size)
{
    ACPI_BUFFER	*buf;

    if ((buf = malloc(size + sizeof(*buf), M_ACPIDEV, M_NOWAIT)) == NULL)
	return(NULL);
    buf->Length = size;
    buf->Pointer = (void *)(buf + 1);
    return(buf);
}

/*
 * Perform the tedious double-get procedure required for fetching something into
 * an ACPI_BUFFER that has not been initialised.
 */
ACPI_STATUS
acpi_GetIntoBuffer(ACPI_HANDLE handle, ACPI_STATUS (*func)(ACPI_HANDLE, ACPI_BUFFER *), ACPI_BUFFER *buf)
{
    ACPI_STATUS	status;

    buf->Length = 0;
    buf->Pointer = NULL;

    if ((status = func(handle, buf)) != AE_BUFFER_OVERFLOW)
	return(status);
    if ((buf->Pointer = AcpiOsCallocate(buf->Length)) == NULL)
	return(AE_NO_MEMORY);
    return(func(handle, buf));
}

/*
 * Perform the tedious double-evaluate procedure for evaluating something into
 * an ACPI_BUFFER that has not been initialised.  Note that this evaluates
 * twice, so avoid applying this to things that may have side-effects.
 *
 * This is like AcpiEvaluateObject with automatic buffer allocation.
 */
ACPI_STATUS
acpi_EvaluateIntoBuffer(ACPI_HANDLE object, ACPI_STRING pathname, ACPI_OBJECT_LIST *params,
			ACPI_BUFFER *buf)
{
    ACPI_STATUS	status;
    
    buf->Length = 0;
    buf->Pointer = NULL;

    if ((status = AcpiEvaluateObject(object, pathname, params, buf)) != AE_BUFFER_OVERFLOW)
	return(status);
    if ((buf->Pointer = AcpiOsCallocate(buf->Length)) == NULL)
	return(AE_NO_MEMORY);
    return(AcpiEvaluateObject(object, pathname, params, buf));
}

/*
 * Evaluate a path that should return an integer.
 */
ACPI_STATUS
acpi_EvaluateInteger(ACPI_HANDLE handle, char *path, int *number)
{
    ACPI_STATUS	error;
    ACPI_BUFFER	buf;
    ACPI_OBJECT	param;

    if (handle == NULL)
	handle = ACPI_ROOT_OBJECT;
    buf.Pointer = &param;
    buf.Length = sizeof(param);
    if ((error = AcpiEvaluateObject(handle, path, NULL, &buf)) == AE_OK) {
	if (param.Type == ACPI_TYPE_INTEGER) {
	    *number = param.Integer.Value;
	} else {
	    error = AE_TYPE;
	}
    }
    return(error);
}

/*
 * Iterate over the elements of an a package object, calling the supplied
 * function for each element.
 *
 * XXX possible enhancement might be to abort traversal on error.
 */
ACPI_STATUS
acpi_ForeachPackageObject(ACPI_OBJECT *pkg, void (* func)(ACPI_OBJECT *comp, void *arg), void *arg)
{
    ACPI_OBJECT	*comp;
    int		i;
    
    if ((pkg == NULL) || (pkg->Type != ACPI_TYPE_PACKAGE))
	return(AE_BAD_PARAMETER);

    /* iterate over components */
    for (i = 0, comp = pkg->Package.Elements; i < pkg->Package.Count; i++, comp++)
	func(comp, arg);

    return(AE_OK);
}


static ACPI_STATUS __inline
acpi_wakeup(UINT8 state)
{
	UINT16			Count;
	ACPI_STATUS		Status;
	ACPI_OBJECT_LIST	Arg_list;
	ACPI_OBJECT		Arg;
	ACPI_OBJECT		Objects[3]; /* package plus 2 number objects */
	ACPI_BUFFER		ReturnBuffer;

	FUNCTION_TRACE_U32(__func__, state);

	/* wait for the WAK_STS bit */
	Count = 0;
	while (!(AcpiHwRegisterBitAccess(ACPI_READ, ACPI_MTX_LOCK, WAK_STS))) {
		AcpiOsSleepUsec(1000);
		/*
		 * Some BIOSes don't set WAK_STS at all,
		 * give up waiting for wakeup if we time out.
		 */
		if (Count > 1000) {
			break;	/* giving up */
		}
		Count++;
	}

	/*
	 * Evaluate the _WAK method
	 */
	MEMSET(&Arg_list, 0, sizeof(Arg_list));
	Arg_list.Count = 1;
	Arg_list.Pointer = &Arg;

	MEMSET(&Arg, 0, sizeof(Arg));
	Arg.Type = ACPI_TYPE_INTEGER;
	Arg.Integer.Value = state;

	/* Set up _WAK result code buffer */
	MEMSET(Objects, 0, sizeof(Objects));
	ReturnBuffer.Length = sizeof(Objects);
	ReturnBuffer.Pointer = Objects;

	AcpiEvaluateObject (NULL, "\\_WAK", &Arg_list, &ReturnBuffer);

	Status = AE_OK;
	/* Check result code for _WAK */
	if (Objects[0].Type != ACPI_TYPE_PACKAGE ||
	    Objects[1].Type != ACPI_TYPE_INTEGER  ||
	    Objects[2].Type != ACPI_TYPE_INTEGER) {
		/*
		 * In many BIOSes, _WAK doesn't return a result code.
		 * We don't need to worry about it too much :-).
		 */
		DEBUG_PRINT (ACPI_INFO,
		    ("acpi_wakeup: _WAK result code is corrupted, "
		     "but should be OK.\n"));
	} else {
		/* evaluate status code */
		switch (Objects[1].Integer.Value) {
		case 0x00000001:
			DEBUG_PRINT (ACPI_ERROR,
			    ("acpi_wakeup: Wake was signaled "
			     "but failed due to lack of power.\n"));
			Status = AE_ERROR;
			break;

		case 0x00000002:
			DEBUG_PRINT (ACPI_ERROR,
			    ("acpi_wakeup: Wake was signaled "
			     "but failed due to thermal condition.\n"));
			Status = AE_ERROR;
			break;
		}
		/* evaluate PSS code */
		if (Objects[2].Integer.Value == 0) {
			DEBUG_PRINT (ACPI_ERROR,
			    ("acpi_wakeup: The targeted S-state "
			     "was not entered because of too much current "
			     "being drawn from the power supply.\n"));
			Status = AE_ERROR;
		}
	}

	return_ACPI_STATUS(Status);
}

/*
 * Set the system sleep state
 *
 * Currently we only support S1 and S5
 */
ACPI_STATUS
acpi_SetSleepState(struct acpi_softc *sc, int state)
{
    ACPI_STATUS	status = AE_OK;

    FUNCTION_TRACE_U32(__func__, state);

    switch (state) {
    case ACPI_STATE_S0:	/* XXX only for testing */
	status = AcpiEnterSleepState((UINT8)state);
	if (status != AE_OK) {
	    device_printf(sc->acpi_dev, "AcpiEnterSleepState failed - %s\n", acpi_strerror(status));
	}
	break;

    case ACPI_STATE_S1:
	/*
	 * Inform all devices that we are going to sleep.
	 */
	if (DEVICE_SUSPEND(root_bus) != 0) {
	    /*
	     * Re-wake the system.
	     *
	     * XXX note that a better two-pass approach with a 'veto' pass
	     *     followed by a "real thing" pass would be better, but the
	     *     current bus interface does not provide for this.
	     */
	    DEVICE_RESUME(root_bus);
	    return_ACPI_STATUS(AE_ERROR);
	}
	sc->acpi_sstate = state;
	status = AcpiEnterSleepState((UINT8)state);
	if (status != AE_OK) {
	    device_printf(sc->acpi_dev, "AcpiEnterSleepState failed - %s\n", acpi_strerror(status));
	    break;
	}
	acpi_wakeup((UINT8)state);
	DEVICE_RESUME(root_bus);
	sc->acpi_sstate = ACPI_STATE_S0;
	acpi_enable_fixed_events(sc);
	break;

    case ACPI_STATE_S3:
	acpi_off_state = ACPI_STATE_S3;
	/* FALLTHROUGH */
    case ACPI_STATE_S5:
	/*
	 * Shut down cleanly and power off.  This will call us back through the
	 * shutdown handlers.
	 */
	shutdown_nice(RB_POWEROFF);
	break;

    default:
	status = AE_BAD_PARAMETER;
	break;
    }
    return_ACPI_STATUS(status);
}

/*
 * Enable/Disable ACPI
 */
ACPI_STATUS
acpi_Enable(struct acpi_softc *sc)
{
    ACPI_STATUS	status;
    u_int32_t	flags;

    FUNCTION_TRACE(__func__);

    flags = ACPI_NO_ADDRESS_SPACE_INIT | ACPI_NO_HARDWARE_INIT |
            ACPI_NO_DEVICE_INIT | ACPI_NO_OBJECT_INIT;
    if (!sc->acpi_enabled) {
	status = AcpiEnableSubsystem(flags);
    } else {
	status = AE_OK;
    }
    if (status == AE_OK)
	sc->acpi_enabled = 1;
    return_ACPI_STATUS(status);
}

ACPI_STATUS
acpi_Disable(struct acpi_softc *sc)
{
    ACPI_STATUS	status;

    FUNCTION_TRACE(__func__);

    if (sc->acpi_enabled) {
	status = AcpiDisable();
    } else {
	status = AE_OK;
    }
    if (status == AE_OK)
	sc->acpi_enabled = 0;
    return_ACPI_STATUS(status);
}

/*
 * ACPI Event Handlers
 */

/* System Event Handlers (registered by EVENTHANDLER_REGISTER) */

static void
acpi_system_eventhandler_sleep(void *arg, int state)
{
    FUNCTION_TRACE_U32(__func__, state);

    if (state >= ACPI_STATE_S0 && state <= ACPI_S_STATES_MAX)
	acpi_SetSleepState((struct acpi_softc *)arg, state);
    return_VOID;
}

static void
acpi_system_eventhandler_wakeup(void *arg, int state)
{
    FUNCTION_TRACE_U32(__func__, state);

    /* Well, what to do? :-) */

    return_VOID;
}

/* 
 * ACPICA Event Handlers (FixedEvent, also called from button notify handler)
 */
UINT32
acpi_eventhandler_power_button_for_sleep(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    FUNCTION_TRACE(__func__);

    EVENTHANDLER_INVOKE(acpi_sleep_event, sc->acpi_power_button_sx);

    return_VALUE(INTERRUPT_HANDLED);
}

UINT32
acpi_eventhandler_power_button_for_wakeup(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    FUNCTION_TRACE(__func__);

    EVENTHANDLER_INVOKE(acpi_wakeup_event, sc->acpi_power_button_sx);

    return_VALUE(INTERRUPT_HANDLED);
}

UINT32
acpi_eventhandler_sleep_button_for_sleep(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    FUNCTION_TRACE(__func__);

    EVENTHANDLER_INVOKE(acpi_sleep_event, sc->acpi_sleep_button_sx);

    return_VALUE(INTERRUPT_HANDLED);
}

UINT32
acpi_eventhandler_sleep_button_for_wakeup(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    FUNCTION_TRACE(__func__);

    EVENTHANDLER_INVOKE(acpi_wakeup_event, sc->acpi_sleep_button_sx);

    return_VALUE(INTERRUPT_HANDLED);
}

/*
 * XXX This is kinda ugly, and should not be here.
 */
struct acpi_staticbuf {
    ACPI_BUFFER	buffer;
    char	data[512];
};

char *
acpi_strerror(ACPI_STATUS excep)
{
    static struct acpi_staticbuf	buf;

    buf.buffer.Length = 512;
    buf.buffer.Pointer = &buf.data[0];

    if (AcpiFormatException(excep, &buf.buffer) == AE_OK)
	return(buf.buffer.Pointer);
    return("(error formatting exception)");
}

char *
acpi_name(ACPI_HANDLE handle)
{
    static struct acpi_staticbuf	buf;

    buf.buffer.Length = 512;
    buf.buffer.Pointer = &buf.data[0];

    if (AcpiGetName(handle, ACPI_FULL_PATHNAME, &buf.buffer) == AE_OK)
	return(buf.buffer.Pointer);
    return("(unknown path)");
}

/*
 * Debugging/bug-avoidance.  Avoid trying to fetch info on various
 * parts of the namespace.
 */
int
acpi_avoid(ACPI_HANDLE handle)
{
    char	*cp, *np;
    int		len;

    np = acpi_name(handle);
    if (*np == '\\')
	np++;
    if ((cp = getenv("debug.acpi.avoid")) == NULL)
	return(0);

    /* scan the avoid list checking for a match */
    for (;;) {
	while ((*cp != 0) && isspace(*cp))
	    cp++;
	if (*cp == 0)
	    break;
	len = 0;
	while ((cp[len] != 0) && !isspace(cp[len]))
	    len++;
	if (!strncmp(cp, np, len)) {
	    DEBUG_PRINT(TRACE_OBJECTS, ("avoiding '%s'\n", np));
	    return(1);
	}
	cp += len;
    }
    return(0);
}

/*
 * Debugging/bug-avoidance.  Disable ACPI subsystem components.
 */
int
acpi_disabled(char *subsys)
{
    char	*cp;
    int		len;

    if ((cp = getenv("debug.acpi.disable")) == NULL)
	return(0);
    if (!strcmp(cp, "all"))
	return(1);

    /* scan the disable list checking for a match */
    for (;;) {
	while ((*cp != 0) && isspace(*cp))
	    cp++;
	if (*cp == 0)
	    break;
	len = 0;
	while ((cp[len] != 0) && !isspace(cp[len]))
	    len++;
	if (!strncmp(cp, subsys, len)) {
	    DEBUG_PRINT(TRACE_OBJECTS, ("disabled '%s'\n", subsys));
	    return(1);
	}
	cp += len;
    }
    return(0);
}

/*
 * Control interface.
 *
 * We multiplex ioctls for all participating ACPI devices here.  Individual 
 * drivers wanting to be accessible via /dev/acpi should use the register/deregister
 * interface to make their handlers visible.
 */
struct acpi_ioctl_hook
{
    TAILQ_ENTRY(acpi_ioctl_hook)	link;
    u_long				cmd;
    int					(* fn)(u_long cmd, caddr_t addr, void *arg);
    void				*arg;
};

static TAILQ_HEAD(,acpi_ioctl_hook)	acpi_ioctl_hooks;
static int				acpi_ioctl_hooks_initted;

/*
 * Register an ioctl handler.
 */
int
acpi_register_ioctl(u_long cmd, int (* fn)(u_long cmd, caddr_t addr, void *arg), void *arg)
{
    struct acpi_ioctl_hook	*hp;

    if ((hp = malloc(sizeof(*hp), M_ACPIDEV, M_NOWAIT)) == NULL)
	return(ENOMEM);
    hp->cmd = cmd;
    hp->fn = fn;
    hp->arg = arg;
    if (acpi_ioctl_hooks_initted == 0) {
	TAILQ_INIT(&acpi_ioctl_hooks);
	acpi_ioctl_hooks_initted = 1;
    }
    TAILQ_INSERT_TAIL(&acpi_ioctl_hooks, hp, link);
    return(0);
}

/*
 * Deregister an ioctl handler.
 */
void	
acpi_deregister_ioctl(u_long cmd, int (* fn)(u_long cmd, caddr_t addr, void *arg))
{
    struct acpi_ioctl_hook	*hp;

    TAILQ_FOREACH(hp, &acpi_ioctl_hooks, link)
	if ((hp->cmd == cmd) && (hp->fn == fn))
	    break;

    if (hp != NULL) {
	TAILQ_REMOVE(&acpi_ioctl_hooks, hp, link);
	free(hp, M_ACPIDEV);
    }
}

static int
acpiopen(dev_t dev, int flag, int fmt, struct proc *p)
{
    return(0);
}

static int
acpiclose(dev_t dev, int flag, int fmt, struct proc *p)
{
    return(0);
}

static int
acpiioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
    struct acpi_softc		*sc;
    struct acpi_ioctl_hook	*hp;
    int				error, xerror, state;

    error = state = 0;
    sc = dev->si_drv1;

    /*
     * Scan the list of registered ioctls, looking for handlers.
     */
    if (acpi_ioctl_hooks_initted) {
	TAILQ_FOREACH(hp, &acpi_ioctl_hooks, link) {
	    if (hp->cmd == cmd) {
		xerror = hp->fn(cmd, addr, hp->arg);
		if (xerror != 0)
		    error = xerror;
		goto out;
	    }
	}
    }

    /*
     * Core system ioctls.
     */
    switch (cmd) {
    case ACPIIO_ENABLE:
	if (ACPI_FAILURE(acpi_Enable(sc)))
	    error = ENXIO;
	break;

    case ACPIIO_DISABLE:
	if (ACPI_FAILURE(acpi_Disable(sc)))
	    error = ENXIO;
	break;

    case ACPIIO_SETSLPSTATE:
	if (!sc->acpi_enabled) {
	    error = ENXIO;
	    break;
	}
	state = *(int *)addr;
	if (state >= ACPI_STATE_S0  && state <= ACPI_S_STATES_MAX) {
	    acpi_SetSleepState(sc, state);
	} else {
	    error = EINVAL;
	}
	break;

    default:
	if (error == 0)
	    error = EINVAL;
	break;
    }

out:
    return(error);
}

static int
acpi_sleep_state_sysctl(SYSCTL_HANDLER_ARGS)
{
    char sleep_state[10];
    int error;
    u_int new_state, old_state;

    old_state = *(u_int *)oidp->oid_arg1;
    if (old_state > ACPI_S_STATES_MAX)
	strcpy(sleep_state, "unknown");
    else
	strncpy(sleep_state, sleep_state_names[old_state],
	    sizeof(sleep_state_names[old_state]));
    error = sysctl_handle_string(oidp, sleep_state, sizeof(sleep_state), req);
    if (error == 0 && req->newptr != NULL) {
	for (new_state = ACPI_STATE_S0; new_state <= ACPI_S_STATES_MAX; new_state++)
	    if (strncmp(sleep_state, sleep_state_names[new_state],
		sizeof(sleep_state)) == 0)
		break;
	if (new_state != old_state && new_state <= ACPI_S_STATES_MAX)
	    *(u_int *)oidp->oid_arg1 = new_state;
	else
	    error = EINVAL;
    }
    return(error);
}

#ifdef ACPI_DEBUG
/*
 * Support for parsing debug options from the kernel environment.
 *
 * Bits may be set in the AcpiDbgLayer and AcpiDbgLevel debug registers
 * by specifying the names of the bits in the debug.acpi.layer and
 * debug.acpi.level environment variables.  Bits may be unset by 
 * prefixing the bit name with !.
 */
struct debugtag
{
    char	*name;
    UINT32	value;
};

static struct debugtag	dbg_layer[] = {
    {"ACPI_UTILITIES",		ACPI_UTILITIES},
    {"ACPI_HARDWARE",		ACPI_HARDWARE},
    {"ACPI_EVENTS",		ACPI_EVENTS},
    {"ACPI_TABLES",		ACPI_TABLES},
    {"ACPI_NAMESPACE",		ACPI_NAMESPACE},
    {"ACPI_PARSER",		ACPI_PARSER},
    {"ACPI_DISPATCHER",		ACPI_DISPATCHER},
    {"ACPI_EXECUTER",		ACPI_EXECUTER},
    {"ACPI_RESOURCES",		ACPI_RESOURCES},
    {"ACPI_DEVICES",		ACPI_DEVICES},
    {"ACPI_POWER",		ACPI_POWER},
    {"ACPI_BUS_MANAGER",	ACPI_BUS_MANAGER},
    {"ACPI_POWER_CONTROL",	ACPI_POWER_CONTROL},
    {"ACPI_EMBEDDED_CONTROLLER", ACPI_EMBEDDED_CONTROLLER},
    {"ACPI_PROCESSOR_CONTROL",	ACPI_PROCESSOR_CONTROL},
    {"ACPI_AC_ADAPTER",		ACPI_AC_ADAPTER},
    {"ACPI_BATTERY",		ACPI_BATTERY},
    {"ACPI_BUTTON",		ACPI_BUTTON},
    {"ACPI_SYSTEM",		ACPI_SYSTEM},
    {"ACPI_THERMAL_ZONE",	ACPI_THERMAL_ZONE},
    {"ACPI_DEBUGGER",		ACPI_DEBUGGER},
    {"ACPI_OS_SERVICES",	ACPI_OS_SERVICES},
    {"ACPI_ALL_COMPONENTS",	ACPI_ALL_COMPONENTS},
    {NULL, 0}
};

static struct debugtag dbg_level[] = {
    {"ACPI_OK",			ACPI_OK},
    {"ACPI_INFO",		ACPI_INFO},
    {"ACPI_WARN",		ACPI_WARN},
    {"ACPI_ERROR",		ACPI_ERROR},
    {"ACPI_FATAL",		ACPI_FATAL},
    {"ACPI_DEBUG_OBJECT",	ACPI_DEBUG_OBJECT},
    {"ACPI_ALL",		ACPI_ALL},
    {"TRACE_THREADS",		TRACE_THREADS},
    {"TRACE_PARSE",		TRACE_PARSE},
    {"TRACE_DISPATCH",		TRACE_DISPATCH},
    {"TRACE_LOAD",		TRACE_LOAD},
    {"TRACE_EXEC",		TRACE_EXEC},
    {"TRACE_NAMES",		TRACE_NAMES},
    {"TRACE_OPREGION",		TRACE_OPREGION},
    {"TRACE_BFIELD",		TRACE_BFIELD},
    {"TRACE_TRASH",		TRACE_TRASH},
    {"TRACE_TABLES",		TRACE_TABLES},
    {"TRACE_FUNCTIONS",		TRACE_FUNCTIONS},
    {"TRACE_VALUES",		TRACE_VALUES},
    {"TRACE_OBJECTS",		TRACE_OBJECTS},
    {"TRACE_ALLOCATIONS",	TRACE_ALLOCATIONS},
    {"TRACE_RESOURCES",		TRACE_RESOURCES},
    {"TRACE_IO",		TRACE_IO},
    {"TRACE_INTERRUPTS",	TRACE_INTERRUPTS},
    {"TRACE_USER_REQUESTS",	TRACE_USER_REQUESTS},
    {"TRACE_PACKAGE",		TRACE_PACKAGE},
    {"TRACE_MUTEX",		TRACE_MUTEX},
    {"TRACE_INIT",		TRACE_INIT},
    {"TRACE_ALL",		TRACE_ALL},
    {"VERBOSE_AML_DISASSEMBLE",	VERBOSE_AML_DISASSEMBLE},
    {"VERBOSE_INFO",		VERBOSE_INFO},
    {"VERBOSE_TABLES",		VERBOSE_TABLES},
    {"VERBOSE_EVENTS",		VERBOSE_EVENTS},
    {"VERBOSE_ALL",		VERBOSE_ALL},
    {NULL, 0}
};    

static void
acpi_parse_debug(char *cp, struct debugtag *tag, UINT32 *flag)
{
    char	*ep;
    int		i, l;
    int		set;

    while (*cp) {
	if (isspace(*cp)) {
	    cp++;
	    continue;
	}
	ep = cp;
	while (*ep && !isspace(*ep))
	    ep++;
	if (*cp == '!') {
	    set = 0;
	    cp++;
	    if (cp == ep)
		continue;
	} else {
	    set = 1;
	}
	l = ep - cp;
	for (i = 0; tag[i].name != NULL; i++) {
	    if (!strncmp(cp, tag[i].name, l)) {
		if (set) {
		    *flag |= tag[i].value;
		} else {
		    *flag &= ~tag[i].value;
		}
		printf("ACPI_DEBUG: set '%s'\n", tag[i].name);
	    }
	}
	cp = ep;
    }
}

static void
acpi_set_debugging(void *junk)
{
    char	*cp;

    AcpiDbgLayer = 0;
    AcpiDbgLevel = 0;
    if ((cp = getenv("debug.acpi.layer")) != NULL)
	acpi_parse_debug(cp, &dbg_layer[0], &AcpiDbgLayer);
    if ((cp = getenv("debug.acpi.level")) != NULL)
	acpi_parse_debug(cp, &dbg_level[0], &AcpiDbgLevel);

    printf("ACPI debug layer 0x%x  debug level 0x%x\n", AcpiDbgLayer, AcpiDbgLevel);
}
SYSINIT(acpi_debugging, SI_SUB_TUNABLES, SI_ORDER_ANY, acpi_set_debugging, NULL);
#endif

/*
 * ACPI Battery Abstruction Layer
 */

struct acpi_batteries {
	TAILQ_ENTRY(acpi_batteries) link;
	struct	 acpi_battdesc battdesc;
};

static TAILQ_HEAD(,acpi_batteries) acpi_batteries;
static int			acpi_batteries_initted = 0;
static int			acpi_batteries_units = 0;
static struct acpi_battinfo	acpi_battery_battinfo;

static int
acpi_battery_get_units(void)
{

	return (acpi_batteries_units);
}

static int
acpi_battery_get_battdesc(int logical_unit, struct acpi_battdesc *battdesc)
{
	int	 i;
	struct acpi_batteries	*bp;

	if (logical_unit < 0 || logical_unit >= acpi_batteries_units) {
		return (ENXIO);
	}

	i = 0;
	TAILQ_FOREACH(bp, &acpi_batteries, link) {
		if (logical_unit == i) {
			battdesc->type = bp->battdesc.type;
			battdesc->phys_unit = bp->battdesc.phys_unit;
			return (0);
		}
		i++;
	}

	return (ENXIO);
}

static int
acpi_battery_get_battinfo(int unit, struct acpi_battinfo *battinfo)
{
	int	 error;
	struct	 acpi_battdesc battdesc;

	error = 0;
	if (unit == -1) {
		error = acpi_cmbat_get_battinfo(-1, battinfo);
		goto out;
	} else {
		if ((error = acpi_battery_get_battdesc(unit, &battdesc)) != 0) {
			goto out;
		}
		switch (battdesc.type) {
		case ACPI_BATT_TYPE_CMBAT:
			error = acpi_cmbat_get_battinfo(battdesc.phys_unit,
			   battinfo);
			break;
		default:
			error = ENXIO;
			break;
		}
	}
out:
	return (error);
}

static int
acpi_battery_ioctl(u_long cmd, caddr_t addr, void *arg)
{
	int	 error;
	int	 logical_unit;
	union acpi_battery_ioctl_arg	*ioctl_arg;

	ioctl_arg = (union acpi_battery_ioctl_arg *)addr;
	error = 0;
	switch (cmd) {
	case ACPIIO_BATT_GET_UNITS:
		*(int *)addr = acpi_battery_get_units();
		break;

	case ACPIIO_BATT_GET_BATTDESC:
		logical_unit = ioctl_arg->unit;
		error = acpi_battery_get_battdesc(logical_unit, &ioctl_arg->battdesc);
		break;

	case ACPIIO_BATT_GET_BATTINFO:
		logical_unit = ioctl_arg->unit;
		error = acpi_battery_get_battinfo(logical_unit,
		    &ioctl_arg->battinfo);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static int
acpi_battery_sysctl(SYSCTL_HANDLER_ARGS)
{
	int	val;
	int	error;

	acpi_battery_get_battinfo(-1, &acpi_battery_battinfo);
	val = *(u_int *)oidp->oid_arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	return (error);
}

static int
acpi_battery_init(void)
{
	device_t		 dev;
	struct acpi_softc	*sc;
	int	 		 error;

	if ((dev = devclass_get_device(acpi_devclass, 0)) == NULL) {
		return (ENXIO);
	}
	if ((sc = device_get_softc(dev)) == NULL) {
		return (ENXIO);
	}

	error = 0;

	TAILQ_INIT(&acpi_batteries);
	acpi_batteries_initted = 1;

	if ((error = acpi_register_ioctl(ACPIIO_BATT_GET_UNITS,
			acpi_battery_ioctl, NULL)) != 0) {
		return (error);
	}
	if ((error = acpi_register_ioctl(ACPIIO_BATT_GET_BATTDESC,
			acpi_battery_ioctl, NULL)) != 0) {
		return (error);
	}
	if ((error = acpi_register_ioctl(ACPIIO_BATT_GET_BATTINFO,
			acpi_battery_ioctl, NULL)) != 0) {
		return (error);
	}

	sysctl_ctx_init(&sc->acpi_battery_sysctl_ctx);
	sc->acpi_battery_sysctl_tree = SYSCTL_ADD_NODE(&sc->acpi_battery_sysctl_ctx,
				SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
				OID_AUTO, "battery", CTLFLAG_RD, 0, "");
	SYSCTL_ADD_PROC(&sc->acpi_battery_sysctl_ctx,
		SYSCTL_CHILDREN(sc->acpi_battery_sysctl_tree),
		OID_AUTO, "life", CTLTYPE_INT | CTLFLAG_RD,
		&acpi_battery_battinfo.cap, 0, acpi_battery_sysctl, "I", "");
	SYSCTL_ADD_PROC(&sc->acpi_battery_sysctl_ctx,
		SYSCTL_CHILDREN(sc->acpi_battery_sysctl_tree),
		OID_AUTO, "time", CTLTYPE_INT | CTLFLAG_RD,
		&acpi_battery_battinfo.min, 0, acpi_battery_sysctl, "I", "");
	SYSCTL_ADD_PROC(&sc->acpi_battery_sysctl_ctx,
		SYSCTL_CHILDREN(sc->acpi_battery_sysctl_tree),
		OID_AUTO, "state", CTLTYPE_INT | CTLFLAG_RD,
		&acpi_battery_battinfo.state, 0, acpi_battery_sysctl, "I", "");
	SYSCTL_ADD_INT(&sc->acpi_battery_sysctl_ctx,
		SYSCTL_CHILDREN(sc->acpi_battery_sysctl_tree),
		OID_AUTO, "units", CTLFLAG_RD, &acpi_batteries_units, 0, "");

	return (error);
}

int
acpi_battery_register(int type, int phys_unit)
{
	int	error;
	struct acpi_batteries	*bp;

	error = 0;
	if ((bp = malloc(sizeof(*bp), M_ACPIDEV, M_NOWAIT)) == NULL) {
		return(ENOMEM);
	}

	bp->battdesc.type = type;
	bp->battdesc.phys_unit = phys_unit;
	if (acpi_batteries_initted == 0) {
		if ((error = acpi_battery_init()) != 0) {
			free(bp, M_ACPIDEV);
			return(error);
		}
	}
		
	TAILQ_INSERT_TAIL(&acpi_batteries, bp, link);
	acpi_batteries_units++;

	return(0);
}

