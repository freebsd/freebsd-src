/******************************************************************************
 *
 * Module Name: aslwalks.c - Miscellaneous analytical parse tree walks
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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


/* Local prototypes */

static void
AnAnalyzeStoreOperator (
    ACPI_PARSE_OBJECT       *Op);


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
    UINT32                  ThisOpBtype;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        Op->Asl.CompileFlags |= NODE_METHOD_TYPED;
        break;

    case PARSEOP_RETURN:

        if ((Op->Asl.Child) &&
            (Op->Asl.Child->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG))
        {
            ThisOpBtype = AnGetBtype (Op->Asl.Child);

            if ((Op->Asl.Child->Asl.ParseOpcode == PARSEOP_METHODCALL) &&
                (ThisOpBtype == (ACPI_UINT32_MAX -1)))
            {
                /*
                 * The called method is untyped at this time (typically a
                 * forward reference).
                 *
                 * Check for a recursive method call first. Note: the
                 * Child->Node will be null if the method has not been
                 * resolved.
                 */
                if (Op->Asl.Child->Asl.Node &&
                    (Op->Asl.ParentMethod != Op->Asl.Child->Asl.Node->Op))
                {
                    /* We must type the method here */

                    TrWalkParseTree (Op->Asl.Child->Asl.Node->Op,
                        ASL_WALK_VISIT_UPWARD, NULL,
                        AnMethodTypingWalkEnd, NULL);

                    ThisOpBtype = AnGetBtype (Op->Asl.Child);
                }
            }

            /* Returns a value, save the value type */

            if (Op->Asl.ParentMethod)
            {
                Op->Asl.ParentMethod->Asl.AcpiBtype |= ThisOpBtype;
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

    ArgOp = Op->Asl.Child;
    OpcodeClass = OpInfo->Class;
    RuntimeArgTypes = OpInfo->RuntimeArgs;

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

    case AML_EXTERNAL_OP:
        /*
         * Not really a "runtime" opcode since it used by disassembler only.
         * The parser will find any issues with the operands.
         */
        return (AE_OK);

    default:

        break;
    }

    /* Ignore the non-executable opcodes */

    if (RuntimeArgTypes == ARGI_INVALID_OPCODE)
    {
        return (AE_OK);
    }

    /*
     * Special handling for certain opcodes.
     */
    switch (Op->Asl.AmlOpcode)
    {
        /* BankField has one TermArg */

    case AML_BANK_FIELD_OP:

        OpcodeClass = AML_CLASS_EXECUTE;
        ArgOp = ArgOp->Asl.Next;
        ArgOp = ArgOp->Asl.Next;
        break;

        /* Operation Region has 2 TermArgs */

    case AML_REGION_OP:

        OpcodeClass = AML_CLASS_EXECUTE;
        ArgOp = ArgOp->Asl.Next;
        ArgOp = ArgOp->Asl.Next;
        break;

        /* DataTableRegion has 3 TermArgs */

    case AML_DATA_REGION_OP:

        OpcodeClass = AML_CLASS_EXECUTE;
        ArgOp = ArgOp->Asl.Next;
        break;

        /* Buffers/Packages have a length that is a TermArg */

    case AML_BUFFER_OP:
    case AML_PACKAGE_OP:
    case AML_VAR_PACKAGE_OP:

            /* If length is a constant, we are done */

        if ((ArgOp->Asl.ParseOpcode == PARSEOP_INTEGER) ||
            (ArgOp->Asl.ParseOpcode == PARSEOP_RAW_DATA))
        {
            return (AE_OK);
        }
        break;

        /* Store can write any object to the Debug object */

    case AML_STORE_OP:
        /*
         * If this is a Store() to the Debug object, we don't need
         * to perform any further validation -- because a Store of
         * any object to Debug is permitted and supported.
         */
        if (ArgOp->Asl.Next->Asl.AmlOpcode == AML_DEBUG_OP)
        {
            return (AE_OK);
        }
        break;

    default:
        break;
    }

    switch (OpcodeClass)
    {
    case AML_CLASS_EXECUTE:
    case AML_CLASS_CREATE:
    case AML_CLASS_CONTROL:
    case AML_CLASS_RETURN_VALUE:

        /* Reverse the runtime argument list */

        RuntimeArgTypes2 = 0;
        while ((ArgType = GET_CURRENT_ARG_TYPE (RuntimeArgTypes)))
        {
            RuntimeArgTypes2 <<= ARG_TYPE_WIDTH;
            RuntimeArgTypes2 |= ArgType;
            INCREMENT_ARG_LIST (RuntimeArgTypes);
        }

        /* Typecheck each argument */

        while ((ArgType = GET_CURRENT_ARG_TYPE (RuntimeArgTypes2)))
        {
            /* Get the required type(s) for the argument */

            RequiredBtypes = AnMapArgTypeToBtype (ArgType);

            if (!ArgOp)
            {
                AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL, Op,
                    "Null ArgOp in argument loop");
                AslAbort ();
            }

            /* Get the actual type of the argument */

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

            /* Fallthrough */

            case ARGI_STORE_TARGET:

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
                        AslError (ASL_ERROR, ASL_MSG_RESOURCE_FIELD,
                            ArgOp, NULL);
                    }
                    else
                    {
                        AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE,
                            ArgOp, NULL);
                    }
                }
                break;


