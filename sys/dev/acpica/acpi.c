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
#include <sys/malloc.h>
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
    0,
    -1
};

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

#ifdef ACPI_DEBUG
static void	acpi_set_debugging(void);
#endif

static void	acpi_system_eventhandler_sleep(void *arg, int state);
static void	acpi_system_eventhandler_wakeup(void *arg, int state);

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

    if(!cold){
	    printf("Don't load this driver from userland!!\n");
	    return ;
    }

    /*
     * Make sure we're not being doubly invoked.
     */
    if (device_find_child(parent, "acpi", 0) != NULL)
	return;
    
#ifdef ACPI_DEBUG
    acpi_set_debugging();
#endif

    /*
     * Start up ACPICA
     */
#ifdef ENABLE_DEBUGGER
    if (debugpoint && !strcmp(debugpoint, "init"))
	acpi_EnterDebugger();
#endif
    if ((error = AcpiInitializeSubsystem()) != AE_OK) {
	printf("ACPI: initialisation failed: %s\n", acpi_strerror(error));
	return;
    }
#ifdef ENABLE_DEBUGGER
    if (debugpoint && !strcmp(debugpoint, "tables"))
	acpi_EnterDebugger();
#endif
    if (((error = AcpiFindRootPointer(&rsdp)) != AE_OK) ||
	((error = AcpiLoadTables(rsdp)) != AE_OK)) {
	printf("ACPI: table load failed: %s\n", acpi_strerror(error));
	return;
    }
    
    /*
     * Attach the actual ACPI device.
     */
    if ((child = BUS_ADD_CHILD(parent, 0, "acpi", 0)) == NULL) {
	    device_printf(parent, "ACPI: could not attach\n");
	    return;
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

    if ((error = AcpiGetTableHeader(ACPI_TABLE_XSDT, 1, &th)) != AE_OK) {
	device_printf(dev, "couldn't get XSDT header: %s\n", acpi_strerror(error));
	return(ENXIO);
    }
    sprintf(buf, "%.6s %.8s", th.OemId, th.OemTableId);
    device_set_desc_copy(dev, buf);

    return(0);
}

static int
acpi_attach(device_t dev)
{
    struct acpi_softc	*sc;
    int			error;
#ifdef ENABLE_DEBUGGER
    char		*debugpoint = getenv("debug.acpi.debugger");
#endif


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
						ADDRESS_SPACE_SYSTEM_MEMORY,
						ACPI_DEFAULT_HANDLER,
						NULL, NULL)) != AE_OK) {
	device_printf(dev, "could not initialise SystemMemory handler: %s\n", acpi_strerror(error));
	return(ENXIO);
    }
    if ((error = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
						ADDRESS_SPACE_SYSTEM_IO,
						ACPI_DEFAULT_HANDLER,
						NULL, NULL)) != AE_OK) {
	device_printf(dev, "could not initialise SystemIO handler: %s\n", acpi_strerror(error));
	return(ENXIO);
    }
    if ((error = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
						ADDRESS_SPACE_PCI_CONFIG,
						ACPI_DEFAULT_HANDLER,
						NULL, NULL)) != AE_OK) {
	device_printf(dev, "could not initialise PciConfig handler: %s\n", acpi_strerror(error));
	return(ENXIO);
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
	return(ENXIO);
    }

    /*
     * Dispatch the default sleep state to devices.
     * TBD: should be configured from userland policy manager.
     */
    sc->acpi_power_button_sx = ACPI_POWER_BUTTON_DEFAULT_SX;
    sc->acpi_sleep_button_sx = ACPI_SLEEP_BUTTON_DEFAULT_SX;
    sc->acpi_lid_switch_sx = ACPI_LID_SWITCH_DEFAULT_SX;

    /* Enable and clear fixed events and install handlers. */
    if (AcpiGbl_FADT != NULL && AcpiGbl_FADT->PwrButton == 0) {
	AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, ACPI_EVENT_FIXED);
	AcpiClearEvent(ACPI_EVENT_POWER_BUTTON, ACPI_EVENT_FIXED);
	AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, acpi_eventhandler_power_button_for_sleep, sc);
	device_printf(dev, "power button is handled as a fixed feature programming model.\n");
    }
    if (AcpiGbl_FADT != NULL && AcpiGbl_FADT->SleepButton == 0) {
	AcpiEnableEvent(ACPI_EVENT_SLEEP_BUTTON, ACPI_EVENT_FIXED);
	AcpiClearEvent(ACPI_EVENT_SLEEP_BUTTON, ACPI_EVENT_FIXED);
	AcpiInstallFixedEventHandler(ACPI_EVENT_SLEEP_BUTTON, acpi_eventhandler_sleep_button_for_sleep, sc);
	device_printf(dev, "sleep button is handled as a fixed feature programming model.\n");
    }

    /*
     * Scan the namespace and attach/initialise children.
     */
