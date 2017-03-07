/** @file
  SMM PCI Root Bridge IO protocol as defined in the PI 1.2 specification.

  This protocol provides PCI I/O and memory access within SMM.

  Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _SMM_PCI_ROOT_BRIDGE_IO_H_
#define _SMM_PCI_ROOT_BRIDGE_IO_H_

#include <Protocol/PciRootBridgeIo.h>

#define EFI_SMM_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID \
  { \
    0x8bc1714d, 0xffcb, 0x41c3, { 0x89, 0xdc, 0x6c, 0x74, 0xd0, 0x6d, 0x98, 0xea } \
  }

///
/// This protocol provides the same functionality as the PCI Root Bridge I/O Protocol defined in the 
/// UEFI 2.1 Specifcation, section 13.2, except that the functions for Map() and Unmap() may return 
/// EFI_UNSUPPORTED.
///
typedef EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  EFI_SMM_PCI_ROOT_BRIDGE_IO_PROTOCOL;

extern EFI_GUID gEfiSmmPciRootBridgeIoProtocolGuid;

#endif

