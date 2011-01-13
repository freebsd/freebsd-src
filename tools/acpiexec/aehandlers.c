/******************************************************************************
 *
 * Module Name: aehandlers - Various handlers for acpiexec
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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
AeNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  Value,
    void                    *Context);

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

static UINT32
AeInterfaceHandler (
    ACPI_STRING             InterfaceName,
    UINT32                  Supported);

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
 * DESCRIPTION: Control-C handler.  Abort running control method if any.
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

    exit (0);
}


/******************************************************************************
 *
 * FUNCTION:    AeNotifyHandler
 *
 * PARAMETERS:  Standard notify handler parameters
 *
 * RETURN:      Status
 *
 * DESCRIPTION: System notify handler for AcpiExec utility.  Used by the ASL
 *              test suite(s) to communicate errors and other information to
 *              this utility via the Notify() operator.
 *
 *****************************************************************************/

static void
AeNotifyHandler (
    ACPI_HANDLE                 Device,
    UINT32                      Value,
    void                        *Context)
{

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
        printf ("[AcpiExec] Received a System Notify on [%4.4s] %p Value 0x%2.2X (%s)\n",
            AcpiUtGetNodeName (Device), Device, Value,
            AcpiUtGetNotifyName (Value));
        if (AcpiGbl_DebugFile)
        {
            AcpiOsPrintf ("[AcpiExec] Received a system notify, Value 0x%2.2X\n", Value);
        }

        (void) AcpiEvaluateObject (Device, "_NOT", NULL, NULL);
        break;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AeDeviceNotifyHandler
 *
 * PARAMETERS:  Standard notify handler parameters
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Device notify handler for AcpiExec utility.  Used by the ASL
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

    printf ("[AcpiExec] Received a Device Notify on [%4.4s] %p Value 0x%2.2X (%s)\n",
        AcpiUtGetNodeName (Device), Device, Value,
        AcpiUtGetNotifyName (Value));
    if (AcpiGbl_DebugFile)
    {
        AcpiOsPrintf ("[AcpiExec] Received a device notify, Value 0x%2.2X\n", Value);
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

static char                *TableEvents[] =
{
    "LOAD",
    "UNLOAD",
    "UNKNOWN"
};

static ACPI_STATUS
AeTableHandler (
    UINT32                  Event,
    void                    *Table,
    void                    *Context)
{
    ACPI_STATUS             Status;


    if (Event > ACPI_NUM_TABLE_EVENTS)
    {
        Event = ACPI_NUM_TABLE_EVENTS;
    }

    /* Enable any GPEs associated with newly-loaded GPE methods */

    Status = AcpiUpdateAllGpes ();
    AE_CHECK_OK (AcpiUpdateAllGpes, Status);

    printf ("[AcpiExec] Table Event %s, [%4.4s] %p\n",
        TableEvents[Event], ((ACPI_TABLE_HEADER *) Table)->Signature, Table);
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


    AcpiOsPrintf ("Received an attached data deletion on %4.4s\n",
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
    /*
     * Real simple, set the RegionContext to the RegionHandle
     */
    *RegionContext = RegionHandle;

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AeInstallHandlers
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

static ACPI_ADR_SPACE_TYPE  SpaceIdList[] = {0, 1, 3, 4, 5, 6, 7, 0x80};
#define AEXEC_NUM_REGIONS   8

ACPI_STATUS
AeInstallHandlers (void)
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

    /* Install global notify handler */

    Status = AcpiInstallNotifyHandler (ACPI_ROOT_OBJECT, ACPI_SYSTEM_NOTIFY,
                                        AeNotifyHandler, NULL);
    if (ACPI_FAILURE (Status))
    {
        printf ("Could not install a global notify handler, %s\n",
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
                                            AeNotifyHandler, NULL);
        if (ACPI_FAILURE (Status))
        {
            printf ("Could not install a notify handler, %s\n",
                AcpiFormatException (Status));
        }

        Status = AcpiRemoveNotifyHandler (Handle, ACPI_SYSTEM_NOTIFY,
                                            AeNotifyHandler);
        if (ACPI_FAILURE (Status))
        {
            printf ("Could not remove a notify handler, %s\n",
                AcpiFormatException (Status));
        }

        Status = AcpiInstallNotifyHandler (Handle, ACPI_ALL_NOTIFY,
                                            AeNotifyHandler, NULL);
        AE_CHECK_OK (AcpiInstallNotifyHandler, Status);

        Status = AcpiRemoveNotifyHandler (Handle, ACPI_ALL_NOTIFY,
                                            AeNotifyHandler);
        AE_CHECK_OK (AcpiRemoveNotifyHandler, Status);

        Status = AcpiInstallNotifyHandler (Handle, ACPI_ALL_NOTIFY,
                                            AeNotifyHandler, NULL);
        if (ACPI_FAILURE (Status))
        {
            printf ("Could not install a notify handler, %s\n",
                AcpiFormatException (Status));
        }

        Status = AcpiAttachData (Handle, AeAttachedDataHandler, Handle);
        AE_CHECK_OK (AcpiAttachData, Status);

        Status = AcpiDetachData (Handle, AeAttachedDataHandler);
        AE_CHECK_OK (AcpiDetachData, Status);

        Status = AcpiAttachData (Handle, AeAttachedDataHandler, Handle);
        AE_CHECK_OK (AcpiAttachData, Status);
    }
    else
    {
        printf ("No _SB_ found, %s\n", AcpiFormatException (Status));
    }

    /* Set a handler for all supported operation regions */

    for (i = 0; i < AEXEC_NUM_REGIONS; i++)
    {
        /* Remove any existing handler */

        (void) AcpiRemoveAddressSpaceHandler (AcpiGbl_RootNode,
                    SpaceIdList[i], AeRegionHandler);

        /* Install handler at the root object.
         * TBD: all default handlers should be installed here!
         */
        Status = AcpiInstallAddressSpaceHandler (AcpiGbl_RootNode,
                        SpaceIdList[i], AeRegionHandler, AeRegionInit, NULL);
        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "Could not install an OpRegion handler for %s space(%u)",
                AcpiUtGetRegionName((UINT8) SpaceIdList[i]), SpaceIdList[i]));
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


    ACPI_FUNCTION_NAME (AeRegionHandler);

    /*
     * If the object is not a region, simply return
     */
    if (RegionObject->Region.Type != ACPI_TYPE_REGION)
    {
        return (AE_OK);
    }

    /*
     * Region support can be disabled with the -r option.
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

    switch (SpaceId)
    {
    case ACPI_ADR_SPACE_SYSTEM_IO:
        /*
         * For I/O space, exercise the port validation
         */
        switch (Function & ACPI_IO_MASK)
        {
        case ACPI_READ:
            Status = AcpiHwReadPort (Address, (UINT32 *) Value, BitWidth);
            AE_CHECK_OK (AcpiHwReadPort, Status);
            break;

        case ACPI_WRITE:
            Status = AcpiHwWritePort (Address, (UINT32) *Value, BitWidth);
            AE_CHECK_OK (AcpiHwWritePort, Status);
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


    case ACPI_ADR_SPACE_SMBUS:

        Length = 0;

        switch (Function & ACPI_IO_MASK)
        {
        case ACPI_READ:
            switch (Function >> 16)
            {
            case AML_FIELD_ATTRIB_SMB_QUICK:
            case AML_FIELD_ATTRIB_SMB_SEND_RCV:
            case AML_FIELD_ATTRIB_SMB_BYTE:
                Length = 1;
                break;

            case AML_FIELD_ATTRIB_SMB_WORD:
            case AML_FIELD_ATTRIB_SMB_WORD_CALL:
                Length = 2;
                break;

            case AML_FIELD_ATTRIB_SMB_BLOCK:
            case AML_FIELD_ATTRIB_SMB_BLOCK_CALL:
                Length = 32;
                break;

            default:
                break;
            }
            break;

        case ACPI_WRITE:
            switch (Function >> 16)
            {
            case AML_FIELD_ATTRIB_SMB_QUICK:
            case AML_FIELD_ATTRIB_SMB_SEND_RCV:
            case AML_FIELD_ATTRIB_SMB_BYTE:
            case AML_FIELD_ATTRIB_SMB_WORD:
            case AML_FIELD_ATTRIB_SMB_BLOCK:
                Length = 0;
                break;

            case AML_FIELD_ATTRIB_SMB_WORD_CALL:
                Length = 2;
                break;

            case AML_FIELD_ATTRIB_SMB_BLOCK_CALL:
                Length = 32;
                break;

            default:
                break;
            }
            break;

        default:
            break;
        }

        for (i = 0; i < Length; i++)
        {
            Buffer[i+2] = (UINT8) (0xA0 + i);
        }

        Buffer[0] = 0x7A;
        Buffer[1] = (UINT8) Length;
        return (AE_OK);


    case ACPI_ADR_SPACE_IPMI: /* ACPI 4.0 */

        AcpiOsPrintf ("AcpiExec: Received IPMI request: "
            "Address %X BaseAddress %X Length %X Width %X BufferLength %u\n",
            (UINT32) Address, (UINT32) BaseAddress,
            Length, BitWidth, Buffer[1]);

        /*
         * Regardless of a READ or WRITE, this handler is passed a 66-byte
         * buffer in which to return the IPMI status/length/data.
         *
         * Return some example data to show use of the bidirectional buffer
         */
        Buffer[0] = 0;       /* Status byte */
        Buffer[1] = 64;      /* Return buffer data length */
        Buffer[2] = 0;       /* Completion code */
        Buffer[3] = 0x34;    /* Power measurement */
        Buffer[4] = 0x12;    /* Power measurement */
        Buffer[65] = 0xEE;   /* last buffer byte */
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

    return (AE_OK);
}


