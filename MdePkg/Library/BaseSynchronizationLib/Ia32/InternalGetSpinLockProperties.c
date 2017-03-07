/** @file
  Internal function to get spin lock alignment.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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
    if (ModelId <= 0x04 || ModelId == 0x06) {
      CacheLineSize *= 2;
    }
  }

  if (CacheLineSize < 32) {
    CacheLineSize = 32;
  }

  return CacheLineSize;
}

