/******************************************************************************
 *
 * Module Name: nsinit - namespace initialization
 *              $Revision: 4 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
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


#define __NSXFINIT_C__

#include "acpi.h"
#include "acnamesp.h"
#include "acdispat.h"

#define _COMPONENT          NAMESPACE
        MODULE_NAME         ("nsinit")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsInitializeObjects
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk the entire namespace and perform any necessary
 *              initialization on the objects found therein
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsInitializeObjects (
    void)
{
    ACPI_STATUS             Status;
    ACPI_INIT_WALK_INFO     Info;


    FUNCTION_TRACE ("NsInitializeObjects");


    DEBUG_PRINT (TRACE_DISPATCH,
        ("NsInitializeObjects: **** Starting initialization of namespace objects ****\n"));
    DEBUG_PRINT_RAW (ACPI_OK, ("Completing Region and Field initialization:"));


    Info.FieldCount = 0;
    Info.FieldInit = 0;
    Info.OpRegionCount = 0;
    Info.OpRegionInit = 0;
    Info.ObjectCount = 0;


    /* Walk entire namespace from the supplied root */

    Status = AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
                                ACPI_UINT32_MAX, AcpiNsInitOneObject,
                                &Info, NULL);
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("NsInitializeObjects: WalkNamespace failed! %x\n", Status));
    }

    DEBUG_PRINT_RAW (ACPI_OK,
        ("\n%d/%d Regions, %d/%d Fields initialized (%d nodes total)\n",
        Info.OpRegionInit, Info.OpRegionCount, Info.FieldInit, Info.FieldCount, Info.ObjectCount));
    DEBUG_PRINT (TRACE_DISPATCH,
        ("NsInitializeObjects: %d Control Methods found\n", Info.MethodCount));
    DEBUG_PRINT (TRACE_DISPATCH,
        ("NsInitializeObjects: %d Op Regions found\n", Info.OpRegionCount));

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsInitializeDevices
 *
 * PARAMETERS:  None
 *
 * RETURN:      ACPI_STATUS
 *
 * DESCRIPTION: Walk the entire namespace and initialize all ACPI devices.
 *              This means running _INI on all present devices.
 *
 *              Also: Install PCI config space handler for all PCI root bridges.
 *              A PCI root bridge is found by searching for devices containing
 *              a HID with the value EISAID("PNP0A03")
 *
 *****************************************************************************/

