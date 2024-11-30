/** @file
  An OrderedCollectionLib instance that provides a red-black tree
  implementation, and allocates and releases tree nodes with
  MemoryAllocationLib.

  This library instance is useful when a fast associative container is needed.
  Worst case time complexity is O(log n) for Find(), Next(), Prev(), Min(),
  Max(), Insert(), and Delete(), where "n" is the number of elements in the
  tree. Complete ordered traversal takes O(n) time.

  The implementation is also useful as a fast priority queue.

  Copyright (C) 2014, Red Hat, Inc.
  Copyright (c) 2014, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/OrderedCollectionLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

typedef enum {
  RedBlackTreeRed,
  RedBlackTreeBlack
} RED_BLACK_TREE_COLOR;

//
// Incomplete types and convenience typedefs are present in the library class
// header. Beside completing the types, we introduce typedefs here that reflect
// the implementation closely.
//
typedef ORDERED_COLLECTION               RED_BLACK_TREE;
typedef ORDERED_COLLECTION_ENTRY         RED_BLACK_TREE_NODE;
typedef ORDERED_COLLECTION_USER_COMPARE  RED_BLACK_TREE_USER_COMPARE;
typedef ORDERED_COLLECTION_KEY_COMPARE   RED_BLACK_TREE_KEY_COMPARE;

struct ORDERED_COLLECTION {
  RED_BLACK_TREE_NODE            *Root;
  RED_BLACK_TREE_USER_COMPARE    UserStructCompare;
  RED_BLACK_TREE_KEY_COMPARE     KeyCompare;
};

struct ORDERED_COLLECTION_ENTRY {
  VOID                    *UserStruct;
  RED_BLACK_TREE_NODE     *Parent;
  RED_BLACK_TREE_NODE     *Left;
  RED_BLACK_TREE_NODE     *Right;
  RED_BLACK_TREE_COLOR    Color;
};

/**
  Retrieve the user structure linked by the specified tree node.

  Read-only operation.

  @param[in] Node  Pointer to the tree node whose associated user structure we
                   want to retrieve. The caller is responsible for passing a
                   non-NULL argument.

  @return  Pointer to user structure linked by Node.
**/
VOID *
EFIAPI
OrderedCollectionUserStruct (
  IN CONST RED_BLACK_TREE_NODE  *Node
  )
{
  return Node->UserStruct;
}

/**
  A slow function that asserts that the tree is a valid red-black tree, and
  that it orders user structures correctly.

  Read-only operation.

  This function uses the stack for recursion and is not recommended for
  "production use".

  @param[in] Tree  The tree to validate.
**/
VOID
RedBlackTreeValidate (
  IN CONST RED_BLACK_TREE  *Tree
  );

/**
  Allocate and initialize the RED_BLACK_TREE structure.

  Allocation occurs via MemoryAllocationLib's AllocatePool() function.

  @param[in]  UserStructCompare  This caller-provided function will be used to
                                 order two user structures linked into the
                                 tree, during the insertion procedure.

  @param[in]  KeyCompare         This caller-provided function will be used to
                                 order the standalone search key against user
                                 structures linked into the tree, during the
                                 lookup procedure.

  @retval NULL  If allocation failed.

  @return       Pointer to the allocated, initialized RED_BLACK_TREE structure,
                otherwise.
**/
RED_BLACK_TREE *
EFIAPI
OrderedCollectionInit (
  IN RED_BLACK_TREE_USER_COMPARE  UserStructCompare,
  IN RED_BLACK_TREE_KEY_COMPARE   KeyCompare
  )
{
  RED_BLACK_TREE  *Tree;

  Tree = AllocatePool (sizeof *Tree);
  if (Tree == NULL) {
    return NULL;
  }

  Tree->Root              = NULL;
  Tree->UserStructCompare = UserStructCompare;
  Tree->KeyCompare        = KeyCompare;

  if (FeaturePcdGet (PcdValidateOrderedCollection)) {
    RedBlackTreeValidate (Tree);
  }

  return Tree;
}

/**
  Check whether the tree is empty (has no nodes).

  Read-only operation.

  @param[in] Tree  The tree to check for emptiness.

  @retval TRUE   The tree is empty.

  @retval FALSE  The tree is not empty.
**/
BOOLEAN
EFIAPI
OrderedCollectionIsEmpty (
  IN CONST RED_BLACK_TREE  *Tree
  )
{
  return (BOOLEAN)(Tree->Root == NULL);
}

/**
  Uninitialize and release an empty RED_BLACK_TREE structure.

  Read-write operation.

  Release occurs via MemoryAllocationLib's FreePool() function.

  It is the caller's responsibility to delete all nodes from the tree before
  calling this function.

  @param[in] Tree  The empty tree to uninitialize and release.
**/
VOID
EFIAPI
OrderedCollectionUninit (
  IN RED_BLACK_TREE  *Tree
  )
{
  ASSERT (OrderedCollectionIsEmpty (Tree));
  FreePool (Tree);
}

/**
  Look up the tree node that links the user structure that matches the
  specified standalone key.

  Read-only operation.

  @param[in] Tree           The tree to search for StandaloneKey.

  @param[in] StandaloneKey  The key to locate among the user structures linked
                            into Tree. StandaloneKey will be passed to
                            Tree->KeyCompare().

  @retval NULL  StandaloneKey could not be found.

  @return       The tree node that links to the user structure matching
                StandaloneKey, otherwise.
**/
RED_BLACK_TREE_NODE *
EFIAPI
OrderedCollectionFind (
  IN CONST RED_BLACK_TREE  *Tree,
  IN CONST VOID            *StandaloneKey
  )
{
  RED_BLACK_TREE_NODE  *Node;

  Node = Tree->Root;
  while (Node != NULL) {
    INTN  Result;

    Result = Tree->KeyCompare (StandaloneKey, Node->UserStruct);
    if (Result == 0) {
      break;
    }

    Node = (Result < 0) ? Node->Left : Node->Right;
  }

  return Node;
}

