/******************************************************************************
 *
 * Module Name: aehandlers - Various handlers for acpiexec
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "aecommon.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aehandlers")

/* Local prototypes */

static void
AeNotifyHandler1 (
    ACPI_HANDLE             Device,
    UINT32                  Value,
    void                    *Context);

static void
AeNotifyHandler2 (
    ACPI_HANDLE             Device,
    UINT32                  Value,
    void                    *Context);

static void
AeCommonNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  Value,
    UINT32                  HandlerId);

static void
AeDeviceNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  Value,
    void                    *Context);

static ACPI_STATUS
AeExceptionHandler (
    ACPI_STATUS             AmlStatus,
    ACPI_NAME               Name,
    UINT16                  Opcode,
    UINT32                  AmlOffset,
    void                    *Context);

static ACPI_STATUS
AeTableHandler (
    UINT32                  Event,
    void                    *Table,
    void                    *Context);

static ACPI_STATUS
AeRegionInit (
    ACPI_HANDLE             RegionHandle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext);

static void
AeAttachedDataHandler (
    ACPI_HANDLE             Object,
    void                    *Data);

static void
AeAttachedDataHandler2 (
    ACPI_HANDLE             Object,
    void                    *Data);

static UINT32
AeInterfaceHandler (
    ACPI_STRING             InterfaceName,
    UINT32                  Supported);

static ACPI_STATUS
AeInstallEcHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
AeInstallPciHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
AeInstallDeviceHandlers (
    void);

#if (!ACPI_REDUCED_HARDWARE)
static UINT32
AeEventHandler (
    void                    *Context);

static UINT32
AeSciHandler (
    void                    *Context);

static char                *TableEvents[] =
{
    "LOAD",
    "UNLOAD",
    "UNKNOWN"
};
#endif /* !ACPI_REDUCED_HARDWARE */


static UINT32               SigintCount = 0;
static AE_DEBUG_REGIONS     AeRegions;
BOOLEAN                     AcpiGbl_DisplayRegionAccess = FALSE;

/*
 * We will override some of the default region handlers, especially the
 * SystemMemory handler, which must be implemented locally. Do not override
 * the PCI_Config handler since we would like to exercise the default handler
 * code. These handlers are installed "early" - before any _REG methods
 * are executed - since they are special in the sense that the ACPI spec
 * declares that they must "always be available". Cannot override the
 * DataTable region handler either -- needed for test execution.
 */
static ACPI_ADR_SPACE_TYPE  DefaultSpaceIdList[] =
{
    ACPI_ADR_SPACE_SYSTEM_MEMORY,
    ACPI_ADR_SPACE_SYSTEM_IO
};

/*
 * We will install handlers for some of the various address space IDs.
 * Test one user-defined address space (used by aslts).
 */
#define ACPI_ADR_SPACE_USER_DEFINED1        0x80
#define ACPI_ADR_SPACE_USER_DEFINED2        0xE4

static ACPI_ADR_SPACE_TYPE  SpaceIdList[] =
{
    ACPI_ADR_SPACE_SMBUS,
    ACPI_ADR_SPACE_CMOS,
    ACPI_ADR_SPACE_PCI_BAR_TARGET,
    ACPI_ADR_SPACE_IPMI,
    ACPI_ADR_SPACE_GPIO,
    ACPI_ADR_SPACE_GSBUS,
    ACPI_ADR_SPACE_FIXED_HARDWARE,
    ACPI_ADR_SPACE_USER_DEFINED1,
    ACPI_ADR_SPACE_USER_DEFINED2
};

static ACPI_CONNECTION_INFO   AeMyContext;


/******************************************************************************
 *
 * FUNCTION:    AeCtrlCHandler
 *
 * PARAMETERS:  Sig
 *
 * RETURN:      none
 *
 * DESCRIPTION: Control-C handler. Abort running control method if any.
 *
 *****************************************************************************/

void ACPI_SYSTEM_XFACE
AeCtrlCHandler (
    int                     Sig)
{

    signal (SIGINT, SIG_IGN);
    SigintCount++;

    AcpiOsPrintf ("Caught a ctrl-c (#%u)\n\n", SigintCount);

    if (AcpiGbl_MethodExecuting)
    {
        AcpiGbl_AbortMethod = TRUE;
        signal (SIGINT, AeCtrlCHandler);

        if (SigintCount < 10)
        {
            return;
        }
    }

    (void) AcpiOsTerminate ();
    exit (0);
}


/******************************************************************************
 *
 * FUNCTION:    AeNotifyHandler(s)
 *
 * PARAMETERS:  Standard notify handler parameters
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Notify handlers for AcpiExec utility. Used by the ASL
 *              test suite(s) to communicate errors and other information to
 *              this utility via the Notify() operator. Tests notify handling
 *              and multiple notify handler support.
 *
 *****************************************************************************/

static void
AeNotifyHandler1 (
    ACPI_HANDLE             Device,
    UINT32                  Value,
    void                    *Context)
{
    AeCommonNotifyHandler (Device, Value, 1);
}

static void
AeNotifyHandler2 (
    ACPI_HANDLE             Device,
    UINT32                  Value,
    void                    *Context)
{
    AeCommonNotifyHandler (Device, Value, 2);
}

