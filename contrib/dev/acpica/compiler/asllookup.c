/******************************************************************************
 *
 * Module Name: asllookup- Namespace lookup functions
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
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdispat.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asllookup")

/* Local prototypes */

static ACPI_STATUS
LkIsObjectUsed (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_PARSE_OBJECT *
LkGetNameOp (
    ACPI_PARSE_OBJECT       *Op);


/*******************************************************************************
 *
 * FUNCTION:    LkFindUnreferencedObjects
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Namespace walk to find objects that are not referenced in any
 *              way. Must be called after the namespace has been cross
 *              referenced.
 *
 ******************************************************************************/

void
LkFindUnreferencedObjects (
    void)
{

    /* Walk entire namespace from the supplied root */

    (void) AcpiNsWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
                ACPI_UINT32_MAX, FALSE, LkIsObjectUsed, NULL,
                NULL, NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    LkIsObjectUsed
 *
 * PARAMETERS:  ACPI_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check for an unreferenced namespace object and emit a warning.
 *              We have to be careful, because some types and names are
 *              typically or always unreferenced, we don't want to issue
 *              excessive warnings. Note: Names that are declared within a
 *              control method are temporary, so we always issue a remark
 *              if they are not referenced.
 *
 ******************************************************************************/

static ACPI_STATUS
LkIsObjectUsed (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, ObjHandle);
    ACPI_NAMESPACE_NODE     *Next;


    /* Referenced flag is set during the namespace xref */

    if (Node->Flags & ANOBJ_IS_REFERENCED)
    {
        return (AE_OK);
    }

    if (!Node->Op)
    {
        return (AE_OK);
    }

    /* These types are typically never directly referenced, ignore them */

    switch (Node->Type)
    {
    case ACPI_TYPE_DEVICE:
    case ACPI_TYPE_PROCESSOR:
    case ACPI_TYPE_POWER:
    case ACPI_TYPE_THERMAL:
    case ACPI_TYPE_LOCAL_RESOURCE:

        return (AE_OK);

    default:

        break;
    }

    /* Determine if the name is within a control method */

    Next = Node->Parent;
    while (Next)
    {
        if (Next->Type == ACPI_TYPE_METHOD)
        {
            /*
             * Name is within a method, therefore it is temporary.
             * Issue a remark even if it is a reserved name (starts
             * with an underscore).
             */
            sprintf (MsgBuffer, "Name is within method [%4.4s]",
                Next->Name.Ascii);
            AslError (ASL_REMARK, ASL_MSG_NOT_REFERENCED,
                LkGetNameOp (Node->Op), MsgBuffer);
            return (AE_OK);
        }

        Next = Next->Parent;
    }

    /* The name is not within a control method */

    /*
     * Ignore names that start with an underscore. These are the reserved
     * ACPI names and are typically not referenced since they are meant
     * to be called by the host OS.
     */
    if (Node->Name.Ascii[0] == '_')
    {
        return (AE_OK);
    }

    /*
     * What remains is an unresolved user name that is not within a method.
     * However, the object could be referenced via another table, so issue
     * the warning at level 2.
     */
    AslError (ASL_WARNING2, ASL_MSG_NOT_REFERENCED,
        LkGetNameOp (Node->Op), NULL);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LkGetNameOp
 *
 * PARAMETERS:  Op              - Current Op
 *
 * RETURN:      NameOp associated with the input op
 *
 * DESCRIPTION: Find the name declaration op associated with the operator
 *
 ******************************************************************************/

static ACPI_PARSE_OBJECT *
LkGetNameOp (
    ACPI_PARSE_OBJECT       *Op)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_PARSE_OBJECT       *NameOp = Op;


    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);


    /* Get the NamePath from the appropriate place */

    if (OpInfo->Flags & AML_NAMED)
    {
        /* For nearly all NAMED operators, the name reference is the first child */

        NameOp = Op->Asl.Child;
        if (Op->Asl.AmlOpcode == AML_ALIAS_OP)
        {
            /*
             * ALIAS is the only oddball opcode, the name declaration
             * (alias name) is the second operand
             */
            NameOp = Op->Asl.Child->Asl.Next;
        }
    }
    else if (OpInfo->Flags & AML_CREATE)
    {
        /* Name must appear as the last parameter */

        NameOp = Op->Asl.Child;
        while (!(NameOp->Asl.CompileFlags & NODE_IS_NAME_DECLARATION))
        {
            NameOp = NameOp->Asl.Next;
        }
    }

    return (NameOp);
}
