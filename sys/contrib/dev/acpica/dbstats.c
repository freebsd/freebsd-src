/*******************************************************************************
 *
 * Module Name: dbstats - Generation and display of ACPI table statistics
 *              $Revision: 70 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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


#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acdebug.h>
#include <contrib/dev/acpica/acnamesp.h>

#ifdef ACPI_DEBUGGER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbstats")

/*
 * Statistics subcommands
 */
static ARGUMENT_INFO        AcpiDbStatTypes [] =
{
    {"ALLOCATIONS"},
    {"OBJECTS"},
    {"MEMORY"},
    {"MISC"},
    {"TABLES"},
    {"SIZES"},
    {"STACK"},
    {NULL}           /* Must be null terminated */
};

#define CMD_STAT_ALLOCATIONS     0
#define CMD_STAT_OBJECTS         1
#define CMD_STAT_MEMORY          2
#define CMD_STAT_MISC            3
#define CMD_STAT_TABLES          4
#define CMD_STAT_SIZES           5
#define CMD_STAT_STACK           6


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbEnumerateObject
 *
 * PARAMETERS:  ObjDesc             - Object to be counted
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add this object to the global counts, by object type.
 *              Limited recursion handles subobjects and packages, and this
 *              is probably acceptable within the AML debugger only.
 *
 ******************************************************************************/

