/** @file
  This file provides location, format and authentication status of a firmware volume.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.3 errata.

**/

#ifndef __EFI_PEI_FIRMWARE_VOLUME_INFO2_H__
#define __EFI_PEI_FIRMWARE_VOLUME_INFO2_H__



#define EFI_PEI_FIRMWARE_VOLUME_INFO2_PPI_GUID \
{ 0xea7ca24b, 0xded5, 0x4dad, { 0xa3, 0x89, 0xbf, 0x82, 0x7e, 0x8f, 0x9b, 0x38 } }

typedef struct _EFI_PEI_FIRMWARE_VOLUME_INFO2_PPI  EFI_PEI_FIRMWARE_VOLUME_INFO2_PPI;

///
///  This PPI describes the location and format of a firmware volume.
///  The FvFormat can be EFI_FIRMWARE_FILE_SYSTEM2_GUID or the GUID for
///  a user-defined format. The  EFI_FIRMWARE_FILE_SYSTEM2_GUID is
///  the PI Firmware Volume format.
///
struct _EFI_PEI_FIRMWARE_VOLUME_INFO2_PPI {
  ///
  /// Unique identifier of the format of the memory-mapped firmware volume.
  ///
  EFI_GUID  FvFormat;
  ///
  /// Points to a buffer which allows the EFI_PEI_FIRMWARE_VOLUME_PPI to process
  /// the volume. The format of this buffer is specific to the FvFormat.
  /// For memory-mapped firmware volumes, this typically points to the first byte
  /// of the firmware volume.
  ///
  VOID      *FvInfo;
  ///
  /// Size of the data provided by FvInfo. For memory-mapped firmware volumes,
  /// this is typically the size of the firmware volume.
  ///
  UINT32    FvInfoSize;
  ///
  /// If the firmware volume originally came from a firmware file, then these
  /// point to the parent firmware volume name and firmware volume file.
  /// If it did not originally come from a firmware file, these should be NULL.
  ///
  EFI_GUID  *ParentFvName;
  ///
  /// If the firmware volume originally came from a firmware file, then these
  /// point to the parent firmware volume name and firmware volume file.
  /// If it did not originally come from a firmware file, these should be NULL.
  ///
  EFI_GUID  *ParentFileName;
  ///
  /// Authentication Status.
  ///
  UINT32    AuthenticationStatus;
};

extern EFI_GUID gEfiPeiFirmwareVolumeInfo2PpiGuid;

#endif

