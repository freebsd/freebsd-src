/** @file
  Linked List Library Functions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  If PcdVerifyNodeInList is TRUE, ASSERTs when SecondEntry is or is not part of
  the same doubly-linked list as FirstEntry depending on the value of InList.
  Independent of PcdVerifyNodeInList, ASSERTs when FirstEntry is not part of a
  valid list.

  If FirstEntry is NULL, then ASSERT().
  If FirstEntry->ForwardLink is NULL, then ASSERT().
  If FirstEntry->BackLink is NULL, then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and List contains more than
  PcdMaximumLinkedListLength nodes, then ASSERT().
  If PcdVerifyNodeInList is TRUE and SecondEntry is NULL, then ASSERT().

  @param  FirstEntry   A pointer to a node in a linked list.
  @param  SecondEntry  A pointer to the node to locate.
  @param  InList       Defines whether to check if SecondEntry is or is not part
                       of the same doubly-linked list as FirstEntry.

**/
#if !defined (MDEPKG_NDEBUG)
#define ASSERT_VERIFY_NODE_IN_VALID_LIST(FirstEntry, SecondEntry, InList)  \
    do {                                                                     \
      if (FeaturePcdGet (PcdVerifyNodeInList)) {                             \
        ASSERT (InList == IsNodeInList ((FirstEntry), (SecondEntry)));       \
      } else {                                                               \
        ASSERT (InternalBaseLibIsListValid (FirstEntry));                    \
      }                                                                      \
    } while (FALSE)
#else
#define ASSERT_VERIFY_NODE_IN_VALID_LIST(FirstEntry, SecondEntry, InList)
#endif

/**
  Worker function that verifies the validity of this list.

  If List is NULL, then ASSERT().
  If List->ForwardLink is NULL, then ASSERT().
  If List->BackLink is NULL, then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and List contains more than
  PcdMaximumLinkedListLength nodes, then ASSERT().

  @param  List              A pointer to a node in a linked list.

  @retval   TRUE if PcdVerifyNodeInList is FALSE
  @retval   TRUE if DoMembershipCheck is FALSE
  @retval   TRUE if PcdVerifyNodeInList is TRUE and DoMembershipCheck is TRUE
            and Node is a member of List.
  @retval   FALSE if PcdVerifyNodeInList is TRUE and DoMembershipCheck is TRUE
            and Node is in not a member of List.

**/
BOOLEAN
EFIAPI
InternalBaseLibIsListValid (
  IN CONST LIST_ENTRY  *List
  )
{
  UINTN             Count;
  CONST LIST_ENTRY  *Ptr;

  //
  // Test the validity of List and Node
  //
  ASSERT (List != NULL);
  ASSERT (List->ForwardLink != NULL);
  ASSERT (List->BackLink != NULL);

  if (PcdGet32 (PcdMaximumLinkedListLength) > 0) {
    Count = 0;
    Ptr   = List;

    //
    // Count the total number of nodes in List.
    // Exit early if the number of nodes in List >= PcdMaximumLinkedListLength
    //
    do {
      Ptr = Ptr->ForwardLink;
      Count++;
    } while ((Ptr != List) && (Count < PcdGet32 (PcdMaximumLinkedListLength)));

    //
    // return whether linked list is too long
    //
    return (BOOLEAN)(Count < PcdGet32 (PcdMaximumLinkedListLength));
  }

  return TRUE;
}

/**
  Checks whether FirstEntry and SecondEntry are part of the same doubly-linked
  list.

  If FirstEntry is NULL, then ASSERT().
  If FirstEntry->ForwardLink is NULL, then ASSERT().
  If FirstEntry->BackLink is NULL, then ASSERT().
  If SecondEntry is NULL, then ASSERT();
  If PcdMaximumLinkedListLength is not zero, and List contains more than
  PcdMaximumLinkedListLength nodes, then ASSERT().

  @param  FirstEntry   A pointer to a node in a linked list.
  @param  SecondEntry  A pointer to the node to locate.

  @retval TRUE   SecondEntry is in the same doubly-linked list as FirstEntry.
  @retval FALSE  SecondEntry isn't in the same doubly-linked list as FirstEntry,
                 or FirstEntry is invalid.

**/
BOOLEAN
EFIAPI
IsNodeInList (
  IN      CONST LIST_ENTRY  *FirstEntry,
  IN      CONST LIST_ENTRY  *SecondEntry
  )
{
  UINTN             Count;
  CONST LIST_ENTRY  *Ptr;

  //
  // ASSERT List not too long
  //
  ASSERT (InternalBaseLibIsListValid (FirstEntry));

  ASSERT (SecondEntry != NULL);

  Count = 0;
  Ptr   = FirstEntry;

  //
  // Check to see if SecondEntry is a member of FirstEntry.
  // Exit early if the number of nodes in List >= PcdMaximumLinkedListLength
  //
  do {
    Ptr = Ptr->ForwardLink;
    if (PcdGet32 (PcdMaximumLinkedListLength) > 0) {
      Count++;

      //
      // Return if the linked list is too long
      //
      if (Count == PcdGet32 (PcdMaximumLinkedListLength)) {
        return (BOOLEAN)(Ptr == SecondEntry);
      }
    }

    if (Ptr == SecondEntry) {
      return TRUE;
    }
  } while (Ptr != FirstEntry);

  return FALSE;
}

