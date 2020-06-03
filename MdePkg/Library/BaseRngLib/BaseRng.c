/** @file
  Random number generator services that uses RdRand instruction access
  to provide high-quality random numbers.

Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

//
// Bit mask used to determine if RdRand instruction is supported.
//
#define RDRAND_MASK                  BIT30

//
// Limited retry number when valid random data is returned.
// Uses the recommended value defined in Section 7.3.17 of "Intel 64 and IA-32
// Architectures Software Developer's Mannual".
//
#define RDRAND_RETRY_LIMIT           10

/**
  The constructor function checks whether or not RDRAND instruction is supported
  by the host hardware.

  The constructor function checks whether or not RDRAND instruction is supported.
  It will ASSERT() if RDRAND instruction is not supported.
  It will always return RETURN_SUCCESS.

  @retval RETURN_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
RETURN_STATUS
EFIAPI
BaseRngLibConstructor (
  VOID
  )
{
  UINT32  RegEcx;

  //
  // Determine RDRAND support by examining bit 30 of the ECX register returned by
  // CPUID. A value of 1 indicates that processor support RDRAND instruction.
  //
  AsmCpuid (1, 0, 0, &RegEcx, 0);
  ASSERT ((RegEcx & RDRAND_MASK) == RDRAND_MASK);

  return RETURN_SUCCESS;
}

/**
  Generates a 16-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 16-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber16 (
  OUT     UINT16                    *Rand
  )
{
  UINT32  Index;

  ASSERT (Rand != NULL);

  //
  // A loop to fetch a 16 bit random value with a retry count limit.
  //
  for (Index = 0; Index < RDRAND_RETRY_LIMIT; Index++) {
    if (AsmRdRand16 (Rand)) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Generates a 32-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 32-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber32 (
  OUT     UINT32                    *Rand
  )
{
  UINT32  Index;

  ASSERT (Rand != NULL);

  //
  // A loop to fetch a 32 bit random value with a retry count limit.
  //
  for (Index = 0; Index < RDRAND_RETRY_LIMIT; Index++) {
    if (AsmRdRand32 (Rand)) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Generates a 64-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber64 (
  OUT     UINT64                    *Rand
  )
{
  UINT32  Index;

  ASSERT (Rand != NULL);

  //
  // A loop to fetch a 64 bit random value with a retry count limit.
  //
  for (Index = 0; Index < RDRAND_RETRY_LIMIT; Index++) {
    if (AsmRdRand64 (Rand)) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Generates a 128-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 128-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber128 (
  OUT     UINT64                    *Rand
  )
{
  ASSERT (Rand != NULL);

  //
  // Read first 64 bits
  //
  if (!GetRandomNumber64 (Rand)) {
    return FALSE;
  }

  //
  // Read second 64 bits
  //
  return GetRandomNumber64 (++Rand);
}
