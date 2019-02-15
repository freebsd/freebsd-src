/*******************************************************************************
 *
 * Module Name: dbcmds - Miscellaneous debug commands and output routines
 *
 ******************************************************************************/

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


#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acevents.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acresrc.h>
#include <contrib/dev/acpica/include/actables.h>

#ifdef ACPI_DEBUGGER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbcmds")


/* Local prototypes */

static void
AcpiDmCompareAmlResources (
    UINT8                   *Aml1Buffer,
    ACPI_RSDESC_SIZE        Aml1BufferLength,
    UINT8                   *Aml2Buffer,
    ACPI_RSDESC_SIZE        Aml2BufferLength);

static ACPI_STATUS
AcpiDmTestResourceConversion (
    ACPI_NAMESPACE_NODE     *Node,
    char                    *Name);

static ACPI_STATUS
AcpiDbResourceCallback (
    ACPI_RESOURCE           *Resource,
    void                    *Context);

static ACPI_STATUS
AcpiDbDeviceResources (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

static void
AcpiDbDoOneSleepState (
    UINT8                   SleepState);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbConvertToNode
 *
 * PARAMETERS:  InString            - String to convert
 *
 * RETURN:      Pointer to a NS node
 *
 * DESCRIPTION: Convert a string to a valid NS pointer. Handles numeric or
 *              alphanumeric strings.
 *
 ******************************************************************************/

ACPI_NAMESPACE_NODE *
AcpiDbConvertToNode (
    char                    *InString)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_SIZE               Address;


    if ((*InString >= 0x30) && (*InString <= 0x39))
    {
        /* Numeric argument, convert */

        Address = ACPI_STRTOUL (InString, NULL, 16);
        Node = ACPI_TO_POINTER (Address);
        if (!AcpiOsReadable (Node, sizeof (ACPI_NAMESPACE_NODE)))
        {
            AcpiOsPrintf ("Address %p is invalid in this address space\n",
                Node);
            return (NULL);
        }

        /* Make sure pointer is valid NS node */

        if (ACPI_GET_DESCRIPTOR_TYPE (Node) != ACPI_DESC_TYPE_NAMED)
        {
            AcpiOsPrintf ("Address %p is not a valid NS node [%s]\n",
                    Node, AcpiUtGetDescriptorName (Node));
            return (NULL);
        }
    }
    else
    {
        /*
         * Alpha argument: The parameter is a name string that must be
         * resolved to a Namespace object.
         */
        Node = AcpiDbLocalNsLookup (InString);
        if (!Node)
        {
            Node = AcpiGbl_RootNode;
        }
    }

    return (Node);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSleep
 *
 * PARAMETERS:  ObjectArg           - Desired sleep state (0-5). NULL means
 *                                    invoke all possible sleep states.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Simulate sleep/wake sequences
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbSleep (
    char                    *ObjectArg)
{
    UINT8                   SleepState;
    UINT32                  i;


    ACPI_FUNCTION_TRACE (AcpiDbSleep);


    /* Null input (no arguments) means to invoke all sleep states */

    if (!ObjectArg)
    {
        AcpiOsPrintf ("Invoking all possible sleep states, 0-%d\n",
            ACPI_S_STATES_MAX);

        for (i = 0; i <= ACPI_S_STATES_MAX; i++)
        {
            AcpiDbDoOneSleepState ((UINT8) i);
        }

        return_ACPI_STATUS (AE_OK);
    }

    /* Convert argument to binary and invoke the sleep state */

    SleepState = (UINT8) ACPI_STRTOUL (ObjectArg, NULL, 0);
    AcpiDbDoOneSleepState (SleepState);
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDoOneSleepState
 *
 * PARAMETERS:  SleepState          - Desired sleep state (0-5)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Simulate a sleep/wake sequence
 *
 ******************************************************************************/

static void
AcpiDbDoOneSleepState (
    UINT8                   SleepState)
{
    ACPI_STATUS             Status;
    UINT8                   SleepTypeA;
    UINT8                   SleepTypeB;


    /* Validate parameter */

    if (SleepState > ACPI_S_STATES_MAX)
    {
        AcpiOsPrintf ("Sleep state %d out of range (%d max)\n",
            SleepState, ACPI_S_STATES_MAX);
        return;
    }

    AcpiOsPrintf ("\n---- Invoking sleep state S%d (%s):\n",
        SleepState, AcpiGbl_SleepStateNames[SleepState]);

    /* Get the values for the sleep type registers (for display only) */

    Status = AcpiGetSleepTypeData (SleepState, &SleepTypeA, &SleepTypeB);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not evaluate [%s] method, %s\n",
            AcpiGbl_SleepStateNames[SleepState],
            AcpiFormatException (Status));
        return;
    }

    AcpiOsPrintf (
        "Register values for sleep state S%d: Sleep-A: %.2X, Sleep-B: %.2X\n",
        SleepState, SleepTypeA, SleepTypeB);

    /* Invoke the various sleep/wake interfaces */

    AcpiOsPrintf ("**** Sleep: Prepare to sleep (S%d) ****\n",
        SleepState);
    Status = AcpiEnterSleepStatePrep (SleepState);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    AcpiOsPrintf ("**** Sleep: Going to sleep (S%d) ****\n",
        SleepState);
    Status = AcpiEnterSleepState (SleepState);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    AcpiOsPrintf ("**** Wake: Prepare to return from sleep (S%d) ****\n",
        SleepState);
    Status = AcpiLeaveSleepStatePrep (SleepState);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    AcpiOsPrintf ("**** Wake: Return from sleep (S%d) ****\n",
        SleepState);
    Status = AcpiLeaveSleepState (SleepState);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    return;


ErrorExit:
    ACPI_EXCEPTION ((AE_INFO, Status, "During invocation of sleep state S%d",
        SleepState));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayLocks
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display information about internal mutexes.
 *
 ******************************************************************************/

void
AcpiDbDisplayLocks (
    void)
{
    UINT32                  i;


    for (i = 0; i < ACPI_MAX_MUTEX; i++)
    {
        AcpiOsPrintf ("%26s : %s\n", AcpiUtGetMutexName (i),
            AcpiGbl_MutexInfo[i].ThreadId == ACPI_MUTEX_NOT_ACQUIRED
                ? "Locked" : "Unlocked");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayTableInfo
 *
 * PARAMETERS:  TableArg            - Name of table to be displayed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display information about loaded tables. Current
 *              implementation displays all loaded tables.
 *
 ******************************************************************************/

void
AcpiDbDisplayTableInfo (
    char                    *TableArg)
{
    UINT32                  i;
    ACPI_TABLE_DESC         *TableDesc;
    ACPI_STATUS             Status;


    /* Header */

    AcpiOsPrintf ("Idx ID Status    Type            Sig  Address  Len   Header\n");

    /* Walk the entire root table list */

    for (i = 0; i < AcpiGbl_RootTableList.CurrentTableCount; i++)
    {
        TableDesc = &AcpiGbl_RootTableList.Tables[i];

        /* Index and Table ID */

        AcpiOsPrintf ("%3u %.2u ", i, TableDesc->OwnerId);

        /* Decode the table flags */

        if (!(TableDesc->Flags & ACPI_TABLE_IS_LOADED))
        {
            AcpiOsPrintf ("NotLoaded ");
        }
        else
        {
            AcpiOsPrintf ("   Loaded ");
        }

        switch (TableDesc->Flags & ACPI_TABLE_ORIGIN_MASK)
        {
        case ACPI_TABLE_ORIGIN_UNKNOWN:

            AcpiOsPrintf ("Unknown   ");
            break;

        case ACPI_TABLE_ORIGIN_MAPPED:

            AcpiOsPrintf ("Mapped    ");
            break;

        case ACPI_TABLE_ORIGIN_ALLOCATED:

            AcpiOsPrintf ("Allocated ");
            break;

        case ACPI_TABLE_ORIGIN_OVERRIDE:

            AcpiOsPrintf ("Override  ");
            break;

        default:

            AcpiOsPrintf ("INVALID   ");
            break;
        }

        /* Make sure that the table is mapped */

        Status = AcpiTbVerifyTable (TableDesc);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump the table header */

        if (TableDesc->Pointer)
        {
            AcpiTbPrintTableHeader (TableDesc->Address, TableDesc->Pointer);
        }
        else
        {
            /* If the pointer is null, the table has been unloaded */

            ACPI_INFO ((AE_INFO, "%4.4s - Table has been unloaded",
                TableDesc->Signature.Ascii));
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbUnloadAcpiTable
 *
 * PARAMETERS:  ObjectName          - Namespace pathname for an object that
 *                                    is owned by the table to be unloaded
 *
 * RETURN:      None
 *
 * DESCRIPTION: Unload an ACPI table, via any namespace node that is owned
 *              by the table.
 *
 ******************************************************************************/

void
AcpiDbUnloadAcpiTable (
    char                    *ObjectName)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Translate name to an Named object */

    Node = AcpiDbConvertToNode (ObjectName);
    if (!Node)
    {
        AcpiOsPrintf ("Could not find [%s] in namespace\n",
            ObjectName);
        return;
    }

    Status = AcpiUnloadParentTable (ACPI_CAST_PTR (ACPI_HANDLE, Node));
    if (ACPI_SUCCESS (Status))
    {
        AcpiOsPrintf ("Parent of [%s] (%p) unloaded and uninstalled\n",
            ObjectName, Node);
    }
    else
    {
        AcpiOsPrintf ("%s, while unloading parent table of [%s]\n",
            AcpiFormatException (Status), ObjectName);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSendNotify
 *
 * PARAMETERS:  Name                - Name of ACPI object where to send notify
 *              Value               - Value of the notify to send.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Send an ACPI notification. The value specified is sent to the
 *              named object as an ACPI notify.
 *
 ******************************************************************************/

void
AcpiDbSendNotify (
    char                    *Name,
    UINT32                  Value)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Translate name to an Named object */

    Node = AcpiDbConvertToNode (Name);
    if (!Node)
    {
        return;
    }

    /* Dispatch the notify if legal */

    if (AcpiEvIsNotifyObject (Node))
    {
        Status = AcpiEvQueueNotifyRequest (Node, Value);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not queue notify\n");
        }
    }
    else
    {
        AcpiOsPrintf (
            "Named object [%4.4s] Type %s, must be Device/Thermal/Processor type\n",
            AcpiUtGetNodeName (Node), AcpiUtGetTypeName (Node->Type));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayInterfaces
 *
 * PARAMETERS:  ActionArg           - Null, "install", or "remove"
 *              InterfaceNameArg    - Name for install/remove options
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display or modify the global _OSI interface list
 *
 ******************************************************************************/

void
AcpiDbDisplayInterfaces (
    char                    *ActionArg,
    char                    *InterfaceNameArg)
{
    ACPI_INTERFACE_INFO     *NextInterface;
    char                    *SubString;
    ACPI_STATUS             Status;


    /* If no arguments, just display current interface list */

    if (!ActionArg)
    {
        (void) AcpiOsAcquireMutex (AcpiGbl_OsiMutex,
                    ACPI_WAIT_FOREVER);

        NextInterface = AcpiGbl_SupportedInterfaces;
        while (NextInterface)
        {
            if (!(NextInterface->Flags & ACPI_OSI_INVALID))
            {
                AcpiOsPrintf ("%s\n", NextInterface->Name);
            }
            NextInterface = NextInterface->Next;
        }

        AcpiOsReleaseMutex (AcpiGbl_OsiMutex);
        return;
    }

    /* If ActionArg exists, so must InterfaceNameArg */

    if (!InterfaceNameArg)
    {
        AcpiOsPrintf ("Missing Interface Name argument\n");
        return;
    }

    /* Uppercase the action for match below */

    AcpiUtStrupr (ActionArg);

    /* Install - install an interface */

    SubString = ACPI_STRSTR ("INSTALL", ActionArg);
    if (SubString)
    {
        Status = AcpiInstallInterface (InterfaceNameArg);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s, while installing \"%s\"\n",
                AcpiFormatException (Status), InterfaceNameArg);
        }
        return;
    }

    /* Remove - remove an interface */

    SubString = ACPI_STRSTR ("REMOVE", ActionArg);
    if (SubString)
    {
        Status = AcpiRemoveInterface (InterfaceNameArg);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s, while removing \"%s\"\n",
                AcpiFormatException (Status), InterfaceNameArg);
        }
        return;
    }

    /* Invalid ActionArg */

    AcpiOsPrintf ("Invalid action argument: %s\n", ActionArg);
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayTemplate
 *
 * PARAMETERS:  BufferArg           - Buffer name or address
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump a buffer that contains a resource template
 *
 ******************************************************************************/

void
AcpiDbDisplayTemplate (
    char                    *BufferArg)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    ACPI_BUFFER             ReturnBuffer;


    /* Translate BufferArg to an Named object */

    Node = AcpiDbConvertToNode (BufferArg);
    if (!Node || (Node == AcpiGbl_RootNode))
    {
        AcpiOsPrintf ("Invalid argument: %s\n", BufferArg);
        return;
    }

    /* We must have a buffer object */

    if (Node->Type != ACPI_TYPE_BUFFER)
    {
        AcpiOsPrintf ("Not a Buffer object, cannot be a template: %s\n",
            BufferArg);
        return;
    }

    ReturnBuffer.Length = ACPI_DEBUG_BUFFER_SIZE;
    ReturnBuffer.Pointer = AcpiGbl_DbBuffer;

    /* Attempt to convert the raw buffer to a resource list */

    Status = AcpiRsCreateResourceList (Node->Object, &ReturnBuffer);

    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
    AcpiDbgLevel |= ACPI_LV_RESOURCES;

    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not convert Buffer to a resource list: %s, %s\n",
            BufferArg, AcpiFormatException (Status));
        goto DumpBuffer;
    }

    /* Now we can dump the resource list */

    AcpiRsDumpResourceList (ACPI_CAST_PTR (ACPI_RESOURCE,
        ReturnBuffer.Pointer));

DumpBuffer:
    AcpiOsPrintf ("\nRaw data buffer:\n");
    AcpiUtDebugDumpBuffer ((UINT8 *) Node->Object->Buffer.Pointer,
        Node->Object->Buffer.Length,
        DB_BYTE_DISPLAY, ACPI_UINT32_MAX);

    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCompareAmlResources
 *
 * PARAMETERS:  Aml1Buffer          - Contains first resource list
 *              Aml1BufferLength    - Length of first resource list
 *              Aml2Buffer          - Contains second resource list
 *              Aml2BufferLength    - Length of second resource list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Compare two AML resource lists, descriptor by descriptor (in
 *              order to isolate a miscompare to an individual resource)
 *
 ******************************************************************************/

static void
AcpiDmCompareAmlResources (
    UINT8                   *Aml1Buffer,
    ACPI_RSDESC_SIZE        Aml1BufferLength,
    UINT8                   *Aml2Buffer,
    ACPI_RSDESC_SIZE        Aml2BufferLength)
{
    UINT8                   *Aml1;
    UINT8                   *Aml2;
    UINT8                   *Aml1End;
    UINT8                   *Aml2End;
    ACPI_RSDESC_SIZE        Aml1Length;
    ACPI_RSDESC_SIZE        Aml2Length;
    ACPI_RSDESC_SIZE        Offset = 0;
    UINT8                   ResourceType;
    UINT32                  Count = 0;
    UINT32                  i;


    /* Compare overall buffer sizes (may be different due to size rounding) */

    if (Aml1BufferLength != Aml2BufferLength)
    {
        AcpiOsPrintf (
            "**** Buffer length mismatch in converted AML: Original %X, New %X ****\n",
            Aml1BufferLength, Aml2BufferLength);
    }

    Aml1 = Aml1Buffer;
    Aml2 = Aml2Buffer;
    Aml1End = Aml1Buffer + Aml1BufferLength;
    Aml2End = Aml2Buffer + Aml2BufferLength;

    /* Walk the descriptor lists, comparing each descriptor */

    while ((Aml1 < Aml1End) && (Aml2 < Aml2End))
    {
        /* Get the lengths of each descriptor */

        Aml1Length = AcpiUtGetDescriptorLength (Aml1);
        Aml2Length = AcpiUtGetDescriptorLength (Aml2);
        ResourceType = AcpiUtGetResourceType (Aml1);

        /* Check for descriptor length match */

        if (Aml1Length != Aml2Length)
        {
            AcpiOsPrintf (
                "**** Length mismatch in descriptor [%.2X] type %2.2X, Offset %8.8X Len1 %X, Len2 %X ****\n",
                Count, ResourceType, Offset, Aml1Length, Aml2Length);
        }

        /* Check for descriptor byte match */

        else if (ACPI_MEMCMP (Aml1, Aml2, Aml1Length))
        {
            AcpiOsPrintf (
                "**** Data mismatch in descriptor [%.2X] type %2.2X, Offset %8.8X ****\n",
                Count, ResourceType, Offset);

            for (i = 0; i < Aml1Length; i++)
            {
                if (Aml1[i] != Aml2[i])
                {
                    AcpiOsPrintf (
                        "Mismatch at byte offset %.2X: is %2.2X, should be %2.2X\n",
                        i, Aml2[i], Aml1[i]);
                }
            }
        }

        /* Exit on EndTag descriptor */

        if (ResourceType == ACPI_RESOURCE_NAME_END_TAG)
        {
            return;
        }

        /* Point to next descriptor in each buffer */

        Count++;
        Offset += Aml1Length;
        Aml1 += Aml1Length;
        Aml2 += Aml2Length;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmTestResourceConversion
 *
 * PARAMETERS:  Node                - Parent device node
 *              Name                - resource method name (_CRS)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compare the original AML with a conversion of the AML to
 *              internal resource list, then back to AML.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmTestResourceConversion (
    ACPI_NAMESPACE_NODE     *Node,
    char                    *Name)
{
    ACPI_STATUS             Status;
    ACPI_BUFFER             ReturnBuffer;
    ACPI_BUFFER             ResourceBuffer;
    ACPI_BUFFER             NewAml;
    ACPI_OBJECT             *OriginalAml;


    AcpiOsPrintf ("Resource Conversion Comparison:\n");

    NewAml.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    ReturnBuffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    ResourceBuffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

    /* Get the original _CRS AML resource template */

    Status = AcpiEvaluateObject (Node, Name, NULL, &ReturnBuffer);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not obtain %s: %s\n",
            Name, AcpiFormatException (Status));
        return (Status);
    }

    /* Get the AML resource template, converted to internal resource structs */

    Status = AcpiGetCurrentResources (Node, &ResourceBuffer);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiGetCurrentResources failed: %s\n",
            AcpiFormatException (Status));
        goto Exit1;
    }

    /* Convert internal resource list to external AML resource template */

    Status = AcpiRsCreateAmlResources (ResourceBuffer.Pointer, &NewAml);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiRsCreateAmlResources failed: %s\n",
            AcpiFormatException (Status));
        goto Exit2;
    }

    /* Compare original AML to the newly created AML resource list */

    OriginalAml = ReturnBuffer.Pointer;

    AcpiDmCompareAmlResources (
        OriginalAml->Buffer.Pointer, (ACPI_RSDESC_SIZE) OriginalAml->Buffer.Length,
        NewAml.Pointer, (ACPI_RSDESC_SIZE) NewAml.Length);

    /* Cleanup and exit */

    ACPI_FREE (NewAml.Pointer);
