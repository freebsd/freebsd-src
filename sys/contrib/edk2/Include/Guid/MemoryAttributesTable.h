/** @file
  GUIDs used for UEFI Memory Attributes Table in the UEFI 2.6 specification.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __UEFI_MEMORY_ATTRIBUTES_TABLE_H__
#define __UEFI_MEMORY_ATTRIBUTES_TABLE_H__

#define EFI_MEMORY_ATTRIBUTES_TABLE_GUID  {\
  0xdcfa911d, 0x26eb, 0x469f, {0xa2, 0x20, 0x38, 0xb7, 0xdc, 0x46, 0x12, 0x20} \
}

typedef struct {
  UINT32    Version;
  UINT32    NumberOfEntries;
  UINT32    DescriptorSize;
  UINT32    Flags;
  // EFI_MEMORY_DESCRIPTOR Entry[1];
} EFI_MEMORY_ATTRIBUTES_TABLE;

#define EFI_MEMORY_ATTRIBUTES_TABLE_VERSION  0x00000002

#define EFI_MEMORY_ATTRIBUTES_FLAGS_RT_FORWARD_CONTROL_FLOW_GUARD  0x1
// BIT0 implies that Runtime code includes the forward control flow guard
// instruction, such as X86 CET-IBT or ARM BTI.

extern EFI_GUID  gEfiMemoryAttributesTableGuid;

#endif
