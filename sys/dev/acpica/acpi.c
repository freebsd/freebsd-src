/*-
 * Copyright (c) 2000 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2000, 2001 Michael Smith
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
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/ctype.h>
#include <sys/linker.h>
#include <sys/power.h>

#include <machine/clock.h>
#include <machine/resource.h>
#include <isa/isavar.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>
#include <contrib/dev/acpica/acnamesp.h>

MALLOC_DEFINE(M_ACPIDEV, "acpidev", "ACPI devices");

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("ACPI")

static d_open_t		acpiopen;
static d_close_t	acpiclose;
static d_ioctl_t	acpiioctl;

#define CDEV_MAJOR 152
static struct cdevsw acpi_cdevsw = {
	.d_open =	acpiopen,
	.d_close =	acpiclose,
	.d_ioctl =	acpiioctl,
	.d_name =	"acpi",
	.d_maj =	CDEV_MAJOR,
};

static const char* sleep_state_names[] = {
    "S0", "S1", "S2", "S3", "S4", "S5", "NONE"};

/* this has to be static, as the softc is gone when we need it */
static int acpi_off_state = ACPI_STATE_S5;

#if __FreeBSD_version >= 500000
struct mtx	acpi_mutex;
#endif

static int	acpi_modevent(struct module *mod, int event, void *junk);
static void	acpi_identify(driver_t *driver, device_t parent);
static int	acpi_probe(device_t dev);
static int	acpi_attach(device_t dev);
static device_t	acpi_add_child(device_t bus, int order, const char *name,
			int unit);
static int	acpi_print_child(device_t bus, device_t child);
static int	acpi_read_ivar(device_t dev, device_t child, int index,
			uintptr_t *result);
static int	acpi_write_ivar(device_t dev, device_t child, int index,
			uintptr_t value);
static int	acpi_set_resource(device_t dev, device_t child, int type,
			int rid, u_long start, u_long count);
static int	acpi_get_resource(device_t dev, device_t child, int type,
			int rid, u_long *startp, u_long *countp);
static struct resource *acpi_alloc_resource(device_t bus, device_t child,
			int type, int *rid, u_long start, u_long end,
			u_long count, u_int flags);
static int	acpi_release_resource(device_t bus, device_t child, int type,
			int rid, struct resource *r);
static u_int32_t acpi_isa_get_logicalid(device_t dev);
static u_int32_t acpi_isa_get_compatid(device_t dev);
static int	acpi_isa_pnp_probe(device_t bus, device_t child,
			struct isa_pnp_id *ids);
static void	acpi_probe_children(device_t bus);
static ACPI_STATUS acpi_probe_child(ACPI_HANDLE handle, UINT32 level,
			void *context, void **status);
static void	acpi_shutdown_pre_sync(void *arg, int howto);
static void	acpi_shutdown_final(void *arg, int howto);
static void	acpi_enable_fixed_events(struct acpi_softc *sc);
static void	acpi_system_eventhandler_sleep(void *arg, int state);
static void	acpi_system_eventhandler_wakeup(void *arg, int state);
static int	acpi_supported_sleep_state_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_sleep_state_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_pm_func(u_long cmd, void *arg, ...);

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

    /* ISA emulation */
    DEVMETHOD(isa_pnp_probe,		acpi_isa_pnp_probe),

    {0, 0}
};

static driver_t acpi_driver = {
    "acpi",
    acpi_methods,
    sizeof(struct acpi_softc),
};

static devclass_t acpi_devclass;
DRIVER_MODULE(acpi, nexus, acpi_driver, acpi_devclass, acpi_modevent, 0);
MODULE_VERSION(acpi, 100);

SYSCTL_NODE(_debug, OID_AUTO, acpi, CTLFLAG_RW, NULL, "ACPI debugging");
static char acpi_ca_version[12];
SYSCTL_STRING(_debug_acpi, OID_AUTO, acpi_ca_version, CTLFLAG_RD,
	      acpi_ca_version, 0, "Version of Intel ACPI-CA");

/*
 * ACPI can only be loaded as a module by the loader; activating it after
 * system bootstrap time is not useful, and can be fatal to the system.
 * It also cannot be unloaded, since the entire system bus heirarchy hangs
 * off it.
 */
static int
acpi_modevent(struct module *mod, int event, void *junk)
{
    switch(event) {
    case MOD_LOAD:
	if (!cold) {
	    printf("The ACPI driver cannot be loaded after boot.\n");
	    return (EPERM);
	}
	break;
    case MOD_UNLOAD:
	if (!cold && power_pm_get_type() == POWER_PM_TYPE_ACPI)
	    return (EBUSY);
	break;
    default:
	break;
    }
    return (0);
}

/*
 * Detect ACPI, perform early initialisation
 */
static void
acpi_identify(driver_t *driver, device_t parent)
{
    device_t	child;
    int		error;
#ifdef ACPI_DEBUGGER
    char	*debugpoint;
#endif

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (!cold)
	return_VOID;

    /* Check that we haven't been disabled with a hint. */
    if (resource_disabled("acpi", 0))
	return_VOID;

    snprintf(acpi_ca_version, sizeof(acpi_ca_version), "0x%x",
	     ACPI_CA_VERSION);

    /* Make sure we're not being doubly invoked. */
    if (device_find_child(parent, "acpi", 0) != NULL)
	return_VOID;

#if __FreeBSD_version >= 500000
    /* Initialise the ACPI mutex */
    mtx_init(&acpi_mutex, "ACPI global lock", NULL, MTX_DEF);
#endif

    /* Start up the ACPI CA subsystem. */
#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "init"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif
    if (ACPI_FAILURE(error = AcpiInitializeSubsystem())) {
	printf("ACPI: initialisation failed: %s\n", AcpiFormatException(error));
	return_VOID;
    }
#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "tables"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif

    if (ACPI_FAILURE(error = AcpiLoadTables())) {
	printf("ACPI: table load failed: %s\n", AcpiFormatException(error));
	return_VOID;
    }
    
    /* Attach the actual ACPI device. */
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
    ACPI_STATUS		status;
    int			error;
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (power_pm_get_type() != POWER_PM_TYPE_NONE &&
	power_pm_get_type() != POWER_PM_TYPE_ACPI) {

	device_printf(dev, "Other PM system enabled.\n");
	return_VALUE(ENXIO);
    }

    ACPI_LOCK;

    if (ACPI_FAILURE(status = AcpiGetTableHeader(ACPI_TABLE_XSDT, 1, &th))) {
	device_printf(dev, "couldn't get XSDT header: %s\n",
		      AcpiFormatException(status));
	error = ENXIO;
    } else {
	sprintf(buf, "%.6s %.8s", th.OemId, th.OemTableId);
	device_set_desc_copy(dev, buf);
	error = 0;
    }
    ACPI_UNLOCK;
    return_VALUE(error);
}

