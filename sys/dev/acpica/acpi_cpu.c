/*-
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
 *
 *	$FreeBSD$
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

/*
 * Support for ACPI Processor devices.
 *
 * Note that this only provides ACPI 1.0 support (with the exception of the
 * PSTATE_CNT field).  2.0 support will involve implementing _PTC, _PCT,
 * _PSS and _PPC.
 */

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_PROCESSOR
MODULE_NAME("PROCESSOR")

struct acpi_cpu_softc {
    device_t		cpu_dev;
    ACPI_HANDLE		cpu_handle;

    u_int32_t		cpu_id;

    /* CPU throttling control register */
    struct resource	*cpu_p_blk;
#define CPU_GET_P_CNT(sc)	(bus_space_read_4(rman_get_bustag((sc)->cpu_p_blk), 	\
						  rman_get_bushandle((sc)->cpu_p_blk),	\
						  0))
#define CPU_SET_P_CNT(sc, val)	(bus_space_write_4(rman_get_bustag((sc)->cpu_p_blk), 	\
						  rman_get_bushandle((sc)->cpu_p_blk),	\
						  0, (val)))
#define CPU_P_CNT_THT_EN	(1<<4)
};

/* 
 * Speeds are stored in counts, from 1 - CPU_MAX_SPEED, and
 * reported to the user in tenths of a percent.
 */
static u_int32_t	cpu_duty_offset;
static u_int32_t	cpu_duty_width;
#define CPU_MAX_SPEED		(1 << cpu_duty_width)
#define CPU_SPEED_PERCENT(x)	((1000 * (x)) / CPU_MAX_SPEED)
#define CPU_SPEED_PRINTABLE(x)	(CPU_SPEED_PERCENT(x) / 10),(CPU_SPEED_PERCENT(x) % 10)

static u_int32_t	cpu_smi_cmd;	/* should be a generic way to do this */
static u_int8_t		cpu_pstate_cnt;

static u_int32_t	cpu_current_state;
static u_int32_t	cpu_performance_state;
static u_int32_t	cpu_economy_state;
static u_int32_t	cpu_max_state;

static device_t		*cpu_devices;
static int		cpu_ndevices;

static struct sysctl_ctx_list	acpi_cpu_sysctl_ctx;
static struct sysctl_oid	*acpi_cpu_sysctl_tree;

static int	acpi_cpu_probe(device_t dev);
static int	acpi_cpu_attach(device_t dev);
static void	acpi_cpu_init_throttling(void *arg);
static void	acpi_cpu_set_speed(u_int32_t speed);
static void	acpi_cpu_powerprofile(void *arg);
static int	acpi_cpu_speed_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t acpi_cpu_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_cpu_probe),
    DEVMETHOD(device_attach,	acpi_cpu_attach),

    {0, 0}
};

static driver_t acpi_cpu_driver = {
    "acpi_cpu",
    acpi_cpu_methods,
    sizeof(struct acpi_cpu_softc),
};

static devclass_t acpi_cpu_devclass;
DRIVER_MODULE(acpi_cpu, acpi, acpi_cpu_driver, acpi_cpu_devclass, 0, 0);

static int
acpi_cpu_probe(device_t dev)
{
    if (!acpi_disabled("cpu") &&
	(acpi_get_type(dev) == ACPI_TYPE_PROCESSOR)) {
	device_set_desc(dev, "CPU");	/* XXX get more verbose description? */
	return(0);
    }
    return(ENXIO);
}