/**
  Initializes the head node of a doubly-linked list, and returns the pointer to
  the head node of the doubly-linked list.

  Initializes the forward and backward links of a new linked list. After
  initializing a linked list with this function, the other linked list
  functions may be used to add and remove nodes from the linked list. It is up
  to the caller of this function to allocate the memory for ListHead.

  If ListHead is NULL, then ASSERT().

  @param  ListHead  A pointer to the head node of a new doubly-linked list.

  @return ListHead

**/
LIST_ENTRY *
EFIAPI
InitializeListHead (
  IN OUT  LIST_ENTRY  *ListHead
  )

{
  ASSERT (ListHead != NULL);

  ListHead->ForwardLink = ListHead;
  ListHead->BackLink    = ListHead;
  return ListHead;
}

/**
  Adds a node to the beginning of a doubly-linked list, and returns the pointer
  to the head node of the doubly-linked list.

  Adds the node Entry at the beginning of the doubly-linked list denoted by
  ListHead, and returns ListHead.

  If ListHead is NULL, then ASSERT().
  If Entry is NULL, then ASSERT().
  If ListHead was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and prior to insertion the number
  of nodes in ListHead, including the ListHead node, is greater than or
  equal to PcdMaximumLinkedListLength, then ASSERT().

  @param  ListHead  A pointer to the head node of a doubly-linked list.
  @param  Entry     A pointer to a node that is to be inserted at the beginning
                    of a doubly-linked list.

  @return ListHead

**/
LIST_ENTRY *
EFIAPI
InsertHeadList (
  IN OUT  LIST_ENTRY  *ListHead,
  IN OUT  LIST_ENTRY  *Entry
  )
{
  //
  // ASSERT List not too long and Entry is not one of the nodes of List
  //
  ASSERT_VERIFY_NODE_IN_VALID_LIST (ListHead, Entry, FALSE);

  Entry->ForwardLink           = ListHead->ForwardLink;
  Entry->BackLink              = ListHead;
  Entry->ForwardLink->BackLink = Entry;
  ListHead->ForwardLink        = Entry;
  return ListHead;
}

/**
  Adds a node to the end of a doubly-linked list, and returns the pointer to
  the head node of the doubly-linked list.

  Adds the node Entry to the end of the doubly-linked list denoted by ListHead,
  and returns ListHead.

  If ListHead is NULL, then ASSERT().
  If Entry is NULL, then ASSERT().
  If ListHead was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and prior to insertion the number
  of nodes in ListHead, including the ListHead node, is greater than or
  equal to PcdMaximumLinkedListLength, then ASSERT().

  @param  ListHead  A pointer to the head node of a doubly-linked list.
  @param  Entry     A pointer to a node that is to be added at the end of the
                    doubly-linked list.

  @return ListHead

**/
LIST_ENTRY *
EFIAPI
InsertTailList (
  IN OUT  LIST_ENTRY  *ListHead,
  IN OUT  LIST_ENTRY  *Entry
  )
{
  //
  // ASSERT List not too long and Entry is not one of the nodes of List
  //
  ASSERT_VERIFY_NODE_IN_VALID_LIST (ListHead, Entry, FALSE);

  Entry->ForwardLink           = ListHead;
  Entry->BackLink              = ListHead->BackLink;
  Entry->BackLink->ForwardLink = Entry;
  ListHead->BackLink           = Entry;
  return ListHead;
}

