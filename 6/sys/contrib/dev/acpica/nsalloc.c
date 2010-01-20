/*******************************************************************************
 *
 * Module Name: nsalloc - Namespace allocation and deletion utilities
 *              $Revision: 88 $
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


#define __NSALLOC_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acnamesp.h>


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsalloc")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCreateNode
 *
 * PARAMETERS:  AcpiName        - Name of the new node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create a namespace node
 *
 ******************************************************************************/

ACPI_NAMESPACE_NODE *
AcpiNsCreateNode (
    UINT32                  Name)
{
    ACPI_NAMESPACE_NODE     *Node;


    ACPI_FUNCTION_TRACE ("NsCreateNode");


    Node = ACPI_MEM_CALLOCATE (sizeof (ACPI_NAMESPACE_NODE));
    if (!Node)
    {
        return_PTR (NULL);
    }

    ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_NSNODE].TotalAllocated++);

    Node->Name.Integer   = Name;
    Node->ReferenceCount = 1;
    ACPI_SET_DESCRIPTOR_TYPE (Node, ACPI_DESC_TYPE_NAMED);

    return_PTR (Node);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsDeleteNode
 *
 * PARAMETERS:  Node            - Node to be deleted
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete a namespace node
 *
 ******************************************************************************/

void
AcpiNsDeleteNode (
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_NAMESPACE_NODE     *ParentNode;
    ACPI_NAMESPACE_NODE     *PrevNode;
    ACPI_NAMESPACE_NODE     *NextNode;


    ACPI_FUNCTION_TRACE_PTR ("NsDeleteNode", Node);


    ParentNode = AcpiNsGetParentNode (Node);

    PrevNode = NULL;
    NextNode = ParentNode->Child;

    /* Find the node that is the previous peer in the parent's child list */

    while (NextNode != Node)
    {
        PrevNode = NextNode;
        NextNode = PrevNode->Peer;
    }

    if (PrevNode)
    {
        /* Node is not first child, unlink it */

        PrevNode->Peer = NextNode->Peer;
        if (NextNode->Flags & ANOBJ_END_OF_PEER_LIST)
        {
            PrevNode->Flags |= ANOBJ_END_OF_PEER_LIST;
        }
    }
    else
    {
        /* Node is first child (has no previous peer) */

        if (NextNode->Flags & ANOBJ_END_OF_PEER_LIST)
        {
            /* No peers at all */

            ParentNode->Child = NULL;
        }
        else
        {   /* Link peer list to parent */

            ParentNode->Child = NextNode->Peer;
        }
    }


    ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_NSNODE].TotalFreed++);

    /*
     * Detach an object if there is one then delete the node
     */
    AcpiNsDetachObject (Node);
    ACPI_MEM_FREE (Node);
    return_VOID;
}


#ifdef ACPI_ALPHABETIC_NAMESPACE
/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCompareNames
 *
 * PARAMETERS:  Name1           - First name to compare
 *              Name2           - Second name to compare
 *
 * RETURN:      value from strncmp
 *
 * DESCRIPTION: Compare two ACPI names.  Names that are prefixed with an
 *              underscore are forced to be alphabetically first.
 *
 ******************************************************************************/

