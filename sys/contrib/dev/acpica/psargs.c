/******************************************************************************
 *
 * Module Name: psargs - Parse AML opcode arguments
 *              $Revision: 1.81 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
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

#define __PSARGS_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/amlcode.h>
#include <contrib/dev/acpica/acnamesp.h>

#define _COMPONENT          ACPI_PARSER
        ACPI_MODULE_NAME    ("psargs")

/* Local prototypes */

static UINT32
AcpiPsGetNextPackageLength (
    ACPI_PARSE_STATE        *ParserState);

static ACPI_PARSE_OBJECT *
AcpiPsGetNextField (
    ACPI_PARSE_STATE        *ParserState);


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextPackageLength
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Decoded package length.  On completion, the AML pointer points
 *              past the length byte or bytes.
 *
 * DESCRIPTION: Decode and return a package length field
 *
 ******************************************************************************/

static UINT32
AcpiPsGetNextPackageLength (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT32                  EncodedLength;
    UINT32                  Length = 0;


    ACPI_FUNCTION_TRACE ("PsGetNextPackageLength");


    EncodedLength = (UINT32) ACPI_GET8 (ParserState->Aml);
    ParserState->Aml++;

    switch (EncodedLength >> 6) /* bits 6-7 contain encoding scheme */
    {
    case 0: /* 1-byte encoding (bits 0-5) */

        Length = (EncodedLength & 0x3F);
        break;


    case 1: /* 2-byte encoding (next byte + bits 0-3) */

        Length = ((ACPI_GET8 (ParserState->Aml) << 04) |
                 (EncodedLength & 0x0F));
        ParserState->Aml++;
        break;


    case 2: /* 3-byte encoding (next 2 bytes + bits 0-3) */

        Length = ((ACPI_GET8 (ParserState->Aml + 1) << 12) |
                  (ACPI_GET8 (ParserState->Aml)     << 04) |
                  (EncodedLength & 0x0F));
        ParserState->Aml += 2;
        break;


    case 3: /* 4-byte encoding (next 3 bytes + bits 0-3) */

        Length = ((ACPI_GET8 (ParserState->Aml + 2) << 20) |
                  (ACPI_GET8 (ParserState->Aml + 1) << 12) |
                  (ACPI_GET8 (ParserState->Aml)     << 04) |
                  (EncodedLength & 0x0F));
        ParserState->Aml += 3;
        break;

    default:

        /* Can't get here, only 2 bits / 4 cases */
        break;
    }

    return_UINT32 (Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextPackageEnd
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Pointer to end-of-package +1
 *
 * DESCRIPTION: Get next package length and return a pointer past the end of
 *              the package.  Consumes the package length field
 *
 ******************************************************************************/

UINT8 *
AcpiPsGetNextPackageEnd (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT8                   *Start = ParserState->Aml;
    ACPI_NATIVE_UINT        Length;


    ACPI_FUNCTION_TRACE ("PsGetNextPackageEnd");


    /* Function below changes ParserState->Aml */

    Length = (ACPI_NATIVE_UINT) AcpiPsGetNextPackageLength (ParserState);

    return_PTR (Start + Length); /* end of package */
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextNamestring
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Pointer to the start of the name string (pointer points into
 *              the AML.
 *
 * DESCRIPTION: Get next raw namestring within the AML stream.  Handles all name
 *              prefix characters.  Set parser state to point past the string.
 *              (Name is consumed from the AML.)
 *
 ******************************************************************************/

char *
AcpiPsGetNextNamestring (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT8                   *Start = ParserState->Aml;
    UINT8                   *End = ParserState->Aml;


    ACPI_FUNCTION_TRACE ("PsGetNextNamestring");


    /* Handle multiple prefix characters */

    while (AcpiPsIsPrefixChar (ACPI_GET8 (End)))
    {
        /* Include prefix '\\' or '^' */

        End++;
    }

    /* Decode the path */

    switch (ACPI_GET8 (End))
    {
    case 0:

        /* NullName */

        if (End == Start)
        {
            Start = NULL;
        }
        End++;
        break;

    case AML_DUAL_NAME_PREFIX:

        /* Two name segments */

        End += 1 + (2 * ACPI_NAME_SIZE);
        break;

    case AML_MULTI_NAME_PREFIX_OP:

        /* Multiple name segments, 4 chars each */

        End += 2 + ((ACPI_SIZE) ACPI_GET8 (End + 1) * ACPI_NAME_SIZE);
        break;

    default:

        /* Single name segment */

        End += ACPI_NAME_SIZE;
        break;
    }

    ParserState->Aml = (UINT8*) End;
    return_PTR ((char *) Start);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextNamepath
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *              Arg                 - Where the namepath will be stored
 *              ArgCount            - If the namepath points to a control method
 *                                    the method's argument is returned here.
 *              MethodCall          - Whether the namepath can possibly be the
 *                                    start of a method call
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get next name (if method call, return # of required args).
 *              Names are looked up in the internal namespace to determine
 *              if the name represents a control method.  If a method
 *              is found, the number of arguments to the method is returned.
 *              This information is critical for parsing to continue correctly.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsGetNextNamepath (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_STATE        *ParserState,
    ACPI_PARSE_OBJECT       *Arg,
    BOOLEAN                 MethodCall)
{
    char                    *Path;
    ACPI_PARSE_OBJECT       *NameOp;
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *MethodDesc;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_GENERIC_STATE      ScopeInfo;


    ACPI_FUNCTION_TRACE ("PsGetNextNamepath");


    Path = AcpiPsGetNextNamestring (ParserState);

    /* Null path case is allowed */

    if (Path)
    {
        /*
         * Lookup the name in the internal namespace
         */
        ScopeInfo.Scope.Node = NULL;
        Node = ParserState->StartNode;
        if (Node)
        {
            ScopeInfo.Scope.Node = Node;
        }

        /*
         * Lookup object.  We don't want to add anything new to the namespace
         * here, however.  So we use MODE_EXECUTE.  Allow searching of the
         * parent tree, but don't open a new scope -- we just want to lookup the
         * object  (MUST BE mode EXECUTE to perform upsearch)
         */
        Status = AcpiNsLookup (&ScopeInfo, Path, ACPI_TYPE_ANY,
                    ACPI_IMODE_EXECUTE,
                    ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE,
                    NULL, &Node);
        if (ACPI_SUCCESS (Status) && MethodCall)
        {
            if (Node->Type == ACPI_TYPE_METHOD)
            {
                /* This name is actually a control method invocation */

                MethodDesc = AcpiNsGetAttachedObject (Node);
                ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
                    "Control Method - %p Desc %p Path=%p\n",
                    Node, MethodDesc, Path));

                NameOp = AcpiPsAllocOp (AML_INT_NAMEPATH_OP);
                if (!NameOp)
                {
                    return_ACPI_STATUS (AE_NO_MEMORY);
                }

                /* Change arg into a METHOD CALL and attach name to it */

                AcpiPsInitOp (Arg, AML_INT_METHODCALL_OP);
                NameOp->Common.Value.Name = Path;

                /* Point METHODCALL/NAME to the METHOD Node */

                NameOp->Common.Node = Node;
                AcpiPsAppendArg (Arg, NameOp);

                if (!MethodDesc)
                {
                    ACPI_REPORT_ERROR ((
                        "PsGetNextNamepath: Control Method %p has no attached object\n",
                        Node));
                    return_ACPI_STATUS (AE_AML_INTERNAL);
                }

                ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
                    "Control Method - %p Args %X\n",
                    Node, MethodDesc->Method.ParamCount));

                /* Get the number of arguments to expect */

                WalkState->ArgCount = MethodDesc->Method.ParamCount;
                return_ACPI_STATUS (AE_OK);
            }

            /*
             * Else this is normal named object reference.
             * Just init the NAMEPATH object with the pathname.
             * (See code below)
             */
        }

        if (ACPI_FAILURE (Status))
        {
            /*
             * 1) Any error other than NOT_FOUND is always severe
             * 2) NOT_FOUND is only important if we are executing a method.
             * 3) If executing a CondRefOf opcode, NOT_FOUND is ok.
             */
            if ((((WalkState->ParseFlags & ACPI_PARSE_MODE_MASK) == ACPI_PARSE_EXECUTE) &&
                (Status == AE_NOT_FOUND)                                                &&
                (WalkState->Op->Common.AmlOpcode != AML_COND_REF_OF_OP)) ||

                (Status != AE_NOT_FOUND))
            {
                ACPI_REPORT_NSERROR (Path, Status);

                AcpiOsPrintf ("SearchNode %p StartNode %p ReturnNode %p\n",
                    ScopeInfo.Scope.Node, ParserState->StartNode, Node);


            }
            else
            {
                /*
                 * We got a NOT_FOUND during table load or we encountered
                 * a CondRefOf(x) where the target does not exist.
                 * Either case is ok
                 */
                Status = AE_OK;
            }
        }
    }

    /*
     * Regardless of success/failure above,
     * Just initialize the Op with the pathname.
     */
    AcpiPsInitOp (Arg, AML_INT_NAMEPATH_OP);
    Arg->Common.Value.Name = Path;

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextSimpleArg
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *              ArgType             - The argument type (AML_*_ARG)
 *              Arg                 - Where the argument is returned
 *
 * RETURN:      None
 *
 * DESCRIPTION: Get the next simple argument (constant, string, or namestring)
 *
 ******************************************************************************/

void
AcpiPsGetNextSimpleArg (
    ACPI_PARSE_STATE        *ParserState,
    UINT32                  ArgType,
    ACPI_PARSE_OBJECT       *Arg)
{

    ACPI_FUNCTION_TRACE_U32 ("PsGetNextSimpleArg", ArgType);


    switch (ArgType)
    {
    case ARGP_BYTEDATA:

        AcpiPsInitOp (Arg, AML_BYTE_OP);
        Arg->Common.Value.Integer = (UINT32) ACPI_GET8 (ParserState->Aml);
        ParserState->Aml++;
        break;


    case ARGP_WORDDATA:

        AcpiPsInitOp (Arg, AML_WORD_OP);

        /* Get 2 bytes from the AML stream */

        ACPI_MOVE_16_TO_32 (&Arg->Common.Value.Integer, ParserState->Aml);
        ParserState->Aml += 2;
        break;


    case ARGP_DWORDDATA:

        AcpiPsInitOp (Arg, AML_DWORD_OP);

        /* Get 4 bytes from the AML stream */

        ACPI_MOVE_32_TO_32 (&Arg->Common.Value.Integer, ParserState->Aml);
        ParserState->Aml += 4;
        break;


    case ARGP_QWORDDATA:

        AcpiPsInitOp (Arg, AML_QWORD_OP);

        /* Get 8 bytes from the AML stream */

        ACPI_MOVE_64_TO_64 (&Arg->Common.Value.Integer, ParserState->Aml);
        ParserState->Aml += 8;
        break;


    case ARGP_CHARLIST:

        AcpiPsInitOp (Arg, AML_STRING_OP);
        Arg->Common.Value.String = (char *) ParserState->Aml;

        while (ACPI_GET8 (ParserState->Aml) != '\0')
        {
            ParserState->Aml++;
        }
        ParserState->Aml++;
        break;


    case ARGP_NAME:
    case ARGP_NAMESTRING:

        AcpiPsInitOp (Arg, AML_INT_NAMEPATH_OP);
        Arg->Common.Value.Name = AcpiPsGetNextNamestring (ParserState);
        break;


    default:

        ACPI_REPORT_ERROR (("Invalid ArgType %X\n", ArgType));
        break;
    }

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextField
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      A newly allocated FIELD op
 *
 * DESCRIPTION: Get next field (NamedField, ReservedField, or AccessField)
 *
 ******************************************************************************/

static ACPI_PARSE_OBJECT *
AcpiPsGetNextField (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT32                  AmlOffset = (UINT32)
                                ACPI_PTR_DIFF (ParserState->Aml,
                                               ParserState->AmlStart);
    ACPI_PARSE_OBJECT       *Field;
    UINT16                  Opcode;
    UINT32                  Name;


    ACPI_FUNCTION_TRACE ("PsGetNextField");


    /* Determine field type */

    switch (ACPI_GET8 (ParserState->Aml))
    {
    default:

        Opcode = AML_INT_NAMEDFIELD_OP;
        break;

    case 0x00:

        Opcode = AML_INT_RESERVEDFIELD_OP;
        ParserState->Aml++;
        break;

    case 0x01:

        Opcode = AML_INT_ACCESSFIELD_OP;
        ParserState->Aml++;
        break;
    }

    /* Allocate a new field op */

    Field = AcpiPsAllocOp (Opcode);
    if (!Field)
    {
        return_PTR (NULL);
    }

    Field->Common.AmlOffset = AmlOffset;

    /* Decode the field type */

    switch (Opcode)
    {
    case AML_INT_NAMEDFIELD_OP:

        /* Get the 4-character name */

        ACPI_MOVE_32_TO_32 (&Name, ParserState->Aml);
        AcpiPsSetName (Field, Name);
        ParserState->Aml += ACPI_NAME_SIZE;

        /* Get the length which is encoded as a package length */

        Field->Common.Value.Size = AcpiPsGetNextPackageLength (ParserState);
        break;


    case AML_INT_RESERVEDFIELD_OP:

        /* Get the length which is encoded as a package length */

        Field->Common.Value.Size = AcpiPsGetNextPackageLength (ParserState);
        break;


    case AML_INT_ACCESSFIELD_OP:

        /*
         * Get AccessType and AccessAttrib and merge into the field Op
         * AccessType is first operand, AccessAttribute is second
         */
        Field->Common.Value.Integer = (ACPI_GET8 (ParserState->Aml) << 8);
        ParserState->Aml++;
        Field->Common.Value.Integer |= ACPI_GET8 (ParserState->Aml);
        ParserState->Aml++;
        break;

    default:

        /* Opcode was set in previous switch */
        break;
    }

    return_PTR (Field);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextArg
 *
 * PARAMETERS:  WalkState           - Current state
 *              ParserState         - Current parser state object
 *              ArgType             - The argument type (AML_*_ARG)
 *              ReturnArg           - Where the next arg is returned
 *
 * RETURN:      Status, and an op object containing the next argument.
 *
 * DESCRIPTION: Get next argument (including complex list arguments that require
 *              pushing the parser stack)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsGetNextArg (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_STATE        *ParserState,
    UINT32                  ArgType,
    ACPI_PARSE_OBJECT       **ReturnArg)
{
    ACPI_PARSE_OBJECT       *Arg = NULL;
    ACPI_PARSE_OBJECT       *Prev = NULL;
    ACPI_PARSE_OBJECT       *Field;
    UINT32                  Subop;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_PTR ("PsGetNextArg", ParserState);


    switch (ArgType)
    {
    case ARGP_BYTEDATA:
    case ARGP_WORDDATA:
    case ARGP_DWORDDATA:
    case ARGP_CHARLIST:
    case ARGP_NAME:
    case ARGP_NAMESTRING:

        /* Constants, strings, and namestrings are all the same size */

        Arg = AcpiPsAllocOp (AML_BYTE_OP);
        if (!Arg)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }
        AcpiPsGetNextSimpleArg (ParserState, ArgType, Arg);
        break;


    case ARGP_PKGLENGTH:

        /* Package length, nothing returned */

        ParserState->PkgEnd = AcpiPsGetNextPackageEnd (ParserState);
        break;


    case ARGP_FIELDLIST:

        if (ParserState->Aml < ParserState->PkgEnd)
        {
            /* Non-empty list */

            while (ParserState->Aml < ParserState->PkgEnd)
            {
                Field = AcpiPsGetNextField (ParserState);
                if (!Field)
                {
                    return_ACPI_STATUS (AE_NO_MEMORY);
                }

                if (Prev)
                {
                    Prev->Common.Next = Field;
                }
                else
                {
                    Arg = Field;
                }
                Prev = Field;
            }

            /* Skip to End of byte data */

            ParserState->Aml = ParserState->PkgEnd;
        }
        break;


    case ARGP_BYTELIST:

        if (ParserState->Aml < ParserState->PkgEnd)
        {
            /* Non-empty list */

            Arg = AcpiPsAllocOp (AML_INT_BYTELIST_OP);
            if (!Arg)
            {
                return_ACPI_STATUS (AE_NO_MEMORY);
            }

            /* Fill in bytelist data */

            Arg->Common.Value.Size = (UINT32)
                ACPI_PTR_DIFF (ParserState->PkgEnd, ParserState->Aml);
            Arg->Named.Data = ParserState->Aml;

            /* Skip to End of byte data */

            ParserState->Aml = ParserState->PkgEnd;
        }
        break;


    case ARGP_TARGET:
    case ARGP_SUPERNAME:
    case ARGP_SIMPLENAME:

        Subop = AcpiPsPeekOpcode (ParserState);
        if (Subop == 0                  ||
            AcpiPsIsLeadingChar (Subop) ||
            AcpiPsIsPrefixChar (Subop))
        {
            /* NullName or NameString */

            Arg = AcpiPsAllocOp (AML_INT_NAMEPATH_OP);
            if (!Arg)
            {
                return_ACPI_STATUS (AE_NO_MEMORY);
            }

            Status = AcpiPsGetNextNamepath (WalkState, ParserState, Arg, 0);
        }
        else
        {
            /* Single complex argument, nothing returned */

            WalkState->ArgCount = 1;
        }
        break;


    case ARGP_DATAOBJ:
    case ARGP_TERMARG:

        /* Single complex argument, nothing returned */

        WalkState->ArgCount = 1;
        break;


    case ARGP_DATAOBJLIST:
    case ARGP_TERMLIST:
    case ARGP_OBJLIST:

        if (ParserState->Aml < ParserState->PkgEnd)
        {
            /* Non-empty list of variable arguments, nothing returned */

            WalkState->ArgCount = ACPI_VAR_ARGS;
        }
        break;


    default:

        ACPI_REPORT_ERROR (("Invalid ArgType: %X\n", ArgType));
        Status = AE_AML_OPERAND_TYPE;
        break;
    }

    *ReturnArg = Arg;
    return_ACPI_STATUS (Status);
}
