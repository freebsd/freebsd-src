/*-
 * Copyright (c) 2003-2005 Nate Lawson (SDG)
 * Copyright (c) 2001 Michael Smith
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/pcpu.h>
#include <sys/power.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/smp.h>

#include <dev/pci/pcivar.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>

/*
 * Support for ACPI Processor devices, including C[1-3] sleep states.
 */

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_PROCESSOR
ACPI_MODULE_NAME("PROCESSOR")

struct acpi_cx {
    struct resource	*p_lvlx;	/* Register to read to enter state. */
    uint32_t		 type;		/* C1-3 (C4 and up treated as C3). */
    uint32_t		 trans_lat;	/* Transition latency (usec). */
    uint32_t		 power;		/* Power consumed (mW). */
    int			 res_type;	/* Resource type for p_lvlx. */
};
#define MAX_CX_STATES	 8

struct acpi_cpu_softc {
    device_t		 cpu_dev;
    ACPI_HANDLE		 cpu_handle;
    struct pcpu		*cpu_pcpu;
    uint32_t		 cpu_acpi_id;	/* ACPI processor id */
    uint32_t		 cpu_p_blk;	/* ACPI P_BLK location */
    uint32_t		 cpu_p_blk_len;	/* P_BLK length (must be 6). */
    struct acpi_cx	 cpu_cx_states[MAX_CX_STATES];
    int			 cpu_cx_count;	/* Number of valid Cx states. */
    int			 cpu_prev_sleep;/* Last idle sleep duration. */
    int			 cpu_features;	/* Child driver supported features. */
    /* Runtime state. */
    int			 cpu_non_c3;	/* Index of lowest non-C3 state. */
    u_int		 cpu_cx_stats[MAX_CX_STATES];/* Cx usage history. */
    /* Values for sysctl. */
    struct sysctl_ctx_list cpu_sysctl_ctx;
    struct sysctl_oid	*cpu_sysctl_tree;
    int			 cpu_cx_lowest;
    char 		 cpu_cx_supported[64];
    int			 cpu_rid;
};

struct acpi_cpu_device {
    struct resource_list	ad_rl;
};

#define CPU_GET_REG(reg, width) 					\
    (bus_space_read_ ## width(rman_get_bustag((reg)), 			\
		      rman_get_bushandle((reg)), 0))
#define CPU_SET_REG(reg, width, val)					\
    (bus_space_write_ ## width(rman_get_bustag((reg)), 			\
		       rman_get_bushandle((reg)), 0, (val)))

#define PM_USEC(x)	 ((x) >> 2)	/* ~4 clocks per usec (3.57955 Mhz) */

#define ACPI_NOTIFY_CX_STATES	0x81	/* _CST changed. */

#define CPU_QUIRK_NO_C3		(1<<0)	/* C3-type states are not usable. */
#define CPU_QUIRK_NO_BM_CTRL	(1<<2)	/* No bus mastering control. */

#define PCI_VENDOR_INTEL	0x8086
#define PCI_DEVICE_82371AB_3	0x7113	/* PIIX4 chipset for quirks. */
#define PCI_REVISION_A_STEP	0
#define PCI_REVISION_B_STEP	1
#define PCI_REVISION_4E		2
#define PCI_REVISION_4M		3
#define PIIX4_DEVACTB_REG	0x58
#define PIIX4_BRLD_EN_IRQ0	(1<<0)
#define PIIX4_BRLD_EN_IRQ	(1<<1)
#define PIIX4_BRLD_EN_IRQ8	(1<<5)
#define PIIX4_STOP_BREAK_MASK	(PIIX4_BRLD_EN_IRQ0 | PIIX4_BRLD_EN_IRQ | PIIX4_BRLD_EN_IRQ8)
#define PIIX4_PCNTRL_BST_EN	(1<<10)

/* Platform hardware resource information. */
static uint32_t		 cpu_smi_cmd;	/* Value to write to SMI_CMD. */
static uint8_t		 cpu_cst_cnt;	/* Indicate we are _CST aware. */
static int		 cpu_quirks;	/* Indicate any hardware bugs. */

/* Runtime state. */
static int		 cpu_disable_idle; /* Disable entry to idle function */
static int		 cpu_cx_count;	/* Number of valid Cx states */

/* Values for sysctl. */
static struct sysctl_ctx_list cpu_sysctl_ctx;
static struct sysctl_oid *cpu_sysctl_tree;
static int		 cpu_cx_generic;
static int		 cpu_cx_lowest;

static device_t		*cpu_devices;
static int		 cpu_ndevices;
static struct acpi_cpu_softc **cpu_softc;
ACPI_SERIAL_DECL(cpu, "ACPI CPU");

static int	acpi_cpu_probe(device_t dev);
static int	acpi_cpu_attach(device_t dev);
static int	acpi_cpu_suspend(device_t dev);
static int	acpi_cpu_resume(device_t dev);
static int	acpi_pcpu_get_id(uint32_t idx, uint32_t *acpi_id,
		    uint32_t *cpu_id);
static struct resource_list *acpi_cpu_get_rlist(device_t dev, device_t child);
static device_t	acpi_cpu_add_child(device_t dev, int order, const char *name,
		    int unit);
static int	acpi_cpu_read_ivar(device_t dev, device_t child, int index,
		    uintptr_t *result);
