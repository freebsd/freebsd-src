/** @file
  Internal function to get spin lock alignment.

  Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseSynchronizationLibInternals.h"

/**
  Internal function to retrieve the architecture specific spin lock alignment
  requirements for optimal spin lock performance.

  @return The architecture specific spin lock alignment.

**/
UINTN
InternalGetSpinLockProperties (
  VOID
  )
{
  UINT32  RegEax;
  UINT32  RegEbx;
  UINTN   FamilyId;
  UINTN   ModelId;
  UINTN   CacheLineSize;

  //
  // Retrieve CPUID Version Information
  //
  AsmCpuid (0x01, &RegEax, &RegEbx, NULL, NULL);
  //
  // EBX: Bits 15 - 08: CLFLUSH line size (Value * 8 = cache line size)
  //
  CacheLineSize = ((RegEbx >> 8) & 0xff) * 8;
  //
  // Retrieve CPU Family and Model
  //
  FamilyId = (RegEax >> 8) & 0xf;
  ModelId  = (RegEax >> 4) & 0xf;
  if (FamilyId == 0x0f) {
    //
    // In processors based on Intel NetBurst microarchitecture, use two cache lines
    //
    ModelId = ModelId | ((RegEax >> 12) & 0xf0);
    if ((ModelId <= 0x04) || (ModelId == 0x06)) {
      CacheLineSize *= 2;
    }
  }

  if (CacheLineSize < 32) {
    CacheLineSize = 32;
  }

  return CacheLineSize;
}
