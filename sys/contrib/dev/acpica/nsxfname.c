/******************************************************************************
 *
 * Module Name: nsxfname - Public interfaces to the ACPI subsystem
 *                         ACPI Namespace oriented interfaces
 *              $Revision: 79 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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

#define __NSXFNAME_C__

#include "acpi.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "amlcode.h"
#include "acparser.h"
#include "acdispat.h"
#include "acevents.h"


#define _COMPONENT          ACPI_NAMESPACE
        MODULE_NAME         ("nsxfname")


/****************************************************************************
 *
 * FUNCTION:    AcpiGetHandle
 *
 * PARAMETERS:  Parent          - Object to search under (search scope).
 *              PathName        - Pointer to an asciiz string containing the
 *                                  name
 *              RetHandle       - Where the return handle is placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This routine will search for a caller specified name in the
 *              name space.  The caller can restrict the search region by
 *              specifying a non NULL parent.  The parent value is itself a
 *              namespace handle.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetHandle (
    ACPI_HANDLE             Parent,
    ACPI_STRING             Pathname,
    ACPI_HANDLE             *RetHandle)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node = NULL;
    ACPI_NAMESPACE_NODE     *PrefixNode = NULL;


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Parameter Validation */

    if (!RetHandle || !Pathname)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Convert a parent handle to a prefix node */

    if (Parent)
    {
        AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);

        PrefixNode = AcpiNsConvertHandleToEntry (Parent);
        if (!PrefixNode)
        {
            AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
            return (AE_BAD_PARAMETER);
        }

        AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    }

    /* Special case for root, since we can't search for it */

    if (STRCMP (Pathname, NS_ROOT_PATH) == 0)
    {
        *RetHandle = AcpiNsConvertEntryToHandle (AcpiGbl_RootNode);
        return (AE_OK);
    }

    /*
     *  Find the Node and convert to a handle
     */
    Status = AcpiNsGetNode (Pathname, PrefixNode, &Node);

    *RetHandle = NULL;
    if (ACPI_SUCCESS (Status))
    {
        *RetHandle = AcpiNsConvertEntryToHandle (Node);
    }

    return (Status);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiGetPathname
 *
 * PARAMETERS:  Handle          - Handle to be converted to a pathname
 *              NameType        - Full pathname or single segment
 *              RetPathPtr      - Buffer for returned path
 *
 * RETURN:      Pointer to a string containing the fully qualified Name.
 *
 * DESCRIPTION: This routine returns the fully qualified name associated with
 *              the Handle parameter.  This and the AcpiPathnameToHandle are
 *              complementary functions.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetName (
    ACPI_HANDLE             Handle,
    UINT32                  NameType,
    ACPI_BUFFER             *RetPathPtr)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Buffer pointer must be valid always */

    if (!RetPathPtr || (NameType > ACPI_NAME_TYPE_MAX))
    {
        return (AE_BAD_PARAMETER);
    }

    /* Allow length to be zero and ignore the pointer */

    if ((RetPathPtr->Length) &&
       (!RetPathPtr->Pointer))
    {
        return (AE_BAD_PARAMETER);
    }

    if (NameType == ACPI_FULL_PATHNAME)
    {
        /* Get the full pathname (From the namespace root) */

        Status = AcpiNsHandleToPathname (Handle, &RetPathPtr->Length,
                                        RetPathPtr->Pointer);
        return (Status);
    }

    /*
     * Wants the single segment ACPI name.
     * Validate handle and convert to an Node
     */

    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    Node = AcpiNsConvertHandleToEntry (Handle);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Check if name will fit in buffer */

    if (RetPathPtr->Length < PATH_SEGMENT_LENGTH)
    {
        RetPathPtr->Length = PATH_SEGMENT_LENGTH;
        Status = AE_BUFFER_OVERFLOW;
        goto UnlockAndExit;
    }

    /* Just copy the ACPI name from the Node and zero terminate it */

    STRNCPY (RetPathPtr->Pointer, (NATIVE_CHAR *) &Node->Name,
                ACPI_NAME_SIZE);
    ((NATIVE_CHAR *) RetPathPtr->Pointer) [ACPI_NAME_SIZE] = 0;
    Status = AE_OK;


UnlockAndExit:

    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (Status);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiGetObjectInfo
 *
 * PARAMETERS:  Handle          - Object Handle
 *              Info            - Where the info is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Returns information about an object as gleaned from the
 *              namespace node and possibly by running several standard
 *              control methods (Such as in the case of a device.)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetObjectInfo (
    ACPI_HANDLE             Handle,
    ACPI_DEVICE_INFO        *Info)
{
    ACPI_DEVICE_ID          Hid;
    ACPI_DEVICE_ID          Uid;
    ACPI_STATUS             Status;
    UINT32                  DeviceStatus = 0;
    ACPI_INTEGER            Address = 0;
    ACPI_NAMESPACE_NODE     *Node;


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Parameter validation */

    if (!Handle || !Info)
    {
        return (AE_BAD_PARAMETER);
    }

    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);

    Node = AcpiNsConvertHandleToEntry (Handle);
    if (!Node)
    {
        AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
        return (AE_BAD_PARAMETER);
    }

    Info->Type      = Node->Type;
    Info->Name      = Node->Name;

    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);

    /*
     * If not a device, we are all done.
     */
    if (Info->Type != ACPI_TYPE_DEVICE)
    {
        return (AE_OK);
    }


    /*
     * Get extra info for ACPI devices only.  Run the
     * _HID, _UID, _STA, and _ADR methods.  Note: none
     * of these methods are required, so they may or may
     * not be present.  The Info->Valid bits are used
     * to indicate which methods ran successfully.
     */

    Info->Valid = 0;

    /* Execute the _HID method and save the result */

    Status = AcpiUtExecute_HID (Node, &Hid);
    if (ACPI_SUCCESS (Status))
    {
        STRNCPY (Info->HardwareId, Hid.Buffer, sizeof(Info->HardwareId));

        Info->Valid |= ACPI_VALID_HID;
    }

    /* Execute the _UID method and save the result */

    Status = AcpiUtExecute_UID (Node, &Uid);
    if (ACPI_SUCCESS (Status))
    {
        STRCPY (Info->UniqueId, Uid.Buffer);

        Info->Valid |= ACPI_VALID_UID;
    }

    /*
     * Execute the _STA method and save the result
     * _STA is not always present
     */

    Status = AcpiUtExecute_STA (Node, &DeviceStatus);
    if (ACPI_SUCCESS (Status))
    {
        Info->CurrentStatus = DeviceStatus;
        Info->Valid |= ACPI_VALID_STA;
    }

    /*
     * Execute the _ADR method and save result if successful
     * _ADR is not always present
     */

    Status = AcpiUtEvaluateNumericObject (METHOD_NAME__ADR,
                                            Node, &Address);

    if (ACPI_SUCCESS (Status))
    {
        Info->Address = Address;
        Info->Valid |= ACPI_VALID_ADR;
    }

    return (AE_OK);
}

