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

#include "acpi.h"
#include <dev/acpica/acpivar.h>

/*
 * Support for ACPI Processor devices, including ACPI 2.0 throttling
 * and C[1-3] sleep states.
 *
 * TODO: implement scans of all CPUs to be sure all Cx states are
 * equivalent.
 */

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_PROCESSOR
ACPI_MODULE_NAME("PROCESSOR")

struct acpi_cx {
    struct resource	*p_lvlx;	/* Register to read to enter state. */
    uint32_t		 type;		/* C1-3 (C4 and up treated as C3). */
    uint32_t		 trans_lat;	/* Transition latency (usec). */
    uint32_t		 power;		/* Power consumed (mW). */
};
#define MAX_CX_STATES	 8

struct acpi_cpu_softc {
    device_t		 cpu_dev;
    ACPI_HANDLE		 cpu_handle;
    struct pcpu		*cpu_pcpu;
    uint32_t		 cpu_acpi_id;	/* ACPI processor id */
    uint32_t		 cpu_p_blk;	/* ACPI P_BLK location */
    uint32_t		 cpu_p_blk_len;	/* P_BLK length (must be 6). */
    struct resource	*cpu_p_cnt;	/* Throttling control register */
    struct acpi_cx	 cpu_cx_states[MAX_CX_STATES];
    int			 cpu_cx_count;	/* Number of valid Cx states. */
    int			 cpu_prev_sleep;/* Last idle sleep duration. */
};

struct acpi_cpu_device {
    struct resource_list        ad_rl;
};

#define CPU_GET_REG(reg, width) 					\
    (bus_space_read_ ## width(rman_get_bustag((reg)), 			\
		      rman_get_bushandle((reg)), 0))
#define CPU_SET_REG(reg, width, val)					\
    (bus_space_write_ ## width(rman_get_bustag((reg)), 			\
		       rman_get_bushandle((reg)), 0, (val)))

/*
 * Speeds are stored in counts, from 1 to CPU_MAX_SPEED, and
 * reported to the user in tenths of a percent.
 */
static uint32_t		 cpu_duty_offset;
static uint32_t		 cpu_duty_width;
#define CPU_MAX_SPEED		(1 << cpu_duty_width)
#define CPU_SPEED_PERCENT(x)	((1000 * (x)) / CPU_MAX_SPEED)
#define CPU_SPEED_PRINTABLE(x)	(CPU_SPEED_PERCENT(x) / 10),	\
				(CPU_SPEED_PERCENT(x) % 10)
#define CPU_P_CNT_THT_EN (1<<4)
#define PM_USEC(x)	 ((x) >> 2)	/* ~4 clocks per usec (3.57955 Mhz) */

#define ACPI_CPU_NOTIFY_PERF_STATES	0x80	/* _PSS changed. */
#define ACPI_CPU_NOTIFY_CX_STATES	0x81	/* _CST changed. */

#define CPU_QUIRK_NO_C3		(1<<0)	/* C3-type states are not usable. */
#define CPU_QUIRK_NO_THROTTLE	(1<<1)	/* Throttling is not usable. */
#define CPU_QUIRK_NO_BM_CTRL	(1<<2)	/* No bus mastering control. */

#define PCI_VENDOR_INTEL	0x8086
#define PCI_DEVICE_82371AB_3	0x7113	/* PIIX4 chipset for quirks. */
#define PCI_REVISION_A_STEP	0
#define PCI_REVISION_B_STEP	1
#define PCI_REVISION_4E		2
#define PCI_REVISION_4M		3

/* Platform hardware resource information. */
static uint32_t		 cpu_smi_cmd;	/* Value to write to SMI_CMD. */
static uint8_t		 cpu_pstate_cnt;/* Register to take over throttling. */
static uint8_t		 cpu_cst_cnt;	/* Indicate we are _CST aware. */
static int		 cpu_rid;	/* Driver-wide resource id. */
static int		 cpu_quirks;	/* Indicate any hardware bugs. */

/* Runtime state. */
static int		 cpu_cx_count;	/* Number of valid states */
static int		 cpu_non_c3;	/* Index of lowest non-C3 state. */
static u_int		 cpu_cx_stats[MAX_CX_STATES];/* Cx usage history. */

/* Values for sysctl. */
static struct sysctl_ctx_list acpi_cpu_sysctl_ctx;
static struct sysctl_oid *acpi_cpu_sysctl_tree;
static uint32_t		 cpu_throttle_state;
static uint32_t		 cpu_throttle_max;
static int		 cpu_cx_lowest;
static char 		 cpu_cx_supported[64];