static int	acpi_cpu_shutdown(device_t dev);
static void	acpi_cpu_cx_probe(struct acpi_cpu_softc *sc);
static void	acpi_cpu_generic_cx_probe(struct acpi_cpu_softc *sc);
static int	acpi_cpu_cx_cst(struct acpi_cpu_softc *sc);
static void	acpi_cpu_startup(void *arg);
static void	acpi_cpu_startup_cx(struct acpi_cpu_softc *sc);
static void	acpi_cpu_cx_list(struct acpi_cpu_softc *sc);
static void	acpi_cpu_idle(void);
static void	acpi_cpu_notify(ACPI_HANDLE h, UINT32 notify, void *context);
static int	acpi_cpu_quirks(void);
static int	acpi_cpu_usage_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_cpu_set_cx_lowest(struct acpi_cpu_softc *sc, int val);
static int	acpi_cpu_cx_lowest_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_cpu_global_cx_lowest_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t acpi_cpu_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_cpu_probe),
    DEVMETHOD(device_attach,	acpi_cpu_attach),
    DEVMETHOD(device_detach,	bus_generic_detach),
    DEVMETHOD(device_shutdown,	acpi_cpu_shutdown),
    DEVMETHOD(device_suspend,	acpi_cpu_suspend),
    DEVMETHOD(device_resume,	acpi_cpu_resume),

    /* Bus interface */
    DEVMETHOD(bus_add_child,	acpi_cpu_add_child),
    DEVMETHOD(bus_read_ivar,	acpi_cpu_read_ivar),
    DEVMETHOD(bus_get_resource_list, acpi_cpu_get_rlist),
    DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
    DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
    DEVMETHOD(bus_alloc_resource, bus_generic_rl_alloc_resource),
    DEVMETHOD(bus_release_resource, bus_generic_rl_release_resource),
    DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
    DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
    DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
    DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
    DEVMETHOD(bus_teardown_intr, bus_generic_teardown_intr),

    {0, 0}
};

static driver_t acpi_cpu_driver = {
    "cpu",
    acpi_cpu_methods,
    sizeof(struct acpi_cpu_softc),
};

static devclass_t acpi_cpu_devclass;
DRIVER_MODULE(cpu, acpi, acpi_cpu_driver, acpi_cpu_devclass, 0, 0);
MODULE_DEPEND(cpu, acpi, 1, 1, 1);

static int
acpi_cpu_probe(device_t dev)
{
    int			   acpi_id, cpu_id;
    ACPI_BUFFER		   buf;
    ACPI_HANDLE		   handle;
    ACPI_OBJECT		   *obj;
    ACPI_STATUS		   status;

    if (acpi_disabled("cpu") || acpi_get_type(dev) != ACPI_TYPE_PROCESSOR)
	return (ENXIO);

    handle = acpi_get_handle(dev);
    if (cpu_softc == NULL)
	cpu_softc = malloc(sizeof(struct acpi_cpu_softc *) *
	    (mp_maxid + 1), M_TEMP /* XXX */, M_WAITOK | M_ZERO);

    /* Get our Processor object. */
    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiEvaluateObject(handle, NULL, NULL, &buf);
    if (ACPI_FAILURE(status)) {
	device_printf(dev, "probe failed to get Processor obj - %s\n",
		      AcpiFormatException(status));
	return (ENXIO);
    }
    obj = (ACPI_OBJECT *)buf.Pointer;
    if (obj->Type != ACPI_TYPE_PROCESSOR) {
	device_printf(dev, "Processor object has bad type %d\n", obj->Type);
	AcpiOsFree(obj);
	return (ENXIO);
    }

    /*
     * Find the processor associated with our unit.  We could use the
     * ProcId as a key, however, some boxes do not have the same values
     * in their Processor object as the ProcId values in the MADT.
     */
    acpi_id = obj->Processor.ProcId;
    AcpiOsFree(obj);
    if (acpi_pcpu_get_id(device_get_unit(dev), &acpi_id, &cpu_id) != 0)
	return (ENXIO);

    /*
     * Check if we already probed this processor.  We scan the bus twice
     * so it's possible we've already seen this one.
     */
    if (cpu_softc[cpu_id] != NULL)
	return (ENXIO);

    /* Mark this processor as in-use and save our derived id for attach. */
    cpu_softc[cpu_id] = (void *)1;
    acpi_set_private(dev, (void*)(intptr_t)cpu_id);
    device_set_desc(dev, "ACPI CPU");

    return (0);
}

