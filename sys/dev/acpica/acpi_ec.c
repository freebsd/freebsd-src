/*-
 * Copyright (c) 2003 Nate Lawson
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

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_EC
ACPI_MODULE_NAME("EC")

/*
 * EC_COMMAND:
 * -----------
 */
typedef UINT8				EC_COMMAND;

#define EC_COMMAND_UNKNOWN		((EC_COMMAND) 0x00)
#define EC_COMMAND_READ			((EC_COMMAND) 0x80)
#define EC_COMMAND_WRITE		((EC_COMMAND) 0x81)
#define EC_COMMAND_BURST_ENABLE		((EC_COMMAND) 0x82)
#define EC_COMMAND_BURST_DISABLE	((EC_COMMAND) 0x83)
#define EC_COMMAND_QUERY		((EC_COMMAND) 0x84)

/* 
 * EC_STATUS:
 * ----------
 * The encoding of the EC status register is illustrated below.
 * Note that a set bit (1) indicates the property is TRUE
 * (e.g. if bit 0 is set then the output buffer is full).
 * +-+-+-+-+-+-+-+-+
 * |7|6|5|4|3|2|1|0|	
 * +-+-+-+-+-+-+-+-+
 *  | | | | | | | |
 *  | | | | | | | +- Output Buffer Full?
 *  | | | | | | +--- Input Buffer Full?
 *  | | | | | +----- <reserved>
 *  | | | | +------- Data Register is Command Byte?
 *  | | | +--------- Burst Mode Enabled?
 *  | | +----------- SCI Event?
 *  | +------------- SMI Event?
 *  +--------------- <Reserved>
 *
 */
typedef UINT8				EC_STATUS;

#define EC_FLAG_OUTPUT_BUFFER		((EC_STATUS) 0x01)
#define EC_FLAG_INPUT_BUFFER		((EC_STATUS) 0x02)
#define EC_FLAG_BURST_MODE		((EC_STATUS) 0x10)
#define EC_FLAG_SCI			((EC_STATUS) 0x20)

/*
 * EC_EVENT:
 * ---------
 */
typedef UINT8				EC_EVENT;

#define EC_EVENT_UNKNOWN		((EC_EVENT) 0x00)
#define EC_EVENT_OUTPUT_BUFFER_FULL	((EC_EVENT) 0x01)
#define EC_EVENT_INPUT_BUFFER_EMPTY	((EC_EVENT) 0x02)
#define EC_EVENT_SCI			((EC_EVENT) 0x20)

/*
 * Register access primitives
 */
#define EC_GET_DATA(sc)							\
	bus_space_read_1((sc)->ec_data_tag, (sc)->ec_data_handle, 0)

#define EC_SET_DATA(sc, v)						\
	bus_space_write_1((sc)->ec_data_tag, (sc)->ec_data_handle, 0, (v))

#define EC_GET_CSR(sc)							\
	bus_space_read_1((sc)->ec_csr_tag, (sc)->ec_csr_handle, 0)

#define EC_SET_CSR(sc, v)						\
	bus_space_write_1((sc)->ec_csr_tag, (sc)->ec_csr_handle, 0, (v))

/*
 * Driver softc.
 */
struct acpi_ec_softc {
    device_t		ec_dev;
    ACPI_HANDLE		ec_handle;
    UINT32		ec_gpebit;
    
    int			ec_data_rid;
    struct resource	*ec_data_res;
    bus_space_tag_t	ec_data_tag;
    bus_space_handle_t	ec_data_handle;

    int			ec_csr_rid;
    struct resource	*ec_csr_res;
    bus_space_tag_t	ec_csr_tag;
    bus_space_handle_t	ec_csr_handle;

    int			ec_glk;
    int			ec_glkhandle;
    struct mtx		ec_mtx;
    UINT32		ec_polldelay;
};

/*
 * XXX
 * I couldn't find it in the spec but other implementations also use a
 * value of 1 ms for the time to acquire global lock.
 */
