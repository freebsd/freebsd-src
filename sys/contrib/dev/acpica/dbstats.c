/*******************************************************************************
 *
 * Module Name: dbstats - Generation and display of ACPI table statistics
 *              $Revision: 34 $
 *
 ******************************************************************************/

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


#include <acpi.h>
#include <acdebug.h>
#include <amlcode.h>
#include <acparser.h>
#include <acnamesp.h>

#ifdef ENABLE_DEBUGGER

#define _COMPONENT          DEBUGGER
        MODULE_NAME         ("dbstats")

/*
 * Statistics subcommands
 */
ARGUMENT_INFO               AcpiDbStatTypes [] =
{
    {"ALLOCATIONS"},
    {"OBJECTS"},
    {"MEMORY"},
    {"MISC"},
    {"TABLES"},
    {"SIZES"},
    {NULL}           /* Must be null terminated */
};

#define CMD_ALLOCATIONS     0
#define CMD_OBJECTS         1
#define CMD_MEMORY          2
#define CMD_MISC            3
#define CMD_TABLES          4
#define CMD_SIZES           5


/*
 * Statistic globals
 */
UINT16                      AcpiGbl_ObjTypeCount[INTERNAL_TYPE_NODE_MAX+1];
UINT16                      AcpiGbl_NodeTypeCount[INTERNAL_TYPE_NODE_MAX+1];
UINT16                      AcpiGbl_ObjTypeCountMisc;
UINT16                      AcpiGbl_NodeTypeCountMisc;
UINT32                      NumNodes;
UINT32                      NumObjects;


UINT32                      SizeOfParseTree;
UINT32                      SizeOfMethodTrees;
UINT32                      SizeOfNodeEntries;
UINT32                      SizeOfAcpiObjects;


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbEnumerateObject
 *
 * PARAMETERS:  ObjDesc             - Object to be counted
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add this object to the global counts, by object type.
 *              Recursively handles subobjects and packages.
 *
 *              [TBD] Restructure - remove recursion.
 *
 ******************************************************************************/

void
AcpiDbEnumerateObject (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    UINT32                  Type;
    UINT32                  i;


    if (!ObjDesc)
    {
        return;
    }


    /* Enumerate this object first */

    NumObjects++;

    Type = ObjDesc->Common.Type;
    if (Type > INTERNAL_TYPE_NODE_MAX)
    {
        AcpiGbl_ObjTypeCountMisc++;
    }
    else
    {
        AcpiGbl_ObjTypeCount [Type]++;
    }

    /* Count the sub-objects */

    switch (Type)
    {
    case ACPI_TYPE_PACKAGE:
        for (i = 0; i< ObjDesc->Package.Count; i++)
        {
            AcpiDbEnumerateObject (ObjDesc->Package.Elements[i]);
        }
        break;

    case ACPI_TYPE_DEVICE:
        AcpiDbEnumerateObject (ObjDesc->Device.SysHandler);
        AcpiDbEnumerateObject (ObjDesc->Device.DrvHandler);
        AcpiDbEnumerateObject (ObjDesc->Device.AddrHandler);
        break;

    case ACPI_TYPE_REGION:
        AcpiDbEnumerateObject (ObjDesc->Region.AddrHandler);
        break;

    case ACPI_TYPE_POWER:
        AcpiDbEnumerateObject (ObjDesc->PowerResource.SysHandler);
        AcpiDbEnumerateObject (ObjDesc->PowerResource.DrvHandler);
        break;

    case ACPI_TYPE_PROCESSOR:
        AcpiDbEnumerateObject (ObjDesc->Processor.SysHandler);
        AcpiDbEnumerateObject (ObjDesc->Processor.DrvHandler);
        AcpiDbEnumerateObject (ObjDesc->Processor.AddrHandler);
        break;

    case ACPI_TYPE_THERMAL:
        AcpiDbEnumerateObject (ObjDesc->ThermalZone.SysHandler);
        AcpiDbEnumerateObject (ObjDesc->ThermalZone.DrvHandler);
        AcpiDbEnumerateObject (ObjDesc->ThermalZone.AddrHandler);
        break;
    }
}