static int
acpi_cpu_attach(device_t dev)
{
    ACPI_BUFFER		   buf;
    ACPI_OBJECT		   arg[4], *obj;
    ACPI_OBJECT_LIST	   arglist;
    struct pcpu		   *pcpu_data;
    struct acpi_cpu_softc *sc;
    struct acpi_softc	  *acpi_sc;
    ACPI_STATUS		   status;
    u_int		   features;
    int			   cpu_id, drv_count, i;
    driver_t 		  **drivers;
    uint32_t		   cap_set[3];

    /* UUID needed by _OSC evaluation */
    static uint8_t cpu_oscuuid[16] = { 0x16, 0xA6, 0x77, 0x40, 0x0C, 0x29,
				       0xBE, 0x47, 0x9E, 0xBD, 0xD8, 0x70,
				       0x58, 0x71, 0x39, 0x53 };

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = device_get_softc(dev);
    sc->cpu_dev = dev;
    sc->cpu_handle = acpi_get_handle(dev);
    cpu_id = (int)(intptr_t)acpi_get_private(dev);
    cpu_softc[cpu_id] = sc;
    pcpu_data = pcpu_find(cpu_id);
    pcpu_data->pc_device = dev;
    sc->cpu_pcpu = pcpu_data;
    cpu_smi_cmd = AcpiGbl_FADT.SmiCommand;
    cpu_cst_cnt = AcpiGbl_FADT.CstControl;

    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiEvaluateObject(sc->cpu_handle, NULL, NULL, &buf);
    if (ACPI_FAILURE(status)) {
	device_printf(dev, "attach failed to get Processor obj - %s\n",
		      AcpiFormatException(status));
	return (ENXIO);
    }
    obj = (ACPI_OBJECT *)buf.Pointer;
    sc->cpu_p_blk = obj->Processor.PblkAddress;
    sc->cpu_p_blk_len = obj->Processor.PblkLength;
    sc->cpu_acpi_id = obj->Processor.ProcId;
    AcpiOsFree(obj);
    ACPI_DEBUG_PRINT((ACPI_DB_INFO, "acpi_cpu%d: P_BLK at %#x/%d\n",
		     device_get_unit(dev), sc->cpu_p_blk, sc->cpu_p_blk_len));

    /*
     * If this is the first cpu we attach, create and initialize the generic
     * resources that will be used by all acpi cpu devices.
     */
    if (device_get_unit(dev) == 0) {
	/* Assume we won't be using generic Cx mode by default */
	cpu_cx_generic = FALSE;

	/* Install hw.acpi.cpu sysctl tree */
	acpi_sc = acpi_device_get_parent_softc(dev);
	sysctl_ctx_init(&cpu_sysctl_ctx);
	cpu_sysctl_tree = SYSCTL_ADD_NODE(&cpu_sysctl_ctx,
	    SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree), OID_AUTO, "cpu",
	    CTLFLAG_RD, 0, "node for CPU children");

	/* Queue post cpu-probing task handler */
	AcpiOsExecute(OSL_NOTIFY_HANDLER, acpi_cpu_startup, NULL);
    }

    /*
     * Before calling any CPU methods, collect child driver feature hints
     * and notify ACPI of them.  We support unified SMP power control
     * so advertise this ourselves.  Note this is not the same as independent
     * SMP control where each CPU can have different settings.
     */
    sc->cpu_features = ACPI_CAP_SMP_SAME | ACPI_CAP_SMP_SAME_C3;
    if (devclass_get_drivers(acpi_cpu_devclass, &drivers, &drv_count) == 0) {
	for (i = 0; i < drv_count; i++) {
	    if (ACPI_GET_FEATURES(drivers[i], &features) == 0)
		sc->cpu_features |= features;
	}
	free(drivers, M_TEMP);
    }

    /*
     * CPU capabilities are specified in
     * Intel Processor Vendor-Specific ACPI Interface Specification.
     */
    if (sc->cpu_features) {
	arglist.Pointer = arg;
	arglist.Count = 4;
	arg[0].Type = ACPI_TYPE_BUFFER;
	arg[0].Buffer.Length = sizeof(cpu_oscuuid);
	arg[0].Buffer.Pointer = cpu_oscuuid;	/* UUID */
	arg[1].Type = ACPI_TYPE_INTEGER;
	arg[1].Integer.Value = 1;		/* revision */
	arg[2].Type = ACPI_TYPE_INTEGER;
	arg[2].Integer.Value = 1;		/* count */
	arg[3].Type = ACPI_TYPE_BUFFER;
	arg[3].Buffer.Length = sizeof(cap_set);	/* Capabilities buffer */
	arg[3].Buffer.Pointer = (uint8_t *)cap_set;
	cap_set[0] = 0;				/* status */
	cap_set[1] = sc->cpu_features;
	status = AcpiEvaluateObject(sc->cpu_handle, "_OSC", &arglist, NULL);
	if (ACPI_SUCCESS(status)) {
	    if (cap_set[0] != 0)
		device_printf(dev, "_OSC returned status %#x\n", cap_set[0]);
	}
	else {
	    arglist.Pointer = arg;
	    arglist.Count = 1;
	    arg[0].Type = ACPI_TYPE_BUFFER;
	    arg[0].Buffer.Length = sizeof(cap_set);
	    arg[0].Buffer.Pointer = (uint8_t *)cap_set;
	    cap_set[0] = 1; /* revision */
	    cap_set[1] = 1; /* number of capabilities integers */
	    cap_set[2] = sc->cpu_features;
	    AcpiEvaluateObject(sc->cpu_handle, "_PDC", &arglist, NULL);
	}
    }

    /* Probe for Cx state support. */
    acpi_cpu_cx_probe(sc);

    return (0);
}

static void
acpi_cpu_postattach(void *unused __unused)
{
    device_t *devices;
    int err;
    int i, n;

    err = devclass_get_devices(acpi_cpu_devclass, &devices, &n);
    if (err != 0) {
	printf("devclass_get_devices(acpi_cpu_devclass) failed\n");
	return;
    }
    for (i = 0; i < n; i++)
	bus_generic_probe(devices[i]);
    for (i = 0; i < n; i++)
	bus_generic_attach(devices[i]);
    free(devices, M_TEMP);
}

SYSINIT(acpi_cpu, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE,
    acpi_cpu_postattach, NULL);

/*
 * Disable any entry to the idle function during suspend and re-enable it
 * during resume.
 */
static int
acpi_cpu_suspend(device_t dev)
{
    int error;

    error = bus_generic_suspend(dev);
    if (error)
	return (error);
    cpu_disable_idle = TRUE;
    return (0);
}

static int
acpi_cpu_resume(device_t dev)
{

    cpu_disable_idle = FALSE;
    return (bus_generic_resume(dev));
}

/*
 * Find the nth present CPU and return its pc_cpuid as well as set the
 * pc_acpi_id from the most reliable source.
 */
static int
acpi_pcpu_get_id(uint32_t idx, uint32_t *acpi_id, uint32_t *cpu_id)
{
    struct pcpu	*pcpu_data;
    uint32_t	 i;

    KASSERT(acpi_id != NULL, ("Null acpi_id"));
    KASSERT(cpu_id != NULL, ("Null cpu_id"));
    CPU_FOREACH(i) {
	pcpu_data = pcpu_find(i);
	KASSERT(pcpu_data != NULL, ("no pcpu data for %d", i));
	if (idx-- == 0) {
	    /*
	     * If pc_acpi_id was not initialized (e.g., a non-APIC UP box)
	     * override it with the value from the ASL.  Otherwise, if the
	     * two don't match, prefer the MADT-derived value.  Finally,
	     * return the pc_cpuid to reference this processor.
	     */
	    if (pcpu_data->pc_acpi_id == 0xffffffff)
		pcpu_data->pc_acpi_id = *acpi_id;
	    else if (pcpu_data->pc_acpi_id != *acpi_id)
		*acpi_id = pcpu_data->pc_acpi_id;
	    *cpu_id = pcpu_data->pc_cpuid;
	    return (0);
	}
    }

    return (ESRCH);
}

