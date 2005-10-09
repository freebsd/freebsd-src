/******************************************************************************
 *
 * Module Name: psxface - Parser external interfaces
 *              $Revision: 75 $
 *
 *****************************************************************************/

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

#define __PSXFACE_C__

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_PARSER
        ACPI_MODULE_NAME    ("psxface")


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsxExecute
 *
 * PARAMETERS:  Info->Node          - A method object containing both the AML
 *                                    address and length.
 *              **Params            - List of parameters to pass to method,
 *                                    terminated by NULL. Params itself may be
 *                                    NULL if no parameters are being passed.
 *              **ReturnObjDesc     - Return object from execution of the
 *                                    method.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a control method
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsxExecute (
    ACPI_PARAMETER_INFO     *Info)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  i;
    ACPI_PARSE_OBJECT       *Op;
    ACPI_WALK_STATE         *WalkState;


    ACPI_FUNCTION_TRACE ("PsxExecute");


    /* Validate the Node and get the attached object */

    if (!Info || !Info->Node)
    {
        return_ACPI_STATUS (AE_NULL_ENTRY);
    }

    ObjDesc = AcpiNsGetAttachedObject (Info->Node);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NULL_OBJECT);
    }

    /* Init for new method, wait on concurrency semaphore */

    Status = AcpiDsBeginMethodExecution (Info->Node, ObjDesc, NULL);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if ((Info->ParameterType == ACPI_PARAM_ARGS) &&
        (Info->Parameters))
    {
        /*
         * The caller "owns" the parameters, so give each one an extra
         * reference
         */
        for (i = 0; Info->Parameters[i]; i++)
        {
            AcpiUtAddReference (Info->Parameters[i]);
        }
    }

    /*
     * 1) Perform the first pass parse of the method to enter any
     * named objects that it creates into the namespace
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
        "**** Begin Method Parse **** Entry=%p obj=%p\n",
        Info->Node, ObjDesc));

    /* Create and init a Root Node */

    Op = AcpiPsCreateScopeOp ();
    if (!Op)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup1;
    }

    /*
     * Get a new OwnerId for objects created by this method.  Namespace
     * objects (such as Operation Regions) can be created during the
     * first pass parse.
     */
    ObjDesc->Method.OwningId = AcpiUtAllocateOwnerId (ACPI_OWNER_TYPE_METHOD);

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (ObjDesc->Method.OwningId,
                                    NULL, NULL, NULL);
    if (!WalkState)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup2;
    }

    Status = AcpiDsInitAmlWalk (WalkState, Op, Info->Node,
                    ObjDesc->Method.AmlStart,
                    ObjDesc->Method.AmlLength, NULL, 1);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup3;
    }

    /* Parse the AML */

    Status = AcpiPsParseAml (WalkState);
    AcpiPsDeleteParseTree (Op);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup1; /* Walk state is already deleted */
    }

    /*
     * 2) Execute the method.  Performs second pass parse simultaneously
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
        "**** Begin Method Execution **** Entry=%p obj=%p\n",
        Info->Node, ObjDesc));

    /* Create and init a Root Node */

    Op = AcpiPsCreateScopeOp ();
    if (!Op)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup1;
    }

    /* Init new op with the method name and pointer back to the NS node */

    AcpiPsSetName (Op, Info->Node->Name.Integer);
    Op->Common.Node = Info->Node;

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    if (!WalkState)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup2;
    }

    Status = AcpiDsInitAmlWalk (WalkState, Op, Info->Node,
                    ObjDesc->Method.AmlStart,
                    ObjDesc->Method.AmlLength, Info, 3);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup3;
    }

    /*
     * The walk of the parse tree is where we actually execute the method
     */
    Status = AcpiPsParseAml (WalkState);
    goto Cleanup2; /* Walk state already deleted */


Cleanup3:
    AcpiDsDeleteWalkState (WalkState);

Cleanup2:
    AcpiPsDeleteParseTree (Op);

Cleanup1:
    if ((Info->ParameterType == ACPI_PARAM_ARGS) &&
        (Info->Parameters))
    {
        /* Take away the extra reference that we gave the parameters above */

        for (i = 0; Info->Parameters[i]; i++)
        {
            /* Ignore errors, just do them all */

            (void) AcpiUtUpdateObjectReference (Info->Parameters[i], REF_DECREMENT);
        }
    }

    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * If the method has returned an object, signal this to the caller with
     * a control exception code
     */
    if (Info->ReturnObject)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "Method returned ObjDesc=%p\n",
            Info->ReturnObject));
        ACPI_DUMP_STACK_ENTRY (Info->ReturnObject);

        Status = AE_CTRL_RETURN_VALUE;
    }

    return_ACPI_STATUS (Status);
}


