/*-
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
/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions 
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.  
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee 
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.  
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE. 
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

/*
 * Processor driver.
 *
 * XXX Note that the power state code here is almost certainly suboptimal.
 *     We should go raid the Linux code for their ideas and experience.
 *
 * Code style here is a hairy mix of BSD-like and Intel-like.  Should be
 * sanitised at some point.
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

#define PR_MAX_POWER_STATES		4
#define PR_MAX_PERFORMANCE_STATES	8
#define PR_MAX_THROTTLING_STATES	8

/* 
 * Processor Commands:
 * -------------------
 */
#define PR_COMMAND_GET_INFO		((BM_COMMAND) 0x80)
#define PR_COMMAND_SET_CX_STATE_INFO	((BM_COMMAND) 0x81)
#define PR_COMMAND_GET_THROTTLING_STATE ((BM_COMMAND) 0x82)
#define PR_COMMAND_SET_THROTTLING_STATE ((BM_COMMAND) 0x83)
#define PR_COMMAND_GET_PERF_STATE	((BM_COMMAND) 0x84)
#define PR_COMMAND_SET_PERF_STATE	((BM_COMMAND) 0x85)
#define PR_COMMAND_GET_CURRENT_FREQ	((BM_COMMAND) 0x86)

/*
 * PR_POWER_STATE:
 * ---------------
 */
typedef u_int32_t				PR_POWER_STATE;

#define PR_POWER_STATE_UNKNOWN		((PR_POWER_STATE) 0xFFFFFFFF)

#define PR_POWER_STATE_C0		((PR_POWER_STATE) 0x00000000)
#define PR_POWER_STATE_C1		((PR_POWER_STATE) 0x00000001)
#define PR_POWER_STATE_C2		((PR_POWER_STATE) 0x00000002)
#define PR_POWER_STATE_C3		((PR_POWER_STATE) 0x00000003)

/* 
 * Processor Notifications:
 * ------------------------
 */
#define PR_NOTIFY_PERF_STATES_CHANGE	((BM_NOTIFY) 0x80)
#define PR_NOTIFY_POWER_STATES_CHANGE	((BM_NOTIFY) 0x81)


typedef struct
{
    u_int32_t		TimeThreshold;
    u_int32_t		CountThreshold;
    u_int32_t		Count;
    PR_POWER_STATE	TargetState;
} PR_POLICY_VALUES;
 
/* 
 * PR_CX_STATE_INFO:
 * -----------------
 */
typedef struct
{
    u_int32_t		Latency;
    u_int64_t		Utilization;
    PR_POLICY_VALUES	PromotionPolicy;
    PR_POLICY_VALUES	DemotionPolicy;
} PR_CX_STATE_INFO;

/* 
 * PR_POWER_INFO:
 * --------------
 */
typedef struct
{
    u_int32_t		Count;
    PR_POWER_STATE	ActiveState;
    PR_CX_STATE_INFO	Info[PR_MAX_POWER_STATES];
} PR_POWER_INFO;

/* 
 * PR_PERFORMANCE_INFO:
 * --------------------
 */
typedef struct
{
    u_int32_t		Count;
    /* TODO... */
} PR_PERFORMANCE_INFO;

/* 
 * PR_THROTTLING_INFO:
 * -------------------
 */
typedef struct
{
    u_int32_t		Count;
    u_int32_t		Percentage[PR_MAX_THROTTLING_STATES];
} PR_THROTTLING_INFO;

struct acpi_pr_softc {
    device_t		pr_dev;
    ACPI_HANDLE		pr_handle;
    PR_POWER_INFO	pr_PowerStates;
    PR_PERFORMANCE_INFO	pr_PerformanceStates;
    PR_THROTTLING_INFO	pr_ThrottlingStates;
    eventhandler_tag	pr_idleevent;

    /* local APIC data */
    PROCESSOR_APIC	pr_lapic;
};