static struct resource_list *
acpi_cpu_get_rlist(device_t dev, device_t child)
{
    struct acpi_cpu_device *ad;

    ad = device_get_ivars(child);
    if (ad == NULL)
	return (NULL);
    return (&ad->ad_rl);
}

static device_t
acpi_cpu_add_child(device_t dev, int order, const char *name, int unit)
{
    struct acpi_cpu_device *ad;
    device_t child;

    if ((ad = malloc(sizeof(*ad), M_TEMP, M_NOWAIT | M_ZERO)) == NULL)
	return (NULL);

    resource_list_init(&ad->ad_rl);
    
    child = device_add_child_ordered(dev, order, name, unit);
    if (child != NULL)
	device_set_ivars(child, ad);
    else
	free(ad, M_TEMP);
    return (child);
}

static int
acpi_cpu_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
    struct acpi_cpu_softc *sc;

    sc = device_get_softc(dev);
    switch (index) {
    case ACPI_IVAR_HANDLE:
	*result = (uintptr_t)sc->cpu_handle;
	break;
    case CPU_IVAR_PCPU:
	*result = (uintptr_t)sc->cpu_pcpu;
	break;
    default:
	return (ENOENT);
    }
    return (0);
}

static int
acpi_cpu_shutdown(device_t dev)
{
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Allow children to shutdown first. */
    bus_generic_shutdown(dev);

    /*
     * Disable any entry to the idle function.  There is a small race where
     * an idle thread have passed this check but not gone to sleep.  This
     * is ok since device_shutdown() does not free the softc, otherwise
     * we'd have to be sure all threads were evicted before returning.
     */
    cpu_disable_idle = TRUE;

    return_VALUE (0);
}

static void
acpi_cpu_cx_probe(struct acpi_cpu_softc *sc)
{
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Use initial sleep value of 1 sec. to start with lowest idle state. */
    sc->cpu_prev_sleep = 1000000;
    sc->cpu_cx_lowest = 0;

    /*
     * Check for the ACPI 2.0 _CST sleep states object. If we can't find
     * any, we'll revert to generic FADT/P_BLK Cx control method which will
     * be handled by acpi_cpu_startup. We need to defer to after having
     * probed all the cpus in the system before probing for generic Cx
     * states as we may already have found cpus with valid _CST packages
     */
    if (!cpu_cx_generic && acpi_cpu_cx_cst(sc) != 0) {
	/*
	 * We were unable to find a _CST package for this cpu or there
	 * was an error parsing it. Switch back to generic mode.
	 */
	cpu_cx_generic = TRUE;
	if (bootverbose)
	    device_printf(sc->cpu_dev, "switching to generic Cx mode\n");
    }

    /*
     * TODO: _CSD Package should be checked here.
     */
}

static void
acpi_cpu_generic_cx_probe(struct acpi_cpu_softc *sc)
{
    ACPI_GENERIC_ADDRESS	 gas;
    struct acpi_cx		*cx_ptr;

    sc->cpu_cx_count = 0;
    cx_ptr = sc->cpu_cx_states;

    /* Use initial sleep value of 1 sec. to start with lowest idle state. */
    sc->cpu_prev_sleep = 1000000;

    /* C1 has been required since just after ACPI 1.0 */
    cx_ptr->type = ACPI_STATE_C1;
    cx_ptr->trans_lat = 0;
    cx_ptr++;
    sc->cpu_cx_count++;

    /* 
     * The spec says P_BLK must be 6 bytes long.  However, some systems
     * use it to indicate a fractional set of features present so we
     * take 5 as C2.  Some may also have a value of 7 to indicate
     * another C3 but most use _CST for this (as required) and having
     * "only" C1-C3 is not a hardship.
     */
    if (sc->cpu_p_blk_len < 5)
	return; 

    /* Validate and allocate resources for C2 (P_LVL2). */
    gas.SpaceId = ACPI_ADR_SPACE_SYSTEM_IO;
    gas.BitWidth = 8;
    if (AcpiGbl_FADT.C2Latency <= 100) {
	gas.Address = sc->cpu_p_blk + 4;
	acpi_bus_alloc_gas(sc->cpu_dev, &cx_ptr->res_type, &sc->cpu_rid,
	    &gas, &cx_ptr->p_lvlx, RF_SHAREABLE);
	if (cx_ptr->p_lvlx != NULL) {
	    sc->cpu_rid++;
	    cx_ptr->type = ACPI_STATE_C2;
	    cx_ptr->trans_lat = AcpiGbl_FADT.C2Latency;
	    cx_ptr++;
	    sc->cpu_cx_count++;
	}
    }
    if (sc->cpu_p_blk_len < 6)
	return;

    /* Validate and allocate resources for C3 (P_LVL3). */
    if (AcpiGbl_FADT.C3Latency <= 1000 && !(cpu_quirks & CPU_QUIRK_NO_C3)) {
	gas.Address = sc->cpu_p_blk + 5;
	acpi_bus_alloc_gas(sc->cpu_dev, &cx_ptr->res_type, &sc->cpu_rid, &gas,
	    &cx_ptr->p_lvlx, RF_SHAREABLE);
	if (cx_ptr->p_lvlx != NULL) {
	    sc->cpu_rid++;
	    cx_ptr->type = ACPI_STATE_C3;
	    cx_ptr->trans_lat = AcpiGbl_FADT.C3Latency;
	    cx_ptr++;
	    sc->cpu_cx_count++;
	}
    }
}

