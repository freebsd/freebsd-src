/******************************************************************************
 *
 * Module Name: nsinit - namespace initialization
 *              $Revision: 50 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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


#define __NSXFINIT_C__

#include "acpi.h"
#include "acnamesp.h"
#include "acdispat.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsinit")


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


    ACPI_FUNCTION_TRACE ("NsInitializeObjects");


    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "**** Starting initialization of namespace objects ****\n"));
    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK, "Completing Region/Field/Buffer/Package initialization:"));

    /* Set all init info to zero */

    ACPI_MEMSET (&Info, 0, sizeof (ACPI_INIT_WALK_INFO));

    /* Walk entire namespace from the supplied root */

    Status = AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
                                ACPI_UINT32_MAX, AcpiNsInitOneObject,
                                &Info, NULL);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "WalkNamespace failed! %s\n",
            AcpiFormatException (Status)));
    }

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
        "\nInitialized %hd/%hd Regions %hd/%hd Fields %hd/%hd Buffers %hd/%hd Packages (%hd nodes)\n",
        Info.OpRegionInit,  Info.OpRegionCount,
        Info.FieldInit,     Info.FieldCount,
        Info.BufferInit,    Info.BufferCount,
        Info.PackageInit,   Info.PackageCount, Info.ObjectCount));
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "%hd Control Methods found\n", Info.MethodCount));
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "%hd Op Regions found\n", Info.OpRegionCount));

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
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
 *              Note: We install PCI config space handler on region access,
 *              not here.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsInitializeDevices (
    void)
{
    ACPI_STATUS             Status;
    ACPI_DEVICE_WALK_INFO   Info;


    ACPI_FUNCTION_TRACE ("NsInitializeDevices");


    /* Init counters */

    Info.DeviceCount = 0;
    Info.Num_STA = 0;
    Info.Num_INI = 0;

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK, "Executing all Device _STA and_INI methods:"));

    /* Walk namespace for all objects of type Device */

    Status = AcpiNsWalkNamespace (ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
                    ACPI_UINT32_MAX, FALSE, AcpiNsInitOneDevice, &Info, NULL);

    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "WalkNamespace failed! %s\n",
            AcpiFormatException (Status)));
    }

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
        "\n%hd Devices found containing: %hd _STA, %hd _INI methods\n",
        Info.DeviceCount, Info.Num_STA, Info.Num_INI));

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
    ACPI_OBJECT_TYPE        Type;
    ACPI_STATUS             Status;
    ACPI_INIT_WALK_INFO     *Info = (ACPI_INIT_WALK_INFO *) Context;
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_NAME ("NsInitOneObject");


    Info->ObjectCount++;

    /* And even then, we are only interested in a few object types */

    Type = AcpiNsGetType (ObjHandle);
    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (!ObjDesc)
    {
        return (AE_OK);
    }

    /* Increment counters for object types we are looking for */

    switch (Type)
    {
    case ACPI_TYPE_REGION:
        Info->OpRegionCount++;
        break;

    case ACPI_TYPE_BUFFER_FIELD:
        Info->FieldCount++;
        break;

    case ACPI_TYPE_BUFFER:
        Info->BufferCount++;
        break;

    case ACPI_TYPE_PACKAGE:
        Info->PackageCount++;
        break;

    default:

        /* No init required, just exit now */
        return (AE_OK);
    }

    /*
     * If the object is already initialized, nothing else to do
     */
    if (ObjDesc->Common.Flags & AOPOBJ_DATA_VALID)
    {
        return (AE_OK);
    }

    /*
     * Must lock the interpreter before executing AML code
     */
    Status = AcpiExEnterInterpreter ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /*
     * Each of these types can contain executable AML code within
     * the declaration.
     */
    switch (Type)
    {
    case ACPI_TYPE_REGION:

        Info->OpRegionInit++;
        Status = AcpiDsGetRegionArguments (ObjDesc);
        break;


    case ACPI_TYPE_BUFFER_FIELD:

        Info->FieldInit++;
        Status = AcpiDsGetBufferFieldArguments (ObjDesc);
        break;


    case ACPI_TYPE_BUFFER:

        Info->BufferInit++;
        Status = AcpiDsGetBufferArguments (ObjDesc);
        break;


    case ACPI_TYPE_PACKAGE:

        Info->PackageInit++;
        Status = AcpiDsGetPackageArguments (ObjDesc);
        break;

    default:
        /* No other types can get here */
        break;
    }

    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_ERROR, "\n"));
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Could not execute arguments for [%4.4s] (%s), %s\n",
                Node->Name.Ascii, AcpiUtGetTypeName (Type), AcpiFormatException (Status)));
    }

    if (!(AcpiDbgLevel & ACPI_LV_INIT))
    {
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK, "."));
    }

    /*
     * We ignore errors from above, and always return OK, since
     * we don't want to abort the walk on any single error.
     */
    AcpiExExitInterpreter ();
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsInitOneDevice
 *
 * PARAMETERS:  ACPI_WALK_CALLBACK
 *
 * RETURN:      ACPI_STATUS
 *
 * DESCRIPTION: This is called once per device soon after ACPI is enabled
 *              to initialize each device. It determines if the device is
 *              present, and if so, calls _INI.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsInitOneDevice (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE    *Node;
    UINT32                  Flags;
    ACPI_DEVICE_WALK_INFO  *Info = (ACPI_DEVICE_WALK_INFO *) Context;


    ACPI_FUNCTION_TRACE ("NsInitOneDevice");


    if ((AcpiDbgLevel <= ACPI_LV_ALL_EXCEPTIONS) && (!(AcpiDbgLevel & ACPI_LV_INFO)))
    {
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK, "."));
    }

    Info->DeviceCount++;

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Node = AcpiNsMapHandleToNode (ObjHandle);
    if (!Node)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Run _STA to determine if we can run _INI on the device.
     */
    ACPI_DEBUG_EXEC (AcpiUtDisplayInitPathname (Node, "_STA  [Method]"));
    Status = AcpiUtExecute_STA (Node, &Flags);
    if (ACPI_FAILURE (Status))
    {
        /* Ignore error and move on to next device */

        return_ACPI_STATUS (AE_OK);
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
    ACPI_DEBUG_EXEC (AcpiUtDisplayInitPathname (ObjHandle, "_INI  [Method]"));
    Status = AcpiNsEvaluateRelative (ObjHandle, "_INI", NULL, NULL);
    if (ACPI_FAILURE (Status))
    {
        /* No _INI (AE_NOT_FOUND) means device requires no initialization */

        if (Status != AE_NOT_FOUND)
        {
            /* Ignore error and move on to next device */

    #ifdef ACPI_DEBUG_OUTPUT
            NATIVE_CHAR *ScopeName = AcpiNsGetExternalPathname (ObjHandle);

            ACPI_DEBUG_PRINT ((ACPI_DB_WARN, "%s._INI failed: %s\n",
                    ScopeName, AcpiFormatException (Status)));

            ACPI_MEM_FREE (ScopeName);
    #endif
        }

        Status = AE_OK;
    }
    else
    {
        /* Count of successful INIs */

        Info->Num_INI++;
    }

    if (AcpiGbl_InitHandler)
    {
        /* External initialization handler is present, call it */

        Status = AcpiGbl_InitHandler (ObjHandle, ACPI_INIT_DEVICE_INI);
    }


    return_ACPI_STATUS (Status);
}
