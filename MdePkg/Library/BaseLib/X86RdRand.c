/** @file
  IA-32/x64 AsmRdRandxx()
  Generates random number through CPU RdRand instruction.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "BaseLibInternals.h"

/**
  Generates a 16-bit random number through RDRAND instruction.

  if Rand is NULL, then ASSERT().

  @param[out]  Rand     Buffer pointer to store the random result.

  @retval TRUE          RDRAND call was successful.
  @retval FALSE         Failed attempts to call RDRAND.

 **/
BOOLEAN
EFIAPI
AsmRdRand16 (
  OUT     UINT16                    *Rand
  )
{
  ASSERT (Rand != NULL);
  return InternalX86RdRand16 (Rand);
}

/**
  Generates a 32-bit random number through RDRAND instruction.

  if Rand is NULL, then ASSERT().

  @param[out]  Rand     Buffer pointer to store the random result.

  @retval TRUE          RDRAND call was successful.
  @retval FALSE         Failed attempts to call RDRAND.

**/
BOOLEAN
EFIAPI
AsmRdRand32 (
  OUT     UINT32                    *Rand
  )
{
  ASSERT (Rand != NULL);
  return InternalX86RdRand32 (Rand);
}

/**
  Generates a 64-bit random number through RDRAND instruction.

  if Rand is NULL, then ASSERT().

  @param[out]  Rand     Buffer pointer to store the random result.

  @retval TRUE          RDRAND call was successful.
  @retval FALSE         Failed attempts to call RDRAND.

**/
BOOLEAN
EFIAPI
AsmRdRand64  (
  OUT     UINT64                    *Rand
  )
{
  ASSERT (Rand != NULL);
  return InternalX86RdRand64 (Rand);
}
