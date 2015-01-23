/******************************************************************************
 *
 * Module Name: aslmaputils - Utilities for the resource descriptor/device maps
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acapps.h>
#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/acinterp.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/amlcode.h>

/* This module used for application-level code only */

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmaputils")


/*******************************************************************************
 *
 * FUNCTION:    MpGetHidFromParseTree
 *
 * PARAMETERS:  HidNode             - Node for a _HID object
 *
 * RETURN:      An _HID string value. Automatically converts _HID integers
 *              to strings. Never NULL.
 *
 * DESCRIPTION: Extract a _HID value from the parse tree, not the namespace.
 *              Used when a fully initialized namespace is not available.
 *
 ******************************************************************************/

char *
MpGetHidFromParseTree (
    ACPI_NAMESPACE_NODE     *HidNode)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_PARSE_OBJECT       *Arg;
    char                    *HidString;


    Op = HidNode->Op;

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_NAME:

        Arg = Op->Asl.Child;  /* Get the NameSeg/NameString node */
        Arg = Arg->Asl.Next;  /* First peer is the object to be associated with the name */

        switch (Arg->Asl.ParseOpcode)
        {
        case PARSEOP_STRING_LITERAL:

            return (Arg->Asl.Value.String);

        case PARSEOP_INTEGER:

            /* Convert EISAID to a string */

            HidString = UtStringCacheCalloc (ACPI_EISAID_STRING_SIZE);
            AcpiExEisaIdToString (HidString, Arg->Asl.Value.Integer);
            return (HidString);

        default:

            return ("UNKNOWN");
        }

    default:
        return ("-No HID-");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    MpGetHidValue
 *
 * PARAMETERS:  DeviceNode          - Node for parent device
 *
 * RETURN:      An _HID string value. Automatically converts _HID integers
 *              to strings. Never NULL.
 *
 * DESCRIPTION: Extract _HID value from within a device scope. Does not
 *              actually execute a method, just gets the string or integer
 *              value for the _HID.
 *
 ******************************************************************************/

char *
MpGetHidValue (
    ACPI_NAMESPACE_NODE     *DeviceNode)
{
    ACPI_NAMESPACE_NODE     *HidNode;
    char                    *HidString;
    ACPI_STATUS             Status;


    Status = AcpiNsGetNode (DeviceNode, METHOD_NAME__HID,
        ACPI_NS_NO_UPSEARCH, &HidNode);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    /* If only partial namespace, get the _HID from the parse tree */

    if (!HidNode->Object)
    {
        return (MpGetHidFromParseTree (HidNode));
    }

    /* Handle the different _HID flavors */

    switch (HidNode->Type)
    {
    case ACPI_TYPE_STRING:

        return (HidNode->Object->String.Pointer);

    case ACPI_TYPE_INTEGER:

        /* Convert EISAID to a string */

        HidString = UtStringCacheCalloc (ACPI_EISAID_STRING_SIZE);
        AcpiExEisaIdToString (HidString, HidNode->Object->Integer.Value);
        return (HidString);

    case ACPI_TYPE_METHOD:

        return ("-Method-");

    default:

        FlPrintFile (ASL_FILE_MAP_OUTPUT, "BAD HID TYPE: %u", HidNode->Type);
        break;
    }


ErrorExit:
    return ("-No HID-");
}


/*******************************************************************************
 *
 * FUNCTION:    MpGetHidViaNamestring
 *
 * PARAMETERS:  DeviceName          - Namepath for parent device
 *
 * RETURN:      _HID string. Never NULL.
 *
 * DESCRIPTION: Get a _HID value via a device pathname (instead of just simply
 *              a device node.)
 *
 ******************************************************************************/

char *
MpGetHidViaNamestring (
    char                    *DeviceName)
{
    ACPI_NAMESPACE_NODE     *DeviceNode;
    ACPI_STATUS             Status;


    Status = AcpiNsGetNode (NULL, DeviceName, ACPI_NS_NO_UPSEARCH,
        &DeviceNode);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    return (MpGetHidValue (DeviceNode));


ErrorExit:
    return ("-No HID-");
}


/*******************************************************************************
 *
 * FUNCTION:    MpGetParentDeviceHid
 *
 * PARAMETERS:  Op                      - Parse Op to be examined
 *              TargetNode              - Where the field node is returned
 *              ParentDeviceName        - Where the node path is returned
 *
 * RETURN:      _HID string. Never NULL.
 *
 * DESCRIPTION: Find the parent Device or Scope Op, get the full pathname to
 *              the parent, and get the _HID associated with the parent.
 *
 ******************************************************************************/

