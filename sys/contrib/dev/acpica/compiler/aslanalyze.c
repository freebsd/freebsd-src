
/******************************************************************************
 *
 * Module Name: aslanalyze.c - check for semantic errors
 *
 *****************************************************************************/

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


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslanalyze")

/* Local prototypes */

static UINT32
AnMapArgTypeToBtype (
    UINT32                  ArgType);

static UINT32
AnMapEtypeToBtype (
    UINT32                  Etype);

static void
AnFormatBtype (
    char                    *Buffer,
    UINT32                  Btype);

static UINT32
AnGetBtype (
    ACPI_PARSE_OBJECT       *Op);

static UINT32
AnMapObjTypeToBtype (
    ACPI_PARSE_OBJECT       *Op);

static BOOLEAN
AnLastStatementIsReturn (
    ACPI_PARSE_OBJECT       *Op);

static void
AnCheckMethodReturnValue (
    ACPI_PARSE_OBJECT       *Op,
    const ACPI_OPCODE_INFO  *OpInfo,
    ACPI_PARSE_OBJECT       *ArgOp,
    UINT32                  RequiredBtypes,
    UINT32                  ThisNodeBtype);

static BOOLEAN
AnIsInternalMethod (
    ACPI_PARSE_OBJECT       *Op);

static UINT32
AnGetInternalMethodReturnType (
    ACPI_PARSE_OBJECT       *Op);

static BOOLEAN
AnIsResultUsed (
    ACPI_PARSE_OBJECT       *Op);


/*******************************************************************************
 *
 * FUNCTION:    AnIsInternalMethod
 *
 * PARAMETERS:  Op              - Current op
 *
 * RETURN:      Boolean
 *
 * DESCRIPTION: Check for an internal control method.
 *
 ******************************************************************************/

