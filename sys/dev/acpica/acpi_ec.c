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
MODULE_NAME("EC")

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

    int			ec_locked;
    int			ec_pendquery;
    int			ec_csrvalue;
};

#define EC_LOCK_TIMEOUT	1000	/* 1ms */

static __inline ACPI_STATUS
EcLock(struct acpi_ec_softc *sc)
{
    ACPI_STATUS	status;

    status = AcpiAcquireGlobalLock();
    if (status == AE_OK)
	(sc)->ec_locked = 1;

    return(status);
}

static __inline void
EcUnlock(struct acpi_ec_softc *sc)
{
    (sc)->ec_locked = 0; 
    AcpiReleaseGlobalLock();
}

static __inline int
EcIsLocked(struct acpi_ec_softc *sc)
{
    return((sc)->ec_locked != 0);
}

typedef struct
{
    EC_COMMAND              Command;
    UINT8                   Address;
    UINT8                   Data;
} EC_REQUEST;

static void		EcGpeHandler(void *Context);
static ACPI_STATUS	EcSpaceSetup(ACPI_HANDLE Region, UINT32 Function, 
					   void *Context, void **return_Context);
static ACPI_STATUS	EcSpaceHandler(UINT32 Function, ACPI_PHYSICAL_ADDRESS Address, UINT32 width, ACPI_INTEGER *Value, 
				      void *Context, void *RegionContext);

static ACPI_STATUS	EcWaitEvent(struct acpi_ec_softc *sc, EC_EVENT Event);
static ACPI_STATUS	EcQuery(struct acpi_ec_softc *sc, UINT8 *Data);
static ACPI_STATUS	EcTransaction(struct acpi_ec_softc *sc, EC_REQUEST *EcRequest);
static ACPI_STATUS	EcRead(struct acpi_ec_softc *sc, UINT8 Address, UINT8 *Data);
static ACPI_STATUS	EcWrite(struct acpi_ec_softc *sc, UINT8 Address, UINT8 *Data);

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
    FUNCTION_TRACE(__func__);

    /* XXX implement - need an ACPI 2.0 system to test this */

    return_VOID;
}

/*
 * We could setup resources in the probe routine in order to have them printed 
 * when the device is attached.
 */
static int
acpi_ec_probe(device_t dev)
{

    if ((acpi_get_type(dev) == ACPI_TYPE_DEVICE) &&
	!acpi_disabled("ec") &&
	acpi_MatchHid(dev, "PNP0C09")) {

	/*
	 * Set device description 
	 */
	device_set_desc(dev, "embedded controller");

	return(0);
    }
    return(ENXIO);
}

static int
acpi_ec_attach(device_t dev)
{
    struct acpi_ec_softc	*sc;
    ACPI_STATUS			Status;

    FUNCTION_TRACE(__func__);

    /*
     * Fetch/initialise softc
     */
    sc = device_get_softc(dev);
    bzero(sc, sizeof(*sc));
    sc->ec_dev = dev;
    sc->ec_handle = acpi_get_handle(dev);

    /* 
     * Attach bus resources
     */
    sc->ec_data_rid = 0;
    if ((sc->ec_data_res = bus_alloc_resource(sc->ec_dev, SYS_RES_IOPORT, &sc->ec_data_rid,
					      0, ~0, 1, RF_ACTIVE)) == NULL) {
	device_printf(dev, "can't allocate data port\n");
	return_VALUE(ENXIO);
    }
    sc->ec_data_tag = rman_get_bustag(sc->ec_data_res);
    sc->ec_data_handle = rman_get_bushandle(sc->ec_data_res);

    sc->ec_csr_rid = 1;
    if ((sc->ec_csr_res = bus_alloc_resource(sc->ec_dev, SYS_RES_IOPORT, &sc->ec_csr_rid,
					     0, ~0, 1, RF_ACTIVE)) == NULL) {
	device_printf(dev, "can't allocate command/status port\n");
	return_VALUE(ENXIO);
    }
    sc->ec_csr_tag = rman_get_bustag(sc->ec_csr_res);
    sc->ec_csr_handle = rman_get_bushandle(sc->ec_csr_res);

    /*
     * Install GPE handler
     *
     * Evaluate the _GPE method to find the GPE bit used by the EC to signal
     * status (SCI).
     */
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "attaching GPE\n"));
    if ((Status = acpi_EvaluateInteger(sc->ec_handle, "_GPE", &sc->ec_gpebit)) != AE_OK) {
	device_printf(dev, "can't evaluate _GPE - %s\n", AcpiFormatException(Status));
	return_VALUE(ENXIO);
    }

    /*
     * Install a handler for this EC's GPE bit.  Note that EC SCIs are 
     * treated as both edge- and level-triggered interrupts; in other words
     * we clear the status bit immediately after getting an EC-SCI, then
     * again after we're done processing the event.  This guarantees that
     * events we cause while performing a transaction (e.g. IBE/OBF) get 
     * cleared before re-enabling the GPE.
     */
    if ((Status = AcpiInstallGpeHandler(sc->ec_gpebit, ACPI_EVENT_LEVEL_TRIGGERED | ACPI_EVENT_EDGE_TRIGGERED, 
					EcGpeHandler, sc)) != AE_OK) {
	device_printf(dev, "can't install GPE handler for %s - %s\n",
		      acpi_name(sc->ec_handle), AcpiFormatException(Status));
	return_VALUE(ENXIO);
    }

    /* 
     * Install address space handler
     */
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "attaching address space handler\n"));
    if ((Status = AcpiInstallAddressSpaceHandler(sc->ec_handle, ACPI_ADR_SPACE_EC, 
						 EcSpaceHandler, EcSpaceSetup, sc)) != AE_OK) {
	device_printf(dev, "can't install address space handler for %s - %s\n",
		      acpi_name(sc->ec_handle), AcpiFormatException(Status));
	panic("very suck");
	return_VALUE(ENXIO);
    }
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "attach complete\n"));

    return_VALUE(0);
}