/**
  Find the tree node of the minimum user structure stored in the tree.

  Read-only operation.

  @param[in] Tree  The tree to return the minimum node of. The user structure
                   linked by the minimum node compares less than all other user
                   structures in the tree.

  @retval NULL  If Tree is empty.

  @return       The tree node that links the minimum user structure, otherwise.
**/
RED_BLACK_TREE_NODE *
EFIAPI
OrderedCollectionMin (
  IN CONST RED_BLACK_TREE  *Tree
  )
{
  RED_BLACK_TREE_NODE  *Node;

  Node = Tree->Root;
  if (Node == NULL) {
    return NULL;
  }

  while (Node->Left != NULL) {
    Node = Node->Left;
  }

  return Node;
}

/**
  Find the tree node of the maximum user structure stored in the tree.

  Read-only operation.

  @param[in] Tree  The tree to return the maximum node of. The user structure
                   linked by the maximum node compares greater than all other
                   user structures in the tree.

  @retval NULL  If Tree is empty.

  @return       The tree node that links the maximum user structure, otherwise.
**/
RED_BLACK_TREE_NODE *
EFIAPI
OrderedCollectionMax (
  IN CONST RED_BLACK_TREE  *Tree
  )
{
  RED_BLACK_TREE_NODE  *Node;

  Node = Tree->Root;
  if (Node == NULL) {
    return NULL;
  }

  while (Node->Right != NULL) {
    Node = Node->Right;
  }

  return Node;
}

/**
  Get the tree node of the least user structure that is greater than the one
  linked by Node.

  Read-only operation.

  @param[in] Node  The node to get the successor node of.

  @retval NULL  If Node is NULL, or Node is the maximum node of its containing
                tree (ie. Node has no successor node).

  @return       The tree node linking the least user structure that is greater
                than the one linked by Node, otherwise.
**/
RED_BLACK_TREE_NODE *
EFIAPI
OrderedCollectionNext (
  IN CONST RED_BLACK_TREE_NODE  *Node
  )
{
  RED_BLACK_TREE_NODE        *Walk;
  CONST RED_BLACK_TREE_NODE  *Child;

  if (Node == NULL) {
    return NULL;
  }

  //
  // If Node has a right subtree, then the successor is the minimum node of
  // that subtree.
  //
  Walk = Node->Right;
  if (Walk != NULL) {
    while (Walk->Left != NULL) {
      Walk = Walk->Left;
    }

    return Walk;
  }

  //
  // Otherwise we have to ascend as long as we're our parent's right child (ie.
  // ascending to the left).
  //
  Child = Node;
  Walk  = Child->Parent;
  while (Walk != NULL && Child == Walk->Right) {
    Child = Walk;
    Walk  = Child->Parent;
  }

  return Walk;
}

/**
  Get the tree node of the greatest user structure that is less than the one
  linked by Node.

  Read-only operation.

  @param[in] Node  The node to get the predecessor node of.

  @retval NULL  If Node is NULL, or Node is the minimum node of its containing
                tree (ie. Node has no predecessor node).

  @return       The tree node linking the greatest user structure that is less
                than the one linked by Node, otherwise.
**/
RED_BLACK_TREE_NODE *
EFIAPI
OrderedCollectionPrev (
  IN CONST RED_BLACK_TREE_NODE  *Node
  )
{
  RED_BLACK_TREE_NODE        *Walk;
  CONST RED_BLACK_TREE_NODE  *Child;

  if (Node == NULL) {
    return NULL;
  }

  //
  // If Node has a left subtree, then the predecessor is the maximum node of
  // that subtree.
  //
  Walk = Node->Left;
  if (Walk != NULL) {
    while (Walk->Right != NULL) {
      Walk = Walk->Right;
    }

    return Walk;
  }

  //
  // Otherwise we have to ascend as long as we're our parent's left child (ie.
  // ascending to the right).
  //
  Child = Node;
  Walk  = Child->Parent;
  while (Walk != NULL && Child == Walk->Left) {
    Child = Walk;
    Walk  = Child->Parent;
  }

  return Walk;
}

/**
  Rotate tree nodes around Pivot to the right.

                Parent                       Parent
                  |                            |
                Pivot                      LeftChild
               /     .                    .         \_
      LeftChild       Node1   --->   Node2           Pivot
         . \                                          / .
    Node2   LeftRightChild              LeftRightChild   Node1

  The ordering Node2 < LeftChild < LeftRightChild < Pivot < Node1 is kept
  intact. Parent (if any) is either at the left extreme or the right extreme of
  this ordering, and that relation is also kept intact.

  Edges marked with a dot (".") don't change during rotation.

  Internal read-write operation.

  @param[in,out] Pivot    The tree node to rotate other nodes right around. It
                          is the caller's responsibility to ensure that
                          Pivot->Left is not NULL.

  @param[out]    NewRoot  If Pivot has a parent node on input, then the
                          function updates Pivot's original parent on output
                          according to the rotation, and NewRoot is not
                          accessed.

                          If Pivot has no parent node on input (ie. Pivot is
                          the root of the tree), then the function stores the
                          new root node of the tree in NewRoot.
**/
VOID
RedBlackTreeRotateRight (
  IN OUT RED_BLACK_TREE_NODE  *Pivot,
  OUT    RED_BLACK_TREE_NODE  **NewRoot
  )
{
  RED_BLACK_TREE_NODE  *Parent;
  RED_BLACK_TREE_NODE  *LeftChild;
  RED_BLACK_TREE_NODE  *LeftRightChild;

  Parent         = Pivot->Parent;
  LeftChild      = Pivot->Left;
  LeftRightChild = LeftChild->Right;

  Pivot->Left = LeftRightChild;
  if (LeftRightChild != NULL) {
    LeftRightChild->Parent = Pivot;
  }

  LeftChild->Parent = Parent;
  if (Parent == NULL) {
    *NewRoot = LeftChild;
  } else {
    if (Pivot == Parent->Left) {
      Parent->Left = LeftChild;
    } else {
      Parent->Right = LeftChild;
    }
  }

  LeftChild->Right = Pivot;
  Pivot->Parent    = LeftChild;
}