#define PR_MAGIC	0x20555043	/* "CPU " */

static void		acpi_pr_identify(driver_t *driver, device_t bus);
static ACPI_STATUS	acpi_pr_identify_cpu(ACPI_HANDLE handle, UINT32 level, void *context, void **status);
static int		acpi_pr_probe(device_t dev);
static int		acpi_pr_attach(device_t dev);

static void		acpi_pr_FindLapic(device_t dev, ACPI_HANDLE handle, PROCESSOR_APIC *lapic);
static ACPI_STATUS	acpi_pr_CalculatePowerStates(struct acpi_pr_softc *sc);
static ACPI_STATUS	acpi_pr_CalculatePerformanceStates(struct acpi_pr_softc *sc);
static ACPI_STATUS	acpi_pr_CalculateThrottlingStates(struct acpi_pr_softc *sc);
static void		acpi_pr_IdleHandler(void *arg, int count);
static ACPI_STATUS	acpi_pr_PolicyInitialize(struct acpi_pr_softc *sc);

static device_method_t acpi_pr_methods[] = {
    /* Device interface */
    DEVMETHOD(device_identify,	acpi_pr_identify),
    DEVMETHOD(device_probe,	acpi_pr_probe),
    DEVMETHOD(device_attach,	acpi_pr_attach),

    {0, 0}
};

static driver_t acpi_pr_driver = {
    "acpi_pr",
    acpi_pr_methods,
    sizeof(struct acpi_pr_softc),
};

devclass_t acpi_pr_devclass;
DRIVER_MODULE(acpi_pr, acpi, acpi_pr_driver, acpi_pr_devclass, 0, 0);

/*
 * Scan the \_PR_ scope for processor objects, and attach them accordingly.
 *
 * XXX note that we should find the local APIC address and obtain a resource
 *     that we can hand to child devices for access to it...
 */