static void
EcGpeQueryHandler(void *Context)
{
    struct acpi_ec_softc	*sc = (struct acpi_ec_softc *)Context;
    UINT8			Data;
    ACPI_STATUS			Status;
    char			qxx[5];

    FUNCTION_TRACE(__func__);

    for (;;) {

	/*
	 * Check EC_SCI.
	 * 
	 * Bail out if the EC_SCI bit of the status register is not set.
	 * Note that this function should only be called when
	 * this bit is set (polling is used to detect IBE/OBF events).
	 *
	 * It is safe to do this without locking the controller, as it's
	 * OK to call EcQuery when there's no data ready; in the worst
	 * case we should just find nothing waiting for us and bail.
	 */
	if (!(EC_GET_CSR(sc) & EC_EVENT_SCI))
	    break;

	/*
	 * Find out why the EC is signalling us
	 */
	Status = EcQuery(sc, &Data);
	    
	/*
	 * If we failed to get anything from the EC, give up
	 */
	if (Status != AE_OK) {
	    ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		"GPE query failed - %s\n", AcpiFormatException(Status));
	    break;
	}

	/*
	 * Evaluate _Qxx to respond to the controller.
	 */
	sprintf(qxx, "_Q%02x", Data);
	strupr(qxx);
	Status = AcpiEvaluateObject(sc->ec_handle, qxx, NULL, NULL);
	/*
	 * Ignore spurious query requests.
	 */
	if (Status != AE_OK && (Data != 0 || Status != AE_NOT_FOUND)) {
	    ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    	"evaluation of GPE query method %s failed - %s\n", 
			qxx, AcpiFormatException(Status));
	}
    }
        /* I know I request Level trigger cleanup */
    if(AcpiClearEvent(sc->ec_gpebit, ACPI_EVENT_GPE) != AE_OK)
	    printf("EcGpeQueryHandler:ClearEvent Failed\n");
    if(AcpiEnableEvent(sc->ec_gpebit, ACPI_EVENT_GPE, 0) != AE_OK)
	    printf("EcGpeQueryHandler:EnableEvent Failed\n");
    return_VOID;
}

/*
 * Handle a GPE sent to us.
 */
static void
EcGpeHandler(void *Context)
{
    struct acpi_ec_softc *sc = Context;
    int csrvalue;

    /* 
     * If EC is locked, the intr must process EcRead/Write wait only.
     * Query request must be pending.
     */
    if (EcIsLocked(sc)){
	csrvalue = EC_GET_CSR(sc);
	if (csrvalue & EC_EVENT_SCI)
	    sc->ec_pendquery = 1;
	if ((csrvalue & EC_FLAG_OUTPUT_BUFFER)
	    || !(csrvalue & EC_FLAG_INPUT_BUFFER)) {
	    sc->ec_csrvalue = csrvalue;
	    wakeup((void *)&sc->ec_csrvalue);
	}
    }else{
	/* Queue GpeQuery Handler */
	if (AcpiOsQueueForExecution(OSD_PRIORITY_HIGH,
				    EcGpeQueryHandler,Context) != AE_OK){
	    printf("QueryHandler Queuing Failed\n");
	}
    }
    return;
}

static ACPI_STATUS
EcSpaceSetup(ACPI_HANDLE Region, UINT32 Function, void *Context, void **RegionContext)
{

    FUNCTION_TRACE(__func__);

    /*
     * Just pass the context through, there's nothing to do here.
     */
    *RegionContext = Context;

    return_ACPI_STATUS(AE_OK);
}