/**
  Rotate tree nodes around Pivot to the left.

          Parent                                 Parent
            |                                      |
          Pivot                                RightChild
         .     \                              /          .
    Node1       RightChild    --->       Pivot            Node2
                    /.                    . \_
      RightLeftChild  Node2          Node1   RightLeftChild

  The ordering Node1 < Pivot < RightLeftChild < RightChild < Node2 is kept
  intact. Parent (if any) is either at the left extreme or the right extreme of
  this ordering, and that relation is also kept intact.

  Edges marked with a dot (".") don't change during rotation.

  Internal read-write operation.

  @param[in,out] Pivot    The tree node to rotate other nodes left around. It
                          is the caller's responsibility to ensure that
                          Pivot->Right is not NULL.

  @param[out]    NewRoot  If Pivot has a parent node on input, then the
                          function updates Pivot's original parent on output
                          according to the rotation, and NewRoot is not
                          accessed.

                          If Pivot has no parent node on input (ie. Pivot is
                          the root of the tree), then the function stores the
                          new root node of the tree in NewRoot.
**/
VOID
RedBlackTreeRotateLeft (
  IN OUT RED_BLACK_TREE_NODE  *Pivot,
  OUT    RED_BLACK_TREE_NODE  **NewRoot
  )
{
  RED_BLACK_TREE_NODE  *Parent;
  RED_BLACK_TREE_NODE  *RightChild;
  RED_BLACK_TREE_NODE  *RightLeftChild;

  Parent         = Pivot->Parent;
  RightChild     = Pivot->Right;
  RightLeftChild = RightChild->Left;

  Pivot->Right = RightLeftChild;
  if (RightLeftChild != NULL) {
    RightLeftChild->Parent = Pivot;
  }

  RightChild->Parent = Parent;
  if (Parent == NULL) {
    *NewRoot = RightChild;
  } else {
    if (Pivot == Parent->Left) {
      Parent->Left = RightChild;
    } else {
      Parent->Right = RightChild;
    }
  }

  RightChild->Left = Pivot;
  Pivot->Parent    = RightChild;
}