/*
 * Parse a _CST package and set up its Cx states.  Since the _CST object
 * can change dynamically, our notify handler may call this function
 * to clean up and probe the new _CST package.
 */
static int
acpi_cpu_cx_cst(struct acpi_cpu_softc *sc)
{
    struct	 acpi_cx *cx_ptr;
    ACPI_STATUS	 status;
    ACPI_BUFFER	 buf;
    ACPI_OBJECT	*top;
    ACPI_OBJECT	*pkg;
    uint32_t	 count;
    int		 i;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiEvaluateObject(sc->cpu_handle, "_CST", NULL, &buf);
    if (ACPI_FAILURE(status))
	return (ENXIO);

    /* _CST is a package with a count and at least one Cx package. */
    top = (ACPI_OBJECT *)buf.Pointer;
    if (!ACPI_PKG_VALID(top, 2) || acpi_PkgInt32(top, 0, &count) != 0) {
	device_printf(sc->cpu_dev, "invalid _CST package\n");
	AcpiOsFree(buf.Pointer);
	return (ENXIO);
    }
    if (count != top->Package.Count - 1) {
	device_printf(sc->cpu_dev, "invalid _CST state count (%d != %d)\n",
	       count, top->Package.Count - 1);
	count = top->Package.Count - 1;
    }
    if (count > MAX_CX_STATES) {
	device_printf(sc->cpu_dev, "_CST has too many states (%d)\n", count);
	count = MAX_CX_STATES;
    }

    /* Set up all valid states. */
    sc->cpu_cx_count = 0;
    cx_ptr = sc->cpu_cx_states;
    for (i = 0; i < count; i++) {
	pkg = &top->Package.Elements[i + 1];
	if (!ACPI_PKG_VALID(pkg, 4) ||
	    acpi_PkgInt32(pkg, 1, &cx_ptr->type) != 0 ||
	    acpi_PkgInt32(pkg, 2, &cx_ptr->trans_lat) != 0 ||
	    acpi_PkgInt32(pkg, 3, &cx_ptr->power) != 0) {

	    device_printf(sc->cpu_dev, "skipping invalid Cx state package\n");
	    continue;
	}

	/* Validate the state to see if we should use it. */
	switch (cx_ptr->type) {
	case ACPI_STATE_C1:
	    sc->cpu_non_c3 = i;
	    cx_ptr++;
	    sc->cpu_cx_count++;
	    continue;
	case ACPI_STATE_C2:
	    if (cx_ptr->trans_lat > 100) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				 "acpi_cpu%d: C2[%d] not available.\n",
				 device_get_unit(sc->cpu_dev), i));
		continue;
	    }
	    sc->cpu_non_c3 = i;
	    break;
	case ACPI_STATE_C3:
	default:
	    if (cx_ptr->trans_lat > 1000 ||
		(cpu_quirks & CPU_QUIRK_NO_C3) != 0) {

		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				 "acpi_cpu%d: C3[%d] not available.\n",
				 device_get_unit(sc->cpu_dev), i));
		continue;
	    }
	    break;
	}

#ifdef notyet
	/* Free up any previous register. */
	if (cx_ptr->p_lvlx != NULL) {
	    bus_release_resource(sc->cpu_dev, 0, 0, cx_ptr->p_lvlx);
	    cx_ptr->p_lvlx = NULL;
	}
#endif

	/* Allocate the control register for C2 or C3. */
	acpi_PkgGas(sc->cpu_dev, pkg, 0, &cx_ptr->res_type, &sc->cpu_rid,
	    &cx_ptr->p_lvlx, RF_SHAREABLE);
	if (cx_ptr->p_lvlx) {
	    sc->cpu_rid++;
	    ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			     "acpi_cpu%d: Got C%d - %d latency\n",
			     device_get_unit(sc->cpu_dev), cx_ptr->type,
			     cx_ptr->trans_lat));
	    cx_ptr++;
	    sc->cpu_cx_count++;
	}
    }
    AcpiOsFree(buf.Pointer);

    return (0);
}

/*
 * Call this *after* all CPUs have been attached.
 */
static void
acpi_cpu_startup(void *arg)
{
    struct acpi_cpu_softc *sc;
    int i;

    /* Get set of CPU devices */
    devclass_get_devices(acpi_cpu_devclass, &cpu_devices, &cpu_ndevices);

    /*
     * Setup any quirks that might necessary now that we have probed
     * all the CPUs
     */
    acpi_cpu_quirks();

    cpu_cx_count = 0;
    if (cpu_cx_generic) {
	/*
	 * We are using generic Cx mode, probe for available Cx states
	 * for all processors.
	 */
	for (i = 0; i < cpu_ndevices; i++) {
	    sc = device_get_softc(cpu_devices[i]);
	    acpi_cpu_generic_cx_probe(sc);
	    if (sc->cpu_cx_count > cpu_cx_count)
		    cpu_cx_count = sc->cpu_cx_count;
	}

	/*
	 * Find the highest Cx state common to all CPUs
	 * in the system, taking quirks into account.
	 */
	for (i = 0; i < cpu_ndevices; i++) {
	    sc = device_get_softc(cpu_devices[i]);
	    if (sc->cpu_cx_count < cpu_cx_count)
		cpu_cx_count = sc->cpu_cx_count;
	}
    } else {
	/*
	 * We are using _CST mode, remove C3 state if necessary.
	 * Update the largest Cx state supported in the global cpu_cx_count.
	 * It will be used in the global Cx sysctl handler.
	 * As we now know for sure that we will be using _CST mode
	 * install our notify handler.
	 */
	for (i = 0; i < cpu_ndevices; i++) {
	    sc = device_get_softc(cpu_devices[i]);
	    if (cpu_quirks & CPU_QUIRK_NO_C3) {
		sc->cpu_cx_count = sc->cpu_non_c3 + 1;
	    }
	    if (sc->cpu_cx_count > cpu_cx_count)
		cpu_cx_count = sc->cpu_cx_count;
	    AcpiInstallNotifyHandler(sc->cpu_handle, ACPI_DEVICE_NOTIFY,
		acpi_cpu_notify, sc);
	}
    }

    /* Perform Cx final initialization. */
    for (i = 0; i < cpu_ndevices; i++) {
	sc = device_get_softc(cpu_devices[i]);
	acpi_cpu_startup_cx(sc);
    }

    /* Add a sysctl handler to handle global Cx lowest setting */
    SYSCTL_ADD_PROC(&cpu_sysctl_ctx, SYSCTL_CHILDREN(cpu_sysctl_tree),
	OID_AUTO, "cx_lowest", CTLTYPE_STRING | CTLFLAG_RW,
	NULL, 0, acpi_cpu_global_cx_lowest_sysctl, "A",
	"Global lowest Cx sleep state to use");

    /* Take over idling from cpu_idle_default(). */
    cpu_cx_lowest = 0;
    cpu_disable_idle = FALSE;
    cpu_idle_hook = acpi_cpu_idle;
}

