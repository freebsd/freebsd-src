/** @file
  An ordered collection library interface.

  The library class provides a set of APIs to manage an ordered collection of
  items.

  Copyright (C) 2014, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License that accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __ORDERED_COLLECTION_LIB__
#define __ORDERED_COLLECTION_LIB__

#include <Base.h>

//
// Opaque structure for a collection.
//
typedef struct ORDERED_COLLECTION ORDERED_COLLECTION;

//
// Opaque structure for collection entries.
//
// Collection entries do not take ownership of the associated user structures,
// they only link them. This makes it easy to link the same user structure into
// several collections. If reference counting is required, the caller is
// responsible for implementing it, as part of the user structure.
//
// A pointer-to-ORDERED_COLLECTION_ENTRY is considered an "iterator". Multiple,
// simultaneous iterations are supported.
//
typedef struct ORDERED_COLLECTION_ENTRY ORDERED_COLLECTION_ENTRY;

//
// Altering the key field of an in-collection user structure (ie. the portion
// of the user structure that ORDERED_COLLECTION_USER_COMPARE and
// ORDERED_COLLECTION_KEY_COMPARE, below, read) is not allowed in-place. The
// caller is responsible for bracketing the key change with the deletion and
// the reinsertion of the user structure, so that the changed key value is
// reflected in the collection.
//

/**
  Comparator function type for two user structures.

  @param[in] UserStruct1  Pointer to the first user structure.

  @param[in] UserStruct2  Pointer to the second user structure.

  @retval <0  If UserStruct1 compares less than UserStruct2.

  @retval  0  If UserStruct1 compares equal to UserStruct2.

  @retval >0  If UserStruct1 compares greater than UserStruct2.
**/
typedef
INTN
(EFIAPI *ORDERED_COLLECTION_USER_COMPARE)(
  IN CONST VOID *UserStruct1,
  IN CONST VOID *UserStruct2
  );

/**
  Compare a standalone key against a user structure containing an embedded key.

  @param[in] StandaloneKey  Pointer to the bare key.

  @param[in] UserStruct     Pointer to the user structure with the embedded
                            key.

  @retval <0  If StandaloneKey compares less than UserStruct's key.

  @retval  0  If StandaloneKey compares equal to UserStruct's key.

  @retval >0  If StandaloneKey compares greater than UserStruct's key.
**/
typedef
INTN
(EFIAPI *ORDERED_COLLECTION_KEY_COMPARE)(
  IN CONST VOID *StandaloneKey,
  IN CONST VOID *UserStruct
  );


//
// Some functions below are read-only, while others are read-write. If any
// write operation is expected to run concurrently with any other operation on
// the same collection, then the caller is responsible for implementing locking
// for the whole collection.
//

/**
  Retrieve the user structure linked by the specified collection entry.

  Read-only operation.

  @param[in] Entry  Pointer to the collection entry whose associated user
                    structure we want to retrieve. The caller is responsible
                    for passing a non-NULL argument.

  @return  Pointer to user structure linked by Entry.
**/
VOID *
EFIAPI
OrderedCollectionUserStruct (
  IN CONST ORDERED_COLLECTION_ENTRY *Entry
  );


/**
  Allocate and initialize the ORDERED_COLLECTION structure.

  @param[in]  UserStructCompare  This caller-provided function will be used to
                                 order two user structures linked into the
                                 collection, during the insertion procedure.

  @param[in]  KeyCompare         This caller-provided function will be used to
                                 order the standalone search key against user
                                 structures linked into the collection, during
                                 the lookup procedure.

  @retval NULL  If allocation failed.

  @return       Pointer to the allocated, initialized ORDERED_COLLECTION
                structure, otherwise.
**/
ORDERED_COLLECTION *
EFIAPI
OrderedCollectionInit (
  IN ORDERED_COLLECTION_USER_COMPARE UserStructCompare,
  IN ORDERED_COLLECTION_KEY_COMPARE  KeyCompare
  );