/**
  Insert (link) a user structure into the tree.

  Read-write operation.

  This function allocates the new tree node with MemoryAllocationLib's
  AllocatePool() function.

  @param[in,out] Tree        The tree to insert UserStruct into.

  @param[out]    Node        The meaning of this optional, output-only
                             parameter depends on the return value of the
                             function.

                             When insertion is successful (RETURN_SUCCESS),
                             Node is set on output to the new tree node that
                             now links UserStruct.

                             When insertion fails due to lack of memory
                             (RETURN_OUT_OF_RESOURCES), Node is not changed.

                             When insertion fails due to key collision (ie.
                             another user structure is already in the tree that
                             compares equal to UserStruct), with return value
                             RETURN_ALREADY_STARTED, then Node is set on output
                             to the node that links the colliding user
                             structure. This enables "find-or-insert" in one
                             function call, or helps with later removal of the
                             colliding element.

  @param[in]     UserStruct  The user structure to link into the tree.
                             UserStruct is ordered against in-tree user
                             structures with the Tree->UserStructCompare()
                             function.

  @retval RETURN_SUCCESS           Insertion successful. A new tree node has
                                   been allocated, linking UserStruct. The new
                                   tree node is reported back in Node (if the
                                   caller requested it).

                                   Existing RED_BLACK_TREE_NODE pointers into
                                   Tree remain valid. For example, on-going
                                   iterations in the caller can continue with
                                   OrderedCollectionNext() /
                                   OrderedCollectionPrev(), and they will
                                   return the new node at some point if user
                                   structure order dictates it.

  @retval RETURN_OUT_OF_RESOURCES  AllocatePool() failed to allocate memory for
                                   the new tree node. The tree has not been
                                   changed. Existing RED_BLACK_TREE_NODE
                                   pointers into Tree remain valid.

  @retval RETURN_ALREADY_STARTED   A user structure has been found in the tree
                                   that compares equal to UserStruct. The node
                                   linking the colliding user structure is
                                   reported back in Node (if the caller
                                   requested it). The tree has not been
                                   changed. Existing RED_BLACK_TREE_NODE
                                   pointers into Tree remain valid.
**/
RETURN_STATUS
EFIAPI
OrderedCollectionInsert (
  IN OUT RED_BLACK_TREE       *Tree,
  OUT    RED_BLACK_TREE_NODE  **Node      OPTIONAL,
  IN     VOID                 *UserStruct
  )
{
  RED_BLACK_TREE_NODE  *Tmp;
  RED_BLACK_TREE_NODE  *Parent;
  INTN                 Result;
  RETURN_STATUS        Status;
  RED_BLACK_TREE_NODE  *NewRoot;

  Tmp    = Tree->Root;
  Parent = NULL;
  Result = 0;

  //
  // First look for a collision, saving the last examined node for the case
  // when there's no collision.
  //
  while (Tmp != NULL) {
    Result = Tree->UserStructCompare (UserStruct, Tmp->UserStruct);
    if (Result == 0) {
      break;
    }

    Parent = Tmp;
    Tmp    = (Result < 0) ? Tmp->Left : Tmp->Right;
  }

  if (Tmp != NULL) {
    if (Node != NULL) {
      *Node = Tmp;
    }

    Status = RETURN_ALREADY_STARTED;
    goto Done;
  }

  //
  // no collision, allocate a new node
  //
  Tmp = AllocatePool (sizeof *Tmp);
  if (Tmp == NULL) {
    Status = RETURN_OUT_OF_RESOURCES;
    goto Done;
  }

  if (Node != NULL) {
    *Node = Tmp;
  }

  //
  // reference the user structure from the node
  //
  Tmp->UserStruct = UserStruct;

  //
  // Link the node as a child to the correct side of the parent.
  // If there's no parent, the new node is the root node in the tree.
  //
  Tmp->Parent = Parent;
  Tmp->Left   = NULL;
  Tmp->Right  = NULL;
  if (Parent == NULL) {
    Tree->Root = Tmp;
    Tmp->Color = RedBlackTreeBlack;
    Status     = RETURN_SUCCESS;
    goto Done;
  }

  if (Result < 0) {
    Parent->Left = Tmp;
  } else {
    Parent->Right = Tmp;
  }

  Tmp->Color = RedBlackTreeRed;

  //
  // Red-black tree properties:
  //
  // #1 Each node is either red or black (RED_BLACK_TREE_NODE.Color).
  //
  // #2 Each leaf (ie. a pseudo-node pointed-to by a NULL valued
  //    RED_BLACK_TREE_NODE.Left or RED_BLACK_TREE_NODE.Right field) is black.
  //
  // #3 Each red node has two black children.
  //
  // #4 For any node N, and for any leaves L1 and L2 reachable from N, the
  //    paths N..L1 and N..L2 contain the same number of black nodes.
  //
  // #5 The root node is black.
  //
  // By replacing a leaf with a red node above, only property #3 may have been
  // broken. (Note that this is the only edge across which property #3 might
  // not hold in the entire tree.) Restore property #3.
  //

  NewRoot = Tree->Root;
  while (Tmp != NewRoot && Parent->Color == RedBlackTreeRed) {
    RED_BLACK_TREE_NODE  *GrandParent;
    RED_BLACK_TREE_NODE  *Uncle;

    //
    // Tmp is not the root node. Tmp is red. Tmp's parent is red. (Breaking
    // property #3.)
    //
    // Due to property #5, Tmp's parent cannot be the root node, hence Tmp's
    // grandparent exists.
    //
    // Tmp's grandparent is black, because property #3 is only broken between
    // Tmp and Tmp's parent.
    //
    GrandParent = Parent->Parent;

    if (Parent == GrandParent->Left) {
      Uncle = GrandParent->Right;
      if ((Uncle != NULL) && (Uncle->Color == RedBlackTreeRed)) {
        //
        //             GrandParent (black)
        //            /                   \_
        // Parent (red)                    Uncle (red)
        //      |
        //  Tmp (red)
        //

        Parent->Color      = RedBlackTreeBlack;
        Uncle->Color       = RedBlackTreeBlack;
        GrandParent->Color = RedBlackTreeRed;

        //
        //                GrandParent (red)
        //               /                 \_
        // Parent (black)                   Uncle (black)
        //       |
        //   Tmp (red)
        //
        // We restored property #3 between Tmp and Tmp's parent, without
        // breaking property #4. However, we may have broken property #3
        // between Tmp's grandparent and Tmp's great-grandparent (if any), so
        // repeat the loop for Tmp's grandparent.
        //
        // If Tmp's grandparent has no parent, then the loop will terminate,
        // and we will have broken property #5, by coloring the root red. We'll
        // restore property #5 after the loop, without breaking any others.
        //
        Tmp    = GrandParent;
        Parent = Tmp->Parent;
      } else {
        //
        // Tmp's uncle is black (satisfied by the case too when Tmp's uncle is
        // NULL, see property #2).
        //

        if (Tmp == Parent->Right) {
          //
          //                 GrandParent (black): D
          //                /                      \_
          // Parent (red): A                        Uncle (black): E
          //      \_
          //       Tmp (red): B
          //            \_
          //             black: C
          //
          // Rotate left, pivoting on node A. This keeps the breakage of
          // property #3 in the same spot, and keeps other properties intact
          // (because both Tmp and its parent are red).
          //
          Tmp = Parent;
          RedBlackTreeRotateLeft (Tmp, &NewRoot);
          Parent = Tmp->Parent;

          //
          // With the rotation we reached the same configuration as if Tmp had
          // been a left child to begin with.
          //
          //                       GrandParent (black): D
          //                      /                      \_
          //       Parent (red): B                        Uncle (black): E
          //             / \_
          // Tmp (red): A   black: C
          //
          ASSERT (GrandParent == Parent->Parent);
        }

        Parent->Color      = RedBlackTreeBlack;
        GrandParent->Color = RedBlackTreeRed;

        //
        // Property #3 is now restored, but we've broken property #4. Namely,
        // paths going through node E now see a decrease in black count, while
        // paths going through node B don't.
        //
        //                        GrandParent (red): D
        //                       /                    \_
        //      Parent (black): B                      Uncle (black): E
        //             / \_
        // Tmp (red): A   black: C
        //

        RedBlackTreeRotateRight (GrandParent, &NewRoot);

        //
        // Property #4 has been restored for node E, and preserved for others.
        //
        //              Parent (black): B
        //             /                 \_
        // Tmp (red): A                   [GrandParent] (red): D
        //                                         / \_
        //                                 black: C   [Uncle] (black): E
        //
        // This configuration terminates the loop because Tmp's parent is now
        // black.
        //
      }
    } else {
      //
      // Symmetrical to the other branch.
      //
      Uncle = GrandParent->Left;
      if ((Uncle != NULL) && (Uncle->Color == RedBlackTreeRed)) {
        Parent->Color      = RedBlackTreeBlack;
        Uncle->Color       = RedBlackTreeBlack;
        GrandParent->Color = RedBlackTreeRed;
        Tmp                = GrandParent;
        Parent             = Tmp->Parent;
      } else {
        if (Tmp == Parent->Left) {
          Tmp = Parent;
          RedBlackTreeRotateRight (Tmp, &NewRoot);
          Parent = Tmp->Parent;
          ASSERT (GrandParent == Parent->Parent);
        }

        Parent->Color      = RedBlackTreeBlack;
        GrandParent->Color = RedBlackTreeRed;
        RedBlackTreeRotateLeft (GrandParent, &NewRoot);
      }
    }
  }

  NewRoot->Color = RedBlackTreeBlack;
  Tree->Root     = NewRoot;
  Status         = RETURN_SUCCESS;

Done:
  if (FeaturePcdGet (PcdValidateOrderedCollection)) {
    RedBlackTreeValidate (Tree);
  }

  return Status;
}