static int
acpi_attach(device_t dev)
{
    struct acpi_softc	*sc;
    ACPI_STATUS		status;
    int			error;
    UINT32		flags;
    char		*env;
#ifdef ACPI_DEBUGGER
    char		*debugpoint;
#endif
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    ACPI_LOCK;
    sc = device_get_softc(dev);
    bzero(sc, sizeof(*sc));
    sc->acpi_dev = dev;

#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "spaces"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif

    /* Install the default address space handlers. */
    error = ENXIO;
    status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
		ACPI_ADR_SPACE_SYSTEM_MEMORY, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if (ACPI_FAILURE(status)) {
	device_printf(dev, "Could not initialise SystemMemory handler: %s\n",
		      AcpiFormatException(status));
	goto out;
    }
    status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
		ACPI_ADR_SPACE_SYSTEM_IO, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if (ACPI_FAILURE(status)) {
	device_printf(dev, "Could not initialise SystemIO handler: %s\n",
		      AcpiFormatException(status));
	goto out;
    }
    status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
		ACPI_ADR_SPACE_PCI_CONFIG, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if (ACPI_FAILURE(status)) {
	device_printf(dev, "could not initialise PciConfig handler: %s\n",
		      AcpiFormatException(status));
	goto out;
    }

    /*
     * Bring ACPI fully online.
     *
     * Note that some systems (specifically, those with namespace evaluation
     * issues that require the avoidance of parts of the namespace) must
     * avoid running _INI and _STA on everything, as well as dodging the final
     * object init pass.
     *
     * For these devices, we set ACPI_NO_DEVICE_INIT and ACPI_NO_OBJECT_INIT).
     *
     * XXX We should arrange for the object init pass after we have attached
     *     all our child devices, but on many systems it works here.
     */
#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "enable"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif
    flags = 0;
    if (testenv("debug.acpi.avoid"))
	flags = ACPI_NO_DEVICE_INIT | ACPI_NO_OBJECT_INIT;
    if (ACPI_FAILURE(status = AcpiEnableSubsystem(flags))) {
	device_printf(dev, "Could not enable ACPI: %s\n",
		      AcpiFormatException(status));
	goto out;
    }

    /*
     * Call the ECDT probe function to provide EC functionality before
     * the namespace has been evaluated.
     */
    acpi_ec_ecdt_probe(dev);

    if (ACPI_FAILURE(status = AcpiInitializeObjects(flags))) {
	device_printf(dev, "Could not initialize ACPI objects: %s\n",
		      AcpiFormatException(status));
	goto out;
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
	OID_AUTO, "supported_sleep_state", CTLTYPE_STRING | CTLFLAG_RD,
	0, 0, acpi_supported_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "power_button_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_power_button_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "sleep_button_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_sleep_button_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "lid_switch_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_lid_switch_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "standby_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_standby_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "suspend_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_suspend_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_INT(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "sleep_delay", CTLFLAG_RD | CTLFLAG_RW,
	&sc->acpi_sleep_delay, 0, "sleep delay");
    SYSCTL_ADD_INT(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "s4bios", CTLFLAG_RD | CTLFLAG_RW,
	&sc->acpi_s4bios, 0, "S4BIOS mode");
    SYSCTL_ADD_INT(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "verbose", CTLFLAG_RD | CTLFLAG_RW,
	&sc->acpi_verbose, 0, "verbose mode");
    SYSCTL_ADD_INT(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "disable_on_poweroff", CTLFLAG_RD | CTLFLAG_RW,
	&sc->acpi_disable_on_poweroff, 0, "ACPI subsystem disable on poweroff");

    /*
     * Default to 5 seconds before sleeping to give some machines time to
     * stabilize.
     */
    sc->acpi_sleep_delay = 5;
    sc->acpi_disable_on_poweroff = 1;
    if (bootverbose)
	sc->acpi_verbose = 1;
    if ((env = getenv("hw.acpi.verbose")) && strcmp(env, "0")) {
	sc->acpi_verbose = 1;
	freeenv(env);
    }

    /* Only enable S4BIOS by default if the FACS says it is available. */
    if (AcpiGbl_FACS->S4Bios_f != 0)
	    sc->acpi_s4bios = 1;

    /*
     * Dispatch the default sleep state to devices.
     * TBD: should be configured from userland policy manager.
     */
    sc->acpi_power_button_sx = ACPI_POWER_BUTTON_DEFAULT_SX;
    sc->acpi_sleep_button_sx = ACPI_SLEEP_BUTTON_DEFAULT_SX;
    sc->acpi_lid_switch_sx = ACPI_LID_SWITCH_DEFAULT_SX;
    sc->acpi_standby_sx = ACPI_STATE_S1;
    sc->acpi_suspend_sx = ACPI_STATE_S3;

    acpi_enable_fixed_events(sc);

    /*
     * Scan the namespace and attach/initialise children.
     */
#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "probe"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif

    /* Register our shutdown handlers */
    EVENTHANDLER_REGISTER(shutdown_pre_sync, acpi_shutdown_pre_sync, sc,
	SHUTDOWN_PRI_LAST);
    EVENTHANDLER_REGISTER(shutdown_final, acpi_shutdown_final, sc,
	SHUTDOWN_PRI_LAST);

    /*
     * Register our acpi event handlers.
     * XXX should be configurable eg. via userland policy manager.
     */
    EVENTHANDLER_REGISTER(acpi_sleep_event, acpi_system_eventhandler_sleep,
	sc, ACPI_EVENT_PRI_LAST);
    EVENTHANDLER_REGISTER(acpi_wakeup_event, acpi_system_eventhandler_wakeup,
	sc, ACPI_EVENT_PRI_LAST);

    /* Flag our initial states. */
    sc->acpi_enabled = 1;
    sc->acpi_sstate = ACPI_STATE_S0;
    sc->acpi_sleep_disabled = 0;

    /* Create the control device */
    sc->acpi_dev_t = make_dev(&acpi_cdevsw, 0, UID_ROOT, GID_WHEEL, 0644,
			      "acpi");
    sc->acpi_dev_t->si_drv1 = sc;

#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (strcmp(debugpoint, "running") == 0)
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif

#ifdef ACPI_USE_THREADS
    if ((error = acpi_task_thread_init()))
	goto out;
#endif

    if ((error = acpi_machdep_init(dev)))
	goto out;

    /* Register ACPI again to pass the correct argument of pm_func. */
    power_pm_register(POWER_PM_TYPE_ACPI, acpi_pm_func, sc);

    if (!acpi_disabled("bus"))
	acpi_probe_children(dev);

    error = 0;

 out:
    ACPI_UNLOCK;
    return_VALUE (error);
}

/*
 * Handle a new device being added
 */
static device_t
acpi_add_child(device_t bus, int order, const char *name, int unit)
{
    struct acpi_device	*ad;
    device_t		child;

    if ((ad = malloc(sizeof(*ad), M_ACPIDEV, M_NOWAIT | M_ZERO)) == NULL)
	return (NULL);

    resource_list_init(&ad->ad_rl);
    
    child = device_add_child_ordered(bus, order, name, unit);
    if (child != NULL)
	device_set_ivars(child, ad);
    return (child);
}

static int
acpi_print_child(device_t bus, device_t child)
{
    struct acpi_device	 *adev = device_get_ivars(child);
    struct resource_list *rl = &adev->ad_rl;
    int retval = 0;

    retval += bus_print_child_header(bus, child);
    retval += resource_list_print_type(rl, "port",  SYS_RES_IOPORT, "%#lx");
    retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#lx");
    retval += resource_list_print_type(rl, "irq",   SYS_RES_IRQ,    "%ld");
    retval += resource_list_print_type(rl, "drq",   SYS_RES_DRQ,    "%ld");
    retval += bus_print_child_footer(bus, child);

    return (retval);
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
	return (ENOENT);
    }

    /* ACPI and ISA compatibility ivars */
    switch(index) {
    case ACPI_IVAR_HANDLE:
	*(ACPI_HANDLE *)result = ad->ad_handle;
	break;
    case ACPI_IVAR_MAGIC:
	*(int *)result = ad->ad_magic;
	break;
    case ACPI_IVAR_PRIVATE:
	*(void **)result = ad->ad_private;
	break;
    case ISA_IVAR_VENDORID:
    case ISA_IVAR_SERIAL:
    case ISA_IVAR_COMPATID:
	*(int *)result = -1;
	break;
    case ISA_IVAR_LOGICALID:
	*(int *)result = acpi_isa_get_logicalid(child);
	break;
    default:
	return (ENOENT);
    }

    return (0);
}

static int
acpi_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
    struct acpi_device	*ad;

    if ((ad = device_get_ivars(child)) == NULL) {
	printf("device has no ivars\n");
	return (ENOENT);
    }

    switch(index) {
    case ACPI_IVAR_HANDLE:
	ad->ad_handle = (ACPI_HANDLE)value;
	break;
    case ACPI_IVAR_MAGIC:
	ad->ad_magic = (int)value;
	break;
    case ACPI_IVAR_PRIVATE:
	ad->ad_private = (void *)value;
	break;
    default:
	panic("bad ivar write request (%d)", index);
	return (ENOENT);
    }

    return (0);
}