static void
AeCommonNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  Value,
    UINT32                  HandlerId)
{
    char                    *Type;


    Type = "Device";
    if (Value <= ACPI_MAX_SYS_NOTIFY)
    {
        Type = "System";
    }

    switch (Value)
    {
#if 0
    case 0:

        printf ("[AcpiExec] Method Error 0x%X: Results not equal\n", Value);
        if (AcpiGbl_DebugFile)
        {
            AcpiOsPrintf ("[AcpiExec] Method Error: Results not equal\n");
        }
        break;

    case 1:

        printf ("[AcpiExec] Method Error: Incorrect numeric result\n");
        if (AcpiGbl_DebugFile)
        {
            AcpiOsPrintf ("[AcpiExec] Method Error: Incorrect numeric result\n");
        }
        break;

    case 2:

        printf ("[AcpiExec] Method Error: An operand was overwritten\n");
        if (AcpiGbl_DebugFile)
        {
            AcpiOsPrintf ("[AcpiExec] Method Error: An operand was overwritten\n");
        }
        break;

#endif

    default:

        printf ("[AcpiExec] Handler %u: Received a %s Notify on [%4.4s] %p Value 0x%2.2X (%s)\n",
            HandlerId, Type, AcpiUtGetNodeName (Device), Device, Value,
            AcpiUtGetNotifyName (Value));
        if (AcpiGbl_DebugFile)
        {
            AcpiOsPrintf ("[AcpiExec] Handler %u: Received a %s notify, Value 0x%2.2X\n",
                HandlerId, Type, Value);
        }

        (void) AcpiEvaluateObject (Device, "_NOT", NULL, NULL);
        break;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AeSystemNotifyHandler
 *
 * PARAMETERS:  Standard notify handler parameters
 *
 * RETURN:      Status
 *
 * DESCRIPTION: System notify handler for AcpiExec utility. Used by the ASL
 *              test suite(s) to communicate errors and other information to
 *              this utility via the Notify() operator.
 *
 *****************************************************************************/

static void
AeSystemNotifyHandler (
    ACPI_HANDLE                 Device,
    UINT32                      Value,
    void                        *Context)
{

    printf ("[AcpiExec] Global:    Received a System Notify on [%4.4s] %p Value 0x%2.2X (%s)\n",
        AcpiUtGetNodeName (Device), Device, Value,
        AcpiUtGetNotifyName (Value));
    if (AcpiGbl_DebugFile)
    {
        AcpiOsPrintf ("[AcpiExec] Global:    Received a System Notify, Value 0x%2.2X\n", Value);
    }

    (void) AcpiEvaluateObject (Device, "_NOT", NULL, NULL);
}


/******************************************************************************
 *
 * FUNCTION:    AeDeviceNotifyHandler
 *
 * PARAMETERS:  Standard notify handler parameters
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Device notify handler for AcpiExec utility. Used by the ASL
 *              test suite(s) to communicate errors and other information to
 *              this utility via the Notify() operator.
 *
 *****************************************************************************/

static void
AeDeviceNotifyHandler (
    ACPI_HANDLE                 Device,
    UINT32                      Value,
    void                        *Context)
{

    printf ("[AcpiExec] Global:    Received a Device Notify on [%4.4s] %p Value 0x%2.2X (%s)\n",
        AcpiUtGetNodeName (Device), Device, Value,
        AcpiUtGetNotifyName (Value));
    if (AcpiGbl_DebugFile)
    {
        AcpiOsPrintf ("[AcpiExec] Global:    Received a Device Notify, Value 0x%2.2X\n", Value);
    }

    (void) AcpiEvaluateObject (Device, "_NOT", NULL, NULL);
}


/******************************************************************************
 *
 * FUNCTION:    AeExceptionHandler
 *
 * PARAMETERS:  Standard exception handler parameters
 *
 * RETURN:      Status
 *
 * DESCRIPTION: System exception handler for AcpiExec utility.
 *
 *****************************************************************************/

static ACPI_STATUS
AeExceptionHandler (
    ACPI_STATUS             AmlStatus,
    ACPI_NAME               Name,
    UINT16                  Opcode,
    UINT32                  AmlOffset,
    void                    *Context)
{
    ACPI_STATUS             NewAmlStatus = AmlStatus;
    ACPI_STATUS             Status;
    ACPI_BUFFER             ReturnObj;
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg[3];
    const char              *Exception;


    Exception = AcpiFormatException (AmlStatus);
    AcpiOsPrintf ("[AcpiExec] Exception %s during execution ", Exception);
    if (Name)
    {
        AcpiOsPrintf ("of method [%4.4s]", (char *) &Name);
    }
    else
    {
        AcpiOsPrintf ("at module level (table load)");
    }
    AcpiOsPrintf (" Opcode [%s] @%X\n", AcpiPsGetOpcodeName (Opcode), AmlOffset);

    /*
     * Invoke the _ERR method if present
     *
     * Setup parameter object
     */
    ArgList.Count = 3;
    ArgList.Pointer = Arg;

    Arg[0].Type = ACPI_TYPE_INTEGER;
    Arg[0].Integer.Value = AmlStatus;

    Arg[1].Type = ACPI_TYPE_STRING;
    Arg[1].String.Pointer = ACPI_CAST_PTR (char, Exception);
    Arg[1].String.Length = ACPI_STRLEN (Exception);

    Arg[2].Type = ACPI_TYPE_INTEGER;
    Arg[2].Integer.Value = AcpiOsGetThreadId();

    /* Setup return buffer */

    ReturnObj.Pointer = NULL;
    ReturnObj.Length = ACPI_ALLOCATE_BUFFER;

    Status = AcpiEvaluateObject (NULL, "\\_ERR", &ArgList, &ReturnObj);
    if (ACPI_SUCCESS (Status))
    {
        if (ReturnObj.Pointer)
        {
            /* Override original status */

            NewAmlStatus = (ACPI_STATUS)
                ((ACPI_OBJECT *) ReturnObj.Pointer)->Integer.Value;

            /* Free a buffer created via ACPI_ALLOCATE_BUFFER */

            AcpiOsFree (ReturnObj.Pointer);
        }
    }
    else if (Status != AE_NOT_FOUND)
    {
        AcpiOsPrintf ("[AcpiExec] Could not execute _ERR method, %s\n",
            AcpiFormatException (Status));
    }

    /* Global override */

    if (AcpiGbl_IgnoreErrors)
    {
        NewAmlStatus = AE_OK;
    }

    if (NewAmlStatus != AmlStatus)
    {
        AcpiOsPrintf ("[AcpiExec] Exception override, new status %s\n",
            AcpiFormatException (NewAmlStatus));
    }

    return (NewAmlStatus);
}


/******************************************************************************
 *
 * FUNCTION:    AeTableHandler
 *
 * PARAMETERS:  Table handler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: System table handler for AcpiExec utility.
 *
 *****************************************************************************/

static ACPI_STATUS
AeTableHandler (
    UINT32                  Event,
    void                    *Table,
    void                    *Context)
{
#if (!ACPI_REDUCED_HARDWARE)
    ACPI_STATUS             Status;
#endif /* !ACPI_REDUCED_HARDWARE */


    if (Event > ACPI_NUM_TABLE_EVENTS)
    {
        Event = ACPI_NUM_TABLE_EVENTS;
    }

#if (!ACPI_REDUCED_HARDWARE)
    /* Enable any GPEs associated with newly-loaded GPE methods */

    Status = AcpiUpdateAllGpes ();
    AE_CHECK_OK (AcpiUpdateAllGpes, Status);

    printf ("[AcpiExec] Table Event %s, [%4.4s] %p\n",
        TableEvents[Event], ((ACPI_TABLE_HEADER *) Table)->Signature, Table);
#endif /* !ACPI_REDUCED_HARDWARE */

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AeGpeHandler
 *
 * DESCRIPTION: Common GPE handler for acpiexec
 *
 *****************************************************************************/

UINT32
AeGpeHandler (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    void                    *Context)
{
    ACPI_NAMESPACE_NODE     *DeviceNode = (ACPI_NAMESPACE_NODE *) GpeDevice;


    AcpiOsPrintf ("[AcpiExec] GPE Handler received GPE%02X (GPE block %4.4s)\n",
        GpeNumber, GpeDevice ? DeviceNode->Name.Ascii : "FADT");

    return (ACPI_REENABLE_GPE);
}


/******************************************************************************
 *
 * FUNCTION:    AeGlobalEventHandler
 *
 * DESCRIPTION: Global GPE/Fixed event handler
 *
 *****************************************************************************/

void
AeGlobalEventHandler (
    UINT32                  Type,
    ACPI_HANDLE             Device,
    UINT32                  EventNumber,
    void                    *Context)
{
    char                    *TypeName;


    switch (Type)
    {
    case ACPI_EVENT_TYPE_GPE:

        TypeName = "GPE";
        break;

    case ACPI_EVENT_TYPE_FIXED:

        TypeName = "FixedEvent";
        break;

    default:

        TypeName = "UNKNOWN";
        break;
    }

    AcpiOsPrintf ("[AcpiExec] Global Event Handler received: Type %s Number %.2X Dev %p\n",
        TypeName, EventNumber, Device);
}


/******************************************************************************
 *
 * FUNCTION:    AeAttachedDataHandler
 *
 * DESCRIPTION: Handler for deletion of nodes with attached data (attached via
 *              AcpiAttachData)
 *
 *****************************************************************************/

static void
AeAttachedDataHandler (
    ACPI_HANDLE             Object,
    void                    *Data)
{
    ACPI_NAMESPACE_NODE     *Node = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, Data);


    AcpiOsPrintf ("Received an attached data deletion (1) on %4.4s\n",
        Node->Name.Ascii);
}


/******************************************************************************
 *
 * FUNCTION:    AeAttachedDataHandler2
 *
 * DESCRIPTION: Handler for deletion of nodes with attached data (attached via
 *              AcpiAttachData)
 *
 *****************************************************************************/

static void
AeAttachedDataHandler2 (
    ACPI_HANDLE             Object,
    void                    *Data)
{
    ACPI_NAMESPACE_NODE     *Node = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, Data);


    AcpiOsPrintf ("Received an attached data deletion (2) on %4.4s\n",
        Node->Name.Ascii);
}


/******************************************************************************
 *
 * FUNCTION:    AeInterfaceHandler
 *
 * DESCRIPTION: Handler for _OSI invocations
 *
 *****************************************************************************/

static UINT32
AeInterfaceHandler (
    ACPI_STRING             InterfaceName,
    UINT32                  Supported)
{
    ACPI_FUNCTION_NAME (AeInterfaceHandler);


    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "Received _OSI (\"%s\"), is %ssupported\n",
        InterfaceName, Supported == 0 ? "not " : ""));

    return (Supported);
}


