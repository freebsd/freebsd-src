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
 */
/*-
 ******************************************************************************
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sx.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <contrib/dev/acpica/acpi.h>
#include <dev/acpica/acpivar.h>

/* Hooks for the ACPI CA debugging infrastructure */
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

/* Embedded Controller Boot Resources Table (ECDT) */
typedef struct {
    ACPI_TABLE_HEADER		header;
    ACPI_GENERIC_ADDRESS	control;
    ACPI_GENERIC_ADDRESS	data;
    UINT32			uid;
    UINT8			gpe_bit;
    char			ec_id[0];
} ACPI_TABLE_ECDT;

/* Additional params to pass from the probe routine */
struct acpi_ec_params {
    int		glk;
    int		gpe_bit;
    ACPI_HANDLE	gpe_handle;
    int		uid;
};

/* Indicate that this device has already been probed via ECDT. */
#define DEV_ECDT(x)		(acpi_get_magic(x) == (int)&acpi_ec_devclass)

/*
 * Driver softc.
 */
struct acpi_ec_softc {
    device_t		ec_dev;
    ACPI_HANDLE		ec_handle;
    int			ec_uid;
    ACPI_HANDLE		ec_gpehandle;
    UINT8		ec_gpebit;
    UINT8		ec_csrvalue;
    
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
};

/*
 * XXX njl
 * I couldn't find it in the spec but other implementations also use a
 * value of 1 ms for the time to acquire global lock.
 */
#define EC_LOCK_TIMEOUT	1000

/* Default interval in microseconds for the status polling loop. */
#define EC_POLL_DELAY	10

/* Total time in ms spent in the poll loop waiting for a response. */
#define EC_POLL_TIMEOUT	100

#define EVENT_READY(event, status)			\
	(((event) == EC_EVENT_OUTPUT_BUFFER_FULL &&	\
	 ((status) & EC_FLAG_OUTPUT_BUFFER) != 0) ||	\
	 ((event) == EC_EVENT_INPUT_BUFFER_EMPTY && 	\
	 ((status) & EC_FLAG_INPUT_BUFFER) == 0))

static int	ec_poll_timeout = EC_POLL_TIMEOUT;
TUNABLE_INT("hw.acpi.ec.poll_timeout", &ec_poll_timeout);

ACPI_SERIAL_DECL(ec, "ACPI embedded controller");

static __inline ACPI_STATUS
EcLock(struct acpi_ec_softc *sc)
{
    ACPI_STATUS	status;

    /* Always acquire the exclusive lock. */
    status = AE_OK;
    ACPI_SERIAL_BEGIN(ec);

    /* If _GLK is non-zero, also acquire the global lock. */
    if (sc->ec_glk) {
	status = AcpiAcquireGlobalLock(EC_LOCK_TIMEOUT, &sc->ec_glkhandle);
	if (ACPI_FAILURE(status))
	    ACPI_SERIAL_END(ec);
    }

    return (status);
}

static __inline void
EcUnlock(struct acpi_ec_softc *sc)
{
    if (sc->ec_glk)
	AcpiReleaseGlobalLock(sc->ec_glkhandle);
    ACPI_SERIAL_END(ec);
}

static uint32_t		EcGpeHandler(void *Context);
static ACPI_STATUS	EcSpaceSetup(ACPI_HANDLE Region, UINT32 Function, 
				void *Context, void **return_Context);
static ACPI_STATUS	EcSpaceHandler(UINT32 Function,
				ACPI_PHYSICAL_ADDRESS Address,
				UINT32 width, ACPI_INTEGER *Value,
				void *Context, void *RegionContext);
static ACPI_STATUS	EcWaitEvent(struct acpi_ec_softc *sc, EC_EVENT Event);
static ACPI_STATUS	EcCommand(struct acpi_ec_softc *sc, EC_COMMAND cmd);
static ACPI_STATUS	EcRead(struct acpi_ec_softc *sc, UINT8 Address,
				UINT8 *Data);
static ACPI_STATUS	EcWrite(struct acpi_ec_softc *sc, UINT8 Address,
				UINT8 *Data);
