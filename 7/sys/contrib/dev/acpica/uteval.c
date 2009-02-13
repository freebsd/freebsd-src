/******************************************************************************
 *
 * Module Name: uteval - Object evaluation
 *              $Revision: 1.71 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2007, Intel Corp.
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

#define __UTEVAL_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/acinterp.h>


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("uteval")

/* Local prototypes */

static void
AcpiUtCopyIdString (
    char                    *Destination,
    char                    *Source,
    ACPI_SIZE               MaxLength);

static ACPI_STATUS
AcpiUtTranslateOneCid (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_COMPATIBLE_ID      *OneCid);


/*
 * Strings supported by the _OSI predefined (internal) method.
 */
static const char               *AcpiInterfacesSupported[] =
{
    /* Operating System Vendor Strings */

    "Linux",
    "Windows 2000",
    "Windows 2001",
    "Windows 2001 SP0",
    "Windows 2001 SP1",
    "Windows 2001 SP2",
    "Windows 2001 SP3",
    "Windows 2001 SP4",
    "Windows 2001.1",
    "Windows 2001.1 SP1",   /* Added 03/2006 */
    "Windows 2006",         /* Added 03/2006 */

    /* Feature Group Strings */

    "Extended Address Space Descriptor"

    /*
     * All "optional" feature group strings (features that are implemented
     * by the host) should be implemented in the host version of
     * AcpiOsValidateInterface and should not be added here.
     */
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtOsiImplementation
 *
 * PARAMETERS:  WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Implementation of the _OSI predefined control method
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtOsiImplementation (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *StringDesc;
    ACPI_OPERAND_OBJECT     *ReturnDesc;
    ACPI_NATIVE_UINT        i;


    ACPI_FUNCTION_TRACE (UtOsiImplementation);


    /* Validate the string input argument */

    StringDesc = WalkState->Arguments[0].Object;
    if (!StringDesc || (StringDesc->Common.Type != ACPI_TYPE_STRING))
    {
        return_ACPI_STATUS (AE_TYPE);
    }

    /* Create a return object */

    ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
    if (!ReturnDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Default return value is SUPPORTED */

    ReturnDesc->Integer.Value = ACPI_UINT32_MAX;
    WalkState->ReturnDesc = ReturnDesc;

    /* Compare input string to static table of supported interfaces */

    for (i = 0; i < ACPI_ARRAY_LENGTH (AcpiInterfacesSupported); i++)
    {
        if (!ACPI_STRCMP (StringDesc->String.Pointer, AcpiInterfacesSupported[i]))
        {
            /* The interface is supported */

            return_ACPI_STATUS (AE_CTRL_TERMINATE);
        }
    }

    /*
     * Did not match the string in the static table, call the host OSL to
     * check for a match with one of the optional strings (such as
     * "Module Device", "3.0 Thermal Model", etc.)
     */
    Status = AcpiOsValidateInterface (StringDesc->String.Pointer);
    if (ACPI_SUCCESS (Status))
    {
        /* The interface is supported */

        return_ACPI_STATUS (AE_CTRL_TERMINATE);
    }

    /* The interface is not supported */

    ReturnDesc->Integer.Value = 0;
    return_ACPI_STATUS (AE_CTRL_TERMINATE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtEvaluateObject
 *
 * PARAMETERS:  PrefixNode          - Starting node
 *              Path                - Path to object from starting node
 *              ExpectedReturnTypes - Bitmap of allowed return types
 *              ReturnDesc          - Where a return value is stored
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Evaluates a namespace object and verifies the type of the
 *              return object.  Common code that simplifies accessing objects
 *              that have required return objects of fixed types.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtEvaluateObject (
    ACPI_NAMESPACE_NODE     *PrefixNode,
    char                    *Path,
    UINT32                  ExpectedReturnBtypes,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_EVALUATE_INFO      *Info;
    ACPI_STATUS             Status;
    UINT32                  ReturnBtype;


    ACPI_FUNCTION_TRACE (UtEvaluateObject);


    /* Allocate the evaluation information block */

    Info = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_EVALUATE_INFO));
    if (!Info)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Info->PrefixNode = PrefixNode;
    Info->Pathname = Path;
    Info->ParameterType = ACPI_PARAM_ARGS;

    /* Evaluate the object/method */

    Status = AcpiNsEvaluate (Info);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NOT_FOUND)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "[%4.4s.%s] was not found\n",
                AcpiUtGetNodeName (PrefixNode), Path));
        }
        else
        {
            ACPI_ERROR_METHOD ("Method execution failed",
                PrefixNode, Path, Status);
        }

        goto Cleanup;
    }

    /* Did we get a return object? */

    if (!Info->ReturnObject)
    {
        if (ExpectedReturnBtypes)
        {
            ACPI_ERROR_METHOD ("No object was returned from",
                PrefixNode, Path, AE_NOT_EXIST);

            Status = AE_NOT_EXIST;
        }

        goto Cleanup;
    }

    /* Map the return object type to the bitmapped type */

    switch (ACPI_GET_OBJECT_TYPE (Info->ReturnObject))
    {
    case ACPI_TYPE_INTEGER:
        ReturnBtype = ACPI_BTYPE_INTEGER;
        break;

    case ACPI_TYPE_BUFFER:
        ReturnBtype = ACPI_BTYPE_BUFFER;
        break;

    case ACPI_TYPE_STRING:
        ReturnBtype = ACPI_BTYPE_STRING;
        break;

    case ACPI_TYPE_PACKAGE:
        ReturnBtype = ACPI_BTYPE_PACKAGE;
        break;

    default:
        ReturnBtype = 0;
        break;
    }

    if ((AcpiGbl_EnableInterpreterSlack) &&
        (!ExpectedReturnBtypes))
    {
        /*
         * We received a return object, but one was not expected.  This can
         * happen frequently if the "implicit return" feature is enabled.
         * Just delete the return object and return AE_OK.
         */
        AcpiUtRemoveReference (Info->ReturnObject);
        goto Cleanup;
    }

    /* Is the return object one of the expected types? */

    if (!(ExpectedReturnBtypes & ReturnBtype))
    {
        ACPI_ERROR_METHOD ("Return object type is incorrect",
            PrefixNode, Path, AE_TYPE);

        ACPI_ERROR ((AE_INFO,
            "Type returned from %s was incorrect: %s, expected Btypes: %X",
            Path, AcpiUtGetObjectTypeName (Info->ReturnObject),
            ExpectedReturnBtypes));

        /* On error exit, we must delete the return object */

        AcpiUtRemoveReference (Info->ReturnObject);
        Status = AE_TYPE;
        goto Cleanup;
    }

    /* Object type is OK, return it */

    *ReturnDesc = Info->ReturnObject;