#if (!ACPI_REDUCED_HARDWARE)
/******************************************************************************
 *
 * FUNCTION:    AeEventHandler, AeSciHandler
 *
 * DESCRIPTION: Handler for Fixed Events and SCIs
 *
 *****************************************************************************/

static UINT32
AeEventHandler (
    void                    *Context)
{
    return (0);
}

static UINT32
AeSciHandler (
    void                    *Context)
{

    AcpiOsPrintf ("[AcpiExec] Received an SCI at handler\n");
    return (0);
}

#endif /* !ACPI_REDUCED_HARDWARE */


/******************************************************************************
 *
 * FUNCTION:    AeRegionInit
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Opregion init function.
 *
 *****************************************************************************/

static ACPI_STATUS
AeRegionInit (
    ACPI_HANDLE                 RegionHandle,
    UINT32                      Function,
    void                        *HandlerContext,
    void                        **RegionContext)
{

    if (Function == ACPI_REGION_DEACTIVATE)
    {
        *RegionContext = NULL;
    }
    else
    {
        *RegionContext = RegionHandle;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AeInstallSciHandler
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install handler for SCIs. Exercise the code by doing an
 *              install/remove/install.
 *
 ******************************************************************************/

static ACPI_STATUS
AeInstallSciHandler (
    void)
{
    ACPI_STATUS             Status;


    Status = AcpiInstallSciHandler (AeSciHandler, &AeMyContext);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "Could not install an SCI handler (1)"));
    }

    Status = AcpiRemoveSciHandler (AeSciHandler);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "Could not remove an SCI handler"));
    }

    Status = AcpiInstallSciHandler (AeSciHandler, &AeMyContext);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "Could not install an SCI handler (2)"));
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AeInstallDeviceHandlers, AeInstallEcHandler,
 *              AeInstallPciHandler
 *
 * PARAMETERS:  ACPI_WALK_NAMESPACE callback
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk entire namespace, install a handler for every EC
 *              and PCI device found.
 *
 ******************************************************************************/