/**
  Check whether the collection is empty (has no entries).

  Read-only operation.

  @param[in] Collection  The collection to check for emptiness.

  @retval TRUE   The collection is empty.

  @retval FALSE  The collection is not empty.
**/
BOOLEAN
EFIAPI
OrderedCollectionIsEmpty (
  IN CONST ORDERED_COLLECTION *Collection
  );


/**
  Uninitialize and release an empty ORDERED_COLLECTION structure.

  Read-write operation.

  It is the caller's responsibility to delete all entries from the collection
  before calling this function.

  @param[in] Collection  The empty collection to uninitialize and release.
**/
VOID
EFIAPI
OrderedCollectionUninit (
  IN ORDERED_COLLECTION *Collection
  );


/**
  Look up the collection entry that links the user structure that matches the
  specified standalone key.

  Read-only operation.

  @param[in] Collection     The collection to search for StandaloneKey.

  @param[in] StandaloneKey  The key to locate among the user structures linked
                            into Collection. StandaloneKey will be passed to
                            ORDERED_COLLECTION_KEY_COMPARE.

  @retval NULL  StandaloneKey could not be found.

  @return       The collection entry that links to the user structure matching
                StandaloneKey, otherwise.
**/
ORDERED_COLLECTION_ENTRY *
EFIAPI
OrderedCollectionFind (
  IN CONST ORDERED_COLLECTION *Collection,
  IN CONST VOID               *StandaloneKey
  );


/**
  Find the collection entry of the minimum user structure stored in the
  collection.

  Read-only operation.

  @param[in] Collection  The collection to return the minimum entry of. The
                         user structure linked by the minimum entry compares
                         less than all other user structures in the collection.

  @retval NULL  If Collection is empty.

  @return       The collection entry that links the minimum user structure,
                otherwise.
**/
ORDERED_COLLECTION_ENTRY *
EFIAPI
OrderedCollectionMin (
  IN CONST ORDERED_COLLECTION *Collection
  );


/**
  Find the collection entry of the maximum user structure stored in the
  collection.

  Read-only operation.

  @param[in] Collection  The collection to return the maximum entry of. The
                         user structure linked by the maximum entry compares
                         greater than all other user structures in the
                         collection.

  @retval NULL  If Collection is empty.

  @return       The collection entry that links the maximum user structure,
                otherwise.
**/
ORDERED_COLLECTION_ENTRY *
EFIAPI
OrderedCollectionMax (
  IN CONST ORDERED_COLLECTION *Collection
  );


/**
  Get the collection entry of the least user structure that is greater than the
  one linked by Entry.

  Read-only operation.

  @param[in] Entry  The entry to get the successor entry of.

  @retval NULL  If Entry is NULL, or Entry is the maximum entry of its
                containing collection (ie. Entry has no successor entry).

  @return       The collection entry linking the least user structure that is
                greater than the one linked by Entry, otherwise.
**/
ORDERED_COLLECTION_ENTRY *
EFIAPI
OrderedCollectionNext (
  IN CONST ORDERED_COLLECTION_ENTRY *Entry
  );


/**
  Get the collection entry of the greatest user structure that is less than the
  one linked by Entry.

  Read-only operation.

  @param[in] Entry  The entry to get the predecessor entry of.

  @retval NULL  If Entry is NULL, or Entry is the minimum entry of its
                containing collection (ie. Entry has no predecessor entry).

  @return       The collection entry linking the greatest user structure that
                is less than the one linked by Entry, otherwise.
**/
ORDERED_COLLECTION_ENTRY *
EFIAPI
OrderedCollectionPrev (
  IN CONST ORDERED_COLLECTION_ENTRY *Entry
  );