/**
  Retrieves the first node of a doubly-linked list.

  Returns the first node of a doubly-linked list.  List must have been
  initialized with INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead().
  If List is empty, then List is returned.

  If List is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes
  in List, including the List node, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().

  @param  List  A pointer to the head node of a doubly-linked list.

  @return The first node of a doubly-linked list.
  @retval List  The list is empty.

**/
LIST_ENTRY *
EFIAPI
GetFirstNode (
  IN      CONST LIST_ENTRY  *List
  )
{
  //
  // ASSERT List not too long
  //
  ASSERT (InternalBaseLibIsListValid (List));

  return List->ForwardLink;
}

/**
  Retrieves the next node of a doubly-linked list.

  Returns the node of a doubly-linked list that follows Node.
  List must have been initialized with INTIALIZE_LIST_HEAD_VARIABLE()
  or InitializeListHead().  If List is empty, then List is returned.

  If List is NULL, then ASSERT().
  If Node is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and List contains more than
  PcdMaximumLinkedListLength nodes, then ASSERT().
  If PcdVerifyNodeInList is TRUE and Node is not a node in List, then ASSERT().

  @param  List  A pointer to the head node of a doubly-linked list.
  @param  Node  A pointer to a node in the doubly-linked list.

  @return A pointer to the next node if one exists. Otherwise List is returned.

**/
LIST_ENTRY *
EFIAPI
GetNextNode (
  IN      CONST LIST_ENTRY  *List,
  IN      CONST LIST_ENTRY  *Node
  )
{
  //
  // ASSERT List not too long and Node is one of the nodes of List
  //
  ASSERT_VERIFY_NODE_IN_VALID_LIST (List, Node, TRUE);

  return Node->ForwardLink;
}

/**
  Retrieves the previous node of a doubly-linked list.

  Returns the node of a doubly-linked list that precedes Node.
  List must have been initialized with INTIALIZE_LIST_HEAD_VARIABLE()
  or InitializeListHead().  If List is empty, then List is returned.

  If List is NULL, then ASSERT().
  If Node is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and List contains more than
  PcdMaximumLinkedListLength nodes, then ASSERT().
  If PcdVerifyNodeInList is TRUE and Node is not a node in List, then ASSERT().

  @param  List  A pointer to the head node of a doubly-linked list.
  @param  Node  A pointer to a node in the doubly-linked list.

  @return A pointer to the previous node if one exists. Otherwise List is returned.

**/
LIST_ENTRY *
EFIAPI
GetPreviousNode (
  IN      CONST LIST_ENTRY  *List,
  IN      CONST LIST_ENTRY  *Node
  )
{
  //
  // ASSERT List not too long and Node is one of the nodes of List
  //
  ASSERT_VERIFY_NODE_IN_VALID_LIST (List, Node, TRUE);

  return Node->BackLink;
}

/**
  Checks to see if a doubly-linked list is empty or not.

  Checks to see if the doubly-linked list is empty. If the linked list contains
  zero nodes, this function returns TRUE. Otherwise, it returns FALSE.

  If ListHead is NULL, then ASSERT().
  If ListHead was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes
  in List, including the List node, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().

  @param  ListHead  A pointer to the head node of a doubly-linked list.

  @retval TRUE  The linked list is empty.
  @retval FALSE The linked list is not empty.

**/
BOOLEAN
EFIAPI
IsListEmpty (
  IN      CONST LIST_ENTRY  *ListHead
  )
{
  //
  // ASSERT List not too long
  //
  ASSERT (InternalBaseLibIsListValid (ListHead));

  return (BOOLEAN)(ListHead->ForwardLink == ListHead);
}

/**
  Determines if a node in a doubly-linked list is the head node of a the same
  doubly-linked list.  This function is typically used to terminate a loop that
  traverses all the nodes in a doubly-linked list starting with the head node.

  Returns TRUE if Node is equal to List.  Returns FALSE if Node is one of the
  nodes in the doubly-linked list specified by List.  List must have been
  initialized with INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead().

  If List is NULL, then ASSERT().
  If Node is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead(),
  then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes
  in List, including the List node, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().
  If PcdVerifyNodeInList is TRUE and Node is not a node in List and Node is not
  equal to List, then ASSERT().

  @param  List  A pointer to the head node of a doubly-linked list.
  @param  Node  A pointer to a node in the doubly-linked list.

  @retval TRUE  Node is the head of the doubly-linked list pointed by List.
  @retval FALSE Node is not the head of the doubly-linked list pointed by List.

**/
BOOLEAN
EFIAPI
IsNull (
  IN      CONST LIST_ENTRY  *List,
  IN      CONST LIST_ENTRY  *Node
  )
{
  //
  // ASSERT List not too long and Node is one of the nodes of List
  //
  ASSERT_VERIFY_NODE_IN_VALID_LIST (List, Node, TRUE);

  return (BOOLEAN)(Node == List);
}