static int		acpi_ec_probe(device_t dev);
static int		acpi_ec_attach(device_t dev);
static int		acpi_ec_shutdown(device_t dev);
static int		acpi_ec_read_method(device_t dev, u_int addr,
				ACPI_INTEGER *val, int width);
static int		acpi_ec_write_method(device_t dev, u_int addr,
				ACPI_INTEGER val, int width);

static device_method_t acpi_ec_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_ec_probe),
    DEVMETHOD(device_attach,	acpi_ec_attach),
    DEVMETHOD(device_shutdown,	acpi_ec_shutdown),

    /* Embedded controller interface */
    DEVMETHOD(acpi_ec_read,	acpi_ec_read_method),
    DEVMETHOD(acpi_ec_write,	acpi_ec_write_method),

    {0, 0}
};

static driver_t acpi_ec_driver = {
    "acpi_ec",
    acpi_ec_methods,
    sizeof(struct acpi_ec_softc),
};

static devclass_t acpi_ec_devclass;
DRIVER_MODULE(acpi_ec, acpi, acpi_ec_driver, acpi_ec_devclass, 0, 0);
MODULE_DEPEND(acpi_ec, acpi, 1, 1, 1);

/*
 * Look for an ECDT and if we find one, set up default GPE and 
 * space handlers to catch attempts to access EC space before
 * we have a real driver instance in place.
 * TODO: if people report invalid ECDTs, add a tunable to disable them.
 */
void
acpi_ec_ecdt_probe(device_t parent)
{
    ACPI_TABLE_ECDT *ecdt;
    ACPI_TABLE_HEADER *hdr;
    ACPI_STATUS	     status;
    device_t	     child;
    ACPI_HANDLE	     h;
    struct acpi_ec_params *params;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Find and validate the ECDT. */
    status = AcpiGetFirmwareTable("ECDT", 1, ACPI_LOGICAL_ADDRESSING, &hdr);
    ecdt = (ACPI_TABLE_ECDT *)hdr;
    if (ACPI_FAILURE(status) ||
	ecdt->control.RegisterBitWidth != 8 ||
	ecdt->data.RegisterBitWidth != 8) {
	return;
    }

    /* Create the child device with the given unit number. */
    child = BUS_ADD_CHILD(parent, 0, "acpi_ec", ecdt->uid);
    if (child == NULL) {
	printf("%s: can't add child\n", __func__);
	return;
    }

    /* Find and save the ACPI handle for this device. */
    status = AcpiGetHandle(NULL, ecdt->ec_id, &h);
    if (ACPI_FAILURE(status)) {
	device_delete_child(parent, child);
	printf("%s: can't get handle\n", __func__);
	return;
    }
    acpi_set_handle(child, h);

    /* Set the data and CSR register addresses. */
    bus_set_resource(child, SYS_RES_IOPORT, 0, ecdt->data.Address,
	/*count*/1);
    bus_set_resource(child, SYS_RES_IOPORT, 1, ecdt->control.Address,
	/*count*/1);

    /*
     * Store values for the probe/attach routines to use.  Store the
     * ECDT GPE bit and set the global lock flag according to _GLK.
     * Note that it is not perfectly correct to be evaluating a method
     * before initializing devices, but in practice this function
     * should be safe to call at this point.
     */
    params = malloc(sizeof(struct acpi_ec_params), M_TEMP, M_WAITOK | M_ZERO);
    params->gpe_handle = NULL;
    params->gpe_bit = ecdt->gpe_bit;
    params->uid = ecdt->uid;
    acpi_GetInteger(h, "_GLK", &params->glk);
    acpi_set_private(child, params);
    acpi_set_magic(child, (int)&acpi_ec_devclass);

    /* Finish the attach process. */
    if (device_probe_and_attach(child) != 0)
	device_delete_child(parent, child);
}