Exit2:
    ACPI_FREE (ResourceBuffer.Pointer);
Exit1:
    ACPI_FREE (ReturnBuffer.Pointer);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbResourceCallback
 *
 * PARAMETERS:  ACPI_WALK_RESOURCE_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Simple callback to exercise AcpiWalkResources and
 *              AcpiWalkResourceBuffer.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbResourceCallback (
    ACPI_RESOURCE           *Resource,
    void                    *Context)
{

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDeviceResources
 *
 * PARAMETERS:  ACPI_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Display the _PRT/_CRS/_PRS resources for a device object.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbDeviceResources (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_NAMESPACE_NODE     *PrtNode = NULL;
    ACPI_NAMESPACE_NODE     *CrsNode = NULL;
    ACPI_NAMESPACE_NODE     *PrsNode = NULL;
    ACPI_NAMESPACE_NODE     *AeiNode = NULL;
    char                    *ParentPath;
    ACPI_BUFFER             ReturnBuffer;
    ACPI_STATUS             Status;


    Node = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, ObjHandle);
    ParentPath = AcpiNsGetExternalPathname (Node);
    if (!ParentPath)
    {
        return (AE_NO_MEMORY);
    }

    /* Get handles to the resource methods for this device */

    (void) AcpiGetHandle (Node, METHOD_NAME__PRT, ACPI_CAST_PTR (ACPI_HANDLE, &PrtNode));
    (void) AcpiGetHandle (Node, METHOD_NAME__CRS, ACPI_CAST_PTR (ACPI_HANDLE, &CrsNode));
    (void) AcpiGetHandle (Node, METHOD_NAME__PRS, ACPI_CAST_PTR (ACPI_HANDLE, &PrsNode));
    (void) AcpiGetHandle (Node, METHOD_NAME__AEI, ACPI_CAST_PTR (ACPI_HANDLE, &AeiNode));
    if (!PrtNode && !CrsNode && !PrsNode && !AeiNode)
    {
        goto Cleanup;   /* Nothing to do */
    }

    AcpiOsPrintf ("\nDevice: %s\n", ParentPath);

    /* Prepare for a return object of arbitrary size */

    ReturnBuffer.Pointer = AcpiGbl_DbBuffer;
    ReturnBuffer.Length  = ACPI_DEBUG_BUFFER_SIZE;


    /* _PRT */

    if (PrtNode)
    {
        AcpiOsPrintf ("Evaluating _PRT\n");

        Status = AcpiEvaluateObject (PrtNode, NULL, NULL, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not evaluate _PRT: %s\n",
                AcpiFormatException (Status));
            goto GetCrs;
        }

        ReturnBuffer.Pointer = AcpiGbl_DbBuffer;
        ReturnBuffer.Length  = ACPI_DEBUG_BUFFER_SIZE;

        Status = AcpiGetIrqRoutingTable (Node, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("GetIrqRoutingTable failed: %s\n",
                AcpiFormatException (Status));
            goto GetCrs;
        }

        AcpiRsDumpIrqList (ACPI_CAST_PTR (UINT8, AcpiGbl_DbBuffer));
    }


    /* _CRS */