static ACPI_STATUS
EcSpaceHandler(UINT32 Function, ACPI_PHYSICAL_ADDRESS Address, UINT32 width, ACPI_INTEGER *Value, 
	       void *Context, void *RegionContext)
{
    struct acpi_ec_softc	*sc = (struct acpi_ec_softc *)Context;
    ACPI_STATUS			Status = AE_OK;
    EC_REQUEST			EcRequest;
    int				i;

    FUNCTION_TRACE_U32(__func__, (UINT32)Address);

    if ((Address > 0xFF) || (width % 8 != 0) || (Value == NULL) || (Context == NULL))
        return_ACPI_STATUS(AE_BAD_PARAMETER);

    switch (Function) {
    case ACPI_READ:
        EcRequest.Command = EC_COMMAND_READ;
        EcRequest.Address = Address;
	(*Value) = 0;
        break;

    case ACPI_WRITE:
        EcRequest.Command = EC_COMMAND_WRITE;
        EcRequest.Address = Address;
        break;

    default:
	device_printf(sc->ec_dev, "invalid Address Space function %d\n", Function);
        return_ACPI_STATUS(AE_BAD_PARAMETER);
    }

    /*
     * Perform the transaction.
     */
    for (i = 0; i < width; i += 8) {
	if (Function == ACPI_READ)
	    EcRequest.Data = 0;
	else
	    EcRequest.Data = (UINT8)((*Value) >> i);
	if ((Status = EcTransaction(sc, &EcRequest)) != AE_OK)
	    break;
        (*Value) |= (ACPI_INTEGER)EcRequest.Data << i;
	if (++EcRequest.Address == 0)
            return_ACPI_STATUS(AE_BAD_PARAMETER);
    }
    return_ACPI_STATUS(Status);
}

/*
 * Wait for an event interrupt for a specific condition.
 */
static ACPI_STATUS
EcWaitEventIntr(struct acpi_ec_softc *sc, EC_EVENT Event)
{
    EC_STATUS	EcStatus;
    int		i;

    FUNCTION_TRACE_U32(__func__, (UINT32)Event);

    /* XXX this should test whether interrupts are available some other way */
    if(cold)
	return_ACPI_STATUS(EcWaitEvent(sc, Event));

    if (!EcIsLocked(sc))
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcWaitEventIntr called without EC lock!\n");

    EcStatus = EC_GET_CSR(sc);

    /* XXX waiting too long? */
    for(i = 0; i < 10; i++){
	/*
	 * Check EC status against the desired event.
	 */
    	if ((Event == EC_EVENT_OUTPUT_BUFFER_FULL) &&
	    (EcStatus & EC_FLAG_OUTPUT_BUFFER))
	    return_ACPI_STATUS(AE_OK);
      
	if ((Event == EC_EVENT_INPUT_BUFFER_EMPTY) && 
	    !(EcStatus & EC_FLAG_INPUT_BUFFER))
	    return_ACPI_STATUS(AE_OK);
	
	sc->ec_csrvalue = 0;
	if (ACPI_MSLEEP(&sc->ec_csrvalue, &acpi_mutex, PZERO, "EcWait", 1) != EWOULDBLOCK){
	    EcStatus = sc->ec_csrvalue;
	}else{
	    EcStatus = EC_GET_CSR(sc);
	}
    }
    return_ACPI_STATUS(AE_ERROR);
}

static ACPI_STATUS
EcWaitEvent(struct acpi_ec_softc *sc, EC_EVENT Event)
{
    EC_STATUS	EcStatus;
    UINT32	i = 0;

    if (!EcIsLocked(sc))
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcWaitEvent called without EC lock!\n");

    /*
     * Stall 1us:
     * ----------
     * Stall for 1 microsecond before reading the status register
     * for the first time.  This allows the EC to set the IBF/OBF
     * bit to its proper state.
     *
     * XXX it is not clear why we read the CSR twice.
     */
    AcpiOsStall(1);
    EcStatus = EC_GET_CSR(sc);

    /*
     * Wait For Event:
     * ---------------
     * Poll the EC status register to detect completion of the last
     * command.  Wait up to 10ms (in 10us chunks) for this to occur.
     */
    for (i = 0; i < 1000; i++) {
	EcStatus = EC_GET_CSR(sc);

        if ((Event == EC_EVENT_OUTPUT_BUFFER_FULL) &&
            (EcStatus & EC_FLAG_OUTPUT_BUFFER))
	    return(AE_OK);

	if ((Event == EC_EVENT_INPUT_BUFFER_EMPTY) && 
            !(EcStatus & EC_FLAG_INPUT_BUFFER))
	    return(AE_OK);
	
	AcpiOsStall(10);
    }

    return(AE_ERROR);
}    