ACPI_HANDLE
acpi_get_handle(device_t dev)
{
    uintptr_t up;
    ACPI_HANDLE	h;

    if (BUS_READ_IVAR(device_get_parent(dev), dev, ACPI_IVAR_HANDLE, &up))
	return(NULL);
    h = (ACPI_HANDLE)up;
    return (h);
}
	    
int
acpi_set_handle(device_t dev, ACPI_HANDLE h)
{
    uintptr_t up;

    up = (uintptr_t)h;
    return (BUS_WRITE_IVAR(device_get_parent(dev), dev, ACPI_IVAR_HANDLE, up));
}
	    
int
acpi_get_magic(device_t dev)
{
    uintptr_t up;
    int	m;

    if (BUS_READ_IVAR(device_get_parent(dev), dev, ACPI_IVAR_MAGIC, &up))
	return(0);
    m = (int)up;
    return (m);
}

int
acpi_set_magic(device_t dev, int m)
{
    uintptr_t up;

    up = (uintptr_t)m;
    return (BUS_WRITE_IVAR(device_get_parent(dev), dev, ACPI_IVAR_MAGIC, up));
}

void *
acpi_get_private(device_t dev)
{
    uintptr_t up;
    void *p;

    if (BUS_READ_IVAR(device_get_parent(dev), dev, ACPI_IVAR_PRIVATE, &up))
	return (NULL);
    p = (void *)up;
    return (p);
}

int
acpi_set_private(device_t dev, void *p)
{
    uintptr_t up;

    up = (uintptr_t)p;
    return (BUS_WRITE_IVAR(device_get_parent(dev), dev, ACPI_IVAR_PRIVATE, up));
}

ACPI_OBJECT_TYPE
acpi_get_type(device_t dev)
{
    ACPI_HANDLE		h;
    ACPI_OBJECT_TYPE	t;

    if ((h = acpi_get_handle(dev)) == NULL)
	return (ACPI_TYPE_NOT_FOUND);
    if (AcpiGetType(h, &t) != AE_OK)
	return (ACPI_TYPE_NOT_FOUND);
    return (t);
}

/*
 * Handle child resource allocation/removal
 */
static int
acpi_set_resource(device_t dev, device_t child, int type, int rid,
		  u_long start, u_long count)
{
    struct acpi_device		*ad = device_get_ivars(child);
    struct resource_list	*rl = &ad->ad_rl;

    resource_list_add(rl, type, rid, start, start + count -1, count);

    return(0);
}

static int
acpi_get_resource(device_t dev, device_t child, int type, int rid,
		  u_long *startp, u_long *countp)
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

    return (0);
}

static struct resource *
acpi_alloc_resource(device_t bus, device_t child, int type, int *rid,
		    u_long start, u_long end, u_long count, u_int flags)
{
    struct acpi_device *ad = device_get_ivars(child);
    struct resource_list *rl = &ad->ad_rl;

    return (resource_list_alloc(rl, bus, child, type, rid, start, end, count,
	    flags));
}

static int
acpi_release_resource(device_t bus, device_t child, int type, int rid, struct resource *r)
{
    struct acpi_device *ad = device_get_ivars(child);
    struct resource_list *rl = &ad->ad_rl;

    return(resource_list_release(rl, bus, child, type, rid, r));
}

/*
 * Handle ISA-like devices probing for a PnP ID to match.
 */
#define PNP_EISAID(s)				\
	((((s[0] - '@') & 0x1f) << 2)		\
	 | (((s[1] - '@') & 0x18) >> 3)		\
	 | (((s[1] - '@') & 0x07) << 13)	\
	 | (((s[2] - '@') & 0x1f) << 8)		\
	 | (PNP_HEXTONUM(s[4]) << 16)		\
	 | (PNP_HEXTONUM(s[3]) << 20)		\
	 | (PNP_HEXTONUM(s[6]) << 24)		\
	 | (PNP_HEXTONUM(s[5]) << 28))

static u_int32_t
acpi_isa_get_logicalid(device_t dev)
{
    ACPI_HANDLE		h;
    ACPI_DEVICE_INFO	devinfo;
    ACPI_BUFFER		buf = {sizeof(devinfo), &devinfo};
    ACPI_STATUS		error;
    u_int32_t		pnpid;
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    pnpid = 0;
    ACPI_LOCK;
    
    /* Fetch and validate the HID. */
    if ((h = acpi_get_handle(dev)) == NULL)
	goto out;
    error = AcpiGetObjectInfo(h, &buf);
    if (ACPI_FAILURE(error))
	goto out;
    if ((devinfo.Valid & ACPI_VALID_HID) == 0)
	goto out;

    pnpid = PNP_EISAID(devinfo.HardwareId.Value);

out:
    ACPI_UNLOCK;
    return_VALUE (pnpid);
}

static u_int32_t
acpi_isa_get_compatid(device_t dev)
{
    ACPI_HANDLE		h;
    ACPI_STATUS		error;
    u_int32_t		pnpid;
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    pnpid = 0;
    ACPI_LOCK;
    
    /* Fetch and validate the HID */
    if ((h = acpi_get_handle(dev)) == NULL)
	goto out;
    if (ACPI_FAILURE(error = acpi_EvaluateInteger(h, "_CID", &pnpid)))
	goto out;

out:
    ACPI_UNLOCK;
    return_VALUE (pnpid);
}


static int
acpi_isa_pnp_probe(device_t bus, device_t child, struct isa_pnp_id *ids)
{
    int			result;
    u_int32_t		lid, cid;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /*
     * ISA-style drivers attached to ACPI may persist and
     * probe manually if we return ENOENT.  We never want
     * that to happen, so don't ever return it.
     */
    result = ENXIO;

    /* Scan the supplied IDs for a match */
    lid = acpi_isa_get_logicalid(child);
    cid = acpi_isa_get_compatid(child);
    while (ids && ids->ip_id) {
	if (lid == ids->ip_id || cid == ids->ip_id) {
	    result = 0;
	    goto out;
	}
	ids++;
    }

 out:
    return_VALUE(result);
}

/*
 * Scan relevant portions of the ACPI namespace and attach child devices.
 *
 * Note that we only expect to find devices in the \_PR_, \_TZ_, \_SI_ and
 * \_SB_ scopes, and \_PR_ and \_TZ_ become obsolete in the ACPI 2.0 spec.
 */
static void
acpi_probe_children(device_t bus)
{
    ACPI_HANDLE	parent;
    ACPI_STATUS	status;
    static char	*scopes[] = {"\\_PR_", "\\_TZ_", "\\_SI", "\\_SB_", NULL};
    int		i;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    ACPI_ASSERTLOCK;

    /* Create any static children by calling device identify methods. */
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "device identify routines\n"));
    bus_generic_probe(bus);

    /*
     * Scan the namespace and insert placeholders for all the devices that
     * we find.
     *
     * Note that we use AcpiWalkNamespace rather than AcpiGetDevices because
     * we want to create nodes for all devices, not just those that are
     * currently present. (This assumes that we don't want to create/remove
     * devices as they appear, which might be smarter.)
     */
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "namespace scan\n"));
    for (i = 0; scopes[i] != NULL; i++) {
	status = AcpiGetHandle(ACPI_ROOT_OBJECT, scopes[i], &parent);
	if (ACPI_SUCCESS(status)) {
	    AcpiWalkNamespace(ACPI_TYPE_ANY, parent, 100, acpi_probe_child,
			      bus, NULL);
	}
    }

    /*
     * Scan all of the child devices we have created and let them probe/attach.
     */
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "first bus_generic_attach\n"));
    bus_generic_attach(bus);

    /*
     * Some of these children may have attached others as part of their attach
     * process (eg. the root PCI bus driver), so rescan.
     */
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "second bus_generic_attach\n"));
    bus_generic_attach(bus);

    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "done attaching children\n"));
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

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Skip this device if we think we'll have trouble with it. */
    if (acpi_avoid(handle))
	return_ACPI_STATUS (AE_OK);

    if (ACPI_SUCCESS(AcpiGetType(handle, &type))) {
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
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "scanning '%s'\n",
			     acpi_name(handle)));
	    child = BUS_ADD_CHILD(bus, level * 10, NULL, -1);
	    if (child == NULL)
		break;
	    acpi_set_handle(child, handle);

	    /*
	     * Check that the device is present.  If it's not present,
	     * leave it disabled (so that we have a device_t attached to
	     * the handle, but we don't probe it).
	     */
	    if (type == ACPI_TYPE_DEVICE && !acpi_DeviceIsPresent(child)) {
		device_disable(child);
		break;
	    }

	    /*
	     * Get the device's resource settings and attach them.
	     * Note that if the device has _PRS but no _CRS, we need
	     * to decide when it's appropriate to try to configure the
	     * device.  Ignore the return value here; it's OK for the
	     * device not to have any resources.
	     */
	    acpi_parse_resources(child, handle, &acpi_res_parse_set);

	    /* If we're debugging, probe/attach now rather than later */
	    ACPI_DEBUG_EXEC(device_probe_and_attach(child));
	    break;
	}
    }

    return_ACPI_STATUS (AE_OK);
}