#define EC_LOCK_TIMEOUT	1000

/*
 * Start with an interval of 1 us for status poll loop.  This delay
 * will be dynamically adjusted based on the actual time waited.
 */
#define EC_POLL_DELAY	1

/* Total time in ms spent in the poll loop waiting for a response. */
#define EC_POLL_TIMEOUT	50

static __inline ACPI_STATUS
EcLock(struct acpi_ec_softc *sc)
{
    ACPI_STATUS	status = AE_OK;

    /* Always acquire this EC's mutex. */
    mtx_lock(&sc->ec_mtx);

    /* If _GLK is non-zero, also acquire the global lock. */
    if (sc->ec_glk) {
	status = AcpiAcquireGlobalLock(EC_LOCK_TIMEOUT, &sc->ec_glkhandle);
	if (ACPI_FAILURE(status))
	    mtx_unlock(&sc->ec_mtx);
    }

    return (status);
}

static __inline void
EcUnlock(struct acpi_ec_softc *sc)
{
    if (sc->ec_glk)
	AcpiReleaseGlobalLock(sc->ec_glkhandle);
    mtx_unlock(&sc->ec_mtx);
}

static void		EcGpeHandler(void *Context);
static ACPI_STATUS	EcSpaceSetup(ACPI_HANDLE Region, UINT32 Function, 
				void *Context, void **return_Context);
static ACPI_STATUS	EcSpaceHandler(UINT32 Function,
				ACPI_PHYSICAL_ADDRESS Address,
				UINT32 width, ACPI_INTEGER *Value,
				void *Context, void *RegionContext);
static ACPI_STATUS	EcWaitEvent(struct acpi_ec_softc *sc, EC_EVENT Event);
static ACPI_STATUS	EcQuery(struct acpi_ec_softc *sc, UINT8 *Data);
static ACPI_STATUS	EcCommand(struct acpi_ec_softc *sc, EC_COMMAND cmd);
static ACPI_STATUS	EcRead(struct acpi_ec_softc *sc, UINT8 Address,
				UINT8 *Data);
static ACPI_STATUS	EcWrite(struct acpi_ec_softc *sc, UINT8 Address,
				UINT8 *Data);
static void		acpi_ec_identify(driver_t driver, device_t bus);
static int		acpi_ec_probe(device_t dev);
static int		acpi_ec_attach(device_t dev);

static device_method_t acpi_ec_methods[] = {
    /* Device interface */
    DEVMETHOD(device_identify,	acpi_ec_identify),
    DEVMETHOD(device_probe,	acpi_ec_probe),
    DEVMETHOD(device_attach,	acpi_ec_attach),

    {0, 0}
};

static driver_t acpi_ec_driver = {
    "acpi_ec",
    acpi_ec_methods,
    sizeof(struct acpi_ec_softc),
};

static devclass_t acpi_ec_devclass;
DRIVER_MODULE(acpi_ec, acpi, acpi_ec_driver, acpi_ec_devclass, 0, 0);

/*
 * Look for an ECDT table and if we find one, set up a default EC 
 * space handler to catch possible attempts to access EC space before
 * we have a real driver instance in place.
 * We're not really an identify routine, but because we get called 
 * before most other things, this works out OK.
 */
static void
acpi_ec_identify(driver_t driver, device_t bus)
{
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* XXX implement - need an ACPI 2.0 system to test this */
}

/*
 * We could setup resources in the probe routine in order to have them printed 
 * when the device is attached.
 */
static int
acpi_ec_probe(device_t dev)
{
    int ret = ENXIO;

    if (acpi_get_type(dev) == ACPI_TYPE_DEVICE && !acpi_disabled("ec") &&
	acpi_MatchHid(dev, "PNP0C09")) {

	/*
	 * Set device description 
	 */
	device_set_desc(dev, "embedded controller");
	ret = 0;
    }

    return (ret);
}

