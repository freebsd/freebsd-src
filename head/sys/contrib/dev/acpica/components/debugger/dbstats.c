/*******************************************************************************
 *
 * Module Name: dbstats - Generation and display of ACPI table statistics
 *
 ******************************************************************************/

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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/acnamesp.h>


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbstats")


/* Local prototypes */

static void
AcpiDbCountNamespaceObjects (
    void);

static void
AcpiDbEnumerateObject (
    ACPI_OPERAND_OBJECT     *ObjDesc);

static ACPI_STATUS
AcpiDbClassifyOneObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

#if defined ACPI_DBG_TRACK_ALLOCATIONS || defined ACPI_USE_LOCAL_CACHE
static void
AcpiDbListInfo (
    ACPI_MEMORY_LIST        *List);
#endif


/*
 * Statistics subcommands
 */
static ACPI_DB_ARGUMENT_INFO    AcpiDbStatTypes [] =
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


#if defined ACPI_DBG_TRACK_ALLOCATIONS || defined ACPI_USE_LOCAL_CACHE
/*******************************************************************************
 *
 * FUNCTION:    AcpiDbListInfo
 *
 * PARAMETERS:  List            - Memory list/cache to be displayed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display information about the input memory list or cache.
 *
 ******************************************************************************/

static void
AcpiDbListInfo (
    ACPI_MEMORY_LIST        *List)
{
#ifdef ACPI_DBG_TRACK_ALLOCATIONS
    UINT32                  Outstanding;
#endif

    AcpiOsPrintf ("\n%s\n", List->ListName);

    /* MaxDepth > 0 indicates a cache object */

    if (List->MaxDepth > 0)
    {
        AcpiOsPrintf (
            "    Cache: [Depth    MaxD Avail  Size]                "
            "%8.2X %8.2X %8.2X %8.2X\n",
            List->CurrentDepth,
            List->MaxDepth,
            List->MaxDepth - List->CurrentDepth,
            (List->CurrentDepth * List->ObjectSize));
    }

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
    if (List->MaxDepth > 0)
    {
        AcpiOsPrintf (
            "    Cache: [Requests Hits Misses ObjSize]             "
            "%8.2X %8.2X %8.2X %8.2X\n",
            List->Requests,
            List->Hits,
            List->Requests - List->Hits,
            List->ObjectSize);
    }

    Outstanding = AcpiDbGetCacheInfo (List);

    if (List->ObjectSize)
    {
        AcpiOsPrintf (
            "    Mem:   [Alloc    Free Max    CurSize Outstanding] "
            "%8.2X %8.2X %8.2X %8.2X %8.2X\n",
            List->TotalAllocated,
            List->TotalFreed,
            List->MaxOccupied,
            Outstanding * List->ObjectSize,
            Outstanding);
    }
    else
    {
        AcpiOsPrintf (
            "    Mem:   [Alloc Free Max CurSize Outstanding Total] "
            "%8.2X %8.2X %8.2X %8.2X %8.2X %8.2X\n",
            List->TotalAllocated,
            List->TotalFreed,
            List->MaxOccupied,
            List->CurrentTotalSize,
            Outstanding,
            List->TotalSize);
    }
#endif
}
#endif


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

