/** @file
  MM PCI Root Bridge IO protocol as defined in the PI 1.5 specification.

  This protocol provides PCI I/O and memory access within MM.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MM_PCI_ROOT_BRIDGE_IO_H_
#define _MM_PCI_ROOT_BRIDGE_IO_H_

#include <Protocol/PciRootBridgeIo.h>

#define EFI_MM_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID \
  { \
    0x8bc1714d, 0xffcb, 0x41c3, { 0x89, 0xdc, 0x6c, 0x74, 0xd0, 0x6d, 0x98, 0xea } \
  }

///
/// This protocol provides the same functionality as the PCI Root Bridge I/O Protocol defined in the
/// UEFI 2.1 Specifcation, section 13.2, except that the functions for Map() and Unmap() may return
/// EFI_UNSUPPORTED.
///
typedef EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL EFI_MM_PCI_ROOT_BRIDGE_IO_PROTOCOL;

extern EFI_GUID  gEfiMmPciRootBridgeIoProtocolGuid;

#endif