static int
acpi_ec_attach(device_t dev)
{
    struct acpi_ec_softc	*sc;
    ACPI_STATUS			Status;
    int				errval = 0;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Fetch/initialize softc (assumes softc is pre-zeroed). */
    sc = device_get_softc(dev);
    sc->ec_dev = dev;
    sc->ec_handle = acpi_get_handle(dev);
    sc->ec_polldelay = EC_POLL_DELAY;
    mtx_init(&sc->ec_mtx, "ACPI embedded controller", NULL, MTX_DEF);

    /* Attach bus resources for data and command/status ports. */
    sc->ec_data_rid = 0;
    sc->ec_data_res = bus_alloc_resource(sc->ec_dev, SYS_RES_IOPORT,
			&sc->ec_data_rid, 0, ~0, 1, RF_ACTIVE);
    if (sc->ec_data_res == NULL) {
	device_printf(dev, "can't allocate data port\n");
	errval = ENXIO;
	goto out;
    }
    sc->ec_data_tag = rman_get_bustag(sc->ec_data_res);
    sc->ec_data_handle = rman_get_bushandle(sc->ec_data_res);

    sc->ec_csr_rid = 1;
    sc->ec_csr_res = bus_alloc_resource(sc->ec_dev, SYS_RES_IOPORT,
			&sc->ec_csr_rid, 0, ~0, 1, RF_ACTIVE);
    if (sc->ec_csr_res == NULL) {
	device_printf(dev, "can't allocate command/status port\n");
	errval = ENXIO;
	goto out;
    }
    sc->ec_csr_tag = rman_get_bustag(sc->ec_csr_res);
    sc->ec_csr_handle = rman_get_bushandle(sc->ec_csr_res);

    /* Check if global lock should be used.  If not, leave flag as 0. */
    acpi_EvaluateInteger(sc->ec_handle, "_GLK", &sc->ec_glk);

    /*
     * Evaluate the _GPE method to find the GPE bit used by the EC to signal
     * status (SCI).  Note that we don't handle the case where it can
     * return a package instead of an int.
     */
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "querying GPE\n"));
    Status = acpi_EvaluateInteger(sc->ec_handle, "_GPE", &sc->ec_gpebit);
    if (ACPI_FAILURE(Status)) {
	device_printf(dev, "can't evaluate _GPE - %s\n",
		      AcpiFormatException(Status));
	errval = ENXIO;
	goto out;
    }

    /*
     * Install a handler for this EC's GPE bit.  We want edge-triggered
     * behavior.
     */
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "attaching GPE handler\n"));
    Status = AcpiInstallGpeHandler(NULL, sc->ec_gpebit,
		ACPI_EVENT_EDGE_TRIGGERED, &EcGpeHandler, sc);
    if (ACPI_FAILURE(Status)) {
	device_printf(dev, "can't install GPE handler for %s - %s\n",
		      acpi_name(sc->ec_handle), AcpiFormatException(Status));
	errval = ENXIO;
	goto out;
    }

    /* 
     * Install address space handler
     */
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "attaching address space handler\n"));
    Status = AcpiInstallAddressSpaceHandler(sc->ec_handle, ACPI_ADR_SPACE_EC,
		&EcSpaceHandler, &EcSpaceSetup, sc);
    if (ACPI_FAILURE(Status)) {
	device_printf(dev, "can't install address space handler for %s - %s\n",
		      acpi_name(sc->ec_handle), AcpiFormatException(Status));
	Status = AcpiRemoveGpeHandler(sc->ec_handle, sc->ec_gpebit,
				      &EcGpeHandler);
	if (ACPI_FAILURE(Status))
	    panic("Added GPE handler but can't remove it");
	errval = ENXIO;
	goto out;
    }

    ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		"GPE bit is %#x, %susing global lock\n", sc->ec_gpebit,
		sc->ec_glk == 0 ? "not " : "");

    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "acpi_ec_attach complete\n"));
    return (0);

 out:
    if (sc->ec_csr_res)
	bus_release_resource(sc->ec_dev, SYS_RES_IOPORT, sc->ec_csr_rid, 
			     sc->ec_csr_res);
    if (sc->ec_data_res)
	bus_release_resource(sc->ec_dev, SYS_RES_IOPORT, sc->ec_data_rid,
			     sc->ec_data_res);
    return (errval);
}