static BOOLEAN
AnIsInternalMethod (
    ACPI_PARSE_OBJECT       *Op)
{

    if ((!ACPI_STRCMP (Op->Asl.ExternalName, "\\_OSI")) ||
        (!ACPI_STRCMP (Op->Asl.ExternalName, "_OSI")))
    {
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AnGetInternalMethodReturnType
 *
 * PARAMETERS:  Op              - Current op
 *
 * RETURN:      Btype
 *
 * DESCRIPTION: Get the return type of an internal method
 *
 ******************************************************************************/

static UINT32
AnGetInternalMethodReturnType (
    ACPI_PARSE_OBJECT       *Op)
{

    if ((!ACPI_STRCMP (Op->Asl.ExternalName, "\\_OSI")) ||
        (!ACPI_STRCMP (Op->Asl.ExternalName, "_OSI")))
    {
        return (ACPI_BTYPE_STRING);
    }

    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    AnMapArgTypeToBtype
 *
 * PARAMETERS:  ArgType      - The ARGI required type(s) for this argument,
 *                             from the opcode info table
 *
 * RETURN:      The corresponding Bit-encoded types
 *
 * DESCRIPTION: Convert an encoded ARGI required argument type code into a
 *              bitfield type code. Implements the implicit source conversion
 *              rules.
 *
 ******************************************************************************/

static UINT32
AnMapArgTypeToBtype (
    UINT32                  ArgType)
{

    switch (ArgType)
    {

    /* Simple types */

    case ARGI_ANYTYPE:
        return (ACPI_BTYPE_OBJECTS_AND_REFS);

    case ARGI_PACKAGE:
        return (ACPI_BTYPE_PACKAGE);

    case ARGI_EVENT:
        return (ACPI_BTYPE_EVENT);

    case ARGI_MUTEX:
        return (ACPI_BTYPE_MUTEX);

    case ARGI_DDBHANDLE:
        /*
         * DDBHandleObject := SuperName
         * ACPI_BTYPE_REFERENCE: Index reference as parameter of Load/Unload
         */
        return (ACPI_BTYPE_DDB_HANDLE | ACPI_BTYPE_REFERENCE);

    /* Interchangeable types */
    /*
     * Source conversion rules:
     * Integer, String, and Buffer are all interchangeable
     */
    case ARGI_INTEGER:
    case ARGI_STRING:
    case ARGI_BUFFER:
    case ARGI_BUFFER_OR_STRING:
    case ARGI_COMPUTEDATA:
        return (ACPI_BTYPE_COMPUTE_DATA);

    /* References */

    case ARGI_INTEGER_REF:
        return (ACPI_BTYPE_INTEGER);

    case ARGI_OBJECT_REF:
        return (ACPI_BTYPE_ALL_OBJECTS);

    case ARGI_DEVICE_REF:
        return (ACPI_BTYPE_DEVICE_OBJECTS);

    case ARGI_REFERENCE:
        return (ACPI_BTYPE_REFERENCE);

    case ARGI_TARGETREF:
    case ARGI_FIXED_TARGET:
    case ARGI_SIMPLE_TARGET:
        return (ACPI_BTYPE_OBJECTS_AND_REFS);

    /* Complex types */

    case ARGI_DATAOBJECT:

        /*
         * Buffer, string, package or reference to a Op -
         * Used only by SizeOf operator
         */
        return (ACPI_BTYPE_STRING | ACPI_BTYPE_BUFFER |
            ACPI_BTYPE_PACKAGE | ACPI_BTYPE_REFERENCE);

    case ARGI_COMPLEXOBJ:

        /* Buffer, String, or package */

        return (ACPI_BTYPE_STRING | ACPI_BTYPE_BUFFER | ACPI_BTYPE_PACKAGE);

    case ARGI_REF_OR_STRING:
        return (ACPI_BTYPE_STRING | ACPI_BTYPE_REFERENCE);

    case ARGI_REGION_OR_BUFFER:

        /* Used by Load() only. Allow buffers in addition to regions/fields */

        return (ACPI_BTYPE_REGION | ACPI_BTYPE_BUFFER | ACPI_BTYPE_FIELD_UNIT);

    case ARGI_DATAREFOBJ:
        return (ACPI_BTYPE_INTEGER |ACPI_BTYPE_STRING | ACPI_BTYPE_BUFFER |
            ACPI_BTYPE_PACKAGE | ACPI_BTYPE_REFERENCE | ACPI_BTYPE_DDB_HANDLE);

    default:
        break;
    }

    return (ACPI_BTYPE_OBJECTS_AND_REFS);
}


/*******************************************************************************
 *
 * FUNCTION:    AnMapEtypeToBtype
 *
 * PARAMETERS:  Etype           - Encoded ACPI Type
 *
 * RETURN:      Btype corresponding to the Etype
 *
 * DESCRIPTION: Convert an encoded ACPI type to a bitfield type applying the
 *              operand conversion rules. In other words, returns the type(s)
 *              this Etype is implicitly converted to during interpretation.
 *
 ******************************************************************************/

static UINT32
AnMapEtypeToBtype (
    UINT32                  Etype)
{


    if (Etype == ACPI_TYPE_ANY)
    {
        return ACPI_BTYPE_OBJECTS_AND_REFS;
    }

    /* Try the standard ACPI data types */

    if (Etype <= ACPI_TYPE_EXTERNAL_MAX)
    {
        /*
         * This switch statement implements the allowed operand conversion
         * rules as per the "ASL Data Types" section of the ACPI
         * specification.
         */
        switch (Etype)
        {
        case ACPI_TYPE_INTEGER:
            return (ACPI_BTYPE_COMPUTE_DATA | ACPI_BTYPE_DDB_HANDLE);

        case ACPI_TYPE_STRING:
        case ACPI_TYPE_BUFFER:
            return (ACPI_BTYPE_COMPUTE_DATA);

        case ACPI_TYPE_PACKAGE:
            return (ACPI_BTYPE_PACKAGE);

        case ACPI_TYPE_FIELD_UNIT:
            return (ACPI_BTYPE_COMPUTE_DATA | ACPI_BTYPE_FIELD_UNIT);

        case ACPI_TYPE_BUFFER_FIELD:
            return (ACPI_BTYPE_COMPUTE_DATA | ACPI_BTYPE_BUFFER_FIELD);

        case ACPI_TYPE_DDB_HANDLE:
            return (ACPI_BTYPE_INTEGER | ACPI_BTYPE_DDB_HANDLE);

        case ACPI_BTYPE_DEBUG_OBJECT:

            /* Cannot be used as a source operand */

            return (0);

        default:
            return (1 << (Etype - 1));
        }
    }

    /* Try the internal data types */

    switch (Etype)
    {
    case ACPI_TYPE_LOCAL_REGION_FIELD:
    case ACPI_TYPE_LOCAL_BANK_FIELD:
    case ACPI_TYPE_LOCAL_INDEX_FIELD:

        /* Named fields can be either Integer/Buffer/String */

        return (ACPI_BTYPE_COMPUTE_DATA | ACPI_BTYPE_FIELD_UNIT);

    case ACPI_TYPE_LOCAL_ALIAS:

        return (ACPI_BTYPE_INTEGER);


    case ACPI_TYPE_LOCAL_RESOURCE:
    case ACPI_TYPE_LOCAL_RESOURCE_FIELD:

        return (ACPI_BTYPE_REFERENCE);

    default:
        printf ("Unhandled encoded type: %X\n", Etype);
        return (0);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AnFormatBtype
 *
 * PARAMETERS:  Btype               - Bitfield of ACPI types
 *              Buffer              - Where to put the ascii string
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Convert a Btype to a string of ACPI types
 *
 ******************************************************************************/

static void
AnFormatBtype (
    char                    *Buffer,
    UINT32                  Btype)
{
    UINT32                  Type;
    BOOLEAN                 First = TRUE;


    *Buffer = 0;

    if (Btype == 0)
    {
        strcat (Buffer, "NoReturnValue");
        return;
    }

    for (Type = 1; Type <= ACPI_TYPE_EXTERNAL_MAX; Type++)
    {
        if (Btype & 0x00000001)
        {
            if (!First)
            {
                strcat (Buffer, "|");
            }
            First = FALSE;
            strcat (Buffer, AcpiUtGetTypeName (Type));
        }
        Btype >>= 1;
    }

    if (Btype & 0x00000001)
    {
        if (!First)
        {
            strcat (Buffer, "|");
        }
        First = FALSE;
        strcat (Buffer, "Reference");
    }

    Btype >>= 1;
    if (Btype & 0x00000001)
    {
        if (!First)
        {
            strcat (Buffer, "|");
        }
        First = FALSE;
        strcat (Buffer, "Resource");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AnGetBtype
 *
 * PARAMETERS:  Op          - Parse node whose type will be returned.
 *
 * RETURN:      The Btype associated with the Op.
 *
 * DESCRIPTION: Get the (bitfield) ACPI type associated with the parse node.
 *              Handles the case where the node is a name or method call and
 *              the actual type must be obtained from the namespace node.
 *
 ******************************************************************************/

static UINT32
AnGetBtype (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_PARSE_OBJECT       *ReferencedNode;
    UINT32                  ThisNodeBtype = 0;


    if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)     ||
        (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING)  ||
        (Op->Asl.ParseOpcode == PARSEOP_METHODCALL))
    {
        Node = Op->Asl.Node;
        if (!Node)
        {
            DbgPrint (ASL_DEBUG_OUTPUT,
                "No attached Nsnode: [%s] at line %u name [%s], ignoring typecheck\n",
                Op->Asl.ParseOpName, Op->Asl.LineNumber,
                Op->Asl.ExternalName);
            return ACPI_UINT32_MAX;
        }

        ThisNodeBtype = AnMapEtypeToBtype (Node->Type);
        if (!ThisNodeBtype)
        {
            AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL, Op,
                "could not map type");
        }

        /*
         * Since it was a named reference, enable the
         * reference bit also
         */
        ThisNodeBtype |= ACPI_BTYPE_REFERENCE;

        if (Op->Asl.ParseOpcode == PARSEOP_METHODCALL)
        {
            ReferencedNode = Node->Op;
            if (!ReferencedNode)
            {
                /* Check for an internal method */

                if (AnIsInternalMethod (Op))
                {
                    return (AnGetInternalMethodReturnType (Op));
                }

                AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL, Op,
                    "null Op pointer");
                return ACPI_UINT32_MAX;
            }

            if (ReferencedNode->Asl.CompileFlags & NODE_METHOD_TYPED)
            {
                ThisNodeBtype = ReferencedNode->Asl.AcpiBtype;
            }
            else
            {
                return (ACPI_UINT32_MAX -1);
            }
        }
    }
    else
    {
        ThisNodeBtype = Op->Asl.AcpiBtype;
    }

    return (ThisNodeBtype);
}


/*******************************************************************************
 *
 * FUNCTION:    AnMapObjTypeToBtype
 *
 * PARAMETERS:  Op              - A parse node
 *
 * RETURN:      A Btype
 *
 * DESCRIPTION: Map object to the associated "Btype"
 *
 ******************************************************************************/

static UINT32
AnMapObjTypeToBtype (
    ACPI_PARSE_OBJECT       *Op)
{

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_OBJECTTYPE_BFF:        /* "BuffFieldObj" */
        return (ACPI_BTYPE_BUFFER_FIELD);

    case PARSEOP_OBJECTTYPE_BUF:        /* "BuffObj" */
        return (ACPI_BTYPE_BUFFER);

    case PARSEOP_OBJECTTYPE_DDB:        /* "DDBHandleObj" */
        return (ACPI_BTYPE_DDB_HANDLE);

    case PARSEOP_OBJECTTYPE_DEV:        /* "DeviceObj" */
        return (ACPI_BTYPE_DEVICE);

    case PARSEOP_OBJECTTYPE_EVT:        /* "EventObj" */
        return (ACPI_BTYPE_EVENT);

    case PARSEOP_OBJECTTYPE_FLD:        /* "FieldUnitObj" */
        return (ACPI_BTYPE_FIELD_UNIT);

    case PARSEOP_OBJECTTYPE_INT:        /* "IntObj" */
        return (ACPI_BTYPE_INTEGER);

    case PARSEOP_OBJECTTYPE_MTH:        /* "MethodObj" */
        return (ACPI_BTYPE_METHOD);

    case PARSEOP_OBJECTTYPE_MTX:        /* "MutexObj" */
        return (ACPI_BTYPE_MUTEX);

    case PARSEOP_OBJECTTYPE_OPR:        /* "OpRegionObj" */
        return (ACPI_BTYPE_REGION);

    case PARSEOP_OBJECTTYPE_PKG:        /* "PkgObj" */
        return (ACPI_BTYPE_PACKAGE);

    case PARSEOP_OBJECTTYPE_POW:        /* "PowerResObj" */
        return (ACPI_BTYPE_POWER);

    case PARSEOP_OBJECTTYPE_STR:        /* "StrObj" */
        return (ACPI_BTYPE_STRING);

    case PARSEOP_OBJECTTYPE_THZ:        /* "ThermalZoneObj" */
        return (ACPI_BTYPE_THERMAL);

    case PARSEOP_OBJECTTYPE_UNK:        /* "UnknownObj" */
        return (ACPI_BTYPE_OBJECTS_AND_REFS);

    default:
        return (0);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AnCheckId
 *
 * PARAMETERS:  Op                  - Current parse op
 *              Type                - HID or CID
 *
 * RETURN:      None
 *
 * DESCRIPTION: Perform various checks on _HID and _CID strings. Only limited
 *              checks can be performed on _CID strings.
 *
 ******************************************************************************/

#define ASL_TYPE_HID        0
#define ASL_TYPE_CID        1
#include <string.h>

static void
AnCheckId (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAME               Type)
{
    UINT32                  i;
    ACPI_SIZE               Length;
    UINT32                  AlphaPrefixLength;


    /* Only care about string versions of _HID/_CID (integers are legal) */

    if (Op->Asl.ParseOpcode != PARSEOP_STRING_LITERAL)
    {
        return;
    }

    /* For both _HID and _CID, the string must be non-null */

    Length = strlen (Op->Asl.Value.String);
    if (!Length)
    {
        AslError (ASL_ERROR, ASL_MSG_NULL_STRING,
            Op, NULL);
        return;
    }

    /*
     * One of the things we want to catch here is the use of a leading
     * asterisk in the string -- an odd construct that certain platform
     * manufacturers are fond of. Technically, a leading asterisk is OK
     * for _CID, but a valid use of this has not been seen.
     */
    if (*Op->Asl.Value.String == '*')
    {
        AslError (ASL_ERROR, ASL_MSG_LEADING_ASTERISK,
            Op, Op->Asl.Value.String);
        return;
    }

    /* _CID strings are bus-specific, no more checks can be performed */

    if (Type == ASL_TYPE_CID)
    {
        return;
    }

    /* For _HID, all characters must be alphanumeric */

    for (i = 0; Op->Asl.Value.String[i]; i++)
    {
        if (!isalnum ((int) Op->Asl.Value.String[i]))
        {
            AslError (ASL_ERROR, ASL_MSG_ALPHANUMERIC_STRING,
                Op, Op->Asl.Value.String);
            break;
        }
    }

    /* _HID String must be of the form "XXX####" or "ACPI####" */

    if ((Length < 7) || (Length > 8))
    {
        AslError (ASL_ERROR, ASL_MSG_HID_LENGTH,
            Op, Op->Asl.Value.String);
        return;
    }

    /* _HID Length is valid, now check for uppercase (first 3 or 4 chars) */

    AlphaPrefixLength = 3;
    if (Length >= 8)
    {
        AlphaPrefixLength = 4;
    }

    /* Ensure the alphabetic prefix is all uppercase */

    for (i = 0; (i < AlphaPrefixLength) && Op->Asl.Value.String[i]; i++)
    {
        if (!isupper ((int) Op->Asl.Value.String[i]))
        {
            AslError (ASL_ERROR, ASL_MSG_UPPER_CASE,
                Op, &Op->Asl.Value.String[i]);
            break;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AnMethodAnalysisWalkBegin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback for the analysis walk. Check methods for:
 *              1) Initialized local variables
 *              2) Valid arguments
 *              3) Return types
 *
 ******************************************************************************/

ACPI_STATUS
AnMethodAnalysisWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_ANALYSIS_WALK_INFO  *WalkInfo = (ASL_ANALYSIS_WALK_INFO *) Context;
    ASL_METHOD_INFO         *MethodInfo = WalkInfo->MethodStack;
    ACPI_PARSE_OBJECT       *Next;
    UINT32                  RegisterNumber;
    UINT32                  i;
    char                    LocalName[] = "Local0";
    char                    ArgName[] = "Arg0";
    ACPI_PARSE_OBJECT       *ArgNode;
    ACPI_PARSE_OBJECT       *NextType;
    ACPI_PARSE_OBJECT       *NextParamType;
    UINT8                   ActualArgs = 0;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        TotalMethods++;

        /* Create and init method info */

        MethodInfo       = UtLocalCalloc (sizeof (ASL_METHOD_INFO));
        MethodInfo->Next = WalkInfo->MethodStack;
        MethodInfo->Op = Op;

        WalkInfo->MethodStack = MethodInfo;

        /* Get the name node, ignored here */

        Next = Op->Asl.Child;

        /* Get the NumArguments node */

        Next = Next->Asl.Next;
        MethodInfo->NumArguments = (UINT8)
            (((UINT8) Next->Asl.Value.Integer) & 0x07);

        /* Get the SerializeRule and SyncLevel nodes, ignored here */

        Next = Next->Asl.Next;
        Next = Next->Asl.Next;
        ArgNode = Next;

        /* Get the ReturnType node */

        Next = Next->Asl.Next;

        NextType = Next->Asl.Child;
        while (NextType)
        {
            /* Get and map each of the ReturnTypes */

            MethodInfo->ValidReturnTypes |= AnMapObjTypeToBtype (NextType);
            NextType->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
            NextType = NextType->Asl.Next;
        }

        /* Get the ParameterType node */

        Next = Next->Asl.Next;

        NextType = Next->Asl.Child;
        while (NextType)
        {
            if (NextType->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
            {
                NextParamType = NextType->Asl.Child;
                while (NextParamType)
                {
                    MethodInfo->ValidArgTypes[ActualArgs] |= AnMapObjTypeToBtype (NextParamType);
                    NextParamType->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
                    NextParamType = NextParamType->Asl.Next;
                }
            }
            else
            {
                MethodInfo->ValidArgTypes[ActualArgs] =
                    AnMapObjTypeToBtype (NextType);
                NextType->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
                ActualArgs++;
            }

            NextType = NextType->Asl.Next;
        }

        if ((MethodInfo->NumArguments) &&
            (MethodInfo->NumArguments != ActualArgs))
        {
            /* error: Param list did not match number of args */
        }

        /* Allow numarguments == 0 for Function() */

        if ((!MethodInfo->NumArguments) && (ActualArgs))
        {
            MethodInfo->NumArguments = ActualArgs;
            ArgNode->Asl.Value.Integer |= ActualArgs;
        }

        /*
         * Actual arguments are initialized at method entry.
         * All other ArgX "registers" can be used as locals, so we
         * track their initialization.
         */
        for (i = 0; i < MethodInfo->NumArguments; i++)
        {
            MethodInfo->ArgInitialized[i] = TRUE;
        }
        break;


    case PARSEOP_METHODCALL:

        if (MethodInfo &&
           (Op->Asl.Node == MethodInfo->Op->Asl.Node))
        {
            AslError (ASL_REMARK, ASL_MSG_RECURSION, Op, Op->Asl.ExternalName);
        }
        break;


    case PARSEOP_LOCAL0:
    case PARSEOP_LOCAL1:
    case PARSEOP_LOCAL2:
    case PARSEOP_LOCAL3:
    case PARSEOP_LOCAL4:
    case PARSEOP_LOCAL5:
    case PARSEOP_LOCAL6:
    case PARSEOP_LOCAL7:

        if (!MethodInfo)
        {
            /*
             * Local was used outside a control method, or there was an error
             * in the method declaration.
             */
            AslError (ASL_REMARK, ASL_MSG_LOCAL_OUTSIDE_METHOD, Op, Op->Asl.ExternalName);
            return (AE_ERROR);
        }

        RegisterNumber = (Op->Asl.AmlOpcode & 0x000F);

        /*
         * If the local is being used as a target, mark the local
         * initialized
         */
        if (Op->Asl.CompileFlags & NODE_IS_TARGET)
        {
            MethodInfo->LocalInitialized[RegisterNumber] = TRUE;
        }

        /*
         * Otherwise, this is a reference, check if the local
         * has been previously initialized.
         *
         * The only operator that accepts an uninitialized value is ObjectType()
         */
        else if ((!MethodInfo->LocalInitialized[RegisterNumber]) &&
                 (Op->Asl.Parent->Asl.ParseOpcode != PARSEOP_OBJECTTYPE))
        {
            LocalName[strlen (LocalName) -1] = (char) (RegisterNumber + 0x30);
            AslError (ASL_ERROR, ASL_MSG_LOCAL_INIT, Op, LocalName);
        }
        break;


    case PARSEOP_ARG0:
    case PARSEOP_ARG1:
    case PARSEOP_ARG2:
    case PARSEOP_ARG3:
    case PARSEOP_ARG4:
    case PARSEOP_ARG5:
    case PARSEOP_ARG6:

        if (!MethodInfo)
        {
            /*
             * Arg was used outside a control method, or there was an error
             * in the method declaration.
             */
            AslError (ASL_REMARK, ASL_MSG_LOCAL_OUTSIDE_METHOD, Op, Op->Asl.ExternalName);
            return (AE_ERROR);
        }

        RegisterNumber = (Op->Asl.AmlOpcode & 0x000F) - 8;
        ArgName[strlen (ArgName) -1] = (char) (RegisterNumber + 0x30);

        /*
         * If the Arg is being used as a target, mark the local
         * initialized
         */
        if (Op->Asl.CompileFlags & NODE_IS_TARGET)
        {
            MethodInfo->ArgInitialized[RegisterNumber] = TRUE;
        }

        /*
         * Otherwise, this is a reference, check if the Arg
         * has been previously initialized.
         *
         * The only operator that accepts an uninitialized value is ObjectType()
         */
        else if ((!MethodInfo->ArgInitialized[RegisterNumber]) &&
                 (Op->Asl.Parent->Asl.ParseOpcode != PARSEOP_OBJECTTYPE))
        {
            AslError (ASL_ERROR, ASL_MSG_ARG_INIT, Op, ArgName);
        }

        /* Flag this arg if it is not a "real" argument to the method */

        if (RegisterNumber >= MethodInfo->NumArguments)
        {
            AslError (ASL_REMARK, ASL_MSG_NOT_PARAMETER, Op, ArgName);
        }
        break;


    case PARSEOP_RETURN:

        if (!MethodInfo)
        {
            /*
             * Probably was an error in the method declaration,
             * no additional error here
             */
            ACPI_WARNING ((AE_INFO, "%p, No parent method", Op));
            return (AE_ERROR);
        }

        /* Child indicates a return value */

        if ((Op->Asl.Child) &&
            (Op->Asl.Child->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG))
        {
            MethodInfo->NumReturnWithValue++;
        }
        else
        {
            MethodInfo->NumReturnNoValue++;
        }
        break;


    case PARSEOP_BREAK:
    case PARSEOP_CONTINUE:

        Next = Op->Asl.Parent;
        while (Next)
        {
            if (Next->Asl.ParseOpcode == PARSEOP_WHILE)
            {
                break;
            }
            Next = Next->Asl.Parent;
        }

        if (!Next)
        {
            AslError (ASL_ERROR, ASL_MSG_NO_WHILE, Op, NULL);
        }
        break;


    case PARSEOP_STALL:

        /* We can range check if the argument is an integer */

        if ((Op->Asl.Child->Asl.ParseOpcode == PARSEOP_INTEGER) &&
            (Op->Asl.Child->Asl.Value.Integer > ACPI_UINT8_MAX))
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_TIME, Op, NULL);
        }
        break;


    case PARSEOP_DEVICE:
    case PARSEOP_EVENT:
    case PARSEOP_MUTEX:
    case PARSEOP_OPERATIONREGION:
    case PARSEOP_POWERRESOURCE:
    case PARSEOP_PROCESSOR:
    case PARSEOP_THERMALZONE:

        /*
         * The first operand is a name to be created in the namespace.
         * Check against the reserved list.
         */
        i = ApCheckForPredefinedName (Op, Op->Asl.NameSeg);
        if (i < ACPI_VALID_RESERVED_NAME_MAX)
        {
            AslError (ASL_ERROR, ASL_MSG_RESERVED_USE, Op, Op->Asl.ExternalName);
        }
        break;


    case PARSEOP_NAME:

        /* Typecheck any predefined names statically defined with Name() */

        ApCheckForPredefinedObject (Op, Op->Asl.NameSeg);

        /* Special typechecking for _HID */

        if (!ACPI_STRCMP (METHOD_NAME__HID, Op->Asl.NameSeg))
        {
            Next = Op->Asl.Child->Asl.Next;
            AnCheckId (Next, ASL_TYPE_HID);
        }

        /* Special typechecking for _CID */

        else if (!ACPI_STRCMP (METHOD_NAME__CID, Op->Asl.NameSeg))
        {
            Next = Op->Asl.Child->Asl.Next;

            if ((Next->Asl.ParseOpcode == PARSEOP_PACKAGE) ||
                (Next->Asl.ParseOpcode == PARSEOP_VAR_PACKAGE))
            {
                Next = Next->Asl.Child;
                while (Next)
                {
                    AnCheckId (Next, ASL_TYPE_CID);
                    Next = Next->Asl.Next;
                }
            }
            else
            {
                AnCheckId (Next, ASL_TYPE_CID);
            }
        }
        break;


    default:
        break;
    }

    return AE_OK;
}


/*******************************************************************************
 *
 * FUNCTION:    AnLastStatementIsReturn
 *
 * PARAMETERS:  Op            - A method parse node
 *
 * RETURN:      TRUE if last statement is an ASL RETURN. False otherwise
 *
 * DESCRIPTION: Walk down the list of top level statements within a method
 *              to find the last one. Check if that last statement is in
 *              fact a RETURN statement.
 *
 ******************************************************************************/

static BOOLEAN
AnLastStatementIsReturn (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Next;


    /*
     * Check if last statement is a return
     */
    Next = ASL_GET_CHILD_NODE (Op);
    while (Next)
    {
        if ((!Next->Asl.Next) &&
            (Next->Asl.ParseOpcode == PARSEOP_RETURN))
        {
            return TRUE;
        }

        Next = ASL_GET_PEER_NODE (Next);
    }

    return FALSE;
}


/*******************************************************************************
 *
 * FUNCTION:    AnMethodAnalysisWalkEnd
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
AnMethodAnalysisWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_ANALYSIS_WALK_INFO  *WalkInfo = (ASL_ANALYSIS_WALK_INFO *) Context;
    ASL_METHOD_INFO         *MethodInfo = WalkInfo->MethodStack;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:
    case PARSEOP_RETURN:
        if (!MethodInfo)
        {
            printf ("No method info for method! [%s]\n", Op->Asl.Namepath);
            AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL, Op,
                "No method info for this method");
            CmCleanupAndExit ();
            return (AE_AML_INTERNAL);
        }
        break;

    default:
        break;
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        WalkInfo->MethodStack = MethodInfo->Next;

        /*
         * Check if there is no return statement at the end of the
         * method AND we can actually get there -- i.e., the execution
         * of the method can possibly terminate without a return statement.
         */
        if ((!AnLastStatementIsReturn (Op)) &&
            (!(Op->Asl.CompileFlags & NODE_HAS_NO_EXIT)))
        {
            /*
             * No return statement, and execution can possibly exit
             * via this path. This is equivalent to Return ()
             */
            MethodInfo->NumReturnNoValue++;
        }

        /*
         * Check for case where some return statements have a return value
         * and some do not. Exit without a return statement is a return with
         * no value
         */
        if (MethodInfo->NumReturnNoValue &&
            MethodInfo->NumReturnWithValue)
        {
            AslError (ASL_WARNING, ASL_MSG_RETURN_TYPES, Op,
                Op->Asl.ExternalName);
        }

        /*
         * If there are any RETURN() statements with no value, or there is a
         * control path that allows the method to exit without a return value,
         * we mark the method as a method that does not return a value. This
         * knowledge can be used to check method invocations that expect a
         * returned value.
         */
        if (MethodInfo->NumReturnNoValue)
        {
            if (MethodInfo->NumReturnWithValue)
            {
                Op->Asl.CompileFlags |= NODE_METHOD_SOME_NO_RETVAL;
            }
            else
            {
                Op->Asl.CompileFlags |= NODE_METHOD_NO_RETVAL;
            }
        }

        /*
         * Check predefined method names for correct return behavior
         * and correct number of arguments
         */
        ApCheckForPredefinedMethod (Op, MethodInfo);
        ACPI_FREE (MethodInfo);
        break;


    case PARSEOP_RETURN:

        /*
         * If the parent is a predefined method name, attempt to typecheck
         * the return value. Only static types can be validated.
         */
        ApCheckPredefinedReturnValue (Op, MethodInfo);

        /*
         * The parent block does not "exit" and continue execution -- the
         * method is terminated here with the Return() statement.
         */
        Op->Asl.Parent->Asl.CompileFlags |= NODE_HAS_NO_EXIT;

        /* Used in the "typing" pass later */

        Op->Asl.ParentMethod = MethodInfo->Op;

        /*
         * If there is a peer node after the return statement, then this
         * node is unreachable code -- i.e., it won't be executed because of
         * the preceeding Return() statement.
         */
        if (Op->Asl.Next)
        {
            AslError (ASL_WARNING, ASL_MSG_UNREACHABLE_CODE, Op->Asl.Next, NULL);
        }
        break;


    case PARSEOP_IF:

        if ((Op->Asl.CompileFlags & NODE_HAS_NO_EXIT) &&
            (Op->Asl.Next) &&
            (Op->Asl.Next->Asl.ParseOpcode == PARSEOP_ELSE))
        {
            /*
             * This IF has a corresponding ELSE. The IF block has no exit,
             * (it contains an unconditional Return)
             * mark the ELSE block to remember this fact.
             */
            Op->Asl.Next->Asl.CompileFlags |= NODE_IF_HAS_NO_EXIT;
        }
        break;


    case PARSEOP_ELSE:

        if ((Op->Asl.CompileFlags & NODE_HAS_NO_EXIT) &&
            (Op->Asl.CompileFlags & NODE_IF_HAS_NO_EXIT))
        {
            /*
             * This ELSE block has no exit and the corresponding IF block
             * has no exit either. Therefore, the parent node has no exit.
             */
            Op->Asl.Parent->Asl.CompileFlags |= NODE_HAS_NO_EXIT;
        }
        break;


    default:

        if ((Op->Asl.CompileFlags & NODE_HAS_NO_EXIT) &&
            (Op->Asl.Parent))
        {
            /* If this node has no exit, then the parent has no exit either */

            Op->Asl.Parent->Asl.CompileFlags |= NODE_HAS_NO_EXIT;
        }
        break;
    }

    return AE_OK;
}