Cleanup:
    ACPI_FREE (Info);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtEvaluateNumericObject
 *
 * PARAMETERS:  ObjectName          - Object name to be evaluated
 *              DeviceNode          - Node for the device
 *              Address             - Where the value is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Evaluates a numeric namespace object for a selected device
 *              and stores result in *Address.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtEvaluateNumericObject (
    char                    *ObjectName,
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_INTEGER            *Address)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (UtEvaluateNumericObject);


    Status = AcpiUtEvaluateObject (DeviceNode, ObjectName,
                ACPI_BTYPE_INTEGER, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get the returned Integer */

    *Address = ObjDesc->Integer.Value;

    /* On exit, we must delete the return object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyIdString
 *
 * PARAMETERS:  Destination         - Where to copy the string
 *              Source              - Source string
 *              MaxLength           - Length of the destination buffer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Copies an ID string for the _HID, _CID, and _UID methods.
 *              Performs removal of a leading asterisk if present -- workaround
 *              for a known issue on a bunch of machines.
 *
 ******************************************************************************/

static void
AcpiUtCopyIdString (
    char                    *Destination,
    char                    *Source,
    ACPI_SIZE               MaxLength)
{

    /*
     * Workaround for ID strings that have a leading asterisk. This construct
     * is not allowed by the ACPI specification  (ID strings must be
     * alphanumeric), but enough existing machines have this embedded in their
     * ID strings that the following code is useful.
     */
    if (*Source == '*')
    {
        Source++;
    }

    /* Do the actual copy */

    ACPI_STRNCPY (Destination, Source, MaxLength);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtExecute_HID
 *
 * PARAMETERS:  DeviceNode          - Node for the device
 *              Hid                 - Where the HID is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes the _HID control method that returns the hardware
 *              ID of the device.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtExecute_HID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_DEVICE_ID          *Hid)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (UtExecute_HID);


    Status = AcpiUtEvaluateObject (DeviceNode, METHOD_NAME__HID,
                ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (ACPI_GET_OBJECT_TYPE (ObjDesc) == ACPI_TYPE_INTEGER)
    {
        /* Convert the Numeric HID to string */

        AcpiExEisaIdToString ((UINT32) ObjDesc->Integer.Value, Hid->Value);
    }
    else
    {
        /* Copy the String HID from the returned object */

        AcpiUtCopyIdString (Hid->Value, ObjDesc->String.Pointer,
                sizeof (Hid->Value));
    }

    /* On exit, we must delete the return object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtTranslateOneCid
 *
 * PARAMETERS:  ObjDesc             - _CID object, must be integer or string
 *              OneCid              - Where the CID string is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Return a numeric or string _CID value as a string.
 *              (Compatible ID)
 *
 *              NOTE:  Assumes a maximum _CID string length of
 *                     ACPI_MAX_CID_LENGTH.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtTranslateOneCid (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_COMPATIBLE_ID      *OneCid)
{


    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_INTEGER:

        /* Convert the Numeric CID to string */

        AcpiExEisaIdToString ((UINT32) ObjDesc->Integer.Value, OneCid->Value);
        return (AE_OK);

    case ACPI_TYPE_STRING:

        if (ObjDesc->String.Length > ACPI_MAX_CID_LENGTH)
        {
            return (AE_AML_STRING_LIMIT);
        }

        /* Copy the String CID from the returned object */

        AcpiUtCopyIdString (OneCid->Value, ObjDesc->String.Pointer,
                ACPI_MAX_CID_LENGTH);
        return (AE_OK);

    default:

        return (AE_TYPE);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtExecute_CID
 *
 * PARAMETERS:  DeviceNode          - Node for the device
 *              ReturnCidList       - Where the CID list is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes the _CID control method that returns one or more
 *              compatible hardware IDs for the device.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtExecute_CID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_COMPATIBLE_ID_LIST **ReturnCidList)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;
    UINT32                  Count;
    UINT32                  Size;
    ACPI_COMPATIBLE_ID_LIST *CidList;
    ACPI_NATIVE_UINT        i;


    ACPI_FUNCTION_TRACE (UtExecute_CID);


    /* Evaluate the _CID method for this device */

    Status = AcpiUtEvaluateObject (DeviceNode, METHOD_NAME__CID,
                ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING | ACPI_BTYPE_PACKAGE,
                &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get the number of _CIDs returned */

    Count = 1;
    if (ACPI_GET_OBJECT_TYPE (ObjDesc) == ACPI_TYPE_PACKAGE)
    {
        Count = ObjDesc->Package.Count;
    }

    /* Allocate a worst-case buffer for the _CIDs */

    Size = (((Count - 1) * sizeof (ACPI_COMPATIBLE_ID)) +
                           sizeof (ACPI_COMPATIBLE_ID_LIST));

    CidList = ACPI_ALLOCATE_ZEROED ((ACPI_SIZE) Size);
    if (!CidList)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Init CID list */

    CidList->Count = Count;
    CidList->Size  = Size;

    /*
     *  A _CID can return either a single compatible ID or a package of
     *  compatible IDs.  Each compatible ID can be one of the following:
     *  1) Integer (32 bit compressed EISA ID) or
     *  2) String (PCI ID format, e.g. "PCI\VEN_vvvv&DEV_dddd&SUBSYS_ssssssss")
     */

    /* The _CID object can be either a single CID or a package (list) of CIDs */

    if (ACPI_GET_OBJECT_TYPE (ObjDesc) == ACPI_TYPE_PACKAGE)
    {
        /* Translate each package element */

        for (i = 0; i < Count; i++)
        {
            Status = AcpiUtTranslateOneCid (ObjDesc->Package.Elements[i],
                            &CidList->Id[i]);
            if (ACPI_FAILURE (Status))
            {
                break;
            }
        }
    }
    else
    {
        /* Only one CID, translate to a string */

        Status = AcpiUtTranslateOneCid (ObjDesc, CidList->Id);
    }

    /* Cleanup on error */

    if (ACPI_FAILURE (Status))
    {
        ACPI_FREE (CidList);
    }
    else
    {
        *ReturnCidList = CidList;
    }

    /* On exit, we must delete the _CID return object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtExecute_UID
 *
 * PARAMETERS:  DeviceNode          - Node for the device
 *              Uid                 - Where the UID is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes the _UID control method that returns the hardware
 *              ID of the device.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtExecute_UID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_DEVICE_ID          *Uid)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (UtExecute_UID);


    Status = AcpiUtEvaluateObject (DeviceNode, METHOD_NAME__UID,
                ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (ACPI_GET_OBJECT_TYPE (ObjDesc) == ACPI_TYPE_INTEGER)
    {
        /* Convert the Numeric UID to string */

        AcpiExUnsignedIntegerToString (ObjDesc->Integer.Value, Uid->Value);
    }
    else
    {
        /* Copy the String UID from the returned object */

        AcpiUtCopyIdString (Uid->Value, ObjDesc->String.Pointer,
                sizeof (Uid->Value));
    }

    /* On exit, we must delete the return object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtExecute_STA
 *
 * PARAMETERS:  DeviceNode          - Node for the device
 *              Flags               - Where the status flags are returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes _STA for selected device and stores results in
 *              *Flags.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtExecute_STA (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    UINT32                  *Flags)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (UtExecute_STA);


    Status = AcpiUtEvaluateObject (DeviceNode, METHOD_NAME__STA,
                ACPI_BTYPE_INTEGER, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        if (AE_NOT_FOUND == Status)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
                "_STA on %4.4s was not found, assuming device is present\n",
                AcpiUtGetNodeName (DeviceNode)));

            *Flags = ACPI_UINT32_MAX;
            Status = AE_OK;
        }

        return_ACPI_STATUS (Status);
    }

    /* Extract the status flags */

    *Flags = (UINT32) ObjDesc->Integer.Value;

    /* On exit, we must delete the return object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtExecute_Sxds
 *
 * PARAMETERS:  DeviceNode          - Node for the device
 *              Flags               - Where the status flags are returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes _STA for selected device and stores results in
 *              *Flags.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtExecute_Sxds (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    UINT8                   *Highest)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;
    UINT32                  i;


    ACPI_FUNCTION_TRACE (UtExecute_Sxds);


    for (i = 0; i < 4; i++)
    {
        Highest[i] = 0xFF;
        Status = AcpiUtEvaluateObject (DeviceNode,
                    ACPI_CAST_PTR (char, AcpiGbl_HighestDstateNames[i]),
                    ACPI_BTYPE_INTEGER, &ObjDesc);
        if (ACPI_FAILURE (Status))
        {
            if (Status != AE_NOT_FOUND)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
                    "%s on Device %4.4s, %s\n",
                    ACPI_CAST_PTR (char, AcpiGbl_HighestDstateNames[i]),
                    AcpiUtGetNodeName (DeviceNode),
                    AcpiFormatException (Status)));

                return_ACPI_STATUS (Status);
            }
        }
        else
        {
            /* Extract the Dstate value */

            Highest[i] = (UINT8) ObjDesc->Integer.Value;

            /* Delete the return object */

            AcpiUtRemoveReference (ObjDesc);
        }
    }

    return_ACPI_STATUS (AE_OK);
}
