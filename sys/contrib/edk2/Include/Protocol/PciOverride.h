/** @file
  This file declares EFI PCI Override protocol which provides the interface between
  the PCI bus driver/PCI Host Bridge Resource Allocation driver and an implementation's
  driver to describe the unique features of a platform.
  This protocol is optional.

  Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is defined in UEFI Platform Initialization Specification 1.2
  Volume 5: Standards

**/

#ifndef _PCI_OVERRIDE_H_
#define _PCI_OVERRIDE_H_

///
/// EFI_PCI_OVERRIDE_PROTOCOL has the same structure with EFI_PCI_PLATFORM_PROTOCOL
///
#include <Protocol/PciPlatform.h>

///
/// Global ID for the EFI_PCI_OVERRIDE_PROTOCOL
///
#define EFI_PCI_OVERRIDE_GUID \
  { \
    0xb5b35764, 0x460c, 0x4a06, {0x99, 0xfc, 0x77, 0xa1, 0x7c, 0x1b, 0x5c, 0xeb} \
  }

///
/// Declaration for EFI_PCI_OVERRIDE_PROTOCOL
///
typedef EFI_PCI_PLATFORM_PROTOCOL EFI_PCI_OVERRIDE_PROTOCOL;

extern EFI_GUID  gEfiPciOverrideProtocolGuid;

#endif
