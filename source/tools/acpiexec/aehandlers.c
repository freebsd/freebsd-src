/******************************************************************************
 *
 * Module Name: aehandlers - Various handlers for acpiexec
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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
            AcpiUtGetNotifyName (Value, ACPI_TYPE_ANY));
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
        AcpiUtGetNotifyName (Value, ACPI_TYPE_ANY));
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
        AcpiUtGetNotifyName (Value, ACPI_TYPE_ANY));
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
    Arg[1].String.Length = strlen (Exception);

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


    AcpiOsPrintf ("[AcpiExec] GPE Handler received GPE %02X (GPE block %4.4s)\n",
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
    AeInstallRegionHandlers ();
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

        /* Test attach data at the root object */

        Status = AcpiAttachData (ACPI_ROOT_OBJECT, AeAttachedDataHandler,
            AcpiGbl_RootNode);
        AE_CHECK_OK (AcpiAttachData, Status);

        Status = AcpiAttachData (ACPI_ROOT_OBJECT, AeAttachedDataHandler2,
            AcpiGbl_RootNode);
        AE_CHECK_OK (AcpiAttachData, Status);

        /* Test support for multiple attaches */

        Status = AcpiAttachData (Handle, AeAttachedDataHandler, Handle);
        AE_CHECK_OK (AcpiAttachData, Status);

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
    AeOverrideRegionHandlers ();

    /*
     * Initialize the global Region Handler space
     * MCW 3/23/00
     */
    AeRegions.NumberOfRegions = 0;
    AeRegions.RegionList = NULL;
    return (AE_OK);
}