static ACPI_STATUS
EcQuery(struct acpi_ec_softc *sc, UINT8 *Data)
{
    ACPI_STATUS	Status;

    Status = EcLock(sc);
    if (ACPI_FAILURE(Status))
	return (Status);

    /*
     * Send a query command to the EC to find out which _Qxx call it
     * wants to make.  This command clears the SCI bit and also the
     * interrupt source since we are edge-triggered.
     */
    Status = EcCommand(sc, EC_COMMAND_QUERY);
    if (ACPI_SUCCESS(Status))
	*Data = EC_GET_DATA(sc);

    EcUnlock(sc);

    return (Status);
}    

static void
EcGpeQueryHandler(void *Context)
{
    struct acpi_ec_softc	*sc = (struct acpi_ec_softc *)Context;
    UINT8			Data;
    ACPI_STATUS			Status;
    EC_STATUS			EcStatus;
    char			qxx[5];

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    KASSERT(Context != NULL, ("EcGpeQueryHandler called with NULL"));

    /*
     * Check status for EC_SCI.
     * 
     * Bail out if the EC_SCI bit of the status register is not set.
     * Note that this function should only be called when
     * this bit is set (polling is used to detect IBE/OBF events).
     *
     * We don't acquire the global lock here but do protect against other
     * running commands (read/write/query) by grabbing ec_mtx.
     */
    mtx_lock(&sc->ec_mtx);
    EcStatus = EC_GET_CSR(sc);
    mtx_unlock(&sc->ec_mtx);
    if ((EcStatus & EC_EVENT_SCI) == 0)
	goto re_enable;

    /* Find out why the EC is signaling us. */
    Status = EcQuery(sc, &Data);
    if (ACPI_FAILURE(Status)) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "GPE query failed - %s\n", AcpiFormatException(Status));
	goto re_enable;
    }

    /* Ignore the value for "no outstanding event". (13.3.5) */
    if (Data == 0)
	goto re_enable;

    /* Evaluate _Qxx to respond to the controller. */
    sprintf(qxx, "_Q%02x", Data);
    strupr(qxx);
    Status = AcpiEvaluateObject(sc->ec_handle, qxx, NULL, NULL);
    if (ACPI_FAILURE(Status) && Status != AE_NOT_FOUND) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "evaluation of GPE query method %s failed - %s\n", 
		    qxx, AcpiFormatException(Status));
    }

re_enable:
    /* Re-enable the GPE event so we'll get future requests. */
    Status = AcpiEnableGpe(NULL, sc->ec_gpebit, ACPI_NOT_ISR);
    if (ACPI_FAILURE(Status))
	printf("EcGpeQueryHandler: AcpiEnableEvent failed\n");
}

/*
 * Handle a GPE.  Currently we only handle SCI events as others must
 * be handled by polling in EcWaitEvent().  This is because some ECs
 * treat events as level when they should be edge-triggered.
 */
static void
EcGpeHandler(void *Context)
{
    struct acpi_ec_softc *sc = Context;
    ACPI_STATUS		       Status;

    KASSERT(Context != NULL, ("EcGpeHandler called with NULL"));

    /* Disable further GPEs while we handle this one. */
    AcpiDisableGpe(NULL, sc->ec_gpebit, ACPI_ISR);

    /* Schedule the GPE query handler. */
    Status = AcpiOsQueueForExecution(OSD_PRIORITY_GPE, EcGpeQueryHandler,
		Context);
    if (ACPI_FAILURE(Status)) {
	printf("Queuing GPE query handler failed.\n");
	Status = AcpiEnableGpe(NULL, sc->ec_gpebit, ACPI_ISR);
	if (ACPI_FAILURE(Status))
	    printf("EcGpeHandler: AcpiEnableEvent failed\n");
    }
}