static device_t		*cpu_devices;
static int		 cpu_ndevices;
static struct acpi_cpu_softc **cpu_softc;
ACPI_SERIAL_DECL(cpu, "ACPI CPU");

static int	acpi_cpu_probe(device_t dev);
static int	acpi_cpu_attach(device_t dev);
static int	acpi_pcpu_get_id(uint32_t idx, uint32_t *acpi_id,
		    uint32_t *cpu_id);
static struct resource_list *acpi_cpu_get_rlist(device_t dev, device_t child);
static device_t	acpi_cpu_add_child(device_t dev, int order, const char *name,
		    int unit);
static int	acpi_cpu_read_ivar(device_t dev, device_t child, int index,
		    uintptr_t *result);
static int	acpi_cpu_shutdown(device_t dev);
static int	acpi_cpu_throttle_probe(struct acpi_cpu_softc *sc);
static int	acpi_cpu_cx_probe(struct acpi_cpu_softc *sc);
static int	acpi_cpu_cx_cst(struct acpi_cpu_softc *sc);
static void	acpi_cpu_startup(void *arg);
static void	acpi_cpu_startup_throttling(void);
static void	acpi_cpu_startup_cx(void);
static void	acpi_cpu_throttle_set(uint32_t speed);
static void	acpi_cpu_idle(void);
static void	acpi_cpu_notify(ACPI_HANDLE h, UINT32 notify, void *context);
static int	acpi_cpu_quirks(struct acpi_cpu_softc *sc);
static int	acpi_cpu_throttle_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_cpu_usage_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_cpu_cx_lowest_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t acpi_cpu_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_cpu_probe),
    DEVMETHOD(device_attach,	acpi_cpu_attach),
    DEVMETHOD(device_detach,	bus_generic_detach),
    DEVMETHOD(device_shutdown,	acpi_cpu_shutdown),
    DEVMETHOD(device_suspend,	bus_generic_suspend),
    DEVMETHOD(device_resume,	bus_generic_resume),

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

static devclass_t cpu_devclass;
DRIVER_MODULE(cpu, acpi, acpi_cpu_driver, cpu_devclass, 0, 0);
MODULE_DEPEND(cpu, acpi, 1, 1, 1);

static int
acpi_cpu_probe(device_t dev)
{
    int			   acpi_id, cpu_id, cx_count;
    ACPI_BUFFER		   buf;
    ACPI_HANDLE		   handle;
    char		   msg[32];
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

    /* Get a count of Cx states for our device string. */
    cx_count = 0;
    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiEvaluateObject(handle, "_CST", NULL, &buf);
    if (ACPI_SUCCESS(status)) {
	obj = (ACPI_OBJECT *)buf.Pointer;
	if (ACPI_PKG_VALID(obj, 2))
	    acpi_PkgInt32(obj, 0, &cx_count);
	AcpiOsFree(obj);
    } else {
	if (AcpiGbl_FADT->Plvl2Lat <= 100)
	    cx_count++;
	if (AcpiGbl_FADT->Plvl3Lat <= 1000)
	    cx_count++;
	if (cx_count > 0)
	    cx_count++;
    }
    if (cx_count > 0)
	snprintf(msg, sizeof(msg), "ACPI CPU (%d Cx states)", cx_count);
    else
	strlcpy(msg, "ACPI CPU", sizeof(msg));
    device_set_desc_copy(dev, msg);

    /* Mark this processor as in-use and save our derived id for attach. */
    cpu_softc[cpu_id] = (void *)1;
    acpi_set_magic(dev, cpu_id);

    return (0);
}