#ifdef ENABLE_DEBUGGER
    if (debugpoint && !strcmp(debugpoint, "probe"))
	acpi_EnterDebugger();
#endif
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
    return(0);
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

    /*
     * Create any static children by calling device identify methods.
     */
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
    for (i = 0; scopes[i] != NULL; i++)
	if ((AcpiGetHandle(ACPI_ROOT_OBJECT, scopes[i], &parent)) == AE_OK)
	    AcpiWalkNamespace(ACPI_TYPE_ANY, parent, 100, acpi_probe_child, bus, NULL);

    /*
     * Scan all of the child devices we have created and let them probe/attach.
     */
    bus_generic_attach(bus);

    /*
     * Some of these children may have attached others as part of their attach
     * process (eg. the root PCI bus driver), so rescan.
     */
    bus_generic_attach(bus);
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

    if (AcpiGetType(handle, &type) == AE_OK) {
	switch(type) {
	case ACPI_TYPE_DEVICE:
	case ACPI_TYPE_PROCESSOR:
	case ACPI_TYPE_THERMAL:
	case ACPI_TYPE_POWER:
	    /* 
	     * Create a placeholder device for this node.  Sort the placeholder
	     * so that the probe/attach passes will run breadth-first.
	     */
	    child = BUS_ADD_CHILD(bus, level * 10, NULL, -1);
	    acpi_set_handle(child, handle);
	}
    }
    return(AE_OK);
}