int
AcpiNsCompareNames (
    char                    *Name1,
    char                    *Name2)
{
    char                    ReversedName1[ACPI_NAME_SIZE];
    char                    ReversedName2[ACPI_NAME_SIZE];
    UINT32                  i;
    UINT32                  j;


    /*
     * Replace all instances of "underscore" with a value that is smaller so
     * that all names that are prefixed with underscore(s) are alphabetically
     * first.
     *
     * Reverse the name bytewise so we can just do a 32-bit compare instead
     * of a strncmp.
     */
    for (i = 0, j= (ACPI_NAME_SIZE - 1); i < ACPI_NAME_SIZE; i++, j--)
    {
        ReversedName1[j] = Name1[i];
        if (Name1[i] == '_')
        {
            ReversedName1[j] = '*';
        }

        ReversedName2[j] = Name2[i];
        if (Name2[i] == '_')
        {
            ReversedName2[j] = '*';
        }
    }

    return (*(int *) ReversedName1 - *(int *) ReversedName2);
}
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsInstallNode
 *
 * PARAMETERS:  WalkState       - Current state of the walk
 *              ParentNode      - The parent of the new Node
 *              Node            - The new Node to install
 *              Type            - ACPI object type of the new Node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialize a new namespace node and install it amongst
 *              its peers.
 *
 *              Note: Current namespace lookup is linear search.  However, the
 *              nodes are linked in alphabetical order to 1) put all reserved
 *              names (start with underscore) first, and to 2) make a readable
 *              namespace dump.
 *
 ******************************************************************************/

void
AcpiNsInstallNode (
    ACPI_WALK_STATE         *WalkState,
    ACPI_NAMESPACE_NODE     *ParentNode,    /* Parent */
    ACPI_NAMESPACE_NODE     *Node,          /* New Child*/
    ACPI_OBJECT_TYPE        Type)
{
    UINT16                  OwnerId = 0;
    ACPI_NAMESPACE_NODE     *ChildNode;
#ifdef ACPI_ALPHABETIC_NAMESPACE

    ACPI_NAMESPACE_NODE     *PreviousChildNode;
#endif


    ACPI_FUNCTION_TRACE ("NsInstallNode");


    /*
     * Get the owner ID from the Walk state
     * The owner ID is used to track table deletion and
     * deletion of objects created by methods
     */
    if (WalkState)
    {
        OwnerId = WalkState->OwnerId;
    }

    /* Link the new entry into the parent and existing children */

    ChildNode = ParentNode->Child;
    if (!ChildNode)
    {
        ParentNode->Child = Node;
        Node->Flags |= ANOBJ_END_OF_PEER_LIST;
        Node->Peer = ParentNode;
    }
    else
    {
#ifdef ACPI_ALPHABETIC_NAMESPACE
        /*
         * Walk the list whilst searching for the correct
         * alphabetic placement.
         */
        PreviousChildNode = NULL;
        while (AcpiNsCompareNames (AcpiUtGetNodeName (ChildNode), AcpiUtGetNodeName (Node)) < 0)
        {
            if (ChildNode->Flags & ANOBJ_END_OF_PEER_LIST)
            {
                /* Last peer;  Clear end-of-list flag */

                ChildNode->Flags &= ~ANOBJ_END_OF_PEER_LIST;

                /* This node is the new peer to the child node */

                ChildNode->Peer = Node;

                /* This node is the new end-of-list */

                Node->Flags |= ANOBJ_END_OF_PEER_LIST;
                Node->Peer = ParentNode;
                break;
            }

            /* Get next peer */

            PreviousChildNode = ChildNode;
            ChildNode = ChildNode->Peer;
        }

        /* Did the node get inserted at the end-of-list? */

        if (!(Node->Flags & ANOBJ_END_OF_PEER_LIST))
        {
            /*
             * Loop above terminated without reaching the end-of-list.
             * Insert the new node at the current location
             */
            if (PreviousChildNode)
            {
                /* Insert node alphabetically */

                Node->Peer = ChildNode;
                PreviousChildNode->Peer = Node;
            }
            else
            {
                /* Insert node alphabetically at start of list */

                Node->Peer = ChildNode;
                ParentNode->Child = Node;
            }
        }
#else
        while (!(ChildNode->Flags & ANOBJ_END_OF_PEER_LIST))
        {
            ChildNode = ChildNode->Peer;
        }

        ChildNode->Peer = Node;

        /* Clear end-of-list flag */

        ChildNode->Flags &= ~ANOBJ_END_OF_PEER_LIST;
        Node->Flags     |= ANOBJ_END_OF_PEER_LIST;
        Node->Peer = ParentNode;
#endif
    }

    /* Init the new entry */

    Node->OwnerId = OwnerId;
    Node->Type = (UINT8) Type;

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
        "%4.4s (%s) [Node %p Owner %X] added to %4.4s (%s) [Node %p]\n",
        AcpiUtGetNodeName (Node), AcpiUtGetTypeName (Node->Type), Node, OwnerId,
        AcpiUtGetNodeName (ParentNode), AcpiUtGetTypeName (ParentNode->Type),
        ParentNode));

    /*
     * Increment the reference count(s) of all parents up to
     * the root!
     */
    while ((Node = AcpiNsGetParentNode (Node)) != NULL)
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
 * DESCRIPTION: Delete all children of the parent object. In other words,
 *              deletes a "scope".
 *
 ******************************************************************************/

