/** @file
  Default PCI Segment Information Library that returns one segment whose
  segment base address equals to PcdPciExpressBaseAddress.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/PciSegmentInfoLib.h>
#include <Library/DebugLib.h>

/**
  Return an array of PCI_SEGMENT_INFO holding the segment information.

  Note: The returned array/buffer is owned by callee.

  @param  Count  Return the count of segments.

  @retval A callee owned array holding the segment information.
**/
PCI_SEGMENT_INFO *
GetPciSegmentInfo (
  UINTN  *Count
  )
{
  ASSERT (FALSE);
  *Count = 0;
  return NULL;
}