static int
acpi_cpu_attach(device_t dev)
{
    struct acpi_cpu_softc	*sc;
    struct acpi_softc		*acpi_sc;
    ACPI_OBJECT			processor;
    ACPI_BUFFER			buf;
    ACPI_STATUS			status;
    u_int32_t			p_blk;
    u_int32_t			p_blk_length;
    u_int32_t			duty_end;
    int				rid;

    FUNCTION_TRACE(__func__);

    ACPI_ASSERTLOCK;

    sc = device_get_softc(dev);
    sc->cpu_dev = dev;
    sc->cpu_handle = acpi_get_handle(dev);

    /*
     * Get global parameters from the FADT.
     */
    if (device_get_unit(sc->cpu_dev) == 0) {
	cpu_duty_offset = AcpiGbl_FADT->DutyOffset;
	cpu_duty_width = AcpiGbl_FADT->DutyWidth;
	cpu_smi_cmd = AcpiGbl_FADT->SmiCmd;
	cpu_pstate_cnt = AcpiGbl_FADT->PstateCnt;

	/* validate the offset/width */
	if (cpu_duty_width > 0) {
		duty_end = cpu_duty_offset + cpu_duty_width - 1;
		/* check that it fits */
		if (duty_end > 31) {
			printf("acpi_cpu: CLK_VAL field overflows P_CNT register\n");
			cpu_duty_width = 0;
		}
		/* check for overlap with the THT_EN bit */
		if ((cpu_duty_offset <= 4) && (duty_end >= 4)) {
			printf("acpi_cpu: CLK_VAL field overlaps THT_EN bit\n");
			cpu_duty_width = 0;
		}
	}

	/* 
	 * Start the throttling process once the probe phase completes, if we think that
	 * it's going to be useful.  If the duty width value is zero, there are no significant
	 * bits in the register and thus no throttled states.
	 */
	if (cpu_duty_width > 0) {
	    AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_cpu_init_throttling, NULL);

	    acpi_sc = acpi_device_get_parent_softc(dev);
	    sysctl_ctx_init(&acpi_cpu_sysctl_ctx);
	    acpi_cpu_sysctl_tree = SYSCTL_ADD_NODE(&acpi_cpu_sysctl_ctx,
						  SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree),
						  OID_AUTO, "cpu", CTLFLAG_RD, 0, "");

	    SYSCTL_ADD_INT(&acpi_cpu_sysctl_ctx, SYSCTL_CHILDREN(acpi_cpu_sysctl_tree),
			   OID_AUTO, "max_speed", CTLFLAG_RD,
			   &cpu_max_state, 0, "maximum CPU speed");
	    SYSCTL_ADD_INT(&acpi_cpu_sysctl_ctx, SYSCTL_CHILDREN(acpi_cpu_sysctl_tree),
			   OID_AUTO, "current_speed", CTLFLAG_RD,
			   &cpu_current_state, 0, "current CPU speed");
	    SYSCTL_ADD_PROC(&acpi_cpu_sysctl_ctx, SYSCTL_CHILDREN(acpi_cpu_sysctl_tree),
			    OID_AUTO, "performance_speed", CTLTYPE_INT | CTLFLAG_RW,
			    &cpu_performance_state, 0, acpi_cpu_speed_sysctl, "I", "");
	    SYSCTL_ADD_PROC(&acpi_cpu_sysctl_ctx, SYSCTL_CHILDREN(acpi_cpu_sysctl_tree),
			    OID_AUTO, "economy_speed", CTLTYPE_INT | CTLFLAG_RW,
			    &cpu_economy_state, 0, acpi_cpu_speed_sysctl, "I", "");
	}
    }

    /*
     * Get the processor object.
     */
    buf.Pointer = &processor;
    buf.Length = sizeof(processor);
    if (ACPI_FAILURE(status = AcpiEvaluateObject(sc->cpu_handle, NULL, NULL, &buf))) {
	device_printf(sc->cpu_dev, "couldn't get Processor object - %s\n", AcpiFormatException(status));
	return_VALUE(ENXIO);
    }
    if (processor.Type != ACPI_TYPE_PROCESSOR) {
	device_printf(sc->cpu_dev, "Processor object has bad type %d\n", processor.Type);
	return_VALUE(ENXIO);
    }
    sc->cpu_id = processor.Processor.ProcId;

    /*
     * If it looks like we support throttling, find this CPU's P_BLK.
     *
     * Note that some systems seem to duplicate the P_BLK pointer across  
     * multiple CPUs, so not getting the resource is not fatal.
     * 
     * XXX should support _PTC here as well, once we work out how to parse it.
     *
     * XXX is it valid to assume that the P_BLK must be 6 bytes long?
     */
    if (cpu_duty_width > 0) {
	p_blk = processor.Processor.PblkAddress;
	p_blk_length = processor.Processor.PblkLength;
    
	/* allocate bus space if possible */
	if ((p_blk > 0) && (p_blk_length == 6)) {
	    rid = 0;
	    bus_set_resource(sc->cpu_dev, SYS_RES_IOPORT, rid, p_blk, p_blk_length);
	    sc->cpu_p_blk = bus_alloc_resource(sc->cpu_dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
					       RF_ACTIVE);

	    ACPI_DEBUG_PRINT((ACPI_DB_IO, "acpi_cpu%d: throttling with P_BLK at 0x%x/%d%s\n", 
			      device_get_unit(sc->cpu_dev), p_blk, p_blk_length,
			      sc->cpu_p_blk ? "" : " (shadowed)"));
	}
    }
    return_VALUE(0);
}

/*
 * Call this *after* all CPUs have been attached.
 *
 * Takes the ACPI lock to avoid fighting anyone over the SMI command
 * port.  Could probably lock less code.
 */