static ACPI_STATUS
AeInstallEcHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;


    /* Install the handler for this EC device */

    Status = AcpiInstallAddressSpaceHandler (ObjHandle, ACPI_ADR_SPACE_EC,
        AeRegionHandler, AeRegionInit, &AeMyContext);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "Could not install an OpRegion handler for EC device (%p)",
            ObjHandle));
    }

    return (Status);
}

static ACPI_STATUS
AeInstallPciHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;


    /* Install memory and I/O handlers for the PCI device */

    Status = AcpiInstallAddressSpaceHandler (ObjHandle, ACPI_ADR_SPACE_SYSTEM_IO,
        AeRegionHandler, AeRegionInit, &AeMyContext);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "Could not install an OpRegion handler for PCI device (%p)",
            ObjHandle));
    }

    Status = AcpiInstallAddressSpaceHandler (ObjHandle, ACPI_ADR_SPACE_SYSTEM_MEMORY,
        AeRegionHandler, AeRegionInit, &AeMyContext);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "Could not install an OpRegion handler for PCI device (%p)",
            ObjHandle));
    }

    return (AE_CTRL_TERMINATE);
}

static ACPI_STATUS
AeInstallDeviceHandlers (
    void)
{

    /* Find all Embedded Controller devices */

    AcpiGetDevices ("PNP0C09", AeInstallEcHandler, NULL, NULL);

    /* Install a PCI handler */

    AcpiGetDevices ("PNP0A08", AeInstallPciHandler, NULL, NULL);
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AeInstallLateHandlers
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install handlers for the AcpiExec utility.
 *
 *****************************************************************************/

ACPI_STATUS
AeInstallLateHandlers (
    void)
{
    ACPI_STATUS             Status;
    UINT32                  i;


#if (!ACPI_REDUCED_HARDWARE)
    if (!AcpiGbl_ReducedHardware)
    {
        /* Install a user SCI handler */

        Status = AeInstallSciHandler ();
        AE_CHECK_OK (AeInstallSciHandler, Status);

        /* Install some fixed event handlers */

        Status = AcpiInstallFixedEventHandler (ACPI_EVENT_GLOBAL, AeEventHandler, NULL);
        AE_CHECK_OK (AcpiInstallFixedEventHandler, Status);

        Status = AcpiInstallFixedEventHandler (ACPI_EVENT_RTC, AeEventHandler, NULL);
        AE_CHECK_OK (AcpiInstallFixedEventHandler, Status);
    }
#endif /* !ACPI_REDUCED_HARDWARE */

    AeMyContext.Connection = NULL;
    AeMyContext.AccessLength = 0xA5;

    /*
     * We will install a handler for each EC device, directly under the EC
     * device definition. This is unlike the other handlers which we install
     * at the root node. Also install memory and I/O handlers at any PCI
     * devices.
     */
    AeInstallDeviceHandlers ();

    /*
     * Install handlers for some of the "device driver" address spaces
     * such as SMBus, etc.
     */
    for (i = 0; i < ACPI_ARRAY_LENGTH (SpaceIdList); i++)
    {
        /* Install handler at the root object */

        Status = AcpiInstallAddressSpaceHandler (ACPI_ROOT_OBJECT,
                    SpaceIdList[i], AeRegionHandler,
                    AeRegionInit, &AeMyContext);
        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "Could not install an OpRegion handler for %s space(%u)",
                AcpiUtGetRegionName((UINT8) SpaceIdList[i]), SpaceIdList[i]));
            return (Status);
        }
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AeInstallEarlyHandlers
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install handlers for the AcpiExec utility.
 *
 * Notes:       Don't install handler for PCI_Config, we want to use the
 *              default handler to exercise that code.
 *
 *****************************************************************************/