static int
acpi_ec_probe(device_t dev)
{
    ACPI_BUFFER buf;
    ACPI_HANDLE h;
    ACPI_OBJECT *obj;
    ACPI_STATUS status;
    device_t	peer;
    char	desc[64];
    int		ret;
    struct acpi_ec_params *params;
    static char *ec_ids[] = { "PNP0C09", NULL };

    /* Check that this is a device and that EC is not disabled. */
    if (acpi_get_type(dev) != ACPI_TYPE_DEVICE || acpi_disabled("ec"))
	return (ENXIO);

    /*
     * If probed via ECDT, set description and continue.  Otherwise,
     * we can access the namespace and make sure this is not a
     * duplicate probe.
     */
    ret = ENXIO;
    params = NULL;
    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    if (DEV_ECDT(dev)) {
	params = acpi_get_private(dev);
	ret = 0;
    } else if (!acpi_disabled("ec") &&
	ACPI_ID_PROBE(device_get_parent(dev), dev, ec_ids)) {
	params = malloc(sizeof(struct acpi_ec_params), M_TEMP,
			M_WAITOK | M_ZERO);
	h = acpi_get_handle(dev);

	/*
	 * Read the unit ID to check for duplicate attach and the
	 * global lock value to see if we should acquire it when
	 * accessing the EC.
	 */
	status = acpi_GetInteger(h, "_UID", &params->uid);
	if (ACPI_FAILURE(status))
	    params->uid = 0;
	status = acpi_GetInteger(h, "_GLK", &params->glk);
	if (ACPI_FAILURE(status))
	    params->glk = 0;

	/*
	 * Evaluate the _GPE method to find the GPE bit used by the EC to
	 * signal status (SCI).  If it's a package, it contains a reference
	 * and GPE bit, similar to _PRW.
	 */
	status = AcpiEvaluateObject(h, "_GPE", NULL, &buf);
	if (ACPI_FAILURE(status)) {
	    device_printf(dev, "can't evaluate _GPE - %s\n",
			  AcpiFormatException(status));
	    goto out;
	}
	obj = (ACPI_OBJECT *)buf.Pointer;
	if (obj == NULL)
	    goto out;

	switch (obj->Type) {
	case ACPI_TYPE_INTEGER:
	    params->gpe_handle = NULL;
	    params->gpe_bit = obj->Integer.Value;
	    break;
	case ACPI_TYPE_PACKAGE:
	    if (!ACPI_PKG_VALID(obj, 2))
		goto out;
	    params->gpe_handle =
		acpi_GetReference(NULL, &obj->Package.Elements[0]);
	    if (params->gpe_handle == NULL ||
		acpi_PkgInt32(obj, 1, &params->gpe_bit) != 0)
		goto out;
	    break;
	default:
	    device_printf(dev, "_GPE has invalid type %d\n", obj->Type);
	    goto out;
	}

	/* Store the values we got from the namespace for attach. */
	acpi_set_private(dev, params);

	/*
	 * Check for a duplicate probe.  This can happen when a probe
	 * via ECDT succeeded already.  If this is a duplicate, disable
	 * this device.
	 */
	peer = devclass_get_device(acpi_ec_devclass, params->uid);
	if (peer == NULL || !device_is_alive(peer))
	    ret = 0;
	else
	    device_disable(dev);
    }

out:
    if (ret == 0) {
	snprintf(desc, sizeof(desc), "Embedded Controller: GPE %#x%s%s",
		 params->gpe_bit, (params->glk) ? ", GLK" : "",
		 DEV_ECDT(dev) ? ", ECDT" : "");
	device_set_desc_copy(dev, desc);
    }

    if (ret > 0 && params)
	free(params, M_TEMP);
    if (buf.Pointer)
	AcpiOsFree(buf.Pointer);
    return (ret);
}