GetCrs:
    if (CrsNode)
    {
        AcpiOsPrintf ("Evaluating _CRS\n");

        ReturnBuffer.Pointer = AcpiGbl_DbBuffer;
        ReturnBuffer.Length  = ACPI_DEBUG_BUFFER_SIZE;

        Status = AcpiEvaluateObject (CrsNode, NULL, NULL, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not evaluate _CRS: %s\n",
                AcpiFormatException (Status));
            goto GetPrs;
        }

        /* This code exercises the AcpiWalkResources interface */

        Status = AcpiWalkResources (Node, METHOD_NAME__CRS,
            AcpiDbResourceCallback, NULL);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("AcpiWalkResources failed: %s\n",
                AcpiFormatException (Status));
            goto GetPrs;
        }

        /* Get the _CRS resource list (test ALLOCATE buffer) */

        ReturnBuffer.Pointer = NULL;
        ReturnBuffer.Length  = ACPI_ALLOCATE_LOCAL_BUFFER;

        Status = AcpiGetCurrentResources (Node, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("AcpiGetCurrentResources failed: %s\n",
                AcpiFormatException (Status));
            goto GetPrs;
        }

        /* This code exercises the AcpiWalkResourceBuffer interface */

        Status = AcpiWalkResourceBuffer (&ReturnBuffer,
            AcpiDbResourceCallback, NULL);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("AcpiWalkResourceBuffer failed: %s\n",
                AcpiFormatException (Status));
            goto EndCrs;
        }

        /* Dump the _CRS resource list */

        AcpiRsDumpResourceList (ACPI_CAST_PTR (ACPI_RESOURCE,
            ReturnBuffer.Pointer));

        /*
         * Perform comparison of original AML to newly created AML. This
         * tests both the AML->Resource conversion and the Resource->AML
         * conversion.
         */
        (void) AcpiDmTestResourceConversion (Node, METHOD_NAME__CRS);

        /* Execute _SRS with the resource list */

        AcpiOsPrintf ("Evaluating _SRS\n");

        Status = AcpiSetCurrentResources (Node, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("AcpiSetCurrentResources failed: %s\n",
                AcpiFormatException (Status));
            goto EndCrs;
        }