static void
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

    if (ObjDesc->Common.Type > ACPI_TYPE_NS_NODE_MAX)
    {
        AcpiGbl_ObjTypeCountMisc++;
    }
    else
    {
        AcpiGbl_ObjTypeCount [ObjDesc->Common.Type]++;
    }

    /* Count the sub-objects */

    switch (ObjDesc->Common.Type)
    {
    case ACPI_TYPE_PACKAGE:

        for (i = 0; i < ObjDesc->Package.Count; i++)
        {
            AcpiDbEnumerateObject (ObjDesc->Package.Elements[i]);
        }
        break;

    case ACPI_TYPE_DEVICE:

        AcpiDbEnumerateObject (ObjDesc->Device.NotifyList[0]);
        AcpiDbEnumerateObject (ObjDesc->Device.NotifyList[1]);
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

        AcpiDbEnumerateObject (ObjDesc->PowerResource.NotifyList[0]);
        AcpiDbEnumerateObject (ObjDesc->PowerResource.NotifyList[1]);
        break;

    case ACPI_TYPE_PROCESSOR:

        AcpiDbEnumerateObject (ObjDesc->Processor.NotifyList[0]);
        AcpiDbEnumerateObject (ObjDesc->Processor.NotifyList[1]);
        AcpiDbEnumerateObject (ObjDesc->Processor.Handler);
        break;

    case ACPI_TYPE_THERMAL:

        AcpiDbEnumerateObject (ObjDesc->ThermalZone.NotifyList[0]);
        AcpiDbEnumerateObject (ObjDesc->ThermalZone.NotifyList[1]);
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

static ACPI_STATUS
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

    return (AE_OK);


#ifdef ACPI_FUTURE_IMPLEMENTATION

    /* TBD: These need to be counted during the initial parsing phase */

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

    SizeOfParseTree   = (NumGrammarElements - NumMethodElements) *
                            (UINT32) sizeof (ACPI_PARSE_OBJECT);
    SizeOfMethodTrees = NumMethodElements * (UINT32) sizeof (ACPI_PARSE_OBJECT);
    SizeOfNodeEntries = NumNodes * (UINT32) sizeof (ACPI_NAMESPACE_NODE);
    SizeOfAcpiObjects = NumNodes * (UINT32) sizeof (ACPI_OPERAND_OBJECT);
#endif
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCountNamespaceObjects
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Count and classify the entire namespace, including all
 *              namespace nodes and attached objects.
 *
 ******************************************************************************/

static void
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

    (void) AcpiNsWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
        ACPI_UINT32_MAX, FALSE, AcpiDbClassifyOneObject, NULL, NULL, NULL);
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
    UINT32                  Temp;


    AcpiUtStrupr (TypeArg);
    Temp = AcpiDbMatchArgument (TypeArg, AcpiDbStatTypes);
    if (Temp == ACPI_TYPE_NOT_FOUND)
    {
        AcpiOsPrintf ("Invalid or unsupported argument\n");
        return (AE_OK);
    }


    switch (Temp)
    {
    case CMD_STAT_ALLOCATIONS:

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
        AcpiUtDumpAllocationInfo ();
#endif
        break;

    case CMD_STAT_TABLES:

        AcpiOsPrintf ("ACPI Table Information (not implemented):\n\n");
        break;

    case CMD_STAT_OBJECTS:

        AcpiDbCountNamespaceObjects ();

        AcpiOsPrintf ("\nObjects defined in the current namespace:\n\n");

        AcpiOsPrintf ("%16.16s %10.10s %10.10s\n",
            "ACPI_TYPE", "NODES", "OBJECTS");

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
        AcpiOsPrintf ("\n----Object Statistics (all in hex)---------\n");

        AcpiDbListInfo (AcpiGbl_GlobalList);
        AcpiDbListInfo (AcpiGbl_NsNodeList);
#endif

#ifdef ACPI_USE_LOCAL_CACHE
        AcpiOsPrintf ("\n----Cache Statistics (all in hex)---------\n");
        AcpiDbListInfo (AcpiGbl_OperandCache);
        AcpiDbListInfo (AcpiGbl_PsNodeCache);
        AcpiDbListInfo (AcpiGbl_PsNodeExtCache);
        AcpiDbListInfo (AcpiGbl_StateCache);
#endif

        break;

    case CMD_STAT_MISC:

        AcpiOsPrintf ("\nMiscellaneous Statistics:\n\n");
        AcpiOsPrintf ("Calls to AcpiPsFind:..  ........% 7ld\n",
            AcpiGbl_PsFindCount);
        AcpiOsPrintf ("Calls to AcpiNsLookup:..........% 7ld\n",
            AcpiGbl_NsLookupCount);

        AcpiOsPrintf ("\n");

        AcpiOsPrintf ("Mutex usage:\n\n");
        for (i = 0; i < ACPI_NUM_MUTEX; i++)
        {
            AcpiOsPrintf ("%-28s:       % 7ld\n",
                AcpiUtGetMutexName (i), AcpiGbl_MutexInfo[i].UseCount);
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
        AcpiOsPrintf ("AcpiObject       %3d\n", sizeof (ACPI_OBJECT));

        AcpiOsPrintf ("\n");

        AcpiOsPrintf ("Generic State    %3d\n", sizeof (ACPI_GENERIC_STATE));
        AcpiOsPrintf ("Common State     %3d\n", sizeof (ACPI_COMMON_STATE));
        AcpiOsPrintf ("Control State    %3d\n", sizeof (ACPI_CONTROL_STATE));
        AcpiOsPrintf ("Update State     %3d\n", sizeof (ACPI_UPDATE_STATE));
        AcpiOsPrintf ("Scope State      %3d\n", sizeof (ACPI_SCOPE_STATE));
        AcpiOsPrintf ("Parse Scope      %3d\n", sizeof (ACPI_PSCOPE_STATE));
        AcpiOsPrintf ("Package State    %3d\n", sizeof (ACPI_PKG_STATE));
        AcpiOsPrintf ("Thread State     %3d\n", sizeof (ACPI_THREAD_STATE));
        AcpiOsPrintf ("Result Values    %3d\n", sizeof (ACPI_RESULT_VALUES));
        AcpiOsPrintf ("Notify Info      %3d\n", sizeof (ACPI_NOTIFY_INFO));
        break;

    case CMD_STAT_STACK:
#if defined(ACPI_DEBUG_OUTPUT)

        Temp = (UINT32) ACPI_PTR_DIFF (
            AcpiGbl_EntryStackPointer, AcpiGbl_LowestStackPointer);

        AcpiOsPrintf ("\nSubsystem Stack Usage:\n\n");
        AcpiOsPrintf ("Entry Stack Pointer          %p\n", AcpiGbl_EntryStackPointer);
        AcpiOsPrintf ("Lowest Stack Pointer         %p\n", AcpiGbl_LowestStackPointer);
        AcpiOsPrintf ("Stack Use                    %X (%u)\n", Temp, Temp);
        AcpiOsPrintf ("Deepest Procedure Nesting    %u\n", AcpiGbl_DeepestNesting);
#endif
        break;

    default:

        break;
    }

    AcpiOsPrintf ("\n");
    return (AE_OK);
}
