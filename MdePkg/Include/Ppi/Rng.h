/** @file
  The Random Number Generator (RNG) PPI is used to provide random bits for use
  in PEIMs, or entropy for seeding other random number generators. The PPI was
  introduced in the PI 1.9 Specification.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef RNG_PPI_H_
#define RNG_PPI_H_

#include <Guid/Rng.h>

///
/// Global ID for the Random Number Generator PPI
///
#define RNG_PPI_GUID \
  { \
    0xeaed0a7e, 0x1a70, 0x4c2b, { 0x85, 0x58, 0x37, 0x17, 0x74, 0x56, 0xd8, 0x06 } \
  }

typedef EFI_RNG_INTERFACE RNG_PPI;

extern EFI_GUID  gEfiRngPpiGuid;

#endif
