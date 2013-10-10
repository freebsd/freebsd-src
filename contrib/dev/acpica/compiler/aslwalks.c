/******************************************************************************
 *
 * Module Name: aslwalks.c - Miscellaneous analytical parse tree walks
 *
 *****************************************************************************/

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


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslwalks")


/*******************************************************************************
 *
 * FUNCTION:    AnMethodTypingWalkEnd
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback for typing walk. Complete the method
 *              return analysis. Check methods for:
 *              1) Initialized local variables
 *              2) Valid arguments
 *              3) Return types
 *
 ******************************************************************************/

ACPI_STATUS
AnMethodTypingWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    UINT32                  ThisNodeBtype;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        Op->Asl.CompileFlags |= NODE_METHOD_TYPED;
        break;

    case PARSEOP_RETURN:

        if ((Op->Asl.Child) &&
            (Op->Asl.Child->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG))
        {
            ThisNodeBtype = AnGetBtype (Op->Asl.Child);

            if ((Op->Asl.Child->Asl.ParseOpcode == PARSEOP_METHODCALL) &&
                (ThisNodeBtype == (ACPI_UINT32_MAX -1)))
            {
                /*
                 * The called method is untyped at this time (typically a
                 * forward reference).
                 *
                 * Check for a recursive method call first.
                 */
                if (Op->Asl.ParentMethod != Op->Asl.Child->Asl.Node->Op)
                {
                    /* We must type the method here */

                    TrWalkParseTree (Op->Asl.Child->Asl.Node->Op,
                        ASL_WALK_VISIT_UPWARD, NULL,
                        AnMethodTypingWalkEnd, NULL);

                    ThisNodeBtype = AnGetBtype (Op->Asl.Child);
                }
            }

            /* Returns a value, save the value type */

            if (Op->Asl.ParentMethod)
            {
                Op->Asl.ParentMethod->Asl.AcpiBtype |= ThisNodeBtype;
            }
        }
        break;

    default:

        break;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AnOperandTypecheckWalkEnd
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback for analysis walk. Complete method
 *              return analysis.
 *
 ******************************************************************************/

ACPI_STATUS
AnOperandTypecheckWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  RuntimeArgTypes;
    UINT32                  RuntimeArgTypes2;
    UINT32                  RequiredBtypes;
    UINT32                  ThisNodeBtype;
    UINT32                  CommonBtypes;
    UINT32                  OpcodeClass;
    ACPI_PARSE_OBJECT       *ArgOp;
    UINT32                  ArgType;


    switch (Op->Asl.AmlOpcode)
    {
    case AML_RAW_DATA_BYTE:
    case AML_RAW_DATA_WORD:
    case AML_RAW_DATA_DWORD:
    case AML_RAW_DATA_QWORD:
    case AML_RAW_DATA_BUFFER:
    case AML_RAW_DATA_CHAIN:
    case AML_PACKAGE_LENGTH:
    case AML_UNASSIGNED_OPCODE:
    case AML_DEFAULT_ARG_OP:

        /* Ignore the internal (compiler-only) AML opcodes */

        return (AE_OK);

    default:

        break;
    }

    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);
    if (!OpInfo)
    {
        return (AE_OK);
    }

    ArgOp           = Op->Asl.Child;
    RuntimeArgTypes = OpInfo->RuntimeArgs;
    OpcodeClass     = OpInfo->Class;

#ifdef ASL_ERROR_NAMED_OBJECT_IN_WHILE
    /*
     * Update 11/2008: In practice, we can't perform this check. A simple
     * analysis is not sufficient. Also, it can cause errors when compiling
     * disassembled code because of the way Switch operators are implemented
     * (a While(One) loop with a named temp variable created within.)
     */

    /*
     * If we are creating a named object, check if we are within a while loop
     * by checking if the parent is a WHILE op. This is a simple analysis, but
     * probably sufficient for many cases.
     *
     * Allow Scope(), Buffer(), and Package().
     */
    if (((OpcodeClass == AML_CLASS_NAMED_OBJECT) && (Op->Asl.AmlOpcode != AML_SCOPE_OP)) ||
        ((OpcodeClass == AML_CLASS_CREATE) && (OpInfo->Flags & AML_NSNODE)))
    {
        if (Op->Asl.Parent->Asl.AmlOpcode == AML_WHILE_OP)
        {
            AslError (ASL_ERROR, ASL_MSG_NAMED_OBJECT_IN_WHILE, Op, NULL);
        }
    }
