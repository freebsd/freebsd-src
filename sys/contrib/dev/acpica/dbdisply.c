/*******************************************************************************
 *
 * Module Name: dbdisply - debug display commands
 *              $Revision: 95 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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
#include "amlcode.h"
#include "acdispat.h"
#include "acnamesp.h"
#include "acparser.h"
#include "acinterp.h"
#include "acdebug.h"
#include "acdisasm.h"


#ifdef ACPI_DEBUGGER


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbdisply")


/******************************************************************************
 *
 * FUNCTION:    AcpiDbGetPointer
 *
 * PARAMETERS:  Target          - Pointer to string to be converted
 *
 * RETURN:      Converted pointer
 *
 * DESCRIPTION: Convert an ascii pointer value to a real value
 *
 *****************************************************************************/

void *
AcpiDbGetPointer (
    void                    *Target)
{
    void                    *ObjPtr;


#if ACPI_MACHINE_WIDTH == 16
#include <stdio.h>

    /* Have to handle 16-bit pointers of the form segment:offset */

    if (!sscanf (Target, "%p", &ObjPtr))
    {
        AcpiOsPrintf ("Invalid pointer: %s\n", Target);
        return (NULL);
    }

#else

    /* Simple flat pointer */

    ObjPtr = ACPI_TO_POINTER (ACPI_STRTOUL (Target, NULL, 16));
#endif

    return (ObjPtr);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDumpParserDescriptor
 *
 * PARAMETERS:  Op              - A parser Op descriptor
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display a formatted parser object
 *
 ******************************************************************************/

void
AcpiDbDumpParserDescriptor (
    ACPI_PARSE_OBJECT       *Op)
{
    const ACPI_OPCODE_INFO  *Info;


    Info = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    AcpiOsPrintf ("Parser Op Descriptor:\n");
    AcpiOsPrintf ("%20.20s : %4.4X\n", "Opcode", Op->Common.AmlOpcode);

    ACPI_DEBUG_ONLY_MEMBERS (AcpiOsPrintf ("%20.20s : %s\n", "Opcode Name", Info->Name));

    AcpiOsPrintf ("%20.20s : %p\n", "Value/ArgList", Op->Common.Value.Arg);
    AcpiOsPrintf ("%20.20s : %p\n", "Parent", Op->Common.Parent);
    AcpiOsPrintf ("%20.20s : %p\n", "NextOp", Op->Common.Next);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDecodeAndDisplayObject
 *
 * PARAMETERS:  Target          - String with object to be displayed.  Names
 *                                and hex pointers are supported.
 *              OutputType      - Byte, Word, Dword, or Qword (B|W|D|Q)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display a formatted ACPI object
 *
 ******************************************************************************/

void
AcpiDbDecodeAndDisplayObject (
    char                    *Target,
    char                    *OutputType)
{
    void                    *ObjPtr;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  Display = DB_BYTE_DISPLAY;
    char                    Buffer[80];
    ACPI_BUFFER             RetBuf;
    ACPI_STATUS             Status;
    UINT32                  Size;


    if (!Target)
    {
        return;
    }

    /* Decode the output type */

    if (OutputType)
    {
        ACPI_STRUPR (OutputType);
        if (OutputType[0] == 'W')
        {
            Display = DB_WORD_DISPLAY;
        }
        else if (OutputType[0] == 'D')
        {
            Display = DB_DWORD_DISPLAY;
        }
        else if (OutputType[0] == 'Q')
        {
            Display = DB_QWORD_DISPLAY;
        }
    }

    RetBuf.Length = sizeof (Buffer);
    RetBuf.Pointer = Buffer;

    /* Differentiate between a number and a name */

    if ((Target[0] >= 0x30) && (Target[0] <= 0x39))
    {
        ObjPtr = AcpiDbGetPointer (Target);
        if (!AcpiOsReadable (ObjPtr, 16))
        {
            AcpiOsPrintf ("Address %p is invalid in this address space\n", ObjPtr);
            return;
        }

        /* Decode the object type */

        switch (ACPI_GET_DESCRIPTOR_TYPE (ObjPtr))
        {
        case ACPI_DESC_TYPE_NAMED:

            /* This is a namespace Node */

            if (!AcpiOsReadable (ObjPtr, sizeof (ACPI_NAMESPACE_NODE)))
            {
                AcpiOsPrintf ("Cannot read entire Named object at address %p\n", ObjPtr);
                return;
            }

            Node = ObjPtr;
            goto DumpNte;


        case ACPI_DESC_TYPE_OPERAND:

            /* This is a ACPI OPERAND OBJECT */

            if (!AcpiOsReadable (ObjPtr, sizeof (ACPI_OPERAND_OBJECT)))
            {
                AcpiOsPrintf ("Cannot read entire ACPI object at address %p\n", ObjPtr);
                return;
            }

            AcpiUtDumpBuffer (ObjPtr, sizeof (ACPI_OPERAND_OBJECT), Display, ACPI_UINT32_MAX);
            AcpiExDumpObjectDescriptor (ObjPtr, 1);
            break;


        case ACPI_DESC_TYPE_PARSER:

            /* This is a Parser Op object */

            if (!AcpiOsReadable (ObjPtr, sizeof (ACPI_PARSE_OBJECT)))
            {
                AcpiOsPrintf ("Cannot read entire Parser object at address %p\n", ObjPtr);
                return;
            }

            AcpiUtDumpBuffer (ObjPtr, sizeof (ACPI_PARSE_OBJECT), Display, ACPI_UINT32_MAX);
            AcpiDbDumpParserDescriptor ((ACPI_PARSE_OBJECT *) ObjPtr);
            break;


        default:

            /* Is not a recognizeable object */

            Size = 16;
            if (AcpiOsReadable (ObjPtr, 64))
            {
                Size = 64;
            }

            /* Just dump some memory */

            AcpiUtDumpBuffer (ObjPtr, Size, Display, ACPI_UINT32_MAX);
            break;
        }

        return;
    }

    /* The parameter is a name string that must be resolved to a Named obj */

    Node = AcpiDbLocalNsLookup (Target);
    if (!Node)
    {
        return;
    }


DumpNte:
    /* Now dump the Named obj */

    Status = AcpiGetName (Node, ACPI_FULL_PATHNAME, &RetBuf);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not convert name to pathname\n");
    }

    else
    {
        AcpiOsPrintf ("Object (%p) Pathname:  %s\n", Node, (char *) RetBuf.Pointer);
    }

    if (!AcpiOsReadable (Node, sizeof (ACPI_NAMESPACE_NODE)))
    {
        AcpiOsPrintf ("Invalid Named object at address %p\n", Node);
        return;
    }

    AcpiUtDumpBuffer ((void *) Node, sizeof (ACPI_NAMESPACE_NODE), Display, ACPI_UINT32_MAX);
    AcpiExDumpNode (Node, 1);

    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (ObjDesc)
    {
        AcpiOsPrintf ("\nAttached Object (%p):\n", ObjDesc);
        if (!AcpiOsReadable (ObjDesc, sizeof (ACPI_OPERAND_OBJECT)))
        {
            AcpiOsPrintf ("Invalid internal ACPI Object at address %p\n", ObjDesc);
            return;
        }

        AcpiUtDumpBuffer ((void *) ObjDesc, sizeof (ACPI_OPERAND_OBJECT), Display, ACPI_UINT32_MAX);
        AcpiExDumpObjectDescriptor (ObjDesc, 1);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayMethodInfo
 *
 * PARAMETERS:  StartOp         - Root of the control method parse tree
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display information about the current method
 *
 ******************************************************************************/

void
AcpiDbDisplayMethodInfo (
    ACPI_PARSE_OBJECT       *StartOp)
{
    ACPI_WALK_STATE         *WalkState;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_PARSE_OBJECT       *RootOp;
    ACPI_PARSE_OBJECT       *Op;
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  NumOps = 0;
    UINT32                  NumOperands = 0;
    UINT32                  NumOperators = 0;
    UINT32                  NumRemainingOps = 0;
    UINT32                  NumRemainingOperands = 0;
    UINT32                  NumRemainingOperators = 0;
    UINT32                  NumArgs;
    UINT32                  Concurrency;
    BOOLEAN                 CountRemaining = FALSE;


    WalkState = AcpiDsGetCurrentWalkState (AcpiGbl_CurrentWalkList);
    if (!WalkState)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }

    ObjDesc = WalkState->MethodDesc;
    Node    = WalkState->MethodNode;

    NumArgs     = ObjDesc->Method.ParamCount;
    Concurrency = ObjDesc->Method.Concurrency;

    AcpiOsPrintf ("Currently executing control method is [%4.4s]\n", Node->Name.Ascii);
    AcpiOsPrintf ("%X arguments, max concurrency = %X\n", NumArgs, Concurrency);


    RootOp = StartOp;
    while (RootOp->Common.Parent)
    {
        RootOp = RootOp->Common.Parent;
    }

    Op = RootOp;

    while (Op)
    {
        if (Op == StartOp)
        {
            CountRemaining = TRUE;
        }

        NumOps++;
        if (CountRemaining)
        {
            NumRemainingOps++;
        }

        /* Decode the opcode */

        OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        switch (OpInfo->Class)
        {
        case AML_CLASS_ARGUMENT:
            if (CountRemaining)
            {
                NumRemainingOperands++;
            }

            NumOperands++;
            break;

        case AML_CLASS_UNKNOWN:
            /* Bad opcode or ASCII character */

            continue;

        default:
            if (CountRemaining)
            {
                NumRemainingOperators++;
            }

            NumOperators++;
            break;
        }

        Op = AcpiPsGetDepthNext (StartOp, Op);
    }

    AcpiOsPrintf ("Method contains:       %X AML Opcodes - %X Operators, %X Operands\n",
                NumOps, NumOperators, NumOperands);

    AcpiOsPrintf ("Remaining to execute:  %X AML Opcodes - %X Operators, %X Operands\n",
                NumRemainingOps, NumRemainingOperators, NumRemainingOperands);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayLocals
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all locals for the currently running control method
 *
 ******************************************************************************/

void
AcpiDbDisplayLocals (void)
{
    ACPI_WALK_STATE         *WalkState;


    WalkState = AcpiDsGetCurrentWalkState (AcpiGbl_CurrentWalkList);
    if (!WalkState)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }

    AcpiDmDisplayLocals (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayArguments
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all arguments for the currently running control method
 *
 ******************************************************************************/

void
AcpiDbDisplayArguments (void)
{
    ACPI_WALK_STATE         *WalkState;


    WalkState = AcpiDsGetCurrentWalkState (AcpiGbl_CurrentWalkList);
    if (!WalkState)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }

    AcpiDmDisplayArguments (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayResults
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display current contents of a method result stack
 *
 ******************************************************************************/

void
AcpiDbDisplayResults (void)
{
    UINT32                  i;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  NumResults = 0;
    ACPI_NAMESPACE_NODE     *Node;


    WalkState = AcpiDsGetCurrentWalkState (AcpiGbl_CurrentWalkList);
    if (!WalkState)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }

    ObjDesc = WalkState->MethodDesc;
    Node = WalkState->MethodNode;

    if (WalkState->Results)
    {
        NumResults = WalkState->Results->Results.NumResults;
    }

    AcpiOsPrintf ("Method [%4.4s] has %X stacked result objects\n",
        Node->Name.Ascii, NumResults);

    for (i = 0; i < NumResults; i++)
    {
        ObjDesc = WalkState->Results->Results.ObjDesc[i];
        AcpiOsPrintf ("Result%d: ", i);
        AcpiDmDisplayInternalObject (ObjDesc, WalkState);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayCallingTree
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display current calling tree of nested control methods
 *
 ******************************************************************************/

void
AcpiDbDisplayCallingTree (void)
{
    ACPI_WALK_STATE         *WalkState;
    ACPI_NAMESPACE_NODE     *Node;


    WalkState = AcpiDsGetCurrentWalkState (AcpiGbl_CurrentWalkList);
    if (!WalkState)
    {
        AcpiOsPrintf ("There is no method currently executing\n");
        return;
    }

    Node = WalkState->MethodNode;
    AcpiOsPrintf ("Current Control Method Call Tree\n");

    while (WalkState)
    {
        Node = WalkState->MethodNode;

        AcpiOsPrintf ("    [%4.4s]\n", Node->Name.Ascii);

        WalkState = WalkState->Next;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayObjectType
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display current calling tree of nested control methods
 *
 ******************************************************************************/

void
AcpiDbDisplayObjectType (
    char                    *ObjectArg)
{
    ACPI_HANDLE             Handle;
    ACPI_BUFFER             Buffer;
    ACPI_DEVICE_INFO        *Info;
    ACPI_STATUS             Status;
    ACPI_NATIVE_UINT        i;


    Handle = ACPI_TO_POINTER (ACPI_STRTOUL (ObjectArg, NULL, 16));
    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

    Status = AcpiGetObjectInfo (Handle, &Buffer);
    if (ACPI_SUCCESS (Status))
    {
        Info = Buffer.Pointer;
        AcpiOsPrintf ("HID: %s, ADR: %8.8X%8.8X, Status %8.8X\n",
                        &Info->HardwareId,
                        ACPI_HIDWORD (Info->Address), ACPI_LODWORD (Info->Address),
                        Info->CurrentStatus);

        if (Info->Valid & ACPI_VALID_CID)
        {
            for (i = 0; i < Info->CompatibilityId.Count; i++)
            {
                AcpiOsPrintf ("CID #%d: %s\n", i, &Info->CompatibilityId.Id[i]);
            }
        }

        ACPI_MEM_FREE (Info);
    }
    else
    {
        AcpiOsPrintf ("%s\n", AcpiFormatException (Status));
    }

}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayResultObject
 *
 * PARAMETERS:  ObjDesc         - Object to be displayed
 *              WalkState       - Current walk state
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display the result of an AML opcode
 *
 * Note: Curently only displays the result object if we are single stepping.
 * However, this output may be useful in other contexts and could be enabled
 * to do so if needed.
 *
 ******************************************************************************/

void
AcpiDbDisplayResultObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{

    /* Only display if single stepping */

    if (!AcpiGbl_CmSingleStep)
    {
        return;
    }

    AcpiOsPrintf ("ResultObj: ");
    AcpiDmDisplayInternalObject (ObjDesc, WalkState);
    AcpiOsPrintf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayArgumentObject
 *
 * PARAMETERS:  ObjDesc         - Object to be displayed
 *              WalkState       - Current walk state
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display the result of an AML opcode
 *
 ******************************************************************************/

void
AcpiDbDisplayArgumentObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{

    if (!AcpiGbl_CmSingleStep)
    {
        return;
    }

    AcpiOsPrintf ("ArgObj:    ");
    AcpiDmDisplayInternalObject (ObjDesc, WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayGpes
 *
 * PARAMETERS:
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display the GPE structures
 *
 ******************************************************************************/

void
AcpiDbDisplayGpes (void)
{
    ACPI_GPE_BLOCK_INFO     *GpeBlock;
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo;
    UINT32                  i = 0;


    GpeXruptInfo = AcpiGbl_GpeXruptListHead;
    while (GpeXruptInfo)
    {
        GpeBlock = GpeXruptInfo->GpeBlockListHead;
        while (GpeBlock)
        {
            AcpiOsPrintf ("Block %d - %p\n", i, GpeBlock);
            AcpiOsPrintf ("    Registers:    %d\n", GpeBlock->RegisterCount);
            AcpiOsPrintf ("    GPE range:    %d to %d\n", GpeBlock->BlockBaseNumber,
                            GpeBlock->BlockBaseNumber + (GpeBlock->RegisterCount * 8) -1);
            AcpiOsPrintf ("    RegisterInfo: %p\n", GpeBlock->RegisterInfo);
            AcpiOsPrintf ("    EventInfo:    %p\n", GpeBlock->EventInfo);
            i++;

            GpeBlock = GpeBlock->Next;
        }

        GpeXruptInfo = GpeXruptInfo->Next;
    }
}


#endif /* ACPI_DEBUGGER */