static int
acpi_ec_attach(device_t dev)
{
    struct acpi_ec_softc	*sc;
    struct acpi_ec_params	*params;
    ACPI_STATUS			Status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Fetch/initialize softc (assumes softc is pre-zeroed). */
    sc = device_get_softc(dev);
    params = acpi_get_private(dev);
    sc->ec_dev = dev;
    sc->ec_handle = acpi_get_handle(dev);

    /* Retrieve previously probed values via device ivars. */
    sc->ec_glk = params->glk;
    sc->ec_gpebit = params->gpe_bit;
    sc->ec_gpehandle = params->gpe_handle;
    sc->ec_uid = params->uid;
    free(params, M_TEMP);

    /* Attach bus resources for data and command/status ports. */
    sc->ec_data_rid = 0;
    sc->ec_data_res = bus_alloc_resource_any(sc->ec_dev, SYS_RES_IOPORT,
			&sc->ec_data_rid, RF_ACTIVE);
    if (sc->ec_data_res == NULL) {
	device_printf(dev, "can't allocate data port\n");
	goto error;
    }
    sc->ec_data_tag = rman_get_bustag(sc->ec_data_res);
    sc->ec_data_handle = rman_get_bushandle(sc->ec_data_res);

    sc->ec_csr_rid = 1;
    sc->ec_csr_res = bus_alloc_resource_any(sc->ec_dev, SYS_RES_IOPORT,
			&sc->ec_csr_rid, RF_ACTIVE);
    if (sc->ec_csr_res == NULL) {
	device_printf(dev, "can't allocate command/status port\n");
	goto error;
    }
    sc->ec_csr_tag = rman_get_bustag(sc->ec_csr_res);
    sc->ec_csr_handle = rman_get_bushandle(sc->ec_csr_res);

    /*
     * Install a handler for this EC's GPE bit.  We want edge-triggered
     * behavior.
     */
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "attaching GPE handler\n"));
    Status = AcpiInstallGpeHandler(sc->ec_gpehandle, sc->ec_gpebit,
		ACPI_GPE_EDGE_TRIGGERED, &EcGpeHandler, sc);
    if (ACPI_FAILURE(Status)) {
	device_printf(dev, "can't install GPE handler for %s - %s\n",
		      acpi_name(sc->ec_handle), AcpiFormatException(Status));
	goto error;
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
	goto error;
    }

    /* Enable runtime GPEs for the handler. */
    Status = AcpiSetGpeType(sc->ec_gpehandle, sc->ec_gpebit,
			    ACPI_GPE_TYPE_RUNTIME);
    if (ACPI_FAILURE(Status)) {
	device_printf(dev, "AcpiSetGpeType failed: %s\n",
		      AcpiFormatException(Status));
	goto error;
    }
    Status = AcpiEnableGpe(sc->ec_gpehandle, sc->ec_gpebit, ACPI_NOT_ISR);
    if (ACPI_FAILURE(Status)) {
	device_printf(dev, "AcpiEnableGpe failed: %s\n",
		      AcpiFormatException(Status));
	goto error;
    }

    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "acpi_ec_attach complete\n"));
    return (0);

error:
    AcpiRemoveGpeHandler(sc->ec_gpehandle, sc->ec_gpebit, &EcGpeHandler);
    AcpiRemoveAddressSpaceHandler(sc->ec_handle, ACPI_ADR_SPACE_EC,
	EcSpaceHandler);
    if (sc->ec_csr_res)
	bus_release_resource(sc->ec_dev, SYS_RES_IOPORT, sc->ec_csr_rid, 
			     sc->ec_csr_res);
    if (sc->ec_data_res)
	bus_release_resource(sc->ec_dev, SYS_RES_IOPORT, sc->ec_data_rid,
			     sc->ec_data_res);
    return (ENXIO);
}

static int
acpi_ec_shutdown(device_t dev)
{
    struct acpi_ec_softc	*sc;

    /* Disable the GPE so we don't get EC events during shutdown. */
    sc = device_get_softc(dev);
    AcpiDisableGpe(sc->ec_gpehandle, sc->ec_gpebit, ACPI_NOT_ISR);
    return (0);
}

/* Methods to allow other devices (e.g., smbat) to read/write EC space. */
static int
acpi_ec_read_method(device_t dev, u_int addr, ACPI_INTEGER *val, int width)
{
    struct acpi_ec_softc *sc;
    ACPI_STATUS status;

    sc = device_get_softc(dev);
    status = EcSpaceHandler(ACPI_READ, addr, width * 8, val, sc, NULL);
    if (ACPI_FAILURE(status))
	return (ENXIO);
    return (0);
}