#ifdef __FUTURE_IMPLEMENTATION
/*
 * Possible future typechecking support
 */
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

                    /* Hard to analyze argument types, so we won't */
                    /* for now. Just treat any arg as a typematch */

                    /* ThisNodeBtype = RequiredBtypes; */
                    break;

                case PARSEOP_DEBUG:
                case PARSEOP_REFOF:
                case PARSEOP_INDEX:
                default:

                    break;
                }
                break;
#endif
            case ARGI_INTEGER:
            default:

                break;
            }


            /* Check for a type mismatch (required versus actual) */

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

                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE,
                    ArgOp, MsgBuffer);
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
    ACPI_PARSE_OBJECT       *ArgOp;
    ACPI_PARSE_OBJECT       *PrevArgOp = NULL;
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
             * Find the target node, it is always the last child. If the target
             * is not specified in the ASL, a default node of type Zero was
             * created by the parser.
             */
            ArgOp = Op->Asl.Child;
            while (ArgOp->Asl.Next)
            {
                PrevArgOp = ArgOp;
                ArgOp = ArgOp->Asl.Next;
            }

            /* Divide() is the only weird case, it has two targets */

            if (Op->Asl.AmlOpcode == AML_DIVIDE_OP)
            {
                if ((ArgOp->Asl.ParseOpcode == PARSEOP_ZERO) &&
                    (PrevArgOp) &&
                    (PrevArgOp->Asl.ParseOpcode == PARSEOP_ZERO))
                {
                    AslError (ASL_ERROR, ASL_MSG_RESULT_NOT_USED,
                        Op, Op->Asl.ExternalName);
                }
            }

            else if (ArgOp->Asl.ParseOpcode == PARSEOP_ZERO)
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
    case PARSEOP_STORE:

        if (Gbl_DoTypechecking)
        {
            AnAnalyzeStoreOperator (Op);
        }
        break;


    case PARSEOP_ACQUIRE:
    case PARSEOP_WAIT:
        /*
         * Emit a warning if the timeout parameter for these operators is not
         * ACPI_WAIT_FOREVER, and the result value from the operator is not
         * checked, meaning that a timeout could happen, but the code
         * would not know about it.
         */

        /* First child is the namepath, 2nd child is timeout */

        ArgOp = Op->Asl.Child;
        ArgOp = ArgOp->Asl.Next;

        /*
         * Check for the WAIT_FOREVER case - defined by the ACPI spec to be
         * 0xFFFF or greater
         */
        if (((ArgOp->Asl.ParseOpcode == PARSEOP_WORDCONST) ||
             (ArgOp->Asl.ParseOpcode == PARSEOP_INTEGER))  &&
             (ArgOp->Asl.Value.Integer >= (UINT64) ACPI_WAIT_FOREVER))
        {
            break;
        }

        /*
         * The operation could timeout. If the return value is not used
         * (indicates timeout occurred), issue a warning
         */
        if (!AnIsResultUsed (Op))
        {
            AslError (ASL_WARNING, ASL_MSG_TIMEOUT, ArgOp,
                Op->Asl.ExternalName);
        }
        break;

    case PARSEOP_CREATEFIELD:
        /*
         * Check for a zero Length (NumBits) operand. NumBits is the 3rd operand
         */
        ArgOp = Op->Asl.Child;
        ArgOp = ArgOp->Asl.Next;
        ArgOp = ArgOp->Asl.Next;

        if ((ArgOp->Asl.ParseOpcode == PARSEOP_ZERO) ||
           ((ArgOp->Asl.ParseOpcode == PARSEOP_INTEGER) &&
            (ArgOp->Asl.Value.Integer == 0)))
        {
            AslError (ASL_ERROR, ASL_MSG_NON_ZERO, ArgOp, NULL);
        }
        break;

    case PARSEOP_CONNECTION:
        /*
         * Ensure that the referenced operation region has the correct SPACE_ID.
         * From the grammar/parser, we know the parent is a FIELD definition.
         */
        ArgOp = Op->Asl.Parent;     /* Field definition */
        ArgOp = ArgOp->Asl.Child;   /* First child is the OpRegion Name */
        Node = ArgOp->Asl.Node;     /* OpRegion namespace node */
        if (!Node)
        {
            break;
        }

        ArgOp = Node->Op;           /* OpRegion definition */
        ArgOp = ArgOp->Asl.Child;   /* First child is the OpRegion Name */
        ArgOp = ArgOp->Asl.Next;    /* Next peer is the SPACE_ID (what we want) */

        /*
         * The Connection() operator is only valid for the following operation
         * region SpaceIds: GeneralPurposeIo and GenericSerialBus.
         */
        if ((ArgOp->Asl.Value.Integer != ACPI_ADR_SPACE_GPIO) &&
            (ArgOp->Asl.Value.Integer != ACPI_ADR_SPACE_GSBUS))
        {
            AslError (ASL_ERROR, ASL_MSG_CONNECTION_INVALID, Op, NULL);
        }
        break;

    case PARSEOP_FIELD:
        /*
         * Ensure that fields for GeneralPurposeIo and GenericSerialBus
         * contain at least one Connection() operator
         */
        ArgOp = Op->Asl.Child;      /* 1st child is the OpRegion Name */
        Node = ArgOp->Asl.Node;     /* OpRegion namespace node */
        if (!Node)
        {
            break;
        }

        ArgOp = Node->Op;           /* OpRegion definition */
        ArgOp = ArgOp->Asl.Child;   /* First child is the OpRegion Name */
        ArgOp = ArgOp->Asl.Next;    /* Next peer is the SPACE_ID (what we want) */

        /* We are only interested in GeneralPurposeIo and GenericSerialBus */

        if ((ArgOp->Asl.Value.Integer != ACPI_ADR_SPACE_GPIO) &&
            (ArgOp->Asl.Value.Integer != ACPI_ADR_SPACE_GSBUS))
        {
            break;
        }

        ArgOp = Op->Asl.Child;      /* 1st child is the OpRegion Name */
        ArgOp = ArgOp->Asl.Next;    /* AccessType */
        ArgOp = ArgOp->Asl.Next;    /* LockRule */
        ArgOp = ArgOp->Asl.Next;    /* UpdateRule */
        ArgOp = ArgOp->Asl.Next;    /* Start of FieldUnitList */

        /* Walk the FieldUnitList */

        while (ArgOp)
        {
            if (ArgOp->Asl.ParseOpcode == PARSEOP_CONNECTION)
            {
                break;
            }
            else if (ArgOp->Asl.ParseOpcode == PARSEOP_NAMESEG)
            {
                AslError (ASL_ERROR, ASL_MSG_CONNECTION_MISSING, ArgOp, NULL);
                break;
            }

            ArgOp = ArgOp->Asl.Next;
        }
        break;

    default:

        break;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AnAnalyzeStoreOperator
 *
 * PARAMETERS:  Op                  - Store() operator
 *
 * RETURN:      None
 *
 * DESCRIPTION: Analyze a store operator. Mostly for stores to/from package
 *              objects where there are more restrictions than other data
 *              types.
 *
 ******************************************************************************/

