/** @file
  Random number generator services that uses CPU RNG instructions to
  provide random numbers.

Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>
Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "BaseRngLibInternals.h"

//
// Limited retry number when valid random data is returned.
// Uses the recommended value defined in Section 7.3.17 of "Intel 64 and IA-32
// Architectures Software Developer's Manual".
//
#define GETRANDOM_RETRY_LIMIT  10

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
  OUT     UINT16  *Rand
  )
{
  UINT32  Index;

  ASSERT (Rand != NULL);

  if (Rand == NULL) {
    return FALSE;
  }

  if (!ArchIsRngSupported ()) {
    return FALSE;
  }

  //
  // A loop to fetch a 16 bit random value with a retry count limit.
  //
  for (Index = 0; Index < GETRANDOM_RETRY_LIMIT; Index++) {
    if (ArchGetRandomNumber16 (Rand)) {
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
  OUT     UINT32  *Rand
  )
{
  UINT32  Index;

  ASSERT (Rand != NULL);

  if (Rand == NULL) {
    return FALSE;
  }

  if (!ArchIsRngSupported ()) {
    return FALSE;
  }

  //
  // A loop to fetch a 32 bit random value with a retry count limit.
  //
  for (Index = 0; Index < GETRANDOM_RETRY_LIMIT; Index++) {
    if (ArchGetRandomNumber32 (Rand)) {
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
  OUT     UINT64  *Rand
  )
{
  UINT32  Index;

  ASSERT (Rand != NULL);

  if (Rand == NULL) {
    return FALSE;
  }

  if (!ArchIsRngSupported ()) {
    return FALSE;
  }

  //
  // A loop to fetch a 64 bit random value with a retry count limit.
  //
  for (Index = 0; Index < GETRANDOM_RETRY_LIMIT; Index++) {
    if (ArchGetRandomNumber64 (Rand)) {
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
  OUT     UINT64  *Rand
  )
{
  ASSERT (Rand != NULL);

  if (Rand == NULL) {
    return FALSE;
  }

  if (!ArchIsRngSupported ()) {
    return FALSE;
  }

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