static ACPI_STATUS
EcQuery(struct acpi_ec_softc *sc, UINT8 *Data)
{
    ACPI_STATUS	Status;

    if ((Status = EcLock(sc)) != AE_OK)
	return(Status);

    EC_SET_CSR(sc, EC_COMMAND_QUERY);
    Status = EcWaitEvent(sc, EC_EVENT_OUTPUT_BUFFER_FULL);
    if (Status == AE_OK)
	*Data = EC_GET_DATA(sc);

    EcUnlock(sc);

    if (Status != AE_OK)
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "timeout waiting for EC to respond to EC_COMMAND_QUERY\n");
    return(Status);
}    

static ACPI_STATUS
EcTransaction(struct acpi_ec_softc *sc, EC_REQUEST *EcRequest)
{
    ACPI_STATUS	Status;

    /*
     * Lock the EC
     */
    if ((Status = EcLock(sc)) != AE_OK)
	return(Status);

    /*
     * Perform the transaction.
     */
    switch (EcRequest->Command) {
    case EC_COMMAND_READ:
	Status = EcRead(sc, EcRequest->Address, &(EcRequest->Data));
	break;

    case EC_COMMAND_WRITE:
	Status = EcWrite(sc, EcRequest->Address, &(EcRequest->Data));
	break;

    default:
	Status = AE_SUPPORT;
	break;
    }

    /*
     * Unlock the EC
     */
    EcUnlock(sc);
    
    /*
     * Clear & Re-Enable the EC GPE:
     * -----------------------------
     * 'Consume' any EC GPE events that we generated while performing
     * the transaction (e.g. IBF/OBF).	Clearing the GPE here shouldn't
     * have an adverse affect on outstanding EC-SCI's, as the source
     * (EC-SCI) will still be high and thus should trigger the GPE
     * immediately after we re-enabling it.
     */
    if (sc->ec_pendquery){
	    if(AcpiOsQueueForExecution(OSD_PRIORITY_HIGH,
		EcGpeQueryHandler, sc) != AE_OK)
		    printf("Pend Query Queuing Failed\n");
	    sc->ec_pendquery = 0;
    }

    if (AcpiClearEvent(sc->ec_gpebit, ACPI_EVENT_GPE) != AE_OK)
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcRequest: Unable to clear the EC GPE.\n");
    if (AcpiEnableEvent(sc->ec_gpebit, ACPI_EVENT_GPE, 0) != AE_OK)
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcRequest: Unable to re-enable the EC GPE.\n");

    return(Status);
}


static ACPI_STATUS
EcRead(struct acpi_ec_softc *sc, UINT8 Address, UINT8 *Data)
{
    ACPI_STATUS	Status;

    if (!EcIsLocked(sc))
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcRead called without EC lock!\n");

    /*EcBurstEnable(EmbeddedController);*/

    EC_SET_CSR(sc, EC_COMMAND_READ);
    if ((Status = EcWaitEventIntr(sc, EC_EVENT_INPUT_BUFFER_EMPTY)) != AE_OK) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcRead: Failed waiting for EC to process read command.\n");
	return(Status);
    }

    EC_SET_DATA(sc, Address);
    if ((Status = EcWaitEventIntr(sc, EC_EVENT_OUTPUT_BUFFER_FULL)) != AE_OK) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcRead: Failed waiting for EC to send data.\n");
	return(Status);
    }

    (*Data) = EC_GET_DATA(sc);

    /*EcBurstDisable(EmbeddedController);*/

    return(AE_OK);
}    

static ACPI_STATUS
EcWrite(struct acpi_ec_softc *sc, UINT8 Address, UINT8 *Data)
{
    ACPI_STATUS	Status;

    if (!EcIsLocked(sc))
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcWrite called without EC lock!\n");

    /*EcBurstEnable(EmbeddedController);*/

    EC_SET_CSR(sc, EC_COMMAND_WRITE);
    if ((Status = EcWaitEventIntr(sc, EC_EVENT_INPUT_BUFFER_EMPTY)) != AE_OK) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcWrite: Failed waiting for EC to process write command.\n");
	return(Status);
    }

    EC_SET_DATA(sc, Address);
    if ((Status = EcWaitEventIntr(sc, EC_EVENT_INPUT_BUFFER_EMPTY)) != AE_OK) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcRead: Failed waiting for EC to process address.\n");
	return(Status);
    }

    EC_SET_DATA(sc, *Data);
    if ((Status = EcWaitEventIntr(sc, EC_EVENT_INPUT_BUFFER_EMPTY)) != AE_OK) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
	    "EcWrite: Failed waiting for EC to process data.\n");
	return(Status);
    }

    /*EcBurstDisable(EmbeddedController);*/

    return(AE_OK);
}