static void
acpi_pr_identify(driver_t *driver, device_t bus)
{
    ACPI_HANDLE			handle;

    if (AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_PR_", &handle) == AE_OK)
	AcpiWalkNamespace(ACPI_TYPE_PROCESSOR, handle, 2, acpi_pr_identify_cpu, bus, NULL);
}

/*
 * Create a child device for CPUs
 */
static ACPI_STATUS
acpi_pr_identify_cpu(ACPI_HANDLE handle, UINT32 level, void *context, void **status)
{
    device_t		bus = (device_t)context;
    device_t		child;
    PROCESSOR_APIC	lapic;
    

    acpi_pr_FindLapic(bus, handle, &lapic);

    if (lapic.ProcessorEnabled) {
	if ((child = BUS_ADD_CHILD(bus, 0, "acpi_pr", -1)) == NULL) {
	    device_printf(bus, "could not create CPU device\n");
	    return(AE_OK);
	}
	acpi_set_handle(child, handle);	
	acpi_set_magic(child, PR_MAGIC);
	device_set_desc(child, "processor device");
    }

    return(AE_OK);
}

static int
acpi_pr_probe(device_t dev)
{
    if (acpi_get_magic(dev) == PR_MAGIC)
	return(0);
    return(ENXIO);
}

static int
acpi_pr_attach(device_t dev)
{
    struct acpi_pr_softc	*sc;

    sc = device_get_softc(dev);
    sc->pr_dev = dev;
    sc->pr_handle = acpi_get_handle(dev);
    acpi_pr_FindLapic(dev, sc->pr_handle, &sc->pr_lapic);

    /*
     * If the APIC information is valid, print it
     */
    if (sc->pr_lapic.LocalApicId != (UINT8)0xff)
	device_printf(dev, "local APIC ID %d\n", sc->pr_lapic.LocalApicId);

    /*
     * Fetch operational parameters.
     */
    if (acpi_pr_CalculatePowerStates(sc) == AE_OK) {
	acpi_pr_PolicyInitialize(sc);
    }
    acpi_pr_CalculatePerformanceStates(sc);
    acpi_pr_CalculateThrottlingStates(sc);
    
    /* XXX call MD cpu-identification here? */

    return(0);
}

/*
 * Find the Local Apic information for this CPU
 */
static void
acpi_pr_FindLapic(device_t dev, ACPI_HANDLE handle, PROCESSOR_APIC *lapic)
{
    ACPI_BUFFER		buf;
    ACPI_STATUS		status;
    APIC_HEADER		*hdr;
    APIC_TABLE		*tbl;
    PROCESSOR_APIC	*pap;
    int			len, cpuno;

    /*
     * Assume that we're not going to suceed in finding/parsing the APIC table.
     * In this case, CPU 0 is valid, and any other CPU is invalid.
     */
    lapic->LocalApicId = 0xff;
    lapic->ProcessorEnabled = 0;
    if ((status = AcpiGetProcessorId(handle, &cpuno)) != AE_OK) {
	device_printf(dev, "error fetching CPU device ID - %s\n", acpi_strerror(status));
	return;
    }
    lapic->ProcessorEnabled = (cpuno == 0);

#if 0	/* broken by new ACPICA code that doesn't support the APIC table */
    /*
     * Perform the tedious double-get to fetch the actual APIC table, and suck it in.
     */
    buf.Length = 0;
    buf.Pointer = NULL;
    if ((status = AcpiGetTable(ACPI_TABLE_APIC, 1, &buf)) != AE_BUFFER_OVERFLOW) {
	if (status != AE_NOT_EXIST)
	    device_printf(dev, "error sizing APIC table - %s\n", acpi_strerror(status));
	return;
    }
    if ((buf.Pointer = AcpiOsAllocate(buf.Length)) == NULL)
	return;
    if ((status = AcpiGetTable(ACPI_TABLE_APIC, 1, &buf)) != AE_OK) {
	device_printf(dev, "error fetching APIC table - %s\n", acpi_strerror(status));
	return;
    }

    /*
     * Scan the tables looking for this CPU index.
     */
    tbl = (APIC_TABLE *)buf.Pointer;
    len = tbl->header.Length - sizeof(APIC_TABLE);
    hdr = (APIC_HEADER *)((char *)buf.Pointer + sizeof(APIC_TABLE));
    while(len > 0) {
	if (hdr->Length > len) {
	    device_printf(dev, "APIC header corrupt (claims %d bytes where only %d left in structure)\n",
			  hdr->Length, len);
	    break;
	}
	/*
	 * If we have found a processor APIC definition with 
	 * matching CPU index, copy it out and return.
	 */
	if (hdr->Type == APIC_PROC) {
	    pap = (PROCESSOR_APIC *)hdr;
	    if (pap->ProcessorApicId == cpuno) {
		bcopy(pap, lapic, sizeof(*pap));
		break;
	    }
	}
	len -= hdr->Length;
	hdr = (APIC_HEADER *)((char *)hdr + hdr->Length);
    }
    AcpiOsFree(buf.Pointer);
#endif
}

static ACPI_STATUS
acpi_pr_CalculatePowerStates(struct acpi_pr_softc *sc)
{
    ACPI_STATUS		    Status = AE_OK;
    ACPI_BUFFER		    Buffer;
    ACPI_CX_STATE	    *State = NULL;
    u_int32_t		    StateCount = 0;
    u_int32_t		    i = 0;

    /*
     * Set Latency Defaults:
     * ---------------------
     * Default state latency to ACPI_UINT32_MAX -- meaning that this state
     * should not be used by policy.  This value is overriden by states
     * that are present and have usable latencies (e.g. <= 1000us for C3).
     */
    for (i = 0; i < PR_MAX_POWER_STATES; i++)
	sc->pr_PowerStates.Info[i].Latency = ACPI_UINT32_MAX;

    /*
     * Get Power State Latencies:
     * --------------------------
     *
     * XXX Note that ACPICA will never give us back C2 if it costs more than 100us,
     *     or C3 if it costs more than 1000us, so some of this code is redundant.
     */
    Status = acpi_GetIntoBuffer(sc->pr_handle, AcpiGetProcessorCxInfo, &Buffer);
    if (Status != AE_OK) {
	device_printf(sc->pr_dev, "could not fetch ProcessorCxInfo - %s\n", acpi_strerror(Status));
	return(Status);
    }

    State = (ACPI_CX_STATE*)(Buffer.Pointer);
    if (State != NULL) {
	device_printf(sc->pr_dev, "supported power states:");
	StateCount = Buffer.Length / sizeof(ACPI_CX_STATE);
	for (i = 0; i < StateCount; i++) {
	    /* XXX C3 isn't supportable in MP configurations, how to best handle this? */
	    if ((State[i].StateNumber < PR_MAX_POWER_STATES) && (State[i].Latency <= 1000)) {
		printf(" C%d (%dus)", i, State[i].Latency);
		sc->pr_PowerStates.Info[State[i].StateNumber].Latency = State[i].Latency;
	    }
	}
	printf("\n");
    }
    sc->pr_PowerStates.Count = PR_MAX_POWER_STATES;
    sc->pr_PowerStates.ActiveState = PR_POWER_STATE_C1;

    AcpiOsFree(Buffer.Pointer);
    return(Status);
}

static ACPI_STATUS
acpi_pr_CalculatePerformanceStates(struct acpi_pr_softc *sc)
{
    ACPI_STATUS		    Status = AE_OK;

    /* TODO... */

    return(Status);
}
    
static ACPI_STATUS
acpi_pr_CalculateThrottlingStates(struct acpi_pr_softc *sc)
{
    ACPI_STATUS		    Status = AE_OK;
    ACPI_BUFFER		    Buffer;
    ACPI_CPU_THROTTLING_STATE *State = NULL;
    u_int32_t		    StateCount = 0;
    u_int32_t		    i = 0;

    /*
     * Get Throttling States:
     * ----------------------
     */
    Status = acpi_GetIntoBuffer(sc->pr_handle, AcpiGetProcessorThrottlingInfo, &Buffer);
    if (Status != AE_OK) {
	device_printf(sc->pr_dev, "could not fetch ThrottlingInfo - %s\n", acpi_strerror(Status));
	return(Status);
    }

    State = (ACPI_CPU_THROTTLING_STATE*)(Buffer.Pointer);
    if (State != NULL) {
	StateCount = Buffer.Length / sizeof(ACPI_CPU_THROTTLING_STATE);
	device_printf(sc->pr_dev, "supported throttling states:");
	for (i = 0; i < StateCount; i++) {
	    if (State[i].StateNumber < PR_MAX_THROTTLING_STATES) {
		/* TODO: Verify that state is *really* supported by this chipset/processor (e.g. errata). */
		sc->pr_ThrottlingStates.Percentage[State[i].StateNumber] = State[i].PercentOfClock;
		sc->pr_ThrottlingStates.Count++;
		printf(" %d%%", State[i].PercentOfClock);
	    }
	}
	printf("\n");
    }

    AcpiOsFree(Buffer.Pointer);
    return(Status);
}

static ACPI_STATUS
acpi_pr_PolicyInitialize(struct acpi_pr_softc *sc)
{
    ACPI_STATUS	status;

    if ((status = AcpiSetProcessorSleepState(sc->pr_handle, sc->pr_PowerStates.ActiveState)) != AE_OK) {
	device_printf(sc->pr_dev, "could not set Active sleep state - %s\n", acpi_strerror(status));
	return(status);
    }

    /* XXX need to hook ourselves to be called when things go idle */
/*    sc->pr_idleevent = EVENTHANDLER_FAST_REGISTER(idle_event, acpi_pr_IdleHandler, sc, IDLE_PRI_FIRST); */
    return(AE_OK);
}

static void
acpi_pr_IdleHandler(void *arg, int count)
{
    struct acpi_pr_softc	*sc = (struct acpi_pr_softc *)arg;
    ACPI_STATUS			Status = AE_OK;
    PR_CX_STATE_INFO		*CxState = NULL;
    PR_POWER_STATE		ActiveState = PR_POWER_STATE_UNKNOWN;
    PR_POWER_STATE		NextState = PR_POWER_STATE_UNKNOWN;
    u_int32_t			PmTimerTicks = 0;

    ActiveState = NextState = sc->pr_PowerStates.ActiveState;
    CxState = &(sc->pr_PowerStates.Info[ActiveState]);
    CxState->Utilization++;

    /*
     * Invoke Cx State:
     * ----------------
     */
    if ((Status = AcpiProcessorSleep(sc->pr_handle, &PmTimerTicks)) != AE_OK) {
	device_printf(sc->pr_dev, "AcpiProcessorSleep() failed - %s\n", acpi_strerror(Status));
	/*
	 * Something went wrong with the sleep attempt, so give up on trying to do this.
	 */
/*	EVENTHANDLER_FAST_DEREGISTER(idle_event, sc->pr_idleevent);*/
	device_printf(sc->pr_dev, "disabling CPU power saving\n");
	return;
    }

    /*
     * Check For State Promotion:
     * --------------------------
     * Only need to check for promotion on C1 and C2, and then only 
     * when the state has a non-zero count threshold and target state.
     */
    if (CxState->PromotionPolicy.CountThreshold && CxState->PromotionPolicy.TargetState && 
	((ActiveState == PR_POWER_STATE_C1) || (ActiveState == PR_POWER_STATE_C2))) {
	/*
	 * Check the amount of time we spent in the Cx state against our
	 * promotion policy.  If successful (asleep longer than our threshold)
	 * increment our count and see if a promotion is in order.
	 */
	if (PmTimerTicks > (CxState->PromotionPolicy.TimeThreshold)) {
	    CxState->PromotionPolicy.Count++;
	    CxState->DemotionPolicy.Count = 0;

	    if (CxState->PromotionPolicy.Count >= CxState->PromotionPolicy.CountThreshold)
		NextState = CxState->PromotionPolicy.TargetState;
	}
    }

    /*
     * Check For State Demotion:
     * -------------------------
     * Only need to check for demotion on C2 and C3, and then only 
     * when the state has a non-zero count threshold and target state.
     */
    if (CxState->DemotionPolicy.CountThreshold && CxState->DemotionPolicy.TargetState && 
	((ActiveState == PR_POWER_STATE_C2) || (ActiveState == PR_POWER_STATE_C3))) {
	/*
	 * Check the amount of time we spent in the Cx state against our
	 * demotion policy.  If unsuccessful (asleep shorter than our threshold)
	 * increment our count and see if a demotion is in order.
	 */
	if (PmTimerTicks < (CxState->DemotionPolicy.TimeThreshold)) {
	    CxState->DemotionPolicy.Count++;
	    CxState->PromotionPolicy.Count = 0;

	    if (CxState->DemotionPolicy.Count >= CxState->DemotionPolicy.CountThreshold)
		NextState = CxState->DemotionPolicy.TargetState;
	}
    }

    /*
     * New Cx State?
     * -------------
     * If so, clean up from the previous Cx state (if necessary).
     */
    if (NextState != sc->pr_PowerStates.ActiveState) {
	if ((Status = AcpiSetProcessorSleepState(sc->pr_handle, NextState)) != AE_OK) {
	    device_printf(sc->pr_dev, "AcpiSetProcessorSleepState() returned error [0x%08X]\n", Status);
	} else {
	    CxState->PromotionPolicy.Count = 0;
	    CxState->DemotionPolicy.Count = 0;
	    sc->pr_PowerStates.ActiveState = NextState;
	}
    }
}
