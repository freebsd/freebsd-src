/******************************************************************************
 *
 * Module Name: cmeval - Object evaluation
 *              $Revision: 18 $
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

#define __CMEVAL_C__

#include "acpi.h"
#include "acnamesp.h"
#include "acinterp.h"


#define _COMPONENT          MISCELLANEOUS
        MODULE_NAME         ("cmeval")


/****************************************************************************
 *
 * FUNCTION:    AcpiCmEvaluateNumericObject
 *
 * PARAMETERS:  *ObjectName         - Object name to be evaluated
 *              DeviceNode          - Node for the device
 *              *Address            - Where the value is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: evaluates a numeric namespace object for a selected device
 *              and stores results in *Address.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ***************************************************************************/

ACPI_STATUS
AcpiCmEvaluateNumericObject (
    NATIVE_CHAR             *ObjectName,
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_INTEGER            *Address)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("CmEvaluateNumericObject");


    /* Execute the method */

    Status = AcpiNsEvaluateRelative (DeviceNode, ObjectName, NULL, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NOT_FOUND)
        {
            DEBUG_PRINT (ACPI_INFO,
                ("%s on %4.4s was not found\n", ObjectName,
                &DeviceNode->Name));
        }
        else
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("%s on %4.4s failed with status %4.4x\n", ObjectName,
                &DeviceNode->Name,
                AcpiCmFormatException (Status)));
        }

        return_ACPI_STATUS (Status);
    }


    /* Did we get a return object? */

    if (!ObjDesc)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("No object was returned from %s\n", ObjectName));
        return_ACPI_STATUS (AE_TYPE);
    }

    /* Is the return object of the correct type? */

    if (ObjDesc->Common.Type != ACPI_TYPE_NUMBER)
    {
        Status = AE_TYPE;
        DEBUG_PRINT (ACPI_ERROR,
            ("Type returned from %s was not a number: %d \n",
            ObjectName, ObjDesc->Common.Type));
    }
    else
    {
        /*
         * Since the structure is a union, setting any field will set all
         * of the variables in the union
         */
        *Address = ObjDesc->Number.Value;
    }

    /* On exit, we must delete the return object */

    AcpiCmRemoveReference (ObjDesc);

    return_ACPI_STATUS (Status);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiCmExecute_HID
 *
 * PARAMETERS:  DeviceNode          - Node for the device
 *              *Hid                - Where the HID is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes the _HID control method that returns the hardware
 *              ID of the device.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ***************************************************************************/

ACPI_STATUS
AcpiCmExecute_HID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    DEVICE_ID               *Hid)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("CmExecute_HID");


    /* Execute the method */

    Status = AcpiNsEvaluateRelative (DeviceNode,
                                     METHOD_NAME__HID, NULL, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NOT_FOUND)
        {
            DEBUG_PRINT (ACPI_INFO,
                ("_HID on %4.4s was not found\n",
                &DeviceNode->Name));
        }

        else
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("_HID on %4.4s failed with status %4.4x\n",
                &DeviceNode->Name,
                AcpiCmFormatException (Status)));
        }

        return_ACPI_STATUS (Status);
    }

    /* Did we get a return object? */

    if (!ObjDesc)
    {
        DEBUG_PRINT (ACPI_ERROR, ("No object was returned from _HID\n"));
        return_ACPI_STATUS (AE_TYPE);
    }

    /*
     *  A _HID can return either a Number (32 bit compressed EISA ID) or
     *  a string
     */

    if ((ObjDesc->Common.Type != ACPI_TYPE_NUMBER) &&
        (ObjDesc->Common.Type != ACPI_TYPE_STRING))
    {
        Status = AE_TYPE;
        DEBUG_PRINT (ACPI_ERROR,
            ("Type returned from _HID was not a number or string: [0x%X] \n",
            ObjDesc->Common.Type));
    }

    else
    {
        if (ObjDesc->Common.Type == ACPI_TYPE_NUMBER)
        {
            /* Convert the Numeric HID to string */

            AcpiAmlEisaIdToString ((UINT32) ObjDesc->Number.Value, Hid->Buffer);
        }

        else
        {
            /* Copy the String HID from the returned object */

            STRNCPY(Hid->Buffer, ObjDesc->String.Pointer, sizeof(Hid->Buffer));
        }
    }


    /* On exit, we must delete the return object */

    AcpiCmRemoveReference (ObjDesc);

    return_ACPI_STATUS (Status);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiCmExecute_UID
 *
 * PARAMETERS:  DeviceNode          - Node for the device
 *              *Uid                - Where the UID is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes the _UID control method that returns the hardware
 *              ID of the device.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ***************************************************************************/

