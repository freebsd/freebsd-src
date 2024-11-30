/** @file
  This is a special GUID extension Hob to describe SMRAM memory regions.

  This file defines:
  * the GUID used to identify the GUID HOB for reserving SMRAM regions.
  * the data structure of SMRAM descriptor to describe SMRAM candidate regions
  * values of state of SMRAM candidate regions
  * the GUID specific data structure of HOB for reserving SMRAM regions.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUIDs defined in PI SPEC version 1.5.

**/

#ifndef _SMRAM_MEMORY_RESERVE_H_
#define _SMRAM_MEMORY_RESERVE_H_

#define EFI_SMM_SMRAM_MEMORY_GUID \
  { \
    0x6dadf1d1, 0xd4cc, 0x4910, {0xbb, 0x6e, 0x82, 0xb1, 0xfd, 0x80, 0xff, 0x3d } \
  }

/**
* The GUID extension hob is to describe SMRAM memory regions supported by the platform.
**/
typedef struct {
  ///
  /// Designates the number of possible regions in the system
  /// that can be usable for SMRAM.
  ///
  UINT32                  NumberOfSmmReservedRegions;
  ///
  /// Used throughout this protocol to describe the candidate
  /// regions for SMRAM that are supported by this platform.
  ///
  EFI_SMRAM_DESCRIPTOR    Descriptor[1];
} EFI_SMRAM_HOB_DESCRIPTOR_BLOCK;

extern EFI_GUID  gEfiSmmSmramMemoryGuid;

#endif