ACPI_STATUS
AcpiNsInitializeDevices (
    UINT32                  Flags)
{
    ACPI_STATUS             Status;
    ACPI_DEVICE_WALK_INFO   Info;


    FUNCTION_TRACE ("NsInitializeDevices");


    Info.Flags = Flags;
    Info.DeviceCount = 0;
    Info.Num_STA = 0;
    Info.Num_INI = 0;
    Info.Num_HID = 0;
    Info.Num_PCI = 0;


    DEBUG_PRINT_RAW (ACPI_OK, ("Executing device _INI methods:"));

    Status = AcpiNsWalkNamespace (ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                        FALSE, AcpiNsInitOneDevice, &Info, NULL);

    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("NsInitializeDevices: WalkNamespace failed! %x\n", Status));
    }


    DEBUG_PRINT_RAW (ACPI_OK,
        ("\n%d Devices found: %d _STA, %d _INI, %d _HID, %d PCIRoot\n",
        Info.DeviceCount, Info.Num_STA, Info.Num_INI,
        Info.Num_HID, Info.Num_PCI));

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsInitOneObject
 *
 * PARAMETERS:  ObjHandle       - Node
 *              Level           - Current nesting level
 *              Context         - Points to a init info struct
 *              ReturnValue     - Not used
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Callback from AcpiWalkNamespace.  Invoked for every object
 *              within the  namespace.
 *
 *              Currently, the only objects that require initialization are:
 *              1) Methods
 *              2) Op Regions
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsInitOneObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    OBJECT_TYPE_INTERNAL    Type;
    ACPI_STATUS             Status;
    ACPI_INIT_WALK_INFO     *Info = (ACPI_INIT_WALK_INFO *) Context;
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    Info->ObjectCount++;


    /* And even then, we are only interested in a few object types */

    Type = AcpiNsGetType (ObjHandle);
    ObjDesc = Node->Object;
    if (!ObjDesc)
    {
        return (AE_OK);
    }

    switch (Type)
    {

    case ACPI_TYPE_REGION:

        Info->OpRegionCount++;
        if (ObjDesc->Common.Flags & AOPOBJ_DATA_VALID)
        {
            break;
        }

        Info->OpRegionInit++;
        Status = AcpiDsGetRegionArguments (ObjDesc);
        DEBUG_PRINT_RAW (ACPI_OK, ("."));
        break;


    case ACPI_TYPE_FIELD_UNIT:

        Info->FieldCount++;
        if (ObjDesc->Common.Flags & AOPOBJ_DATA_VALID)
        {
            break;
        }

        Info->FieldInit++;
        Status = AcpiDsGetFieldUnitArguments (ObjDesc);
        DEBUG_PRINT_RAW (ACPI_OK, ("."));

        break;

    default:
        break;
    }

    /*
     * We ignore errors from above, and always return OK, since
     * we don't want to abort the walk on a single error.
     */
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsInitOneDevice
 *
 * PARAMETERS:  The usual "I'm a namespace callback" stuff
 *
 * RETURN:      ACPI_STATUS
 *
 * DESCRIPTION: This is called once per device soon after ACPI is enabled
 *              to initialize each device. It determines if the device is
 *              present, and if so, calls _INI.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiNsInitOneDevice (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT    *RetObj = NULL;
    ACPI_NAMESPACE_NODE    *Node;
    UINT32                  Flags;
    ACPI_DEVICE_WALK_INFO  *Info = (ACPI_DEVICE_WALK_INFO *) Context;


    FUNCTION_TRACE ("AcpiNsInitOneDevice");


    DEBUG_PRINT_RAW (ACPI_OK, ("."));
    Info->DeviceCount++;

    AcpiCmAcquireMutex (ACPI_MTX_NAMESPACE);

    Node = AcpiNsConvertHandleToEntry (ObjHandle);
    if (!Node)
    {
        AcpiCmReleaseMutex (ACPI_MTX_NAMESPACE);
        return (AE_BAD_PARAMETER);
    }

    AcpiCmReleaseMutex (ACPI_MTX_NAMESPACE);

    /*
     * Run _STA to determine if we can run _INI on the device.
     */

    Status = AcpiCmExecute_STA (Node, &Flags);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Info->Num_STA++;

    if (!(Flags & 0x01))
    {
        /* don't look at children of a not present device */
        return_ACPI_STATUS(AE_CTRL_DEPTH);
    }

    /*
     * The device is present. Run _INI.
     */

    Status = AcpiNsEvaluateRelative (ObjHandle, "_INI", NULL, NULL);
    if (AE_NOT_FOUND == Status)
    {
         /* No _INI means device requires no initialization */
    }

    else if (ACPI_FAILURE (Status))
    {
#ifdef ACPI_DEBUG
        NATIVE_CHAR *ScopeName = AcpiNsGetTablePathname (ObjHandle);

        DEBUG_PRINT (ACPI_ERROR, ("%s._INI failed: %s\n",
                ScopeName, AcpiCmFormatException (Status)));

        AcpiCmFree (ScopeName);
#endif
        return_ACPI_STATUS (Status);
    }

    else
    {
        Info->Num_INI++;
    }


    /*
     * Examine the HID of the device.  _HID can be an executable
     * control method -- it simply has to return a string or number
     * containing the HID.
     */

    if (RetObj)
    {
        AcpiCmRemoveReference (RetObj);
    }

    RetObj = NULL;
    Status = AcpiNsEvaluateRelative (ObjHandle, "_HID", NULL, &RetObj);
    if (AE_NOT_FOUND == Status)
    {
         /* No _HID --> Can't be a PCI root bridge */

        return_ACPI_STATUS (AE_OK);
    }

    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Info->Num_HID++;


    /*
     * Found an _HID object.
     * Check for a PCI Root Bridge.  We need to install the PCI_CONFIG space
     * handler on all PCI Root Bridges found within the namespace
     *
     * A PCI Root Bridge has an HID with the value EISAID("PNP0A03")
     * The HID can be either a number or a string.
     */

    switch (RetObj->Common.Type)
    {
    case ACPI_TYPE_NUMBER:

        if (RetObj->Number.Value != PCI_ROOT_HID_VALUE)
        {
            goto Cleanup;
        }

        break;

    case ACPI_TYPE_STRING:

        if (STRNCMP (RetObj->String.Pointer, PCI_ROOT_HID_STRING,
                     sizeof (PCI_ROOT_HID_STRING)))
        {
            goto Cleanup;
        }

        break;

    default:

        goto Cleanup;
    }


    /*
     * We found a valid PCI_ROOT_HID.
     * The parent of the HID entry is the PCI device;  Install the default PCI
     * handler for this PCI device.
     */

    Info->Num_PCI++;

    if (!(Info->Flags & ACPI_NO_PCI_INIT))
    {
        Status = AcpiInstallAddressSpaceHandler (ObjHandle,
                                                 ADDRESS_SPACE_PCI_CONFIG,
                                                 ACPI_DEFAULT_HANDLER, NULL, NULL);
    }

Cleanup:

    if (RetObj)
    {
        AcpiCmRemoveReference (RetObj);
    }

    return_ACPI_STATUS (AE_OK);
}