static ACPI_STATUS
EcSpaceSetup(ACPI_HANDLE Region, UINT32 Function, void *Context,
	     void **RegionContext)
{

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /*
     * Just pass the context through, there's nothing to do here.
     */
    *RegionContext = Context;

    return_ACPI_STATUS (AE_OK);
}

static ACPI_STATUS
EcSpaceHandler(UINT32 Function, ACPI_PHYSICAL_ADDRESS Address, UINT32 width,
	       ACPI_INTEGER *Value, void *Context, void *RegionContext)
{
    struct acpi_ec_softc	*sc = (struct acpi_ec_softc *)Context;
    ACPI_STATUS			Status = AE_OK;
    UINT8			EcAddr, EcData;
    int				i;

    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, (UINT32)Address);

    if (Address > 0xFF || width % 8 != 0 || Value == NULL || Context == NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    /*
     * Perform the transaction.
     */
    EcAddr = Address;
    for (i = 0; i < width; i += 8) {
	Status = EcLock(sc);
	if (ACPI_FAILURE(Status))
	    return (Status);

	switch (Function) {
	case ACPI_READ:
	    EcData = 0;
	    Status = EcRead(sc, EcAddr, &EcData);
	    break;
	case ACPI_WRITE:
	    EcData = (UINT8)((*Value) >> i);
	    Status = EcWrite(sc, EcAddr, &EcData);
	    break;
	default:
	    device_printf(sc->ec_dev, "invalid EcSpaceHandler function %d\n",
			  Function);
	    Status = AE_BAD_PARAMETER;
	    break;
	}

	EcUnlock(sc);
	if (ACPI_FAILURE(Status))
	    return (Status);

	*Value |= (ACPI_INTEGER)EcData << i;
	if (++EcAddr == 0)
	    return_ACPI_STATUS (AE_BAD_PARAMETER);
    }
    return_ACPI_STATUS (Status);
}

static ACPI_STATUS
EcWaitEvent(struct acpi_ec_softc *sc, EC_EVENT Event)
{
    EC_STATUS	EcStatus;
    ACPI_STATUS	Status;
    UINT32	i, period;

    mtx_assert(&sc->ec_mtx, MA_OWNED);
    Status = AE_NO_HARDWARE_RESPONSE;

    /* 
     * Wait for 1 us before checking the CSR.  Testing shows about
     * 50% of requests complete in 1 us and 90% of them complete
     * in 5 us or less.
     */
    AcpiOsStall(1);

    /*
     * Poll the EC status register to detect completion of the last
     * command.  Wait up to 50 ms (in chunks of sc->ec_polldelay
     * microseconds for the first 1 ms, in 1 ms chunks for the remainder
     * of the period) for this to occur.
     */
    for (i = 0; i < (1000 / sc->ec_polldelay) + EC_POLL_TIMEOUT; i++) {
	EcStatus = EC_GET_CSR(sc);

	/* Check if the user's event occurred. */
	if ((Event == EC_EVENT_OUTPUT_BUFFER_FULL &&
	    (EcStatus & EC_FLAG_OUTPUT_BUFFER) != 0) ||
	    (Event == EC_EVENT_INPUT_BUFFER_EMPTY && 
	    (EcStatus & EC_FLAG_INPUT_BUFFER) == 0)) {
		Status = AE_OK;
		break;
	}

	/* For the first 1 ms, DELAY, after that, msleep. */
	if (i < 1000)
	    AcpiOsStall(sc->ec_polldelay);
	else
	    msleep(&sc->ec_polldelay, &sc->ec_mtx, PZERO, "ecpoll", 1/*ms*/);
    }

    /* Scale poll delay by the amount of time actually waited. */
    if (Status == AE_OK) {
	period = i * sc->ec_polldelay;
	if (period <= 5)
	    sc->ec_polldelay = 1;
	else if (period <= 20)
	    sc->ec_polldelay = 5;
	else if (period <= 100)
	    sc->ec_polldelay = 10;
	else
	    sc->ec_polldelay = 100;
    }

    return (Status);
}    