static void
acpi_cpu_cx_list(struct acpi_cpu_softc *sc)
{
    struct sbuf sb;
    int i;

    /*
     * Set up the list of Cx states
     */
    sc->cpu_non_c3 = 0;
    sbuf_new(&sb, sc->cpu_cx_supported, sizeof(sc->cpu_cx_supported),
	SBUF_FIXEDLEN);
    for (i = 0; i < sc->cpu_cx_count; i++) {
	sbuf_printf(&sb, "C%d/%d ", i + 1, sc->cpu_cx_states[i].trans_lat);
	if (sc->cpu_cx_states[i].type < ACPI_STATE_C3)
	    sc->cpu_non_c3 = i;
    }
    sbuf_trim(&sb);
    sbuf_finish(&sb);
}	

static void
acpi_cpu_startup_cx(struct acpi_cpu_softc *sc)
{
    acpi_cpu_cx_list(sc);
    
    SYSCTL_ADD_STRING(&sc->cpu_sysctl_ctx,
		      SYSCTL_CHILDREN(device_get_sysctl_tree(sc->cpu_dev)),
		      OID_AUTO, "cx_supported", CTLFLAG_RD,
		      sc->cpu_cx_supported, 0,
		      "Cx/microsecond values for supported Cx states");
    SYSCTL_ADD_PROC(&sc->cpu_sysctl_ctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->cpu_dev)),
		    OID_AUTO, "cx_lowest", CTLTYPE_STRING | CTLFLAG_RW,
		    (void *)sc, 0, acpi_cpu_cx_lowest_sysctl, "A",
		    "lowest Cx sleep state to use");
    SYSCTL_ADD_PROC(&sc->cpu_sysctl_ctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->cpu_dev)),
		    OID_AUTO, "cx_usage", CTLTYPE_STRING | CTLFLAG_RD,
		    (void *)sc, 0, acpi_cpu_usage_sysctl, "A",
		    "percent usage for each Cx state");

#ifdef notyet
    /* Signal platform that we can handle _CST notification. */
    if (!cpu_cx_generic && cpu_cst_cnt != 0) {
	ACPI_LOCK(acpi);
	AcpiOsWritePort(cpu_smi_cmd, cpu_cst_cnt, 8);
	ACPI_UNLOCK(acpi);
    }
#endif
}

/*
 * Idle the CPU in the lowest state possible.  This function is called with
 * interrupts disabled.  Note that once it re-enables interrupts, a task
 * switch can occur so do not access shared data (i.e. the softc) after
 * interrupts are re-enabled.
 */
