
/******************************************************************************
 *
 * Module Name: exstoren - AML Interpreter object store support,
 *                        Store to Node (namespace object)
 *              $Revision: 38 $
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

#define __EXSTOREN_C__

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "actables.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exstoren")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExResolveObject
 *
 * PARAMETERS:  SourceDescPtr       - Pointer to the source object
 *              TargetType          - Current type of the target
 *              WalkState           - Current walk state
 *
 * RETURN:      Status, resolved object in SourceDescPtr.
 *
 * DESCRIPTION: Resolve an object.  If the object is a reference, dereference
 *              it and return the actual object in the SourceDescPtr.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExResolveObject (
    ACPI_OPERAND_OBJECT     **SourceDescPtr,
    ACPI_OBJECT_TYPE8       TargetType,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *SourceDesc = *SourceDescPtr;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("ExResolveObject");


    /*
     * Ensure we have a Source that can be stored in the target
     */
    switch (TargetType)
    {

    /* This case handles the "interchangeable" types Integer, String, and Buffer. */

    /*
     * These cases all require only Integers or values that
     * can be converted to Integers (Strings or Buffers)
     */
    case ACPI_TYPE_BUFFER_FIELD:
    case INTERNAL_TYPE_REGION_FIELD:
    case INTERNAL_TYPE_BANK_FIELD:
    case INTERNAL_TYPE_INDEX_FIELD:

    /*
     * Stores into a Field/Region or into a Buffer/String
     * are all essentially the same.
     */
    case ACPI_TYPE_INTEGER:
    case ACPI_TYPE_STRING:
    case ACPI_TYPE_BUFFER:


        /* TBD: FIX - check for source==REF, resolve, then check type */

        /*
         * If SourceDesc is not a valid type, try to resolve it to one.
         */
        if ((SourceDesc->Common.Type != ACPI_TYPE_INTEGER)     &&
            (SourceDesc->Common.Type != ACPI_TYPE_BUFFER)      &&
            (SourceDesc->Common.Type != ACPI_TYPE_STRING))
        {
            /*
             * Initially not a valid type, convert
             */
            Status = AcpiExResolveToValue (SourceDescPtr, WalkState);
            if (ACPI_SUCCESS (Status) &&
                (SourceDesc->Common.Type != ACPI_TYPE_INTEGER)     &&
                (SourceDesc->Common.Type != ACPI_TYPE_BUFFER)      &&
                (SourceDesc->Common.Type != ACPI_TYPE_STRING))
            {
                /*
                 * Conversion successful but still not a valid type
                 */
                DEBUG_PRINTP (ACPI_ERROR,
                    ("Cannot assign type %s to %s (must be type Int/Str/Buf)\n",
                    AcpiUtGetTypeName ((*SourceDescPtr)->Common.Type),
                    AcpiUtGetTypeName (TargetType)));
                Status = AE_AML_OPERAND_TYPE;
            }
        }
        break;


    case INTERNAL_TYPE_ALIAS:

        /*
         * Aliases are resolved by AcpiExPrepOperands
         */
        DEBUG_PRINTP (ACPI_WARN, ("Store into Alias - should never happen\n"));
        Status = AE_AML_INTERNAL;
        break;


    case ACPI_TYPE_PACKAGE:
    default:

        /*
         * All other types than Alias and the various Fields come here,
         * including the untyped case - ACPI_TYPE_ANY.
         */
        break;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExStoreObject
 *
 * PARAMETERS:  SourceDesc          - Object to store
 *              TargetType          - Current type of the target
 *              TargetDescPtr       - Pointer to the target
 *              WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: "Store" an object to another object.  This may include
 *              converting the source type to the target type (implicit
 *              conversion), and a copy of the value of the source to
 *              the target.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExStoreObject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OBJECT_TYPE8       TargetType,
    ACPI_OPERAND_OBJECT     **TargetDescPtr,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *TargetDesc = *TargetDescPtr;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("ExStoreObject");


    /*
     * Perform the "implicit conversion" of the source to the current type
     * of the target - As per the ACPI specification.
     *
     * If no conversion performed, SourceDesc is left alone, otherwise it
     * is updated with a new object.
     */
    Status = AcpiExConvertToTargetType (TargetType, &SourceDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * We now have two objects of identical types, and we can perform a
     * copy of the *value* of the source object.
     */
    switch (TargetType)
    {
    case ACPI_TYPE_ANY:
    case INTERNAL_TYPE_DEF_ANY:

        /*
         * The target namespace node is uninitialized (has no target object),
         * and will take on the type of the source object
         */

        *TargetDescPtr = SourceDesc;
        break;


    case ACPI_TYPE_INTEGER:

        TargetDesc->Integer.Value = SourceDesc->Integer.Value;

        /* Truncate value if we are executing from a 32-bit ACPI table */

        AcpiExTruncateFor32bitTable (TargetDesc, WalkState);
        break;

    case ACPI_TYPE_STRING:

        Status = AcpiExCopyStringToString (SourceDesc, TargetDesc);
        break;


    case ACPI_TYPE_BUFFER:

        Status = AcpiExCopyBufferToBuffer (SourceDesc, TargetDesc);
        break;


    case ACPI_TYPE_PACKAGE:

        /*
         * TBD: [Unhandled] Not real sure what to do here
         */
        Status = AE_NOT_IMPLEMENTED;
        break;


    default:

        /*
         * All other types come here.
         */
        DEBUG_PRINTP (ACPI_WARN, ("Store into type %s not implemented\n",
            AcpiUtGetTypeName (TargetType)));

        Status = AE_NOT_IMPLEMENTED;
        break;
    }


    return_ACPI_STATUS (Status);
}


