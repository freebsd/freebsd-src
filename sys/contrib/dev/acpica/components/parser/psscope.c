/******************************************************************************
 *
 * Module Name: psscope - Parser scope stack management routines
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2025, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acparser.h>

#define _COMPONENT          ACPI_PARSER
        ACPI_MODULE_NAME    ("psscope")


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetParentScope
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Pointer to an Op object
 *
 * DESCRIPTION: Get parent of current op being parsed
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
AcpiPsGetParentScope (
    ACPI_PARSE_STATE        *ParserState)
{

    return (ParserState->Scope->ParseScope.Op);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsHasCompletedScope
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Boolean, TRUE = scope completed.
 *
 * DESCRIPTION: Is parsing of current argument complete?  Determined by
 *              1) AML pointer is at or beyond the end of the scope
 *              2) The scope argument count has reached zero.
 *
 ******************************************************************************/

BOOLEAN
AcpiPsHasCompletedScope (
    ACPI_PARSE_STATE        *ParserState)
{

    return ((BOOLEAN)
            ((ParserState->Aml >= ParserState->Scope->ParseScope.ArgEnd ||
             !ParserState->Scope->ParseScope.ArgCount)));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsInitScope
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *              Root                - the Root Node of this new scope
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Allocate and init a new scope object
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsInitScope (
    ACPI_PARSE_STATE        *ParserState,
    ACPI_PARSE_OBJECT       *RootOp)
{
    ACPI_GENERIC_STATE      *Scope;


    ACPI_FUNCTION_TRACE_PTR (PsInitScope, RootOp);


    Scope = AcpiUtCreateGenericState ();
    if (!Scope)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Scope->Common.DescriptorType = ACPI_DESC_TYPE_STATE_RPSCOPE;
    Scope->ParseScope.Op = RootOp;
    Scope->ParseScope.ArgCount = ACPI_VAR_ARGS;
    Scope->ParseScope.ArgEnd = ParserState->AmlEnd;
    Scope->ParseScope.PkgEnd = ParserState->AmlEnd;

    ParserState->Scope = Scope;
    ParserState->StartOp = RootOp;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsPushScope
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *              Op                  - Current op to be pushed
 *              RemainingArgs       - List of args remaining
 *              ArgCount            - Fixed or variable number of args
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Push current op to begin parsing its argument
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsPushScope (
    ACPI_PARSE_STATE        *ParserState,
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  RemainingArgs,
    UINT32                  ArgCount)
{
    ACPI_GENERIC_STATE      *Scope;


    ACPI_FUNCTION_TRACE_PTR (PsPushScope, Op);


    Scope = AcpiUtCreateGenericState ();
    if (!Scope)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Scope->Common.DescriptorType = ACPI_DESC_TYPE_STATE_PSCOPE;
    Scope->ParseScope.Op = Op;
    Scope->ParseScope.ArgList = RemainingArgs;
    Scope->ParseScope.ArgCount = ArgCount;
    Scope->ParseScope.PkgEnd = ParserState->PkgEnd;

    /* Push onto scope stack */

    AcpiUtPushGenericState (&ParserState->Scope, Scope);

    if (ArgCount == ACPI_VAR_ARGS)
    {
        /* Multiple arguments */

        Scope->ParseScope.ArgEnd = ParserState->PkgEnd;
    }
    else
    {
        /* Single argument */

        Scope->ParseScope.ArgEnd = ACPI_TO_POINTER (ACPI_MAX_PTR);
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsPopScope
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *              Op                  - Where the popped op is returned
 *              ArgList             - Where the popped "next argument" is
 *                                    returned
 *              ArgCount            - Count of objects in ArgList
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Return to parsing a previous op
 *
 ******************************************************************************/

void
AcpiPsPopScope (
    ACPI_PARSE_STATE        *ParserState,
    ACPI_PARSE_OBJECT       **Op,
    UINT32                  *ArgList,
    UINT32                  *ArgCount)
{
    ACPI_GENERIC_STATE      *Scope = ParserState->Scope;


    ACPI_FUNCTION_TRACE (PsPopScope);


    /* Only pop the scope if there is in fact a next scope */

    if (Scope->Common.Next)
    {
        Scope = AcpiUtPopGenericState (&ParserState->Scope);

        /* Return to parsing previous op */

        *Op = Scope->ParseScope.Op;
        *ArgList = Scope->ParseScope.ArgList;
        *ArgCount = Scope->ParseScope.ArgCount;
        ParserState->PkgEnd = Scope->ParseScope.PkgEnd;

        /* All done with this scope state structure */

        AcpiUtDeleteGenericState (Scope);
    }
    else
    {
        /* Empty parse stack, prepare to fetch next opcode */

        *Op = NULL;
        *ArgList = 0;
        *ArgCount = 0;
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
        "Popped Op %p Args %X\n", *Op, *ArgCount));
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsCleanupScope
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Destroy available list, remaining stack levels, and return
 *              root scope
 *
 ******************************************************************************/

void
AcpiPsCleanupScope (
    ACPI_PARSE_STATE        *ParserState)
{
    ACPI_GENERIC_STATE      *Scope;


    ACPI_FUNCTION_TRACE_PTR (PsCleanupScope, ParserState);


    if (!ParserState)
    {
        return_VOID;
    }

    /* Delete anything on the scope stack */

    while (ParserState->Scope)
    {
        Scope = AcpiUtPopGenericState (&ParserState->Scope);
        AcpiUtDeleteGenericState (Scope);
    }

    return_VOID;
}