ACPI_STATUS
AeInstallEarlyHandlers (
    void)
{
    ACPI_STATUS             Status;
    UINT32                  i;
    ACPI_HANDLE             Handle;


    ACPI_FUNCTION_ENTRY ();


    Status = AcpiInstallInterfaceHandler (AeInterfaceHandler);
    if (ACPI_FAILURE (Status))
    {
        printf ("Could not install interface handler, %s\n",
            AcpiFormatException (Status));
    }

    Status = AcpiInstallTableHandler (AeTableHandler, NULL);
    if (ACPI_FAILURE (Status))
    {
        printf ("Could not install table handler, %s\n",
            AcpiFormatException (Status));
    }

    Status = AcpiInstallExceptionHandler (AeExceptionHandler);
    if (ACPI_FAILURE (Status))
    {
        printf ("Could not install exception handler, %s\n",
            AcpiFormatException (Status));
    }

    /* Install global notify handlers */

    Status = AcpiInstallNotifyHandler (ACPI_ROOT_OBJECT, ACPI_SYSTEM_NOTIFY,
        AeSystemNotifyHandler, NULL);
    if (ACPI_FAILURE (Status))
    {
        printf ("Could not install a global system notify handler, %s\n",
            AcpiFormatException (Status));
    }

    Status = AcpiInstallNotifyHandler (ACPI_ROOT_OBJECT, ACPI_DEVICE_NOTIFY,
        AeDeviceNotifyHandler, NULL);
    if (ACPI_FAILURE (Status))
    {
        printf ("Could not install a global notify handler, %s\n",
            AcpiFormatException (Status));
    }

    Status = AcpiGetHandle (NULL, "\\_SB", &Handle);
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiInstallNotifyHandler (Handle, ACPI_SYSTEM_NOTIFY,
            AeNotifyHandler1, NULL);
        if (ACPI_FAILURE (Status))
        {
            printf ("Could not install a notify handler, %s\n",
                AcpiFormatException (Status));
        }

        Status = AcpiRemoveNotifyHandler (Handle, ACPI_SYSTEM_NOTIFY,
            AeNotifyHandler1);
        if (ACPI_FAILURE (Status))
        {
            printf ("Could not remove a notify handler, %s\n",
                AcpiFormatException (Status));
        }

        Status = AcpiInstallNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler1, NULL);
        AE_CHECK_OK (AcpiInstallNotifyHandler, Status);

        Status = AcpiRemoveNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler1);
        AE_CHECK_OK (AcpiRemoveNotifyHandler, Status);

#if 0
        Status = AcpiInstallNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler1, NULL);
        if (ACPI_FAILURE (Status))
        {
            printf ("Could not install a notify handler, %s\n",
                AcpiFormatException (Status));
        }