ACPI_STATUS
AcpiCmExecute_UID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    DEVICE_ID               *Uid)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    /* Execute the method */

    Status = AcpiNsEvaluateRelative (DeviceNode,
                                     METHOD_NAME__UID, NULL, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NOT_FOUND)
        {
            DEBUG_PRINT (ACPI_INFO,
                ("_UID on %4.4s was not found\n",
                &DeviceNode->Name));
        }

        else
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("_UID on %4.4s failed with status %4.4x\n",
                &DeviceNode->Name,
                AcpiCmFormatException (Status)));
        }

        return (Status);
    }

    /* Did we get a return object? */

    if (!ObjDesc)
    {
        DEBUG_PRINT (ACPI_ERROR, ("No object was returned from _UID\n"));
        return (AE_TYPE);
    }

    /*
     *  A _UID can return either a Number (32 bit compressed EISA ID) or
     *  a string
     */

    if ((ObjDesc->Common.Type != ACPI_TYPE_NUMBER) &&
        (ObjDesc->Common.Type != ACPI_TYPE_STRING))
    {
        Status = AE_TYPE;
        DEBUG_PRINT (ACPI_ERROR,
            ("Type returned from _UID was not a number or string: %d \n",
            ObjDesc->Common.Type));
    }

    else
    {
        if (ObjDesc->Common.Type == ACPI_TYPE_NUMBER)
        {
            /* Convert the Numeric UID to string */

            AcpiAmlUnsignedIntegerToString (ObjDesc->Number.Value, Uid->Buffer);
        }

        else
        {
            /* Copy the String UID from the returned object */

            STRNCPY(Uid->Buffer, ObjDesc->String.Pointer, sizeof(Uid->Buffer));
        }
    }


    /* On exit, we must delete the return object */

    AcpiCmRemoveReference (ObjDesc);

    return (Status);
}

/****************************************************************************
 *
 * FUNCTION:    AcpiCmExecute_STA
 *
 * PARAMETERS:  DeviceNode          - Node for the device
 *              *Flags              - Where the status flags are returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes _STA for selected device and stores results in
 *              *Flags.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ***************************************************************************/

ACPI_STATUS
AcpiCmExecute_STA (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    UINT32                  *Flags)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("CmExecute_STA");

    /* Execute the method */

    Status = AcpiNsEvaluateRelative (DeviceNode,
                                     METHOD_NAME__STA, NULL, &ObjDesc);
    if (AE_NOT_FOUND == Status)
    {
        DEBUG_PRINT (ACPI_INFO,
            ("_STA on %4.4s was not found, assuming present.\n",
            &DeviceNode->Name));

        *Flags = 0x0F;
        Status = AE_OK;
    }

    else if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("_STA on %4.4s failed with status %s\n",
            &DeviceNode->Name,
            AcpiCmFormatException (Status)));
    }

    else /* success */
    {
        /* Did we get a return object? */

        if (!ObjDesc)
        {
            DEBUG_PRINT (ACPI_ERROR, ("No object was returned from _STA\n"));
            return_ACPI_STATUS (AE_TYPE);
        }

        /* Is the return object of the correct type? */

        if (ObjDesc->Common.Type != ACPI_TYPE_NUMBER)
        {
            Status = AE_TYPE;
            DEBUG_PRINT (ACPI_ERROR,
                ("Type returned from _STA was not a number: %d \n",
                ObjDesc->Common.Type));
        }

        else
        {
            /* Extract the status flags */

            *Flags = (UINT32) ObjDesc->Number.Value;
        }

        /* On exit, we must delete the return object */

        AcpiCmRemoveReference (ObjDesc);
    }

    return_ACPI_STATUS (Status);
}