void
AcpiNsDeleteChildren (
    ACPI_NAMESPACE_NODE     *ParentNode)
{
    ACPI_NAMESPACE_NODE     *ChildNode;
    ACPI_NAMESPACE_NODE     *NextNode;
    ACPI_NAMESPACE_NODE     *Node;
    UINT8                   Flags;


    ACPI_FUNCTION_TRACE_PTR ("NsDeleteChildren", ParentNode);


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
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Found a grandchild! P=%p C=%p\n",
                ParentNode, ChildNode));
        }

        /* Now we can free this child object */

        ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_NSNODE].TotalFreed++);

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "Object %p, Remaining %X\n",
            ChildNode, AcpiGbl_CurrentNodeCount));

        /*
         * Detach an object if there is one, then free the child node
         */
        AcpiNsDetachObject (ChildNode);

        /*
         * Decrement the reference count(s) of all parents up to
         * the root! (counts were incremented when the node was created)
         */
        Node = ChildNode;
        while ((Node = AcpiNsGetParentNode (Node)) != NULL)
        {
            Node->ReferenceCount--;
        }

        /* There should be only one reference remaining on this node */

        if (ChildNode->ReferenceCount != 1)
        {
            ACPI_REPORT_WARNING (("Existing references (%d) on node being deleted (%p)\n",
                ChildNode->ReferenceCount, ChildNode));
        }

        /* Now we can delete the node */

        ACPI_MEM_FREE (ChildNode);

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
 * PARAMETERS:  ParentNode      - Root of the subtree to be deleted
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete a subtree of the namespace.  This includes all objects
 *              stored within the subtree.
 *
 ******************************************************************************/