static void
acpi_shutdown_pre_sync(void *arg, int howto)
{
    struct acpi_softc *sc = arg;

    ACPI_ASSERTLOCK;

    /*
     * Disable all ACPI events before soft off, otherwise the system
     * will be turned on again on some laptops.
     *
     * XXX this should probably be restricted to masking some events just
     *     before powering down, since we may still need ACPI during the
     *     shutdown process.
     */
    if (sc->acpi_disable_on_poweroff)
	acpi_Disable(sc);
}

static void
acpi_shutdown_final(void *arg, int howto)
{
    ACPI_STATUS	status;

    ACPI_ASSERTLOCK;

    if ((howto & RB_POWEROFF) != 0) {
	printf("Powering system off using ACPI\n");
	status = AcpiEnterSleepStatePrep(acpi_off_state);
	if (ACPI_FAILURE(status)) {
	    printf("AcpiEnterSleepStatePrep failed - %s\n",
		   AcpiFormatException(status));
	    return;
	}
	ACPI_DISABLE_IRQS();
	status = AcpiEnterSleepState(acpi_off_state);
	if (ACPI_FAILURE(status)) {
	    printf("ACPI power-off failed - %s\n", AcpiFormatException(status));
	} else {
	    DELAY(1000000);
	    printf("ACPI power-off failed - timeout\n");
	}
    } else {
	printf("Shutting down ACPI\n");
	AcpiTerminate();
    }
}

static void
acpi_enable_fixed_events(struct acpi_softc *sc)
{
    static int	first_time = 1;

    ACPI_ASSERTLOCK;

    /* Enable and clear fixed events and install handlers. */
    if (AcpiGbl_FADT != NULL && AcpiGbl_FADT->PwrButton == 0) {
	AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
	AcpiClearEvent(ACPI_EVENT_POWER_BUTTON);
	AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
				     acpi_eventhandler_power_button_for_sleep,
				     sc);
	if (first_time)
	    device_printf(sc->acpi_dev, "Power Button (fixed)\n");
    }
    if (AcpiGbl_FADT != NULL && AcpiGbl_FADT->SleepButton == 0) {
	AcpiEnableEvent(ACPI_EVENT_SLEEP_BUTTON, 0);
	AcpiClearEvent(ACPI_EVENT_SLEEP_BUTTON);
	AcpiInstallFixedEventHandler(ACPI_EVENT_SLEEP_BUTTON,
				     acpi_eventhandler_sleep_button_for_sleep,
				     sc);
	if (first_time)
	    device_printf(sc->acpi_dev, "Sleep Button (fixed)\n");
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
    ACPI_BUFFER		buf = {sizeof(devinfo), &devinfo};
    ACPI_STATUS		error;

    ACPI_ASSERTLOCK;
    
    if ((h = acpi_get_handle(dev)) == NULL)
	return (FALSE);
    error = AcpiGetObjectInfo(h, &buf);
    if (ACPI_FAILURE(error))
	return (FALSE);

    /* If no _STA method, must be present */
    if ((devinfo.Valid & ACPI_VALID_STA) == 0)
	return (TRUE);

    /* Return true for 'present' and 'functioning' */
    if ((devinfo.CurrentStatus & 0x9) == 0x9)
	return (TRUE);

    return (FALSE);
}

/*
 * Returns true if the battery is actually present and inserted.
 */
BOOLEAN
acpi_BatteryIsPresent(device_t dev)
{
    ACPI_HANDLE		h;
    ACPI_DEVICE_INFO	devinfo;
    ACPI_BUFFER		buf = {sizeof(devinfo), &devinfo};
    ACPI_STATUS		error;

    ACPI_ASSERTLOCK;
    
    if ((h = acpi_get_handle(dev)) == NULL)
	return (FALSE);
    error = AcpiGetObjectInfo(h, &buf);
    if (ACPI_FAILURE(error))
	return (FALSE);

    /* If no _STA method, must be present */
    if ((devinfo.Valid & ACPI_VALID_STA) == 0)
	return (TRUE);

    /* Return true for 'present' and 'functioning' */
    if ((devinfo.CurrentStatus & 0x19) == 0x19)
	return (TRUE);

    return (FALSE);
}

/*
 * Match a HID string against a device
 */
