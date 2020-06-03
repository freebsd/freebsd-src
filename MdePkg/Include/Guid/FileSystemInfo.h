/** @file
  Provides a GUID and a data structure that can be used with EFI_FILE_PROTOCOL.GetInfo()
  or EFI_FILE_PROTOCOL.SetInfo() to get or set information about the system's volume.
  This GUID is defined in UEFI specification.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FILE_SYSTEM_INFO_H__
#define __FILE_SYSTEM_INFO_H__

#define EFI_FILE_SYSTEM_INFO_ID \
  { \
    0x9576e93, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

typedef struct {
  ///
  /// The size of the EFI_FILE_SYSTEM_INFO structure, including the Null-terminated VolumeLabel string.
  ///
  UINT64  Size;
  ///
  /// TRUE if the volume only supports read access.
  ///
  BOOLEAN ReadOnly;
  ///
  /// The number of bytes managed by the file system.
  ///
  UINT64  VolumeSize;
  ///
  /// The number of available bytes for use by the file system.
  ///
  UINT64  FreeSpace;
  ///
  /// The nominal block size by which files are typically grown.
  ///
  UINT32  BlockSize;
  ///
  /// The Null-terminated string that is the volume's label.
  ///
  CHAR16  VolumeLabel[1];
} EFI_FILE_SYSTEM_INFO;

///
/// The VolumeLabel field of the EFI_FILE_SYSTEM_INFO data structure is variable length.
/// Whenever code needs to know the size of the EFI_FILE_SYSTEM_INFO data structure, it needs
/// to be the size of the data structure without the VolumeLable field.  The following macro
/// computes this size correctly no matter how big the VolumeLable array is declared.
/// This is required to make the EFI_FILE_SYSTEM_INFO data structure ANSI compilant.
///
#define SIZE_OF_EFI_FILE_SYSTEM_INFO  OFFSET_OF (EFI_FILE_SYSTEM_INFO, VolumeLabel)

extern EFI_GUID gEfiFileSystemInfoGuid;

#endif