#endif

        /* Install two handlers for _SB_ */

        Status = AcpiInstallNotifyHandler (Handle, ACPI_SYSTEM_NOTIFY,
            AeNotifyHandler1, ACPI_CAST_PTR (void, 0x01234567));

        Status = AcpiInstallNotifyHandler (Handle, ACPI_SYSTEM_NOTIFY,
            AeNotifyHandler2, ACPI_CAST_PTR (void, 0x89ABCDEF));

        /* Attempt duplicate handler installation, should fail */

        Status = AcpiInstallNotifyHandler (Handle, ACPI_SYSTEM_NOTIFY,
            AeNotifyHandler1, ACPI_CAST_PTR (void, 0x77777777));

        Status = AcpiAttachData (Handle, AeAttachedDataHandler, Handle);
        AE_CHECK_OK (AcpiAttachData, Status);

        Status = AcpiDetachData (Handle, AeAttachedDataHandler);
        AE_CHECK_OK (AcpiDetachData, Status);

        Status = AcpiAttachData (Handle, AeAttachedDataHandler, Handle);
        AE_CHECK_OK (AcpiAttachData, Status);

        /* Test support for multiple attaches */

        Status = AcpiAttachData (Handle, AeAttachedDataHandler2, Handle);
        AE_CHECK_OK (AcpiAttachData, Status);
    }
    else
    {
        printf ("No _SB_ found, %s\n", AcpiFormatException (Status));
    }


    Status = AcpiGetHandle (NULL, "\\_TZ.TZ1", &Handle);
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiInstallNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler1, ACPI_CAST_PTR (void, 0x01234567));

        Status = AcpiInstallNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler2, ACPI_CAST_PTR (void, 0x89ABCDEF));

        Status = AcpiRemoveNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler1);
        Status = AcpiRemoveNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler2);

        Status = AcpiInstallNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler2, ACPI_CAST_PTR (void, 0x89ABCDEF));

        Status = AcpiInstallNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler1, ACPI_CAST_PTR (void, 0x01234567));
    }

    Status = AcpiGetHandle (NULL, "\\_PR.CPU0", &Handle);
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiInstallNotifyHandler (Handle, ACPI_ALL_NOTIFY,
            AeNotifyHandler1, ACPI_CAST_PTR (void, 0x01234567));

        Status = AcpiInstallNotifyHandler (Handle, ACPI_SYSTEM_NOTIFY,
            AeNotifyHandler2, ACPI_CAST_PTR (void, 0x89ABCDEF));
    }

    /*
     * Install handlers that will override the default handlers for some of
     * the space IDs.
     */
    for (i = 0; i < ACPI_ARRAY_LENGTH (DefaultSpaceIdList); i++)
    {
        /* Install handler at the root object */

        Status = AcpiInstallAddressSpaceHandler (ACPI_ROOT_OBJECT,
                    DefaultSpaceIdList[i], AeRegionHandler,
                    AeRegionInit, &AeMyContext);
        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "Could not install a default OpRegion handler for %s space(%u)",
                AcpiUtGetRegionName ((UINT8) DefaultSpaceIdList[i]),
                DefaultSpaceIdList[i]));
            return (Status);
        }
    }

    /*
     * Initialize the global Region Handler space
     * MCW 3/23/00
     */
    AeRegions.NumberOfRegions = 0;
    AeRegions.RegionList = NULL;
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AeRegionHandler
 *
 * PARAMETERS:  Standard region handler parameters
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Test handler - Handles some dummy regions via memory that can
 *              be manipulated in Ring 3. Simulates actual reads and writes.
 *
 *****************************************************************************/

