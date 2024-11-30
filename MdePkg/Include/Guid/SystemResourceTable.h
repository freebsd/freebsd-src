/** @file
  Guid & data structure used for EFI System Resource Table (ESRT)

  Copyright (c) 2015 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUIDs defined in UEFI 2.5 spec.

**/

#ifndef _SYSTEM_RESOURCE_TABLE_H__
#define _SYSTEM_RESOURCE_TABLE_H__

#define EFI_SYSTEM_RESOURCE_TABLE_GUID \
  { \
    0xb122a263, 0x3661, 0x4f68, {0x99, 0x29, 0x78, 0xf8, 0xb0, 0xd6, 0x21, 0x80 } \
  }

///
/// Current Entry Version
///
#define EFI_SYSTEM_RESOURCE_TABLE_FIRMWARE_RESOURCE_VERSION  1

///
/// Firmware Type Definitions
///
#define ESRT_FW_TYPE_UNKNOWN         0x00000000
#define ESRT_FW_TYPE_SYSTEMFIRMWARE  0x00000001
#define ESRT_FW_TYPE_DEVICEFIRMWARE  0x00000002
#define ESRT_FW_TYPE_UEFIDRIVER      0x00000003

///
/// Last Attempt Status Values
///
#define LAST_ATTEMPT_STATUS_SUCCESS                         0x00000000
#define LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL              0x00000001
#define LAST_ATTEMPT_STATUS_ERROR_INSUFFICIENT_RESOURCES    0x00000002
#define LAST_ATTEMPT_STATUS_ERROR_INCORRECT_VERSION         0x00000003
#define LAST_ATTEMPT_STATUS_ERROR_INVALID_FORMAT            0x00000004
#define LAST_ATTEMPT_STATUS_ERROR_AUTH_ERROR                0x00000005
#define LAST_ATTEMPT_STATUS_ERROR_PWR_EVT_AC                0x00000006
#define LAST_ATTEMPT_STATUS_ERROR_PWR_EVT_BATT              0x00000007
#define LAST_ATTEMPT_STATUS_ERROR_UNSATISFIED_DEPENDENCIES  0x00000008

///
/// LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL_VENDOR_RANGE_MAX is defined as
/// 0x4000 as of UEFI Specification 2.8B. This will be modified in the
/// future to the correct value 0x3FFF. To ensure correct implementation,
/// this change is preemptively made in the value defined below.
///
/// When the UEFI Specification is updated, this comment block can be
/// removed.
///
#define LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL_VENDOR_RANGE_MIN  0x00001000
#define LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL_VENDOR_RANGE_MAX  0x00003FFF

typedef struct {
  ///
  /// The firmware class field contains a GUID that identifies a firmware component
  /// that can be updated via UpdateCapsule(). This GUID must be unique within all
  /// entries of the ESRT.
  ///
  EFI_GUID    FwClass;
  ///
  /// Identifies the type of firmware resource.
  ///
  UINT32      FwType;
  ///
  /// The firmware version field represents the current version of the firmware
  /// resource, value must always increase as a larger number represents a newer
  /// version.
  ///
  UINT32      FwVersion;
  ///
  /// The lowest firmware resource version to which a firmware resource can be
  /// rolled back for the given system/device. Generally this is used to protect
  /// against known and fixed security issues.
  ///
  UINT32      LowestSupportedFwVersion;
  ///
  /// The capsule flags field contains the CapsuleGuid flags (bits 0- 15) as defined
  /// in the EFI_CAPSULE_HEADER that will be set in the capsule header.
  ///
  UINT32      CapsuleFlags;
  ///
  /// The last attempt version field describes the last firmware version for which
  /// an update was attempted (uses the same format as Firmware Version).
  /// Last Attempt Version is updated each time an UpdateCapsule() is attempted for
  /// an ESRT entry and is preserved across reboots (non-volatile). However, in
  /// cases where the attempt version is not recorded due to limitations in the
  /// update process, the field shall set to zero after a failed update. Similarly,
  /// in the case of a removable device, this value is set to 0 in cases where the
  /// device has not been updated since being added to the system.
  ///
  UINT32    LastAttemptVersion;
  ///
  /// The last attempt status field describes the result of the last firmware update
  /// attempt for the firmware resource entry.
  /// LastAttemptStatus is updated each time an UpdateCapsule() is attempted for an
  /// ESRT entry and is preserved across reboots (non-volatile).
  /// If a firmware update has never been attempted or is unknown, for example after
  /// fresh insertion of a removable device, LastAttemptStatus must be set to Success.
  ///
  UINT32    LastAttemptStatus;
} EFI_SYSTEM_RESOURCE_ENTRY;

typedef struct {
  ///
  /// The number of firmware resources in the table, must not be zero.
  ///
  UINT32    FwResourceCount;
  ///
  /// The maximum number of resource array entries that can be within the table
  /// without reallocating the table, must not be zero.
  ///
  UINT32    FwResourceCountMax;
  ///
  /// The version of the EFI_SYSTEM_RESOURCE_ENTRY entities used in this table.
  /// This field should be set to 1.
  ///
  UINT64    FwResourceVersion;
  ///
  /// Array of EFI_SYSTEM_RESOURCE_ENTRY
  ///
  // EFI_SYSTEM_RESOURCE_ENTRY  Entries[];
} EFI_SYSTEM_RESOURCE_TABLE;

extern EFI_GUID  gEfiSystemResourceTableGuid;

#endif