static int
acpi_cpu_attach(device_t dev)
{
    ACPI_BUFFER		   buf;
    ACPI_OBJECT		   *obj;
    struct pcpu		   *pcpu_data;
    struct acpi_cpu_softc *sc;
    struct acpi_softc	  *acpi_sc;
    ACPI_STATUS		   status;
    int			   cx_ret, cpu_id, thr_ret;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = device_get_softc(dev);
    sc->cpu_dev = dev;
    sc->cpu_handle = acpi_get_handle(dev);
    cpu_id = acpi_get_magic(dev);
    cpu_softc[cpu_id] = sc;
    pcpu_data = pcpu_find(cpu_id);
    pcpu_data->pc_device = dev;
    sc->cpu_pcpu = pcpu_data;

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

    acpi_sc = acpi_device_get_parent_softc(dev);
    sysctl_ctx_init(&acpi_cpu_sysctl_ctx);
    acpi_cpu_sysctl_tree = SYSCTL_ADD_NODE(&acpi_cpu_sysctl_ctx,
	SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree), OID_AUTO, "cpu",
	CTLFLAG_RD, 0, "");

    /*
     * Probe for throttling and Cx state support.
     * If none of these is present, free up unused resources.
     */
    thr_ret = acpi_cpu_throttle_probe(sc);
    cx_ret = acpi_cpu_cx_probe(sc);
    if (thr_ret == 0 || cx_ret == 0) {
	status = AcpiInstallNotifyHandler(sc->cpu_handle, ACPI_DEVICE_NOTIFY,
					  acpi_cpu_notify, sc);
	if (device_get_unit(dev) == 0)
	    AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_cpu_startup, NULL);
    } else {
	sysctl_ctx_free(&acpi_cpu_sysctl_ctx);
    }

    /* Call identify and then probe/attach for cpu child drivers. */
    bus_generic_probe(dev);
    bus_generic_attach(dev);

    return (0);
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
    for (i = 0; i <= mp_maxid; i++) {
	if (CPU_ABSENT(i))
	    continue;
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
    struct acpi_cpu_device  *ad;
    device_t            child;

    if ((ad = malloc(sizeof(*ad), M_TEMP, M_NOWAIT | M_ZERO)) == NULL)
        return (NULL);

    resource_list_init(&ad->ad_rl);
    
    child = device_add_child_ordered(dev, order, name, unit);
    if (child != NULL)
        device_set_ivars(child, ad);
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

    /* Disable any entry to the idle function. */
    cpu_cx_count = 0;

    /* Signal and wait for all processors to exit acpi_cpu_idle(). */
    smp_rendezvous(NULL, NULL, NULL, NULL);

    return_VALUE (0);
}

static int
acpi_cpu_throttle_probe(struct acpi_cpu_softc *sc)
{
    uint32_t		 duty_end;
    ACPI_BUFFER		 buf;
    ACPI_OBJECT		 obj;
    ACPI_GENERIC_ADDRESS gas;
    ACPI_STATUS		 status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Get throttling parameters from the FADT.  0 means not supported. */
    if (device_get_unit(sc->cpu_dev) == 0) {
	cpu_smi_cmd = AcpiGbl_FADT->SmiCmd;
	cpu_pstate_cnt = AcpiGbl_FADT->PstateCnt;
	cpu_cst_cnt = AcpiGbl_FADT->CstCnt;
	cpu_duty_offset = AcpiGbl_FADT->DutyOffset;
	cpu_duty_width = AcpiGbl_FADT->DutyWidth;
    }
    if (cpu_duty_width == 0 || (cpu_quirks & CPU_QUIRK_NO_THROTTLE) != 0)
	return (ENXIO);

    /* Validate the duty offset/width. */
    duty_end = cpu_duty_offset + cpu_duty_width - 1;
    if (duty_end > 31) {
	device_printf(sc->cpu_dev, "CLK_VAL field overflows P_CNT register\n");
	return (ENXIO);
    }
    if (cpu_duty_offset <= 4 && duty_end >= 4) {
	device_printf(sc->cpu_dev, "CLK_VAL field overlaps THT_EN bit\n");
	return (ENXIO);
    }

    /*
     * If not present, fall back to using the processor's P_BLK to find
     * the P_CNT register.
     *
     * Note that some systems seem to duplicate the P_BLK pointer
     * across multiple CPUs, so not getting the resource is not fatal.
     */
    buf.Pointer = &obj;
    buf.Length = sizeof(obj);
    status = AcpiEvaluateObject(sc->cpu_handle, "_PTC", NULL, &buf);
    if (ACPI_SUCCESS(status)) {
	if (obj.Buffer.Length < sizeof(ACPI_GENERIC_ADDRESS) + 3) {
	    device_printf(sc->cpu_dev, "_PTC buffer too small\n");
	    return (ENXIO);
	}
	memcpy(&gas, obj.Buffer.Pointer + 3, sizeof(gas));
	sc->cpu_p_cnt = acpi_bus_alloc_gas(sc->cpu_dev, &cpu_rid, &gas);
	if (sc->cpu_p_cnt != NULL) {
	    ACPI_DEBUG_PRINT((ACPI_DB_INFO, "acpi_cpu%d: P_CNT from _PTC\n",
			     device_get_unit(sc->cpu_dev)));
	}
    }

    /* If _PTC not present or other failure, try the P_BLK. */
    if (sc->cpu_p_cnt == NULL) {
	/* 
	 * The spec says P_BLK must be 6 bytes long.  However, some
	 * systems use it to indicate a fractional set of features
	 * present so we take anything >= 4.
	 */
	if (sc->cpu_p_blk_len < 4)
	    return (ENXIO);
	gas.Address = sc->cpu_p_blk;
	gas.AddressSpaceId = ACPI_ADR_SPACE_SYSTEM_IO;
	gas.RegisterBitWidth = 32;
	sc->cpu_p_cnt = acpi_bus_alloc_gas(sc->cpu_dev, &cpu_rid, &gas);
	if (sc->cpu_p_cnt != NULL) {
	    ACPI_DEBUG_PRINT((ACPI_DB_INFO, "acpi_cpu%d: P_CNT from P_BLK\n",
			     device_get_unit(sc->cpu_dev)));
	} else {
	    device_printf(sc->cpu_dev, "Failed to attach throttling P_CNT\n");
	    return (ENXIO);
	}
    }
    cpu_rid++;

    return (0);
}