ACPI_STATUS
AeRegionHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT64                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{

    ACPI_OPERAND_OBJECT     *RegionObject = ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, RegionContext);
    UINT8                   *Buffer = ACPI_CAST_PTR (UINT8, Value);
    ACPI_PHYSICAL_ADDRESS   BaseAddress;
    ACPI_SIZE               Length;
    BOOLEAN                 BufferExists;
    AE_REGION               *RegionElement;
    void                    *BufferValue;
    ACPI_STATUS             Status;
    UINT32                  ByteWidth;
    UINT32                  i;
    UINT8                   SpaceId;
    ACPI_CONNECTION_INFO    *MyContext;
    UINT32                  Value1;
    UINT32                  Value2;
    ACPI_RESOURCE           *Resource;


    ACPI_FUNCTION_NAME (AeRegionHandler);

    /*
     * If the object is not a region, simply return
     */
    if (RegionObject->Region.Type != ACPI_TYPE_REGION)
    {
        return (AE_OK);
    }

    /* Check that we actually got back our context parameter */

    if (HandlerContext != &AeMyContext)
    {
        printf ("Region handler received incorrect context %p, should be %p\n",
            HandlerContext, &AeMyContext);
    }

    MyContext = ACPI_CAST_PTR (ACPI_CONNECTION_INFO, HandlerContext);

    /*
     * Find the region's address space and length before searching
     * the linked list.
     */
    BaseAddress = RegionObject->Region.Address;
    Length = (ACPI_SIZE) RegionObject->Region.Length;
    SpaceId = RegionObject->Region.SpaceId;

    ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION, "Operation Region request on %s at 0x%X\n",
            AcpiUtGetRegionName (RegionObject->Region.SpaceId),
            (UINT32) Address));

    /*
     * Region support can be disabled with the -do option.
     * We use this to support dynamically loaded tables where we pass a valid
     * address to the AML.
     */
    if (AcpiGbl_DbOpt_NoRegionSupport)
    {
        BufferValue = ACPI_TO_POINTER (Address);
        ByteWidth = (BitWidth / 8);

        if (BitWidth % 8)
        {
            ByteWidth += 1;
        }
        goto DoFunction;
    }

    switch (SpaceId)
    {
    case ACPI_ADR_SPACE_SYSTEM_IO:
        /*
         * For I/O space, exercise the port validation
         * Note: ReadPort currently always returns all ones, length=BitLength
         */
        switch (Function & ACPI_IO_MASK)
        {
        case ACPI_READ:

            if (BitWidth == 64)
            {
                /* Split the 64-bit request into two 32-bit requests */

                Status = AcpiHwReadPort (Address, &Value1, 32);
                AE_CHECK_OK (AcpiHwReadPort, Status);
                Status = AcpiHwReadPort (Address+4, &Value2, 32);
                AE_CHECK_OK (AcpiHwReadPort, Status);

                *Value = Value1 | ((UINT64) Value2 << 32);
            }
            else
            {
                Status = AcpiHwReadPort (Address, &Value1, BitWidth);
                AE_CHECK_OK (AcpiHwReadPort, Status);
                *Value = (UINT64) Value1;
            }
            break;

        case ACPI_WRITE:

            if (BitWidth == 64)
            {
                /* Split the 64-bit request into two 32-bit requests */

                Status = AcpiHwWritePort (Address, ACPI_LODWORD (*Value), 32);
                AE_CHECK_OK (AcpiHwWritePort, Status);
                Status = AcpiHwWritePort (Address+4, ACPI_HIDWORD (*Value), 32);
                AE_CHECK_OK (AcpiHwWritePort, Status);
            }
            else
            {
                Status = AcpiHwWritePort (Address, (UINT32) *Value, BitWidth);
                AE_CHECK_OK (AcpiHwWritePort, Status);
            }
            break;

        default:

            Status = AE_BAD_PARAMETER;
            break;
        }

        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Now go ahead and simulate the hardware */
        break;

    /*
     * SMBus and GenericSerialBus support the various bidirectional
     * protocols.
     */
    case ACPI_ADR_SPACE_SMBUS:
    case ACPI_ADR_SPACE_GSBUS:  /* ACPI 5.0 */

        Length = 0;

        switch (Function & ACPI_IO_MASK)
        {
        case ACPI_READ:

            switch (Function >> 16)
            {
            case AML_FIELD_ATTRIB_QUICK:
            case AML_FIELD_ATTRIB_SEND_RCV:
            case AML_FIELD_ATTRIB_BYTE:

                Length = 1;
                break;

            case AML_FIELD_ATTRIB_WORD:
            case AML_FIELD_ATTRIB_WORD_CALL:

                Length = 2;
                break;

            case AML_FIELD_ATTRIB_BLOCK:
            case AML_FIELD_ATTRIB_BLOCK_CALL:

                Length = 32;
                break;

            case AML_FIELD_ATTRIB_MULTIBYTE:
            case AML_FIELD_ATTRIB_RAW_BYTES:
            case AML_FIELD_ATTRIB_RAW_PROCESS:

                /* (-2) for status/length */
                Length = MyContext->AccessLength - 2;
                break;

            default:

                break;
            }
            break;

        case ACPI_WRITE:

            switch (Function >> 16)
            {
            case AML_FIELD_ATTRIB_QUICK:
            case AML_FIELD_ATTRIB_SEND_RCV:
            case AML_FIELD_ATTRIB_BYTE:
            case AML_FIELD_ATTRIB_WORD:
            case AML_FIELD_ATTRIB_BLOCK:

                Length = 0;
                break;

            case AML_FIELD_ATTRIB_WORD_CALL:
                Length = 2;
                break;

            case AML_FIELD_ATTRIB_BLOCK_CALL:
                Length = 32;
                break;

            case AML_FIELD_ATTRIB_MULTIBYTE:
            case AML_FIELD_ATTRIB_RAW_BYTES:
            case AML_FIELD_ATTRIB_RAW_PROCESS:

                /* (-2) for status/length */
                Length = MyContext->AccessLength - 2;
                break;

            default:

                break;
            }
            break;

        default:

            break;
        }

        if (AcpiGbl_DisplayRegionAccess)
        {
            AcpiOsPrintf ("AcpiExec: %s "
                "%s: Attr %X Addr %.4X BaseAddr %.4X Len %.2X Width %X BufLen %X",
                AcpiUtGetRegionName (SpaceId),
                (Function & ACPI_IO_MASK) ? "Write" : "Read ",
                (UINT32) (Function >> 16),
                (UINT32) Address, (UINT32) BaseAddress,
                Length, BitWidth, Buffer[1]);

            /* GenericSerialBus has a Connection() parameter */

            if (SpaceId == ACPI_ADR_SPACE_GSBUS)
            {
                Status = AcpiBufferToResource (MyContext->Connection,
                    MyContext->Length, &Resource);

                AcpiOsPrintf (" [AccLen %.2X Conn %p]",
                    MyContext->AccessLength, MyContext->Connection);
            }
            AcpiOsPrintf ("\n");
        }

        /* Setup the return buffer. Note: ASLTS depends on these fill values */

        for (i = 0; i < Length; i++)
        {
            Buffer[i+2] = (UINT8) (0xA0 + i);
        }

        Buffer[0] = 0x7A;
        Buffer[1] = (UINT8) Length;
        return (AE_OK);


    case ACPI_ADR_SPACE_IPMI: /* ACPI 4.0 */

        if (AcpiGbl_DisplayRegionAccess)
        {
            AcpiOsPrintf ("AcpiExec: IPMI "
                "%s: Attr %X Addr %.4X BaseAddr %.4X Len %.2X Width %X BufLen %X\n",
                (Function & ACPI_IO_MASK) ? "Write" : "Read ",
                (UINT32) (Function >> 16), (UINT32) Address, (UINT32) BaseAddress,
                Length, BitWidth, Buffer[1]);
        }

        /*
         * Regardless of a READ or WRITE, this handler is passed a 66-byte
         * buffer in which to return the IPMI status/length/data.
         *
         * Return some example data to show use of the bidirectional buffer
         */
        Buffer[0] = 0;       /* Status byte */
        Buffer[1] = 64;      /* Return buffer data length */
        Buffer[2] = 0;       /* Completion code */
        Buffer[3] = 0;       /* Reserved */

        /*
         * Fill the 66-byte buffer with the return data.
         * Note: ASLTS depends on these fill values.
         */
        for (i = 4; i < 66; i++)
        {
            Buffer[i] = (UINT8) (i);
        }
        return (AE_OK);

    default:
        break;
    }

    /*
     * Search through the linked list for this region's buffer
     */
    BufferExists = FALSE;
    RegionElement = AeRegions.RegionList;

    if (AeRegions.NumberOfRegions)
    {
        while (!BufferExists && RegionElement)
        {
            if (RegionElement->Address == BaseAddress &&
                RegionElement->Length == Length &&
                RegionElement->SpaceId == SpaceId)
            {
                BufferExists = TRUE;
            }
            else
            {
                RegionElement = RegionElement->NextRegion;
            }
        }
    }

    /*
     * If the Region buffer does not exist, create it now
     */
    if (!BufferExists)
    {
        /*
         * Do the memory allocations first
         */
        RegionElement = AcpiOsAllocate (sizeof (AE_REGION));
        if (!RegionElement)
        {
            return (AE_NO_MEMORY);
        }

        RegionElement->Buffer = AcpiOsAllocate (Length);
        if (!RegionElement->Buffer)
        {
            AcpiOsFree (RegionElement);
            return (AE_NO_MEMORY);
        }

        /* Initialize the region with the default fill value */

        ACPI_MEMSET (RegionElement->Buffer, AcpiGbl_RegionFillValue, Length);

        RegionElement->Address      = BaseAddress;
        RegionElement->Length       = Length;
        RegionElement->SpaceId      = SpaceId;
        RegionElement->NextRegion   = NULL;

        /*
         * Increment the number of regions and put this one
         *  at the head of the list as it will probably get accessed
         *  more often anyway.
         */
        AeRegions.NumberOfRegions += 1;

        if (AeRegions.RegionList)
        {
            RegionElement->NextRegion = AeRegions.RegionList;
        }

        AeRegions.RegionList = RegionElement;
    }

    /*
     * Calculate the size of the memory copy
     */
    ByteWidth = (BitWidth / 8);

    if (BitWidth % 8)
    {
        ByteWidth += 1;
    }

    /*
     * The buffer exists and is pointed to by RegionElement.
     * We now need to verify the request is valid and perform the operation.
     *
     * NOTE: RegionElement->Length is in bytes, therefore it we compare against
     * ByteWidth (see above)
     */
    if (((UINT64) Address + ByteWidth) >
        ((UINT64)(RegionElement->Address) + RegionElement->Length))
    {
        ACPI_WARNING ((AE_INFO,
            "Request on [%4.4s] is beyond region limit Req-0x%X+0x%X, Base=0x%X, Len-0x%X",
            (RegionObject->Region.Node)->Name.Ascii, (UINT32) Address,
            ByteWidth, (UINT32)(RegionElement->Address),
            RegionElement->Length));

        return (AE_AML_REGION_LIMIT);
    }

    /*
     * Get BufferValue to point to the "address" in the buffer
     */
    BufferValue = ((UINT8 *) RegionElement->Buffer +
                    ((UINT64) Address - (UINT64) RegionElement->Address));