#endif

    /*
     * Special case for control opcodes IF/RETURN/WHILE since they
     * have no runtime arg list (at this time)
     */
    switch (Op->Asl.AmlOpcode)
    {
    case AML_IF_OP:
    case AML_WHILE_OP:
    case AML_RETURN_OP:

        if (ArgOp->Asl.ParseOpcode == PARSEOP_METHODCALL)
        {
            /* Check for an internal method */

            if (AnIsInternalMethod (ArgOp))
            {
                return (AE_OK);
            }

            /* The lone arg is a method call, check it */

            RequiredBtypes = AnMapArgTypeToBtype (ARGI_INTEGER);
            if (Op->Asl.AmlOpcode == AML_RETURN_OP)
            {
                RequiredBtypes = 0xFFFFFFFF;
            }

            ThisNodeBtype = AnGetBtype (ArgOp);
            if (ThisNodeBtype == ACPI_UINT32_MAX)
            {
                return (AE_OK);
            }
            AnCheckMethodReturnValue (Op, OpInfo, ArgOp,
                RequiredBtypes, ThisNodeBtype);
        }
        return (AE_OK);

    default:

        break;
    }

    /* Ignore the non-executable opcodes */

    if (RuntimeArgTypes == ARGI_INVALID_OPCODE)
    {
        return (AE_OK);
    }

    switch (OpcodeClass)
    {
    case AML_CLASS_EXECUTE:
    case AML_CLASS_CREATE:
    case AML_CLASS_CONTROL:
    case AML_CLASS_RETURN_VALUE:

        /* TBD: Change class or fix typechecking for these */

        if ((Op->Asl.AmlOpcode == AML_BUFFER_OP)        ||
            (Op->Asl.AmlOpcode == AML_PACKAGE_OP)       ||
            (Op->Asl.AmlOpcode == AML_VAR_PACKAGE_OP))
        {
            break;
        }

        /* Reverse the runtime argument list */

        RuntimeArgTypes2 = 0;
        while ((ArgType = GET_CURRENT_ARG_TYPE (RuntimeArgTypes)))
        {
            RuntimeArgTypes2 <<= ARG_TYPE_WIDTH;
            RuntimeArgTypes2 |= ArgType;
            INCREMENT_ARG_LIST (RuntimeArgTypes);
        }

        while ((ArgType = GET_CURRENT_ARG_TYPE (RuntimeArgTypes2)))
        {
            RequiredBtypes = AnMapArgTypeToBtype (ArgType);

            ThisNodeBtype = AnGetBtype (ArgOp);
            if (ThisNodeBtype == ACPI_UINT32_MAX)
            {
                goto NextArgument;
            }

            /* Examine the arg based on the required type of the arg */

            switch (ArgType)
            {
            case ARGI_TARGETREF:

                if (ArgOp->Asl.ParseOpcode == PARSEOP_ZERO)
                {
                    /* ZERO is the placeholder for "don't store result" */

                    ThisNodeBtype = RequiredBtypes;
                    break;
                }

                if (ArgOp->Asl.ParseOpcode == PARSEOP_INTEGER)
                {
                    /*
                     * This is the case where an original reference to a resource
                     * descriptor field has been replaced by an (Integer) offset.
                     * These named fields are supported at compile-time only;
                     * the names are not passed to the interpreter (via the AML).
                     */
                    if ((ArgOp->Asl.Node->Type == ACPI_TYPE_LOCAL_RESOURCE_FIELD) ||
                        (ArgOp->Asl.Node->Type == ACPI_TYPE_LOCAL_RESOURCE))
                    {
                        AslError (ASL_ERROR, ASL_MSG_RESOURCE_FIELD, ArgOp, NULL);
                    }
                    else
                    {
                        AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ArgOp, NULL);
                    }
                    break;
                }

                if ((ArgOp->Asl.ParseOpcode == PARSEOP_METHODCALL) ||
                    (ArgOp->Asl.ParseOpcode == PARSEOP_DEREFOF))
                {
                    break;
                }

                ThisNodeBtype = RequiredBtypes;
                break;


            case ARGI_REFERENCE:            /* References */
            case ARGI_INTEGER_REF:
            case ARGI_OBJECT_REF:
            case ARGI_DEVICE_REF:

                switch (ArgOp->Asl.ParseOpcode)
                {
                case PARSEOP_LOCAL0:
                case PARSEOP_LOCAL1:
                case PARSEOP_LOCAL2:
                case PARSEOP_LOCAL3:
                case PARSEOP_LOCAL4:
                case PARSEOP_LOCAL5:
                case PARSEOP_LOCAL6:
                case PARSEOP_LOCAL7:

                    /* TBD: implement analysis of current value (type) of the local */
                    /* For now, just treat any local as a typematch */

                    /*ThisNodeBtype = RequiredBtypes;*/
                    break;

                case PARSEOP_ARG0:
                case PARSEOP_ARG1:
                case PARSEOP_ARG2:
                case PARSEOP_ARG3:
                case PARSEOP_ARG4:
                case PARSEOP_ARG5:
                case PARSEOP_ARG6:

                    /* Hard to analyze argument types, sow we won't */
                    /* For now, just treat any arg as a typematch */

                    /* ThisNodeBtype = RequiredBtypes; */
                    break;

                case PARSEOP_DEBUG:

                    break;

                case PARSEOP_REFOF:
                case PARSEOP_INDEX:
                default:

                    break;

                }
                break;

            case ARGI_INTEGER:
            default:

                break;
            }


            CommonBtypes = ThisNodeBtype & RequiredBtypes;

            if (ArgOp->Asl.ParseOpcode == PARSEOP_METHODCALL)
            {
                if (AnIsInternalMethod (ArgOp))
                {
                    return (AE_OK);
                }

                /* Check a method call for a valid return value */

                AnCheckMethodReturnValue (Op, OpInfo, ArgOp,
                    RequiredBtypes, ThisNodeBtype);
            }

            /*
             * Now check if the actual type(s) match at least one
             * bit to the required type
             */
            else if (!CommonBtypes)
            {
                /* No match -- this is a type mismatch error */

                AnFormatBtype (StringBuffer, ThisNodeBtype);
                AnFormatBtype (StringBuffer2, RequiredBtypes);

                sprintf (MsgBuffer, "[%s] found, %s operator requires [%s]",
                            StringBuffer, OpInfo->Name, StringBuffer2);

                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ArgOp, MsgBuffer);
            }

        NextArgument:
            ArgOp = ArgOp->Asl.Next;
            INCREMENT_ARG_LIST (RuntimeArgTypes2);
        }
        break;

    default:

        break;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AnOtherSemanticAnalysisWalkBegin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback for the analysis walk. Checks for
 *              miscellaneous issues in the code.
 *
 ******************************************************************************/