BOOLEAN
acpi_MatchHid(device_t dev, char *hid) 
{
    ACPI_HANDLE		h;
    ACPI_DEVICE_INFO	devinfo;
    ACPI_BUFFER		buf = {sizeof(devinfo), &devinfo};
    ACPI_STATUS		error;
    int			cid;

    ACPI_ASSERTLOCK;

    if (hid == NULL)
	return (FALSE);
    if ((h = acpi_get_handle(dev)) == NULL)
	return (FALSE);
    error = AcpiGetObjectInfo(h, &buf);
    if (ACPI_FAILURE(error))
	return (FALSE);
    if ((devinfo.Valid & ACPI_VALID_HID) != 0 &&
	strcmp(hid, devinfo.HardwareId.Value) == 0)
	return (TRUE);

    if (ACPI_FAILURE(error = acpi_EvaluateInteger(h, "_CID", &cid)))
	return (FALSE);
    if (cid == PNP_EISAID(hid))
	return (TRUE);

    return (FALSE);
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

    ACPI_ASSERTLOCK;

    /* Walk back up the tree to the root */
    for (;;) {
	status = AcpiGetHandle(parent, path, &r);
	if (ACPI_SUCCESS(status)) {
	    *result = r;
	    return (AE_OK);
	}
	if (status != AE_NOT_FOUND)
	    return (AE_OK);
	if (ACPI_FAILURE(AcpiGetParent(parent, &r)))
	    return (AE_NOT_FOUND);
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
	return (NULL);
    buf->Length = size;
    buf->Pointer = (void *)(buf + 1);
    return (buf);
}

/*
 * Evaluate a path that should return an integer.
 */
ACPI_STATUS
acpi_EvaluateInteger(ACPI_HANDLE handle, char *path, int *number)
{
    ACPI_STATUS	status;
    ACPI_BUFFER	buf;
    ACPI_OBJECT	param;

    ACPI_ASSERTLOCK;

    if (handle == NULL)
	handle = ACPI_ROOT_OBJECT;

    /*
     * Assume that what we've been pointed at is an Integer object, or
     * a method that will return an Integer.
     */
    buf.Pointer = &param;
    buf.Length = sizeof(param);
    status = AcpiEvaluateObject(handle, path, NULL, &buf);
    if (ACPI_SUCCESS(status)) {
	if (param.Type == ACPI_TYPE_INTEGER)
	    *number = param.Integer.Value;
	else
	    status = AE_TYPE;
    }

    /* 
     * In some applications, a method that's expected to return an Integer
     * may instead return a Buffer (probably to simplify some internal
     * arithmetic).  We'll try to fetch whatever it is, and if it's a Buffer,
     * convert it into an Integer as best we can.
     *
     * This is a hack.
     */
    if (status == AE_BUFFER_OVERFLOW) {
	if ((buf.Pointer = AcpiOsAllocate(buf.Length)) == NULL) {
	    status = AE_NO_MEMORY;
	} else {
	    status = AcpiEvaluateObject(handle, path, NULL, &buf);
	    if (ACPI_SUCCESS(status))
		status = acpi_ConvertBufferToInteger(&buf, number);
	    AcpiOsFree(buf.Pointer);
	}
    }
    return (status);
}

ACPI_STATUS
acpi_ConvertBufferToInteger(ACPI_BUFFER *bufp, int *number)
{
    ACPI_OBJECT	*p;
    int		i;

    p = (ACPI_OBJECT *)bufp->Pointer;
    if (p->Type == ACPI_TYPE_INTEGER) {
	*number = p->Integer.Value;
	return (AE_OK);
    }
    if (p->Type != ACPI_TYPE_BUFFER)
	return (AE_TYPE);
    if (p->Buffer.Length > sizeof(int))
	return (AE_BAD_DATA);

    *number = 0;
    for (i = 0; i < p->Buffer.Length; i++)
	*number += (*(p->Buffer.Pointer + i) << (i * 8));
    return (AE_OK);
}

/*
 * Iterate over the elements of an a package object, calling the supplied
 * function for each element.
 *
 * XXX possible enhancement might be to abort traversal on error.
 */
ACPI_STATUS
acpi_ForeachPackageObject(ACPI_OBJECT *pkg,
	void (*func)(ACPI_OBJECT *comp, void *arg), void *arg)
{
    ACPI_OBJECT	*comp;
    int		i;
    
    if (pkg == NULL || pkg->Type != ACPI_TYPE_PACKAGE)
	return (AE_BAD_PARAMETER);

    /* Iterate over components */
    i = 0;
    comp = pkg->Package.Elements;
    for (; i < pkg->Package.Count; i++, comp++)
	func(comp, arg);

    return (AE_OK);
}

/*
 * Find the (index)th resource object in a set.
 */
ACPI_STATUS
acpi_FindIndexedResource(ACPI_BUFFER *buf, int index, ACPI_RESOURCE **resp)
{
    ACPI_RESOURCE	*rp;
    int			i;

    rp = (ACPI_RESOURCE *)buf->Pointer;
    i = index;
    while (i-- > 0) {
	/* Range check */	
	if (rp > (ACPI_RESOURCE *)((u_int8_t *)buf->Pointer + buf->Length))
	    return (AE_BAD_PARAMETER);

	/* Check for terminator */
	if (rp->Id == ACPI_RSTYPE_END_TAG || rp->Length == 0)
	    return (AE_NOT_FOUND);
	rp = ACPI_RESOURCE_NEXT(rp);
    }
    if (resp != NULL)
	*resp = rp;

    return (AE_OK);
}

/*
 * Append an ACPI_RESOURCE to an ACPI_BUFFER.
 *
 * Given a pointer to an ACPI_RESOURCE structure, expand the ACPI_BUFFER
 * provided to contain it.  If the ACPI_BUFFER is empty, allocate a sensible
 * backing block.  If the ACPI_RESOURCE is NULL, return an empty set of
 * resources.
 */
#define ACPI_INITIAL_RESOURCE_BUFFER_SIZE	512

ACPI_STATUS
acpi_AppendBufferResource(ACPI_BUFFER *buf, ACPI_RESOURCE *res)
{
    ACPI_RESOURCE	*rp;
    void		*newp;
    
    /* Initialise the buffer if necessary. */
    if (buf->Pointer == NULL) {
	buf->Length = ACPI_INITIAL_RESOURCE_BUFFER_SIZE;
	if ((buf->Pointer = AcpiOsAllocate(buf->Length)) == NULL)
	    return (AE_NO_MEMORY);
	rp = (ACPI_RESOURCE *)buf->Pointer;
	rp->Id = ACPI_RSTYPE_END_TAG;
	rp->Length = 0;
    }
    if (res == NULL)
	return (AE_OK);
    
    /*
     * Scan the current buffer looking for the terminator.
     * This will either find the terminator or hit the end
     * of the buffer and return an error.
     */
    rp = (ACPI_RESOURCE *)buf->Pointer;
    for (;;) {
	/* Range check, don't go outside the buffer */
	if (rp >= (ACPI_RESOURCE *)((u_int8_t *)buf->Pointer + buf->Length))
	    return (AE_BAD_PARAMETER);
	if (rp->Id == ACPI_RSTYPE_END_TAG || rp->Length == 0)
	    break;
	rp = ACPI_RESOURCE_NEXT(rp);
    }

    /*
     * Check the size of the buffer and expand if required.
     *
     * Required size is:
     *	size of existing resources before terminator + 
     *	size of new resource and header +
     * 	size of terminator.
     *
     * Note that this loop should really only run once, unless
     * for some reason we are stuffing a *really* huge resource.
     */
    while ((((u_int8_t *)rp - (u_int8_t *)buf->Pointer) + 
	    res->Length + ACPI_RESOURCE_LENGTH_NO_DATA +
	    ACPI_RESOURCE_LENGTH) >= buf->Length) {
	if ((newp = AcpiOsAllocate(buf->Length * 2)) == NULL)
	    return (AE_NO_MEMORY);
	bcopy(buf->Pointer, newp, buf->Length);
	rp = (ACPI_RESOURCE *)((u_int8_t *)newp +
			       ((u_int8_t *)rp - (u_int8_t *)buf->Pointer));
	AcpiOsFree(buf->Pointer);
	buf->Pointer = newp;
	buf->Length += buf->Length;
    }
    
    /* Insert the new resource. */
    bcopy(res, rp, res->Length + ACPI_RESOURCE_LENGTH_NO_DATA);
    
    /* And add the terminator. */
    rp = ACPI_RESOURCE_NEXT(rp);
    rp->Id = ACPI_RSTYPE_END_TAG;
    rp->Length = 0;

    return (AE_OK);
}

/*
 * Set interrupt model.
 */
ACPI_STATUS
acpi_SetIntrModel(int model)
{
    ACPI_OBJECT_LIST ArgList;
    ACPI_OBJECT Arg;

    Arg.Type = ACPI_TYPE_INTEGER;
    Arg.Integer.Value = model;
    ArgList.Count = 1;
    ArgList.Pointer = &Arg;
    return (AcpiEvaluateObject(ACPI_ROOT_OBJECT, "_PIC", &ArgList, NULL));
}

#define ACPI_MINIMUM_AWAKETIME	5

static void
acpi_sleep_enable(void *arg)
{
    ((struct acpi_softc *)arg)->acpi_sleep_disabled = 0;
}

/*
 * Set the system sleep state
 *
 * Currently we support S1-S5 but S4 is only S4BIOS
 */
ACPI_STATUS
acpi_SetSleepState(struct acpi_softc *sc, int state)
{
    ACPI_STATUS	status = AE_OK;
    UINT8	TypeA;
    UINT8	TypeB;

    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, state);
    ACPI_ASSERTLOCK;

    /* Avoid reentry if already attempting to suspend. */
    if (sc->acpi_sstate != ACPI_STATE_S0)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    /* We recently woke up so don't suspend again for a while. */
    if (sc->acpi_sleep_disabled)
	return_ACPI_STATUS (AE_OK);

    switch (state) {
    case ACPI_STATE_S1:
    case ACPI_STATE_S2:
    case ACPI_STATE_S3:
    case ACPI_STATE_S4:
	status = AcpiGetSleepTypeData((UINT8)state, &TypeA, &TypeB);
	if (status == AE_NOT_FOUND) {
	    device_printf(sc->acpi_dev,
			  "Sleep state S%d not supported by BIOS\n", state);
	    break;
	} else if (ACPI_FAILURE(status)) {
	    device_printf(sc->acpi_dev, "AcpiGetSleepTypeData failed - %s\n",
			  AcpiFormatException(status));
	    break;
	}

	sc->acpi_sstate = state;
	sc->acpi_sleep_disabled = 1;

	/* Inform all devices that we are going to sleep. */
	if (DEVICE_SUSPEND(root_bus) != 0) {
	    /*
	     * Re-wake the system.
	     *
	     * XXX note that a better two-pass approach with a 'veto' pass
	     *     followed by a "real thing" pass would be better, but the
	     *     current bus interface does not provide for this.
	     */
	    DEVICE_RESUME(root_bus);
	    return_ACPI_STATUS (AE_ERROR);
	}

	status = AcpiEnterSleepStatePrep(state);
	if (ACPI_FAILURE(status)) {
	    device_printf(sc->acpi_dev, "AcpiEnterSleepStatePrep failed - %s\n",
			  AcpiFormatException(status));
	    break;
	}

	if (sc->acpi_sleep_delay > 0)
	    DELAY(sc->acpi_sleep_delay * 1000000);

	if (state != ACPI_STATE_S1) {
	    acpi_sleep_machdep(sc, state);

	    /* AcpiEnterSleepState() may be incomplete, unlock if locked. */
	    if (AcpiGbl_MutexInfo[ACPI_MTX_HARDWARE].OwnerId !=
		ACPI_MUTEX_NOT_ACQUIRED) {

		AcpiUtReleaseMutex(ACPI_MTX_HARDWARE);
	    }

	    /* Re-enable ACPI hardware on wakeup from sleep state 4. */
	    if (state == ACPI_STATE_S4)
		AcpiEnable();
	} else {
	    status = AcpiEnterSleepState((UINT8)state);
	    if (ACPI_FAILURE(status)) {
		device_printf(sc->acpi_dev, "AcpiEnterSleepState failed - %s\n",
			      AcpiFormatException(status));
		break;
	    }
	}
	AcpiLeaveSleepState((UINT8)state);
	DEVICE_RESUME(root_bus);
	sc->acpi_sstate = ACPI_STATE_S0;
	acpi_enable_fixed_events(sc);
	break;
    case ACPI_STATE_S5:
	/*
	 * Shut down cleanly and power off.  This will call us back through the
	 * shutdown handlers.
	 */
	shutdown_nice(RB_POWEROFF);
	break;
    case ACPI_STATE_S0:
    default:
	status = AE_BAD_PARAMETER;
	break;
    }

    /* Disable a second sleep request for a short period */
    if (sc->acpi_sleep_disabled)
	timeout(acpi_sleep_enable, (caddr_t)sc, hz * ACPI_MINIMUM_AWAKETIME);

    return_ACPI_STATUS (status);
}

