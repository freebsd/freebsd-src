/** @file

  Architecture specific interface to RNG functionality.

Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef BASE_RNGLIB_INTERNALS_H_
#define BASE_RNGLIB_INTERNALS_H_

/**
  Generates a 16-bit random number.

  @param[out] Rand     Buffer pointer to store the 16-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
ArchGetRandomNumber16 (
  OUT UINT16  *Rand
  );

/**
  Generates a 32-bit random number.

  @param[out] Rand     Buffer pointer to store the 32-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
ArchGetRandomNumber32 (
  OUT UINT32  *Rand
  );

/**
  Generates a 64-bit random number.

  @param[out] Rand     Buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
ArchGetRandomNumber64 (
  OUT UINT64  *Rand
  );

/**
  Checks whether the RNG instruction is supported.

  @retval TRUE         RNG instruction is supported.
  @retval FALSE        RNG instruction is not supported.

**/
BOOLEAN
EFIAPI
ArchIsRngSupported (
  VOID
  );

#if defined (MDE_CPU_AARCH64)

// RNDR, Random Number
#define RNDR  S3_3_C2_C4_0

#endif

#endif // BASE_RNGLIB_INTERNALS_H_