static int
acpi_cpu_cx_probe(struct acpi_cpu_softc *sc)
{
    ACPI_GENERIC_ADDRESS gas;
    struct acpi_cx	*cx_ptr;
    int			 error;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /*
     * Bus mastering arbitration control is needed to keep caches coherent
     * while sleeping in C3.  If it's not present but a working flush cache
     * instruction is present, flush the caches before entering C3 instead.
     * Otherwise, just disable C3 completely.
     */
    if (AcpiGbl_FADT->V1_Pm2CntBlk == 0 || AcpiGbl_FADT->Pm2CntLen == 0) {
	if (AcpiGbl_FADT->WbInvd && AcpiGbl_FADT->WbInvdFlush == 0) {
	    cpu_quirks |= CPU_QUIRK_NO_BM_CTRL;
	    ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		"acpi_cpu%d: no BM control, using flush cache method\n",
		device_get_unit(sc->cpu_dev)));
	} else {
	    cpu_quirks |= CPU_QUIRK_NO_C3;
	    ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		"acpi_cpu%d: no BM control, C3 not available\n",
		device_get_unit(sc->cpu_dev)));
	}
    }

    /*
     * First, check for the ACPI 2.0 _CST sleep states object.
     * If not usable, fall back to the P_BLK's P_LVL2 and P_LVL3.
     */
    sc->cpu_cx_count = 0;
    error = acpi_cpu_cx_cst(sc);
    if (error != 0) {
	cx_ptr = sc->cpu_cx_states;

	/* C1 has been required since just after ACPI 1.0 */
	cx_ptr->type = ACPI_STATE_C1;
	cx_ptr->trans_lat = 0;
	cpu_non_c3 = 0;
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
	    goto done;

	/* Validate and allocate resources for C2 (P_LVL2). */
	gas.AddressSpaceId = ACPI_ADR_SPACE_SYSTEM_IO;
	gas.RegisterBitWidth = 8;
	if (AcpiGbl_FADT->Plvl2Lat <= 100) {
	    gas.Address = sc->cpu_p_blk + 4;
	    cx_ptr->p_lvlx = acpi_bus_alloc_gas(sc->cpu_dev, &cpu_rid, &gas);
	    if (cx_ptr->p_lvlx != NULL) {
		cpu_rid++;
		cx_ptr->type = ACPI_STATE_C2;
		cx_ptr->trans_lat = AcpiGbl_FADT->Plvl2Lat;
		cpu_non_c3 = 1;
		cx_ptr++;
		sc->cpu_cx_count++;
	    }
	}
	if (sc->cpu_p_blk_len < 6)
	    goto done;

	/* Validate and allocate resources for C3 (P_LVL3). */
	if (AcpiGbl_FADT->Plvl3Lat <= 1000 &&
	    (cpu_quirks & CPU_QUIRK_NO_C3) == 0) {

	    gas.Address = sc->cpu_p_blk + 5;
	    cx_ptr->p_lvlx = acpi_bus_alloc_gas(sc->cpu_dev, &cpu_rid, &gas);
	    if (cx_ptr->p_lvlx != NULL) {
		cpu_rid++;
		cx_ptr->type = ACPI_STATE_C3;
		cx_ptr->trans_lat = AcpiGbl_FADT->Plvl3Lat;
		cx_ptr++;
		sc->cpu_cx_count++;
	    }
	}
    }

