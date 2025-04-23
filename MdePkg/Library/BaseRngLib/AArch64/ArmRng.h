/** @file
  Random number generator service that uses the RNDR instruction
  to provide pseudorandom numbers.

  Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ARM_RNG_H_
#define ARM_RNG_H_

#include <AArch64/AArch64.h>

/**
  Generates a random number using RNDR.
  Returns TRUE on success; FALSE on failure.

  @param[out] Rand     Buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
ArmRndr (
  OUT UINT64  *Rand
  );

#endif /* ARM_RNG_H_ */