void
AcpiNsDeleteNamespaceSubtree (
    ACPI_NAMESPACE_NODE     *ParentNode)
{
    ACPI_NAMESPACE_NODE     *ChildNode = NULL;
    UINT32                  Level = 1;


    ACPI_FUNCTION_TRACE ("NsDeleteNamespaceSubtree");


    if (!ParentNode)
    {
        return_VOID;
    }

    /*
     * Traverse the tree of objects until we bubble back up
     * to where we started.
     */
    while (Level > 0)
    {
        /* Get the next node in this scope (NULL if none) */

        ChildNode = AcpiNsGetNextNode (ACPI_TYPE_ANY, ParentNode,
                                            ChildNode);
        if (ChildNode)
        {
            /* Found a child node - detach any attached object */

            AcpiNsDetachObject (ChildNode);

            /* Check if this node has any children */

            if (AcpiNsGetNextNode (ACPI_TYPE_ANY, ChildNode, NULL))
            {
                /*
                 * There is at least one child of this node,
                 * visit the node
                 */
                Level++;
                ParentNode = ChildNode;
                ChildNode  = NULL;
            }
        }
        else
        {
            /*
             * No more children of this parent node.
             * Move up to the grandparent.
             */
            Level--;

            /*
             * Now delete all of the children of this parent
             * all at the same time.
             */
            AcpiNsDeleteChildren (ParentNode);

            /* New "last child" is this parent node */

            ChildNode = ParentNode;

            /* Move up the tree to the grandparent */

            ParentNode = AcpiNsGetParentNode (ParentNode);
        }
    }

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsRemoveReference
 *
 * PARAMETERS:  Node           - Named node whose reference count is to be
 *                               decremented
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Remove a Node reference.  Decrements the reference count
 *              of all parent Nodes up to the root.  Any node along
 *              the way that reaches zero references is freed.
 *
 ******************************************************************************/

void
AcpiNsRemoveReference (
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_NAMESPACE_NODE     *ParentNode;
    ACPI_NAMESPACE_NODE     *ThisNode;


    ACPI_FUNCTION_ENTRY ();


    /*
     * Decrement the reference count(s) of this node and all
     * nodes up to the root,  Delete anything with zero remaining references.
     */
    ThisNode = Node;
    while (ThisNode)
    {
        /* Prepare to move up to parent */

        ParentNode = AcpiNsGetParentNode (ThisNode);

        /* Decrement the reference count on this node */

        ThisNode->ReferenceCount--;

        /* Delete the node if no more references */

        if (!ThisNode->ReferenceCount)
        {
            /* Delete all children and delete the node */

            AcpiNsDeleteChildren (ThisNode);
            AcpiNsDeleteNode (ThisNode);
        }

        ThisNode = ParentNode;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsDeleteNamespaceByOwner
 *
 * PARAMETERS:  OwnerId     - All nodes with this owner will be deleted
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete entries within the namespace that are owned by a
 *              specific ID.  Used to delete entire ACPI tables.  All
 *              reference counts are updated.
 *
 ******************************************************************************/

void
AcpiNsDeleteNamespaceByOwner (
    UINT16                  OwnerId)
{
    ACPI_NAMESPACE_NODE     *ChildNode;
    ACPI_NAMESPACE_NODE     *DeletionNode;
    UINT32                  Level;
    ACPI_NAMESPACE_NODE     *ParentNode;


    ACPI_FUNCTION_TRACE_U32 ("NsDeleteNamespaceByOwner", OwnerId);


    ParentNode    = AcpiGbl_RootNode;
    ChildNode     = NULL;
    DeletionNode  = NULL;
    Level         = 1;

    /*
     * Traverse the tree of nodes until we bubble back up
     * to where we started.
     */
    while (Level > 0)
    {
        /*
         * Get the next child of this parent node. When ChildNode is NULL,
         * the first child of the parent is returned
         */
        ChildNode = AcpiNsGetNextNode (ACPI_TYPE_ANY, ParentNode, ChildNode);

        if (DeletionNode)
        {
            AcpiNsRemoveReference (DeletionNode);
            DeletionNode = NULL;
        }

        if (ChildNode)
        {
            if (ChildNode->OwnerId == OwnerId)
            {
                /* Found a matching child node - detach any attached object */

                AcpiNsDetachObject (ChildNode);
            }

            /* Check if this node has any children */

            if (AcpiNsGetNextNode (ACPI_TYPE_ANY, ChildNode, NULL))
            {
                /*
                 * There is at least one child of this node,
                 * visit the node
                 */
                Level++;
                ParentNode = ChildNode;
                ChildNode  = NULL;
            }
            else if (ChildNode->OwnerId == OwnerId)
            {
                DeletionNode = ChildNode;
            }
        }
        else
        {
            /*
             * No more children of this parent node.
             * Move up to the grandparent.
             */
            Level--;
            if (Level != 0)
            {
                if (ParentNode->OwnerId == OwnerId)
                {
                    DeletionNode = ParentNode;
                }
            }

            /* New "last child" is this parent node */

            ChildNode = ParentNode;

            /* Move up the tree to the grandparent */

            ParentNode = AcpiNsGetParentNode (ParentNode);
        }
    }

    return_VOID;
}