EndCrs:
        ACPI_FREE_BUFFER (ReturnBuffer);
    }


    /* _PRS */

GetPrs:
    if (PrsNode)
    {
        AcpiOsPrintf ("Evaluating _PRS\n");

        ReturnBuffer.Pointer = AcpiGbl_DbBuffer;
        ReturnBuffer.Length  = ACPI_DEBUG_BUFFER_SIZE;

        Status = AcpiEvaluateObject (PrsNode, NULL, NULL, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not evaluate _PRS: %s\n",
                AcpiFormatException (Status));
            goto GetAei;
        }

        ReturnBuffer.Pointer = AcpiGbl_DbBuffer;
        ReturnBuffer.Length  = ACPI_DEBUG_BUFFER_SIZE;

        Status = AcpiGetPossibleResources (Node, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("AcpiGetPossibleResources failed: %s\n",
                AcpiFormatException (Status));
            goto GetAei;
        }

        AcpiRsDumpResourceList (ACPI_CAST_PTR (ACPI_RESOURCE, AcpiGbl_DbBuffer));
    }


    /* _AEI */

GetAei:
    if (AeiNode)
    {
        AcpiOsPrintf ("Evaluating _AEI\n");

        ReturnBuffer.Pointer = AcpiGbl_DbBuffer;
        ReturnBuffer.Length  = ACPI_DEBUG_BUFFER_SIZE;

        Status = AcpiEvaluateObject (AeiNode, NULL, NULL, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not evaluate _AEI: %s\n",
                AcpiFormatException (Status));
            goto Cleanup;
        }

        ReturnBuffer.Pointer = AcpiGbl_DbBuffer;
        ReturnBuffer.Length  = ACPI_DEBUG_BUFFER_SIZE;

        Status = AcpiGetEventResources (Node, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("AcpiGetEventResources failed: %s\n",
                AcpiFormatException (Status));
            goto Cleanup;
        }

        AcpiRsDumpResourceList (ACPI_CAST_PTR (ACPI_RESOURCE, AcpiGbl_DbBuffer));
    }