ACPI_STATUS
AnOtherSemanticAnalysisWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_PARSE_OBJECT       *ArgNode;
    ACPI_PARSE_OBJECT       *PrevArgNode = NULL;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_NAMESPACE_NODE     *Node;


    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);

    /*
     * Determine if an execution class operator actually does something by
     * checking if it has a target and/or the function return value is used.
     * (Target is optional, so a standalone statement can actually do nothing.)
     */
    if ((OpInfo->Class == AML_CLASS_EXECUTE) &&
        (OpInfo->Flags & AML_HAS_RETVAL) &&
        (!AnIsResultUsed (Op)))
    {
        if (OpInfo->Flags & AML_HAS_TARGET)
        {
            /*
             * Find the target node, it is always the last child. If the traget
             * is not specified in the ASL, a default node of type Zero was
             * created by the parser.
             */
            ArgNode = Op->Asl.Child;
            while (ArgNode->Asl.Next)
            {
                PrevArgNode = ArgNode;
                ArgNode = ArgNode->Asl.Next;
            }

            /* Divide() is the only weird case, it has two targets */

            if (Op->Asl.AmlOpcode == AML_DIVIDE_OP)
            {
                if ((ArgNode->Asl.ParseOpcode == PARSEOP_ZERO) &&
                    (PrevArgNode) &&
                    (PrevArgNode->Asl.ParseOpcode == PARSEOP_ZERO))
                {
                    AslError (ASL_ERROR, ASL_MSG_RESULT_NOT_USED,
                        Op, Op->Asl.ExternalName);
                }
            }
            else if (ArgNode->Asl.ParseOpcode == PARSEOP_ZERO)
            {
                AslError (ASL_ERROR, ASL_MSG_RESULT_NOT_USED,
                    Op, Op->Asl.ExternalName);
            }
        }
        else
        {
            /*
             * Has no target and the result is not used. Only a couple opcodes
             * can have this combination.
             */
            switch (Op->Asl.ParseOpcode)
            {
            case PARSEOP_ACQUIRE:
            case PARSEOP_WAIT:
            case PARSEOP_LOADTABLE:

                break;

            default:

                AslError (ASL_ERROR, ASL_MSG_RESULT_NOT_USED,
                    Op, Op->Asl.ExternalName);
                break;
            }
        }
    }


    /*
     * Semantic checks for individual ASL operators
     */
    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_ACQUIRE:
    case PARSEOP_WAIT:
        /*
         * Emit a warning if the timeout parameter for these operators is not
         * ACPI_WAIT_FOREVER, and the result value from the operator is not
         * checked, meaning that a timeout could happen, but the code
         * would not know about it.
         */

        /* First child is the namepath, 2nd child is timeout */

        ArgNode = Op->Asl.Child;
        ArgNode = ArgNode->Asl.Next;

        /*
         * Check for the WAIT_FOREVER case - defined by the ACPI spec to be
         * 0xFFFF or greater
         */
        if (((ArgNode->Asl.ParseOpcode == PARSEOP_WORDCONST) ||
             (ArgNode->Asl.ParseOpcode == PARSEOP_INTEGER))  &&
             (ArgNode->Asl.Value.Integer >= (UINT64) ACPI_WAIT_FOREVER))
        {
            break;
        }

        /*
         * The operation could timeout. If the return value is not used
         * (indicates timeout occurred), issue a warning
         */
        if (!AnIsResultUsed (Op))
        {
            AslError (ASL_WARNING, ASL_MSG_TIMEOUT, ArgNode,
                Op->Asl.ExternalName);
        }
        break;

    case PARSEOP_CREATEFIELD:
        /*
         * Check for a zero Length (NumBits) operand. NumBits is the 3rd operand
         */
        ArgNode = Op->Asl.Child;
        ArgNode = ArgNode->Asl.Next;
        ArgNode = ArgNode->Asl.Next;

        if ((ArgNode->Asl.ParseOpcode == PARSEOP_ZERO) ||
           ((ArgNode->Asl.ParseOpcode == PARSEOP_INTEGER) &&
            (ArgNode->Asl.Value.Integer == 0)))
        {
            AslError (ASL_ERROR, ASL_MSG_NON_ZERO, ArgNode, NULL);
        }
        break;

    case PARSEOP_CONNECTION:
        /*
         * Ensure that the referenced operation region has the correct SPACE_ID.
         * From the grammar/parser, we know the parent is a FIELD definition.
         */
        ArgNode = Op->Asl.Parent;       /* Field definition */
        ArgNode = ArgNode->Asl.Child;   /* First child is the OpRegion Name */
        Node = ArgNode->Asl.Node;       /* OpRegion namespace node */

        ArgNode = Node->Op;             /* OpRegion definition */
        ArgNode = ArgNode->Asl.Child;   /* First child is the OpRegion Name */
        ArgNode = ArgNode->Asl.Next;    /* Next peer is the SPACE_ID (what we want) */

        /*
         * The Connection() operator is only valid for the following operation
         * region SpaceIds: GeneralPurposeIo and GenericSerialBus.
         */
        if ((ArgNode->Asl.Value.Integer != ACPI_ADR_SPACE_GPIO) &&
            (ArgNode->Asl.Value.Integer != ACPI_ADR_SPACE_GSBUS))
        {
            AslError (ASL_ERROR, ASL_MSG_CONNECTION_INVALID, Op, NULL);
        }
        break;

    case PARSEOP_FIELD:
        /*
         * Ensure that fields for GeneralPurposeIo and GenericSerialBus
         * contain at least one Connection() operator
         */
        ArgNode = Op->Asl.Child;        /* 1st child is the OpRegion Name */
        Node = ArgNode->Asl.Node;       /* OpRegion namespace node */
        if (!Node)
        {
            break;
        }

        ArgNode = Node->Op;             /* OpRegion definition */
        ArgNode = ArgNode->Asl.Child;   /* First child is the OpRegion Name */
        ArgNode = ArgNode->Asl.Next;    /* Next peer is the SPACE_ID (what we want) */

        /* We are only interested in GeneralPurposeIo and GenericSerialBus */

        if ((ArgNode->Asl.Value.Integer != ACPI_ADR_SPACE_GPIO) &&
            (ArgNode->Asl.Value.Integer != ACPI_ADR_SPACE_GSBUS))
        {
            break;
        }

        ArgNode = Op->Asl.Child;        /* 1st child is the OpRegion Name */
        ArgNode = ArgNode->Asl.Next;    /* AccessType */
        ArgNode = ArgNode->Asl.Next;    /* LockRule */
        ArgNode = ArgNode->Asl.Next;    /* UpdateRule */
        ArgNode = ArgNode->Asl.Next;    /* Start of FieldUnitList */

        /* Walk the FieldUnitList */

        while (ArgNode)
        {
            if (ArgNode->Asl.ParseOpcode == PARSEOP_CONNECTION)
            {
                break;
            }
            else if (ArgNode->Asl.ParseOpcode == PARSEOP_NAMESEG)
            {
                AslError (ASL_ERROR, ASL_MSG_CONNECTION_MISSING, ArgNode, NULL);
                break;
            }

            ArgNode = ArgNode->Asl.Next;
        }
        break;

    default:

        break;
    }

    return (AE_OK);
}