static void
acpi_shutdown_pre_sync(void *arg, int howto)
{
    /*
     * disable all of ACPI events before soft off, otherwise the system
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

    if (howto == RB_POWEROFF) {
	printf("Power system off using ACPI...\n");
	if ((status = AcpiSetSystemSleepState(ACPI_STATE_S5)) != AE_OK) {
	    printf("ACPI power-off failed - %s\n", acpi_strerror(status));
	} else {
	    DELAY(1000000);
	    printf("ACPI power-off failed - timeout\n");
	}
    }
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
    
    if ((hid == NULL) || (strlen(hid) != 7))
	return(FALSE);
    if ((h = acpi_get_handle(dev)) == NULL)
	return(FALSE);
    if ((error = AcpiGetObjectInfo(h, &devinfo)) != AE_OK)
	return(FALSE);
    if ((devinfo.Valid & ACPI_VALID_HID) && !strncmp(hid, devinfo.HardwareId, 7))
	return(TRUE);
    return(FALSE);
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
 * Set the system sleep state
 *
 * Currently we only support S1 and S5
 */
ACPI_STATUS
acpi_SetSleepState(struct acpi_softc *sc, int state)
{
    ACPI_STATUS	status = AE_OK;

    switch (state) {
    case ACPI_STATE_S0:	/* XXX only for testing */
	status = AcpiSetSystemSleepState((UINT8)state);
	if (status != AE_OK) {
	    device_printf(sc->acpi_dev, "AcpiSetSystemSleepState failed - %s\n", acpi_strerror(status));
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
	    return(AE_ERROR);
	}
	sc->acpi_sstate = state;
	status = AcpiSetSystemSleepState((UINT8)state);
	if (status != AE_OK) {
	    device_printf(sc->acpi_dev, "AcpiSetSystemSleepState failed - %s\n", acpi_strerror(status));
	}
	DEVICE_RESUME(root_bus);
	sc->acpi_sstate = ACPI_STATE_S0;
	break;

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
    return(status);
}

/*
 * Enable/Disable ACPI
 */
ACPI_STATUS
acpi_Enable(struct acpi_softc *sc)
{
    ACPI_STATUS	status;
    u_int32_t	flags;

    flags = ACPI_NO_ADDRESS_SPACE_INIT | ACPI_NO_HARDWARE_INIT |
            ACPI_NO_DEVICE_INIT | ACPI_NO_OBJECT_INIT;
    if (!sc->acpi_enabled) {
	status = AcpiEnableSubsystem(flags);
    } else {
	status = AE_OK;
    }
    if (status == AE_OK)
	sc->acpi_enabled = 1;
    return(status);
}

ACPI_STATUS
acpi_Disable(struct acpi_softc *sc)
{
    ACPI_STATUS	status;

    if (sc->acpi_enabled) {
	status = AcpiDisable();
    } else {
	status = AE_OK;
    }
    if (status == AE_OK)
	sc->acpi_enabled = 0;
    return(status);
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
 * Evaluate a path that should return a number
 */
ACPI_STATUS
acpi_EvaluateNumber(ACPI_HANDLE handle, char *path, int *number)
{
    ACPI_STATUS	error;
    ACPI_BUFFER	buf;
    int		param[4];

    if (handle == NULL)
	handle = ACPI_ROOT_OBJECT;
    buf.Pointer = &param[0];
    buf.Length = sizeof(param);
    if ((error = AcpiEvaluateObject(handle, path, NULL, &buf)) == AE_OK) {
	if (param[0] == ACPI_TYPE_NUMBER) {
	    *number = param[1];
	} else {
	    error = AE_TYPE;
	}
    }
    return(error);
}

/*
 * ACPI Event Handlers
 */

/* System Event Handlers (registered by EVENTHANDLER_REGISTER) */

static void
acpi_system_eventhandler_sleep(void *arg, int state)
{
    if (state < ACPI_STATE_S0 || state > ACPI_STATE_S5) {
	return;
    }

    acpi_SetSleepState((struct acpi_softc *)arg, state);
}


static void
acpi_system_eventhandler_wakeup(void *arg, int state)
{
    /* Well, what to do? :-) */
}

/* 
 * ACPICA Event Handlers (FixedEvent, also called from button notify handler)
 */
UINT32
acpi_eventhandler_power_button_for_sleep(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    EVENTHANDLER_INVOKE(acpi_sleep_event, sc->acpi_power_button_sx);
    return(INTERRUPT_HANDLED);
}

UINT32
acpi_eventhandler_power_button_for_wakeup(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    EVENTHANDLER_INVOKE(acpi_wakeup_event, sc->acpi_power_button_sx);
    return(INTERRUPT_HANDLED);
}

UINT32
acpi_eventhandler_sleep_button_for_sleep(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    EVENTHANDLER_INVOKE(acpi_sleep_event, sc->acpi_sleep_button_sx);
    return(INTERRUPT_HANDLED);
}

UINT32
acpi_eventhandler_sleep_button_for_wakeup(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    EVENTHANDLER_INVOKE(acpi_wakeup_event, sc->acpi_sleep_button_sx);
    return(INTERRUPT_HANDLED);
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
	    printf("avoiding '%s'\n", np);
	    return(1);
	}
	cp += len;
    }
    return(0);
}

/*
 * Control interface.
 *
 * XXX this is provided as a temporary measure for
 *     backwards compatibility for now.  A better
 *     interface will probably use sysctl or similar.
 */
static int
acpiopen(dev_t dev, int flag, int fmt, struct proc * p)
{
    return(0);
}

static int
acpiclose(dev_t dev, int flag, int fmt, struct proc * p)
{
    return(0);
}

static int
acpiioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc * p)
{
    int		error, state;
    struct acpi_softc	*sc;

    error = state = 0;
    sc = dev->si_drv1;

    switch (cmd) {
    case ACPIIO_ENABLE:
	if (ACPI_FAILURE(acpi_Enable(sc))) {
	    error = ENXIO;
	}
	break;

    case ACPIIO_DISABLE:
	if (ACPI_FAILURE(acpi_Disable(sc))) {
	    error = ENXIO;
	}
	break;

    case ACPIIO_SETSLPSTATE:
	if (!sc->acpi_enabled) {
	    error = ENXIO;
	    break;
	}
	state = *(int *)addr;
	if (state >= ACPI_STATE_S0  && state <= ACPI_STATE_S5) {
	    acpi_SetSleepState(sc, state);
	} else {
	    error = EINVAL;
	}

	break;

    default:
	error = EINVAL;
	break;
    }

    return(error);
}