static int
acpi_ec_write_method(device_t dev, u_int addr, ACPI_INTEGER val, int width)
{
    struct acpi_ec_softc *sc;
    ACPI_STATUS status;

    sc = device_get_softc(dev);
    status = EcSpaceHandler(ACPI_WRITE, addr, width * 8, &val, sc, NULL);
    if (ACPI_FAILURE(status))
	return (ENXIO);
    return (0);
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

    Status = EcLock(sc);
    if (ACPI_FAILURE(Status)) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "GpeQuery lock error: %s\n", AcpiFormatException(Status));
	return;
    }

    /*
     * If the EC_SCI bit of the status register is not set, then pass
     * it along to any potential waiters as it may be an IBE/OBF event.
     */
    EcStatus = EC_GET_CSR(sc);
    if ((EcStatus & EC_EVENT_SCI) == 0) {
	sc->ec_csrvalue = EcStatus;
	wakeup(&sc->ec_csrvalue);
	EcUnlock(sc);
	goto re_enable;
    }

    /*
     * Send a query command to the EC to find out which _Qxx call it
     * wants to make.  This command clears the SCI bit and also the
     * interrupt source since we are edge-triggered.
     */
    Status = EcCommand(sc, EC_COMMAND_QUERY);
    if (ACPI_FAILURE(Status)) {
	EcUnlock(sc);
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "GPE query failed - %s\n", AcpiFormatException(Status));
	goto re_enable;
    }
    Data = EC_GET_DATA(sc);
    EcUnlock(sc);

    /* Ignore the value for "no outstanding event". (13.3.5) */
    if (Data == 0)
	goto re_enable;

    /* Evaluate _Qxx to respond to the controller. */
    sprintf(qxx, "_Q%02x", Data);
    AcpiUtStrupr(qxx);
    Status = AcpiEvaluateObject(sc->ec_handle, qxx, NULL, NULL);
    if (ACPI_FAILURE(Status) && Status != AE_NOT_FOUND) {
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "evaluation of GPE query method %s failed - %s\n", 
		    qxx, AcpiFormatException(Status));
    }

re_enable:
    /* Re-enable the GPE event so we'll get future requests. */
    Status = AcpiEnableGpe(sc->ec_gpehandle, sc->ec_gpebit, ACPI_NOT_ISR);
    if (ACPI_FAILURE(Status))
	printf("EcGpeQueryHandler: AcpiEnableEvent failed\n");
}

/*
 * Handle a GPE.  Currently we only handle SCI events as others must
 * be handled by polling in EcWaitEvent().  This is because some ECs
 * treat events as level when they should be edge-triggered.
 */
static uint32_t
EcGpeHandler(void *Context)
{
    struct acpi_ec_softc *sc = Context;
    ACPI_STATUS		       Status;

    KASSERT(Context != NULL, ("EcGpeHandler called with NULL"));

    /*
     * Disable further GPEs while we handle this one.  Since we are directly
     * called by ACPI-CA and it may have unknown locks held, we specify the
     * ACPI_ISR flag to keep it from acquiring any more mutexes (which could
     * potentially sleep.)
     */
    AcpiDisableGpe(sc->ec_gpehandle, sc->ec_gpebit, ACPI_ISR);

    /* Schedule the GPE query handler. */
    Status = AcpiOsQueueForExecution(OSD_PRIORITY_GPE, EcGpeQueryHandler,
		Context);
    if (ACPI_FAILURE(Status)) {
	printf("Queuing GPE query handler failed.\n");
	Status = AcpiEnableGpe(sc->ec_gpehandle, sc->ec_gpebit, ACPI_ISR);
	if (ACPI_FAILURE(Status))
	    printf("EcGpeHandler: AcpiEnableEvent failed\n");
    }

    return (0);
}

static ACPI_STATUS
EcSpaceSetup(ACPI_HANDLE Region, UINT32 Function, void *Context,
	     void **RegionContext)
{

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /*
     * If deactivating a region, always set the output to NULL.  Otherwise,
     * just pass the context through.
     */
    if (Function == ACPI_REGION_DEACTIVATE)
	*RegionContext = NULL;
    else
	*RegionContext = Context;

    return_ACPI_STATUS (AE_OK);
}

