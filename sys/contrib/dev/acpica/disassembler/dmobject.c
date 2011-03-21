/*******************************************************************************
 *
 * Module Name: dmobject - ACPI object decode and display
 *
 ******************************************************************************/

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

        AcpiOsPrintf ("(%u) \"%.24s",
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

        AcpiOsPrintf ("(%u)", ObjDesc->Buffer.Length);
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
        AcpiOsPrintf ("    Arg%u:   ", i);
        AcpiDmDisplayInternalObject (ObjDesc, WalkState);
    }
}

#endif