/*
 * Enable/Disable ACPI
 */
ACPI_STATUS
acpi_Enable(struct acpi_softc *sc)
{
    ACPI_STATUS	status;
    u_int32_t	flags;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    ACPI_ASSERTLOCK;

    flags = ACPI_NO_ADDRESS_SPACE_INIT | ACPI_NO_HARDWARE_INIT |
	    ACPI_NO_DEVICE_INIT | ACPI_NO_OBJECT_INIT;
    if (!sc->acpi_enabled)
	status = AcpiEnableSubsystem(flags);
    else
	status = AE_OK;

    if (status == AE_OK)
	sc->acpi_enabled = 1;

    return_ACPI_STATUS (status);
}

ACPI_STATUS
acpi_Disable(struct acpi_softc *sc)
{
    ACPI_STATUS	status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    ACPI_ASSERTLOCK;

    if (sc->acpi_enabled)
	status = AcpiDisable();
    else
	status = AE_OK;

    if (status == AE_OK)
	sc->acpi_enabled = 0;

    return_ACPI_STATUS (status);
}

/*
 * ACPI Event Handlers
 */

/* System Event Handlers (registered by EVENTHANDLER_REGISTER) */

static void
acpi_system_eventhandler_sleep(void *arg, int state)
{
    ACPI_LOCK_DECL;
    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, state);

    ACPI_LOCK;
    if (state >= ACPI_STATE_S0 && state <= ACPI_S_STATES_MAX)
	acpi_SetSleepState((struct acpi_softc *)arg, state);
    ACPI_UNLOCK;
    return_VOID;
}

static void
acpi_system_eventhandler_wakeup(void *arg, int state)
{
    ACPI_LOCK_DECL;
    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, state);

    /* Well, what to do? :-) */

    ACPI_LOCK;
    ACPI_UNLOCK;

    return_VOID;
}

/* 
 * ACPICA Event Handlers (FixedEvent, also called from button notify handler)
 */
UINT32
acpi_eventhandler_power_button_for_sleep(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    EVENTHANDLER_INVOKE(acpi_sleep_event, sc->acpi_power_button_sx);

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}

UINT32
acpi_eventhandler_power_button_for_wakeup(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    EVENTHANDLER_INVOKE(acpi_wakeup_event, sc->acpi_power_button_sx);

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}

UINT32
acpi_eventhandler_sleep_button_for_sleep(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    EVENTHANDLER_INVOKE(acpi_sleep_event, sc->acpi_sleep_button_sx);

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}

UINT32
acpi_eventhandler_sleep_button_for_wakeup(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    EVENTHANDLER_INVOKE(acpi_wakeup_event, sc->acpi_sleep_button_sx);

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}

/*
 * XXX This is kinda ugly, and should not be here.
 */
struct acpi_staticbuf {
    ACPI_BUFFER	buffer;
    char	data[512];
};

char *
acpi_name(ACPI_HANDLE handle)
{
    static struct acpi_staticbuf	buf;

    ACPI_ASSERTLOCK;

    buf.buffer.Length = 512;
    buf.buffer.Pointer = &buf.data[0];

    if (ACPI_SUCCESS(AcpiGetName(handle, ACPI_FULL_PATHNAME, &buf.buffer)))
	return (buf.buffer.Pointer);

    return ("(unknown path)");
}

/*
 * Debugging/bug-avoidance.  Avoid trying to fetch info on various
 * parts of the namespace.
 */
int
acpi_avoid(ACPI_HANDLE handle)
{
    char	*cp, *env, *np;
    int		len;

    np = acpi_name(handle);
    if (*np == '\\')
	np++;
    if ((env = getenv("debug.acpi.avoid")) == NULL)
	return (0);

    /* Scan the avoid list checking for a match */
    cp = env;
    for (;;) {
	while ((*cp != 0) && isspace(*cp))
	    cp++;
	if (*cp == 0)
	    break;
	len = 0;
	while ((cp[len] != 0) && !isspace(cp[len]))
	    len++;
	if (!strncmp(cp, np, len)) {
	    freeenv(env);
	    return(1);
	}
	cp += len;
    }
    freeenv(env);

    return (0);
}

/*
 * Debugging/bug-avoidance.  Disable ACPI subsystem components.
 */
int
acpi_disabled(char *subsys)
{
    char	*cp, *env;
    int		len;

    if ((env = getenv("debug.acpi.disable")) == NULL)
	return (0);
    if (!strcmp(env, "all")) {
	freeenv(env);
	return (1);
    }

    /* scan the disable list checking for a match */
    cp = env;
    for (;;) {
	while ((*cp != 0) && isspace(*cp))
	    cp++;
	if (*cp == 0)
	    break;
	len = 0;
	while ((cp[len] != 0) && !isspace(cp[len]))
	    len++;
	if (!strncmp(cp, subsys, len)) {
	    freeenv(env);
	    return (1);
	}
	cp += len;
    }
    freeenv(env);

    return (0);
}

/*
 * Device wake capability enable/disable.
 */
void
acpi_device_enable_wake_capability(ACPI_HANDLE h, int enable)
{
    ACPI_OBJECT_LIST		ArgList;
    ACPI_OBJECT			Arg;

    /*
     * TBD: All Power Resources referenced by elements 2 through N
     *      of the _PRW object are put into the ON state.
     */

    ArgList.Count = 1;
    ArgList.Pointer = &Arg;

    Arg.Type = ACPI_TYPE_INTEGER;
    Arg.Integer.Value = enable;

    (void)AcpiEvaluateObject(h, "_PSW", &ArgList, NULL);
}