char *
MpGetParentDeviceHid (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     **TargetNode,
    char                    **ParentDeviceName)
{
    ACPI_NAMESPACE_NODE     *DeviceNode;


    /* Find parent Device() or Scope() Op */

    while (Op &&
        (Op->Asl.AmlOpcode != AML_DEVICE_OP) &&
        (Op->Asl.AmlOpcode != AML_SCOPE_OP))
    {
        Op = Op->Asl.Parent;
    }

    if (!Op)
    {
        FlPrintFile (ASL_FILE_MAP_OUTPUT, " No_Parent_Device ");
        goto ErrorExit;
    }

    /* Get the full pathname to the device and the _HID */

    DeviceNode = Op->Asl.Node;
    if (!DeviceNode)
    {
        FlPrintFile (ASL_FILE_MAP_OUTPUT, " No_Device_Node ");
        goto ErrorExit;
    }

    *ParentDeviceName = AcpiNsGetExternalPathname (DeviceNode);
    return (MpGetHidValue (DeviceNode));


ErrorExit:
    return ("-No HID-");
}


/*******************************************************************************
 *
 * FUNCTION:    MpGetDdnValue
 *
 * PARAMETERS:  DeviceName          - Namepath for parent device
 *
 * RETURN:      _DDN description string. NULL on failure.
 *
 * DESCRIPTION: Execute the _DDN method for the device.
 *
 ******************************************************************************/

char *
MpGetDdnValue (
    char                    *DeviceName)
{
    ACPI_NAMESPACE_NODE     *DeviceNode;
    ACPI_NAMESPACE_NODE     *DdnNode;
    ACPI_STATUS             Status;


    Status = AcpiNsGetNode (NULL, DeviceName, ACPI_NS_NO_UPSEARCH,
        &DeviceNode);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    Status = AcpiNsGetNode (DeviceNode, METHOD_NAME__DDN, ACPI_NS_NO_UPSEARCH,
        &DdnNode);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    if ((DdnNode->Type != ACPI_TYPE_STRING) ||
        !DdnNode->Object)
    {
        goto ErrorExit;
    }

    return (DdnNode->Object->String.Pointer);


ErrorExit:
    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    MpGetConnectionInfo
 *
 * PARAMETERS:  Op                      - Parse Op to be examined
 *              PinIndex                - Index into GPIO PinList
 *              TargetNode              - Where the field node is returned
 *              TargetName              - Where the node path is returned
 *
 * RETURN:      A substitute _HID string, indicating that the name is actually
 *              a field. NULL if the Op does not refer to a Connection.
 *
 * DESCRIPTION: Get the Field Unit that corresponds to the PinIndex after
 *              a Connection() invocation.
 *
 ******************************************************************************/

char *
MpGetConnectionInfo (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  PinIndex,
    ACPI_NAMESPACE_NODE     **TargetNode,
    char                    **TargetName)
{
    ACPI_PARSE_OBJECT       *NextOp;
    UINT32                  i;


    /*
     * Handle Connection() here. Find the next named FieldUnit.
     * Note: we look at the ParseOpcode for the compiler, look
     * at the AmlOpcode for the disassembler.
     */
    if ((Op->Asl.AmlOpcode == AML_INT_CONNECTION_OP) ||
        (Op->Asl.ParseOpcode == PARSEOP_CONNECTION))
    {
        /* Find the correct field unit definition */

        NextOp = Op;
        for (i = 0; i <= PinIndex;)
        {
            NextOp = NextOp->Asl.Next;
            while (NextOp &&
                (NextOp->Asl.ParseOpcode != PARSEOP_NAMESEG) &&
                (NextOp->Asl.AmlOpcode != AML_INT_NAMEDFIELD_OP))
            {
                NextOp = NextOp->Asl.Next;
            }

            if (!NextOp)
            {
                return ("UNKNOWN");
            }

            /* Add length of this field to the current pin index */

            if (NextOp->Asl.ParseOpcode == PARSEOP_NAMESEG)
            {
                i += (UINT32) NextOp->Asl.Child->Asl.Value.Integer;
            }
            else /* AML_INT_NAMEDFIELD_OP */
            {
                i += (UINT32) NextOp->Asl.Value.Integer;
            }
        }

        /* Return the node and pathname for the field unit */

        *TargetNode = NextOp->Asl.Node;
        *TargetName = AcpiNsGetExternalPathname (*TargetNode);
        return ("-Field-");
    }

    return (NULL);
}
