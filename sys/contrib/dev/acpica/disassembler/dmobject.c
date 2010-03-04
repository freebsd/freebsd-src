/*******************************************************************************
 *
 * Module Name: dmobject - ACPI object decode and display
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


#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdisasm.h>


#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmnames")

/* Local prototypes */

static void
AcpiDmDecodeNode (
    ACPI_NAMESPACE_NODE     *Node);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpMethodInfo
 *
 * PARAMETERS:  Status          - Method execution status
 *              WalkState       - Current state of the parse tree walk
 *              Op              - Executing parse op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Called when a method has been aborted because of an error.
 *              Dumps the method execution stack, and the method locals/args,
 *              and disassembles the AML opcode that failed.
 *
 ******************************************************************************/

void
AcpiDmDumpMethodInfo (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Next;
    ACPI_THREAD_STATE       *Thread;
    ACPI_WALK_STATE         *NextWalkState;
    ACPI_NAMESPACE_NODE     *PreviousMethod = NULL;


    /* Ignore control codes, they are not errors */

    if ((Status & AE_CODE_MASK) == AE_CODE_CONTROL)
    {
        return;
    }

    /* We may be executing a deferred opcode */

    if (WalkState->DeferredNode)
    {
        AcpiOsPrintf ("Executing subtree for Buffer/Package/Region\n");
        return;
    }

    /*
     * If there is no Thread, we are not actually executing a method.
     * This can happen when the iASL compiler calls the interpreter
     * to perform constant folding.
     */
    Thread = WalkState->Thread;
    if (!Thread)
    {
        return;
    }

    /* Display exception and method name */

    AcpiOsPrintf ("\n**** Exception %s during execution of method ",
        AcpiFormatException (Status));
    AcpiNsPrintNodePathname (WalkState->MethodNode, NULL);

    /* Display stack of executing methods */

    AcpiOsPrintf ("\n\nMethod Execution Stack:\n");
    NextWalkState = Thread->WalkStateList;

    /* Walk list of linked walk states */

    while (NextWalkState)
    {
        AcpiOsPrintf ("    Method [%4.4s] executing: ",
                AcpiUtGetNodeName (NextWalkState->MethodNode));

        /* First method is the currently executing method */

        if (NextWalkState == WalkState)
        {
            if (Op)
            {
                /* Display currently executing ASL statement */

                Next = Op->Common.Next;
                Op->Common.Next = NULL;

                AcpiDmDisassemble (NextWalkState, Op, ACPI_UINT32_MAX);
                Op->Common.Next = Next;
            }
        }
        else
        {
            /*
             * This method has called another method
             * NOTE: the method call parse subtree is already deleted at this
             * point, so we cannot disassemble the method invocation.
             */
            AcpiOsPrintf ("Call to method ");
            AcpiNsPrintNodePathname (PreviousMethod, NULL);
        }

        PreviousMethod = NextWalkState->MethodNode;
        NextWalkState = NextWalkState->Next;
        AcpiOsPrintf ("\n");
    }

    /* Display the method locals and arguments */

    AcpiOsPrintf ("\n");
    AcpiDmDisplayLocals (WalkState);
    AcpiOsPrintf ("\n");
    AcpiDmDisplayArguments (WalkState);
    AcpiOsPrintf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDecodeInternalObject
 *
 * PARAMETERS:  ObjDesc         - Object to be displayed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Short display of an internal object.  Numbers/Strings/Buffers.
 *
 ******************************************************************************/

void
AcpiDmDecodeInternalObject (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    UINT32                  i;


    if (!ObjDesc)
    {
        AcpiOsPrintf (" Uninitialized");
        return;
    }

    if (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc) != ACPI_DESC_TYPE_OPERAND)
    {
        AcpiOsPrintf (" %p [%s]", ObjDesc, AcpiUtGetDescriptorName (ObjDesc));
        return;
    }

    AcpiOsPrintf (" %s", AcpiUtGetObjectTypeName (ObjDesc));

    switch (ObjDesc->Common.Type)
    {
    case ACPI_TYPE_INTEGER:

        AcpiOsPrintf (" %8.8X%8.8X",
                ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
        break;


    case ACPI_TYPE_STRING:

        AcpiOsPrintf ("(%d) \"%.24s",
                ObjDesc->String.Length, ObjDesc->String.Pointer);

        if (ObjDesc->String.Length > 24)
        {
            AcpiOsPrintf ("...");
        }
        else
        {
            AcpiOsPrintf ("\"");
        }
        break;


    case ACPI_TYPE_BUFFER:

        AcpiOsPrintf ("(%d)", ObjDesc->Buffer.Length);
        for (i = 0; (i < 8) && (i < ObjDesc->Buffer.Length); i++)
        {
            AcpiOsPrintf (" %2.2X", ObjDesc->Buffer.Pointer[i]);
        }
        break;


    default:

        AcpiOsPrintf (" %p", ObjDesc);
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDecodeNode
 *
 * PARAMETERS:  Node        - Object to be displayed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Short display of a namespace node
 *
 ******************************************************************************/

static void
AcpiDmDecodeNode (
    ACPI_NAMESPACE_NODE     *Node)
{

    AcpiOsPrintf ("<Node>            Name %4.4s",
            AcpiUtGetNodeName (Node));

    if (Node->Flags & ANOBJ_METHOD_ARG)
    {
        AcpiOsPrintf (" [Method Arg]");
    }
    if (Node->Flags & ANOBJ_METHOD_LOCAL)
    {
        AcpiOsPrintf (" [Method Local]");
    }

    switch (Node->Type)
    {
    /* These types have no attached object */

    case ACPI_TYPE_DEVICE:
        AcpiOsPrintf (" Device");
        break;

    case ACPI_TYPE_THERMAL:
        AcpiOsPrintf (" Thermal Zone");
        break;

    default:
        AcpiDmDecodeInternalObject (AcpiNsGetAttachedObject (Node));
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisplayInternalObject
 *
 * PARAMETERS:  ObjDesc         - Object to be displayed
 *              WalkState       - Current walk state
 *
 * RETURN:      None
 *
 * DESCRIPTION: Short display of an internal object
 *
 ******************************************************************************/

void
AcpiDmDisplayInternalObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    UINT8                   Type;


    AcpiOsPrintf ("%p ", ObjDesc);

    if (!ObjDesc)
    {
        AcpiOsPrintf ("<Null Object>\n");
        return;
    }

    /* Decode the object type */

    switch (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc))
    {
    case ACPI_DESC_TYPE_PARSER:

        AcpiOsPrintf ("<Parser>  ");
        break;


    case ACPI_DESC_TYPE_NAMED:

        AcpiDmDecodeNode ((ACPI_NAMESPACE_NODE *) ObjDesc);
        break;


    case ACPI_DESC_TYPE_OPERAND:

        Type = ObjDesc->Common.Type;
        if (Type > ACPI_TYPE_LOCAL_MAX)
        {
            AcpiOsPrintf (" Type %X [Invalid Type]", (UINT32) Type);
            return;
        }

        /* Decode the ACPI object type */

        switch (ObjDesc->Common.Type)
        {
        case ACPI_TYPE_LOCAL_REFERENCE:

            AcpiOsPrintf ("[%s] ", AcpiUtGetReferenceName (ObjDesc));

            /* Decode the refererence */

            switch (ObjDesc->Reference.Class)
            {
            case ACPI_REFCLASS_LOCAL:

                AcpiOsPrintf ("%X ", ObjDesc->Reference.Value);
                if (WalkState)
                {
                    ObjDesc = WalkState->LocalVariables
                                [ObjDesc->Reference.Value].Object;
                    AcpiOsPrintf ("%p", ObjDesc);
                    AcpiDmDecodeInternalObject (ObjDesc);
                }
                break;


            case ACPI_REFCLASS_ARG:

                AcpiOsPrintf ("%X ", ObjDesc->Reference.Value);
                if (WalkState)
                {
                    ObjDesc = WalkState->Arguments
                                [ObjDesc->Reference.Value].Object;
                    AcpiOsPrintf ("%p", ObjDesc);
                    AcpiDmDecodeInternalObject (ObjDesc);
                }
                break;


            case ACPI_REFCLASS_INDEX:

                switch (ObjDesc->Reference.TargetType)
                {
                case ACPI_TYPE_BUFFER_FIELD:

                    AcpiOsPrintf ("%p", ObjDesc->Reference.Object);
                    AcpiDmDecodeInternalObject (ObjDesc->Reference.Object);
                    break;

                case ACPI_TYPE_PACKAGE:

                    AcpiOsPrintf ("%p", ObjDesc->Reference.Where);
                    if (!ObjDesc->Reference.Where)
                    {
                        AcpiOsPrintf (" Uninitialized WHERE pointer");
                    }
                    else
                    {
                        AcpiDmDecodeInternalObject (
                            *(ObjDesc->Reference.Where));
                    }
                    break;

                default:

                    AcpiOsPrintf ("Unknown index target type");
                    break;
                }
                break;


            case ACPI_REFCLASS_REFOF:

                if (!ObjDesc->Reference.Object)
                {
                    AcpiOsPrintf ("Uninitialized reference subobject pointer");
                    break;
                }

                /* Reference can be to a Node or an Operand object */

                switch (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc->Reference.Object))
                {
                case ACPI_DESC_TYPE_NAMED:
                    AcpiDmDecodeNode (ObjDesc->Reference.Object);
                    break;

                case ACPI_DESC_TYPE_OPERAND:
                    AcpiDmDecodeInternalObject (ObjDesc->Reference.Object);
                    break;

                default:
                    break;
                }
                break;


            case ACPI_REFCLASS_NAME:

                AcpiDmDecodeNode (ObjDesc->Reference.Node);
                break;


            case ACPI_REFCLASS_DEBUG:
            case ACPI_REFCLASS_TABLE:

                AcpiOsPrintf ("\n");
                break;


            default:    /* Unknown reference class */

                AcpiOsPrintf ("%2.2X\n", ObjDesc->Reference.Class);
                break;
            }
            break;


        default:

            AcpiOsPrintf ("<Obj>            ");
            AcpiDmDecodeInternalObject (ObjDesc);
            break;
        }
        break;


    default:

        AcpiOsPrintf ("<Not a valid ACPI Object Descriptor> [%s]",
            AcpiUtGetDescriptorName (ObjDesc));
        break;
    }

    AcpiOsPrintf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisplayLocals
 *
 * PARAMETERS:  WalkState       - State for current method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all locals for the currently running control method
 *
 ******************************************************************************/

void
AcpiDmDisplayLocals (
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node;


    ObjDesc = WalkState->MethodDesc;
    Node    = WalkState->MethodNode;
    if (!Node)
    {
        AcpiOsPrintf (
            "No method node (Executing subtree for buffer or opregion)\n");
        return;
    }

    if (Node->Type != ACPI_TYPE_METHOD)
    {
        AcpiOsPrintf ("Executing subtree for Buffer/Package/Region\n");
        return;
    }

    AcpiOsPrintf ("Local Variables for method [%4.4s]:\n",
            AcpiUtGetNodeName (Node));

    for (i = 0; i < ACPI_METHOD_NUM_LOCALS; i++)
    {
        ObjDesc = WalkState->LocalVariables[i].Object;
        AcpiOsPrintf ("    Local%X: ", i);
        AcpiDmDisplayInternalObject (ObjDesc, WalkState);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisplayArguments
 *
 * PARAMETERS:  WalkState       - State for current method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all arguments for the currently running control method
 *
 ******************************************************************************/

void
AcpiDmDisplayArguments (
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node;


    ObjDesc = WalkState->MethodDesc;
    Node    = WalkState->MethodNode;
    if (!Node)
    {
        AcpiOsPrintf (
            "No method node (Executing subtree for buffer or opregion)\n");
        return;
    }

    if (Node->Type != ACPI_TYPE_METHOD)
    {
        AcpiOsPrintf ("Executing subtree for Buffer/Package/Region\n");
        return;
    }

    AcpiOsPrintf (
        "Arguments for Method [%4.4s]:  (%X arguments defined, max concurrency = %X)\n",
        AcpiUtGetNodeName (Node), ObjDesc->Method.ParamCount, ObjDesc->Method.SyncLevel);

    for (i = 0; i < ACPI_METHOD_NUM_ARGS; i++)
    {
        ObjDesc = WalkState->Arguments[i].Object;
        AcpiOsPrintf ("    Arg%d:   ", i);
        AcpiDmDisplayInternalObject (ObjDesc, WalkState);
    }
}

#endif