void
acpi_device_enable_wake_event(ACPI_HANDLE h)
{
    struct acpi_softc		*sc;
    ACPI_STATUS			status;
    ACPI_BUFFER			prw_buffer;
    ACPI_OBJECT			*res;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = devclass_get_softc(acpi_devclass, 0);
    if (sc == NULL)
	return;

    /*
     * _PRW object is only required for devices that have the ability
     * to wake the system from a system sleeping state.
     */
    prw_buffer.Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiEvaluateObject(h, "_PRW", NULL, &prw_buffer);
    if (ACPI_FAILURE(status))
	return;

    res = (ACPI_OBJECT *)prw_buffer.Pointer;
    if (res == NULL)
	return;

    if ((res->Type != ACPI_TYPE_PACKAGE) || (res->Package.Count < 2)) {
	goto out;
    }

    /*
     * The element 1 of the _PRW object:
     * The lowest power system sleeping state that can be entered
     * while still providing wake functionality.
     * The sleeping state being entered must be greater or equal to
     * the power state declared in element 1 of the _PRW object.
     */
    if (res->Package.Elements[1].Type != ACPI_TYPE_INTEGER)
	goto out;

    if (sc->acpi_sstate > res->Package.Elements[1].Integer.Value)
	goto out;

    /*
     * The element 0 of the _PRW object:
     */
    switch(res->Package.Elements[0].Type) {
    case ACPI_TYPE_INTEGER:
	/* 
	 * If the data type of this package element is numeric, then this
	 * _PRW package element is the bit index in the GPEx_EN, in the
	 * GPE blocks described in the FADT, of the enable bit that is
	 * enabled for the wake event.
	 */

	status = AcpiEnableGpe(NULL, res->Package.Elements[0].Integer.Value,
			       ACPI_EVENT_WAKE_ENABLE);
	if (ACPI_FAILURE(status))
	    printf("%s: EnableEvent Failed\n", __func__);
	break;
    case ACPI_TYPE_PACKAGE:
	/*
	 * XXX TBD
	 *
	 * If the data type of this package element is a package, then this
	 * _PRW package element is itself a package containing two
	 * elements. The first is an object reference to the GPE Block
	 * device that contains the GPE that will be triggered by the wake
	 * event. The second element is numeric and it contains the bit
	 * index in the GPEx_EN, in the GPE Block referenced by the
	 * first element in the package, of the enable bit that is enabled for
	 * the wake event.
	 * For example, if this field is a package then it is of the form:
	 * Package() {\_SB.PCI0.ISA.GPE, 2}
	 */
	break;
    default:
	break;
    }

out:
    if (prw_buffer.Pointer != NULL)
	AcpiOsFree(prw_buffer.Pointer);
}

/*
 * Control interface.
 *
 * We multiplex ioctls for all participating ACPI devices here.  Individual 
 * drivers wanting to be accessible via /dev/acpi should use the
 * register/deregister interface to make their handlers visible.
 */
struct acpi_ioctl_hook
{
    TAILQ_ENTRY(acpi_ioctl_hook) link;
    u_long			 cmd;
    acpi_ioctl_fn		 fn;
    void			 *arg;
};

static TAILQ_HEAD(,acpi_ioctl_hook)	acpi_ioctl_hooks;
static int				acpi_ioctl_hooks_initted;

/*
 * Register an ioctl handler.
 */
int
acpi_register_ioctl(u_long cmd, acpi_ioctl_fn fn, void *arg)
{
    struct acpi_ioctl_hook	*hp;

    if ((hp = malloc(sizeof(*hp), M_ACPIDEV, M_NOWAIT)) == NULL)
	return (ENOMEM);
    hp->cmd = cmd;
    hp->fn = fn;
    hp->arg = arg;
    if (acpi_ioctl_hooks_initted == 0) {
	TAILQ_INIT(&acpi_ioctl_hooks);
	acpi_ioctl_hooks_initted = 1;
    }
    TAILQ_INSERT_TAIL(&acpi_ioctl_hooks, hp, link);
    return (0);
}

/*
 * Deregister an ioctl handler.
 */
void	
acpi_deregister_ioctl(u_long cmd, acpi_ioctl_fn fn)
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
acpiopen(dev_t dev, int flag, int fmt, d_thread_t *td)
{
    return (0);
}

static int
acpiclose(dev_t dev, int flag, int fmt, d_thread_t *td)
{
    return (0);
}

static int
acpiioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, d_thread_t *td)
{
    struct acpi_softc		*sc;
    struct acpi_ioctl_hook	*hp;
    int				error, xerror, state;
    ACPI_LOCK_DECL;

    ACPI_LOCK;

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
     * Core ioctls are not permitted for non-writable user.
     * Currently, other ioctls just fetch information.
     * Not changing system behavior.
     */
    if((flag & FWRITE) == 0)
	return (EPERM);

    /* Core system ioctls. */
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
	    if (ACPI_FAILURE(acpi_SetSleepState(sc, state)))
		error = EINVAL;
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
    ACPI_UNLOCK;
    return (error);
}

static int
acpi_supported_sleep_state_sysctl(SYSCTL_HANDLER_ARGS)
{
    char sleep_state[4];
    char buf[16];
    int error;
    UINT8 state, TypeA, TypeB;

    buf[0] = '\0';
    for (state = ACPI_STATE_S1; state < ACPI_S_STATES_MAX+1; state++) {
	if (ACPI_SUCCESS(AcpiGetSleepTypeData(state, &TypeA, &TypeB))) {
	    sprintf(sleep_state, "S%d ", state);
	    strcat(buf, sleep_state);
	}
    }
    error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
    return (error);
}

static int
acpi_sleep_state_sysctl(SYSCTL_HANDLER_ARGS)
{
    char sleep_state[10];
    int error;
    u_int new_state, old_state;

    old_state = *(u_int *)oidp->oid_arg1;
    if (old_state > ACPI_S_STATES_MAX+1) {
	strcpy(sleep_state, "unknown");
    } else {
	bzero(sleep_state, sizeof(sleep_state));
	strncpy(sleep_state, sleep_state_names[old_state],
		sizeof(sleep_state_names[old_state]));
    }
    error = sysctl_handle_string(oidp, sleep_state, sizeof(sleep_state), req);
    if (error == 0 && req->newptr != NULL) {
	new_state = ACPI_STATE_S0;
	for (; new_state <= ACPI_S_STATES_MAX + 1; new_state++) {
	    if (strncmp(sleep_state, sleep_state_names[new_state],
			sizeof(sleep_state)) == 0)
		break;
	}
	if (new_state <= ACPI_S_STATES_MAX + 1) {
	    if (new_state != old_state)
		*(u_int *)oidp->oid_arg1 = new_state;
	} else {
	    error = EINVAL;
	}
    }

    return (error);
}

/* Inform devctl(4) when we receive a Notify. */
void
acpi_UserNotify(const char *subsystem, ACPI_HANDLE h, uint8_t notify)
{
    char		notify_buf[16];
    ACPI_BUFFER		handle_buf;
    ACPI_STATUS		status;

    if (subsystem == NULL)
	return;

    handle_buf.Pointer = NULL;
    handle_buf.Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiNsHandleToPathname(h, &handle_buf);
    if (ACPI_FAILURE(status))
	return;
    snprintf(notify_buf, sizeof(notify_buf), "notify=0x%02x", notify);
    devctl_notify("ACPI", subsystem, handle_buf.Pointer, notify_buf);
    AcpiOsFree(handle_buf.Pointer);
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
    {"ACPI_CA_DEBUGGER",	ACPI_CA_DEBUGGER},
    {"ACPI_OS_SERVICES",	ACPI_OS_SERVICES},
    {"ACPI_CA_DISASSEMBLER",	ACPI_CA_DISASSEMBLER},
    {"ACPI_ALL_COMPONENTS",	ACPI_ALL_COMPONENTS},

    {"ACPI_BUS",		ACPI_BUS},
    {"ACPI_SYSTEM",		ACPI_SYSTEM},
    {"ACPI_POWER",		ACPI_POWER},
    {"ACPI_EC", 		ACPI_EC},
    {"ACPI_AC_ADAPTER",		ACPI_AC_ADAPTER},
    {"ACPI_BATTERY",		ACPI_BATTERY},
    {"ACPI_BUTTON",		ACPI_BUTTON},
    {"ACPI_PROCESSOR",		ACPI_PROCESSOR},
    {"ACPI_THERMAL",		ACPI_THERMAL},
    {"ACPI_FAN",		ACPI_FAN},
    {"ACPI_ALL_DRIVERS",	ACPI_ALL_DRIVERS},
    {NULL, 0}
};