/**
  Determines if a node the last node in a doubly-linked list.

  Returns TRUE if Node is the last node in the doubly-linked list specified by
  List. Otherwise, FALSE is returned. List must have been initialized with
  INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead().

  If List is NULL, then ASSERT().
  If Node is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes
  in List, including the List node, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().
  If PcdVerifyNodeInList is TRUE and Node is not a node in List, then ASSERT().

  @param  List  A pointer to the head node of a doubly-linked list.
  @param  Node  A pointer to a node in the doubly-linked list.

  @retval TRUE  Node is the last node in the linked list.
  @retval FALSE Node is not the last node in the linked list.

**/
BOOLEAN
EFIAPI
IsNodeAtEnd (
  IN      CONST LIST_ENTRY  *List,
  IN      CONST LIST_ENTRY  *Node
  )
{
  //
  // ASSERT List not too long and Node is one of the nodes of List
  //
  ASSERT_VERIFY_NODE_IN_VALID_LIST (List, Node, TRUE);

  return (BOOLEAN)(!IsNull (List, Node) && List->BackLink == Node);
}

/**
  Swaps the location of two nodes in a doubly-linked list, and returns the
  first node after the swap.

  If FirstEntry is identical to SecondEntry, then SecondEntry is returned.
  Otherwise, the location of the FirstEntry node is swapped with the location
  of the SecondEntry node in a doubly-linked list. SecondEntry must be in the
  same double linked list as FirstEntry and that double linked list must have
  been initialized with INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead().
  SecondEntry is returned after the nodes are swapped.

  If FirstEntry is NULL, then ASSERT().
  If SecondEntry is NULL, then ASSERT().
  If PcdVerifyNodeInList is TRUE and SecondEntry and FirstEntry are not in the
  same linked list, then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes in the
  linked list containing the FirstEntry and SecondEntry nodes, including
  the FirstEntry and SecondEntry nodes, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().

  @param  FirstEntry  A pointer to a node in a linked list.
  @param  SecondEntry A pointer to another node in the same linked list.

  @return SecondEntry.

**/
LIST_ENTRY *
EFIAPI
SwapListEntries (
  IN OUT  LIST_ENTRY  *FirstEntry,
  IN OUT  LIST_ENTRY  *SecondEntry
  )
{
  LIST_ENTRY  *Ptr;

  if (FirstEntry == SecondEntry) {
    return SecondEntry;
  }

  //
  // ASSERT Entry1 and Entry2 are in the same linked list
  //
  ASSERT_VERIFY_NODE_IN_VALID_LIST (FirstEntry, SecondEntry, TRUE);

  //
  // Ptr is the node pointed to by FirstEntry->ForwardLink
  //
  Ptr = RemoveEntryList (FirstEntry);

  //
  // If FirstEntry immediately follows SecondEntry, FirstEntry will be placed
  // immediately in front of SecondEntry
  //
  if (Ptr->BackLink == SecondEntry) {
    return InsertTailList (SecondEntry, FirstEntry);
  }

  //
  // Ptr == SecondEntry means SecondEntry immediately follows FirstEntry,
  // then there are no further steps necessary
  //
  if (Ptr == InsertHeadList (SecondEntry, FirstEntry)) {
    return Ptr;
  }

  //
  // Move SecondEntry to the front of Ptr
  //
  RemoveEntryList (SecondEntry);
  InsertTailList (Ptr, SecondEntry);
  return SecondEntry;
}

/**
  Removes a node from a doubly-linked list, and returns the node that follows
  the removed node.

  Removes the node Entry from a doubly-linked list. It is up to the caller of
  this function to release the memory used by this node if that is required. On
  exit, the node following Entry in the doubly-linked list is returned. If
  Entry is the only node in the linked list, then the head node of the linked
  list is returned.

  If Entry is NULL, then ASSERT().
  If Entry is the head node of an empty list, then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes in the
  linked list containing Entry, including the Entry node, is greater than
  or equal to PcdMaximumLinkedListLength, then ASSERT().

  @param  Entry A pointer to a node in a linked list.

  @return Entry.

**/
LIST_ENTRY *
EFIAPI
RemoveEntryList (
  IN      CONST LIST_ENTRY  *Entry
  )
{
  ASSERT (!IsListEmpty (Entry));

  Entry->ForwardLink->BackLink = Entry->BackLink;
  Entry->BackLink->ForwardLink = Entry->ForwardLink;
  return Entry->ForwardLink;
}