static void
acpi_cpu_init_throttling(void *arg)
{
    int cpu_temp_speed;

    ACPI_LOCK;

    /* get set of CPU devices */
    devclass_get_devices(acpi_cpu_devclass, &cpu_devices, &cpu_ndevices);

    /* initialise throttling states */
    cpu_max_state = CPU_MAX_SPEED;
    cpu_performance_state = cpu_max_state;
    cpu_economy_state = cpu_performance_state / 2;
    if (cpu_economy_state == 0)		/* 0 is 'reserved' */
	cpu_economy_state++;
    if (TUNABLE_INT_FETCH("hw.acpi.cpu.performance_speed",
	&cpu_temp_speed) && cpu_temp_speed > 0 &&
	cpu_temp_speed <= cpu_max_state)
	cpu_performance_state = cpu_temp_speed;
    if (TUNABLE_INT_FETCH("hw.acpi.cpu.economy_speed",
	&cpu_temp_speed) && cpu_temp_speed > 0 &&
	cpu_temp_speed <= cpu_max_state)
	cpu_economy_state = cpu_temp_speed;

    /* register performance profile change handler */
    EVENTHANDLER_REGISTER(powerprofile_change, acpi_cpu_powerprofile, NULL, 0);

    /* if ACPI 2.0+, signal platform that we are taking over throttling */
    if (cpu_pstate_cnt != 0) {
	/* XXX should be a generic interface for this */
	AcpiOsWritePort(cpu_smi_cmd, cpu_pstate_cnt, 8);
    }

    ACPI_UNLOCK;

    /* set initial speed */
    acpi_cpu_powerprofile(NULL);
    
    printf("acpi_cpu: CPU throttling enabled, %d steps from 100%% to %d.%d%%\n", 
	   CPU_MAX_SPEED, CPU_SPEED_PRINTABLE(1));
}

/*
 * Set CPUs to the new state.
 *
 * Must be called with the ACPI lock held.
 */
static void
acpi_cpu_set_speed(u_int32_t speed)
{
    struct acpi_cpu_softc	*sc;
    int				i;
    u_int32_t			p_cnt, clk_val;

    ACPI_ASSERTLOCK;

    /* iterate over processors */
    for (i = 0; i < cpu_ndevices; i++) {
	sc = device_get_softc(cpu_devices[i]);
	if (sc->cpu_p_blk == NULL)
	    continue;

	/* get the current P_CNT value and disable throttling */
	p_cnt = CPU_GET_P_CNT(sc);
	p_cnt &= ~CPU_P_CNT_THT_EN;
	CPU_SET_P_CNT(sc, p_cnt);

	/* if we're at maximum speed, that's all */
	if (speed < CPU_MAX_SPEED) {

	    /* mask the old CLK_VAL off and or-in the new value */
	    clk_val = CPU_MAX_SPEED << cpu_duty_offset;
	    p_cnt &= ~clk_val;
	    p_cnt |= (speed << cpu_duty_offset);
	
	    /* write the new P_CNT value and then enable throttling */
	    CPU_SET_P_CNT(sc, p_cnt);
	    p_cnt |= CPU_P_CNT_THT_EN;
	    CPU_SET_P_CNT(sc, p_cnt);
	}
	ACPI_VPRINT(sc->cpu_dev, acpi_device_get_parent_softc(sc->cpu_dev),
	    "set speed to %d.%d%%\n", CPU_SPEED_PRINTABLE(speed));
    }
    cpu_current_state = speed;
}

/*
 * Power profile change hook.
 *
 * Uses the ACPI lock to avoid reentrancy.
 */
static void
acpi_cpu_powerprofile(void *arg)
{
    u_int32_t	new;

    ACPI_LOCK;
    
    new = (powerprofile_get_state() == POWERPROFILE_PERFORMANCE) ? cpu_performance_state : cpu_economy_state;
    if (cpu_current_state != new)
	acpi_cpu_set_speed(new);

    ACPI_UNLOCK;
}

/*
 * Handle changes in the performance/ecomony CPU settings.
 *
 * Does not need the ACPI lock (although setting *argp should
 * probably be atomic).
 */
static int
acpi_cpu_speed_sysctl(SYSCTL_HANDLER_ARGS)
{
    u_int32_t	*argp;
    u_int32_t	arg;
    int		error;

    argp = (u_int32_t *)oidp->oid_arg1;
    arg = *argp;
    error = sysctl_handle_int(oidp, &arg, 0, req);

    /* error or no new value */
    if ((error != 0) || (req->newptr == NULL))
	return(error);
    
    /* range check */
    if ((arg < 1) || (arg > cpu_max_state))
	return(EINVAL);

    /* set new value and possibly switch */
    *argp = arg;
    acpi_cpu_powerprofile(NULL);

    return(0);
}