static struct debugtag dbg_level[] = {
    {"ACPI_LV_ERROR",		ACPI_LV_ERROR},
    {"ACPI_LV_WARN",		ACPI_LV_WARN},
    {"ACPI_LV_INIT",		ACPI_LV_INIT},
    {"ACPI_LV_DEBUG_OBJECT",	ACPI_LV_DEBUG_OBJECT},
    {"ACPI_LV_INFO",		ACPI_LV_INFO},
    {"ACPI_LV_ALL_EXCEPTIONS",	ACPI_LV_ALL_EXCEPTIONS},

    /* Trace verbosity level 1 [Standard Trace Level] */
    {"ACPI_LV_INIT_NAMES",	ACPI_LV_INIT_NAMES},
    {"ACPI_LV_PARSE",		ACPI_LV_PARSE},
    {"ACPI_LV_LOAD",		ACPI_LV_LOAD},
    {"ACPI_LV_DISPATCH",	ACPI_LV_DISPATCH},
    {"ACPI_LV_EXEC",		ACPI_LV_EXEC},
    {"ACPI_LV_NAMES",		ACPI_LV_NAMES},
    {"ACPI_LV_OPREGION",	ACPI_LV_OPREGION},
    {"ACPI_LV_BFIELD",		ACPI_LV_BFIELD},
    {"ACPI_LV_TABLES",		ACPI_LV_TABLES},
    {"ACPI_LV_VALUES",		ACPI_LV_VALUES},
    {"ACPI_LV_OBJECTS",		ACPI_LV_OBJECTS},
    {"ACPI_LV_RESOURCES",	ACPI_LV_RESOURCES},
    {"ACPI_LV_USER_REQUESTS",	ACPI_LV_USER_REQUESTS},
    {"ACPI_LV_PACKAGE",		ACPI_LV_PACKAGE},
    {"ACPI_LV_VERBOSITY1",	ACPI_LV_VERBOSITY1},

    /* Trace verbosity level 2 [Function tracing and memory allocation] */
    {"ACPI_LV_ALLOCATIONS",	ACPI_LV_ALLOCATIONS},
    {"ACPI_LV_FUNCTIONS",	ACPI_LV_FUNCTIONS},
    {"ACPI_LV_OPTIMIZATIONS",	ACPI_LV_OPTIMIZATIONS},
    {"ACPI_LV_VERBOSITY2",	ACPI_LV_VERBOSITY2},
    {"ACPI_LV_ALL",		ACPI_LV_ALL},

    /* Trace verbosity level 3 [Threading, I/O, and Interrupts] */
    {"ACPI_LV_MUTEX",		ACPI_LV_MUTEX},
    {"ACPI_LV_THREADS",		ACPI_LV_THREADS},
    {"ACPI_LV_IO",		ACPI_LV_IO},
    {"ACPI_LV_INTERRUPTS",	ACPI_LV_INTERRUPTS},
    {"ACPI_LV_VERBOSITY3",	ACPI_LV_VERBOSITY3},

    /* Exceptionally verbose output -- also used in the global "DebugLevel"  */
    {"ACPI_LV_AML_DISASSEMBLE",	ACPI_LV_AML_DISASSEMBLE},
    {"ACPI_LV_VERBOSE_INFO",	ACPI_LV_VERBOSE_INFO},
    {"ACPI_LV_FULL_TABLES",	ACPI_LV_FULL_TABLES},
    {"ACPI_LV_EVENTS",		ACPI_LV_EVENTS},
    {"ACPI_LV_VERBOSE",		ACPI_LV_VERBOSE},
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
		if (set)
		    *flag |= tag[i].value;
		else
		    *flag &= ~tag[i].value;
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

    if (cold) {
	AcpiDbgLayer = 0;
	AcpiDbgLevel = 0;
    }

    if ((cp = getenv("debug.acpi.layer")) != NULL) {
	acpi_parse_debug(cp, &dbg_layer[0], &AcpiDbgLayer);
	freeenv(cp);
    }
    if ((cp = getenv("debug.acpi.level")) != NULL) {
	acpi_parse_debug(cp, &dbg_level[0], &AcpiDbgLevel);
	freeenv(cp);
    }

    if (cold) {
	printf("ACPI debug layer 0x%x debug level 0x%x\n",
	       AcpiDbgLayer, AcpiDbgLevel);
    }
}
SYSINIT(acpi_debugging, SI_SUB_TUNABLES, SI_ORDER_ANY, acpi_set_debugging,
	NULL);

static int
acpi_debug_sysctl(SYSCTL_HANDLER_ARGS)
{
    char	*options;
    int		 error, len, *dbg;
    struct	 debugtag *tag;

    len = 512;
    MALLOC(options, char *, len, M_TEMP, M_WAITOK);
    options[0] = '\0';

    if (strcmp(oidp->oid_arg1, "debug.acpi.layer") == 0) {
	tag = &dbg_layer[0];
	dbg = &AcpiDbgLayer;
    } else {
	tag = &dbg_level[0];
	dbg = &AcpiDbgLevel;
    }

    /* Get old values if this is a get request. */
    if (*dbg == 0) {
	strlcpy(options, "NONE", sizeof(options));
    } else if (req->newptr == NULL) {
	for (; tag->name != NULL; tag++) {
	    if ((*dbg & tag->value) == tag->value) {
		strlcat(options, tag->name, len);
		strlcat(options, " ", len); /* XXX */
	    }
	}
    }

    error = sysctl_handle_string(oidp, options, len, req);

    /* If the user is setting a string, parse it. */
    if (error == 0 && req->newptr != NULL) {
	*dbg = 0;
	setenv((char *)oidp->oid_arg1, (char *)req->newptr);
	acpi_set_debugging(NULL);
    }
    FREE(options, M_TEMP);

    return (error);
}
SYSCTL_PROC(_debug_acpi, OID_AUTO, layer, CTLFLAG_RW | CTLTYPE_STRING,
	    "debug.acpi.layer", 0, acpi_debug_sysctl, "A", "");
SYSCTL_PROC(_debug_acpi, OID_AUTO, level, CTLFLAG_RW | CTLTYPE_STRING,
	    "debug.acpi.level", 0, acpi_debug_sysctl, "A", "");
#endif

static int
acpi_pm_func(u_long cmd, void *arg, ...)
{
	int	state, acpi_state;
	int	error;
	struct	acpi_softc *sc;
	va_list	ap;

	error = 0;
	switch (cmd) {
	case POWER_CMD_SUSPEND:
		sc = (struct acpi_softc *)arg;
		if (sc == NULL) {
			error = EINVAL;
			goto out;
		}

		va_start(ap, arg);
		state = va_arg(ap, int);
		va_end(ap);	

		switch (state) {
		case POWER_SLEEP_STATE_STANDBY:
			acpi_state = sc->acpi_standby_sx;
			break;
		case POWER_SLEEP_STATE_SUSPEND:
			acpi_state = sc->acpi_suspend_sx;
			break;
		case POWER_SLEEP_STATE_HIBERNATE:
			acpi_state = ACPI_STATE_S4;
			break;
		default:
			error = EINVAL;
			goto out;
		}

		acpi_SetSleepState(sc, acpi_state);
		break;
	default:
		error = EINVAL;
		goto out;
	}

out:
	return (error);
}

static void
acpi_pm_register(void *arg)
{
    if (!cold || resource_disabled("acpi", 0))
	return;

    power_pm_register(POWER_PM_TYPE_ACPI, acpi_pm_func, NULL);
}

SYSINIT(power, SI_SUB_KLD, SI_ORDER_ANY, acpi_pm_register, 0);
