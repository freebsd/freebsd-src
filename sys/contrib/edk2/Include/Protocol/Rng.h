/** @file
  EFI_RNG_PROTOCOL as defined in UEFI 2.4.
  The UEFI Random Number Generator Protocol is used to provide random bits for use
  in applications, or entropy for seeding other random number generators.

Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_RNG_PROTOCOL_H_
#define EFI_RNG_PROTOCOL_H_

#include <Guid/Rng.h>

///
/// Global ID for the Random Number Generator Protocol
///
#define EFI_RNG_PROTOCOL_GUID \
  { \
    0x3152bca5, 0xeade, 0x433d, {0x86, 0x2e, 0xc0, 0x1c, 0xdc, 0x29, 0x1f, 0x44 } \
  }

typedef EFI_RNG_INTERFACE EFI_RNG_PROTOCOL;

extern EFI_GUID  gEfiRngProtocolGuid;

#endif
