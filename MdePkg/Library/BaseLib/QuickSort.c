/** @file
  Math worker functions.

  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  This function is identical to perform QuickSort,
  except that is uses the pre-allocated buffer so the in place sorting does not need to
  allocate and free buffers constantly.

  Each element must be equal sized.

  if BufferToSort is NULL, then ASSERT.
  if CompareFunction is NULL, then ASSERT.
  if BufferOneElement is NULL, then ASSERT.
  if ElementSize is < 1, then ASSERT.

  if Count is < 2 then perform no action.

  @param[in, out] BufferToSort   on call a Buffer of (possibly sorted) elements
                                 on return a buffer of sorted elements
  @param[in] Count               the number of elements in the buffer to sort
  @param[in] ElementSize         Size of an element in bytes
  @param[in] CompareFunction     The function to call to perform the comparison
                                 of any 2 elements
  @param[out] BufferOneElement   Caller provided buffer whose size equals to ElementSize.
                                 It's used by QuickSort() for swapping in sorting.
**/
VOID
EFIAPI
QuickSort (
  IN OUT VOID                 *BufferToSort,
  IN CONST UINTN              Count,
  IN CONST UINTN              ElementSize,
  IN       BASE_SORT_COMPARE  CompareFunction,
  OUT VOID                    *BufferOneElement
  )
{
  VOID   *Pivot;
  UINTN  LoopCount;
  UINTN  NextSwapLocation;

  ASSERT (BufferToSort     != NULL);
  ASSERT (CompareFunction  != NULL);
  ASSERT (BufferOneElement != NULL);
  ASSERT (ElementSize      >= 1);

  if (Count < 2) {
    return;
  }

  NextSwapLocation = 0;

  //
  // pick a pivot (we choose last element)
  //
  Pivot = ((UINT8 *)BufferToSort + ((Count - 1) * ElementSize));

  //
  // Now get the pivot such that all on "left" are below it
  // and everything "right" are above it
  //
  for (LoopCount = 0; LoopCount < Count -1; LoopCount++) {
    //
    // if the element is less than or equal to the pivot
    //
    if (CompareFunction ((VOID *)((UINT8 *)BufferToSort + ((LoopCount) * ElementSize)), Pivot) <= 0) {
      //
      // swap
      //
      CopyMem (BufferOneElement, (UINT8 *)BufferToSort + (NextSwapLocation * ElementSize), ElementSize);
      CopyMem ((UINT8 *)BufferToSort + (NextSwapLocation * ElementSize), (UINT8 *)BufferToSort + ((LoopCount) * ElementSize), ElementSize);
      CopyMem ((UINT8 *)BufferToSort + ((LoopCount)*ElementSize), BufferOneElement, ElementSize);

      //
      // increment NextSwapLocation
      //
      NextSwapLocation++;
    }
  }

  //
  // swap pivot to it's final position (NextSwapLocation)
  //
  CopyMem (BufferOneElement, Pivot, ElementSize);
  CopyMem (Pivot, (UINT8 *)BufferToSort + (NextSwapLocation * ElementSize), ElementSize);
  CopyMem ((UINT8 *)BufferToSort + (NextSwapLocation * ElementSize), BufferOneElement, ElementSize);

  //
  // Now recurse on 2 partial lists.  neither of these will have the 'pivot' element
  // IE list is sorted left half, pivot element, sorted right half...
  //
  if (NextSwapLocation >= 2) {
    QuickSort (
      BufferToSort,
      NextSwapLocation,
      ElementSize,
      CompareFunction,
      BufferOneElement
      );
  }

  if ((Count - NextSwapLocation - 1) >= 2) {
    QuickSort (
      (UINT8 *)BufferToSort + (NextSwapLocation + 1) * ElementSize,
      Count - NextSwapLocation - 1,
      ElementSize,
      CompareFunction,
      BufferOneElement
      );
  }
}
