/** @file
  GUID and related data structures used with the Debug Image Info Table.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUID defined in UEFI 2.0 spec.

**/

#ifndef __DEBUG_IMAGE_INFO_GUID_H__
#define __DEBUG_IMAGE_INFO_GUID_H__

#include <Protocol/LoadedImage.h>

///
/// EFI_DEBUG_IMAGE_INFO_TABLE configuration table GUID declaration.
///
#define EFI_DEBUG_IMAGE_INFO_TABLE_GUID \
  { \
    0x49152e77, 0x1ada, 0x4764, {0xb7, 0xa2, 0x7a, 0xfe, 0xfe, 0xd9, 0x5e, 0x8b } \
  }

#define EFI_DEBUG_IMAGE_INFO_UPDATE_IN_PROGRESS 0x01
#define EFI_DEBUG_IMAGE_INFO_TABLE_MODIFIED     0x02

#define EFI_DEBUG_IMAGE_INFO_TYPE_NORMAL        0x01

typedef struct {
  UINT64                Signature;          ///< A constant UINT64 that has the value EFI_SYSTEM_TABLE_SIGNATURE
  EFI_PHYSICAL_ADDRESS  EfiSystemTableBase; ///< The physical address of the EFI system table.
  UINT32                Crc32;              ///< A 32-bit CRC value that is used to verify the EFI_SYSTEM_TABLE_POINTER structure is valid.
} EFI_SYSTEM_TABLE_POINTER;

typedef struct {
  ///
  /// Indicates the type of image info structure. For PE32 EFI images,
  /// this is set to EFI_DEBUG_IMAGE_INFO_TYPE_NORMAL.
  ///
  UINT32                     ImageInfoType;
  ///
  /// A pointer to an instance of the loaded image protocol for the associated image.
  ///
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImageProtocolInstance;
  ///
  /// Indicates the image handle of the associated image.
  ///
  EFI_HANDLE                 ImageHandle;
} EFI_DEBUG_IMAGE_INFO_NORMAL;

typedef union {
  UINT32                      *ImageInfoType;
  EFI_DEBUG_IMAGE_INFO_NORMAL *NormalImage;
} EFI_DEBUG_IMAGE_INFO;

typedef struct {
  ///
  /// UpdateStatus is used by the system to indicate the state of the debug image info table.
  ///
  volatile UINT32       UpdateStatus;
  ///
  /// The number of EFI_DEBUG_IMAGE_INFO elements in the array pointed to by EfiDebugImageInfoTable.
  ///
  UINT32                TableSize;
  ///
  /// A pointer to the first element of an array of EFI_DEBUG_IMAGE_INFO structures.
  ///
  EFI_DEBUG_IMAGE_INFO  *EfiDebugImageInfoTable;
} EFI_DEBUG_IMAGE_INFO_TABLE_HEADER;

extern EFI_GUID gEfiDebugImageInfoTableGuid;

#endif