static ACPI_STATUS
EcCommand(struct acpi_ec_softc *sc, EC_COMMAND cmd)
{
    ACPI_STATUS	Status;
    EC_EVENT	Event;

    mtx_assert(&sc->ec_mtx, MA_OWNED);

    /* Decide what to wait for based on command type. */
    switch (cmd) {
    case EC_COMMAND_READ:
    case EC_COMMAND_WRITE:
    case EC_COMMAND_BURST_DISABLE:
	Event = EC_EVENT_INPUT_BUFFER_EMPTY;
	break;
    case EC_COMMAND_QUERY:
    case EC_COMMAND_BURST_ENABLE:
	Event = EC_EVENT_OUTPUT_BUFFER_FULL;
	break;
    default:
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "EcCommand: Invalid command %#x\n", cmd);
	return (AE_BAD_PARAMETER);
    }

    /* Run the command and wait for the chosen event. */
    EC_SET_CSR(sc, cmd);
    Status = EcWaitEvent(sc, Event);
    if (ACPI_FAILURE(Status)) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "EcCommand: no response to %#x\n", cmd);
    }

    return (Status);
}

static ACPI_STATUS
EcRead(struct acpi_ec_softc *sc, UINT8 Address, UINT8 *Data)
{
    ACPI_STATUS	Status;

    mtx_assert(&sc->ec_mtx, MA_OWNED);

#ifdef notyet
    /* If we can't start burst mode, continue anyway. */
    EcCommand(sc, EC_COMMAND_BURST_ENABLE);
#endif

    Status = EcCommand(sc, EC_COMMAND_READ);
    if (ACPI_FAILURE(Status))
	return (Status);

    EC_SET_DATA(sc, Address);
    Status = EcWaitEvent(sc, EC_EVENT_OUTPUT_BUFFER_FULL);
    if (ACPI_FAILURE(Status)) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "EcRead: Failed waiting for EC to send data.\n");
	return (Status);
    }

    *Data = EC_GET_DATA(sc);

#ifdef notyet
    if (sc->ec_burstactive) {
	Status = EcCommand(sc, EC_COMMAND_BURST_DISABLE);
	if (ACPI_FAILURE(Status))
	    return (Status);
    }
#endif

    return (AE_OK);
}    

static ACPI_STATUS
EcWrite(struct acpi_ec_softc *sc, UINT8 Address, UINT8 *Data)
{
    ACPI_STATUS	Status;

    mtx_assert(&sc->ec_mtx, MA_OWNED);

#ifdef notyet
    /* If we can't start burst mode, continue anyway. */
    EcCommand(sc, EC_COMMAND_BURST_ENABLE);
#endif

    Status = EcCommand(sc, EC_COMMAND_WRITE);
    if (ACPI_FAILURE(Status))
	return (Status);

    EC_SET_DATA(sc, Address);
    Status = EcWaitEvent(sc, EC_EVENT_INPUT_BUFFER_EMPTY);
    if (ACPI_FAILURE(Status)) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "EcRead: Failed waiting for EC to process address\n");
	return (Status);
    }

    EC_SET_DATA(sc, *Data);
    Status = EcWaitEvent(sc, EC_EVENT_INPUT_BUFFER_EMPTY);
    if (ACPI_FAILURE(Status)) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "EcWrite: Failed waiting for EC to process data\n");
	return (Status);
    }

#ifdef notyet
    if (sc->ec_burstactive) {
	Status = EcCommand(sc, EC_COMMAND_BURST_DISABLE);
	if (ACPI_FAILURE(Status))
	    return (Status);
    }
#endif

    return (AE_OK);
}
