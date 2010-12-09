/*******************************************************************************
 *
 * Module Name: dbcmds - debug commands and output routines
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
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
#include "accommon.h"
#include "acdispat.h"
#include "acnamesp.h"
#include "acevents.h"
#include "acdebug.h"
#include "acresrc.h"
#include "acdisasm.h"
#include "actables.h"
#include "acparser.h"

#ifdef ACPI_DEBUGGER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbcmds")


/* Local prototypes */

static ACPI_STATUS
AcpiDbIntegrityWalk (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
AcpiDbWalkAndMatchName (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
AcpiDbWalkForReferences (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
AcpiDbWalkForSpecificObjects (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_NAMESPACE_NODE *
AcpiDbConvertToNode (
    char                    *InString);

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


/*
 * Arguments for the Objects command
 * These object types map directly to the ACPI_TYPES
 */
static ARGUMENT_INFO        AcpiDbObjectTypes [] =
{
    {"ANY"},
    {"INTEGERS"},
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
    {"DEBUG"},
    {"REGIONFIELDS"},
    {"BANKFIELDS"},
    {"INDEXFIELDS"},
    {"REFERENCES"},
    {"ALIAS"},
    {NULL}           /* Must be null terminated */
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbConvertToNode
 *
 * PARAMETERS:  InString        - String to convert
 *
 * RETURN:      Pointer to a NS node
 *
 * DESCRIPTION: Convert a string to a valid NS pointer.  Handles numeric or
 *              alpha strings.
 *
 ******************************************************************************/

static ACPI_NAMESPACE_NODE *
AcpiDbConvertToNode (
    char                    *InString)
{
    ACPI_NAMESPACE_NODE     *Node;


    if ((*InString >= 0x30) && (*InString <= 0x39))
    {
        /* Numeric argument, convert */

        Node = ACPI_TO_POINTER (ACPI_STRTOUL (InString, NULL, 16));
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
        /* Alpha argument */
        /* The parameter is a name string that must be resolved to a
         * Named obj
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
 * PARAMETERS:  ObjectArg       - Desired sleep state (0-5)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Simulate a sleep/wake sequence
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbSleep (
    char                    *ObjectArg)
{
    ACPI_STATUS             Status;
    UINT8                   SleepState;


    SleepState = (UINT8) ACPI_STRTOUL (ObjectArg, NULL, 0);

    AcpiOsPrintf ("**** Prepare to sleep ****\n");
    Status = AcpiEnterSleepStatePrep (SleepState);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    AcpiOsPrintf ("**** Going to sleep ****\n");
    Status = AcpiEnterSleepState (SleepState);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    AcpiOsPrintf ("**** returning from sleep ****\n");
    Status = AcpiLeaveSleepState (SleepState);

    return (Status);
}


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

static ACPI_STATUS
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
        AcpiOsPrintf ("Object is a Node [%4.4s]\n",
            AcpiUtGetNodeName (Node));
    }

    /* Check for match against the object attached to the node */

    if (AcpiNsGetAttachedObject (Node) == ObjDesc)
    {
        AcpiOsPrintf ("Reference at Node->Object %p [%4.4s]\n",
            Node, AcpiUtGetNodeName (Node));
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
    char                    *ObjectArg)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;


    /* Convert string to object pointer */

    ObjDesc = ACPI_TO_POINTER (ACPI_STRTOUL (ObjectArg, NULL, 16));

    /* Search all nodes in namespace */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                    AcpiDbWalkForReferences, NULL, (void *) ObjDesc, NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbWalkForPredefinedNames
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Detect and display predefined ACPI names (names that start with
 *              an underscore)
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbWalkForPredefinedNames (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE         *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    UINT32                      *Count = (UINT32 *) Context;
    const ACPI_PREDEFINED_INFO  *Predefined;
    const ACPI_PREDEFINED_INFO  *Package = NULL;
    char                        *Pathname;


    Predefined = AcpiNsCheckForPredefinedName (Node);
    if (!Predefined)
    {
        return (AE_OK);
    }

    Pathname = AcpiNsGetExternalPathname (Node);
    if (!Pathname)
    {
        return (AE_OK);
    }

    /* If method returns a package, the info is in the next table entry */

    if (Predefined->Info.ExpectedBtypes & ACPI_BTYPE_PACKAGE)
    {
        Package = Predefined + 1;
    }

    AcpiOsPrintf ("%-32s arg %X ret %2.2X", Pathname,
        Predefined->Info.ParamCount, Predefined->Info.ExpectedBtypes);

    if (Package)
    {
        AcpiOsPrintf (" PkgType %2.2X ObjType %2.2X Count %2.2X",
            Package->RetInfo.Type, Package->RetInfo.ObjectType1,
            Package->RetInfo.Count1);
    }

    AcpiOsPrintf("\n");

    AcpiNsCheckParameterCount (Pathname, Node, ACPI_UINT32_MAX, Predefined);
    ACPI_FREE (Pathname);
    (*Count)++;

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCheckPredefinedNames
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Validate all predefined names in the namespace
 *
 ******************************************************************************/

void
AcpiDbCheckPredefinedNames (
    void)
{
    UINT32                  Count = 0;


    /* Search all nodes in namespace */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                AcpiDbWalkForPredefinedNames, NULL, (void *) &Count, NULL);

    AcpiOsPrintf ("Found %u predefined names in the namespace\n", Count);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbWalkForExecute
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Batch execution module. Currently only executes predefined
 *              ACPI names.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbWalkForExecute (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_EXECUTE_WALK       *Info = (ACPI_EXECUTE_WALK *) Context;
    ACPI_BUFFER             ReturnObj;
    ACPI_STATUS             Status;
    char                    *Pathname;
    UINT32                  i;
    ACPI_DEVICE_INFO        *ObjInfo;
    ACPI_OBJECT_LIST        ParamObjects;
    ACPI_OBJECT             Params[ACPI_METHOD_NUM_ARGS];
    const ACPI_PREDEFINED_INFO *Predefined;


    Predefined = AcpiNsCheckForPredefinedName (Node);
    if (!Predefined)
    {
        return (AE_OK);
    }

    if (Node->Type == ACPI_TYPE_LOCAL_SCOPE)
    {
        return (AE_OK);
    }

    Pathname = AcpiNsGetExternalPathname (Node);
    if (!Pathname)
    {
        return (AE_OK);
    }

    /* Get the object info for number of method parameters */

    Status = AcpiGetObjectInfo (ObjHandle, &ObjInfo);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParamObjects.Pointer = NULL;
    ParamObjects.Count   = 0;

    if (ObjInfo->Type == ACPI_TYPE_METHOD)
    {
        /* Setup default parameters */

        for (i = 0; i < ObjInfo->ParamCount; i++)
        {
            Params[i].Type           = ACPI_TYPE_INTEGER;
            Params[i].Integer.Value  = 1;
        }

        ParamObjects.Pointer     = Params;
        ParamObjects.Count       = ObjInfo->ParamCount;
    }

    ACPI_FREE (ObjInfo);
    ReturnObj.Pointer = NULL;
    ReturnObj.Length = ACPI_ALLOCATE_BUFFER;

    /* Do the actual method execution */

    AcpiGbl_MethodExecuting = TRUE;

    Status = AcpiEvaluateObject (Node, NULL, &ParamObjects, &ReturnObj);

    AcpiOsPrintf ("%-32s returned %s\n", Pathname, AcpiFormatException (Status));
    AcpiGbl_MethodExecuting = FALSE;
    ACPI_FREE (Pathname);

    /* Ignore status from method execution */

    Status = AE_OK;

    /* Update count, check if we have executed enough methods */

    Info->Count++;
    if (Info->Count >= Info->MaxCount)
    {
        Status = AE_CTRL_TERMINATE;
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbBatchExecute
 *
 * PARAMETERS:  CountArg            - Max number of methods to execute
 *
 * RETURN:      None
 *
 * DESCRIPTION: Namespace batch execution. Execute predefined names in the
 *              namespace, up to the max count, if specified.
 *
 ******************************************************************************/

void
AcpiDbBatchExecute (
    char                    *CountArg)
{
    ACPI_EXECUTE_WALK       Info;


    Info.Count = 0;
    Info.MaxCount = ACPI_UINT32_MAX;

    if (CountArg)
    {
        Info.MaxCount = ACPI_STRTOUL (CountArg, NULL, 0);
    }


    /* Search all nodes in namespace */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                AcpiDbWalkForExecute, NULL, (void *) &Info, NULL);

    AcpiOsPrintf ("Executed %u predefined names in the namespace\n", Info.Count);
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
    char                    *TableArg)
{
    UINT32                  i;
    ACPI_TABLE_DESC         *TableDesc;
    ACPI_STATUS             Status;


    /* Walk the entire root table list */

    for (i = 0; i < AcpiGbl_RootTableList.CurrentTableCount; i++)
    {
        TableDesc = &AcpiGbl_RootTableList.Tables[i];
        AcpiOsPrintf ("%u ", i);

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
    char                    *TableArg,
    char                    *InstanceArg)
{
/* TBD: Need to reimplement for new data structures */

#if 0
    UINT32                  i;
    ACPI_STATUS             Status;


    /* Search all tables for the target type */

    for (i = 0; i < (ACPI_TABLE_ID_MAX+1); i++)
    {
        if (!ACPI_STRNCMP (TableArg, AcpiGbl_TableData[i].Signature,
                AcpiGbl_TableData[i].SigLength))
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
#endif
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
    char                    *Location,
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
        AcpiOsPrintf ("Breakpoint %X is beyond current address %X\n",
            Address, Op->Common.AmlOffset);
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
    char                    *Statements,
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

    AcpiDmDisassemble (NULL, Op, NumStatements);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisassembleMethod
 *
 * PARAMETERS:  Name            - Name of control method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display disassembled AML (ASL) starting from Op for the number
 *              of statements specified.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbDisassembleMethod (
    char                    *Name)
{
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *Op;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Method;


    Method = AcpiDbConvertToNode (Name);
    if (!Method)
    {
        return (AE_BAD_PARAMETER);
    }

    ObjDesc = Method->Object;

    Op = AcpiPsCreateScopeOp ();
    if (!Op)
    {
        return (AE_NO_MEMORY);
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (0, Op, NULL, NULL);
    if (!WalkState)
    {
        return (AE_NO_MEMORY);
    }

    Status = AcpiDsInitAmlWalk (WalkState, Op, NULL,
                    ObjDesc->Method.AmlStart,
                    ObjDesc->Method.AmlLength, NULL, ACPI_IMODE_LOAD_PASS1);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Parse the AML */

    WalkState->ParseFlags &= ~ACPI_PARSE_DELETE_TREE;
    WalkState->ParseFlags |= ACPI_PARSE_DISASSEMBLE;
    Status = AcpiPsParseAml (WalkState);

    AcpiDmDisassemble (NULL, Op, 0);
    AcpiPsDeleteParseTree (Op);
    return (AE_OK);
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
    char                    *StartArg,
    char                    *DepthArg)
{
    ACPI_HANDLE             SubtreeEntry = AcpiGbl_RootNode;
    UINT32                  MaxDepth = ACPI_UINT32_MAX;


    /* No argument given, just start at the root and dump entire namespace */

    if (StartArg)
    {
        SubtreeEntry = AcpiDbConvertToNode (StartArg);
        if (!SubtreeEntry)
        {
            return;
        }

        /* Now we can check for the depth argument */

        if (DepthArg)
        {
            MaxDepth = ACPI_STRTOUL (DepthArg, NULL, 0);
        }
    }

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("ACPI Namespace (from %4.4s (%p) subtree):\n",
        ((ACPI_NAMESPACE_NODE *) SubtreeEntry)->Name.Ascii, SubtreeEntry);

    /* Display the subtree */

    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
    AcpiNsDumpObjects (ACPI_TYPE_ANY, ACPI_DISPLAY_SUMMARY, MaxDepth,
        ACPI_OWNER_ID_MAX, SubtreeEntry);
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
    char                    *OwnerArg,
    char                    *DepthArg)
{
    ACPI_HANDLE             SubtreeEntry = AcpiGbl_RootNode;
    UINT32                  MaxDepth = ACPI_UINT32_MAX;
    ACPI_OWNER_ID           OwnerId;


    OwnerId = (ACPI_OWNER_ID) ACPI_STRTOUL (OwnerArg, NULL, 0);

    /* Now we can check for the depth argument */

    if (DepthArg)
    {
        MaxDepth = ACPI_STRTOUL (DepthArg, NULL, 0);
    }

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("ACPI Namespace by owner %X:\n", OwnerId);

    /* Display the subtree */

    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
    AcpiNsDumpObjects (ACPI_TYPE_ANY, ACPI_DISPLAY_SUMMARY, MaxDepth, OwnerId,
        SubtreeEntry);
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
    char                    *TypeArg,
    char                    *IndexArg,
    char                    *ValueArg)
{
    char                    Type;
    UINT32                  Index;
    UINT32                  Value;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;


    /* Validate TypeArg */

    AcpiUtStrupr (TypeArg);
    Type = TypeArg[0];
    if ((Type != 'L') &&
        (Type != 'A') &&
        (Type != 'N'))
    {
        AcpiOsPrintf ("Invalid SET operand: %s\n", TypeArg);
        return;
    }

    Value = ACPI_STRTOUL (ValueArg, NULL, 16);

    if (Type == 'N')
    {
        Node = AcpiDbConvertToNode (IndexArg);
        if (Node->Type != ACPI_TYPE_INTEGER)
        {
            AcpiOsPrintf ("Can only set Integer nodes\n");
            return;
        }
        ObjDesc = Node->Object;
        ObjDesc->Integer.Value = Value;
        return;
    }

    /* Get the index and value */

    Index = ACPI_STRTOUL (IndexArg, NULL, 16);

    WalkState = AcpiDsGetCurrentWalkState (AcpiGbl_CurrentWalkList);
    if (!WalkState)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }

    /* Create and initialize the new object */

    ObjDesc = AcpiUtCreateIntegerObject ((UINT64) Value);
    if (!ObjDesc)
    {
        AcpiOsPrintf ("Could not create an internal object\n");
        return;
    }

    /* Store the new object into the target */

    switch (Type)
    {
    case 'A':

        /* Set a method argument */

        if (Index > ACPI_METHOD_MAX_ARG)
        {
            AcpiOsPrintf ("Arg%u - Invalid argument name\n", Index);
            goto Cleanup;
        }

        Status = AcpiDsStoreObjectToLocal (ACPI_REFCLASS_ARG, Index, ObjDesc,
                    WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        ObjDesc = WalkState->Arguments[Index].Object;

        AcpiOsPrintf ("Arg%u: ", Index);
        AcpiDmDisplayInternalObject (ObjDesc, WalkState);
        break;

    case 'L':

        /* Set a method local */

        if (Index > ACPI_METHOD_MAX_LOCAL)
        {
            AcpiOsPrintf ("Local%u - Invalid local variable name\n", Index);
            goto Cleanup;
        }

        Status = AcpiDsStoreObjectToLocal (ACPI_REFCLASS_LOCAL, Index, ObjDesc,
                    WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        ObjDesc = WalkState->LocalVariables[Index].Object;

        AcpiOsPrintf ("Local%u: ", Index);
        AcpiDmDisplayInternalObject (ObjDesc, WalkState);
        break;

    default:
        break;
    }

Cleanup:
    AcpiUtRemoveReference (ObjDesc);
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

static ACPI_STATUS
AcpiDbWalkForSpecificObjects (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_WALK_INFO          *Info = (ACPI_WALK_INFO *) Context;
    ACPI_BUFFER             Buffer;
    ACPI_STATUS             Status;


    Info->Count++;

    /* Get and display the full pathname to this object */

    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiNsHandleToPathname (ObjHandle, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could Not get pathname for object %p\n", ObjHandle);
        return (AE_OK);
    }

    AcpiOsPrintf ("%32s", (char *) Buffer.Pointer);
    ACPI_FREE (Buffer.Pointer);

    /* Dump short info about the object */

    (void) AcpiNsDumpOneObject (ObjHandle, NestingLevel, Info, NULL);
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
    char                    *ObjTypeArg,
    char                    *DisplayCountArg)
{
    ACPI_WALK_INFO          Info;
    ACPI_OBJECT_TYPE        Type;


    /* Get the object type */

    Type = AcpiDbMatchArgument (ObjTypeArg, AcpiDbObjectTypes);
    if (Type == ACPI_TYPE_NOT_FOUND)
    {
        AcpiOsPrintf ("Invalid or unsupported argument\n");
        return (AE_OK);
    }

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf (
        "Objects of type [%s] defined in the current ACPI Namespace:\n",
        AcpiUtGetTypeName (Type));

    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);

    Info.Count = 0;
    Info.OwnerId = ACPI_OWNER_ID_MAX;
    Info.DebugLevel = ACPI_UINT32_MAX;
    Info.DisplayType = ACPI_DISPLAY_SUMMARY | ACPI_DISPLAY_SHORT;

    /* Walk the namespace from the root */

    (void) AcpiWalkNamespace (Type, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                AcpiDbWalkForSpecificObjects, NULL, (void *) &Info, NULL);

    AcpiOsPrintf (
        "\nFound %u objects of type [%s] in the current ACPI Namespace\n",
        Info.Count, AcpiUtGetTypeName (Type));

    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
    return (AE_OK);
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

static ACPI_STATUS
AcpiDbWalkAndMatchName (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;
    char                    *RequestedName = (char *) Context;
    UINT32                  i;
    ACPI_BUFFER             Buffer;
    ACPI_WALK_INFO          Info;


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
        Info.OwnerId = ACPI_OWNER_ID_MAX;
        Info.DebugLevel = ACPI_UINT32_MAX;
        Info.DisplayType = ACPI_DISPLAY_SUMMARY | ACPI_DISPLAY_SHORT;

        AcpiOsPrintf ("%32s", (char *) Buffer.Pointer);
        (void) AcpiNsDumpOneObject (ObjHandle, NestingLevel, &Info, NULL);
        ACPI_FREE (Buffer.Pointer);
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
    char                    *NameArg)
{
    char                    AcpiName[5] = "____";
    char                    *AcpiNamePtr = AcpiName;


    if (ACPI_STRLEN (NameArg) > 4)
    {
        AcpiOsPrintf ("Name must be no longer than 4 characters\n");
        return (AE_OK);
    }

    /* Pad out name with underscores as necessary to create a 4-char name */

    AcpiUtStrupr (NameArg);
    while (*NameArg)
    {
        *AcpiNamePtr = *NameArg;
        AcpiNamePtr++;
        NameArg++;
    }

    /* Walk the namespace from the root */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                        AcpiDbWalkAndMatchName, NULL, AcpiName, NULL);

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
    char                    *Name)
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

        Status = AcpiNsGetNode (AcpiGbl_RootNode, Name, ACPI_NS_NO_UPSEARCH,
                    &Node);
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

        Status = AcpiNsGetNode (AcpiGbl_DbScopeNode, Name, ACPI_NS_NO_UPSEARCH,
                    &Node);
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

    AcpiOsPrintf ("Could not attach scope: %s, %s\n",
        Name, AcpiFormatException (Status));
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
    ACPI_RSDESC_SIZE        Aml1Length;
    ACPI_RSDESC_SIZE        Aml2Length;
    ACPI_RSDESC_SIZE        Offset = 0;
    UINT8                   ResourceType;
    UINT32                  Count = 0;


    /* Compare overall buffer sizes (may be different due to size rounding) */

    if (Aml1BufferLength != Aml2BufferLength)
    {
        AcpiOsPrintf (
            "**** Buffer length mismatch in converted AML: original %X new %X ****\n",
            Aml1BufferLength, Aml2BufferLength);
    }

    Aml1 = Aml1Buffer;
    Aml2 = Aml2Buffer;

    /* Walk the descriptor lists, comparing each descriptor */

    while (Aml1 < (Aml1Buffer + Aml1BufferLength))
    {
        /* Get the lengths of each descriptor */

        Aml1Length = AcpiUtGetDescriptorLength (Aml1);
        Aml2Length = AcpiUtGetDescriptorLength (Aml2);
        ResourceType = AcpiUtGetResourceType (Aml1);

        /* Check for descriptor length match */

        if (Aml1Length != Aml2Length)
        {
            AcpiOsPrintf (
                "**** Length mismatch in descriptor [%.2X] type %2.2X, Offset %8.8X L1 %X L2 %X ****\n",
                Count, ResourceType, Offset, Aml1Length, Aml2Length);
        }

        /* Check for descriptor byte match */

        else if (ACPI_MEMCMP (Aml1, Aml2, Aml1Length))
        {
            AcpiOsPrintf (
                "**** Data mismatch in descriptor [%.2X] type %2.2X, Offset %8.8X ****\n",
                Count, ResourceType, Offset);
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
 * PARAMETERS:  Node            - Parent device node
 *              Name            - resource method name (_CRS)
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
    ACPI_BUFFER             ReturnObj;
    ACPI_BUFFER             ResourceObj;
    ACPI_BUFFER             NewAml;
    ACPI_OBJECT             *OriginalAml;


    AcpiOsPrintf ("Resource Conversion Comparison:\n");

    NewAml.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    ReturnObj.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    ResourceObj.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

    /* Get the original _CRS AML resource template */

    Status = AcpiEvaluateObject (Node, Name, NULL, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not obtain %s: %s\n",
            Name, AcpiFormatException (Status));
        return (Status);
    }

    /* Get the AML resource template, converted to internal resource structs */

    Status = AcpiGetCurrentResources (Node, &ResourceObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiGetCurrentResources failed: %s\n",
            AcpiFormatException (Status));
        goto Exit1;
    }

    /* Convert internal resource list to external AML resource template */

    Status = AcpiRsCreateAmlResources (ResourceObj.Pointer, &NewAml);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiRsCreateAmlResources failed: %s\n",
            AcpiFormatException (Status));
        goto Exit2;
    }

    /* Compare original AML to the newly created AML resource list */

    OriginalAml = ReturnObj.Pointer;

    AcpiDmCompareAmlResources (
        OriginalAml->Buffer.Pointer, (ACPI_RSDESC_SIZE) OriginalAml->Buffer.Length,
        NewAml.Pointer, (ACPI_RSDESC_SIZE) NewAml.Length);

    /* Cleanup and exit */

    ACPI_FREE (NewAml.Pointer);
Exit2:
    ACPI_FREE (ResourceObj.Pointer);
Exit1:
    ACPI_FREE (ReturnObj.Pointer);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayResources
 *
 * PARAMETERS:  ObjectArg       - String with hex value of the object
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
    ACPI_STATUS             Status;
    ACPI_BUFFER             ReturnObj;


    AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
    AcpiDbgLevel |= ACPI_LV_RESOURCES;

    /* Convert string to object pointer */

    Node = AcpiDbConvertToNode (ObjectArg);
    if (!Node)
    {
        return;
    }

    /* Prepare for a return object of arbitrary size */

    ReturnObj.Pointer = AcpiGbl_DbBuffer;
    ReturnObj.Length  = ACPI_DEBUG_BUFFER_SIZE;

    /* _PRT */

    AcpiOsPrintf ("Evaluating _PRT\n");

    /* Check if _PRT exists */

    Status = AcpiEvaluateObject (Node, METHOD_NAME__PRT, NULL, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not obtain _PRT: %s\n",
            AcpiFormatException (Status));
        goto GetCrs;
    }

    ReturnObj.Pointer = AcpiGbl_DbBuffer;
    ReturnObj.Length  = ACPI_DEBUG_BUFFER_SIZE;

    Status = AcpiGetIrqRoutingTable (Node, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("GetIrqRoutingTable failed: %s\n",
            AcpiFormatException (Status));
        goto GetCrs;
    }

    AcpiRsDumpIrqList (ACPI_CAST_PTR (UINT8, AcpiGbl_DbBuffer));


    /* _CRS */

GetCrs:
    AcpiOsPrintf ("Evaluating _CRS\n");

    ReturnObj.Pointer = AcpiGbl_DbBuffer;
    ReturnObj.Length  = ACPI_DEBUG_BUFFER_SIZE;

    /* Check if _CRS exists */

    Status = AcpiEvaluateObject (Node, METHOD_NAME__CRS, NULL, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not obtain _CRS: %s\n",
            AcpiFormatException (Status));
        goto GetPrs;
    }

    /* Get the _CRS resource list */

    ReturnObj.Pointer = AcpiGbl_DbBuffer;
    ReturnObj.Length  = ACPI_DEBUG_BUFFER_SIZE;

    Status = AcpiGetCurrentResources (Node, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiGetCurrentResources failed: %s\n",
            AcpiFormatException (Status));
        goto GetPrs;
    }

    /* Dump the _CRS resource list */

    AcpiRsDumpResourceList (ACPI_CAST_PTR (ACPI_RESOURCE,
        ReturnObj.Pointer));

    /*
     * Perform comparison of original AML to newly created AML. This tests both
     * the AML->Resource conversion and the Resource->Aml conversion.
     */
    Status = AcpiDmTestResourceConversion (Node, METHOD_NAME__CRS);

    /* Execute _SRS with the resource list */

    Status = AcpiSetCurrentResources (Node, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiSetCurrentResources failed: %s\n",
            AcpiFormatException (Status));
        goto GetPrs;
    }


    /* _PRS */

GetPrs:
    AcpiOsPrintf ("Evaluating _PRS\n");

    ReturnObj.Pointer = AcpiGbl_DbBuffer;
    ReturnObj.Length  = ACPI_DEBUG_BUFFER_SIZE;

    /* Check if _PRS exists */

    Status = AcpiEvaluateObject (Node, METHOD_NAME__PRS, NULL, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not obtain _PRS: %s\n",
            AcpiFormatException (Status));
        goto Cleanup;
    }

    ReturnObj.Pointer = AcpiGbl_DbBuffer;
    ReturnObj.Length  = ACPI_DEBUG_BUFFER_SIZE;

    Status = AcpiGetPossibleResources (Node, &ReturnObj);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("AcpiGetPossibleResources failed: %s\n",
            AcpiFormatException (Status));
        goto Cleanup;
    }

    AcpiRsDumpResourceList (ACPI_CAST_PTR (ACPI_RESOURCE, AcpiGbl_DbBuffer));

Cleanup:

    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
    return;
}


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

static ACPI_STATUS
AcpiDbIntegrityWalk (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_INTEGRITY_INFO     *Info = (ACPI_INTEGRITY_INFO *) Context;
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_OPERAND_OBJECT     *Object;
    BOOLEAN                 Alias = TRUE;


    Info->Nodes++;

    /* Verify the NS node, and dereference aliases */

    while (Alias)
    {
        if (ACPI_GET_DESCRIPTOR_TYPE (Node) != ACPI_DESC_TYPE_NAMED)
        {
            AcpiOsPrintf ("Invalid Descriptor Type for Node %p [%s] - is %2.2X should be %2.2X\n",
                Node, AcpiUtGetDescriptorName (Node), ACPI_GET_DESCRIPTOR_TYPE (Node),
                ACPI_DESC_TYPE_NAMED);
            return (AE_OK);
        }

        if ((Node->Type == ACPI_TYPE_LOCAL_ALIAS)  ||
            (Node->Type == ACPI_TYPE_LOCAL_METHOD_ALIAS))
        {
            Node = (ACPI_NAMESPACE_NODE *) Node->Object;
        }
        else
        {
            Alias = FALSE;
        }
    }

    if (Node->Type > ACPI_TYPE_LOCAL_MAX)
    {
        AcpiOsPrintf ("Invalid Object Type for Node %p, Type = %X\n",
            Node, Node->Type);
        return (AE_OK);
    }

    if (!AcpiUtValidAcpiName (Node->Name.Integer))
    {
        AcpiOsPrintf ("Invalid AcpiName for Node %p\n", Node);
        return (AE_OK);
    }

    Object = AcpiNsGetAttachedObject (Node);
    if (Object)
    {
        Info->Objects++;
        if (ACPI_GET_DESCRIPTOR_TYPE (Object) != ACPI_DESC_TYPE_OPERAND)
        {
            AcpiOsPrintf ("Invalid Descriptor Type for Object %p [%s]\n",
                Object, AcpiUtGetDescriptorName (Object));
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
AcpiDbCheckIntegrity (
    void)
{
    ACPI_INTEGRITY_INFO     Info = {0,0};

    /* Search all nodes in namespace */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                    AcpiDbIntegrityWalk, NULL, (void *) &Info, NULL);

    AcpiOsPrintf ("Verified %u namespace nodes with %u Objects\n",
        Info.Nodes, Info.Objects);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGenerateGpe
 *
 * PARAMETERS:  GpeArg          - Raw GPE number, ascii string
 *              BlockArg        - GPE block number, ascii string
 *                                0 or 1 for FADT GPE blocks
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generate a GPE
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


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbBusWalk
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Display info about device objects that have a corresponding
 *              _PRT method.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbBusWalk (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_STATUS             Status;
    ACPI_BUFFER             Buffer;
    ACPI_NAMESPACE_NODE     *TempNode;
    ACPI_DEVICE_INFO        *Info;
    UINT32                  i;


    if ((Node->Type != ACPI_TYPE_DEVICE) &&
        (Node->Type != ACPI_TYPE_PROCESSOR))
    {
        return (AE_OK);
    }

    /* Exit if there is no _PRT under this device */

    Status = AcpiGetHandle (Node, METHOD_NAME__PRT,
                ACPI_CAST_PTR (ACPI_HANDLE, &TempNode));
    if (ACPI_FAILURE (Status))
    {
        return (AE_OK);
    }

    /* Get the full path to this device object */

    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiNsHandleToPathname (ObjHandle, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could Not get pathname for object %p\n", ObjHandle);
        return (AE_OK);
    }

    Status = AcpiGetObjectInfo (ObjHandle, &Info);
    if (ACPI_FAILURE (Status))
    {
        return (AE_OK);
    }

    /* Display the full path */

    AcpiOsPrintf ("%-32s Type %X", (char *) Buffer.Pointer, Node->Type);
    ACPI_FREE (Buffer.Pointer);

    if (Info->Flags & ACPI_PCI_ROOT_BRIDGE)
    {
        AcpiOsPrintf ("  - Is PCI Root Bridge");
    }
    AcpiOsPrintf ("\n");

    /* _PRT info */

    AcpiOsPrintf ("_PRT: %p\n", TempNode);

    /* Dump _ADR, _HID, _UID, _CID */

    if (Info->Valid & ACPI_VALID_ADR)
    {
        AcpiOsPrintf ("_ADR: %8.8X%8.8X\n", ACPI_FORMAT_UINT64 (Info->Address));
    }
    else
    {
        AcpiOsPrintf ("_ADR: <Not Present>\n");
    }

    if (Info->Valid & ACPI_VALID_HID)
    {
        AcpiOsPrintf ("_HID: %s\n", Info->HardwareId.String);
    }
    else
    {
        AcpiOsPrintf ("_HID: <Not Present>\n");
    }

    if (Info->Valid & ACPI_VALID_UID)
    {
        AcpiOsPrintf ("_UID: %s\n", Info->UniqueId.String);
    }
    else
    {
        AcpiOsPrintf ("_UID: <Not Present>\n");
    }

    if (Info->Valid & ACPI_VALID_CID)
    {
        for (i = 0; i < Info->CompatibleIdList.Count; i++)
        {
            AcpiOsPrintf ("_CID: %s\n",
                Info->CompatibleIdList.Ids[i].String);
        }
    }
    else
    {
        AcpiOsPrintf ("_CID: <Not Present>\n");
    }

    ACPI_FREE (Info);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGetBusInfo
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display info about system busses.
 *
 ******************************************************************************/

void
AcpiDbGetBusInfo (
    void)
{
    /* Search all nodes in namespace */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                    AcpiDbBusWalk, NULL, NULL, NULL);
}

#endif /* ACPI_DEBUGGER */
