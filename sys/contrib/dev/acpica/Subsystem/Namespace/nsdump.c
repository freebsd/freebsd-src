/******************************************************************************
 *
 * Module Name: nsdump - table dumping routines for debug
 *              $Revision: 80 $
 *
 *****************************************************************************/

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

#define __NSDUMP_C__

#include "acpi.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "actables.h"


#define _COMPONENT          NAMESPACE
        MODULE_NAME         ("nsdump")


#ifdef ACPI_DEBUG

/****************************************************************************
 *
 * FUNCTION:    AcpiNsDumpPathname
 *
 * PARAMETERS:  Handle              - Object
 *              Msg                 - Prefix message
 *              Level               - Desired debug level
 *              Component           - Caller's component ID
 *
 * DESCRIPTION: Print an object's full namespace pathname
 *              Manages allocation/freeing of a pathname buffer
 *
 ***************************************************************************/

ACPI_STATUS
AcpiNsDumpPathname (
    ACPI_HANDLE             Handle,
    NATIVE_CHAR             *Msg,
    UINT32                  Level,
    UINT32                  Component)
{
    NATIVE_CHAR             *Buffer;
    UINT32                  Length;


    FUNCTION_TRACE ("NsDumpPathname");

    /* Do this only if the requested debug level and component are enabled */

    if (!(AcpiDbgLevel & Level) || !(AcpiDbgLayer & Component))
    {
        return_ACPI_STATUS (AE_OK);
    }

    Buffer = AcpiCmAllocate (PATHNAME_MAX);
    if (!Buffer)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Convert handle to a full pathname and print it (with supplied message) */

    Length = PATHNAME_MAX;
    if (ACPI_SUCCESS (AcpiNsHandleToPathname (Handle, &Length, Buffer)))
    {
        AcpiOsPrintf ("%s %s (%p)\n", Msg, Buffer, Handle);
    }

    AcpiCmFree (Buffer);

    return_ACPI_STATUS (AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiNsDumpOneObject
 *
 * PARAMETERS:  Handle              - Node to be dumped
 *              Level               - Nesting level of the handle
 *              Context             - Passed into WalkNamespace
 *
 * DESCRIPTION: Dump a single Node
 *              This procedure is a UserFunction called by AcpiNsWalkNamespace.
 *
 ***************************************************************************/

ACPI_STATUS
AcpiNsDumpOneObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_WALK_INFO          *Info = (ACPI_WALK_INFO *) Context;
    ACPI_NAMESPACE_NODE     *ThisNode;
    UINT8                   *Value;
    ACPI_OPERAND_OBJECT     *ObjDesc = NULL;
    OBJECT_TYPE_INTERNAL    ObjType;
    OBJECT_TYPE_INTERNAL    Type;
    UINT32                  BytesToDump;
    UINT32                  DownstreamSiblingMask = 0;
    UINT32                  LevelTmp;
    UINT32                  WhichBit;


    ThisNode = AcpiNsConvertHandleToEntry (ObjHandle);

    LevelTmp    = Level;
    Type        = ThisNode->Type;
    WhichBit    = 1;


    if (!(AcpiDbgLevel & Info->DebugLevel))
    {
        return (AE_OK);
    }

    if (!ObjHandle)
    {
        DEBUG_PRINT (ACPI_INFO, ("Null object handle\n"));
        return (AE_OK);
    }

    /* Check if the owner matches */

    if ((Info->OwnerId != ACPI_UINT32_MAX) &&
        (Info->OwnerId != ThisNode->OwnerId))
    {
        return (AE_OK);
    }


    /* Indent the object according to the level */

    while (LevelTmp--)
    {

        /* Print appropriate characters to form tree structure */

        if (LevelTmp)
        {
            if (DownstreamSiblingMask & WhichBit)
            {
                DEBUG_PRINT_RAW (TRACE_TABLES, ("|"));
            }

            else
            {
                DEBUG_PRINT_RAW (TRACE_TABLES, (" "));
            }

            WhichBit <<= 1;
        }

        else
        {
            if (AcpiNsExistDownstreamSibling (ThisNode + 1))
            {
                DownstreamSiblingMask |= (1 << (Level - 1));
                DEBUG_PRINT_RAW (TRACE_TABLES, ("+"));
            }

            else
            {
                DownstreamSiblingMask &= ACPI_UINT32_MAX ^ (1 << (Level - 1));
                DEBUG_PRINT_RAW (TRACE_TABLES, ("+"));
            }

            if (ThisNode->Child == NULL)
            {
                DEBUG_PRINT_RAW (TRACE_TABLES, ("-"));
            }

            else if (AcpiNsExistDownstreamSibling (ThisNode->Child))
            {
                DEBUG_PRINT_RAW (TRACE_TABLES, ("+"));
            }

            else
            {
                DEBUG_PRINT_RAW (TRACE_TABLES, ("-"));
            }
        }
    }


    /* Check the integrity of our data */

    if (Type > INTERNAL_TYPE_MAX)
    {
        Type = INTERNAL_TYPE_DEF_ANY;                                /* prints as *ERROR* */
    }

    if (!AcpiCmValidAcpiName (ThisNode->Name))
    {
        REPORT_WARNING (("Invalid ACPI Name 0x%X\n", ThisNode->Name));
    }

    /*
     * Now we can print out the pertinent information
     */

    DEBUG_PRINT_RAW (TRACE_TABLES, (" %4.4s %-9s ", &ThisNode->Name, AcpiCmGetTypeName (Type)));
    DEBUG_PRINT_RAW (TRACE_TABLES, ("%p S:%p O:%p",  ThisNode, ThisNode->Child, ThisNode->Object));


    if (!ThisNode->Object)
    {
        /* No attached object, we are done */

        DEBUG_PRINT_RAW (TRACE_TABLES, ("\n"));
        return (AE_OK);
    }

    switch (Type)
    {

    case ACPI_TYPE_METHOD:

        /* Name is a Method and its AML offset/length are set */

        DEBUG_PRINT_RAW (TRACE_TABLES, (" M:%p-%X\n",
                    ((ACPI_OPERAND_OBJECT  *) ThisNode->Object)->Method.Pcode,
                    ((ACPI_OPERAND_OBJECT  *) ThisNode->Object)->Method.PcodeLength));

        break;


    case ACPI_TYPE_NUMBER:

        DEBUG_PRINT_RAW (TRACE_TABLES, (" N:%X\n",
                    ((ACPI_OPERAND_OBJECT  *) ThisNode->Object)->Number.Value));
        break;


    case ACPI_TYPE_STRING:

        DEBUG_PRINT_RAW (TRACE_TABLES, (" S:%p-%X\n",
                    ((ACPI_OPERAND_OBJECT  *) ThisNode->Object)->String.Pointer,
                    ((ACPI_OPERAND_OBJECT  *) ThisNode->Object)->String.Length));
        break;


    case ACPI_TYPE_BUFFER:

        DEBUG_PRINT_RAW (TRACE_TABLES, (" B:%p-%X\n",
                    ((ACPI_OPERAND_OBJECT  *) ThisNode->Object)->Buffer.Pointer,
                    ((ACPI_OPERAND_OBJECT  *) ThisNode->Object)->Buffer.Length));
        break;


    default:

        DEBUG_PRINT_RAW (TRACE_TABLES, ("\n"));
        break;
    }

    /* If debug turned off, done */

    if (!(AcpiDbgLevel & TRACE_VALUES))
    {
        return (AE_OK);
    }


    /* If there is an attached object, display it */

    Value = ThisNode->Object;

    /* Dump attached objects */

    while (Value)
    {
        ObjType = INTERNAL_TYPE_INVALID;

        /* Decode the type of attached object and dump the contents */

        DEBUG_PRINT_RAW (TRACE_TABLES, ("        Attached Object %p: ", Value));

        if (AcpiTbSystemTablePointer (Value))
        {
            DEBUG_PRINT_RAW (TRACE_TABLES, ("(Ptr to AML Code)\n"));
            BytesToDump = 16;
        }

        else if (VALID_DESCRIPTOR_TYPE (Value, ACPI_DESC_TYPE_NAMED))
        {
            DEBUG_PRINT_RAW (TRACE_TABLES, ("(Ptr to Node)\n"));
            BytesToDump = sizeof (ACPI_NAMESPACE_NODE);
        }


        else if (VALID_DESCRIPTOR_TYPE (Value, ACPI_DESC_TYPE_INTERNAL))
        {
            ObjDesc = (ACPI_OPERAND_OBJECT  *) Value;
            ObjType = ObjDesc->Common.Type;

            if (ObjType > INTERNAL_TYPE_MAX)
            {
                DEBUG_PRINT_RAW (TRACE_TABLES, ("(Ptr to ACPI Object type 0x%X [UNKNOWN])\n", ObjType));
                BytesToDump = 32;
            }

            else
            {
                DEBUG_PRINT_RAW (TRACE_TABLES, ("(Ptr to ACPI Object type 0x%X [%s])\n",
                                    ObjType, AcpiCmGetTypeName (ObjType)));
                BytesToDump = sizeof (ACPI_OPERAND_OBJECT);
            }
        }

        else
        {
            DEBUG_PRINT_RAW (TRACE_TABLES, ("(String or Buffer - not descriptor)\n", Value));
            BytesToDump = 16;
        }

        DUMP_BUFFER (Value, BytesToDump);

        /* If value is NOT an internal object, we are done */

        if ((AcpiTbSystemTablePointer (Value)) ||
            (VALID_DESCRIPTOR_TYPE (Value, ACPI_DESC_TYPE_NAMED)))
        {
            goto Cleanup;
        }

        /*
         * Valid object, get the pointer to next level, if any
         */
        switch (ObjType)
        {
        case ACPI_TYPE_STRING:
            Value = (UINT8 *) ObjDesc->String.Pointer;
            break;

        case ACPI_TYPE_BUFFER:
            Value = (UINT8 *) ObjDesc->Buffer.Pointer;
            break;

        case ACPI_TYPE_PACKAGE:
            Value = (UINT8 *) ObjDesc->Package.Elements;
            break;

        case ACPI_TYPE_METHOD:
            Value = (UINT8 *) ObjDesc->Method.Pcode;
            break;

        case ACPI_TYPE_FIELD_UNIT:
            Value = (UINT8 *) ObjDesc->FieldUnit.Container;
            break;

        case INTERNAL_TYPE_DEF_FIELD:
            Value = (UINT8 *) ObjDesc->Field.Container;
            break;

        case INTERNAL_TYPE_BANK_FIELD:
            Value = (UINT8 *) ObjDesc->BankField.Container;
            break;

        case INTERNAL_TYPE_INDEX_FIELD:
            Value = (UINT8 *) ObjDesc->IndexField.Index;
            break;

       default:
            goto Cleanup;
        }

        ObjType = INTERNAL_TYPE_INVALID;     /* Terminate loop after next pass */
    }

Cleanup:
    DEBUG_PRINT_RAW (TRACE_TABLES, ("\n"));
    return (AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiNsDumpObjects
 *
 * PARAMETERS:  Type                - Object type to be dumped
 *              MaxDepth            - Maximum depth of dump.  Use ACPI_UINT32_MAX
 *                                    for an effectively unlimited depth.
 *              OwnerId             - Dump only objects owned by this ID.  Use
 *                                    ACPI_UINT32_MAX to match all owners.
 *              StartHandle         - Where in namespace to start/end search
 *
 * DESCRIPTION: Dump typed objects within the loaded namespace.
 *              Uses AcpiNsWalkNamespace in conjunction with AcpiNsDumpOneObject.
 *
 ***************************************************************************/

void
AcpiNsDumpObjects (
    OBJECT_TYPE_INTERNAL    Type,
    UINT32                  MaxDepth,
    UINT32                  OwnerId,
    ACPI_HANDLE             StartHandle)
{
    ACPI_WALK_INFO          Info;


    Info.DebugLevel = TRACE_TABLES;
    Info.OwnerId = OwnerId;

    AcpiNsWalkNamespace (Type, StartHandle, MaxDepth, NS_WALK_NO_UNLOCK, AcpiNsDumpOneObject,
                        (void *) &Info, NULL);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiNsDumpOneDevice
 *
 * PARAMETERS:  Handle              - Node to be dumped
 *              Level               - Nesting level of the handle
 *              Context             - Passed into WalkNamespace
 *
 * DESCRIPTION: Dump a single Node that represents a device
 *              This procedure is a UserFunction called by AcpiNsWalkNamespace.
 *
 ***************************************************************************/

ACPI_STATUS
AcpiNsDumpOneDevice (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_DEVICE_INFO        Info;
    ACPI_STATUS             Status;
    UINT32                  i;


    Status = AcpiNsDumpOneObject (ObjHandle, Level, Context, ReturnValue);

    Status = AcpiGetObjectInfo (ObjHandle, &Info);
    if (ACPI_SUCCESS (Status))
    {
        for (i = 0; i < Level; i++)
        {
            DEBUG_PRINT_RAW (TRACE_TABLES, (" "));
        }

        DEBUG_PRINT_RAW (TRACE_TABLES, ("    HID: %.8X, ADR: %.8X, Status: %x\n",
                        Info.HardwareId, Info.Address, Info.CurrentStatus));
    }

    return (Status);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiNsDumpRootDevices
 *
 * PARAMETERS:  None
 *
 * DESCRIPTION: Dump all objects of type "device"
 *
 ***************************************************************************/

void
AcpiNsDumpRootDevices (void)
{
    ACPI_HANDLE             SysBusHandle;


    /* Only dump the table if tracing is enabled */

    if (!(TRACE_TABLES & AcpiDbgLevel))
    {
        return;
    }

    AcpiGetHandle (0, NS_SYSTEM_BUS, &SysBusHandle);

    DEBUG_PRINT (TRACE_TABLES, ("Display of all devices in the namespace:\n"));
    AcpiNsWalkNamespace (ACPI_TYPE_DEVICE, SysBusHandle, ACPI_UINT32_MAX, NS_WALK_NO_UNLOCK,
                        AcpiNsDumpOneDevice, NULL, NULL);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiNsDumpTables
 *
 * PARAMETERS:  SearchBase          - Root of subtree to be dumped, or
 *                                    NS_ALL to dump the entire namespace
 *              MaxDepth            - Maximum depth of dump.  Use INT_MAX
 *                                    for an effectively unlimited depth.
 *
 * DESCRIPTION: Dump the name space, or a portion of it.
 *
 ***************************************************************************/

void
AcpiNsDumpTables (
    ACPI_HANDLE             SearchBase,
    UINT32                  MaxDepth)
{
    ACPI_HANDLE             SearchHandle = SearchBase;


    FUNCTION_TRACE ("NsDumpTables");


    if (!AcpiGbl_RootNode)
    {
        /*
         * If the name space has not been initialized,
         * there is nothing to dump.
         */
        DEBUG_PRINT (TRACE_TABLES, ("NsDumpTables: name space not initialized!\n"));
        return_VOID;
    }

    if (NS_ALL == SearchBase)
    {
        /*  entire namespace    */

        SearchHandle = AcpiGbl_RootNode;
        DEBUG_PRINT (TRACE_TABLES, ("\\\n"));
    }


    AcpiNsDumpObjects (ACPI_TYPE_ANY, MaxDepth, ACPI_UINT32_MAX, SearchHandle);
    return_VOID;
}


/****************************************************************************
 *
 * FUNCTION:    AcpiNsDumpEntry
 *
 * PARAMETERS:  Handle              - Node to be dumped
 *              DebugLevel          - Output level
 *
 * DESCRIPTION: Dump a single Node
 *
 ***************************************************************************/

void
AcpiNsDumpEntry (
    ACPI_HANDLE             Handle,
    UINT32                  DebugLevel)
{
    ACPI_WALK_INFO          Info;


    FUNCTION_TRACE_PTR ("NsDumpEntry", Handle);

    Info.DebugLevel = DebugLevel;
    Info.OwnerId = ACPI_UINT32_MAX;

    AcpiNsDumpOneObject (Handle, 1, &Info, NULL);

    DEBUG_PRINT (TRACE_EXEC, ("leave AcpiNsDumpEntry %p\n", Handle));
    return_VOID;
}

#endif