#ifdef ACPI_DEBUG
struct debugtag
{
    char	*name;
    UINT32	value;
};

static struct debugtag	dbg_layer[] = {
    {"GLOBAL",			0x00000001},
    {"COMMON",			0x00000002},
    {"PARSER",			0x00000004},
    {"DISPATCHER",		0x00000008},
    {"INTERPRETER",		0x00000010},
    {"NAMESPACE",		0x00000020},
    {"RESOURCE_MANAGER",	0x00000040},
    {"TABLE_MANAGER",		0x00000080},
    {"EVENT_HANDLING",		0x00000100},
    {"HARDWARE",		0x00000200},
    {"MISCELLANEOUS",		0x00000400},
    {"OS_DEPENDENT",		0x00000800},
    {"BUS_MANAGER",		0x00001000},
    {"PROCESSOR_CONTROL",	0x00002000},
    {"SYSTEM_CONTROL",		0x00004000},
    {"THERMAL_CONTROL",		0x00008000},
    {"POWER_CONTROL",		0x00010000},
    {"EMBEDDED_CONTROLLER",	0x00020000},
    {"BATTERY",			0x00040000},
    {"DEBUGGER",		0x00100000},
    {"ALL_COMPONENTS",		0x001FFFFF},
    {NULL, 0}
};

static struct debugtag dbg_level[] = {
    {"ACPI_OK",                     0x00000001},    
    {"ACPI_INFO",                   0x00000002},    
    {"ACPI_WARN",                   0x00000004},    
    {"ACPI_ERROR",                  0x00000008},    
    {"ACPI_FATAL",                  0x00000010},    
    {"ACPI_DEBUG_OBJECT",           0x00000020},    
    {"ACPI_ALL",                    0x0000003F},    
    {"TRACE_PARSE",                 0x00000100},    
    {"TRACE_DISPATCH",              0x00000200},    
    {"TRACE_LOAD",                  0x00000400},    
    {"TRACE_EXEC",                  0x00000800},    
    {"TRACE_NAMES",                 0x00001000},    
    {"TRACE_OPREGION",              0x00002000},    
    {"TRACE_BFIELD",                0x00004000},    
    {"TRACE_TRASH",                 0x00008000},    
    {"TRACE_TABLES",                0x00010000},    
    {"TRACE_FUNCTIONS",             0x00020000},    
    {"TRACE_VALUES",                0x00040000},    
    {"TRACE_OBJECTS",               0x00080000},    
    {"TRACE_ALLOCATIONS",           0x00100000},    
    {"TRACE_RESOURCES",             0x00200000},    
    {"TRACE_IO",                    0x00400000},    
    {"TRACE_INTERRUPTS",            0x00800000},    
    {"TRACE_USER_REQUESTS",         0x01000000},    
    {"TRACE_PACKAGE",               0x02000000},    
    {"TRACE_MUTEX",                 0x04000000},    
    {"TRACE_ALL",                   0x0FFFFF00},    
    {"VERBOSE_AML_DISASSEMBLE",     0x10000000},    
    {"VERBOSE_INFO",                0x20000000},    
    {"VERBOSE_TABLES",              0x40000000},    
    {"VERBOSE_EVENTS",              0x80000000},    
    {"VERBOSE_ALL",                 0xF0000000},    
    {NULL, 0}
};    

static void
acpi_parse_debug(char *cp, struct debugtag *tag, UINT32 *flag)
{
    char	*ep;
    int		i, l;

    while (*cp) {
	if (isspace(*cp)) {
	    cp++;
	    continue;
	}
	ep = cp;
	while (*ep && !isspace(*ep))
	    ep++;
	l = ep - cp;
	for (i = 0; tag[i].name != NULL; i++) {
	    if (!strncmp(cp, tag[i].name, l)) {
		*flag |= tag[i].value;
		printf("ACPI_DEBUG: set '%s'\n", tag[i].name);
	    }
	}
	cp = ep;
    }
}

static void
acpi_set_debugging(void)
{
    char	*cp;
    
    if ((cp = getenv("debug.acpi.layer")) != NULL)
	acpi_parse_debug(cp, &dbg_layer[0], &AcpiDbgLayer);
    if ((cp = getenv("debug.acpi.level")) != NULL)
	acpi_parse_debug(cp, &dbg_level[0], &AcpiDbgLevel);
}
#endif
