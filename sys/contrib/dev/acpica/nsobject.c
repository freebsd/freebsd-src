/*******************************************************************************
 *
 * Module Name: nsobject - Utilities for objects attached to namespace
 *                         table entries
 *              $Revision: 67 $
 *
 ******************************************************************************/

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


#define __NSOBJECT_C__

#include "acpi.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "actables.h"


#define _COMPONENT          ACPI_NAMESPACE
        MODULE_NAME         ("nsobject")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsAttachObject
 *
 * PARAMETERS:  Node                - Parent Node
 *              Object              - Object to be attached
 *              Type                - Type of object, or ACPI_TYPE_ANY if not
 *                                    known
 *
 * DESCRIPTION: Record the given object as the value associated with the
 *              name whose ACPI_HANDLE is passed.  If Object is NULL
 *              and Type is ACPI_TYPE_ANY, set the name as having no value.
 *
 * MUTEX:       Assumes namespace is locked
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsAttachObject (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_OPERAND_OBJECT     *Object,
    ACPI_OBJECT_TYPE8       Type)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *PreviousObjDesc;
    ACPI_OBJECT_TYPE8       ObjType = ACPI_TYPE_ANY;
    UINT8                   Flags;


    FUNCTION_TRACE ("NsAttachObject");


    /*
     * Parameter validation
     */
    if (!AcpiGbl_RootNode)
    {
        /* Name space not initialized  */

        REPORT_ERROR (("NsAttachObject: Namespace not initialized\n"));
        return_ACPI_STATUS (AE_NO_NAMESPACE);
    }

    if (!Node)
    {
        /* Invalid handle */

        REPORT_ERROR (("NsAttachObject: Null NamedObj handle\n"));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (!Object && (ACPI_TYPE_ANY != Type))
    {
        /* Null object */

        REPORT_ERROR (("NsAttachObject: Null object, but type not ACPI_TYPE_ANY\n"));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (!VALID_DESCRIPTOR_TYPE (Node, ACPI_DESC_TYPE_NAMED))
    {
        /* Not a name handle */

        REPORT_ERROR (("NsAttachObject: Invalid handle\n"));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Check if this object is already attached */

    if (Node->Object == Object)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Obj %p already installed in NameObj %p\n",
            Object, Node));

        return_ACPI_STATUS (AE_OK);
    }


    /* Get the current flags field of the Node */

    Flags = Node->Flags;
    Flags &= ~ANOBJ_AML_ATTACHMENT;


    /* If null object, we will just install it */

    if (!Object)
    {
        ObjDesc = NULL;
        ObjType = ACPI_TYPE_ANY;
    }

    /*
     * If the source object is a namespace Node with an attached object,
     * we will use that (attached) object
     */
    else if (VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_NAMED) &&
            ((ACPI_NAMESPACE_NODE *) Object)->Object)
    {
        /*
         * Value passed is a name handle and that name has a
         * non-null value.  Use that name's value and type.
         */
        ObjDesc = ((ACPI_NAMESPACE_NODE *) Object)->Object;
        ObjType = ((ACPI_NAMESPACE_NODE *) Object)->Type;

        /*
         * Copy appropriate flags
         */
        if (((ACPI_NAMESPACE_NODE *) Object)->Flags & ANOBJ_AML_ATTACHMENT)
        {
            Flags |= ANOBJ_AML_ATTACHMENT;
        }
    }


    /*
     * Otherwise, we will use the parameter object, but we must type
     * it first
     */
    else
    {
        ObjDesc = (ACPI_OPERAND_OBJECT  *) Object;

        /* If a valid type (non-ANY) was given, just use it */

        if (ACPI_TYPE_ANY != Type)
        {
            ObjType = Type;
        }

        else
        {
            /*
             * Cannot figure out the type -- set to DefAny which
             * will print as an error in the name table dump
             */
            if (AcpiDbgLevel > 0)
            {
                DUMP_PATHNAME (Node,
                    "NsAttachObject confused: setting bogus type for  ",
                    ACPI_LV_INFO, _COMPONENT);

                if (VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_NAMED))
                {
                    DUMP_PATHNAME (Object, "name ", ACPI_LV_INFO, _COMPONENT);
                }

                else
                {
                    DUMP_PATHNAME (Object, "object ", ACPI_LV_INFO, _COMPONENT);
                    DUMP_STACK_ENTRY (Object);
                }
            }

            ObjType = INTERNAL_TYPE_DEF_ANY;
        }
    }


    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Installing %p into Node %p [%4.4s]\n",
        ObjDesc, Node, (char*)&Node->Name));


    /*
     * Must increment the new value's reference count
     * (if it is an internal object)
     */
    AcpiUtAddReference (ObjDesc);

    /* Save the existing object (if any) for deletion later */

    PreviousObjDesc = Node->Object;

    /* Install the object and set the type, flags */

    Node->Object   = ObjDesc;
    Node->Type     = (UINT8) ObjType;
    Node->Flags    |= Flags;


    /*
     * Delete an existing attached object.
     */
    if (PreviousObjDesc)
    {
        /* One for the attach to the Node */

        AcpiUtRemoveReference (PreviousObjDesc);

        /* Now delete */

        AcpiUtRemoveReference (PreviousObjDesc);
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsDetachObject
 *
 * PARAMETERS:  Node           - An object whose Value will be deleted
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete the Value associated with a namespace object.  If the
 *              Value is an allocated object, it is freed.  Otherwise, the
 *              field is simply cleared.
 *
 ******************************************************************************/

void
AcpiNsDetachObject (
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;


    FUNCTION_TRACE ("NsDetachObject");


    ObjDesc = Node->Object;
    if (!ObjDesc)
    {
        return_VOID;
    }

    /* Clear the entry in all cases */

    Node->Object = NULL;

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Object=%p Value=%p Name %4.4s\n",
        Node, ObjDesc, (char*)&Node->Name));

    /* Remove one reference on the object (and all subobjects) */

    AcpiUtRemoveReference (ObjDesc);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetAttachedObject
 *
 * PARAMETERS:  Node             - Parent Node to be examined
 *
 * RETURN:      Current value of the object field from the Node whose
 *              handle is passed
 *
 ******************************************************************************/

void *
AcpiNsGetAttachedObject (
    ACPI_NAMESPACE_NODE     *Node)
{
    FUNCTION_TRACE_PTR ("NsGetAttachedObject", Node);


    if (!Node)
    {
        /* handle invalid */

        ACPI_DEBUG_PRINT ((ACPI_DB_WARN, "Null Node ptr\n"));
        return_PTR (NULL);
    }

    return_PTR (Node->Object);
}


