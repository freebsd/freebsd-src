
/******************************************************************************
 *
 * Module Name: amstore - AML Interpreter object store support
 *              $Revision: 123 $
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

#define __AMSTORE_C__

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "actables.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("amstore")






/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlExecStore
 *
 * PARAMETERS:  *ValDesc            - Value to be stored
 *              *DestDesc           - Where to store it 0 Must be (ACPI_HANDLE)
 *                                    or an ACPI_OPERAND_OBJECT  of type
 *                                    Reference; if the latter the descriptor
 *                                    will be either reused or deleted.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store the value described by ValDesc into the location
 *              described by DestDesc.  Called by various interpreter
 *              functions to store the result of an operation into
 *              the destination operand.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlExecStore (
    ACPI_OPERAND_OBJECT     *ValDesc,
    ACPI_OPERAND_OBJECT     *DestDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *RefDesc = DestDesc;


    FUNCTION_TRACE_PTR ("AmlExecStore", DestDesc);


    /* Validate parameters */

    if (!ValDesc || !DestDesc)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlExecStore: Internal error - null pointer\n"));
        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }

    /* DestDesc can be either a namespace node or an ACPI object */

    if (VALID_DESCRIPTOR_TYPE (DestDesc, ACPI_DESC_TYPE_NAMED))
    {
        /* 
         * Dest is a namespace node,
         * Storing an object into a Name "container" 
         */
        Status = AcpiAmlStoreObjectToNode (ValDesc, 
                    (ACPI_NAMESPACE_NODE *) DestDesc, WalkState);

        /* All done, that's it */

        return_ACPI_STATUS (Status);
    }


    /* Destination object must be an object of type Reference */

    if (DestDesc->Common.Type != INTERNAL_TYPE_REFERENCE)
    {
        /* Destination is not an Reference */

        DEBUG_PRINT (ACPI_ERROR,
            ("AmlExecStore: Destination is not a ReferenceObj [%p]\n", DestDesc));

        DUMP_STACK_ENTRY (ValDesc);
        DUMP_STACK_ENTRY (DestDesc);
        DUMP_OPERANDS (&DestDesc, IMODE_EXECUTE, "AmlExecStore",
                        2, "Target is not a ReferenceObj");

        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
    }


    /* 
     * Examine the Reference opcode.  These cases are handled:
     *
     * 1) Store to Name (Change the object associated with a name)
     * 2) Store to an indexed area of a Buffer or Package
     * 3) Store to a Method Local or Arg
     * 4) Store to the debug object
     * 5) Store to a constant -- a noop
     */

    switch (RefDesc->Reference.OpCode)
    {

    case AML_NAME_OP:

        /* Storing an object into a Name "container" */

        Status = AcpiAmlStoreObjectToNode (ValDesc, RefDesc->Reference.Object,
                        WalkState);
        break;


    case AML_INDEX_OP:

        /* Storing to an Index (pointer into a packager or buffer) */

        Status = AcpiAmlStoreObjectToIndex (ValDesc, RefDesc, WalkState);
        break;


    case AML_LOCAL_OP:

        Status = AcpiDsMethodDataSetValue (MTH_TYPE_LOCAL,
                        (RefDesc->Reference.Offset), ValDesc, WalkState);
        break;


    case AML_ARG_OP:

        Status = AcpiDsMethodDataSetValue (MTH_TYPE_ARG,
                        (RefDesc->Reference.Offset), ValDesc, WalkState);
        break;


    case AML_DEBUG_OP:

        /*
         * Storing to the Debug object causes the value stored to be
         * displayed and otherwise has no effect -- see ACPI Specification
         *
         * TBD: print known object types "prettier".
         */
        DEBUG_PRINT (ACPI_INFO, ("**** Write to Debug Object: ****: \n"));
        
        if (ValDesc->Common.Type == ACPI_TYPE_STRING)
        {
            DEBUG_PRINT (ACPI_INFO, ("%s\n", ValDesc->String.Pointer));
        }
        else
        {
            DUMP_STACK_ENTRY (ValDesc);
        }

        break;


    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:

        /*
         * Storing to a constant is a no-op -- see ACPI Specification
         * Delete the reference descriptor, however
         */
        break;


    default:

        DEBUG_PRINT (ACPI_ERROR,
            ("AmlExecStore: Internal error - Unknown Reference subtype %02x\n",
            RefDesc->Reference.OpCode));

        /* TBD: [Restructure] use object dump routine !! */

        DUMP_BUFFER (RefDesc, sizeof (ACPI_OPERAND_OBJECT));

        Status = AE_AML_INTERNAL;
        break;

    }   /* switch (RefDesc->Reference.OpCode) */



    /* Always delete the reference descriptor object */

    if (RefDesc)
    {
        AcpiCmRemoveReference (RefDesc);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlStoreObjectToIndex
 *
 * PARAMETERS:  *ValDesc            - Value to be stored
 *              *Node           - Named object to recieve the value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store the object to the named object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlStoreObjectToIndex (
    ACPI_OPERAND_OBJECT     *ValDesc,
    ACPI_OPERAND_OBJECT     *DestDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  Length;
    UINT32                  i;
    UINT8                   Value = 0;


    FUNCTION_TRACE ("AcpiAmlStoreObjectToIndex");


    /*
     * Destination must be a reference pointer, and
     * must point to either a buffer or a package
     */

    switch (DestDesc->Reference.TargetType)
    {
    case ACPI_TYPE_PACKAGE:
        /*
         * Storing to a package element is not simple.  The source must be
         * evaluated and converted to the type of the destination and then the
         * source is copied into the destination - we can't just point to the
         * source object.
         */
        if (DestDesc->Reference.TargetType == ACPI_TYPE_PACKAGE)
        {
            /*
             * The object at *(DestDesc->Reference.Where) is the
             *  element within the package that is to be modified.
             */
            ObjDesc = *(DestDesc->Reference.Where);
            if (ObjDesc)
            {
                /*
                 * If the Destination element is a package, we will delete
                 *  that object and construct a new one.
                 *
                 * TBD: [Investigate] Should both the src and dest be required
                 *      to be packages?
                 *       && (ValDesc->Common.Type == ACPI_TYPE_PACKAGE)
                 */
                if (ObjDesc->Common.Type == ACPI_TYPE_PACKAGE)
                {
                    /*
                     * Take away the reference for being part of a package and
                     * delete
                     */
                    AcpiCmRemoveReference (ObjDesc);
                    AcpiCmRemoveReference (ObjDesc);

                    ObjDesc = NULL;
                }
            }

            if (!ObjDesc)
            {
                /*
                 * If the ObjDesc is NULL, it means that an uninitialized package
                 * element has been used as a destination (this is OK), therefore, 
                 * we must create the destination element to match the type of the 
                 * source element NOTE: ValDesc can be of any type.
                 */
                ObjDesc = AcpiCmCreateInternalObject (ValDesc->Common.Type);
                if (!ObjDesc)
                {
                    return_ACPI_STATUS (AE_NO_MEMORY);
                }

                /*
                 * If the source is a package, copy the source to the new dest
                 */
                if (ACPI_TYPE_PACKAGE == ObjDesc->Common.Type)
                {
                    Status = AcpiCmCopyIpackageToIpackage (ValDesc, ObjDesc, WalkState);
                    if (ACPI_FAILURE (Status))
                    {
                        AcpiCmRemoveReference (ObjDesc);
                        return_ACPI_STATUS (Status);
                    }
                }

                /*
                 * Install the new descriptor into the package and add a
                 * reference to the newly created descriptor for now being
                 * part of the parent package
                 */

                *(DestDesc->Reference.Where) = ObjDesc;
                AcpiCmAddReference (ObjDesc);
            }

            if (ACPI_TYPE_PACKAGE != ObjDesc->Common.Type)
            {
                /*
                 * The destination element is not a package, so we need to
                 * convert the contents of the source (ValDesc) and copy into
                 * the destination (ObjDesc)
                 */
                Status = AcpiAmlStoreObjectToObject (ValDesc, ObjDesc,
                                                        WalkState);
                if (ACPI_FAILURE (Status))
                {
                    /*
                     * An error occurrered when copying the internal object
                     * so delete the reference.
                     */
                    DEBUG_PRINT (ACPI_ERROR,
                        ("AmlExecStore/Index: Unable to copy the internal object\n"));
                    return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
                }
            }
        }
        break;


    case ACPI_TYPE_BUFFER_FIELD:
        /*
         * Storing into a buffer at a location defined by an Index.
         *
         * Each 8-bit element of the source object is written to the
         * 8-bit Buffer Field of the Index destination object.
         */

        /*
         * Set the ObjDesc to the destination object and type check.
         */
        ObjDesc = DestDesc->Reference.Object;
        if (ObjDesc->Common.Type != ACPI_TYPE_BUFFER)
        {
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        /*
         * The assignment of the individual elements will be slightly
         * different for each source type.
         */

        switch (ValDesc->Common.Type)
        {
        /*
         * If the type is Integer, assign bytewise
         * This loop to assign each of the elements is somewhat
         * backward because of the Big Endian-ness of IA-64
         */
        case ACPI_TYPE_INTEGER:
            Length = sizeof (ACPI_INTEGER);
            for (i = Length; i != 0; i--)
            {
                Value = (UINT8)(ValDesc->Integer.Value >> (MUL_8 (i - 1)));
                ObjDesc->Buffer.Pointer[DestDesc->Reference.Offset] = Value;
            }
            break;

        /*
         * If the type is Buffer, the Length is in the structure.
         * Just loop through the elements and assign each one in turn.
         */
        case ACPI_TYPE_BUFFER:
            Length = ValDesc->Buffer.Length;
            for (i = 0; i < Length; i++)
            {
                Value = *(ValDesc->Buffer.Pointer + i);
                ObjDesc->Buffer.Pointer[DestDesc->Reference.Offset] = Value;
            }
            break;

        /*
         * If the type is String, the Length is in the structure.
         * Just loop through the elements and assign each one in turn.
         */
        case ACPI_TYPE_STRING:
            Length = ValDesc->String.Length;
            for (i = 0; i < Length; i++)
            {
                Value = *(ValDesc->String.Pointer + i);
                ObjDesc->Buffer.Pointer[DestDesc->Reference.Offset] = Value;
            }
            break;

        /*
         * If source is not a valid type so return an error.
         */
        default:
            DEBUG_PRINT (ACPI_ERROR,
                ("AmlExecStore/Index: Source must be Number/Buffer/String type, not %X\n",
                ValDesc->Common.Type));
            Status = AE_AML_OPERAND_TYPE;
            break;
        }
        break;


    default:
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlExecStoreIndex: Target is not a Package or BufferField\n"));
        Status = AE_AML_OPERAND_TYPE;
        break;
    }


    return_ACPI_STATUS (Status);
}





/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlStoreObjectToNode
 *
 * PARAMETERS:  *SourceDesc            - Value to be stored
 *              *Node               - Named object to recieve the value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store the object to the named object.
 *
 *              The Assignment of an object to a named object is handled here
 *              The val passed in will replace the current value (if any)
 *              with the input value.
 *
 *              When storing into an object the data is converted to the
 *              target object type then stored in the object.  This means
 *              that the target object type (for an initialized target) will
 *              not be changed by a store operation.
 *
 *              NOTE: the global lock is acquired early.  This will result
 *              in the global lock being held a bit longer.  Also, if the
 *              function fails during set up we may get the lock when we
 *              don't really need it.  I don't think we care.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlStoreObjectToNode (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *TargetDesc;
    OBJECT_TYPE_INTERNAL    TargetType = ACPI_TYPE_ANY;


    FUNCTION_TRACE ("AmlStoreObjectToNode");

    /*
     * Assuming the parameters were already validated
     */
    ACPI_ASSERT((Node) && (SourceDesc));


    /*
     * Get current type of the node, and object attached to Node
     */
    TargetType = AcpiNsGetType (Node);
    TargetDesc = AcpiNsGetAttachedObject (Node);

    DEBUG_PRINT (ACPI_INFO, ("AmlStoreObjectToNode: Storing %p(%s) into node %p(%s)\n",
        Node, AcpiCmGetTypeName (SourceDesc->Common.Type),
        SourceDesc, AcpiCmGetTypeName (TargetType)));


    /*
     * Resolve the source object to an actual value
     * (If it is a reference object) 
     */
    Status = AcpiAmlResolveObject (&SourceDesc, TargetType, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }


    /*
     * Do the actual store operation 
     */
    switch (TargetType)
    {
    case INTERNAL_TYPE_DEF_FIELD:

        /* Raw data copy for target types Integer/String/Buffer */

        Status = AcpiAmlCopyDataToNamedField (SourceDesc, Node);
        break;


    case ACPI_TYPE_INTEGER:
    case ACPI_TYPE_STRING:
    case ACPI_TYPE_BUFFER:
    case INTERNAL_TYPE_BANK_FIELD:
    case INTERNAL_TYPE_INDEX_FIELD:
    case ACPI_TYPE_FIELD_UNIT:

        /* 
         * These target types are all of type Integer/String/Buffer, and
         * therefore support implicit conversion before the store.
         * 
         * Copy and/or convert the source object to a new target object 
         */
        Status = AcpiAmlStoreObject (SourceDesc, TargetType, &TargetDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /*
         * Store the new TargetDesc as the new value of the Name, and set
         * the Name's type to that of the value being stored in it.
         * SourceDesc reference count is incremented by AttachObject.
         */
        Status = AcpiNsAttachObject (Node, TargetDesc, TargetType);
        DEBUG_PRINT (ACPI_INFO,
            ("AmlStoreObjectToNode: Store %s into %s via Convert/Attach\n",
            AcpiCmGetTypeName (TargetDesc->Common.Type),
            AcpiCmGetTypeName (TargetType)));
        break;
    

    default:

        /* No conversions for all other types.  Just attach the source object */

        Status = AcpiNsAttachObject (Node, SourceDesc, SourceDesc->Common.Type);
        
        DEBUG_PRINT (ACPI_INFO,
            ("AmlStoreObjectToNode: Store %s into %s via Attach only\n",
            AcpiCmGetTypeName (SourceDesc->Common.Type),
            AcpiCmGetTypeName (SourceDesc->Common.Type)));
        break;
    }


    return_ACPI_STATUS (Status);
}



/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlStoreObjectToObject
 *
 * PARAMETERS:  *SourceDesc            - Value to be stored
 *              *DestDesc           - Object to receive the value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store an object to another object.
 *
 *              The Assignment of an object to another (not named) object
 *              is handled here.
 *              The val passed in will replace the current value (if any)
 *              with the input value.
 *
 *              When storing into an object the data is converted to the
 *              target object type then stored in the object.  This means
 *              that the target object type (for an initialized target) will
 *              not be changed by a store operation.
 *
 *              This module allows destination types of Number, String,
 *              and Buffer.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlStoreObjectToObject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *DestDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    OBJECT_TYPE_INTERNAL    DestinationType = DestDesc->Common.Type;


    FUNCTION_TRACE ("AmlStoreObjectToObject");


    /*
     *  Assuming the parameters are valid!
     */
    ACPI_ASSERT((DestDesc) && (SourceDesc));

    DEBUG_PRINT (ACPI_INFO, ("AmlStoreObjectToObject: Storing %p(%s) to %p(%s)\n",
                    SourceDesc, AcpiCmGetTypeName (SourceDesc->Common.Type),
                    DestDesc, AcpiCmGetTypeName (DestDesc->Common.Type)));


    /*
     * From this interface, we only support Integers/Strings/Buffers
     */
    switch (DestinationType)
    {
    case ACPI_TYPE_INTEGER:
    case ACPI_TYPE_STRING:
    case ACPI_TYPE_BUFFER:
        break;

    default:
        DEBUG_PRINT (ACPI_WARN,
            ("AmlStoreObjectToObject: Store into %s not implemented\n",
            AcpiCmGetTypeName (DestDesc->Common.Type)));

        return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
    }



    /*
     * Resolve the source object to an actual value
     * (If it is a reference object) 
     */
    Status = AcpiAmlResolveObject (&SourceDesc, DestinationType, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }


    /*
     * Copy and/or convert the source object to the destination object 
     */
    Status = AcpiAmlStoreObject (SourceDesc, DestinationType, &DestDesc, WalkState);


    return_ACPI_STATUS (Status);
}