/**
  Check if a node is black, allowing for leaf nodes (see property #2).

  This is a convenience shorthand.

  param[in] Node  The node to check. Node may be NULL, corresponding to a leaf.

  @return  If Node is NULL or colored black.
**/
BOOLEAN
NodeIsNullOrBlack (
  IN CONST RED_BLACK_TREE_NODE  *Node
  )
{
  return (BOOLEAN)(Node == NULL || Node->Color == RedBlackTreeBlack);
}

/**
  Delete a node from the tree, unlinking the associated user structure.

  Read-write operation.

  @param[in,out] Tree        The tree to delete Node from.

  @param[in]     Node        The tree node to delete from Tree. The caller is
                             responsible for ensuring that Node belongs to
                             Tree, and that Node is non-NULL and valid. Node is
                             typically an earlier return value, or output
                             parameter, of:

                             - OrderedCollectionFind(), for deleting a node by
                               user structure key,

                             - OrderedCollectionMin() / OrderedCollectionMax(),
                               for deleting the minimum / maximum node,

                             - OrderedCollectionNext() /
                               OrderedCollectionPrev(), for deleting a node
                               found during an iteration,

                             - OrderedCollectionInsert() with return value
                               RETURN_ALREADY_STARTED, for deleting a node
                               whose linked user structure caused collision
                               during insertion.

                             Given a non-empty Tree, Tree->Root is also a valid
                             Node argument (typically used for simplicity in
                             loops that empty the tree completely).

                             Node is released with MemoryAllocationLib's
                             FreePool() function.

                             Existing RED_BLACK_TREE_NODE pointers (ie.
                             iterators) *different* from Node remain valid. For
                             example:

                             - OrderedCollectionNext() /
                               OrderedCollectionPrev() iterations in the caller
                               can be continued from Node, if
                               OrderedCollectionNext() or
                               OrderedCollectionPrev() is called on Node
                               *before* OrderedCollectionDelete() is. That is,
                               fetch the successor / predecessor node first,
                               then delete Node.

                             - On-going iterations in the caller that would
                               have otherwise returned Node at some point, as
                               dictated by user structure order, will correctly
                               reflect the absence of Node after
                               OrderedCollectionDelete() is called
                               mid-iteration.

  @param[out]    UserStruct  If the caller provides this optional output-only
                             parameter, then on output it is set to the user
                             structure originally linked by Node (which is now
                             freed).

                             This is a convenience that may save the caller a
                             OrderedCollectionUserStruct() invocation before
                             calling OrderedCollectionDelete(), in order to
                             retrieve the user structure being unlinked.
**/
VOID
EFIAPI
OrderedCollectionDelete (
  IN OUT RED_BLACK_TREE       *Tree,
  IN     RED_BLACK_TREE_NODE  *Node,
  OUT    VOID                 **UserStruct OPTIONAL
  )
{
  RED_BLACK_TREE_NODE   *NewRoot;
  RED_BLACK_TREE_NODE   *OrigLeftChild;
  RED_BLACK_TREE_NODE   *OrigRightChild;
  RED_BLACK_TREE_NODE   *OrigParent;
  RED_BLACK_TREE_NODE   *Child;
  RED_BLACK_TREE_NODE   *Parent;
  RED_BLACK_TREE_COLOR  ColorOfUnlinked;

  NewRoot        = Tree->Root;
  OrigLeftChild  = Node->Left,
  OrigRightChild = Node->Right,
  OrigParent     = Node->Parent;

  if (UserStruct != NULL) {
    *UserStruct = Node->UserStruct;
  }

  //
  // After this block, no matter which branch we take:
  // - Child will point to the unique (or NULL) original child of the node that
  //   we will have unlinked,
  // - Parent will point to the *position* of the original parent of the node
  //   that we will have unlinked.
  //
  if ((OrigLeftChild == NULL) || (OrigRightChild == NULL)) {
    //
    // Node has at most one child. We can connect that child (if any) with
    // Node's parent (if any), unlinking Node. This will preserve ordering
    // because the subtree rooted in Node's child (if any) remains on the same
    // side of Node's parent (if any) that Node was before.
    //
    Parent          = OrigParent;
    Child           = (OrigLeftChild != NULL) ? OrigLeftChild : OrigRightChild;
    ColorOfUnlinked = Node->Color;

    if (Child != NULL) {
      Child->Parent = Parent;
    }

    if (OrigParent == NULL) {
      NewRoot = Child;
    } else {
      if (Node == OrigParent->Left) {
        OrigParent->Left = Child;
      } else {
        OrigParent->Right = Child;
      }
    }
  } else {
    //
    // Node has two children. We unlink Node's successor, and then link it into
    // Node's place, keeping Node's original color. This preserves ordering
    // because:
    // - Node's left subtree is less than Node, hence less than Node's
    //   successor.
    // - Node's right subtree is greater than Node. Node's successor is the
    //   minimum of that subtree, hence Node's successor is less than Node's
    //   right subtree with its minimum removed.
    // - Node's successor is in Node's subtree, hence it falls on the same side
    //   of Node's parent as Node itself. The relinking doesn't change this
    //   relation.
    //
    RED_BLACK_TREE_NODE  *ToRelink;

    ToRelink = OrigRightChild;
    if (ToRelink->Left == NULL) {
      //
      // OrigRightChild itself is Node's successor, it has no left child:
      //
      //                OrigParent
      //                    |
      //                  Node: B
      //                 /       \_
      // OrigLeftChild: A         OrigRightChild: E <--- Parent, ToRelink
      //                                           \_
      //                                            F <--- Child
      //
      Parent = OrigRightChild;
      Child  = OrigRightChild->Right;
    } else {
      do {
        ToRelink = ToRelink->Left;
      } while (ToRelink->Left != NULL);

      //
      // Node's successor is the minimum of OrigRightChild's proper subtree:
      //
      //                OrigParent
      //                    |
      //                  Node: B
      //                 /       \_
      // OrigLeftChild: A         OrigRightChild: E <--- Parent
      //                                  /
      //                                 C <--- ToRelink
      //                                  \_
      //                                   D <--- Child
      Parent = ToRelink->Parent;
      Child  = ToRelink->Right;

      //
      // Unlink Node's successor (ie. ToRelink):
      //
      //                OrigParent
      //                    |
      //                  Node: B
      //                 /       \_
      // OrigLeftChild: A         OrigRightChild: E <--- Parent
      //                                  /
      //                                 D <--- Child
      //
      //                                 C <--- ToRelink
      //
      Parent->Left = Child;
      if (Child != NULL) {
        Child->Parent = Parent;
      }

      //
      // We start to link Node's unlinked successor into Node's place:
      //
      //                OrigParent
      //                    |
      //                  Node: B     C <--- ToRelink
      //                 /             \_
      // OrigLeftChild: A               OrigRightChild: E <--- Parent
      //                                        /
      //                                       D <--- Child
      //
      //
      //
      ToRelink->Right        = OrigRightChild;
      OrigRightChild->Parent = ToRelink;
    }

    //
    // The rest handles both cases, attaching ToRelink (Node's original
    // successor) to OrigLeftChild and OrigParent.
    //
    //                           Parent,
    //              OrigParent   ToRelink             OrigParent
    //                  |        |                        |
    //                Node: B    |                      Node: B          Parent
    //                           v                                          |
    //           OrigRightChild: E                        C <--- ToRelink   |
    //                 / \                               / \                v
    // OrigLeftChild: A   F              OrigLeftChild: A   OrigRightChild: E
    //                    ^                                         /
    //                    |                                        D <--- Child
    //                  Child
    //
    ToRelink->Left        = OrigLeftChild;
    OrigLeftChild->Parent = ToRelink;

    //
    // Node's color must be preserved in Node's original place.
    //
    ColorOfUnlinked = ToRelink->Color;
    ToRelink->Color = Node->Color;

    //
    // Finish linking Node's unlinked successor into Node's place.
    //
    //                           Parent,
    //                Node: B    ToRelink               Node: B
    //                           |
    //              OrigParent   |                    OrigParent         Parent
    //                  |        v                        |                 |
    //           OrigRightChild: E                        C <--- ToRelink   |
    //                 / \                               / \                v
    // OrigLeftChild: A   F              OrigLeftChild: A   OrigRightChild: E
    //                    ^                                         /
    //                    |                                        D <--- Child
    //                  Child
    //
    ToRelink->Parent = OrigParent;
    if (OrigParent == NULL) {
      NewRoot = ToRelink;
    } else {
      if (Node == OrigParent->Left) {
        OrigParent->Left = ToRelink;
      } else {
        OrigParent->Right = ToRelink;
      }
    }
  }

  FreePool (Node);

  //
  // If the node that we unlinked from its original spot (ie. Node itself, or
  // Node's successor), was red, then we broke neither property #3 nor property
  // #4: we didn't create any red-red edge between Child and Parent, and we
  // didn't change the black count on any path.
  //
  if (ColorOfUnlinked == RedBlackTreeBlack) {
    //
    // However, if the unlinked node was black, then we have to transfer its
    // "black-increment" to its unique child (pointed-to by Child), lest we
    // break property #4 for its ancestors.
    //
    // If Child is red, we can simply color it black. If Child is black
    // already, we can't technically transfer a black-increment to it, due to
    // property #1.
    //
    // In the following loop we ascend searching for a red node to color black,
    // or until we reach the root (in which case we can drop the
    // black-increment). Inside the loop body, Child has a black value of 2,
    // transitorily breaking property #1 locally, but maintaining property #4
    // globally.
    //
    // Rotations in the loop preserve property #4.
    //
    while (Child != NewRoot && NodeIsNullOrBlack (Child)) {
      RED_BLACK_TREE_NODE  *Sibling;
      RED_BLACK_TREE_NODE  *LeftNephew;
      RED_BLACK_TREE_NODE  *RightNephew;

      if (Child == Parent->Left) {
        Sibling = Parent->Right;
        //
        // Sibling can never be NULL (ie. a leaf).
        //
        // If Sibling was NULL, then the black count on the path from Parent to
        // Sibling would equal Parent's black value, plus 1 (due to property
        // #2). Whereas the black count on the path from Parent to any leaf via
        // Child would be at least Parent's black value, plus 2 (due to Child's
        // black value of 2). This would clash with property #4.
        //
        // (Sibling can be black of course, but it has to be an internal node.
        // Internality allows Sibling to have children, bumping the black
        // counts of paths that go through it.)
        //
        ASSERT (Sibling != NULL);
        if (Sibling->Color == RedBlackTreeRed) {
          //
          // Sibling's red color implies its children (if any), node C and node
          // E, are black (property #3). It also implies that Parent is black.
          //
          //           grandparent                                 grandparent
          //                |                                           |
          //            Parent,b:B                                     b:D
          //           /          \                                   /   \_
          // Child,2b:A            Sibling,r:D  --->        Parent,r:B     b:E
          //                           /\                       /\_
          //                        b:C  b:E          Child,2b:A  Sibling,b:C
          //
          Sibling->Color = RedBlackTreeBlack;
          Parent->Color  = RedBlackTreeRed;
          RedBlackTreeRotateLeft (Parent, &NewRoot);
          Sibling = Parent->Right;
          //
          // Same reasoning as above.
          //
          ASSERT (Sibling != NULL);
        }

        //
        // Sibling is black, and not NULL. (Ie. Sibling is a black internal
        // node.)
        //
        ASSERT (Sibling->Color == RedBlackTreeBlack);
        LeftNephew  = Sibling->Left;
        RightNephew = Sibling->Right;
        if (NodeIsNullOrBlack (LeftNephew) &&
            NodeIsNullOrBlack (RightNephew))
        {
          //
          // In this case we can "steal" one black value from Child and Sibling
          // each, and pass it to Parent. "Stealing" means that Sibling (black
          // value 1) becomes red, Child (black value 2) becomes singly-black,
          // and Parent will have to be examined if it can eat the
          // black-increment.
          //
          // Sibling is allowed to become red because both of its children are
          // black (property #3).
          //
          //           grandparent                             Parent
          //                |                                     |
          //            Parent,x:B                            Child,x:B
          //           /          \                          /         \_
          // Child,2b:A            Sibling,b:D    --->    b:A           r:D
          //                           /\                                /\_
          //             LeftNephew,b:C  RightNephew,b:E              b:C  b:E
          //
          Sibling->Color = RedBlackTreeRed;
          Child          = Parent;
          Parent         = Parent->Parent;
          //
          // Continue ascending.
          //
        } else {
          //
          // At least one nephew is red.
          //
          if (NodeIsNullOrBlack (RightNephew)) {
            //
            // Since the right nephew is black, the left nephew is red. Due to
            // property #3, LeftNephew has two black children, hence node E is
            // black.
            //
            // Together with the rotation, this enables us to color node F red
            // (because property #3 will be satisfied). We flip node D to black
            // to maintain property #4.
            //
            //      grandparent                         grandparent
            //           |                                   |
            //       Parent,x:B                          Parent,x:B
            //           /\                                  /\_
            // Child,2b:A  Sibling,b:F     --->    Child,2b:A  Sibling,b:D
            //                  /\                            /   \_
            //    LeftNephew,r:D  RightNephew,b:G          b:C  RightNephew,r:F
            //               /\                                       /\_
            //            b:C  b:E                                 b:E  b:G
            //
            LeftNephew->Color = RedBlackTreeBlack;
            Sibling->Color    = RedBlackTreeRed;
            RedBlackTreeRotateRight (Sibling, &NewRoot);
            Sibling     = Parent->Right;
            RightNephew = Sibling->Right;
            //
            // These operations ensure that...
            //
          }

          //
          // ... RightNephew is definitely red here, plus Sibling is (still)
          // black and non-NULL.
          //
          ASSERT (RightNephew != NULL);
          ASSERT (RightNephew->Color == RedBlackTreeRed);
          ASSERT (Sibling != NULL);
          ASSERT (Sibling->Color == RedBlackTreeBlack);
          //
          // In this case we can flush the extra black-increment immediately,
          // restoring property #1 for Child (node A): we color RightNephew
          // (node E) from red to black.
          //
          // In order to maintain property #4, we exchange colors between
          // Parent and Sibling (nodes B and D), and rotate left around Parent
          // (node B). The transformation doesn't change the black count
          // increase incurred by each partial path, eg.
          // - ascending from node A: 2 + x     == 1 + 1 + x
          // - ascending from node C: y + 1 + x == y + 1 + x
          // - ascending from node E: 0 + 1 + x == 1 + x
          //
          // The color exchange is valid, because even if x stands for red,
          // both children of node D are black after the transformation
          // (preserving property #3).
          //
          //           grandparent                                  grandparent
          //                |                                            |
          //            Parent,x:B                                      x:D
          //           /          \                                    /   \_
          // Child,2b:A            Sibling,b:D              --->    b:B     b:E
          //                         /     \                       /   \_
          //                      y:C       RightNephew,r:E     b:A     y:C
          //
          //
          Sibling->Color     = Parent->Color;
          Parent->Color      = RedBlackTreeBlack;
          RightNephew->Color = RedBlackTreeBlack;
          RedBlackTreeRotateLeft (Parent, &NewRoot);
          Child = NewRoot;
          //
          // This terminates the loop.
          //
        }
      } else {
        //
        // Mirrors the other branch.
        //
        Sibling = Parent->Left;
        ASSERT (Sibling != NULL);
        if (Sibling->Color == RedBlackTreeRed) {
          Sibling->Color = RedBlackTreeBlack;
          Parent->Color  = RedBlackTreeRed;
          RedBlackTreeRotateRight (Parent, &NewRoot);
          Sibling = Parent->Left;
          ASSERT (Sibling != NULL);
        }

        ASSERT (Sibling->Color == RedBlackTreeBlack);
        RightNephew = Sibling->Right;
        LeftNephew  = Sibling->Left;
        if (NodeIsNullOrBlack (RightNephew) &&
            NodeIsNullOrBlack (LeftNephew))
        {
          Sibling->Color = RedBlackTreeRed;
          Child          = Parent;
          Parent         = Parent->Parent;
        } else {
          if (NodeIsNullOrBlack (LeftNephew)) {
            RightNephew->Color = RedBlackTreeBlack;
            Sibling->Color     = RedBlackTreeRed;
            RedBlackTreeRotateLeft (Sibling, &NewRoot);
            Sibling    = Parent->Left;
            LeftNephew = Sibling->Left;
          }

          ASSERT (LeftNephew != NULL);
          ASSERT (LeftNephew->Color == RedBlackTreeRed);
          ASSERT (Sibling != NULL);
          ASSERT (Sibling->Color == RedBlackTreeBlack);
          Sibling->Color    = Parent->Color;
          Parent->Color     = RedBlackTreeBlack;
          LeftNephew->Color = RedBlackTreeBlack;
          RedBlackTreeRotateRight (Parent, &NewRoot);
          Child = NewRoot;
        }
      }
    }

    if (Child != NULL) {
      Child->Color = RedBlackTreeBlack;
    }
  }

  Tree->Root = NewRoot;

  if (FeaturePcdGet (PcdValidateOrderedCollection)) {
    RedBlackTreeValidate (Tree);
  }
}