static void
acpi_cpu_idle()
{
    struct	acpi_cpu_softc *sc;
    struct	acpi_cx *cx_next;
    uint32_t	start_time, end_time;
    int		bm_active, cx_next_idx, i;

    /* If disabled, return immediately. */
    if (cpu_disable_idle) {
	ACPI_ENABLE_IRQS();
	return;
    }

    /*
     * Look up our CPU id to get our softc.  If it's NULL, we'll use C1
     * since there is no ACPI processor object for this CPU.  This occurs
     * for logical CPUs in the HTT case.
     */
    sc = cpu_softc[PCPU_GET(cpuid)];
    if (sc == NULL) {
	acpi_cpu_c1();
	return;
    }

    /* Find the lowest state that has small enough latency. */
    cx_next_idx = 0;
    for (i = sc->cpu_cx_lowest; i >= 0; i--) {
	if (sc->cpu_cx_states[i].trans_lat * 3 <= sc->cpu_prev_sleep) {
	    cx_next_idx = i;
	    break;
	}
    }

    /*
     * Check for bus master activity.  If there was activity, clear
     * the bit and use the lowest non-C3 state.  Note that the USB
     * driver polling for new devices keeps this bit set all the
     * time if USB is loaded.
     */
    if ((cpu_quirks & CPU_QUIRK_NO_BM_CTRL) == 0) {
	AcpiReadBitRegister(ACPI_BITREG_BUS_MASTER_STATUS, &bm_active);
	if (bm_active != 0) {
	    AcpiWriteBitRegister(ACPI_BITREG_BUS_MASTER_STATUS, 1);
	    cx_next_idx = min(cx_next_idx, sc->cpu_non_c3);
	}
    }

    /* Select the next state and update statistics. */
    cx_next = &sc->cpu_cx_states[cx_next_idx];
    sc->cpu_cx_stats[cx_next_idx]++;
    KASSERT(cx_next->type != ACPI_STATE_C0, ("acpi_cpu_idle: C0 sleep"));

    /*
     * Execute HLT (or equivalent) and wait for an interrupt.  We can't
     * precisely calculate the time spent in C1 since the place we wake up
     * is an ISR.  Assume we slept no more then half of quantum.
     */
    if (cx_next->type == ACPI_STATE_C1) {
	AcpiHwRead(&start_time, &AcpiGbl_FADT.XPmTimerBlock);
	acpi_cpu_c1();
	AcpiHwRead(&end_time, &AcpiGbl_FADT.XPmTimerBlock);
        end_time = acpi_TimerDelta(end_time, start_time);
	sc->cpu_prev_sleep = (sc->cpu_prev_sleep * 3 +
	    min(PM_USEC(end_time), 500000 / hz)) / 4;
	return;
    }

    /*
     * For C3, disable bus master arbitration and enable bus master wake
     * if BM control is available, otherwise flush the CPU cache.
     */
    if (cx_next->type == ACPI_STATE_C3) {
	if ((cpu_quirks & CPU_QUIRK_NO_BM_CTRL) == 0) {
	    AcpiWriteBitRegister(ACPI_BITREG_ARB_DISABLE, 1);
	    AcpiWriteBitRegister(ACPI_BITREG_BUS_MASTER_RLD, 1);
	} else
	    ACPI_FLUSH_CPU_CACHE();
    }

    /*
     * Read from P_LVLx to enter C2(+), checking time spent asleep.
     * Use the ACPI timer for measuring sleep time.  Since we need to
     * get the time very close to the CPU start/stop clock logic, this
     * is the only reliable time source.
     */
    AcpiHwRead(&start_time, &AcpiGbl_FADT.XPmTimerBlock);
    CPU_GET_REG(cx_next->p_lvlx, 1);

    /*
     * Read the end time twice.  Since it may take an arbitrary time
     * to enter the idle state, the first read may be executed before
     * the processor has stopped.  Doing it again provides enough
     * margin that we are certain to have a correct value.
     */
    AcpiHwRead(&end_time, &AcpiGbl_FADT.XPmTimerBlock);
    AcpiHwRead(&end_time, &AcpiGbl_FADT.XPmTimerBlock);

    /* Enable bus master arbitration and disable bus master wakeup. */
    if (cx_next->type == ACPI_STATE_C3 &&
	(cpu_quirks & CPU_QUIRK_NO_BM_CTRL) == 0) {
	AcpiWriteBitRegister(ACPI_BITREG_ARB_DISABLE, 0);
	AcpiWriteBitRegister(ACPI_BITREG_BUS_MASTER_RLD, 0);
    }
    ACPI_ENABLE_IRQS();

    /* Find the actual time asleep in microseconds. */
    end_time = acpi_TimerDelta(end_time, start_time);
    sc->cpu_prev_sleep = (sc->cpu_prev_sleep * 3 + PM_USEC(end_time)) / 4;
}

/*
 * Re-evaluate the _CST object when we are notified that it changed.
 *
 * XXX Re-evaluation disabled until locking is done.
 */
static void
acpi_cpu_notify(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_cpu_softc *sc = (struct acpi_cpu_softc *)context;
    struct acpi_cpu_softc *isc;
    int i;
    
    if (notify != ACPI_NOTIFY_CX_STATES)
	return;

    /* Update the list of Cx states. */
    acpi_cpu_cx_cst(sc);
    acpi_cpu_cx_list(sc);

    /* Update the new lowest useable Cx state for all CPUs. */
    ACPI_SERIAL_BEGIN(cpu);
    cpu_cx_count = 0;
    for (i = 0; i < cpu_ndevices; i++) {
	isc = device_get_softc(cpu_devices[i]);
	if (isc->cpu_cx_count > cpu_cx_count)
	    cpu_cx_count = isc->cpu_cx_count;
    }
    if (sc->cpu_cx_lowest < cpu_cx_lowest)
	acpi_cpu_set_cx_lowest(sc, min(cpu_cx_lowest, sc->cpu_cx_count - 1));
    ACPI_SERIAL_END(cpu);
}

