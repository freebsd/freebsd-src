/*******************************************************************************
 *
 * Module Name: dbcmds - debug commands and output routines
 *              $Revision: 88 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
 * All rights reserved.
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


#include "acpi.h"
#include "acdispat.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acevents.h"
#include "acdebug.h"
#include "acresrc.h"
#include "acdisasm.h"

#ifdef ACPI_DEBUGGER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbcmds")


/*
 * Arguments for the Objects command
 * These object types map directly to the ACPI_TYPES
 */

static ARGUMENT_INFO        AcpiDbObjectTypes [] =
{
    {"ANY"},
    {"NUMBERS"},
    {"STRINGS"},
    {"BUFFERS"},
    {"PACKAGES"},
    {"FIELDS"},
    {"DEVICES"},
    {"EVENTS"},
    {"METHODS"},
    {"MUTEXES"},
    {"REGIONS"},
    {"POWERRESOURCES"},
    {"PROCESSORS"},
    {"THERMALZONES"},
    {"BUFFERFIELDS"},
    {"DDBHANDLES"},
    {NULL}           /* Must be null terminated */
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbWalkForReferences
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check if this namespace object refers to the target object
 *              that is passed in as the context value.
 *
 * Note: Currently doesn't check subobjects within the Node's object
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbWalkForReferences (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_OPERAND_OBJECT     *ObjDesc = (ACPI_OPERAND_OBJECT  *) Context;
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;


    /* Check for match against the namespace node itself */

    if (Node == (void *) ObjDesc)
    {
        AcpiOsPrintf ("Object is a Node [%4.4s]\n", Node->Name.Ascii);
    }

    /* Check for match against the object attached to the node */

    if (AcpiNsGetAttachedObject (Node) == ObjDesc)
    {
        AcpiOsPrintf ("Reference at Node->Object %p [%4.4s]\n", Node, Node->Name.Ascii);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbFindReferences
 *
 * PARAMETERS:  ObjectArg       - String with hex value of the object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Search namespace for all references to the input object
 *
 ******************************************************************************/

void
AcpiDbFindReferences (
    NATIVE_CHAR             *ObjectArg)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;


    /* Convert string to object pointer */

    ObjDesc = ACPI_TO_POINTER (ACPI_STRTOUL (ObjectArg, NULL, 16));

    /* Search all nodes in namespace */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                    AcpiDbWalkForReferences, (void *) ObjDesc, NULL);
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
AcpiDbDisplayLocks (void)
{
    UINT32                  i;


    for (i = 0; i < MAX_MTX; i++)
    {
        AcpiOsPrintf ("%26s : %s\n", AcpiUtGetMutexName (i),
                    AcpiGbl_AcpiMutexInfo[i].OwnerId == ACPI_MUTEX_NOT_ACQUIRED
                        ? "Locked" : "Unlocked");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayTableInfo
 *
 * PARAMETERS:  TableArg        - String with name of table to be displayed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display information about loaded tables.  Current
 *              implementation displays all loaded tables.
 *
 ******************************************************************************/

void
AcpiDbDisplayTableInfo (
    NATIVE_CHAR             *TableArg)
{
    UINT32                  i;


    for (i = 0; i < NUM_ACPI_TABLES; i++)
    {
        if (AcpiGbl_AcpiTables[i].Pointer)
        {
            AcpiOsPrintf ("%s at %p length %X\n", AcpiGbl_AcpiTableData[i].Name,
                        AcpiGbl_AcpiTables[i].Pointer, 
                        (UINT32) AcpiGbl_AcpiTables[i].Length);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbUnloadAcpiTable
 *
 * PARAMETERS:  TableArg        - Name of the table to be unloaded
 *              InstanceArg     - Which instance of the table to unload (if
 *                                there are multiple tables of the same type)
 *
 * RETURN:      Nonde
 *
 * DESCRIPTION: Unload an ACPI table.
 *              Instance is not implemented
 *
 ******************************************************************************/

void
AcpiDbUnloadAcpiTable (
    NATIVE_CHAR             *TableArg,
    NATIVE_CHAR             *InstanceArg)
{
    UINT32                  i;
    ACPI_STATUS             Status;


    /* Search all tables for the target type */

    for (i = 0; i < NUM_ACPI_TABLES; i++)
    {
        if (!ACPI_STRNCMP (TableArg, AcpiGbl_AcpiTableData[i].Signature,
                AcpiGbl_AcpiTableData[i].SigLength))
        {
            /* Found the table, unload it */

            Status = AcpiUnloadTable (i);
            if (ACPI_SUCCESS (Status))
            {
                AcpiOsPrintf ("[%s] unloaded and uninstalled\n", TableArg);
            }
            else
            {
                AcpiOsPrintf ("%s, while unloading [%s]\n",
                    AcpiFormatException (Status), TableArg);
            }

            return;
        }
    }

    AcpiOsPrintf ("Unknown table type [%s]\n", TableArg);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSetMethodBreakpoint
 *
 * PARAMETERS:  Location            - AML offset of breakpoint
 *              WalkState           - Current walk info
 *              Op                  - Current Op (from parse walk)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set a breakpoint in a control method at the specified
 *              AML offset
 *
 ******************************************************************************/

void
AcpiDbSetMethodBreakpoint (
    NATIVE_CHAR             *Location,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  Address;


    if (!Op)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }

    /* Get and verify the breakpoint address */

    Address = ACPI_STRTOUL (Location, NULL, 16);
    if (Address <= Op->Common.AmlOffset)
    {
        AcpiOsPrintf ("Breakpoint %X is beyond current address %X\n", Address, Op->Common.AmlOffset);
    }

    /* Save breakpoint in current walk */

    WalkState->UserBreakpoint = Address;
    AcpiOsPrintf ("Breakpoint set at AML offset %X\n", Address);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSetMethodCallBreakpoint
 *
 * PARAMETERS:  Op                  - Current Op (from parse walk)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set a breakpoint in a control method at the specified
 *              AML offset
 *
 ******************************************************************************/

void
AcpiDbSetMethodCallBreakpoint (
    ACPI_PARSE_OBJECT       *Op)
{


    if (!Op)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }


    AcpiGbl_StepToNextCall = TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisassembleAml
 *
 * PARAMETERS:  Statements          - Number of statements to disassemble
 *              Op                  - Current Op (from parse walk)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display disassembled AML (ASL) starting from Op for the number
 *              of statements specified.
 *
 ******************************************************************************/

void
AcpiDbDisassembleAml (
    NATIVE_CHAR             *Statements,
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  NumStatements = 8;


    if (!Op)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }

    if (Statements)
    {
        NumStatements = ACPI_STRTOUL (Statements, NULL, 0);
    }

#ifdef ACPI_DISASSEMBLER
    AcpiDmDisassemble (NULL, Op, NumStatements);
#endif
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDumpNamespace
 *
 * PARAMETERS:  StartArg        - Node to begin namespace dump
 *              DepthArg        - Maximum tree depth to be dumped
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump entire namespace or a subtree.  Each node is displayed
 *              with type and other information.
 *
 ******************************************************************************/

void
AcpiDbDumpNamespace (
    NATIVE_CHAR             *StartArg,
    NATIVE_CHAR             *DepthArg)
{
    ACPI_HANDLE             SubtreeEntry = AcpiGbl_RootNode;
    UINT32                  MaxDepth = ACPI_UINT32_MAX;


    /* No argument given, just start at the root and dump entire namespace */

    if (StartArg)
    {
        /* Check if numeric argument, must be a Node */

        if ((StartArg[0] >= 0x30) && (StartArg[0] <= 0x39))
        {
            SubtreeEntry = ACPI_TO_POINTER (ACPI_STRTOUL (StartArg, NULL, 16));
            if (!AcpiOsReadable (SubtreeEntry, sizeof (ACPI_NAMESPACE_NODE)))
            {
                AcpiOsPrintf ("Address %p is invalid in this address space\n", SubtreeEntry);
                return;
            }

            if (ACPI_GET_DESCRIPTOR_TYPE (SubtreeEntry) != ACPI_DESC_TYPE_NAMED)
            {
                AcpiOsPrintf ("Address %p is not a valid Named object\n", SubtreeEntry);
                return;
            }
        }

        /* Alpha argument */

        else
        {
            /* The parameter is a name string that must be resolved to a Named obj*/

            SubtreeEntry = AcpiDbLocalNsLookup (StartArg);
            if (!SubtreeEntry)
            {
                SubtreeEntry = AcpiGbl_RootNode;
            }
        }

        /* Now we can check for the depth argument */

        if (DepthArg)
        {
            MaxDepth = ACPI_STRTOUL (DepthArg, NULL, 0);
        }
    }

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("ACPI Namespace (from %p subtree):\n", SubtreeEntry);

    /* Display the subtree */

    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
    AcpiNsDumpObjects (ACPI_TYPE_ANY, ACPI_DISPLAY_SUMMARY, MaxDepth, ACPI_UINT32_MAX, SubtreeEntry);
    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDumpNamespaceByOwner
 *
 * PARAMETERS:  OwnerArg        - Owner ID whose nodes will be displayed
 *              DepthArg        - Maximum tree depth to be dumped
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump elements of the namespace that are owned by the OwnerId.
 *
 ******************************************************************************/

void
AcpiDbDumpNamespaceByOwner (
    NATIVE_CHAR             *OwnerArg,
    NATIVE_CHAR             *DepthArg)
{
    ACPI_HANDLE             SubtreeEntry = AcpiGbl_RootNode;
    UINT32                  MaxDepth = ACPI_UINT32_MAX;
    UINT16                  OwnerId;


    OwnerId = (UINT16) ACPI_STRTOUL (OwnerArg, NULL, 0);

    /* Now we can check for the depth argument */

    if (DepthArg)
    {
        MaxDepth = ACPI_STRTOUL (DepthArg, NULL, 0);
    }

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("ACPI Namespace by owner %X:\n", OwnerId);

    /* Display the subtree */

    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
    AcpiNsDumpObjects (ACPI_TYPE_ANY, ACPI_DISPLAY_SUMMARY, MaxDepth, OwnerId, SubtreeEntry);
    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSendNotify
 *
 * PARAMETERS:  Name            - Name of ACPI object to send the notify to
 *              Value           - Value of the notify to send.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Send an ACPI notification.  The value specified is sent to the
 *              named object as an ACPI notify.
 *
 ******************************************************************************/

void
AcpiDbSendNotify (
    NATIVE_CHAR             *Name,
    UINT32                  Value)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Translate name to an Named object */

    Node = AcpiDbLocalNsLookup (Name);
    if (!Node)
    {
        return;
    }

    /* Decode Named object type */

    switch (Node->Type)
    {
    case ACPI_TYPE_DEVICE:
    case ACPI_TYPE_THERMAL:

         /* Send the notify */

        Status = AcpiEvQueueNotifyRequest (Node, Value);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not queue notify\n");
        }
        break;

    default:
        AcpiOsPrintf ("Named object is not a device or a thermal object\n");
        break;
    }

}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSetMethodData
 *
 * PARAMETERS:  TypeArg         - L for local, A for argument
 *              IndexArg        - which one
 *              ValueArg        - Value to set.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set a local or argument for the running control method.
 *              NOTE: only object supported is Number.
 *
 ******************************************************************************/

void
AcpiDbSetMethodData (
    NATIVE_CHAR             *TypeArg,
    NATIVE_CHAR             *IndexArg,
    NATIVE_CHAR             *ValueArg)
{
    NATIVE_CHAR             Type;
    UINT32                  Index;
    UINT32                  Value;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    /* Validate TypeArg */

    ACPI_STRUPR (TypeArg);
    Type = TypeArg[0];
    if ((Type != 'L') &&
        (Type != 'A'))
    {
        AcpiOsPrintf ("Invalid SET operand: %s\n", TypeArg);
        return;
    }

    /* Get the index and value */

    Index = ACPI_STRTOUL (IndexArg, NULL, 16);
    Value = ACPI_STRTOUL (ValueArg, NULL, 16);

    WalkState = AcpiDsGetCurrentWalkState (AcpiGbl_CurrentWalkList);
    if (!WalkState)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }


    /* Create and initialize the new object */

    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
    if (!ObjDesc)
    {
        AcpiOsPrintf ("Could not create an internal object\n");
        return;
    }

    ObjDesc->Integer.Value = Value;


    /* Store the new object into the target */

    switch (Type)
    {
    case 'A':

        /* Set a method argument */

        if (Index > MTH_MAX_ARG)
        {
            AcpiOsPrintf ("Arg%d - Invalid argument name\n", Index);
            return;
        }

        Status = AcpiDsStoreObjectToLocal (AML_ARG_OP, Index, ObjDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        ObjDesc = WalkState->Arguments[Index].Object;

        AcpiOsPrintf ("Arg%d: ", Index);
        AcpiDbDisplayInternalObject (ObjDesc, WalkState);
        break;

    case 'L':

        /* Set a method local */

        if (Index > MTH_MAX_LOCAL)
        {
            AcpiOsPrintf ("Local%d - Invalid local variable name\n", Index);
            return;
        }

        Status = AcpiDsStoreObjectToLocal (AML_LOCAL_OP, Index, ObjDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        ObjDesc = WalkState->LocalVariables[Index].Object;

        AcpiOsPrintf ("Local%d: ", Index);
        AcpiDbDisplayInternalObject (ObjDesc, WalkState);
        break;

    default:
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbWalkForSpecificObjects
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Display short info about objects in the namespace
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbWalkForSpecificObjects (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;
    ACPI_BUFFER             Buffer;


    ObjDesc = AcpiNsGetAttachedObject ((ACPI_NAMESPACE_NODE *) ObjHandle);

    /* Get and display the full pathname to this object */

    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiNsHandleToPathname (ObjHandle, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could Not get pathname for object %p\n", ObjHandle);
        return (AE_OK);
    }

    AcpiOsPrintf ("%32s", (char *) Buffer.Pointer);
    ACPI_MEM_FREE (Buffer.Pointer);


    /* Display short information about the object */

    if (ObjDesc)
    {
        switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
        {
        case ACPI_TYPE_METHOD:
            AcpiOsPrintf ("  #Args %d  Concurrency %X", 
                    ObjDesc->Method.ParamCount, ObjDesc->Method.Concurrency);
            break;

        case ACPI_TYPE_INTEGER:
            AcpiOsPrintf ("  Value %8.8X%8.8X", 
                    ACPI_HIDWORD (ObjDesc->Integer.Value),
                    ACPI_LODWORD (ObjDesc->Integer.Value));
            break;

        case ACPI_TYPE_STRING:
            AcpiOsPrintf ("  \"%s\"", ObjDesc->String.Pointer);
            break;

        case ACPI_TYPE_REGION:
            AcpiOsPrintf ("  SpaceId %X Length %X Address %8.8X%8.8X", 
                    ObjDesc->Region.SpaceId,
                    ObjDesc->Region.Length,
                    ACPI_HIDWORD (ObjDesc->Region.Address),
                    ACPI_LODWORD (ObjDesc->Region.Address));
            break;

        case ACPI_TYPE_PACKAGE:
            AcpiOsPrintf ("  #Elements %X", ObjDesc->Package.Count);
            break;

        case ACPI_TYPE_BUFFER:
            AcpiOsPrintf ("  Length %X", ObjDesc->Buffer.Length);
            break;

        default:
            /* Ignore other object types */
            break;
        }
    }

    AcpiOsPrintf ("\n");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayObjects
 *
 * PARAMETERS:  ObjTypeArg          - Type of object to display
 *              DisplayCountArg     - Max depth to display
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display objects in the namespace of the requested type
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbDisplayObjects (
    NATIVE_CHAR             *ObjTypeArg,
    NATIVE_CHAR             *DisplayCountArg)
{
    ACPI_OBJECT_TYPE        Type;


    /* Get the object type */

    Type = AcpiDbMatchArgument (ObjTypeArg, AcpiDbObjectTypes);
    if (Type == ACPI_TYPE_NOT_FOUND)
    {
        AcpiOsPrintf ("Invalid or unsupported argument\n");
        return (AE_OK);
    }

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("Objects of type [%s] defined in the current ACPI Namespace: \n",
        AcpiUtGetTypeName (Type));

    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);

    /* Walk the namespace from the root */

    (void) AcpiWalkNamespace (Type, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                        AcpiDbWalkForSpecificObjects, (void *) &Type, NULL);

    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbWalkAndMatchName
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find a particular name/names within the namespace.  Wildcards
 *              are supported -- '?' matches any character.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbWalkAndMatchName (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;
    NATIVE_CHAR             *RequestedName = (NATIVE_CHAR *) Context;
    UINT32                  i;
    ACPI_BUFFER             Buffer;


    /* Check for a name match */

    for (i = 0; i < 4; i++)
    {
        /* Wildcard support */

        if ((RequestedName[i] != '?') &&
            (RequestedName[i] != ((ACPI_NAMESPACE_NODE *) ObjHandle)->Name.Ascii[i]))
        {
            /* No match, just exit */

            return (AE_OK);
        }
    }


    /* Get the full pathname to this object */

    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiNsHandleToPathname (ObjHandle, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could Not get pathname for object %p\n", ObjHandle);
    }
    else
    {
        AcpiOsPrintf ("%32s (%p) - %s\n", (char *) Buffer.Pointer, ObjHandle,
            AcpiUtGetTypeName (((ACPI_NAMESPACE_NODE *) ObjHandle)->Type));
        ACPI_MEM_FREE (Buffer.Pointer);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbFindNameInNamespace
 *
 * PARAMETERS:  NameArg         - The 4-character ACPI name to find.
 *                                wildcards are supported.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Search the namespace for a given name (with wildcards)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbFindNameInNamespace (
    NATIVE_CHAR             *NameArg)
{

    if (ACPI_STRLEN (NameArg) > 4)
    {
        AcpiOsPrintf ("Name must be no longer than 4 characters\n");
        return (AE_OK);
    }

    /* Walk the namespace from the root */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                        AcpiDbWalkAndMatchName, NameArg, NULL);

    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSetScope
 *
 * PARAMETERS:  Name                - New scope path
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Set the "current scope" as maintained by this utility.
 *              The scope is used as a prefix to ACPI paths.
 *
 ******************************************************************************/

void
AcpiDbSetScope (
    NATIVE_CHAR             *Name)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;


    if (!Name || Name[0] == 0)
    {
        AcpiOsPrintf ("Current scope: %s\n", AcpiGbl_DbScopeBuf);
        return;
    }

    AcpiDbPrepNamestring (Name);


    if (Name[0] == '\\')
    {
        /* Validate new scope from the root */

        Status = AcpiNsGetNodeByPath (Name, AcpiGbl_RootNode, ACPI_NS_NO_UPSEARCH, &Node);
        if (ACPI_FAILURE (Status))
        {
            goto ErrorExit;
        }

        ACPI_STRCPY (AcpiGbl_DbScopeBuf, Name);
        ACPI_STRCAT (AcpiGbl_DbScopeBuf, "\\");
    }
    else
    {
        /* Validate new scope relative to old scope */

        Status = AcpiNsGetNodeByPath (Name, AcpiGbl_DbScopeNode, ACPI_NS_NO_UPSEARCH, &Node);
        if (ACPI_FAILURE (Status))
        {
            goto ErrorExit;
        }

        ACPI_STRCAT (AcpiGbl_DbScopeBuf, Name);
        ACPI_STRCAT (AcpiGbl_DbScopeBuf, "\\");
    }

    AcpiGbl_DbScopeNode = Node;
    AcpiOsPrintf ("New scope: %s\n", AcpiGbl_DbScopeBuf);
    return;


ErrorExit:

    AcpiOsPrintf ("Could not attach scope: %s, %s\n", Name, AcpiFormatException (Status));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayResources
 *
 * PARAMETERS:  ObjectArg       - String with hex value of the object
 *
 * RETURN:      None
 *
 * DESCRIPTION:
 *
 ******************************************************************************/

void
AcpiDbDisplayResources (
    NATIVE_CHAR             *ObjectArg)
{
#if ACPI_MACHINE_WIDTH != 16

    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;
    ACPI_BUFFER             ReturnObj;


    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
    AcpiDbgLevel |= ACPI_LV_RESOURCES;

    /* Convert string to object pointer */

    ObjDesc = ACPI_TO_POINTER (ACPI_STRTOUL (ObjectArg, NULL, 16));

    /* Prepare for a return object of arbitrary size */

    ReturnObj.Pointer           = AcpiGbl_DbBuffer;
    ReturnObj.Length            = ACPI_DEBUG_BUFFER_SIZE;

    /* _PRT */

    AcpiOsPrintf ("Evaluating _PRT\n");

    Status = AcpiEvaluateObject (ObjDesc, "_PRT", NULL, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not obtain _PRT: %s\n", AcpiFormatException (Status));
        goto GetCrs;
    }

    ReturnObj.Pointer           = AcpiGbl_DbBuffer;
    ReturnObj.Length            = ACPI_DEBUG_BUFFER_SIZE;

    Status = AcpiGetIrqRoutingTable (ObjDesc, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("GetIrqRoutingTable failed: %s\n", AcpiFormatException (Status));
    }

    else
    {
        AcpiRsDumpIrqList ((UINT8 *) AcpiGbl_DbBuffer);
    }


    /* _CRS */

GetCrs:
    AcpiOsPrintf ("Evaluating _CRS\n");

    ReturnObj.Pointer           = AcpiGbl_DbBuffer;
    ReturnObj.Length            = ACPI_DEBUG_BUFFER_SIZE;

    Status = AcpiEvaluateObject (ObjDesc, "_CRS", NULL, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not obtain _CRS: %s\n", AcpiFormatException (Status));
        goto GetPrs;
    }

    ReturnObj.Pointer           = AcpiGbl_DbBuffer;
    ReturnObj.Length            = ACPI_DEBUG_BUFFER_SIZE;

    Status = AcpiGetCurrentResources (ObjDesc, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiGetCurrentResources failed: %s\n", AcpiFormatException (Status));
        goto GetPrs;
    }

    else
    {
        AcpiRsDumpResourceList (ACPI_CAST_PTR (ACPI_RESOURCE, AcpiGbl_DbBuffer));
    }

    Status = AcpiSetCurrentResources (ObjDesc, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiSetCurrentResources failed: %s\n", AcpiFormatException (Status));
        goto GetPrs;
    }


    /* _PRS */

GetPrs:
    AcpiOsPrintf ("Evaluating _PRS\n");

    ReturnObj.Pointer           = AcpiGbl_DbBuffer;
    ReturnObj.Length            = ACPI_DEBUG_BUFFER_SIZE;

    Status = AcpiEvaluateObject (ObjDesc, "_PRS", NULL, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not obtain _PRS: %s\n", AcpiFormatException (Status));
        goto Cleanup;
    }

    ReturnObj.Pointer           = AcpiGbl_DbBuffer;
    ReturnObj.Length            = ACPI_DEBUG_BUFFER_SIZE;

    Status = AcpiGetPossibleResources (ObjDesc, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiGetPossibleResources failed: %s\n", AcpiFormatException (Status));
    }

    else
    {
        AcpiRsDumpResourceList (ACPI_CAST_PTR (ACPI_RESOURCE, AcpiGbl_DbBuffer));
    }


Cleanup:

    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
    return;
#endif

}


typedef struct
{
    UINT32              Nodes;
    UINT32              Objects;
} ACPI_INTEGRITY_INFO;

/*******************************************************************************
 *
 * FUNCTION:    AcpiDbIntegrityWalk
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Examine one NS node for valid values.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbIntegrityWalk (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_INTEGRITY_INFO     *Info = (ACPI_INTEGRITY_INFO *) Context;
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_OPERAND_OBJECT     *Object;


    Info->Nodes++;
    if (ACPI_GET_DESCRIPTOR_TYPE (Node) != ACPI_DESC_TYPE_NAMED)
    {
        AcpiOsPrintf ("Invalid Descriptor Type for Node %p, Type = %X\n",
            Node, ACPI_GET_DESCRIPTOR_TYPE (Node));
    }

    if (Node->Type > INTERNAL_TYPE_MAX)
    {
        AcpiOsPrintf ("Invalid Object Type for Node %p, Type = %X\n",
            Node, Node->Type);
    }

    if (!AcpiUtValidAcpiName (Node->Name.Integer))
    {
        AcpiOsPrintf ("Invalid AcpiName for Node %p\n", Node);
    }

    Object = AcpiNsGetAttachedObject (Node);
    if (Object)
    {
        Info->Objects++;
        if (ACPI_GET_DESCRIPTOR_TYPE (Object) != ACPI_DESC_TYPE_OPERAND)
        {
            AcpiOsPrintf ("Invalid Descriptor Type for Object %p, Type = %X\n",
                Object, ACPI_GET_DESCRIPTOR_TYPE (Object));
        }
    }


    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCheckIntegrity
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check entire namespace for data structure integrity
 *
 ******************************************************************************/

void
AcpiDbCheckIntegrity (void)
{
    ACPI_INTEGRITY_INFO     Info = {0,0};

    /* Search all nodes in namespace */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                    AcpiDbIntegrityWalk, (void *) &Info, NULL);

    AcpiOsPrintf ("Verified %d namespace nodes with %d Objects\n", Info.Nodes, Info.Objects);

}

#endif /* ACPI_DEBUGGER */