DoFunction:
    /*
     * Perform a read or write to the buffer space
     */
    switch (Function)
    {
    case ACPI_READ:
        /*
         * Set the pointer Value to whatever is in the buffer
         */
        ACPI_MEMCPY (Value, BufferValue, ByteWidth);
        break;

    case ACPI_WRITE:
        /*
         * Write the contents of Value to the buffer
         */
        ACPI_MEMCPY (BufferValue, Value, ByteWidth);
        break;

    default:

        return (AE_BAD_PARAMETER);
    }

    if (AcpiGbl_DisplayRegionAccess)
    {
        switch (SpaceId)
        {
        case ACPI_ADR_SPACE_SYSTEM_MEMORY:

            AcpiOsPrintf ("AcpiExec: SystemMemory "
                "%s: Val %.8X Addr %.4X Width %X [REGION: BaseAddr %.4X Len %.2X]\n",
                (Function & ACPI_IO_MASK) ? "Write" : "Read ",
                (UINT32) *Value, (UINT32) Address, BitWidth, (UINT32) BaseAddress, Length);
            break;

        case ACPI_ADR_SPACE_GPIO:   /* ACPI 5.0 */

            /* This space is required to always be ByteAcc */

            Status = AcpiBufferToResource (MyContext->Connection,
                MyContext->Length, &Resource);

            AcpiOsPrintf ("AcpiExec: GeneralPurposeIo "
                "%s: Val %.8X Addr %.4X BaseAddr %.4X Len %.2X Width %X AccLen %.2X Conn %p\n",
                (Function & ACPI_IO_MASK) ? "Write" : "Read ", (UINT32) *Value,
                (UINT32) Address, (UINT32) BaseAddress, Length, BitWidth,
                MyContext->AccessLength, MyContext->Connection);
            break;

        default:

            break;
        }
    }

    return (AE_OK);
}
