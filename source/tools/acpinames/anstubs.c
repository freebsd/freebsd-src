/******************************************************************************
 *
 * Module Name: anstubs - Stub routines for the AcpiNames utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#include "acpinames.h"

#include <acutils.h>
#include <acevents.h>
#include <acdispat.h>

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("anstubs")


/******************************************************************************
 *
 * DESCRIPTION: Stubs used to facilitate linkage of the NsDump utility.
 *
 *****************************************************************************/


/* Utilities */

ACPI_STATUS
AcpiUtCopyIobjectToEobject (
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_BUFFER             *RetBuffer)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiUtCopyEobjectToIobject (
    ACPI_OBJECT             *Obj,
    ACPI_OPERAND_OBJECT     **InternalObj)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiUtCopyIobjectToIobject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     **DestDesc,
    ACPI_WALK_STATE         *WalkState)
{
    return (AE_NOT_IMPLEMENTED);
}


/* Event manager */

ACPI_STATUS
AcpiEvInstallRegionHandlers (
    void)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiEvInitializeOpRegions (
    void)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiEvInitializeRegion (
    ACPI_OPERAND_OBJECT     *RegionObj,
    BOOLEAN                 AcpiNsLocked)
{
    return (AE_OK);
}


/* AML Interpreter */

ACPI_STATUS
AcpiExReadDataFromField (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **RetBufferDesc)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiExWriteDataToField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiExPrepFieldValue (
    ACPI_CREATE_FIELD_INFO  *Info)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiExStoreObjectToNode (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_WALK_STATE         *WalkState,
    UINT8                   ImplicitConversion)
{
    return (AE_NOT_IMPLEMENTED);
}


/* Namespace manager */

ACPI_STATUS
AcpiNsEvaluate (
    ACPI_EVALUATE_INFO      *Info)
{
    return (AE_NOT_IMPLEMENTED);
}

void
AcpiNsExecModuleCodeList (
    void)
{
}

void
AcpiExDoDebugObject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    UINT32                  Level,
    UINT32                  Index)
{
    return;
}

void
AcpiExStartTraceMethod (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    return;
}

void
AcpiExStopTraceMethod (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    return;
}

void
AcpiExStartTraceOpcode (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    return;
}

void
AcpiExStopTraceOpcode (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)

{
    return;
}

void
AcpiExTracePoint (
    ACPI_TRACE_EVENT_TYPE   Type,
    BOOLEAN                 Begin,
    UINT8                   *Aml,
    char                    *Pathname)
{
    return;
}


/* Dispatcher */

ACPI_STATUS
AcpiDsInitializeObjects (
    UINT32                  TableIndex,
    ACPI_NAMESPACE_NODE     *StartNode)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsCallControlMethod (
    ACPI_THREAD_STATE       *Thread,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiDsRestartControlMethod (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ReturnDesc)
{
    return (AE_NOT_IMPLEMENTED);
}

void
AcpiDsTerminateControlMethod (
    ACPI_OPERAND_OBJECT     *MethodDesc,
    ACPI_WALK_STATE         *WalkState)
{
}

ACPI_STATUS
AcpiDsMethodError (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiDsBeginMethodExecution (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiDsGetPredicateValue (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ResultObj)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiDsGetBufferFieldArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsGetBankFieldArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsGetRegionArguments (
    ACPI_OPERAND_OBJECT     *RgnDesc)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsGetBufferArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsGetPackageArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsExecBeginOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiDsExecEndOp (
    ACPI_WALK_STATE         *State)
{
    return (AE_NOT_IMPLEMENTED);
}