Cleanup:
    ACPI_FREE (ParentPath);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayResources
 *
 * PARAMETERS:  ObjectArg           - String object name or object pointer.
 *                                    NULL or "*" means "display resources for
 *                                    all devices"
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display the resource objects associated with a device.
 *
 ******************************************************************************/

void
AcpiDbDisplayResources (
    char                    *ObjectArg)
{
    ACPI_NAMESPACE_NODE     *Node;


    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
    AcpiDbgLevel |= ACPI_LV_RESOURCES;

    /* Asterisk means "display resources for all devices" */

    if (!ObjectArg || (!ACPI_STRCMP (ObjectArg, "*")))
    {
        (void) AcpiWalkNamespace (ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
                    ACPI_UINT32_MAX, AcpiDbDeviceResources, NULL, NULL, NULL);
    }
    else
    {
        /* Convert string to object pointer */

        Node = AcpiDbConvertToNode (ObjectArg);
        if (Node)
        {
            if (Node->Type != ACPI_TYPE_DEVICE)
            {
                AcpiOsPrintf ("%4.4s: Name is not a device object (%s)\n",
                    Node->Name.Ascii, AcpiUtGetTypeName (Node->Type));
            }
            else
            {
                (void) AcpiDbDeviceResources (Node, 0, NULL, NULL);
            }
        }
    }

    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
}


#if (!ACPI_REDUCED_HARDWARE)
/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGenerateGpe
 *
 * PARAMETERS:  GpeArg              - Raw GPE number, ascii string
 *              BlockArg            - GPE block number, ascii string
 *                                    0 or 1 for FADT GPE blocks
 *
 * RETURN:      None
 *
 * DESCRIPTION: Simulate firing of a GPE
 *
 ******************************************************************************/

void
AcpiDbGenerateGpe (
    char                    *GpeArg,
    char                    *BlockArg)
{
    UINT32                  BlockNumber;
    UINT32                  GpeNumber;
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;


    GpeNumber   = ACPI_STRTOUL (GpeArg, NULL, 0);
    BlockNumber = ACPI_STRTOUL (BlockArg, NULL, 0);


    GpeEventInfo = AcpiEvGetGpeEventInfo (ACPI_TO_POINTER (BlockNumber),
        GpeNumber);
    if (!GpeEventInfo)
    {
        AcpiOsPrintf ("Invalid GPE\n");
        return;
    }

    (void) AcpiEvGpeDispatch (NULL, GpeEventInfo, GpeNumber);
}
#endif /* !ACPI_REDUCED_HARDWARE */

#endif /* ACPI_DEBUGGER */