/*******************************************************************************
 *
 * FUNCTION:    AnMethodTypingWalkBegin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback for the typing walk.
 *
 ******************************************************************************/

ACPI_STATUS
AnMethodTypingWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    return AE_OK;
}


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
                        ASL_WALK_VISIT_TWICE, AnMethodTypingWalkBegin,
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

    return AE_OK;
}


/*******************************************************************************
 *
 * FUNCTION:    AnCheckMethodReturnValue
 *
 * PARAMETERS:  Op                  - Parent
 *              OpInfo              - Parent info
 *              ArgOp               - Method invocation op
 *              RequiredBtypes      - What caller requires
 *              ThisNodeBtype       - What this node returns (if anything)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check a method invocation for 1) A return value and if it does
 *              in fact return a value, 2) check the type of the return value.
 *
 ******************************************************************************/

static void
AnCheckMethodReturnValue (
    ACPI_PARSE_OBJECT       *Op,
    const ACPI_OPCODE_INFO  *OpInfo,
    ACPI_PARSE_OBJECT       *ArgOp,
    UINT32                  RequiredBtypes,
    UINT32                  ThisNodeBtype)
{
    ACPI_PARSE_OBJECT       *OwningOp;
    ACPI_NAMESPACE_NODE     *Node;


    Node = ArgOp->Asl.Node;


    /* Examine the parent op of this method */

    OwningOp = Node->Op;
    if (OwningOp->Asl.CompileFlags & NODE_METHOD_NO_RETVAL)
    {
        /* Method NEVER returns a value */

        AslError (ASL_ERROR, ASL_MSG_NO_RETVAL, Op, Op->Asl.ExternalName);
    }
    else if (OwningOp->Asl.CompileFlags & NODE_METHOD_SOME_NO_RETVAL)
    {
        /* Method SOMETIMES returns a value, SOMETIMES not */

        AslError (ASL_WARNING, ASL_MSG_SOME_NO_RETVAL, Op, Op->Asl.ExternalName);
    }
    else if (!(ThisNodeBtype & RequiredBtypes))
    {
        /* Method returns a value, but the type is wrong */

        AnFormatBtype (StringBuffer, ThisNodeBtype);
        AnFormatBtype (StringBuffer2, RequiredBtypes);


        /*
         * The case where the method does not return any value at all
         * was already handled in the namespace cross reference
         * -- Only issue an error if the method in fact returns a value,
         * but it is of the wrong type
         */
        if (ThisNodeBtype != 0)
        {
            sprintf (MsgBuffer,
                "Method returns [%s], %s operator requires [%s]",
                StringBuffer, OpInfo->Name, StringBuffer2);

            AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ArgOp, MsgBuffer);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AnOperandTypecheckWalkBegin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback for the analysis walk. Check methods for:
 *              1) Initialized local variables
 *              2) Valid arguments
 *              3) Return types
 *
 ******************************************************************************/