void
AcpiDbEnumerateObject (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    UINT32                  i;


    if (!ObjDesc)
    {
        return;
    }


    /* Enumerate this object first */

    AcpiGbl_NumObjects++;

    if (ACPI_GET_OBJECT_TYPE (ObjDesc) > ACPI_TYPE_NS_NODE_MAX)
    {
        AcpiGbl_ObjTypeCountMisc++;
    }
    else
    {
        AcpiGbl_ObjTypeCount [ACPI_GET_OBJECT_TYPE (ObjDesc)]++;
    }

    /* Count the sub-objects */

    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_PACKAGE:

        for (i = 0; i < ObjDesc->Package.Count; i++)
        {
            AcpiDbEnumerateObject (ObjDesc->Package.Elements[i]);
        }
        break;

    case ACPI_TYPE_DEVICE:

        AcpiDbEnumerateObject (ObjDesc->Device.SystemNotify);
        AcpiDbEnumerateObject (ObjDesc->Device.DeviceNotify);
        AcpiDbEnumerateObject (ObjDesc->Device.Handler);
        break;

    case ACPI_TYPE_BUFFER_FIELD:

        if (AcpiNsGetSecondaryObject (ObjDesc))
        {
            AcpiGbl_ObjTypeCount [ACPI_TYPE_BUFFER_FIELD]++;
        }
        break;

    case ACPI_TYPE_REGION:

        AcpiGbl_ObjTypeCount [ACPI_TYPE_LOCAL_REGION_FIELD ]++;
        AcpiDbEnumerateObject (ObjDesc->Region.Handler);
        break;

    case ACPI_TYPE_POWER:

        AcpiDbEnumerateObject (ObjDesc->PowerResource.SystemNotify);
        AcpiDbEnumerateObject (ObjDesc->PowerResource.DeviceNotify);
        break;

    case ACPI_TYPE_PROCESSOR:

        AcpiDbEnumerateObject (ObjDesc->Processor.SystemNotify);
        AcpiDbEnumerateObject (ObjDesc->Processor.DeviceNotify);
        AcpiDbEnumerateObject (ObjDesc->Processor.Handler);
        break;

    case ACPI_TYPE_THERMAL:

        AcpiDbEnumerateObject (ObjDesc->ThermalZone.SystemNotify);
        AcpiDbEnumerateObject (ObjDesc->ThermalZone.DeviceNotify);
        AcpiDbEnumerateObject (ObjDesc->ThermalZone.Handler);
        break;

    default:
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbClassifyOneObject
 *
 * PARAMETERS:  Callback for WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enumerate both the object descriptor (including subobjects) and
 *              the parent namespace node.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbClassifyOneObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  Type;


    AcpiGbl_NumNodes++;

    Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ObjDesc = AcpiNsGetAttachedObject (Node);

    AcpiDbEnumerateObject (ObjDesc);

    Type = Node->Type;
    if (Type > ACPI_TYPE_NS_NODE_MAX)
    {
        AcpiGbl_NodeTypeCountMisc++;
    }

    else
    {
        AcpiGbl_NodeTypeCount [Type]++;
    }

    return AE_OK;


    /* TBD: These need to be counted during the initial parsing phase */
    /*
    if (AcpiPsIsNamedOp (Op->Opcode))
    {
        NumNodes++;
    }

    if (IsMethod)
    {
        NumMethodElements++;
    }

    NumGrammarElements++;
    Op = AcpiPsGetDepthNext (Root, Op);

    SizeOfParseTree             = (NumGrammarElements - NumMethodElements) * (UINT32) sizeof (ACPI_PARSE_OBJECT);
    SizeOfMethodTrees           = NumMethodElements * (UINT32) sizeof (ACPI_PARSE_OBJECT);
    SizeOfNodeEntries           = NumNodes * (UINT32) sizeof (ACPI_NAMESPACE_NODE);
    SizeOfAcpiObjects           = NumNodes * (UINT32) sizeof (ACPI_OPERAND_OBJECT);

    */
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCountNamespaceObjects
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Count and classify the entire namespace, including all
 *              namespace nodes and attached objects.
 *
 ******************************************************************************/

void
AcpiDbCountNamespaceObjects (
    void)
{
    UINT32                  i;


    AcpiGbl_NumNodes = 0;
    AcpiGbl_NumObjects = 0;

    AcpiGbl_ObjTypeCountMisc = 0;
    for (i = 0; i < (ACPI_TYPE_NS_NODE_MAX -1); i++)
    {
        AcpiGbl_ObjTypeCount [i] = 0;
        AcpiGbl_NodeTypeCount [i] = 0;
    }

    (void) AcpiNsWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                        FALSE, AcpiDbClassifyOneObject, NULL, NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayStatistics
 *
 * PARAMETERS:  TypeArg         - Subcommand
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Display various statistics
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbDisplayStatistics (
    char                    *TypeArg)
{
    UINT32                  i;
    UINT32                  Type;
    UINT32                  Size;
#ifdef ACPI_DBG_TRACK_ALLOCATIONS
    UINT32                  Outstanding;
#endif


    if (!AcpiGbl_DSDT)
    {
        AcpiOsPrintf ("*** Warning:  There is no DSDT loaded\n");
    }

    if (!TypeArg)
    {
        AcpiOsPrintf ("The following subcommands are available:\n    ALLOCATIONS, OBJECTS, MEMORY, MISC, SIZES, TABLES\n");
        return (AE_OK);
    }

    ACPI_STRUPR (TypeArg);
    Type = AcpiDbMatchArgument (TypeArg, AcpiDbStatTypes);
    if (Type == (UINT32) -1)
    {
        AcpiOsPrintf ("Invalid or unsupported argument\n");
        return (AE_OK);
    }


    switch (Type)
    {
    case CMD_STAT_ALLOCATIONS:
#ifdef ACPI_DBG_TRACK_ALLOCATIONS
        AcpiUtDumpAllocationInfo ();
#endif
        break;

    case CMD_STAT_TABLES:

        AcpiOsPrintf ("ACPI Table Information:\n\n");
        if (AcpiGbl_DSDT)
        {
            AcpiOsPrintf ("DSDT Length:................% 7ld (%X)\n", AcpiGbl_DSDT->Length, AcpiGbl_DSDT->Length);
        }
        break;

    case CMD_STAT_OBJECTS:

        AcpiDbCountNamespaceObjects ();

        AcpiOsPrintf ("\nObjects defined in the current namespace:\n\n");

        AcpiOsPrintf ("%16.16s %10.10s %10.10s\n", "ACPI_TYPE", "NODES", "OBJECTS");

        for (i = 0; i < ACPI_TYPE_NS_NODE_MAX; i++)
        {
            AcpiOsPrintf ("%16.16s % 10ld% 10ld\n", AcpiUtGetTypeName (i),
                AcpiGbl_NodeTypeCount [i], AcpiGbl_ObjTypeCount [i]);
        }
        AcpiOsPrintf ("%16.16s % 10ld% 10ld\n", "Misc/Unknown",
            AcpiGbl_NodeTypeCountMisc, AcpiGbl_ObjTypeCountMisc);

        AcpiOsPrintf ("%16.16s % 10ld% 10ld\n", "TOTALS:",
            AcpiGbl_NumNodes, AcpiGbl_NumObjects);
        break;

    case CMD_STAT_MEMORY:

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
        AcpiOsPrintf ("\n----Object and Cache Statistics---------------------------------------------\n");

        for (i = 0; i < ACPI_NUM_MEM_LISTS; i++)
        {
            AcpiOsPrintf ("\n%s\n", AcpiGbl_MemoryLists[i].ListName);

            if (AcpiGbl_MemoryLists[i].MaxCacheDepth > 0)
            {
                AcpiOsPrintf ("    Cache: [Depth Max Avail Size]         % 7d % 7d % 7d % 7d B\n",
                        AcpiGbl_MemoryLists[i].CacheDepth,
                        AcpiGbl_MemoryLists[i].MaxCacheDepth,
                        AcpiGbl_MemoryLists[i].MaxCacheDepth - AcpiGbl_MemoryLists[i].CacheDepth,
                        (AcpiGbl_MemoryLists[i].CacheDepth * AcpiGbl_MemoryLists[i].ObjectSize));

                AcpiOsPrintf ("    Cache: [Requests Hits Misses ObjSize] % 7d % 7d % 7d % 7d B\n",
                        AcpiGbl_MemoryLists[i].CacheRequests,
                        AcpiGbl_MemoryLists[i].CacheHits,
                        AcpiGbl_MemoryLists[i].CacheRequests - AcpiGbl_MemoryLists[i].CacheHits,
                        AcpiGbl_MemoryLists[i].ObjectSize);
            }

            Outstanding = AcpiGbl_MemoryLists[i].TotalAllocated -
                            AcpiGbl_MemoryLists[i].TotalFreed -
                            AcpiGbl_MemoryLists[i].CacheDepth;

            if (AcpiGbl_MemoryLists[i].ObjectSize)
            {
                Size = ACPI_ROUND_UP_TO_1K (Outstanding * AcpiGbl_MemoryLists[i].ObjectSize);
            }
            else
            {
                Size = ACPI_ROUND_UP_TO_1K (AcpiGbl_MemoryLists[i].CurrentTotalSize);
            }

            AcpiOsPrintf ("    Mem:   [Alloc Free Outstanding Size]  % 7d % 7d % 7d % 7d Kb\n",
                    AcpiGbl_MemoryLists[i].TotalAllocated,
                    AcpiGbl_MemoryLists[i].TotalFreed,
                    Outstanding, Size);
        }
#endif

        break;

    case CMD_STAT_MISC:

        AcpiOsPrintf ("\nMiscellaneous Statistics:\n\n");
        AcpiOsPrintf ("Calls to AcpiPsFind:..  ........% 7ld\n", AcpiGbl_PsFindCount);
        AcpiOsPrintf ("Calls to AcpiNsLookup:..........% 7ld\n", AcpiGbl_NsLookupCount);

        AcpiOsPrintf ("\n");

        AcpiOsPrintf ("Mutex usage:\n\n");
        for (i = 0; i < NUM_MUTEX; i++)
        {
            AcpiOsPrintf ("%-28s:       % 7ld\n", AcpiUtGetMutexName (i), AcpiGbl_MutexInfo[i].UseCount);
        }
        break;


    case CMD_STAT_SIZES:

        AcpiOsPrintf ("\nInternal object sizes:\n\n");

        AcpiOsPrintf ("Common           %3d\n", sizeof (ACPI_OBJECT_COMMON));
        AcpiOsPrintf ("Number           %3d\n", sizeof (ACPI_OBJECT_INTEGER));
        AcpiOsPrintf ("String           %3d\n", sizeof (ACPI_OBJECT_STRING));
        AcpiOsPrintf ("Buffer           %3d\n", sizeof (ACPI_OBJECT_BUFFER));
        AcpiOsPrintf ("Package          %3d\n", sizeof (ACPI_OBJECT_PACKAGE));
        AcpiOsPrintf ("BufferField      %3d\n", sizeof (ACPI_OBJECT_BUFFER_FIELD));
        AcpiOsPrintf ("Device           %3d\n", sizeof (ACPI_OBJECT_DEVICE));
        AcpiOsPrintf ("Event            %3d\n", sizeof (ACPI_OBJECT_EVENT));
        AcpiOsPrintf ("Method           %3d\n", sizeof (ACPI_OBJECT_METHOD));
        AcpiOsPrintf ("Mutex            %3d\n", sizeof (ACPI_OBJECT_MUTEX));
        AcpiOsPrintf ("Region           %3d\n", sizeof (ACPI_OBJECT_REGION));
        AcpiOsPrintf ("PowerResource    %3d\n", sizeof (ACPI_OBJECT_POWER_RESOURCE));
        AcpiOsPrintf ("Processor        %3d\n", sizeof (ACPI_OBJECT_PROCESSOR));
        AcpiOsPrintf ("ThermalZone      %3d\n", sizeof (ACPI_OBJECT_THERMAL_ZONE));
        AcpiOsPrintf ("RegionField      %3d\n", sizeof (ACPI_OBJECT_REGION_FIELD));
        AcpiOsPrintf ("BankField        %3d\n", sizeof (ACPI_OBJECT_BANK_FIELD));
        AcpiOsPrintf ("IndexField       %3d\n", sizeof (ACPI_OBJECT_INDEX_FIELD));
        AcpiOsPrintf ("Reference        %3d\n", sizeof (ACPI_OBJECT_REFERENCE));
        AcpiOsPrintf ("Notify           %3d\n", sizeof (ACPI_OBJECT_NOTIFY_HANDLER));
        AcpiOsPrintf ("AddressSpace     %3d\n", sizeof (ACPI_OBJECT_ADDR_HANDLER));
        AcpiOsPrintf ("Extra            %3d\n", sizeof (ACPI_OBJECT_EXTRA));
        AcpiOsPrintf ("Data             %3d\n", sizeof (ACPI_OBJECT_DATA));

        AcpiOsPrintf ("\n");

        AcpiOsPrintf ("ParseObject      %3d\n", sizeof (ACPI_PARSE_OBJ_COMMON));
        AcpiOsPrintf ("ParseObjectNamed %3d\n", sizeof (ACPI_PARSE_OBJ_NAMED));
        AcpiOsPrintf ("ParseObjectAsl   %3d\n", sizeof (ACPI_PARSE_OBJ_ASL));
        AcpiOsPrintf ("OperandObject    %3d\n", sizeof (ACPI_OPERAND_OBJECT));
        AcpiOsPrintf ("NamespaceNode    %3d\n", sizeof (ACPI_NAMESPACE_NODE));

        break;


    case CMD_STAT_STACK:
#if defined(ACPI_DEBUG_OUTPUT)

        Size = (UINT32) (AcpiGbl_EntryStackPointer - AcpiGbl_LowestStackPointer);

        AcpiOsPrintf ("\nSubsystem Stack Usage:\n\n");
        AcpiOsPrintf ("Entry Stack Pointer          %X\n", AcpiGbl_EntryStackPointer);
        AcpiOsPrintf ("Lowest Stack Pointer         %X\n", AcpiGbl_LowestStackPointer);
        AcpiOsPrintf ("Stack Use                    %X (%d)\n", Size, Size);
        AcpiOsPrintf ("Deepest Procedure Nesting    %d\n", AcpiGbl_DeepestNesting);
#endif
        break;

    default:
        break;
    }

    AcpiOsPrintf ("\n");
    return (AE_OK);
}


#endif /* ACPI_DEBUGGER  */