static void
AnAnalyzeStoreOperator (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_NAMESPACE_NODE     *SourceNode;
    ACPI_NAMESPACE_NODE     *TargetNode;
    ACPI_PARSE_OBJECT       *SourceOperandOp;
    ACPI_PARSE_OBJECT       *TargetOperandOp;
    UINT32                  SourceOperandBtype;
    UINT32                  TargetOperandBtype;


    /* Extract the two operands for STORE */

    SourceOperandOp = Op->Asl.Child;
    TargetOperandOp = SourceOperandOp->Asl.Next;

    /*
     * Ignore these Source operand opcodes, they cannot be typechecked,
     * the actual result is unknown here.
     */
    switch (SourceOperandOp->Asl.ParseOpcode)
    {
    /* For these, type of the returned value is unknown at compile time */

    case PARSEOP_DEREFOF:
    case PARSEOP_METHODCALL:
    case PARSEOP_STORE:
    case PARSEOP_COPYOBJECT:

        return;

    case PARSEOP_INDEX:
    case PARSEOP_REFOF:

        if (!Gbl_EnableReferenceTypechecking)
        {
            return;
        }

        /*
         * These opcodes always return an object reference, and thus
         * the result can only be stored to a Local, Arg, or Debug.
         */
        if (TargetOperandOp->Asl.AmlOpcode == AML_DEBUG_OP)
        {
            return;
        }

        if ((TargetOperandOp->Asl.AmlOpcode < AML_LOCAL0) ||
            (TargetOperandOp->Asl.AmlOpcode > AML_ARG6))
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, TargetOperandOp,
                "Source [Reference], Target must be [Local/Arg/Debug]");
        }
        return;

    default:
        break;
    }

    /*
     * Ignore these Target operand opcodes, they cannot be typechecked
     */
    switch (TargetOperandOp->Asl.ParseOpcode)
    {
    case PARSEOP_DEBUG:
    case PARSEOP_DEREFOF:
    case PARSEOP_REFOF:
    case PARSEOP_INDEX:
    case PARSEOP_STORE:

        return;

    default:
        break;
    }

    /*
     * Ignore typecheck for External() operands of type "UnknownObj",
     * we don't know the actual type (source or target).
     */
    SourceNode = SourceOperandOp->Asl.Node;
    if (SourceNode &&
        (SourceNode->Flags & ANOBJ_IS_EXTERNAL) &&
        (SourceNode->Type == ACPI_TYPE_ANY))
    {
        return;
    }

    TargetNode = TargetOperandOp->Asl.Node;
    if (TargetNode &&
        (TargetNode->Flags & ANOBJ_IS_EXTERNAL) &&
        (TargetNode->Type == ACPI_TYPE_ANY))
    {
        return;
    }

    /*
     * A NULL node with a namepath AML opcode indicates non-existent
     * name. Just return, the error message is generated elsewhere.
     */
    if ((!SourceNode && (SourceOperandOp->Asl.AmlOpcode == AML_INT_NAMEPATH_OP)) ||
        (!TargetNode && (TargetOperandOp->Asl.AmlOpcode == AML_INT_NAMEPATH_OP)))
    {
        return;
    }

    /*
     * Simple check for source same as target via NS node.
     * -- Could be expanded to locals and args.
     */
    if (SourceNode && TargetNode)
    {
        if (SourceNode == TargetNode)
        {
            AslError (ASL_WARNING, ASL_MSG_DUPLICATE_ITEM,
                TargetOperandOp, "Source is the same as Target");
            return;
        }
    }

    /* Ignore typecheck if either source or target is a local or arg */

    if ((SourceOperandOp->Asl.AmlOpcode >= AML_LOCAL0) &&
        (SourceOperandOp->Asl.AmlOpcode <= AML_ARG6))
    {
        return; /* Cannot type a local/arg at compile time */
    }

    if ((TargetOperandOp->Asl.AmlOpcode >= AML_LOCAL0) &&
        (TargetOperandOp->Asl.AmlOpcode <= AML_ARG6))
    {
        return; /* Cannot type a local/arg at compile time */
    }

    /*
     * Package objects are a special case because they cannot by implicitly
     * converted to/from anything. Check for these two illegal cases:
     *
     *      Store (non-package, package)
     *      Store (package, non-package)
     */
    SourceOperandBtype = AnGetBtype (SourceOperandOp);
    TargetOperandBtype = AnGetBtype (TargetOperandOp);

    /* Check source first for (package, non-package) case */

    if (SourceOperandBtype & ACPI_BTYPE_PACKAGE)
    {
        /* If Source is PACKAGE-->Target must be PACKAGE */

        if (!(TargetOperandBtype & ACPI_BTYPE_PACKAGE))
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, TargetOperandOp,
                "Source is [Package], Target must be a package also");
        }
    }

    /* Else check target for (non-package, package) case */

    else if (TargetOperandBtype & ACPI_BTYPE_PACKAGE)
    {
        /* If Target is PACKAGE, Source must be PACKAGE */

        if (!(SourceOperandBtype & ACPI_BTYPE_PACKAGE))
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, SourceOperandOp,
                "Target is [Package], Source must be a package also");
        }
    }
}