static ACPI_STATUS
EcSpaceHandler(UINT32 Function, ACPI_PHYSICAL_ADDRESS Address, UINT32 width,
	       ACPI_INTEGER *Value, void *Context, void *RegionContext)
{
    struct acpi_ec_softc	*sc = (struct acpi_ec_softc *)Context;
    ACPI_STATUS			Status;
    UINT8			EcAddr, EcData;
    int				i;

    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, (UINT32)Address);

    if (width % 8 != 0 || Value == NULL || Context == NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);
    if (Address + (width / 8) - 1 > 0xFF)
	return_ACPI_STATUS (AE_BAD_ADDRESS);

    if (Function == ACPI_READ)
	*Value = 0;
    EcAddr = Address;
    Status = AE_ERROR;

    Status = EcLock(sc);
    if (ACPI_FAILURE(Status))
	return_ACPI_STATUS (Status);

    /* Perform the transaction(s), based on width. */
    for (i = 0; i < width; i += 8, EcAddr++) {
	switch (Function) {
	case ACPI_READ:
	    Status = EcRead(sc, EcAddr, &EcData);
	    if (ACPI_SUCCESS(Status))
		*Value |= ((ACPI_INTEGER)EcData) << i;
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
	if (ACPI_FAILURE(Status))
	    break;
    }

    EcUnlock(sc);
    return_ACPI_STATUS (Status);
}

static ACPI_STATUS
EcWaitEvent(struct acpi_ec_softc *sc, EC_EVENT Event)
{
    EC_STATUS	EcStatus;
    ACPI_STATUS	Status;
    int		count, i, period, retval, slp_ival;
    static int	EcDbgMaxDelay;

    ACPI_SERIAL_ASSERT(ec);
    Status = AE_NO_HARDWARE_RESPONSE;

    /* 
     * Wait for 1 us before checking the CSR.  Testing shows about
     * 50% of requests complete in 1 us and 90% of them complete
     * in 5 us or less.
     */
    AcpiOsStall(1);

    /*
     * Poll the EC status register for up to 1 ms in chunks of 10 us 
     * to detect completion of the last command.
     */
    for (i = 0; i < 1000 / EC_POLL_DELAY; i++) {
	EcStatus = EC_GET_CSR(sc);
	if (EVENT_READY(Event, EcStatus)) {
	    Status = AE_OK;
	    break;
	}
	AcpiOsStall(EC_POLL_DELAY);
    }
    period = i * EC_POLL_DELAY;

    /*
     * If we still don't have a response and we're up and running, wait up
     * to ec_poll_timeout ms for completion, sleeping for chunks of 10 ms.
     */
    slp_ival = 0;
    if (Status != AE_OK) {
	retval = ENXIO;
	count = ec_poll_timeout / 10;
	if (count == 0)
	    count = 1;
	slp_ival = hz / 100;
	if (slp_ival == 0)
	    slp_ival = 1;
	for (i = 0; i < count; i++) {
	    if (retval != 0)
		EcStatus = EC_GET_CSR(sc);
	    else
		EcStatus = sc->ec_csrvalue;
	    if (EVENT_READY(Event, EcStatus)) {
		Status = AE_OK;
		break;
	    }
	    if (!cold)
		retval = tsleep(&sc->ec_csrvalue, PZERO, "ecpoll", slp_ival);
	    else
		AcpiOsStall(10000);
	}
    }

    /* Calculate new delay and print it if it exceeds the max. */
    if (slp_ival > 0)
	period += i * 10000;
    if (period > EcDbgMaxDelay) {
	EcDbgMaxDelay = period;
	ACPI_VPRINT(sc->ec_dev, acpi_device_get_parent_softc(sc->ec_dev),
		    "info: new max delay is %d us\n", period);
    }

    return (Status);
}    

static ACPI_STATUS
EcCommand(struct acpi_ec_softc *sc, EC_COMMAND cmd)
{
    ACPI_STATUS	Status;
    EC_EVENT	Event;

    ACPI_SERIAL_ASSERT(ec);

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

    ACPI_SERIAL_ASSERT(ec);

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

    ACPI_SERIAL_ASSERT(ec);

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
