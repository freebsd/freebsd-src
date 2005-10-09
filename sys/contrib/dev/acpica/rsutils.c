/*******************************************************************************
 *
 * Module Name: rsutils - Utilities for the resource manager
 *              $Revision: 39 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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


#define __RSUTILS_C__

#include "acpi.h"
#include "acnamesp.h"
#include "acresrc.h"


#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rsutils")


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetPrtMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              RetBuffer       - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _PRT value of an object
 *              contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsGetPrtMethodData (
    ACPI_HANDLE             Handle,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsGetPrtMethodData");


    /* Parameters guaranteed valid by caller */

    /*
     * Execute the method, no parameters
     */
    Status = AcpiUtEvaluateObject (Handle, "_PRT", ACPI_BTYPE_PACKAGE, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Create a resource linked list from the byte stream buffer that comes
     * back from the _CRS method execution.
     */
    Status = AcpiRsCreatePciRoutingTable (ObjDesc, RetBuffer);

    /* On exit, we must delete the object returned by EvaluateObject */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetCrsMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              RetBuffer       - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _CRS value of an object
 *              contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsGetCrsMethodData (
    ACPI_HANDLE             Handle,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsGetCrsMethodData");


    /* Parameters guaranteed valid by caller */

    /*
     * Execute the method, no parameters
     */
    Status = AcpiUtEvaluateObject (Handle, "_CRS", ACPI_BTYPE_BUFFER, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Make the call to create a resource linked list from the
     * byte stream buffer that comes back from the _CRS method
     * execution.
     */
    Status = AcpiRsCreateResourceList (ObjDesc, RetBuffer);

    /* On exit, we must delete the object returned by evaluateObject */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetPrsMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              RetBuffer       - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _PRS value of an object
 *              contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsGetPrsMethodData (
    ACPI_HANDLE             Handle,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsGetPrsMethodData");


    /* Parameters guaranteed valid by caller */

    /*
     * Execute the method, no parameters
     */
    Status = AcpiUtEvaluateObject (Handle, "_PRS", ACPI_BTYPE_BUFFER, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Make the call to create a resource linked list from the
     * byte stream buffer that comes back from the _CRS method
     * execution.
     */
    Status = AcpiRsCreateResourceList (ObjDesc, RetBuffer);

    /* On exit, we must delete the object returned by evaluateObject */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              RetBuffer       - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _CRS or _PRS value of an
 *              object contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsGetMethodData (
    ACPI_HANDLE             Handle,
    char                    *Path,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsGetMethodData");


    /* Parameters guaranteed valid by caller */

    /*
     * Execute the method, no parameters
     */
    Status = AcpiUtEvaluateObject (Handle, Path, ACPI_BTYPE_BUFFER, &ObjDesc);
    if (ACPI_FAILURE (Status)) {
        return_ACPI_STATUS (Status);
    }

    /*
     * Make the call to create a resource linked list from the
     * byte stream buffer that comes back from the method
     * execution.
     */
    Status = AcpiRsCreateResourceList (ObjDesc, RetBuffer);

    /* On exit, we must delete the object returned by EvaluateObject */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiRsSetSrsMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              InBuffer        - a pointer to a buffer structure of the
 *                                  parameter
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to set the _SRS of an object contained
 *              in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsSetSrsMethodData (
    ACPI_HANDLE             Handle,
    ACPI_BUFFER             *InBuffer)
{
    ACPI_PARAMETER_INFO     Info;
    ACPI_OPERAND_OBJECT     *Params[2];
    ACPI_STATUS             Status;
    ACPI_BUFFER             Buffer;


    ACPI_FUNCTION_TRACE ("RsSetSrsMethodData");


    /* Parameters guaranteed valid by caller */

    /*
     * The InBuffer parameter will point to a linked list of
     * resource parameters.  It needs to be formatted into a
     * byte stream to be sent in as an input parameter to _SRS
     *
     * Convert the linked list into a byte stream
     */
    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiRsCreateByteStream (InBuffer->Pointer, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Init the param object
     */
    Params[0] = AcpiUtCreateInternalObject (ACPI_TYPE_BUFFER);
    if (!Params[0])
    {
        AcpiOsFree (Buffer.Pointer);
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /*
     * Set up the parameter object
     */
    Params[0]->Buffer.Length  = (UINT32) Buffer.Length;
    Params[0]->Buffer.Pointer = Buffer.Pointer;
    Params[0]->Common.Flags   = AOPOBJ_DATA_VALID;
    Params[1] = NULL;

    Info.Node = Handle;
    Info.Parameters = Params;
    Info.ParameterType = ACPI_PARAM_ARGS;

    /*
     * Execute the method, no return value
     */
    Status = AcpiNsEvaluateRelative ("_SRS", &Info);

    /*
     * Clean up and return the status from AcpiNsEvaluateRelative
     */
    AcpiUtRemoveReference (Params[0]);
    return_ACPI_STATUS (Status);
}

