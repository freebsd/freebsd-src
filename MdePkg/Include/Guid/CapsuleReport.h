/** @file
  Guid & data structure used for Capsule process result variables
  
  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:
  GUIDs defined in UEFI 2.4 spec.

**/


#ifndef _CAPSULE_REPORT_GUID_H__
#define _CAPSULE_REPORT_GUID_H__

//
// This is the GUID for capsule result variable.
//
#define EFI_CAPSULE_REPORT_GUID \
  { \
    0x39b68c46, 0xf7fb, 0x441b, {0xb6, 0xec, 0x16, 0xb0, 0xf6, 0x98, 0x21, 0xf3 } \
  }


typedef struct {

  ///
  /// Size in bytes of the variable including any data beyond header as specified by CapsuleGuid
  ///
  UINT32     VariableTotalSize;

  ///
  /// For alignment
  ///
  UINT32     Reserved;

  ///
  /// Guid from EFI_CAPSULE_HEADER
  ///
  EFI_GUID   CapsuleGuid;

  ///
  /// Timestamp using system time when processing completed
  ///
  EFI_TIME   CapsuleProcessed;

  ///
  /// Result of the capsule processing. Exact interpretation of any error code may depend
  /// upon type of capsule processed
  ///
  EFI_STATUS CapsuleStatus;
} EFI_CAPSULE_RESULT_VARIABLE_HEADER;


typedef struct {

  ///
  /// Version of this structure, currently 0x00000001
  ///
  UINT16   Version;

  ///
  /// The index of the payload within the FMP capsule which was processed to generate this report
  /// Starting from zero
  ///
  UINT8    PayloadIndex;

  ///
  /// The UpdateImageIndex from EFI_FIRMWARE_MANAGEMENT_CAPSULE_IMAGE_HEADER
  /// (after unsigned conversion from UINT8 to UINT16).
  ///
  UINT8    UpdateImageIndex;

  ///
  /// The UpdateImageTypeId Guid from EFI_FIRMWARE_MANAGEMENT_CAPSULE_IMAGE_HEADER.
  ///
  EFI_GUID UpdateImageTypeId;

  ///
  /// In case of capsule loaded from disk, the zero-terminated array containing file name of capsule that was processed.
  /// In case of capsule submitted directly to UpdateCapsule() there is no file name, and this field is required to contain a single 16-bit zero character 
  ///  which is included in VariableTotalSize.
  ///
  /// CHAR16 CapsuleFileName[];
  ///

  ///
  /// This field will contain a zero-terminated CHAR16 string containing the text representation of the device path of device publishing Firmware Management Protocol  
  /// (if present). In case where device path is not present and the target is not otherwise known to firmware, or when payload was blocked by policy, or skipped,
  /// this field is required to contain a single 16-bit zero character which is included in VariableTotalSize.
  ///
  /// CHAR16 CapsuleTarget[];
  ///
} EFI_CAPSULE_RESULT_VARIABLE_FMP;


extern EFI_GUID gEfiCapsuleReportGuid;

#endif