/**
  Recursively check the red-black tree properties #1 to #4 on a node.

  @param[in] Node  The root of the subtree to validate.

  @retval  The black-height of Node's parent.
**/
UINT32
RedBlackTreeRecursiveCheck (
  IN CONST RED_BLACK_TREE_NODE  *Node
  )
{
  UINT32  LeftHeight;
  UINT32  RightHeight;

  //
  // property #2
  //
  if (Node == NULL) {
    return 1;
  }

  //
  // property #1
  //
  ASSERT (Node->Color == RedBlackTreeRed || Node->Color == RedBlackTreeBlack);

  //
  // property #3
  //
  if (Node->Color == RedBlackTreeRed) {
    ASSERT (NodeIsNullOrBlack (Node->Left));
    ASSERT (NodeIsNullOrBlack (Node->Right));
  }

  //
  // property #4
  //
  LeftHeight  = RedBlackTreeRecursiveCheck (Node->Left);
  RightHeight = RedBlackTreeRecursiveCheck (Node->Right);
  ASSERT (LeftHeight == RightHeight);

  return (Node->Color == RedBlackTreeBlack) + LeftHeight;
}

/**
  A slow function that asserts that the tree is a valid red-black tree, and
  that it orders user structures correctly.

  Read-only operation.

  This function uses the stack for recursion and is not recommended for
  "production use".

  @param[in] Tree  The tree to validate.
**/
VOID
RedBlackTreeValidate (
  IN CONST RED_BLACK_TREE  *Tree
  )
{
  UINT32                     BlackHeight;
  UINT32                     ForwardCount;
  UINT32                     BackwardCount;
  CONST RED_BLACK_TREE_NODE  *Last;
  CONST RED_BLACK_TREE_NODE  *Node;

  DEBUG ((DEBUG_VERBOSE, "%a: Tree=%p\n", __func__, Tree));

  //
  // property #5
  //
  ASSERT (NodeIsNullOrBlack (Tree->Root));

  //
  // check the other properties
  //
  BlackHeight = RedBlackTreeRecursiveCheck (Tree->Root) - 1;

  //
  // forward ordering
  //
  Last         = OrderedCollectionMin (Tree);
  ForwardCount = (Last != NULL);
  for (Node = OrderedCollectionNext (Last); Node != NULL;
       Node = OrderedCollectionNext (Last))
  {
    ASSERT (Tree->UserStructCompare (Last->UserStruct, Node->UserStruct) < 0);
    Last = Node;
    ++ForwardCount;
  }

  //
  // backward ordering
  //
  Last          = OrderedCollectionMax (Tree);
  BackwardCount = (Last != NULL);
  for (Node = OrderedCollectionPrev (Last); Node != NULL;
       Node = OrderedCollectionPrev (Last))
  {
    ASSERT (Tree->UserStructCompare (Last->UserStruct, Node->UserStruct) > 0);
    Last = Node;
    ++BackwardCount;
  }

  ASSERT (ForwardCount == BackwardCount);

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Tree=%p BlackHeight=%Ld Count=%Ld\n",
    __func__,
    Tree,
    (INT64)BlackHeight,
    (INT64)ForwardCount
    ));
}
