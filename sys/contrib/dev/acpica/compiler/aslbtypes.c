/******************************************************************************
 *
 * Module Name: aslbtypes - Support for bitfield types
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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
#include <contrib/dev/acpica/include/amlcode.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslbtypes")

/* Local prototypes */

static UINT32
AnMapEtypeToBtype (
    UINT32                  Etype);


/*******************************************************************************
 *
 * FUNCTION:    AnMapArgTypeToBtype
 *
 * PARAMETERS:  ArgType             - The ARGI required type(s) for this
 *                                    argument, from the opcode info table
 *
 * RETURN:      The corresponding Bit-encoded types
 *
 * DESCRIPTION: Convert an encoded ARGI required argument type code into a
 *              bitfield type code. Implements the implicit source conversion
 *              rules.
 *
 ******************************************************************************/

UINT32
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
 * PARAMETERS:  Etype               - Encoded ACPI Type
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
        return (ACPI_BTYPE_OBJECTS_AND_REFS);
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

        case ACPI_TYPE_DEBUG_OBJECT:

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

void
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
 * PARAMETERS:  Op                  - Parse node whose type will be returned.
 *
 * RETURN:      The Btype associated with the Op.
 *
 * DESCRIPTION: Get the (bitfield) ACPI type associated with the parse node.
 *              Handles the case where the node is a name or method call and
 *              the actual type must be obtained from the namespace node.
 *
 ******************************************************************************/

UINT32
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
            return (ACPI_UINT32_MAX);
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
                return (ACPI_UINT32_MAX);
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
 * PARAMETERS:  Op                  - A parse node
 *
 * RETURN:      A Btype
 *
 * DESCRIPTION: Map object to the associated "Btype"
 *
 ******************************************************************************/

UINT32
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
        return (0);
    }

    Etype = 1;
    for (i = 1; i < Btype; i *= 2)
    {
        Etype++;
    }

    return (Etype);
}
#endif