/**
  Insert (link) a user structure into the collection, allocating a new
  collection entry.

  Read-write operation.

  @param[in,out] Collection  The collection to insert UserStruct into.

  @param[out]    Entry       The meaning of this optional, output-only
                             parameter depends on the return value of the
                             function.

                             When insertion is successful (RETURN_SUCCESS),
                             Entry is set on output to the new collection entry
                             that now links UserStruct.

                             When insertion fails due to lack of memory
                             (RETURN_OUT_OF_RESOURCES), Entry is not changed.

                             When insertion fails due to key collision (ie.
                             another user structure is already in the
                             collection that compares equal to UserStruct),
                             with return value RETURN_ALREADY_STARTED, then
                             Entry is set on output to the entry that links the
                             colliding user structure. This enables
                             "find-or-insert" in one function call, or helps
                             with later removal of the colliding element.

  @param[in]     UserStruct  The user structure to link into the collection.
                             UserStruct is ordered against in-collection user
                             structures with the
                             ORDERED_COLLECTION_USER_COMPARE function.

  @retval RETURN_SUCCESS           Insertion successful. A new collection entry
                                   has been allocated, linking UserStruct. The
                                   new collection entry is reported back in
                                   Entry (if the caller requested it).

                                   Existing ORDERED_COLLECTION_ENTRY pointers
                                   into Collection remain valid. For example,
                                   on-going iterations in the caller can
                                   continue with OrderedCollectionNext() /
                                   OrderedCollectionPrev(), and they will
                                   return the new entry at some point if user
                                   structure order dictates it.

  @retval RETURN_OUT_OF_RESOURCES  The function failed to allocate memory for
                                   the new collection entry. The collection has
                                   not been changed. Existing
                                   ORDERED_COLLECTION_ENTRY pointers into
                                   Collection remain valid.

  @retval RETURN_ALREADY_STARTED   A user structure has been found in the
                                   collection that compares equal to
                                   UserStruct. The entry linking the colliding
                                   user structure is reported back in Entry (if
                                   the caller requested it). The collection has
                                   not been changed. Existing
                                   ORDERED_COLLECTION_ENTRY pointers into
                                   Collection remain valid.
**/
RETURN_STATUS
EFIAPI
OrderedCollectionInsert (
  IN OUT ORDERED_COLLECTION       *Collection,
  OUT    ORDERED_COLLECTION_ENTRY **Entry      OPTIONAL,
  IN     VOID                     *UserStruct
  );


/**
  Delete an entry from the collection, unlinking the associated user structure.

  Read-write operation.

  @param[in,out] Collection  The collection to delete Entry from.

  @param[in]     Entry       The collection entry to delete from Collection.
                             The caller is responsible for ensuring that Entry
                             belongs to Collection, and that Entry is non-NULL
                             and valid. Entry is typically an earlier return
                             value, or output parameter, of:

                             - OrderedCollectionFind(), for deleting an entry
                               by user structure key,

                             - OrderedCollectionMin() / OrderedCollectionMax(),
                               for deleting the minimum / maximum entry,

                             - OrderedCollectionNext() /
                               OrderedCollectionPrev(), for deleting an entry
                               found during an iteration,

                             - OrderedCollectionInsert() with return value
                               RETURN_ALREADY_STARTED, for deleting an entry
                               whose linked user structure caused collision
                               during insertion.

                             Existing ORDERED_COLLECTION_ENTRY pointers (ie.
                             iterators) *different* from Entry remain valid.
                             For example:

                             - OrderedCollectionNext() /
                               OrderedCollectionPrev() iterations in the caller
                               can be continued from Entry, if
                               OrderedCollectionNext() or
                               OrderedCollectionPrev() is called on Entry
                               *before* OrderedCollectionDelete() is. That is,
                               fetch the successor / predecessor entry first,
                               then delete Entry.

                             - On-going iterations in the caller that would
                               have otherwise returned Entry at some point, as
                               dictated by user structure order, will correctly
                               reflect the absence of Entry after
                               OrderedCollectionDelete() is called
                               mid-iteration.

  @param[out]    UserStruct  If the caller provides this optional output-only
                             parameter, then on output it is set to the user
                             structure originally linked by Entry (which is now
                             freed).

                             This is a convenience that may save the caller a
                             OrderedCollectionUserStruct() invocation before
                             calling OrderedCollectionDelete(), in order to
                             retrieve the user structure being unlinked.
**/
VOID
EFIAPI
OrderedCollectionDelete (
  IN OUT ORDERED_COLLECTION       *Collection,
  IN     ORDERED_COLLECTION_ENTRY *Entry,
  OUT    VOID                     **UserStruct OPTIONAL
  );

#endif
