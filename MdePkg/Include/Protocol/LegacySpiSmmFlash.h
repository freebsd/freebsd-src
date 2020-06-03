/** @file
  This file defines the Legacy SPI SMM Flash Protocol.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    This Protocol was introduced in UEFI PI Specification 1.6.

**/

#ifndef __LEGACY_SPI_SMM_FLASH_PROTOCOL_H__
#define __LEGACY_SPI_SMM_FLASH_PROTOCOL_H__

#include <Protocol/LegacySpiFlash.h>

///
/// Global ID for the Legacy SPI SMM Flash Protocol
///
#define EFI_LEGACY_SPI_SMM_FLASH_PROTOCOL_GUID  \
  { 0x5e3848d4, 0x0db5, 0x4fc0,                 \
    { 0x97, 0x29, 0x3f, 0x35, 0x3d, 0x4f, 0x87, 0x9f }}

typedef
struct _EFI_LEGACY_SPI_FLASH_PROTOCOL
EFI_LEGACY_SPI_SMM_FLASH_PROTOCOL;

extern EFI_GUID gEfiLegacySpiSmmFlashProtocolGuid;

#endif // __SPI_SMM_FLASH_PROTOCOL_H__
