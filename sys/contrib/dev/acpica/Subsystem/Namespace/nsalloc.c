/*******************************************************************************
 *
 * Module Name: nsalloc - Namespace allocation and deletion utilities
 *              $Revision: 41 $
 *
 ******************************************************************************/

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


#define __NSALLOC_C__

#include "acpi.h"
#include "acnamesp.h"
#include "acinterp.h"


#define _COMPONENT          NAMESPACE
        MODULE_NAME         ("nsalloc")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCreateNode
 *
 * PARAMETERS:
 *
 * RETURN:      None
 *
 * DESCRIPTION:
 *
 ******************************************************************************/

ACPI_NAMESPACE_NODE *
AcpiNsCreateNode (
    UINT32                  AcpiName)
{
    ACPI_NAMESPACE_NODE     *Node;

    FUNCTION_TRACE ("NsCreateNode");


    Node = AcpiCmCallocate (sizeof (ACPI_NAMESPACE_NODE));
    if (!Node)
    {
        return_PTR (NULL);
    }

    INCREMENT_NAME_TABLE_METRICS (sizeof (ACPI_NAMESPACE_NODE));

    Node->DataType       = ACPI_DESC_TYPE_NAMED;
    Node->Name           = AcpiName;
    Node->ReferenceCount = 1;

    return_PTR (Node);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsDeleteNode
 *
 * PARAMETERS:
 *
 * RETURN:      None
 *
 * DESCRIPTION:
 *
 ******************************************************************************/

void
AcpiNsDeleteNode (
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_NAMESPACE_NODE     *ParentNode;
    ACPI_NAMESPACE_NODE     *PrevNode;
    ACPI_NAMESPACE_NODE     *NextNode;


    FUNCTION_TRACE_PTR ("NsDeleteNode", Node);

    ParentNode = AcpiNsGetParentObject (Node);

    PrevNode = NULL;
    NextNode = ParentNode->Child;

    while (NextNode != Node)
    {
        PrevNode = NextNode;
        NextNode = PrevNode->Peer;
    }

    if (PrevNode)
    {
        PrevNode->Peer = NextNode->Peer;
        if (NextNode->Flags & ANOBJ_END_OF_PEER_LIST)
        {
            PrevNode->Flags |= ANOBJ_END_OF_PEER_LIST;
        }
    }
    else
    {
        ParentNode->Child = NextNode->Peer;
    }


    DECREMENT_NAME_TABLE_METRICS (sizeof (ACPI_NAMESPACE_NODE));

    /*
     * Detach an object if there is one
     */

    if (Node->Object)
    {
        AcpiNsDetachObject (Node);
    }

    AcpiCmFree (Node);


    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsInstallNode
 *
 * PARAMETERS:  WalkState       - Current state of the walk
 *              ParentNode      - The parent of the new Node
 *              Node        - The new Node to install
 *              Type            - ACPI object type of the new Node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialize a new entry within a namespace table.
 *
 ******************************************************************************/

void
AcpiNsInstallNode (
    ACPI_WALK_STATE         *WalkState,
    ACPI_NAMESPACE_NODE     *ParentNode,    /* Parent */
    ACPI_NAMESPACE_NODE     *Node,      /* New Child*/
    OBJECT_TYPE_INTERNAL    Type)
{
    UINT16                  OwnerId = TABLE_ID_DSDT;
    ACPI_NAMESPACE_NODE     *ChildNode;


    FUNCTION_TRACE ("NsInstallNode");


    /*
     * Get the owner ID from the Walk state
     * The owner ID is used to track table deletion and
     * deletion of objects created by methods
     */
    if (WalkState)
    {
        OwnerId = WalkState->OwnerId;
    }


    /* link the new entry into the parent and existing children */

    /* TBD: Could be first, last, or alphabetic */

    ChildNode = ParentNode->Child;
    if (!ChildNode)
    {
        ParentNode->Child = Node;
    }

    else
    {
        while (!(ChildNode->Flags & ANOBJ_END_OF_PEER_LIST))
        {
            ChildNode = ChildNode->Peer;
        }

        ChildNode->Peer = Node;

        /* Clear end-of-list flag */

        ChildNode->Flags &= ~ANOBJ_END_OF_PEER_LIST;
    }

    /* Init the new entry */

    Node->OwnerId   = OwnerId;
    Node->Flags     |= ANOBJ_END_OF_PEER_LIST;
    Node->Peer      = ParentNode;


    /*
     * If adding a name with unknown type, or having to
     * add the region in order to define fields in it, we
     * have a forward reference.
     */

    if ((ACPI_TYPE_ANY == Type) ||
        (INTERNAL_TYPE_DEF_FIELD_DEFN == Type) ||
        (INTERNAL_TYPE_BANK_FIELD_DEFN == Type))
    {
        /*
         * We don't want to abort here, however!
         * We will fill in the actual type when the
         * real definition is found later.
         */

        DEBUG_PRINT (ACPI_INFO,
            ("NsInstallNode: [%4.4s] is a forward reference\n",
            &Node->Name));

    }

    /*
     * The DefFieldDefn and BankFieldDefn cases are actually
     * looking up the Region in which the field will be defined
     */

    if ((INTERNAL_TYPE_DEF_FIELD_DEFN == Type) ||
        (INTERNAL_TYPE_BANK_FIELD_DEFN == Type))
    {
        Type = ACPI_TYPE_REGION;
    }

    /*
     * Scope, DefAny, and IndexFieldDefn are bogus "types" which do
     * not actually have anything to do with the type of the name
     * being looked up.  Save any other value of Type as the type of
     * the entry.
     */

    if ((Type != INTERNAL_TYPE_SCOPE) &&
        (Type != INTERNAL_TYPE_DEF_ANY) &&
        (Type != INTERNAL_TYPE_INDEX_FIELD_DEFN))
    {
        Node->Type = (UINT8) Type;
    }

    DEBUG_PRINT (TRACE_NAMES,
        ("NsInstallNode: %4.4s added to %p at %p\n",
        &Node->Name, ParentNode, Node));

    /*
     * Increment the reference count(s) of all parents up to
     * the root!
     */

    while ((Node = AcpiNsGetParentObject (Node)) != NULL)
    {
        Node->ReferenceCount++;
    }

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsDeleteChildren
 *
 * PARAMETERS:  ParentNode      - Delete this objects children
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete all children of the parent object. Deletes a
 *              "scope".
 *
 ******************************************************************************/

void
AcpiNsDeleteChildren (
    ACPI_NAMESPACE_NODE     *ParentNode)
{
    ACPI_NAMESPACE_NODE     *ChildNode;
    ACPI_NAMESPACE_NODE     *NextNode;
    UINT8                   Flags;


    FUNCTION_TRACE_PTR ("AcpiNsDeleteChildren", ParentNode);


    if (!ParentNode)
    {
        return_VOID;
    }

    /* If no children, all done! */

    ChildNode = ParentNode->Child;
    if (!ChildNode)
    {
        return_VOID;
    }

    /*
     * Deallocate all children at this level
     */
    do
    {
        /* Get the things we need */

        NextNode    = ChildNode->Peer;
        Flags       = ChildNode->Flags;

        /* Grandchildren should have all been deleted already */

        if (ChildNode->Child)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("NsDeleteChildren: Found a grandchild! P=%X C=%X\n",
                ParentNode, ChildNode));
        }

        /* Now we can free this child object */

        DECREMENT_NAME_TABLE_METRICS (sizeof (ACPI_NAMESPACE_NODE));

        DEBUG_PRINT (ACPI_INFO,
            ("AcpiNsDeleteChildren: Object %p, Remaining %d\n",
            ChildNode, AcpiGbl_CurrentNodeCount));

        /*
         * Detach an object if there is one
         */

        if (ChildNode->Object)
        {
            AcpiNsDetachObject (ChildNode);
        }

        AcpiCmFree (ChildNode);

        /* And move on to the next child in the list */

        ChildNode = NextNode;

    } while (!(Flags & ANOBJ_END_OF_PEER_LIST));


    /* Clear the parent's child pointer */

    ParentNode->Child = NULL;

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsDeleteNamespaceSubtree
 *
 * PARAMETERS:  None.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete a subtree of the namespace.  This includes all objects
 *              stored within the subtree.  Scope tables are deleted also
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsDeleteNamespaceSubtree (
    ACPI_NAMESPACE_NODE     *ParentNode)
{
    ACPI_NAMESPACE_NODE     *ChildNode;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  Level;


    FUNCTION_TRACE ("NsDeleteNamespaceSubtree");


    if (!ParentNode)
    {
        return_ACPI_STATUS (AE_OK);
    }


    ChildNode   = 0;
    Level       = 1;

    /*
     * Traverse the tree of objects until we bubble back up
     * to where we started.
     */

    while (Level > 0)
    {
        /*
         * Get the next typed object in this scope.
         * Null returned if not found
         */

        ChildNode = AcpiNsGetNextObject (ACPI_TYPE_ANY, ParentNode,
                                            ChildNode);
        if (ChildNode)
        {
            /*
             * Found an object - delete the object within
             * the Value field
             */

            ObjDesc = AcpiNsGetAttachedObject (ChildNode);
            if (ObjDesc)
            {
                AcpiNsDetachObject (ChildNode);
                AcpiCmRemoveReference (ObjDesc);
            }


            /* Check if this object has any children */

            if (AcpiNsGetNextObject (ACPI_TYPE_ANY, ChildNode, 0))
            {
                /*
                 * There is at least one child of this object,
                 * visit the object
                 */

                Level++;
                ParentNode    = ChildNode;
                ChildNode     = 0;
            }
        }

        else
        {
            /*
             * No more children in this object.
             * We will move up to the grandparent.
             */
            Level--;

            /*
             * Now delete all of the children of this parent
             * all at the same time.
             */
            AcpiNsDeleteChildren (ParentNode);

            /* New "last child" is this parent object */

            ChildNode = ParentNode;

            /* Now we can move up the tree to the grandparent */

            ParentNode = AcpiNsGetParentObject (ParentNode);
        }
    }


    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsRemoveReference
 *
 * PARAMETERS:  Node           - Named object whose reference count is to be
 *                                decremented
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Remove a Node reference.  Decrements the reference count
 *              of all parent Nodes up to the root.  Any object along
 *              the way that reaches zero references is freed.
 *
 ******************************************************************************/

void
AcpiNsRemoveReference (
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_NAMESPACE_NODE     *NextNode;


    /*
     * Decrement the reference count(s) of this object and all
     * objects up to the root,  Delete anything with zero remaining references.
     */
    NextNode = Node;
    while (NextNode)
    {
        /* Decrement the reference count on this object*/

        NextNode->ReferenceCount--;

        /* Delete the object if no more references */

        if (!NextNode->ReferenceCount)
        {
            /* Delete all children and delete the object */

            AcpiNsDeleteChildren (NextNode);
            AcpiNsDeleteNode (NextNode);
        }

        /* Move up to parent */

        NextNode = AcpiNsGetParentObject (NextNode);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsDeleteNamespaceByOwner
 *
 * PARAMETERS:  None.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete entries within the namespace that are owned by a
 *              specific ID.  Used to delete entire ACPI tables.  All
 *              reference counts are updated.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsDeleteNamespaceByOwner (
    UINT16                  OwnerId)
{
    ACPI_NAMESPACE_NODE     *ChildNode;
    UINT32                  Level;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *ParentNode;


    FUNCTION_TRACE ("NsDeleteNamespaceSubtree");


    ParentNode  = AcpiGbl_RootNode;
    ChildNode   = 0;
    Level       = 1;

    /*
     * Traverse the tree of objects until we bubble back up
     * to where we started.
     */

    while (Level > 0)
    {
        /*
         * Get the next typed object in this scope.
         * Null returned if not found
         */

        ChildNode = AcpiNsGetNextObject (ACPI_TYPE_ANY, ParentNode,
                                            ChildNode);

        if (ChildNode)
        {
            if (ChildNode->OwnerId == OwnerId)
            {
                /*
                 * Found an object - delete the object within
                 * the Value field
                 */

                ObjDesc = AcpiNsGetAttachedObject (ChildNode);
                if (ObjDesc)
                {
                    AcpiNsDetachObject (ChildNode);
                    AcpiCmRemoveReference (ObjDesc);
                }
            }

            /* Check if this object has any children */

            if (AcpiNsGetNextObject (ACPI_TYPE_ANY, ChildNode, 0))
            {
                /*
                 * There is at least one child of this object,
                 * visit the object
                 */

                Level++;
                ParentNode    = ChildNode;
                ChildNode     = 0;
            }

            else if (ChildNode->OwnerId == OwnerId)
            {
                AcpiNsRemoveReference (ChildNode);
            }
        }

        else
        {
            /*
             * No more children in this object.  Move up to grandparent.
             */
            Level--;

            if (Level != 0)
            {
                if (ParentNode->OwnerId == OwnerId)
                {
                    AcpiNsRemoveReference (ParentNode);
                }
            }

            /* New "last child" is this parent object */

            ChildNode = ParentNode;

            /* Now we can move up the tree to the grandparent */

            ParentNode = AcpiNsGetParentObject (ParentNode);
        }
    }


    return_ACPI_STATUS (AE_OK);
}