#ifndef PARSER_ONLY

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


    NumNodes++;

    Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ObjDesc = ((ACPI_NAMESPACE_NODE *) ObjHandle)->Object;

    AcpiDbEnumerateObject (ObjDesc);

    Type = Node->Type;
    if (Type > INTERNAL_TYPE_INVALID)
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

ACPI_STATUS
AcpiDbCountNamespaceObjects (
    void)
{
    UINT32                  i;


    NumNodes = 0;
    NumObjects = 0;

    AcpiGbl_ObjTypeCountMisc = 0;
    for (i = 0; i < INTERNAL_TYPE_INVALID; i++)
    {
        AcpiGbl_ObjTypeCount [i] = 0;
        AcpiGbl_NodeTypeCount [i] = 0;
    }

    AcpiNsWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                        FALSE, AcpiDbClassifyOneObject, NULL, NULL);

    return (AE_OK);
}

#endif


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
    NATIVE_CHAR             *TypeArg)
{
    UINT32                  i;
    UINT32                  Type;


    if (!AcpiGbl_DSDT)
    {
        AcpiOsPrintf ("*** Warning:  There is no DSDT loaded\n");
    }

    if (!TypeArg)
    {
        AcpiOsPrintf ("The following subcommands are available:\n    ALLOCATIONS, OBJECTS, MEMORY, MISC, SIZES, TABLES\n");
        return (AE_OK);
    }

    STRUPR (TypeArg);
    Type = AcpiDbMatchArgument (TypeArg, AcpiDbStatTypes);
    if (Type == (UINT32) -1)
    {
        AcpiOsPrintf ("Invalid or unsupported argument\n");
        return (AE_OK);
    }

#ifndef PARSER_ONLY

    AcpiDbCountNamespaceObjects ();
#endif


    switch (Type)
    {
#ifndef PARSER_ONLY
    case CMD_ALLOCATIONS:
        AcpiCmDumpAllocationInfo ();
        break;
#endif

    case CMD_TABLES:

        AcpiOsPrintf ("ACPI Table Information:\n\n");
        if (AcpiGbl_DSDT)
        {
            AcpiOsPrintf ("DSDT Length:................% 7ld (0x%X)\n", AcpiGbl_DSDT->Length, AcpiGbl_DSDT->Length);
        }
        break;

    case CMD_OBJECTS:

        AcpiOsPrintf ("\nObjects defined in the current namespace:\n\n");

        AcpiOsPrintf ("%16.16s % 10.10s % 10.10s\n", "ACPI_TYPE", "NODES", "OBJECTS");

        for (i = 0; i < INTERNAL_TYPE_NODE_MAX; i++)
        {
            AcpiOsPrintf ("%16.16s % 10ld% 10ld\n", AcpiCmGetTypeName (i),
                AcpiGbl_NodeTypeCount [i], AcpiGbl_ObjTypeCount [i]);
        }
        AcpiOsPrintf ("%16.16s % 10ld% 10ld\n", "Misc/Unknown",
            AcpiGbl_NodeTypeCountMisc, AcpiGbl_ObjTypeCountMisc);

        AcpiOsPrintf ("%16.16s % 10ld% 10ld\n", "TOTALS:",
            NumNodes, NumObjects);


/*
        AcpiOsPrintf ("\n");

        AcpiOsPrintf ("ASL/AML Grammar Usage:\n\n");
        AcpiOsPrintf ("Elements Inside Methods:....% 7ld\n", NumMethodElements);
        AcpiOsPrintf ("Elements Outside Methods:...% 7ld\n", NumGrammarElements - NumMethodElements);
        AcpiOsPrintf ("Total Grammar Elements:.....% 7ld\n", NumGrammarElements);
*/
        break;

    case CMD_MEMORY:

        AcpiOsPrintf ("\nDynamic Memory Estimates:\n\n");
        AcpiOsPrintf ("Parse Tree without Methods:.% 7ld\n", SizeOfParseTree);
        AcpiOsPrintf ("Control Method Parse Trees:.% 7ld (If parsed simultaneously)\n", SizeOfMethodTrees);
        AcpiOsPrintf ("Namespace Nodes:............% 7ld (%d nodes)\n", sizeof (ACPI_NAMESPACE_NODE) * NumNodes, NumNodes);
        AcpiOsPrintf ("Named Internal Objects......% 7ld\n", SizeOfAcpiObjects);
        AcpiOsPrintf ("State Cache size............% 7ld\n", AcpiGbl_GenericStateCacheDepth * sizeof (ACPI_GENERIC_STATE));
        AcpiOsPrintf ("Parse Cache size............% 7ld\n", AcpiGbl_ParseCacheDepth * sizeof (ACPI_PARSE_OBJECT));
        AcpiOsPrintf ("Object Cache size...........% 7ld\n", AcpiGbl_ObjectCacheDepth * sizeof (ACPI_OPERAND_OBJECT));
        AcpiOsPrintf ("WalkState Cache size........% 7ld\n", AcpiGbl_WalkStateCacheDepth * sizeof (ACPI_WALK_STATE));

        AcpiOsPrintf ("\n");

        AcpiOsPrintf ("Cache Statistics:\n\n");
        AcpiOsPrintf ("State Cache requests........% 7ld\n", AcpiGbl_StateCacheRequests);
        AcpiOsPrintf ("State Cache hits............% 7ld\n", AcpiGbl_StateCacheHits);
        AcpiOsPrintf ("State Cache depth...........% 7ld (%d remaining entries)\n", AcpiGbl_GenericStateCacheDepth,
                                                            MAX_STATE_CACHE_DEPTH - AcpiGbl_GenericStateCacheDepth);
        AcpiOsPrintf ("Parse Cache requests........% 7ld\n", AcpiGbl_ParseCacheRequests);
        AcpiOsPrintf ("Parse Cache hits............% 7ld\n", AcpiGbl_ParseCacheHits);
        AcpiOsPrintf ("Parse Cache depth...........% 7ld (%d remaining entries)\n", AcpiGbl_ParseCacheDepth,
                                                            MAX_PARSE_CACHE_DEPTH - AcpiGbl_ParseCacheDepth);
        AcpiOsPrintf ("Ext Parse Cache requests....% 7ld\n", AcpiGbl_ExtParseCacheRequests);
        AcpiOsPrintf ("Ext Parse Cache hits........% 7ld\n", AcpiGbl_ExtParseCacheHits);
        AcpiOsPrintf ("Ext Parse Cache depth.......% 7ld (%d remaining entries)\n", AcpiGbl_ExtParseCacheDepth,
                                                            MAX_EXTPARSE_CACHE_DEPTH - AcpiGbl_ExtParseCacheDepth);
        AcpiOsPrintf ("Object Cache requests.......% 7ld\n", AcpiGbl_ObjectCacheRequests);
        AcpiOsPrintf ("Object Cache hits...........% 7ld\n", AcpiGbl_ObjectCacheHits);
        AcpiOsPrintf ("Object Cache depth..........% 7ld (%d remaining entries)\n", AcpiGbl_ObjectCacheDepth,
                                                            MAX_OBJECT_CACHE_DEPTH - AcpiGbl_ObjectCacheDepth);
        AcpiOsPrintf ("WalkState Cache requests....% 7ld\n", AcpiGbl_WalkStateCacheRequests);
        AcpiOsPrintf ("WalkState Cache hits........% 7ld\n", AcpiGbl_WalkStateCacheHits);
        AcpiOsPrintf ("WalkState Cache depth.......% 7ld (%d remaining entries)\n", AcpiGbl_WalkStateCacheDepth,
                                                            MAX_WALK_CACHE_DEPTH - AcpiGbl_WalkStateCacheDepth);
        break;

    case CMD_MISC:

        AcpiOsPrintf ("\nMiscellaneous Statistics:\n\n");
        AcpiOsPrintf ("Calls to AcpiPsFind:..  ........% 7ld\n", AcpiGbl_PsFindCount);
        AcpiOsPrintf ("Calls to AcpiNsLookup:..........% 7ld\n", AcpiGbl_NsLookupCount);

        AcpiOsPrintf ("\n");

        AcpiOsPrintf ("Mutex usage:\n\n");
        for (i = 0; i < NUM_MTX; i++)
        {
            AcpiOsPrintf ("%-20s:       % 7ld\n", AcpiCmGetMutexName (i), AcpiGbl_AcpiMutexInfo[i].UseCount);
        }
        break;


    case CMD_SIZES:

        AcpiOsPrintf ("\nInternal object sizes:\n\n");

        AcpiOsPrintf ("Common           %3d\n", sizeof (ACPI_OBJECT_COMMON));
        AcpiOsPrintf ("Number           %3d\n", sizeof (ACPI_OBJECT_NUMBER));
        AcpiOsPrintf ("String           %3d\n", sizeof (ACPI_OBJECT_STRING));
        AcpiOsPrintf ("Buffer           %3d\n", sizeof (ACPI_OBJECT_BUFFER));
        AcpiOsPrintf ("Package          %3d\n", sizeof (ACPI_OBJECT_PACKAGE));
        AcpiOsPrintf ("FieldUnit        %3d\n", sizeof (ACPI_OBJECT_FIELD_UNIT));
        AcpiOsPrintf ("Device           %3d\n", sizeof (ACPI_OBJECT_DEVICE));
        AcpiOsPrintf ("Event            %3d\n", sizeof (ACPI_OBJECT_EVENT));
        AcpiOsPrintf ("Method           %3d\n", sizeof (ACPI_OBJECT_METHOD));
        AcpiOsPrintf ("Mutex            %3d\n", sizeof (ACPI_OBJECT_MUTEX));
        AcpiOsPrintf ("Region           %3d\n", sizeof (ACPI_OBJECT_REGION));
        AcpiOsPrintf ("PowerResource    %3d\n", sizeof (ACPI_OBJECT_POWER_RESOURCE));
        AcpiOsPrintf ("Processor        %3d\n", sizeof (ACPI_OBJECT_PROCESSOR));
        AcpiOsPrintf ("ThermalZone      %3d\n", sizeof (ACPI_OBJECT_THERMAL_ZONE));
        AcpiOsPrintf ("Field            %3d\n", sizeof (ACPI_OBJECT_FIELD));
        AcpiOsPrintf ("BankField        %3d\n", sizeof (ACPI_OBJECT_BANK_FIELD));
        AcpiOsPrintf ("IndexField       %3d\n", sizeof (ACPI_OBJECT_INDEX_FIELD));
        AcpiOsPrintf ("Reference        %3d\n", sizeof (ACPI_OBJECT_REFERENCE));
        AcpiOsPrintf ("NotifyHandler    %3d\n", sizeof (ACPI_OBJECT_NOTIFY_HANDLER));
        AcpiOsPrintf ("AddrHandler      %3d\n", sizeof (ACPI_OBJECT_ADDR_HANDLER));
        AcpiOsPrintf ("Extra            %3d\n", sizeof (ACPI_OBJECT_EXTRA));

        AcpiOsPrintf ("\n");

        AcpiOsPrintf ("ParseObject      %3d\n", sizeof (ACPI_PARSE_OBJECT));
        AcpiOsPrintf ("Parse2Object     %3d\n", sizeof (ACPI_PARSE2_OBJECT));
        AcpiOsPrintf ("OperandObject    %3d\n", sizeof (ACPI_OPERAND_OBJECT));
        AcpiOsPrintf ("NamespaceNode    %3d\n", sizeof (ACPI_NAMESPACE_NODE));

        break;

    }

    AcpiOsPrintf ("\n");
    return (AE_OK);
}


#endif /* ENABLE_DEBUGGER  */