done:
    /* If no valid registers were found, don't attach. */
    if (sc->cpu_cx_count == 0)
	return (ENXIO);

    /* Use initial sleep value of 1 sec. to start with lowest idle state. */
    sc->cpu_prev_sleep = 1000000;

    return (0);
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
	device_printf(sc->cpu_dev, "Invalid _CST package\n");
	AcpiOsFree(buf.Pointer);
	return (ENXIO);
    }
    if (count != top->Package.Count - 1) {
	device_printf(sc->cpu_dev, "Invalid _CST state count (%d != %d)\n",
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

	    device_printf(sc->cpu_dev, "Skipping invalid Cx state package\n");
	    continue;
	}

	/* Validate the state to see if we should use it. */
	switch (cx_ptr->type) {
	case ACPI_STATE_C1:
	    cpu_non_c3 = i;
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
	    cpu_non_c3 = i;
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
	acpi_PkgGas(sc->cpu_dev, pkg, 0, &cpu_rid, &cx_ptr->p_lvlx);
	if (cx_ptr->p_lvlx != NULL) {
	    cpu_rid++;
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
    int count, i;

    /* Get set of CPU devices */
    devclass_get_devices(cpu_devclass, &cpu_devices, &cpu_ndevices);

    /* Check for quirks via the first CPU device. */
    sc = device_get_softc(cpu_devices[0]);
    acpi_cpu_quirks(sc);

    /*
     * Make sure all the processors' Cx counts match.  We should probably
     * also check the contents of each.  However, no known systems have
     * non-matching Cx counts so we'll deal with this later.
     */
    count = MAX_CX_STATES;
    for (i = 0; i < cpu_ndevices; i++) {
	sc = device_get_softc(cpu_devices[i]);
	count = min(sc->cpu_cx_count, count);
    }
    cpu_cx_count = count;

    /* Perform throttling and Cx final initialization. */
    sc = device_get_softc(cpu_devices[0]);
    if (sc->cpu_p_cnt != NULL)
	acpi_cpu_startup_throttling();
    if (cpu_cx_count > 0)
	acpi_cpu_startup_cx();
}

/*
 * Takes the ACPI lock to avoid fighting anyone over the SMI command
 * port.
 */
static void
acpi_cpu_startup_throttling()
{

    /* If throttling is not usable, don't initialize it. */
    if (cpu_quirks & CPU_QUIRK_NO_THROTTLE)
	return;

    /* Initialise throttling states */
    cpu_throttle_max = CPU_MAX_SPEED;
    cpu_throttle_state = CPU_MAX_SPEED;

    SYSCTL_ADD_INT(&acpi_cpu_sysctl_ctx,
		   SYSCTL_CHILDREN(acpi_cpu_sysctl_tree),
		   OID_AUTO, "throttle_max", CTLFLAG_RD,
		   &cpu_throttle_max, 0, "maximum CPU speed");
    SYSCTL_ADD_PROC(&acpi_cpu_sysctl_ctx,
		    SYSCTL_CHILDREN(acpi_cpu_sysctl_tree),
		    OID_AUTO, "throttle_state",
		    CTLTYPE_INT | CTLFLAG_RW, &cpu_throttle_state,
		    0, acpi_cpu_throttle_sysctl, "I", "current CPU speed");

    /* If ACPI 2.0+, signal platform that we are taking over throttling. */
    if (cpu_pstate_cnt != 0) {
	ACPI_LOCK(acpi);
	AcpiOsWritePort(cpu_smi_cmd, cpu_pstate_cnt, 8);
	ACPI_UNLOCK(acpi);
    }

    /* Set initial speed to maximum. */
    ACPI_SERIAL_BEGIN(cpu);
    acpi_cpu_throttle_set(cpu_throttle_max);
    ACPI_SERIAL_END(cpu);

    printf("acpi_cpu: throttling enabled, %d steps (100%% to %d.%d%%), "
	   "currently %d.%d%%\n", CPU_MAX_SPEED, CPU_SPEED_PRINTABLE(1),
	   CPU_SPEED_PRINTABLE(cpu_throttle_state));
}

static void
acpi_cpu_startup_cx()
{
    struct acpi_cpu_softc *sc;
    struct sbuf sb;
    int i;

    /*
     * Set up the list of Cx states, eliminating C3 states by truncating
     * cpu_cx_count if quirks indicate C3 is not usable.
     */
    sc = device_get_softc(cpu_devices[0]);
    sbuf_new(&sb, cpu_cx_supported, sizeof(cpu_cx_supported), SBUF_FIXEDLEN);
    for (i = 0; i < cpu_cx_count; i++) {
	if ((cpu_quirks & CPU_QUIRK_NO_C3) == 0 ||
	    sc->cpu_cx_states[i].type != ACPI_STATE_C3)
	    sbuf_printf(&sb, "C%d/%d ", i + 1, sc->cpu_cx_states[i].trans_lat);
	else
	    cpu_cx_count = i;
    }
    sbuf_trim(&sb);
    sbuf_finish(&sb);
    SYSCTL_ADD_STRING(&acpi_cpu_sysctl_ctx,
		      SYSCTL_CHILDREN(acpi_cpu_sysctl_tree),
		      OID_AUTO, "cx_supported", CTLFLAG_RD, cpu_cx_supported,
		      0, "Cx/microsecond values for supported Cx states");
    SYSCTL_ADD_PROC(&acpi_cpu_sysctl_ctx,
		    SYSCTL_CHILDREN(acpi_cpu_sysctl_tree),
		    OID_AUTO, "cx_lowest", CTLTYPE_STRING | CTLFLAG_RW,
		    NULL, 0, acpi_cpu_cx_lowest_sysctl, "A",
		    "lowest Cx sleep state to use");
    SYSCTL_ADD_PROC(&acpi_cpu_sysctl_ctx,
		    SYSCTL_CHILDREN(acpi_cpu_sysctl_tree),
		    OID_AUTO, "cx_usage", CTLTYPE_STRING | CTLFLAG_RD,
		    NULL, 0, acpi_cpu_usage_sysctl, "A",
		    "percent usage for each Cx state");

#ifdef notyet
    /* Signal platform that we can handle _CST notification. */
    if (cpu_cst_cnt != 0) {
	ACPI_LOCK(acpi);
	AcpiOsWritePort(cpu_smi_cmd, cpu_cst_cnt, 8);
	ACPI_UNLOCK(acpi);
    }
#endif

    /* Take over idling from cpu_idle_default(). */
    cpu_idle_hook = acpi_cpu_idle;
}

/*
 * Set CPUs to the new state.
 *
 * Must be called with the ACPI lock held.
 */
static void
acpi_cpu_throttle_set(uint32_t speed)
{
    struct acpi_cpu_softc	*sc;
    int				i;
    uint32_t			p_cnt, clk_val;

    ACPI_SERIAL_ASSERT(cpu);

    /* Iterate over processors */
    for (i = 0; i < cpu_ndevices; i++) {
	sc = device_get_softc(cpu_devices[i]);
	if (sc->cpu_p_cnt == NULL)
	    continue;

	/* Get the current P_CNT value and disable throttling */
	p_cnt = CPU_GET_REG(sc->cpu_p_cnt, 4);
	p_cnt &= ~CPU_P_CNT_THT_EN;
	CPU_SET_REG(sc->cpu_p_cnt, 4, p_cnt);

	/* If we're at maximum speed, that's all */
	if (speed < CPU_MAX_SPEED) {
	    /* Mask the old CLK_VAL off and or-in the new value */
	    clk_val = (CPU_MAX_SPEED - 1) << cpu_duty_offset;
	    p_cnt &= ~clk_val;
	    p_cnt |= (speed << cpu_duty_offset);

	    /* Write the new P_CNT value and then enable throttling */
	    CPU_SET_REG(sc->cpu_p_cnt, 4, p_cnt);
	    p_cnt |= CPU_P_CNT_THT_EN;
	    CPU_SET_REG(sc->cpu_p_cnt, 4, p_cnt);
	}
	ACPI_VPRINT(sc->cpu_dev, acpi_device_get_parent_softc(sc->cpu_dev),
		    "set speed to %d.%d%%\n", CPU_SPEED_PRINTABLE(speed));
    }
    cpu_throttle_state = speed;
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
    if (cpu_cx_count == 0) {
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

    /*
     * If we slept 100 us or more, use the lowest Cx state.  Otherwise,
     * find the lowest state that has a latency less than or equal to
     * the length of our last sleep.
     */
    cx_next_idx = cpu_cx_lowest;
    if (sc->cpu_prev_sleep < 100)
	for (i = cpu_cx_lowest; i >= 0; i--)
	    if (sc->cpu_cx_states[i].trans_lat <= sc->cpu_prev_sleep) {
		cx_next_idx = i;
		break;
	    }

    /*
     * Check for bus master activity.  If there was activity, clear
     * the bit and use the lowest non-C3 state.  Note that the USB
     * driver polling for new devices keeps this bit set all the
     * time if USB is loaded.
     */
    if ((cpu_quirks & CPU_QUIRK_NO_BM_CTRL) == 0) {
	AcpiGetRegister(ACPI_BITREG_BUS_MASTER_STATUS, &bm_active,
	    ACPI_MTX_DO_NOT_LOCK);
	if (bm_active != 0) {
	    AcpiSetRegister(ACPI_BITREG_BUS_MASTER_STATUS, 1,
		ACPI_MTX_DO_NOT_LOCK);
	    cx_next_idx = min(cx_next_idx, cpu_non_c3);
	}
    }

    /* Select the next state and update statistics. */
    cx_next = &sc->cpu_cx_states[cx_next_idx];
    cpu_cx_stats[cx_next_idx]++;
    KASSERT(cx_next->type != ACPI_STATE_C0, ("acpi_cpu_idle: C0 sleep"));

    /*
     * Execute HLT (or equivalent) and wait for an interrupt.  We can't
     * calculate the time spent in C1 since the place we wake up is an
     * ISR.  Assume we slept one quantum and return.
     */
    if (cx_next->type == ACPI_STATE_C1) {
	sc->cpu_prev_sleep = 1000000 / hz;
	acpi_cpu_c1();
	return;
    }

    /*
     * For C3, disable bus master arbitration and enable bus master wake
     * if BM control is available, otherwise flush the CPU cache.
     */
    if (cx_next->type == ACPI_STATE_C3) {
	if ((cpu_quirks & CPU_QUIRK_NO_BM_CTRL) == 0) {
	    AcpiSetRegister(ACPI_BITREG_ARB_DISABLE, 1, ACPI_MTX_DO_NOT_LOCK);
	    AcpiSetRegister(ACPI_BITREG_BUS_MASTER_RLD, 1,
		ACPI_MTX_DO_NOT_LOCK);
	} else
	    ACPI_FLUSH_CPU_CACHE();
    }

    /*
     * Read from P_LVLx to enter C2(+), checking time spent asleep.
     * Use the ACPI timer for measuring sleep time.  Since we need to
     * get the time very close to the CPU start/stop clock logic, this
     * is the only reliable time source.
     */
    AcpiHwLowLevelRead(32, &start_time, &AcpiGbl_FADT->XPmTmrBlk);
    CPU_GET_REG(cx_next->p_lvlx, 1);

    /*
     * Read the end time twice.  Since it may take an arbitrary time
     * to enter the idle state, the first read may be executed before
     * the processor has stopped.  Doing it again provides enough
     * margin that we are certain to have a correct value.
     */
    AcpiHwLowLevelRead(32, &end_time, &AcpiGbl_FADT->XPmTmrBlk);
    AcpiHwLowLevelRead(32, &end_time, &AcpiGbl_FADT->XPmTmrBlk);

    /* Enable bus master arbitration and disable bus master wakeup. */
    if (cx_next->type == ACPI_STATE_C3 &&
	(cpu_quirks & CPU_QUIRK_NO_BM_CTRL) == 0) {
	AcpiSetRegister(ACPI_BITREG_ARB_DISABLE, 0, ACPI_MTX_DO_NOT_LOCK);
	AcpiSetRegister(ACPI_BITREG_BUS_MASTER_RLD, 0, ACPI_MTX_DO_NOT_LOCK);
    }

    /* Find the actual time asleep in microseconds, minus overhead. */
    end_time = acpi_TimerDelta(end_time, start_time);
    sc->cpu_prev_sleep = PM_USEC(end_time) - cx_next->trans_lat;
    ACPI_ENABLE_IRQS();
}

/*
 * Re-evaluate the _PSS and _CST objects when we are notified that they
 * have changed.
 *
 * XXX Re-evaluation disabled until locking is done.
 */
static void
acpi_cpu_notify(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_cpu_softc *sc = (struct acpi_cpu_softc *)context;

    switch (notify) {
    case ACPI_CPU_NOTIFY_PERF_STATES:
	device_printf(sc->cpu_dev, "Performance states changed\n");
	/* acpi_cpu_px_available(sc); */
	break;
    case ACPI_CPU_NOTIFY_CX_STATES:
	device_printf(sc->cpu_dev, "Cx states changed\n");
	/* acpi_cpu_cx_cst(sc); */
	break;
    default:
	device_printf(sc->cpu_dev, "Unknown notify %#x\n", notify);
	break;
    }
}

static int
acpi_cpu_quirks(struct acpi_cpu_softc *sc)
{
    device_t acpi_dev;

    /*
     * C3 on multiple CPUs requires using the expensive flush cache
     * instruction.
     */
    if (mp_ncpus > 1)
	cpu_quirks |= CPU_QUIRK_NO_BM_CTRL;

    /* Look for various quirks of the PIIX4 part. */
    acpi_dev = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_82371AB_3);
    if (acpi_dev != NULL) {
	switch (pci_get_revid(acpi_dev)) {
	/*
	 * Disable throttling control on PIIX4 A and B-step.
	 * See specification changes #13 ("Manual Throttle Duty Cycle")
	 * and #14 ("Enabling and Disabling Manual Throttle"), plus
	 * erratum #5 ("STPCLK# Deassertion Time") from the January
	 * 2002 PIIX4 specification update.  Note that few (if any)
	 * mobile systems ever used this part.
	 */
	case PCI_REVISION_A_STEP:
	case PCI_REVISION_B_STEP:
	    cpu_quirks |= CPU_QUIRK_NO_THROTTLE;
	    /* FALLTHROUGH */
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
	 */
	case PCI_REVISION_4E:
	case PCI_REVISION_4M:
	    cpu_quirks |= CPU_QUIRK_NO_C3;
	    break;
	default:
	    break;
	}
    }

    return (0);
}

/* Handle changes in the CPU throttling setting. */
static int
acpi_cpu_throttle_sysctl(SYSCTL_HANDLER_ARGS)
{
    uint32_t	*argp;
    uint32_t	 arg;
    int		 error;

    argp = (uint32_t *)oidp->oid_arg1;
    arg = *argp;
    error = sysctl_handle_int(oidp, &arg, 0, req);

    /* Error or no new value */
    if (error != 0 || req->newptr == NULL)
	return (error);
    if (arg < 1 || arg > cpu_throttle_max)
	return (EINVAL);

    /* If throttling changed, notify the BIOS of the new rate. */
    ACPI_SERIAL_BEGIN(cpu);
    if (*argp != arg) {
	*argp = arg;
	acpi_cpu_throttle_set(arg);
    }
    ACPI_SERIAL_END(cpu);

    return (0);
}

static int
acpi_cpu_usage_sysctl(SYSCTL_HANDLER_ARGS)
{
    struct sbuf	 sb;
    char	 buf[128];
    int		 i;
    uintmax_t	 fract, sum, whole;

    sum = 0;
    for (i = 0; i < cpu_cx_count; i++)
	sum += cpu_cx_stats[i];
    sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
    for (i = 0; i < cpu_cx_count; i++) {
	if (sum > 0) {
	    whole = (uintmax_t)cpu_cx_stats[i] * 100;
	    fract = (whole % sum) * 100;
	    sbuf_printf(&sb, "%u.%02u%% ", (u_int)(whole / sum),
		(u_int)(fract / sum));
	} else
	    sbuf_printf(&sb, "0%% ");
    }
    sbuf_trim(&sb);
    sbuf_finish(&sb);
    sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
    sbuf_delete(&sb);

    return (0);
}

static int
acpi_cpu_cx_lowest_sysctl(SYSCTL_HANDLER_ARGS)
{
    struct	 acpi_cpu_softc *sc;
    char	 state[8];
    int		 val, error, i;

    sc = device_get_softc(cpu_devices[0]);
    snprintf(state, sizeof(state), "C%d", cpu_cx_lowest + 1);
    error = sysctl_handle_string(oidp, state, sizeof(state), req);
    if (error != 0 || req->newptr == NULL)
	return (error);
    if (strlen(state) < 2 || toupper(state[0]) != 'C')
	return (EINVAL);
    val = (int) strtol(state + 1, NULL, 10) - 1;
    if (val < 0 || val > cpu_cx_count - 1)
	return (EINVAL);

    ACPI_SERIAL_BEGIN(cpu);
    cpu_cx_lowest = val;

    /* If not disabling, cache the new lowest non-C3 state. */
    cpu_non_c3 = 0;
    for (i = cpu_cx_lowest; i >= 0; i--) {
	if (sc->cpu_cx_states[i].type < ACPI_STATE_C3) {
	    cpu_non_c3 = i;
	    break;
	}
    }

    /* Reset the statistics counters. */
    bzero(cpu_cx_stats, sizeof(cpu_cx_stats));
    ACPI_SERIAL_END(cpu);

    return (0);
}