static int
acpi_cpu_quirks(void)
{
    device_t acpi_dev;
    uint32_t val;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /*
     * Bus mastering arbitration control is needed to keep caches coherent
     * while sleeping in C3.  If it's not present but a working flush cache
     * instruction is present, flush the caches before entering C3 instead.
     * Otherwise, just disable C3 completely.
     */
    if (AcpiGbl_FADT.Pm2ControlBlock == 0 ||
	AcpiGbl_FADT.Pm2ControlLength == 0) {
	if ((AcpiGbl_FADT.Flags & ACPI_FADT_WBINVD) &&
	    (AcpiGbl_FADT.Flags & ACPI_FADT_WBINVD_FLUSH) == 0) {
	    cpu_quirks |= CPU_QUIRK_NO_BM_CTRL;
	    ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		"acpi_cpu: no BM control, using flush cache method\n"));
	} else {
	    cpu_quirks |= CPU_QUIRK_NO_C3;
	    ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		"acpi_cpu: no BM control, C3 not available\n"));
	}
    }

    /*
     * If we are using generic Cx mode, C3 on multiple CPUs requires using
     * the expensive flush cache instruction.
     */
    if (cpu_cx_generic && mp_ncpus > 1) {
	cpu_quirks |= CPU_QUIRK_NO_BM_CTRL;
	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
	    "acpi_cpu: SMP, using flush cache mode for C3\n"));
    }

    /* Look for various quirks of the PIIX4 part. */
    acpi_dev = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_82371AB_3);
    if (acpi_dev != NULL) {
	switch (pci_get_revid(acpi_dev)) {
	/*
	 * Disable C3 support for all PIIX4 chipsets.  Some of these parts
	 * do not report the BMIDE status to the BM status register and
	 * others have a livelock bug if Type-F DMA is enabled.  Linux
	 * works around the BMIDE bug by reading the BM status directly
	 * but we take the simpler approach of disabling C3 for these
	 * parts.
	 *
	 * See erratum #18 ("C3 Power State/BMIDE and Type-F DMA
	 * Livelock") from the January 2002 PIIX4 specification update.
	 * Applies to all PIIX4 models.
	 *
	 * Also, make sure that all interrupts cause a "Stop Break"
	 * event to exit from C2 state.
	 * Also, BRLD_EN_BM (ACPI_BITREG_BUS_MASTER_RLD in ACPI-speak)
	 * should be set to zero, otherwise it causes C2 to short-sleep.
	 * PIIX4 doesn't properly support C3 and bus master activity
	 * need not break out of C2.
	 */
	case PCI_REVISION_A_STEP:
	case PCI_REVISION_B_STEP:
	case PCI_REVISION_4E:
	case PCI_REVISION_4M:
	    cpu_quirks |= CPU_QUIRK_NO_C3;
	    ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		"acpi_cpu: working around PIIX4 bug, disabling C3\n"));

	    val = pci_read_config(acpi_dev, PIIX4_DEVACTB_REG, 4);
	    if ((val & PIIX4_STOP_BREAK_MASK) != PIIX4_STOP_BREAK_MASK) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		    "acpi_cpu: PIIX4: enabling IRQs to generate Stop Break\n"));
	    	val |= PIIX4_STOP_BREAK_MASK;
		pci_write_config(acpi_dev, PIIX4_DEVACTB_REG, val, 4);
	    }
	    AcpiReadBitRegister(ACPI_BITREG_BUS_MASTER_RLD, &val);
	    if (val) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		    "acpi_cpu: PIIX4: reset BRLD_EN_BM\n"));
		AcpiWriteBitRegister(ACPI_BITREG_BUS_MASTER_RLD, 0);
	    }
	    break;
	default:
	    break;
	}
    }

    return (0);
}

static int
acpi_cpu_usage_sysctl(SYSCTL_HANDLER_ARGS)
{
    struct acpi_cpu_softc *sc;
    struct sbuf	 sb;
    char	 buf[128];
    int		 i;
    uintmax_t	 fract, sum, whole;

    sc = (struct acpi_cpu_softc *) arg1;
    sum = 0;
    for (i = 0; i < sc->cpu_cx_count; i++)
	sum += sc->cpu_cx_stats[i];
    sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
    for (i = 0; i < sc->cpu_cx_count; i++) {
	if (sum > 0) {
	    whole = (uintmax_t)sc->cpu_cx_stats[i] * 100;
	    fract = (whole % sum) * 100;
	    sbuf_printf(&sb, "%u.%02u%% ", (u_int)(whole / sum),
		(u_int)(fract / sum));
	} else
	    sbuf_printf(&sb, "0.00%% ");
    }
    sbuf_printf(&sb, "last %dus", sc->cpu_prev_sleep);
    sbuf_trim(&sb);
    sbuf_finish(&sb);
    sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
    sbuf_delete(&sb);

    return (0);
}

static int
acpi_cpu_set_cx_lowest(struct acpi_cpu_softc *sc, int val)
{
    int i;

    ACPI_SERIAL_ASSERT(cpu);
    sc->cpu_cx_lowest = val;

    /* If not disabling, cache the new lowest non-C3 state. */
    sc->cpu_non_c3 = 0;
    for (i = sc->cpu_cx_lowest; i >= 0; i--) {
	if (sc->cpu_cx_states[i].type < ACPI_STATE_C3) {
	    sc->cpu_non_c3 = i;
	    break;
	}
    }

    /* Reset the statistics counters. */
    bzero(sc->cpu_cx_stats, sizeof(sc->cpu_cx_stats));
    return (0);
}

static int
acpi_cpu_cx_lowest_sysctl(SYSCTL_HANDLER_ARGS)
{
    struct	 acpi_cpu_softc *sc;
    char	 state[8];
    int		 val, error;

    sc = (struct acpi_cpu_softc *) arg1;
    snprintf(state, sizeof(state), "C%d", sc->cpu_cx_lowest + 1);
    error = sysctl_handle_string(oidp, state, sizeof(state), req);
    if (error != 0 || req->newptr == NULL)
	return (error);
    if (strlen(state) < 2 || toupper(state[0]) != 'C')
	return (EINVAL);
    val = (int) strtol(state + 1, NULL, 10) - 1;
    if (val < 0 || val > sc->cpu_cx_count - 1)
	return (EINVAL);

    ACPI_SERIAL_BEGIN(cpu);
    acpi_cpu_set_cx_lowest(sc, val);
    ACPI_SERIAL_END(cpu);

    return (0);
}

static int
acpi_cpu_global_cx_lowest_sysctl(SYSCTL_HANDLER_ARGS)
{
    struct	acpi_cpu_softc *sc;
    char	state[8];
    int		val, error, i;

    snprintf(state, sizeof(state), "C%d", cpu_cx_lowest + 1);
    error = sysctl_handle_string(oidp, state, sizeof(state), req);
    if (error != 0 || req->newptr == NULL)
	return (error);
    if (strlen(state) < 2 || toupper(state[0]) != 'C')
	return (EINVAL);
    val = (int) strtol(state + 1, NULL, 10) - 1;
    if (val < 0 || val > cpu_cx_count - 1)
	return (EINVAL);
    cpu_cx_lowest = val;

    /* Update the new lowest useable Cx state for all CPUs. */
    ACPI_SERIAL_BEGIN(cpu);
    for (i = 0; i < cpu_ndevices; i++) {
	sc = device_get_softc(cpu_devices[i]);
	acpi_cpu_set_cx_lowest(sc, min(val, sc->cpu_cx_count - 1));
    }
    ACPI_SERIAL_END(cpu);

    return (0);
}