ACPI_STATUS
AnOperandTypecheckWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    return AE_OK;
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
 * FUNCTION:    AnIsResultUsed
 *
 * PARAMETERS:  Op              - Parent op for the operator
 *
 * RETURN:      TRUE if result from this operation is actually consumed
 *
 * DESCRIPTION: Determine if the function result value from an operator is
 *              used.
 *
 ******************************************************************************/

static BOOLEAN
AnIsResultUsed (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Parent;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_INCREMENT:
    case PARSEOP_DECREMENT:

        /* These are standalone operators, no return value */

        return (TRUE);

    default:
        break;
    }

    /* Examine parent to determine if the return value is used */

    Parent = Op->Asl.Parent;
    switch (Parent->Asl.ParseOpcode)
    {
    /* If/While - check if the operator is the predicate */

    case PARSEOP_IF:
    case PARSEOP_WHILE:

        /* First child is the predicate */

        if (Parent->Asl.Child == Op)
        {
            return (TRUE);
        }
        return (FALSE);

    /* Not used if one of these is the parent */

    case PARSEOP_METHOD:
    case PARSEOP_DEFINITIONBLOCK:
    case PARSEOP_ELSE:

        return (FALSE);

    default:
        /* Any other type of parent means that the result is used */

        return (TRUE);
    }
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
                    AslError (ASL_WARNING, ASL_MSG_RESULT_NOT_USED, Op, Op->Asl.ExternalName);
                }
            }
            else if (ArgNode->Asl.ParseOpcode == PARSEOP_ZERO)
            {
                AslError (ASL_WARNING, ASL_MSG_RESULT_NOT_USED, Op, Op->Asl.ExternalName);
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
                AslError (ASL_WARNING, ASL_MSG_RESULT_NOT_USED, Op, Op->Asl.ExternalName);
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
            AslError (ASL_WARNING, ASL_MSG_TIMEOUT, ArgNode, Op->Asl.ExternalName);
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

    default:
        break;
    }

    return AE_OK;
}


/*******************************************************************************
 *
 * FUNCTION:    AnOtherSemanticAnalysisWalkEnd
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
AnOtherSemanticAnalysisWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    return AE_OK;

}


#ifdef ACPI_OBSOLETE_FUNCTIONS
/*******************************************************************************
 *
 * FUNCTION:    AnMapBtypeToEtype
 *
 * PARAMETERS:  Btype               - Bitfield of ACPI types
 *
 * RETURN:      The Etype corresponding the the Btype
 *
 * DESCRIPTION: Convert a bitfield type to an encoded type
 *
 ******************************************************************************/

UINT32
AnMapBtypeToEtype (
    UINT32              Btype)
{
    UINT32              i;
    UINT32              Etype;


    if (Btype == 0)
    {
        return 0;
    }

    Etype = 1;
    for (i = 1; i < Btype; i *= 2)
    {
        Etype++;
    }

    return (Etype);
}
#endif

